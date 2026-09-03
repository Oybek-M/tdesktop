#include "custom_tab_common.h"

namespace {

// U1: ko'rish filtri, sozlama emas — shuning uchun registrga yozilmaydi
// va ilova qayta ishga tushganda o'chiq holatda boshlanadi.
bool gOnlyRead = false;

} // namespace

void fillArchiveTab(
		not_null<Ui::VerticalLayout*> content,
		Fn<void()> onRefresh) {
	// 1-bo'lim: ko'rish sozlamalari (standart OCHIQ) — kichik va
	// har safar kerak bo'ladi, shuning uchun yopilmaydi.
	const auto s0 = AddCollapsibleSection(
		content,
		u"🔎 Ko'rish"_q,
		true);

	s0->add(
		object_ptr<Ui::RoundButton>(
			s0,
			rpl::single(u"🔄 Yangilash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([onRefresh] {
		if (onRefresh) onRefresh();
	});

	// U1: "faqat o'qilgani ma'lum" filtri. Oddiy ro'yxat xabar sanasi
	// bo'yicha 300 ta bilan kesiladi, shu sababli o'qilgani aniqlangan
	// eski xabarlar unga tushmay qolardi. Filtr yoqilganda saralash
	// o'qilgan vaqti bo'yicha ketadi.
	Ui::AddSkip(s0, 8);
	{
		const auto btn = s0->add(
			object_ptr<Ui::SettingsButton>(
				s0,
				rpl::single(u"✓✓ Faqat o'qilgani ma'lum xabarlar"_q),
				st::settingsButtonNoIcon));
		btn->toggleOn(rpl::single(gOnlyRead));
		btn->toggledValue()
			| rpl::skip(1)
			| rpl::on_next([onRefresh](bool on) {
				gOnlyRead = on;
				// Ro'yxatni qayta qurish shu tugmaning o'zini yo'q qiladi,
				// shuning uchun joriy signal tugagach chaqiramiz.
				if (onRefresh) crl::on_main([onRefresh] { onRefresh(); });
			}, btn->lifetime());
	}

	const auto addMessageRows = [&](
			not_null<Ui::VerticalLayout*> target,
			const QVector<CustomDB::DeletedMessageWithPeer> &messages) {
		if (messages.isEmpty()) {
			target->add(
				object_ptr<Ui::FlatLabel>(
					target,
					rpl::single(gOnlyRead
						? u"O'qilgani ma'lum xabar yo'q."_q
						: u"Arxivda xabarlar yo'q."_q),
					st::boxLabel),
				st::boxRowPadding);
			return;
		}

		target->add(
			object_ptr<Ui::FlatLabel>(
				target,
				rpl::single((gOnlyRead
						? u"%1 ta xabar — o'qilgan vaqti bo'yicha."_q
						: u"%1 ta xabar — eng yangidan."_q)
					.arg(messages.size())),
				st::customModHintLabel),
			st::defaultSubsectionTitlePadding);

		Ui::AddSkip(target, st::settingsThumbSkip);

		for (const auto &msg : messages) {
			const auto dateStr = msg.date
				? QDateTime::fromSecsSinceEpoch(msg.date)
					.toString(u"yyyy-MM-dd  hh:mm"_q)
				: u"unknown date"_q;
			const auto arrow = msg.isOut ? u"↑ You"_q : u"↓ Them"_q;
			const auto peerName =
				CustomSettings::GetPeerDisplayName(msg.peerId);
			const auto peerDisplay = peerName.isEmpty()
				? msg.peerId
				: (peerName + u" ("_q + msg.peerId + u")"_q);
			const auto header = u"[%1]  %2  —  Chat: %3"_q
				.arg(dateStr, arrow, peerDisplay);

			target->add(
				object_ptr<Ui::FlatLabel>(
					target,
					rpl::single(header),
					st::defaultSubsectionTitle),
				st::defaultSubsectionTitlePadding);

			const auto hasText = !msg.text.isEmpty();
			const auto hasMedia = !msg.mediaPath.isEmpty();
			auto body = QString();
			if (hasText) {
				body = msg.text.left(200).replace(u'\n', u' ');
				if (msg.text.size() > 200) body += u"…"_q;
			}
			if (hasMedia) {
				const auto mediaNote = u"[media: %1]"_q
					.arg(QFileInfo(msg.mediaPath).fileName());
				body = body.isEmpty()
					? mediaNote
					: (body + u"  "_q + mediaNote);
			}
			// YANGI-1: media fayl saqlanmagan, lekin is_media flag bor (background
			// delete) — "(media xabar)" ko'rsatamiz, "(empty)" emas.
			if (body.isEmpty() && msg.isMedia) body = u"(media xabar)"_q;
			if (body.isEmpty()) body = u"(empty)"_q;

			if (msg.readAt > 0) {
				const auto readStr = QDateTime::fromSecsSinceEpoch(msg.readAt)
					.toString(u"dd.MM.yyyy HH:mm"_q);
				body += u"\n✓✓ o'qilgan: "_q + readStr;
			} else if (msg.readAt == -1) {
				body += u"\n✓✓ o'qilgan vaqti yashirilgan"_q;
			}

			target->add(
				object_ptr<Ui::FlatLabel>(
					target,
					rpl::single(body),
					st::boxLabel),
				st::boxRowPadding);

			Ui::AddSkip(target, 6);
		}
	};

	// 2-bo'lim: o'chirilgan xabarlar (standart OCHIQ) — arxivning
	// asosiy mazmuni shu.
	const auto s1 = AddCollapsibleSection(
		content,
		u"🗑️ O'chirilgan xabarlar"_q,
		true);
	addMessageRows(s1, CustomDB::GetAllDeletedMessages(300, gOnlyRead));

	// 3-bo'lim: tahrirlangan xabarlar (standart YOPIQ) — har yozuv
	// uchta qator egallaydi, ochiq holda tab juda uzayib ketardi.
	const auto s2 = AddCollapsibleSection(
		content,
		u"✏ Tahrirlangan xabarlar"_q,
		false);
	{
		const auto records = CustomDB::GetAllEditedMessages(300, gOnlyRead);
		if (records.isEmpty()) {
			s2->add(
				object_ptr<Ui::FlatLabel>(
					s2,
					rpl::single(gOnlyRead
						? u"O'qilgani ma'lum tahrir yozuvi yo'q."_q
						: u"Arxivda tahrir yozuvlari yo'q."_q),
					st::boxLabel),
				st::boxRowPadding);
		} else {
			s2->add(
				object_ptr<Ui::FlatLabel>(
					s2,
					rpl::single((gOnlyRead
							? u"%1 ta tahrir yozuvi — o'qilgan vaqti bo'yicha."_q
							: u"%1 ta tahrir yozuvi — eng yangidan."_q)
						.arg(records.size())),
					st::customModHintLabel),
				st::defaultSubsectionTitlePadding);

			Ui::AddSkip(s2, st::settingsThumbSkip);

			for (const auto &rec : records) {
				const auto when = rec.editedAt.isValid()
					? rec.editedAt.toString(u"yyyy-MM-dd  hh:mm"_q)
					: (rec.msgDate
						? QDateTime::fromSecsSinceEpoch(rec.msgDate)
							.toString(u"yyyy-MM-dd  hh:mm"_q)
						: u"unknown date"_q);
				const auto editPeerName =
					CustomSettings::GetPeerDisplayName(rec.peerId);
				const auto editPeerDisplay = editPeerName.isEmpty()
					? rec.peerId
					: (editPeerName + u" ("_q + rec.peerId + u")"_q);
				const auto header =
					u"[%1]  Chat: %2  xabar #%3"_q
						.arg(when, editPeerDisplay)
						.arg(rec.msgId);

				s2->add(
					object_ptr<Ui::FlatLabel>(
						s2,
						rpl::single(header),
						st::defaultSubsectionTitle),
					st::defaultSubsectionTitlePadding);

				const auto orig = rec.originalText.isEmpty()
					? u"(empty)"_q
					: rec.originalText.left(200).replace(u'\n', u' ');
				s2->add(
					object_ptr<Ui::FlatLabel>(
						s2,
						rpl::single(u"Avvalgi: "_q + orig),
						st::boxLabel),
					st::boxRowPadding);

				if (!rec.newText.isEmpty()) {
					const auto nw =
						rec.newText.left(200).replace(u'\n', u' ');
					s2->add(
						object_ptr<Ui::FlatLabel>(
							s2,
							rpl::single(u"Yangi:  "_q + nw),
							st::boxLabel),
						st::boxRowPadding);

					const auto diff = MakeWordDiff(
						rec.originalText.left(200).replace(u'\n', u' '),
						rec.newText.left(200).replace(u'\n', u' '));
					if (diff != orig) {
						s2->add(
							object_ptr<Ui::FlatLabel>(
								s2,
								rpl::single(u"Farq:   "_q + diff),
								st::customModHintLabel),
							st::boxRowPadding);
					}
				}

				// D1: o'chirilgan xabarlardagi bilan bir xil belgi.
				if (rec.readAt != 0) {
					const auto readLine = (rec.readAt > 0)
						? (u"✓✓ o'qilgan: "_q
							+ QDateTime::fromSecsSinceEpoch(rec.readAt)
								.toString(u"dd.MM.yyyy HH:mm"_q))
						: u"✓✓ o'qilgan vaqti yashirilgan"_q;
					s2->add(
						object_ptr<Ui::FlatLabel>(
							s2,
							rpl::single(readLine),
							st::boxLabel),
						st::boxRowPadding);
				}

				Ui::AddSkip(s2, 6);
			}
		}
	}

	Ui::AddSkip(content, st::settingsThumbSkip);
}

