#pragma once

#include "custom_sync_record.h"
#include "custom_sync_keyshare.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
#include <QtNetwork/QAbstractSocket>
class QWebSocket;
class QTimer;
#endif

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

enum class MergeStatus {
    Merged,
    Rejected,
    Corrupt,
    TombstoneSkipped,
    Unsupported,
};

struct MergeResult {
    MergeStatus status = MergeStatus::Unsupported;
    QString reason;
};

// Bitta yozuvni deshifrlab, hash'larini tekshirib, retention filtri orqali
// o'tkazib lokal bazaga (CustomDB) yozadi. MergeGuard ostida ishlaydi.
[[nodiscard]] MergeResult MergeRecord(
    const Record &record,
    const QByteArray &contentKey,
    const QByteArray &peerKey,
    const QByteArray &accountKey);

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

    // Serverdan yozuvlarni tortib olib (pull), ularni lokal bazaga kiritadi (merge)
    void pullAndMerge(Fn<void(int merged, int rejected, bool hasMore, QString error)> done);

    // Key sharing (Task 12a)
    void listKeyWraps(Fn<void(bool ok, QVector<KeyShare::Wrap> wraps, QString error)> done);
    void getKeyWrap(const QString &wrapId, Fn<void(bool ok, KeyShare::Wrap wrap, QString error)> done);
    void createKeyWrap(const KeyShare::Wrap &wrap, Fn<void(bool ok, QString wrapId, QString error)> done);

#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
    // WebSocket boshqaruvi (K5: faqat sync yoqilgan va autentifikatsiya qilingan bo'lsa)
    void startWebSocket();
    void stopWebSocket();
#endif

    // Soket ulangan bo'lsa server o'zgarishlarni ITARADI, ya'ni qisqa
    // intervalli so'rab turishning keragi yo'q (orkestrator intervalni
    // shunga qarab uzaytiradi). WebSocket'siz qurilmada doim false.
    [[nodiscard]] bool webSocketConnected() const;

Q_SIGNALS:
#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
    void changesAvailable(qint64 seq);
#endif

private:
    void ensureAccessToken(Fn<void(bool success)> done);
    [[nodiscard]] QUrl makeUrl(const QString &path) const;

#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
    [[nodiscard]] QUrl makeWebSocketUrl() const;
    void handleWebSocketMessage(const QString &message);
    void handleWebSocketClosed();
    void handleWebSocketError(QAbstractSocket::SocketError error);
    void scheduleWebSocketReconnect();

    QWebSocket *_socket = nullptr;
    QTimer *_wsReconnectTimer = nullptr;
    int _wsBackoffSeconds = 1;
#endif

    QNetworkAccessManager *_network = nullptr;
    QString _accessToken;
    qint64 _tokenExpiresAt = 0; // unix timestamp (soniya)
    bool _refreshing = false;
    QVector<Fn<void(bool success)>> _pendingTokenWaiters;
};

} // namespace CustomSync
