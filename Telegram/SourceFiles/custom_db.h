#pragma once

#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

class HistoryItem;

namespace CustomDB {

struct ActionedMessage {
    QString peerId;
    long long msgId;
    QString type; // "deleted", "edited", "backup"
    QString originalText;
    QString newText;
    QString mediaPath;
    QDateTime timestamp;
};

void Init();
void SaveGhostRead(const QString &peerId, long long msgId);
long long GetGhostRead(const QString &peerId);
void MarkDeleted(long long msgId, const QString &peerId, const QString &mediaPath = QString());
void SaveActionedMessage(const ActionedMessage &msg);
QString GetMessageHistory(long long msgId, const QString &peerId);
struct DeletedMessage {
    long long msgId;
    QString mediaPath;
    bool isOut;
    unsigned int date;
    QString text;
};
QVector<DeletedMessage> GetDeletedMessages(const QString &peerId);
void SaveMessage(HistoryItem *item);

void ExportDatabase(const QString &targetPath);
void ImportDatabase(const QString &sourcePath);

// New: Media storage functions
QString SaveMediaFile(const QString &sourcePath, const QString &type); // "image", "video", "voice", "file"

} // namespace CustomDB
