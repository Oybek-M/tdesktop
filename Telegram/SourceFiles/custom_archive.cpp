#include "custom_archive.h"

#include "custom_db.h"
#include "custom_settings.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
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
