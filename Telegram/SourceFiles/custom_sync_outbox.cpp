#include "custom_sync_outbox.h"
#include "custom_db.h"

#include <QtCore/QDateTime>
#include <sqlite3.h>
#include <algorithm>

namespace CustomSync {
namespace {

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
    // Task 5 da to'ldiriladi (CustomSync::Keystore orqali tekshirish).
    // Hozircha doim false qaytaradi -- soxta record_id yozilishining oldini oladi (K5).
    return false;
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

    // Task 5 da master kalit mavjud bo'lganda record_id hisoblanadi.
    // Hozir bu kodga kirmaydi (KeysAvailable() == false).
    QString recordId;

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

} // namespace Outbox
} // namespace CustomSync
