# CustomMod Standalone Window — Design Spec
**Date:** 2026-05-24  
**Status:** Approved  

---

## Overview

Move "Customizations (By Oybek)" from a `GenericBox` overlay inside the Settings panel to a fully independent, non-modal OS-level window. The window lives alongside Telegram's main window — users can interact with chats while the customization panel is open.

---

## Architecture

### New files
| File | Purpose |
|------|---------|
| `Telegram/SourceFiles/custom_mod_window.h` | `CustomModWindow` class declaration + `OpenOrRaise()` helper |
| `Telegram/SourceFiles/custom_mod_window.cpp` | Implementation: window setup, tab bar, panel fill functions |

### Modified files
| File | Change |
|------|--------|
| `settings/sections/settings_main.cpp` | Replace `controller->show(Box(CustomModBox, controller))` with `CustomModWindow::OpenOrRaise(controller)`. Remove `CustomModBox` and all its helper lambdas/functions. |

### Class hierarchy

```
Ui::RpWidget
  └── CustomModWindow
        ├── CustomTabBar  (inner widget)
        └── 4× QScrollArea  (one per tab, show/hide on tab switch)
```

`CustomModWindow` is **not** a `BoxContent`. It is a real top-level OS window (`Qt::Window`).

---

## Window Properties

| Property | Value |
|----------|-------|
| Window flags | `Qt::Window` |
| Modal | No (non-modal) |
| Default size | 560 × 700 px |
| Minimum size | 480 × 400 px |
| Resizable | Yes |
| Title bar text | `"Customizations (By Oybek)"` |
| Delete-on-close | `Qt::WA_DeleteOnClose` set — no manual lifetime management |
| Positioning | Centered relative to main window on first open |
| Geometry persistence | Saved/restored via `QSettings("CustomMod", "WindowGeometry")` |

---

## Singleton Pattern

```cpp
// custom_mod_window.h
class CustomModWindow : public Ui::RpWidget {
public:
    static void OpenOrRaise(not_null<Window::SessionController*> controller);
    ...
private:
    static QPointer<CustomModWindow> _instance;
};

// Usage (settings_main.cpp):
CustomModWindow::OpenOrRaise(controller);
```

`OpenOrRaise`:
1. If `_instance` is non-null → call `raise()` then `activateWindow()`, return.
2. Otherwise → `new CustomModWindow(controller)`, set `WA_DeleteOnClose`, call `show()`.

This guarantees exactly one instance at a time without manual lifetime bookkeeping.

---

## Tab Bar

A lightweight horizontal tab row rendered by `CustomTabBar : public Ui::RpWidget`:
- One button per tab (text label).
- Active tab: underline indicator drawn with `Painter` using `st::activeButtonBg` color.
- Button height: `st::defaultBoxButton.height` (scale-safe, never hardcoded).
- Tab press emits `tabSelected(int index)` signal → parent shows/hides panels.

No animation required for tab switching (instant show/hide is sufficient and matches tdesktop's own tab patterns in stats windows).

---

## Tabs & Content

### Tab 1 — General

Five toggle rows, each built by a `fillGeneralTab(not_null<Ui::VerticalLayout*>, not_null<Window::SessionController*>)` free function:

| Toggle | Setting ID | Default |
|--------|-----------|---------|
| Ghost Mode | `"ghostMode"` | ON |
| Anti-Delete | `"antiDelete"` | ON |
| Anti-Edit | `"antiEdit"` | ON |
| Bypass Restrictions | `"bypassRestrictions"` | ON |
| Spoof Mobile | `"spoofMobile"` | ON |

Each row: `Ui::SettingsButton` with subtitle description (same pattern as current `addToggle`).

### Tab 2 — Peers

Content from `fillPeersTab(not_null<Ui::VerticalLayout*>, not_null<Window::SessionController*>)`:
- "Add to Whitelist" section header + input area
- Whitelist entry list (name + move/remove buttons)
- "Add to Blocklist" section header + input area  
- Blocklist entry list (name + move/remove buttons)
- Entry toggle animation: `Ui::SlideWrap` (preserved from current implementation)
- Duplicate detection: `CustomSettings::IsInWhitelist` / `IsInBlocklist` before adding
- Double-click guard: both buttons disabled on first click (preserved fix)

### Tab 3 — Archive

Content from `fillArchiveTab(not_null<Ui::VerticalLayout*>, not_null<Window::SessionController*>)`:
- **Deleted messages** section: list from `CustomDB::GetAllDeletedMessages(300)`
- **Edited messages** section: list from `CustomDB::GetAllEditedMessages(300)`
- D17 word-diff markers preserved for edited message display
- "Clear deleted archive" / "Clear edited archive" / "Clear all" buttons
- Stats header showing counts from `CustomDB::GetArchiveStats()`

### Tab 4 — About

Content from `fillAboutTab(not_null<Ui::VerticalLayout*>, not_null<Window::SessionController*>)`:
- DB stats: deleted count, edited count
- Export DB button → `CustomDB::ExportDatabase()`
- Import DB button → `CustomDB::ImportDatabase()`
- Full backup / restore buttons → `CustomDB::ExportFullBackup()` / `ImportFullBackup()`
- Version label (static string)

---

## Content Refactoring Strategy

Each tab's content is extracted into a `fill*Tab()` free function with signature:
```cpp
void fillGeneralTab(
    not_null<Ui::VerticalLayout*> container,
    not_null<Window::SessionController*> controller);
```

This decouples content from the window frame. If a `GenericBox` variant is ever needed again, the same fill functions can be reused.

The `CustomModBox` function and all its inner lambdas are **deleted** from `settings_main.cpp`.

---

## Data Flow

```
OpenOrRaise(controller)
    │
    ├── [instance exists] → raise() + activateWindow()
    │
    └── [new] CustomModWindow(controller)
            │
            ├── setupTabs()
            │     ├── fillGeneralTab(layout, controller)
            │     ├── fillPeersTab(layout, controller)
            │     ├── fillArchiveTab(layout, controller)
            │     └── fillAboutTab(layout, controller)
            │
            └── restoreGeometry() from QSettings
                on closeEvent → saveGeometry() to QSettings
```

---

## Error Handling

| Scenario | Behavior |
|----------|---------|
| User clicks settings button while window is already open | `raise()` + `activateWindow()` — window comes to foreground |
| Window closed by OS (X button) | `WA_DeleteOnClose` triggers destructor, `_instance` pointer becomes null automatically |
| `controller` becomes invalid (session logout) | `CustomModWindow` observes `controller->session().alive()` rpl event and calls `close()` |
| DB operations fail (export/import) | `Ui::Toast::Show(error message)` — same pattern as existing code |

---

## Build Integration

`custom_mod_window.cpp` must be added to `Telegram/CMakeLists.txt` in the `NICE_FILES` or `SOURCES` list (same section as `custom_db.cpp` / `custom_settings.cpp`).

---

## Out of Scope

- Keyboard shortcut to open the window (can be added later)
- Tray icon menu entry (can be added later)
- Per-tab state persistence across open/close cycles (tabs always start at General)
- Search/filter within archive tab (future feature)
