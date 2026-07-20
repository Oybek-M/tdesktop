#include "custom_activity_history.h"

#include "custom_db.h"
#include "custom_settings.h"
#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_lastseen_status.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include <QtCore/QDateTime>

namespace CustomActivityHistory {
namespace {

void RecordField(
		const QString &peerId,
		const QString &field,
		const QString &newValue,
		qint64 observedAt) {
	QString oldValue;
	const auto hadPrevious = CustomDB::GetLatestActivityHistoryValue(
		peerId, field, oldValue);
	if (hadPrevious && oldValue == newValue) {
		return; // haqiqiy o'zgarish yo'q — qayta yozmaymiz
	}
	CustomDB::SaveActivityHistoryEntry(
		peerId, field, hadPrevious, oldValue, newValue, observedAt);
}

} // namespace

QString EncodeStatus(const Data::LastseenStatus &status, int32 now) {
	if (status.isRecently()) {
		return u"recently"_q;
	} else if (status.isWithinWeek()) {
		return u"within_week"_q;
	} else if (status.isWithinMonth()) {
		return u"within_month"_q;
	} else if (status.isLongAgo()) {
		return u"long_ago"_q;
	} else if (status.isOnline(now)) {
		return u"online:"_q + QString::number(status.onlineTill());
	}
	const auto till = status.onlineTill();
	if (till > 0) {
		return u"offline:"_q + QString::number(till);
	}
	return u"empty"_q;
}

QString DecodeStatusLabel(const QString &encoded) {
	if (encoded == u"recently"_q) {
		return u"yaqinda (aniq vaqt yashiringan)"_q;
	} else if (encoded == u"within_week"_q) {
		return u"shu hafta ichida (aniq vaqt yashiringan)"_q;
	} else if (encoded == u"within_month"_q) {
		return u"shu oy ichida (aniq vaqt yashiringan)"_q;
	} else if (encoded == u"long_ago"_q) {
		return u"uzoq vaqt oldin (aniq vaqt yashiringan)"_q;
	} else if (encoded == u"empty"_q || encoded.isEmpty()) {
		return u"noma'lum"_q;
	} else if (encoded.startsWith(u"online:"_q)) {
		const auto ts = encoded.mid(7).toLongLong();
		return u"hozir online (taxminan "_q
			+ QDateTime::fromSecsSinceEpoch(ts).toString(u"dd.MM.yyyy HH:mm"_q)
			+ u" gacha)"_q;
	} else if (encoded.startsWith(u"offline:"_q)) {
		const auto ts = encoded.mid(8).toLongLong();
		return u"oxirgi marta ko'rilgan: "_q
			+ QDateTime::fromSecsSinceEpoch(ts).toString(u"dd.MM.yyyy HH:mm"_q);
	}
	return encoded;
}

void Init(not_null<Main::Session*> session) {
	using Flag = Data::PeerUpdate::Flag;

	session->changes().peerUpdates(
		Flag::Name | Flag::Username | Flag::Photo | Flag::OnlineStatus
	) | rpl::on_next([=](const Data::PeerUpdate &update) {
		const auto user = update.peer->asUser();
		if (!user) {
			return; // faqat User (shaxsiy chat) kuzatiladi — spec §7
		}
		const auto peerId = QString::number(user->id.value);
		if (!CustomSettings::ShouldTrackActivity(peerId, user->isContact())) {
			return;
		}

		const auto now = base::unixtime::now();

		if (update.flags & Flag::Name) {
			RecordField(peerId, u"name"_q, user->name(), now);
		}
		if (update.flags & Flag::Username) {
			RecordField(peerId, u"username"_q, user->username(), now);
		}
		if (update.flags & Flag::Photo) {
			const auto value = user->hasUserpic()
				? QString::number(user->userpicPhotoId())
				: u"empty"_q;
			RecordField(peerId, u"photo"_q, value, now);
		}
		if (update.flags & Flag::OnlineStatus) {
			RecordField(
				peerId,
				u"status"_q,
				EncodeStatus(user->lastseen(), now),
				now);
		}
	}, session->lifetime());
}

} // namespace CustomActivityHistory
