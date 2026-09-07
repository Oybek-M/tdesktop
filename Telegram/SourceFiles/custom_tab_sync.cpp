#include "custom_tab_common.h"
#include "custom_sync.h"
#include "custom_sync_client.h"
#include "custom_sync_outbox.h"
#include "custom_sync_crypto.h"
#include "custom_sync_keyshare.h"

#include "ui/widgets/fields/password_input.h"
#include "ui/layers/generic_box.h"
#include "crl/crl_async.h"
#include "crl/crl_on_main.h"

#include <algorithm>

namespace {

// Arxiv paroli uchun minimal uzunlik talabi (kamida 8 ta belgi, spec §4.4).
constexpr int kMinPassphraseLength = 8;

// ─── A shoxcha: Serverda mavjud arxivga ulanish oynasi (Join) ────────────────
void ShowJoinArchiveBox(
		QWidget *dialogParent,
		CustomSync::Client *client,
		const QVector<CustomSync::KeyShare::Wrap> &wraps,
		Fn<void()> onComplete,
		Fn<void()> onCancel) {
	const auto it = std::find_if(wraps.begin(), wraps.end(), [](const CustomSync::KeyShare::Wrap &w) {
		return w.wrapType == u"passphrase"_q || w.wrapType.isEmpty();
	});
	const auto targetWrap = (it != wraps.end()) ? *it : wraps.front();

	ShowCustomBox(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"🔐 Umumiy arxivga ulanish"_q));
		Ui::AddSkip(box->verticalLayout(), 6);

		box->verticalLayout()->add(
			object_ptr<Ui::FlatLabel>(
				box->verticalLayout(),
				rpl::single(u"Serverda mavjud umumiy arxiv topildi. "
					"Barcha qurilmalaringizda bir xil arxivdan foydalanish uchun arxiv parolini kiriting:"_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);

		Ui::AddSkip(box->verticalLayout(), 6);

		box->verticalLayout()->add(
			object_ptr<Ui::FlatLabel>(
				box->verticalLayout(),
				rpl::single(u"Arxiv paroli:"_q),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		const auto passInput = box->verticalLayout()->add(
			object_ptr<Ui::PasswordInput>(
				box->verticalLayout(),
				st::defaultInputField,
				rpl::single(u"Parolni kiriting"_q),
				QString()),
			st::boxRowPadding);

		Ui::AddSkip(box->verticalLayout(), 4);

		const auto statusLabel = box->verticalLayout()->add(
			object_ptr<Ui::FlatLabel>(
				box->verticalLayout(),
				rpl::single(QString()),
				st::boxLabel),
			st::boxRowPadding);

		const auto weakBox = base::make_weak(box);

		const auto submitBtn = box->addButton(rpl::single(u"Qulfni ochish va ulanish"_q), [=] {
			const auto pass = passInput->getLastText();
			if (pass.isEmpty()) {
				statusLabel->setText(u"Iltimos, arxiv parolini kiriting."_q);
				return;
			}

			passInput->setEnabled(false);
			submitBtn->setEnabled(false);
			statusLabel->setText(u"Parol tekshirilmoqda (PBKDF2 hisoblanmoqda)…"_q);

			const auto proceedWithWrap = [=](const CustomSync::KeyShare::Wrap &wrapToUnwrap) {
				// 🔴 PBKDF2 fon oqimida (crl::async) bajariladi (UI qotib qolmasligi uchun):
				crl::async([wrapToUnwrap, pass, weakBox, submitBtn, passInput, statusLabel, onComplete] {
					auto unwrapped = CustomSync::KeyShare::UnwrapMasterKey(wrapToUnwrap, pass);
					crl::on_main([unwrapped = std::move(unwrapped), weakBox, submitBtn, passInput, statusLabel, onComplete] {
						if (!weakBox) return;

						// Parolni maydondan darhol tozalaymiz
						if (passInput) {
							passInput->setText(QString());
							passInput->setEnabled(true);
						}
						if (submitBtn) {
							submitBtn->setEnabled(true);
						}

						if (!unwrapped.has_value()) {
							if (statusLabel) {
								statusLabel->setText(u"Xato: Parol noto'g'ri yoki arxiv ma'lumotlari shikastlangan!"_q);
							}
							return;
						}

						const auto rawMasterKey = *unwrapped;
						const auto serverFp = CustomSync::Crypto::KeyFingerprint(rawMasterKey);
						const auto localFp = CustomSync::Outbox::KeyFingerprint();

						// 🔴 5. Mismatch holati: Ushbu qurilmada allaqachon boshqa kalit mavjud bo'lsa
						if (!CustomSync::Outbox::MasterKey().isEmpty() && !localFp.isEmpty() && localFp != serverFp) {
							if (statusLabel) {
								statusLabel->setText(
									u"⚠️ Ushbu qurilmada allaqachon boshqa lokal master kalit mavjud (FP: %1), "
									"serverdagi kalit esa boshqa (FP: %2).\n\n"
									"Bu qurilmaning o'z arxivi umumiy arxiv bilan avtomatik birlasha olmaydi. "
									"Mavjud ma'lumotlar xavfsizligi uchun lokal kalit saqlab qolindi (o'chirilmadi)."_q
									.arg(localFp, serverFp));
							}
							return;
						}

						// Master kalitni lokal bazaga qabul qilamiz
						const bool adopted = CustomSync::Outbox::AdoptMasterKey(rawMasterKey);
						if (!adopted) {
							if (statusLabel) {
								statusLabel->setText(u"Lokal kalitni qabul qilib bo'lmadi (boshqa kalit allaqachon mavjud)."_q);
							}
							return;
						}

						weakBox->closeBox();
						Ui::Toast::Show(u"Umumiy arxivga muvaffaqiyatli ulandi! Kalit barmoq izi (FP): %1 ✓"_q.arg(serverFp));
						if (onComplete) {
							onComplete();
						}
					});
				});
			};

			if (targetWrap.wrappedKey.isEmpty() && !targetWrap.wrapId.isEmpty()) {
				client->getKeyWrap(targetWrap.wrapId, [=](bool ok, CustomSync::KeyShare::Wrap fullWrap, QString error) {
					if (!weakBox) return;
					if (!ok) {
						if (passInput) passInput->setEnabled(true);
						if (submitBtn) submitBtn->setEnabled(true);
						if (statusLabel) {
							statusLabel->setText(u"Serverdan kalit ma'lumotlarini yuklab bo'lmadi: %1"_q.arg(error));
						}
						return;
					}
					proceedWithWrap(fullWrap);
				});
			} else {
				proceedWithWrap(targetWrap);
			}
		});

		box->addButton(rpl::single(u"Bekor qilish"_q), [=] {
			if (passInput) passInput->setText(QString());
			box->closeBox();
			if (onCancel) onCancel();
		});
	}));
}

// ─── B shoxcha: Yangi umumiy arxiv yaratish oynasi (Create) ──────────────────
void ShowCreateArchiveBox(
		QWidget *dialogParent,
		CustomSync::Client *client,
		const QString &deviceName,
		Fn<void()> onComplete,
		Fn<void()> onCancel) {
	ShowCustomBox(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"🔐 Yangi umumiy arxiv yaratish"_q));
		Ui::AddSkip(box->verticalLayout(), 6);

		// 🔴 Ogohlantirish: Parol unutilsa arxiv butunlay yo'qoladi
		box->verticalLayout()->add(
			object_ptr<Ui::FlatLabel>(
				box->verticalLayout(),
				rpl::single(u"⚠️ DIQQAT: Agar arxiv parolini unutsangiz, serverdagi barcha ma'lumotlar "
					"butunlay o'qib bo'lmas holga keladi va ularni tiklashning hech qanday imkoni bo'lmaydi!\n"
					"Tiklash kodlari hozircha mavjud emas, shuning uchun parolni xavfsiz joyda saqlang."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);

		Ui::AddSkip(box->verticalLayout(), 6);

		box->verticalLayout()->add(
			object_ptr<Ui::FlatLabel>(
				box->verticalLayout(),
				rpl::single(u"Yangi arxiv paroli (kamida %1 ta belgi):"_q.arg(kMinPassphraseLength)),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		const auto pass1Input = box->verticalLayout()->add(
			object_ptr<Ui::PasswordInput>(
				box->verticalLayout(),
				st::defaultInputField,
				rpl::single(u"Yangi parol"_q),
				QString()),
			st::boxRowPadding);

		Ui::AddSkip(box->verticalLayout(), 4);

		box->verticalLayout()->add(
			object_ptr<Ui::FlatLabel>(
				box->verticalLayout(),
				rpl::single(u"Parolni tasdiqlash:"_q),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		const auto pass2Input = box->verticalLayout()->add(
			object_ptr<Ui::PasswordInput>(
				box->verticalLayout(),
				st::defaultInputField,
				rpl::single(u"Parolni qayta kiriting"_q),
				QString()),
			st::boxRowPadding);

		Ui::AddSkip(box->verticalLayout(), 4);

		const auto statusLabel = box->verticalLayout()->add(
			object_ptr<Ui::FlatLabel>(
				box->verticalLayout(),
				rpl::single(QString()),
				st::boxLabel),
			st::boxRowPadding);

		const auto weakBox = base::make_weak(box);

		const auto submitBtn = box->addButton(rpl::single(u"Arxiv yaratish"_q), [=] {
			const auto pass1 = pass1Input->getLastText();
			const auto pass2 = pass2Input->getLastText();

			if (pass1.length() < kMinPassphraseLength) {
				statusLabel->setText(u"Parol juda qisqa (kamida %1 ta belgi bo'lishi shart)."_q.arg(kMinPassphraseLength));
				return;
			}
			if (pass1 != pass2) {
				statusLabel->setText(u"Kiritilgan ikkala parol bir-biriga mos kelmadi!"_q);
				return;
			}

			pass1Input->setEnabled(false);
			pass2Input->setEnabled(false);
			submitBtn->setEnabled(false);
			statusLabel->setText(u"Kalit yaratilmoqda va shifrlanmoqda (PBKDF2 hisoblanmoqda)…"_q);

			// 🔴 QAT'IY QOIDA: Kalit avvaldan diskka saqlanmaydi!
			// 32 bayt faqat xotirada (RAM) generatsiya qilinadi.
			const auto rawMasterKey = CustomSync::Crypto::RandomBytes(32);
			if (rawMasterKey.size() != 32) {
				pass1Input->setEnabled(true);
				pass2Input->setEnabled(true);
				submitBtn->setEnabled(true);
				statusLabel->setText(u"Xato: Tasodifiy kalit hosil qilib bo'lmadi."_q);
				return;
			}

			// 🔴 PBKDF2 fon oqimida (crl::async) bajariladi:
			crl::async([rawMasterKey, pass1, deviceName, weakBox, submitBtn, pass1Input, pass2Input, statusLabel, client, onComplete] {
				const auto wrap = CustomSync::KeyShare::WrapMasterKey(rawMasterKey, pass1, deviceName);
				crl::on_main([rawMasterKey, wrap, weakBox, submitBtn, pass1Input, pass2Input, statusLabel, client, onComplete] {
					if (!weakBox) return;

					// Parolni maydonlardan darhol tozalaymiz
					if (pass1Input) pass1Input->setText(QString());
					if (pass2Input) pass2Input->setText(QString());

					if (statusLabel) {
						statusLabel->setText(u"Kalit serverga yuborilmoqda (POST /api/v1/keys/wraps)…"_q);
					}

					client->createKeyWrap(wrap, [=](bool ok, QString wrapId, QString error) {
						if (!weakBox) return;

						if (!ok) {
							if (pass1Input) pass1Input->setEnabled(true);
							if (pass2Input) pass2Input->setEnabled(true);
							if (submitBtn) submitBtn->setEnabled(true);

							// 🔴 403 xatosi: admin roli talab qilinadi
							if (error == u"forbidden_admin_required"_q
								|| error.contains(u"403")
								|| error.contains(u"forbidden", Qt::CaseInsensitive)) {
								if (statusLabel) {
									statusLabel->setText(
										u"⛔ Xatolik (403): Yangi arxiv yaratish uchun qurilma admin kodi (--admin) "
										"bilan ro'yxatdan o'tgan bo'lishi shart! Oddiy qurilma arxiv yarata olmaydi."_q);
								}
							} else {
								if (statusLabel) {
									statusLabel->setText(u"Serverga yuklashda xatolik yuz berdi: %1"_q.arg(error));
								}
							}
							// DIQQAT: Hech qachon lokal kalit generatsiya qilinmaydi va qayta urinilmaydi!
							return;
						}

						// 🔴 FAQAT VA FAQAT POST muvaffaqiyatli bo'lgandan keyin lokal bazaga qabul qilamiz:
						const bool adopted = CustomSync::Outbox::AdoptMasterKey(rawMasterKey);
						if (!adopted) {
							if (statusLabel) {
								statusLabel->setText(u"Lokal kalitni saqlab bo'lmadi (boshqa kalit allaqachon mavjud)."_q);
							}
							return;
						}

						weakBox->closeBox();
						const auto fp = CustomSync::Outbox::KeyFingerprint();
						Ui::Toast::Show(u"Yangi arxiv muvaffaqiyatli yaratildi! Kalit barmoq izi (FP): %1 ✓"_q.arg(fp));
						if (onComplete) {
							onComplete();
						}
					});
				});
			});
		});

		box->addButton(rpl::single(u"Bekor qilish"_q), [=] {
			if (pass1Input) pass1Input->setText(QString());
			if (pass2Input) pass2Input->setText(QString());
			box->closeBox();
			if (onCancel) onCancel();
		});
	}));
}

} // namespace

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

	const auto keyStatusLabel = sStatus->add(
		object_ptr<Ui::FlatLabel>(
			sStatus,
			rpl::single(u"Kalit holati: Tekshirilmoqda..."_q),
			st::boxLabel),
		st::boxRowPadding);

	const auto keyFingerprintLabel = sStatus->add(
		object_ptr<Ui::FlatLabel>(
			sStatus,
			rpl::single(u"Kalit barmoq izi (FP): Tekshirilmoqda..."_q),
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
					"Barcha qurilmalarda bir xil arxiv parolidan foydalanilganiga va kalit barmoq izi (FP) "
					"mos kelishiga ishonch hosil qiling."_q),
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
	const auto hasDiagKey = !CustomSync::Outbox::MasterKey().isEmpty();
	const auto diagFp = CustomSync::Outbox::KeyFingerprint();
	sDiag->add(
		object_ptr<Ui::FlatLabel>(
			sDiag,
			rpl::single(hasDiagKey
				? u"Lokal master kalit: mavjud (FP: %1) ✓"_q.arg(diagFp)
				: u"Lokal master kalit: yo'q — ro'yxatdan o'ting"_q),
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

		const auto fp = CustomSync::Outbox::KeyFingerprint();
		const bool hasKey = !CustomSync::Outbox::MasterKey().isEmpty();
		if (hasKey && !fp.isEmpty()) {
			keyStatusLabel->setText(u"Kalit: mavjud (DPAPI bilan himoyalangan) ✓"_q);
			keyFingerprintLabel->setText(u"Kalit barmoq izi (FP): %1"_q.arg(fp));
		} else {
			keyStatusLabel->setText(u"Kalit: yo'q — ro'yxatdan o'ting"_q);
			keyFingerprintLabel->setText(u"Kalit barmoq izi (FP): yo'q"_q);
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
			&& hasKey
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
		if (CustomSync::Outbox::MasterKey().isEmpty()) {
			Ui::Toast::Show(u"Master kalit mavjud emas. Avval ro'yxatdan o'tib kalitni oling"_q);
			return;
		}
		syncNowBtn->setEnabled(false);
		CustomSync::SyncNow();
		Ui::Toast::Show(u"Sinxronlash so'rovi yuborildi ✓"_q);
		updateStatus(CustomSync::CurrentSyncStatus());
	});

	// ─── Ro'yxatdan o'tish tugmasi va Kalit almashish oqimi ────────────────────
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

		enrollBtn->setEnabled(false);
		enrollStatusLabel->setText(u"Serverga ulanmoqda..."_q);

		auto *client = new CustomSync::Client(content);
		client->enroll(url, code, deviceName, [=](bool success, QString error) {
			enrollBtn->setEnabled(true);
			if (!success) {
				client->deleteLater();
				enrollStatusLabel->setText(u"Ro'yxatdan o'tishda xatolik: %1"_q.arg(error));
				Ui::Toast::Show(u"Server xatosi: %1"_q.arg(error));
				return;
			}

			const auto newDevId = CustomSync::Outbox::GetState(QStringLiteral("device_id"));
			enrollStatusLabel->setText(u"Qurilma ro'yxatdan o'tdi ✓ (ID: %1). Serverdagi kalitlar tekshirilmoqda…"_q.arg(newDevId));

			// 🔴 TARTIB: Har doim birinchi bo'lib listKeyWraps() tekshiriladi!
			client->listKeyWraps([=](bool ok, QVector<CustomSync::KeyShare::Wrap> wraps, QString listError) {
				if (!ok) {
					client->deleteLater();
					enrollStatusLabel->setText(u"Kalitlarni tekshirishda xatolik: %1"_q.arg(listError));
					Ui::Toast::Show(u"Serverdan kalitlarni tekshirib bo'lmadi: %1"_q.arg(listError));
					return;
				}

				const auto onComplete = [=] {
					client->deleteLater();
					enrollStatusLabel->setText(u"Holat: Qurilma ro'yxatdan o'tgan ✓ (ID: %1)"_q.arg(newDevId));
					updateStatus(CustomSync::CurrentSyncStatus());
					if (CustomSettings::SyncEnabled()) {
						CustomSync::Start();
					}
				};

				const auto onCancel = [=] {
					client->deleteLater();
					enrollStatusLabel->setText(u"Qurilma ro'yxatdan o'tdi (ID: %1), lekin arxivga ulanish bekor qilindi"_q.arg(newDevId));
				};

				if (!wraps.isEmpty()) {
					// ── A shoxcha: Serverda o'ram mavjud -> Umumiy arxivga ulanish ──
					ShowJoinArchiveBox(dialogParent, client, wraps, onComplete, onCancel);
				} else {
					// ── B shoxcha: Serverda o'ram yo'q -> Yangi arxiv yaratish ──
					ShowCreateArchiveBox(dialogParent, client, deviceName, onComplete, onCancel);
				}
			});
		});
	});

	Ui::AddSkip(content, st::settingsThumbSkip);
}
