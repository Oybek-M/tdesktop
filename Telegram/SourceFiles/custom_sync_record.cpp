#include "custom_sync_record.h"
#include "custom_sync_crypto.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonArray>

namespace CustomSync {
namespace {

void appendField(QByteArray &buffer, const QByteArray &value) {
    buffer.append(value);
    buffer.append('\0');
}

} // namespace

QString ComputeRecordId(
        const QString &kind,
        const QString &accountHash,
        const QString &peerHash,
        qint64 msgId,
        qint64 occurredAt) {
    QByteArray buffer;
    appendField(buffer, kind.toUtf8());
    appendField(buffer, accountHash.toUtf8());
    appendField(buffer, peerHash.toUtf8());
    appendField(buffer, QByteArray::number(msgId));
    // Oxirgi maydondan keyin ajratuvchi qo'yilmaydi — server
    // implementatsiyasi bilan aynan mos bo'lishi uchun (spec §0.12).
    buffer.append(QByteArray::number(occurredAt));

    return QString::fromLatin1(
        QCryptographicHash::hash(buffer, QCryptographicHash::Sha256).toHex());
}

QString ComputeRecordIdFor(
        const QByteArray &masterKey,
        const QString &kind,
        qint64 accountId,
        const QString &peerId,
        qint64 msgId,
        qint64 occurredAt) {
    const QByteArray zeros32(32, '\0');
    const auto peerKey = Crypto::HkdfSha256(
        masterKey,
        zeros32,
        QByteArrayLiteral("customsync-peer-v1"),
        32);
    const auto peerHash = Crypto::ComputePeerHash(peerKey, peerId);

    QString accountHash;
    if (kind != QLatin1String(Kind::Activity)) {
        const auto accountKey = Crypto::HkdfSha256(
            masterKey,
            zeros32,
            QByteArrayLiteral("customsync-account-v1"),
            32);
        accountHash = Crypto::ComputeAccountHash(
            accountKey,
            QString::number(accountId));
    }

    return ComputeRecordId(kind, accountHash, peerHash, msgId, occurredAt);
}

qint64 DiscriminatorFor(const QString &text) {
    const auto digest = QCryptographicHash::hash(
        text.toUtf8(), QCryptographicHash::Sha256);
    qint64 result = 0;
    for (int i = 0; i < 8; ++i) {
        result = (result << 8) | quint8(digest[i]);
    }
    // Manfiy bo'lib qolmasligi uchun eng yuqori bitni tozalaymiz —
    // server tomonda bu qiymat oddiy musbat bigint sifatida saqlanadi.
    return result & 0x7FFFFFFFFFFFFFFFLL;
}

QJsonObject ToJson(const Record &record) {
    QJsonArray media;
    for (const auto &item : record.media) {
        media.append(QJsonObject{
            { QStringLiteral("hash"), item.hash },
            { QStringLiteral("size"), item.size },
            { QStringLiteral("nonce"),
              QString::fromLatin1(item.nonce.toBase64()) },
        });
    }

    auto obj = QJsonObject{
        { QStringLiteral("record_id"),    record.recordId },
        { QStringLiteral("kind"),         record.kind },
        { QStringLiteral("account_hash"), record.accountHash },
        { QStringLiteral("peer_hash"),    record.peerHash },
        { QStringLiteral("msg_id"),       record.msgId },
        { QStringLiteral("occurred_at"),  record.occurredAt },
        { QStringLiteral("observed_at"),  record.observedAt },
        { QStringLiteral("device_id"),    record.deviceId },
        { QStringLiteral("nonce"),        QString::fromLatin1(record.nonce.toBase64()) },
        { QStringLiteral("payload"),      QString::fromLatin1(record.payload.toBase64()) },
        { QStringLiteral("media"),        media },
    };

    // spec §0.13: target_record_id faqat tombstone uchun va ochiq matnda uzatiladi
    if (record.kind == Kind::Tombstone && !record.targetRecordId.isEmpty()) {
        obj.insert(QStringLiteral("target_record_id"), record.targetRecordId);
    }

    return obj;
}

Record FromJson(const QJsonObject &object) {
    Record record;
    record.recordId       = object.value(QStringLiteral("record_id")).toString();
    record.kind           = object.value(QStringLiteral("kind")).toString();
    record.accountHash    = object.value(QStringLiteral("account_hash")).toString();
    record.peerHash       = object.value(QStringLiteral("peer_hash")).toString();
    record.msgId          = object.value(QStringLiteral("msg_id")).toInteger();
    record.occurredAt     = object.value(QStringLiteral("occurred_at")).toInteger();
    record.observedAt     = object.value(QStringLiteral("observed_at")).toInteger();
    record.deviceId       = object.value(QStringLiteral("device_id")).toString();
    record.nonce          = QByteArray::fromBase64(
        object.value(QStringLiteral("nonce")).toString().toLatin1());
    record.payload        = QByteArray::fromBase64(
        object.value(QStringLiteral("payload")).toString().toLatin1());
    record.targetRecordId = object.value(QStringLiteral("target_record_id")).toString();

    for (const auto &item : object.value(QStringLiteral("media")).toArray()) {
        const auto entry = item.toObject();
        record.media.append(MediaRef{
            entry.value(QStringLiteral("hash")).toString(),
            entry.value(QStringLiteral("size")).toInteger(),
            QByteArray::fromBase64(
                entry.value(QStringLiteral("nonce")).toString().toLatin1()),
        });
    }
    return record;
}

} // namespace CustomSync
