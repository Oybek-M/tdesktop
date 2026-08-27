#include "custom_archive.h"

#include "custom_db.h"
#include "custom_peer_key.h"
#include "custom_media_quota.h"
#include "custom_settings.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_histories.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h" // imageBytes() — rasmni arxivga yozish
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "storage/file_download.h" // Storage::kMaxFileInMemory
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QMimeDatabase>
#include <QtCore/QTimer>
#include <vector>

namespace CustomArchive {
namespace {

struct PendingRow {
	qint64 accountId = 0;
	QString peerId;
	QString text;
	QString senderId;
	long long msgId = 0;
	unsigned int msgDate = 0;
	bool isOut = false;
	bool isMedia = false;
};

// Partiya chuqurligi: BeginBatch/EndBatch ichma-ich chaqirilishi mumkin.
int gBatchDepth = 0;
std::vector<PendingRow> gPending;

// Partiya juda kattalashib ketmasin — oraliq yozuv chegarasi.
constexpr auto kFlushThreshold = 200;

// Davriy WAL checkpoint oralig'i (A13/D5).
constexpr auto kCheckpointIntervalMs = 5 * 60 * 1000;

void WriteRow(const PendingRow &row) {
	CustomDB::CacheMessageText(
		CustomDB::PeerKey{row.accountId, row.peerId},
		row.msgId,
		row.text,
		row.isOut,
		row.msgDate,
		row.senderId,
		row.isMedia,
		true); // archived = true → PruneStaleCachedText tegmaydi (D4)
}

[[nodiscard]] QString KindSubDir(const QString &kind) {
	if (kind == u"voice"_q) return u"medias/voices"_q;
	if (kind == u"video"_q) return u"medias/videos"_q;
	if (kind == u"image"_q) return u"medias/images"_q;
	return u"medias/files"_q;
}

// Arxiv ildizidan nisbiy yo'l: medias/<turi>/<peerId>_<msgId>_<nom>.
// peerId+msgId prefiksi bir xil nomli fayllar bir-birini yozib
// yubormasligi uchun (SaveMediaFile() da bu muammo bor edi — u faqat
// fayl nomini ishlatadi va mavjud bo'lsa nusxalashni o'tkazib yuboradi).
[[nodiscard]] QString ArchiveRelPath(
		not_null<HistoryItem*> item,
		not_null<DocumentData*> document,
		const QString &kind) {
	// 2026-08-15 (sinovda topildi): document->filename() MEDIA sifatida
	// yuborilgan fayllar uchun bo'sh bo'ladi — video, ovozli xabar,
	// animatsiya. Ilgari bu yerda oddiy "file" qo'yilardi va natijada
	// eksportda kengaytmasiz, ikki marta bosib ochib bo'lmaydigan
	// fayllar chiqardi (34 ta videodan ko'pchiligi, 14 ta ovozli
	// xabarning HAMMASI).
	auto name = document->filename();
	if (name.isEmpty()) {
		name = u"media"_q;
	}
	// MediaExtensionFor() avval HAQIQIY nomdagi kengaytmani qaytaradi,
	// shuning uchun nomda to'g'ri kengaytma bo'lsa quyidagi shart
	// bajarilmaydi va nom o'zgarishsiz qoladi. Mavjud kengaytma hech
	// qachon almashtirilmaydi — faqat yo'q bo'lsa qo'shiladi.
	const auto ext = MediaExtensionFor(document);
	if (!ext.isEmpty() && QFileInfo(name).suffix().toLower() != ext) {
		name += u"."_q + ext;
	}
	// SaveMediaFile() dagi bilan bir xil xavfsizlik qoidalari + Windows'da
	// taqiqlangan belgilar.
	for (const auto ch : { u'/', u'\\', u':', u'*', u'?', u'"', u'<', u'>', u'|' }) {
		name.remove(ch);
	}
	while (name.contains(u"..")) {
		name.remove(u".."_q);
	}
	if (name.isEmpty()) {
		name = u"file"_q;
	}
	if (name.length() > 150) {
		const auto ext = QFileInfo(name).suffix();
		name = name.left(140) + (ext.isEmpty() ? QString() : u"."_q + ext);
	}
	return KindSubDir(kind)
		+ u"/"_q
		+ QString::number(item->history()->peer->id.value)
		+ u"_"_q
		+ QString::number(item->id.bare)
		+ u"_"_q
		+ name;
}

void RecordIndex(
		not_null<HistoryItem*> item,
		not_null<DocumentData*> document,
		const QString &kind,
		const QString &status,
		const QString &reason,
		const QString &relPath,
		const QString &layer) {
	auto entry = CustomDB::MediaIndexEntry();
	entry.peerId = QString::number(item->history()->peer->id.value);
	entry.msgId = static_cast<long long>(item->id.bare);
	entry.kind = kind;
	entry.relPath = relPath;
	entry.fileName = relPath.isEmpty()
		? QString()
		: relPath.mid(relPath.lastIndexOf(u'/') + 1);
	entry.size = document->size;
	entry.msgDate = static_cast<unsigned int>(item->date());
	entry.archivedAt = static_cast<unsigned int>(
		QDateTime::currentSecsSinceEpoch());
	entry.layer = layer;
	entry.status = status;
	entry.reason = reason;
	// 2026-08-24: nomni SHU YERDA ham eslab qolamiz. MaybeArchiveItem()
	// dagi chaqiruv faqat ShouldBackgroundCache() rost bo'lgan chatlarni
	// qamraydi; media esa boshqa yo'llardan ham indekslanadi va o'shanda
	// eksport ro'yxatida "ID 7472003734" ko'rinardi.
	CustomSettings::RememberPeerName(
		entry.peerId,
		item->history()->peer->name());
	CustomDB::UpsertMediaIndex(CustomDB::Key(item), entry);
}

// A13/K4 + 2026-08-14 (L2): kuzatilayotgan chatda media avtomatik yuklab
// olinadi, shunda suhbatdosh butun chatni o'chirganda ham fayl qo'lda
// qoladi.
//
// Ikki xil yo'l bor:
//   * L2 yoqilgan chat  — fayl TO'G'RIDAN-TO'G'RI arxiv papkasiga
//     yuklanadi (haqiqiy maqsad yo'li), hajm chegarasi va kvota bilan.
//   * Boshqa chatlar    — eski xatti-harakat: bo'sh nom bilan keshga,
//     faqat 10 MB gacha (undan kattasi crash beradi, pastga qarang).
// 2026-08-24: rasm arxivi. Hujjatlardan farqi — DocumentData'da
// `filepath()` va `finishLoad()` hook'i bor, PhotoData'da esa yo'q.
// Shuning uchun yuklanishini o'zimiz kuzatamiz va baytlarni o'zimiz
// yozamiz. `imageBytes()` ASL JPEG baytlarini beradi, ya'ni QImage
// orqali qayta kodlash va sifat yo'qotish yo'q.
struct PendingPhoto {
	PhotoData *photo = nullptr;
	std::shared_ptr<Data::PhotoMedia> view;
	QString relPath;
	QString fullPath;
	QString peerId;
	long long msgId = 0;
	unsigned int msgDate = 0;
};
std::vector<PendingPhoto> gPendingPhotos;

void RecordPhotoIndex(
		not_null<HistoryItem*> item,
		not_null<PhotoData*> photo,
		const QString &status,
		const QString &reason,
		const QString &relPath,
		long long size) {
	auto entry = CustomDB::MediaIndexEntry();
	entry.peerId = QString::number(item->history()->peer->id.value);
	entry.msgId = static_cast<long long>(item->id.bare);
	entry.kind = u"image"_q;
	entry.relPath = relPath;
	entry.fileName = relPath.isEmpty()
		? QString()
		: relPath.mid(relPath.lastIndexOf(u'/') + 1);
	entry.size = size;
	entry.msgDate = static_cast<unsigned int>(item->date());
	entry.archivedAt = static_cast<unsigned int>(
		QDateTime::currentSecsSinceEpoch());
	entry.layer = u"l2"_q;
	entry.status = status;
	entry.reason = reason;
	// 2026-08-24: nomni SHU YERDA ham eslab qolamiz. MaybeArchiveItem()
	// dagi chaqiruv faqat ShouldBackgroundCache() rost bo'lgan chatlarni
	// qamraydi; media esa boshqa yo'llardan ham indekslanadi va o'shanda
	// eksport ro'yxatida "ID 7472003734" ko'rinardi.
	CustomSettings::RememberPeerName(
		entry.peerId,
		item->history()->peer->name());
	CustomDB::UpsertMediaIndex(CustomDB::Key(item), entry);
}

void MaybeDownloadMedia(not_null<HistoryItem*> item) {
	const auto media = item->media();
	if (!media) {
		return;
	}
	const auto peer = item->history()->peer;
	if (peer->isSelf()) {
		// Saved Messages: foydalanuvchidan boshqa hech kim o'chira
		// olmaydi, ya'ni AntiDelete'ning tahdid modeli ("suhbatdosh
		// o'chirib yubordi") bu yerda umuman qo'llanmaydi. Qattiq
		// istisno — sozlama emas.
		return;
	}
	const auto peerIdStr = QString::number(peer->id.value);
	const auto origin = Data::FileOriginMessage(item->fullId());
	if (const auto document = media->document()) {
		if (!document->filepath(true).isEmpty() || document->loading()) {
			return; // allaqachon diskda yoki yuklanmoqda
		}
		if (CustomSettings::ShouldMediaBackup(peerIdStr)) {
			const auto kind = MediaKindOf(document);
			const auto maxBytes = qint64(
				CustomSettings::MediaBackupMaxFileMb()) * 1024 * 1024;
			if (document->size > maxBytes) {
				// Yuklamaymiz, lekin IZ QOLDIRAMIZ — ilgari bunday
				// fayllar haqida hech qanday ma'lumot saqlanmasdi.
				RecordIndex(item, document, kind,
					u"pending"_q, u"too_large"_q, QString(), u"l2"_q);
				return;
			}
			if (CustomMediaQuota::IsFull()) {
				RecordIndex(item, document, kind,
					u"pending"_q, u"quota_full"_q, QString(), u"l2"_q);
				return;
			}
			const auto relPath = ArchiveRelPath(item, document, kind);
			const auto fullPath = CustomSettings::ArchiveRoot()
				+ u"/"_q + relPath;
			QDir().mkpath(QFileInfo(fullPath).absolutePath());

			// 🔴 BO'SH NOM EMAS. Bo'sh nom "faylni xotiraga yukla"
			// degani va FileLoader uni 10 MB bilan cheklaydi
			// (file_download.cpp:114 Expects) — 2026-08-14 dagi
			// crash aynan shundan edi. Bu yerni "soddalashtirib"
			// QString() ga qaytarmang.
			document->save(origin, fullPath);

			// save() yuklashni BOSHLAYDI, tugatmaydi. Shuning uchun
			// hozircha 'pending' — yuklash tugagach data_document.cpp
			// dagi finishLoad() hook'i NoteArchivedDownloadFinished()
			// orqali uni 'present' ga o'tkazadi. Darhol 'present' deb
			// yozish indeksni yolg'onchi qilardi (yuklash uzilishi
			// mumkin), bu esa Track C sinxronizatsiyasida tarqalardi.
			RecordIndex(item, document, kind,
				u"pending"_q, u"downloading"_q, relPath, u"l2"_q);
			return;
		}
		// L2 yoqilmagan chat — eski, keshga yuklash yo'li.
		// 10 MB chegarasi MAJBURIY (yuqoridagi izohga qarang).
		// KRITIK (2026-08-14 crash): save() ga BO'SH nom berish "faylni
		// xotiraga yukla" degani. FileLoader konstruktori buni
		// Storage::kMaxFileInMemory (10 MB) bilan cheklaydi:
		//
		//   Expects(!_filename.isEmpty() || (_fullSize <= kMaxFileInMemory));
		//   -- file_download.cpp:114
		//
		// Keshlanmaydigan hujjat uchun DocumentData::save() LoadToFileOnly
		// rejimini tanlaydi (data_document.cpp:1292), u esa haqiqiy maqsad
		// yo'lisiz ishlay olmaydi. Natijada 10 MB dan katta har qanday fayl
		// ilovani darhol yiqitardi — chatda eski xabarlarni yuklashda,
		// ayniqsa Saved Messages'da (u yerda katta video/fayllar ko'p).
		//
		// Bu chat uchun L2 yoqilmagan, ya'ni haqiqiy maqsad yo'li yo'q —
		// demak faqat xotiraga sig'adigan hajmdagilarni olamiz. Katta
		// fayl kerak bo'lsa foydalanuvchi chatni White List'ga qo'shadi
		// yoki per-chat "Media Backup" toggle'ini yoqadi.
		if (document->size > Storage::kMaxFileInMemory) {
			return;
		}
		document->save(origin, QString());
	} else if (const auto photo = media->photo()) {
		// 2026-08-24: RASMLAR ILGARI UMUMAN ARXIVLANMASDI.
		//
		// Bu yerda faqat `photo->load()` bor edi — u faylni Telegram'ning
		// O'Z keshiga yuklaydi, bizning arxivga EMAS, va media_index ga
		// hech narsa yozmasdi. Hujjatlar uchun butun L2 mantig'i bor edi,
		// rasmlar esa chetda qolgan.
		//
		// Natija: rasm ko'rinishidagi post o'chirilsa, "(media xabar)"
		// yozuvi qolar, faylning o'zi esa yo'qolardi. Diskdagi dalil:
		// bitta kanalda o'chirilgan 8 ta xabardan 7 tasi (hammasi rasm)
		// media_index da umuman yo'q edi.
		if (!CustomSettings::ShouldMediaBackup(peerIdStr)) {
			photo->load(Data::PhotoSize::Large, origin); // eski yo'l
			return;
		}
		if (CustomMediaQuota::IsFull()) {
			RecordPhotoIndex(item, photo, u"pending"_q, u"quota_full"_q,
				QString(), 0);
			return;
		}
		const auto relPath = u"medias/images/"_q + peerIdStr + u"_"_q
			+ QString::number(item->id.bare) + u".jpg"_q;
		const auto fullPath = CustomSettings::ArchiveRoot() + u"/"_q + relPath;
		if (QFile::exists(fullPath)) {
			return; // allaqachon saqlangan
		}
		QDir().mkpath(QFileInfo(fullPath).absolutePath());

		auto view = photo->createMediaView();
		photo->load(Data::PhotoSize::Large, origin);
		gPendingPhotos.push_back({
			photo,
			std::move(view),
			relPath,
			fullPath,
			peerIdStr,
			static_cast<long long>(item->id.bare),
			static_cast<unsigned int>(item->date()) });
		RecordPhotoIndex(item, photo, u"pending"_q, u"downloading"_q,
			relPath, 0);
		// Rasm ko'pincha allaqachon keshda bo'ladi — darhol tekshiramiz,
		// aks holda yangi yuklash boshlanmay `downloaderTaskFinished`
		// hech qachon otilmaydi.
		CheckPendingPhotos(&item->history()->session());
	}
}

} // namespace

// Yuklanishi tugagan rasmlarni arxivga yozadi. main_session.cpp dagi
// downloaderTaskFinished() dan chaqiriladi.
void CheckPendingPhotos(not_null<Main::Session*> session) {
	for (auto it = gPendingPhotos.begin(); it != gPendingPhotos.end();) {
		auto done = false;
		if (!it->photo || !it->view) {
			done = true;
		} else if (it->view->loaded()) {
			// ASL baytlar — QImage orqali qayta kodlash yo'q.
			auto bytes = it->view->imageBytes(Data::PhotoSize::Large);
			if (bytes.isEmpty()) {
				bytes = it->view->imageBytes(Data::PhotoSize::Thumbnail);
			}
			if (!bytes.isEmpty()) {
				QFile f(it->fullPath);
				if (f.open(QIODevice::WriteOnly)) {
					f.write(bytes);
					f.close();
					auto entry = CustomDB::MediaIndexEntry();
					entry.peerId = it->peerId;
					entry.msgId = it->msgId;
					entry.kind = u"image"_q;
					entry.relPath = it->relPath;
					entry.fileName = it->relPath.mid(
						it->relPath.lastIndexOf(u'/') + 1);
					entry.size = bytes.size();
					entry.msgDate = it->msgDate;
					entry.archivedAt = static_cast<unsigned int>(
						QDateTime::currentSecsSinceEpoch());
					entry.layer = u"l2"_q;
					entry.status = u"present"_q;
					CustomDB::UpsertMediaIndex(CustomDB::PeerKey{qint64(session->userId().bare), it->peerId}, entry);
					CustomMediaQuota::AddBytes(bytes.size());
				}
			}
			done = true;
		}
		it = done ? gPendingPhotos.erase(it) : std::next(it);
	}
}

namespace {

void FlushPending() {
	if (gPending.empty()) {
		return;
	}
	// Bitta tranzaksiya — yuzlab alohida yozuv o'rniga.
	CustomDB::ExecRaw("BEGIN");
	for (const auto &row : gPending) {
		WriteRow(row);
	}
	CustomDB::ExecRaw("COMMIT");
	gPending.clear();
}

} // namespace

QString MediaKindOf(not_null<DocumentData*> document) {
	// 🔴 isVideoMessage() — bu YUMALOQ VIDEO (video note), audio EMAS.
	// 2026-08-15 gacha u `isVoiceMessage()` bilan birga "voice" deb
	// tasniflanardi va yumaloq videolar `medias/voices/` papkasiga
	// tushardi. Xato uch joyda takrorlangan edi; endi mantiq faqat
	// shu yerda.
	if (document->isVideoMessage()
		|| document->isVideoFile()
		|| document->isAnimation()
		|| document->isGifv()) {
		return u"video"_q;
	} else if (document->isVoiceMessage()) {
		return u"voice"_q;
	} else if (document->isImage()) {
		return u"image"_q;
	}
	// Musiqa fayllari ataylab "file" — ularda deyarli har doim haqiqiy
	// fayl nomi bo'ladi va "voices" papkasi ovozli XABARLAR uchun.
	return u"file"_q;
}

QString ExtensionForMime(const QString &mimeRaw) {
	const auto mime = mimeRaw.trimmed().toLower();
	if (mime.isEmpty()
		|| mime == u"application/octet-stream"_q) {
		return QString(); // noaniq — taxmin qilmaymiz
	}
	// Qo'lda jadval — QMimeDatabase ba'zan kam tarqalgan variantni beradi
	// (audio/ogg -> "oga", image/jpeg -> "jpeg"), bu yerda esa keng
	// tarqalgan shakl kerak.
	static const auto kKnown = QHash<QString, QString>{
		{ u"audio/ogg"_q,          u"ogg"_q },
		{ u"audio/opus"_q,         u"ogg"_q },
		{ u"audio/x-opus+ogg"_q,   u"ogg"_q },
		{ u"application/ogg"_q,    u"ogg"_q },
		{ u"audio/mpeg"_q,         u"mp3"_q },
		{ u"audio/mp4"_q,          u"m4a"_q },
		{ u"audio/x-m4a"_q,        u"m4a"_q },
		{ u"audio/x-wav"_q,        u"wav"_q },
		{ u"audio/wav"_q,          u"wav"_q },
		{ u"audio/flac"_q,         u"flac"_q },
		{ u"audio/x-flac"_q,       u"flac"_q },
		{ u"video/mp4"_q,          u"mp4"_q },
		{ u"video/quicktime"_q,    u"mov"_q },
		{ u"video/webm"_q,         u"webm"_q },
		{ u"video/x-matroska"_q,   u"mkv"_q },
		{ u"video/x-msvideo"_q,    u"avi"_q },
		{ u"video/3gpp"_q,         u"3gp"_q },
		{ u"image/jpeg"_q,         u"jpg"_q },
		{ u"image/png"_q,          u"png"_q },
		{ u"image/gif"_q,          u"gif"_q },
		{ u"image/webp"_q,         u"webp"_q },
		{ u"image/heic"_q,         u"heic"_q },
		{ u"application/pdf"_q,    u"pdf"_q },
		{ u"application/x-tgsticker"_q, u"tgs"_q },
	};
	const auto known = kKnown.constFind(mime);
	if (known != kKnown.constEnd()) {
		return known.value();
	}
	// Jadvalda yo'q bo'lsa — Qt'ning MIME bazasi.
	return QMimeDatabase()
		.mimeTypeForName(mime)
		.preferredSuffix()
		.toLower();
}

QString KindForMime(const QString &mimeRaw) {
	const auto mime = mimeRaw.trimmed().toLower();
	if (mime.startsWith(u"video/"_q)) {
		return u"video"_q;
	} else if (mime.startsWith(u"audio/"_q)) {
		return u"voice"_q;
	} else if (mime.startsWith(u"image/"_q)) {
		return u"image"_q;
	}
	// Boshqa hamma narsa (hujjat, arxiv, noaniq) — "file" emas, BO'SH.
	// Bo'sh qiymat "papkasini o'zgartirma" degani: files/ ichida turgan
	// PDF yoki sticker joyida qolishi kerak.
	return QString();
}

QString MediaExtensionFor(not_null<DocumentData*> document) {
	// 1) Haqiqiy fayl nomidagi kengaytma — eng ishonchli manba.
	//    Foydalanuvchi talabi: "native holatdagi file qanday extension
	//    bilan bo'lsa aynan o'zinikiga qaytara olsak zo'r bo'lardi".
	const auto name = document->filename();
	if (!name.isEmpty()) {
		const auto suffix = QFileInfo(name).suffix().toLower();
		// Uzun "kengaytma" — aslida nuqtali fayl nomi, ishonmaymiz.
		if (!suffix.isEmpty() && suffix.length() <= 8) {
			return suffix;
		}
	}

	// 2) MIME turi. Telegram uni to'g'ri yuboradi, shuning uchun bu
	//    hujjat turiga qarab TAXMIN qilishdan ancha ishonchli.
	const auto byMime = ExtensionForMime(document->mimeString());
	if (!byMime.isEmpty()) {
		return byMime;
	}

	// 3) Oxirgi chora — hujjat turiga qarab. Faqat shu yerda taxmin
	//    qilamiz, va faqat keng tarqalgan konteynerlardan.
	if (document->isVideoMessage()
		|| document->isVideoFile()
		|| document->isAnimation()
		|| document->isGifv()) {
		return u"mp4"_q;
	} else if (document->isVoiceMessage()) {
		// DIQQAT: ovozli xabar .mp3 EMAS. Telegram uni OGG/Opus
		// konteynerida yuboradi — .mp3 deb nomlash faylni buzmaydi,
		// lekin pleyerlar ochmay qo'yishi mumkin. Native format ustun.
		return u"ogg"_q;
	} else if (document->isSong() || document->isAudioFile()) {
		return u"mp3"_q;
	} else if (document->isImage()) {
		return u"jpg"_q;
	}
	return u"bin"_q;
}

void MaybeArchiveItem(not_null<HistoryItem*> item) {
	if (!item->isRegular()) {
		// Lokal/yuborilayotgan/xizmat xabari — hali server ID yo'q.
		// Yuborilgan xabar server ID olgach, HistoryItem::setRealId()
		// bizni qaytadan chaqiradi.
		return;
	}
	// 2026-08-25: QO'YILGAN placeholder'ni QAYTA ARXIVLAMAYMIZ.
	//
	// `loadDeletedMessages()` o'chirilgan xabarlar o'rniga lokal
	// placeholder qo'yadi va uning matni "—— O'CHIRILDI ——" markeri
	// bilan boshlanadi. Chat qayta ochilganda scrollback hook'i uni
	// oddiy xabar deb qabul qilib, MARKER MATNINI arxivga yozardi —
	// ya'ni asl matn o'rniga ko'rsatish matni saqlanardi.
	//
	// DB dalili: 12 ta shunday yozuv topildi, ba'zilari ikki marta
	// (msg 393835 — 08-13 da asl matn, 08-23 da marker bilan). Asl
	// yozuv keyinchalik tozalansa, faqat buzilgani qolardi.
	//
	// v3->v4 migratsiyasi bir marta shunday yozuvlarni tozalagan edi
	// (custom_db.cpp) — demak bu nuqson QAYTALANGAN.
	if (item->isDeletedLocally()) {
		return;
	}
	const auto peerIdStr = QString::number(item->history()->peer->id.value);
	if (!CustomSettings::ShouldBackgroundCache(peerIdStr)) {
		return; // kuzatilmayotgan chat — hech narsa qilmaymiz
	}
	// 2026-08-24: peer nomini eslab qolamiz. Keyinchalik eksport
	// ro'yxatida nom ko'rsatish uchun kerak: u yerda faqat peerId bo'ladi
	// va peerLoaded() ni yuklanmagan peer uchun ishlatib bo'lmaydi.
	// RememberPeerName() o'zgarmagan nomda darhol qaytadi, shuning uchun
	// bu har xabarda registry'ga yozmaydi.
	CustomSettings::RememberPeerName(
		peerIdStr,
		item->history()->peer->name());
	const auto text = item->originalText().text;
	const auto isMedia = (item->media() != nullptr);
	if (text.isEmpty() && !isMedia) {
		return; // saqlashga arzimaydi (CacheMessageText ning o'z mantig'i)
	}

	auto row = PendingRow();
	row.accountId = qint64(item->history()->session().userId().bare);
	row.peerId = peerIdStr;
	row.text = text;
	row.senderId = QString::number(item->from()->id.value);
	row.msgId = static_cast<long long>(item->id.bare);
	row.msgDate = static_cast<unsigned int>(item->date());
	row.isOut = item->out();
	row.isMedia = isMedia;

	if (isMedia) {
		MaybeDownloadMedia(item);
	}

	if (gBatchDepth > 0) {
		gPending.push_back(std::move(row));
		if (gPending.size() >= kFlushThreshold) {
			FlushPending();
		}
		return;
	}
	// Partiyadan tashqarida (real-vaqt yo'li) — bittalab yozamiz.
	WriteRow(row);
}

void BeginBatch() {
	++gBatchDepth;
}

void EndBatch() {
	if (gBatchDepth > 0) {
		--gBatchDepth;
	}
	if (gBatchDepth == 0) {
		FlushPending();
	}
}

void RestoreDeletedChats(not_null<Main::Session*> session) {
	const auto peers = CustomDB::GetPeersWithDeletedMessages(qint64(session->userId().bare));
	if (peers.isEmpty()) {
		return;
	}
	auto &owner = session->data();
	for (const auto &peerIdStr : peers) {
		auto ok = false;
		const auto raw = peerIdStr.toULongLong(&ok);
		if (!ok || !raw) {
			continue;
		}
		if (!CustomSettings::ShouldAntiDelete(peerIdStr)) {
			continue; // bu chat uchun AntiDelete o'chirilgan
		}
		const auto peerId = PeerId(raw);

		// MUHIM (performans): bu ro'yxatda 300+ peer bo'lishi mumkin va
		// ayrimlarida o'n minglab o'chirilgan xabar bor. Shuning uchun
		// hech narsa YARATMAYDIGAN tekshiruvlardan boshlaymiz — History
		// yaratish va inject qilish faqat haqiqatan yo'qolgan chat uchun.
		if (!owner.peerLoaded(peerId)) {
			continue; // peer hali yuklanmagan — tegmaymiz
		}
		if (const auto existing = owner.historyLoaded(peerId)) {
			if (existing->inChatList()) {
				continue; // chat allaqachon ro'yxatda — ish yo'q
			}
		}

		const auto history = owner.history(peerId);
		if (history->inChatList()) {
			continue;
		}
		history->loadDeletedMessages();
		if (history->isEmpty()) {
			continue; // inject qilinmadi (masalan hammasi allaqachon bor)
		}
		if (history->folderKnown()) {
			// Papkasi ma'lum — to'g'ridan-to'g'ri ro'yxatga qo'shamiz.
			// refreshChatListEntry o'zi "ro'yxatda yo'q" holatini ham
			// qayta ishlaydi (existenceChanged).
			owner.refreshChatListEntry(history);
		} else {
			// Papka noma'lum: refreshChatListEntry Expects(folderKnown())
			// bilan yiqilardi. Serverdan dialog yozuvini so'raymiz —
			// javob kelgach tdesktop uni ro'yxatga o'zi qo'shadi.
			owner.histories().requestDialogEntry(history);
		}
	}
}

void TryRescueMedia(not_null<HistoryItem*> item) {
	const auto media = item->media();
	if (!media) {
		return;
	}
	const auto document = media->document();
	if (!document) {
		return; // rasm uchun setDeletedLocally() ning o'z yo'li bor
	}
	const auto peer = item->history()->peer;
	if (peer->isSelf()) {
		return;
	}
	const auto peerIdStr = QString::number(peer->id.value);
	if (!CustomSettings::ShouldMediaBackup(peerIdStr)) {
		return;
	}
	if (document->loading()) {
		return; // yuklanmoqda — aralashmaymiz
	}
	const auto msgId = static_cast<long long>(item->id.bare);
	if (CustomDB::HasPresentMediaIndexEntry(CustomDB::Key(item), msgId)) {
		return; // allaqachon arxivda
	}

	const auto kind = MediaKindOf(document);
	const auto maxBytes = qint64(
		CustomSettings::MediaBackupMaxFileMb()) * 1024 * 1024;
	if (document->size > maxBytes) {
		// L3 da 'missing' — L2 dagi 'pending' dan farqli. Sabab: xabar
		// o'chirilgan, ya'ni keyinroq qayta urinishning ma'nosi yo'q.
		RecordIndex(item, document, kind,
			u"missing"_q, u"too_large"_q, QString(), u"l3"_q);
		return;
	}
	if (CustomMediaQuota::IsFull()) {
		RecordIndex(item, document, kind,
			u"missing"_q, u"quota_full"_q, QString(), u"l3"_q);
		return;
	}

	const auto relPath = ArchiveRelPath(item, document, kind);
	const auto fullPath = CustomSettings::ArchiveRoot() + u"/"_q + relPath;
	QDir().mkpath(QFileInfo(fullPath).absolutePath());
	document->save(Data::FileOriginMessage(item->fullId()), fullPath);

	// Urinish boshlandi. Muvaffaqiyatli bo'lsa finishLoad() buni
	// 'present' ga o'tkazadi; bo'lmasa (file_reference eskirgan bo'lsa)
	// yozuv shu holatda qoladi va keyingi ishga tushishda
	// ReconcileMediaIndex() uni 'missing' ga tushiradi.
	RecordIndex(item, document, kind,
		u"pending"_q, u"rescue_downloading"_q, relPath, u"l3"_q);
}

void NoteArchivedDownloadFinished(
		const CustomDB::PeerKey &key,
		long long msgId,
		long long size) {
	if (key.peerId.isEmpty()) {
		return;
	}
	CustomDB::SetMediaIndexStatus(key, msgId, u"present"_q, QString());
	CustomMediaQuota::AddBytes(size);
}

void StartMaintenance() {
	static QTimer *timer = nullptr;
	if (timer) {
		return; // allaqachon ishga tushirilgan
	}
	timer = new QTimer();
	QObject::connect(timer, &QTimer::timeout, [] {
		CustomDB::Checkpoint();
	});
	timer->start(kCheckpointIntervalMs);
}

} // namespace CustomArchive
