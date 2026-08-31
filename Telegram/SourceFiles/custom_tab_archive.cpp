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

	const auto addMessageRows = [&](
			not_null<Ui::VerticalLayout*> parent,
			const QString &sectionTitle,
			const QVector<CustomDB::DeletedMessageWithPeer> &messages) {
		if (messages.isEmpty()) {
			parent->add(
				object_ptr<Ui::FlatLabel>(
					parent,
					rpl::single(u"Arxivda xabarlar yo'q."_q),
					st::boxLabel),
				st::boxRowPadding);
			return;
		}

		parent->add(
			object_ptr<Ui::FlatLabel>(
				parent,
				rpl::single(u"%1 ta xabar — eng yangidan."_q
					.arg(messages.size())),
				st::customModHintLabel),
			st::defaultSubsectionTitlePadding);

		Ui::AddSkip(parent, st::settingsThumbSkip);

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

			parent->add(
				object_ptr<Ui::FlatLabel>(
					parent,
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
			if (body.isEmpty() && msg.isMedia) body = u"(media xabar)"_q;
			if (body.isEmpty()) body = u"(empty)"_q;

			if (msg.readAt > 0) {
				const auto readStr = QDateTime::fromSecsSinceEpoch(msg.readAt)
					.toString(u"dd.MM.yyyy HH:mm"_q);
				body += u"\n✓✓ o'qilgan: "_q + readStr;
			} else if (msg.readAt == -1) {
				body += u"\n✓✓ o'qilgan vaqti yashirilgan"_q;
			}

			parent->add(
				object_ptr<Ui::FlatLabel>(
					parent,
					rpl::single(body),
					st::boxLabel),
				st::boxRowPadding);

			Ui::AddSkip(parent, 6);
		}
	};

	// 1-bo'lim: O'chirilgan xabarlar (Standart ochiq)
	const auto s1 = AddCollapsibleSection(content, u"🗑️ O'chirilgan xabarlar"_q, true);
	addMessageRows(s1, u"🗑️ O'chirilgan xabarlar"_q, CustomDB::GetAllDeletedMessages(300));

	// 2-bo'lim: Tahrirlangan xabarlar (Standart yopiq)
	const auto s2 = AddCollapsibleSection(content, u"✏️ Tahrirlangan xabarlar"_q, false);
	{
		const auto edits = CustomDB::GetAllEditedMessages(300);
		if (edits.isEmpty()) {
			s2->add(
				object_ptr<Ui::FlatLabel>(
					s2,
					rpl::single(u"Arxivda tahrirlangan xabarlar yo'q."_q),
					st::boxLabel),
				st::boxRowPadding);
		} else {
			s2->add(
				object_ptr<Ui::FlatLabel>(
					s2,
					rpl::single(u"%1 ta tahrir — eng yangidan."_q
						.arg(edits.size())),
					st::customModHintLabel),
				st::defaultSubsectionTitlePadding);

			Ui::AddSkip(s2, st::settingsThumbSkip);

			for (const auto &edit : edits) {
				const auto dateStr = edit.date
					? QDateTime::fromSecsSinceEpoch(edit.date)
						.toString(u"yyyy-MM-dd  hh:mm"_q)
					: u"unknown date"_q;
				const auto editDateStr = edit.editDate
					? QDateTime::fromSecsSinceEpoch(edit.editDate)
						.toString(u"yyyy-MM-dd  hh:mm"_q)
					: u"unknown date"_q;
				const auto arrow = edit.isOut ? u"↑ You"_q : u"↓ Them"_q;
				const auto peerName =
					CustomSettings::GetPeerDisplayName(edit.peerId);
				const auto peerDisplay = peerName.isEmpty()
					? edit.peerId
					: (peerName + u" ("_q + edit.peerId + u")"_q);
				const auto header = u"[%1 -> %2]  %3  —  Chat: %4"_q
					.arg(dateStr, editDateStr, arrow, peerDisplay);

				s2->add(
					object_ptr<Ui::FlatLabel>(
						s2,
						rpl::single(header),
						st::defaultSubsectionTitle),
					st::defaultSubsectionTitlePadding);

				const auto oldText = edit.oldText.isEmpty()
					? u"(bo'sh)"_q
					: edit.oldText.left(150).replace(u'\n', u' ');
				const auto newText = edit.newText.isEmpty()
					? u"(bo'sh)"_q
					: edit.newText.left(150).replace(u'\n', u' ');
				const auto diff = MakeWordDiff(oldText, newText);

				s2->add(
					object_ptr<Ui::FlatLabel>(
						s2,
						rpl::single(u"Oldingi: "_q + oldText),
						st::customModHintLabel),
					st::boxRowPadding);
				s2->add(
					object_ptr<Ui::FlatLabel>(
						s2,
						rpl::single(u"Yangi:   "_q + newText),
						st::boxLabel),
					st::boxRowPadding);
				s2->add(
					object_ptr<Ui::FlatLabel>(
						s2,
						rpl::single(u"Farq:    "_q + diff),
						st::customModHintLabel),
					st::boxRowPadding);

				Ui::AddSkip(s2, 6);
			}
		}
	}

	Ui::AddSkip(content, st::settingsThumbSkip);
}
