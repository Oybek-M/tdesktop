#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
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

// spec §0.12: HMAC-SHA256(account_key, account_id)[0..16] -> lowercase hex.
// account_id — o'nlik satr (masalan "111222333").
[[nodiscard]] QString ComputeAccountHash(
    const QByteArray &accountKey,
    const QString &accountId);

// spec §0.12: HMAC-SHA256(peer_key, peer_id)[0..16] -> lowercase hex.
// peer_id — o'nlik satr (masalan "7053823996").
[[nodiscard]] QString ComputePeerHash(
    const QByteArray &peerKey,
    const QString &peerId);

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
