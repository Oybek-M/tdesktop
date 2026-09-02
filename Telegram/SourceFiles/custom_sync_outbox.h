#pragma once

#include <QtCore/QString>
#include <QtCore/QVector>

namespace CustomSync {

struct OutboxEntry {
    QString recordId;
    QString kind;
    qint64 accountId = 0;
    QString peerId;       // lokal ochiq peer id
    qint64 msgId = 0;
    qint64 occurredAt = 0;
    qint64 observedAt = 0;
    QString targetRecordId;
    int attempts = 0;
    QString lastError;
    qint64 nextRetryAt = 0;
};

namespace Outbox {

// Kalitlar mavjudligini tekshiradi (Task 5 keystore to'ldiradi).
// Hozircha doim false qaytaradi — soxta record_id yozilishining oldini oladi (K5).
[[nodiscard]] bool KeysAvailable();

// Navbatga qo'shadi. KeysAvailable() false bo'lsa hech narsa qilmaydi (bazaga tegmaydi).
// Bu funksiya custom_db.cpp dagi capture funksiyalarining OXIRIDA chaqiriladi.
void Enqueue(
    const QString &kind,
    qint64 accountId,
    const QString &peerId,
    qint64 msgId,
    qint64 occurredAt,
    const QString &targetRecordId = {});

// Jo'natishga tayyor yozuvlar (next_retry_at <= hozir), eng eskisidan (occurred_at ASC).
[[nodiscard]] QVector<OutboxEntry> Pending(int limit);

void MarkSent(const QString &recordId);

// Eksponensial backoff: 1s, 2s, 4s… maksimum 300s (5 daqiqa).
// next_retry_at diskda saqlanadi — ilova qayta ishga tushsa backoff nolga qaytmaydi.
void MarkFailed(const QString &recordId, const QString &error);

[[nodiscard]] int PendingCount();

// sync_state kalit-qiymat qatlami.
[[nodiscard]] QString GetState(const QString &key, const QString &fallback = {});
void SetState(const QString &key, const QString &value);

} // namespace Outbox
} // namespace CustomSync
