#include "custom_tab_common.h"

void fillArchiveTab(
		not_null<Ui::VerticalLayout*> content,
		Fn<void()> onRefresh) {
	// ── Yangilash tugmasi ────────────────────────────────────────────
	Ui::AddSkip(content, st::settingsThumbSkip);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"🔄 Yangilash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([onRefresh] {
		if (onRefresh) onRefresh();
	});
	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);

	const auto addMessageRows = [&](
			const QString &sectionTitle,
			const QVector<CustomDB::DeletedMessageWithPeer> &messages) {
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(sectionTitle),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		if (messages.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"Arxivda xabarlar yo'q."_q),
					st::boxLabel),
				st::boxRowPadding);
			return;
		}

		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"%1 ta xabar — eng yangidan."_q
					.arg(messages.size())),
				st::customModHintLabel),
			st::defaultSubsectionTitlePadding);

		Ui::AddSkip(content, st::settingsThumbSkip);

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

			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
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

			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(body),
					st::boxLabel),
				st::boxRowPadding);

			Ui::AddSkip(content, 6);
		}
	};

	addMessageRows(
		u"🗑️ O'chirilgan xabarlar"_q,
		CustomDB::GetAllDeletedMessages(300));

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);

	{
		const auto records = CustomDB::GetAllEditedMessages(300);
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"✏ Tahrirlangan xabarlar"_q),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		if (records.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"Arxivda tahrir yozuvlari yo'q."_q),
					st::boxLabel),
				st::boxRowPadding);
		} else {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"%1 ta tahrir yozuvi — eng yangidan."_q
						.arg(records.size())),
					st::customModHintLabel),
				st::defaultSubsectionTitlePadding);

			Ui::AddSkip(content, st::settingsThumbSkip);

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

				content->add(
					object_ptr<Ui::FlatLabel>(
						content,
						rpl::single(header),
						st::defaultSubsectionTitle),
					st::defaultSubsectionTitlePadding);

				const auto orig = rec.originalText.isEmpty()
					? u"(empty)"_q
					: rec.originalText.left(200).replace(u'\n', u' ');
				content->add(
					object_ptr<Ui::FlatLabel>(
						content,
						rpl::single(u"Avvalgi: "_q + orig),
						st::boxLabel),
					st::boxRowPadding);

				if (!rec.newText.isEmpty()) {
					const auto nw =
						rec.newText.left(200).replace(u'\n', u' ');
					content->add(
						object_ptr<Ui::FlatLabel>(
							content,
							rpl::single(u"Yangi:  "_q + nw),
							st::boxLabel),
						st::boxRowPadding);

					const auto diff = MakeWordDiff(
						rec.originalText.left(200).replace(u'\n', u' '),
						rec.newText.left(200).replace(u'\n', u' '));
					if (diff != orig) {
						content->add(
							object_ptr<Ui::FlatLabel>(
								content,
								rpl::single(u"Farq:   "_q + diff),
								st::customModHintLabel),
							st::boxRowPadding);
					}
				}

				Ui::AddSkip(content, 6);
			}
		}
	}

	Ui::AddSkip(content, st::settingsThumbSkip);
}

