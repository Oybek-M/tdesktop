#include "custom_sync_client.h"
#include "custom_sync_outbox.h"
#include "custom_sync_crypto.h"
#include "custom_sync_payload.h"
#include "custom_settings.h"
#include "custom_db.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <sqlite3.h>

#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketProtocol>
#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>
#endif

#include <optional>
#include <algorithm>

namespace CustomSync {

#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
// WebSocket qayta ulanish chegaralari (eksponensial backoff: 1s -> 60s)
constexpr int kMinWsReconnectSeconds = 1;
constexpr int kMaxWsReconnectSeconds = 60;
#endif

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

Client::~Client() {
#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
    stopWebSocket();
#endif
}

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
#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
        startWebSocket();
#endif
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
#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
            startWebSocket();
#endif
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

#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
            startWebSocket();
#endif
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

void Client::pushPending(Fn<void(int sentCount, int failedCount)> done) {
    if (!Outbox::KeysAvailable()) {
        if (done) done(0, 0);
        return;
    }

    // 3-bosqich: navbat ANIQ bo'sh bo'lsa Pending() so'rovi ham shart emas
    // (hisoblagich hech qachon haqiqiy sondan kam bo'lmaydi).
    if (Outbox::ProbablyEmpty()) {
        if (done) done(0, 0);
        return;
    }

    const int chunkSize = CustomSettings::SyncPushChunkSize();
    const auto entries = Outbox::Pending(chunkSize);
    if (entries.isEmpty()) {
        // Hisoblagich eskirgan: yozuvlar yuborib bo'lingan yoki backoff'da
        // kutyapti. Aniq songa qaytaramiz -- shunda keyingi bo'sh sikllar
        // so'rovsiz o'tadi.
        Outbox::ResyncRowCount();
        if (done) done(0, 0);
        return;
    }

    const auto deviceId = Outbox::GetState(QStringLiteral("device_id"));
    const auto peerKey = Outbox::PeerKey();
    const auto accountKey = Outbox::AccountKey();
    const auto contentKey = Outbox::ContentKey();

    int droppedCount = 0;
    QVector<Record> records;
    records.reserve(entries.size());
    for (const auto &e : entries) {
        const auto buildRes = CustomSync::Build(e);
        if (buildRes.status == BuildStatus::SourceGone) {
            // Manba qator bazadan o'chirilgan (masalan arxiv tozalangan) -- outboxdan drop qilamiz
            Outbox::Drop(e.recordId, QStringLiteral("source_gone"));
            droppedCount++;
            continue;
        } else if (buildRes.status != BuildStatus::Ok) {
            // Kind hali qo'llab-quvvatlanmaydi yoki vaqtincha xatolik -- outbox'da qoladi
            continue;
        }

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

        rec.nonce = Crypto::RandomBytes(12);
        // Seal() QByteArray qaytaradi, optional emas -- bo'sh natija xatolik
        // demakdir. (Bo'sh matn shifrlanganda ham 16 baytlik tag qaytadi.)
        const auto enc = Crypto::Seal(contentKey, rec.nonce, buildRes.json);
        if (enc.isEmpty()) {
            continue;
        }
        rec.payload = enc;
        records.append(rec);
    }

    if (droppedCount > 0) {
        qWarning().noquote() << QStringLiteral("[Sync] pushPending: %1 ta yetishmayotgan (source gone) yozuv outboxdan olib tashlandi.")
            .arg(droppedCount);
    }

    if (records.isEmpty()) {
        if (done) done(0, 0);
        return;
    }

    // Butun to'plam yiqilganda qaysi yozuvlarni belgilashni bilishimiz
    // uchun id'larni saqlab qo'yamiz -- javobda ular bo'lmaydi.
    QVector<QString> batchIds;
    batchIds.reserve(records.size());
    for (const auto &r : records) {
        batchIds.append(r.recordId);
    }

    push(records, [done, batchIds](
            bool success, QVector<PushResult> results, QString error) {
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
        } else {
            // Tarmoq yoki HTTP xatosi -- javob umuman kelmadi. Har bir
            // yozuvni MarkFailed qilamiz, aks holda next_retry_at
            // surilmaydi va orkestrator har siklda o'sha to'plamni
            // backoff'siz qayta yuboraveradi.
            for (const auto &id : batchIds) {
                Outbox::MarkFailed(id, error);
            }
            failed = int(batchIds.size());
        }
        if (done) done(sent, failed);
    });
}

namespace {

static void bindText(sqlite3_stmt *stmt, int index, const QString &str) {
    if (str.isEmpty()) {
        sqlite3_bind_text(stmt, index, "", 0, SQLITE_STATIC);
    } else {
        const auto utf8 = str.toUtf8();
        sqlite3_bind_text(stmt, index, utf8.constData(), utf8.size(), SQLITE_TRANSIENT);
    }
}

static QString colText(sqlite3_stmt *stmt, int col) {
    const auto *text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? QString::fromUtf8(text) : QString();
}

static bool HasEditedMessage(sqlite3 *db, const CustomDB::PeerKey &key, qint64 msgId) {
    if (!db) return false;
    sqlite3_stmt *stmt = nullptr;
    bool exists = false;
    if (sqlite3_prepare_v2(db,
            "SELECT 1 FROM actioned_messages "
            "WHERE peer_id = ? AND msg_id = ? AND type = 'edited' AND account_id IN (0, ?) "
            "LIMIT 1",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, key.peerId);
        sqlite3_bind_int64(stmt, 2, msgId);
        sqlite3_bind_int64(stmt, 3, key.accountId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = true;
        }
        sqlite3_finalize(stmt);
    }
    return exists;
}

static void RecordCorrupt(const QString &recordId) {
    qWarning().noquote() << QStringLiteral("[Sync] Buzilgan (corrupt) yozuv aniqlandi va o'tkazib yuborildi: record_id=") << recordId;
    const auto current = Outbox::GetState(QStringLiteral("corrupt_records"));
    auto list = current.isEmpty() ? QStringList() : current.split(QLatin1Char(','));
    list.append(recordId);
    while (list.size() > 50) {
        list.removeFirst();
    }
    Outbox::SetState(QStringLiteral("corrupt_records"), list.join(QLatin1Char(',')));
}

static void AddPendingTombstone(const QString &targetRecordId) {
    if (targetRecordId.isEmpty()) return;
    const auto current = Outbox::GetState(QStringLiteral("pending_tombstones"));
    auto list = current.isEmpty() ? QStringList() : current.split(QLatin1Char(','));
    if (!list.contains(targetRecordId)) {
        list.append(targetRecordId);
        while (list.size() > 50) {
            list.removeFirst();
        }
        Outbox::SetState(QStringLiteral("pending_tombstones"), list.join(QLatin1Char(',')));
    }
}

static bool CheckAndRemovePendingTombstone(const QString &recordId) {
    if (recordId.isEmpty()) return false;
    const auto current = Outbox::GetState(QStringLiteral("pending_tombstones"));
    if (current.isEmpty()) return false;
    auto list = current.split(QLatin1Char(','));
    const int removed = list.removeAll(recordId);
    if (removed > 0) {
        Outbox::SetState(QStringLiteral("pending_tombstones"), list.join(QLatin1Char(',')));
        return true;
    }
    return false;
}

static void ApplyTombstone(const Outbox::RecordMapEntry &target) {
    const CustomDB::PeerKey key{
        .accountId = target.accountId,
        .peerId = target.peerId,
    };

    if (target.kind == QLatin1String(Kind::Deleted)) {
        CustomDB::DeleteDeletedMessageForSync(key, target.msgId);
        return;
    }

    if (target.kind == QLatin1String(Kind::Edited)) {
        CustomDB::DeleteEditedMessageForSync(key, target.msgId);
        return;
    }

    if (target.kind == QLatin1String(Kind::GhostRead)) {
        CustomDB::ResetGhostRead(key);
        return;
    }

    if (target.kind == QLatin1String(Kind::MediaIndex)) {
        CustomDB::DeleteMediaIndexForSync(key, target.msgId);
        return;
    }

    if (target.kind == QLatin1String(Kind::Activity)) {
        auto *db = CustomDB::RawHandle();
        if (!db) return;
        sqlite3_stmt *stmt = nullptr;
        // DIQQAT: spec §0.13 ga asosan account_id filtrisiz o'qiladi
        if (sqlite3_prepare_v2(db,
                "SELECT id, field FROM activity_history WHERE peer_id = ? AND observed_at = ?",
                -1, &stmt, nullptr) == SQLITE_OK) {
            bindText(stmt, 1, target.peerId);
            sqlite3_bind_int64(stmt, 2, target.occurredAt);
            qint64 matchedId = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const qint64 id = sqlite3_column_int64(stmt, 0);
                const QString field = colText(stmt, 1);
                if (CustomSync::DiscriminatorFor(field) == target.msgId) {
                    matchedId = id;
                    break;
                }
            }
            sqlite3_finalize(stmt);
            if (matchedId > 0) {
                CustomDB::DeleteActivityEntryForSync(matchedId);
            }
        }
        return;
    }
}

} // namespace

MergeResult MergeRecord(
        const Record &record,
        const QByteArray &contentKey,
        const QByteArray &peerKey,
        const QByteArray &accountKey) {
    // 0. Pending tombstones tekshiruvi (§0.13, 3-band)
    // Agar bu record_id uchun avval tombstone kelgan bo'lsa, merge qilinmaydi,
    // pending_tombstones ro'yxatidan o'chiriladi va rejected deb hisoblanadi.
    if (CheckAndRemovePendingTombstone(record.recordId)) {
        qDebug().noquote() << QStringLiteral("[Sync] Yozuv pending_tombstones ro'yxatida topildi va merge bekor qilindi: record_id=")
            << record.recordId;
        return { MergeStatus::Rejected, QStringLiteral("killed_by_pending_tombstone") };
    }

    // Tombstone yozuvini qayta ishlash (Task 7c)
    if (record.kind == QLatin1String(Kind::Tombstone)) {
        if (record.targetRecordId.isEmpty()) {
            return { MergeStatus::Corrupt, QStringLiteral("tombstone_missing_target") };
        }
        const auto mapped = Outbox::LookupRecordMap(record.targetRecordId);
        if (!mapped.has_value()) {
            // Target hali xaritada yo'q -- kechikib kelishi mumkin (§0.13, 3-band).
            // pending_tombstones ro'yxatiga saqlaymiz.
            AddPendingTombstone(record.targetRecordId);
            return { MergeStatus::Merged, QStringLiteral("tombstone_pending") };
        }
        ApplyTombstone(*mapped);
        return { MergeStatus::Merged, QString() };
    }

    // 1. Retention filter (faqat activity va ghost_read uchun!)
    // deleted, edited, media_index hech qachon prune qilinmaydi va cheksiz saqlanadi.
    const qint64 retentionCutoff = QDateTime::currentDateTime()
        .addDays(-CustomDB::kActivityRetentionDays)
        .toSecsSinceEpoch();

    if (record.kind == QLatin1String(Kind::Activity)
            || record.kind == QLatin1String(Kind::GhostRead)) {
        if (record.occurredAt < retentionCutoff) {
            return { MergeStatus::Rejected, QStringLiteral("outside_retention_window") };
        }
    }

    // 2. Deshifrlash (AES-256-GCM)
    const auto plain = Crypto::Open(contentKey, record.nonce, record.payload);
    if (!plain.has_value()) {
        return { MergeStatus::Corrupt, QStringLiteral("decrypt_failed") };
    }

    // 3. JSON parse va pre-image'larni o'qish (§0.14)
    const auto doc = QJsonDocument::fromJson(*plain);
    if (!doc.isObject()) {
        return { MergeStatus::Corrupt, QStringLiteral("json_not_object") };
    }
    const auto obj = doc.object();
    const auto accountIdStr = obj.value(QStringLiteral("account_id")).toString();
    const auto peerId = obj.value(QStringLiteral("peer_id")).toString();
    if (accountIdStr.isEmpty() || peerId.isEmpty()) {
        return { MergeStatus::Corrupt, QStringLiteral("missing_account_or_peer_id") };
    }

    // 4. Hash'larni pre-image bilan solishtirish (yaxlitlik va to'g'ri kalit tekshiruvi)
    const auto expectedPeerHash = Crypto::ComputePeerHash(peerKey, peerId);
    if (expectedPeerHash != record.peerHash) {
        return { MergeStatus::Rejected, QStringLiteral("peer_hash_mismatch") };
    }
    if (record.kind != QLatin1String(Kind::Activity)) {
        const auto expectedAccountHash = Crypto::ComputeAccountHash(accountKey, accountIdStr);
        if (expectedAccountHash != record.accountHash) {
            return { MergeStatus::Rejected, QStringLiteral("account_hash_mismatch") };
        }
    }

    bool accountIdOk = false;
    const qint64 accountId = accountIdStr.toLongLong(&accountIdOk);
    if (!accountIdOk) {
        return { MergeStatus::Corrupt, QStringLiteral("invalid_account_id") };
    }
    const CustomDB::PeerKey key{ .accountId = accountId, .peerId = peerId };

    // 5. Lokal bazaga yozish -- MergeGuard ostida!
    Outbox::MergeGuard guard;

    if (record.kind == QLatin1String(Kind::Deleted)) {
        const auto text = obj.value(QStringLiteral("text")).toString();
        const auto senderId = obj.value(QStringLiteral("sender_id")).toString();
        const bool isOut = obj.value(QStringLiteral("is_out")).toBool();
        const bool isMedia = obj.value(QStringLiteral("is_media")).toBool();
        const auto msgDate = static_cast<unsigned int>(std::max<qint64>(0, record.occurredAt));
        CustomDB::MarkDeleted(record.msgId, key, QString(), text, msgDate, isOut, senderId, isMedia);
        Outbox::SaveRecordMap(record.recordId, record.kind, key.accountId, key.peerId, record.msgId, record.occurredAt);
        return { MergeStatus::Merged, QString() };
    }

    if (record.kind == QLatin1String(Kind::Edited)) {
        auto *db = CustomDB::RawHandle();
        if (HasEditedMessage(db, key, record.msgId)) {
            // Allaqachon mavjud -- qayta insert qilmaymiz (K4 idempotency)
            Outbox::SaveRecordMap(record.recordId, record.kind, key.accountId, key.peerId, record.msgId, record.occurredAt);
            return { MergeStatus::Merged, QStringLiteral("already_exists") };
        }
        const auto oldText = obj.value(QStringLiteral("old_text")).toString();
        const auto newText = obj.value(QStringLiteral("new_text")).toString();
        const bool isOut = obj.value(QStringLiteral("is_out")).toBool();
        CustomDB::ActionedMessage msg;
        msg.accountId = key.accountId;
        msg.peerId = key.peerId;
        msg.msgId = record.msgId;
        msg.type = QStringLiteral("edited");
        msg.originalText = oldText;
        msg.newText = newText;
        msg.isOut = isOut;
        msg.msgDate = static_cast<unsigned int>(std::max<qint64>(0, record.occurredAt));
        msg.timestamp = (record.observedAt > 0)
            ? QDateTime::fromSecsSinceEpoch(record.observedAt)
            : QDateTime::currentDateTime();
        CustomDB::SaveActionedMessage(msg);
        Outbox::SaveRecordMap(record.recordId, record.kind, key.accountId, key.peerId, record.msgId, record.occurredAt);
        return { MergeStatus::Merged, QString() };
    }

    if (record.kind == QLatin1String(Kind::Activity)) {
        const auto field = obj.value(QStringLiteral("field")).toString();
        const bool hasOldValue = obj.value(QStringLiteral("has_old_value")).toBool();
        const auto oldValue = hasOldValue ? obj.value(QStringLiteral("old_value")).toString() : QString();
        const auto newValue = obj.value(QStringLiteral("new_value")).toString();
        if (CustomDB::HasActivityEntryAt(key.peerId, field, record.occurredAt)) {
            // Allaqachon mavjud -- qayta insert qilmaymiz (K4 idempotency)
            Outbox::SaveRecordMap(record.recordId, record.kind, key.accountId, key.peerId, record.msgId, record.occurredAt);
            return { MergeStatus::Merged, QStringLiteral("already_exists") };
        }
        CustomDB::SaveActivityHistoryEntry(key, field, hasOldValue, oldValue, newValue, record.occurredAt, u"observed"_q);
        Outbox::SaveRecordMap(record.recordId, record.kind, key.accountId, key.peerId, record.msgId, record.occurredAt);
        return { MergeStatus::Merged, QString() };
    }

    if (record.kind == QLatin1String(Kind::GhostRead)) {
        CustomDB::SaveGhostRead(key, record.msgId);
        Outbox::SaveRecordMap(record.recordId, record.kind, key.accountId, key.peerId, record.msgId, record.occurredAt);
        return { MergeStatus::Merged, QString() };
    }

    if (record.kind == QLatin1String(Kind::MediaIndex)) {
        CustomDB::MediaIndexEntry entry;
        entry.peerId = key.peerId;
        entry.msgId = record.msgId;
        entry.kind = obj.value(QStringLiteral("kind")).toString();
        entry.fileName = obj.value(QStringLiteral("file_name")).toString();
        entry.relPath = obj.value(QStringLiteral("rel_path")).toString();
        entry.size = obj.value(QStringLiteral("size")).toVariant().toLongLong();
        entry.sha256 = obj.value(QStringLiteral("sha256")).toString();
        entry.status = obj.value(QStringLiteral("status")).toString();
        entry.reason = obj.value(QStringLiteral("reason")).toString();
        entry.layer = obj.value(QStringLiteral("layer")).toString();
        entry.msgDate = static_cast<unsigned int>(obj.value(QStringLiteral("msg_date")).toVariant().toLongLong());
        entry.archivedAt = static_cast<unsigned int>(QDateTime::currentSecsSinceEpoch());
        CustomDB::UpsertMediaIndex(key, entry);
        Outbox::SaveRecordMap(record.recordId, record.kind, key.accountId, key.peerId, record.msgId, record.occurredAt);
        return { MergeStatus::Merged, QString() };
    }

    return { MergeStatus::Unsupported, QStringLiteral("unsupported_kind") };
}

void Client::pullAndMerge(Fn<void(int merged, int rejected, bool hasMore, QString error)> done) {
    if (!Outbox::KeysAvailable()) {
        if (done) done(0, 0, false, QString());
        return;
    }

    if (Outbox::GetState(QStringLiteral("tombstone_backfill_done")) != QStringLiteral("1")) {
        Outbox::SetState(QStringLiteral("pull_cursor"), QStringLiteral("0"));
        Outbox::SetState(QStringLiteral("tombstone_backfill_done"), QStringLiteral("1"));
    }

    const auto cursorStr = Outbox::GetState(QStringLiteral("pull_cursor"), QStringLiteral("0"));
    const qint64 since = cursorStr.toLongLong();
    const int limit = CustomSettings::SyncPushChunkSize();

    pull(since, limit, [done](bool success, QVector<Record> records, qint64 nextSince, bool hasMore, QString error) {
        if (!success) {
            if (done) done(0, 0, false, error);
            return;
        }

        const auto peerKey = Outbox::PeerKey();
        const auto accountKey = Outbox::AccountKey();
        const auto contentKey = Outbox::ContentKey();

        int mergedCount = 0;
        int rejectedCount = 0;

        for (const auto &rec : records) {
            const auto res = MergeRecord(rec, contentKey, peerKey, accountKey);
            switch (res.status) {
            case MergeStatus::Merged:
                mergedCount++;
                break;
            case MergeStatus::Corrupt:
                RecordCorrupt(rec.recordId);
                rejectedCount++;
                break;
            case MergeStatus::Rejected:
            case MergeStatus::TombstoneSkipped:
            case MergeStatus::Unsupported:
                rejectedCount++;
                break;
            }
        }

        Outbox::SetState(QStringLiteral("pull_cursor"), QString::number(nextSince));

        if (done) done(mergedCount, rejectedCount, hasMore, QString());
    });
}

void Client::listKeyWraps(Fn<void(bool ok, QVector<KeyShare::Wrap> wraps, QString error)> done) {
    ensureAccessToken([this, done](bool authOk) {
        if (!authOk) {
            if (done) done(false, {}, QStringLiteral("auth_failed"));
            return;
        }
        QNetworkRequest req(makeUrl(QStringLiteral("/api/v1/keys/wraps")));
        req.setRawHeader("Authorization", "Bearer " + _accessToken.toLatin1());

        auto *reply = _network->get(req);
        connect(reply, &QNetworkReply::finished, this, [reply, done] {
            reply->deleteLater();
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto data = reply->readAll();
            const auto resp = KeyShare::ParseKeyWrapListResponse(data, httpStatus);
            if (httpStatus == 200 && resp.error.isEmpty()) {
                if (done) done(true, resp.wraps, QString());
            } else {
                if (done) done(false, {}, resp.error.isEmpty() ? QStringLiteral("list_wraps_failed") : resp.error);
            }
        });
    });
}

void Client::getKeyWrap(const QString &wrapId, Fn<void(bool ok, KeyShare::Wrap wrap, QString error)> done) {
    if (wrapId.isEmpty()) {
        if (done) done(false, {}, QStringLiteral("empty_wrap_id"));
        return;
    }
    ensureAccessToken([this, wrapId, done](bool authOk) {
        if (!authOk) {
            if (done) done(false, {}, QStringLiteral("auth_failed"));
            return;
        }
        QNetworkRequest req(makeUrl(QStringLiteral("/api/v1/keys/wraps/") + wrapId));
        req.setRawHeader("Authorization", "Bearer " + _accessToken.toLatin1());

        auto *reply = _network->get(req);
        connect(reply, &QNetworkReply::finished, this, [reply, done] {
            reply->deleteLater();
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto data = reply->readAll();
            const auto resp = KeyShare::ParseKeyWrapResponse(data, httpStatus);
            if (httpStatus == 200 && resp.error.isEmpty()) {
                if (done) done(true, resp.wrap, QString());
            } else {
                if (done) done(false, {}, resp.error.isEmpty() ? QStringLiteral("get_wrap_failed") : resp.error);
            }
        });
    });
}

void Client::createKeyWrap(const KeyShare::Wrap &wrap, Fn<void(bool ok, QString wrapId, QString error)> done) {
    ensureAccessToken([this, wrap, done](bool authOk) {
        if (!authOk) {
            if (done) done(false, QString(), QStringLiteral("auth_failed"));
            return;
        }
        QNetworkRequest req(makeUrl(QStringLiteral("/api/v1/keys/wraps")));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setRawHeader("Authorization", "Bearer " + _accessToken.toLatin1());

        QJsonObject body{
            { QStringLiteral("wrap_type"), wrap.wrapType },
            { QStringLiteral("label"), wrap.label },
            { QStringLiteral("salt"), QString::fromLatin1(wrap.salt.toBase64()) },
            { QStringLiteral("nonce"), QString::fromLatin1(wrap.nonce.toBase64()) },
            { QStringLiteral("wrapped_key"), QString::fromLatin1(wrap.wrappedKey.toBase64()) },
            { QStringLiteral("iterations"), wrap.iterations },
        };

        auto *reply = _network->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, [reply, done] {
            reply->deleteLater();
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto data = reply->readAll();
            const auto resp = KeyShare::ParseCreateKeyWrapResponse(data, httpStatus);
            if ((httpStatus == 200 || httpStatus == 201) && resp.error.isEmpty()) {
                if (done) done(true, resp.wrapId, QString());
            } else {
                if (done) done(false, QString(), resp.error.isEmpty() ? QStringLiteral("create_wrap_failed") : resp.error);
            }
        });
    });
}

quint64 Client::webSocketConnectionId() const {
#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
    return _wsConnectionId;
#else
    return 0;
#endif
}

bool Client::webSocketConnected() const {
#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
    return _socket
        && (_socket->state() == QAbstractSocket::ConnectedState);
#else
    return false;
#endif
}

#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
QUrl Client::makeWebSocketUrl() const {
    auto base = CustomSettings::SyncServerUrl().trimmed();
    while (base.endsWith('/')) {
        base.chop(1);
    }
    QUrl url(base + QStringLiteral("/ws/notify"));
    if (url.scheme() == QStringLiteral("https")) {
        url.setScheme(QStringLiteral("wss"));
    } else if (url.scheme() == QStringLiteral("http")) {
        url.setScheme(QStringLiteral("ws"));
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("access_token"), _accessToken);
    url.setQuery(query);
    return url;
}

void Client::startWebSocket() {
    // 🔴 K5: Faqat quyidagi shartlarning barchasi bajarilgandagina soket ochiladi:
    // SyncEnabled() && device_id mavjud && yaroqli access_token mavjud
    const auto now = QDateTime::currentSecsSinceEpoch();
    const auto deviceId = Outbox::GetState(QStringLiteral("device_id"));
    if (!CustomSettings::SyncEnabled()
        || deviceId.isEmpty()
        || _accessToken.isEmpty()
        || now >= _tokenExpiresAt) {
        stopWebSocket();
        return;
    }

    if (_socket) {
        if (_socket->state() == QAbstractSocket::ConnectedState
            || _socket->state() == QAbstractSocket::ConnectingState) {
            return;
        }
    } else {
        _socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        connect(_socket, &QWebSocket::connected, this, [this] {
            ++_wsConnectionId;
            _wsBackoffSeconds = kMinWsReconnectSeconds;
            if (_wsReconnectTimer) {
                _wsReconnectTimer->stop();
            }
        });
        connect(_socket, &QWebSocket::disconnected, this, &Client::handleWebSocketClosed);
        connect(_socket, &QWebSocket::textMessageReceived, this, &Client::handleWebSocketMessage);
        connect(_socket, &QWebSocket::errorOccurred, this, &Client::handleWebSocketError);
    }

    if (_wsReconnectTimer) {
        _wsReconnectTimer->stop();
    }
    _socket->open(makeWebSocketUrl());
}

void Client::stopWebSocket() {
    if (_wsReconnectTimer) {
        _wsReconnectTimer->stop();
    }
    if (_socket) {
        // Ixtiyoriy yopish paytida takroriy ulanish chaqirilmasligi uchun signallarni uzish
        _socket->disconnect(this);
        _socket->abort();
        _socket->deleteLater();
        _socket = nullptr;
    }
    _wsBackoffSeconds = kMinWsReconnectSeconds;
}

void Client::handleWebSocketMessage(const QString &message) {
    const auto doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "CustomSync: WebSocket invalid JSON received";
        return;
    }
    const auto obj = doc.object();
    const auto type = obj.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("changes")) {
        const auto seq = obj.value(QStringLiteral("seq")).toInteger();
        qInfo() << "CustomSync: WebSocket changes notification received, seq:" << seq;
        Q_EMIT changesAvailable(seq);
    } else {
        // Noma'lum freym turi — bir marta log qilinadi va e'tiborsiz qoldiriladi
        qInfo() << "CustomSync: unknown WebSocket frame type ignored:" << type;
    }
}

namespace {

bool isAuthFailure(QWebSocketProtocol::CloseCode code, const QString &reason, const QString &errorStr) {
    if (code == QWebSocketProtocol::CloseCodePolicyViolated) { // 1008 (RFC 6455 policy violation)
        return true;
    }
    const int codeInt = static_cast<int>(code);
    if (codeInt == 4401 || codeInt == 4403 || codeInt == 4001 || codeInt == 4003) {
        return true;
    }
    // DIQQAT: bu yerda "auth" kabi qisqa bo'lakni qidirmang -- u "author",
    // "authority" kabi begunoh so'zlarga ham tushadi, va close reason
    // matnini server yozadi. Noto'g'ri "auth xatosi" degan xulosa qayta
    // ulanishni butunlay to'xtatadi: soket keyingi sikl tokenni
    // yangilagunicha o'lik qoladi.
    const auto lowerReason = reason.toLower();
    const auto lowerErr = errorStr.toLower();
    if (lowerReason.contains(QStringLiteral("401"))
        || lowerReason.contains(QStringLiteral("403"))
        || lowerReason.contains(QStringLiteral("unauthorized"))
        || lowerReason.contains(QStringLiteral("unauthorised"))
        || lowerReason.contains(QStringLiteral("forbidden"))) {
        return true;
    }
    if (lowerErr.contains(QStringLiteral("401"))
        || lowerErr.contains(QStringLiteral("403"))
        || lowerErr.contains(QStringLiteral("unauthorized"))
        || lowerErr.contains(QStringLiteral("unauthorised"))
        || lowerErr.contains(QStringLiteral("forbidden"))) {
        return true;
    }
    return false;
}

} // namespace

void Client::handleWebSocketClosed() {
    // K5: Agar sync o'chirilgan bo'lsa, qayta ulanish bo'lmaydi
    if (!CustomSettings::SyncEnabled()) {
        stopWebSocket();
        return;
    }

    const auto code = _socket ? _socket->closeCode() : QWebSocketProtocol::CloseCodeNormal;
    const auto reason = _socket ? _socket->closeReason() : QString();
    const auto errStr = _socket ? _socket->errorString() : QString();

    if (isAuthFailure(code, reason, errStr)) {
        // Autentifikatsiya xatosi (401/403): eski token bilan serverni qayta urmaslik
        // uchun bevosita qayta ulanmaymiz. Navbatdagi sikl tokenni yangilaganda
        // startWebSocket chaqiriladi.
        qWarning() << "CustomSync: WebSocket auth failure, close code:" << code
                   << "reason:" << reason << "error:" << errStr;
        if (_wsReconnectTimer) {
            _wsReconnectTimer->stop();
        }
        return;
    }

    // Tarmoq uzilishi bo'lsa — eksponensial backoff bilan qayta ulanish
    scheduleWebSocketReconnect();
}

void Client::handleWebSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    if (!CustomSettings::SyncEnabled()) {
        stopWebSocket();
        return;
    }
    const auto code = _socket ? _socket->closeCode() : QWebSocketProtocol::CloseCodeNormal;
    const auto reason = _socket ? _socket->closeReason() : QString();
    const auto errStr = _socket ? _socket->errorString() : QString();

    if (isAuthFailure(code, reason, errStr)) {
        qWarning() << "CustomSync: WebSocket auth error:" << errStr;
        if (_wsReconnectTimer) {
            _wsReconnectTimer->stop();
        }
        return;
    }

    scheduleWebSocketReconnect();
}

void Client::scheduleWebSocketReconnect() {
    if (!CustomSettings::SyncEnabled()) {
        stopWebSocket();
        return;
    }
    if (!_wsReconnectTimer) {
        _wsReconnectTimer = new QTimer(this);
        _wsReconnectTimer->setSingleShot(true);
        connect(_wsReconnectTimer, &QTimer::timeout, this, [this] {
            startWebSocket();
        });
    }
    if (_wsReconnectTimer->isActive()) {
        return;
    }
    const int delay = _wsBackoffSeconds;
    _wsBackoffSeconds = std::min(_wsBackoffSeconds * 2, kMaxWsReconnectSeconds);
    _wsReconnectTimer->start(delay * 1000);
}
#endif

} // namespace CustomSync
