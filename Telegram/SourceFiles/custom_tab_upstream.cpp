#include "custom_tab_common.h"

void fillUpstreamCheckSection(not_null<Ui::VerticalLayout*> content) {
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"🔔 Rasmiy versiya tekshiruvi"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	{
		const auto desc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Rasmiy telegramdesktop/tdesktop'da yangi "
					"reliz chiqqanini tekshiradi va bildiradi. Hech qanday "
					"avtomatik yangilash yoki build qilmaydi — faqat "
					"xabardor qiladi."_q),
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

	// ── Status qatori ────────────────────────────────────────────────
	auto statusText = std::make_shared<rpl::variable<QString>>(
		u"Hali tekshirilmagan"_q);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			statusText->value(),
			st::boxLabel),
		st::boxRowPadding);

	// ── GitHub'da ko'rish tugmasi (faqat yangilanish bo'lsa ko'rinadi) ──
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

	// ── "Hozir tekshirish" tugmasi ──────────────────────────────────
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

	// ── Avtomatik tekshirish toggle ───────────────────────────────────
	Ui::AddSkip(content, 12);
	const auto autoBtn = content->add(
		object_ptr<Ui::SettingsButton>(
			content,
			rpl::single(u"Avtomatik tekshirish"_q),
			st::settingsButtonNoIcon));
	autoBtn->toggleOn(rpl::single(CustomSettings::UpstreamCheckEnabled()));

	// ── Chastota tanlovi (auto yoqilgandagina ko'rinadi) ──────────────
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
		customWrap->toggle(true, anim::type::normal);
	});

	freqWrap->toggle(CustomSettings::UpstreamCheckEnabled(), anim::type::instant);

	autoBtn->toggledValue()
		| rpl::skip(1)
		| rpl::on_next([=](bool on) {
			CustomSettings::Set(u"upstreamCheckEnabled"_q, on);
			CustomUpstream::UpdateAutoTimer();
			freqWrap->toggle(on, anim::type::normal);
			Ui::Toast::Show(on
				? u"Avtomatik tekshirish yoqildi"_q
				: u"Avtomatik tekshirish o'chirildi"_q);
		}, autoBtn->lifetime());

	// ── Oxirgi tekshiruv vaqti ──────────────────────────────────────
	Ui::AddSkip(content, 8);
	{
		const auto lastAt = CustomSettings::UpstreamLastCheckedAt();
		const auto text = (lastAt > 0)
			? (u"Oxirgi tekshiruv: "_q
				+ QDateTime::fromSecsSinceEpoch(lastAt)
					.toString(u"yyyy-MM-dd HH:mm"_q))
			: u"Oxirgi tekshiruv: hali yo'q"_q;
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(text),
				st::customModHintLabel),
			st::boxRowPadding);
	}
}

