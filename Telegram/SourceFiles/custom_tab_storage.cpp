#include "custom_tab_common.h"

void fillStorageTab(
		not_null<Ui::VerticalLayout*> content,
		QWidget *dialogParent,
		Fn<void()> onArchiveChanged) {
	// 1-bo'lim: Zaxira nusxa (Standart ochiq)
	const auto s1 = AddCollapsibleSection(content, u"📦 Zaxira nusxa"_q, true);

	const auto stats0 = CustomDB::GetArchiveStats();
	const auto statsLabel = s1->add(
		object_ptr<Ui::FlatLabel>(
			s1,
			rpl::single(
				u"🗄️ Arxiv holati: %1 o'chirilgan • %2 tahrirlangan"_q
				.arg(stats0.deletedCount).arg(stats0.editedCount)),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	const auto refreshStats = [statsLabel]() {
		const auto s = CustomDB::GetArchiveStats();
		statsLabel->setText(
			u"🗄️ Arxiv holati: %1 o'chirilgan • %2 tahrirlangan"_q
				.arg(s.deletedCount).arg(s.editedCount));
	};

	s1->add(
		object_ptr<Ui::FlatLabel>(
			s1,
			rpl::single(u"Baza, media, ro'yxatlar va sozlamalarni ZIP zaxiraga saqlash va tiklash."_q),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	Ui::AddSkip(s1, 8);
	s1->add(
		object_ptr<Ui::FlatLabel>(
			s1,
			rpl::single(u"Arxiv joylashuvi: "_q + CustomSettings::ArchiveRoot()),
			st::customModHintLabel),
		st::boxRowPadding);
	s1->add(
		object_ptr<Ui::RoundButton>(
			s1,
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
			+ u"\n\nMavjud ma'lumotlarni yangi papkaga ko'chiraymi?"_q);
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
			return;
		}
		if (role == QMessageBox::AcceptRole) {
			CustomSettings::ScheduleArchiveRootMove(current);
		}
		CustomSettings::SetArchiveRoot(chosen);
		Ui::Toast::Show(
			(role == QMessageBox::AcceptRole)
				? u"Papka o'zgartirildi. Ma'lumotlar qayta ishga tushganda ko'chiriladi. Dastur 3 soniyada qayta yuklanadi..."_q
				: u"Papka o'zgartirildi. Dastur 3 soniyada qayta yuklanadi..."_q);
		QTimer::singleShot(3000, [] { Core::Restart(); });
	});

	// 2-bo'lim: Media backup (Standart yopiq)
	const auto s2 = AddCollapsibleSection(content, u"🎞 Media backup"_q, false);
	{
		const auto used = CustomMediaQuota::UsedBytes();
		const auto limit = CustomMediaQuota::LimitBytes();
		const auto gb = [](long long bytes) {
			return QString::number(
				double(bytes) / (1024.0 * 1024 * 1024), 'f', 1);
		};
		s2->add(
			object_ptr<Ui::FlatLabel>(
				s2,
				rpl::single(u"White List'dagi yoki 'Media Backup' yoqilgan chatlarda media oldindan yuklab olinadi.\nIshlatilgan: "_q
					+ gb(used) + u" GB / "_q + gb(limit) + u" GB"_q),
				st::customModHintLabel),
			st::boxRowPadding);

		const auto maxInput = s2->add(
			object_ptr<Ui::InputField>(
				s2,
				st::defaultInputField,
				rpl::single(u"Bitta fayl chegarasi, MB (10–4096)"_q),
				QString::number(CustomSettings::MediaBackupMaxFileMb())),
			st::boxRowPadding);
		const auto quotaInput = s2->add(
			object_ptr<Ui::InputField>(
				s2,
				st::defaultInputField,
				rpl::single(u"Umumiy kvota, GB (0.1–500, kasr mumkin: 5.5)"_q),
				QString::number(
					CustomSettings::MediaBackupQuotaMb() / 1024.0, 'f', 2)),
			st::boxRowPadding);
		s2->add(
			object_ptr<Ui::RoundButton>(
				s2,
				rpl::single(u"💾 Saqlash"_q),
				st::defaultBoxButton),
			st::boxRowPadding
		)->addClickHandler([=] {
			auto ok = false;
			const auto maxMb = maxInput->getLastText().trimmed().toInt(&ok);
			if (ok) {
				CustomSettings::SetInt(u"mediaBackupMaxFileMb"_q, maxMb);
			}
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

		const auto addStorageToggle = [&](
				const QString &id,
				const QString &text,
				const QString &description) {
			const auto &val = CustomSettings::Get();
			auto current = false;
			if (id == u"mediaBackupVoice"_q) current = val.mediaBackupVoice;
			else if (id == u"mediaBackupVideoNote"_q) current = val.mediaBackupVideoNote;
			else if (id == u"mediaBackupPhoto"_q) current = val.mediaBackupPhoto;
			else if (id == u"mediaBackupVideo"_q) current = val.mediaBackupVideo;
			else if (id == u"mediaBackupDocument"_q) current = val.mediaBackupDocument;
			else if (id == u"mediaQuotaAutoClean"_q) current = val.mediaQuotaAutoClean;

			const auto btn = s2->add(
				object_ptr<Ui::SettingsButton>(
					s2,
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
				const auto descLabel = s2->add(
					object_ptr<Ui::FlatLabel>(
						s2,
						rpl::single(description),
						st::customModHintLabel),
					st::boxRowPadding,
					style::al_justify);
				s2->widthValue() | rpl::on_next([=](int w) {
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

		addStorageToggle(
			u"mediaQuotaAutoClean"_q,
			u"Avtomatik kvota tozalash"_q,
			u"Limit to'lganda eng eski medialarni avtomatik o'chiradi."_q);

		addStorageToggle(u"mediaBackupPhoto"_q, u"Rasmlar zaxirasi"_q, QString());
		addStorageToggle(u"mediaBackupVideo"_q, u"Videolar zaxirasi"_q, QString());
		addStorageToggle(u"mediaBackupVoice"_q, u"Ovozli xabarlar zaxirasi"_q, QString());
		addStorageToggle(u"mediaBackupVideoNote"_q, u"Video xabarlar (dumaloq) zaxirasi"_q, QString());
		addStorageToggle(u"mediaBackupDocument"_q, u"Hujjatlar zaxirasi"_q, QString());
	}

	// 3-bo'lim: Indekslash va tuzatish (Standart yopiq)
	const auto s3 = AddCollapsibleSection(content, u"🔍 Indekslash va tuzatish"_q, false);
	{
		const auto scanBtn = s3->add(
			object_ptr<Ui::RoundButton>(
				s3,
				rpl::single(u"🔍 Eski media fayllarni indekslash va tuzatish"_q),
				st::defaultBoxButton),
			st::boxRowPadding);
		scanBtn->addClickHandler([=] {
			scanBtn->setDisabled(true);
			Ui::Toast::Show(u"Skanerlanmoqda, biroz kuting..."_q);
			const auto weak = base::make_weak(content);

			CustomDB::ScanArchiveMediaAsync([=](int added) {
				if (!weak) return;

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

	// 4-bo'lim: Eksport va tiklash (Standart yopiq)
	const auto s4 = AddCollapsibleSection(content, u"📤 Eksport va tiklash"_q, false);
	{
		struct MediaExportState {
			bool includeAll = false;
			QStringList selected;
			long long allBytes = 0;
			Ui::FlatLabel *totalLabel = nullptr;
		};
		const auto mediaState = s4->lifetime().make_state<MediaExportState>();

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
			s4->add(
				object_ptr<Ui::FlatLabel>(
					s4,
					rpl::single(u"Arxivda media fayllar yo'q — eksport faqat indeks va sozlamalarni oladi."_q),
					st::customModHintLabel),
				st::boxRowPadding);
		} else {
			const auto totalLabel = s4->add(
				object_ptr<Ui::FlatLabel>(
					s4,
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

			const auto allBtn = s4->add(
				object_ptr<Ui::SettingsButton>(
					s4,
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
					name = u"Noma'lum (eski fayllar)"_q;
				} else if (name.isEmpty()) {
					auto ok = false;
					const auto rawId = summary.peerId.toULongLong(&ok);
					if (ok && rawId) {
						if (const auto window = Core::App().activeWindow()) {
							if (const auto c = window->sessionController()) {
								const auto peer = c->session().data().peerLoaded(PeerId(rawId));
								if (peer) {
									name = peer->name();
									CustomSettings::RememberPeerName(summary.peerId, name);
								}
							}
						}
					}
					if (name.isEmpty()) {
						name = u"ID "_q + summary.peerId;
					}
				}
				const auto btn = s4->add(
					object_ptr<Ui::SettingsButton>(
						s4,
						rpl::single(name
							+ u"  —  "_q + formatSize(summary.totalBytes)
							+ u" ("_q + QString::number(summary.fileCount)
							+ u" fayl)"_q),
						st::settingsButtonNoIcon));
				if (summary.peerId != u"0"_q) {
					s4->add(
						object_ptr<Ui::FlatLabel>(
							s4,
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

		Ui::AddSkip(s4, 8);
		const auto exportFullBtn = s4->add(
			object_ptr<Ui::RoundButton>(
				s4,
				rpl::single(u"📤 To'liq zaxira nusxa olish"_q),
				st::defaultBoxButton),
			st::boxRowPadding);
		exportFullBtn->addClickHandler([=] {
			const auto dir = QFileDialog::getExistingDirectory(
				dialogParent,
				u"Saqlash papkasini tanlang"_q,
				QDir::homePath());
			if (dir.isEmpty()) return;

			exportFullBtn->setDisabled(true);
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
					if (!weak) return;
					exportFullBtn->setDisabled(false);
					if (result.mainZipPath.isEmpty()) {
						Ui::Toast::Show(u"Eksport amalga oshmadi."_q);
					} else if (result.mediaZipPath.isEmpty()) {
						Ui::Toast::Show(u"Eksport saqlandi (media'siz): "_q + result.mainZipPath);
					} else {
						Ui::Toast::Show(u"Eksport saqlandi: "_q + result.mainZipPath + u"\n+ media: "_q + result.mediaZipPath);
					}
				},
				[=](const QString &stage, int percent) {
					if (!weak) return;
					Ui::Toast::Show(u"%1... (%2%)"_q.arg(stage).arg(percent));
				});
		});

		Ui::AddSkip(s4, 4);
		const auto importFullBtn = s4->add(
			object_ptr<Ui::RoundButton>(
				s4,
				rpl::single(u"📥 Zaxira nusxadan tiklash"_q),
				st::defaultBoxButton),
			st::boxRowPadding);
		importFullBtn->addClickHandler([=] {
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

			const auto role = modeBox.buttonRole(modeBox.clickedButton());
			if (role != QMessageBox::AcceptRole
					&& role != QMessageBox::DestructiveRole) {
				return;
			}
			const bool fullReplace = (role == QMessageBox::DestructiveRole);

			const auto reply = QMessageBox::warning(
				dialogParent,
				u"Zaxiradan tiklash"_q,
				fullReplace
					? u"DIQQAT! Joriy arxivdagi BARCHA ma'lumotlar\n(o'chirilgan/tahrirlangan xabarlar, media) O'CHIRILADI\nva tanlangan zaxira bilan to'liq almashtiriladi.\n\nBu amalni ortga qaytarib bo'lmaydi. Davom etasizmi?"_q
					: u"Tanlangan zaxiradagi ma'lumotlar JORIY arxivga QO'SHILADI\n(birlashtiriladi) — hozirgi qurilmadagi o'chirilgan/tahrirlangan\nxabarlar va media saqlanib qoladi, o'chirilmaydi.\n\nDavom etasizmi?"_q,
				QMessageBox::Yes | QMessageBox::Cancel,
				QMessageBox::Cancel);
			if (reply != QMessageBox::Yes) return;

			importFullBtn->setDisabled(true);
			Ui::Toast::Show(u"Zaxiradan tiklanmoqda, biroz kuting..."_q);

			const auto weak = base::make_weak(content);
			CustomDB::ImportFullBackupAsync(source, fullReplace, [=](bool ok) {
				if (!weak) return;
				importFullBtn->setDisabled(false);
				if (ok) {
					refreshStats();
					if (onArchiveChanged) onArchiveChanged();
					const auto s = CustomDB::GetArchiveStats();
					Ui::Toast::Show(
						(fullReplace
							? u"To'liq almashtirish muvaffaqiyatli! "_q
							: u"Tiklash muvaffaqiyatli! "_q)
						+ u"%1 o'chirilgan, %2 tahrirlangan. Dastur 3 soniya ichida qayta yuklanadi..."_q
							.arg(s.deletedCount).arg(s.editedCount));
					QTimer::singleShot(3000, [] { Core::Restart(); });
				} else {
					Ui::Toast::Show(u"Tiklash amalga oshmadi. Fayl/papkani tekshiring."_q);
				}
			});
		});

		Ui::AddSkip(s4, 4);
		const auto importMediaArchiveBtn = s4->add(
			object_ptr<Ui::RoundButton>(
				s4,
				rpl::single(u"🎞 Media arxivini qo'shish (CustomModMedia_*.zip)"_q),
				st::defaultBoxButton),
			st::boxRowPadding);
		importMediaArchiveBtn->addClickHandler([=] {
			const auto path = QFileDialog::getOpenFileName(
				dialogParent,
				u"Media arxivini tanlang (.zip)"_q,
				QDir::homePath(),
				u"Media arxivi (CustomModMedia_*.zip);;Barcha fayllar (*)"_q);
			if (path.isEmpty()) return;

			importMediaArchiveBtn->setDisabled(true);
			Ui::Toast::Show(u"Media arxivi ochilmoqda, biroz kuting..."_q);

			const auto weak = base::make_weak(content);
			CustomDB::ImportMediaArchiveAsync(path, [=](bool ok) {
				if (!weak) return;
				importMediaArchiveBtn->setDisabled(false);
				Ui::Toast::Show(ok
					? u"Media arxivi qo'shildi ✓"_q
					: u"Media arxivini qo'shib bo'lmadi."_q);
			});
		});
	}

	// 5-bo'lim: Arxiv boshqaruvi (Standart yopiq)
	const auto s5 = AddCollapsibleSection(content, u"📊 Arxiv boshqaruvi"_q, false);
	s5->add(
		object_ptr<Ui::FlatLabel>(
			s5,
			rpl::single(u"Arxiv statistikasi va holatini yangilash."_q),
			st::customModHintLabel),
		st::boxRowPadding);
	s5->add(
		object_ptr<Ui::RoundButton>(
			s5,
			rpl::single(u"🔄 Statistikani yangilash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		refreshStats();
		Ui::Toast::Show(u"Statistika yangilandi ✓"_q);
	});

	// 6-bo'lim: Vaqtinchalik media papkasi (Standart yopiq)
	const auto s6 = AddCollapsibleSection(content, u"💣 Vaqtinchalik media papkasi"_q, false);
	s6->add(
		object_ptr<Ui::RoundButton>(
			s6,
			rpl::single(u"💣 Vaqtinchalik media papkasini ochish"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto bombDir = CustomSettings::ArchiveRoot() + u"/bombmedia/"_q;
		QDir().mkpath(bombDir);
		QDesktopServices::openUrl(QUrl::fromLocalFile(bombDir));
	});

	Ui::AddSkip(content, st::settingsThumbSkip);
}
