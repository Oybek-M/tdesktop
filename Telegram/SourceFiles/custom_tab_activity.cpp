#include "custom_tab_common.h"

void fillActivityTab(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		Fn<void()> onRebuild) {
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"🕒 Activity History"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	{
		const auto desc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Kontaktlarning ism, username, rasm va "
					"last-seen o'zgarishlarini vaqt bilan saqlaydi — faqat "
					"ilova legal ravishda qabul qilgan ma'lumot, hech qanday "
					"maxfiylik cheklovi aylanib o'tilmaydi."_q),
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

	// ── Global toggle ─────────────────────────────────────────────
	Ui::AddSkip(content, 8);
	{
		const auto btn = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(u"Barcha Contact'larni kuzatish"_q),
				st::settingsButtonNoIcon));
		btn->toggleOn(rpl::single(
			CustomSettings::ActivityHistoryTrackAllContacts()));
		btn->toggledValue()
			| rpl::skip(1)
			| rpl::on_next([=](bool on) {
				CustomSettings::Set(u"activityHistoryTrackAllContacts"_q, on);
				Ui::Toast::Show(on
					? u"Barcha kontaktlarni kuzatish yoqildi ✓"_q
					: u"Barcha kontaktlarni kuzatish o'chirildi"_q);
			}, btn->lifetime());
	}

	// ── Vaqtinchalik bufer (daqiqa) ──────────────────────────────
	Ui::AddSkip(content, 12);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Vaqtinchalik bufer (daqiqa)"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Kuzatilmayotgan odamlarning faolligi shu muddat davomida xotirada saqlanadi. Odamni kuzatuvga qo'shsangiz, shu davrdagi yozuvlar tiklanadi."_q),
			st::customModHintLabel),
		st::boxRowPadding);

	struct BufferPreset { QString label; int minutes; };
	const BufferPreset kBufferPresets[4] = {
		{ u"5 daqiqa"_q, 5 },
		{ u"10 daqiqa"_q, 10 },
		{ u"30 daqiqa"_q, 30 },
		{ u"60 daqiqa"_q, 60 },
	};
	const auto isBufferPreset = [](int minutes) {
		return minutes == 5 || minutes == 10 || minutes == 30 || minutes == 60;
	};
	const auto selectedBufferMinutes = std::make_shared<rpl::variable<int>>(
		CustomSettings::ActivityBufferMinutes());

	const auto customBufferWrap = content->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			content,
			object_ptr<Ui::VerticalLayout>(content)),
		style::margins(0, 0, 0, 0));
	const auto customBufferForm = customBufferWrap->entity();
	const auto customBufferInput = customBufferForm->add(
		object_ptr<Ui::InputField>(
			customBufferForm,
			st::defaultInputField,
			rpl::single(u"Daqiqa (1 – 120)"_q),
			QString::number(CustomSettings::ActivityBufferMinutes())),
		st::boxRowPadding);
	customBufferForm->add(
		object_ptr<Ui::RoundButton>(
			customBufferForm,
			rpl::single(u"💾 Saqlash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		bool ok = false;
		const auto parsed = customBufferInput->getLastText().trimmed().toInt(&ok);
		const auto clamped = std::clamp(ok ? parsed : 10, 1, 120);
		CustomSettings::SetInt(u"activityBufferMinutes"_q, clamped);
		*selectedBufferMinutes = clamped;
		Ui::Toast::Show(
			u"Bufer muddati saqlandi: "_q + QString::number(clamped) + u" daqiqa"_q);
	});
	customBufferWrap->toggle(
		!isBufferPreset(CustomSettings::ActivityBufferMinutes()),
		anim::type::instant);

	for (const auto &preset : kBufferPresets) {
		const auto minutes = preset.minutes;
		const auto label = preset.label;
		content->add(
			object_ptr<Ui::RoundButton>(
				content,
				selectedBufferMinutes->value() | rpl::map([=](int current) {
					return (current == minutes)
						? (u"✓ "_q + label)
						: label;
				}),
				st::defaultBoxButton),
			st::boxRowPadding)
		->addClickHandler([=] {
			CustomSettings::SetInt(u"activityBufferMinutes"_q, minutes);
			*selectedBufferMinutes = minutes;
			customBufferWrap->toggle(false, anim::type::normal);
			Ui::Toast::Show(u"Bufer muddati saqlandi."_q);
		});
	}
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"Boshqa..."_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		customBufferWrap->toggle(
			!customBufferWrap->toggled(),
			anim::type::normal);
	});

	// ── Include List ──────────────────────────────────────────────
	Ui::AddSkip(content, 12);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Include List — standart holatdan qat'iy nazar "
				"har doim kuzatiladi:"_q),
			st::customModHintLabel),
		st::boxRowPadding);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"Chat tanlash — Include"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (!gInstance) return;
		gInstance->showBox(ChoosePeerBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				if (!peer->isUser()) {
					Ui::Toast::Show(
						u"Faqat shaxsiy chatlar (User) kuzatiladi."_q);
					return true;
				}
				const auto peerId = QString::number(peer->id.value);
				const auto name = peer->name();
				if (CustomSettings::IsInActivityInclude(peerId)) {
					Ui::Toast::Show(u"Bu chat allaqachon Include List'da."_q);
					return true;
				}
				CustomSettings::AddToActivityInclude(peerId, name);
				Ui::Toast::Show(name + u" Include List'ga qo'shildi."_q);
				if (onRebuild) onRebuild();
				return true;
			},
			rpl::single(u"Include List'ga qo'shish"_q)));
	});
	for (const auto &e : CustomSettings::GetActivityInclude()) {
		const auto peerId = e.first;
		const auto name = e.second;
		AddAvatarPeerRow(content, controller, peerId, name, [=] {
			CustomSettings::RemoveFromActivityInclude(peerId);
			Ui::Toast::Show(name + u" Include List'dan olib tashlandi."_q);
			if (onRebuild) onRebuild();
		});
		const auto historyRow = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(u"📜 Tarixni ko'rish — "_q + name),
				st::settingsButtonNoIcon));
		historyRow->addClickHandler([=] {
			if (!gInstance) return;
			gInstance->showBox(CustomActivityHistory::MakeHistoryBox(
				&controller->session(), peerId, name));
		});
	}

	// ── Qo'lda faollik yozuvi qo'shish (A16 §2) ───────────────────
	Ui::AddSkip(content, 12);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"✍️ Qo'lda faollik yozuvi qo'shish"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (!gInstance) return;
		gInstance->showBox(ChoosePeerBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				if (!peer->isUser()) {
					Ui::Toast::Show(
						u"Faqat shaxsiy chatlar (User) uchun faollik kiritiladi."_q);
					return true;
				}
				const auto peerId = QString::number(peer->id.value);
				const auto name = peer->name();

				if (!gInstance) return true;
				gInstance->showBox(Box([=](not_null<Ui::GenericBox*> box) {
					box->setTitle(rpl::single(u"Qo'lda faollik yozish — "_q + name));
					const auto form = box->verticalLayout();

					form->add(
						object_ptr<Ui::FlatLabel>(
							form,
							rpl::single(u"Sana va vaqt (dd.MM.yyyy HH:mm):"_q),
							st::defaultSubsectionTitle),
						st::defaultSubsectionTitlePadding);

					const auto nowStr = QDateTime::currentDateTime().toString(u"dd.MM.yyyy HH:mm"_q);
					const auto timeInput = form->add(
						object_ptr<Ui::InputField>(
							form,
							st::defaultInputField,
							rpl::single(u"dd.MM.yyyy HH:mm"_q),
							nowStr),
						st::boxRowPadding);

					form->add(
						object_ptr<Ui::FlatLabel>(
							form,
							rpl::single(u"Davomiyligi (soniya, 0 = faqat online):"_q),
							st::defaultSubsectionTitle),
						st::defaultSubsectionTitlePadding);

					const auto durationInput = form->add(
						object_ptr<Ui::InputField>(
							form,
							st::defaultInputField,
							rpl::single(u"Soniya (standart 30)"_q),
							u"30"_q),
						st::boxRowPadding);

					box->addButton(rpl::single(u"Saqlash"_q), [=] {
						const auto dt = QDateTime::fromString(timeInput->getLastText().trimmed(), u"dd.MM.yyyy HH:mm"_q);
						if (!dt.isValid()) {
							Ui::Toast::Show(u"Noto'g'ri sana formati! Masalan: 28.08.2026 17:03"_q);
							return;
						}
						bool ok = false;
						const auto duration = durationInput->getLastText().trimmed().toInt(&ok);
						const auto durationSecs = ok ? std::max(0, duration) : 30;

						const auto on = dt.toSecsSinceEpoch();
						const auto off = on + durationSecs;
						const auto accountId = qint64(controller->session().userId().bare);

						int added = 0;
						if (!CustomDB::HasActivityEntryAt(peerId, u"status"_q, on)) {
							CustomDB::SaveActivityHistoryEntry(
								CustomDB::PeerKey{ accountId, peerId },
								u"status"_q,
								false,
								QString(),
								u"online:"_q + QString::number(on),
								on,
								u"manual"_q);
							++added;
						}
						if (durationSecs > 0 && !CustomDB::HasActivityEntryAt(peerId, u"status"_q, off)) {
							CustomDB::SaveActivityHistoryEntry(
								CustomDB::PeerKey{ accountId, peerId },
								u"status"_q,
								false,
								QString(),
								u"offline:"_q + QString::number(on),
								off,
								u"manual"_q);
							++added;
						}

						box->closeBox();
						if (added > 0) {
							Ui::Toast::Show(u"Faollik yozuvi saqlandi ✓"_q);
						} else {
							Ui::Toast::Show(u"Bu vaqt uchun yozuv allaqachon mavjud."_q);
						}
					});
					box->addButton(
						rpl::single(u"Bekor qilish"_q),
						[=] { box->closeBox(); });
				}));

				return true;
			},
			rpl::single(u"Qo'lda yozuv — chat tanlash"_q)));
	});

	// ── Exclude List ──────────────────────────────────────────────
	Ui::AddSkip(content, 12);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Exclude List — hech qachon kuzatilmaydi:"_q),
			st::customModHintLabel),
		st::boxRowPadding);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"Chat tanlash — Exclude"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (!gInstance) return;
		gInstance->showBox(ChoosePeerBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				if (!peer->isUser()) {
					Ui::Toast::Show(
						u"Faqat shaxsiy chatlar (User) kuzatiladi."_q);
					return true;
				}
				const auto peerId = QString::number(peer->id.value);
				const auto name = peer->name();
				if (CustomSettings::IsInActivityExclude(peerId)) {
					Ui::Toast::Show(u"Bu chat allaqachon Exclude List'da."_q);
					return true;
				}
				CustomSettings::AddToActivityExclude(peerId, name);
				Ui::Toast::Show(name + u" Exclude List'ga qo'shildi."_q);
				if (onRebuild) onRebuild();
				return true;
			},
			rpl::single(u"Exclude List'ga qo'shish"_q)));
	});
	for (const auto &e : CustomSettings::GetActivityExclude()) {
		const auto peerId = e.first;
		const auto name = e.second;
		AddAvatarPeerRow(content, controller, peerId, name, [=] {
			CustomSettings::RemoveFromActivityExclude(peerId);
			Ui::Toast::Show(name + u" Exclude List'dan olib tashlandi."_q);
			if (onRebuild) onRebuild();
		});
		const auto historyRow = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(u"📜 Tarixni ko'rish — "_q + name),
				st::settingsButtonNoIcon));
		historyRow->addClickHandler([=] {
			if (!gInstance) return;
			gInstance->showBox(CustomActivityHistory::MakeHistoryBox(
				&controller->session(), peerId, name));
		});
	}

	Ui::AddSkip(content, st::settingsThumbSkip);
}

