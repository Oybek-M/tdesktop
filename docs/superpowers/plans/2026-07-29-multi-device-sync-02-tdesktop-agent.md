# tdesktop Sync Agent Implementation Plan (02)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** tdesktop'ga sync agenti qo'shish — lokal capture natijalarini shifrlab serverga yuborish, boshqa qurilmalar yozuvlarini qabul qilib lokal SQLite'ga merge qilish, va bularning barchasini mavjud capture kodiga **umuman tegmasdan** qilish.

**Architecture:** Yangi kod olti fokusli modulga bo'lingan. Ulardan ikkitasi (`custom_sync_record`, `custom_sync_crypto`) **tdesktop'ga umuman bog'liq emas** — faqat QtCore va OpenSSL. Bu ataylab: shu ikkisi alohida kichik test dasturida sekundlar ichida kompilyatsiya qilinadi va `test-vectors.json` ga qarshi tekshiriladi, 34 daqiqalik to'liq build'ni kutmasdan.

**Tech Stack:** C++20, Qt 5.15.18 (QtCore, QtNetwork), OpenSSL (allaqachon linklangan), SQLite (mavjud `custom_db` ulanishi orqali).

**Kirish sharti:** [01b](2026-07-29-multi-device-sync-01b-backend-sync.md) tugagan va `docs/sync-protocol/test-vectors.json` mavjud. Ishlaydigan server (lokal yoki VPS) kerak.

**Umumiy qoidalar:** [00-index](2026-07-29-multi-device-sync-00-index.md) dagi K1–K7. Ayniqsa **K5**: sync o'chiq bo'lganda regressiya nolga teng bo'lishi kerak.

---

## Build haqida muhim eslatma

tdesktop'ning to'liq build'i bu mashinada **~34 daqiqa** oladi va boshqa
og'ir ilovalar bilan raqobatlashadi. Shuning uchun bu plan ataylab shunday
tuzilgan:

- Task 1–2 (`record` va `crypto`) **alohida kichik dasturda** sinaladi —
  har bir iteratsiya sekundlar oladi, to'liq build kerak emas.
- To'liq build faqat **uch marta** talab qilinadi: Task 6 dan keyin
  (birinchi uchi-uchiga sinov), Task 10 dan keyin (UI), va oxirida.
- Har bir to'liq build'dan **oldin foydalanuvchidan so'ralishi shart**
  (bu loyihaning qat'iy qoidasi).

---

## File Structure

```
Telegram/SourceFiles/
├── custom_sync_record.h / .cpp     # record_id, yozuv strukturasi, JSON.
│                                   # tdesktop'ga bog'liq EMAS.
├── custom_sync_crypto.h / .cpp     # AES-256-GCM, PBKDF2, HKDF, HMAC.
│                                   # tdesktop'ga bog'liq EMAS.
├── custom_sync_keystore.h / .cpp   # Master kalitni OS keystore'da saqlash.
├── custom_sync_outbox.h / .cpp     # sync_outbox / sync_state SQLite qatlami.
├── custom_sync_client.h / .cpp     # HTTP: enroll, push, pull, media.
└── custom_sync.h / .cpp            # Orkestrator: timer, fon oqimi, holat.

Telegram/SourceFiles/custom_db.cpp  # v6 migratsiya + 4 ta enqueue chaqiruvi
Telegram/SourceFiles/custom_settings.h / .cpp   # sync sozlamalari
Telegram/SourceFiles/custom_mod_window.cpp      # "☁️ Sinxronizatsiya" bo'limi
Telegram/CMakeLists.txt                          # yangi fayllar

tools/sync-selftest/                # Mustaqil test dasturi (Qt + OpenSSL)
├── CMakeLists.txt
└── main.cpp
```

**Nima uchun olti fayl, bitta emas:** har birining bitta aniq vazifasi bor
va alohida tushunilishi mumkin. Bundan tashqari `record` va `crypto` ning
mustaqilligi ularni tez sinash imkonini beradi — bu build vaqti tufayli
amaliy zaruriyat.

---

## Task 1: Mustaqil test dasturi va `record_id`

**Files:**
- Create: `tools/sync-selftest/CMakeLists.txt`
- Create: `tools/sync-selftest/main.cpp`
- Create: `Telegram/SourceFiles/custom_sync_record.h`
- Create: `Telegram/SourceFiles/custom_sync_record.cpp`

- [ ] **Step 1: Test dasturining skeletini yozish**

`tools/sync-selftest/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(sync_selftest CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core REQUIRED)
find_package(OpenSSL REQUIRED)

# Ataylab faqat tdesktop'ga bog'liq bo'lmagan fayllar.
add_executable(sync_selftest
    main.cpp
    ../../Telegram/SourceFiles/custom_sync_record.cpp
    ../../Telegram/SourceFiles/custom_sync_crypto.cpp
)

target_include_directories(sync_selftest PRIVATE
    ../../Telegram/SourceFiles)

target_link_libraries(sync_selftest PRIVATE
    Qt5::Core OpenSSL::Crypto)
```

`tools/sync-selftest/main.cpp`:

```cpp
// Mustaqil tekshiruv dasturi. tdesktop'ni build qilmasdan
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

} // namespace

int main(int argc, char *argv[]) {
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

    qInfo() << "record_id vektorlari:";
    for (const auto &item : root.value(u"record_id"_qs).toArray()) {
        const auto entry = item.toObject();
        const auto in = entry.value(u"input"_qs).toObject();
        const auto actual = CustomSync::ComputeRecordId(
            in.value(u"kind"_qs).toString(),
            in.value(u"peer_hash"_qs).toString(),
            qint64(in.value(u"msg_id"_qs).toDouble()),
            qint64(in.value(u"occurred_at"_qs).toDouble()));
        check(in.value(u"kind"_qs).toString(), actual,
              entry.value(u"expected"_qs).toString());
    }

    qInfo() << (gFailures == 0
        ? "\nBarcha vektorlar mos keldi."
        : QString("\n%1 ta nomuvofiqlik.").arg(gFailures).toUtf8().constData());
    return gFailures == 0 ? 0 : 1;
}
```

**Eslatma:** `u"..."_qs` — Qt 5.15 da `QStringLiteral` ning qisqartmasi
emas; agar kompilyator qabul qilmasa `QStringLiteral("...")` ishlating.

- [ ] **Step 2: Testni ishga tushirib, yiqilishini ko'rish**

```bash
cmake -S tools/sync-selftest -B build/selftest
cmake --build build/selftest
```

Kutilgan: FAIL — `custom_sync_record.h` mavjud emas.

- [ ] **Step 3: `custom_sync_record` ni yozish**

`Telegram/SourceFiles/custom_sync_record.h`:

```cpp
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
} // namespace Kind

struct MediaRef {
    QString hash;
    qint64 size = 0;
    QByteArray nonce;
};

struct Record {
    QString recordId;
    QString kind;
    QString peerHash;
    qint64 msgId = 0;
    qint64 occurredAt = 0;
    qint64 observedAt = 0;
    QString deviceId;
    QByteArray nonce;
    QByteArray payload;
    QVector<MediaRef> media;
};

// record_id = hex(SHA256(kind ‖ 0x00 ‖ peerHash ‖ 0x00 ‖
//                        msgId ‖ 0x00 ‖ occurredAt))
//
// Beshala platforma bayt-ma-bayt bir xil natija berishi SHART.
// Har qanday o'zgarish oldingi barcha yozuvlarni yaroqsiz qiladi.
[[nodiscard]] QString ComputeRecordId(
    const QString &kind,
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
```

`Telegram/SourceFiles/custom_sync_record.cpp`:

```cpp
#include "custom_sync_record.h"

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
        const QString &peerHash,
        qint64 msgId,
        qint64 occurredAt) {
    QByteArray buffer;
    appendField(buffer, kind.toUtf8());
    appendField(buffer, peerHash.toUtf8());
    appendField(buffer, QByteArray::number(msgId));
    // Oxirgi maydondan keyin ajratuvchi qo'yilmaydi — server
    // implementatsiyasi bilan aynan mos bo'lishi uchun.
    buffer.append(QByteArray::number(occurredAt));

    return QString::fromLatin1(
        QCryptographicHash::hash(buffer, QCryptographicHash::Sha256).toHex());
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
            { QStringLiteral("size"), double(item.size) },
            { QStringLiteral("nonce"),
              QString::fromLatin1(item.nonce.toBase64()) },
        });
    }

    return QJsonObject{
        { QStringLiteral("record_id"),   record.recordId },
        { QStringLiteral("kind"),        record.kind },
        { QStringLiteral("peer_hash"),   record.peerHash },
        { QStringLiteral("msg_id"),      double(record.msgId) },
        { QStringLiteral("occurred_at"), double(record.occurredAt) },
        { QStringLiteral("observed_at"), double(record.observedAt) },
        { QStringLiteral("device_id"),   record.deviceId },
        { QStringLiteral("nonce"),   QString::fromLatin1(record.nonce.toBase64()) },
        { QStringLiteral("payload"), QString::fromLatin1(record.payload.toBase64()) },
        { QStringLiteral("media"),   media },
    };
}

Record FromJson(const QJsonObject &object) {
    Record record;
    record.recordId   = object.value(QStringLiteral("record_id")).toString();
    record.kind       = object.value(QStringLiteral("kind")).toString();
    record.peerHash   = object.value(QStringLiteral("peer_hash")).toString();
    record.msgId      = qint64(object.value(QStringLiteral("msg_id")).toDouble());
    record.occurredAt = qint64(object.value(QStringLiteral("occurred_at")).toDouble());
    record.observedAt = qint64(object.value(QStringLiteral("observed_at")).toDouble());
    record.deviceId   = object.value(QStringLiteral("device_id")).toString();
    record.nonce   = QByteArray::fromBase64(
        object.value(QStringLiteral("nonce")).toString().toLatin1());
    record.payload = QByteArray::fromBase64(
        object.value(QStringLiteral("payload")).toString().toLatin1());

    for (const auto &item : object.value(QStringLiteral("media")).toArray()) {
        const auto entry = item.toObject();
        record.media.append(MediaRef{
            entry.value(QStringLiteral("hash")).toString(),
            qint64(entry.value(QStringLiteral("size")).toDouble()),
            QByteArray::fromBase64(
                entry.value(QStringLiteral("nonce")).toString().toLatin1()),
        });
    }
    return record;
}

} // namespace CustomSync
```

**Diqqat — `msg_id` va JSON:** `msgId` `double` orqali o'tkazilmoqda,
bu 2⁵³ dan katta qiymatlarda aniqlikni yo'qotadi. Telegram msg_id lari
bunchalik katta emas, lekin `occurred_at` va `discriminator` uchun bu
xavfli. Task 2 da bu tekshiriladi va zarur bo'lsa satr sifatida
uzatishga o'tiladi.

- [ ] **Step 4: `crypto` uchun bo'sh stub yaratish**

Test dasturi ikkala faylni ham kompilyatsiya qilgani uchun, hozircha
bo'sh `custom_sync_crypto.h/.cpp` yarating (Task 2 da to'ldiriladi):

```cpp
// custom_sync_crypto.h
#pragma once
namespace CustomSync { }
```

```cpp
// custom_sync_crypto.cpp
#include "custom_sync_crypto.h"
```

- [ ] **Step 5: Testni ishga tushirish**

```bash
cmake --build build/selftest
./build/selftest/sync_selftest docs/sync-protocol/test-vectors.json
```

Kutilgan chiqish:

```
record_id vektorlari:
  ok   deleted
  ok   edited
  ok   activity
  ok   deleted
  ok   ab
  ok   a

Barcha vektorlar mos keldi.
```

Agar nomuvofiqlik bo'lsa — bu haqiqiy interop bug'i. Server
implementatsiyasi bilan bayt darajasida solishtiring (ayniqsa oxirgi
maydondan keyingi ajratuvchi masalasini).

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: add record contract and record_id with a standalone verifier

custom_sync_record deliberately depends on nothing from tdesktop, only
QtCore. That lets tools/sync-selftest compile and check it against the
published vectors in seconds instead of waiting on a 34-minute full build,
which matters because record_id has to match the server byte for byte or
dedup silently stops working."
```

---

## Task 2: Kriptografik primitivlar

**Files:**
- Modify: `Telegram/SourceFiles/custom_sync_crypto.h` / `.cpp`
- Modify: `tools/sync-selftest/main.cpp`

- [ ] **Step 1: Testni kengaytirish**

`main.cpp` da `record_id` blokidan keyin qo'shing:

```cpp
#include "custom_sync_crypto.h"

// ... main() ichida:

    qInfo() << "\nPBKDF2 vektorlari:";
    for (const auto &item : root.value(u"pbkdf2_hmac_sha256"_qs).toArray()) {
        const auto entry = item.toObject();
        const auto in = entry.value(u"input"_qs).toObject();
        const auto derived = CustomSync::Crypto::Pbkdf2(
            in.value(u"password"_qs).toString().toUtf8(),
            QByteArray::fromHex(in.value(u"salt_hex"_qs).toString().toLatin1()),
            in.value(u"iterations"_qs).toInt(),
            32);
        check(QStringLiteral("pbkdf2/%1").arg(in.value(u"iterations"_qs).toInt()),
              QString::fromLatin1(derived.toHex()),
              entry.value(u"expected_key_hex"_qs).toString());
    }

    qInfo() << "\nHKDF vektorlari:";
    for (const auto &item : root.value(u"hkdf_sha256"_qs).toArray()) {
        const auto entry = item.toObject();
        const auto in = entry.value(u"input"_qs).toObject();
        const auto derived = CustomSync::Crypto::HkdfSha256(
            QByteArray::fromHex(in.value(u"master_key_hex"_qs).toString().toLatin1()),
            QByteArray::fromHex(in.value(u"salt_hex"_qs).toString().toLatin1()),
            in.value(u"info"_qs).toString().toUtf8(),
            32);
        check(in.value(u"info"_qs).toString(),
              QString::fromLatin1(derived.toHex()),
              entry.value(u"expected_key_hex"_qs).toString());
    }

    qInfo() << "\nAES-256-GCM aylanma sinov:";
    {
        const QByteArray key(32, '\x11');
        const QByteArray nonce(12, '\x22');
        const QByteArray plaintext = "CustomMod sync test payload";
        const auto sealed = CustomSync::Crypto::Seal(key, nonce, plaintext);
        const auto opened = CustomSync::Crypto::Open(key, nonce, sealed);
        check(QStringLiteral("seal/open"),
              QString::fromUtf8(opened.value_or(QByteArray("<XATO>"))),
              QString::fromUtf8(plaintext));

        auto tampered = sealed;
        tampered[tampered.size() - 1] =
            char(tampered[tampered.size() - 1] ^ 0x01);
        check(QStringLiteral("buzilgan tag rad etiladi"),
              CustomSync::Crypto::Open(key, nonce, tampered).has_value()
                  ? QStringLiteral("qabul qilindi")
                  : QStringLiteral("rad etildi"),
              QStringLiteral("rad etildi"));
    }
```

- [ ] **Step 2: Testni ishga tushirib, yiqilishini ko'rish**

```bash
cmake --build build/selftest
```

Kutilgan: FAIL — `CustomSync::Crypto` mavjud emas.

- [ ] **Step 3: `custom_sync_crypto.h` ni yozish**

```cpp
#pragma once

#include <QtCore/QByteArray>
#include <optional>

// Kriptografik primitivlar — OpenSSL ustidan yupqa qatlam.
//
// Bu fayl ham ATAYLAB tdesktop'ga bog'liq emas (faqat QtCore + OpenSSL),
// shuning uchun tools/sync-selftest da tez sinaladi.
//
// Barcha funksiyalar test-vectors.json ga mos kelishi SHART.

namespace CustomSync::Crypto {

// PBKDF2-HMAC-SHA256. Paroldan kalit shifrlash kaliti (KEK) hosil qilish.
// Iteratsiya: oddiy o'ramlar uchun 600000, email escrow uchun 2000000
// (spec 4.4.1 — email qismi qo'lga tushganda PIN'ni offline brute-force
// qilish narxini oshirish uchun).
[[nodiscard]] QByteArray Pbkdf2(
    const QByteArray &password,
    const QByteArray &salt,
    int iterations,
    int keyLength);

// HKDF-SHA256. Master kalitdan maqsadga xos kichik kalitlar chiqarish.
//
// MUHIM: salt HAR DOIM oshkora beriladi (32 baytlik nol) — "salt yo'q"
// holatini kutubxonalar turlicha talqin qiladi va bu jimgina interop
// buzilishiga olib keladi.
[[nodiscard]] QByteArray HkdfSha256(
    const QByteArray &masterKey,
    const QByteArray &salt,
    const QByteArray &info,
    int keyLength);

[[nodiscard]] QByteArray HmacSha256(
    const QByteArray &key,
    const QByteArray &message);

// AES-256-GCM. Natija: ciphertext ‖ tag (16 bayt).
[[nodiscard]] QByteArray Seal(
    const QByteArray &key,
    const QByteArray &nonce,
    const QByteArray &plaintext);

// Muvaffaqiyatsiz bo'lsa (tag mos kelmasa) bo'sh optional qaytaradi.
// Chaqiruvchi buni ALBATTA tekshirishi kerak — buzilgan yozuv ilovani
// yiqitmasligi kerak.
[[nodiscard]] std::optional<QByteArray> Open(
    const QByteArray &key,
    const QByteArray &nonce,
    const QByteArray &sealed);

[[nodiscard]] QByteArray RandomBytes(int count);

// Master kalitning barmoq izi — .cmx import qilishdan oldin kalit mos
// kelishini tekshirish uchun. Domen ajratilgan: master kalitning
// to'g'ridan-to'g'ri hash'i emas.
[[nodiscard]] QString KeyFingerprint(const QByteArray &masterKey);

} // namespace CustomSync::Crypto
```

- [ ] **Step 4: `custom_sync_crypto.cpp` ni yozish**

```cpp
#include "custom_sync_crypto.h"

#include <QtCore/QCryptographicHash>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

namespace CustomSync::Crypto {
namespace {

constexpr auto kNonceSize = 12;
constexpr auto kTagSize = 16;

} // namespace

QByteArray Pbkdf2(
        const QByteArray &password,
        const QByteArray &salt,
        int iterations,
        int keyLength) {
    QByteArray result(keyLength, '\0');
    const auto ok = PKCS5_PBKDF2_HMAC(
        password.constData(), password.size(),
        reinterpret_cast<const unsigned char*>(salt.constData()), salt.size(),
        iterations, EVP_sha256(), keyLength,
        reinterpret_cast<unsigned char*>(result.data()));
    return ok == 1 ? result : QByteArray();
}

QByteArray HkdfSha256(
        const QByteArray &masterKey,
        const QByteArray &salt,
        const QByteArray &info,
        int keyLength) {
    auto *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!ctx) return QByteArray();

    QByteArray result(keyLength, '\0');
    size_t outLength = size_t(keyLength);
    auto ok = (EVP_PKEY_derive_init(ctx) == 1)
        && (EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256()) == 1)
        && (EVP_PKEY_CTX_set1_hkdf_salt(
                ctx,
                reinterpret_cast<const unsigned char*>(salt.constData()),
                salt.size()) == 1)
        && (EVP_PKEY_CTX_set1_hkdf_key(
                ctx,
                reinterpret_cast<const unsigned char*>(masterKey.constData()),
                masterKey.size()) == 1)
        && (EVP_PKEY_CTX_add1_hkdf_info(
                ctx,
                reinterpret_cast<const unsigned char*>(info.constData()),
                info.size()) == 1)
        && (EVP_PKEY_derive(
                ctx,
                reinterpret_cast<unsigned char*>(result.data()),
                &outLength) == 1);

    EVP_PKEY_CTX_free(ctx);
    return ok ? result : QByteArray();
}

QByteArray HmacSha256(const QByteArray &key, const QByteArray &message) {
    unsigned int length = 0;
    QByteArray result(EVP_MAX_MD_SIZE, '\0');
    HMAC(EVP_sha256(),
        key.constData(), key.size(),
        reinterpret_cast<const unsigned char*>(message.constData()), message.size(),
        reinterpret_cast<unsigned char*>(result.data()), &length);
    result.resize(int(length));
    return result;
}

QByteArray Seal(
        const QByteArray &key,
        const QByteArray &nonce,
        const QByteArray &plaintext) {
    auto *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return QByteArray();

    QByteArray output(plaintext.size() + kTagSize, '\0');
    auto *out = reinterpret_cast<unsigned char*>(output.data());
    int length = 0;
    int total = 0;
    auto ok = false;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) == 1
        && EVP_EncryptInit_ex(ctx, nullptr, nullptr,
               reinterpret_cast<const unsigned char*>(key.constData()),
               reinterpret_cast<const unsigned char*>(nonce.constData())) == 1
        && EVP_EncryptUpdate(ctx, out, &length,
               reinterpret_cast<const unsigned char*>(plaintext.constData()),
               plaintext.size()) == 1) {
        total = length;
        if (EVP_EncryptFinal_ex(ctx, out + total, &length) == 1) {
            total += length;
            ok = (EVP_CIPHER_CTX_ctrl(
                ctx, EVP_CTRL_GCM_GET_TAG, kTagSize, out + total) == 1);
            total += kTagSize;
        }
    }

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return QByteArray();
    output.resize(total);
    return output;
}

std::optional<QByteArray> Open(
        const QByteArray &key,
        const QByteArray &nonce,
        const QByteArray &sealed) {
    if (sealed.size() < kTagSize) return std::nullopt;

    auto *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return std::nullopt;

    const auto bodySize = sealed.size() - kTagSize;
    const auto *body = reinterpret_cast<const unsigned char*>(sealed.constData());
    auto *tag = const_cast<unsigned char*>(body + bodySize);

    QByteArray output(bodySize, '\0');
    auto *out = reinterpret_cast<unsigned char*>(output.data());
    int length = 0;
    int total = 0;
    auto ok = false;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) == 1
        && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
               reinterpret_cast<const unsigned char*>(key.constData()),
               reinterpret_cast<const unsigned char*>(nonce.constData())) == 1
        && EVP_DecryptUpdate(ctx, out, &length, body, bodySize) == 1) {
        total = length;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagSize, tag) == 1) {
            // Final EVP_DecryptFinal_ex tag'ni tekshiradi. 1 dan farqli
            // qiymat = ma'lumot buzilgan yoki kalit noto'g'ri.
            ok = (EVP_DecryptFinal_ex(ctx, out + total, &length) == 1);
            total += length;
        }
    }

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return std::nullopt;
    output.resize(total);
    return output;
}

QByteArray RandomBytes(int count) {
    QByteArray result(count, '\0');
    if (RAND_bytes(
            reinterpret_cast<unsigned char*>(result.data()), count) != 1) {
        return QByteArray();
    }
    return result;
}

QString KeyFingerprint(const QByteArray &masterKey) {
    QByteArray buffer = QByteArrayLiteral("customsync-fingerprint-v1");
    buffer.append(masterKey);
    const auto digest = QCryptographicHash::hash(
        buffer, QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.left(8).toHex());
}

} // namespace CustomSync::Crypto
```

- [ ] **Step 5: Testni ishga tushirish**

```bash
cmake --build build/selftest
./build/selftest/sync_selftest docs/sync-protocol/test-vectors.json
```

Kutilgan: barcha `record_id`, `pbkdf2`, `hkdf` vektorlari `ok`, hamda
`seal/open` va `buzilgan tag rad etiladi` — `ok`.

**Agar HKDF nomuvofiq bo'lsa:** salt masalasini tekshiring. Ikkala tomon
ham 32 baytlik oshkora nol salt ishlatishi kerak.

**Agar PBKDF2 2 000 000 iteratsiyada sekin bo'lsa:** bu normal, bir necha
soniya oladi. Bu ataylab — offline brute-force narxini oshiradi.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: add AES-GCM, PBKDF2, HKDF and HMAC over OpenSSL

HKDF always takes an explicit 32-byte zero salt rather than relying on the
no-salt path, because libraries disagree on whether that means an empty
salt or HashLen zeros -- the mismatch produces different keys on different
platforms and shows up only as data that will not decrypt.

Open() returns an optional instead of throwing: a corrupted or
wrong-key record must never take the app down."
```

---

## Task 3: Sxema v6 — outbox va sync_state

**Files:**
- Modify: `Telegram/SourceFiles/custom_db.h` (`kCurrentSchemaVersion`)
- Modify: `Telegram/SourceFiles/custom_db.cpp` (`RunMigrations`)

- [ ] **Step 1: Sxema versiyasini oshirish**

`custom_db.h` da:

```cpp
// v6: sync_outbox + sync_state jadvallari qo'shildi (multi-device sync).
constexpr int kCurrentSchemaVersion = 6;
```

- [ ] **Step 2: Migratsiyani yozish**

`custom_db.cpp` dagi `RunMigrations()` ichiga, mavjud v5 blokidan keyin:

```cpp
    if (version < 6) {
        // Sync outbox: serverga hali jo'natilmagan yozuvlar navbati.
        //
        // Faqat IDENTIFIKATSIYA maydonlari saqlanadi — payload push
        // paytida mavjud jadvallardan o'qib olinadi. Bu ma'lumot
        // dublikatini va ular orasidagi nomuvofiqlikni oldini oladi.
        execSql("CREATE TABLE IF NOT EXISTS sync_outbox ("
                "record_id TEXT PRIMARY KEY, "
                "kind TEXT NOT NULL, "
                "peer_id TEXT NOT NULL, "      // lokal: ochiq (HMAC push paytida)
                "msg_id INTEGER NOT NULL DEFAULT 0, "
                "occurred_at INTEGER NOT NULL, "
                "observed_at INTEGER NOT NULL, "
                "attempts INTEGER NOT NULL DEFAULT 0, "
                "last_error TEXT, "
                "next_retry_at INTEGER NOT NULL DEFAULT 0)");
        execSql("CREATE INDEX IF NOT EXISTS idx_outbox_retry "
                "ON sync_outbox(next_retry_at)");

        // Sync holati: cursor, device_id, tokenlar, oxirgi muvaffaqiyat.
        execSql("CREATE TABLE IF NOT EXISTS sync_state ("
                "key TEXT PRIMARY KEY, "
                "value TEXT NOT NULL)");

        execSql("UPDATE schema_version SET version = 6");
    }
```

- [ ] **Step 3: Migratsiyani tekshirish**

To'liq build hali kerak emas — migratsiya kodini ko'z bilan tekshiring
va mavjud v5 blokining uslubiga mos ekaniga ishonch hosil qiling.

Keyingi to'liq build'dan keyin (Task 6) tekshiriladi:

```bash
sqlite3 "%APPDATA%/CustomMod/custom_mod.db" ".schema sync_outbox"
sqlite3 "%APPDATA%/CustomMod/custom_mod.db" "SELECT version FROM schema_version;"
```

Kutilgan: jadval mavjud, versiya `6`.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat: add schema v6 with sync outbox and state tables

The outbox stores only identifying fields and re-reads the payload from
the existing tables at push time, so there is no second copy of the data
that could drift out of step with the first. next_retry_at is persisted so
backoff survives an app restart rather than resetting to zero every launch."
```

---

## Task 4: Outbox qatlami va enqueue nuqtalari

**Files:**
- Create: `Telegram/SourceFiles/custom_sync_outbox.h` / `.cpp`
- Modify: `Telegram/SourceFiles/custom_db.cpp` (4 ta chaqiruv)

- [ ] **Step 1: Outbox API'sini yozish**

`Telegram/SourceFiles/custom_sync_outbox.h`:

```cpp
#pragma once

#include <QtCore/QString>
#include <QtCore/QVector>

namespace CustomSync {

struct OutboxEntry {
    QString recordId;
    QString kind;
    QString peerId;      // lokal ochiq peer id
    qint64 msgId = 0;
    qint64 occurredAt = 0;
    qint64 observedAt = 0;
    int attempts = 0;
};

namespace Outbox {

// Navbatga qo'shadi. Sync o'chiq bo'lsa hech narsa qilmaydi.
//
// Bu funksiya custom_db.cpp dagi capture funksiyalarining OXIRIDA
// chaqiriladi. Capture logikasining o'zi o'zgarmaydi — qoida K5.
void Enqueue(
    const QString &kind,
    const QString &peerId,
    qint64 msgId,
    qint64 occurredAt);

// Jo'natishga tayyor yozuvlar (next_retry_at <= hozir), eng eskisidan.
[[nodiscard]] QVector<OutboxEntry> Pending(int limit);

void MarkSent(const QString &recordId);

// Eksponensial backoff: 1s, 2s, 4s… maksimum 5 daqiqa.
// next_retry_at diskda saqlanadi — ilova qayta ishga tushsa backoff
// nolga qaytmaydi.
void MarkFailed(const QString &recordId, const QString &error);

[[nodiscard]] int PendingCount();

// sync_state kalit-qiymat qatlami.
[[nodiscard]] QString GetState(const QString &key, const QString &fallback = {});
void SetState(const QString &key, const QString &value);

} // namespace Outbox
} // namespace CustomSync
```

- [ ] **Step 2: Implementatsiyani yozish**

`custom_sync_outbox.cpp` — mavjud `custom_db.cpp` dagi SQLite ishlash
uslubiga mos qiling (o'sha `gDb` ulanishi va `execSql` yordamchisi).
Backoff hisoblash:

```cpp
void MarkFailed(const QString &recordId, const QString &error) {
    // 1s, 2s, 4s, 8s… 300s da to'xtaydi.
    const auto attempts = CurrentAttempts(recordId) + 1;
    const auto delay = std::min<qint64>(300, qint64(1) << std::min(attempts, 9));
    const auto next = QDateTime::currentSecsSinceEpoch() + delay;

    // ... UPDATE sync_outbox SET attempts = ?, last_error = ?,
    //     next_retry_at = ? WHERE record_id = ?
}
```

- [ ] **Step 3: Enqueue chaqiruvlarini qo'shish**

`custom_db.cpp` da **to'rtta** nuqta. Har biri bitta satr, mavjud
logikadan **keyin**, funksiya oxirida:

| Funksiya | Qo'shiladigan |
|---|---|
| `MarkDeleted(...)` | `CustomSync::Outbox::Enqueue(Kind::Deleted, peerId, msgId, msgDate);` |
| `SaveActionedMessage(...)` (`type == "edited"` bo'lganda) | `CustomSync::Outbox::Enqueue(Kind::Edited, msg.peerId, msg.msgId, msg.msgDate);` |
| `SaveActivityHistoryEntry(...)` | `CustomSync::Outbox::Enqueue(Kind::Activity, peerId, DiscriminatorFor(field), observedAt);` |
| `SaveGhostRead(...)` | `CustomSync::Outbox::Enqueue(Kind::GhostRead, peerId, msgId, QDateTime::currentSecsSinceEpoch());` |

`custom_db.cpp` boshiga `#include "custom_sync_outbox.h"` va
`#include "custom_sync_record.h"` qo'shing.

**Nima uchun SQLite TRIGGER emas:** trigger C++ kodga tegmasdan ishlagan
bo'lardi, lekin nima bo'layotgani ko'rinmay qolardi va debug qilish
qiyinlashardi. To'rtta oshkora chaqiruv kodbazaning uslubiga mos.

- [ ] **Step 4: CMakeLists'ga qo'shish**

`Telegram/CMakeLists.txt` da mavjud `custom_db.cpp` yonidagi ro'yxatga:

```cmake
    SourceFiles/custom_sync_record.cpp
    SourceFiles/custom_sync_record.h
    SourceFiles/custom_sync_crypto.cpp
    SourceFiles/custom_sync_crypto.h
    SourceFiles/custom_sync_outbox.cpp
    SourceFiles/custom_sync_outbox.h
```

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat: add sync outbox with persisted exponential backoff

Enqueue is four explicit one-line calls at the end of the existing capture
functions rather than SQLite triggers: triggers would touch no C++ at all
but make the data flow invisible and hard to debug. Enqueue is a no-op when
sync is off, so capture behaviour is unchanged in that case."
```

---

## Task 5: Kalit saqlash (OS keystore)

**Files:**
- Create: `Telegram/SourceFiles/custom_sync_keystore.h` / `.cpp`

- [ ] **Step 1: API'ni yozish**

```cpp
#pragma once

#include <QtCore/QByteArray>
#include <optional>

// Master kalitni OS himoyasi ostida saqlash.
//
// Windows: DPAPI (CryptProtectData) — kalit joriy Windows foydalanuvchi
// hisobiga bog'lanadi, boshqa foydalanuvchi yoki boshqa mashina uni
// ocha olmaydi.
//
// Boshqa platformalar keyingi bo'laklarda qo'shiladi; hozircha ular
// uchun "saqlanmaydi" rejimi ishlaydi (har ishga tushganda parol
// so'raladi) — bu xavfsizroq, faqat qulaysizroq.

namespace CustomSync::Keystore {

[[nodiscard]] bool Available();

// Master kalitni OS himoyasi ostida saqlaydi.
[[nodiscard]] bool Store(const QByteArray &masterKey);

// Saqlangan kalitni qaytaradi. Yo'q bo'lsa yoki ochib bo'lmasa —
// bo'sh optional.
[[nodiscard]] std::optional<QByteArray> Load();

void Clear();

} // namespace CustomSync::Keystore
```

- [ ] **Step 2: Windows implementatsiyasini yozish**

```cpp
#include "custom_sync_keystore.h"

#include "custom_sync_outbox.h"   // sync_state kalit-qiymat saqlash uchun

#ifdef Q_OS_WIN
#include <windows.h>
#include <dpapi.h>
#endif

namespace CustomSync::Keystore {
namespace {

constexpr auto kStateKey = "master_key_protected";

} // namespace

bool Available() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool Store(const QByteArray &masterKey) {
#ifdef Q_OS_WIN
    DATA_BLOB input{ DWORD(masterKey.size()),
        reinterpret_cast<BYTE*>(const_cast<char*>(masterKey.constData())) };
    DATA_BLOB output{};

    if (!CryptProtectData(&input, L"CustomSync master key",
            nullptr, nullptr, nullptr, 0, &output)) {
        return false;
    }

    const auto protectedKey = QByteArray(
        reinterpret_cast<char*>(output.pbData), int(output.cbData));
    LocalFree(output.pbData);

    Outbox::SetState(QString::fromLatin1(kStateKey),
        QString::fromLatin1(protectedKey.toBase64()));
    return true;
#else
    Q_UNUSED(masterKey);
    return false;
#endif
}

std::optional<QByteArray> Load() {
#ifdef Q_OS_WIN
    const auto stored = Outbox::GetState(QString::fromLatin1(kStateKey));
    if (stored.isEmpty()) return std::nullopt;

    auto protectedKey = QByteArray::fromBase64(stored.toLatin1());
    DATA_BLOB input{ DWORD(protectedKey.size()),
        reinterpret_cast<BYTE*>(protectedKey.data()) };
    DATA_BLOB output{};

    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
        return std::nullopt;
    }

    const auto masterKey = QByteArray(
        reinterpret_cast<char*>(output.pbData), int(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return masterKey;
#else
    return std::nullopt;
#endif
}

void Clear() {
    Outbox::SetState(QString::fromLatin1(kStateKey), QString());
}

} // namespace CustomSync::Keystore
```

`Telegram/CMakeLists.txt` ga fayllarni qo'shing va Windows uchun
`Crypt32.lib` linklanganini tekshiring (tdesktop odatda uni allaqachon
linklaydi; bo'lmasa `target_link_libraries` ga qo'shing).

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: store the master key under Windows DPAPI

DPAPI ties the key to the Windows account, so another user on the same
machine -- or a copy of the profile on a different machine -- cannot
unwrap it. Platforms without an implementation fall back to prompting each
launch, which is less convenient but strictly safer than a weaker store."
```

---

## Task 6: HTTP klient — enroll va push

**Files:**
- Create: `Telegram/SourceFiles/custom_sync_client.h` / `.cpp`
- Modify: `Telegram/SourceFiles/custom_settings.h` / `.cpp`

- [ ] **Step 1: Sozlamalarni qo'shish**

`custom_settings.h` dagi `Values` struct'iga:

```cpp
    // Multi-device sync.
    bool syncEnabled = false;              // ataylab standart o'chiq
    QString syncServerUrl;
    int syncIntervalSeconds = 30;
```

Va getter'lar:

```cpp
inline bool    SyncEnabled()         { return Get().syncEnabled; }
inline QString SyncServerUrl()       { return Get().syncServerUrl; }
inline int     SyncIntervalSeconds() { return Get().syncIntervalSeconds; }
```

`custom_settings.cpp` da `UpdateValue`/`UpdateString`/`SetInt` va `Init()`
ga mos qatorlarni qo'shing — mavjud sozlamalar naqshiga aynan ergashing.

**`syncEnabled` standart `false`:** foydalanuvchi ataylab yoqmaguncha
hech narsa tarmoqqa chiqmaydi.

- [ ] **Step 2: Klient API'sini yozish**

`custom_sync_client.h`:

```cpp
#pragma once

#include "custom_sync_record.h"

#include <QtCore/QObject>
#include <functional>

class QNetworkAccessManager;

namespace CustomSync {

struct PushResult {
    QString recordId;
    QString status;   // created | duplicate | superseded | error
    QString message;
};

// Serverga HTTP orqali murojaat. Barcha chaqiruvlar asinxron —
// UI oqimi hech qachon bloklanmaydi (qoida K2).
class Client : public QObject {
    Q_OBJECT

public:
    explicit Client(QObject *parent = nullptr);
    ~Client();

    // Bir martalik kod bilan qurilmani ro'yxatdan o'tkazadi.
    void enroll(
        const QString &serverUrl,
        const QString &code,
        const QString &deviceName,
        Fn<void(bool success, QString error)> done);

    void push(
        const QVector<Record> &records,
        Fn<void(bool success, QVector<PushResult> results, QString error)> done);

    void pull(
        qint64 since,
        int limit,
        Fn<void(bool success, QVector<Record> records,
                qint64 nextSince, bool hasMore, QString error)> done);

    void mediaExists(const QString &hash, Fn<void(bool)> done);
    void mediaUpload(
        const QString &hash, const QByteArray &encrypted,
        const QByteArray &nonce, Fn<void(bool)> done);

private:
    void ensureAccessToken(Fn<void(bool)> done);

    QNetworkAccessManager *_network = nullptr;
    QString _accessToken;
    qint64 _tokenExpiresAt = 0;
};

} // namespace CustomSync
```

- [ ] **Step 3: Implementatsiyani yozish**

`QNetworkAccessManager` bilan standart Qt naqshi. Muhim jihatlar:

- `Authorization: Bearer <token>` sarlavhasi
- `ensureAccessToken()` — token muddati tugagan bo'lsa avtomatik
  `/devices/refresh` chaqiradi va yangi refresh token'ni
  `Outbox::SetState("refresh_token", ...)` orqali saqlaydi
- Har bir javob `QNetworkReply::finished` signalida qayta ishlanadi
- HTTP status va tarmoq xatosi alohida farqlanadi — 4xx qayta urinishga
  arzimaydi, 5xx va tarmoq xatosi arziydi

- [ ] **Step 4: Birinchi to'liq build**

⚠️ **Bu qadam ~34 daqiqalik to'liq build talab qiladi. Boshlashdan oldin
foydalanuvchidan so'rang.**

```bash
cmake --build out/Debug --target Telegram
```

Kutilgan: `Build succeeded`.

- [ ] **Step 5: Uchi-uchiga qo'lda tekshirish**

1. Serverda kod yarating:
   `dotnet run --project src/CustomSync.Api -- --create-enrollment-code`
2. tdesktop'ni ishga tushiring, sozlamalarda server URL va kodni kiriting
3. Bazani tekshiring:

```bash
sqlite3 "%APPDATA%/CustomMod/custom_mod.db" "SELECT key, substr(value,1,20) FROM sync_state;"
```

Kutilgan: `device_id` va `refresh_token` yozuvlari mavjud.

4. Biror chatda xabar o'chiring va outbox'ni tekshiring:

```bash
sqlite3 "%APPDATA%/CustomMod/custom_mod.db" "SELECT count(*) FROM sync_outbox;"
```

Kutilgan: navbat to'ldi, so'ng bo'shadi (push muvaffaqiyatli).

5. Serverda tekshiring:

```bash
psql -U customsync -d customsync -c "SELECT count(*), kind FROM records GROUP BY kind;"
```

Kutilgan: `deleted` turidagi yozuv paydo bo'ldi.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: add sync HTTP client with device enrolment and push

Sync defaults to off: nothing leaves the machine until it is explicitly
enabled. 4xx responses are treated as permanent and 5xx or network errors
as retryable, so a malformed record does not sit in the outbox retrying
forever."
```

---

## Task 7: Pull va lokal merge

**Files:**
- Modify: `Telegram/SourceFiles/custom_sync_client.cpp`
- Create: merge logikasi `custom_sync.cpp` ichida

- [ ] **Step 1: Merge qoidasini yozish**

Serverdan kelgan yozuv lokal SQLite'ga qo'shiladi. Konflikt qoidasi
server bilan **aynan bir xil**: kichikroq `observed_at` g'olib.

```cpp
// Serverdan kelgan yozuvni lokal bazaga qo'shadi.
//
// Konflikt qoidasi server bilan bir xil bo'lishi SHART — aks holda
// qurilmalar bir xil ma'lumotdan turli xulosaga keladi va farq
// abadiy saqlanib qoladi.
void MergeIncoming(const Record &record) {
    const auto payload = Crypto::Open(ContentKey(), record.nonce, record.payload);
    if (!payload) {
        // Deshifrlab bo'lmadi — kalit noto'g'ri yoki ma'lumot buzilgan.
        // Loglaymiz va DAVOM ETAMIZ. Bitta buzilgan yozuv butun sync'ni
        // to'xtatmasligi kerak.
        LogCorruptRecord(record.recordId);
        return;
    }
    // ... kind bo'yicha mavjud custom_db funksiyalariga yozish
}
```

- [ ] **Step 2: Buzilgan yozuvlar ro'yxati**

`sync_state` da `corrupt_records` kalitida saqlang va UI'da ko'rsating.
Foydalanuvchi muammoni bilishi kerak, lekin ilova ishlashda davom etishi
kerak.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: merge pulled records into the local database

The conflict rule is duplicated from the server on purpose and must stay
identical: if the two disagree, devices reach different conclusions from
the same data and the divergence never heals. A record that fails to
decrypt is logged and skipped rather than aborting the batch."
```

---

## Task 8: Orkestrator

**Files:**
- Create: `Telegram/SourceFiles/custom_sync.h` / `.cpp`

- [ ] **Step 1: Orkestratorni yozish**

Vazifalari:

- `Start()` — ilova ishga tushganda chaqiriladi (`main_window` yoki
  `application.cpp` dagi mavjud CustomMod init yonida)
- `crl::async` fon oqimida timer: har `SyncIntervalSeconds()` da push+pull
- `SyncNow()` — qo'lda ishga tushirish (UI tugmasi)
- Holat signallari: `lastSuccessAt`, `pendingCount`, `lastError`, `locked`
- Kalit qulflangan bo'lsa push'ni **o'tkazib yuboradi**, capture davom etadi

```cpp
// Barcha tarmoq va crypto ishi crl::async fon oqimida bajariladi
// (qoida K2). ExportFullBackupAsync allaqachon shu naqshni ishlatadi.
void Tick() {
    crl::async([] {
        const auto entries = Outbox::Pending(BatchSize());
        // ... shifrlash + push
        crl::on_main([] {
            // ... UI holatini yangilash
        });
    });
}
```

- [ ] **Step 2: Commit**

```bash
git add -A
git commit -m "feat: add sync orchestrator on a background thread

All network and crypto work runs through crl::async so the UI thread is
never blocked, following the pattern ExportFullBackupAsync already
established. When the key is locked the orchestrator skips pushing but
capture keeps running -- locking pauses upload, it never drops data."
```

---

## Task 9: WebSocket (shartli)

⚠️ **Bu task boshlanishidan oldin bog'liqlik tekshiriladi.**

- [ ] **Step 1: Qt WebSockets mavjudligini tekshirish**

```bash
ls "$(qmake -query QT_INSTALL_LIBS)" | grep -i websocket
```

**Agar `Qt5WebSockets` mavjud bo'lsa** — Step 2 ga o'ting.

**Agar mavjud bo'lmasa** — bu taskni **o'tkazib yuboring**. Tizim
periodik pull bilan to'liq to'g'ri ishlaydi; WebSocket faqat kechikishni
30 soniyadan bir necha soniyagacha kamaytiradi. Buning uchun RFC 6455
freym qatlamini qo'lda yozish yoki Qt'ni qayta build qilish oqlanmaydi.
Uni keyinroq, alohida vazifa sifatida ko'rib chiqamiz.

- [ ] **Step 2: `QWebSocket` bilan ulanish**

`custom_sync.cpp` ichida: `wss://<server>/ws/notify?access_token=<jwt>`,
`textMessageReceived` signalida `{"type":"changes"}` kelsa darhol pull.
Uzilsa eksponensial backoff (1s → 60s) bilan qayta ulanish.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: add WebSocket change notifications to the desktop agent

Guarded behind an availability check for Qt5WebSockets: the design treats
push as an optimisation over polling, so a build without the module simply
syncs on the 30-second timer instead of hand-rolling RFC 6455 framing."
```

---

## Task 10: UI bo'limi

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp`

- [ ] **Step 1: "☁️ Sinxronizatsiya" bo'limini qo'shish**

Mavjud bo'limlar naqshiga ergashing (`fillActivityHistorySection` kabi).
Tarkibi:

**Ulanish**
- Server URL kiritish maydoni
- Enrollment kodi kiritish + "Ulash" tugmasi
- Ulanish holati: qurilma nomi, `device_id`, oxirgi ko'rinish

**Holat** (jonli yangilanadi)
- Sync yoqiq/o'chiq toggle
- Oxirgi muvaffaqiyatli sync vaqti
- Navbatdagi yozuvlar soni
- Oxirgi xato (bo'lsa)
- "Hozir sinxronlash" tugmasi

**Qulf**
- Qulf holati (ochiq/yopiq)
- Qulf ochish usullari: custom parol / OS keystore+biometrika
- "Har ishga tushganda so'rash" toggle'i
- Avtomatik qulflash vaqti

**Almashuv**
- "Almashuv eksporti" — qamrov tanlash bilan
- "Almashuv importi"
- Mavjud "To'liq zaxira" tugmalaridan **aniq ajratilgan** — ular boshqa
  maqsad uchun (qurilmani tiklash), chalkashmasligi kerak

- [ ] **Step 2: Ikkinchi to'liq build**

⚠️ **Foydalanuvchidan so'rang.**

- [ ] **Step 3: UI'ni qo'lda tekshirish**

- Bo'lim ochiladi, freez yo'q
- Toggle'lar holatni saqlaydi (ilovani qayta ochib tekshiring)
- "Hozir sinxronlash" UI'ni bloklamaydi
- Sync o'chirilganda barcha maydonlar mantiqiy ravishda o'chadi

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat: add sync section to the CustomMod window

Interchange export sits visibly apart from the existing full backup
buttons: the two look similar but mean different things -- one merges
records across devices, the other restores a device wholesale -- and
confusing them would lose data."
```

---

## Task 11: Regressiya tekshiruvi (qoida K5)

Bu plandagi **eng muhim** tekshiruv. Sync — qo'shimcha qatlam; u mavjud
funksionallikni buzsa, butun ish qiymatini yo'qotadi.

- [ ] **Step 1: Sync o'chiq holatda to'liq regressiya**

Sozlamalarda sync'ni **o'chiring** va quyidagilarni tekshiring:

| Tekshiruv | Kutilgan |
|---|---|
| Ilova ishga tushish vaqti | Sync qo'shilishidan oldingidek |
| O'chirilgan xabarlar ko'rinadi | Ha, oldingidek |
| Tahrirlangan xabarlar tarixi | Ha, oldingidek |
| Activity History oynasi | Ha, oldingidek |
| Ghost Mode | Ha, oldingidek |
| `sync_outbox` jadvali | **Bo'sh qoladi** |
| Tarmoq faolligi | **Yo'q** |

- [ ] **Step 2: Sync yoqilgan, lekin server o'chiq**

| Tekshiruv | Kutilgan |
|---|---|
| Ilova normal ishlaydi | Ha |
| Capture ishlaydi | Ha |
| `sync_outbox` to'ladi | Ha |
| UI'da "offline" ko'rsatiladi | Ha |
| Freez yoki lag | **Yo'q** |
| Server qaytgach navbat jo'natiladi | Ha |

- [ ] **Step 3: Kalit qulflangan holat**

| Tekshiruv | Kutilgan |
|---|---|
| Capture ishlaydi | Ha |
| `sync_outbox` to'ladi | Ha |
| Push to'xtaydi | Ha |
| Qulf ochilgach navbat jo'natiladi | Ha |
| Ma'lumot yo'qolishi | **Yo'q** |

- [ ] **Step 4: Natijani hujjatlashtirish**

Natijalarni `docs/superpowers/plans/` ga qo'shimcha fayl sifatida emas,
commit xabarida yozing.

```bash
git commit --allow-empty -m "test: verify zero regression with sync disabled

Manual pass over the three states that matter: sync off, sync on with the
server unreachable, and sync on with the key locked. In all three the
existing capture and archive behaviour is unchanged and no data is lost --
the outbox simply accumulates and drains later."
```

---

## Qabul qilish mezonlari (02)

1. `sync_selftest` barcha `test-vectors.json` vektorlarini o'tkazadi.
2. Sxema v6 migratsiyasi mavjud bazada muammosiz qo'llanadi.
3. Qurilma enrollment kodi bilan ulanadi va `device_id` saqlanadi.
4. O'chirilgan xabar 60 soniya ichida serverda paydo bo'ladi.
5. Server o'chirilganda outbox to'ladi, qaytgach bo'shaydi — hech narsa
   yo'qolmaydi.
6. Kalit qulflangan bo'lsa capture davom etadi, push pauza qiladi.
7. **Sync o'chiq bo'lganda hech qanday regressiya yo'q** (Task 11).
8. UI hech qanday operatsiyada freez bo'lmaydi.

---

## Keyingi qadam

Plan 03 — `server-controller` web app. U 01b dagi API'ga ulanadi va
shu yerdagi shifrlash sxemasining brauzer versiyasini quradi (Web Crypto
API, xuddi shu test vektorlariga qarshi tekshiriladi).
