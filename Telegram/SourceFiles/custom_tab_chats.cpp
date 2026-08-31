#include "custom_tab_common.h"

void fillPeerSection(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		bool isWhitelist,
		Fn<void()> onRebuild) {
	struct State {
		int visibleCount = 0;
		Ui::VerticalLayout *entriesLayout = nullptr;
		Ui::SlideWrap<Ui::FlatLabel> *emptyWrap = nullptr;
		Ui::FlatLabel *countLabel = nullptr;
		QVector<Ui::SlideWrap<Ui::RpWidget>*> entryWraps;
		Fn<void(const QString &, const QString &)> addEntry;
	};
	const auto state = content->lifetime().make_state<State>();

	// ── Section header ───────────────────────────────────────────
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(isWhitelist ? u"White List"_q : u"Black List"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	{
		const auto sectionDesc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(isWhitelist
					? u"Bu ro'yxatdagi chatlar uchun barcha funksiyalar doim yoqiq bo'ladi."_q
					: u"Bu ro'yxatdagi chatlar uchun barcha funksiyalar doim o'chiq bo'ladi."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
		content->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				sectionDesc->resizeToWidth(lw);
				sectionDesc->update();
			}
		}, sectionDesc->lifetime());
	}

	// ── T42: Kategoriya bo'yicha tanlash ────────────────────────────
	Ui::AddSkip(content, 8);
	{
		const auto addCategoryToggle = [&](
				const QString &label,
				CustomSettings::PeerType type) {
			const auto btn = content->add(
				object_ptr<Ui::SettingsButton>(
					content,
					rpl::single(label),
					st::settingsButtonNoIcon),
				style::margins(0, 0, 0, 0));
			const bool initial = isWhitelist
				? CustomSettings::IsWhitelistCategoryEnabled(type)
				: CustomSettings::IsBlocklistCategoryEnabled(type);
			btn->toggleOn(rpl::single(initial));
			btn->toggledValue() | rpl::skip(1) | rpl::on_next([=](bool on) {
				const bool hadConflict = on && (isWhitelist
					? CustomSettings::IsBlocklistCategoryEnabled(type)
					: CustomSettings::IsWhitelistCategoryEnabled(type));
				if (isWhitelist) {
					CustomSettings::SetWhitelistCategory(type, on);
				} else {
					CustomSettings::SetBlocklistCategory(type, on);
				}
				if (hadConflict) {
					Ui::Toast::Show(
						u"Qarama-qarshi ro'yxatdagi mos kategoriya "
						"avtomatik o'chirildi."_q);
					if (onRebuild) onRebuild();
				}
			}, btn->lifetime());
		};
		addCategoryToggle(
			u"Shaxsiy chatlar"_q,
			CustomSettings::PeerType::User);
		addCategoryToggle(
			u"Guruhlar"_q,
			CustomSettings::PeerType::Group);
		addCategoryToggle(
			u"Kanallar / superguruhlar"_q,
			CustomSettings::PeerType::Channel);
	}

	// ── Chat tanlash (primary) ───────────────────────────────────
	Ui::AddSkip(content, 12);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(isWhitelist
				? u"Chat tanlash — White List"_q
				: u"Chat tanlash — Black List"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (!gInstance) return;
		// Box custom window ichida ochiladi (LayerManager orqali).
		gInstance->showBox(ChoosePeerBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				const auto peerId = QString::number(peer->id.value);
				const auto name = peer->name();
				const auto already = isWhitelist
					? CustomSettings::IsInWhitelist(peerId)
					: CustomSettings::IsInBlocklist(peerId);
				if (already) {
					Ui::Toast::Show(u"Bu chat allaqachon ro'yxatda."_q);
					return true;
				}
				// wasInOpposite AddToWhitelist/AddToBlocklist dan OLDIN hisoblanishi shart —
				// bu funksiyalar chaqirilgach, peer avtomatik ravishda qarama-qarshi
				// ro'yxatdan olib tashlanadi (mutual exclusion), shuning uchun keyin
				// tekshirsak har doim false qaytadi.
				const auto wasInOpposite = isWhitelist
					? CustomSettings::IsInBlocklist(peerId)
					: CustomSettings::IsInWhitelist(peerId);
				if (isWhitelist) {
					CustomSettings::AddToWhitelist(peerId, name);
				} else {
					CustomSettings::AddToBlocklist(peerId, name);
				}
				if (wasInOpposite) {
					Ui::Toast::Show(name + (isWhitelist
						? u" Black List'dan olib tashlandi va White List'ga qo'shildi."_q
						: u" White List'dan olib tashlandi va Black List'ga qo'shildi."_q));
					if (onRebuild) onRebuild();
				} else {
					state->addEntry(peerId, name);
					Ui::Toast::Show(name + u" qo'shildi."_q);
				}
				// raise()/activateWindow() shart emas — dialog window ichida ochiladi.
				return true;
			},
			rpl::single(isWhitelist
				? u"White List ga qo'shish"_q
				: u"Black List ga qo'shish"_q)));
	});

	// ── ID orqali qo'shish (secondary) ──────────────────────────
	Ui::AddSkip(content, 8);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Yoki ID orqali qo'shish:"_q),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	const auto peerIdInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Chat ID (masalan: 123456789)"_q),
			QString()),
		st::boxRowPadding);

	const auto nameInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Nom (ixtiyoriy)"_q),
			QString()),
		st::boxRowPadding);

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(isWhitelist
				? u"ID bo'yicha White List ga qo'shish"_q
				: u"ID bo'yicha Black List ga qo'shish"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto peerId = peerIdInput->getLastText().trimmed();
		if (peerId.isEmpty()) {
			Ui::Toast::Show(u"Chat ID bo'sh. Iltimos kiriting."_q);
			return;
		}
		const auto already = isWhitelist
			? CustomSettings::IsInWhitelist(peerId)
			: CustomSettings::IsInBlocklist(peerId);
		if (already) {
			Ui::Toast::Show(u"Bu chat allaqachon ro'yxatda."_q);
			return;
		}
		// wasInOpposite AddToWhitelist/AddToBlocklist dan OLDIN hisoblanishi shart —
		// bu funksiyalar chaqirilgach, peer avtomatik ravishda qarama-qarshi
		// ro'yxatdan olib tashlanadi (mutual exclusion), shuning uchun keyin
		// tekshirsak har doim false qaytadi.
		const auto wasInOpposite = isWhitelist
			? CustomSettings::IsInBlocklist(peerId)
			: CustomSettings::IsInWhitelist(peerId);
		const auto name = nameInput->getLastText().trimmed();
		if (isWhitelist) {
			CustomSettings::AddToWhitelist(peerId, name);
		} else {
			CustomSettings::AddToBlocklist(peerId, name);
		}
		peerIdInput->setText(QString());
		nameInput->setText(QString());
		if (wasInOpposite) {
			Ui::Toast::Show(u"Qarama-qarshi ro'yxatdan olib tashlandi va "_q
				+ (isWhitelist ? u"White List"_q : u"Black List"_q)
				+ u"'ga qo'shildi: "_q + peerId);
			if (onRebuild) onRebuild();
		} else {
			state->addEntry(peerId, name);
			Ui::Toast::Show(u"Qo'shildi: "_q + peerId);
		}
	});

	// ── Ro'yxat: header + bo'sh holat + entries ─────────────────
	Ui::AddSkip(content, 16);

	state->countLabel = content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString()),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	// updateHeader state->emptyWrap ni runtime da o'qiydi — shu sababli
	// emptyWrap dan oldin aniqlanishi mumkin.
	const auto updateHeader = [=](bool animated) {
		const auto n = state->visibleCount;
		state->countLabel->setText(isWhitelist
			? (n == 0
				? u"White List — bo'sh"_q
				: u"White List — %1 ta chat"_q.arg(n))
			: (n == 0
				? u"Black List — bo'sh"_q
				: u"Black List — %1 ta chat"_q.arg(n)));
		state->emptyWrap->toggle(
			n == 0,
			animated ? anim::type::normal : anim::type::instant);
	};

	// ── Barchasini tozalash tugmasi ──────────────────────────────
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"Barchasini tozalash"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (state->visibleCount == 0) {
			Ui::Toast::Show(u"Ro'yxat allaqachon bo'sh."_q);
			return;
		}
		if (isWhitelist) {
			CustomSettings::ClearWhitelist();
		} else {
			CustomSettings::ClearBlocklist();
		}
		for (auto *wrap : state->entryWraps) {
			wrap->toggle(false, anim::type::normal);
		}
		state->visibleCount = 0;
		updateHeader(true);
		Ui::Toast::Show(isWhitelist
			? u"White List tozalandi."_q
			: u"Black List tozalandi."_q);
	});

	state->entriesLayout = content->add(
		object_ptr<Ui::VerticalLayout>(content),
		{0, 0, 0, 0});

	state->emptyWrap = state->entriesLayout->add(
		object_ptr<Ui::SlideWrap<Ui::FlatLabel>>(
			state->entriesLayout,
			object_ptr<Ui::FlatLabel>(
				state->entriesLayout,
				rpl::single(u"Ro'yxat bo'sh. Yuqoridan chat qo'shing."_q),
				st::customModHintLabel)),
		st::defaultSubsectionTitlePadding,
		style::al_justify);

	// ── Entry row builder ────────────────────────────────────────
	state->addEntry = [=](const QString &peerId, const QString &name) {
		constexpr int kRowH    = 56;
		constexpr int kAvSize  = 38;
		[[maybe_unused]] constexpr int kPadL = 14;
		[[maybe_unused]] constexpr int kPadR = 12;
		[[maybe_unused]] constexpr int kGap  = 12;
		constexpr int kDelBtnW = 76;

		const auto entryWrap = state->entriesLayout->add(
			object_ptr<Ui::SlideWrap<Ui::RpWidget>>(
				state->entriesLayout,
				object_ptr<Ui::RpWidget>(state->entriesLayout)),
			style::margins(),
			style::al_justify);
		state->entryWraps.append(entryWrap);
		const auto row = entryWrap->entity();
		row->setFixedHeight(kRowH);

		// Avatar circle — real userpic agar peer cache da bo'lsa,
		// aks holda fallback (harf + rang).
		const auto av = Ui::CreateChild<Ui::RpWidget>(row);
		av->setFixedSize(kAvSize, kAvSize);
		const auto userpicView =
			std::make_shared<Ui::PeerUserpicView>();
		const auto session = &controller->session();
		av->paintRequest() | rpl::on_next([=](QRect) {
			Painter p(av);
			PaintPeerAvatar(
				p,
				QRect(0, 0, kAvSize, kAvSize),
				peerId,
				name,
				session,
				*userpicView);
		}, av->lifetime());

		// Name label
		const auto nameLabel = Ui::CreateChild<Ui::FlatLabel>(
			row,
			rpl::single(name.isEmpty() ? peerId : name),
			st::boxLabel);

		// Peer ID label
		const auto idLabel = Ui::CreateChild<Ui::FlatLabel>(
			row,
			rpl::single(u"ID: "_q + peerId),
			st::customModHintLabel);

		// Delete button
		const auto delBtn = Ui::CreateChild<Ui::RoundButton>(
			row,
			rpl::single(u"O'chirish"_q),
			st::attentionBoxButton);
		delBtn->setFixedWidth(kDelBtnW);
		// Barcha child widgetlar uchun show() majburiy — Qt yangi child larni
		// parent visible bo'lganda avtomatik ko'rsatmaydi.
		av->show();
		nameLabel->show();
		idLabel->show();
		delBtn->show();

		// Bottom separator
		row->paintRequest() | rpl::on_next([=](QRect) {
			Painter p(row);
			p.fillRect(kPadL + kAvSize + kGap, kRowH - 1,
				row->width() - kPadL - kAvSize - kGap - kPadR, 1,
				st::shadowFg->c);
		}, row->lifetime());

		// Layout children — alohida lambda, darhol ham chaqiriladi.
		const auto layoutRow = [=](int w) {
			const auto avY = (kRowH - kAvSize) / 2;
			av->move(kPadL, avY);
			av->update();
			const auto textX = kPadL + kAvSize + kGap;
			const auto textW = w - textX - kGap - kDelBtnW - kPadR;
			if (textW <= 0) return;
			nameLabel->resizeToWidth(textW);
			nameLabel->move(textX, 10);
			nameLabel->update();
			idLabel->resizeToWidth(textW);
			idLabel->move(textX, 10 + nameLabel->height() + 2);
			idLabel->update();
			const auto btnY = (kRowH - st::defaultBoxButton.height) / 2;
			delBtn->move(w - kPadR - kDelBtnW, btnY);
		};
		row->widthValue() | rpl::on_next(layoutRow, row->lifetime());
		// w=0 bo'lganda subscription ishlamaydi — parent width bilan darhol chaqirish.
		const auto initW = state->entriesLayout->width();
		if (initW > 0) layoutRow(initW);

		delBtn->addClickHandler([=] {
			delBtn->setDisabled(true);
			if (isWhitelist) {
				CustomSettings::RemoveFromWhitelist(peerId);
			} else {
				CustomSettings::RemoveFromBlocklist(peerId);
			}
			entryWrap->toggle(false, anim::type::normal);
			--state->visibleCount;
			updateHeader(true);
			Ui::Toast::Show(u"Ro'yxatdan o'chirildi."_q);
		});

		// SlideWrap default _toggled=false (yashirin, height=0) — darhol ko'rsatish.
		entryWrap->toggle(true, anim::type::instant);

		++state->visibleCount;
		updateHeader(false);

		// entriesLayout va content ni qayta o'lchamlab, yangi row ko'rinsin.
		// update() — Qt faqat yangi exposed area ni qayta chizadi, shu sababli
		// mavjud widget lar uchun ham update() majburan chaqiriladi.
		if (state->entriesLayout->width() > 0) {
			state->entriesLayout->resizeToWidth(
				state->entriesLayout->width());
			state->entriesLayout->update();
		}
		if (content->width() > 0) {
			content->resizeToWidth(content->width());
			content->update();
		}
	};

	// ── Load saved peers ─────────────────────────────────────────
	updateHeader(false);
	{
		const auto entries = isWhitelist
			? CustomSettings::GetWhitelist()
			: CustomSettings::GetBlocklist();
		for (const auto &e : entries) {
			state->addEntry(e.first, e.second);
		}
	}

	Ui::AddSkip(content, st::settingsThumbSkip);
}

// ── NEXT-6: Per-Chat Settings paneli ─────────────────────────────────────
// Har bir chat uchun Ghost / AntiDelete / AntiEdit ni alohida boshqarish.
// Priority: Blocklist > Whitelist > Per-peer override > Global flag
void fillPerChatSection(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller) {
	struct State {
		Ui::VerticalLayout *entriesLayout = nullptr;
		Ui::SlideWrap<Ui::FlatLabel> *emptyWrap = nullptr;
		Ui::FlatLabel *countLabel = nullptr;
		int visibleCount = 0;
		QVector<Ui::SlideWrap<Ui::VerticalLayout>*> entryWraps;
		Fn<void(const CustomSettings::PerPeerEntry &)> addEntry;
	};
	const auto state = content->lifetime().make_state<State>();

	// ── Header ────────────────────────────────────────────────────
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"⚙️ Individual sozlamalar (istisnolar)"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	{
		const auto desc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Alohida chatlar uchun individual sozlamalar (White/Black List ustunroq)."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
		content->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				desc->resizeToWidth(lw);
				desc->update();
			}
		}, desc->lifetime());
	}

	// ── Chat tanlash tugmasi ──────────────────────────────────────
	Ui::AddSkip(content, 12);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"Chat tanlash — Per-Chat"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (!gInstance) return;
		gInstance->showBox(ChoosePeerBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				const auto peerId = QString::number(peer->id.value);
				const auto name = peer->name();
				if (CustomSettings::HasPerPeerOverride(peerId)) {
					Ui::Toast::Show(u"Bu chat allaqachon ro'yxatda."_q);
					return true;
				}
				CustomSettings::AddPerPeerOverride(peerId, name);
				const auto entries = CustomSettings::GetPerPeerOverrides();
				for (const auto &e : entries) {
					if (e.peerId == peerId) {
						state->addEntry(e);
						break;
					}
				}
				Ui::Toast::Show(name + u" qo'shildi."_q);
				return true;
			},
			rpl::single(u"Per-Chat sozlash uchun chat tanlang"_q)));
	});

	// ── Count label + empty wrap ──────────────────────────────────
	Ui::AddSkip(content, 16);
	state->countLabel = content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString()),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	const auto updateHeader = [=](bool animated) {
		const auto n = state->visibleCount;
		state->countLabel->setText(n == 0
			? u"Per-Chat — bo'sh"_q
			: u"Per-Chat — %1 ta chat"_q.arg(n));
		state->emptyWrap->toggle(
			n == 0,
			animated ? anim::type::normal : anim::type::instant);
	};

	// ── Barchasini tozalash (NEXT-9) ──────────────────────────────
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"Barchasini tozalash"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (state->visibleCount == 0) {
			Ui::Toast::Show(u"Ro'yxat allaqachon bo'sh."_q);
			return;
		}
		CustomSettings::ClearAllPerPeerOverrides();
		for (auto *wrap : state->entryWraps) {
			wrap->toggle(false, anim::type::normal);
		}
		state->visibleCount = 0;
		updateHeader(true);
		Ui::Toast::Show(u"Per-Chat ro'yxati tozalandi."_q);
	});

	state->entriesLayout = content->add(
		object_ptr<Ui::VerticalLayout>(content),
		{0, 0, 0, 0});

	state->emptyWrap = state->entriesLayout->add(
		object_ptr<Ui::SlideWrap<Ui::FlatLabel>>(
			state->entriesLayout,
			object_ptr<Ui::FlatLabel>(
				state->entriesLayout,
				rpl::single(u"Per-Chat sozlash uchun yuqoridan chat qo'shing."_q),
				st::customModHintLabel)),
		st::defaultSubsectionTitlePadding,
		style::al_justify);

	// ── Entry row builder ─────────────────────────────────────────
	// Har bir entry — VerticalLayout container ichida:
	//   header row (avatar + name + ID + delete)
	//   Ghost toggle
	//   Delete toggle
	//   Edit toggle
	state->addEntry = [=](const CustomSettings::PerPeerEntry &entry) {
		constexpr int kHeaderH = 56;
		constexpr int kAvSize  = 38;
		[[maybe_unused]] constexpr int kPadL = 14;
		[[maybe_unused]] constexpr int kPadR = 12;
		[[maybe_unused]] constexpr int kGap  = 12;
		constexpr int kDelBtnW = 76;

		const auto peerId = entry.peerId;
		const auto name = entry.displayName;

		// SlideWrap ichida — VerticalLayout container
		const auto entryWrap = state->entriesLayout->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				state->entriesLayout,
				object_ptr<Ui::VerticalLayout>(state->entriesLayout)),
			style::margins(),
			style::al_justify);
		state->entryWraps.append(entryWrap);
		const auto container = entryWrap->entity();

		// ── Header row (avatar + name + ID + delete) ──────────
		const auto header = container->add(
			object_ptr<Ui::RpWidget>(container),
			style::margins());
		header->setFixedHeight(kHeaderH);

		const auto av = Ui::CreateChild<Ui::RpWidget>(header);
		av->setFixedSize(kAvSize, kAvSize);
		const auto userpicView =
			std::make_shared<Ui::PeerUserpicView>();
		const auto session = &controller->session();
		av->paintRequest() | rpl::on_next([=](QRect) {
			Painter p(av);
			PaintPeerAvatar(
				p,
				QRect(0, 0, kAvSize, kAvSize),
				peerId,
				name,
				session,
				*userpicView);
		}, av->lifetime());

		const auto nameLabel = Ui::CreateChild<Ui::FlatLabel>(
			header,
			rpl::single(name.isEmpty() ? peerId : name),
			st::boxLabel);

		const auto idLabel = Ui::CreateChild<Ui::FlatLabel>(
			header,
			rpl::single(u"ID: "_q + peerId),
			st::customModHintLabel);

		const auto rmBtn = Ui::CreateChild<Ui::RoundButton>(
			header,
			rpl::single(u"O'chirish"_q),
			st::attentionBoxButton);
		rmBtn->setFixedWidth(kDelBtnW);

		av->show();
		nameLabel->show();
		idLabel->show();
		rmBtn->show();

		header->paintRequest() | rpl::on_next([=](QRect) {
			Painter p(header);
			p.fillRect(kPadL + kAvSize + kGap, kHeaderH - 1,
				header->width() - kPadL - kAvSize - kGap - kPadR, 1,
				st::shadowFg->c);
		}, header->lifetime());

		const auto layoutHeader = [=](int w) {
			av->move(kPadL, (kHeaderH - kAvSize) / 2);
			av->update();
			const auto textX = kPadL + kAvSize + kGap;
			const auto textW = w - textX - kGap - kDelBtnW - kPadR;
			if (textW <= 0) return;
			nameLabel->resizeToWidth(textW);
			nameLabel->move(textX, 10);
			nameLabel->update();
			idLabel->resizeToWidth(textW);
			idLabel->move(textX, 10 + nameLabel->height() + 2);
			idLabel->update();
			const auto btnY = (kHeaderH - st::defaultBoxButton.height) / 2;
			rmBtn->move(w - kPadR - kDelBtnW, btnY);
		};
		header->widthValue() | rpl::on_next(layoutHeader, header->lifetime());
		const auto initW = state->entriesLayout->width();
		if (initW > 0) layoutHeader(initW);

		// ── 3 ta toggle (Ghost / Delete / Edit) ──────────────
		const auto addToggle = [&](
				const QString &label,
				bool initial,
				Fn<void(bool)> onChange) {
			const auto btn = container->add(
				object_ptr<Ui::SettingsButton>(
					container,
					rpl::single(label),
					st::settingsButtonNoIcon));
			btn->toggleOn(rpl::single(initial));
			btn->toggledValue() | rpl::skip(1) | rpl::on_next([=](bool on) {
				onChange(on);
			}, btn->lifetime());
		};
		addToggle(u"Ghost Mode"_q, entry.ghostEnabled,
			[=](bool on) { CustomSettings::SetGhostModeForPeer(peerId, on); });
		addToggle(u"Anti-Delete"_q, entry.antiDeleteEnabled,
			[=](bool on) { CustomSettings::SetAntiDeleteForPeer(peerId, on); });
		addToggle(u"Anti-Edit"_q, entry.antiEditEnabled,
			[=](bool on) { CustomSettings::SetAntiEditForPeer(peerId, on); });
		// 2026-08-14: katta media'ni oldindan yuklab olish. Boshqa uchtadan
		// FARQLI — global bayrog'i yo'q, faqat shu toggle yoki White List
		// uni yoqadi (ShouldMediaBackup izohiga qarang).
		addToggle(u"Media Backup"_q, entry.mediaBackupEnabled,
			[=](bool on) { CustomSettings::SetMediaBackupForPeer(peerId, on); });

		rmBtn->addClickHandler([=] {
			rmBtn->setDisabled(true);
			CustomSettings::RemovePerPeerOverride(peerId);
			entryWrap->toggle(false, anim::type::normal);
			--state->visibleCount;
			updateHeader(true);
			Ui::Toast::Show(u"Per-Chat dan o'chirildi."_q);
		});

		entryWrap->toggle(true, anim::type::instant);
		++state->visibleCount;
		updateHeader(false);

		if (state->entriesLayout->width() > 0) {
			state->entriesLayout->resizeToWidth(
				state->entriesLayout->width());
			state->entriesLayout->update();
		}
		if (content->width() > 0) {
			content->resizeToWidth(content->width());
			content->update();
		}
	};

	// ── Load saved overrides ──────────────────────────────────────
	updateHeader(false);
	{
		const auto entries = CustomSettings::GetPerPeerOverrides();
		for (const auto &e : entries) {
			state->addEntry(e);
		}
	}
}

// ── Activity History Log: kuzatish qamrovi + kuzatilayotganlar ro'yxati ──



void fillChatsTab(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		Fn<void()> onRebuild) {
	Ui::AddSkip(content, st::settingsThumbSkip);
	{
		const auto lbl = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"AntiDelete / AntiEdit uchun global rejim:"_q),
				st::customModHintLabel),
			st::boxRowPadding);
		content->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w - st::boxRowPadding.left() - st::boxRowPadding.right();
			if (lw > 0) { lbl->resizeToWidth(lw); lbl->update(); }
		}, lbl->lifetime());
	}

	const auto modeWrap = content->add(
		object_ptr<Ui::VerticalLayout>(content));
	const auto modeGroup = std::make_shared<Ui::RadiobuttonGroup>(
		int(CustomSettings::GetPeerListMode()));

	const auto rAll = modeWrap->add(
		object_ptr<Ui::Radiobutton>(
			modeWrap,
			modeGroup,
			int(CustomSettings::PeerListMode::All),
			u"Barcha chatlar (standart)"_q),
		st::boxRowPadding);
	const auto rWhite = modeWrap->add(
		object_ptr<Ui::Radiobutton>(
			modeWrap,
			modeGroup,
			int(CustomSettings::PeerListMode::WhiteList),
			u"Faqat White List chatlari"_q),
		st::boxRowPadding);
	const auto rBlack = modeWrap->add(
		object_ptr<Ui::Radiobutton>(
			modeWrap,
			modeGroup,
			int(CustomSettings::PeerListMode::BlackList),
			u"Black List'dan tashqari barcha chatlar"_q),
		st::boxRowPadding);

	modeGroup->setChangedCallback([=](int value) {
		CustomSettings::SetPeerListMode(
			static_cast<CustomSettings::PeerListMode>(value));
		Ui::Toast::Show(u"Rejim saqlandi ✓"_q);
	});

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);

	fillPeerSection(
		content,
		controller,
		u"White List (Ruxsat berilganlar)"_q,
		u"Faqat shu chatlarda AntiDelete/AntiEdit ishlaydi."_q,
		true,
		CustomSettings::IsInWhiteList,
		CustomSettings::AddToWhiteList,
		CustomSettings::RemoveFromWhiteList,
		CustomSettings::GetWhiteList,
		[=](const QString &id) {
			CustomSettings::RemoveFromBlackList(id);
			Ui::Toast::Show(u"Black List'dan olib tashlandi."_q);
			if (onRebuild) onRebuild();
		},
		CustomSettings::IsInBlackList,
		u"Black List");

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);

	fillPeerSection(
		content,
		controller,
		u"Black List (Taqiqlanganlar)"_q,
		u"Bu chatlarda AntiDelete/AntiEdit ISHLAMAYDI."_q,
		false,
		CustomSettings::IsInBlackList,
		CustomSettings::AddToBlackList,
		CustomSettings::RemoveFromBlackList,
		CustomSettings::GetBlackList,
		[=](const QString &id) {
			CustomSettings::RemoveFromWhiteList(id);
			Ui::Toast::Show(u"White List'dan olib tashlandi."_q);
			if (onRebuild) onRebuild();
		},
		CustomSettings::IsInWhiteList,
		u"White List");

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	fillPerChatSection(content, controller);
	Ui::AddSkip(content, st::settingsThumbSkip);
}
