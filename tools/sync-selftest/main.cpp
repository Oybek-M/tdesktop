// Mustaqil tekshiruv dasturi. tdesktop'ni to'liq build qilmasdan
// custom_sync_record va custom_sync_crypto ni test-vectors.json ga
// qarshi tekshiradi.
//
// Ishlatish: sync_selftest <test-vectors.json yo'li>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>

#include "custom_sync_record.h"

namespace {

int gFailures = 0;

void messageHandler(QtMsgType, const QMessageLogContext &, const QString &msg) {
    fprintf(stderr, "%s\n", qPrintable(msg));
    fflush(stderr);
}

void check(const QString &name, const QString &actual, const QString &expected) {
    if (actual == expected) {
        qInfo().noquote() << "  ok  " << name;
    } else {
        qWarning().noquote() << "  FAIL" << name
            << "\n        kutilgan:" << expected
            << "\n        olingan :" << actual;
        ++gFailures;
    }
}

void checkRecordRoundtrip() {
    qInfo() << "\nRecord ToJson/FromJson roundtrip tekshirilmoqda:";

    CustomSync::Record original;
    original.recordId = QStringLiteral("25cc97f881e1b7dde2de8b11599e608476d8054e853a92f8d9b6619d6941cc0c");
    original.kind = QString::fromLatin1(CustomSync::Kind::Tombstone);
    original.accountHash = QStringLiteral("8ce7fd6f2c871df09e218375ad4bb5c4");
    original.peerHash = QStringLiteral("cbcd16f7c84f024ee6791c08453e35e0");
    original.msgId = -5190442718973336697LL;
    original.occurredAt = 1787000000;
    original.observedAt = 1787000001;
    original.deviceId = QStringLiteral("dev_test_123");
    original.nonce = QByteArray::fromHex("000102030405060708090a0b");
    original.payload = QByteArray::fromHex("deadbeefcafebabe");
    original.targetRecordId = QStringLiteral("target_rec_id_999");
    original.media.append(CustomSync::MediaRef{
        QStringLiteral("hash123"),
        1234567890123LL,
        QByteArray::fromHex("0c0d0e0f1011121314151617")
    });

    const auto json = CustomSync::ToJson(original);
    const auto parsed = CustomSync::FromJson(json);

    check(QStringLiteral("roundtrip.record_id"), parsed.recordId, original.recordId);
    check(QStringLiteral("roundtrip.kind"), parsed.kind, original.kind);
    check(QStringLiteral("roundtrip.account_hash"), parsed.accountHash, original.accountHash);
    check(QStringLiteral("roundtrip.peer_hash"), parsed.peerHash, original.peerHash);
    check(QStringLiteral("roundtrip.msg_id"), QString::number(parsed.msgId), QString::number(original.msgId));
    check(QStringLiteral("roundtrip.occurred_at"), QString::number(parsed.occurredAt), QString::number(original.occurredAt));
    check(QStringLiteral("roundtrip.observed_at"), QString::number(parsed.observedAt), QString::number(original.observedAt));
    check(QStringLiteral("roundtrip.device_id"), parsed.deviceId, original.deviceId);
    check(QStringLiteral("roundtrip.nonce"), QString::fromLatin1(parsed.nonce.toHex()), QString::fromLatin1(original.nonce.toHex()));
    check(QStringLiteral("roundtrip.payload"), QString::fromLatin1(parsed.payload.toHex()), QString::fromLatin1(original.payload.toHex()));
    check(QStringLiteral("roundtrip.target_record_id"), parsed.targetRecordId, original.targetRecordId);
    check(QStringLiteral("roundtrip.media_count"), QString::number(parsed.media.size()), QStringLiteral("1"));
    if (!parsed.media.isEmpty()) {
        check(QStringLiteral("roundtrip.media_hash"), parsed.media[0].hash, original.media[0].hash);
        check(QStringLiteral("roundtrip.media_size"), QString::number(parsed.media[0].size), QString::number(original.media[0].size));
    }

    // Non-tombstone da target_record_id json'da bo'lmasligi kerak
    CustomSync::Record edited;
    edited.kind = QString::fromLatin1(CustomSync::Kind::Edited);
    edited.targetRecordId = QStringLiteral("should_not_serialize");
    const auto editedJson = CustomSync::ToJson(edited);
    if (editedJson.contains(QStringLiteral("target_record_id"))) {
        qWarning() << "FAIL target_record_id faqat tombstone uchun uzatilishi shart";
        ++gFailures;
    } else {
        qInfo() << "  ok   non-tombstone target_record_id omitted";
    }
}

} // namespace

int main(int argc, char *argv[]) {
    qInstallMessageHandler(messageHandler);
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        qWarning() << "Ishlatish: sync_selftest <test-vectors.json>";
        return 2;
    }

    QFile file(argv[1]);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Faylni ochib bo'lmadi:" << argv[1];
        return 2;
    }

    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    const auto cases = root.value(QStringLiteral("record_id"))
                           .toObject()
                           .value(QStringLiteral("cases"))
                           .toArray();

    qInfo() << "record_id vektorlari tekshirilmoqda:";
    int checkedCases = 0;
    int vectorFailuresBefore = gFailures;

    for (const auto &item : cases) {
        const auto entry = item.toObject();
        const auto kind = entry.value(QStringLiteral("kind")).toString();
        const auto accountHash = entry.value(QStringLiteral("account_hash")).toString();
        const auto peerHash = entry.value(QStringLiteral("peer_hash")).toString();
        const auto msgId = entry.value(QStringLiteral("msg_id")).toInteger();
        const auto occurredAt = entry.value(QStringLiteral("occurred_at")).toInteger();
        const auto expected = entry.value(QStringLiteral("record_id")).toString();

        const auto actual = CustomSync::ComputeRecordId(
            kind,
            accountHash,
            peerHash,
            msgId,
            occurredAt);

        const auto label = QStringLiteral("%1:%2:%3")
            .arg(kind)
            .arg(accountHash.isEmpty() ? QStringLiteral("empty_acc") : accountHash.left(8))
            .arg(msgId);

        check(label, actual, expected);
        ++checkedCases;
    }

    // Himoya: test-vectors.json ichida aynan 11 ta holat bo'lishi shart.
    // 0 yoki boshqa son bo'lsa xato hisoblanadi (soxta muvaffaqiyatning oldini olish).
    if (checkedCases != 11) {
        qCritical().noquote() << "XATO: Aynan 11 ta holat tekshirilishi kerak edi, lekin"
                              << checkedCases << "ta holat tekshirildi.";
        return 1;
    }

    const int passedCases = checkedCases - (gFailures - vectorFailuresBefore);
    qInfo().noquote() << QStringLiteral("\nrecord_id natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(passedCases)
        .arg(checkedCases);

    checkRecordRoundtrip();

    if (gFailures == 0) {
        qInfo().noquote() << "\nBarcha vektorlar va tekshiruvlar mos keldi.";
        return 0;
    } else {
        qWarning().noquote() << QStringLiteral("\n%1 ta nomuvofiqlik aniqlandi.").arg(gFailures);
        return 1;
    }
}
