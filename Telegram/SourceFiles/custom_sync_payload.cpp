#include "custom_sync_payload.h"
#include "custom_sync_record.h"
#include "custom_db.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <sqlite3.h>

namespace CustomSync {
namespace {

void bindText(sqlite3_stmt *stmt, int index, const QString &str) {
    if (str.isEmpty()) {
        sqlite3_bind_text(stmt, index, "", 0, SQLITE_STATIC);
    } else {
        const auto utf8 = str.toUtf8();
        sqlite3_bind_text(stmt, index, utf8.constData(), utf8.size(), SQLITE_TRANSIENT);
    }
}

QString colText(sqlite3_stmt *stmt, int col) {
    const auto *text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? QString::fromUtf8(text) : QString();
}

BuildResult BuildDeleted(sqlite3 *db, const OutboxEntry &entry) {
    sqlite3_stmt *stmt = nullptr;
    const char *sql =
        "SELECT original_text, sender_id, is_out, is_media "
        "FROM actioned_messages "
        "WHERE account_id = ? AND peer_id = ? AND msg_id = ? AND type = 'deleted' "
        "ORDER BY id DESC LIMIT 1";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return { BuildStatus::Unsupported, {} };
    }

    sqlite3_bind_int64(stmt, 1, entry.accountId);
    bindText(stmt, 2, entry.peerId);
    sqlite3_bind_int64(stmt, 3, entry.msgId);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return { BuildStatus::SourceGone, {} };
    }

    const QString text = colText(stmt, 0);
    const QString senderId = colText(stmt, 1);
    const bool isOut = (sqlite3_column_int(stmt, 2) != 0);
    const bool isMedia = (sqlite3_column_int(stmt, 3) != 0);
    sqlite3_finalize(stmt);

    // spec §0.14: account_id va peer_id o'nlik satrlar sifatida
    QJsonObject obj{
        { QStringLiteral("account_id"), QString::number(entry.accountId) },
        { QStringLiteral("peer_id"),    entry.peerId },
        { QStringLiteral("text"),       text },
        { QStringLiteral("sender_id"),  senderId },
        { QStringLiteral("is_out"),     isOut },
        { QStringLiteral("is_media"),   isMedia },
    };
    return { BuildStatus::Ok, QJsonDocument(obj).toJson(QJsonDocument::Compact) };
}

BuildResult BuildEdited(sqlite3 *db, const OutboxEntry &entry) {
    sqlite3_stmt *stmt = nullptr;
    const char *sql =
        "SELECT original_text, new_text, is_out "
        "FROM actioned_messages "
        "WHERE account_id = ? AND peer_id = ? AND msg_id = ? AND type = 'edited' "
        "ORDER BY id DESC LIMIT 1";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return { BuildStatus::Unsupported, {} };
    }

    sqlite3_bind_int64(stmt, 1, entry.accountId);
    bindText(stmt, 2, entry.peerId);
    sqlite3_bind_int64(stmt, 3, entry.msgId);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return { BuildStatus::SourceGone, {} };
    }

    const QString oldText = colText(stmt, 0);
    const QString newText = colText(stmt, 1);
    const bool isOut = (sqlite3_column_int(stmt, 2) != 0);
    sqlite3_finalize(stmt);

    // spec §0.14: account_id va peer_id o'nlik satrlar sifatida
    QJsonObject obj{
        { QStringLiteral("account_id"), QString::number(entry.accountId) },
        { QStringLiteral("peer_id"),    entry.peerId },
        { QStringLiteral("old_text"),   oldText },
        { QStringLiteral("new_text"),   newText },
        { QStringLiteral("is_out"),     isOut },
    };
    return { BuildStatus::Ok, QJsonDocument(obj).toJson(QJsonDocument::Compact) };
}

BuildResult BuildActivity(sqlite3 *db, const OutboxEntry &entry) {
    sqlite3_stmt *stmt = nullptr;
    // DIQQAT: account_id bo'yicha filtr QO'YILMAYDI -- activity_history
    // spec §0.13 ga asosan barcha akkauntlar bo'ylab birlashadi.
    const char *sql =
        "SELECT field, old_value, new_value "
        "FROM activity_history "
        "WHERE peer_id = ? AND observed_at = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return { BuildStatus::Unsupported, {} };
    }

    bindText(stmt, 1, entry.peerId);
    sqlite3_bind_int64(stmt, 2, entry.occurredAt);

    bool found = false;
    QString matchedField;
    bool hasOldValue = false;
    QString oldValue;
    QString newValue;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const QString field = colText(stmt, 0);
        if (CustomSync::DiscriminatorFor(field) == entry.msgId) {
            matchedField = field;
            // has_old_value faqat SQL NULL ga qarab aniqlanadi (bo'sh satr ham qiymat)
            hasOldValue = (sqlite3_column_type(stmt, 1) != SQLITE_NULL);
            oldValue = hasOldValue ? colText(stmt, 1) : QString();
            newValue = (sqlite3_column_type(stmt, 2) != SQLITE_NULL) ? colText(stmt, 2) : QString();
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);

    if (!found) {
        return { BuildStatus::SourceGone, {} };
    }

    // spec §0.14: activity uchun account_hash bo'sh satr bo'lsa ham,
    // payload'da account_id va peer_id o'nlik satrlar sifatida mavjud bo'ladi.
    QJsonObject obj{
        { QStringLiteral("account_id"),    QString::number(entry.accountId) },
        { QStringLiteral("peer_id"),       entry.peerId },
        { QStringLiteral("field"),         matchedField },
        { QStringLiteral("has_old_value"), hasOldValue },
        { QStringLiteral("old_value"),     hasOldValue ? QJsonValue(oldValue) : QJsonValue(QJsonValue::Null) },
        { QStringLiteral("new_value"),     newValue },
    };
    return { BuildStatus::Ok, QJsonDocument(obj).toJson(QJsonDocument::Compact) };
}

BuildResult BuildGhostRead(const OutboxEntry &entry) {
    // spec §3.2: ghost_read faqat metadata olib yuradi, lokal bazadan o'qilmaydi.
    QJsonObject obj{
        { QStringLiteral("account_id"), QString::number(entry.accountId) },
        { QStringLiteral("peer_id"),    entry.peerId },
    };
    return { BuildStatus::Ok, QJsonDocument(obj).toJson(QJsonDocument::Compact) };
}

BuildResult BuildMediaIndex(sqlite3 *db, const OutboxEntry &entry) {
    sqlite3_stmt *stmt = nullptr;
    const char *sql =
        "SELECT kind, file_name, rel_path, size, sha256, status, reason, layer, msg_date "
        "FROM media_index "
        "WHERE account_id = ? AND peer_id = ? AND msg_id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return { BuildStatus::Unsupported, {} };
    }

    sqlite3_bind_int64(stmt, 1, entry.accountId);
    bindText(stmt, 2, entry.peerId);
    sqlite3_bind_int64(stmt, 3, entry.msgId);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return { BuildStatus::SourceGone, {} };
    }

    const QString kind = colText(stmt, 0);
    const QString fileName = colText(stmt, 1);
    const QString relPath = colText(stmt, 2);
    const qint64 size = sqlite3_column_int64(stmt, 3);
    const QString sha256 = colText(stmt, 4);
    const QString status = colText(stmt, 5);
    const QString reason = colText(stmt, 6);
    const QString layer = colText(stmt, 7);
    const qint64 msgDate = sqlite3_column_int64(stmt, 8);
    sqlite3_finalize(stmt);

    // sha256 bo'sh bo'lsa ham kalit kiritiladi (K4 barqaror JSON kalitlar to'plami)
    QJsonObject obj{
        { QStringLiteral("account_id"), QString::number(entry.accountId) },
        { QStringLiteral("peer_id"),    entry.peerId },
        { QStringLiteral("kind"),       kind },
        { QStringLiteral("file_name"),  fileName },
        { QStringLiteral("rel_path"),   relPath },
        { QStringLiteral("size"),       size },
        { QStringLiteral("sha256"),     sha256 },
        { QStringLiteral("status"),     status },
        { QStringLiteral("reason"),     reason },
        { QStringLiteral("layer"),      layer },
        { QStringLiteral("msg_date"),   msgDate },
    };
    return { BuildStatus::Ok, QJsonDocument(obj).toJson(QJsonDocument::Compact) };
}

BuildResult BuildTombstone(const OutboxEntry &entry) {
    // spec §0.3 / §3.2: tombstone faqat target_record_id olib yuradi.
    QJsonObject obj{
        { QStringLiteral("account_id"),       QString::number(entry.accountId) },
        { QStringLiteral("peer_id"),          entry.peerId },
        { QStringLiteral("target_record_id"), entry.targetRecordId },
    };
    return { BuildStatus::Ok, QJsonDocument(obj).toJson(QJsonDocument::Compact) };
}

} // namespace

BuildResult Build(const OutboxEntry &entry) {
    if (entry.kind == QLatin1String(Kind::GhostRead)) {
        return BuildGhostRead(entry);
    }
    if (entry.kind == QLatin1String(Kind::Tombstone)) {
        return BuildTombstone(entry);
    }

    auto *db = CustomDB::RawHandle();
    if (!db) {
        return { BuildStatus::Unsupported, {} };
    }

    if (entry.kind == QLatin1String(Kind::Deleted)) {
        return BuildDeleted(db, entry);
    }
    if (entry.kind == QLatin1String(Kind::Edited)) {
        return BuildEdited(db, entry);
    }
    if (entry.kind == QLatin1String(Kind::Activity)) {
        return BuildActivity(db, entry);
    }
    if (entry.kind == QLatin1String(Kind::MediaIndex)) {
        return BuildMediaIndex(db, entry);
    }

    return { BuildStatus::Unsupported, {} };
}

} // namespace CustomSync
