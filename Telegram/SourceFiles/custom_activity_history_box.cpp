#include "custom_activity_history_box.h"

#include "custom_activity_history.h"
#include "custom_db.h"
#include "custom_settings.h" // ArchiveRoot (saqlangan avatar yo'li)
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h" // Ui::LinkButton
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/widgets/scroll_area.h"
#include "ui/toast/toast.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"
#include "styles/style_custom_mod.h" // customModHintLabel
#include "lang/lang_keys.h"
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

namespace CustomActivityHistory {
namespace {

// Faollik tarixi oynasi har yozuv uchun alohida widget yaratadi, shuning
// uchun cheklov SHART: eng faol kontaktda 12 000 dan ortiq yozuv bor edi.
constexpr auto kActivityHistoryLimit = 300;

// Qisqa "online" davrlar ikki xil sababdan bo'ladi va ularni ajratib
// bo'lmaydi:
//   1) telefonning davriy ulanishi (shovqin),
//   2) Ghost Mode ishlatadigan klient — odam xabar yuborgan LAHZADA
//      online bo'ladi va shu zahoti offline (eng qimmatli signal).
// Shuning uchun ularni YO'QOTMAYMIZ, faqat ketma-ket kelganlarini
// bitta qatorga guruhlaymiz — ro'yxat o'qilishi oson bo'lsin.
constexpr auto kMinMeaningfulOnlineSeconds = 60;

// Ketma-ket qisqa ulanishlar shu oraliqdan yaqin bo'lsa bitta guruhga
// yig'iladi (30 daqiqa).
constexpr auto kShortGroupGapSeconds = 30 * 60;

// Jurnalda ketma-ket kelgan kuzatilgan last-seen yozuvlari shu sondan
// ko'p bo'lsa bitta guruh qatoriga yig'iladi. 2 ta yozuv uchun ham
// guruhlash foydali: ikki qator o'rniga bitta qator qoladi.
constexpr auto kMinGroupedStatusRun = 2;

// Jurnalda kuzatilgan last-seen o'zgarishlarini ko'rsatish. Bu ko'rish
// filtri, sozlama emas — registrga yozilmaydi, ilova qayta ishga
// tushganda o'chiq holatda boshlanadi.
bool gShowObservedStatus = false;

// 2026-08-15: arxivda saqlangan profil rasmining yo'li (yo'q bo'lsa bo'sh).
// Nom sxemasi custom_activity_history.cpp dagi MaybeBackupUserpic() bilan
// bir xil bo'lishi SHART.
[[nodiscard]] QString ArchivedAvatarPath(
		const QString &peerId,
		const QString &photoIdValue) {
	if (photoIdValue.isEmpty() || photoIdValue == u"empty"_q) {
		return QString(); // rasm o'chirilgan — saqlanadigan narsa yo'q
	}
	const auto path = CustomSettings::ArchiveRoot()
		+ u"/medias/avatars/"_q + peerId + u"_"_q + photoIdValue + u".jpg"_q;
	return QFile::exists(path) ? path : QString();
}

[[nodiscard]] QString SourceMarker(const QString &source) {
	if (source == u"story"_q) return u"📖"_q;
	if (source == u"manual"_q) return u"✍️"_q;
	if (source == u"buffer"_q) return u"⏱"_q;
	if (source == u"read"_q) return u"✓✓"_q;
	return QString();
}

QString FormatEntryLine(const CustomDB::ActivityHistoryEntry &entry) {
	const auto when = QDateTime::fromSecsSinceEpoch(entry.observedAt)
		.toString(u"dd.MM.yyyy HH:mm"_q);
	const auto marker = SourceMarker(entry.source);
	const auto sourcePrefix = marker.isEmpty() ? QString() : (marker + u" "_q);
	const auto fieldLabel = [&] {
		if (entry.field == u"name"_q) return u"Ism"_q;
		if (entry.field == u"username"_q) return u"Username"_q;
		if (entry.field == u"photo"_q) return u"Rasm"_q;
		if (entry.field == u"status"_q) return u"Last-seen"_q;
		if (entry.field == u"story"_q) return u"Hikoya"_q;
		return entry.field;
	}();
	const auto valueLabel = (entry.field == u"status"_q)
		? DecodeStatusLabel(entry.newValue)
		: (entry.field == u"story"_q)
		? DecodeStoryLabel(entry.newValue)
		: (entry.newValue.isEmpty() ? u"(bo'sh)"_q : entry.newValue);
	if (!entry.hasOldValue) {
		return sourcePrefix + fieldLabel + u": "_q + valueLabel + u" (kuzatish boshlandi, "_q + when + u")"_q;
	}
	const auto oldLabel = (entry.field == u"status"_q)
		? DecodeStatusLabel(entry.oldValue)
		: (entry.field == u"story"_q)
		? DecodeStoryLabel(entry.oldValue)
		: (entry.oldValue.isEmpty() ? u"(bo'sh)"_q : entry.oldValue);
	return sourcePrefix + fieldLabel + u": '"_q + oldLabel + u"' -> '"_q + valueLabel + u"' ("_q + when + u")"_q;
}

struct OnlinePeriod {
	qint64 from = 0;
	qint64 to = 0;
	bool instant = false;   // true = juftisiz nuqta (davr emas, LAHZA)
	QString source;         // "observed" | "story" | "manual" | "buffer" | "read"
};

QString FormatInstantLabel(const OnlinePeriod &p) {
	const auto when = QDateTime::fromSecsSinceEpoch(p.from)
		.toString(u"dd.MM HH:mm"_q);
	if (p.source == u"story"_q) {
		return when + u" — 📖 hikoya qo'ygan"_q;
	} else if (p.source == u"manual"_q) {
		return when + u" — ✍️ qo'lda kiritilgan"_q;
	} else if (p.source == u"buffer"_q) {
		return when + u" — ⏱ buferdan tiklangan"_q;
	} else if (p.source == u"read"_q) {
		return when + u" — ✓✓ xabarni o'qigan"_q;
	}
	return when + u" — aniq lahza"_q;
}

// T1 (online-observed) dan keyingi birinchi T2 (offline-observed) bilan
// juftlaydi. Juftisiz online yozuvlari (story, manual, yoki offline kelmagan holatlar)
// alohida LAHZA (instant=true) sifatida saqlanadi.
// entries — GetActivityHistory() natijasi (newest-first).
QVector<OnlinePeriod> ReconstructOnlinePeriods(
		const QVector<CustomDB::ActivityHistoryEntry> &entries) {
	// Chronological tartibga o'tkazamiz (eskisidan yangisiga).
	QVector<CustomDB::ActivityHistoryEntry> chrono;
	chrono.reserve(entries.size());
	for (auto it = entries.crbegin(); it != entries.crend(); ++it) {
		if (it->field == u"status"_q) chrono.append(*it);
	}

	QVector<OnlinePeriod> result;
	qint64 openFrom = 0;
	QString openSource;
	for (const auto &e : chrono) {
		if (e.newValue.startsWith(u"online:"_q)) {
			// Agar oldingi online: uchun offline: kelmasdan yangi online: kelsa,
			// avvalgi ochiq qolgan yozuvni alohida LAHZA sifatida saqlaymiz.
			if (openFrom > 0) {
				result.append({ openFrom, openFrom, true, openSource });
			}
			openFrom = e.observedAt;
			openSource = e.source;
		} else if (e.newValue.startsWith(u"offline:"_q)) {
			if (openFrom > 0) {
				const auto till = e.newValue.mid(8).toLongLong();
				const auto toTime = (till > 0) ? till : e.observedAt;
				result.append({ openFrom, toTime, false, openSource });
				openFrom = 0;
				openSource.clear();
			}
		}
	}
	if (openFrom > 0) {
		result.append({ openFrom, openFrom, true, openSource });
	}
	return result;
}

} // namespace

object_ptr<Ui::BoxContent> MakeHistoryBox(
		not_null<Main::Session*> session,
		const QString &peerId,
		const QString &displayName) {
	return Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"📜 "_q + displayName + u" — Faollik tarixi"_q));

		const auto entries = CustomDB::GetActivityHistory(
			peerId,
			kActivityHistoryLimit);
		const auto content = box->verticalLayout();

		// ── 1) Joriy holat + 2) So'nggi ko'ra olgan holatim ─────────
		QString latestStatus;
		const auto hasStatus = CustomDB::GetLatestActivityHistoryValue(
			peerId, u"status"_q, latestStatus);
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Eng so'nggi aniqlangan holat: "_q + (hasStatus
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
		// Davrlarni chiqarish: uzunlari alohida qator, ketma-ket qisqalari
		// esa bitta guruh qatoriga yig'iladi. HECH NARSA yo'qolmaydi —
		// guruh qatori nechta ulanish bo'lganini va oraliqni ko'rsatadi.
		const auto allPeriods = ReconstructOnlinePeriods(entries);
		auto rows = QVector<QString>();
		auto shortFrom = qint64(0);
		auto shortTo = qint64(0);
		auto shortCount = 0;
		// D2: guruhga tushgan davrlarning manba belgilari. Ilgari guruh
		// qatori belgisiz chiqardi, natijada `read` va `story` kabi eng
		// qimmatli signallar butunlay ko'rinmay qolardi — ular tabiatan
		// qisqa (o'qish ham, hikoya qo'yish ham bir lahzalik), ya'ni
		// deyarli HAR DOIM aynan shu guruhga tushadi.
		auto shortMarkers = QString();
		const auto flushShort = [&] {
			if (!shortCount) return;
			const auto a = QDateTime::fromSecsSinceEpoch(shortFrom);
			const auto b = QDateTime::fromSecsSinceEpoch(shortTo);
			auto line = QString((shortCount == 1)
				? (a.toString(u"dd.MM HH:mm:ss"_q)
					+ u" - qisqa ulanish"_q)
				: (a.toString(u"dd.MM HH:mm"_q) + u" - "_q
					+ b.toString(u"HH:mm"_q) + u": "_q
					+ QString::number(shortCount)
					+ u" ta qisqa ulanish"_q));
			if (!shortMarkers.isEmpty()) {
				line += u"  "_q + shortMarkers;
			}
			rows.append(line);
			shortCount = 0;
			shortMarkers.clear();
		};
		for (const auto &p : allPeriods) {
			if (p.instant) {
				flushShort();
				rows.append(FormatInstantLabel(p));
				continue;
			}
			if (p.to - p.from >= kMinMeaningfulOnlineSeconds) {
				flushShort();
				const auto from = QDateTime::fromSecsSinceEpoch(p.from);
				const auto to = QDateTime::fromSecsSinceEpoch(p.to);
				const auto minutes = std::max<qint64>(0, (p.to - p.from) / 60);
				const auto marker = SourceMarker(p.source);
				const auto suffix = marker.isEmpty() ? QString() : (u"  "_q + marker);
				rows.append(from.toString(u"dd.MM HH:mm"_q) + u" - "_q
					+ to.toString(u"HH:mm"_q) + u" ("_q
					+ QString::number(minutes) + u" daqiqa)"_q
					+ suffix);
				continue;
			}
			// Qisqa ulanish — guruhga qo'shamiz.
			if (shortCount
					&& (p.from - shortTo) > kShortGroupGapSeconds) {
				flushShort();
			}
			if (!shortCount) shortFrom = p.from;
			shortTo = p.to;
			++shortCount;
			// Bir xil belgi takrorlanmasin — guruhda 20 ta "observed" va
			// bitta "read" bo'lsa, faqat ✓✓ ko'rinishi kifoya.
			const auto marker = SourceMarker(p.source);
			if (!marker.isEmpty() && !shortMarkers.contains(marker)) {
				if (!shortMarkers.isEmpty()) shortMarkers += u" "_q;
				shortMarkers += marker;
			}
		}
		flushShort();

		if (rows.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"(hali ma'lumot yo'q — bu ro'yxat faqat "
						"ilova ochiq turgan paytda kuzatilgan online→"
						"offline juftliklaridan tuziladi)"_q),
					st::boxLabel),
				st::boxRowPadding);
		}
		for (const auto &line : rows) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(line),
					st::boxLabel),
				st::boxRowPadding);
		}

		// ── 4) O'zgarishlar jurnali ──────────────────────────────────
		// 2026-08-24: ro'yxat 300 ta bilan cheklangan (widget soni).
		// 2026-09-02: cheklovning o'zi yetarli emas edi — yozuvlarning
		// ~95% i kuzatilgan last-seen o'zgarishlari bo'lgani uchun ular
		// LIMIT ni to'ldirib, kamyob ism/username o'zgarishlarini siqib
		// chiqarardi (bir peerda 266 tadan atigi 10 tasi ko'rinardi).
		// Shuning uchun ular sukut bo'yicha YASHIRILADI, yoqilganda esa
		// ketma-ketlari bitta qatorga guruhlanadi.
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"O'zgarishlar jurnali:"_q),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		const auto statusToggle = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(u"Last-seen o'zgarishlarini ham ko'rsatish"_q),
				st::settingsButtonNoIcon));
		const auto journal = content->add(
			object_ptr<Ui::VerticalLayout>(content));

		// Bitta yozuv qatori + unga tegishli havolalar.
		const auto addEntryRow = [=](
				const CustomDB::ActivityHistoryEntry &e) {
			const auto label = journal->add(
				object_ptr<Ui::FlatLabel>(
					journal,
					rpl::single(FormatEntryLine(e)),
					st::boxLabel),
				st::boxRowPadding);

			if (e.source != u"observed"_q) {
				const auto delLink = journal->add(
					object_ptr<Ui::LinkButton>(
						journal,
						u"🗑 O'chirish"_q),
					st::boxRowPadding);
				delLink->setClickedCallback([=] {
					if (CustomDB::DeleteActivityEntry(e.id)) {
						label->hide();
						delLink->hide();
						Ui::Toast::Show(u"Yozuv o'chirildi"_q);
					} else {
						Ui::Toast::Show(u"O'chirib bo'lmadi"_q);
					}
				});
			}

			// 2026-08-15: rasm o'zgarishida SAQLANGAN rasmni ochish
			// imkoni. Ilgari avatar faqat diskda yotardi va uni faqat
			// papkani qo'lda ochib ko'rish mumkin edi.
			if (e.field != u"photo"_q) {
				return;
			}
			const auto path = ArchivedAvatarPath(peerId, e.newValue);
			if (path.isEmpty()) {
				return;
			}
			const auto open = journal->add(
				object_ptr<Ui::LinkButton>(
					journal,
					u"🖼 Saqlangan rasmni ochish"_q),
				st::boxRowPadding);
			open->setClickedCallback([=] {
				QDesktopServices::openUrl(QUrl::fromLocalFile(path));
			});
		};

		const auto addHint = [=](const QString &text) {
			journal->add(
				object_ptr<Ui::FlatLabel>(
					journal,
					rpl::single(text),
					st::customModHintLabel),
				st::boxRowPadding);
		};

		const auto rebuildJournal = [=] {
			journal->clear();

			const auto list = CustomDB::GetActivityHistory(
				peerId,
				kActivityHistoryLimit,
				gShowObservedStatus);

			if (list.isEmpty()) {
				// Bo'sh ro'yxat nosozlikka o'xshab ko'rinmasin — sabab
				// aytiladi, aks holda "tarix yo'qolibdi" deb o'ylanadi.
				addHint(gShowObservedStatus
					? u"(hali hech qanday yozuv yo'q)"_q
					: u"Ism, username, rasm yoki hikoya o'zgarishi qayd "
						"etilmagan. Bu kontaktda faqat last-seen "
						"kuzatilgan — uni ko'rish uchun yuqoridagi "
						"tugmani yoqing."_q);
				return;
			}
			if (list.size() >= kActivityHistoryLimit) {
				addHint(u"So'nggi %1 ta yozuv ko'rsatilmoqda."_q
					.arg(kActivityHistoryLimit));
			}

			// Ketma-ket kelgan kuzatilgan last-seen yozuvlarini bitta
			// qatorga yig'amiz. Ular qiymati emas, SONI ma'noli — aniq
			// vaqtlar yuqoridagi "Online bo'lgan davrlar" ro'yxatida.
			const auto groupable = [&](int at) {
				return (list[at].field == u"status"_q)
					&& (list[at].source == u"observed"_q);
			};
			auto i = 0;
			while (i < list.size()) {
				if (!groupable(i)) {
					addEntryRow(list[i]);
					++i;
					continue;
				}
				auto j = i;
				while (j < list.size() && groupable(j)) {
					++j;
				}
				const auto count = j - i;
				if (count < kMinGroupedStatusRun) {
					for (auto k = i; k < j; ++k) {
						addEntryRow(list[k]);
					}
					i = j;
					continue;
				}
				// Ro'yxat eng yangisidan boshlanadi, shuning uchun
				// oraliqning boshi — oxirgi element.
				const auto oldest = QDateTime::fromSecsSinceEpoch(
					list[j - 1].observedAt);
				const auto newest = QDateTime::fromSecsSinceEpoch(
					list[i].observedAt);
				addHint(oldest.toString(u"dd.MM HH:mm"_q) + u" - "_q
					+ newest.toString(u"dd.MM HH:mm"_q) + u"  ·  "_q
					+ QString::number(count)
					+ u" ta last-seen o'zgarishi"_q);
				i = j;
			}
		};

		statusToggle->toggleOn(rpl::single(gShowObservedStatus));
		statusToggle->toggledValue()
			| rpl::skip(1)
			| rpl::on_next([=](bool on) {
				gShowObservedStatus = on;
				rebuildJournal();
			}, statusToggle->lifetime());
		rebuildJournal();

		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	});
}

} // namespace CustomActivityHistory
