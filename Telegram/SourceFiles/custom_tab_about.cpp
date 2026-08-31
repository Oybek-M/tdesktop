#include "custom_tab_common.h"

void fillAboutTab(
		not_null<Ui::VerticalLayout*> content,
		QWidget *dialogParent,
		Fn<void()> onArchiveChanged) {
	// 2026-08-15: alpha build'da AppVersionStr rasmiy raqam bo'lib
	// qolaveradi ("7.0.9"), ya'ni ikkita custom build'ni ajratib
	// bo'lmaydi. Alpha raqami aynan shu uchun ko'rsatiladi.
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(AppAlphaVersion
				? (u"CustomMod %1 · alpha %2"_q
					.arg(QString::fromUtf8(AppVersionStr))
					.arg(int(AppAlphaVersion % 1000)))
				: (u"CustomMod %1"_q
					.arg(QString::fromUtf8(AppVersionStr)))),
			st::customModHintLabel),
		st::boxRowPadding);

	const auto stats0 = CustomDB::GetArchiveStats();
	const auto statsLabel = content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(
				u"🗄️ Arxiv holati: %1 o'chirilgan • %2 tahrirlangan"_q
				.arg(stats0.deletedCount).arg(stats0.editedCount)),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	// Arxiv statistikasini yangilash uchun yordamchi.
	const auto refreshStats = [statsLabel]() {
		const auto s = CustomDB::GetArchiveStats();
		statsLabel->setText(
			u"🗄️ Arxiv holati: %1 o'chirilgan • %2 tahrirlangan"_q
				.arg(s.deletedCount).arg(s.editedCount));
	};

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"📦 Zaxira nusxa"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Eksport ZIP fayli QUYIDAGILARNI saqlaydi:\n"
				"• O'chirilgan/tahrirlangan xabarlar bazasi\n"
				"• Bomb media (rasm/video)\n"
				"• White/Black List (peer_lists.json)\n"
				"• Branding sozlamalari (branding.json)\n"
				"• Ghost/AntiDelete/AntiEdit togglelar va Per-Chat (registry)\n\n"
				"Import — barchasini tiklaydi va dastur AVTOMATIK qayta yuklanadi."_q),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	// ── Arxiv papkasi (2026-08-15) ─────────────────────────────
	// AntiDelete va zaxira uchun saqlanadigan HAMMA narsa shu ildiz
	// ostida: medias/, db/, config/, backups/. Ilgari ular ikkiga
	// bo'lingan edi (~/customizationMainFolder va %APPDATA%/CustomMod).
	Ui::AddSkip(content, 8);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Arxiv papkasi"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Barcha saqlanadigan ma'lumotlar shu yerda: "_q
				+ CustomSettings::ArchiveRoot()
				+ u"  —  ichida: medias/ (fayllar), db/ (baza), "
				  "config/ (sozlamalar), backups/ (avtomatik zaxiralar), "
				  "bombmedia/."_q),
			st::customModHintLabel),
		st::boxRowPadding);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"📁 Papkani o'zgartirish"_q),
			st::defaultBoxButton),
		st::boxRowPadding
	)->addClickHandler([=] {
		const auto current = CustomSettings::ArchiveRoot();
		const auto chosen = QFileDialog::getExistingDirectory(
			dialogParent,
			u"Arxiv uchun papka tanlang"_q,
			current);
		if (chosen.isEmpty()
			|| QDir::cleanPath(chosen) == QDir::cleanPath(current)) {
			return;
		}

		QMessageBox moveBox(dialogParent);
		moveBox.setWindowTitle(u"Arxiv papkasini o'zgartirish"_q);
		moveBox.setText(
			u"Eski: "_q + current
			+ u"\nYangi: "_q + QDir::cleanPath(chosen)
			+ u"\n\nMavjud ma'lumotlarni yangi papkaga "
			  "ko'chiraymi?"_q);
		const auto moveBtn = moveBox.addButton(
			u"📦 Ko'chirish"_q, QMessageBox::AcceptRole);
		moveBox.addButton(
			u"Ko'chirmasdan, yangi joydan boshlansin"_q,
			QMessageBox::DestructiveRole);
		moveBox.addButton(QMessageBox::Cancel);
		moveBox.setDefaultButton(moveBtn);
		moveBox.exec();

		const auto role = moveBox.buttonRole(moveBox.clickedButton());
		if (role != QMessageBox::AcceptRole
			&& role != QMessageBox::DestructiveRole) {
			return; // Cancel
		}
		if (role == QMessageBox::AcceptRole) {
			// Ko'chirishning O'ZI keyingi ishga tushishda bajariladi:
			// hozir baza ochiq va uni ko'chirish xavfli bo'lardi.
			CustomSettings::ScheduleArchiveRootMove(current);
		}
		CustomSettings::SetArchiveRoot(chosen);
		Ui::Toast::Show(
			(role == QMessageBox::AcceptRole)
				? u"Papka o'zgartirildi. Ma'lumotlar qayta ishga "
				  "tushganda ko'chiriladi. Dastur 3 soniyada qayta "
				  "yuklanadi..."_q
				: u"Papka o'zgartirildi. Dastur 3 soniyada qayta "
				  "yuklanadi..."_q);
		QTimer::singleShot(3000, [] { Core::Restart(); });
	});

	// ── Katta media backup sozlamalari (2026-08-14) ─────────────────────
	Ui::AddSkip(content, 8);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Katta media backup"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	{
		const auto used = CustomMediaQuota::UsedBytes();
		const auto limit = CustomMediaQuota::LimitBytes();
		const auto gb = [](long long bytes) {
			return QString::number(
				double(bytes) / (1024.0 * 1024 * 1024), 'f', 1);
		};
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"White List'dagi yoki 'Media Backup' yoqilgan "
					"chatlarda media oldindan yuklab olinadi.\n"
					"Ishlatilgan: "_q + gb(used) + u" GB / "_q
					+ gb(limit) + u" GB"_q),
				st::customModHintLabel),
			st::boxRowPadding);

		const auto maxInput = content->add(
			object_ptr<Ui::InputField>(
				content,
				st::defaultInputField,
				rpl::single(u"Bitta fayl chegarasi, MB (10–4096)"_q),
				QString::number(CustomSettings::MediaBackupMaxFileMb())),
			st::boxRowPadding);
		// Kvota ichkarida MB'da saqlanadi, UI'da esa GB — shuning uchun
		// "5.5" kabi kasrli qiymat kiritish mumkin.
		const auto quotaInput = content->add(
			object_ptr<Ui::InputField>(
				content,
				st::defaultInputField,
				rpl::single(u"Umumiy kvota, GB (0.1–500, kasr mumkin: 5.5)"_q),
				QString::number(
					CustomSettings::MediaBackupQuotaMb() / 1024.0, 'f', 2)),
			st::boxRowPadding);
		content->add(
			object_ptr<Ui::RoundButton>(
				content,
				rpl::single(u"💾 Saqlash"_q),
				st::defaultBoxButton),
			st::boxRowPadding
		)->addClickHandler([=] {
			auto ok = false;
			const auto maxMb = maxInput->getLastText().trimmed().toInt(&ok);
			if (ok) {
				// Chegaralar CustomSettings::UpdateInt() da qisiladi.
				CustomSettings::SetInt(u"mediaBackupMaxFileMb"_q, maxMb);
			}
			// Kasrli GB qabul qilamiz ("5.5"), MB'ga aylantirib saqlaymiz.
			// Vergul ham ishlaydi — klaviatura tilida nuqta o'rniga
			// vergul chiqishi odatiy hol.
			auto quotaText = quotaInput->getLastText().trimmed();
			quotaText.replace(u',', u'.');
			const auto quotaGb = quotaText.toDouble(&ok);
			if (ok && quotaGb > 0) {
				CustomSettings::SetInt(
					u"mediaBackupQuotaMb"_q,
					int(std::lround(quotaGb * 1024)));
			}
			maxInput->setText(
				QString::number(CustomSettings::MediaBackupMaxFileMb()));
			quotaInput->setText(
				QString::number(
					CustomSettings::MediaBackupQuotaMb() / 1024.0, 'f', 2));
			Ui::Toast::Show(u"Saqlandi ✓"_q);
		});

		// ── Bir martalik backfill skaneri ───────────────────────────
		// media_index v7 da paydo bo'ldi, ya'ni undan OLDIN arxivlangan
		// fayllar indeksda yo'q va shuning uchun eksportga tushmaydi.
		// Dalillar yo'qolmasligi uchun ularni indeksga kiritish kerak.
		const auto scanBtn = content->add(
			object_ptr<Ui::RoundButton>(
				content,
				rpl::single(u"🔍 Eski media fayllarni indekslash va tuzatish"_q),
				st::defaultBoxButton),
			st::boxRowPadding);
		scanBtn->addClickHandler([=] {
			scanBtn->setDisabled(true);
			Ui::Toast::Show(u"Skanerlanmoqda, biroz kuting..."_q);
			const auto weak = base::make_weak(content);

			// 1-bosqich: indeksga yetishmayotgan fayllarni qo'shish.
			CustomDB::ScanArchiveMediaAsync([=](int added) {
				if (!weak) return;

				// 2-bosqich: TUZATISHNI avval quruq (dry-run) chopamiz.
				// Fayllarni qayta nomlash va ko'chirish qaytarib
				// bo'lmaydigan amal, shuning uchun foydalanuvchi avval
				// nima o'zgarishini KO'RADI va o'zi tasdiqlaydi.
				CustomDB::RepairArchiveMediaAsync(true, [=](
						CustomDB::RepairReport preview) {
					if (!weak) return;
					scanBtn->setDisabled(false);

					const auto indexed = (added > 0)
						? (u"%1 ta fayl indeksga qo'shildi.\n\n"_q).arg(added)
						: u"Indeksga yangi fayl qo'shilmadi.\n\n"_q;
					const auto toFix = preview.extensionAdded
						+ preview.movedFolder;
					if (!toFix) {
						Ui::Toast::Show(indexed
							+ u"Tuzatishga muhtoj fayl topilmadi."_q);
						return;
					}
					const auto window = Core::App().activeWindow();
					if (!window) return;
					window->show(Ui::MakeConfirmBox({
						.text = indexed
							+ u"Tuzatish kerak bo'lgan fayllar:\n"_q
							+ u"• kengaytma qo'shiladi: "_q
							+ QString::number(preview.extensionAdded)
							+ u"\n• to'g'ri papkaga ko'chiriladi: "_q
							+ QString::number(preview.movedFolder)
							+ u"\n• turi aniqlanmadi (tegilmaydi): "_q
							+ QString::number(preview.unknown)
							+ u"\n\nTur fayl MAZMUNIDAN aniqlanadi. "
							  "Mavjud kengaytma hech qachon "
							  "almashtirilmaydi. Davom etamizmi?"_q,
						.confirmed = [=](Fn<void()> close) {
							close();
							Ui::Toast::Show(u"Tuzatilmoqda..."_q);
							CustomDB::RepairArchiveMediaAsync(false, [=](
									CustomDB::RepairReport done) {
								Ui::Toast::Show(
									u"Tuzatildi: %1 kengaytma, %2 ko'chirildi"
									", %3 xato."_q
										.arg(done.extensionAdded)
										.arg(done.movedFolder)
										.arg(done.failed));
							});
						},
						.confirmText = u"Tuzatish"_q,
					}));
				});
			});
		});
	}

	// ── Media eksport tanlovi (2026-08-14) ──────────────────────────────
	//
	// Ro'yxat media_index dan tuziladi — ya'ni faqat HAQIQATAN media'si
	// bor chatlar ko'rinadi (odatda 3-7 qator, yuzlab emas), har birida
	// hajmi bilan. Shunda "bu 8 GB, keyingi safar" deb qaror qilish
	// mumkin. Bu "Individual sozlamalar"dan olinmaydi: u toggle
	// ARXIVLASHNI boshqaradi, EKSPORTNI emas — 10 ta chatni arxivlab
	// ulardan 2 tasini eksport qilish mutlaqo normal.
	Ui::AddSkip(content, 8);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Media eksporti"_q),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	struct MediaExportState {
		bool includeAll = false;
		// QStringList (QVector<QString> emas) — join() kerak. Qt6'da
		// QVector = QList, shuning uchun ExportOptions::mediaPeerIds ga
		// to'g'ridan-to'g'ri berish mumkin.
		QStringList selected;
		long long allBytes = 0;
		Ui::FlatLabel *totalLabel = nullptr;
	};
	const auto mediaState = content->lifetime()
		.make_state<MediaExportState>();

	const auto summaries = CustomDB::GetMediaPeerSummaries();
	for (const auto &s : summaries) {
		mediaState->allBytes += s.totalBytes;
	}
	const auto formatSize = [](long long bytes) {
		if (bytes >= 1024LL * 1024 * 1024) {
			return QString::number(
				double(bytes) / (1024.0 * 1024 * 1024), 'f', 1) + u" GB"_q;
		}
		return QString::number(
			double(bytes) / (1024.0 * 1024), 'f', 1) + u" MB"_q;
	};

	// Oxirgi tanlovni eslab qolamiz — takroriy eksport bir bosishda.
	{
		QSettings s("CustomMod", "TelegramDesktop");
		mediaState->includeAll = s.value("mediaExportIncludeAll", false).toBool();
		const auto saved = s.value("mediaExportPeers", QString()).toString();
		if (!saved.isEmpty()) {
			mediaState->selected = saved.split(u',', Qt::SkipEmptyParts);
		}
	}
	const auto saveSelection = [=] {
		QSettings s("CustomMod", "TelegramDesktop");
		s.setValue("mediaExportIncludeAll", mediaState->includeAll);
		s.setValue("mediaExportPeers", mediaState->selected.join(u','));
	};

	const auto selectedBytes = [=] {
		if (mediaState->includeAll) return mediaState->allBytes;
		auto total = 0LL;
		for (const auto &s : summaries) {
			if (mediaState->selected.contains(s.peerId)) total += s.totalBytes;
		}
		return total;
	};

	if (summaries.isEmpty()) {
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Arxivda media fayllar yo'q — eksport faqat "
					"indeks va sozlamalarni oladi."_q),
				st::customModHintLabel),
			st::boxRowPadding);
	} else {
		const auto totalLabel = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(QString()),
				st::customModHintLabel),
			st::boxRowPadding);
		mediaState->totalLabel = totalLabel;
		const auto refreshTotal = [=] {
			const auto bytes = selectedBytes();
			totalLabel->setText(bytes > 0
				? (u"Tanlangan: "_q + formatSize(bytes))
				: u"Tanlangan: yo'q — faqat indeks eksport qilinadi"_q);
		};
		refreshTotal();

		const auto allBtn = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(u"Hammasi ("_q
					+ formatSize(mediaState->allBytes) + u")"_q),
				st::settingsButtonNoIcon));
		allBtn->toggleOn(rpl::single(mediaState->includeAll));
		allBtn->toggledValue()
			| rpl::skip(1)
			| rpl::on_next([=](bool on) {
				mediaState->includeAll = on;
				saveSelection();
				refreshTotal();
			}, allBtn->lifetime());

		for (const auto &summary : summaries) {
			auto name = CustomSettings::GetPeerDisplayName(summary.peerId);
			if (summary.peerId == u"0"_q) {
				// Backfill skaneri: fayl nomidan peer aniqlanmagan eski
				// fayllar shu guruhda. Ular ham eksport qilinishi kerak.
				name = u"Noma'lum (eski fayllar)"_q;
			} else if (name.isEmpty()) {
				// 2026-08-15: GetPeerDisplayName() faqat White/Black List
				// va per-chat ro'yxatlariga qaraydi, shuning uchun oddiy
				// shaxsiy chatlar "ID 620565940" bo'lib chiqardi.
				// Haqiqiy nomni sessiyadan olamiz.
				auto ok = false;
				const auto rawId = summary.peerId.toULongLong(&ok);
				if (ok && rawId) {
					if (const auto window = Core::App().activeWindow()) {
						if (const auto c = window->sessionController()) {
							const auto peer = c->session().data().peerLoaded(
								PeerId(rawId));
							if (peer) {
								name = peer->name();
								// Keyingi safar sessiyaga bog'liq
								// bo'lmasin — nomni saqlab qo'yamiz.
								CustomSettings::RememberPeerName(
									summary.peerId, name);
							}
						}
					}
				}
				if (name.isEmpty()) {
					name = u"ID "_q + summary.peerId; // hali yuklanmagan
				}
			}
			const auto btn = content->add(
				object_ptr<Ui::SettingsButton>(
					content,
					rpl::single(name
						+ u"  —  "_q + formatSize(summary.totalBytes)
						+ u" ("_q + QString::number(summary.fileCount)
						+ u" fayl)"_q),
					st::settingsButtonNoIcon));
			// Ikkinchi qator: ID kichik shriftda, nom ostida.
			// SettingsButton bir qatorli, shuning uchun alohida label —
			// bu to'liq custom widget yozishdan ancha arzon va vizual
			// natija bir xil.
			if (summary.peerId != u"0"_q) {
				content->add(
					object_ptr<Ui::FlatLabel>(
						content,
						rpl::single(u"ID: "_q + summary.peerId),
						st::customModHintLabel),
					st::boxRowPadding);
			}
			const auto peerId = summary.peerId;
			btn->toggleOn(rpl::single(mediaState->selected.contains(peerId)));
			btn->toggledValue()
				| rpl::skip(1)
				| rpl::on_next([=](bool on) {
					mediaState->selected.removeAll(peerId);
					if (on) mediaState->selected.append(peerId);
					saveSelection();
					refreshTotal();
				}, btn->lifetime());
		}
	}
	Ui::AddSkip(content, 8);

	const auto exportBtn = content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"📤 To'liq zaxira nusxa olish"_q),
			st::defaultBoxButton),
		st::boxRowPadding);
	exportBtn->addClickHandler([=] {
		const auto dir = QFileDialog::getExistingDirectory(
			dialogParent,
			u"Saqlash papkasini tanlang"_q,
			QDir::homePath());
		if (dir.isEmpty()) return;

		// Eksport fon (background) thread'da ishlaydi — UI qotib qolmaydi.
		exportBtn->setDisabled(true);
		Ui::Toast::Show(u"Zaxira nusxa olinmoqda, biroz kuting..."_q);

		auto options = CustomDB::ExportOptions();
		options.includeAllMedia = mediaState->includeAll;
		if (!mediaState->includeAll) {
			options.mediaPeerIds = mediaState->selected;
		}

		const auto weak = base::make_weak(content);
		CustomDB::ExportFullBackupAsync(
			dir,
			options,
			[=](const CustomDB::ExportResult &result) {
				if (!weak) return; // Oyna yopilgan bo'lishi mumkin.
				exportBtn->setDisabled(false);
				if (result.mainZipPath.isEmpty()) {
					Ui::Toast::Show(u"Eksport amalga oshmadi."_q);
				} else if (result.mediaZipPath.isEmpty()) {
					Ui::Toast::Show(u"Eksport saqlandi (media'siz): "_q
						+ result.mainZipPath);
				} else {
					Ui::Toast::Show(u"Eksport saqlandi: "_q
						+ result.mainZipPath
						+ u"\n+ media: "_q + result.mediaZipPath);
				}
			},
			[=](const QString &stage, int percent) {
				if (!weak) return;
				Ui::Toast::Show(u"%1... (%2%)"_q.arg(stage).arg(percent));
			});
	});

	const auto importBtn = content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"📥 Zaxira nusxadan tiklash"_q),
			st::defaultBoxButton),
		st::boxRowPadding);
	importBtn->addClickHandler([=] {
		const auto path = QFileDialog::getOpenFileName(
			dialogParent,
			u"Zaxira faylini tanlang (.zip)"_q,
			QDir::homePath(),
			u"Zaxira fayllari (*.zip);;Barcha fayllar (*)"_q);
		const auto source = path.isEmpty()
			? QFileDialog::getExistingDirectory(
				dialogParent,
				u"Zaxira papkasini tanlang"_q,
				QDir::homePath())
			: path;
		if (source.isEmpty()) return;

		// Rejim tanlash: Merge (birlashtirish) yoki To'liq almashtirish.
		QMessageBox modeBox(dialogParent);
		modeBox.setWindowTitle(u"Zaxiradan tiklash rejimi"_q);
		modeBox.setText(u"Tiklash rejimini tanlang:"_q);
		const auto mergeBtn = modeBox.addButton(
			u"🔗 Birlashtirish"_q, QMessageBox::AcceptRole);
		modeBox.addButton(
			u"🔄 To'liq almashtirish"_q, QMessageBox::DestructiveRole);
		modeBox.addButton(QMessageBox::Cancel);
		modeBox.setDefaultButton(mergeBtn);
		modeBox.exec();

		// buttonRole() orqali solishtirish — QPushButton*/QAbstractButton*
		// pointer taqqoslashdan ko'ra ishonchliroq (MSVC ba'zan bu ikki tur
		// orasidagi standart yuqoriga cast'ni comparison operatorида
		// tan olmaydi, garchi QPushButton QAbstractButton'dan meros bo'lsa ham).
		const auto role = modeBox.buttonRole(modeBox.clickedButton());
		if (role != QMessageBox::AcceptRole
				&& role != QMessageBox::DestructiveRole) {
			return; // Cancel yoki oyna yopildi.
		}
		const bool fullReplace = (role == QMessageBox::DestructiveRole);

		const auto reply = QMessageBox::warning(
			dialogParent,
			u"Zaxiradan tiklash"_q,
			fullReplace
				? u"DIQQAT! Joriy arxivdagi BARCHA ma'lumotlar\n"
				  "(o'chirilgan/tahrirlangan xabarlar, media) O'CHIRILADI\n"
				  "va tanlangan zaxira bilan to'liq almashtiriladi.\n\n"
				  "Bu amalni ortga qaytarib bo'lmaydi. Davom etasizmi?"_q
				: u"Tanlangan zaxiradagi ma'lumotlar JORIY arxivga QO'SHILADI\n"
				  "(birlashtiriladi) — hozirgi qurilmadagi o'chirilgan/tahrirlangan\n"
				  "xabarlar va media saqlanib qoladi, o'chirilmaydi.\n\n"
				  "Davom etasizmi?"_q,
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (reply != QMessageBox::Yes) return;

		// Tiklash fon (background) thread'da ishlaydi — UI qotib qolmaydi.
		importBtn->setDisabled(true);
		Ui::Toast::Show(u"Zaxiradan tiklanmoqda, biroz kuting..."_q);

		const auto weak = base::make_weak(content);
		CustomDB::ImportFullBackupAsync(source, fullReplace, [=](bool ok) {
			if (!weak) return; // Oyna yopilgan bo'lishi mumkin.
			importBtn->setDisabled(false);
			if (ok) {
				refreshStats();
				if (onArchiveChanged) onArchiveChanged();
				const auto s = CustomDB::GetArchiveStats();
				Ui::Toast::Show(
					(fullReplace
						? u"To'liq almashtirish muvaffaqiyatli! "_q
						: u"Tiklash muvaffaqiyatli! "_q)
					+ u"%1 o'chirilgan, %2 tahrirlangan. "
					  "Dastur 3 soniya ichida qayta yuklanadi..."_q
						.arg(s.deletedCount).arg(s.editedCount));
				// Registry va JSON sozlamalar yangi qiymatlar bilan to'liq
				// qo'llanishi uchun avtomatik restart. 3 soniya — foydalanuvchi
				// toast o'qisin uchun.
				QTimer::singleShot(3000, [] { Core::Restart(); });
			} else {
				Ui::Toast::Show(u"Tiklash amalga oshmadi. Fayl/papkani tekshiring."_q);
			}
		});
	});

	// ── Media arxivini alohida import qilish (2026-08-14) ───────────────
	// Eksport ikkita faylga bo'linadi, shuning uchun media'ni asosiy
	// tiklashdan KEYIN, istalgan vaqtda qo'shish mumkin bo'lishi kerak.
	// Qayta ishga tushirish shart emas — faqat fayllar joyiga qo'yiladi
	// va indeks 'present' ga qaytariladi.
	const auto importMediaBtn = content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"🎞 Media arxivini qo'shish (CustomModMedia_*.zip)"_q),
			st::defaultBoxButton),
		st::boxRowPadding);
	importMediaBtn->addClickHandler([=] {
		const auto path = QFileDialog::getOpenFileName(
			dialogParent,
			u"Media arxivini tanlang (.zip)"_q,
			QDir::homePath(),
			u"Media arxivi (CustomModMedia_*.zip);;Barcha fayllar (*)"_q);
		if (path.isEmpty()) return;

		importMediaBtn->setDisabled(true);
		Ui::Toast::Show(u"Media arxivi ochilmoqda, biroz kuting..."_q);

		const auto weak = base::make_weak(content);
		CustomDB::ImportMediaArchiveAsync(path, [=](bool ok) {
			if (!weak) return;
			importMediaBtn->setDisabled(false);
			Ui::Toast::Show(ok
				? u"Media arxivi qo'shildi ✓"_q
				: u"Media arxivini qo'shib bo'lmadi."_q);
		});
	});

	Ui::AddSkip(content, st::settingsThumbSkip);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"📊 Arxiv boshqaruvi"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"💣 Vaqtinchalik media papkasi"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto bombDir =
			CustomSettings::ArchiveRoot()
			+ u"/bombmedia/"_q; // 2026-08-15: yagona arxiv ildizi ostida
		QDir().mkpath(bombDir);
		QDesktopServices::openUrl(QUrl::fromLocalFile(bombDir));
	});

	Ui::AddSkip(content, st::settingsThumbSkip);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"⚠️  XAVFLI HUDUD  ⚠️"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Quyidagi amallar arxiv ma'lumotlarini BUTUNLAY o'chiradi.\n"
				"Bu amalni bekor qilib bo'lmaydi. Tasdiqlash talab qilinadi."_q),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
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
		refreshStats();
		if (onArchiveChanged) onArchiveChanged();
		Ui::Toast::Show(u"🗑️ O'chirilganlar arxivi tozalandi."_q);
	});

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
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
		refreshStats();
		if (onArchiveChanged) onArchiveChanged();
		Ui::Toast::Show(u"✏️ Tahrir tarixi arxivi tozalandi."_q);
	});

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
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
		refreshStats();
		if (onArchiveChanged) onArchiveChanged();
		Ui::Toast::Show(u"☠️ Barcha arxiv ma'lumotlari tozalandi."_q);
	});

	Ui::AddSkip(content, st::settingsThumbSkip);
}
