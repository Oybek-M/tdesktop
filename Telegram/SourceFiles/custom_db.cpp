#include "custom_db.h"
#include "sqlite3.h"
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDirIterator>
#include <QtCore/QDebug>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtCore/QProcess>
#include <QtCore/QMutex>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSysInfo>
#include "history/history_item.h"
#include "history/history.h"
#include "data/data_peer.h"

namespace CustomDB {

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

static sqlite3 *gDb = nullptr;

// In-memory caches populated at startup for O(1) constructor lookups.
// gCacheMutex guards both caches against concurrent reads/writes from
// background threads (e.g. MTProto callbacks vs. UI thread).
static QMutex gCacheMutex;
static QHash<QString, QSet<long long>> gDeletedCache;
static QHash<QString, QHash<long long, QString>> gEditedCache;

// Batch write queue: SaveMessage() enqueues here; FlushPendingWrites() commits all at once.
static QVector<ActionedMessage> gPendingWrites;
static QTimer *gFlushTimer = nullptr;

// Fast-path flag: avoids sqlite3 overhead on every Init() call.
static bool gInitialized = false;

// Hot-reload callback: called after a successful ImportFullBackup().
static ReloadCallback gReloadCallback;

// User-initiated delete tracking: messages the local user is deleting.
// Set in ScheduleUserDelete() (before API call) and cleared after server ACK.
static QSet<QPair<QString, long long>> gUserDeletePending;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static QString dbFilePath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/CustomMod/actioned_messages.db";
}

// Execute a SQL statement with no results expected. Returns true on success.
static bool execSql(const char *sql) {
    char *errmsg = nullptr;
    const int rc = sqlite3_exec(gDb, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        qDebug() << "CustomDB execSql error:" << errmsg << "| SQL:" << sql;
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

// Bind a UTF-8 QString to a prepared statement parameter (1-based index).
static void bindText(sqlite3_stmt *stmt, int idx, const QString &val) {
    const QByteArray utf8 = val.toUtf8();
    sqlite3_bind_text(stmt, idx, utf8.constData(), utf8.size(), SQLITE_TRANSIENT);
}

// Read a text column as QString.
static QString colText(sqlite3_stmt *stmt, int col) {
    const unsigned char *raw = sqlite3_column_text(stmt, col);
    if (!raw) return {};
    return QString::fromUtf8(reinterpret_cast<const char *>(raw));
}

// QDateTime → ISO 8601 string for storage.
static QString dtToStr(const QDateTime &dt) {
    return dt.toString(Qt::ISODate);
}

// ISO 8601 string → QDateTime.
static QDateTime strToDt(const QString &s) {
    return QDateTime::fromString(s, Qt::ISODate);
}

// ---------------------------------------------------------------------------
// Init / Migrations
// ---------------------------------------------------------------------------

void Init() {
    if (gInitialized && gDb) return;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/CustomMod";
    QDir().mkpath(dir);

    const QByteArray pathUtf8 = dbFilePath().toUtf8();
    const int rc = sqlite3_open_v2(
        pathUtf8.constData(),
        &gDb,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);

    if (rc != SQLITE_OK) {
        qDebug() << "CustomDB::Init: failed to open DB:" << sqlite3_errmsg(gDb);
        sqlite3_close(gDb);
        gDb = nullptr;
        return;
    }

    gInitialized = true;

    // Performance pragmas:
    // WAL mode: readers don't block writers and vice versa.
    // NORMAL synchronous: safe, faster than FULL.
    // cache_size: 8 MB page cache keeps hot pages in RAM.
    // temp_store MEMORY: temp tables live in RAM.
    // mmap_size: 32 MB memory-mapped I/O for large reads.
    execSql("PRAGMA journal_mode=WAL");
    execSql("PRAGMA synchronous=NORMAL");
    execSql("PRAGMA cache_size=-8000");
    execSql("PRAGMA temp_store=MEMORY");
    execSql("PRAGMA mmap_size=33554432");

    // schema_version table — always created first so migrations can read it.
    execSql("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL)");
    execSql("INSERT OR IGNORE INTO schema_version (rowid, version) VALUES (1, 0)");

    // Ghost reads table.
    execSql("CREATE TABLE IF NOT EXISTS ghost_reads "
            "(peer_id TEXT PRIMARY KEY, msg_id INTEGER, timestamp TEXT)");

    // Main messages table (baseline schema — migrations extend it).
    execSql("CREATE TABLE IF NOT EXISTS actioned_messages ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "peer_id TEXT, "
            "msg_id INTEGER, "
            "type TEXT, "
            "original_text TEXT, "
            "new_text TEXT, "
            "media_path TEXT, "
            "is_out INTEGER DEFAULT 0, "
            "msg_date INTEGER DEFAULT 0, "
            "timestamp TEXT)");

    // T27: Background AntiEdit cache.
    // Telegram tray/minimize holatda ham kelgan xabar matnlarini saqlaymiz
    // (faqat AntiEdit yoqilgan peerlar uchun) — keyin edit kelganda
    // eski matnni shu yerdan olib applyEdition logikasini bajaramiz.
    execSql("CREATE TABLE IF NOT EXISTS text_cache ("
            "peer_id TEXT, "
            "msg_id INTEGER, "
            "text TEXT, "
            "is_out INTEGER DEFAULT 0, "
            "msg_date INTEGER DEFAULT 0, "
            "cached_at INTEGER, "
            "PRIMARY KEY(peer_id, msg_id))");
    execSql("CREATE INDEX IF NOT EXISTS idx_tc_cached_at "
            "ON text_cache(cached_at)");

    RunMigrations();
}

void RunMigrations() {
    if (!gDb) return;

    int version = 0;
    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "SELECT version FROM schema_version WHERE rowid = 1",
                -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                version = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
    }
    if (version >= kCurrentSchemaVersion) return;

    execSql("BEGIN");

    // v0 → v1: ensure is_out and msg_date columns exist.
    if (version < 1) {
        execSql("ALTER TABLE actioned_messages ADD COLUMN is_out INTEGER DEFAULT 0");
        execSql("ALTER TABLE actioned_messages ADD COLUMN msg_date INTEGER DEFAULT 0");
    }

    // v1 → v2: add notes column for user annotations.
    if (version < 2) {
        execSql("ALTER TABLE actioned_messages ADD COLUMN notes TEXT DEFAULT ''");
    }

    // v2 → v3: add indexes for fast lookups.
    if (version < 3) {
        execSql("CREATE INDEX IF NOT EXISTS idx_am_peer_type "
                "ON actioned_messages(peer_id, type)");
        execSql("CREATE INDEX IF NOT EXISTS idx_am_peer_msg "
                "ON actioned_messages(peer_id, msg_id)");
        execSql("CREATE INDEX IF NOT EXISTS idx_am_timestamp "
                "ON actioned_messages(timestamp DESC)");
    }

    // v3 → v4: purge corrupted backup records.
    // Earlier versions saved _text (already containing display markers) instead
    // of the clean server text. Remove those rows so restoreFromCustomDB() no
    // longer shows spurious TAHRIRLANDI badges on unedited messages.
    if (version < 4) {
        execSql("DELETE FROM actioned_messages "
                "WHERE type = 'backup' "
                "AND (original_text LIKE '%TAHRIRLANDI%' "
                "     OR original_text LIKE '%O''CHIRILDI%')");
    }

    // v4 → v5: add sender_id + is_media columns.
    //   sender_id — guruhda o'chirilgan xabar haqiqiy yuboruvchisi (faqat
    //               guruh nomidan emas, to'g'ri ko'rsatish uchun).
    //   is_media  — media xabar (matn bo'sh) edi: background delete da
    //               o'chirilganini bilib, placeholder ko'rsatamiz.
    // execSql xatoni faqat log qiladi — ustun allaqachon mavjud bo'lsa zararsiz.
    if (version < 5) {
        execSql("ALTER TABLE actioned_messages ADD COLUMN sender_id TEXT DEFAULT ''");
        execSql("ALTER TABLE actioned_messages ADD COLUMN is_media INTEGER DEFAULT 0");
        execSql("ALTER TABLE text_cache ADD COLUMN sender_id TEXT DEFAULT ''");
        execSql("ALTER TABLE text_cache ADD COLUMN is_media INTEGER DEFAULT 0");
    }

    // Update version stamp.
    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "UPDATE schema_version SET version = ? WHERE rowid = 1",
                -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, kCurrentSchemaVersion);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    execSql("COMMIT");
}

// ---------------------------------------------------------------------------
// Cache loading
// ---------------------------------------------------------------------------

void LoadRestoreCache() {
    Init();
    {
        QMutexLocker locker(&gCacheMutex);
        gDeletedCache.clear();
        gEditedCache.clear();
    }
    if (!gDb) return;

    // Build local copies outside the lock (SQLite calls must NOT hold the mutex
    // since they can block), then swap atomically.
    QHash<QString, QSet<long long>> newDeleted;
    QHash<QString, QHash<long long, QString>> newEdited;

    // Pass 1: deleted messages.
    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "SELECT peer_id, msg_id FROM actioned_messages WHERE type = 'deleted'",
                -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const QString peerId = colText(stmt, 0);
                const long long msgId = sqlite3_column_int64(stmt, 1);
                newDeleted[peerId].insert(msgId);
            }
            sqlite3_finalize(stmt);
        }
    }

    // Pass 2: earliest backup/edited record per message (original text before first edit).
    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "SELECT peer_id, msg_id, original_text FROM actioned_messages "
                "WHERE type IN ('backup', 'edited') ORDER BY rowid ASC",
                -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const QString peerId = colText(stmt, 0);
                const long long msgId = sqlite3_column_int64(stmt, 1);
                if (!newEdited[peerId].contains(msgId)) {
                    newEdited[peerId][msgId] = colText(stmt, 2);
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    // Swap under lock.
    {
        QMutexLocker locker(&gCacheMutex);
        gDeletedCache = std::move(newDeleted);
        gEditedCache  = std::move(newEdited);
    }
}

// ---------------------------------------------------------------------------
// Fast O(1) cache lookups
// ---------------------------------------------------------------------------

bool IsDeletedLocally(const QString &peerId, long long msgId) {
    QMutexLocker locker(&gCacheMutex);
    const auto it = gDeletedCache.constFind(peerId);
    return it != gDeletedCache.constEnd() && it->contains(msgId);
}

QString GetOriginalTextBeforeEdit(const QString &peerId, long long msgId) {
    QMutexLocker locker(&gCacheMutex);
    const auto it = gEditedCache.constFind(peerId);
    if (it == gEditedCache.constEnd()) return {};
    return it->value(msgId);
}

// ---------------------------------------------------------------------------
// Ghost reads
// ---------------------------------------------------------------------------

void SaveGhostRead(const QString &peerId, long long msgId) {
    Init();
    // E22: Prune stale entries once every 50 saves to keep the table lean.
    static int sSaveCount = 0;
    if (++sSaveCount % 50 == 0) {
        PruneStaleGhostReads(30);
    }
    if (!gDb) return;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "INSERT OR REPLACE INTO ghost_reads (peer_id, msg_id, timestamp) VALUES (?, ?, ?)",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, msgId);
        bindText(stmt, 3, dtToStr(QDateTime::currentDateTime()));
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

long long GetGhostRead(const QString &peerId) {
    Init();
    if (!gDb) return 0;

    long long result = 0;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT msg_id FROM ghost_reads WHERE peer_id = ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

// E22: Remove the ghost-read record for a single peer.
void ResetGhostRead(const QString &peerId) {
    Init();
    if (!gDb) return;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "DELETE FROM ghost_reads WHERE peer_id = ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// E22: Delete ghost_reads entries whose timestamp is older than |days| days.
void PruneStaleGhostReads(int days) {
    Init();
    if (!gDb) return;

    const QString cutoff = dtToStr(QDateTime::currentDateTime().addDays(-days));
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "DELETE FROM ghost_reads WHERE timestamp < ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, cutoff);
        sqlite3_step(stmt);
        const int removed = sqlite3_changes(gDb);
        sqlite3_finalize(stmt);
        if (removed > 0) {
            qDebug() << "PruneStaleGhostReads: removed" << removed
                     << "entries older than" << days << "days.";
        }
    }
}

// ---------------------------------------------------------------------------
// Mark deleted
// ---------------------------------------------------------------------------

void MarkDeleted(
    long long msgId,
    const QString &peerId,
    const QString &mediaPath,
    const QString &originalText,
    unsigned int msgDate,
    bool isOut,
    const QString &senderId,
    bool isMedia)
{
    Init();
    if (!gDb) return;

    {
        QMutexLocker locker(&gCacheMutex);
        gDeletedCache[peerId].insert(msgId);
    }

    // Sprint 5: simplified UPDATE-first pattern — eliminates the extra SELECT.
    // Try UPDATE; if it touched 0 rows the record doesn't exist yet → INSERT.
    // v5: sender_id va is_media ham bo'sh bo'lmagan holda yangilanadi.
    {
        sqlite3_stmt *upd = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "UPDATE actioned_messages "
                "SET media_path    = CASE WHEN LENGTH(?)>0 THEN ? ELSE media_path    END,"
                "    original_text = CASE WHEN LENGTH(?)>0 THEN ? ELSE original_text END,"
                "    is_out        = ?,"
                "    msg_date      = CASE WHEN ?>0         THEN ? ELSE msg_date      END,"
                "    sender_id     = CASE WHEN LENGTH(?)>0 THEN ? ELSE sender_id     END,"
                "    is_media      = CASE WHEN ?<>0        THEN ? ELSE is_media      END "
                "WHERE peer_id=? AND msg_id=? AND type='deleted'",
                -1, &upd, nullptr) == SQLITE_OK) {
            bindText(upd, 1, mediaPath);    bindText(upd, 2, mediaPath);
            bindText(upd, 3, originalText); bindText(upd, 4, originalText);
            sqlite3_bind_int(upd, 5, isOut ? 1 : 0);
            sqlite3_bind_int64(upd, 6, msgDate); sqlite3_bind_int64(upd, 7, msgDate);
            bindText(upd, 8, senderId);     bindText(upd, 9, senderId);
            sqlite3_bind_int(upd, 10, isMedia ? 1 : 0); sqlite3_bind_int(upd, 11, isMedia ? 1 : 0);
            bindText(upd, 12, peerId);      sqlite3_bind_int64(upd, 13, msgId);
            sqlite3_step(upd);
            const int changed = sqlite3_changes(gDb);
            sqlite3_finalize(upd);
            if (changed > 0) return; // Row existed and was patched — done.
        }
    }

    // No existing row found — insert a fresh record.
    ActionedMessage msg;
    msg.peerId = peerId;
    msg.msgId = msgId;
    msg.type = "deleted";
    msg.mediaPath = mediaPath;
    msg.originalText = originalText;
    msg.msgDate = msgDate;
    msg.isOut = isOut;
    msg.senderId = senderId;
    msg.isMedia = isMedia;
    msg.timestamp = QDateTime::currentDateTime();
    SaveActionedMessage(msg);
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

QVector<DeletedMessage> GetDeletedMessages(const QString &peerId) {
    Init();
    QVector<DeletedMessage> result;
    if (!gDb) return result;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT msg_id, media_path, is_out, msg_date, original_text, sender_id, is_media "
            "FROM actioned_messages WHERE peer_id = ? AND type = 'deleted'",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DeletedMessage dm;
            dm.msgId     = sqlite3_column_int64(stmt, 0);
            dm.mediaPath = colText(stmt, 1);
            dm.isOut     = sqlite3_column_int(stmt, 2) != 0;
            dm.date      = static_cast<unsigned int>(sqlite3_column_int64(stmt, 3));
            dm.text      = colText(stmt, 4);
            dm.senderId  = colText(stmt, 5);
            dm.isMedia   = sqlite3_column_int(stmt, 6) != 0;
            result.push_back(dm);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

QString GetSavedMediaPath(const QString &peerId, long long msgId) {
    Init();
    if (!gDb) return {};

    sqlite3_stmt *stmt = nullptr;
    QString result;
    if (sqlite3_prepare_v2(gDb,
            "SELECT media_path FROM actioned_messages "
            "WHERE peer_id = ? AND msg_id = ? AND type = 'deleted' LIMIT 1",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, msgId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = colText(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

QVector<DeletedMessageWithPeer> GetAllDeletedMessages(int limit) {
    Init();
    QVector<DeletedMessageWithPeer> result;
    if (!gDb) return result;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT peer_id, msg_id, original_text, media_path, is_out, msg_date, sender_id, is_media "
            "FROM actioned_messages "
            "WHERE type = 'deleted' "
            "ORDER BY msg_date DESC "
            "LIMIT ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DeletedMessageWithPeer dm;
            dm.peerId    = colText(stmt, 0);
            dm.msgId     = sqlite3_column_int64(stmt, 1);
            dm.text      = colText(stmt, 2);
            dm.mediaPath = colText(stmt, 3);
            dm.isOut     = sqlite3_column_int(stmt, 4) != 0;
            dm.date      = static_cast<unsigned int>(sqlite3_column_int64(stmt, 5));
            dm.senderId  = colText(stmt, 6);
            dm.isMedia   = sqlite3_column_int(stmt, 7) != 0;
            result.push_back(dm);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

QVector<EditRecord> GetAllEditedMessages(int limit) {
    Init();
    QVector<EditRecord> result;
    if (!gDb) return result;

    // 'backup' records are pre-edit snapshots; 'edited' are edit events.
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT peer_id, msg_id, original_text, new_text, msg_date, timestamp "
            "FROM actioned_messages "
            "WHERE type IN ('edited', 'backup') "
            "ORDER BY timestamp DESC "
            "LIMIT ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            EditRecord rec;
            rec.peerId       = colText(stmt, 0);
            rec.msgId        = sqlite3_column_int64(stmt, 1);
            rec.originalText = colText(stmt, 2);
            rec.newText      = colText(stmt, 3);
            rec.msgDate      = static_cast<unsigned int>(sqlite3_column_int64(stmt, 4));
            rec.editedAt     = strToDt(colText(stmt, 5));
            result.push_back(rec);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

QString GetMessageHistory(long long msgId, const QString &peerId) {
    Init();
    if (!gDb) return {};

    QString result;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT original_text FROM actioned_messages "
            "WHERE msg_id = ? AND peer_id = ? AND type = 'backup' "
            "ORDER BY rowid DESC LIMIT 1",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, msgId);
        bindText(stmt, 2, peerId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = colText(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

QVector<QString> GetEditHistory(const QString &peerId, long long msgId) {
    Init();
    QVector<QString> result;
    if (!gDb) return result;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT original_text FROM actioned_messages "
            "WHERE peer_id = ? AND msg_id = ? AND type = 'backup' "
            "ORDER BY rowid ASC",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, msgId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const QString text = colText(stmt, 0);
            if (!text.isEmpty()) {
                result.push_back(text);
            }
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Insert helpers
// ---------------------------------------------------------------------------

void SaveActionedMessage(const ActionedMessage &msg) {
    Init();
    if (!gDb) return;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "INSERT INTO actioned_messages "
            "(peer_id, msg_id, type, original_text, new_text, media_path, is_out, msg_date, timestamp, sender_id, is_media) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, msg.peerId);
        sqlite3_bind_int64(stmt, 2, msg.msgId);
        bindText(stmt, 3, msg.type);
        bindText(stmt, 4, msg.originalText);
        bindText(stmt, 5, msg.newText);
        bindText(stmt, 6, msg.mediaPath);
        sqlite3_bind_int(stmt, 7, msg.isOut ? 1 : 0);
        sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(msg.msgDate));
        bindText(stmt, 9, dtToStr(msg.timestamp));
        bindText(stmt, 10, msg.senderId);
        sqlite3_bind_int(stmt, 11, msg.isMedia ? 1 : 0);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void FlushPendingWrites() {
    if (gPendingWrites.isEmpty()) return;
    Init();
    if (!gDb) {
        qWarning() << "CustomDB::FlushPendingWrites: DB not open, pending writes lost!";
        return;
    }

    execSql("BEGIN");

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "INSERT OR IGNORE INTO actioned_messages "
            "(peer_id, msg_id, type, original_text, is_out, msg_date, timestamp) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)",
            -1, &stmt, nullptr) == SQLITE_OK) {
        for (const ActionedMessage &msg : std::as_const(gPendingWrites)) {
            sqlite3_reset(stmt);
            bindText(stmt, 1, msg.peerId);
            sqlite3_bind_int64(stmt, 2, msg.msgId);
            bindText(stmt, 3, msg.type);
            bindText(stmt, 4, msg.originalText);
            sqlite3_bind_int(stmt, 5, msg.isOut ? 1 : 0);
            sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(msg.msgDate));
            bindText(stmt, 7, dtToStr(msg.timestamp));
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
    }

    execSql("COMMIT");
    gPendingWrites.clear();
}

void SaveMessage(HistoryItem *item) {
    if (!item) return;

    // Extract clean server text — strip any display markers that
    // restoreFromCustomDB() may have prepended to _text already.
    QString cleanText = item->originalText().text;
    // Strip TAHRIRLANDI chain: keep only the part after the last separator.
    // Format: "<old versions>—— TAHRIRLANDI N ——\n<current server text>"
    const QString tahrMarker = QString::fromUtf8("\xe2\x80\x94\xe2\x80\x94 TAHRIRLANDI");
    const int lastTm = cleanText.lastIndexOf(tahrMarker);
    if (lastTm >= 0) {
        // Find the newline after the closing ——, skip it, take the rest.
        const int nl = cleanText.indexOf('\n', lastTm);
        cleanText = (nl >= 0)
            ? cleanText.mid(nl + 1).trimmed()
            : cleanText.mid(lastTm).trimmed();
    }
    // Strip O'CHIRILDI marker if present.
    const QString ochMarker = QString::fromUtf8("\xe2\x80\x94\xe2\x80\x94 O'CHIRILDI");
    const int ochPos = cleanText.indexOf(ochMarker);
    if (ochPos >= 0) {
        const int nl = cleanText.indexOf('\n', ochPos);
        cleanText = (nl >= 0)
            ? cleanText.mid(nl + 1).trimmed()
            : QString();
    }

    ActionedMessage msg;
    msg.peerId = QString::number(item->history()->peer->id.value);
    msg.msgId = static_cast<long long>(item->id.bare);
    msg.type = "backup";
    msg.originalText = cleanText;
    msg.isOut = item->out();
    msg.msgDate = static_cast<unsigned int>(item->date());
    msg.timestamp = QDateTime::currentDateTime();

    // Update in-session edited cache so restoreFromCustomDB() works if this
    // HistoryItem is ever re-created in the same session (e.g. after unload).
    // Only store the FIRST original (before any subsequent edits), matching
    // the LoadRestoreCache() strategy.
    {
        QMutexLocker locker(&gCacheMutex);
        if (!gEditedCache[msg.peerId].contains(msg.msgId)) {
            gEditedCache[msg.peerId][msg.msgId] = cleanText;
        }
    }

    gPendingWrites.append(msg);

    // Arm a 100ms one-shot timer to flush the queue; restart if already running.
    if (!gFlushTimer) {
        gFlushTimer = new QTimer();
        gFlushTimer->setSingleShot(true);
        QObject::connect(gFlushTimer, &QTimer::timeout, []() {
            FlushPendingWrites();
        });
    }
    if (!gFlushTimer->isActive()) {
        gFlushTimer->start(100);
    }
}

// ---------------------------------------------------------------------------
// T27: Background AntiEdit cache
// ---------------------------------------------------------------------------

void PruneStaleCachedText(int days) {
    Init();
    if (!gDb) return;
    const qint64 cutoff =
        QDateTime::currentSecsSinceEpoch() - qint64(days) * 86400;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "DELETE FROM text_cache WHERE cached_at < ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, cutoff);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void CacheMessageText(
        const QString &peerId,
        long long msgId,
        const QString &text,
        bool isOut,
        unsigned int msgDate,
        const QString &senderId,
        bool isMedia) {
    Init();
    if (!gDb || peerId.isEmpty() || msgId == 0) return;
    // Matn bo'sh: faqat media xabar bo'lsa cache qilamiz (delete ni bilish uchun).
    // Aks holda (na matn, na media) — yozishdan ma'no yo'q.
    if (text.isEmpty() && !isMedia) return;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "INSERT OR REPLACE INTO text_cache "
            "(peer_id, msg_id, text, is_out, msg_date, cached_at, sender_id, is_media) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, msgId);
        bindText(stmt, 3, text);
        sqlite3_bind_int(stmt, 4, isOut ? 1 : 0);
        sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(msgDate));
        sqlite3_bind_int64(stmt, 6, QDateTime::currentSecsSinceEpoch());
        bindText(stmt, 7, senderId);
        sqlite3_bind_int(stmt, 8, isMedia ? 1 : 0);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Soft cleanup — har 1000-chi cache da eski yozuvlarni tozalash.
    static int counter = 0;
    if (++counter % 1000 == 0) {
        PruneStaleCachedText(30);
    }
}

QString GetCachedText(const QString &peerId, long long msgId) {
    Init();
    if (!gDb || peerId.isEmpty() || msgId == 0) return QString();

    QString result;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT text FROM text_cache WHERE peer_id=? AND msg_id=? LIMIT 1",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, msgId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = colText(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

QString GetCachedTextAndDate(
        const QString &peerId,
        long long msgId,
        unsigned int &outDate,
        QString *outSenderId,
        bool *outIsMedia) {
    outDate = 0;
    if (outSenderId) *outSenderId = QString();
    if (outIsMedia) *outIsMedia = false;
    Init();
    if (!gDb || peerId.isEmpty() || msgId == 0) return QString();

    QString result;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT text, msg_date, sender_id, is_media FROM text_cache "
            "WHERE peer_id=? AND msg_id=? LIMIT 1",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, msgId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result   = colText(stmt, 0);
            outDate  = static_cast<unsigned int>(sqlite3_column_int64(stmt, 1));
            if (outSenderId) *outSenderId = colText(stmt, 2);
            if (outIsMedia) *outIsMedia = (sqlite3_column_int(stmt, 3) != 0);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

bool RecordBackgroundEdit(
        const QString &peerId,
        long long msgId,
        const QString &newText,
        bool isOut,
        unsigned int msgDate) {
    Init();
    if (!gDb || peerId.isEmpty() || msgId == 0) return false;

    // YANGI-2: avvalgi cache yozuvidan sender_id + is_media ni ham o'qib olamiz.
    // INSERT OR REPLACE butun qatorni almashtirgani uchun, re-cache da bularni
    // saqlab qolmasak — T36 (haqiqiy yuboruvchi) edit→delete ketma-ketligida
    // yo'qolardi.
    unsigned int cachedDate = 0;
    QString cachedSender;
    bool cachedMedia = false;
    const QString oldText = GetCachedTextAndDate(
        peerId, msgId, cachedDate, &cachedSender, &cachedMedia);
    if (oldText.isEmpty()) {
        // Eski matn yo'q — saqlay olmaymiz. Yangi matnni cache ga yozamiz
        // (kelajakdagi qayta tahrirlash uchun), avvalgi sender/media bilan.
        CacheMessageText(peerId, msgId, newText, isOut, msgDate,
            cachedSender, cachedMedia);
        return false;
    }

    // Agar matn o'zgarmagan bo'lsa — yozmaymiz.
    if (oldText == newText) return false;

    // actioned_messages ga 'edited' yozuvi (rasmiy applyEdition o'rniga).
    // Format `restoreFromCustomDB` bilan mos: original_text = eski, new_text = yangi.
    ActionedMessage msg;
    msg.peerId = peerId;
    msg.msgId = msgId;
    msg.type = "edited";
    msg.originalText = oldText;
    msg.newText = newText;
    msg.isOut = isOut;
    msg.msgDate = msgDate;
    msg.timestamp = QDateTime::currentDateTime();
    SaveActionedMessage(msg);

    // In-memory cache ham yangilash — restoreFromCustomDB() uchun.
    {
        QMutexLocker locker(&gCacheMutex);
        if (!gEditedCache[peerId].contains(msgId)) {
            gEditedCache[peerId][msgId] = oldText;
        }
    }

    // Cache ni yangi matn bilan yangilab qo'yamiz — keyingi tahrir uchun.
    // YANGI-2: avvalgi sender/media saqlanadi (T36 edit→delete da buzilmasin).
    CacheMessageText(peerId, msgId, newText, isOut, msgDate,
        cachedSender, cachedMedia);
    return true;
}

void TryRecordBackgroundDelete(long long msgId) {
    Init();
    if (!gDb || msgId == 0) return;

    // Bu funksiya FAQAT non-channel delete (updateDeleteMessages) uchun chaqiriladi.
    // Lekin msg_id global emas: kanallar o'z ID ketma-ketligiga ega, shuning uchun
    // bir xil msg_id li kanal yozuvi ham cache da bo'lishi mumkin. Noto'g'ri peer ga
    // "o'chirildi" yozib qo'ymaslik uchun — barcha kandidatlardan FAQAT non-channel
    // (user/chat) peer ni tanlaymiz. Channel peer id: (value >> 48) & 0xFF == 2.
    QString peerId;
    QString text;
    QString senderId;
    bool isOut = false;
    bool isMedia = false;
    unsigned int msgDate = 0;
    bool found = false;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT peer_id, text, is_out, msg_date, sender_id, is_media "
            "FROM text_cache WHERE msg_id=?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, msgId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const QString candidate = colText(stmt, 0);
            const quint64 value = candidate.toULongLong();
            const bool isChannel = (((value >> 48) & 0xFFULL) == 2ULL);
            if (isChannel) continue; // non-channel delete — kanal yozuvini o'tkazib yuboramiz
            peerId   = candidate;
            text     = colText(stmt, 1);
            isOut    = (sqlite3_column_int(stmt, 2) != 0);
            msgDate  = static_cast<unsigned int>(sqlite3_column_int64(stmt, 3));
            senderId = colText(stmt, 4);
            isMedia  = (sqlite3_column_int(stmt, 5) != 0);
            found = true;
            break; // non-channel msg_id yagona bo'ladi
        }
        sqlite3_finalize(stmt);
    }

    if (!found || peerId.isEmpty()) return;

    // Foydalanuvchi o'zi o'chirgan bo'lsa, skip.
    if (IsUserDeletePending(peerId, msgId)) {
        ClearUserDeletePending(peerId, msgId);
        return;
    }

    // AntiDelete bu peer uchun yoqilganmi (global yoki Whitelist)?
    // Bu yerda CustomSettings dan tekshira olmaymiz (circular dep), shu sababli
    // cache ga faqat ShouldBackgroundCache true bo'lgan peerlar tushadi (data
    // qatlamida), demak bu yerga kelganlarning hammasini yozaveramiz. Agar
    // AntiDelete o'chirilgan bo'lsa, loadDeletedMessages() baribir ko'rsatmaydi.
    MarkDeleted(msgId, peerId, QString(), text, msgDate, isOut, senderId, isMedia);
}

// ---------------------------------------------------------------------------
// User-initiated delete support
// ---------------------------------------------------------------------------

void PermanentlyDeleteMessage(const QString &peerId, long long msgId) {
    Init();
    // Remove from SQLite.
    if (gDb) {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "DELETE FROM actioned_messages WHERE peer_id = ? AND msg_id = ?",
                -1, &stmt, nullptr) == SQLITE_OK) {
            bindText(stmt, 1, peerId);
            sqlite3_bind_int64(stmt, 2, msgId);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    // Remove from in-memory caches.
    {
        QMutexLocker locker(&gCacheMutex);
        if (gDeletedCache.contains(peerId)) {
            gDeletedCache[peerId].remove(msgId);
        }
        if (gEditedCache.contains(peerId)) {
            gEditedCache[peerId].remove(msgId);
        }
    }
    // Drop any queued (unflushed) writes for this message.
    gPendingWrites.erase(
        std::remove_if(gPendingWrites.begin(), gPendingWrites.end(),
            [&](const ActionedMessage &m) {
                return m.peerId == peerId && m.msgId == msgId;
            }),
        gPendingWrites.end());
}

void ScheduleUserDelete(const QString &peerId, long long msgId) {
    // Record intent before the API call so the server ACK can skip re-saving.
    gUserDeletePending.insert({peerId, msgId});
    // Also wipe any existing DB record immediately so restart won't restore it.
    PermanentlyDeleteMessage(peerId, msgId);
}

bool IsUserDeletePending(const QString &peerId, long long msgId) {
    return gUserDeletePending.contains({peerId, msgId});
}

void ClearUserDeletePending(const QString &peerId, long long msgId) {
    gUserDeletePending.remove({peerId, msgId});
}

// ---------------------------------------------------------------------------
// Export / Import
// ---------------------------------------------------------------------------

void ExportDatabase(const QString &targetPath) {
    Init();
    if (!gDb) return;
    // WAL checkpoint so the DB file is consistent before copying.
    sqlite3_wal_checkpoint_v2(gDb, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);
    QFile::copy(dbFilePath(), targetPath);
}

void ImportDatabase(const QString &sourcePath) {
    Init();
    if (!gDb) return;

    const QString currentDb = dbFilePath();
    sqlite3_close(gDb);
    gDb = nullptr;
    gInitialized = false;

    QFile::remove(currentDb);
    QFile::copy(sourcePath, currentDb);

    Init(); // re-open + re-run pragmas/migrations
}

static bool CopyDirRecursive(const QString &src, const QString &dst) {
    QDir srcDir(src);
    if (!srcDir.exists()) return true;
    QDir().mkpath(dst);
    for (const QFileInfo &info : srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString targetPath = dst + "/" + info.fileName();
        if (info.isDir()) {
            if (!CopyDirRecursive(info.filePath(), targetPath)) return false;
        } else {
            if (QFile::exists(targetPath)) QFile::remove(targetPath);
            if (!QFile::copy(info.filePath(), targetPath)) return false;
        }
    }
    return true;
}

QString ExportFullBackup(const QString &targetDir) {
    Init();
    if (!gDb) return {};
    FlushPendingWrites();

    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString zipPath = targetDir + "/CustomModBackup_" + stamp + ".zip";
    const QString stageDir = targetDir + "/CustomModBackup_" + stamp + "_tmp";
    if (!QDir().mkpath(stageDir)) return {};

    // Checkpoint WAL so the DB file on disk is complete.
    sqlite3_wal_checkpoint_v2(gDb, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);

    // 1) DB (asosiy arxiv).
    if (!QFile::copy(dbFilePath(), stageDir + "/actioned_messages.db")) {
        QDir(stageDir).removeRecursively();
        return {};
    }

    // 2) Media papkasi.
    const QString mediaSrc = QDir::homePath() + "/customizationMainFolder";
    if (QDir(mediaSrc).exists()) {
        if (!CopyDirRecursive(mediaSrc, stageDir + "/customizationMainFolder")) {
            QDir(stageDir).removeRecursively();
            return {};
        }
    }

    // 3) JSON sozlamalar fayllari (mavjud bo'lsa).
    const QString appDataCustom = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/CustomMod";
    const QString peerListsSrc = appDataCustom + "/peer_lists.json";
    const QString brandingSrc = appDataCustom + "/branding.json";
    if (QFile::exists(peerListsSrc)) {
        QFile::copy(peerListsSrc, stageDir + "/peer_lists.json");
    }
    if (QFile::exists(brandingSrc)) {
        QFile::copy(brandingSrc, stageDir + "/branding.json");
    }

    // 4) Registry export (Ghost/AntiDelete/AntiEdit togglelar + per-peer overrides).
    //    Windows-specific — boshqa platformalarda o'tkazib yuboriladi.
    #ifdef Q_OS_WIN
    {
        const QString regPath = stageDir + "/settings.reg";
        const QString regCmd = QString(
            "reg export \"HKCU\\Software\\CustomMod\" \"%1\" /y"
        ).arg(QDir::toNativeSeparators(regPath));
        QProcess regProc;
        regProc.start("cmd.exe", {"/c", regCmd});
        regProc.waitForFinished(10000);
        if (regProc.exitCode() != 0) {
            qDebug() << "ExportFullBackup: registry export failed:"
                     << regProc.readAllStandardError();
            // Non-fatal — davom etamiz.
        }
    }
    #endif

    // 5) Manifest fayli — backup metadata.
    {
        QJsonObject manifest;
        manifest["version"] = 2;
        manifest["createdAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        manifest["sourceHost"] = QSysInfo::machineHostName();
        manifest["os"] = QSysInfo::prettyProductName();
        manifest["hasRegistry"] = QFile::exists(stageDir + "/settings.reg");
        manifest["hasPeerLists"] = QFile::exists(stageDir + "/peer_lists.json");
        manifest["hasBranding"] = QFile::exists(stageDir + "/branding.json");
        manifest["hasMedia"] = QDir(stageDir + "/customizationMainFolder").exists();
        QFile mf(stageDir + "/manifest.json");
        if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
        }
    }

    // 6) ZIP qilish.
    const QString psCmd = QString(
        "Compress-Archive -Path '%1\\*' -DestinationPath '%2' -Force"
    ).arg(QDir::toNativeSeparators(stageDir), QDir::toNativeSeparators(zipPath));

    QProcess ps;
    ps.start("powershell.exe", {"-NonInteractive", "-Command", psCmd});
    ps.waitForFinished(30000);

    QDir(stageDir).removeRecursively();

    if (ps.exitCode() != 0 || !QFile::exists(zipPath)) {
        qDebug() << "ExportFullBackup: PowerShell compress failed:" << ps.readAllStandardError();
        return {};
    }

    return zipPath;
}

bool ImportFullBackup(const QString &sourcePath) {
    Init();
    if (!gDb) return false;

    QString sourceDir = sourcePath;
    QString tempExtractDir;

    if (sourcePath.endsWith(".zip", Qt::CaseInsensitive) && QFile::exists(sourcePath)) {
        tempExtractDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                         + "/CustomModImport_tmp";
        QDir(tempExtractDir).removeRecursively();
        QDir().mkpath(tempExtractDir);

        const QString psCmd = QString(
            "Expand-Archive -Path '%1' -DestinationPath '%2' -Force"
        ).arg(QDir::toNativeSeparators(sourcePath),
              QDir::toNativeSeparators(tempExtractDir));

        QProcess ps;
        ps.start("powershell.exe", {"-NonInteractive", "-Command", psCmd});
        ps.waitForFinished(30000);

        if (ps.exitCode() != 0) {
            qDebug() << "ImportFullBackup: failed to extract ZIP:" << ps.readAllStandardError();
            QDir(tempExtractDir).removeRecursively();
            return false;
        }
        sourceDir = tempExtractDir;
    }

    const QString srcDb = sourceDir + "/actioned_messages.db";
    if (!QFile::exists(srcDb)) {
        qDebug() << "ImportFullBackup: no DB file found in" << sourceDir;
        if (!tempExtractDir.isEmpty()) QDir(tempExtractDir).removeRecursively();
        return false;
    }

    // E20: Validate source DB with a temporary read-only connection.
    {
        sqlite3 *vdb = nullptr;
        const QByteArray srcUtf8 = srcDb.toUtf8();
        const int vrc = sqlite3_open_v2(
            srcUtf8.constData(), &vdb,
            SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);

        bool valid = false;
        if (vrc == SQLITE_OK) {
            // integrity_check
            auto integrityOk = [&]() -> bool {
                sqlite3_stmt *s = nullptr;
                if (sqlite3_prepare_v2(vdb, "PRAGMA integrity_check", -1, &s, nullptr) != SQLITE_OK)
                    return false;
                bool ok = false;
                if (sqlite3_step(s) == SQLITE_ROW) {
                    ok = (colText(s, 0).compare("ok", Qt::CaseInsensitive) == 0);
                }
                sqlite3_finalize(s);
                return ok;
            };

            // table existence check
            auto tableExists = [&]() -> bool {
                sqlite3_stmt *s = nullptr;
                if (sqlite3_prepare_v2(vdb,
                        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='actioned_messages'",
                        -1, &s, nullptr) != SQLITE_OK)
                    return false;
                const bool ok = (sqlite3_step(s) == SQLITE_ROW);
                sqlite3_finalize(s);
                return ok;
            };

            valid = integrityOk() && tableExists();
            sqlite3_close(vdb);
        }

        if (!valid) {
            qDebug() << "ImportFullBackup: source DB failed integrity check — aborting.";
            if (!tempExtractDir.isEmpty()) QDir(tempExtractDir).removeRecursively();
            return false;
        }
    }

    // Replace database: close live connection, overwrite, reopen.
    const QString currentDb = dbFilePath();
    sqlite3_close(gDb);
    gDb = nullptr;
    gInitialized = false;

    QFile::remove(currentDb);
    if (!QFile::copy(srcDb, currentDb)) {
        Init(); // try to reopen old DB (may fail but at least resets state)
        if (!tempExtractDir.isEmpty()) QDir(tempExtractDir).removeRecursively();
        return false;
    }

    Init(); // reopen with pragmas + migrations
    if (!gDb) {
        qDebug() << "ImportFullBackup: failed to reopen DB after import.";
        if (!tempExtractDir.isEmpty()) QDir(tempExtractDir).removeRecursively();
        return false;
    }

    // Replace media folder.
    const QString srcMedia = sourceDir + "/customizationMainFolder";
    if (QDir(srcMedia).exists()) {
        const QString dstMedia = QDir::homePath() + "/customizationMainFolder";
        QDir(dstMedia).removeRecursively();
        if (!CopyDirRecursive(srcMedia, dstMedia)) {
            qDebug() << "ImportFullBackup: media copy failed (non-fatal)";
        }
    }

    // Replace JSON config files (mavjud bo'lsa).
    const QString appDataCustom = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/CustomMod";
    QDir().mkpath(appDataCustom);
    const QString srcPeerLists = sourceDir + "/peer_lists.json";
    const QString srcBranding = sourceDir + "/branding.json";
    if (QFile::exists(srcPeerLists)) {
        QFile::remove(appDataCustom + "/peer_lists.json");
        QFile::copy(srcPeerLists, appDataCustom + "/peer_lists.json");
    }
    if (QFile::exists(srcBranding)) {
        QFile::remove(appDataCustom + "/branding.json");
        QFile::copy(srcBranding, appDataCustom + "/branding.json");
    }

    // Registry import (Windows-specific).
    #ifdef Q_OS_WIN
    {
        const QString srcReg = sourceDir + "/settings.reg";
        if (QFile::exists(srcReg)) {
            const QString regCmd = QString(
                "reg import \"%1\""
            ).arg(QDir::toNativeSeparators(srcReg));
            QProcess regProc;
            regProc.start("cmd.exe", {"/c", regCmd});
            regProc.waitForFinished(10000);
            if (regProc.exitCode() != 0) {
                qDebug() << "ImportFullBackup: registry import failed:"
                         << regProc.readAllStandardError();
                // Non-fatal — import davom etadi.
            }
        }
    }
    #endif

    if (!tempExtractDir.isEmpty()) {
        QDir(tempExtractDir).removeRecursively();
    }

    // Refresh in-memory caches.
    LoadRestoreCache();

    // Notify open history views to reload without a restart.
    if (gReloadCallback) {
        QTimer::singleShot(0, []() { gReloadCallback(); });
    }

    return true;
}

// ---------------------------------------------------------------------------
// Archive stats / clear
// ---------------------------------------------------------------------------

ArchiveStats GetArchiveStats() {
    Init();
    ArchiveStats stats;
    if (!gDb) return stats;

    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "SELECT COUNT(*) FROM actioned_messages WHERE type='deleted'",
                -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW)
                stats.deletedCount = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
    }
    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "SELECT COUNT(*) FROM actioned_messages WHERE type='edited' OR type='backup'",
                -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW)
                stats.editedCount = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
    }
    return stats;
}

void ClearDeletedArchive() {
    Init();
    execSql("DELETE FROM actioned_messages WHERE type='deleted'");
    {
        QMutexLocker locker(&gCacheMutex);
        gDeletedCache.clear();
    }
}

void ClearEditedArchive() {
    Init();
    execSql("DELETE FROM actioned_messages WHERE type='edited' OR type='backup'");
    {
        QMutexLocker locker(&gCacheMutex);
        gEditedCache.clear();
    }
}

void ClearAllArchive() {
    Init();
    execSql("DELETE FROM actioned_messages");
    {
        QMutexLocker locker(&gCacheMutex);
        gDeletedCache.clear();
        gEditedCache.clear();
    }
}

// ---------------------------------------------------------------------------
// Media file saving
// ---------------------------------------------------------------------------

QString SaveMediaFile(const QString &sourcePath, const QString &type) {
    if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) return "";

    // E21: Media path sanitization — reject names that escape the target dir.
    const QString rawName = QFileInfo(sourcePath).fileName();
    QString safeFileName = rawName;
    safeFileName.remove(u'/').remove(u'\\');
    if (safeFileName.isEmpty()
            || safeFileName.startsWith(u"..")
            || safeFileName.contains(u"..")) {
        qDebug() << "SaveMediaFile: rejected unsafe filename:" << rawName;
        return "";
    }
    if (safeFileName.length() > 200) {
        const QString ext = QFileInfo(safeFileName).suffix();
        safeFileName = safeFileName.left(190)
            + (ext.isEmpty() ? QString() : u"."_q + ext);
    }

    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/customizationMainFolder";
    QString subDir = "medias/files";
    if (type == "image")      subDir = "medias/images";
    else if (type == "video") subDir = "medias/videos";
    else if (type == "voice") subDir = "medias/voices";
    const QString fullPath = baseDir + "/" + subDir;
    QDir().mkpath(fullPath);

    const QString targetPath = fullPath + "/" + safeFileName;
    const QString canonicalBase   = QDir(fullPath).canonicalPath();
    const QString canonicalTarget = QFileInfo(targetPath).absolutePath();
    if (!canonicalTarget.startsWith(canonicalBase)) {
        qDebug() << "SaveMediaFile: path escapes target dir — blocked:" << targetPath;
        return "";
    }

    if (QFile::exists(targetPath)) return targetPath;
    if (QFile::copy(sourcePath, targetPath)) return targetPath;
    return "";
}

// ---------------------------------------------------------------------------
// Auto backup
// ---------------------------------------------------------------------------

static void RunAutoBackup() {
    const QString backupRoot = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/CustomMod/AutoBackups";
    QDir().mkpath(backupRoot);

    const QString result = ExportFullBackup(backupRoot);
    if (result.isEmpty()) {
        qDebug() << "AutoBackup: export failed";
        return;
    }
    qDebug() << "AutoBackup: saved to" << result;

    // Keep only the 3 most recent backups.
    QDir dir(backupRoot);
    QFileInfoList entries = dir.entryInfoList(
        {"CustomModBackup_*.zip"}, QDir::Files, QDir::Time);
    while (entries.size() > 3) {
        QFile::remove(entries.last().filePath());
        entries.removeLast();
    }
}

void SetImportReloadCallback(ReloadCallback cb) {
    gReloadCallback = std::move(cb);
}

void StartAutoBackup() {
    // Run once at startup after a short delay so DB is fully ready.
    QTimer::singleShot(5000, []() { RunAutoBackup(); });

    // Then repeat every 24 hours.
    static QTimer *autoTimer = nullptr;
    if (!autoTimer) {
        autoTimer = new QTimer();
        autoTimer->setInterval(24 * 60 * 60 * 1000);
        QObject::connect(autoTimer, &QTimer::timeout, []() { RunAutoBackup(); });
        autoTimer->start();
    }
}

} // namespace CustomDB
