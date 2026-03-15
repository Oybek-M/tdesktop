#include "custom_db.h"
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtConcurrent/QtConcurrent>
#include "history/history_item.h"
#include "history/history.h"
#include "data/data_peer.h"

namespace CustomDB {

namespace {
	QString gDataLocation;
	bool gInitialized = false;
}

void Init() {
    if (gInitialized) return;

    gDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(gDataLocation);
	
	// PostgreSQL ulanish sozlamalari
	auto db = QSqlDatabase::addDatabase("QPSQL", "tdesktop_custom_init");
	db.setHostName("localhost");
	db.setDatabaseName("telegram_db");
	db.setUserName("saidjon");
	db.setPassword("1234");
    db.setPort(5432);

	if (db.open()) {
		QSqlQuery query(db);
		query.exec(
			"CREATE TABLE IF NOT EXISTS messages ("
			"id SERIAL PRIMARY KEY, "
			"msg_id BIGINT, "
			"peer_id TEXT, "
			"peer_name TEXT, "
			"date INTEGER, "
			"text TEXT, "
			"is_out INTEGER, "
			"is_deleted INTEGER DEFAULT 0"
			")"
		);
		db.close();
	} else {
		qDebug() << "PostgreSQL ulanishda xato:" << db.lastError().text();
	}
	QSqlDatabase::removeDatabase("tdesktop_custom_init");
	gInitialized = true;
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
		item->date(),
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
				db = QSqlDatabase::addDatabase("QPSQL", connectionName);
				db.setHostName("localhost");
				db.setDatabaseName("telegram_db");
				db.setUserName("saidjon");
				db.setPassword("1234");
			}

			if (db.open()) {
				QSqlQuery query(db);
				query.prepare(
					"INSERT INTO messages (msg_id, peer_id, peer_name, date, text, is_out) "
					"VALUES (:msg_id, :peer_id, :peer_name, :date, :text, :is_out)"
				);
				query.bindValue(":msg_id", data.msgId);
				query.bindValue(":peer_id", data.peerId);
				query.bindValue(":peer_name", data.peerName);
				query.bindValue(":date", data.date);
				query.bindValue(":text", data.text);
				query.bindValue(":is_out", data.isOut);
				query.exec();
				db.close();
			}
		}
	});
}

void MarkDeleted(qint64 msgId, const QString &peerId) {
    if (!gInitialized) return;

	QtConcurrent::run([msgId, peerId]() {
		const QString connectionName = "tdesktop_custom_del_" + QString::number((quintptr)QThread::currentThreadId());
		{
			QSqlDatabase db;
			if (QSqlDatabase::contains(connectionName)) {
				db = QSqlDatabase::database(connectionName);
			} else {
				db = QSqlDatabase::addDatabase("QPSQL", connectionName);
				db.setHostName("localhost");
				db.setDatabaseName("telegram_db");
				db.setUserName("saidjon");
				db.setPassword("1234");
			}

			if (db.open()) {
				QSqlQuery query(db);
				query.prepare(
					"UPDATE messages SET is_deleted = 1 WHERE msg_id = :msg_id AND peer_id = :peer_id"
				);
				query.bindValue(":msg_id", msgId);
				query.bindValue(":peer_id", peerId);
				query.exec();
				db.close();
			}
		}
	});
}

} // namespace CustomDB
