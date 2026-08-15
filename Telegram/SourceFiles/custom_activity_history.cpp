#include "custom_activity_history.h"

#include "custom_archive.h"     // MediaExtensionFor
#include "custom_db.h"
#include "custom_media_quota.h" // kvota tekshiruvi
#include "custom_settings.h"
#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_lastseen_status.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/data_stories.h"
#include "data/data_story.h"
#include "main/main_session.h"
#include <QtCore/QDateTime>
#include <range/v3/algorithm/max_element.hpp>
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "ui/image/image.h"
#include "storage/file_download.h" // Storage::kMaxFileInMemory
#include "base/flat_set.h"
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

namespace CustomActivityHistory {
namespace {

struct PendingStoryMedia {
	PhotoData *photo = nullptr;
	DocumentData *document = nullptr;
	std::shared_ptr<Data::PhotoMedia> photoMedia; // photo bo'lsa, media view'ni tirik saqlaydi
	// 2026-08-15: hujjat to'g'ridan-to'g'ri arxivga yuklanadi, shuning
	// uchun tugagach media_index ni yangilash uchun kerak.
	QString relPath;
	QString peerId;
	long long storyId = 0;
};

std::vector<PendingStoryMedia> gPendingStoryMedia;
base::flat_set<FullStoryId> gProcessedStoryMedia;

QString SaveStoryImage(const QImage &image, PhotoId id) {
	// 2026-08-15: arxiv ildizi endi sozlanadi — yagona manba
	// CustomSettings::ArchiveMediasDir().
	const auto baseDir = CustomSettings::ArchiveMediasDir() + u"/images"_q;
	QDir().mkpath(baseDir);
	const auto path = baseDir + u"/story_"_q + QString::number(id) + u".jpg"_q;
	image.save(path, "JPEG");
	return path;
}

void CheckPendingStoryMedia() {
	for (auto it = gPendingStoryMedia.begin(); it != gPendingStoryMedia.end();) {
		auto done = false;
		if (it->document) {
			const auto path = it->document->filepath(true);
			if (!path.isEmpty()) {
				// Fayl allaqachon arxiv ichiga yuklandi (yuqoriga qarang),
				// shuning uchun nusxalash SHART EMAS — faqat indeksga
				// yozamiz. Story'lar uchun finishLoad() hook'i ishlamaydi:
				// u FileOriginMessage kutadi, story esa FileOriginStory.
				//
				// msgId MANFIY: story ID'lari xabar ID'lari bilan bir xil
				// fazoda emas, manfiy qiymat ularni ajratib turadi (skaner
				// ham noma'lum fayllar uchun shu usulni ishlatadi).
				if (!it->relPath.isEmpty()) {
					auto entry = CustomDB::MediaIndexEntry();
					entry.peerId = it->peerId;
					entry.msgId = -it->storyId;
					entry.kind = u"story"_q;
					entry.relPath = it->relPath;
					entry.fileName = it->relPath.mid(
						it->relPath.lastIndexOf(u'/') + 1);
					entry.size = it->document->size;
					entry.archivedAt = static_cast<unsigned int>(
						QDateTime::currentSecsSinceEpoch());
					entry.layer = u"story"_q;
					entry.status = u"present"_q;
					CustomDB::UpsertMediaIndex(entry);
					CustomMediaQuota::AddBytes(it->document->size);
				} else {
					CustomDB::SaveMediaFile(path, u"video"_q); // eski yo'l
				}
				done = true;
			}
		} else if (it->photo && it->photoMedia) {
			if (it->photoMedia->loaded()) {
				if (const auto image = it->photoMedia->image(
						Data::PhotoSize::Large)) {
					const auto qimg = image->original();
					if (!qimg.isNull()) {
						SaveStoryImage(qimg, it->photo->id);
					}
				}
				done = true;
			}
		} else {
			done = true; // noto'g'ri holat, ro'yxatdan chiqarish
		}
		it = done ? gPendingStoryMedia.erase(it) : std::next(it);
	}
}

void MaybeBackupStoryMedia(
		not_null<Data::Story*> story,
		Data::FileOriginStory origin) {
	if (!CustomSettings::StoryMediaBackupEnabled()) {
		return;
	}
	if (const auto photo = story->photo()) {
		auto media = photo->createMediaView();
		photo->load(Data::PhotoSize::Large, origin);
		gPendingStoryMedia.push_back({ photo, nullptr, std::move(media) });
	} else if (const auto document = story->document()) {
		// Raw pointer, no keepalive (unlike photo's PhotoMedia shared_ptr
		// above): Data::Session owns DocumentData objects for the whole
		// session lifetime once created (matches how DocumentData* is held
		// elsewhere in this codebase, e.g. data_document.cpp's own
		// finishLoad() hook) — safe to assume it outlives this pending
		// download.
		// 2026-08-15: L2 mexanizmiga o'tkazildi.
		//
		// Ilgari bu yerda `save(origin, QString())` — bo'sh nom — ishlatilardi
		// va u "faylni xotiraga yukla" degani. FileLoader uni 10 MB bilan
		// cheklaydi (file_download.cpp:114 Expects), shuning uchun 2026-08-14
		// da 10 MB chegarasi qo'yilgan edi. Natijada katta story videolari
		// UMUMAN saqlanmasdi.
		//
		// Endi xuddi CustomArchive'ning L2 qatlami kabi HAQIQIY maqsad yo'li
		// beriladi, ya'ni chegara foydalanuvchi sozlamasidan olinadi
		// (default 100 MB) va kvota hisobga olinadi.
		const auto maxBytes = qint64(
			CustomSettings::MediaBackupMaxFileMb()) * 1024 * 1024;
		if (document->size > maxBytes || CustomMediaQuota::IsFull()) {
			return;
		}
		// Kengaytma CustomArchive'dagi yagona mantiqdan (haqiqiy nom ->
		// MIME -> tur), shuning uchun kengaytmasiz fayl chiqmaydi.
		const auto relPath = u"medias/stories/"_q
			+ QString::number(story->peer()->id.value)
			+ u"_"_q
			+ QString::number(story->id())
			+ u"."_q
			+ CustomArchive::MediaExtensionFor(document);
		const auto fullPath = CustomSettings::ArchiveRoot() + u"/"_q + relPath;
		QDir().mkpath(QFileInfo(fullPath).absolutePath());
		document->save(origin, fullPath);
		gPendingStoryMedia.push_back({
			nullptr,
			document,
			nullptr,
			relPath,
			QString::number(story->peer()->id.value),
			static_cast<long long>(story->id()) });
	}
}

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
	}
	if (encoded.startsWith(u"online:"_q)) {
		bool ok = false;
		const auto ts = encoded.mid(7).toLongLong(&ok);
		if (ok && ts > 0) {
			return u"hozir online (taxminan "_q
				+ QDateTime::fromSecsSinceEpoch(ts).toString(u"dd.MM.yyyy HH:mm"_q)
				+ u" gacha)"_q;
		}
	} else if (encoded.startsWith(u"offline:"_q)) {
		bool ok = false;
		const auto ts = encoded.mid(8).toLongLong(&ok);
		if (ok && ts > 0) {
			return u"oxirgi marta ko'rilgan: "_q
				+ QDateTime::fromSecsSinceEpoch(ts).toString(u"dd.MM.yyyy HH:mm"_q);
		}
	}
	return encoded;
}

QString DecodeStoryLabel(const QString &encoded) {
	bool ok = false;
	const auto ts = encoded.toLongLong(&ok);
	if (!ok || ts <= 0) {
		return u"noma'lum"_q;
	}
	return u"📖 Hikoya qo'ydi: "_q
		+ QDateTime::fromSecsSinceEpoch(ts).toString(u"dd.MM.yyyy HH:mm"_q);
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

	// ── Story post-vaqti signali (A11 §1) ────────────────────────────────
	// Story HECH QACHON ochilmaydi — faqat mavjud sinxronizatsiya orqali
	// kelgan metadata (sana) o'qiladi. Stories::markAsRead() bu yerda
	// HECH QACHON chaqirilmaydi (xavfsizlik invarianti).
	session->data().stories().itemsChanged(
	) | rpl::on_next([=](PeerId peerId) {
		const auto user = session->data().peer(peerId)->asUser();
		if (!user) {
			return; // faqat User (shaxsiy chat) kuzatiladi
		}
		const auto peerId2 = QString::number(user->id.value);
		if (!CustomSettings::ShouldTrackActivity(peerId2, user->isContact())) {
			return;
		}

		auto &stories = session->data().stories();
		const auto source = stories.source(peerId);
		if (!source || source->ids.empty()) {
			return; // hozircha aktiv story yo'q
		}
		const auto latest = *ranges::max_element(
			source->ids,
			ranges::less{},
			&Data::StoryIdDates::date);
		const auto found = stories.lookup({ peerId, latest.id });
		if (!found) {
			return;
		}
		const auto story = *found;
		const auto now = base::unixtime::now();

		RecordField(
			peerId2,
			u"story"_q,
			QString::number(story->date()),
			now);

		const auto fullId = FullStoryId{ peerId, latest.id };
		if (!gProcessedStoryMedia.contains(fullId)) {
			gProcessedStoryMedia.emplace(fullId);
			MaybeBackupStoryMedia(story, fullId);
		}
	}, session->lifetime());

	session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		CheckPendingStoryMedia();
	}, session->lifetime());
}

} // namespace CustomActivityHistory
