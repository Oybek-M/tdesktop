#include "custom_db.h"
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtConcurrent/QtConcurrent>
#include "history/history_item.h"
#include "history/history.h"
#include "data/data_peer.h"

namespace CustomDB {

namespace {
	QString gDataLocation;
	bool gInitialized = false;
	base::flat_map<QString, qint64> gGhostReadsCache;
	QMutex gCacheMutex;
}

void Init() {
    if (gInitialized) return;

    gDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(gDataLocation);

    const QString initConn = "tdesktop_custom_init";
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", initConn);
        db.setDatabaseName(gDataLocation + "/tdesktop_custom.sqlite");
        if (db.open()) {
            QSqlQuery query(db);
            // Enable WAL mode for better concurrency
            query.exec("PRAGMA journal_mode = WAL");
            query.exec("PRAGMA synchronous = NORMAL");

            query.exec(
                "CREATE TABLE IF NOT EXISTS messages ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "msg_id BIGINT, "
                "peer_id TEXT, "
                "peer_name TEXT, "
                "date INTEGER, "
                "text TEXT, "
                "is_out INTEGER, "
                "is_deleted INTEGER DEFAULT 0, "
                "version INTEGER DEFAULT 1, "
                "local_media_path TEXT"
                ")"
            );

            // Safer way to add columns if they don't exist
            QSqlQuery checkColumn(db);
            checkColumn.exec("PRAGMA table_info(messages)");
            bool hasMediaPath = false;
            while (checkColumn.next()) {
                if (checkColumn.value(1).toString() == "local_media_path") {
                    hasMediaPath = true;
                    break;
                }
            }
            if (!hasMediaPath) {
                query.exec("ALTER TABLE messages ADD COLUMN local_media_path TEXT");
            }

            query.exec(
                "CREATE TABLE IF NOT EXISTS ghost_reads ("
                "peer_id TEXT PRIMARY KEY, "
                "read_till_id BIGINT"
                ")"
            );

            QSqlQuery fetch(db);
            if (fetch.exec("SELECT peer_id, read_till_id FROM ghost_reads")) {
                QMutexLocker locker(&gCacheMutex);
                while (fetch.next()) {
                    gGhostReadsCache[fetch.value(0).toString()] = fetch.value(1).toLongLong();
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(initConn);
    gInitialized = true;
    qDebug() << "Custom SQLite DB initialized safely with WAL mode!";
}

void SaveMessage(not_null<HistoryItem*> item) {
    if (!gInitialized) return;

    const auto text = item->originalText().text;
    if (text.isEmpty()) return;

    struct MsgData {
        qint64 msgId;
        QString peerId;
        QString peerName;
        int date;
        QString text;
        int isOut;
    };

    MsgData data{
        (qint64)item->id.bare,
        QString::number(item->history()->peer->id.value),
        item->history()->peer->name(),
        (int)item->date(),
        text,
        item->out() ? 1 : 0
    };

    QtConcurrent::run([data = std::move(data)]() {
        const QString connectionName = "tdesktop_custom_save_" + QString::number((quintptr)QThread::currentThreadId());
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
            db.setDatabaseName(gDataLocation + "/tdesktop_custom.sqlite");

            if (db.open()) {
                int version = 1;
                QSqlQuery check(db);
                check.prepare("SELECT text, version FROM messages WHERE msg_id = :msg_id AND peer_id = :peer_id ORDER BY version DESC LIMIT 1");
                check.bindValue(":msg_id", data.msgId);
                check.bindValue(":peer_id", data.peerId);
                if (check.exec() && check.next()) {
                    if (check.value(0).toString() == data.text) {
                        db.close();
                        QSqlDatabase::removeDatabase(connectionName);
                        return;
                    }
                    version = check.value(1).toInt() + 1;
                }

                QSqlQuery query(db);
                query.prepare(
                    "INSERT INTO messages (msg_id, peer_id, peer_name, date, text, is_out, version) "
                    "VALUES (:msg_id, :peer_id, :peer_name, :date, :text, :is_out, :version)"
                );
                query.bindValue(":msg_id", data.msgId);
                query.bindValue(":peer_id", data.peerId);
                query.bindValue(":peer_name", data.peerName);
                query.bindValue(":date", data.date);
                query.bindValue(":text", data.text);
                query.bindValue(":is_out", data.isOut);
                query.bindValue(":version", version);
                query.exec();
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connectionName);
    });
}

void MarkDeleted(qint64 msgId, const QString &peerId, const QString &localMediaPath) {
    if (!gInitialized) return;

    QtConcurrent::run([msgId, peerId, localMediaPath]() {
        const QString connectionName = "tdesktop_custom_del_" + QString::number((quintptr)QThread::currentThreadId());
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
            db.setDatabaseName(gDataLocation + "/tdesktop_custom.sqlite");

            if (db.open()) {
                QSqlQuery query(db);
                query.prepare(
                    "UPDATE messages SET is_deleted = 1, local_media_path = :path WHERE msg_id = :msg_id AND peer_id = :peer_id"
                );
                query.bindValue(":msg_id", msgId);
                query.bindValue(":peer_id", peerId);
                query.bindValue(":path", localMediaPath);
                query.exec();
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connectionName);
    });
}

QString GetMessageHistory(qint64 msgId, const QString &peerId) {
    if (!gInitialized) return QString();
    QString result;
    const QString connectionName = "tdesktop_custom_get_" + QString::number((quintptr)QThread::currentThreadId());
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(gDataLocation + "/tdesktop_custom.sqlite");
        if (db.open()) {
            QSqlQuery query(db);
            query.prepare("SELECT text, is_deleted FROM messages WHERE msg_id = :msg_id AND peer_id = :peer_id ORDER BY version ASC");
            query.bindValue(":msg_id", msgId);
            query.bindValue(":peer_id", peerId);

            QStringList versions;
            bool isDeleted = false;
            if (query.exec()) {
                while (query.next()) {
                    versions << query.value(0).toString();
                    if (query.value(1).toInt() == 1) isDeleted = true;
                }
            }
            db.close();

            if (!versions.isEmpty()) {
                if (isDeleted) {
                    result += QString::fromUtf8("\xe2\x80\x94\xe2\x80\x94 DELETED \xe2\x80\x94\xe2\x80\x94\n\n");
                }

                if (versions.size() > 1) {
                    result += versions.last();
                    result += QString::fromUtf8("\n\n\xe2\x80\x94\xe2\x80\x94 ORIGINAL \xe2\x80\x94\xe2\x80\x94\n") + versions.first();
                } else if (isDeleted) {
                    result += versions.last();
                }
                if (!result.isEmpty()) {
                    qDebug() << "[CustomDB] Loaded history for msgId:" << msgId << "versions:" << versions.size() << "deleted:" << isDeleted;
                }
            }
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return result;
}
std::vector<DeletedMessage> GetDeletedMessages(const QString &peerId) {
    if (!gInitialized) return {};
    std::vector<DeletedMessage> results;
    const QString connectionName = "tdesktop_custom_list_" + QString::number((quintptr)QThread::currentThreadId());
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(gDataLocation + "/tdesktop_custom.sqlite");
        if (db.open()) {
            QSqlQuery query(db);
            // Fetch only deleted messages, ordered by date
            query.prepare(
                "SELECT msg_id, peer_name, date, text, is_out, local_media_path "
                "FROM messages WHERE peer_id = :peer_id AND is_deleted = 1 "
                "GROUP BY msg_id ORDER BY date ASC"
            );
            query.bindValue(":peer_id", peerId);
            
            if (query.exec()) {
                while (query.next()) {
                    results.push_back({
                        query.value(0).toLongLong(),
                        query.value(1).toString(),
                        query.value(2).toInt(),
                        query.value(3).toString(),
                        query.value(4).toInt(),
                        query.value(5).toString()
                    });
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return results;
}

void SaveGhostRead(const QString &peerId, qint64 msgId) {
    if (!gInitialized) return;
    {
        QMutexLocker locker(&gCacheMutex);
        gGhostReadsCache[peerId] = msgId;
    }
    QtConcurrent::run([peerId, msgId]() {
        const QString connectionName = "tdesktop_custom_ghost_" + QString::number((quintptr)QThread::currentThreadId());
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
            db.setDatabaseName(gDataLocation + "/tdesktop_custom.sqlite");
            if (db.open()) {
                QSqlQuery query(db);
                query.prepare("INSERT OR REPLACE INTO ghost_reads (peer_id, read_till_id) VALUES (:peer_id, :read_till_id)");
                query.bindValue(":peer_id", peerId);
                query.bindValue(":read_till_id", msgId);
                query.exec();
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connectionName);
    });
}
qint64 GetGhostRead(const QString &peerId) {
    if (!gInitialized) return 0;
	QMutexLocker locker(&gCacheMutex);
	const auto i = gGhostReadsCache.find(peerId);
	return (i != gGhostReadsCache.end()) ? i->second : 0;
}

} // namespace CustomDB
