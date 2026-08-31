#include "custom_tab_common.h"

void fillActivityTab(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		Fn<void()> onRebuild) {
	// 1-bo'lim: Faollik tarixi va sozlamalari (Standart ochiq)
	const auto s1 = AddCollapsibleSection(content, u"⏱️ Faollik tarixi va sozlamalari"_q, true);

	{
		const auto desc = s1->add(
			object_ptr<Ui::FlatLabel>(
				s1,
				rpl::single(u"Kontaktlarning ism, rasm va last-seen o'zgarishlarini vaqt bilan saqlaydi."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
		s1->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				desc->resizeToWidth(lw);
				desc->update();
			}
		}, desc->lifetime());
	}

	// Global toggle
	Ui::AddSkip(s1, 8);
	{
		const auto btn = s1->add(
			object_ptr<Ui::SettingsButton>(
				s1,
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

	// Vaqtinchalik bufer (daqiqa)
	Ui::AddSkip(s1, 12);
	s1->add(
		object_ptr<Ui::FlatLabel>(
			s1,
			rpl::single(u"Vaqtinchalik bufer (daqiqa)"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	s1->add(
		object_ptr<Ui::FlatLabel>(
			s1,
			rpl::single(u"Kuzatuvga qo'shilganda o'tgan muddatdagi faollik yozuvlarini tiklaydi."_q),
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

	const auto customBufferWrap = s1->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			s1,
			object_ptr<Ui::VerticalLayout>(s1)),
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
		s1->add(
			object_ptr<Ui::RoundButton>(
				s1,
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
	s1->add(
		object_ptr<Ui::RoundButton>(
			s1,
			rpl::single(u"Boshqa..."_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		customBufferWrap->toggle(
			!customBufferWrap->toggled(),
			anim::type::normal);
	});

	// Qo'lda yozuv kiritish
	Ui::AddSkip(s1, 12);
	s1->add(
		object_ptr<Ui::FlatLabel>(
			s1,
			rpl::single(u"✍️ Qo'lda faollik yozuvi kiritish"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	s1->add(
		object_ptr<Ui::FlatLabel>(
			s1,
			rpl::single(u"Oflayn/tashqi manbadan bilgan faollik vaqtingizni qo'lda kiritish."_q),
			st::customModHintLabel),
		st::boxRowPadding);
	s1->add(
		object_ptr<Ui::RoundButton>(
			s1,
			rpl::single(u"Qo'lda yozuv kiritish..."_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		ShowCustomBox(ChoosePeerBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				if (!peer->isUser()) {
					Ui::Toast::Show(u"Faqat shaxsiy chatlar (User) uchun kiritish mumkin."_q);
					return true;
				}
				const auto peerId = QString::number(peer->id.value);
				const auto name = peer->name();

				ShowCustomBox(Box([=](not_null<Ui::GenericBox*> box) {
					box->setTitle(rpl::single(u"Qo'lda yozuv — "_q + name));
					Ui::AddSkip(box->verticalLayout(), 8);
					box->verticalLayout()->add(
						object_ptr<Ui::FlatLabel>(
							box->verticalLayout(),
							rpl::single(u"Online bo'lgan vaqti (dd.MM.yyyy HH:mm):"_q),
							st::defaultSubsectionTitle),
						st::defaultSubsectionTitlePadding);
					const auto nowStr = QDateTime::currentDateTime().toString(u"dd.MM.yyyy HH:mm"_q);
					const auto timeInput = box->verticalLayout()->add(
						object_ptr<Ui::InputField>(
							box->verticalLayout(),
							st::defaultInputField,
							rpl::single(u"dd.MM.yyyy HH:mm"_q),
							nowStr),
						st::boxRowPadding);

					box->verticalLayout()->add(
						object_ptr<Ui::FlatLabel>(
							box->verticalLayout(),
							rpl::single(u"Davomiyligi (sekund, 0 = aniq lahza):"_q),
							st::defaultSubsectionTitle),
						st::defaultSubsectionTitlePadding);
					const auto durationInput = box->verticalLayout()->add(
						object_ptr<Ui::InputField>(
							box->verticalLayout(),
							st::defaultInputField,
							rpl::single(u"Sekund (masalan: 30)"_q),
							u"30"_q),
						st::boxRowPadding);

					box->addButton(rpl::single(u"Saqlash"_q), [=] {
						const auto text = timeInput->getLastText().trimmed();
						const auto dt = QDateTime::fromString(text, u"dd.MM.yyyy HH:mm"_q);
						if (!dt.isValid()) {
							Ui::Toast::Show(u"Vaqt formati noto'g'ri (dd.MM.yyyy HH:mm bo'lishi kerak)."_q);
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

	// 2-bo'lim: Include List (Standart yopiq)
	const auto s2 = AddCollapsibleSection(content, u"📋 Include List (Majburiy kuzatuv)"_q, false);
	s2->add(
		object_ptr<Ui::FlatLabel>(
			s2,
			rpl::single(u"Global toggle o'chiq bo'lsa ham, bu odamlar doimiy kuzatiladi:"_q),
			st::customModHintLabel),
		st::boxRowPadding);
	s2->add(
		object_ptr<Ui::RoundButton>(
			s2,
			rpl::single(u"Chat tanlash — Include"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		ShowCustomBox(ChoosePeerBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				if (!peer->isUser()) {
					Ui::Toast::Show(u"Faqat shaxsiy chatlar (User) kuzatiladi."_q);
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
		AddAvatarPeerRow(s2, controller, peerId, name, [=] {
			CustomSettings::RemoveFromActivityInclude(peerId);
			Ui::Toast::Show(name + u" Include List'dan olib tashlandi."_q);
			if (onRebuild) onRebuild();
		});
		const auto historyRow = s2->add(
			object_ptr<Ui::SettingsButton>(
				s2,
				rpl::single(u"📜 Tarixni ko'rish — "_q + name),
				st::settingsButtonNoIcon));
		historyRow->addClickHandler([=] {
			ShowCustomBox(CustomActivityHistory::MakeHistoryBox(
				&controller->session(), peerId, name));
		});
	}

	// 3-bo'lim: Exclude List (Standart yopiq)
	const auto s3 = AddCollapsibleSection(content, u"🚫 Exclude List (Kuzatilmaydiganlar)"_q, false);
	s3->add(
		object_ptr<Ui::FlatLabel>(
			s3,
			rpl::single(u"Barcha contactlar yoqilgan bo'lsa ham, bu odamlar hech qachon kuzatilmaydi:"_q),
			st::customModHintLabel),
		st::boxRowPadding);
	s3->add(
		object_ptr<Ui::RoundButton>(
			s3,
			rpl::single(u"Chat tanlash — Exclude"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		ShowCustomBox(ChoosePeerBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				if (!peer->isUser()) {
					Ui::Toast::Show(u"Faqat shaxsiy chatlar (User) kuzatiladi."_q);
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
		AddAvatarPeerRow(s3, controller, peerId, name, [=] {
			CustomSettings::RemoveFromActivityExclude(peerId);
			Ui::Toast::Show(name + u" Exclude List'dan olib tashlandi."_q);
			if (onRebuild) onRebuild();
		});
		const auto historyRow = s3->add(
			object_ptr<Ui::SettingsButton>(
				s3,
				rpl::single(u"📜 Tarixni ko'rish — "_q + name),
				st::settingsButtonNoIcon));
		historyRow->addClickHandler([=] {
			ShowCustomBox(CustomActivityHistory::MakeHistoryBox(
				&controller->session(), peerId, name));
		});
	}

	Ui::AddSkip(content, st::settingsThumbSkip);
}
