#include "custom_sync_keystore.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace CustomSync::Keystore {

#ifdef Q_OS_WIN
namespace {

// OGOHLANTIRISH: Bu entropiya qiymatini HECH QACHON o'zgartirmang!
// O'zgartirilsa, avval saqlangan barcha master kalitlar butunlay ochilmay qoladi.
constexpr char kEntropy[] = "CustomSync-Keystore-Entropy-v1";

DATA_BLOB MakeEntropyBlob() {
    return DATA_BLOB{
        static_cast<DWORD>(sizeof(kEntropy) - 1),
        reinterpret_cast<BYTE*>(const_cast<char*>(kEntropy))
    };
}

} // namespace

bool Available() {
    return true;
}

std::optional<QByteArray> ProtectBytes(const QByteArray &plain) {
    BYTE dummy = 0;
    DATA_BLOB input{
        static_cast<DWORD>(plain.size()),
        plain.isEmpty() ? &dummy : reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()))
    };
    DATA_BLOB entropy = MakeEntropyBlob();
    DATA_BLOB output{};

    if (!CryptProtectData(
            &input,
            L"CustomSync master key",
            &entropy,
            nullptr,
            nullptr,
            0,
            &output)) {
        return std::nullopt;
    }

    const auto protectedBlob = QByteArray(
        reinterpret_cast<const char*>(output.pbData),
        static_cast<int>(output.cbData));
    if (output.pbData) {
        LocalFree(output.pbData);
    }
    return protectedBlob;
}

std::optional<QByteArray> UnprotectBytes(const QByteArray &blob) {
    if (blob.isEmpty()) {
        return std::nullopt;
    }

    DATA_BLOB input{
        static_cast<DWORD>(blob.size()),
        reinterpret_cast<BYTE*>(const_cast<char*>(blob.constData()))
    };
    DATA_BLOB entropy = MakeEntropyBlob();
    DATA_BLOB output{};

    if (!CryptUnprotectData(
            &input,
            nullptr,
            &entropy,
            nullptr,
            nullptr,
            0,
            &output)) {
        return std::nullopt;
    }

    const auto plain = QByteArray(
        reinterpret_cast<const char*>(output.pbData),
        static_cast<int>(output.cbData));
    if (output.pbData && output.cbData > 0) {
        SecureZeroMemory(output.pbData, output.cbData);
    }
    if (output.pbData) {
        LocalFree(output.pbData);
    }
    return plain;
}

#else

// Windows bo'lmagan platformalar: kalit saqlanmaydi va har ishga tushganda
// paroldan tiklanadi (parol so'rovi). Bu zaifroq doimiy saqlashdan ko'ra
// ancha xavfsizroq.
bool Available() {
    return false;
}

std::optional<QByteArray> ProtectBytes(const QByteArray &plain) {
    return std::nullopt;
}

std::optional<QByteArray> UnprotectBytes(const QByteArray &blob) {
    return std::nullopt;
}

#endif

} // namespace CustomSync::Keystore
