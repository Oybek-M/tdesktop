#include "custom_archive.h"

#include "custom_db.h"
#include "custom_media_quota.h"
#include "custom_settings.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_histories.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "storage/file_download.h" // Storage::kMaxFileInMemory
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <vector>

namespace CustomArchive {
namespace {

struct PendingRow {
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
		row.peerId,
		row.msgId,
		row.text,
		row.isOut,
		row.msgDate,
		row.senderId,
		row.isMedia,
		true); // archived = true → PruneStaleCachedText tegmaydi (D4)
}

// Media turini SaveMediaFile() kutgan nomlar bilan bir xil aniqlaydi
// (data_document.cpp:1233 dagi mantiq bilan mos).
[[nodiscard]] QString MediaKindOf(not_null<DocumentData*> document) {
	if (document->isVideoMessage() || document->isVoiceMessage()) {
		return u"voice"_q;
	} else if (document->isVideoFile()) {
		return u"video"_q;
	}
	return u"file"_q;
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
	auto name = document->filename();
	if (name.isEmpty()) {
		// 2026-08-15 (sinovda topildi): document->filename() MEDIA sifatida
		// yuborilgan fayllar uchun bo'sh bo'ladi — video, ovozli xabar,
		// animatsiya. Ilgari bu yerda oddiy "file" qo'yilardi va natijada
		// eksport qilingan arxivda kengaytmasiz, ikki marta bosib
		// ochib bo'lmaydigan fayllar chiqardi (34 ta videodan ko'pchiligi
		// va 14 ta ovozli xabarning HAMMASI shunday edi).
		//
		// Qoida history_item.cpp dagi setDeletedLocally() bilan bir xil —
		// o'sha yerda bu muammo allaqachon hal qilingan edi.
		if (document->isVideoMessage() || document->isAnimation()) {
			name = u"video.mp4"_q;
		} else if (document->isVoiceMessage()) {
			name = u"voice.ogg"_q;
		} else if (document->isVideoFile()) {
			name = u"video.mp4"_q;
		} else {
			name = u"file.bin"_q;
		}
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
	CustomDB::UpsertMediaIndex(entry);
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
			const auto fullPath = CustomMediaQuota::ArchiveRoot()
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
		photo->load(Data::PhotoSize::Large, origin);
	}
}

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

void MaybeArchiveItem(not_null<HistoryItem*> item) {
	if (!item->isRegular()) {
		// Lokal/yuborilayotgan/xizmat xabari — hali server ID yo'q.
		// Yuborilgan xabar server ID olgach, HistoryItem::setRealId()
		// bizni qaytadan chaqiradi.
		return;
	}
	const auto peerIdStr = QString::number(item->history()->peer->id.value);
	if (!CustomSettings::ShouldBackgroundCache(peerIdStr)) {
		return; // kuzatilmayotgan chat — hech narsa qilmaymiz
	}
	const auto text = item->originalText().text;
	const auto isMedia = (item->media() != nullptr);
	if (text.isEmpty() && !isMedia) {
		return; // saqlashga arzimaydi (CacheMessageText ning o'z mantig'i)
	}

	auto row = PendingRow();
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
	const auto peers = CustomDB::GetPeersWithDeletedMessages();
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
	if (CustomDB::HasPresentMediaIndexEntry(peerIdStr, msgId)) {
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
	const auto fullPath = CustomMediaQuota::ArchiveRoot() + u"/"_q + relPath;
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
		const QString &peerId,
		long long msgId,
		long long size) {
	if (peerId.isEmpty()) {
		return;
	}
	CustomDB::SetMediaIndexStatus(peerId, msgId, u"present"_q, QString());
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
