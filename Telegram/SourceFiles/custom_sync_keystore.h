#pragma once

#include <QtCore/QByteArray>
#include <optional>

// Master kalitni OS himoyasi ostida saqlash.
//
// Windows: DPAPI (CryptProtectData / CryptUnprotectData) — kalit joriy
// Windows foydalanuvchi hisobiga bog'lanadi, boshqa foydalanuvchi yoki
// boshqa mashina uni ocha olmaydi.
//
// Boshqa platformalar: hozircha Available() false qaytaradi va har
// ishga tushganda parol so'raladi (strictly safer).

namespace CustomSync::Keystore {

[[nodiscard]] bool Available();

// OS himoyasi ostiga oladi. Windows: DPAPI.
[[nodiscard]] std::optional<QByteArray> ProtectBytes(
    const QByteArray &plain);

// Qaytaradi. Blob buzilgan, boshqa foydalanuvchi yoki boshqa
// mashinada yaratilgan bo'lsa -- bo'sh optional.
[[nodiscard]] std::optional<QByteArray> UnprotectBytes(
    const QByteArray &blob);

} // namespace CustomSync::Keystore
