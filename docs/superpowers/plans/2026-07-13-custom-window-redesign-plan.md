# Custom Window Redesign (General + Peers tabs) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize the General and Peers tabs of the CustomMod settings window (`custom_mod_window.cpp`) to be friendlier for a non-IT user: Device Spoof form collapses when its toggle is off, related privacy/mod toggles are merged into one section, and White List / Black List entries can no longer silently conflict with each other.

**Architecture:** All changes live in `Telegram/SourceFiles/custom_mod_window.cpp` (UI) and `Telegram/SourceFiles/custom_settings.cpp`/`.h` (data layer). Category-level mutual exclusion is enforced directly inside `SetWhitelistCategory`/`SetBlocklistCategory` (mirroring the existing individual-peer exclusion already present in `AddToWhitelist`/`AddToBlocklist`). Individual-peer mutual exclusion already exists at the data layer — this plan only adds the UI-facing toast + live refresh. Cross-list UI refresh is implemented by **rebuilding the whole Peers tab panel**, the same pattern already used for the Archive tab's "🔄 Yangilash" button (`setupContent`, `_panels[2]`/`rebuildArchive`) — no new reactive (`rpl::variable`) wiring is introduced, keeping the change small and consistent with existing code.

**Tech Stack:** C++17/Qt, tdesktop's `Ui::` widget library (`Ui::VerticalLayout`, `Ui::SlideWrap`, `Ui::SettingsButton`, `rpl::producer`), no automated test framework for this UI — verification is manual build + manual run in the app.

---

## Before you start

This is a native Qt/C++ desktop app built with Visual Studio (no CLI test suite, no headless build command available to the agent). Every "Verify" step in this plan means: **ask the human partner to build the solution in Visual Studio and manually exercise the described UI**, then report back either "works as described" or the exact build error / behavior mismatch. Do not claim a step is done without that confirmation.

All edits are in two files:
- `Telegram/SourceFiles/custom_mod_window.cpp` (edited in Tasks 1, 2, 4, 5, 6)
- `Telegram/SourceFiles/custom_settings.cpp` (edited in Task 3)

---

### Task 1: Make `addToggle` return the created button

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:420-472` (the `addToggle` lambda inside `fillGeneralTab`)

**Why:** Task 2 needs a handle to the `spoofMobile` toggle's `Ui::SettingsButton` so it can drive the Device Spoof form's collapse/expand animation. Currently `addToggle` returns `void`.

**Status:** ✅ Implemented, spec-reviewed, and code-quality-reviewed via subagent-driven-development. Commits `bd802e1184`, `5f4f8461ae` (blank-line fixup).

- [ ] **Step 1: Add a `return btn;` at the end of the `addToggle` lambda**

Find this exact block (lines 420-472 today):

```cpp
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
	};
```

Replace only the final two lines (the closing of the `if (!description.isEmpty())` block and the lambda's closing `};`) so the lambda ends with:

```cpp
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
```

(Everything above `return btn;` is unchanged — only that one line is new.)

- [ ] **Step 2: Verify it still builds**

The lambda's return type is deduced automatically from the single `return btn;` statement (`Ui::SettingsButton*`). No other code calls `addToggle` and captures its return value yet, so this step alone must compile cleanly with no behavior change. Ask the human partner to build in Visual Studio and confirm no new errors/warnings from `custom_mod_window.cpp`.

- [ ] **Step 3: Commit**

```bash
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "custom_mod: addToggle endi Ui::SettingsButton* qaytaradi"
```

---

### Task 2: Reorder General tab — Device Spoof to top with collapse, merge Privacy sections

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:474-594` (inside `fillGeneralTab`, between the `addToggle` lambda from Task 1 and the unchanged Branding section that already starts at line 596)

**Why:** Per the approved spec (`docs/superpowers/specs/2026-07-13-custom-window-redesign-design.md`, section 2), Device Spoof becomes the first block and collapses when off; the three separate toggle groups ("Privacy & Ghost Mode" minus Device Spoof, "Cheklovlar", "Xabarlar tarixi") merge into one "🛡️ Privacy & Custom Mods" block. The Branding section (current lines 596-715) is untouched — it already sits last, so no code needs to move for it.

**Status:** ✅ Implemented, spec-reviewed, and code-quality-reviewed via subagent-driven-development. Commit `862bc94eec`.

- [ ] **Step 1: Replace lines 474-594 with the new block order**

Find the block starting at `Ui::AddSkip(content, st::settingsThumbSkip);` (line 474) and ending right before `// ── Branding sektsiyasi ──` (line 596). Today it reads:

```cpp
	Ui::AddSkip(content, st::settingsThumbSkip);
	addSection(u"Privacy & Ghost Mode"_q);
	addToggle(
		u"ghostMode"_q,
		u"Ghost Mode"_q,
		u"Online holatini, yozmoqda belgisini va xabar oʻqildi bildirishnomasini yashiradi."_q);
	addToggle(
		u"storyAnonymousView"_q,
		u"Hikoyalarni anonim koʻrish"_q,
		u"Hikoyani koʻrganingiz haqida egasiga bildirish yuborilmaydi."_q);
	addToggle(
		u"spoofMobile"_q,
		u"Mobil qurilma koʻrinishi"_q,
		u"Telegram mobil ilovadan ishlatilayotgandek koʻrinadi."_q);

	Ui::AddSkip(content, 8);

	// C22: Dynamic device spoof — qurilma nomi va versiya
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Qurilma nomi:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto spoofModelInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Qurilma nomi"_q),
			CustomSettings::SpoofDeviceModel()),
		st::boxRowPadding);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Tizim versiyasi:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto spoofVersionInput = content->add(
		object_ptr<Ui::InputField>(
			content,
			st::defaultInputField,
			rpl::single(u"Tizim versiyasi"_q),
			CustomSettings::SpoofSystemVersion()),
		st::boxRowPadding);

	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
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
			content->add(
				object_ptr<Ui::RoundButton>(
					content,
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

	Ui::AddSkip(content, 8);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
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

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	addSection(u"Cheklovlar"_q);
	addToggle(
		u"bypassRestrictions"_q,
		u"Cheklangan chatda nusxalash va yuborish"_q,
		u"Cheklov qoʻyilgan chatlardagi xabarlarni boshqaga yuborish yoki nusxa olishga ruxsat beradi."_q);

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	addSection(u"Xabarlar tarixi"_q);
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
```

Replace the entire block above with:

```cpp
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
```

The very next line in the file after this block is already `// ── Branding sektsiyasi ──` (previously line 596) — leave that and everything after it (through line 715) completely unchanged.

- [ ] **Step 2: Verify build**

Ask the human partner to build in Visual Studio. Expected: clean build, no new errors. `Ui::SlideWrap` and `anim::type` are already used elsewhere in this same file (`fillPeerSection`), and `slide_wrap.h` is already `#include`d at the top of `custom_mod_window.cpp` — no new includes are needed.

- [ ] **Step 3: Manually verify in the running app**

Ask the human partner to:
1. Open Custom Window → General tab.
2. Confirm the order top-to-bottom is: "📱 Qurilma ko'rinishini almashtirish" (with the device-name/version/preset/save form) → "🛡️ Privacy & Custom Mods" (6 toggles: Ghost Mode, Hikoyalarni anonim koʻrish, Cheklangan chatda nusxalash va yuborish, Anti-Delete, Anti-Edit, Offline xabar bazasi) → "🎨 Branding".
3. Toggle "Mobil qurilma koʻrinishi" OFF — the device-name/version/preset/save form should animate closed.
4. Toggle it back ON — the form should animate back open, and the previously-entered device name/version should still be there (values aren't cleared by collapsing).
5. Confirm the Ghost Mode toggle's description now shows the extra sentence about the 1-2 minute delay.

Report back pass/fail with specifics if something looks wrong.

- [ ] **Step 4: Commit**

```bash
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "custom_mod: General tab qayta tashkil qilindi (Device Spoof yuqorida, Privacy bo'limlari birlashtirildi)"
```

---

### Task 3: Category-level mutual exclusion between White List and Black List

**Files:**
- Modify: `Telegram/SourceFiles/custom_settings.cpp:397-413` (`SetWhitelistCategory` / `SetBlocklistCategory`)

**Why:** Per spec section 3.1 item 1 — turning a category (Users/Groups/Channels) ON in one list must automatically turn the same category OFF in the other list. This mirrors the mutual exclusion already implemented for individual peers in `AddToWhitelist`/`AddToBlocklist` (lines 295-300, 331-336 of the same file), which directly manipulate the sibling `QMap` rather than calling the sibling's public setter — this plan follows that exact existing style.

**Status:** ✅ Implemented, spec-reviewed, and code-quality-reviewed via subagent-driven-development. Commit `3a79c68b47`.

- [ ] **Step 1: Add mutual exclusion to `SetWhitelistCategory` and `SetBlocklistCategory`**

Find (lines 397-413 today):

```cpp
void SetWhitelistCategory(PeerType type, bool enabled) {
    if (!gInitialized) Init();
    gWhitelistCategories[static_cast<int>(type)] = enabled;
    SavePeerLists();
}

bool IsBlocklistCategoryEnabled(PeerType type) {
    if (!gInitialized) Init();
    const auto it = gBlocklistCategories.constFind(static_cast<int>(type));
    return it != gBlocklistCategories.constEnd() && it.value();
}

void SetBlocklistCategory(PeerType type, bool enabled) {
    if (!gInitialized) Init();
    gBlocklistCategories[static_cast<int>(type)] = enabled;
    SavePeerLists();
}
```

Replace with:

```cpp
void SetWhitelistCategory(PeerType type, bool enabled) {
    if (!gInitialized) Init();
    gWhitelistCategories[static_cast<int>(type)] = enabled;
    if (enabled) {
        gBlocklistCategories[static_cast<int>(type)] = false; // mutual exclusion
    }
    SavePeerLists();
}

bool IsBlocklistCategoryEnabled(PeerType type) {
    if (!gInitialized) Init();
    const auto it = gBlocklistCategories.constFind(static_cast<int>(type));
    return it != gBlocklistCategories.constEnd() && it.value();
}

void SetBlocklistCategory(PeerType type, bool enabled) {
    if (!gInitialized) Init();
    gBlocklistCategories[static_cast<int>(type)] = enabled;
    if (enabled) {
        gWhitelistCategories[static_cast<int>(type)] = false; // mutual exclusion
    }
    SavePeerLists();
}
```

- [ ] **Step 2: Verify build**

Ask the human partner to build in Visual Studio. Expected: clean build — this only adds two `if` blocks to existing functions, no signature changes.

- [ ] **Step 3: Commit**

```bash
git add Telegram/SourceFiles/custom_settings.cpp
git commit -m "custom_settings: kategoriya darajasida White/Black List mutual exclusion"
```

---

### Task 4: Make the Peers tab rebuildable, wire category-toggle conflicts to trigger it

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:219-222` (forward declaration of `fillPeersTab`)
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:303-333` (`setupContent`)
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:717-720` (`fillPeerSection` signature)
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:762-793` (category toggles inside `fillPeerSection`)
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:1384-1443` (`fillPeersTab`)

**Why:** Per spec section 3.1 item 1, when a category conflict happens the *other* list's toggle must visually flip off. The simplest, lowest-risk way to refresh that toggle's visual state — without introducing new `rpl::variable` cross-wiring — is to rebuild the whole Peers tab panel, exactly like the Archive tab's existing `rebuildArchive` mechanism (`setupContent`, lines 318-330 today) already does for its own refresh button.

**Status:** ✅ Implemented, spec-reviewed, and code-quality-reviewed via subagent-driven-development (including a verified analysis confirming no use-after-free risk from rebuilding the panel mid-callback — `_layerManager` and `_panels[1]` are sibling members of `CustomModWindow`, not parent/child). Commit `ca7e4fd276`.

- [ ] **Step 1: Update the forward declarations**

Find (lines 219-222):

```cpp
void fillGeneralTab(not_null<Ui::VerticalLayout*> content);
void fillPeersTab(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller);
```

Replace with:

```cpp
void fillGeneralTab(not_null<Ui::VerticalLayout*> content);
void fillPeersTab(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	Fn<void()> onRebuild);
```

- [ ] **Step 2: Give `fillPeerSection` an `onRebuild` parameter**

Find (lines 717-720):

```cpp
void fillPeerSection(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		bool isWhitelist) {
```

Replace with:

```cpp
void fillPeerSection(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		bool isWhitelist,
		Fn<void()> onRebuild) {
```

- [ ] **Step 3: Trigger rebuild from the category toggles on conflict**

Find (lines 762-793):

```cpp
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
				if (isWhitelist) {
					CustomSettings::SetWhitelistCategory(type, on);
				} else {
					CustomSettings::SetBlocklistCategory(type, on);
				}
			}, btn->lifetime());
		};
		addCategoryToggle(
			u"Barcha shaxsiy chatlar (Users)"_q,
			CustomSettings::PeerType::User);
		addCategoryToggle(
			u"Barcha guruhlar (Groups)"_q,
			CustomSettings::PeerType::Group);
		addCategoryToggle(
			u"Barcha kanallar / superguruhlar (Channels)"_q,
			CustomSettings::PeerType::Channel);
	}
```

Replace with:

```cpp
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
```

(This also applies the spec's "engil soddalashtiriladi" naming simplification from section 3.1: "Barcha shaxsiy chatlar (Users)" → "Shaxsiy chatlar", etc.)

- [ ] **Step 4: Update `fillPeersTab` to accept and forward `onRebuild`**

Find (lines 1384-1443, the whole function):

```cpp
void fillPeersTab(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller) {
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

	fillPeerSection(content, controller, true);

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);

	fillPeerSection(content, controller, false);

	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	fillPerChatSection(content, controller);
	Ui::AddSkip(content, st::settingsThumbSkip);
}
```

Replace only the signature and the two `fillPeerSection` calls, keeping everything else identical:

```cpp
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
}
```

- [ ] **Step 5: Make the Peers tab panel rebuildable in `setupContent`**

Find (lines 303-333, the whole function):

```cpp
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
	fillPeersTab(makeInner(1), controller);

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
```

Replace with:

```cpp
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
```

- [ ] **Step 6: Verify build**

Ask the human partner to build in Visual Studio. Expected: clean build. `onRebuild` is already used within this same task by the category-toggle handler (Step 3 above); the two individual-peer add handlers ("Chat tanlash" and "ID orqali qo'shish") don't reference it yet — that's fine, they're updated in Task 5.

- [ ] **Step 7: Manually verify in the running app**

Ask the human partner to:
1. Open Custom Window → Peers tab.
2. In White List, turn ON the "Guruhlar" category toggle.
3. In Black List, turn ON the "Guruhlar" category toggle too.
4. Confirm: a toast appears ("Qarama-qarshi roʻyxatdagi mos kategoriya avtomatik oʻchirildi."), the White List's "Guruhlar" toggle visually flips OFF, and the Black List's "Guruhlar" toggle stays ON.
5. Repeat in the other direction (Black List ON first, then White List ON) and confirm the same auto-off behavior applies to Black List this time.

Report back pass/fail with specifics if something looks wrong.

- [ ] **Step 8: Commit**

```bash
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "custom_mod: Peers tab endi to'liq qayta quriladi, kategoriya konflikti UI'da aks etadi"
```

---

### Task 5: Individual-peer conflict toast + live refresh

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:805-834` ("Chat tanlash" click handler inside `fillPeerSection`)
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:861-892` ("ID orqali qo'shish" click handler inside `fillPeerSection`)

**Why:** Per spec section 3.1 item 2 — adding a peer to one list that's already in the other list must show `"«Ism» BlackList'dan olib tashlandi va WhiteList'ga qo'shildi."` (or the mirror message) and the other list's on-screen entry must disappear. The data-layer removal already happens automatically today (`AddToWhitelist`/`AddToBlocklist` in `custom_settings.cpp` already call `gBlocklist.remove(peerId)` / `gWhitelist.remove(peerId)`) — this task only adds the UI-facing detection, toast, and rebuild call.

**Status:** ✅ Implemented, spec-reviewed, and code-quality-reviewed via subagent-driven-development. Commits `b565ce0fcb`, `ba8a17b7f0` (clarifying comment added per review feedback).

- [ ] **Step 1: Update the "Chat tanlash" handler**

Find (lines 805-834):

```cpp
	->addClickHandler([=] {
		if (!gInstance) return;
		// Box custom window ichida ochiladi (LayerManager orqali).
		gInstance->showBox(Window::PrepareChooseRecipientBox(
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
				if (isWhitelist) {
					CustomSettings::AddToWhitelist(peerId, name);
				} else {
					CustomSettings::AddToBlocklist(peerId, name);
				}
				state->addEntry(peerId, name);
				Ui::Toast::Show(name + u" qoʻshildi."_q);
				// raise()/activateWindow() shart emas — dialog window ichida ochiladi.
				return true;
			},
			rpl::single(isWhitelist
				? u"White List ga qoʻshish"_q
				: u"Black List ga qoʻshish"_q)));
	});
```

Replace with:

```cpp
	->addClickHandler([=] {
		if (!gInstance) return;
		// Box custom window ichida ochiladi (LayerManager orqali).
		gInstance->showBox(Window::PrepareChooseRecipientBox(
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
```

- [ ] **Step 2: Update the "ID orqali qo'shish" handler**

Find (lines 861-892):

```cpp
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
		const auto name = nameInput->getLastText().trimmed();
		if (isWhitelist) {
			CustomSettings::AddToWhitelist(peerId, name);
		} else {
			CustomSettings::AddToBlocklist(peerId, name);
		}
		state->addEntry(peerId, name);
		peerIdInput->setText(QString());
		nameInput->setText(QString());
		Ui::Toast::Show(u"Qoʻshildi: "_q + peerId);
	});
```

Replace with:

```cpp
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
```

- [ ] **Step 3: Verify build**

Ask the human partner to build in Visual Studio. Expected: clean build — `onRebuild` (added as a `fillPeerSection` parameter in Task 4 Step 2) is now also used by these two handlers.

- [ ] **Step 4: Manually verify in the running app**

Ask the human partner to:
1. Open Custom Window → Peers tab.
2. Add a specific chat to Black List (via "Chat tanlash — Black List" or ID).
3. Add that same chat to White List (via "Chat tanlash — White List" or the same ID).
4. Confirm: the cross-removal toast appears, the chat disappears from the Black List's on-screen entries, and appears in the White List's on-screen entries.
5. Repeat in the reverse direction (White List first, then White → Black) and confirm the mirrored behavior.
6. As a regression check, add a brand new chat that is in neither list — confirm it's added normally with the plain "qoʻshildi" toast and no unnecessary tab rebuild (list stays where it was, no flicker).

Report back pass/fail with specifics if something looks wrong.

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "custom_mod: individual chat qo'shishda White/Black List konflikti toast va UI yangilanishi"
```

---

### Task 6: Rename and re-describe the Per-Chat section

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:1116-1142` (header + description inside `fillPerChatSection`)

**Why:** Per spec section 3.2 — rename from "Per-Chat Sozlamalar" to "⚙️ Individual sozlamalar (istisnolar)" with an updated, friendlier description that also clarifies priority order relative to White/Black List.

**Status:** ✅ Implemented, spec-reviewed, and code-quality-reviewed via subagent-driven-development. Commit `f95eb9530c`.

- [ ] **Step 1: Replace the header and description**

Find (lines 1116-1142):

```cpp
	// ── Header ────────────────────────────────────────────────────
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"Per-Chat Sozlamalar"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	{
		const auto desc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Bu yerdagi chatlar uchun Ghost, Anti-Delete va Anti-Edit "
					"alohida sozlanadi. White/Black list dan past prioritet."_q),
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
```

Replace with:

```cpp
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
```

- [ ] **Step 2: Verify build**

Ask the human partner to build in Visual Studio. Expected: clean build — text-only change.

- [ ] **Step 3: Manually verify in the running app**

Ask the human partner to open Custom Window → Peers tab, scroll to the bottom section, and confirm the header now reads "⚙️ Individual sozlamalar (istisnolar)" with the new description text, and that adding/removing entries there still works as before (this section's functionality is otherwise untouched).

- [ ] **Step 4: Commit**

```bash
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "custom_mod: Per-Chat bo'limi 'Individual sozlamalar (istisnolar)' deb qayta nomlandi"
```

---

### Task 7: Full manual regression pass

**Files:** none (verification only)

**Why:** Tasks 1-6 touch shared state (`fillPeerSection`'s `state->addEntry`, the Peers tab rebuild lambda, `addToggle`'s new return value) — a full pass through both tabs catches integration issues that per-task verification might miss.

**Status:** ⏳ Pending — all 6 code tasks above are implemented, spec-reviewed, and code-quality-reviewed, but this manual build+run verification pass has not happened yet. The human partner's local machine hit two unrelated environment issues while attempting to build: (1) the disk ran out of space during a Debug build (`out/Telegram/Telegram.dir/Debug`, ~17.6 GB of intermediate `.obj` files, was cleared — freed 17.59 GB), and (2) a subsequent Release build failed with `LNK1102` (linker out of memory — machine has 15.4 GB total RAM, ~6 GB free; `PreferredToolArchitecture=x64` is already correctly set in the generated `.vcxproj` files, so this is genuine RAM pressure, not a 32-bit-toolset misconfiguration). Recommended before retrying: close other memory-heavy applications and reduce Visual Studio's "maximum number of parallel project builds" to 1 (Tools → Options → Projects and Solutions → Build and Run) to lower peak memory during compilation/linking. Once a build succeeds, resume this task's Step 1 manual walkthrough.

- [ ] **Step 1: Full manual walkthrough**

Ask the human partner to do a final build, then in the running app:
1. General tab: toggle every one of the 6 merged Privacy & Custom Mods toggles on/off once, confirm each shows its own toast and its own description text is intact.
2. General tab: change the device spoof name/version by hand (not via preset), click "💾 Saqlash", confirm the toast appears and re-opening Custom Window later still shows the saved values.
3. General tab: Branding section — unchanged behavior, quick smoke test (edit window title field, confirm no crash, don't need to click Saqlash unless the human partner wants to actually rename the window).
4. Peers tab: add 2-3 chats to White List and 2-3 different chats to Black List via both "Chat tanlash" and "ID orqali qo'shish", confirm all appear with correct avatars/names.
5. Peers tab: delete an entry from each list via "Oʻchirish", confirm it disappears.
6. Peers tab: "Barchasini tozalash" on each list, confirm both empty out and show the empty-state label.
7. Peers tab: re-run the category-conflict scenario from Task 4 Step 7 and the individual-conflict scenario from Task 5 Step 4 once more end-to-end.
8. Peers tab: Individual sozlamalar (istisnolar) section — add a chat, toggle its Ghost/AntiDelete/AntiEdit switches, remove it — confirm unaffected by this plan's changes.

Report back a pass/fail summary. If anything fails, treat it as a bug against the specific task above that owns that code path, fix it there, and re-verify.

- [ ] **Step 2: No commit needed for this task** (verification only — any fixes found go into a follow-up commit referencing the task they belong to).

---

## Out of scope (explicitly deferred by the user)

- Archive tab redesign
- About tab redesign
- Visual polish (colors, fonts, spacing) beyond what's specified above

Do not touch these without explicit user request.
