#include "custom_db.h"
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QDebug>
#include "history/history_item.h"
#include "history/history.h"
#include "data/data_peer.h"

namespace CustomDB {

QSqlDatabase gDb;

void Init() {
    if (gDb.isOpen()) return;

    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/CustomMod";
    QDir().mkpath(dbPath);
    
    gDb = QSqlDatabase::addDatabase("QSQLITE");
    gDb.setDatabaseName(dbPath + "/actioned_messages.db");

    if (!gDb.open()) {
        qDebug() << "Error: connection with database failed" << gDb.lastError().text();
        return;
    }

    QSqlQuery query;
    // Table for Ghost Reads
    query.exec("CREATE TABLE IF NOT EXISTS ghost_reads (peer_id TEXT PRIMARY KEY, msg_id INTEGER, timestamp DATETIME)");
    
    // Table for Edited/Deleted messages (Enhanced with is_out and msg_date)
    query.exec("CREATE TABLE IF NOT EXISTS actioned_messages ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "peer_id TEXT, "
               "msg_id INTEGER, "
               "type TEXT, "
               "original_text TEXT, "
               "new_text TEXT, "
               "media_path TEXT, "
               "is_out INTEGER DEFAULT 0, "
               "msg_date INTEGER DEFAULT 0, "
               "timestamp DATETIME)");
}

void SaveGhostRead(const QString &peerId, long long msgId) {
    Init();
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO ghost_reads (peer_id, msg_id, timestamp) VALUES (?, ?, ?)");
    query.addBindValue(peerId);
    query.addBindValue(msgId);
    query.addBindValue(QDateTime::currentDateTime());
    query.exec();
}

long long GetGhostRead(const QString &peerId) {
    Init();
    QSqlQuery query;
    query.prepare("SELECT msg_id FROM ghost_reads WHERE peer_id = ?");
    query.addBindValue(peerId);
    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    return 0;
}

void MarkDeleted(long long msgId, const QString &peerId, const QString &mediaPath) {
    Init();
    // We try to find existing info if possible, but for now we just record the action
    ActionedMessage msg;
    msg.peerId = peerId;
    msg.msgId = msgId;
    msg.type = "deleted";
    msg.mediaPath = mediaPath;
    msg.timestamp = QDateTime::currentDateTime();
    SaveActionedMessage(msg);
}

QVector<DeletedMessage> GetDeletedMessages(const QString &peerId) {
    Init();
    QVector<DeletedMessage> result;
    QSqlQuery query;
    query.prepare("SELECT msg_id, media_path, is_out, msg_date, original_text FROM actioned_messages WHERE peer_id = ? AND type = 'deleted'");
    query.addBindValue(peerId);
    if (query.exec()) {
        while (query.next()) {
            DeletedMessage dm;
            dm.msgId = query.value(0).toLongLong();
            dm.mediaPath = query.value(1).toString();
            dm.isOut = query.value(2).toBool();
            dm.date = query.value(3).toUInt();
            dm.text = query.value(4).toString();
            result.push_back(dm);
        }
    }
    return result;
}

QString GetMessageHistory(long long msgId, const QString &peerId) {
    Init();
    QSqlQuery query;
    query.prepare("SELECT original_text FROM actioned_messages WHERE msg_id = ? AND peer_id = ? AND type = 'edited' ORDER BY timestamp DESC LIMIT 1");
    query.addBindValue(msgId);
    query.addBindValue(peerId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "";
}

void SaveActionedMessage(const ActionedMessage &msg) {
    Init();
    QSqlQuery query;
    query.prepare("INSERT INTO actioned_messages (peer_id, msg_id, type, original_text, new_text, media_path, is_out, msg_date, timestamp) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(msg.peerId);
    query.addBindValue(msg.msgId);
    query.addBindValue(msg.type);
    query.addBindValue(msg.originalText);
    query.addBindValue(msg.newText);
    query.addBindValue(msg.mediaPath);
    query.addBindValue(0); // is_out placeholder
    query.addBindValue(0); // msg_date placeholder
    query.addBindValue(msg.timestamp);
    query.exec();
}

void SaveMessage(HistoryItem *item) {
    if (!item) return;
    Init();
    QSqlQuery query;
    query.prepare("INSERT INTO actioned_messages (peer_id, msg_id, type, original_text, is_out, msg_date, timestamp) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(QString::number(item->history()->peer->id.value));
    query.addBindValue((long long)item->id.bare);
    query.addBindValue("backup");
    query.addBindValue(item->originalText().text);
    query.addBindValue(item->out() ? 1 : 0);
    query.addBindValue((unsigned int)item->date());
    query.addBindValue(QDateTime::currentDateTime());
    query.exec();
}

void ExportDatabase(const QString &targetPath) {
    Init();
    QString currentDb = gDb.databaseName();
    gDb.close();
    QFile::copy(currentDb, targetPath);
    gDb.open();
}

void ImportDatabase(const QString &sourcePath) {
    Init();
    QString currentDb = gDb.databaseName();
    gDb.close();
    QFile::remove(currentDb);
    QFile::copy(sourcePath, currentDb);
    gDb.open();
}

QString SaveMediaFile(const QString &sourcePath, const QString &type) {
    if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) return "";
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/customizationMainFolder";
    QString subDir = "files";
    if (type == "image") subDir = "medias/images";
    else if (type == "video") subDir = "medias/videos";
    else if (type == "voice") subDir = "mediaMessages";
    QString fullPath = baseDir + "/" + subDir;
    QDir().mkpath(fullPath);
    QString fileName = QFileInfo(sourcePath).fileName();
    QString targetPath = fullPath + "/" + fileName;
    if (QFile::exists(targetPath)) return targetPath;
    if (QFile::copy(sourcePath, targetPath)) return targetPath;
    return "";
}

} // namespace CustomDB
