#include "custom_mod_window.h"

#include "base/weak_ptr.h"
#include "core/application.h" // Core::Restart() (Import dan keyin)
#include "core/version.h"     // AppVersionStr / AppAlphaVersion
#include "custom_branding.h"
#include "custom_db.h"
#include "custom_activity_history_box.h"
#include "custom_media_quota.h"
#include "custom_settings.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "main/main_session.h"
#include "ui/userpic_view.h"
#include "ui/painter.h"
#include "ui/toast/toast.h"
#include "custom_upstream_checker.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "ui/boxes/confirm_box.h" // media tuzatish tasdiqlash oynasi
#include "ui/layers/layer_manager.h"
#include "window/window_controller.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/themes/window_theme.h"
#include "window/window_peer_menu.h"
#include "inline_bots/bot_attach_web_view.h" // InlineBots::PeerType(s)
#include "window/window_session_controller.h"
#include "styles/style_basic.h"
#include "styles/style_custom_mod.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <algorithm>
#include <cmath> // std::lround (kvota GB -> MB)
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtCore/QOperatingSystemVersion>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {

// Avatar fallback palitra (peer cache da bo'lmagan holatlar uchun).
constexpr int kAvatarColorsCount = 7;
inline QColor AvatarFallbackColor(int idx) {
	static const QColor kColors[kAvatarColorsCount] = {
		QColor(0xE5, 0x39, 0x35), QColor(0x1E, 0x88, 0xE5),
		QColor(0x43, 0xA0, 0x47), QColor(0xFB, 0x8C, 0x00),
		QColor(0x8E, 0x24, 0xAA), QColor(0x00, 0x89, 0x7B),
		QColor(0xD8, 0x1B, 0x60),
	};
	return kColors[((idx % kAvatarColorsCount) + kAvatarColorsCount)
		% kAvatarColorsCount];
}

// Avatar chizish: agar peer cache da bor bo'lsa — haqiqiy userpic (rasm yoki
// Telegram default), aks holda fallback (harf + random rang).
void PaintPeerAvatar(
		QPainter &p,
		const QRect &rect,
		const QString &peerId,
		const QString &name,
		Main::Session *session,
		Ui::PeerUserpicView &view) {
	bool ok = false;
	const auto rawId = peerId.toLongLong(&ok);
	if (ok && session) {
		const auto peer = session->data().peerLoaded(PeerId(uint64(rawId)));
		if (peer) {
			peer->paintUserpic(p, view, rect.x(), rect.y(), rect.width());
			return;
		}
	}
	// Fallback: harf + rang.
	p.setRenderHint(QPainter::Antialiasing);
	const auto idx = ok ? int(std::abs(rawId % kAvatarColorsCount)) : 0;
	p.setBrush(AvatarFallbackColor(idx));
	p.setPen(Qt::NoPen);
	p.drawEllipse(rect.adjusted(1, 1, -1, -1));
	p.setPen(Qt::white);
	QFont f;
	f.setPixelSize(rect.width() * 15 / 38);
	f.setBold(true);
	p.setFont(f);
	const auto letter = name.isEmpty() ? u"?"_q : name.left(1).toUpper();
	p.drawText(rect, letter, QTextOption(Qt::AlignCenter));
}

// Peer tanlash oynasi — CustomMod ro'yxatlari (White/Black List, Per-Chat,
// Activity Include/Exclude) uchun.
//
// typesRestriction ATAYLAB berilmoqda va uni olib tashlash mumkin emas.
//
// Usiz PrepareChooseRecipientBox filtr sifatida nullptr uzatadi
// (window_peer_menu.cpp: `typesRestriction ? ... : Fn<...>()`), va shunda
// ChooseRecipientBoxController::createRow o'zining FORWARD uchun
// mo'ljallangan standart qoidasiga tushadi:
//
//     peer->isBroadcast() && !Data::CanSendAnything(peer)  → o'tkazib yuboriladi
//
// Ya'ni post qila olmaydigan kanallar ro'yxatga umuman tushmaydi. Oddiy
// obunachi bo'lgan kanallar (odatda shaxsiy kanallar) aynan shu holatda —
// natijada ular tanlash oynasida ko'rinmaydi. Ochiq kanallar esa global
// username qidiruvi orqali baribir topilgani uchun "faqat ochiq kanallar
// chiqyapti" degan taassurot paydo bo'ladi.
//
// Bizga yozish huquqi umuman ahamiyatsiz: biz peer'ni ro'yxatga
// qo'shyapmiz, unga xabar yubormayapmiz. Barcha turlarni berish esa
// tur bo'yicha filtr yaratadi va yuborish huquqi tekshiruvini butunlay
// almashtiradi.
[[nodiscard]] object_ptr<Ui::BoxContent> ChoosePeerBox(
		not_null<Main::Session*> session,
		FnMut<bool(not_null<Data::Thread*>)> &&chosen,
		rpl::producer<QString> title) {
	auto types = InlineBots::PeerTypes();
	types |= InlineBots::PeerType::Bot;
	types |= InlineBots::PeerType::User;
	types |= InlineBots::PeerType::Group;
	types |= InlineBots::PeerType::Broadcast;

	return Window::PrepareChooseRecipientBox(
		session,
		std::move(chosen),
		std::move(title),
		nullptr,
		types);
}

[[nodiscard]] QString MakeWordDiff(
		const QString &before,
		const QString &after) {
	if (before == after) return before;
	const auto bWords = before.split(u' ', Qt::SkipEmptyParts);
	const auto aWords = after.split(u' ', Qt::SkipEmptyParts);
	auto start = 0;
	while (start < bWords.size() && start < aWords.size()
	       && bWords[start] == aWords[start]) {
		++start;
	}
	auto bEnd = int(bWords.size());
	auto aEnd = int(aWords.size());
	while (bEnd > start && aEnd > start
	       && bWords[bEnd - 1] == aWords[aEnd - 1]) {
		--bEnd; --aEnd;
	}
	auto parts = QStringList();
	for (auto i = 0; i < start; ++i) parts.append(bWords[i]);
	for (auto i = start; i < bEnd; ++i)
		parts.append(u"[-"_q + bWords[i] + u"]"_q);
	for (auto i = start; i < aEnd; ++i)
		parts.append(u"[+"_q + aWords[i] + u"]"_q);
	for (auto i = bEnd; i < bWords.size(); ++i) parts.append(bWords[i]);
	return parts.join(u' ');
}

#ifdef Q_OS_WIN
void ApplyTitleBar(QWidget *w) {
	const auto dark = Window::Theme::IsNightMode();
	const HWND hwnd = reinterpret_cast<HWND>(w->winId());
	if (!hwnd) return;

	static const auto kBuild = QOperatingSystemVersion::current().microVersion();

	// Dark/light mode toggle (Win10 build 17763+)
	if (kBuild >= 17763) {
		const BOOL value = dark ? TRUE : FALSE;
		static const auto kDarkAttr = (kBuild >= 18985) ? DWORD(20) : DWORD(19);
		DwmSetWindowAttribute(hwnd, kDarkAttr, &value, sizeof(value));
	}

	// Custom caption color (Win11 build 22000+)
	// DWMWA_CAPTION_COLOR = 35
	// Dark: #1F2936  →  COLORREF(R=0x1F, G=0x29, B=0x36)
	// Light: 0xFFFFFFFE = DWMWA_COLOR_DEFAULT (system resets to its default)
	if (kBuild >= 22000) {
		const COLORREF color = dark ? RGB(0x1F, 0x29, 0x36) : COLORREF(0xFFFFFFFE);
		DwmSetWindowAttribute(hwnd, DWORD(35), &color, sizeof(color));
	}
}
#endif

class CustomTabBar final : public Ui::RpWidget {
public:
	CustomTabBar(QWidget *parent, std::initializer_list<QString> names)
	: Ui::RpWidget(parent)
	, _names(names) {
		setFixedHeight(st::defaultBoxButton.height + st::customModTabBarVSkip);
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_OpaquePaintEvent);
		style::PaletteChanged() | rpl::on_next([=] {
			update();
		}, lifetime());
	}

	[[nodiscard]] rpl::producer<int> tabSelected() const {
		return _tabSelected.events();
	}

	void setActiveTab(int index) {
		_active = index;
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override {
		Painter p(this);
		const auto h = height();
		p.fillRect(rect(), st::windowBg);
		p.fillRect(0, h - 1, width(), 1, st::shadowFg->c);
		if (_names.empty()) return;
		const auto tabW = width() / int(_names.size());
		for (auto i = 0; i < int(_names.size()); ++i) {
			const auto active = (i == _active);
			p.setPen(active
				? st::windowActiveTextFg->c
				: st::windowSubTextFg->c);
			p.setFont(active ? st::semiboldFont : st::normalFont);
			p.drawText(
				QRect(i * tabW, 0, tabW, h - 3),
				_names[i],
				QTextOption(Qt::AlignCenter));
			if (active) {
				p.fillRect(
					i * tabW + 4, h - 3,
					tabW - 8, 2,
					st::windowActiveTextFg->c);
			}
		}
	}

	void mousePressEvent(QMouseEvent *e) override {
		if (_names.empty()) return;
		const auto tabW = width() / int(_names.size());
		const auto idx = e->pos().x() / tabW;
		if (idx >= 0 && idx < int(_names.size())) {
			_active = idx;
			_tabSelected.fire_copy(idx);
			update();
		}
	}

private:
	std::vector<QString> _names;
	int _active = 0;
	rpl::event_stream<int> _tabSelected;
};

void fillGeneralTab(not_null<Ui::VerticalLayout*> content);
void fillPeersTab(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	Fn<void()> onRebuild);
void fillPerChatSection(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller);
// Avatar + ism + ID + o'chirish tugmasi bilan bitta peer qatorini quradi
// va uni |content| ga qo'shadi. White/Black List'dagi fillPeerSection's
// state->addEntry bilan bir xil vizual pattern, lekin SlideWrap/entryWraps
// holat boshqaruvisiz — chaqiruvchi butun bo'limni onRebuild() orqali
// to'liq qayta quradi (Include/Exclude List shu tarzda ishlaydi).
void AddAvatarPeerRow(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	const QString &peerId,
	const QString &name,
	Fn<void()> onDelete);
void fillActivityHistorySection(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	Fn<void()> onRebuild);
void fillUpstreamCheckSection(not_null<Ui::VerticalLayout*> content);
void fillArchiveTab(
	not_null<Ui::VerticalLayout*> content,
	Fn<void()> onRefresh);
void fillAboutTab(
	not_null<Ui::VerticalLayout*> content,
	QWidget *dialogParent,
	Fn<void()> onArchiveChanged);

class CustomModWindow final : public Ui::RpWidget {
public:
	explicit CustomModWindow(
		not_null<Window::SessionController*> controller);

	// Chat tanlash box ni custom window ichida ochish.
	void showBox(object_ptr<Ui::BoxContent> box);

protected:
	void resizeEvent(QResizeEvent *e) override;
	void closeEvent(QCloseEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void showEvent(QShowEvent *e) override;

private:
	void switchTab(int index);
	void setupContent(not_null<Window::SessionController*> controller);

	CustomTabBar *_tabBar = nullptr;
	std::array<Ui::ScrollArea*, 4> _panels = {};
	std::array<QPointer<Ui::VerticalLayout>, 4> _inners = {};
	std::unique_ptr<Ui::LayerManager> _layerManager;
};

CustomModWindow::CustomModWindow(
	not_null<Window::SessionController*> controller)
: Ui::RpWidget(nullptr) {
	setWindowFlags(Qt::Window);
	// CustomMod: title branding.json dan
	setWindowTitle(CustomBranding::Get().customModTitle);
	setMinimumSize(480, 400);
	setAttribute(Qt::WA_OpaquePaintEvent);
	style::PaletteChanged() | rpl::on_next([=] {
		update();
#ifdef Q_OS_WIN
		ApplyTitleBar(this);
#endif
	}, lifetime());

	auto s = QSettings(u"CustomMod"_q, u"TelegramDesktop"_q);
	const auto geom = s.value(u"WindowGeometry"_q).toByteArray();
	if (geom.isEmpty()) {
		resize(560, 700);
		const auto screen = QGuiApplication::primaryScreen()
			->availableGeometry();
		move(screen.center() - QPoint(280, 350));
	} else {
		restoreGeometry(geom);
	}

	_tabBar = new CustomTabBar(
		this,
		{ u"General"_q, u"Peers"_q, u"Archive"_q, u"About"_q });

	for (auto i = 0; i < 4; ++i) {
		_panels[i] = new Ui::ScrollArea(this);
	}

	setupContent(controller);
	switchTab(0);

	// LayerManager: "Chat tanlash" box shu window ichida ochiladi.
	_layerManager = std::make_unique<Ui::LayerManager>(this);

	std::move(_tabBar->tabSelected()) | rpl::on_next([=](int idx) {
		switchTab(idx);
	}, lifetime());
}

void CustomModWindow::setupContent(
		not_null<Window::SessionController*> controller) {
	const auto makeInner = [&](int idx) -> not_null<Ui::VerticalLayout*> {
		const auto inner = _panels[idx]->setOwnedWidget(
			object_ptr<Ui::VerticalLayout>(_panels[idx]));
		_inners[idx] = inner;
		_panels[idx]->widthValue() | rpl::on_next([=](int w) {
			inner->resizeToWidth(w);
		}, inner->lifetime());
		return inner;
	};

	fillGeneralTab(makeInner(0));

	// Peers tab — White/Black List konflikti hal qilinganda to'liq qayta quriladi.
	const auto panel1 = _panels[1];
	const auto rebuildPeers = std::make_shared<Fn<void()>>();
	*rebuildPeers = [=]() {
		const auto inner = panel1->setOwnedWidget(
			object_ptr<Ui::VerticalLayout>(panel1));
		_inners[1] = inner;
		panel1->widthValue() | rpl::on_next([=](int w) {
			inner->resizeToWidth(w);
		}, inner->lifetime());
		fillPeersTab(inner, controller, *rebuildPeers);
	};
	(*rebuildPeers)();

	// Archive tab — yangilash tugmasi bosilganda to'liq qayta quriladi.
	const auto panel2 = _panels[2];
	const auto rebuildArchive = std::make_shared<Fn<void()>>();
	*rebuildArchive = [=]() {
		const auto inner = panel2->setOwnedWidget(
			object_ptr<Ui::VerticalLayout>(panel2));
		_inners[2] = inner;
		panel2->widthValue() | rpl::on_next([=](int w) {
			inner->resizeToWidth(w);
		}, inner->lifetime());
		fillArchiveTab(inner, *rebuildArchive);
	};
	(*rebuildArchive)();

	fillAboutTab(makeInner(3), this, *rebuildArchive);
}

void CustomModWindow::resizeEvent(QResizeEvent *) {
	const auto tabH = _tabBar->height();
	_tabBar->setGeometry(0, 0, width(), tabH);
	const auto panelW = width();
	const auto panelH = height() - tabH;
	for (auto i = 0; i < 4; ++i) {
		_panels[i]->setGeometry(0, tabH, panelW, panelH);
		if (const auto inner = _inners[i].data(); inner && panelW > 0) {
			inner->resizeToWidth(panelW);
			// Qt faqat yangi exposed area ni qayta chizadi — window toraytirilganda
			// FlatLabel lar ko'rinishini yangilamaydi. update() buni majburlaydi.
			if (_panels[i]->isVisible()) inner->update();
		}
	}
	// LayerManager (LayerStackWidget) panellar ustida turishi kerak.
	if (_layerManager) _layerManager->raise();
}

void CustomModWindow::closeEvent(QCloseEvent *e) {
	auto s = QSettings(u"CustomMod"_q, u"TelegramDesktop"_q);
	s.setValue(u"WindowGeometry"_q, saveGeometry());
	Ui::RpWidget::closeEvent(e);
}

void CustomModWindow::paintEvent(QPaintEvent *) {
	Painter p(this);
	p.fillRect(rect(), st::windowBg);
}

void CustomModWindow::showEvent(QShowEvent *e) {
	Ui::RpWidget::showEvent(e);
	// Oyna birinchi marta ko'rsatilganda barcha inner layoutlarni majburan
	// resize qilamiz. Yashirin panellar startup vaqtida QResizeEvent olmaydi —
	// shu sababli entries (whitelist, blacklist) noto'g'ri kenglikda qolishi mumkin.
	if (_tabBar) {
		const auto panelW = width();
		const auto panelH = height() - _tabBar->height();
		if (panelW > 0 && panelH > 0) {
			for (auto i = 0; i < 4; ++i) {
				if (const auto inner = _inners[i].data()) {
					inner->resizeToWidth(panelW);
					inner->update();
				}
			}
		}
	}
#ifdef Q_OS_WIN
	ApplyTitleBar(this);
#endif
}

void CustomModWindow::switchTab(int index) {
	_tabBar->setActiveTab(index);
	for (auto i = 0; i < 4; ++i) {
		_panels[i]->setVisible(i == index);
	}
	// Ko'rinadigan panel o'zgarganda inner layoutni majburan resize qilamiz.
	// Qt hidden widget larga QResizeEvent yubormaydi — shu sababli widthValue()
	// reactive chain ishlamagan bo'lishi mumkin. To'g'ridan-to'g'ri chaqiramiz.
	if (const auto inner = _inners[index].data()) {
		const auto panelW = width();
		if (panelW > 0) {
			inner->resizeToWidth(panelW);
			inner->update(); // Paint event ni majburlash
		}
	}
}

void CustomModWindow::showBox(object_ptr<Ui::BoxContent> box) {
	_layerManager->showBox(std::move(box));
}

// Singleton instance — fill* funksiyalar tomonidan ham ishlatiladi.
QPointer<CustomModWindow> gInstance;

void fillGeneralTab(not_null<Ui::VerticalLayout*> content) {
	const auto addSection = [&](const QString &title) {
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(title),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);
	};

	const auto addToggle = [&](
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
		else if (id == u"spoofMobile"_q) current = val.spoofMobile;
		else if (id == u"storyAnonymousView"_q) current = val.storyAnonymousView;
		else if (id == u"storyMediaBackupEnabled"_q) current = val.storyMediaBackupEnabled;
		else if (id == u"mutualContactShowInChatList"_q) current = val.mutualContactShowInChatList;
		else if (id == u"mutualContactShowInContactsList"_q) current = val.mutualContactShowInContactsList;
		else if (id == u"mutualContactShowInProfile"_q) current = val.mutualContactShowInProfile;
		else if (id == u"mutualContactShowInMembersList"_q) current = val.mutualContactShowInMembersList;

		const auto btn = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(text),
				st::settingsButtonNoIcon));
		btn->toggleOn(rpl::single(current));

		// rpl::skip(1) — startup da dastlabki emit o'tkazib yuboriladi.
		// Faqat foydalanuvchi toggle bosganida Set() va toast ishlaydi.
		btn->toggledValue()
			| rpl::skip(1)
			| rpl::on_next([=](bool on) {
			CustomSettings::Set(id, on);
			Ui::Toast::Show(
				on ? (text + u" yoqildi ✓"_q)
				   : (text + u" oʻchirildi"_q));
		}, btn->lifetime());

		if (!description.isEmpty()) {
			const auto descLabel = content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(description),
					st::customModHintLabel),
				st::boxRowPadding,
				style::al_justify);
			// Belt-and-suspenders: al_justify + manual resize → har qanday
			// oyna kengligi o'zgarganda text wrap to'g'ri ishlaydi.
			content->widthValue() | rpl::on_next([=](int w) {
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

	Ui::AddSkip(content, st::settingsThumbSkip);
	addSection(u"📱 Qurilma ko'rinishini almashtirish"_q);
	const auto spoofToggleBtn = addToggle(
		u"spoofMobile"_q,
		u"Mobil qurilma koʻrinishi"_q,
		u"Telegram mobil ilovadan ishlatilayotgandek koʻrinadi."_q);

	Ui::AddSkip(content, 8);

	// C22: Dynamic device spoof — spoofMobile OFF bo'lganda forma yashirinadi.
	const auto spoofFormWrap = content->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			content,
			object_ptr<Ui::VerticalLayout>(content)),
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

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	addSection(u"🛡️ Privacy & Custom Mods"_q);
	addToggle(
		u"ghostMode"_q,
		u"Ghost Mode"_q,
		u"Online holatini, yozmoqda belgisini va xabar oʻqildi bildirishnomasini yashiradi.\n\nYoqilgach, to'liq kuchga kirishi uchun 1-2 daqiqa ketishi mumkin."_q);
	addToggle(
		u"storyAnonymousView"_q,
		u"Hikoyalarni anonim koʻrish"_q,
		u"Hikoyani koʻrganingiz haqida egasiga bildirish yuborilmaydi."_q);
	addToggle(
		u"storyMediaBackupEnabled"_q,
		u"Hikoya media'sini saqlash"_q,
		u"Kuzatilayotgan userlarning hikoya (story) rasmi/videosi avtomatik, ko'rmasdan lokal saqlanadi. Disk joyini sarflaydi, standart holatda o'chirilgan."_q);
	addToggle(
		u"bypassRestrictions"_q,
		u"Cheklangan chatda nusxalash va yuborish"_q,
		u"Cheklov qoʻyilgan chatlardagi xabarlarni boshqaga yuborish yoki nusxa olishga ruxsat beradi."_q);
	addToggle(
		u"antiDelete"_q,
		u"Anti-Delete"_q,
		u"Oʻchirilgan xabarlarni koʻrinishda qoldiradi."_q);
	addToggle(
		u"antiEdit"_q,
		u"Anti-Edit"_q,
		u"Tahrirdan oldingi matnni koʻrsatadi."_q);
	addToggle(
		u"offlineDb"_q,
		u"Offline xabar bazasi"_q,
		u"Xabarlar va medialarni internet boʻlmaganda ham koʻrish uchun qurilmada saqlaydi."_q);

	// A13/K6.3: arxiv statistikasi — foydalanuvchi arxiv haqiqatan
	// to'layotganini va qancha joy egallayotganini ko'rib tursin.
	{
		const auto archived = CustomDB::ArchivedMessageCount();
		const auto sizeMb = CustomDB::DatabaseSizeBytes() / (1024 * 1024);
		const auto stats = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Arxivda "_q
					+ QString::number(archived)
					+ u" ta xabar saqlangan. Baza hajmi: "_q
					+ QString::number(sizeMb)
					+ u" MB."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
		content->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				stats->resizeToWidth(lw);
				stats->update();
			}
		}, stats->lifetime());
	}

	// ── Mutual-Contact Indikatori ─────────────────────────────────────
	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	addSection(u"🤝 Mutual-Contact Indikatori"_q);
	{
		const auto desc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Sizni ham qaytarib contact'ga qoʻshgan odamlar ismi "
					"yoniga belgi qoʻyadi. Har bir joy uchun mustaqil yoqish va "
					"mustaqil belgi (emoji) tanlash mumkin."_q),
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
	addToggle(
		u"mutualContactShowInChatList"_q,
		u"Chat roʻyxatida koʻrsatish"_q,
		QString());
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Belgi (chat roʻyxati uchun):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto chatListEmojiInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Emoji"_q),
			CustomSettings::MutualContactChatListEmoji()),
		st::boxRowPadding);

	Ui::AddSkip(content, 8);
	addToggle(
		u"mutualContactShowInContactsList"_q,
		u"Contacts roʻyxatida koʻrsatish"_q,
		QString());
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Belgi (Contacts roʻyxati uchun):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto contactsListEmojiInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Emoji"_q),
			CustomSettings::MutualContactContactsListEmoji()),
		st::boxRowPadding);

	Ui::AddSkip(content, 8);
	addToggle(
		u"mutualContactShowInProfile"_q,
		u"Profil sarlavhasida koʻrsatish"_q,
		QString());
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Belgi (Profil uchun):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto profileEmojiInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Emoji"_q),
			CustomSettings::MutualContactProfileEmoji()),
		st::boxRowPadding);

	Ui::AddSkip(content, 8);
	addToggle(
		u"mutualContactShowInMembersList"_q,
		u"Guruh a'zolari roʻyxatida koʻrsatish"_q,
		QString());
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Belgi (Guruh a'zolari roʻyxati uchun):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto membersListEmojiInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Emoji"_q),
			CustomSettings::MutualContactMembersListEmoji()),
		st::boxRowPadding);

	Ui::AddSkip(content, 8);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
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

	// ── Branding sektsiyasi ──────────────────────────────────────────
	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	addSection(u"🎨 Branding"_q);

	{
		const auto desc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Oyna nomi va icon ni o'zgartirish. "
					"Barcha o'zgartirishlar uchun dasturni qayta yoqing."_q),
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

	// 1) Asosiy oyna nomi
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Asosiy oyna nomi:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto titleInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Masalan: MyChat"_q),
			CustomBranding::Get().windowTitle),
		st::boxRowPadding);

	// 2) Custom oyna nomi
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Sozlamalar oyna nomi:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto modInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Masalan: Mening Sozlamalarim"_q),
			CustomBranding::Get().customModTitle),
		st::boxRowPadding);

	// 3) Icon path
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Icon fayl yo'li (PNG yoki ICO):"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto iconInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Bo'sh = standart icon"_q),
			CustomBranding::Get().iconPath),
		st::boxRowPadding);

	// Icon tanlash + tozalash tugmalari
	Ui::AddSkip(content, 4);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
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

	Ui::AddSkip(content, 4);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"🗑️ Iconni tozalash"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		iconInput->setText(QString());
	});

	// Saqlash tugmasi
	Ui::AddSkip(content, 12);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
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
	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	fillUpstreamCheckSection(content);

	Ui::AddSkip(content, st::settingsThumbSkip);
}

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

void fillPeerSection(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		bool isWhitelist,
		Fn<void()> onRebuild) {
	struct State {
		int visibleCount = 0;
		Ui::VerticalLayout *entriesLayout = nullptr;
		Ui::SlideWrap<Ui::FlatLabel> *emptyWrap = nullptr;
		Ui::FlatLabel *countLabel = nullptr;
		QVector<Ui::SlideWrap<Ui::RpWidget>*> entryWraps;
		Fn<void(const QString &, const QString &)> addEntry;
	};
	const auto state = content->lifetime().make_state<State>();

	// ── Section header ───────────────────────────────────────────
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(isWhitelist ? u"White List"_q : u"Black List"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	{
		const auto sectionDesc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(isWhitelist
					? u"Bu roʻyxatdagi chatlar uchun barcha funksiyalar doim yoqiq boʻladi."_q
					: u"Bu roʻyxatdagi chatlar uchun barcha funksiyalar doim oʻchiq boʻladi."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
		content->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				sectionDesc->resizeToWidth(lw);
				sectionDesc->update();
			}
		}, sectionDesc->lifetime());
	}

	// ── T42: Kategoriya bo'yicha tanlash ────────────────────────────
	Ui::AddSkip(content, 8);
	{
		const auto addCategoryToggle = [&](
				const QString &label,
				CustomSettings::PeerType type) {
			const auto btn = content->add(
				object_ptr<Ui::SettingsButton>(
					content,
					rpl::single(label),
					st::settingsButtonNoIcon),
				style::margins(0, 0, 0, 0));
			const bool initial = isWhitelist
				? CustomSettings::IsWhitelistCategoryEnabled(type)
				: CustomSettings::IsBlocklistCategoryEnabled(type);
			btn->toggleOn(rpl::single(initial));
			btn->toggledValue() | rpl::skip(1) | rpl::on_next([=](bool on) {
				const bool hadConflict = on && (isWhitelist
					? CustomSettings::IsBlocklistCategoryEnabled(type)
					: CustomSettings::IsWhitelistCategoryEnabled(type));
				if (isWhitelist) {
					CustomSettings::SetWhitelistCategory(type, on);
				} else {
					CustomSettings::SetBlocklistCategory(type, on);
				}
				if (hadConflict) {
					Ui::Toast::Show(
						u"Qarama-qarshi roʻyxatdagi mos kategoriya "
						"avtomatik oʻchirildi."_q);
					if (onRebuild) onRebuild();
				}
			}, btn->lifetime());
		};
		addCategoryToggle(
			u"Shaxsiy chatlar"_q,
			CustomSettings::PeerType::User);
		addCategoryToggle(
			u"Guruhlar"_q,
			CustomSettings::PeerType::Group);
		addCategoryToggle(
			u"Kanallar / superguruhlar"_q,
			CustomSettings::PeerType::Channel);
	}

	// ── Chat tanlash (primary) ───────────────────────────────────
	Ui::AddSkip(content, 12);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(isWhitelist
				? u"Chat tanlash — White List"_q
				: u"Chat tanlash — Black List"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (!gInstance) return;
		// Box custom window ichida ochiladi (LayerManager orqali).
		gInstance->showBox(ChoosePeerBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				const auto peerId = QString::number(peer->id.value);
				const auto name = peer->name();
				const auto already = isWhitelist
					? CustomSettings::IsInWhitelist(peerId)
					: CustomSettings::IsInBlocklist(peerId);
				if (already) {
					Ui::Toast::Show(u"Bu chat allaqachon roʻyxatda."_q);
					return true;
				}
				// wasInOpposite AddToWhitelist/AddToBlocklist dan OLDIN hisoblanishi shart —
				// bu funksiyalar chaqirilgach, peer avtomatik ravishda qarama-qarshi
				// roʻyxatdan olib tashlanadi (mutual exclusion), shuning uchun keyin
				// tekshirsak har doim false qaytadi.
				const auto wasInOpposite = isWhitelist
					? CustomSettings::IsInBlocklist(peerId)
					: CustomSettings::IsInWhitelist(peerId);
				if (isWhitelist) {
					CustomSettings::AddToWhitelist(peerId, name);
				} else {
					CustomSettings::AddToBlocklist(peerId, name);
				}
				if (wasInOpposite) {
					Ui::Toast::Show(name + (isWhitelist
						? u" Black List'dan olib tashlandi va White List'ga qoʻshildi."_q
						: u" White List'dan olib tashlandi va Black List'ga qoʻshildi."_q));
					if (onRebuild) onRebuild();
				} else {
					state->addEntry(peerId, name);
					Ui::Toast::Show(name + u" qoʻshildi."_q);
				}
				// raise()/activateWindow() shart emas — dialog window ichida ochiladi.
				return true;
			},
			rpl::single(isWhitelist
				? u"White List ga qoʻshish"_q
				: u"Black List ga qoʻshish"_q)));
	});

	// ── ID orqali qo'shish (secondary) ──────────────────────────
	Ui::AddSkip(content, 8);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Yoki ID orqali qoʻshish:"_q),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	const auto peerIdInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Chat ID (masalan: 123456789)"_q),
			QString()),
		st::boxRowPadding);

	const auto nameInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Nom (ixtiyoriy)"_q),
			QString()),
		st::boxRowPadding);

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(isWhitelist
				? u"ID boʻyicha White List ga qoʻshish"_q
				: u"ID boʻyicha Black List ga qoʻshish"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto peerId = peerIdInput->getLastText().trimmed();
		if (peerId.isEmpty()) {
			Ui::Toast::Show(u"Chat ID boʻsh. Iltimos kiriting."_q);
			return;
		}
		const auto already = isWhitelist
			? CustomSettings::IsInWhitelist(peerId)
			: CustomSettings::IsInBlocklist(peerId);
		if (already) {
			Ui::Toast::Show(u"Bu chat allaqachon roʻyxatda."_q);
			return;
		}
		// wasInOpposite AddToWhitelist/AddToBlocklist dan OLDIN hisoblanishi shart —
		// bu funksiyalar chaqirilgach, peer avtomatik ravishda qarama-qarshi
		// roʻyxatdan olib tashlanadi (mutual exclusion), shuning uchun keyin
		// tekshirsak har doim false qaytadi.
		const auto wasInOpposite = isWhitelist
			? CustomSettings::IsInBlocklist(peerId)
			: CustomSettings::IsInWhitelist(peerId);
		const auto name = nameInput->getLastText().trimmed();
		if (isWhitelist) {
			CustomSettings::AddToWhitelist(peerId, name);
		} else {
			CustomSettings::AddToBlocklist(peerId, name);
		}
		peerIdInput->setText(QString());
		nameInput->setText(QString());
		if (wasInOpposite) {
			Ui::Toast::Show(u"Qarama-qarshi roʻyxatdan olib tashlandi va "_q
				+ (isWhitelist ? u"White List"_q : u"Black List"_q)
				+ u"'ga qoʻshildi: "_q + peerId);
			if (onRebuild) onRebuild();
		} else {
			state->addEntry(peerId, name);
			Ui::Toast::Show(u"Qoʻshildi: "_q + peerId);
		}
	});

	// ── Ro'yxat: header + bo'sh holat + entries ─────────────────
	Ui::AddSkip(content, 16);

	state->countLabel = content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString()),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	// updateHeader state->emptyWrap ni runtime da o'qiydi — shu sababli
	// emptyWrap dan oldin aniqlanishi mumkin.
	const auto updateHeader = [=](bool animated) {
		const auto n = state->visibleCount;
		state->countLabel->setText(isWhitelist
			? (n == 0
				? u"White List — boʻsh"_q
				: u"White List — %1 ta chat"_q.arg(n))
			: (n == 0
				? u"Black List — boʻsh"_q
				: u"Black List — %1 ta chat"_q.arg(n)));
		state->emptyWrap->toggle(
			n == 0,
			animated ? anim::type::normal : anim::type::instant);
	};

	// ── Barchasini tozalash tugmasi ──────────────────────────────
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"Barchasini tozalash"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (state->visibleCount == 0) {
			Ui::Toast::Show(u"Roʻyxat allaqachon boʻsh."_q);
			return;
		}
		if (isWhitelist) {
			CustomSettings::ClearWhitelist();
		} else {
			CustomSettings::ClearBlocklist();
		}
		for (auto *wrap : state->entryWraps) {
			wrap->toggle(false, anim::type::normal);
		}
		state->visibleCount = 0;
		updateHeader(true);
		Ui::Toast::Show(isWhitelist
			? u"White List tozalandi."_q
			: u"Black List tozalandi."_q);
	});

	state->entriesLayout = content->add(
		object_ptr<Ui::VerticalLayout>(content),
		{0, 0, 0, 0});

	state->emptyWrap = state->entriesLayout->add(
		object_ptr<Ui::SlideWrap<Ui::FlatLabel>>(
			state->entriesLayout,
			object_ptr<Ui::FlatLabel>(
				state->entriesLayout,
				rpl::single(u"Roʻyxat boʻsh. Yuqoridan chat qoʻshing."_q),
				st::customModHintLabel)),
		st::defaultSubsectionTitlePadding,
		style::al_justify);

	// ── Entry row builder ────────────────────────────────────────
	state->addEntry = [=](const QString &peerId, const QString &name) {
		constexpr int kRowH    = 56;
		constexpr int kAvSize  = 38;
		[[maybe_unused]] constexpr int kPadL = 14;
		[[maybe_unused]] constexpr int kPadR = 12;
		[[maybe_unused]] constexpr int kGap  = 12;
		constexpr int kDelBtnW = 76;

		const auto entryWrap = state->entriesLayout->add(
			object_ptr<Ui::SlideWrap<Ui::RpWidget>>(
				state->entriesLayout,
				object_ptr<Ui::RpWidget>(state->entriesLayout)),
			style::margins(),
			style::al_justify);
		state->entryWraps.append(entryWrap);
		const auto row = entryWrap->entity();
		row->setFixedHeight(kRowH);

		// Avatar circle — real userpic agar peer cache da bo'lsa,
		// aks holda fallback (harf + rang).
		const auto av = Ui::CreateChild<Ui::RpWidget>(row);
		av->setFixedSize(kAvSize, kAvSize);
		const auto userpicView =
			std::make_shared<Ui::PeerUserpicView>();
		const auto session = &controller->session();
		av->paintRequest() | rpl::on_next([=](QRect) {
			Painter p(av);
			PaintPeerAvatar(
				p,
				QRect(0, 0, kAvSize, kAvSize),
				peerId,
				name,
				session,
				*userpicView);
		}, av->lifetime());

		// Name label
		const auto nameLabel = Ui::CreateChild<Ui::FlatLabel>(
			row,
			rpl::single(name.isEmpty() ? peerId : name),
			st::boxLabel);

		// Peer ID label
		const auto idLabel = Ui::CreateChild<Ui::FlatLabel>(
			row,
			rpl::single(u"ID: "_q + peerId),
			st::customModHintLabel);

		// Delete button
		const auto delBtn = Ui::CreateChild<Ui::RoundButton>(
			row,
			rpl::single(u"Oʻchirish"_q),
			st::attentionBoxButton);
		delBtn->setFixedWidth(kDelBtnW);
		// Barcha child widgetlar uchun show() majburiy — Qt yangi child larni
		// parent visible bo'lganda avtomatik ko'rsatmaydi.
		av->show();
		nameLabel->show();
		idLabel->show();
		delBtn->show();

		// Bottom separator
		row->paintRequest() | rpl::on_next([=](QRect) {
			Painter p(row);
			p.fillRect(kPadL + kAvSize + kGap, kRowH - 1,
				row->width() - kPadL - kAvSize - kGap - kPadR, 1,
				st::shadowFg->c);
		}, row->lifetime());

		// Layout children — alohida lambda, darhol ham chaqiriladi.
		const auto layoutRow = [=](int w) {
			const auto avY = (kRowH - kAvSize) / 2;
			av->move(kPadL, avY);
			av->update();
			const auto textX = kPadL + kAvSize + kGap;
			const auto textW = w - textX - kGap - kDelBtnW - kPadR;
			if (textW <= 0) return;
			nameLabel->resizeToWidth(textW);
			nameLabel->move(textX, 10);
			nameLabel->update();
			idLabel->resizeToWidth(textW);
			idLabel->move(textX, 10 + nameLabel->height() + 2);
			idLabel->update();
			const auto btnY = (kRowH - st::defaultBoxButton.height) / 2;
			delBtn->move(w - kPadR - kDelBtnW, btnY);
		};
		row->widthValue() | rpl::on_next(layoutRow, row->lifetime());
		// w=0 bo'lganda subscription ishlamaydi — parent width bilan darhol chaqirish.
		const auto initW = state->entriesLayout->width();
		if (initW > 0) layoutRow(initW);

		delBtn->addClickHandler([=] {
			delBtn->setDisabled(true);
			if (isWhitelist) {
				CustomSettings::RemoveFromWhitelist(peerId);
			} else {
				CustomSettings::RemoveFromBlocklist(peerId);
			}
			entryWrap->toggle(false, anim::type::normal);
			--state->visibleCount;
			updateHeader(true);
			Ui::Toast::Show(u"Roʻyxatdan oʻchirildi."_q);
		});

		// SlideWrap default _toggled=false (yashirin, height=0) — darhol ko'rsatish.
		entryWrap->toggle(true, anim::type::instant);

		++state->visibleCount;
		updateHeader(false);

		// entriesLayout va content ni qayta o'lchamlab, yangi row ko'rinsin.
		// update() — Qt faqat yangi exposed area ni qayta chizadi, shu sababli
		// mavjud widget lar uchun ham update() majburan chaqiriladi.
		if (state->entriesLayout->width() > 0) {
			state->entriesLayout->resizeToWidth(
				state->entriesLayout->width());
			state->entriesLayout->update();
		}
		if (content->width() > 0) {
			content->resizeToWidth(content->width());
			content->update();
		}
	};

	// ── Load saved peers ─────────────────────────────────────────
	updateHeader(false);
	{
		const auto entries = isWhitelist
			? CustomSettings::GetWhitelist()
			: CustomSettings::GetBlocklist();
		for (const auto &e : entries) {
			state->addEntry(e.first, e.second);
		}
	}

	Ui::AddSkip(content, st::settingsThumbSkip);
}

// ── NEXT-6: Per-Chat Settings paneli ─────────────────────────────────────
// Har bir chat uchun Ghost / AntiDelete / AntiEdit ni alohida boshqarish.
// Priority: Blocklist > Whitelist > Per-peer override > Global flag
void fillPerChatSection(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller) {
	struct State {
		Ui::VerticalLayout *entriesLayout = nullptr;
		Ui::SlideWrap<Ui::FlatLabel> *emptyWrap = nullptr;
		Ui::FlatLabel *countLabel = nullptr;
		int visibleCount = 0;
		QVector<Ui::SlideWrap<Ui::VerticalLayout>*> entryWraps;
		Fn<void(const CustomSettings::PerPeerEntry &)> addEntry;
	};
	const auto state = content->lifetime().make_state<State>();

	// ── Header ────────────────────────────────────────────────────
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"⚙️ Individual sozlamalar (istisnolar)"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	{
		const auto desc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Agar biror chat uchun faqat bitta funksiyani (masalan "
					"faqat Ghost Mode) alohida sozlamoqchi bo'lsangiz — shu yerdan "
					"qo'shing. Bu ro'yxat White/Black List'dan KEYIN tekshiriladi "
					"(ular ustunroq)."_q),
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

	// ── Chat tanlash tugmasi ──────────────────────────────────────
	Ui::AddSkip(content, 12);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"Chat tanlash — Per-Chat"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (!gInstance) return;
		gInstance->showBox(ChoosePeerBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				const auto peerId = QString::number(peer->id.value);
				const auto name = peer->name();
				if (CustomSettings::HasPerPeerOverride(peerId)) {
					Ui::Toast::Show(u"Bu chat allaqachon roʻyxatda."_q);
					return true;
				}
				CustomSettings::AddPerPeerOverride(peerId, name);
				const auto entries = CustomSettings::GetPerPeerOverrides();
				for (const auto &e : entries) {
					if (e.peerId == peerId) {
						state->addEntry(e);
						break;
					}
				}
				Ui::Toast::Show(name + u" qoʻshildi."_q);
				return true;
			},
			rpl::single(u"Per-Chat sozlash uchun chat tanlang"_q)));
	});

	// ── Count label + empty wrap ──────────────────────────────────
	Ui::AddSkip(content, 16);
	state->countLabel = content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString()),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	const auto updateHeader = [=](bool animated) {
		const auto n = state->visibleCount;
		state->countLabel->setText(n == 0
			? u"Per-Chat — boʻsh"_q
			: u"Per-Chat — %1 ta chat"_q.arg(n));
		state->emptyWrap->toggle(
			n == 0,
			animated ? anim::type::normal : anim::type::instant);
	};

	// ── Barchasini tozalash (NEXT-9) ──────────────────────────────
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"Barchasini tozalash"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		if (state->visibleCount == 0) {
			Ui::Toast::Show(u"Roʻyxat allaqachon boʻsh."_q);
			return;
		}
		CustomSettings::ClearAllPerPeerOverrides();
		for (auto *wrap : state->entryWraps) {
			wrap->toggle(false, anim::type::normal);
		}
		state->visibleCount = 0;
		updateHeader(true);
		Ui::Toast::Show(u"Per-Chat roʻyxati tozalandi."_q);
	});

	state->entriesLayout = content->add(
		object_ptr<Ui::VerticalLayout>(content),
		{0, 0, 0, 0});

	state->emptyWrap = state->entriesLayout->add(
		object_ptr<Ui::SlideWrap<Ui::FlatLabel>>(
			state->entriesLayout,
			object_ptr<Ui::FlatLabel>(
				state->entriesLayout,
				rpl::single(u"Per-Chat sozlash uchun yuqoridan chat qoʻshing."_q),
				st::customModHintLabel)),
		st::defaultSubsectionTitlePadding,
		style::al_justify);

	// ── Entry row builder ─────────────────────────────────────────
	// Har bir entry — VerticalLayout container ichida:
	//   header row (avatar + name + ID + delete)
	//   Ghost toggle
	//   Delete toggle
	//   Edit toggle
	state->addEntry = [=](const CustomSettings::PerPeerEntry &entry) {
		constexpr int kHeaderH = 56;
		constexpr int kAvSize  = 38;
		[[maybe_unused]] constexpr int kPadL = 14;
		[[maybe_unused]] constexpr int kPadR = 12;
		[[maybe_unused]] constexpr int kGap  = 12;
		constexpr int kDelBtnW = 76;

		const auto peerId = entry.peerId;
		const auto name = entry.displayName;

		// SlideWrap ichida — VerticalLayout container
		const auto entryWrap = state->entriesLayout->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				state->entriesLayout,
				object_ptr<Ui::VerticalLayout>(state->entriesLayout)),
			style::margins(),
			style::al_justify);
		state->entryWraps.append(entryWrap);
		const auto container = entryWrap->entity();

		// ── Header row (avatar + name + ID + delete) ──────────
		const auto header = container->add(
			object_ptr<Ui::RpWidget>(container),
			style::margins());
		header->setFixedHeight(kHeaderH);

		const auto av = Ui::CreateChild<Ui::RpWidget>(header);
		av->setFixedSize(kAvSize, kAvSize);
		const auto userpicView =
			std::make_shared<Ui::PeerUserpicView>();
		const auto session = &controller->session();
		av->paintRequest() | rpl::on_next([=](QRect) {
			Painter p(av);
			PaintPeerAvatar(
				p,
				QRect(0, 0, kAvSize, kAvSize),
				peerId,
				name,
				session,
				*userpicView);
		}, av->lifetime());

		const auto nameLabel = Ui::CreateChild<Ui::FlatLabel>(
			header,
			rpl::single(name.isEmpty() ? peerId : name),
			st::boxLabel);

		const auto idLabel = Ui::CreateChild<Ui::FlatLabel>(
			header,
			rpl::single(u"ID: "_q + peerId),
			st::customModHintLabel);

		const auto rmBtn = Ui::CreateChild<Ui::RoundButton>(
			header,
			rpl::single(u"Oʻchirish"_q),
			st::attentionBoxButton);
		rmBtn->setFixedWidth(kDelBtnW);

		av->show();
		nameLabel->show();
		idLabel->show();
		rmBtn->show();

		header->paintRequest() | rpl::on_next([=](QRect) {
			Painter p(header);
			p.fillRect(kPadL + kAvSize + kGap, kHeaderH - 1,
				header->width() - kPadL - kAvSize - kGap - kPadR, 1,
				st::shadowFg->c);
		}, header->lifetime());

		const auto layoutHeader = [=](int w) {
			av->move(kPadL, (kHeaderH - kAvSize) / 2);
			av->update();
			const auto textX = kPadL + kAvSize + kGap;
			const auto textW = w - textX - kGap - kDelBtnW - kPadR;
			if (textW <= 0) return;
			nameLabel->resizeToWidth(textW);
			nameLabel->move(textX, 10);
			nameLabel->update();
			idLabel->resizeToWidth(textW);
			idLabel->move(textX, 10 + nameLabel->height() + 2);
			idLabel->update();
			const auto btnY = (kHeaderH - st::defaultBoxButton.height) / 2;
			rmBtn->move(w - kPadR - kDelBtnW, btnY);
		};
		header->widthValue() | rpl::on_next(layoutHeader, header->lifetime());
		const auto initW = state->entriesLayout->width();
		if (initW > 0) layoutHeader(initW);

		// ── 3 ta toggle (Ghost / Delete / Edit) ──────────────
		const auto addToggle = [&](
				const QString &label,
				bool initial,
				Fn<void(bool)> onChange) {
			const auto btn = container->add(
				object_ptr<Ui::SettingsButton>(
					container,
					rpl::single(label),
					st::settingsButtonNoIcon));
			btn->toggleOn(rpl::single(initial));
			btn->toggledValue() | rpl::skip(1) | rpl::on_next([=](bool on) {
				onChange(on);
			}, btn->lifetime());
		};
		addToggle(u"Ghost Mode"_q, entry.ghostEnabled,
			[=](bool on) { CustomSettings::SetGhostModeForPeer(peerId, on); });
		addToggle(u"Anti-Delete"_q, entry.antiDeleteEnabled,
			[=](bool on) { CustomSettings::SetAntiDeleteForPeer(peerId, on); });
		addToggle(u"Anti-Edit"_q, entry.antiEditEnabled,
			[=](bool on) { CustomSettings::SetAntiEditForPeer(peerId, on); });
		// 2026-08-14: katta media'ni oldindan yuklab olish. Boshqa uchtadan
		// FARQLI — global bayrog'i yo'q, faqat shu toggle yoki White List
		// uni yoqadi (ShouldMediaBackup izohiga qarang).
		addToggle(u"Media Backup"_q, entry.mediaBackupEnabled,
			[=](bool on) { CustomSettings::SetMediaBackupForPeer(peerId, on); });

		rmBtn->addClickHandler([=] {
			rmBtn->setDisabled(true);
			CustomSettings::RemovePerPeerOverride(peerId);
			entryWrap->toggle(false, anim::type::normal);
			--state->visibleCount;
			updateHeader(true);
			Ui::Toast::Show(u"Per-Chat dan oʻchirildi."_q);
		});

		entryWrap->toggle(true, anim::type::instant);
		++state->visibleCount;
		updateHeader(false);

		if (state->entriesLayout->width() > 0) {
			state->entriesLayout->resizeToWidth(
				state->entriesLayout->width());
			state->entriesLayout->update();
		}
		if (content->width() > 0) {
			content->resizeToWidth(content->width());
			content->update();
		}
	};

	// ── Load saved overrides ──────────────────────────────────────
	updateHeader(false);
	{
		const auto entries = CustomSettings::GetPerPeerOverrides();
		for (const auto &e : entries) {
			state->addEntry(e);
		}
	}
}

// ── Activity History Log: kuzatish qamrovi + kuzatilayotganlar ro'yxati ──
void AddAvatarPeerRow(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		const QString &peerId,
		const QString &name,
		Fn<void()> onDelete) {
	constexpr int kRowH    = 56;
	constexpr int kAvSize  = 38;
	constexpr int kPadL = 14;
	constexpr int kPadR = 12;
	constexpr int kGap  = 12;
	constexpr int kDelBtnW = 76;

	const auto row = content->add(object_ptr<Ui::RpWidget>(content));
	row->setFixedHeight(kRowH);

	// Avatar circle — real userpic agar peer cache da bo'lsa,
	// aks holda fallback (harf + rang).
	const auto av = Ui::CreateChild<Ui::RpWidget>(row);
	av->setFixedSize(kAvSize, kAvSize);
	const auto userpicView = std::make_shared<Ui::PeerUserpicView>();
	const auto session = &controller->session();
	av->paintRequest() | rpl::on_next([=](QRect) {
		Painter p(av);
		PaintPeerAvatar(
			p,
			QRect(0, 0, kAvSize, kAvSize),
			peerId,
			name,
			session,
			*userpicView);
	}, av->lifetime());

	const auto nameLabel = Ui::CreateChild<Ui::FlatLabel>(
		row,
		rpl::single(name.isEmpty() ? peerId : name),
		st::boxLabel);
	const auto idLabel = Ui::CreateChild<Ui::FlatLabel>(
		row,
		rpl::single(u"ID: "_q + peerId),
		st::customModHintLabel);
	const auto delBtn = Ui::CreateChild<Ui::RoundButton>(
		row,
		rpl::single(u"Oʻchirish"_q),
		st::attentionBoxButton);
	delBtn->setFixedWidth(kDelBtnW);
	av->show();
	nameLabel->show();
	idLabel->show();
	delBtn->show();

	row->paintRequest() | rpl::on_next([=](QRect) {
		Painter p(row);
		p.fillRect(kPadL + kAvSize + kGap, kRowH - 1,
			row->width() - kPadL - kAvSize - kGap - kPadR, 1,
			st::shadowFg->c);
	}, row->lifetime());

	const auto layoutRow = [=](int w) {
		const auto avY = (kRowH - kAvSize) / 2;
		av->move(kPadL, avY);
		av->update();
		const auto textX = kPadL + kAvSize + kGap;
		const auto textW = w - textX - kGap - kDelBtnW - kPadR;
		if (textW <= 0) return;
		nameLabel->resizeToWidth(textW);
		nameLabel->move(textX, 10);
		nameLabel->update();
		idLabel->resizeToWidth(textW);
		idLabel->move(textX, 10 + nameLabel->height() + 2);
		idLabel->update();
		const auto btnY = (kRowH - st::defaultBoxButton.height) / 2;
		delBtn->move(w - kPadR - kDelBtnW, btnY);
	};
	row->widthValue() | rpl::on_next(layoutRow, row->lifetime());
	if (content->width() > 0) layoutRow(content->width());

	if (onDelete) {
		delBtn->addClickHandler([=] {
			delBtn->setDisabled(true);
			onDelete();
		});
	}
}

void fillActivityHistorySection(
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
					: u"Barcha kontaktlarni kuzatish oʻchirildi"_q);
			}, btn->lifetime());
	}

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
				Ui::Toast::Show(name + u" Include List'ga qoʻshildi."_q);
				if (onRebuild) onRebuild();
				return true;
			},
			rpl::single(u"Include List'ga qoʻshish"_q)));
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
				Ui::Toast::Show(name + u" Exclude List'ga qoʻshildi."_q);
				if (onRebuild) onRebuild();
				return true;
			},
			rpl::single(u"Exclude List'ga qoʻshish"_q)));
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

void fillPeersTab(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		Fn<void()> onRebuild) {
	Ui::AddSkip(content, st::settingsThumbSkip);
	{
		const auto lbl = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Ghost Mode, Anti-Delete va Anti-Edit funksiyalari "
					"qaysi chatlar uchun ishlashini bu yerdan boshqaring."_q),
				st::boxLabel),
			st::boxRowPadding,
			style::al_justify);
		content->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				lbl->resizeToWidth(lw);
				lbl->update();
			}
		}, lbl->lifetime());
	}

	Ui::AddSkip(content, 4);
	{
		const auto hint = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"💡 \"Chat tanlash\" tugmasi bosilganda asosiy Telegram oynasida "
					"chat tanlash oynasi ochiladi. Tanlagandan soʻng bu oyna avvalgi holatiga "
					"qaytadi."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
		content->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				hint->resizeToWidth(lw);
				hint->update();
			}
		}, hint->lifetime());
	}
	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);

	fillPeerSection(content, controller, true, onRebuild);

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);

	fillPeerSection(content, controller, false, onRebuild);

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	fillPerChatSection(content, controller);
	Ui::AddSkip(content, st::settingsThumbSkip);

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	fillActivityHistorySection(content, controller, onRebuild);
}

void fillArchiveTab(
		not_null<Ui::VerticalLayout*> content,
		Fn<void()> onRefresh) {
	// ── Yangilash tugmasi ────────────────────────────────────────────
	Ui::AddSkip(content, st::settingsThumbSkip);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"🔄 Yangilash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([onRefresh] {
		if (onRefresh) onRefresh();
	});
	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);

	const auto addMessageRows = [&](
			const QString &sectionTitle,
			const QVector<CustomDB::DeletedMessageWithPeer> &messages) {
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(sectionTitle),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		if (messages.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"Arxivda xabarlar yoʻq."_q),
					st::boxLabel),
				st::boxRowPadding);
			return;
		}

		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"%1 ta xabar — eng yangidan."_q
					.arg(messages.size())),
				st::customModHintLabel),
			st::defaultSubsectionTitlePadding);

		Ui::AddSkip(content, st::settingsThumbSkip);

		for (const auto &msg : messages) {
			const auto dateStr = msg.date
				? QDateTime::fromSecsSinceEpoch(msg.date)
					.toString(u"yyyy-MM-dd  hh:mm"_q)
				: u"unknown date"_q;
			const auto arrow = msg.isOut ? u"↑ You"_q : u"↓ Them"_q;
			const auto peerName =
				CustomSettings::GetPeerDisplayName(msg.peerId);
			const auto peerDisplay = peerName.isEmpty()
				? msg.peerId
				: (peerName + u" ("_q + msg.peerId + u")"_q);
			const auto header = u"[%1]  %2  —  Chat: %3"_q
				.arg(dateStr, arrow, peerDisplay);

			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(header),
					st::defaultSubsectionTitle),
				st::defaultSubsectionTitlePadding);

			const auto hasText = !msg.text.isEmpty();
			const auto hasMedia = !msg.mediaPath.isEmpty();
			auto body = QString();
			if (hasText) {
				body = msg.text.left(200).replace(u'\n', u' ');
				if (msg.text.size() > 200) body += u"…"_q;
			}
			if (hasMedia) {
				const auto mediaNote = u"[media: %1]"_q
					.arg(QFileInfo(msg.mediaPath).fileName());
				body = body.isEmpty()
					? mediaNote
					: (body + u"  "_q + mediaNote);
			}
			// YANGI-1: media fayl saqlanmagan, lekin is_media flag bor (background
			// delete) — "(media xabar)" ko'rsatamiz, "(empty)" emas.
			if (body.isEmpty() && msg.isMedia) body = u"(media xabar)"_q;
			if (body.isEmpty()) body = u"(empty)"_q;

			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(body),
					st::boxLabel),
				st::boxRowPadding);

			Ui::AddSkip(content, 6);
		}
	};

	addMessageRows(
		u"🗑️ Oʻchirilgan xabarlar"_q,
		CustomDB::GetAllDeletedMessages(300));

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);

	{
		const auto records = CustomDB::GetAllEditedMessages(300);
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"✏ Tahrirlangan xabarlar"_q),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		if (records.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"Arxivda tahrir yozuvlari yoʻq."_q),
					st::boxLabel),
				st::boxRowPadding);
		} else {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"%1 ta tahrir yozuvi — eng yangidan."_q
						.arg(records.size())),
					st::customModHintLabel),
				st::defaultSubsectionTitlePadding);

			Ui::AddSkip(content, st::settingsThumbSkip);

			for (const auto &rec : records) {
				const auto when = rec.editedAt.isValid()
					? rec.editedAt.toString(u"yyyy-MM-dd  hh:mm"_q)
					: (rec.msgDate
						? QDateTime::fromSecsSinceEpoch(rec.msgDate)
							.toString(u"yyyy-MM-dd  hh:mm"_q)
						: u"unknown date"_q);
				const auto editPeerName =
					CustomSettings::GetPeerDisplayName(rec.peerId);
				const auto editPeerDisplay = editPeerName.isEmpty()
					? rec.peerId
					: (editPeerName + u" ("_q + rec.peerId + u")"_q);
				const auto header =
					u"[%1]  Chat: %2  xabar #%3"_q
						.arg(when, editPeerDisplay)
						.arg(rec.msgId);

				content->add(
					object_ptr<Ui::FlatLabel>(
						content,
						rpl::single(header),
						st::defaultSubsectionTitle),
					st::defaultSubsectionTitlePadding);

				const auto orig = rec.originalText.isEmpty()
					? u"(empty)"_q
					: rec.originalText.left(200).replace(u'\n', u' ');
				content->add(
					object_ptr<Ui::FlatLabel>(
						content,
						rpl::single(u"Avvalgi: "_q + orig),
						st::boxLabel),
					st::boxRowPadding);

				if (!rec.newText.isEmpty()) {
					const auto nw =
						rec.newText.left(200).replace(u'\n', u' ');
					content->add(
						object_ptr<Ui::FlatLabel>(
							content,
							rpl::single(u"Yangi:  "_q + nw),
							st::boxLabel),
						st::boxRowPadding);

					const auto diff = MakeWordDiff(
						rec.originalText.left(200).replace(u'\n', u' '),
						rec.newText.left(200).replace(u'\n', u' '));
					if (diff != orig) {
						content->add(
							object_ptr<Ui::FlatLabel>(
								content,
								rpl::single(u"Farq:   "_q + diff),
								st::customModHintLabel),
							st::boxRowPadding);
					}
				}

				Ui::AddSkip(content, 6);
			}
		}
	}

	Ui::AddSkip(content, st::settingsThumbSkip);
}

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
				u"🗄️ Arxiv holati: %1 oʻchirilgan • %2 tahrirlangan"_q
				.arg(stats0.deletedCount).arg(stats0.editedCount)),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	// Arxiv statistikasini yangilash uchun yordamchi.
	const auto refreshStats = [statsLabel]() {
		const auto s = CustomDB::GetArchiveStats();
		statsLabel->setText(
			u"🗄️ Arxiv holati: %1 oʻchirilgan • %2 tahrirlangan"_q
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
				"• Oʻchirilgan/tahrirlangan xabarlar bazasi\n"
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
			rpl::single(u"📁 Papkani o‘zgartirish"_q),
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
		moveBox.setWindowTitle(u"Arxiv papkasini o‘zgartirish"_q);
		moveBox.setText(
			u"Eski: "_q + current
			+ u"\nYangi: "_q + QDir::cleanPath(chosen)
			+ u"\n\nMavjud maʼlumotlarni yangi papkaga "
			  "ko‘chiraymi?"_q);
		const auto moveBtn = moveBox.addButton(
			u"📦 Ko‘chirish"_q, QMessageBox::AcceptRole);
		moveBox.addButton(
			u"Ko‘chirmasdan, yangi joydan boshlansin"_q,
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
				? u"Papka o‘zgartirildi. Maʼlumotlar qayta ishga "
				  "tushganda ko‘chiriladi. Dastur 3 soniyada qayta "
				  "yuklanadi..."_q
				: u"Papka o‘zgartirildi. Dastur 3 soniyada qayta "
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
						? (u"%1 ta fayl indeksga qoʻshildi.\n\n"_q).arg(added)
						: u"Indeksga yangi fayl qoʻshilmadi.\n\n"_q;
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
							+ u"Tuzatish kerak boʻlgan fayllar:\n"_q
							+ u"• kengaytma qoʻshiladi: "_q
							+ QString::number(preview.extensionAdded)
							+ u"\n• toʻgʻri papkaga koʻchiriladi: "_q
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
									u"Tuzatildi: %1 kengaytma, %2 koʻchirildi"
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
			rpl::single(u"📤 Toʻliq zaxira nusxa olish"_q),
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
			u"🔄 Toʻliq almashtirish"_q, QMessageBox::DestructiveRole);
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
				? u"DIQQAT! Joriy arxivdagi BARCHA maʻlumotlar\n"
				  "(oʻchirilgan/tahrirlangan xabarlar, media) OʻCHIRILADI\n"
				  "va tanlangan zaxira bilan toʻliq almashtiriladi.\n\n"
				  "Bu amalni ortga qaytarib boʻlmaydi. Davom etasizmi?"_q
				: u"Tanlangan zaxiradagi maʻlumotlar JORIY arxivga QOʻSHILADI\n"
				  "(birlashtiriladi) — hozirgi qurilmadagi oʻchirilgan/tahrirlangan\n"
				  "xabarlar va media saqlanib qoladi, oʻchirilmaydi.\n\n"
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
						? u"Toʻliq almashtirish muvaffaqiyatli! "_q
						: u"Tiklash muvaffaqiyatli! "_q)
					+ u"%1 oʻchirilgan, %2 tahrirlangan. "
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
			rpl::single(u"🎞 Media arxivini qoʻshish (CustomModMedia_*.zip)"_q),
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
				? u"Media arxivi qoʻshildi ✓"_q
				: u"Media arxivini qoʻshib boʻlmadi."_q);
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
			rpl::single(u"Quyidagi amallar arxiv maʻlumotlarini BUTUNLAY oʻchiradi.\n"
				"Bu amalni bekor qilib boʻlmaydi. Tasdiqlash talab qilinadi."_q),
			st::customModHintLabel),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"🗑️  Oʻchirilganlar arxivini tozalash"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto reply = QMessageBox::warning(
			dialogParent,
			u"Oʻchirilganlar arxivini tozalash"_q,
			u"Bu amal BARCHA saqlangan oʻchirilgan xabarlarni oʻchiradi.\n\n"
			"Bu amalni bekor qilib boʻlmaydi.\n\nDavom etasizmi?"_q,
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (reply != QMessageBox::Yes) return;
		CustomDB::ClearDeletedArchive();
		refreshStats();
		if (onArchiveChanged) onArchiveChanged();
		Ui::Toast::Show(u"🗑️ Oʻchirilganlar arxivi tozalandi."_q);
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
			u"Bu amal BARCHA saqlangan tahrir yozuvlarini oʻchiradi.\n\n"
			"Bu amalni bekor qilib boʻlmaydi.\n\nDavom etasizmi?"_q,
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
			rpl::single(u"☠️  BARCHA arxivni tozalash (Oʻchirilgan + Tahrir)"_q),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto first = QMessageBox::warning(
			dialogParent,
			u"BARCHA arxivni tozalash"_q,
			u"Bu amal BARCHA arxiv maʻlumotlarini oʻchiradi:\n"
			"  • Barcha oʻchirilgan xabar yozuvlari\n"
			"  • Barcha tahrir tarixi yozuvlari\n\n"
			"Bu amalni bekor qilib boʻlmaydi."_q,
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (first != QMessageBox::Yes) return;
		const auto second = QMessageBox::critical(
			dialogParent,
			u"Yakuniy tasdiqlash"_q,
			u"HAQIQATAN HAM ishonchingiz komilmi?\n\n"
			"Barcha oʻchirilgan xabar va tahrir yozuvlari\n"
			"BUTUNLAY yoʻqoladi."_q,
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (second != QMessageBox::Yes) return;
		CustomDB::ClearAllArchive();
		refreshStats();
		if (onArchiveChanged) onArchiveChanged();
		Ui::Toast::Show(u"☠️ Barcha arxiv maʻlumotlari tozalandi."_q);
	});

	Ui::AddSkip(content, st::settingsThumbSkip);
}

} // namespace

namespace CustomMod {

void OpenOrRaise(not_null<Window::SessionController*> controller) {
	if (gInstance) {
		gInstance->raise();
		gInstance->activateWindow();
		return;
	}
	const auto w = new CustomModWindow(controller);
	gInstance = w;
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->show();
}

} // namespace CustomMod
