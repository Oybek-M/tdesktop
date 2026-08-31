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
	std::array<Ui::ScrollArea*, 7> _panels = {};
	std::array<QPointer<Ui::VerticalLayout>, 7> _inners = {};
	std::unique_ptr<Ui::LayerManager> _layerManager;
};

CustomModWindow::CustomModWindow(
	not_null<Window::SessionController*> controller)
: Ui::RpWidget(nullptr) {
	setWindowFlags(Qt::Window);
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
		resize(600, 720);
		const auto screen = QGuiApplication::primaryScreen()
			->availableGeometry();
		move(screen.center() - QPoint(300, 360));
	} else {
		restoreGeometry(geom);
	}

	_tabBar = new CustomTabBar(
		this,
		{
			u"Yashirinlik"_q,
			u"Ko'rinish"_q,
			u"Chatlar"_q,
			u"Faollik"_q,
			u"Arxiv"_q,
			u"Ombor"_q,
			u"Tizim"_q,
		});

	for (auto i = 0; i < 7; ++i) {
		_panels[i] = new Ui::ScrollArea(this);
	}

	setupContent(controller);
	switchTab(0);

	_layerManager = std::make_unique<Ui::LayerManager>(this);

	std::move(_tabBar->tabSelected()) | rpl::on_next([=](int idx) {
		switchTab(idx);
	}, lifetime());
}

QPointer<CustomModWindow> gInstance;

void ShowCustomBox(object_ptr<Ui::BoxContent> box) {
	if (gInstance) {
		gInstance->showBox(std::move(box));
	}
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

	// 1. Yashirinlik
	fillPrivacyTab(makeInner(0));

	// 2. Ko'rinish
	fillAppearanceTab(makeInner(1));

	// 3. Chatlar
	const auto panelChats = _panels[2];
	const auto rebuildChats = std::make_shared<Fn<void()>>();
	*rebuildChats = [=]() {
		const auto inner = panelChats->setOwnedWidget(
			object_ptr<Ui::VerticalLayout>(panelChats));
		_inners[2] = inner;
		panelChats->widthValue() | rpl::on_next([=](int w) {
			inner->resizeToWidth(w);
		}, inner->lifetime());
		fillChatsTab(inner, controller, *rebuildChats);
	};
	(*rebuildChats)();

	// 4. Faollik
	const auto panelAct = _panels[3];
	const auto rebuildAct = std::make_shared<Fn<void()>>();
	*rebuildAct = [=]() {
		const auto inner = panelAct->setOwnedWidget(
			object_ptr<Ui::VerticalLayout>(panelAct));
		_inners[3] = inner;
		panelAct->widthValue() | rpl::on_next([=](int w) {
			inner->resizeToWidth(w);
		}, inner->lifetime());
		fillActivityTab(inner, controller, *rebuildAct);
	};
	(*rebuildAct)();

	// 5. Arxiv
	const auto panelArch = _panels[4];
	const auto rebuildArchive = std::make_shared<Fn<void()>>();
	*rebuildArchive = [=]() {
		const auto inner = panelArch->setOwnedWidget(
			object_ptr<Ui::VerticalLayout>(panelArch));
		_inners[4] = inner;
		panelArch->widthValue() | rpl::on_next([=](int w) {
			inner->resizeToWidth(w);
		}, inner->lifetime());
		fillArchiveTab(inner, *rebuildArchive);
	};
	(*rebuildArchive)();

	// 6. Ombor
	fillStorageTab(makeInner(5), this, *rebuildArchive);

	// 7. Tizim
	fillSystemTab(makeInner(6), this, *rebuildArchive);
}

void CustomModWindow::resizeEvent(QResizeEvent *) {
	const auto tabH = _tabBar->height();
	_tabBar->setGeometry(0, 0, width(), tabH);
	const auto panelW = width();
	const auto panelH = height() - tabH;
	for (auto i = 0; i < 7; ++i) {
		_panels[i]->setGeometry(0, tabH, panelW, panelH);
		if (const auto inner = _inners[i].data(); inner && panelW > 0) {
			inner->resizeToWidth(panelW);
			if (_panels[i]->isVisible()) inner->update();
		}
	}
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
	if (_tabBar) {
		const auto panelW = width();
		const auto panelH = height() - _tabBar->height();
		if (panelW > 0 && panelH > 0) {
			for (auto i = 0; i < 7; ++i) {
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
	for (auto i = 0; i < 7; ++i) {
		_panels[i]->setVisible(i == index);
	}
	if (const auto inner = _inners[index].data()) {
		const auto panelW = width();
		if (panelW > 0) {
			inner->resizeToWidth(panelW);
			inner->update();
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
