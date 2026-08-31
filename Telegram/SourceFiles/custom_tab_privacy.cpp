#include "custom_tab_common.h"

void fillPrivacyTab(not_null<Ui::VerticalLayout*> content) {
	const auto addToggle = [&](
			not_null<Ui::VerticalLayout*> parent,
			const QString &id,
			const QString &text,
			const QString &description) {
		const auto &val = CustomSettings::Get();
		auto current = false;
		if (id == u"ghostMode"_q) current = val.ghostMode;
		else if (id == u"bypassRestrictions"_q) current = val.bypassRestrictions;
		else if (id == u"offlineDb"_q) current = val.offlineDb;
		else if (id == u"antiDelete"_q) current = val.antiDelete;
		else if (id == u"antiEdit"_q) current = val.antiEdit;
		else if (id == u"storyAnonymousView"_q) current = val.storyAnonymousView;
		else if (id == u"storyMediaBackupEnabled"_q) current = val.storyMediaBackupEnabled;

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

	// 1-bo'lim: Yashirinlik (Standart ochiq)
	const auto s1 = AddCollapsibleSection(content, u"🛡️ Yashirinlik va Maxfiylik"_q, true);

	addToggle(
		s1,
		u"ghostMode"_q,
		u"Ghost Mode"_q,
		u"Online holatini, yozmoqda belgisini va xabar o'qildi bildirishnomasini yashiradi.\n\nYoqilgach, to'liq kuchga kirishi uchun 1-2 daqiqa ketishi mumkin."_q);
	addToggle(
		s1,
		u"storyAnonymousView"_q,
		u"Hikoyalarni anonim ko'rish"_q,
		u"Hikoyani ko'rganingiz haqida egasiga bildirish yuborilmaydi."_q);
	addToggle(
		s1,
		u"storyMediaBackupEnabled"_q,
		u"Hikoya media'sini saqlash"_q,
		u"Kuzatilayotgan userlarning hikoya (story) rasmi/videosi avtomatik, ko'rmasdan lokal saqlanadi. Disk joyini sarflaydi, standart holatda o'chirilgan."_q);
	addToggle(
		s1,
		u"bypassRestrictions"_q,
		u"Cheklangan chatda nusxalash va yuborish"_q,
		u"Cheklov qo'yilgan chatlardagi xabarlarni boshqaga yuborish yoki nusxa olishga ruxsat beradi."_q);
	addToggle(
		s1,
		u"antiDelete"_q,
		u"Anti-Delete"_q,
		u"O'chirilgan xabarlarni ko'rinishda qoldiradi."_q);
	addToggle(
		s1,
		u"antiEdit"_q,
		u"Anti-Edit"_q,
		u"Tahrirdan oldingi matnni ko'rsatadi."_q);
	addToggle(
		s1,
		u"offlineDb"_q,
		u"Offline xabar bazasi"_q,
		u"Xabarlar va medialarni internet bo'lmaganda ham ko'rish uchun qurilmada saqlaydi."_q);

	// Arxiv statistikasi
	{
		const auto archived = CustomDB::ArchivedMessageCount();
		const auto sizeMb = CustomDB::DatabaseSizeBytes() / (1024 * 1024);
		const auto stats = s1->add(
			object_ptr<Ui::FlatLabel>(
				s1,
				rpl::single(u"Arxivda "_q
					+ QString::number(archived)
					+ u" ta xabar saqlangan. Baza hajmi: "_q
					+ QString::number(sizeMb)
					+ u" MB."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
		s1->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				stats->resizeToWidth(lw);
				stats->update();
			}
		}, stats->lifetime());
	}

	Ui::AddSkip(content, st::settingsThumbSkip);
}
