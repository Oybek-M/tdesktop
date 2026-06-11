# CustomMod Standalone Window — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract all CustomMod UI from `settings_main.cpp` GenericBox into a non-modal, resizable, singleton OS-level window with four tabs (General, Peers, Archive, About).

**Architecture:** `CustomModWindow` is a bare `Ui::RpWidget` with `Qt::Window` flag — a real top-level OS window, not a dialog. A lightweight `CustomTabBar` (also `Ui::RpWidget`) lives at the top; four `Ui::ScrollArea` panels sit below, only one visible at a time. Content is built by four `fill*Tab()` free functions that accept a `not_null<Ui::VerticalLayout*>` — identical pattern to tdesktop's stats windows.

**Tech Stack:** Qt 6, tdesktop `Ui::RpWidget`, `rpl::`, `QSettings`, `Ui::ScrollArea`, `Ui::VerticalLayout`, `Ui::SettingsButton`, `Ui::SlideWrap`, `Painter`

---

## File Map

| Action | File | Responsibility |
|--------|------|----------------|
| Create | `Telegram/SourceFiles/custom_mod_window.h` | Public `CustomMod::OpenOrRaise()` declaration |
| Create | `Telegram/SourceFiles/custom_mod_window.cpp` | Full implementation: window, tab bar, all 4 tab fill functions |
| Create | `Telegram/SourceFiles/styles/style_custom_mod.style` | Scale-safe dimension constants for the window |
| Modify | `Telegram/CMakeLists.txt` | Register new `.cpp`, `.h`, `.style` files |
| Modify | `Telegram/SourceFiles/settings/sections/settings_main.cpp` | Replace `Box(CustomModBox, controller)` with `CustomMod::OpenOrRaise(controller)`; delete all dead CustomMod functions and their now-unused includes |

---

## Task 1: Style file + CMakeLists + header skeleton

**Files:**
- Create: `Telegram/SourceFiles/styles/style_custom_mod.style`
- Create: `Telegram/SourceFiles/custom_mod_window.h`
- Modify: `Telegram/CMakeLists.txt`

- [ ] **Step 1.1: Create the style file**

```style
using "ui/basic.style";
using "styles/style_layers.style";

customModTabBarVSkip: 8px;
```

Save as `Telegram/SourceFiles/styles/style_custom_mod.style` (UTF-8 without BOM, CRLF).

- [ ] **Step 1.2: Create the header**

```cpp
#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace CustomMod {

void OpenOrRaise(not_null<Window::SessionController*> controller);

} // namespace CustomMod
```

Save as `Telegram/SourceFiles/custom_mod_window.h` (UTF-8 without BOM, CRLF).

- [ ] **Step 1.3: Register in CMakeLists.txt**

Open `Telegram/CMakeLists.txt`. Find the block that currently reads:

```cmake
    custom_db.cpp
    custom_db.h
    custom_settings.cpp
    custom_settings.h
```

Add the three new files immediately after:

```cmake
    custom_db.cpp
    custom_db.h
    custom_settings.cpp
    custom_settings.h
    custom_mod_window.cpp
    custom_mod_window.h
    styles/style_custom_mod.style
```

- [ ] **Step 1.4: Commit**

```
git add Telegram/SourceFiles/custom_mod_window.h
git add Telegram/SourceFiles/styles/style_custom_mod.style
git add Telegram/CMakeLists.txt
git commit -m "chore: add custom_mod_window skeleton files and register in CMake"
```

---

## Task 2: CustomTabBar widget

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp` (create if first task writing it)

- [ ] **Step 2.1: Create the file with includes and CustomTabBar**

```cpp
#include "custom_mod_window.h"

#include "custom_db.h"
#include "custom_settings.h"
#include "data/data_thread.h"
#include "ui/painter.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_basic.h"
#include "styles/style_custom_mod.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

namespace {

class CustomTabBar final : public Ui::RpWidget {
public:
	CustomTabBar(QWidget *parent, std::initializer_list<QString> names)
	: Ui::RpWidget(parent)
	, _names(names) {
		setFixedHeight(st::defaultBoxButton.height + st::customModTabBarVSkip);
		setCursor(Qt::PointingHandCursor);
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
		if (_names.empty()) return;
		const auto tabW = width() / int(_names.size());
		const auto h = height();
		for (auto i = 0; i < int(_names.size()); ++i) {
			const auto active = (i == _active);
			p.setPen(active
				? st::windowActiveTextFg->c
				: st::windowSubTextFg->c);
			p.setFont(active ? st::semiboldFont : st::normalFont);
			p.drawText(
				QRect(i * tabW, 0, tabW, h - 2),
				_names[i],
				QTextOption(Qt::AlignCenter));
			if (active) {
				p.fillRect(
					i * tabW + 4, h - 2,
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
	rpl::event_source<int> _tabSelected;
};

} // namespace
```

Save as `Telegram/SourceFiles/custom_mod_window.cpp` (UTF-8 without BOM, CRLF).

- [ ] **Step 2.2: Commit**

```
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "feat: add CustomTabBar widget"
```

---

## Task 3: CustomModWindow class + singleton

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp`

The forward-declarations of `fill*Tab` go **above** `CustomModWindow` in the anonymous namespace; their bodies come later. The singleton `gInstance` and the public `OpenOrRaise` go in a separate block **outside** the anonymous namespace.

- [ ] **Step 3.1: Add forward declarations and CustomModWindow class after CustomTabBar in the anonymous namespace**

Append after the closing `} // namespace` of the CustomTabBar class, but **before** the closing `} // namespace` of the anonymous namespace:

```cpp
void fillGeneralTab(not_null<Ui::VerticalLayout*> content);
void fillPeersTab(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller);
void fillArchiveTab(not_null<Ui::VerticalLayout*> content);
void fillAboutTab(
	not_null<Ui::VerticalLayout*> content,
	QWidget *dialogParent);

class CustomModWindow final : public Ui::RpWidget {
public:
	explicit CustomModWindow(
		not_null<Window::SessionController*> controller);

protected:
	void resizeEvent(QResizeEvent *e) override;
	void closeEvent(QCloseEvent *e) override;

private:
	void switchTab(int index);
	void setupContent(not_null<Window::SessionController*> controller);

	CustomTabBar *_tabBar = nullptr;
	std::array<Ui::ScrollArea*, 4> _panels = {};
};
```

- [ ] **Step 3.2: Add CustomModWindow constructor and methods, still inside the anonymous namespace**

```cpp
CustomModWindow::CustomModWindow(
	not_null<Window::SessionController*> controller)
: Ui::RpWidget(nullptr) {
	setWindowFlags(Qt::Window);
	setWindowTitle(u"Customizations (By Oybek)"_q);
	setMinimumSize(480, 400);

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

	std::move(_tabBar->tabSelected()) | rpl::on_next([=](int idx) {
		switchTab(idx);
	}, lifetime());
}

void CustomModWindow::setupContent(
		not_null<Window::SessionController*> controller) {
	const auto makeInner = [&](int idx) -> not_null<Ui::VerticalLayout*> {
		const auto inner = _panels[idx]->setOwnedWidget(
			object_ptr<Ui::VerticalLayout>(_panels[idx]));
		_panels[idx]->widthValue() | rpl::on_next([=](int w) {
			inner->resizeToWidth(w);
		}, inner->lifetime());
		return inner;
	};

	fillGeneralTab(makeInner(0));
	fillPeersTab(makeInner(1), controller);
	fillArchiveTab(makeInner(2));
	fillAboutTab(makeInner(3), this);
}

void CustomModWindow::resizeEvent(QResizeEvent *) {
	const auto tabH = _tabBar->height();
	_tabBar->setGeometry(0, 0, width(), tabH);
	for (auto *panel : _panels) {
		panel->setGeometry(0, tabH, width(), height() - tabH);
	}
}

void CustomModWindow::closeEvent(QCloseEvent *e) {
	auto s = QSettings(u"CustomMod"_q, u"TelegramDesktop"_q);
	s.setValue(u"WindowGeometry"_q, saveGeometry());
	Ui::RpWidget::closeEvent(e);
}

void CustomModWindow::switchTab(int index) {
	_tabBar->setActiveTab(index);
	for (auto i = 0; i < 4; ++i) {
		_panels[i]->setVisible(i == index);
	}
}
```

- [ ] **Step 3.3: Add singleton and public API after the closing `} // namespace` of the anonymous namespace**

```cpp
namespace {
QPointer<CustomModWindow> gInstance;
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
```

- [ ] **Step 3.4: Commit**

```
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "feat: add CustomModWindow skeleton with singleton and tab switching"
```

---

## Task 4: fillGeneralTab

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp`

- [ ] **Step 4.1: Add fillGeneralTab body inside the anonymous namespace, before the forward declarations**

```cpp
void fillGeneralTab(not_null<Ui::VerticalLayout*> content) {
	const auto addSection = [&](const QString &title) {
		Ui::AddSkip(content, st::settingsThumbSkip);
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
		auto current = true;
		if (id == u"ghostMode"_q) current = val.ghostMode;
		else if (id == u"bypassRestrictions"_q) current = val.bypassRestrictions;
		else if (id == u"offlineDb"_q) current = val.offlineDb;
		else if (id == u"antiDelete"_q) current = val.antiDelete;
		else if (id == u"antiEdit"_q) current = val.antiEdit;
		else if (id == u"spoofMobile"_q) current = val.spoofMobile;

		const auto btn = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(text),
				st::settingsButtonNoIcon));
		btn->toggleOn(rpl::single(current));
		btn->toggledValue() | rpl::on_next([=](bool on) {
			CustomSettings::Set(id, on);
		}, btn->lifetime());

		if (!description.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(description),
					st::settingsScaleLabel),
				st::defaultSubsectionTitlePadding);
		}
	};

	{
		const auto stats = CustomDB::GetArchiveStats();
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(QString::fromUtf8(
					"\xF0\x9F\x97\x84 Arxiv: %1 o\xCA\xBB"
					"chirilgan \xE2\x80\xA2 %2 tahrirlangan"
				).arg(stats.deletedCount).arg(stats.editedCount)),
				st::settingsScaleLabel),
			st::defaultSubsectionTitlePadding);
	}

	addSection(QString::fromUtf8("\xF0\x9F\x91\xBB Maxfiylik va Ghost Mode"));
	addToggle(
		u"ghostMode"_q,
		QString::fromUtf8("Ghost Mode (Ko\xCA\xBBrinmaslik)"),
		QString::fromUtf8(
			"Online holat, yozish belgisi va o\xCA\xBBqilganlik tasdiqini yashiradi. "
			"So\xCA\xBBnggi ko\xCA\xBBrilish vaqti serverlarga UMUMAN yuborilmaydi."));
	addToggle(
		u"spoofMobile"_q,
		QString::fromUtf8("Mobil qurilma ko\xCA\xBBrinishi"),
		QString::fromUtf8(
			"Telegram mobil ilovadan ishlatilayotgandek ko\xCA\xBBrinadi."));

	addSection(QString::fromUtf8(
		"\xF0\x9F\x94\x93 Cheklovlar va Yo\xCA\xBBnaltirish"));
	addToggle(
		u"bypassRestrictions"_q,
		QString::fromUtf8(
			"Yo\xCA\xBBnaltirish/Ko\xCA\xBB" "chirish cheklovini chetlab o\xCA\xBBtish"),
		QString::fromUtf8(
			"Cheklangan chatlardagi xabarlarni yo\xCA\xBBnaltirish va "
			"ko\xCA\xBB" "chirishga ruxsat beradi."));

	addSection(QString::fromUtf8("\xF0\x9F\x92\xBE Xabarlarni Saqlash"));
	addToggle(
		u"antiDelete"_q,
		QString::fromUtf8("Anti-Delete (O\xCA\xBB" "chirilgan xabarlar)"),
		QString::fromUtf8(
			"O\xCA\xBB" "chirilgan xabarlarni ko\xCA\xBBrinishda qoldiradi. "
			"SQLite bazasida saqlanadi."));
	addToggle(
		u"antiEdit"_q,
		QString::fromUtf8("Anti-Edit (Tahrir tarixi)"),
		QString::fromUtf8(
			"Tahrirdan oldingi matnni ko\xCA\xBBrsatadi. Mahalliy bazada saqlanadi."));
	addToggle(
		u"offlineDb"_q,
		QString::fromUtf8("Offline xabar bazasi"),
		QString::fromUtf8(
			"Xabarlar va medialarni oflayn foydalanish uchun mahalliy nusxada saqlaydi."));

	Ui::AddSkip(content, st::settingsThumbSkip);
}
```

- [ ] **Step 4.2: Commit**

```
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "feat: implement fillGeneralTab with 5 toggle rows"
```

---

## Task 5: fillPeerSection + fillPeersTab

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp`

This is the most complex tab — it mirrors the logic of `PeerListBox` from `settings_main.cpp` but outputs into a `VerticalLayout` instead of a `GenericBox`. All `box->addRow(w, p)` → `content->add(w, p)`, all `box->addSkip(n)` → `Ui::AddSkip(content, n)`, all `box->lifetime().make_state<>()` → `content->lifetime().make_state<>()`. The "OK" close button is removed entirely — not needed in a tab.

- [ ] **Step 5.1: Add fillPeerSection inside the anonymous namespace**

```cpp
void fillPeerSection(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		bool isWhitelist) {
	struct State {
		int visibleCount = 0;
		Ui::VerticalLayout *entriesLayout = nullptr;
		Ui::FlatLabel *countLabel = nullptr;
		Ui::SlideWrap<Ui::FlatLabel> *emptyWrap = nullptr;
		Fn<void(const QString &, const QString &)> addEntry;
	};
	const auto state = content->lifetime().make_state<State>();

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(isWhitelist
				? QString::fromUtf8(
					"Bu ro\xCA\xBByxatdagi peerlar uchun Ghost, Anti-Delete va "
					"Anti-Edit \xF0\x9F\x9F\xA2 DOIM YOQIQ bo\xCA\xBBladi.\n"
					"(Black List dan past ustuvorlik)")
				: QString::fromUtf8(
					"Bu ro\xCA\xBByxatdagi peerlar uchun Ghost, Anti-Delete va "
					"Anti-Edit \xF0\x9F\x94\xB4 DOIM O\xCA\xBB" "CHIQ bo\xCA\xBBladi.\n"
					"(Eng yuqori ustuvorlik — White List ta\xCA\xBBsir qilmaydi)")),
			st::boxLabel),
		st::boxRowPadding);

	Ui::AddSkip(content, 4);

	state->countLabel = content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString()),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	state->entriesLayout = content->add(
		object_ptr<Ui::VerticalLayout>(content),
		{0, 0, 0, 0});

	state->emptyWrap = state->entriesLayout->add(
		object_ptr<Ui::SlideWrap<Ui::FlatLabel>>(
			state->entriesLayout,
			object_ptr<Ui::FlatLabel>(
				state->entriesLayout,
				rpl::single(QString::fromUtf8(
					"\xE2\x84\xB9\xEF\xB8\x8F  Ro\xCA\xBByxat bo\xCA\xBBsh. "
					"Quyida peer qo\xCA\xBBshing.")),
				st::settingsScaleLabel)),
		st::defaultSubsectionTitlePadding);

	const auto updateHeader = [=](bool animated) {
		const auto n = state->visibleCount;
		if (n == 0) {
			state->countLabel->setText(isWhitelist
				? QString::fromUtf8(
					"\xF0\x9F\x93\x8B White List (bo\xCA\xBBsh):")
				: QString::fromUtf8(
					"\xF0\x9F\x9A\xAB Black List (bo\xCA\xBBsh):"));
		} else {
			state->countLabel->setText(isWhitelist
				? QString::fromUtf8(
					"\xF0\x9F\x93\x8B White List (%1 ta peer):").arg(n)
				: QString::fromUtf8(
					"\xF0\x9F\x9A\xAB Black List (%1 ta peer):").arg(n));
		}
		state->emptyWrap->toggle(
			n == 0,
			animated ? anim::type::normal : anim::type::instant);
	};

	state->addEntry = [=](const QString &peerId, const QString &name) {
		const auto isWL = isWhitelist;

		const auto entryWrap = state->entriesLayout->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				state->entriesLayout,
				object_ptr<Ui::VerticalLayout>(state->entriesLayout)));
		const auto entryInner = entryWrap->entity();

		entryInner->add(
			object_ptr<Ui::FlatLabel>(
				entryInner,
				rpl::single(QString::fromUtf8(
					"\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
					"\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
					"\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80")),
				st::settingsScaleLabel),
			{8, 6, 8, 0});

		const auto nameStr = name.isEmpty()
			? (QString::fromUtf8("\xF0\x9F\x86\x94 ") + peerId)
			: (QString::fromUtf8("\xF0\x9F\x91\xA4 ") + name);
		entryInner->add(
			object_ptr<Ui::FlatLabel>(
				entryInner,
				rpl::single(nameStr),
				st::boxLabel),
			{8, 4, 8, 0});

		if (!name.isEmpty()) {
			entryInner->add(
				object_ptr<Ui::FlatLabel>(
					entryInner,
					rpl::single(QString::fromUtf8("ID: ") + peerId),
					st::settingsScaleLabel),
				{8, 2, 8, 0});
		}

		const auto btnH = st::defaultBoxButton.height;
		const auto btnRow = entryInner->add(
			object_ptr<Ui::RpWidget>(entryInner),
			{8, 6, 8, 8});
		btnRow->setFixedHeight(btnH);

		const auto moveBtnText = isWL
			? QString::fromUtf8("\xF0\x9F\x9A\xAB Black List ga")
			: QString::fromUtf8("\xF0\x9F\x93\x8B White List ga");

		const auto moveBtn = Ui::CreateChild<Ui::RoundButton>(
			btnRow,
			rpl::single(moveBtnText),
			st::defaultActiveButton);
		moveBtn->show();

		const auto removeBtn = Ui::CreateChild<Ui::RoundButton>(
			btnRow,
			rpl::single(QString::fromUtf8("\xF0\x9F\x97\x91 O\xCA\xBB" "chir")),
			st::attentionBoxButton);
		removeBtn->show();

		btnRow->widthValue() | rpl::on_next([=](int w) {
			const auto gap = st::lineWidth * 4;
			const auto half = (w - gap) / 2;
			if (half <= 0) return;
			moveBtn->setGeometry(0, 0, half, btnH);
			removeBtn->setGeometry(half + gap, 0, w - half - gap, btnH);
		}, btnRow->lifetime());

		const auto peerIdCopy = peerId;
		const auto nameCopy = name;

		moveBtn->addClickHandler([=] {
			moveBtn->setDisabled(true);
			removeBtn->setDisabled(true);
			if (isWL) {
				CustomSettings::RemoveFromWhitelist(peerIdCopy);
				CustomSettings::AddToBlocklist(peerIdCopy, nameCopy);
				Ui::Toast::Show(QString::fromUtf8(
					"\xF0\x9F\x9A\xAB Black List ga ko\xCA\xBB" "chirildi."));
			} else {
				CustomSettings::RemoveFromBlocklist(peerIdCopy);
				CustomSettings::AddToWhitelist(peerIdCopy, nameCopy);
				Ui::Toast::Show(QString::fromUtf8(
					"\xF0\x9F\x93\x8B White List ga ko\xCA\xBB" "chirildi."));
			}
			entryWrap->toggle(false, anim::type::normal);
			--state->visibleCount;
			updateHeader(true);
		});

		removeBtn->addClickHandler([=] {
			moveBtn->setDisabled(true);
			removeBtn->setDisabled(true);
			if (isWL) {
				CustomSettings::RemoveFromWhitelist(peerIdCopy);
			} else {
				CustomSettings::RemoveFromBlocklist(peerIdCopy);
			}
			entryWrap->toggle(false, anim::type::normal);
			--state->visibleCount;
			updateHeader(true);
			Ui::Toast::Show(QString::fromUtf8(
				"Ro\xCA\xBByxatdan o\xCA\xBB" "chirildi."));
		});

		++state->visibleCount;
		updateHeader(false);

		if (state->entriesLayout->width() > 0) {
			state->entriesLayout->resizeToWidth(
				state->entriesLayout->width());
		}
	};

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
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(isWhitelist
				? QString::fromUtf8(
					"\xE2\x9E\x95 White List ga yangi peer qo\xCA\xBBshish")
				: QString::fromUtf8(
					"\xE2\x9E\x95 Black List ga yangi peer qo\xCA\xBBshish")),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(QString::fromUtf8(
				"\xF0\x9F\x92\xAC  Chat tanlash (dialog ro\xCA\xBByxatidan)")),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		controller->show(Window::PrepareChooseRecipientBox(
			&controller->session(),
			[=](not_null<Data::Thread*> thread) -> bool {
				const auto peer = thread->peer();
				const auto peerId = QString::number(peer->id.value);
				const auto name = peer->name();
				const auto already = isWhitelist
					? CustomSettings::IsInWhitelist(peerId)
					: CustomSettings::IsInBlocklist(peerId);
				if (already) {
					Ui::Toast::Show(QString::fromUtf8(
						"\xE2\x9A\xA0\xEF\xB8\x8F Bu peer allaqachon "
						"ro\xCA\xBByxatda."));
					return true;
				}
				if (isWhitelist) {
					CustomSettings::AddToWhitelist(peerId, name);
				} else {
					CustomSettings::AddToBlocklist(peerId, name);
				}
				state->addEntry(peerId, name);
				Ui::Toast::Show(
					QString::fromUtf8("Qo\xCA\xBBshildi: ") + name);
				return true;
			},
			rpl::single(isWhitelist
				? QString::fromUtf8("White List ga qo\xCA\xBBshish")
				: QString::fromUtf8("Black List ga qo\xCA\xBBshish"))));
	});

	Ui::AddSkip(content, 8);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString::fromUtf8(
				"Yoki Peer ID orqali qo\xCA\xBBshing:")),
			st::settingsScaleLabel),
		st::defaultSubsectionTitlePadding);

	const auto peerIdInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(QString::fromUtf8("Peer ID  (masalan: 123456789)")),
			QString()),
		st::boxRowPadding);

	const auto nameInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(QString::fromUtf8("Nom (ixtiyoriy)")),
			QString()),
		st::boxRowPadding);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString::fromUtf8(
				"\xF0\x9F\x92\xA1 Peer ID ni chat URL yoki "
				"debug rejimidan topish mumkin.")),
			st::settingsScaleLabel),
		st::boxRowPadding);

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(isWhitelist
				? QString::fromUtf8(
					"\xE2\x9C\x85 White List ga qo\xCA\xBBshish (ID bo\xCA\xBByicha)")
				: QString::fromUtf8(
					"\xF0\x9F\x9A\xAB Black List ga qo\xCA\xBBshish "
					"(ID bo\xCA\xBByicha)")),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto peerId = peerIdInput->getLastText().trimmed();
		if (peerId.isEmpty()) {
			Ui::Toast::Show(QString::fromUtf8(
				"\xE2\x9A\xA0\xEF\xB8\x8F Peer ID bo\xCA\xBBsh. "
				"Iltimos kiriting."));
			return;
		}
		const auto already = isWhitelist
			? CustomSettings::IsInWhitelist(peerId)
			: CustomSettings::IsInBlocklist(peerId);
		if (already) {
			Ui::Toast::Show(QString::fromUtf8(
				"\xE2\x9A\xA0\xEF\xB8\x8F Bu peer allaqachon ro\xCA\xBByxatda."));
			return;
		}
		const auto name = nameInput->getLastText().trimmed();
		if (isWhitelist) {
			CustomSettings::AddToWhitelist(peerId, name);
		} else {
			CustomSettings::AddToBlocklist(peerId, name);
		}
		state->addEntry(peerId, name);
		peerIdInput->setText(QString());
		nameInput->setText(QString());
		Ui::Toast::Show(QString::fromUtf8("Qo\xCA\xBBshildi: ") + peerId);
	});

	Ui::AddSkip(content, st::settingsThumbSkip);
}
```

- [ ] **Step 5.2: Add fillPeersTab after fillPeerSection**

```cpp
void fillPeersTab(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller) {
	fillPeerSection(content, controller, true);
	Ui::AddDivider(content);
	Ui::AddSkip(content, 8);
	fillPeerSection(content, controller, false);
	Ui::AddSkip(content, st::settingsThumbSkip);
}
```

- [ ] **Step 5.3: Commit**

```
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "feat: implement fillPeerSection and fillPeersTab (WL + BL tabs)"
```

---

## Task 6: fillArchiveTab (MakeWordDiff + deleted + edited sections)

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp`

`MakeWordDiff` is moved here from `settings_main.cpp`. It goes in the anonymous namespace near the top of the file, before the tab fill functions.

- [ ] **Step 6.1: Add MakeWordDiff to the anonymous namespace (near top, before CustomTabBar)**

```cpp
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
```

- [ ] **Step 6.2: Add fillArchiveTab inside the anonymous namespace**

```cpp
void fillArchiveTab(not_null<Ui::VerticalLayout*> content) {
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
					rpl::single(QString::fromUtf8(
						"Arxivda xabarlar yo\xCA\xBBq.")),
					st::boxLabel),
				st::boxRowPadding);
			return;
		}

		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(QString("%1 ta xabar — eng yangidan.")
					.arg(messages.size())),
				st::settingsScaleLabel),
			st::defaultSubsectionTitlePadding);

		Ui::AddSkip(content, st::settingsThumbSkip);

		for (const auto &msg : messages) {
			const auto dateStr = msg.date
				? QDateTime::fromSecsSinceEpoch(msg.date)
					.toString(u"yyyy-MM-dd  hh:mm"_q)
				: u"unknown date"_q;
			const auto arrow = msg.isOut ? u"↑ You"_q : u"↓ Them"_q;
			const auto header = u"[%1]  %2  —  peer %3"_q
				.arg(dateStr, arrow, msg.peerId);

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
		QString::fromUtf8(
			"\xF0\x9F\x97\x91 O\xCA\xBB" "chirilgan xabarlar"),
		CustomDB::GetAllDeletedMessages(300));

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);

	{
		const auto records = CustomDB::GetAllEditedMessages(300);
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(QString::fromUtf8(
					"\xE2\x9C\x8F Tahrirlangan xabarlar")),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		if (records.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(QString::fromUtf8(
						"Arxivda tahrir yozuvlari yo\xCA\xBBq.")),
					st::boxLabel),
				st::boxRowPadding);
		} else {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(QString("%1 ta tahrir yozuvi — eng yangidan.")
						.arg(records.size())),
					st::settingsScaleLabel),
				st::defaultSubsectionTitlePadding);

			Ui::AddSkip(content, st::settingsThumbSkip);

			for (const auto &rec : records) {
				const auto when = rec.editedAt.isValid()
					? rec.editedAt.toString(u"yyyy-MM-dd  hh:mm"_q)
					: (rec.msgDate
						? QDateTime::fromSecsSinceEpoch(rec.msgDate)
							.toString(u"yyyy-MM-dd  hh:mm"_q)
						: u"unknown date"_q);
				const auto header =
					u"[%1]  peer %2  msg #%3"_q
						.arg(when, rec.peerId)
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
						rpl::single(u"Before: "_q + orig),
						st::boxLabel),
					st::boxRowPadding);

				if (!rec.newText.isEmpty()) {
					const auto nw =
						rec.newText.left(200).replace(u'\n', u' ');
					content->add(
						object_ptr<Ui::FlatLabel>(
							content,
							rpl::single(u"After:  "_q + nw),
							st::boxLabel),
						st::boxRowPadding);

					const auto diff = MakeWordDiff(
						rec.originalText.left(200).replace(u'\n', u' '),
						rec.newText.left(200).replace(u'\n', u' '));
					if (diff != orig) {
						content->add(
							object_ptr<Ui::FlatLabel>(
								content,
								rpl::single(u"Diff:   "_q + diff),
								st::settingsScaleLabel),
							st::boxRowPadding);
					}
				}

				Ui::AddSkip(content, 6);
			}
		}
	}

	Ui::AddSkip(content, st::settingsThumbSkip);
}
```

- [ ] **Step 6.3: Commit**

```
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "feat: implement fillArchiveTab with deleted + edited sections"
```

---

## Task 7: fillAboutTab

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp`

`dialogParent` is needed for `QFileDialog` and `QMessageBox` calls (they need a real `QWidget*` parent for proper modal behavior). We pass `this` (the `CustomModWindow`) from `setupContent`.

- [ ] **Step 7.1: Add fillAboutTab inside the anonymous namespace**

```cpp
void fillAboutTab(
		not_null<Ui::VerticalLayout*> content,
		QWidget *dialogParent) {
	{
		const auto stats = CustomDB::GetArchiveStats();
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(QString::fromUtf8(
					"\xF0\x9F\x97\x84 Arxiv holati: %1 o\xCA\xBB"
					"chirilgan \xE2\x80\xA2 %2 tahrirlangan"
				).arg(stats.deletedCount).arg(stats.editedCount)),
				st::settingsScaleLabel),
			st::defaultSubsectionTitlePadding);
	}

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString::fromUtf8(
				"\xF0\x9F\x93\xA6 Zaxira nusxa")),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString::fromUtf8(
				"Eksport: baza + barcha medialarni papkaga saqlaydi.\n"
				"Import: avval eksport qilingan papkadan tiklaydi.")),
			st::settingsScaleLabel),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(QString::fromUtf8(
				"\xF0\x9F\x93\xA4 To\xCA\xBBliq zaxira nusxa olish")),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto dir = QFileDialog::getExistingDirectory(
			dialogParent,
			QString::fromUtf8("Saqlash papkasini tanlang"),
			QDir::homePath());
		if (dir.isEmpty()) return;
		const auto result = CustomDB::ExportFullBackup(dir);
		if (result.isEmpty()) {
			Ui::Toast::Show(QString::fromUtf8("Eksport amalga oshmadi."));
		} else {
			Ui::Toast::Show(
				QString::fromUtf8("Eksport saqlandi: ") + result);
		}
	});

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(QString::fromUtf8(
				"\xF0\x9F\x93\xA5 Zaxira nusxadan tiklash")),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto path = QFileDialog::getOpenFileName(
			dialogParent,
			QString::fromUtf8("Zaxira faylini tanlang (.zip)"),
			QDir::homePath(),
			QString::fromUtf8(
				"Zaxira fayllari (*.zip);;Barcha fayllar (*)"));
		const auto source = path.isEmpty()
			? QFileDialog::getExistingDirectory(
				dialogParent,
				QString::fromUtf8("Zaxira papkasini tanlang"),
				QDir::homePath())
			: path;
		if (source.isEmpty()) return;

		const auto reply = QMessageBox::warning(
			dialogParent,
			QString::fromUtf8("Zaxiradan tiklash"),
			QString::fromUtf8(
				"Bu amal JORIY arxiv ma\xCA\xBBlumotlarini o\xCA\xBB" "CHIRADI.\n"
				"Barcha o\xCA\xBB" "chirilgan/tahrirlangan xabarlar almashtiriladi.\n\n"
				"Davom etasizmi?"),
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (reply != QMessageBox::Yes) return;

		const auto ok = CustomDB::ImportFullBackup(source);
		if (ok) {
			const auto stats = CustomDB::GetArchiveStats();
			Ui::Toast::Show(QString::fromUtf8(
				"Tiklash muvaffaqiyatli! %1 o\xCA\xBB"
				"chirilgan, %2 tahrirlangan. Arxiv yangilandi."
			).arg(stats.deletedCount).arg(stats.editedCount));
		} else {
			Ui::Toast::Show(QString::fromUtf8(
				"Tiklash amalga oshmadi. Fayl/papkani tekshiring."));
		}
	});

	Ui::AddSkip(content, st::settingsThumbSkip);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString::fromUtf8(
				"\xF0\x9F\x93\x8A Arxiv boshqaruvi")),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(QString::fromUtf8(
				"\xF0\x9F\x92\xA3 Vaqtinchalik media papkasi")),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto bombDir =
			QStandardPaths::writableLocation(
				QStandardPaths::AppDataLocation)
			+ u"/CustomMod/BombMedia/"_q;
		QDir().mkpath(bombDir);
		QDesktopServices::openUrl(QUrl::fromLocalFile(bombDir));
	});

	Ui::AddSkip(content, st::settingsThumbSkip);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString::fromUtf8(
				"\xE2\x9A\xA0\xEF\xB8\x8F  XAVFLI HUDUD  "
				"\xE2\x9A\xA0\xEF\xB8\x8F")),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(QString::fromUtf8(
				"Quyidagi amallar arxiv ma\xCA\xBBlumotlarini BUTUNLAY "
				"o\xCA\xBB" "chiradi.\n"
				"Bu amalni bekor qilib bo\xCA\xBBlmaydi. "
				"Tasdiqlash talab qilinadi.")),
			st::settingsScaleLabel),
		st::defaultSubsectionTitlePadding);

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(QString::fromUtf8(
				"\xF0\x9F\x97\x91  O\xCA\xBB" "chirilganlar arxivini tozalash")),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto reply = QMessageBox::warning(
			dialogParent,
			QString::fromUtf8("O\xCA\xBB" "chirilganlar arxivini tozalash"),
			QString::fromUtf8(
				"Bu amal BARCHA saqlangan o\xCA\xBB"
				"chirilgan xabarlarni o\xCA\xBB" "chiradi.\n\n"
				"Bu amalni bekor qilib bo\xCA\xBBlmaydi.\n\n"
				"Davom etasizmi?"),
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (reply != QMessageBox::Yes) return;
		CustomDB::ClearDeletedArchive();
		Ui::Toast::Show(QString::fromUtf8(
			"\xF0\x9F\x97\x91 O\xCA\xBB" "chirilganlar arxivi tozalandi."));
	});

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(QString::fromUtf8(
				"\xE2\x9C\x8F\xEF\xB8\x8F  Tahrir tarixi arxivini tozalash")),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto reply = QMessageBox::warning(
			dialogParent,
			QString::fromUtf8("Tahrir tarixini tozalash"),
			QString::fromUtf8(
				"Bu amal BARCHA saqlangan tahrir yozuvlarini "
				"o\xCA\xBB" "chiradi.\n\n"
				"Bu amalni bekor qilib bo\xCA\xBBlmaydi.\n\n"
				"Davom etasizmi?"),
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (reply != QMessageBox::Yes) return;
		CustomDB::ClearEditedArchive();
		Ui::Toast::Show(QString::fromUtf8(
			"\xE2\x9C\x8F\xEF\xB8\x8F Tahrir tarixi arxivi tozalandi."));
	});

	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(QString::fromUtf8(
				"\xE2\x98\xA0\xEF\xB8\x8F  BARCHA arxivni tozalash "
				"(O\xCA\xBB" "chirilgan + Tahrir)")),
			st::attentionBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		const auto first = QMessageBox::warning(
			dialogParent,
			QString::fromUtf8("BARCHA arxivni tozalash"),
			QString::fromUtf8(
				"Bu amal BARCHA arxiv ma\xCA\xBBlumotlarini o\xCA\xBB" "chiradi:\n"
				"  \xE2\x80\xA2 Barcha o\xCA\xBB" "chirilgan xabar yozuvlari\n"
				"  \xE2\x80\xA2 Barcha tahrir tarixi yozuvlari\n\n"
				"Bu amalni bekor qilib bo\xCA\xBBlmaydi."),
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (first != QMessageBox::Yes) return;
		const auto second = QMessageBox::critical(
			dialogParent,
			QString::fromUtf8("Yakuniy tasdiqlash"),
			QString::fromUtf8(
				"HAQIQATAN HAM ishonchingiz komilmi?\n\n"
				"Barcha o\xCA\xBB" "chirilgan xabar va tahrir yozuvlari\n"
				"BUTUNLAY yo\xCA\xBBqoladi."),
			QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel);
		if (second != QMessageBox::Yes) return;
		CustomDB::ClearAllArchive();
		Ui::Toast::Show(QString::fromUtf8(
			"\xE2\x98\xA0\xEF\xB8\x8F Barcha arxiv ma\xCA\xBBlumotlari tozalandi."));
	});

	Ui::AddSkip(content, st::settingsThumbSkip);
}
```

- [ ] **Step 7.2: Commit**

```
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "feat: implement fillAboutTab (backup, archive management)"
```

---

## Task 8: Wire settings_main.cpp — replace, delete, clean includes

**Files:**
- Modify: `Telegram/SourceFiles/settings/sections/settings_main.cpp`

- [ ] **Step 8.1: Add the new include and replace the onClick**

Find and replace:

```cpp
#include "custom_settings.h"
#include "ui/toast/toast.h"
#include "window/window_peer_menu.h"
#include "data/data_thread.h"
```

Replace with:

```cpp
#include "custom_mod_window.h"
```

Then find:

```cpp
		.onClick = [=] {
			controller->show(Box(CustomModBox, controller));
		},
```

Replace with:

```cpp
		.onClick = [=] {
			CustomMod::OpenOrRaise(controller);
		},
```

- [ ] **Step 8.2: Delete MakeWordDiff (lines 111–136)**

Delete the block from the comment line through the closing `}`:

```cpp
// D17: Simple word-level diff: marks removed words as [-word] and added as [+word].
// Uses common-prefix / common-suffix algorithm — O(n+m), good enough for message text.
[[nodiscard]] QString MakeWordDiff(const QString &before, const QString &after) {
	...
}
```

Delete the entire function and its preceding comment block (lines 111–136).

- [ ] **Step 8.3: Delete PeerListBox (lines 138–487)**

Delete the block from the comment lines (`// Unified peer list box...`) through the closing `}` of `PeerListBox`, inclusive.

- [ ] **Step 8.4: Delete DeletedArchiveBox (lines 489–562)**

Delete from `// C10: Cross-chat deleted messages archive viewer.` through the closing `}` of `DeletedArchiveBox`.

- [ ] **Step 8.5: Delete EditArchiveBox (lines 564–649)**

Delete from `// C11: Per-message edit history viewer...` through the closing `}` of `EditArchiveBox`.

- [ ] **Step 8.6: Delete CustomModBox (lines 651–963)**

Delete from `void CustomModBox(` through the closing `}`.

- [ ] **Step 8.7: Remove the custom_db.h include and verify other includes**

`custom_db.h` was only used by the deleted CustomMod box functions. Remove this line:

```cpp
#include "custom_db.h"
```

Also check `#include "ui/widgets/fields/input_field.h"` — if no remaining code in `settings_main.cpp` uses `Ui::InputField`, remove it too. Search the file for `InputField` after deletion; if none found, remove the include.

- [ ] **Step 8.8: Verify the file compiles cleanly**

Inspect the file: confirm no remaining references to `CustomModBox`, `PeerListBox`, `DeletedArchiveBox`, `EditArchiveBox`, `MakeWordDiff`. Confirm `CustomMod::OpenOrRaise` is the only call site.

- [ ] **Step 8.9: Commit**

```
git add Telegram/SourceFiles/settings/sections/settings_main.cpp
git commit -m "refactor: replace CustomModBox GenericBox with CustomMod::OpenOrRaise standalone window"
```

---

## Verification Checklist

After all tasks are committed, verify these properties without building:

- [ ] `custom_mod_window.h` declares only `CustomMod::OpenOrRaise`; no implementation
- [ ] `custom_mod_window.cpp` includes `"styles/style_custom_mod.h"` (generated from the `.style` file)
- [ ] `custom_mod_window.cpp` has exactly one `QPointer<CustomModWindow> gInstance` — in its own anonymous namespace block **outside** the main `namespace { }` block that holds the classes
- [ ] `settings_main.cpp` has no references to `CustomModBox`, `PeerListBox`, `DeletedArchiveBox`, `EditArchiveBox`, `MakeWordDiff`
- [ ] `CMakeLists.txt` lists `custom_mod_window.cpp`, `custom_mod_window.h`, `styles/style_custom_mod.style`
- [ ] No hardcoded pixel integers in `custom_mod_window.cpp` (all spacing via `st::` or existing margin literals `{top, right, bottom, left}` inherited from source code)
