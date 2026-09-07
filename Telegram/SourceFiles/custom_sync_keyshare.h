#pragma once

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QVector>
#include <optional>

namespace CustomSync::KeyShare {

// Barcha platformalar (tdesktop, Android, iOS, server) uchun yagona protokol konstantasi (spec §4.4).
inline constexpr int kPassphraseIterations = 600'000;

struct Wrap {
    QString wrapId;
    QString wrapType;   // "passphrase" | "recovery" | "email"
    QString label;
    QByteArray salt;
    QByteArray nonce;
    QByteArray wrappedKey;
    int iterations = 0;
};

// Master kalitni paroldan chiqarilgan KEK bilan o'raydi.
// salt va nonce har chaqiruvda YANGI tasodifiy qiymat bo'ladi.
// DIQQAT: PBKDF2 (600 000 iteratsiya) UI oqimini qotirib qo'yishi mumkin!
// Chaqiruvchi buni crl::async orqali bajarishi va natijani crl::on_main
// bilan asosiy oqimga qaytarishi tavsiya etiladi.
[[nodiscard]] Wrap WrapMasterKey(
    const QByteArray &masterKey,
    const QString &passphrase,
    const QString &label);

// Ochadi. Parol noto'g'ri bo'lsa -- bo'sh optional (AES-GCM tegi
// mos kelmaydi). Bu yagona to'g'ri xato signali.
// wrap.iterations maydonidan foydalanadi, qiymatni qat'iy 600 000 deb hisoblamaydi.
[[nodiscard]] std::optional<QByteArray> UnwrapMasterKey(
    const Wrap &wrap,
    const QString &passphrase);

struct KeyWrapResponse {
    Wrap wrap;
    QString error;
};

struct KeyWrapListResponse {
    QVector<Wrap> wraps;
    QString error;
};

struct CreateKeyWrapResponse {
    QString wrapId;
    QString error;
};

// Tarmoqsiz test qilinishi mumkin bo'lgan erkin parser funksiyalar:
[[nodiscard]] KeyWrapResponse ParseKeyWrapResponse(const QByteArray &jsonBytes, int httpStatus);
[[nodiscard]] KeyWrapListResponse ParseKeyWrapListResponse(const QByteArray &jsonBytes, int httpStatus);
[[nodiscard]] CreateKeyWrapResponse ParseCreateKeyWrapResponse(const QByteArray &jsonBytes, int httpStatus);

} // namespace CustomSync::KeyShare

namespace CustomSync {
using KeyShare::Wrap;
using KeyShare::WrapMasterKey;
using KeyShare::UnwrapMasterKey;
using KeyShare::KeyWrapResponse;
using KeyShare::KeyWrapListResponse;
using KeyShare::CreateKeyWrapResponse;
using KeyShare::ParseKeyWrapResponse;
using KeyShare::ParseKeyWrapListResponse;
using KeyShare::ParseCreateKeyWrapResponse;
} // namespace CustomSync
