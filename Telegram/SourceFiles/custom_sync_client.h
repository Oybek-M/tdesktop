#pragma once

#include "custom_sync_record.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace CustomSync {

template <typename T>
using Fn = std::function<T>;

struct PushResult {
    QString recordId;
    QString status;   // created | duplicate | superseded | error
    QString message;
};

struct EnrollResponse {
    QString deviceId;
    QString refreshToken;
    QString accessToken;
    QString expiresAt;
    QString error;
};

struct RefreshResponse {
    QString refreshToken;
    QString accessToken;
    QString expiresAt;
    QString error;
};

struct PushResponse {
    QVector<PushResult> results;
    int maxBatch = 0;
    QString error;
};

struct PullResponse {
    QVector<Record> records;
    qint64 nextSince = 0;
    bool hasMore = false;
    QString error;
};

// Tarmoqsiz test qilinishi mumkin bo'lgan erkin parser funksiyalar:
[[nodiscard]] EnrollResponse ParseEnrollResponse(const QByteArray &jsonBytes, int httpStatus);
[[nodiscard]] RefreshResponse ParseRefreshResponse(const QByteArray &jsonBytes, int httpStatus);
[[nodiscard]] PushResponse ParsePushResponse(const QByteArray &jsonBytes, int httpStatus);
[[nodiscard]] PullResponse ParsePullResponse(const QByteArray &jsonBytes, int httpStatus);

// Serverga HTTP orqali murojaat. Barcha chaqiruvlar asinxron —
// UI oqimi hech qachon bloklanmaydi (qoida K2).
// QNetworkAccessManager faqat bitta (GUI) oqimda ishlaydi.
class Client : public QObject {
    Q_OBJECT

public:
    explicit Client(QObject *parent = nullptr);
    ~Client() override;

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

    void mediaExists(const QString &hash, Fn<void(bool exists)> done);

    // mediaUpload: parametr faqat hash va encrypted baytlar (nonce record ichida bor).
    // Hash ochiq matn (plaintext) ustidan hisoblangan bo'lishi shart (spec §0.5).
    void mediaUpload(
        const QString &hash,
        const QByteArray &encrypted,
        Fn<void(bool success)> done);

    void mediaDownload(
        const QString &hash,
        Fn<void(bool success, QByteArray data)> done);

    // Outbox-dagi navbatda turgan yozuvlarni serverga push qiladi
    void pushPending(Fn<void(int sentCount, int failedCount)> done = nullptr);

private:
    void ensureAccessToken(Fn<void(bool success)> done);
    [[nodiscard]] QUrl makeUrl(const QString &path) const;

    QNetworkAccessManager *_network = nullptr;
    QString _accessToken;
    qint64 _tokenExpiresAt = 0; // unix timestamp (soniya)
    bool _refreshing = false;
    QVector<Fn<void(bool success)>> _pendingTokenWaiters;
};

} // namespace CustomSync
