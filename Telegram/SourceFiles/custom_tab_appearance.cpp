#include "custom_tab_common.h"

void fillAppearanceTab(not_null<Ui::VerticalLayout*> content) {
	const auto addToggle = [&](
			not_null<Ui::VerticalLayout*> parent,
			const QString &id,
			const QString &text,
			const QString &description) {
		const auto &val = CustomSettings::Get();
		auto current = false;
		if (id == u"spoofMobile"_q) current = val.spoofMobile;
		else if (id == u"mutualContactShowInChatList"_q) current = val.mutualContactShowInChatList;
		else if (id == u"mutualContactShowInContactsList"_q) current = val.mutualContactShowInContactsList;
		else if (id == u"mutualContactShowInProfile"_q) current = val.mutualContactShowInProfile;
		else if (id == u"mutualContactShowInMembersList"_q) current = val.mutualContactShowInMembersList;

		const auto btn = parent->add(
			object_ptr<Ui::SettingsButton>(
				parent,
				rpl::single(text),
				st::settingsButtonNoIcon));
		btn->toggleOn(rpl::single(current));

		btn->toggledValue()
			| rpl::skip(1)
			| rpl::on_next([=](bool on) {
			CustomSettings::Set(id, on);
			Ui::Toast::Show(
				on ? (text + u" yoqildi ✓"_q)
				   : (text + u" o'chirildi"_q));
		}, btn->lifetime());

		if (!description.isEmpty()) {
			const auto descLabel = parent->add(
				object_ptr<Ui::FlatLabel>(
					parent,
					rpl::single(description),
					st::customModHintLabel),
				st::boxRowPadding,
				style::al_justify);
			parent->widthValue() | rpl::on_next([=](int w) {
				const auto lw = w
					- st::boxRowPadding.left()
					- st::boxRowPadding.right();
				if (lw > 0) {
					descLabel->resizeToWidth(lw);
					descLabel->update();
				}
			}, descLabel->lifetime());
		}
		return btn;
	};

	// 1-bo'lim: Qurilma ko'rinishi (Standart ochiq)
	const auto s1 = AddCollapsibleSection(content, u"📱 Qurilma ko'rinishini almashtirish"_q, true);

	const auto spoofToggleBtn = addToggle(
		s1,
		u"spoofMobile"_q,
		u"Mobil qurilma ko'rinishi"_q,
		u"Telegram mobil ilovadan ishlatilayotgandek ko'rinadi."_q);

	Ui::AddSkip(s1, 8);

	const auto spoofFormWrap = s1->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			s1,
			object_ptr<Ui::VerticalLayout>(s1)),
		style::margins(0, 0, 0, 0));
	const auto spoofForm = spoofFormWrap->entity();

	spoofForm->add(
		object_ptr<Ui::FlatLabel>(
			spoofForm,
			rpl::single(u"Qurilma nomi:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto spoofModelInput = spoofForm->add(
		object_ptr<Ui::InputField>(
			spoofForm,
			st::defaultInputField,
			rpl::single(u"Qurilma nomi"_q),
			CustomSettings::SpoofDeviceModel()),
		st::boxRowPadding);

	spoofForm->add(
		object_ptr<Ui::FlatLabel>(
			spoofForm,
			rpl::single(u"Tizim versiyasi:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto spoofVersionInput = spoofForm->add(
		object_ptr<Ui::InputField>(
			spoofForm,
			st::defaultInputField,
			rpl::single(u"Tizim versiyasi"_q),
			CustomSettings::SpoofSystemVersion()),
		st::boxRowPadding);

	spoofForm->add(
		object_ptr<Ui::FlatLabel>(
			spoofForm,
			rpl::single(u"Qurilma turi (icon):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	{
		struct Preset { QString label, model, version; };
		const Preset kPresets[4] = {
			{ u"Android"_q,  u"Samsung Galaxy S26 Ultra"_q, u"Android 15"_q },
			{ u"iOS"_q,      u"iPhone 17 Pro Max"_q,        u"iOS 18"_q     },
			{ u"Windows"_q,  u"PC"_q,                       u"Windows 11"_q },
			{ u"Linux"_q,    u"PC"_q,                       u"Linux"_q      },
		};
		for (int i = 0; i < 4; ++i) {
			const auto idx = i;
			const auto model = kPresets[i].model;
			const auto version = kPresets[i].version;
			spoofForm->add(
				object_ptr<Ui::RoundButton>(
					spoofForm,
					rpl::single(kPresets[i].label),
					st::defaultBoxButton),
				st::boxRowPadding)
			->addClickHandler([=] {
				CustomSettings::SetInt(u"spoofDeviceType"_q, idx);
				spoofModelInput->setText(model);
				spoofVersionInput->setText(version);
			});
		}
	}

	Ui::AddSkip(spoofForm, 8);
	spoofForm->add(
		object_ptr<Ui::RoundButton>(
			spoofForm,
			rpl::single(u"💾 Saqlash (qurilma sozlamalari)"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		CustomSettings::SetString(
			u"spoofDeviceModel"_q,
			spoofModelInput->getLastText().trimmed());
		CustomSettings::SetString(
			u"spoofSystemVersion"_q,
			spoofVersionInput->getLastText().trimmed());
		Ui::Toast::Show(
			u"Saqlandi! Yangi nom keyingi restart'dan boshlab "
			"qo'llanadi (qayta login shart emas)."_q);
	});
	Ui::AddSkip(spoofForm, 8);

	spoofFormWrap->toggle(
		CustomSettings::Get().spoofMobile,
		anim::type::instant);
	spoofToggleBtn->toggledValue()
		| rpl::skip(1)
		| rpl::on_next([=](bool on) {
			spoofFormWrap->toggle(on, anim::type::normal);
		}, spoofFormWrap->lifetime());

	// 2-bo'lim: Mutual-Contact Indikatori (Standart yopiq)
	const auto s2 = AddCollapsibleSection(content, u"🤝 Mutual-Contact Indikatori"_q, false);

	{
		const auto desc = s2->add(
			object_ptr<Ui::FlatLabel>(
				s2,
				rpl::single(u"Sizni contact'ga qo'shgan suhbatdoshlar ismi yoniga belgi (emoji) qo'yadi."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
		s2->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				desc->resizeToWidth(lw);
				desc->update();
			}
		}, desc->lifetime());
	}

	Ui::AddSkip(s2, 8);
	addToggle(
		s2,
		u"mutualContactShowInChatList"_q,
		u"Chat ro'yxatida ko'rsatish"_q,
		QString());
	s2->add(
		object_ptr<Ui::FlatLabel>(
			s2,
			rpl::single(u"Belgi (chat ro'yxati uchun):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto chatListEmojiInput = s2->add(
		object_ptr<Ui::InputField>(
			s2,
			st::defaultInputField,
			rpl::single(u"Emoji"_q),
			CustomSettings::MutualContactChatListEmoji()),
		st::boxRowPadding);

	Ui::AddSkip(s2, 8);
	addToggle(
		s2,
		u"mutualContactShowInContactsList"_q,
		u"Contacts ro'yxatida ko'rsatish"_q,
		QString());
	s2->add(
		object_ptr<Ui::FlatLabel>(
			s2,
			rpl::single(u"Belgi (Contacts ro'yxati uchun):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto contactsListEmojiInput = s2->add(
		object_ptr<Ui::InputField>(
			s2,
			st::defaultInputField,
			rpl::single(u"Emoji"_q),
			CustomSettings::MutualContactContactsListEmoji()),
		st::boxRowPadding);

	Ui::AddSkip(s2, 8);
	addToggle(
		s2,
		u"mutualContactShowInProfile"_q,
		u"Profil sarlavhasida ko'rsatish"_q,
		QString());
	s2->add(
		object_ptr<Ui::FlatLabel>(
			s2,
			rpl::single(u"Belgi (Profil uchun):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto profileEmojiInput = s2->add(
		object_ptr<Ui::InputField>(
			s2,
			st::defaultInputField,
			rpl::single(u"Emoji"_q),
			CustomSettings::MutualContactProfileEmoji()),
		st::boxRowPadding);

	Ui::AddSkip(s2, 8);
	addToggle(
		s2,
		u"mutualContactShowInMembersList"_q,
		u"Guruh a'zolari ro'yxatida ko'rsatish"_q,
		QString());
	s2->add(
		object_ptr<Ui::FlatLabel>(
			s2,
			rpl::single(u"Belgi (Guruh a'zolari ro'yxati uchun):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto membersListEmojiInput = s2->add(
		object_ptr<Ui::InputField>(
			s2,
			st::defaultInputField,
			rpl::single(u"Emoji"_q),
			CustomSettings::MutualContactMembersListEmoji()),
		st::boxRowPadding);

	Ui::AddSkip(s2, 8);
	s2->add(
		object_ptr<Ui::RoundButton>(
			s2,
			rpl::single(u"💾 Saqlash (belgilar)"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		CustomSettings::SetString(
			u"mutualContactChatListEmoji"_q,
			chatListEmojiInput->getLastText().trimmed());
		CustomSettings::SetString(
			u"mutualContactContactsListEmoji"_q,
			contactsListEmojiInput->getLastText().trimmed());
		CustomSettings::SetString(
			u"mutualContactProfileEmoji"_q,
			profileEmojiInput->getLastText().trimmed());
		CustomSettings::SetString(
			u"mutualContactMembersListEmoji"_q,
			membersListEmojiInput->getLastText().trimmed());
		Ui::Toast::Show(u"Saqlandi!"_q);
	});

	// 3-bo'lim: Branding (Standart yopiq)
	const auto s3 = AddCollapsibleSection(content, u"🎨 Branding"_q, false);

	{
		const auto desc = s3->add(
			object_ptr<Ui::FlatLabel>(
				s3,
				rpl::single(u"Oyna nomi va belgisini o'zgartirish (qayta ishga tushirish talab etiladi)."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
		s3->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				desc->resizeToWidth(lw);
				desc->update();
			}
		}, desc->lifetime());
	}

	Ui::AddSkip(s3, 8);

	s3->add(
		object_ptr<Ui::FlatLabel>(
			s3,
			rpl::single(u"Asosiy oyna nomi:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto titleInput = s3->add(
		object_ptr<Ui::InputField>(
			s3,
			st::defaultInputField,
			rpl::single(u"Masalan: MyChat"_q),
			CustomBranding::Get().windowTitle),
		st::boxRowPadding);

	s3->add(
		object_ptr<Ui::FlatLabel>(
			s3,
			rpl::single(u"Sozlamalar oyna nomi:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto modInput = s3->add(
		object_ptr<Ui::InputField>(
			s3,
			st::defaultInputField,
			rpl::single(u"Masalan: Mening Sozlamalarim"_q),
			CustomBranding::Get().customModTitle),
		st::boxRowPadding);

	s3->add(
		object_ptr<Ui::FlatLabel>(
			s3,
			rpl::single(u"Icon fayl yo'li (PNG yoki ICO):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto iconInput = s3->add(
		object_ptr<Ui::InputField>(
			s3,
			st::defaultInputField,
			rpl::single(u"Bo'sh = standart icon"_q),
			CustomBranding::Get().iconPath),
		st::boxRowPadding);

	Ui::AddSkip(s3, 4);
	s3->add(
		object_ptr<Ui::RoundButton>(
			s3,
			rpl::single(u"📁 Icon faylini tanlash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto path = QFileDialog::getOpenFileName(
			nullptr,
			u"Icon tanlang (PNG yoki ICO)"_q,
			QStandardPaths::writableLocation(
				QStandardPaths::PicturesLocation),
			u"Rasm fayllar (*.png *.ico *.jpg *.jpeg)"_q);
		if (!path.isEmpty()) {
			iconInput->setText(path);
		}
	});

	Ui::AddSkip(s3, 4);
	s3->add(
		object_ptr<Ui::RoundButton>(
			s3,
			rpl::single(u"🗑️ Iconni tozalash"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		iconInput->setText(QString());
	});

	Ui::AddSkip(s3, 12);
	s3->add(
		object_ptr<Ui::RoundButton>(
			s3,
			rpl::single(u"💾 Saqlash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		CustomBranding::SetWindowTitle(titleInput->getLastText().trimmed());
		CustomBranding::SetCustomModTitle(modInput->getLastText().trimmed());
		CustomBranding::SetIconPath(iconInput->getLastText().trimmed());
		Ui::Toast::Show(u"Saqlandi! O'zgartirishlar uchun dasturni qayta yoqing."_q);
	});

	Ui::AddSkip(content, st::settingsThumbSkip);
}
