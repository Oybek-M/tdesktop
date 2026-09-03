#include "custom_sync_outbox.h"
#include "custom_sync_record.h"
#include "custom_sync_crypto.h"
#include "custom_sync_keystore.h"
#include "custom_settings.h"
#include "custom_db.h"

#include <QtCore/QDateTime>
#include <sqlite3.h>
#include <algorithm>

namespace CustomSync {
namespace {

QByteArray gMasterKey;
QByteArray gContentKey;
QByteArray gPeerKey;
QByteArray gAccountKey;
QByteArray gMediaKey;
bool gMasterKeyLoaded = false;

void bindText(sqlite3_stmt *stmt, int index, const QString &str) {
    if (str.isEmpty()) {
        sqlite3_bind_text(stmt, index, "", 0, SQLITE_STATIC);
    } else {
        const auto utf8 = str.toUtf8();
        sqlite3_bind_text(stmt, index, utf8.constData(), utf8.size(), SQLITE_TRANSIENT);
    }
}

QString colText(sqlite3_stmt *stmt, int col) {
    const auto *text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? QString::fromUtf8(text) : QString();
}

int CurrentAttempts(sqlite3 *db, const QString &recordId) {
    sqlite3_stmt *stmt = nullptr;
    int attempts = 0;
    if (sqlite3_prepare_v2(db,
            "SELECT attempts FROM sync_outbox WHERE record_id = ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, recordId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            attempts = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return attempts;
}

} // namespace

namespace Outbox {

bool KeysAvailable() {
    return CustomSettings::SyncEnabled()
        && !CustomSettings::SyncServerUrl().trimmed().isEmpty()
        && LoadMasterKey();
}

void Enqueue(
        const QString &kind,
        qint64 accountId,
        const QString &peerId,
        qint64 msgId,
        qint64 occurredAt,
        const QString &targetRecordId) {
    if (!KeysAvailable()) {
        return;
    }

    auto *db = CustomDB::RawHandle();
    if (!db) return;

    const auto observedAt = QDateTime::currentSecsSinceEpoch();

    const QString recordId = ComputeRecordIdFor(
        gMasterKey,
        kind,
        accountId,
        peerId,
        msgId,
        occurredAt);

    // Qulf: agar recordId bo'sh bo'lsa, hech qachon bazaga yozmaymiz
    // (aks holda bo'sh '' kalitiga urilib navbatdagi hamma yozuv o'chib ketardi).
    if (recordId.isEmpty()) {
        return;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
            "INSERT OR REPLACE INTO sync_outbox ("
            "record_id, kind, account_id, peer_id, msg_id, "
            "occurred_at, observed_at, target_record_id, "
            "attempts, last_error, next_retry_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, NULL, 0)",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, recordId);
        bindText(stmt, 2, kind);
        sqlite3_bind_int64(stmt, 3, accountId);
        bindText(stmt, 4, peerId);
        sqlite3_bind_int64(stmt, 5, msgId);
        sqlite3_bind_int64(stmt, 6, occurredAt);
        sqlite3_bind_int64(stmt, 7, observedAt);
        if (targetRecordId.isEmpty()) {
            sqlite3_bind_null(stmt, 8);
        } else {
            bindText(stmt, 8, targetRecordId);
        }
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

QVector<OutboxEntry> Pending(int limit) {
    QVector<OutboxEntry> result;
    auto *db = CustomDB::RawHandle();
    if (!db) return result;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
            "SELECT record_id, kind, account_id, peer_id, msg_id, "
            "occurred_at, observed_at, target_record_id, attempts, "
            "last_error, next_retry_at "
            "FROM sync_outbox "
            "WHERE next_retry_at <= ? "
            "ORDER BY occurred_at ASC "
            "LIMIT ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        const auto now = QDateTime::currentSecsSinceEpoch();
        sqlite3_bind_int64(stmt, 1, now);
        sqlite3_bind_int(stmt, 2, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            OutboxEntry entry;
            entry.recordId = colText(stmt, 0);
            entry.kind = colText(stmt, 1);
            entry.accountId = sqlite3_column_int64(stmt, 2);
            entry.peerId = colText(stmt, 3);
            entry.msgId = sqlite3_column_int64(stmt, 4);
            entry.occurredAt = sqlite3_column_int64(stmt, 5);
            entry.observedAt = sqlite3_column_int64(stmt, 6);
            entry.targetRecordId = colText(stmt, 7);
            entry.attempts = sqlite3_column_int(stmt, 8);
            entry.lastError = colText(stmt, 9);
            entry.nextRetryAt = sqlite3_column_int64(stmt, 10);
            result.push_back(std::move(entry));
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

void MarkSent(const QString &recordId) {
    auto *db = CustomDB::RawHandle();
    if (!db) return;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
            "DELETE FROM sync_outbox WHERE record_id = ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, recordId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void MarkFailed(const QString &recordId, const QString &error) {
    auto *db = CustomDB::RawHandle();
    if (!db) return;

    // 1s, 2s, 4s, 8s… 300s da to'xtaydi.
    const auto current = CurrentAttempts(db, recordId);
    const auto attempts = current + 1;
    const auto shift = std::min(attempts - 1, 9);
    const auto delay = std::min<qint64>(300, qint64(1) << shift);
    const auto nextRetryAt = QDateTime::currentSecsSinceEpoch() + delay;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
            "UPDATE sync_outbox "
            "SET attempts = ?, last_error = ?, next_retry_at = ? "
            "WHERE record_id = ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, attempts);
        bindText(stmt, 2, error);
        sqlite3_bind_int64(stmt, 3, nextRetryAt);
        bindText(stmt, 4, recordId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

int PendingCount() {
    auto *db = CustomDB::RawHandle();
    if (!db) return 0;

    int count = 0;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM sync_outbox WHERE next_retry_at <= ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        const auto now = QDateTime::currentSecsSinceEpoch();
        sqlite3_bind_int64(stmt, 1, now);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

QString GetState(const QString &key, const QString &fallback) {
    auto *db = CustomDB::RawHandle();
    if (!db) return fallback;

    sqlite3_stmt *stmt = nullptr;
    QString result = fallback;
    if (sqlite3_prepare_v2(db,
            "SELECT value FROM sync_state WHERE key = ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, key);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = colText(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

void SetState(const QString &key, const QString &value) {
    auto *db = CustomDB::RawHandle();
    if (!db) return;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
            "INSERT INTO sync_state (key, value) VALUES (?, ?) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, key);
        bindText(stmt, 2, value);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// OGOHLANTIRISH / XAVF: Ikkinchi qurilma bugun MUTLAQO BOSHQA master key yaratadi!
// Bir xil kalitni bir nechta qurilmalar o'rtasida bo'lishish uchun serverdagi
// passphrase-wrap oqimi (/api/v1/keys/wraps) va parolni kiritish oynasi talab
// qilinadi (keyingi vazifa). Ungacha ikkinchi qurilmani enroll qilish bir-birining
// yozuvlarini ocholmaydigan va record_id'lari hech qachon mos kelmaydigan
// ikkita mustaqil qurilma hosil qiladi — va bu haqda hech narsa ogohlantirmaydi.
bool EnsureMasterKeyCreated() {
    // Agar master key allaqachon mavjud bo'lsa, uni HECH QACHON almashtirmaymiz!
    // Chunki avval yuborilgan barcha yozuvlar shu kalitdan hosil qilingan;
    // uni almashtirish butun tarixni o'chirib yuboradi (records undecryptable bo'lib qoladi).
    const auto existingProtected = GetState(QStringLiteral("master_key_protected"));
    if (!existingProtected.isEmpty()) {
        return LoadMasterKey();
    }

    if (!Keystore::Available()) {
        return false;
    }

    const auto rawKey = Crypto::RandomBytes(32);
    if (rawKey.size() != 32) {
        return false;
    }

    const auto protectedBlob = Keystore::ProtectBytes(rawKey);
    if (!protectedBlob.has_value() || protectedBlob->isEmpty()) {
        return false;
    }

    SetState(QStringLiteral("master_key_protected"),
             QString::fromLatin1(protectedBlob->toBase64()));

    return LoadMasterKey();
}

bool LoadMasterKey() {
    if (gMasterKeyLoaded && !gMasterKey.isEmpty()) {
        return true;
    }

    const auto existingProtected = GetState(QStringLiteral("master_key_protected"));
    if (existingProtected.isEmpty()) {
        return false;
    }

    const auto blob = QByteArray::fromBase64(existingProtected.toLatin1());
    if (blob.isEmpty()) {
        return false;
    }

    const auto plain = Keystore::UnprotectBytes(blob);
    if (!plain.has_value() || plain->size() != 32) {
        return false;
    }

    gMasterKey = *plain;
    const QByteArray zeros32(32, '\0');
    gPeerKey = Crypto::HkdfSha256(gMasterKey, zeros32, QByteArrayLiteral("customsync-peer-v1"), 32);
    gAccountKey = Crypto::HkdfSha256(gMasterKey, zeros32, QByteArrayLiteral("customsync-account-v1"), 32);
    gContentKey = Crypto::HkdfSha256(gMasterKey, zeros32, QByteArrayLiteral("customsync-content-v1"), 32);
    gMediaKey = Crypto::HkdfSha256(gMasterKey, zeros32, QByteArrayLiteral("customsync-media-v1"), 32);
    gMasterKeyLoaded = true;
    return true;
}

QByteArray MasterKey() {
    // Yuklash muvaffaqiyatsiz bo'lsa BO'SH qaytaramiz va buni oshkora
    // qilamiz. Aks holda chaqiruvchi bo'sh kalit bilan HMAC hisoblab,
    // yaroqli ko'rinadigan, lekin butunlay xato hash olardi.
    if (!gMasterKeyLoaded && !LoadMasterKey()) {
        return QByteArray();
    }
    return gMasterKey;
}

QByteArray ContentKey() {
    // Yuklash muvaffaqiyatsiz bo'lsa BO'SH qaytaramiz va buni oshkora
    // qilamiz. Aks holda chaqiruvchi bo'sh kalit bilan HMAC hisoblab,
    // yaroqli ko'rinadigan, lekin butunlay xato hash olardi.
    if (!gMasterKeyLoaded && !LoadMasterKey()) {
        return QByteArray();
    }
    return gContentKey;
}

QByteArray PeerKey() {
    // Yuklash muvaffaqiyatsiz bo'lsa BO'SH qaytaramiz va buni oshkora
    // qilamiz. Aks holda chaqiruvchi bo'sh kalit bilan HMAC hisoblab,
    // yaroqli ko'rinadigan, lekin butunlay xato hash olardi.
    if (!gMasterKeyLoaded && !LoadMasterKey()) {
        return QByteArray();
    }
    return gPeerKey;
}

QByteArray AccountKey() {
    // Yuklash muvaffaqiyatsiz bo'lsa BO'SH qaytaramiz va buni oshkora
    // qilamiz. Aks holda chaqiruvchi bo'sh kalit bilan HMAC hisoblab,
    // yaroqli ko'rinadigan, lekin butunlay xato hash olardi.
    if (!gMasterKeyLoaded && !LoadMasterKey()) {
        return QByteArray();
    }
    return gAccountKey;
}

QByteArray MediaKey() {
    // Yuklash muvaffaqiyatsiz bo'lsa BO'SH qaytaramiz va buni oshkora
    // qilamiz. Aks holda chaqiruvchi bo'sh kalit bilan HMAC hisoblab,
    // yaroqli ko'rinadigan, lekin butunlay xato hash olardi.
    if (!gMasterKeyLoaded && !LoadMasterKey()) {
        return QByteArray();
    }
    return gMediaKey;
}

} // namespace Outbox
} // namespace CustomSync
