#include "custom_sync_client.h"
#include "custom_sync_outbox.h"
#include "custom_sync_crypto.h"
#include "custom_settings.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDateTime>

#include <optional>

namespace CustomSync {

EnrollResponse ParseEnrollResponse(const QByteArray &jsonBytes, int httpStatus) {
    EnrollResponse res;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(jsonBytes, &err);
    if (!doc.isObject()) {
        res.error = QStringLiteral("invalid_json");
        return res;
    }
    const auto obj = doc.object();
    if (httpStatus == 200) {
        res.deviceId = obj.value(QStringLiteral("device_id")).toString();
        res.refreshToken = obj.value(QStringLiteral("refresh_token")).toString();
        res.accessToken = obj.value(QStringLiteral("access_token")).toString();
        res.expiresAt = obj.value(QStringLiteral("expires_at")).toString();
        if (res.deviceId.isEmpty() || res.refreshToken.isEmpty() || res.accessToken.isEmpty()) {
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

RefreshResponse ParseRefreshResponse(const QByteArray &jsonBytes, int httpStatus) {
    RefreshResponse res;
    const auto doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        res.error = QStringLiteral("invalid_json");
        return res;
    }
    const auto obj = doc.object();
    if (httpStatus == 200) {
        res.refreshToken = obj.value(QStringLiteral("refresh_token")).toString();
        res.accessToken = obj.value(QStringLiteral("access_token")).toString();
        res.expiresAt = obj.value(QStringLiteral("expires_at")).toString();
        if (res.refreshToken.isEmpty() || res.accessToken.isEmpty()) {
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

PushResponse ParsePushResponse(const QByteArray &jsonBytes, int httpStatus) {
    PushResponse res;
    const auto doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        res.error = QStringLiteral("invalid_json");
        return res;
    }
    const auto obj = doc.object();
    if (httpStatus == 200) {
        const auto arr = obj.value(QStringLiteral("results")).toArray();
        for (const auto &val : arr) {
            const auto item = val.toObject();
            res.results.append(PushResult{
                item.value(QStringLiteral("record_id")).toString(),
                item.value(QStringLiteral("status")).toString(),
                item.value(QStringLiteral("message")).toString(),
            });
        }
    } else {
        res.error = obj.value(QStringLiteral("error")).toString();
        if (res.error == QStringLiteral("batch_too_large")) {
            res.maxBatch = obj.value(QStringLiteral("max")).toInt();
        }
        if (res.error.isEmpty()) {
            res.error = QStringLiteral("http_%1").arg(httpStatus);
        }
    }
    return res;
}

PullResponse ParsePullResponse(const QByteArray &jsonBytes, int httpStatus) {
    PullResponse res;
    const auto doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        res.error = QStringLiteral("invalid_json");
        return res;
    }
    const auto obj = doc.object();
    if (httpStatus == 200) {
        const auto arr = obj.value(QStringLiteral("records")).toArray();
        for (const auto &val : arr) {
            res.records.append(FromJson(val.toObject()));
        }
        res.nextSince = obj.value(QStringLiteral("next_since")).toInteger();
        res.hasMore = obj.value(QStringLiteral("has_more")).toBool();
    } else {
        res.error = obj.value(QStringLiteral("error")).toString();
        if (res.error.isEmpty()) {
            res.error = QStringLiteral("http_%1").arg(httpStatus);
        }
    }
    return res;
}

Client::Client(QObject *parent)
    : QObject(parent)
    , _network(new QNetworkAccessManager(this)) {
}

Client::~Client() = default;

QUrl Client::makeUrl(const QString &path) const {
    auto base = CustomSettings::SyncServerUrl().trimmed();
    while (base.endsWith('/')) {
        base.chop(1);
    }
    return QUrl(base + path);
}

void Client::ensureAccessToken(Fn<void(bool success)> done) {
    const auto now = QDateTime::currentSecsSinceEpoch();
    constexpr qint64 kExpirationMarginSeconds = 60;
    if (!_accessToken.isEmpty() && now + kExpirationMarginSeconds < _tokenExpiresAt) {
        if (done) done(true);
        return;
    }

    const auto deviceId = Outbox::GetState(QStringLiteral("device_id"));
    const auto refreshToken = Outbox::GetState(QStringLiteral("refresh_token"));
    if (deviceId.isEmpty() || refreshToken.isEmpty()) {
        if (done) done(false);
        return;
    }

    if (_refreshing) {
        if (done) _pendingTokenWaiters.append(std::move(done));
        return;
    }

    _refreshing = true;
    if (done) _pendingTokenWaiters.append(std::move(done));

    QNetworkRequest req(makeUrl(QStringLiteral("/api/v1/devices/refresh")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body{
        { QStringLiteral("device_id"), deviceId },
        { QStringLiteral("refresh_token"), refreshToken },
    };

    auto *reply = _network->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto data = reply->readAll();
        const auto resp = ParseRefreshResponse(data, httpStatus);

        bool success = false;
        if (httpStatus == 200 && resp.error.isEmpty() && !resp.refreshToken.isEmpty()) {
            // Bir martalik refresh token: server eski tokenni o'ldirdi.
            // Avval YANGI refresh tokenni saqlaymiz (birinchi navbatda!), keyin davom etamiz.
            Outbox::SetState(QStringLiteral("refresh_token"), resp.refreshToken);
            _accessToken = resp.accessToken;
            const auto dt = QDateTime::fromString(resp.expiresAt, Qt::ISODateWithMs);
            _tokenExpiresAt = dt.isValid()
                ? dt.toSecsSinceEpoch()
                : (QDateTime::currentSecsSinceEpoch() + 3600);
            success = true;
        }

        _refreshing = false;
        const auto waiters = std::move(_pendingTokenWaiters);
        _pendingTokenWaiters.clear();
        for (const auto &w : waiters) {
            if (w) w(success);
        }
    });
}

void Client::enroll(
        const QString &serverUrl,
        const QString &code,
        const QString &deviceName,
        Fn<void(bool success, QString error)> done) {
    auto cleanUrl = serverUrl.trimmed();
    while (cleanUrl.endsWith('/')) {
        cleanUrl.chop(1);
    }
    QUrl url(cleanUrl + QStringLiteral("/api/v1/devices/enroll"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body{
        { QStringLiteral("code"), code },
        { QStringLiteral("name"), deviceName },
        { QStringLiteral("platform"), QStringLiteral("tdesktop") },
    };

    auto *reply = _network->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, cleanUrl, done] {
        reply->deleteLater();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto data = reply->readAll();
        const auto resp = ParseEnrollResponse(data, httpStatus);

        if (httpStatus == 200 && resp.error.isEmpty()) {
            CustomSettings::SetString(QStringLiteral("syncServerUrl"), cleanUrl);
            Outbox::SetState(QStringLiteral("device_id"), resp.deviceId);
            Outbox::SetState(QStringLiteral("refresh_token"), resp.refreshToken);
            _accessToken = resp.accessToken;
            const auto dt = QDateTime::fromString(resp.expiresAt, Qt::ISODateWithMs);
            _tokenExpiresAt = dt.isValid()
                ? dt.toSecsSinceEpoch()
                : (QDateTime::currentSecsSinceEpoch() + 3600);

            // Master key: faqat birinchi marta (mavjud bo'lmasa) yaratiladi!
            // Mavjud bo'lsa hech qachon almashtirilmaydi (tarix undecryptable bo'lib qolmasligi uchun).
            // OGOHLANTIRISH / XAVF: Ikkinchi qurilma bugun boshqa master key yaratadi (gap)!
            if (!Outbox::EnsureMasterKeyCreated()) {
                if (done) done(false, QStringLiteral("master_key_creation_failed"));
                return;
            }

            if (done) done(true, QString());
        } else {
            if (done) done(false, resp.error.isEmpty() ? QStringLiteral("enroll_failed") : resp.error);
        }
    });
}

void Client::push(
        const QVector<Record> &records,
        Fn<void(bool success, QVector<PushResult> results, QString error)> done) {
    if (records.isEmpty()) {
        if (done) done(true, {}, QString());
        return;
    }

    ensureAccessToken([this, records, done](bool authOk) {
        if (!authOk) {
            if (done) done(false, {}, QStringLiteral("auth_failed"));
            return;
        }
        QNetworkRequest req(makeUrl(QStringLiteral("/api/v1/sync/push")));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setRawHeader("Authorization", "Bearer " + _accessToken.toLatin1());

        QJsonArray recordsArr;
        for (const auto &rec : records) {
            recordsArr.append(ToJson(rec));
        }
        QJsonObject body{
            { QStringLiteral("records"), recordsArr },
        };

        auto *reply = _network->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, [reply, done] {
            reply->deleteLater();
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto data = reply->readAll();
            const auto resp = ParsePushResponse(data, httpStatus);

            if (httpStatus == 200 && resp.error.isEmpty()) {
                if (done) done(true, resp.results, QString());
            } else {
                // batch_too_large bo'lsa server bildirgan max'ni sozlamaga olamiz (K1)
                if (resp.error == QStringLiteral("batch_too_large") && resp.maxBatch > 0) {
                    CustomSettings::SetInt(QStringLiteral("syncPushChunkSize"), resp.maxBatch);
                }
                if (done) done(false, {}, resp.error);
            }
        });
    });
}

void Client::pull(
        qint64 since,
        int limit,
        Fn<void(bool success, QVector<Record> records,
                qint64 nextSince, bool hasMore, QString error)> done) {
    ensureAccessToken([this, since, limit, done](bool authOk) {
        if (!authOk) {
            if (done) done(false, {}, 0, false, QStringLiteral("auth_failed"));
            return;
        }
        const QString path = QStringLiteral("/api/v1/sync/pull?since=%1&limit=%2").arg(since).arg(limit);
        QNetworkRequest req(makeUrl(path));
        req.setRawHeader("Authorization", "Bearer " + _accessToken.toLatin1());

        auto *reply = _network->get(req);
        connect(reply, &QNetworkReply::finished, this, [reply, done] {
            reply->deleteLater();
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto data = reply->readAll();
            const auto resp = ParsePullResponse(data, httpStatus);

            if (httpStatus == 200 && resp.error.isEmpty()) {
                if (done) done(true, resp.records, resp.nextSince, resp.hasMore, QString());
            } else {
                if (done) done(false, {}, 0, false, resp.error);
            }
        });
    });
}

void Client::mediaExists(const QString &hash, Fn<void(bool exists)> done) {
    ensureAccessToken([this, hash, done](bool authOk) {
        if (!authOk) {
            if (done) done(false);
            return;
        }
        QNetworkRequest req(makeUrl(QStringLiteral("/api/v1/media/") + hash));
        req.setRawHeader("Authorization", "Bearer " + _accessToken.toLatin1());

        auto *reply = _network->sendCustomRequest(req, "HEAD");
        connect(reply, &QNetworkReply::finished, this, [reply, done] {
            reply->deleteLater();
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (done) done(httpStatus == 200);
        });
    });
}

void Client::mediaUpload(
        const QString &hash,
        const QByteArray &encrypted,
        Fn<void(bool success)> done) {
    ensureAccessToken([this, hash, encrypted, done](bool authOk) {
        if (!authOk) {
            if (done) done(false);
            return;
        }
        QNetworkRequest req(makeUrl(QStringLiteral("/api/v1/media/") + hash));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
        req.setRawHeader("Authorization", "Bearer " + _accessToken.toLatin1());

        auto *reply = _network->put(req, encrypted);
        connect(reply, &QNetworkReply::finished, this, [reply, done] {
            reply->deleteLater();
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (done) done(httpStatus == 200 || httpStatus == 201);
        });
    });
}

void Client::mediaDownload(
        const QString &hash,
        Fn<void(bool success, QByteArray data)> done) {
    ensureAccessToken([this, hash, done](bool authOk) {
        if (!authOk) {
            if (done) done(false, {});
            return;
        }
        QNetworkRequest req(makeUrl(QStringLiteral("/api/v1/media/") + hash));
        req.setRawHeader("Authorization", "Bearer " + _accessToken.toLatin1());

        auto *reply = _network->get(req);
        connect(reply, &QNetworkReply::finished, this, [reply, done] {
            reply->deleteLater();
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto data = reply->readAll();
            if (done) done(httpStatus == 200, httpStatus == 200 ? data : QByteArray());
        });
    });
}

namespace {

// Yozuvning shifrlanadigan mazmuni. HALI YOZILMAGAN.
//
// Har bir kind uchun mazmun lokal jadvallardan o'qiladi: o'chirilgan
// xabar matni, tahrir tarixi, faollik qiymatlari, media indeks qatori.
// Bu Task 7 da yoziladi.
//
// Ungacha nullopt qaytaradi va yozuv JO'NATILMAYDI. Bo'sh payload bilan
// jo'natish qaytarib bo'lmas edi: server "created" qaytaradi, MarkSent
// yozuvni outbox'dan o'chiradi, record_id esa deterministik bo'lgani
// uchun keyingi urinish "duplicate" oladi -- ya'ni hodisa butunlay
// yo'qoladi va uni qayta yuborishning iloji qolmaydi.
[[nodiscard]] std::optional<QByteArray> BuildPayload(const OutboxEntry &entry) {
    Q_UNUSED(entry);
    return std::nullopt;
}

} // namespace

void Client::pushPending(Fn<void(int sentCount, int failedCount)> done) {
    if (!Outbox::KeysAvailable()) {
        if (done) done(0, 0);
        return;
    }

    const int chunkSize = CustomSettings::SyncPushChunkSize();
    const auto entries = Outbox::Pending(chunkSize);
    if (entries.isEmpty()) {
        if (done) done(0, 0);
        return;
    }

    const auto deviceId = Outbox::GetState(QStringLiteral("device_id"));
    const auto peerKey = Outbox::PeerKey();
    const auto accountKey = Outbox::AccountKey();
    const auto contentKey = Outbox::ContentKey();

    QVector<Record> records;
    records.reserve(entries.size());
    for (const auto &e : entries) {
        Record rec;
        rec.recordId = e.recordId;
        rec.kind = e.kind;
        rec.peerHash = Crypto::ComputePeerHash(peerKey, e.peerId);
        if (e.kind != QLatin1String(Kind::Activity)) {
            rec.accountHash = Crypto::ComputeAccountHash(accountKey, QString::number(e.accountId));
        }
        rec.msgId = e.msgId;
        rec.occurredAt = e.occurredAt;
        rec.observedAt = e.observedAt;
        rec.deviceId = deviceId;
        rec.targetRecordId = e.targetRecordId;
        const auto plain = BuildPayload(e);
        if (!plain.has_value()) {
            // Yozuv outbox'da QOLADI -- yo'qolmaydi, payload yozilgach ketadi.
            continue;
        }

        rec.nonce = Crypto::RandomBytes(12);
        // Seal() QByteArray qaytaradi, optional emas -- bo'sh natija xatolik
        // demakdir. (Bo'sh matn shifrlanganda ham 16 baytlik tag qaytadi.)
        const auto enc = Crypto::Seal(contentKey, rec.nonce, *plain);
        if (enc.isEmpty()) {
            continue;
        }
        rec.payload = enc;
        records.append(rec);
    }

    if (records.isEmpty()) {
        // Jo'natadigan hech nima yo'q (payload hali qurilmaydi).
        if (done) done(0, 0);
        return;
    }

    push(records, [done](bool success, QVector<PushResult> results, QString error) {
        int sent = 0;
        int failed = 0;
        if (success) {
            for (const auto &r : results) {
                if (r.status == QStringLiteral("created")
                        || r.status == QStringLiteral("duplicate")
                        || r.status == QStringLiteral("superseded")) {
                    Outbox::MarkSent(r.recordId);
                    sent++;
                } else if (r.status == QStringLiteral("error")) {
                    Outbox::MarkFailed(r.recordId, r.message);
                    failed++;
                }
            }
        }
        if (done) done(sent, failed);
    });
}

} // namespace CustomSync
