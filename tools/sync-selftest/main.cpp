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
#include "custom_sync.h"
#include "custom_sync_keyshare.h"

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

    qInfo().noquote() << "\nScheduler NextAction mantiqi tekshirilmoqda:";
    int schedulerChecked = 0;
    const int schedulerFailuresBefore = gFailures;

    // 1. disabled -> never runs
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = false;
        s.inFlight = false;
        s.consecutiveFailures = 0;
        s.hasMore = false;
        s.catchUpCycles = 0;
        s.intervalSeconds = 30;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/disabled/runNow", d.runNow ? "true" : "false", "false");
        check("scheduler/disabled/delay", QString::number(d.delaySeconds), "0");
    }

    // 2. inFlight -> never runs, whatever else is true
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = true;
        s.inFlight = true;
        s.consecutiveFailures = 0;
        s.hasMore = true;
        s.catchUpCycles = 1;
        s.intervalSeconds = 30;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/inFlight/runNow", d.runNow ? "true" : "false", "false");
        check("scheduler/inFlight/delay", QString::number(d.delaySeconds), "0");
    }

    // 3. clean state -> runs after intervalSeconds
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = true;
        s.inFlight = false;
        s.consecutiveFailures = 0;
        s.hasMore = false;
        s.catchUpCycles = 0;
        s.intervalSeconds = 30;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/clean/runNow", d.runNow ? "true" : "false", "false");
        check("scheduler/clean/delay", QString::number(d.delaySeconds), "30");
    }

    // 4. hasMore and under the cap -> runs immediately
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = true;
        s.inFlight = false;
        s.consecutiveFailures = 0;
        s.hasMore = true;
        s.catchUpCycles = 2;
        s.intervalSeconds = 30;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/hasMore_under_cap/runNow", d.runNow ? "true" : "false", "true");
        check("scheduler/hasMore_under_cap/delay", QString::number(d.delaySeconds), "0");
    }

    // 5. hasMore and at the cap -> falls back to intervalSeconds
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = true;
        s.inFlight = false;
        s.consecutiveFailures = 0;
        s.hasMore = true;
        s.catchUpCycles = 5;
        s.intervalSeconds = 30;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/hasMore_at_cap/runNow", d.runNow ? "true" : "false", "false");
        check("scheduler/hasMore_at_cap/delay", QString::number(d.delaySeconds), "30");
    }

    // 6. failures 1,2,3 -> the delay doubles
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = true;
        s.inFlight = false;
        s.hasMore = false;
        s.catchUpCycles = 0;
        s.intervalSeconds = 30;

        s.consecutiveFailures = 1;
        const auto d1 = CustomSync::NextAction(s);
        s.consecutiveFailures = 2;
        const auto d2 = CustomSync::NextAction(s);
        s.consecutiveFailures = 3;
        const auto d3 = CustomSync::NextAction(s);

        check("scheduler/failures_1_2_3/d1", QString::number(d1.delaySeconds), "30");
        check("scheduler/failures_1_2_3/d2", QString::number(d2.delaySeconds), "60");
        check("scheduler/failures_1_2_3/d3", QString::number(d3.delaySeconds), "120");
    }

    // 7. failures large -> capped at 300 seconds
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = true;
        s.inFlight = false;
        s.hasMore = false;
        s.catchUpCycles = 0;
        s.intervalSeconds = 30;
        s.consecutiveFailures = 20;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/failures_large/runNow", d.runNow ? "true" : "false", "false");
        check("scheduler/failures_large/delay", QString::number(d.delaySeconds), "300");
    }

    // 8. failure count reset -> back to intervalSeconds
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = true;
        s.inFlight = false;
        s.hasMore = false;
        s.catchUpCycles = 0;
        s.intervalSeconds = 30;
        s.consecutiveFailures = 0;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/failure_reset/runNow", d.runNow ? "true" : "false", "false");
        check("scheduler/failure_reset/delay", QString::number(d.delaySeconds), "30");
    }

    // 9. pendingNotify, not in flight, under the cap -> runs immediately
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = true;
        s.inFlight = false;
        s.consecutiveFailures = 0;
        s.hasMore = false;
        s.pendingNotify = true;
        s.catchUpCycles = 1;
        s.intervalSeconds = 30;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/pendingNotify_under_cap/runNow", d.runNow ? "true" : "false", "true");
        check("scheduler/pendingNotify_under_cap/delay", QString::number(d.delaySeconds), "0");
    }

    // 10. pendingNotify while disabled -> never runs
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = false;
        s.inFlight = false;
        s.consecutiveFailures = 0;
        s.hasMore = false;
        s.pendingNotify = true;
        s.catchUpCycles = 0;
        s.intervalSeconds = 30;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/pendingNotify_disabled/runNow", d.runNow ? "true" : "false", "false");
        check("scheduler/pendingNotify_disabled/delay", QString::number(d.delaySeconds), "0");
    }

    // 11. pendingNotify while inFlight -> does not run
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = true;
        s.inFlight = true;
        s.consecutiveFailures = 0;
        s.hasMore = false;
        s.pendingNotify = true;
        s.catchUpCycles = 0;
        s.intervalSeconds = 30;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/pendingNotify_inFlight/runNow", d.runNow ? "true" : "false", "false");
        check("scheduler/pendingNotify_inFlight/delay", QString::number(d.delaySeconds), "0");
    }

    // 12. pendingNotify at the catch-up cap -> falls back to intervalSeconds
    {
        schedulerChecked++;
        CustomSync::SchedulerState s;
        s.enabled = true;
        s.inFlight = false;
        s.consecutiveFailures = 0;
        s.hasMore = false;
        s.pendingNotify = true;
        s.catchUpCycles = CustomSync::kMaxCatchUpCycles;
        s.intervalSeconds = 30;
        const auto d = CustomSync::NextAction(s);
        check("scheduler/pendingNotify_at_cap/runNow", d.runNow ? "true" : "false", "false");
        check("scheduler/pendingNotify_at_cap/delay", QString::number(d.delaySeconds), "30");
    }

    constexpr int kExpectedSchedulerCases = 12;
    if (schedulerChecked != kExpectedSchedulerCases) {
        qWarning().noquote() << QStringLiteral("XATO: %1 ta scheduler holati kutilgan edi, lekin %2 ta tekshirildi!")
            .arg(kExpectedSchedulerCases)
            .arg(schedulerChecked);
        return 1;
    }
    const int schedulerPassed = schedulerChecked - (gFailures - schedulerFailuresBefore);
    qInfo().noquote() << QStringLiteral("scheduler natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(schedulerPassed)
        .arg(schedulerChecked);

    qInfo() << "\nKeyShare (Wrap/Unwrap/Parser) tekshirilmoqda:";
    int keyshareChecked = 0;
    const int keyshareFailuresBefore = gFailures;

    // 1. Round trip
    {
        keyshareChecked++;
        const auto masterKey = QByteArray("01234567890123456789012345678901", 32);
        const QString passphrase = QStringLiteral("secret_pass_123");
        const QString label = QStringLiteral("laptop-wrap");
        const auto wrap = CustomSync::KeyShare::WrapMasterKey(masterKey, passphrase, label);
        check("keyshare/roundtrip/wrapType", wrap.wrapType, QStringLiteral("passphrase"));
        check("keyshare/roundtrip/label", wrap.label, label);
        check("keyshare/roundtrip/iterations", QString::number(wrap.iterations), QString::number(CustomSync::KeyShare::kPassphraseIterations));
        const auto unwrapped = CustomSync::KeyShare::UnwrapMasterKey(wrap, passphrase);
        check("keyshare/roundtrip/has_value", unwrapped.has_value() ? "true" : "false", "true");
        if (unwrapped.has_value()) {
            check("keyshare/roundtrip/key_match", QString::fromLatin1(unwrapped->toHex()), QString::fromLatin1(masterKey.toHex()));
        }
    }

    // 2. Wrong passphrase -> empty optional
    {
        keyshareChecked++;
        const auto masterKey = QByteArray("01234567890123456789012345678901", 32);
        const auto wrap = CustomSync::KeyShare::WrapMasterKey(masterKey, QStringLiteral("correct_pass"), QStringLiteral("dev"));
        const auto unwrapped = CustomSync::KeyShare::UnwrapMasterKey(wrap, QStringLiteral("wrong_pass"));
        check("keyshare/wrong_passphrase/empty", unwrapped.has_value() ? "false" : "true", "true");
    }

    // 3. Tampered wrappedKey (flip one byte) -> empty optional
    {
        keyshareChecked++;
        const auto masterKey = QByteArray("01234567890123456789012345678901", 32);
        const QString pass = QStringLiteral("tamper_test_pass");
        auto wrap = CustomSync::KeyShare::WrapMasterKey(masterKey, pass, QStringLiteral("dev"));
        if (!wrap.wrappedKey.isEmpty()) {
            wrap.wrappedKey[0] = static_cast<char>(wrap.wrappedKey[0] ^ 0xFF);
        }
        const auto unwrapped = CustomSync::KeyShare::UnwrapMasterKey(wrap, pass);
        check("keyshare/tampered_wrappedKey/empty", unwrapped.has_value() ? "false" : "true", "true");
    }

    // 4. Tampered salt -> empty optional
    {
        keyshareChecked++;
        const auto masterKey = QByteArray("01234567890123456789012345678901", 32);
        const QString pass = QStringLiteral("tamper_salt_pass");
        auto wrap = CustomSync::KeyShare::WrapMasterKey(masterKey, pass, QStringLiteral("dev"));
        if (!wrap.salt.isEmpty()) {
            wrap.salt[0] = static_cast<char>(wrap.salt[0] ^ 0xAA);
        }
        const auto unwrapped = CustomSync::KeyShare::UnwrapMasterKey(wrap, pass);
        check("keyshare/tampered_salt/empty", unwrapped.has_value() ? "false" : "true", "true");
    }

    // 5. Two wraps of same key with same pass have different salt and nonce, and both unwrap
    {
        keyshareChecked++;
        const auto masterKey = QByteArray("01234567890123456789012345678901", 32);
        const QString pass = QStringLiteral("fresh_salt_nonce_pass");
        const auto wrap1 = CustomSync::KeyShare::WrapMasterKey(masterKey, pass, QStringLiteral("dev1"));
        const auto wrap2 = CustomSync::KeyShare::WrapMasterKey(masterKey, pass, QStringLiteral("dev2"));

        const bool saltsDifferent = (wrap1.salt != wrap2.salt);
        const bool noncesDifferent = (wrap1.nonce != wrap2.nonce);
        check("keyshare/fresh_salt_different", saltsDifferent ? "true" : "false", "true");
        check("keyshare/fresh_nonce_different", noncesDifferent ? "true" : "false", "true");

        const auto unwrap1 = CustomSync::KeyShare::UnwrapMasterKey(wrap1, pass);
        const auto unwrap2 = CustomSync::KeyShare::UnwrapMasterKey(wrap2, pass);
        check("keyshare/fresh_unwrap1_match", unwrap1.has_value() && *unwrap1 == masterKey ? "true" : "false", "true");
        check("keyshare/fresh_unwrap2_match", unwrap2.has_value() && *unwrap2 == masterKey ? "true" : "false", "true");
    }

    // 6. iterations read from wrap, not assumed: custom iteration count honours it
    {
        keyshareChecked++;
        const auto masterKey = QByteArray("fedcba9876543210fedcba9876543210", 32);
        const QString pass = QStringLiteral("custom_iter_pass");
        const int customIterations = 10000;
        const auto salt = CustomSync::Crypto::RandomBytes(16);
        const auto nonce = CustomSync::Crypto::RandomBytes(12);
        const auto kek = CustomSync::Crypto::Pbkdf2(pass.toUtf8(), salt, customIterations, 32);
        const auto wrappedKey = CustomSync::Crypto::Seal(kek, nonce, masterKey);

        CustomSync::KeyShare::Wrap wrap;
        wrap.wrapType = QStringLiteral("passphrase");
        wrap.label = QStringLiteral("custom-iter");
        wrap.salt = salt;
        wrap.nonce = nonce;
        wrap.wrappedKey = wrappedKey;
        wrap.iterations = customIterations;

        const auto unwrapped = CustomSync::KeyShare::UnwrapMasterKey(wrap, pass);
        check("keyshare/custom_iterations/has_value", unwrapped.has_value() ? "true" : "false", "true");
        if (unwrapped.has_value()) {
            check("keyshare/custom_iterations/key_match", QString::fromLatin1(unwrapped->toHex()), QString::fromLatin1(masterKey.toHex()));
        }
    }

    // 7. Parsing: literal snake_case JSON body with base64 fields, assert every field and base64 decoding
    {
        keyshareChecked++;
        const QByteArray expectedSalt = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f");
        const QByteArray expectedNonce = QByteArray::fromHex("101112131415161718191a1b");
        const QByteArray expectedWrappedKey = QByteArray::fromHex("202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f");

        const QString saltB64 = QString::fromLatin1(expectedSalt.toBase64());
        const QString nonceB64 = QString::fromLatin1(expectedNonce.toBase64());
        const QString wrappedKeyB64 = QString::fromLatin1(expectedWrappedKey.toBase64());

        const QByteArray json = QString(
            "{\n"
            "  \"wrap_id\": \"wrap_test_777\",\n"
            "  \"wrap_type\": \"passphrase\",\n"
            "  \"label\": \"Device Alfa\",\n"
            "  \"salt\": \"%1\",\n"
            "  \"nonce\": \"%2\",\n"
            "  \"wrapped_key\": \"%3\",\n"
            "  \"iterations\": 600000\n"
            "}"
        ).arg(saltB64, nonceB64, wrappedKeyB64).toUtf8();

        const auto res = CustomSync::KeyShare::ParseKeyWrapResponse(json, 200);
        check("keyshare/parsing/error_empty", res.error.isEmpty() ? "true" : "false", "true");
        check("keyshare/parsing/wrap_id", res.wrap.wrapId, QStringLiteral("wrap_test_777"));
        check("keyshare/parsing/wrap_type", res.wrap.wrapType, QStringLiteral("passphrase"));
        check("keyshare/parsing/label", res.wrap.label, QStringLiteral("Device Alfa"));
        check("keyshare/parsing/iterations", QString::number(res.wrap.iterations), QStringLiteral("600000"));
        check("keyshare/parsing/salt_bytes", QString::fromLatin1(res.wrap.salt.toHex()), QString::fromLatin1(expectedSalt.toHex()));
        check("keyshare/parsing/nonce_bytes", QString::fromLatin1(res.wrap.nonce.toHex()), QString::fromLatin1(expectedNonce.toHex()));
        check("keyshare/parsing/wrapped_key_bytes", QString::fromLatin1(res.wrap.wrappedKey.toHex()), QString::fromLatin1(expectedWrappedKey.toHex()));
        check("keyshare/parsing/base64_not_hex", (res.wrap.salt == expectedSalt && res.wrap.salt != QByteArray::fromHex(saltB64.toLatin1())) ? "true" : "false", "true");
    }

    constexpr int kExpectedKeyShareCases = 7;
    if (keyshareChecked != kExpectedKeyShareCases) {
        qWarning().noquote() << QStringLiteral("XATO: %1 ta keyshare holati kutilgan edi, lekin %2 ta tekshirildi!")
            .arg(kExpectedKeyShareCases)
            .arg(keyshareChecked);
        return 1;
    }
    const int keysharePassed = keyshareChecked - (gFailures - keyshareFailuresBefore);
    qInfo().noquote() << QStringLiteral("keyshare natijasi: %1/%2 holat muvaffaqiyatli o'tdi.")
        .arg(keysharePassed)
        .arg(keyshareChecked);

    if (gFailures == 0) {
        qInfo().noquote() << "\nBarcha vektorlar va tekshiruvlar mos keldi.";
        return 0;
    } else {
        qWarning().noquote() << QStringLiteral("\n%1 ta nomuvofiqlik aniqlandi.").arg(gFailures);
        return 1;
    }
}
