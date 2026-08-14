#include "custom_db.h"
#include "custom_settings.h"
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
#include <QtCore/QJsonArray>
#include <QtCore/QSysInfo>
#include "history/history_item.h"
#include "history/history.h"
#include "data/data_peer.h"
#include "crl/crl.h"

namespace CustomDB {

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

static sqlite3 *gDb = nullptr;

// In-memory caches for O(1) lookups, populated lazily per-peer (see
// EnsurePeerCacheLoaded()) rather than in bulk for the whole archive at
// startup — with 200k+ rows a full-table load blocked the UI for seconds.
// gCacheMutex guards both caches against concurrent reads/writes from
// background threads (e.g. MTProto callbacks vs. UI thread).
static QMutex gCacheMutex;
static QHash<QString, QSet<long long>> gDeletedCache;
static QHash<QString, QHash<long long, QString>> gEditedCache;
// Which peers have already had their entries loaded from the DB into the
// caches above (via EnsurePeerCacheLoaded()). A peer can also be "loaded"
// implicitly by live writes (SaveMessage()/MarkEdited()) before it's ever
// explicitly loaded; EnsurePeerCacheLoaded() merges rather than overwrites,
// so that's safe either way.
static QSet<QString> gLoadedPeers;
// Activity History Log: peerId -> field -> latest new_value. Same lazy
// per-peer-load pattern as gDeletedCache/gEditedCache above (see
// EnsureActivityCacheLoaded below) — added to fix a first-start
// performance regression (repeated per-field SQLite reads with no cache).
static QHash<QString, QHash<QString, QString>> gActivityLatestCache;
static QSet<QString> gActivityLoadedPeers;

// A13/perf: qaysi peer'larda UMUMAN 'deleted' yozuvi bor. Bitta DISTINCT
// so'rov bilan bir marta to'ldiriladi (idx_am_peer_type indeksidan foydalanadi).
//
// Nima uchun: GetDeletedMessages() History::loadDeletedMessages() dan
// chaqiriladi, u esa addOlderSlice/addNewerSlice ichida — ya'ni har bir
// scroll bo'lagida. Chatlarning aksariyatida o'chirilgan xabar yo'q, lekin
// har safar baribir SQLite so'rovi ketardi. Bu to'plam "ehtimol bor" filtri:
// peer bu yerda bo'lmasa, so'rov aniq bo'sh qaytadi — demak bemalol
// o'tkazib yuborsa bo'ladi.
static QSet<QString> gPeersWithDeleted;
static bool gPeersWithDeletedLoaded = false;

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

    // Activity History Log: kontaktlarning ism/username/rasm/last-seen
    // o'zgarishlari — faqat ilova legal ravishda qabul qilgan ma'lumot
    // (see custom_activity_history.cpp for the capture logic).
    execSql("CREATE TABLE IF NOT EXISTS activity_history ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "peer_id TEXT NOT NULL, "
            "field TEXT NOT NULL, "
            "old_value TEXT, "
            "new_value TEXT, "
            "observed_at INTEGER NOT NULL)");
    execSql("CREATE INDEX IF NOT EXISTS idx_activity_history_peer "
            "ON activity_history(peer_id, observed_at DESC)");

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

    // v5 → v6 (A13/D4): text_cache ga is_archived ustuni.
    //   0 = vaqtinchalik cache (eskirsa PruneStaleCachedText o'chirishi mumkin)
    //   1 = doimiy arxiv (hech qachon avtomatik o'chirilmaydi)
    // Butun-chat o'chirilishida faqat arxivdagi xabarlar qutqariladi, shuning
    // uchun arxivni 30 kunlik TTL dan ajratish shart edi.
    if (version < 6) {
        execSql("ALTER TABLE text_cache ADD COLUMN is_archived INTEGER DEFAULT 0");
    }

    // v6 → v7: media_index jadvali.
    //
    // Nima uchun kerak: 2026-08-14 gacha saqlanmagan media haqida HECH
    // QANDAY iz qolmasdi — "bunday fayl bor edi" ma'lumoti ham yo'q edi.
    // Endi har bir media xabar shu yerda qayd etiladi, hatto fayl
    // yuklanmagan bo'lsa ham (status='pending'/'missing').
    //
    // Nima uchun alohida fayl emas, balki JADVAL: baza eksportga
    // allaqachon kiradi (ExportFullBackup 1-qadam), demak "indeks doim
    // eksport qilinadi" talabi qo'shimcha ishsiz bajariladi.
    //
    // sha256 — Track C (customsync-server) da blob identifikatsiyasi va
    // dublikatlarni aniqlash uchun; faqat status='present' bo'lganda
    // hisoblanadi (katta fayl uchun qimmat operatsiya).
    if (version < 7) {
        execSql(
            "CREATE TABLE IF NOT EXISTS media_index ("
            "    peer_id     TEXT    NOT NULL,"
            "    msg_id      INTEGER NOT NULL,"
            "    kind        TEXT    NOT NULL,"   // image | video | voice | file
            "    file_name   TEXT,"
            "    rel_path    TEXT,"               // arxiv ildizidan nisbiy
            "    size        INTEGER NOT NULL DEFAULT 0,"
            "    sha256      TEXT,"
            "    msg_date    INTEGER,"
            "    archived_at INTEGER,"
            "    layer       TEXT,"               // l1 | l2 | l3
            "    status      TEXT    NOT NULL,"   // present | pending | missing
            "    reason      TEXT,"
            "    PRIMARY KEY (peer_id, msg_id))");
        execSql("CREATE INDEX IF NOT EXISTS idx_mi_status ON media_index(status)");
        execSql("CREATE INDEX IF NOT EXISTS idx_mi_peer   ON media_index(peer_id)");
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
    QMutexLocker locker(&gCacheMutex);
    gDeletedCache.clear();
    gEditedCache.clear();
    gLoadedPeers.clear();
    gActivityLatestCache.clear();
    gActivityLoadedPeers.clear();
    gPeersWithDeleted.clear();
    gPeersWithDeletedLoaded = false;
}

// A13/perf: gPeersWithDeleted ni bir marta to'ldiradi. EnsurePeerCacheLoaded
// bilan bir xil naqsh — DB o'qish qulfsiz, so'ng qulflab yozish.
static void EnsurePeersWithDeletedLoaded() {
    {
        QMutexLocker locker(&gCacheMutex);
        if (gPeersWithDeletedLoaded) return;
    }
    Init();
    QSet<QString> peers;
    if (gDb) {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "SELECT DISTINCT peer_id FROM actioned_messages "
                "WHERE type = 'deleted'",
                -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const auto peerId = colText(stmt, 0);
                if (!peerId.isEmpty()) {
                    peers.insert(peerId);
                }
            }
            sqlite3_finalize(stmt);
        }
    }
    QMutexLocker locker(&gCacheMutex);
    // unite() — o'qish davomida MarkDeleted() qo'shgan peer yo'qolmasin.
    gPeersWithDeleted.unite(peers);
    gPeersWithDeletedLoaded = true;
}

// Loads one peer's deleted/edited records from DB into the caches, unless
// already loaded. Called lazily from the read/write sites below instead of
// eagerly for the whole archive at startup (see LoadRestoreCache()).
static void EnsurePeerCacheLoaded(const QString &peerId) {
    {
        QMutexLocker locker(&gCacheMutex);
        if (gLoadedPeers.contains(peerId)) return;
    }
    Init();
    if (!gDb) return;

    QSet<long long> deletedIds;
    QHash<long long, QString> editedTexts;

    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "SELECT msg_id FROM actioned_messages WHERE peer_id = ? AND type = 'deleted'",
                -1, &stmt, nullptr) == SQLITE_OK) {
            bindText(stmt, 1, peerId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                deletedIds.insert(sqlite3_column_int64(stmt, 0));
            }
            sqlite3_finalize(stmt);
        }
    }
    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "SELECT msg_id, original_text FROM actioned_messages "
                "WHERE peer_id = ? AND type IN ('backup', 'edited') ORDER BY rowid ASC",
                -1, &stmt, nullptr) == SQLITE_OK) {
            bindText(stmt, 1, peerId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const long long msgId = sqlite3_column_int64(stmt, 0);
                if (!editedTexts.contains(msgId)) {
                    editedTexts[msgId] = colText(stmt, 1);
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    QMutexLocker locker(&gCacheMutex);
    if (gLoadedPeers.contains(peerId)) return; // race guard
    gDeletedCache[peerId].unite(deletedIds);
    for (auto it = editedTexts.constBegin(); it != editedTexts.constEnd(); ++it) {
        if (!gEditedCache[peerId].contains(it.key())) {
            gEditedCache[peerId][it.key()] = it.value();
        }
    }
    gLoadedPeers.insert(peerId);
}

// ---------------------------------------------------------------------------
// Fast O(1) cache lookups
// ---------------------------------------------------------------------------

bool IsDeletedLocally(const QString &peerId, long long msgId) {
    EnsurePeerCacheLoaded(peerId);
    QMutexLocker locker(&gCacheMutex);
    const auto it = gDeletedCache.constFind(peerId);
    return it != gDeletedCache.constEnd() && it->contains(msgId);
}

QString GetOriginalTextBeforeEdit(const QString &peerId, long long msgId) {
    EnsurePeerCacheLoaded(peerId);
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
        // A13/perf: quyidagi UPDATE yo'li SaveActionedMessage() ga
        // bormasligi mumkin — filtrni shu yerda ham yangilab qo'yamiz.
        gPeersWithDeleted.insert(peerId);
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

    // A13/perf: bu peer'da o'chirilgan xabar umuman bo'lmasa, SQLite'ga
    // bormaymiz. Bu issiq yo'l — har bir scroll bo'lagida chaqiriladi.
    EnsurePeersWithDeletedLoaded();
    {
        QMutexLocker locker(&gCacheMutex);
        if (!gPeersWithDeleted.contains(peerId)) return result;
    }

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

    // A13/perf: 'deleted' yozuvlarning YAGONA insert nuqtasi shu yer
    // (MarkDeleted ham shu orqali o'tadi) — filtr to'plamini shu yerda
    // yangilaymiz, aks holda yangi o'chirilgan xabar o'qilmay qolardi.
    if (msg.type == "deleted" && !msg.peerId.isEmpty()) {
        QMutexLocker locker(&gCacheMutex);
        gPeersWithDeleted.insert(msg.peerId);
    }

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
    EnsurePeerCacheLoaded(msg.peerId);
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
            // A13/D4: arxivlangan qatorlarga TEGMAYMIZ — ular doimiy.
            // COALESCE eski (migratsiyadan oldingi) NULL qiymatlar uchun.
            "DELETE FROM text_cache "
            "WHERE cached_at < ? AND COALESCE(is_archived, 0) = 0",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, cutoff);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

QVector<QString> GetPeersWithDeletedMessages() {
    // A13/perf: aynan shu ro'yxat gPeersWithDeleted filtrini to'ldiradi —
    // takroriy DISTINCT so'rov o'rniga o'sha keshni qaytaramiz.
    EnsurePeersWithDeletedLoaded();
    QMutexLocker locker(&gCacheMutex);
    QVector<QString> result;
    result.reserve(gPeersWithDeleted.size());
    for (const auto &peerId : gPeersWithDeleted) {
        result.append(peerId);
    }
    return result;
}

// ── Media indeks (schema v7) ─────────────────────────────────────────────
//
// 7.1-eslatma (sha256): hozircha bo'sh qoldiriladi. 100 MB fayl uchun
// SHA-256 ~0.3 soniya oladi va bu arxivlash paytida UI oqimida bo'lardi.
// Ustun sxemada bor, shuning uchun keyinchalik fonda to'ldirish mumkin
// (Track C blob identifikatsiyasi kerak bo'lganda).

void UpsertMediaIndex(const MediaIndexEntry &entry) {
    Init();
    if (!gDb || entry.peerId.isEmpty()) return;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "INSERT INTO media_index "
            "(peer_id, msg_id, kind, file_name, rel_path, size, sha256, "
            " msg_date, archived_at, layer, status, reason) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(peer_id, msg_id) DO UPDATE SET "
            "  kind=excluded.kind,"
            "  file_name=excluded.file_name,"
            "  rel_path=excluded.rel_path,"
            "  size=excluded.size,"
            // sha256: mavjud qiymat saqlanadi, yangisi bo'sh bo'lsa yo'qotmaymiz
            "  sha256=CASE WHEN LENGTH(excluded.sha256)>0 "
            "              THEN excluded.sha256 ELSE sha256 END,"
            "  msg_date=excluded.msg_date,"
            "  archived_at=excluded.archived_at,"
            "  layer=excluded.layer,"
            "  status=excluded.status,"
            "  reason=excluded.reason",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, entry.peerId);
        sqlite3_bind_int64(stmt, 2, entry.msgId);
        bindText(stmt, 3, entry.kind);
        bindText(stmt, 4, entry.fileName);
        bindText(stmt, 5, entry.relPath);
        sqlite3_bind_int64(stmt, 6, entry.size);
        bindText(stmt, 7, entry.sha256);
        sqlite3_bind_int64(stmt, 8, entry.msgDate);
        sqlite3_bind_int64(stmt, 9, entry.archivedAt);
        bindText(stmt, 10, entry.layer);
        bindText(stmt, 11, entry.status);
        bindText(stmt, 12, entry.reason);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void SetMediaIndexStatus(
        const QString &peerId,
        long long msgId,
        const QString &status,
        const QString &reason) {
    Init();
    if (!gDb) return;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "UPDATE media_index SET status = ?, reason = ? "
            "WHERE peer_id = ? AND msg_id = ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, status);
        bindText(stmt, 2, reason);
        bindText(stmt, 3, peerId);
        sqlite3_bind_int64(stmt, 4, msgId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

bool HasPresentMediaIndexEntry(const QString &peerId, long long msgId) {
    Init();
    if (!gDb) return false;
    bool found = false;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT 1 FROM media_index "
            "WHERE peer_id = ? AND msg_id = ? AND status = 'present' LIMIT 1",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, msgId);
        found = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }
    return found;
}

QVector<MediaPeerSummary> GetMediaPeerSummaries() {
    Init();
    QVector<MediaPeerSummary> result;
    if (!gDb) return result;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT peer_id, COUNT(*), COALESCE(SUM(size), 0) "
            "FROM media_index WHERE status = 'present' "
            "GROUP BY peer_id ORDER BY SUM(size) DESC",
            -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MediaPeerSummary s;
            s.peerId = colText(stmt, 0);
            s.fileCount = sqlite3_column_int(stmt, 1);
            s.totalBytes = sqlite3_column_int64(stmt, 2);
            if (!s.peerId.isEmpty()) result.append(s);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

long long TotalArchivedMediaBytes() {
    Init();
    if (!gDb) return 0;
    long long total = 0;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT COALESCE(SUM(size), 0) FROM media_index "
            "WHERE status = 'present'",
            -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return total;
}

int ReconcileMediaIndex(const QString &archiveRoot) {
    Init();
    if (!gDb) return 0;

    // Avval barcha 'present' yozuvlarni o'qiymiz, so'ng fayl tizimini
    // tekshiramiz — SELECT davomida UPDATE qilish SQLite'da nozik.
    struct Row { QString peerId; long long msgId; QString relPath; QString status; };
    QVector<Row> rows;
    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "SELECT peer_id, msg_id, rel_path, status FROM media_index "
                "WHERE status IN ('present', 'pending')",
                -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                rows.append({
                    colText(stmt, 0),
                    sqlite3_column_int64(stmt, 1),
                    colText(stmt, 2),
                    colText(stmt, 3) });
            }
            sqlite3_finalize(stmt);
        }
    }

    // Ikki tomonlama moslashtirish — indeks o'zini-o'zi tuzatadi:
    //   present + fayl yo'q  → missing  (import'dan keyin, yoki fayl
    //                                    qo'lda o'chirilgan bo'lsa)
    //   pending + fayl bor   → present  (yuklash tugagan, lekin
    //                                    finishLoad hook'i o'tkazib
    //                                    yuborgan — masalan ilova
    //                                    yuklash oxirida yopilgan)
    int changed = 0;
    execSql("BEGIN");
    for (const auto &row : rows) {
        const auto exists = !row.relPath.isEmpty()
            && QFile::exists(archiveRoot + "/" + row.relPath);
        if (row.status == "present" && !exists) {
            SetMediaIndexStatus(
                row.peerId, row.msgId, u"missing"_q, u"file_not_found"_q);
            ++changed;
        } else if (row.status == "pending" && exists) {
            SetMediaIndexStatus(
                row.peerId, row.msgId, u"present"_q, QString());
            ++changed;
        }
    }
    execSql("COMMIT");
    return changed;
}

void Checkpoint() {
    Init();
    if (!gDb) return;
    // A13/D5: WAL faylini asosiy DB ga ko'chiradi. journal_mode=WAL +
    // synchronous=NORMAL da to'satdan tok o'chsa, oxirgi checkpoint'dan
    // keyingi tranzaksiyalar yo'qolishi mumkin — muntazam chaqirish shu
    // oynani daqiqalargacha qisqartiradi, yozuv tezligini esa saqlaydi
    // (synchronous=FULL ga o'tish hamma yozuvni sekinlashtirardi).
    execSql("PRAGMA wal_checkpoint(TRUNCATE)");
}

void ExecRaw(const char *sql) {
    Init();
    if (!gDb) return;
    execSql(sql);
}

qint64 DatabaseSizeBytes() {
    return QFileInfo(dbFilePath()).size();
}

int ArchivedMessageCount() {
    Init();
    if (!gDb) return 0;
    int result = 0;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT COUNT(*) FROM text_cache "
            "WHERE COALESCE(is_archived, 0) = 1",
            -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

void CacheMessageText(
        const QString &peerId,
        long long msgId,
        const QString &text,
        bool isOut,
        unsigned int msgDate,
        const QString &senderId,
        bool isMedia,
        bool archived) {
    Init();
    if (!gDb || peerId.isEmpty() || msgId == 0) return;
    // Matn bo'sh: faqat media xabar bo'lsa cache qilamiz (delete ni bilish uchun).
    // Aks holda (na matn, na media) — yozishdan ma'no yo'q.
    if (text.isEmpty() && !isMedia) return;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "INSERT OR REPLACE INTO text_cache "
            "(peer_id, msg_id, text, is_out, msg_date, cached_at, "
            "sender_id, is_media, is_archived) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, msgId);
        bindText(stmt, 3, text);
        sqlite3_bind_int(stmt, 4, isOut ? 1 : 0);
        sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(msgDate));
        sqlite3_bind_int64(stmt, 6, QDateTime::currentSecsSinceEpoch());
        bindText(stmt, 7, senderId);
        sqlite3_bind_int(stmt, 8, isMedia ? 1 : 0);
        sqlite3_bind_int(stmt, 9, archived ? 1 : 0);
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
    EnsurePeerCacheLoaded(peerId);
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

// Like CopyDirRecursive, but for merging a restored backup's media into the
// current device's media folder: files that already exist locally are left
// untouched instead of being overwritten. Media filenames are
// content-addressed (peerId_msgId or documentId based, see SaveMediaFile()/
// setDeletedLocally()), so a name collision means it's the same file —
// skipping is both safe and avoids needless I/O.
static bool MergeDirRecursive(const QString &src, const QString &dst) {
    QDir srcDir(src);
    if (!srcDir.exists()) return true;
    QDir().mkpath(dst);
    for (const QFileInfo &info : srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString targetPath = dst + "/" + info.fileName();
        if (info.isDir()) {
            if (!MergeDirRecursive(info.filePath(), targetPath)) return false;
        } else if (!QFile::exists(targetPath)) {
            if (!QFile::copy(info.filePath(), targetPath)) return false;
        }
    }
    return true;
}

// Unions whitelist/blacklist entries (by "id") from an imported
// peer_lists.json into the current device's copy, instead of overwriting it
// — otherwise per-peer AntiDelete overrides configured only on this device
// would be lost on restore. Safe no-op if either file is missing/invalid.
static void MergePeerListsJson(const QString &srcPath, const QString &dstPath) {
    QFile srcFile(srcPath);
    if (!srcFile.open(QIODevice::ReadOnly)) return;
    const auto srcRoot = QJsonDocument::fromJson(srcFile.readAll()).object();
    srcFile.close();

    QJsonObject dstRoot;
    QFile dstFile(dstPath);
    if (dstFile.open(QIODevice::ReadOnly)) {
        dstRoot = QJsonDocument::fromJson(dstFile.readAll()).object();
        dstFile.close();
    }

    const auto mergeArray = [](const QJsonArray &a, const QJsonArray &b) {
        QJsonArray result = a;
        QSet<QString> seenIds;
        for (const auto &v : a) {
            seenIds.insert(v.toObject().value(QStringLiteral("id")).toString());
        }
        for (const auto &v : b) {
            const auto id = v.toObject().value(QStringLiteral("id")).toString();
            if (!seenIds.contains(id)) {
                result.append(v);
                seenIds.insert(id);
            }
        }
        return result;
    };

    QJsonObject merged;
    merged[QStringLiteral("whitelist")] = mergeArray(
        dstRoot.value(QStringLiteral("whitelist")).toArray(),
        srcRoot.value(QStringLiteral("whitelist")).toArray());
    merged[QStringLiteral("blacklist")] = mergeArray(
        dstRoot.value(QStringLiteral("blacklist")).toArray(),
        srcRoot.value(QStringLiteral("blacklist")).toArray());

    if (dstFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        dstFile.write(QJsonDocument(merged).toJson(QJsonDocument::Indented));
    }
}

QJsonArray MediaIndexToJson() {
    Init();
    QJsonArray array;
    if (!gDb) return array;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT peer_id, msg_id, kind, file_name, rel_path, size, "
            "       sha256, msg_date, archived_at, layer, status, reason "
            "FROM media_index ORDER BY peer_id, msg_id",
            -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            QJsonObject o;
            o["peerId"] = colText(stmt, 0);
            // msgId JSON'da STRING — 64-bitli qiymat JSON number'da
            // aniqlikni yo'qotishi mumkin (JavaScript 2^53 chegarasi),
            // server tomoni ham shu formatni kutadi.
            o["msgId"] = QString::number(sqlite3_column_int64(stmt, 1));
            o["kind"] = colText(stmt, 2);
            o["fileName"] = colText(stmt, 3);
            o["relPath"] = colText(stmt, 4);
            o["size"] = QString::number(sqlite3_column_int64(stmt, 5));
            o["sha256"] = colText(stmt, 6);
            o["msgDate"] = double(sqlite3_column_int64(stmt, 7));
            o["archivedAt"] = double(sqlite3_column_int64(stmt, 8));
            o["layer"] = colText(stmt, 9);
            o["status"] = colText(stmt, 10);
            o["reason"] = colText(stmt, 11);
            array.append(o);
        }
        sqlite3_finalize(stmt);
    }
    return array;
}

namespace {

// ZIP qilish — PowerShell'ga bog'liqlik ATAYLAB shu yagona funksiyada
// yakkalangan. Format boshqa platformaga yoki server tomoniga
// ko'chirilganda faqat shu joyni almashtirish kifoya; qolgan eksport
// mantig'i platformadan mustaqil.
[[nodiscard]] bool ZipDirectory(
        const QString &sourceDir,
        const QString &zipPath) {
    const QString psCmd = QString(
        "Compress-Archive -Path '%1\\*' -DestinationPath '%2' -Force"
    ).arg(QDir::toNativeSeparators(sourceDir), QDir::toNativeSeparators(zipPath));
    QProcess ps;
    ps.start("powershell.exe", {"-NonInteractive", "-Command", psCmd});
    // Media arxivi GB'lar bo'lishi mumkin — 30 soniya yetmaydi.
    ps.waitForFinished(30 * 60 * 1000);
    if (ps.exitCode() != 0 || !QFile::exists(zipPath)) {
        qDebug() << "ZipDirectory: PowerShell compress failed:"
                 << ps.readAllStandardError();
        return false;
    }
    return true;
}

} // namespace

ExportResult ExportFullBackup(
        const QString &targetDir,
        const ExportOptions &options,
        const ExportProgressCallback &onProgress) {
    Init();
    ExportResult result;
    if (!gDb) return result;
    FlushPendingWrites();

    const auto reportProgress = [&](const QString &stage, int percent) {
        if (onProgress) onProgress(stage, percent);
    };
    reportProgress(u"Tayyorlanmoqda"_q, 0);

    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString zipPath = targetDir + "/CustomModBackup_" + stamp + ".zip";
    const QString stageDir = targetDir + "/CustomModBackup_" + stamp + "_tmp";
    if (!QDir().mkpath(stageDir)) return result;

    // Checkpoint WAL so the DB file on disk is complete.
    sqlite3_wal_checkpoint_v2(gDb, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);

    // ── BOSQICH 1: asosiy arxiv (media YO'Q, shuning uchun tez) ──────────

    // 1) DB.
    reportProgress(u"Baza nusxalanmoqda"_q, 10);
    if (!QFile::copy(dbFilePath(), stageDir + "/actioned_messages.db")) {
        QDir(stageDir).removeRecursively();
        return result;
    }

    // 2) JSON sozlamalar fayllari (mavjud bo'lsa).
    reportProgress(u"Sozlamalar saqlanmoqda"_q, 60);
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

    // 3) settings.json — KANONIK sozlamalar shakli (2026-08-14).
    //    Quyidagi settings.reg Windows registry dump'i bo'lib, Linux/macOS
    //    va server tomonida o'qib bo'lmaydi. Format boshqa ilovalarimizda
    //    ham ishlatilgani uchun platformadan mustaqil shakl kerak.
    //    Import shu faylni afzal ko'radi; .reg faqat orqaga moslik uchun.
    reportProgress(u"Sozlamalar (JSON) yozilmoqda"_q, 45);
    {
        QFile f(stageDir + "/settings.json");
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(QJsonDocument(CustomSettings::ExportToJson())
                .toJson(QJsonDocument::Indented));
        }
    }

    // 4) index.json — media indeksi (2026-08-14).
    //    Baza ichida ham bor, lekin JSON shakli server/boshqa ilovalar
    //    uchun: ular SQLite drayveri va bizning ichki sxemamizni bilishi
    //    shart bo'lmasin. Media fayllarsiz eksportda ham SHU FAYL bo'ladi
    //    — "qanday media bor edi" ma'lumoti har doim uzatiladi.
    reportProgress(u"Media indeksi yozilmoqda"_q, 55);
    {
        QFile f(stageDir + "/index.json");
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(QJsonDocument(MediaIndexToJson())
                .toJson(QJsonDocument::Indented));
        }
    }

    // 5) Registry export — Windows-specific, ORQAGA MOSLIK uchun.
    //    Yangi import yo'li settings.json ni ishlatadi.
    reportProgress(u"Registry eksport qilinmoqda"_q, 70);
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

    // ── BOSQICH 2: media fayllarni tanlab yig'ish ───────────────────────
    //
    // Tanlangan chatlar fayllari media_index dagi rel_path bo'yicha
    // topiladi. ALOHIDA staging papkasi — asosiy arxiv media'siz
    // bo'lishi kerak (ZIP yaratilgach unga qo'shib bo'lmaydi, shuning
    // uchun bo'linish).
    const QString mediaStageDir = targetDir
        + "/CustomModMedia_" + stamp + "_tmp";
    const QString mediaZipPath = targetDir
        + "/CustomModMedia_" + stamp + ".zip";
    const auto archiveRoot = QDir::homePath() + "/customizationMainFolder";
    auto mediaFileCount = 0;

    const auto wantsMedia = options.includeAllMedia
        || !options.mediaPeerIds.isEmpty();
    if (wantsMedia) {
        reportProgress(u"Media fayllar tanlanmoqda"_q, 78);
        const auto selected = QSet<QString>(
            options.mediaPeerIds.begin(),
            options.mediaPeerIds.end());
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb,
                "SELECT peer_id, rel_path FROM media_index "
                "WHERE status = 'present' AND LENGTH(rel_path) > 0",
                -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const auto peerId = colText(stmt, 0);
                if (!options.includeAllMedia && !selected.contains(peerId)) {
                    continue;
                }
                const auto relPath = colText(stmt, 1);
                const auto src = archiveRoot + "/" + relPath;
                if (!QFile::exists(src)) {
                    continue; // ReconcileMediaIndex keyinroq 'missing' qiladi
                }
                const auto dst = mediaStageDir + "/" + relPath;
                QDir().mkpath(QFileInfo(dst).absolutePath());
                if (QFile::copy(src, dst)) {
                    result.mediaBytes += QFileInfo(src).size();
                    ++mediaFileCount;
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    // 6) Manifest — backup metadata (v3).
    reportProgress(u"Manifest yaratilmoqda"_q, 84);
    {
        QJsonObject manifest;
        manifest["version"] = 3;
        manifest["createdAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        manifest["sourceHost"] = QSysInfo::machineHostName();
        manifest["os"] = QSysInfo::prettyProductName();
        manifest["hasRegistry"] = QFile::exists(stageDir + "/settings.reg");
        manifest["hasSettingsJson"] = QFile::exists(stageDir + "/settings.json");
        manifest["hasIndexJson"] = QFile::exists(stageDir + "/index.json");
        manifest["hasPeerLists"] = QFile::exists(stageDir + "/peer_lists.json");
        manifest["hasBranding"] = QFile::exists(stageDir + "/branding.json");

        // v2 dagi `hasMedia` "media yo'q edi" va "media ataylab chiqarib
        // tashlandi" holatlarini AJRATA OLMASDI. v3 da ikkita alohida
        // maydon: nima so'ralgani va nima chiqqani.
        manifest["mediaRequested"] = wantsMedia;
        manifest["mediaIncluded"] = (mediaFileCount > 0);
        manifest["mediaArchive"] = (mediaFileCount > 0)
            ? QFileInfo(mediaZipPath).fileName()
            : QString();
        manifest["mediaTotalBytes"] = double(result.mediaBytes);
        manifest["mediaFileCount"] = mediaFileCount;
        // Media qaysi chatlar uchun so'ralgani — import tomoni nima
        // kutishini bilsin.
        if (!options.includeAllMedia && !options.mediaPeerIds.isEmpty()) {
            QJsonArray peers;
            for (const auto &id : options.mediaPeerIds) peers.append(id);
            manifest["mediaPeerIds"] = peers;
        }

        QJsonObject counts;
        const auto stats = GetArchiveStats();
        counts["deleted"] = stats.deletedCount;
        counts["edited"] = stats.editedCount;
        const auto indexCount = [](const char *status) {
            auto n = 0;
            sqlite3_stmt *s = nullptr;
            if (sqlite3_prepare_v2(gDb,
                    "SELECT COUNT(*) FROM media_index WHERE status = ?",
                    -1, &s, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(s, 1, status, -1, SQLITE_STATIC);
                if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
                sqlite3_finalize(s);
            }
            return n;
        };
        counts["indexPresent"] = indexCount("present");
        counts["indexPending"] = indexCount("pending");
        counts["indexMissing"] = indexCount("missing");
        manifest["counts"] = counts;

        QFile mf(stageDir + "/manifest.json");
        if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
        }
    }

    // 7) ZIP: avval ASOSIY arxiv (tez), so'ng media (uzoq).
    reportProgress(u"Asosiy arxiv yaratilmoqda"_q, 88);
    const auto mainOk = ZipDirectory(stageDir, zipPath);
    QDir(stageDir).removeRecursively();
    if (!mainOk) {
        QDir(mediaStageDir).removeRecursively();
        return result;
    }
    result.mainZipPath = zipPath;

    if (mediaFileCount > 0) {
        reportProgress(u"Media arxivi yaratilmoqda"_q, 94);
        if (ZipDirectory(mediaStageDir, mediaZipPath)) {
            result.mediaZipPath = mediaZipPath;
        }
        // Media ZIP muvaffaqiyatsiz bo'lsa ham asosiy arxiv haqiqiy —
        // shuning uchun result.mainZipPath saqlanadi.
    }
    QDir(mediaStageDir).removeRecursively();

    reportProgress(u"Tayyor"_q, 100);
    return result;
}

bool ImportFullBackup(const QString &sourcePath, bool fullReplace) {
    Init();
    if (!gDb) return false;

    // Full-replace mode: wipe the existing archive BEFORE the merge step
    // below runs. The merge logic (INSERT ... WHERE NOT EXISTS) then simply
    // inserts everything from the backup, since nothing pre-exists to
    // collide with — no separate "replace" code path needed.
    if (fullReplace) {
        ClearAllArchive();
        execSql("DELETE FROM ghost_reads");
    }

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

    // E28/Vazifa 2.3: MERGE the imported archive into the live DB instead of
    // replacing it outright. A straight replace loses whatever accumulated
    // on THIS device since its own last backup — e.g. restoring a laptop
    // backup onto a home-pc used to wipe out the home-pc's own history. See
    // docs/superpowers/plans/2026-07-10-custom-mod-v2-improvements.md.
    //
    // actioned_messages is append-only by design — multiple 'edited' rows
    // per message are meaningful edit history, not duplicates to collapse —
    // so "merge" here means append rows from the import that don't already
    // have an identical counterpart, not a timestamp/"newest wins" replace
    // (which would silently drop genuine history unique to either device).
    bool mergeOk = true;
    {
        sqlite3_stmt *attachStmt = nullptr;
        if (sqlite3_prepare_v2(gDb, "ATTACH DATABASE ? AS import_db", -1, &attachStmt, nullptr) == SQLITE_OK) {
            bindText(attachStmt, 1, srcDb);
            if (sqlite3_step(attachStmt) != SQLITE_DONE) mergeOk = false;
            sqlite3_finalize(attachStmt);
        } else {
            mergeOk = false;
        }
    }

    if (mergeOk) {
        execSql("BEGIN");
        execSql(
            "INSERT INTO main.actioned_messages "
            "(peer_id, msg_id, type, original_text, new_text, media_path, is_out, msg_date, timestamp, notes, sender_id, is_media) "
            "SELECT imp.peer_id, imp.msg_id, imp.type, imp.original_text, imp.new_text, imp.media_path, "
            "       imp.is_out, imp.msg_date, imp.timestamp, imp.notes, imp.sender_id, imp.is_media "
            "FROM import_db.actioned_messages imp "
            "WHERE NOT EXISTS ("
            "  SELECT 1 FROM main.actioned_messages cur "
            "  WHERE cur.peer_id = imp.peer_id AND cur.msg_id = imp.msg_id AND cur.type = imp.type "
            "    AND cur.original_text = imp.original_text "
            "    AND IFNULL(cur.media_path,'') = IFNULL(imp.media_path,'')"
            ")");
        // ghost_reads is a genuine single-value-per-peer table (unlike
        // actioned_messages), so "newest timestamp wins" is the correct
        // merge rule here, not append.
        execSql(
            "INSERT OR REPLACE INTO main.ghost_reads (peer_id, msg_id, timestamp) "
            "SELECT imp.peer_id, imp.msg_id, imp.timestamp FROM import_db.ghost_reads imp "
            "WHERE NOT EXISTS ("
            "  SELECT 1 FROM main.ghost_reads cur "
            "  WHERE cur.peer_id = imp.peer_id AND cur.timestamp >= imp.timestamp"
            ")");
        // activity_history rows are immutable point-in-time records (like
        // actioned_messages, not like ghost_reads), so append-only merge is
        // correct here too — skip rows that already exist identically. If
        // importing an older backup predating this table, import_db simply
        // has no such table and this statement fails silently (execSql only
        // logs via qDebug, does not abort the transaction).
        execSql(
            "INSERT INTO main.activity_history "
            "(peer_id, field, old_value, new_value, observed_at) "
            "SELECT imp.peer_id, imp.field, imp.old_value, imp.new_value, imp.observed_at "
            "FROM import_db.activity_history imp "
            "WHERE NOT EXISTS ("
            "  SELECT 1 FROM main.activity_history cur "
            "  WHERE cur.peer_id = imp.peer_id AND cur.field = imp.field "
            "    AND cur.observed_at = imp.observed_at "
            "    AND IFNULL(cur.old_value,'') = IFNULL(imp.old_value,'') "
            "    AND cur.new_value = imp.new_value"
            ")");
        execSql("COMMIT");

        sqlite3_stmt *detachStmt = nullptr;
        if (sqlite3_prepare_v2(gDb, "DETACH DATABASE import_db", -1, &detachStmt, nullptr) == SQLITE_OK) {
            sqlite3_step(detachStmt);
            sqlite3_finalize(detachStmt);
        }
    } else {
        qDebug() << "ImportFullBackup: ATTACH DATABASE failed — DB merge skipped:" << srcDb;
        if (!tempExtractDir.isEmpty()) QDir(tempExtractDir).removeRecursively();
        return false;
    }

    // Merge media folder: copy files that don't already exist locally
    // (see MergeDirRecursive doc comment) instead of wiping the local tree.
    const QString srcMedia = sourceDir + "/customizationMainFolder";
    if (QDir(srcMedia).exists()) {
        const QString dstMedia = QDir::homePath() + "/customizationMainFolder";
        if (!MergeDirRecursive(srcMedia, dstMedia)) {
            qDebug() << "ImportFullBackup: media merge failed (non-fatal)";
        }
    }

    // Merge peer_lists.json (union whitelist/blacklist) instead of
    // overwriting — otherwise per-peer AntiDelete overrides configured only
    // on this device would be lost. branding.json (device name/icon spoof)
    // is a single-device display preference, not accumulated data, so it's
    // intentionally left as this device's own value rather than imported.
    const QString appDataCustom = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/CustomMod";
    QDir().mkpath(appDataCustom);
    const QString srcPeerLists = sourceDir + "/peer_lists.json";
    if (QFile::exists(srcPeerLists)) {
        MergePeerListsJson(srcPeerLists, appDataCustom + "/peer_lists.json");
    }

    // Sozlamalar. settings.json KANONIK (platformadan mustaqil, v3+);
    // settings.reg faqat u yo'q bo'lganda — ya'ni v2 va undan eski
    // zaxiralar uchun. Ikkalasi ham qo'llanmaydi: JSON bor bo'lsa .reg
    // o'tkazib yuboriladi, aks holda registry qiymatlari JSON'nikini
    // ustidan yozib, natija manbaga bog'liq bo'lib qolardi.
    auto settingsRestored = false;
    {
        const QString srcJson = sourceDir + "/settings.json";
        if (QFile::exists(srcJson)) {
            QFile f(srcJson);
            if (f.open(QIODevice::ReadOnly)) {
                const auto doc = QJsonDocument::fromJson(f.readAll());
                if (doc.isObject()) {
                    CustomSettings::ImportFromJson(doc.object());
                    settingsRestored = true;
                }
            }
        }
    }

    // Registry import (Windows-specific, orqaga moslik).
    #ifdef Q_OS_WIN
    if (!settingsRestored) {
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

    // Media indeksini fayl tizimi bilan moslashtirish. Import qilingan
    // bazada yozuvlar 'present' bo'lishi mumkin, lekin bu qurilmada
    // fayllar yo'q (media arxivi alohida va ixtiyoriy) — ular 'missing'
    // ga o'tadi. Aks holda indeks yolg'on gapirardi va bu Track C
    // sinxronizatsiyasida tarqalardi.
    ReconcileMediaIndex(QDir::homePath() + "/customizationMainFolder");

    // Note: the reload callback (refreshing open history views) is NOT
    // invoked here. It used to fire via QTimer::singleShot(0, ...), which
    // only works when called from a thread with a running Qt event loop.
    // ImportFullBackup() is also called from a background thread by
    // ImportFullBackupAsync(), where that assumption doesn't hold — so the
    // callback is triggered from there instead, on the main thread.

    return true;
}

// ---------------------------------------------------------------------------
// Async export/import — run the synchronous work on a background thread and
// deliver results on the main thread, so callers never block the UI.
// ---------------------------------------------------------------------------

void ExportFullBackupAsync(
        const QString &targetDir,
        const ExportOptions &options,
        ExportResultCallback callback,
        const ExportProgressCallback &onProgress) {
    crl::async([targetDir, options, callback, onProgress] {
        const auto result = ExportFullBackup(
            targetDir,
            options,
            [onProgress](const QString &stage, int percent) {
                if (!onProgress) return;
                crl::on_main([onProgress, stage, percent] {
                    onProgress(stage, percent);
                });
            });
        crl::on_main([result, callback] {
            if (callback) callback(result);
        });
    });
}

bool ImportMediaArchive(const QString &zipPath) {
    Init();
    if (zipPath.isEmpty() || !QFile::exists(zipPath)) return false;

    const auto archiveRoot = QDir::homePath() + "/customizationMainFolder";
    QDir().mkpath(archiveRoot);

    // Media arxivi ichidagi yo'llar arxiv ildizidan NISBIY (medias/...),
    // shuning uchun to'g'ridan-to'g'ri arxiv ildiziga ochamiz.
    const QString psCmd = QString(
        "Expand-Archive -Path '%1' -DestinationPath '%2' -Force"
    ).arg(
        QDir::toNativeSeparators(zipPath),
        QDir::toNativeSeparators(archiveRoot));
    QProcess ps;
    ps.start("powershell.exe", {"-NonInteractive", "-Command", psCmd});
    ps.waitForFinished(30 * 60 * 1000);
    if (ps.exitCode() != 0) {
        qDebug() << "ImportMediaArchive: expand failed:"
                 << ps.readAllStandardError();
        return false;
    }

    // Endi fayllar joyida — indeksdagi 'pending' yozuvlar 'present' ga
    // qaytadi (ReconcileMediaIndex ikki tomonlama ishlaydi).
    ReconcileMediaIndex(archiveRoot);
    return true;
}

void ImportMediaArchiveAsync(
        const QString &zipPath,
        ImportResultCallback callback) {
    crl::async([zipPath, callback] {
        const bool ok = ImportMediaArchive(zipPath);
        crl::on_main([ok, callback] {
            if (ok && gReloadCallback) {
                gReloadCallback();
            }
            if (callback) callback(ok);
        });
    });
}

void ImportFullBackupAsync(
        const QString &sourcePath,
        bool fullReplace,
        ImportResultCallback callback) {
    crl::async([sourcePath, fullReplace, callback] {
        const bool ok = ImportFullBackup(sourcePath, fullReplace);
        crl::on_main([ok, callback] {
            if (ok && gReloadCallback) {
                gReloadCallback();
            }
            if (callback) callback(ok);
        });
    });
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
        gPeersWithDeleted.clear(); // endi hech kimda 'deleted' yo'q
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
        gPeersWithDeleted.clear(); // endi hech kimda 'deleted' yo'q
    }
}

// ---------------------------------------------------------------------------
// Activity History Log
// ---------------------------------------------------------------------------

// Loads one peer's latest-value-per-field into gActivityLatestCache on
// first touch, unless already loaded — mirrors EnsurePeerCacheLoaded()
// above, applied to the activity_history table.
static void EnsureActivityCacheLoaded(const QString &peerId) {
    {
        QMutexLocker locker(&gCacheMutex);
        if (gActivityLoadedPeers.contains(peerId)) return;
    }
    Init();
    if (!gDb) return;

    QHash<QString, QString> latest;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT field, new_value FROM activity_history "
            "WHERE peer_id = ? ORDER BY observed_at ASC, id ASC",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            // ASC tartib — oxirgi yozilgan qator eng yangi qiymat, shuning
            // uchun hash'ga ustma-ust yozilaveradi va oxiri eng yangisi qoladi.
            latest[colText(stmt, 0)] = colText(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    QMutexLocker locker(&gCacheMutex);
    if (gActivityLoadedPeers.contains(peerId)) return; // race guard
    gActivityLatestCache[peerId] = latest;
    gActivityLoadedPeers.insert(peerId);
}

void SaveActivityHistoryEntry(
        const QString &peerId,
        const QString &field,
        bool hasOldValue,
        const QString &oldValue,
        const QString &newValue,
        qint64 observedAt) {
    Init();
    // Prune once every 50 saves — same pattern as SaveGhostRead/CacheMessageText.
    static int sActivitySaveCount = 0;
    if (++sActivitySaveCount % 50 == 0) {
        PruneStaleActivityHistory(365);
    }
    if (!gDb) return;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "INSERT INTO activity_history "
            "(peer_id, field, old_value, new_value, observed_at) "
            "VALUES (?, ?, ?, ?, ?)",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        bindText(stmt, 2, field);
        if (hasOldValue) {
            bindText(stmt, 3, oldValue);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
        bindText(stmt, 4, newValue);
        sqlite3_bind_int64(stmt, 5, observedAt);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Keep the in-memory cache in sync so repeat lookups this session see
    // the new value without another SQLite round-trip.
    {
        QMutexLocker locker(&gCacheMutex);
        gActivityLatestCache[peerId][field] = newValue;
        gActivityLoadedPeers.insert(peerId);
    }
}

// Delete activity_history entries older than |days| days. Same pattern as
// PruneStaleGhostReads, but observed_at is a unix-timestamp INTEGER column
// (not a formatted TEXT timestamp like ghost_reads.timestamp), so the
// cutoff is computed and bound as an int64 instead of a string.
void PruneStaleActivityHistory(int days) {
    Init();
    if (!gDb) return;

    const qint64 cutoff =
        QDateTime::currentDateTime().addDays(-days).toSecsSinceEpoch();
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "DELETE FROM activity_history WHERE observed_at < ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, cutoff);
        sqlite3_step(stmt);
        const int removed = sqlite3_changes(gDb);
        sqlite3_finalize(stmt);
        if (removed > 0) {
            qDebug() << "PruneStaleActivityHistory: removed" << removed
                     << "entries older than" << days << "days.";
        }
    }
}

bool GetLatestActivityHistoryValue(
        const QString &peerId,
        const QString &field,
        QString &outValue) {
    EnsureActivityCacheLoaded(peerId);
    QMutexLocker locker(&gCacheMutex);
    const auto peerIt = gActivityLatestCache.constFind(peerId);
    if (peerIt == gActivityLatestCache.constEnd()) return false;
    const auto fieldIt = peerIt->constFind(field);
    if (fieldIt == peerIt->constEnd()) return false;
    outValue = fieldIt.value();
    return true;
}

QVector<ActivityHistoryEntry> GetActivityHistory(const QString &peerId) {
    Init();
    QVector<ActivityHistoryEntry> result;
    if (!gDb) return result;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT field, old_value, new_value, observed_at "
            "FROM activity_history WHERE peer_id = ? "
            "ORDER BY observed_at DESC, id DESC",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ActivityHistoryEntry entry;
            entry.field = colText(stmt, 0);
            entry.hasOldValue = (sqlite3_column_type(stmt, 1) != SQLITE_NULL);
            entry.oldValue = entry.hasOldValue ? colText(stmt, 1) : QString();
            entry.newValue = colText(stmt, 2);
            entry.observedAt = sqlite3_column_int64(stmt, 3);
            result.append(entry);
        }
        sqlite3_finalize(stmt);
    }
    return result;
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

    // Async: this runs 5s after startup and then every 24h, and used to
    // block the UI thread for as long as ExportFullBackup() took (the same
    // freeze users hit on manual backup, just less predictably timed).
    ExportFullBackupAsync(backupRoot, [backupRoot](const QString &result) {
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
    });
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
