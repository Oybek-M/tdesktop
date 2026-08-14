#include "custom_archive.h"

#include "custom_db.h"
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

// A13/K4: kuzatilayotgan chatda media avtomatik yuklab olinadi, shunda
// suhbatdosh butun chatni o'chirganda ham fayl qo'lda qoladi. Yuklab
// olingach data_document.cpp dagi mavjud finishLoad() hook uni doimiy
// papkaga (~/customizationMainFolder/medias/) ko'chiradi — bu yerda faqat
// yuklashni BOSHLAYMIZ.
void MaybeDownloadMedia(not_null<HistoryItem*> item) {
	const auto media = item->media();
	if (!media) {
		return;
	}
	const auto origin = Data::FileOriginMessage(item->fullId());
	if (const auto document = media->document()) {
		if (!document->filepath(true).isEmpty() || document->loading()) {
			return; // allaqachon diskda yoki yuklanmoqda
		}
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
		// Katta fayllarni arxivlash uchun haqiqiy maqsad yo'li kerak; bu
		// alohida vazifa (disk sarfi va fayl joylashuvi semantikasi bilan
		// birga o'ylanishi kerak). Shu sababli hozir faqat xotiraga
		// sig'adigan hajmdagilarni olamiz.
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
