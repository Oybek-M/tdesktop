#include "custom_tab_common.h"

void fillUpstreamCheckSection(not_null<Ui::VerticalLayout*> content) {
	{
		const auto desc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Rasmiy tdesktop relizlarini tekshiradi va xabardor qiladi."_q),
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

	Ui::AddSkip(content, 8);

	auto statusText = std::make_shared<rpl::variable<QString>>(
		u"Hali tekshirilmagan"_q);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			statusText->value(),
			st::boxLabel),
		st::boxRowPadding);

	const auto linkWrap = content->add(
		object_ptr<Ui::SlideWrap<Ui::RoundButton>>(
			content,
			object_ptr<Ui::RoundButton>(
				content,
				rpl::single(u"🔗 GitHub'da ko'rish"_q),
				st::defaultBoxButton)),
		st::boxRowPadding);
	const auto releaseUrl = std::make_shared<QString>();
	linkWrap->entity()->addClickHandler([=] {
		if (!releaseUrl->isEmpty()) {
			QDesktopServices::openUrl(QUrl(*releaseUrl));
		}
	});
	linkWrap->toggle(false, anim::type::instant);

	const auto applyResult = [=](CustomUpstream::CheckResult r) {
		if (!r.checked) {
			*statusText = r.error.isEmpty()
				? u"Hali tekshirilmagan"_q
				: (u"Tekshirib bo'lmadi: "_q + r.error);
			linkWrap->toggle(false, anim::type::normal);
			return;
		}
		const auto base = u"Siz asoslangan: "_q + r.localVersion
			+ u"  |  Rasmiy so'nggi: "_q + r.latestVersion;
		*statusText = r.hasNewer
			? (base + u"  —  YANGILANISH BOR!"_q)
			: base;
		*releaseUrl = r.releaseUrl;
		linkWrap->toggle(r.hasNewer, anim::type::normal);
	};
	applyResult(CustomUpstream::LastResult());

	Ui::AddSkip(content, 4);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"🔄 Hozir tekshirish"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		*statusText = u"Tekshirilmoqda..."_q;
		CustomUpstream::CheckNow(crl::guard(content, applyResult));
	});

	Ui::AddSkip(content, 12);
	const auto autoBtn = content->add(
		object_ptr<Ui::SettingsButton>(
			content,
			rpl::single(u"Avtomatik tekshirish"_q),
			st::settingsButtonNoIcon));
	autoBtn->toggleOn(rpl::single(CustomSettings::UpstreamCheckEnabled()));

	const auto freqWrap = content->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			content,
			object_ptr<Ui::VerticalLayout>(content)),
		style::margins(0, 0, 0, 0));
	const auto freqForm = freqWrap->entity();

	freqForm->add(
		object_ptr<Ui::FlatLabel>(
			freqForm,
			rpl::single(u"Chastota:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	struct Preset { QString label; int minutes; };
	const Preset kPresets[3] = {
		{ u"Soatlik"_q, 60 },
		{ u"Kunlik"_q, 1440 },
		{ u"Haftalik"_q, 10080 },
	};
	const auto isPreset = [](int minutes) {
		return minutes == 60 || minutes == 1440 || minutes == 10080;
	};
	const auto selectedMinutes = std::make_shared<rpl::variable<int>>(
		CustomSettings::UpstreamCheckIntervalMinutes());

	const auto customWrap = freqForm->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			freqForm,
			object_ptr<Ui::VerticalLayout>(freqForm)),
		style::margins(0, 0, 0, 0));
	const auto customForm = customWrap->entity();
	const auto customInput = customForm->add(
		object_ptr<Ui::InputField>(
			customForm,
			st::defaultInputField,
			rpl::single(u"Daqiqada (min 15)"_q),
			QString::number(CustomSettings::UpstreamCheckIntervalMinutes())),
		st::boxRowPadding);
	customForm->add(
		object_ptr<Ui::RoundButton>(
			customForm,
			rpl::single(u"💾 Saqlash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		bool ok = false;
		const auto parsed = customInput->getLastText().trimmed().toInt(&ok);
		const auto clamped = std::max(ok ? parsed : 1440, 15);
		CustomSettings::SetInt(u"upstreamCheckIntervalMinutes"_q, clamped);
		CustomUpstream::UpdateAutoTimer();
		*selectedMinutes = clamped;
		Ui::Toast::Show(
			u"Chastota saqlandi: "_q + QString::number(clamped) + u" daqiqa"_q);
	});
	customWrap->toggle(
		!isPreset(CustomSettings::UpstreamCheckIntervalMinutes()),
		anim::type::instant);

	for (const auto &preset : kPresets) {
		const auto minutes = preset.minutes;
		const auto label = preset.label;
		freqForm->add(
			object_ptr<Ui::RoundButton>(
				freqForm,
				selectedMinutes->value() | rpl::map([=](int current) {
					return (current == minutes)
						? (u"✓ "_q + label)
						: label;
				}),
				st::defaultBoxButton),
			st::boxRowPadding)
		->addClickHandler([=] {
			CustomSettings::SetInt(u"upstreamCheckIntervalMinutes"_q, minutes);
			CustomUpstream::UpdateAutoTimer();
			*selectedMinutes = minutes;
			customWrap->toggle(false, anim::type::normal);
			Ui::Toast::Show(u"Chastota saqlandi."_q);
		});
	}

	freqForm->add(
		object_ptr<Ui::RoundButton>(
			freqForm,
			rpl::single(u"Boshqa..."_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		customWrap->toggle(
			!customWrap->toggled(),
			anim::type::normal);
	});

	freqWrap->toggle(
		CustomSettings::UpstreamCheckEnabled(),
		anim::type::instant);
	autoBtn->toggledValue()
		| rpl::skip(1)
		| rpl::on_next([=](bool on) {
			CustomSettings::Set(u"upstreamCheckEnabled"_q, on);
			CustomUpstream::UpdateAutoTimer();
			freqWrap->toggle(on, anim::type::normal);
			Ui::Toast::Show(on
				? u"Avtomatik tekshirish yoqildi ✓"_q
				: u"Avtomatik tekshirish o'chirildi"_q);
		}, autoBtn->lifetime());
}

void fillSystemTab(
		not_null<Ui::VerticalLayout*> content,
		QWidget *dialogParent,
		Fn<void()> onArchiveChanged) {
	// 1-bo'lim: Dastur haqida (Standart ochiq)
	const auto s1 = AddCollapsibleSection(content, u"ℹ️ Dastur haqida"_q, true);

	s1->add(
		object_ptr<Ui::FlatLabel>(
			s1,
			rpl::single(AppAlphaVersion
				? (u"CustomMod %1 · alpha %2"_q
					.arg(QString::fromUtf8(AppVersionStr))
					.arg(int(AppAlphaVersion % 1000)))
				: (u"CustomMod %1"_q
					.arg(QString::fromUtf8(AppVersionStr)))),
			st::customModHintLabel),
		st::boxRowPadding);

	s1->add(
		object_ptr<Ui::FlatLabel>(
			s1,
			rpl::single(u"Telegram Desktop uchun CustomMod forki.\n"
				"Ghost Mode, Anti-Delete, Anti-Edit, Faollik kuzatuv va Media Backup imkoniyatlari."_q),
			st::customModHintLabel),
		st::boxRowPadding);

	// 2-bo'lim: Rasmiy versiya tekshiruvi (Standart yopiq)
	const auto s2 = AddCollapsibleSection(content, u"🔔 Rasmiy versiya tekshiruvi"_q, false);
	fillUpstreamCheckSection(s2);

	// 3-bo'lim: ⚠️ Xavfli hudud (HAR DOIM YOPIQ!)
	const auto s3 = AddCollapsibleSection(content, u"⚠️  XAVFLI HUDUD  ⚠️"_q, false);

	s3->add(
		object_ptr<Ui::FlatLabel>(
			s3,
			rpl::single(u"Quyidagi amallar arxiv ma'lumotlarini BUTUNLAY o'chiradi.\n"
				"Bu amalni bekor qilib bo'lmaydi. Tasdiqlash talab qilinadi."_q),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	s3->add(
		object_ptr<Ui::RoundButton>(
			s3,
			rpl::single(u"🗑️  O'chirilganlar arxivini tozalash"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto reply = QMessageBox::warning(
			dialogParent,
			u"O'chirilganlar arxivini tozalash"_q,
			u"Bu amal BARCHA saqlangan o'chirilgan xabarlarni o'chiradi.\n\n"
			"Bu amalni bekor qilib bo'lmaydi.\n\nDavom etasizmi?"_q,
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (reply != QMessageBox::Yes) return;
		CustomDB::ClearDeletedArchive();
		if (onArchiveChanged) onArchiveChanged();
		Ui::Toast::Show(u"🗑️ O'chirilganlar arxivi tozalandi."_q);
	});

	s3->add(
		object_ptr<Ui::RoundButton>(
			s3,
			rpl::single(u"✏️  Tahrir tarixi arxivini tozalash"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto reply = QMessageBox::warning(
			dialogParent,
			u"Tahrir tarixini tozalash"_q,
			u"Bu amal BARCHA saqlangan tahrir yozuvlarini o'chiradi.\n\n"
			"Bu amalni bekor qilib bo'lmaydi.\n\nDavom etasizmi?"_q,
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (reply != QMessageBox::Yes) return;
		CustomDB::ClearEditedArchive();
		if (onArchiveChanged) onArchiveChanged();
		Ui::Toast::Show(u"✏️ Tahrir tarixi arxivi tozalandi."_q);
	});

	s3->add(
		object_ptr<Ui::RoundButton>(
			s3,
			rpl::single(u"☠️  BARCHA arxivni tozalash (O'chirilgan + Tahrir)"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto first = QMessageBox::warning(
			dialogParent,
			u"BARCHA arxivni tozalash"_q,
			u"Bu amal BARCHA arxiv ma'lumotlarini o'chiradi:\n"
			"  • Barcha o'chirilgan xabar yozuvlari\n"
			"  • Barcha tahrir tarixi yozuvlari\n\n"
			"Bu amalni bekor qilib bo'lmaydi."_q,
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (first != QMessageBox::Yes) return;
		const auto second = QMessageBox::critical(
			dialogParent,
			u"Yakuniy tasdiqlash"_q,
			u"HAQIQATAN HAM ishonchingiz komilmi?\n\n"
			"Barcha o'chirilgan xabar va tahrir yozuvlari\n"
			"BUTUNLAY yo'qoladi."_q,
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (second != QMessageBox::Yes) return;
		CustomDB::ClearAllArchive();
		if (onArchiveChanged) onArchiveChanged();
		Ui::Toast::Show(u"☠️ Barcha arxiv ma'lumotlari tozalandi."_q);
	});

	Ui::AddSkip(content, st::settingsThumbSkip);
}
