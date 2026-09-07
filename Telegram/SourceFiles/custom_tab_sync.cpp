#include "custom_tab_common.h"
#include "custom_sync.h"
#include "custom_sync_client.h"
#include "custom_sync_outbox.h"

#include <algorithm>

void fillSyncTab(
		not_null<Ui::VerticalLayout*> content,
		QWidget *dialogParent) {
	// ─── Tushuntirish sarlavhasi ──────────────────────────────────────────────
	{
		const auto desc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Qurilmalararo ma'lumotlarni (o'chirilgan xabarlar, tahrirlar, faollik va media indeksi) "
					"shifrlangan holda xavfsiz sinxronlash imkoniyati."_q),
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

	// ─── 1-bo'lim: Sozlamalar (Standart ochiq) ────────────────────────────────
	const auto sSettings = AddCollapsibleSection(content, u"⚙️ Sozlamalar"_q, true);

	const auto syncToggle = sSettings->add(
		object_ptr<Ui::SettingsButton>(
			sSettings,
			rpl::single(u"Sinxronizatsiyani yoqish"_q),
			st::settingsButtonNoIcon));
	syncToggle->toggleOn(rpl::single(CustomSettings::SyncEnabled()));

	sSettings->add(
		object_ptr<Ui::FlatLabel>(
			sSettings,
			rpl::single(u"Sinxronlash serveri manzili (http:// yoki https://):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	const auto urlInput = sSettings->add(
		object_ptr<Ui::InputField>(
			sSettings,
			st::defaultInputField,
			rpl::single(u"https://sync.example.com"_q),
			CustomSettings::SyncServerUrl()),
		st::boxRowPadding);

	sSettings->add(
		object_ptr<Ui::RoundButton>(
			sSettings,
			rpl::single(u"💾 Server manzilini saqlash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto text = urlInput->getLastText().trimmed();
		if (!text.startsWith(u"http://"_q) && !text.startsWith(u"https://"_q)) {
			Ui::Toast::Show(u"Xato: Server manzili http:// yoki https:// bilan boshlanishi shart"_q);
			return;
		}
		auto clean = text;
		while (clean.endsWith('/')) {
			clean.chop(1);
		}
		CustomSettings::SetString(u"syncServerUrl"_q, clean);
		Ui::Toast::Show(u"Server manzili saqlandi ✓"_q);
	});

	Ui::AddSkip(sSettings, 6);

	sSettings->add(
		object_ptr<Ui::FlatLabel>(
			sSettings,
			rpl::single(u"Sinxronlash oralig'i (5 dan 3600 soniyagacha):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	const auto intervalInput = sSettings->add(
		object_ptr<Ui::InputField>(
			sSettings,
			st::defaultInputField,
			rpl::single(u"Oraliq soniyalarda (5–3600)"_q),
			QString::number(CustomSettings::SyncIntervalSeconds())),
		st::boxRowPadding);

	sSettings->add(
		object_ptr<Ui::RoundButton>(
			sSettings,
			rpl::single(u"💾 Oraliqni saqlash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		bool ok = false;
		const auto val = intervalInput->getLastText().trimmed().toInt(&ok);
		if (!ok) {
			Ui::Toast::Show(u"Faqat butun son kiriting"_q);
			return;
		}
		const auto clamped = std::clamp(val, 5, 3600);
		CustomSettings::SetInt(u"syncIntervalSeconds"_q, clamped);
		intervalInput->setText(QString::number(clamped));
		Ui::Toast::Show(u"Sinxronlash oralig'i saqlandi: %1 soniya ✓"_q.arg(clamped));
	});

	// ─── 2-bo'lim: Ro'yxatdan o'tish (Enrollment) ─────────────────────────────
	const auto sEnroll = AddCollapsibleSection(content, u"📱 Ro'yxatdan o'tish (Enrollment)"_q, true);

	const auto initialDevId = CustomSync::Outbox::GetState(QStringLiteral("device_id"));
	const auto enrollStatusLabel = sEnroll->add(
		object_ptr<Ui::FlatLabel>(
			sEnroll,
			rpl::single(!initialDevId.isEmpty()
				? u"Holat: Qurilma ro'yxatdan o'tgan ✓ (ID: %1)"_q.arg(initialDevId)
				: u"Holat: Qurilma hali serverda ro'yxatdan o'tmagan"_q),
			st::boxLabel),
		st::boxRowPadding);

	sEnroll->add(
		object_ptr<Ui::FlatLabel>(
			sEnroll,
			rpl::single(u"Bir martalik kod (One-time code):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	const auto codeInput = sEnroll->add(
		object_ptr<Ui::InputField>(
			sEnroll,
			st::defaultInputField,
			rpl::single(u"Serverdan olingan 8 xonali kod"_q),
			QString()),
		st::boxRowPadding);

	sEnroll->add(
		object_ptr<Ui::FlatLabel>(
			sEnroll,
			rpl::single(u"Qurilma nomi:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	const auto nameInput = sEnroll->add(
		object_ptr<Ui::InputField>(
			sEnroll,
			st::defaultInputField,
			rpl::single(u"Masalan: Ish kompyuteri"_q),
			u"Telegram Desktop"_q),
		st::boxRowPadding);

	const auto enrollBtn = sEnroll->add(
		object_ptr<Ui::RoundButton>(
			sEnroll,
			rpl::single(u"📲 Qurilmani ro'yxatdan o'tkazish"_q),
			st::defaultBoxButton),
		st::boxRowPadding);

	// ─── 3-bo'lim: Sinxronizatsiya holati (Status) ────────────────────────────
	const auto sStatus = AddCollapsibleSection(content, u"📊 Sinxronizatsiya holati"_q, true);

	const auto orchStateLabel = sStatus->add(
		object_ptr<Ui::FlatLabel>(
			sStatus,
			rpl::single(u"Holat: Tekshirilmoqda..."_q),
			st::boxLabel),
		st::boxRowPadding);

	const auto lastSuccessLabel = sStatus->add(
		object_ptr<Ui::FlatLabel>(
			sStatus,
			rpl::single(u"Oxirgi muvaffaqiyatli: ... "_q),
			st::customModHintLabel),
		st::boxRowPadding);

	const auto pendingLabel = sStatus->add(
		object_ptr<Ui::FlatLabel>(
			sStatus,
			rpl::single(u"Navbatdagi yozuvlar: ... "_q),
			st::customModHintLabel),
		st::boxRowPadding);

	const auto inFlightLabel = sStatus->add(
		object_ptr<Ui::FlatLabel>(
			sStatus,
			rpl::single(u"Jarayon: ... "_q),
			st::customModHintLabel),
		st::boxRowPadding);

	const auto failureLabel = sStatus->add(
		object_ptr<Ui::FlatLabel>(
			sStatus,
			rpl::single(u"Xatoliklar: ... "_q),
			st::customModHintLabel),
		st::boxRowPadding);

	Ui::AddSkip(sStatus, 4);

	const auto syncNowBtn = sStatus->add(
		object_ptr<Ui::RoundButton>(
			sStatus,
			rpl::single(u"🔄 Hozir sinxronlash"_q),
			st::defaultBoxButton),
		st::boxRowPadding);

	// ─── 4-bo'lim: Diagnostika ────────────────────────────────────────────────
	const auto sDiag = AddCollapsibleSection(content, u"🔍 Diagnostika"_q, false);

	const auto corruptStr = CustomSync::Outbox::GetState(QStringLiteral("corrupt_records"), QStringLiteral("0"));
	const auto corruptCount = corruptStr.toInt();
	if (corruptCount > 0) {
		sDiag->add(
			object_ptr<Ui::FlatLabel>(
				sDiag,
				rpl::single(u"⚠️ %1 ta yozuv deshifrlanmadi, kalit mos kelmasligi mumkin!"_q.arg(corruptCount)),
				st::boxLabel),
			st::boxRowPadding);
		sDiag->add(
			object_ptr<Ui::FlatLabel>(
				sDiag,
				rpl::single(u"Ushbu yozuvlar boshqa qurilmaning master kaliti bilan shifrlangan bo'lishi mumkin. "
					"Qurilmalar o'rtasida kalit almashish (passphrase wrap) qo'llab-quvvatlanmaguncha "
					"bu yozuvlar ochilmaydi."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
	} else {
		sDiag->add(
			object_ptr<Ui::FlatLabel>(
				sDiag,
				rpl::single(u"Deshifrlash holati: 0 ta buzilgan yozuv (barcha olingan yozuvlar soz)"_q),
				st::customModHintLabel),
			st::boxRowPadding);
	}

	Ui::AddSkip(sDiag, 4);
	const auto cursor = CustomSync::Outbox::GetState(QStringLiteral("pull_cursor"), QStringLiteral("0"));
	const auto cursorText = (cursor == u"0"_q || cursor.isEmpty())
		? u"Server pull kursori (pull_cursor): 0 (hali sinxronlanmagan)"_q
		: (u"Server pull kursori (pull_cursor): "_q + cursor + u" (sinxronlangan)"_q);
	sDiag->add(
		object_ptr<Ui::FlatLabel>(
			sDiag,
			rpl::single(cursorText),
			st::customModHintLabel),
		st::boxRowPadding);

	Ui::AddSkip(sDiag, 4);
	const auto hasKey = !CustomSync::Outbox::GetState(QStringLiteral("master_key_protected")).isEmpty();
	sDiag->add(
		object_ptr<Ui::FlatLabel>(
			sDiag,
			rpl::single(hasKey
				? u"Lokal master kalit: mavjud (DPAPI bilan himoyalangan) ✓"_q
				: u"Lokal master kalit: hali yaratilmagan (ro'yxatdan o'tgach yaratiladi)"_q),
			st::customModHintLabel),
		st::boxRowPadding);

	// ─── Statusni yangilash funksiyasi ────────────────────────────────────────
	const auto updateStatus = [=](const CustomSync::SyncStatus &st) {
		const auto orch = CustomSync::GetOrchestrator();
		if (!orch || !CustomSettings::SyncEnabled()) {
			orchStateLabel->setText(u"Holat: Sinxronizatsiya to'xtatilgan (yoqilmagan)"_q);
		} else {
			orchStateLabel->setText(u"Holat: Sinxronizatsiya orkestratori faol ✓"_q);
		}

		if (st.lastSuccessAt > 0) {
			lastSuccessLabel->setText(u"Oxirgi muvaffaqiyatli: %1"_q.arg(
				QDateTime::fromSecsSinceEpoch(st.lastSuccessAt).toString(u"yyyy-MM-dd HH:mm:ss"_q)));
		} else {
			lastSuccessLabel->setText(u"Oxirgi muvaffaqiyatli: hali muvaffaqiyat bo'lmagan"_q);
		}

		const auto pending = CustomSync::Outbox::PendingCount();
		pendingLabel->setText(u"Navbatdagi yozuvlar (outbox): %1 ta"_q.arg(pending));

		if (st.inFlight) {
			inFlightLabel->setText(u"Jarayon: sinxronlanmoqda… (faol)"_q);
		} else {
			inFlightLabel->setText(u"Jarayon: kutilmoqda (tinch)"_q);
		}

		if (st.consecutiveFailures > 0) {
			failureLabel->setText(
				u"Ketma-ket xatolar: %1 ta (qayta urinish oraliqni uzaytirmoqda — backoff)\n"
				"Oxirgi xatolik: %2"_q
					.arg(st.consecutiveFailures)
					.arg(st.lastError.isEmpty() ? u"noma'lum xato"_q : st.lastError));
		} else {
			failureLabel->setText(u"Xatoliklar: yo'q (tizim soz holatda)"_q);
		}

		const auto currentDev = CustomSync::Outbox::GetState(QStringLiteral("device_id"));
		const bool canSyncNow = CustomSettings::SyncEnabled()
			&& !currentDev.isEmpty()
			&& !st.inFlight;
		syncNowBtn->setEnabled(canSyncNow);
	};

	// ─── Orkestrator signallariga ulanish ──────────────────────────────────────
	if (const auto orch = CustomSync::GetOrchestrator()) {
		orch->connect(orch, &CustomSync::Orchestrator::statusChanged, content, [=](const CustomSync::SyncStatus &s) {
			updateStatus(s);
		});
	}

	// Boshlang'ich holatni chizish
	updateStatus(CustomSync::CurrentSyncStatus());

	// ─── Sinxronlash yoqish/o'chirish hodisasi ──────────────────────────────────
	syncToggle->toggledValue()
		| rpl::skip(1)
		| rpl::on_next([=](bool on) {
			CustomSettings::Set(u"syncEnabled"_q, on);
			if (on) {
				CustomSync::Start();
				if (const auto orch = CustomSync::GetOrchestrator()) {
					orch->connect(orch, &CustomSync::Orchestrator::statusChanged, content, [=](const CustomSync::SyncStatus &s) {
						updateStatus(s);
					});
				}
			} else {
				CustomSync::Stop();
			}
			updateStatus(CustomSync::CurrentSyncStatus());
			Ui::Toast::Show(on
				? u"Sinxronizatsiya yoqildi ✓"_q
				: u"Sinxronizatsiya o'chirildi"_q);
		}, syncToggle->lifetime());

	// ─── Hozir sinxronlash tugmasi ─────────────────────────────────────────────
	syncNowBtn->addClickHandler([=] {
		if (!CustomSettings::SyncEnabled()) {
			Ui::Toast::Show(u"Sinxronizatsiya o'chirilgan"_q);
			return;
		}
		const auto devId = CustomSync::Outbox::GetState(QStringLiteral("device_id"));
		if (devId.isEmpty()) {
			Ui::Toast::Show(u"Avval qurilmani ro'yxatdan o'tkazing"_q);
			return;
		}
		syncNowBtn->setEnabled(false);
		CustomSync::SyncNow();
		Ui::Toast::Show(u"Sinxronlash so'rovi yuborildi ✓"_q);
		updateStatus(CustomSync::CurrentSyncStatus());
	});

	// ─── Ro'yxatdan o'tish tugmasi va Tasdiqlash oynasi ────────────────────────
	enrollBtn->addClickHandler([=] {
		// K5: Sinxronizatsiya o'chiq bo'lsa tarmoqqa tegilmaydi
		if (!CustomSettings::SyncEnabled()) {
			Ui::Toast::Show(u"Avval yuqoridagi sozlamalarda sinxronizatsiyani yoqing"_q);
			return;
		}
		const auto url = CustomSettings::SyncServerUrl().trimmed();
		if (!url.startsWith(u"http://"_q) && !url.startsWith(u"https://"_q)) {
			Ui::Toast::Show(u"Avval to'g'ri server manzilini kiriting va saqlang"_q);
			return;
		}
		const auto code = codeInput->getLastText().trimmed();
		if (code.isEmpty()) {
			Ui::Toast::Show(u"Bir martalik kodni kiriting"_q);
			return;
		}
		const auto deviceName = nameInput->getLastText().trimmed();
		if (deviceName.isEmpty()) {
			Ui::Toast::Show(u"Qurilma nomini kiriting"_q);
			return;
		}

		// 🔴 MUHIM: Ikkinchi qurilma boshqa kalit yaratishi haqida qat'iy ogohlantirish
		const auto window = Core::App().activeWindow();
		if (!window) return;

		window->show(Ui::MakeConfirmBox({
			.text = u"⚠️ DIQQAT: Kalit almashish (passphrase wrap) hali joriy qilinmagan!\n\n"
				"Bu qurilma o'zining alohida master kalitini yaratadi. Natijada:\n"
				"• Boshqa qurilmalardan yuborilgan yozuvlar bu yerda ochilmaydi (deshifrlanmaydi);\n"
				"• Har bir qurilma o'zining alohida arxivini saqlaydi;\n"
				"• Bir-birining yozuvlari bilan birlashmaydi (deduplikatsiya bo'lmaydi).\n\n"
				"Qurilmani ro'yxatdan o'tkazishni davom ettirasizmi?"_q,
			.confirmed = [=](Fn<void()> close) {
				close();
				enrollBtn->setEnabled(false);
				enrollStatusLabel->setText(u"Serverga ulanmoqda..."_q);

				auto *client = new CustomSync::Client(content);
				client->enroll(url, code, deviceName, [=](bool success, QString error) {
					client->deleteLater();
					enrollBtn->setEnabled(true);
					if (success) {
						const auto newDevId = CustomSync::Outbox::GetState(QStringLiteral("device_id"));
						enrollStatusLabel->setText(u"Holat: Qurilma ro'yxatdan o'tdi ✓ (ID: %1)"_q.arg(newDevId));
						Ui::Toast::Show(u"Ro'yxatdan o'tish muvaffaqiyatli yakunlandi! ID: %1"_q.arg(newDevId));
						updateStatus(CustomSync::CurrentSyncStatus());
						if (CustomSettings::SyncEnabled()) {
							CustomSync::Start();
						}
					} else {
						enrollStatusLabel->setText(u"Ro'yxatdan o'tishda xatolik: %1"_q.arg(error));
						Ui::Toast::Show(u"Server xatosi: %1"_q.arg(error));
					}
				});
			},
			.confirmText = u"Ha, davom etish"_q,
			.cancelText = u"Bekor qilish"_q,
		}));
	});

	Ui::AddSkip(content, st::settingsThumbSkip);
}
