#include "custom_sync_keyshare.h"
#include "custom_sync_crypto.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

namespace CustomSync::KeyShare {

Wrap WrapMasterKey(
        const QByteArray &masterKey,
        const QString &passphrase,
        const QString &label) {
    Wrap wrap;
    if (masterKey.size() != 32 || passphrase.isEmpty()) {
        return wrap;
    }

    // salt va nonce har doim YANGI va tasodifiy bo'lishi shart (qayta ishlatish taqiqlangan)
    const auto salt = Crypto::RandomBytes(16);
    const auto nonce = Crypto::RandomBytes(12);
    const int iterations = kPassphraseIterations;

    const auto kek = Crypto::Pbkdf2(passphrase.toUtf8(), salt, iterations, 32);
    if (kek.size() != 32) {
        return wrap;
    }

    const auto wrappedKey = Crypto::Seal(kek, nonce, masterKey);
    if (wrappedKey.isEmpty()) {
        return wrap;
    }

    wrap.wrapType = QStringLiteral("passphrase");
    wrap.label = label;
    wrap.salt = salt;
    wrap.nonce = nonce;
    wrap.wrappedKey = wrappedKey;
    wrap.iterations = iterations;

    return wrap;
}

std::optional<QByteArray> UnwrapMasterKey(
        const Wrap &wrap,
        const QString &passphrase) {
    if (wrap.salt.isEmpty() || wrap.nonce.isEmpty() || wrap.wrappedKey.isEmpty()) {
        return std::nullopt;
    }
    if (wrap.iterations <= 0 || passphrase.isEmpty()) {
        return std::nullopt;
    }

    // Iteratsiyalar soni qat'iy 600 000 deb olinmaydi — wrap obyekti ichidan o'qiladi
    const auto kek = Crypto::Pbkdf2(passphrase.toUtf8(), wrap.salt, wrap.iterations, 32);
    if (kek.size() != 32) {
        return std::nullopt;
    }

    const auto masterKey = Crypto::Open(kek, wrap.nonce, wrap.wrappedKey);
    if (!masterKey.has_value() || masterKey->size() != 32) {
        return std::nullopt;
    }

    return masterKey;
}

KeyWrapResponse ParseKeyWrapResponse(const QByteArray &jsonBytes, int httpStatus) {
    KeyWrapResponse res;
    const auto doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        res.error = QStringLiteral("invalid_json");
        return res;
    }
    const auto obj = doc.object();
    if (httpStatus == 200) {
        res.wrap.wrapId = obj.value(QStringLiteral("wrap_id")).toString();
        res.wrap.wrapType = obj.value(QStringLiteral("wrap_type")).toString();
        res.wrap.label = obj.value(QStringLiteral("label")).toString();
        res.wrap.iterations = obj.value(QStringLiteral("iterations")).toInt();

        // Bayt maydonlari base64 orqali kodlangan bo'ladi (hex EMAS)
        res.wrap.salt = QByteArray::fromBase64(obj.value(QStringLiteral("salt")).toString().toLatin1());
        res.wrap.nonce = QByteArray::fromBase64(obj.value(QStringLiteral("nonce")).toString().toLatin1());
        res.wrap.wrappedKey = QByteArray::fromBase64(obj.value(QStringLiteral("wrapped_key")).toString().toLatin1());

        if (res.wrap.salt.isEmpty() || res.wrap.nonce.isEmpty() || res.wrap.wrappedKey.isEmpty() || res.wrap.iterations <= 0) {
            res.error = QStringLiteral("missing_fields");
        }
    } else {
        res.error = obj.value(QStringLiteral("error")).toString();
        if (res.error.isEmpty()) {
            res.error = QStringLiteral("http_%1").arg(httpStatus);
        }
    }
    return res;
}

KeyWrapListResponse ParseKeyWrapListResponse(const QByteArray &jsonBytes, int httpStatus) {
    KeyWrapListResponse res;
    if (httpStatus != 200) {
        const auto doc = QJsonDocument::fromJson(jsonBytes);
        if (doc.isObject()) {
            res.error = doc.object().value(QStringLiteral("error")).toString();
        }
        if (res.error.isEmpty()) {
            res.error = QStringLiteral("http_%1").arg(httpStatus);
        }
        return res;
    }

    const auto doc = QJsonDocument::fromJson(jsonBytes);
    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject() && doc.object().value(QStringLiteral("wraps")).isArray()) {
        arr = doc.object().value(QStringLiteral("wraps")).toArray();
    } else {
        res.error = QStringLiteral("invalid_json");
        return res;
    }

    for (const auto &val : arr) {
        if (!val.isObject()) continue;
        const auto obj = val.toObject();
        Wrap w;
        w.wrapId = obj.value(QStringLiteral("wrap_id")).toString();
        w.wrapType = obj.value(QStringLiteral("wrap_type")).toString();
        w.label = obj.value(QStringLiteral("label")).toString();
        w.iterations = obj.value(QStringLiteral("iterations")).toInt();
        w.salt = QByteArray::fromBase64(obj.value(QStringLiteral("salt")).toString().toLatin1());
        w.nonce = QByteArray::fromBase64(obj.value(QStringLiteral("nonce")).toString().toLatin1());
        w.wrappedKey = QByteArray::fromBase64(obj.value(QStringLiteral("wrapped_key")).toString().toLatin1());
        res.wraps.append(std::move(w));
    }
    return res;
}

CreateKeyWrapResponse ParseCreateKeyWrapResponse(const QByteArray &jsonBytes, int httpStatus) {
    CreateKeyWrapResponse res;
    if (httpStatus == 403) {
        // Oddiy "device" roli POST qila olmaydi, admin talab etiladi (aniq xato matni)
        res.error = QStringLiteral("forbidden_admin_required");
        const auto doc = QJsonDocument::fromJson(jsonBytes);
        if (doc.isObject()) {
            const auto err = doc.object().value(QStringLiteral("error")).toString();
            if (!err.isEmpty() && err != QStringLiteral("Forbidden")) {
                res.error = err;
            }
        }
        return res;
    }

    const auto doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        res.error = QStringLiteral("invalid_json");
        return res;
    }

    const auto obj = doc.object();
    if (httpStatus == 200 || httpStatus == 201) {
        res.wrapId = obj.value(QStringLiteral("wrap_id")).toString();
        if (res.wrapId.isEmpty()) {
            res.error = QStringLiteral("missing_fields");
        }
    } else {
        res.error = obj.value(QStringLiteral("error")).toString();
        if (res.error.isEmpty()) {
            res.error = QStringLiteral("http_%1").arg(httpStatus);
        }
    }
    return res;
}

} // namespace CustomSync::KeyShare
