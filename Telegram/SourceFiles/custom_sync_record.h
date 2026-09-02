#pragma once

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QVector>
#include <QtCore/QJsonObject>

// Sync yozuvi va uning identifikatori.
//
// MUHIM: bu fayl ATAYLAB tdesktop'ning hech qanday sarlavhasiga bog'liq
// emas — faqat QtCore. Shu sababli uni tools/sync-selftest da bir necha
// soniyada kompilyatsiya qilib, to'liq build'ni kutmasdan sinash mumkin.

namespace CustomSync {

// Yozuv turlari — bu satrlar simli protokolning bir qismi.
namespace Kind {
inline constexpr auto Deleted       = "deleted";
inline constexpr auto Edited        = "edited";
inline constexpr auto Activity      = "activity";
inline constexpr auto GhostRead     = "ghost_read";
inline constexpr auto Setting       = "setting";
inline constexpr auto PeerDirectory = "peer_directory";
inline constexpr auto MediaIndex    = "media_index";   // spec 0.4
inline constexpr auto Tombstone     = "tombstone";     // spec 0.3
} // namespace Kind

struct MediaRef {
    QString hash;
    qint64 size = 0;
    QByteArray nonce;
};

struct Record {
    QString recordId;
    QString kind;
    QString accountHash;    // 0.12, "" faqat kind=="activity" uchun
    QString peerHash;
    qint64 msgId = 0;
    qint64 occurredAt = 0;
    qint64 observedAt = 0;
    QString deviceId;
    QByteArray nonce;
    QByteArray payload;
    QString targetRecordId; // 0.13, faqat kind=="tombstone" uchun
    QVector<MediaRef> media;
};

// record_id = hex(SHA256(kind ‖ 0x00 ‖ account_hash ‖ 0x00 ‖ peer_hash ‖ 0x00 ‖
//                        msg_id ‖ 0x00 ‖ occurred_at))
//
// Beshala platforma bayt-ma-bayt bir xil natija berishi SHART.
// Har qanday o'zgarish oldingi barcha yozuvlarni yaroqsiz qiladi.
[[nodiscard]] QString ComputeRecordId(
    const QString &kind,
    const QString &accountHash,
    const QString &peerHash,
    qint64 msgId,
    qint64 occurredAt);

// msg_id tabiiy bo'lmagan kind'lar uchun diskriminator:
// SHA256(text) ning birinchi 8 bayti, int64 sifatida (big-endian).
// Bitta peer uchun bir soniyada sodir bo'lgan ikki xil hodisa bir xil
// record_id olib, biri jimgina yo'qolmasligi uchun.
[[nodiscard]] qint64 DiscriminatorFor(const QString &text);

[[nodiscard]] QJsonObject ToJson(const Record &record);
[[nodiscard]] Record FromJson(const QJsonObject &object);

} // namespace CustomSync
