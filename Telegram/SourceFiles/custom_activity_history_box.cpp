#include "custom_activity_history_box.h"

#include "custom_activity_history.h"
#include "custom_db.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/widgets/scroll_area.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"
#include <QtCore/QDateTime>

namespace CustomActivityHistory {
namespace {

QString FormatEntryLine(const CustomDB::ActivityHistoryEntry &entry) {
	const auto when = QDateTime::fromSecsSinceEpoch(entry.observedAt)
		.toString(u"dd.MM.yyyy HH:mm"_q);
	const auto fieldLabel = [&] {
		if (entry.field == u"name"_q) return u"Ism"_q;
		if (entry.field == u"username"_q) return u"Username"_q;
		if (entry.field == u"photo"_q) return u"Rasm"_q;
		if (entry.field == u"status"_q) return u"Last-seen"_q;
		return entry.field;
	}();
	const auto valueLabel = (entry.field == u"status"_q)
		? DecodeStatusLabel(entry.newValue)
		: (entry.newValue.isEmpty() ? u"(bo'sh)"_q : entry.newValue);
	if (!entry.hasOldValue) {
		return fieldLabel + u": " + valueLabel + u" (kuzatish boshlandi, " + when + u")"_q;
	}
	const auto oldLabel = (entry.field == u"status"_q)
		? DecodeStatusLabel(entry.oldValue)
		: (entry.oldValue.isEmpty() ? u"(bo'sh)"_q : entry.oldValue);
	return fieldLabel + u": '" + oldLabel + u"' -> '" + valueLabel + u"' (" + when + u")"_q;
}

struct OnlinePeriod {
	qint64 from = 0;
	qint64 to = 0;
};

// T1 (online-observed) dan keyingi birinchi T2 (offline-observed) bilan
// juftlaydi. entries — GetActivityHistory() natijasi (newest-first).
QVector<OnlinePeriod> ReconstructOnlinePeriods(
		const QVector<CustomDB::ActivityHistoryEntry> &entries) {
	// Chronological tartibga o'tkazamiz (eskisidan yangisiga), algoritm
	// shunday ishlashi osonroq.
	QVector<CustomDB::ActivityHistoryEntry> chrono;
	chrono.reserve(entries.size());
	for (auto it = entries.crbegin(); it != entries.crend(); ++it) {
		if (it->field == u"status"_q) chrono.append(*it);
	}

	QVector<OnlinePeriod> result;
	qint64 openFrom = 0;
	for (const auto &e : chrono) {
		if (e.newValue.startsWith(u"online:"_q)) {
			openFrom = e.observedAt; // ketma-ket online — oxirgisi ustun oladi
		} else if (e.newValue.startsWith(u"offline:"_q) && openFrom > 0) {
			const auto till = e.newValue.mid(8).toLongLong();
			result.append({ openFrom, till > 0 ? till : e.observedAt });
			openFrom = 0;
		}
	}
	// Eslatma: agar loop tugaganda openFrom hali ham > 0 bo'lsa (ya'ni oxirgi
	// status yozuvi "online:" bo'lib, undan keyin "offline:" kelmagan bo'lsa),
	// bu ochiq davr e'tiborga olinmaydi. v1 uchun qabul qilinadi — "hozirgi
	// vaqt = now()" kodlanmaydi.
	return result;
}

} // namespace

object_ptr<Ui::BoxContent> MakeHistoryBox(
		not_null<Main::Session*> session,
		const QString &peerId,
		const QString &displayName) {
	return Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"📜 "_q + displayName + u" — Faollik tarixi"_q));

		const auto entries = CustomDB::GetActivityHistory(peerId);
		const auto content = box->verticalLayout();

		// ── 1) Joriy holat + 2) So'nggi ko'ra olgan holatim ─────────
		QString latestStatus;
		const auto hasStatus = CustomDB::GetLatestActivityHistoryValue(
			peerId, u"status"_q, latestStatus);
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Joriy holat: "_q + (hasStatus
					? DecodeStatusLabel(latestStatus)
					: u"noma'lum (hali kuzatilmagan)"_q)),
				st::boxLabel),
			st::boxRowPadding);

		if (hasStatus && (latestStatus == u"recently"_q
				|| latestStatus == u"within_week"_q
				|| latestStatus == u"within_month"_q
				|| latestStatus == u"long_ago"_q)) {
			// entries ro'yxati eng yangisidan boshlanadi (newest-first), shuning uchun
			// birinchi topilgan online:/offline: yozuv — bizning eng so'nggi haqiqiy
			// ko'ra olgan last-seen qiymatimiz.
			for (const auto &e : entries) {
				if (e.field == u"status"_q && (e.newValue.startsWith(u"online:"_q)
						|| e.newValue.startsWith(u"offline:"_q))) {
					content->add(
						object_ptr<Ui::FlatLabel>(
							content,
							rpl::single(u"So'nggi ko'ra olgan holatim: "_q
								+ DecodeStatusLabel(e.newValue)),
							st::boxLabel),
						st::boxRowPadding);
					break;
				}
			}
		}

		// ── 3) Online bo'lgan davrlar ────────────────────────────────
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Online bo'lgan davrlar:"_q),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);
		const auto periods = ReconstructOnlinePeriods(entries);
		if (periods.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"(hali ma'lumot yo'q)"_q),
					st::boxLabel),
				st::boxRowPadding);
		}
		for (const auto &p : periods) {
			const auto from = QDateTime::fromSecsSinceEpoch(p.from);
			const auto to = QDateTime::fromSecsSinceEpoch(p.to);
			const auto minutes = std::max<qint64>(0, (p.to - p.from) / 60);
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(
						from.toString(u"dd.MM HH:mm"_q) + u" - "_q
						+ to.toString(u"HH:mm"_q) + u" ("_q
						+ QString::number(minutes) + u" daqiqa)"_q),
					st::boxLabel),
				st::boxRowPadding);
		}

		// ── 4) To'liq o'zgarishlar jurnali ───────────────────────────
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"To'liq o'zgarishlar jurnali:"_q),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);
		if (entries.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"(hali hech qanday yozuq yo'q)"_q),
					st::boxLabel),
				st::boxRowPadding);
		}
		for (const auto &e : entries) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(FormatEntryLine(e)),
					st::boxLabel),
				st::boxRowPadding);
		}

		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	});
}

} // namespace CustomActivityHistory
