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
			query.exec("ALTER TABLE messages ADD COLUMN local_media_path TEXT");
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
    qDebug() << "Custom SQLite DB initialized safely!";
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
		const QString connectionName = "tdesktop_custom_" + QString::number((quintptr)QThread::currentThreadId());
		{
			QSqlDatabase db;
			if (QSqlDatabase::contains(connectionName)) {
				db = QSqlDatabase::database(connectionName);
			} else {
				db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
				db.setDatabaseName(gDataLocation + "/tdesktop_custom.sqlite");
			}

			if (db.open()) {
				QSqlQuery query(db);
                
                int version = 1;
                QSqlQuery check(db);
                check.prepare("SELECT MAX(version) FROM messages WHERE msg_id = :msg_id AND peer_id = :peer_id");
                check.bindValue(":msg_id", data.msgId);
                check.bindValue(":peer_id", data.peerId);
                if (check.exec() && check.next()) {
                    version = check.value(0).toInt() + 1;
                }

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
	});
}

void MarkDeleted(qint64 msgId, const QString &peerId, const QString &localMediaPath) {
    if (!gInitialized) return;

	QtConcurrent::run([msgId, peerId, localMediaPath]() {
		const QString connectionName = "tdesktop_custom_del_" + QString::number((quintptr)QThread::currentThreadId());
		{
			QSqlDatabase db;
			if (QSqlDatabase::contains(connectionName)) {
				db = QSqlDatabase::database(connectionName);
			} else {
				db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
				db.setDatabaseName(gDataLocation + "/tdesktop_custom.sqlite");
			}

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
	});
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
			QSqlDatabase db;
			if (QSqlDatabase::contains(connectionName)) {
				db = QSqlDatabase::database(connectionName);
			} else {
				db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
				db.setDatabaseName(gDataLocation + "/tdesktop_custom.sqlite");
			}
			if (db.open()) {
				QSqlQuery query(db);
				query.prepare("INSERT OR REPLACE INTO ghost_reads (peer_id, read_till_id) VALUES (:peer_id, :read_till_id)");
				query.bindValue(":peer_id", peerId);
				query.bindValue(":read_till_id", msgId);
				query.exec();
				db.close();
			}
		}
	});
}

qint64 GetGhostRead(const QString &peerId) {
    if (!gInitialized) return 0;
	QMutexLocker locker(&gCacheMutex);
	const auto i = gGhostReadsCache.find(peerId);
	return (i != gGhostReadsCache.end()) ? i->second : 0;
}

} // namespace CustomDB
