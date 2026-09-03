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
#include "custom_sync_crypto.h"
#include "custom_sync_keystore.h"

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

    // 1. record_id (11 ta holat)
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

    if (checkedCases != 11) {
        qCritical().noquote() << "XATO: Aynan 11 ta record_id holati tekshirilishi kerak edi, lekin"
                              << checkedCases << "ta holat tekshirildi.";
        return 1;
    }

    const int passedCases = checkedCases - (gFailures - vectorFailuresBefore);
    qInfo().noquote() << QStringLiteral("record_id natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(passedCases)
        .arg(checkedCases);

    checkRecordRoundtrip();

    // 2. hkdf (4 ta hosilaviy kalit, salt = 32 ta nol bayt)
    qInfo() << "\nHKDF-SHA256 vektorlari tekshirilmoqda:";
    const auto hkdfObj = root.value(QStringLiteral("hkdf")).toObject();
    const auto masterKeyHex = hkdfObj.value(QStringLiteral("master_key_hex")).toString();
    const auto masterKey = QByteArray::fromHex(masterKeyHex.toLatin1());
    const auto salt32Zeros = QByteArray(32, '\0');
    const auto derivedObj = hkdfObj.value(QStringLiteral("derived")).toObject();

    int hkdfChecked = 0;
    int hkdfFailuresBefore = gFailures;
    for (auto it = derivedObj.begin(); it != derivedObj.end(); ++it) {
        const auto info = it.key();
        const auto expectedHex = it.value().toString();
        const auto actual = CustomSync::Crypto::HkdfSha256(
            masterKey, salt32Zeros, info.toUtf8(), 32);
        check(info, QString::fromLatin1(actual.toHex()), expectedHex);
        ++hkdfChecked;
    }

    if (hkdfChecked != 4) {
        qCritical().noquote() << "XATO: hkdf uchun 4 ta kalit tekshirilishi kerak edi, lekin"
                              << hkdfChecked << "ta tekshirildi.";
        return 1;
    }
    const int hkdfPassed = hkdfChecked - (gFailures - hkdfFailuresBefore);
    qInfo().noquote() << QStringLiteral("hkdf natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(hkdfPassed)
        .arg(hkdfChecked);

    // 3. account_hash (3 ta holat)
    qInfo() << "\naccount_hash vektorlari tekshirilmoqda:";
    const auto accountKey = CustomSync::Crypto::HkdfSha256(
        masterKey, salt32Zeros, QByteArrayLiteral("customsync-account-v1"), 32);
    const auto accountCases = root.value(QStringLiteral("account_hash"))
                                  .toObject()
                                  .value(QStringLiteral("cases"))
                                  .toArray();
    int accountChecked = 0;
    int accountFailuresBefore = gFailures;
    for (const auto &item : accountCases) {
        const auto entry = item.toObject();
        const auto accountId = entry.value(QStringLiteral("account_id")).toString();
        const auto expected = entry.value(QStringLiteral("account_hash")).toString();
        const auto actual = CustomSync::Crypto::ComputeAccountHash(accountKey, accountId);
        check(QStringLiteral("account_hash:%1").arg(accountId), actual, expected);
        ++accountChecked;
    }

    if (accountChecked != 3) {
        qCritical().noquote() << "XATO: account_hash uchun 3 ta holat tekshirilishi kerak edi, lekin"
                              << accountChecked << "ta tekshirildi.";
        return 1;
    }
    const int accountPassed = accountChecked - (gFailures - accountFailuresBefore);
    qInfo().noquote() << QStringLiteral("account_hash natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(accountPassed)
        .arg(accountChecked);

    // 4. peer_hash (3 ta holat)
    qInfo() << "\npeer_hash vektorlari tekshirilmoqda:";
    const auto peerKey = CustomSync::Crypto::HkdfSha256(
        masterKey, salt32Zeros, QByteArrayLiteral("customsync-peer-v1"), 32);
    const auto peerCases = root.value(QStringLiteral("peer_hash"))
                               .toObject()
                               .value(QStringLiteral("cases"))
                               .toArray();
    int peerChecked = 0;
    int peerFailuresBefore = gFailures;
    for (const auto &item : peerCases) {
        const auto entry = item.toObject();
        const auto peerId = entry.value(QStringLiteral("peer_id")).toString();
        const auto expected = entry.value(QStringLiteral("peer_hash")).toString();
        const auto actual = CustomSync::Crypto::ComputePeerHash(peerKey, peerId);
        check(QStringLiteral("peer_hash:%1").arg(peerId), actual, expected);
        ++peerChecked;
    }

    if (peerChecked != 3) {
        qCritical().noquote() << "XATO: peer_hash uchun 3 ta holat tekshirilishi kerak edi, lekin"
                              << peerChecked << "ta tekshirildi.";
        return 1;
    }
    const int peerPassed = peerChecked - (gFailures - peerFailuresBefore);
    qInfo().noquote() << QStringLiteral("peer_hash natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(peerPassed)
        .arg(peerChecked);

    // 5. aes_gcm (3 ta holat, shifrlash va deshifrlash)
    qInfo() << "\nAES-256-GCM vektorlari tekshirilmoqda:";
    const auto aesCases = root.value(QStringLiteral("aes_gcm"))
                              .toObject()
                              .value(QStringLiteral("cases"))
                              .toArray();
    int aesChecked = 0;
    int aesFailuresBefore = gFailures;
    for (const auto &item : aesCases) {
        const auto entry = item.toObject();
        const auto name = entry.value(QStringLiteral("name")).toString();
        const auto key = QByteArray::fromHex(entry.value(QStringLiteral("key_hex")).toString().toLatin1());
        const auto nonce = QByteArray::fromHex(entry.value(QStringLiteral("nonce_hex")).toString().toLatin1());
        const auto plaintext = QByteArray::fromHex(entry.value(QStringLiteral("plaintext_hex")).toString().toLatin1());
        const auto expectedCiphertextHex = entry.value(QStringLiteral("ciphertext_hex")).toString();
        const auto expectedTagHex = entry.value(QStringLiteral("tag_hex")).toString();

        // Shifrlash tekshiruvi (Seal -> ciphertext || tag)
        const auto sealed = CustomSync::Crypto::Seal(key, nonce, plaintext);
        if (sealed.size() < 16) {
            check(QStringLiteral("aes_gcm/encrypt/%1/length").arg(name),
                  QString::number(sealed.size()),
                  QStringLiteral(">= 16"));
        } else {
            const auto actualCiphertext = sealed.left(sealed.size() - 16);
            const auto actualTag = sealed.right(16);
            check(QStringLiteral("aes_gcm/encrypt/%1/ciphertext").arg(name),
                  QString::fromLatin1(actualCiphertext.toHex()),
                  expectedCiphertextHex);
            check(QStringLiteral("aes_gcm/encrypt/%1/tag").arg(name),
                  QString::fromLatin1(actualTag.toHex()),
                  expectedTagHex);
        }

        // Deshifrlash tekshiruvi (Open <- ciphertext || tag)
        const auto expectedCiphertext = QByteArray::fromHex(expectedCiphertextHex.toLatin1());
        const auto expectedTag = QByteArray::fromHex(expectedTagHex.toLatin1());
        const auto sealedInput = expectedCiphertext + expectedTag;
        const auto opened = CustomSync::Crypto::Open(key, nonce, sealedInput);
        if (opened.has_value()) {
            check(QStringLiteral("aes_gcm/decrypt/%1").arg(name),
                  QString::fromLatin1(opened->toHex()),
                  entry.value(QStringLiteral("plaintext_hex")).toString());
        } else {
            check(QStringLiteral("aes_gcm/decrypt/%1").arg(name),
                  QStringLiteral("<DESHIFRLASH_XATOSI>"),
                  entry.value(QStringLiteral("plaintext_hex")).toString());
        }

        ++aesChecked;
    }

    if (aesChecked != 3) {
        qCritical().noquote() << "XATO: aes_gcm uchun 3 ta holat tekshirilishi kerak edi, lekin"
                              << aesChecked << "ta tekshirildi.";
        return 1;
    }

    // Tamper tekshiruvi: buzilgan tag rad etilishi shart
    {
        const QByteArray key(32, '\x11');
        const QByteArray nonce(12, '\x22');
        const QByteArray plaintext = "CustomMod sync test payload";
        const auto sealed = CustomSync::Crypto::Seal(key, nonce, plaintext);
        auto tampered = sealed;
        if (tampered.size() > 0) {
            tampered[tampered.size() - 1] = char(tampered[tampered.size() - 1] ^ 0x01);
        }
        const auto tamperedResult = CustomSync::Crypto::Open(key, nonce, tampered);
        check(QStringLiteral("aes_gcm/tamper (buzilgan tag rad etiladi)"),
              tamperedResult.has_value() ? QStringLiteral("qabul qilindi") : QStringLiteral("rad etildi"),
              QStringLiteral("rad etildi"));
    }

    const int aesPassed = aesChecked - (gFailures - aesFailuresBefore);
    qInfo().noquote() << QStringLiteral("aes_gcm natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(aesPassed)
        .arg(aesChecked);

    // 6. pbkdf2 (3 ta holat, 600k va 2M iteratsiyalar)
    qInfo() << "\nPBKDF2-HMAC-SHA256 vektorlari tekshirilmoqda:";
    const auto pbkdf2Cases = root.value(QStringLiteral("pbkdf2"))
                                 .toObject()
                                 .value(QStringLiteral("cases"))
                                 .toArray();
    int pbkdf2Checked = 0;
    int pbkdf2FailuresBefore = gFailures;
    for (const auto &item : pbkdf2Cases) {
        const auto entry = item.toObject();
        const auto password = entry.value(QStringLiteral("password")).toString();
        const auto salt = QByteArray::fromHex(entry.value(QStringLiteral("salt_hex")).toString().toLatin1());
        const auto iterations = entry.value(QStringLiteral("iterations")).toInteger();
        const auto expectedKek = entry.value(QStringLiteral("kek_hex")).toString();

        const auto actual = CustomSync::Crypto::Pbkdf2(
            password.toUtf8(), salt, int(iterations), 32);

        check(QStringLiteral("pbkdf2/%1/%2").arg(password.left(8)).arg(iterations),
              QString::fromLatin1(actual.toHex()),
              expectedKek);
        ++pbkdf2Checked;
    }

    if (pbkdf2Checked != 3) {
        qCritical().noquote() << "XATO: pbkdf2 uchun 3 ta holat tekshirilishi kerak edi, lekin"
                              << pbkdf2Checked << "ta tekshirildi.";
        return 1;
    }
    const int pbkdf2Passed = pbkdf2Checked - (gFailures - pbkdf2FailuresBefore);
    qInfo().noquote() << QStringLiteral("pbkdf2 natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(pbkdf2Passed)
        .arg(pbkdf2Checked);

#ifdef Q_OS_WIN
    qInfo().noquote() << "\nOS Keystore (Windows DPAPI) tekshirilmoqda:";
    int keystoreChecked = 0;
    const int keystoreFailuresBefore = gFailures;

    // 1. Round-trip: 32 random bytes
    {
        keystoreChecked++;
        const auto random = CustomSync::Crypto::RandomBytes(32);
        const auto protectedBlob = CustomSync::Keystore::ProtectBytes(random);
        const auto recovered = protectedBlob.has_value()
            ? CustomSync::Keystore::UnprotectBytes(*protectedBlob)
            : std::nullopt;
        check("keystore/roundtrip (32 bayt random)",
              recovered.value_or(QByteArray()).toHex(),
              random.toHex());
    }

    // 2. Empty input: bo'sh QByteArray
    {
        keystoreChecked++;
        const auto emptyPlain = QByteArray();
        const auto protectedBlob = CustomSync::Keystore::ProtectBytes(emptyPlain);
        const auto recovered = protectedBlob.has_value()
            ? CustomSync::Keystore::UnprotectBytes(*protectedBlob)
            : std::nullopt;
        check("keystore/empty (bo'sh bayt to'g'ri qaytdi)",
              recovered.has_value() ? QString::fromLatin1(recovered->toHex()) : QStringLiteral("<nullopt>"),
              QStringLiteral(""));
    }

    // 3. Tampered blob: o'rtadagi bitta baytni o'zgartirish
    {
        keystoreChecked++;
        const auto random = CustomSync::Crypto::RandomBytes(32);
        const auto protectedBlob = CustomSync::Keystore::ProtectBytes(random);
        std::optional<QByteArray> result;
        if (protectedBlob.has_value() && protectedBlob->size() >= 10) {
            auto tampered = *protectedBlob;
            tampered[tampered.size() / 2] = tampered[tampered.size() / 2] ^ 0xFF;
            result = CustomSync::Keystore::UnprotectBytes(tampered);
        }
        check("keystore/tamper (buzilgan blob rad etildi)",
              result.has_value() ? QStringLiteral("accepted") : QStringLiteral("rejected"),
              QStringLiteral("rejected"));
    }

    // 4. Not-a-blob: 16 bayt axlat
    {
        keystoreChecked++;
        const QByteArray junk(16, 'x');
        const auto result = CustomSync::Keystore::UnprotectBytes(junk);
        check("keystore/junk (not-a-blob rad etildi)",
              result.has_value() ? QStringLiteral("accepted") : QStringLiteral("rejected"),
              QStringLiteral("rejected"));
    }

    constexpr int kExpectedKeystoreCases = 4;
    if (keystoreChecked != kExpectedKeystoreCases) {
        qWarning().noquote() << QStringLiteral("XATO: %1 ta keystore holati kutilgan edi, lekin %2 ta tekshirildi!")
            .arg(kExpectedKeystoreCases)
            .arg(keystoreChecked);
        return 1;
    }
    const int keystorePassed = keystoreChecked - (gFailures - keystoreFailuresBefore);
    qInfo().noquote() << QStringLiteral("keystore natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(keystorePassed)
        .arg(keystoreChecked);
#else
    qInfo().noquote() << "\nOS Keystore: Windows bo'lmagan platforma (skipped).";
#endif

    qInfo().noquote() << "\nrecord_id_from_master vektorlari tekshirilmoqda:";
    int masterRecordIdChecked = 0;
    const int masterRecordIdFailuresBefore = gFailures;

    // 1. deleted case (real account_hash path)
    {
        masterRecordIdChecked++;
        const auto actual = CustomSync::ComputeRecordIdFor(
            masterKey,
            QString::fromLatin1(CustomSync::Kind::Deleted),
            111222333LL,
            QStringLiteral("7053823996"),
            395278LL,
            1787000000LL);
        check("record_id_from_master/deleted",
              actual,
              QStringLiteral("25cc97f881e1b7dde2de8b11599e608476d8054e853a92f8d9b6619d6941cc0c"));
    }

    // 2. activity case (empty account_hash path per spec §0.12)
    {
        masterRecordIdChecked++;
        const auto actual = CustomSync::ComputeRecordIdFor(
            masterKey,
            QString::fromLatin1(CustomSync::Kind::Activity),
            111222333LL,
            QStringLiteral("7053823996"),
            0LL,
            1787000002LL);
        check("record_id_from_master/activity",
              actual,
              QStringLiteral("eea4dc5e11301c50d7460fc9ebc7f86be92f9dbf3b95ab661a004f4c57a3c919"));
    }

    // 3. media_index case with negative msg_id
    {
        masterRecordIdChecked++;
        const auto actual = CustomSync::ComputeRecordIdFor(
            masterKey,
            QString::fromLatin1(CustomSync::Kind::MediaIndex),
            111222333LL,
            QStringLiteral("7053823996"),
            -5190442718973336697LL,
            1787000004LL);
        check("record_id_from_master/media_index_negative",
              actual,
              QStringLiteral("f18caff74521dde269e0374db680581b1ff05cb8e8c4d54bab060758a6694854"));
    }

    constexpr int kExpectedMasterRecordIdCases = 3;
    if (masterRecordIdChecked != kExpectedMasterRecordIdCases) {
        qWarning().noquote() << QStringLiteral("XATO: %1 ta record_id_from_master holati kutilgan edi, lekin %2 ta tekshirildi!")
            .arg(kExpectedMasterRecordIdCases)
            .arg(masterRecordIdChecked);
        return 1;
    }
    const int masterRecordIdPassed = masterRecordIdChecked - (gFailures - masterRecordIdFailuresBefore);
    qInfo().noquote() << QStringLiteral("record_id_from_master natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(masterRecordIdPassed)
        .arg(masterRecordIdChecked);

    if (gFailures == 0) {
        qInfo().noquote() << "\nBarcha vektorlar va tekshiruvlar mos keldi.";
        return 0;
    } else {
        qWarning().noquote() << QStringLiteral("\n%1 ta nomuvofiqlik aniqlandi.").arg(gFailures);
        return 1;
    }
}
