# Mutual-Contact Indikator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show a small user-configurable emoji suffix next to a peer's display name — in the chat list, the Contacts list, and the profile header — whenever that peer is both a contact of yours and has confirmed you back (`isContact() && mutual_contact` per Telegram's own `User` API field), with independent on/off toggles and independent emoji text per location.

**Architecture:** Pure additive feature, no bypass of any kind — it renders data (`mutual_contact` flag) Telegram's server already sends and this codebase already parses (`UserDataFlag::MutualContact`, `data_session.cpp:696`). Settings storage follows the existing `CustomSettings::Values` struct pattern (bool + QString pairs, `Set`/`SetString`/`Get()`). Each of the 3 display locations gets its own narrowly-scoped code change — no shared "should show" helper is introduced in `custom_settings.cpp` (that module has no dependency on `UserData` today and shouldn't gain one); the two-line condition (`user->isContact() && (user->flags() & UserDataFlag::MutualContact)`) is written directly at each of the 3 call sites.

**Tech Stack:** C++17/Qt, tdesktop's `Ui::`/`rpl::` reactive framework. No automated test framework for this UI — verification is manual build + manual run in the app (see "Before you start").

---

## Before you start

Same as prior plans in this repo: this is a native Qt/C++ desktop app built with Visual Studio, no CLI build/test command available to the agent. Every "Verify" step means **ask the human partner to build in Visual Studio and manually exercise the described UI**, then report back pass/fail with specifics. Do not claim a step is done without that confirmation.

All edits are in five files:
- `Telegram/SourceFiles/custom_settings.h` / `custom_settings.cpp` (Task 1)
- `Telegram/SourceFiles/custom_mod_window.cpp` (Task 2)
- `Telegram/SourceFiles/dialogs/dialogs_entry.cpp` (Task 3)
- `Telegram/SourceFiles/boxes/peer_list_controllers.cpp` (Task 4)
- `Telegram/SourceFiles/info/profile/info_profile_top_bar.cpp` (Task 5)

---

### Task 1: Data layer — 6 new settings fields in `custom_settings.h`/`.cpp`

**Files:**
- Modify: `Telegram/SourceFiles/custom_settings.h:17-53`
- Modify: `Telegram/SourceFiles/custom_settings.cpp:125-162`

**Why:** Follows the exact existing pattern used by `spoofDeviceModel`/`spoofSystemVersion` (bool + QString pairs, loaded in `Init()`, dispatched via `UpdateValue`/`UpdateString`, exposed via `inline` global getters) — this task only adds data, no UI, no behavior.

- [ ] **Step 1: Add 6 fields to `Values` struct**

Find (`custom_settings.h`, inside `struct Values { ... };`):

```cpp
struct Values {
    bool ghostMode = true;
    bool bypassRestrictions = true;
    bool offlineDb = true;
    bool antiDelete = true;
    bool antiEdit = true;
    bool spoofMobile = true;
    bool storyAnonymousView = true;
    // C22: Dynamic device spoof.
    // NOTE: spoofDeviceType itself is NOT sent to Telegram's MTProto layer —
    // the server infers the device icon from the spoofDeviceModel/
    // spoofSystemVersion strings, not a separate type code. This field only
    // drives the preset buttons in custom_mod_window.cpp (which pre-fill
    // those two strings); it has no other effect.
    int  spoofDeviceType = 0; // 0=Android, 1=iOS, 2=Windows, 3=Linux
    QString spoofDeviceModel = u"Samsung Galaxy S26 Ultra"_q;
    QString spoofSystemVersion = u"Android 15"_q;
};
```

Replace with:

```cpp
struct Values {
    bool ghostMode = true;
    bool bypassRestrictions = true;
    bool offlineDb = true;
    bool antiDelete = true;
    bool antiEdit = true;
    bool spoofMobile = true;
    bool storyAnonymousView = true;
    // C22: Dynamic device spoof.
    // NOTE: spoofDeviceType itself is NOT sent to Telegram's MTProto layer —
    // the server infers the device icon from the spoofDeviceModel/
    // spoofSystemVersion strings, not a separate type code. This field only
    // drives the preset buttons in custom_mod_window.cpp (which pre-fill
    // those two strings); it has no other effect.
    int  spoofDeviceType = 0; // 0=Android, 1=iOS, 2=Windows, 3=Linux
    QString spoofDeviceModel = u"Samsung Galaxy S26 Ultra"_q;
    QString spoofSystemVersion = u"Android 15"_q;
    // Mutual-Contact Indikatori: mutual_contact bo'lgan peerlar ismi yoniga
    // emoji qo'shish, 3 mustaqil joy uchun mustaqil toggle+emoji.
    bool mutualContactShowInChatList = true;
    QString mutualContactChatListEmoji = u"🤝"_q;
    bool mutualContactShowInContactsList = true;
    QString mutualContactContactsListEmoji = u"🤝"_q;
    bool mutualContactShowInProfile = true;
    QString mutualContactProfileEmoji = u"🤝"_q;
};
```

- [ ] **Step 2: Add 6 global inline getters**

Find (`custom_settings.h`):

```cpp
inline int     SpoofDeviceType()    { return Get().spoofDeviceType; }
inline QString SpoofDeviceModel()   { return Get().spoofDeviceModel; }
inline QString SpoofSystemVersion() { return Get().spoofSystemVersion; }
[[nodiscard]] QString SpoofLangPack();
```

Replace with:

```cpp
inline int     SpoofDeviceType()    { return Get().spoofDeviceType; }
inline QString SpoofDeviceModel()   { return Get().spoofDeviceModel; }
inline QString SpoofSystemVersion() { return Get().spoofSystemVersion; }
[[nodiscard]] QString SpoofLangPack();

inline bool    MutualContactShowInChatList()     { return Get().mutualContactShowInChatList; }
inline QString MutualContactChatListEmoji()      { return Get().mutualContactChatListEmoji; }
inline bool    MutualContactShowInContactsList() { return Get().mutualContactShowInContactsList; }
inline QString MutualContactContactsListEmoji()  { return Get().mutualContactContactsListEmoji; }
inline bool    MutualContactShowInProfile()      { return Get().mutualContactShowInProfile; }
inline QString MutualContactProfileEmoji()       { return Get().mutualContactProfileEmoji; }
```

- [ ] **Step 3: Wire the 3 new bool ids into `UpdateValue()`**

Find (`custom_settings.cpp`):

```cpp
void UpdateValue(const QString &id, bool value) {
    if (id == "ghostMode") gValues.ghostMode = value;
    else if (id == "bypassRestrictions") gValues.bypassRestrictions = value;
    else if (id == "offlineDb") gValues.offlineDb = value;
    else if (id == "antiDelete") gValues.antiDelete = value;
    else if (id == "antiEdit") gValues.antiEdit = value;
    else if (id == "spoofMobile") gValues.spoofMobile = value;
    else if (id == "storyAnonymousView") gValues.storyAnonymousView = value;
}
```

Replace with:

```cpp
void UpdateValue(const QString &id, bool value) {
    if (id == "ghostMode") gValues.ghostMode = value;
    else if (id == "bypassRestrictions") gValues.bypassRestrictions = value;
    else if (id == "offlineDb") gValues.offlineDb = value;
    else if (id == "antiDelete") gValues.antiDelete = value;
    else if (id == "antiEdit") gValues.antiEdit = value;
    else if (id == "spoofMobile") gValues.spoofMobile = value;
    else if (id == "storyAnonymousView") gValues.storyAnonymousView = value;
    else if (id == "mutualContactShowInChatList") gValues.mutualContactShowInChatList = value;
    else if (id == "mutualContactShowInContactsList") gValues.mutualContactShowInContactsList = value;
    else if (id == "mutualContactShowInProfile") gValues.mutualContactShowInProfile = value;
}
```

- [ ] **Step 4: Wire the 3 new string ids into `UpdateString()`**

Find (`custom_settings.cpp`):

```cpp
void UpdateString(const QString &id, const QString &value) {
    if (id == "spoofDeviceModel") gValues.spoofDeviceModel = value;
    else if (id == "spoofSystemVersion") gValues.spoofSystemVersion = value;
}
```

Replace with:

```cpp
void UpdateString(const QString &id, const QString &value) {
    if (id == "spoofDeviceModel") gValues.spoofDeviceModel = value;
    else if (id == "spoofSystemVersion") gValues.spoofSystemVersion = value;
    else if (id == "mutualContactChatListEmoji") gValues.mutualContactChatListEmoji = value;
    else if (id == "mutualContactContactsListEmoji") gValues.mutualContactContactsListEmoji = value;
    else if (id == "mutualContactProfileEmoji") gValues.mutualContactProfileEmoji = value;
}
```

- [ ] **Step 5: Load the 6 new fields in `Init()`**

Find (`custom_settings.cpp`):

```cpp
    gValues.spoofDeviceType    = settings.value("spoofDeviceType", 0).toInt();
    gValues.spoofDeviceModel   = settings.value("spoofDeviceModel",
        u"Samsung Galaxy S26 Ultra"_q).toString();
    gValues.spoofSystemVersion = settings.value("spoofSystemVersion",
        u"Android 15"_q).toString();
```

Replace with:

```cpp
    gValues.spoofDeviceType    = settings.value("spoofDeviceType", 0).toInt();
    gValues.spoofDeviceModel   = settings.value("spoofDeviceModel",
        u"Samsung Galaxy S26 Ultra"_q).toString();
    gValues.spoofSystemVersion = settings.value("spoofSystemVersion",
        u"Android 15"_q).toString();
    gValues.mutualContactShowInChatList = settings.value(
        "mutualContactShowInChatList", true).toBool();
    gValues.mutualContactChatListEmoji = settings.value(
        "mutualContactChatListEmoji", u"🤝"_q).toString();
    gValues.mutualContactShowInContactsList = settings.value(
        "mutualContactShowInContactsList", true).toBool();
    gValues.mutualContactContactsListEmoji = settings.value(
        "mutualContactContactsListEmoji", u"🤝"_q).toString();
    gValues.mutualContactShowInProfile = settings.value(
        "mutualContactShowInProfile", true).toBool();
    gValues.mutualContactProfileEmoji = settings.value(
        "mutualContactProfileEmoji", u"🤝"_q).toString();
```

- [ ] **Step 6: Verify build**

Ask the human partner to build in Visual Studio. Expected: clean build — this task only adds data fields and their load/save plumbing, nothing reads them yet (Tasks 2-5 do).

- [ ] **Step 7: Commit**

```bash
git add Telegram/SourceFiles/custom_settings.h Telegram/SourceFiles/custom_settings.cpp
git commit -m "custom_settings: Mutual-Contact Indikator uchun 6 ta yangi sozlama maydoni"
```

---

### Task 2: General tab UI — new "🤝 Mutual-Contact Indikatori" section

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:434-446` (the `addToggle` lambda's id-matching chain, inside `fillGeneralTab`)
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp:622-624` (insertion point between the last `addToggle` call and the Branding section)

**Why:** Exposes the 3 toggles + 3 emoji inputs added in Task 1, using the exact same `addToggle` helper (for the toggles) and the exact same manual `Ui::InputField` + save-button pattern already used for the Device Spoof form (for the emoji text fields, since `addToggle` only builds boolean toggle rows, not text inputs).

- [ ] **Step 1: Add the 3 new bool ids to `addToggle`'s id-matching chain**

Find (inside `fillGeneralTab`, the `addToggle` lambda body):

```cpp
		const auto &val = CustomSettings::Get();
		auto current = false;
		if (id == u"ghostMode"_q) current = val.ghostMode;
		else if (id == u"bypassRestrictions"_q) current = val.bypassRestrictions;
		else if (id == u"offlineDb"_q) current = val.offlineDb;
		else if (id == u"antiDelete"_q) current = val.antiDelete;
		else if (id == u"antiEdit"_q) current = val.antiEdit;
		else if (id == u"spoofMobile"_q) current = val.spoofMobile;
		else if (id == u"storyAnonymousView"_q) current = val.storyAnonymousView;
```

Replace with:

```cpp
		const auto &val = CustomSettings::Get();
		auto current = false;
		if (id == u"ghostMode"_q) current = val.ghostMode;
		else if (id == u"bypassRestrictions"_q) current = val.bypassRestrictions;
		else if (id == u"offlineDb"_q) current = val.offlineDb;
		else if (id == u"antiDelete"_q) current = val.antiDelete;
		else if (id == u"antiEdit"_q) current = val.antiEdit;
		else if (id == u"spoofMobile"_q) current = val.spoofMobile;
		else if (id == u"storyAnonymousView"_q) current = val.storyAnonymousView;
		else if (id == u"mutualContactShowInChatList"_q) current = val.mutualContactShowInChatList;
		else if (id == u"mutualContactShowInContactsList"_q) current = val.mutualContactShowInContactsList;
		else if (id == u"mutualContactShowInProfile"_q) current = val.mutualContactShowInProfile;
```

- [ ] **Step 2: Insert the new section between "🛡️ Privacy & Custom Mods" and Branding**

Find (inside `fillGeneralTab`, right before the Branding section comment):

```cpp
	addToggle(
		u"offlineDb"_q,
		u"Offline xabar bazasi"_q,
		u"Xabarlar va medialarni internet boʻlmaganda ham koʻrish uchun qurilmada saqlaydi."_q);

	// ── Branding sektsiyasi ──────────────────────────────────────────
```

Replace with:

```cpp
	addToggle(
		u"offlineDb"_q,
		u"Offline xabar bazasi"_q,
		u"Xabarlar va medialarni internet boʻlmaganda ham koʻrish uchun qurilmada saqlaydi."_q);

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
		Ui::Toast::Show(u"Saqlandi!"_q);
	});

	// ── Branding sektsiyasi ──────────────────────────────────────────
```

Note: `addToggle(id, text, QString())` — passing an empty `QString()` as the `description` argument is already-supported behavior (the lambda's existing `if (!description.isEmpty())` check skips adding a description label), used here because the section-level description above already explains the feature; repeating it 3 times per-toggle would be redundant.

- [ ] **Step 3: Verify build**

Ask the human partner to build in Visual Studio. Expected: clean build. `Ui::InputField`, `Ui::RoundButton`, `st::defaultInputField`, `st::boxRowPadding` are all already used elsewhere in this exact function (Device Spoof form) — no new includes needed.

- [ ] **Step 4: Manually verify in the running app**

Ask the human partner to open Custom Window → General tab, scroll to the new "🤝 Mutual-Contact Indikatori" section (between Privacy & Custom Mods and Branding), confirm 3 toggle+emoji-input pairs are visible and readable, toggle each on/off (confirm toast appears), change an emoji value and click "💾 Saqlash (belgilar)" (confirm toast appears), close and reopen Custom Window (confirm the changed emoji persisted).

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "custom_mod: General tab ga Mutual-Contact Indikatori sozlamalari qo'shildi"
```

---

### Task 3: Chat list integration

**Files:**
- Modify: `Telegram/SourceFiles/dialogs/dialogs_entry.cpp:8-30` (includes)
- Modify: `Telegram/SourceFiles/dialogs/dialogs_entry.cpp:335-345` (`Entry::chatListNameText()`)

**Why:** Per the spec, only the display-text builder changes — not `chatListName()` itself, which is also used for sorting/search and must stay unmodified.

- [ ] **Step 1: Add the two needed includes**

Find (`dialogs_entry.cpp`, near the top):

```cpp
#include "dialogs/dialogs_key.h"
#include "dialogs/dialogs_indexed_list.h"
#include "base/options.h"
#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_session.h"
```

Replace with:

```cpp
#include "dialogs/dialogs_key.h"
#include "dialogs/dialogs_indexed_list.h"
#include "base/options.h"
#include "base/unixtime.h"
#include "custom_settings.h"
#include "data/data_changes.h"
#include "data/data_session.h"
#include "data/data_user.h"
```

- [ ] **Step 2: Append the emoji suffix in `chatListNameText()`**

Find:

```cpp
const Ui::Text::String &Entry::chatListNameText() const {
	const auto version = chatListNameVersion();
	if (_chatListNameVersion < version) {
		_chatListNameVersion = version;
		_chatListNameText.setText(
			st::semiboldTextStyle,
			chatListName(),
			Ui::NameTextOptions());
	}
	return _chatListNameText;
}
```

Replace with:

```cpp
const Ui::Text::String &Entry::chatListNameText() const {
	const auto version = chatListNameVersion();
	if (_chatListNameVersion < version) {
		_chatListNameVersion = version;
		auto name = chatListName();
		if (const auto history = asHistory()) {
			if (const auto user = history->peer()->asUser()) {
				if (CustomSettings::MutualContactShowInChatList()
						&& user->isContact()
						&& (user->flags() & UserDataFlag::MutualContact)) {
					name += u" "_q + CustomSettings::MutualContactChatListEmoji();
				}
			}
		}
		_chatListNameText.setText(
			st::semiboldTextStyle,
			name,
			Ui::NameTextOptions());
	}
	return _chatListNameText;
}
```

- [ ] **Step 3: Verify build**

Ask the human partner to build in Visual Studio. Expected: clean build. `Entry::asHistory() const` is an existing method on the same class (`dialogs_entry.h:95`), `UserDataFlag` comes from the newly-added `data/data_user.h` include.

- [ ] **Step 4: Manually verify in the running app**

Ask the human partner to open the main chat list and confirm: any chat with a peer who is both in their contacts and has `mutual_contact` shows the 🤝 (or whatever emoji was set in Task 2 Step 4) after the name; other chats (groups, channels, non-mutual contacts) are unaffected. Toggling "Chat roʻyxatida koʻrsatish" off in Custom Window should make the emoji disappear from the chat list (may require switching chats or scrolling to force a repaint, since this doesn't trigger a live re-render on toggle — that's expected, not a bug, for this simple implementation).

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/dialogs/dialogs_entry.cpp
git commit -m "dialogs: chat ro'yxatida Mutual-Contact Indikatori"
```

---

### Task 4: Contacts list integration

**Files:**
- Modify: `Telegram/SourceFiles/boxes/peer_list_controllers.cpp:8-25` (includes)
- Modify: `Telegram/SourceFiles/boxes/peer_list_controllers.cpp:815-818` (`ContactsBoxController::createRow`)

**Why:** `ContactsBoxController::createRow` currently returns a plain `PeerListRow`. A small local subclass overriding the existing `virtual QString generateName()` hook is the narrowest possible change — it affects ONLY the Contacts list, not other `PeerListRow`-based boxes (add participants, forward picker, etc.), since those use different controller classes with their own `createRow` overrides.

- [ ] **Step 1: Add the include**

Find (`peer_list_controllers.cpp`, near the top):

```cpp
#include "boxes/peer_list_controllers.h"

#include "api/api_chat_participants.h"
#include "api/api_premium.h" // MessageMoneyRestriction.
#include "base/random.h"
```

Replace with:

```cpp
#include "boxes/peer_list_controllers.h"

#include "api/api_chat_participants.h"
#include "api/api_premium.h" // MessageMoneyRestriction.
#include "base/random.h"
#include "custom_settings.h"
```

(`data/data_user.h` is already included in this file — confirmed, no change needed there.)

- [ ] **Step 2: Add the row subclass and update `createRow`**

Find:

```cpp
std::unique_ptr<PeerListRow> ContactsBoxController::createRow(
		not_null<UserData*> user) {
	return std::make_unique<PeerListRow>(user);
}
```

Replace with:

```cpp
namespace {

class MutualContactPeerListRow final : public PeerListRow {
public:
	explicit MutualContactPeerListRow(not_null<UserData*> user)
	: PeerListRow(user) {
	}

	QString generateName() override {
		auto name = PeerListRow::generateName();
		if (const auto user = peer()->asUser()) {
			if (CustomSettings::MutualContactShowInContactsList()
					&& user->isContact()
					&& (user->flags() & UserDataFlag::MutualContact)) {
				name += u" "_q
					+ CustomSettings::MutualContactContactsListEmoji();
			}
		}
		return name;
	}
};

} // namespace

std::unique_ptr<PeerListRow> ContactsBoxController::createRow(
		not_null<UserData*> user) {
	return std::make_unique<MutualContactPeerListRow>(user);
}
```

- [ ] **Step 3: Verify build**

Ask the human partner to build in Visual Studio. Expected: clean build. `PeerListRow::generateName()` is `virtual` (`boxes/peer_list_box.h:98`) and `PeerListRow(not_null<PeerData*> peer)` is an existing `explicit` constructor (`boxes/peer_list_box.h:69`) — `MutualContactPeerListRow`'s constructor passes the `UserData*` up to it (valid, since `UserData` derives from `PeerData`).

- [ ] **Step 4: Manually verify in the running app**

Ask the human partner to open the main menu → Contacts, and confirm: contacts who are mutual (isContact + mutual_contact) show the configured emoji after their name; the emoji respects the "Contacts roʻyxatida koʻrsatish" toggle and its own emoji text from Task 2; other peer-picker dialogs (e.g. "Add members to group", "Forward message") are visually unaffected (no emoji appears there — confirms the change didn't leak into shared `PeerListRow` usage).

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/boxes/peer_list_controllers.cpp
git commit -m "boxes: Contacts ro'yxatida Mutual-Contact Indikatori"
```

---

### Task 5: Profile header integration

**Files:**
- Modify: `Telegram/SourceFiles/info/profile/info_profile_top_bar.cpp` (includes near top)
- Modify: `Telegram/SourceFiles/info/profile/info_profile_top_bar.cpp:3067-3072` (`TopBar::nameValue()`)

**Why:** `TopBar::nameValue()` is a narrowly-scoped member function used only by the profile header's title label (`_title(this, nameValue(), _st.title)`) — wrapping it here, rather than the shared free function `Info::Profile::NameValue()` (used by many other UI elements across the app), keeps the change confined to exactly the "Profil sarlavhasi" location from the spec.

- [ ] **Step 1: Add the include**

Find (`info_profile_top_bar.cpp`, near the top, alongside the existing `data/data_peer.h`/`data/data_user.h` includes):

```cpp
#include "data/data_peer.h"
```

Replace with:

```cpp
#include "custom_settings.h"
#include "data/data_peer.h"
```

(`data/data_user.h` is already included in this file — confirmed, no change needed there.)

- [ ] **Step 2: Wrap the name producer in `nameValue()`**

Find:

```cpp
rpl::producer<QString> TopBar::nameValue() const {
	if (const auto topic = _key.topic()) {
		return Info::Profile::TitleValue(topic);
	}
	return Info::Profile::NameValue(_peer);
}
```

Replace with:

```cpp
rpl::producer<QString> TopBar::nameValue() const {
	if (const auto topic = _key.topic()) {
		return Info::Profile::TitleValue(topic);
	}
	const auto peer = _peer;
	return Info::Profile::NameValue(peer) | rpl::map([=](QString name) {
		if (const auto user = peer->asUser()) {
			if (CustomSettings::MutualContactShowInProfile()
					&& user->isContact()
					&& (user->flags() & UserDataFlag::MutualContact)) {
				name += u" "_q + CustomSettings::MutualContactProfileEmoji();
			}
		}
		return name;
	});
}
```

- [ ] **Step 3: Verify build**

Ask the human partner to build in Visual Studio. Expected: clean build. `_peer` is `const not_null<PeerData*> _peer;` (`info_profile_top_bar.h:208`), captured by value into the `rpl::map` lambda.

- [ ] **Step 4: Manually verify in the running app**

Ask the human partner to open a mutual contact's profile (click their name/avatar to open the info panel) and confirm the emoji appears after their name in the profile header, respects the "Profil sarlavhasida koʻrsatish" toggle and its emoji text from Task 2, and does NOT appear on group/channel info headers or forum topic headers (those go through the `_key.topic()` branch or a non-user peer, both unaffected by this change).

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/info/profile/info_profile_top_bar.cpp
git commit -m "info/profile: Profil sarlavhasida Mutual-Contact Indikatori"
```

---

### Task 6: Full manual regression pass

**Files:** none (verification only)

**Why:** Tasks 3-5 each touch a different, otherwise-unrelated part of the codebase (dialogs, peer-list boxes, profile info) that share only the Task 1/2 settings — a full pass confirms all three respect their independent toggles/emoji correctly and that nothing outside the 3 intended locations was affected.

- [ ] **Step 1: Full manual walkthrough**

Ask the human partner to do a final build, then in the running app:
1. Confirm Custom Window → General tab shows the new section with 3 toggle+emoji pairs, correctly positioned between "🛡️ Privacy & Custom Mods" and "🎨 Branding".
2. Pick a real mutual contact (or create the condition by adding each other on two test accounts, if available) and confirm the same emoji-suffixed name appears in: the chat list, the Contacts list, and that person's profile header.
3. Turn off each of the 3 toggles one at a time and confirm the emoji disappears from exactly that one location (restart the app or reopen the relevant screen if a location doesn't refresh live — this implementation does not add live re-render wiring, only respects the setting on next natural repaint/screen-open).
4. Change each of the 3 emoji fields to a different character (e.g. "👌"), save, and confirm each location now shows its own distinct emoji.
5. Confirm a non-mutual contact, a non-contact user, and a group/channel never show the emoji anywhere, regardless of toggle state.
6. Confirm other `PeerListRow`-based dialogs (add participants to a group, forward-message recipient picker) do NOT show the emoji — this would indicate Task 4's change leaked beyond `ContactsBoxController`.

Report back a pass/fail summary. If anything fails, treat it as a bug against the specific task above that owns that code path, fix it there, and re-verify.

- [ ] **Step 2: No commit needed for this task** (verification only — any fixes found go into a follow-up commit referencing the task they belong to).

---

## Out of scope (explicitly deferred)

- "Last seen" / profile-photo privacy-bypass features — per the design spec, technical investigation found no evidence of client-side-hideable data (the server itself withholds the real value when privacy restricts it). The user is gathering additional first-hand observations before any further work on this is planned. Do not implement anything for this without a new, separate brainstorming/spec cycle.
