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
    if (!ok || outLength != size_t(keyLength)) {
        return QByteArray();
    }
    result.resize(int(outLength));
    return result;
}

QByteArray HmacSha256(const QByteArray &key, const QByteArray &message) {
    unsigned int length = 0;
    QByteArray result(EVP_MAX_MD_SIZE, '\0');
    if (!HMAC(EVP_sha256(),
              key.constData(), key.size(),
              reinterpret_cast<const unsigned char*>(message.constData()), message.size(),
              reinterpret_cast<unsigned char*>(result.data()), &length)) {
        return QByteArray();
    }
    result.resize(int(length));
    return result;
}

QString ComputeAccountHash(const QByteArray &accountKey, const QString &accountId) {
    const auto hmac = HmacSha256(accountKey, accountId.toUtf8());
    return QString::fromLatin1(hmac.left(16).toHex());
}

QString ComputePeerHash(const QByteArray &peerKey, const QString &peerId) {
    const auto hmac = HmacSha256(peerKey, peerId.toUtf8());
    return QString::fromLatin1(hmac.left(16).toHex());
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
               reinterpret_cast<const unsigned char*>(nonce.constData())) == 1) {

        bool updateOk = true;
        if (plaintext.size() > 0) {
            updateOk = (EVP_EncryptUpdate(ctx, out, &length,
                           reinterpret_cast<const unsigned char*>(plaintext.constData()),
                           plaintext.size()) == 1);
            total = length;
        } else {
            total = 0;
        }

        if (updateOk && EVP_EncryptFinal_ex(ctx, out + total, &length) == 1) {
            total += length;
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagSize, out + total) == 1) {
                total += kTagSize;
                ok = true;
            }
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
    unsigned char dummy = 0;
    auto *out = bodySize > 0 ? reinterpret_cast<unsigned char*>(output.data()) : &dummy;
    int length = 0;
    int total = 0;
    auto ok = false;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) == 1
        && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
               reinterpret_cast<const unsigned char*>(key.constData()),
               reinterpret_cast<const unsigned char*>(nonce.constData())) == 1) {

        bool updateOk = true;
        if (bodySize > 0) {
            updateOk = (EVP_DecryptUpdate(ctx, out, &length, body, bodySize) == 1);
            total = length;
        } else {
            total = 0;
        }

        if (updateOk && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagSize, tag) == 1) {
            // EVP_DecryptFinal_ex tag mos kelishini tekshiradi
            if (EVP_DecryptFinal_ex(ctx, out + total, &length) == 1) {
                total += length;
                ok = true;
            }
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
