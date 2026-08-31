#include "custom_mod_window.h"
#include "custom_tab_common.h"

class CustomModWindow final : public Ui::RpWidget {
public:
	explicit CustomModWindow(
		not_null<Window::SessionController*> controller);

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
