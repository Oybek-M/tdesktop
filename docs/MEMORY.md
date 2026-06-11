# CustomMod — Session Memory (Agent uchun)

**Oxirgi yangilanish:** 2026-06-10 (T33–T40, schema v5)  
**Loyiha:** Telegram Desktop custom mod

---

## Muhim Qoidalar (DOIM amal qilish kerak)

1. **Build va Git bilan foydalanuvchi o'zi ishlaydi** — men teginmayman
2. **Git commit larga teginmayman** — foydalanuvchi o'zi qiladi
3. **Javoblar O'zbek lotin tilida** — har doim

---

## Fayl Xaritasi — Custom Mod

```
Telegram/SourceFiles/
├── custom_settings.h/.cpp     — Values struct, inline helperlar, Should* API
├── custom_db.h/.cpp           — SQLite: deleted/edited/cached messages, backup/restore
├── custom_branding.h/.cpp     — JSON orqali window title + icon override
├── custom_mod_window.h/.cpp   — ASOSIY FAYL: barcha UI tab funksiyalari
├── custom_mod_settings.h/.cpp — Eski Qt widget (ishlatilmayapti, saqlanib turibdi)
└── data/data_stories.cpp      — Story anonim ko'rish hook (3 joy)
```

---

## custom_settings.h — Hozirgi Values struct

```cpp
struct Values {
    bool ghostMode = true;
    bool bypassRestrictions = true;
    bool offlineDb = true;
    bool antiDelete = true;
    bool antiEdit = true;
    bool spoofMobile = true;
    bool storyAnonymousView = true;
};

struct PerPeerEntry {
    QString peerId;
    QString displayName;
    bool ghostEnabled;
    bool antiDeleteEnabled;
    bool antiEditEnabled;
};
```

**Should* helpers (priority zanjiri):**
```
Blocklist (false) → Whitelist (true) → Per-peer override → Global flag
```

```cpp
[[nodiscard]] bool ShouldAntiDelete(const QString &peerId);
[[nodiscard]] bool ShouldAntiEdit(const QString &peerId);
[[nodiscard]] bool ShouldGhost(const QString &peerId);
[[nodiscard]] bool ShouldAnonymousStory(const QString &peerId);

// T37: background cache (text_cache) ga qaysi peerlar tushishini hal qiladi.
// = WhiteList | (HasPerPeerOverride && (AntiDeleteForPeer | AntiEditForPeer))
// Global flag ON bo'lsa ham qolgan chatlar cache QILINMAYDI (performance).
[[nodiscard]] bool ShouldBackgroundCache(const QString &peerId);
```

**Per-peer API:**
```cpp
[[nodiscard]] bool GhostModeForPeer(const QString &peerId);
[[nodiscard]] bool AntiDeleteForPeer(const QString &peerId);
[[nodiscard]] bool AntiEditForPeer(const QString &peerId);
void SetGhostModeForPeer(const QString &peerId, bool enabled);
void SetAntiDeleteForPeer(const QString &peerId, bool enabled);
void SetAntiEditForPeer(const QString &peerId, bool enabled);
void AddPerPeerOverride(const QString &peerId, const QString &displayName);
void RemovePerPeerOverride(const QString &peerId);
bool HasPerPeerOverride(const QString &peerId);
QVector<PerPeerEntry> GetPerPeerOverrides();
```

---

## custom_db.h — To'liq API

### Asosiy actioned_messages jadvali
```cpp
void MarkDeleted(long long msgId, const QString &peerId,
    const QString &mediaPath, const QString &originalText,
    unsigned int msgDate, bool isOut);
bool IsDeletedLocally(const QString &peerId, long long msgId);
void PermanentlyDeleteMessage(const QString &peerId, long long msgId);
bool IsUserDeletePending(const QString &peerId, long long msgId);
void ClearUserDeletePending(const QString &peerId, long long msgId);
```

### text_cache jadvali (T27 — Background AntiEdit/Delete; v5: sender_id + is_media)
```sql
CREATE TABLE text_cache (
    peer_id TEXT, msg_id INTEGER, text TEXT,
    is_out INTEGER DEFAULT 0, msg_date INTEGER DEFAULT 0,
    cached_at INTEGER,
    sender_id TEXT DEFAULT '',     -- v5 (T36): guruhda haqiqiy yuboruvchi
    is_media INTEGER DEFAULT 0,    -- v5 (T38): captionsiz media ham cache ga tushadi
    PRIMARY KEY(peer_id, msg_id)
);
CREATE INDEX idx_tc_cached_at ON text_cache(cached_at);
-- actioned_messages ga ham v5 da sender_id + is_media qo'shildi.
-- kCurrentSchemaVersion = 5. Migration: ALTER TABLE ... ADD COLUMN (execSql xatoni log qiladi).
```

```cpp
// Yangi xabar kelganda chaqiriladi (agar ShouldBackgroundCache — T37)
// v5: senderId — guruhda haqiqiy yuboruvchi; isMedia — media xabar (matnsiz ham cache)
void CacheMessageText(const QString &peerId, long long msgId,
    const QString &text, bool isOut, unsigned int msgDate,
    const QString &senderId = QString(), bool isMedia = false);

// Cache dan text (bo'lmasa "")
QString GetCachedText(const QString &peerId, long long msgId);

// Cache dan text + date + sender + media birga (T31, v5 da kengaytirildi)
QString GetCachedTextAndDate(const QString &peerId, long long msgId,
    unsigned int &outDate,
    QString *outSenderId = nullptr, bool *outIsMedia = nullptr);

// MarkDeleted (v5): + senderId + isMedia (default ""/false)
void MarkDeleted(msgId, peerId, mediaPath, originalText, msgDate, isOut,
    senderId="", isMedia=false);

// Background edit (existing==nullptr da)
bool RecordBackgroundEdit(const QString &peerId, long long msgId,
    const QString &newText, bool isOut, unsigned int msgDate);

// 30 kundan eski yozuvlarni tozalash (avtomatik har 1000 ta cache da)
void PruneStaleCachedText(int days = 30);

// Non-channel background delete (peerId ma'lum emas)
void TryRecordBackgroundDelete(long long msgId);
```

### Backup/Restore (v2)
```cpp
bool ExportFullBackup(const QString &destDir);
bool ImportFullBackup(const QString &srcDir);
```
Export v2 tarkibi: `custom_mod.db` + `BombMedia/` + `peer_lists.json` + `branding.json` + registry + `manifest.json`

---

## data/data_session.cpp — Custom Hook Joylari

### addNewMessage(MsgId id, ...) — cache MARKAZI (T33)
```cpp
// ~line 3317: BARCHA yangi xabar yo'llarining yagona funnel'i.
// MUHIM: hook avval processMessages() da edi, lekin online real-time xabarlar
// (updateNewMessage/updateShortMessage/updateNewChannelMessage) processMessages
// dan O'TMAYDI → addNewMessage ga to'g'ridan-to'g'ri keladi. Shu sababli ko'chirildi.
if (result && type == NewMessageType::Unread) {
    peerIdStr = result->history()->peer->id.value;
    if (ShouldBackgroundCache(peerIdStr)) {            // T37: WhiteList | Per-Chat override
        text = result->originalText().text;
        hasMedia = (result->media() != nullptr);
        if (!text.isEmpty() || hasMedia) {             // T38: media ham cache ga
            senderId = result->from()->id.value;       // T36: haqiqiy yuboruvchi
            CacheMessageText(peerId, msgId, text, isOut, date, senderId, hasMedia);
        }
    }
}
```

### processMessages() — endi cache QILMAYDI (T33)
```cpp
// Hook addNewMessage() ga ko'chirildi. processMessages faqat addNewMessage chaqiradi.
```

### processMessagesDeleted() — channel/group (updateDeleteChannelMessages)
```cpp
// Avval list = messagesList(peerId) olinadi
// Har bir messageId uchun:
//   1. Memory da item bor → text + date + isOut olinadi
//   2. Memory da yo'q → GetCachedTextAndDate(peerId, msgId, cachedDate)  ← T31
//   3. msgDate == 0 → MarkDeleted CHAQIRILMAYDI (spam oldini olish)  ← T30
//   4. msgDate > 0 → MarkDeleted(text, date, isOut)
```

### processNonChannelMessagesDeleted() — user-user (updateDeleteMessages)
```cpp
// item == nullptr → TryRecordBackgroundDelete(messageId.v)  ← T28
```

### updateEditedMessage()
```cpp
// existing == nullptr → RecordBackgroundEdit(peerId, msgId, newText, isOut, date)  ← T27
```

---

## history/history.cpp — loadDeletedMessages()

```
if (isEmpty()) return;  // ← QAYTARILDI (T29 da o'chib crash berdi)

Har bir msg uchun:
  → date == 0 && text.isEmpty() → skip (spam yoqoldi)
  → date == 0 && text bor → effectiveDate = currentSecsSinceEpoch() (taxminiy)
  → date > 0 → effectiveDate = msg.date
  → text.isEmpty() → "(matn saqlanmagan)" placeholder
```

**T30+T31 ta'siri:** Endi `msgDate==0 + text==""` lar DB ga yozilmaydi, shuning uchun
`loadDeletedMessages` ga bunday yozuvlar umuman kelmaydi.

---

## custom_mod_window.cpp — Tuzilma

### Forward declarations:
```cpp
void fillGeneralTab(not_null<Ui::VerticalLayout*> content);
void fillPeersTab(not_null<Ui::VerticalLayout*> content,
                  not_null<Window::SessionController*> controller);
void fillArchiveTab(not_null<Ui::VerticalLayout*> content, Fn<void()> onRefresh);
void fillAboutTab(not_null<Ui::VerticalLayout*> content,
                  QWidget *dialogParent, Fn<void()> onArchiveChanged);
```

### CustomModWindow class:
```cpp
class CustomModWindow final : public Ui::RpWidget {
public:
    void showBox(object_ptr<Ui::BoxContent> box);
private:
    CustomTabBar *_tabBar = nullptr;
    std::array<Ui::ScrollArea*, 4> _panels = {};
    std::array<QPointer<Ui::VerticalLayout>, 4> _inners = {};
    std::unique_ptr<Ui::LayerManager> _layerManager;
};
QPointer<CustomModWindow> gInstance;
```

### fillGeneralTab — Togglelar (tartib saqlansin)
```
"Privacy & Ghost Mode":
  - ghostMode          → "Ghost Mode"
  - storyAnonymousView → "Hikoyalarni anonim ko'rish"
  - spoofMobile        → "Mobil qurilma ko'rinishi"

"Cheklovlar":
  - bypassRestrictions → "Cheklangan chatda nusxalash va yuborish"

"Xabarlar tarixi":
  - antiDelete → "Anti-Delete"
  - antiEdit   → "Anti-Edit"
  - offlineDb  → "Offline xabar bazasi"

"Branding" (T24):
  - titleInput  → windowTitle
  - modInput    → customModTitle
  - iconInput + file picker + clear button
  - "Saqlash" tugmasi → CustomBranding::Set*() + Toast
```

### fillAboutTab — 7 ta backup element:
1. SQLite DB (`custom_mod.db`)
2. Barcha media (`BombMedia/`)
3. White/Black list (`peer_lists.json`)
4. Branding (`branding.json`)
5. Registry sozlamalari
6. Manifest (`manifest.json`)
7. Auto-restart import dan keyin (3 soniya QTimer)

---

## custom_branding.h/.cpp — Branding

**Fayl:** `%AppData%/CustomMod/branding.json`
```json
{
  "windowTitle":    "Telegram",
  "customModTitle": "Customizations (By Oybek)",
  "iconPath":       ""
}
```

**API:**
```cpp
void CustomBranding::Load();
const BrandingValues& CustomBranding::Get();
void CustomBranding::SetWindowTitle(const QString &title);
void CustomBranding::SetCustomModTitle(const QString &title);
void CustomBranding::SetIconPath(const QString &path);
void CustomBranding::Save();
```

**Hook joylari:**
- `Application::run()` → `CustomBranding::Load()` + `OverrideApplicationIcon`
- `MainWindow::updateTitle()` → `CustomBranding::Get().windowTitle`
- `CustomModWindow` ctor → `setWindowTitle(CustomBranding::Get().customModTitle)`

**Rebuild kerak (JSON da YO'Q):**
1. Executable nomi: `Telegram/CMakeLists.txt` → `OUTPUT_NAME`
2. Exe icon: `Telegram/Resources/winrc/Telegram.rc`
3. App ID: `core/launcher.cpp:338` — DIQQAT: registry path o'zgaradi, sozlamalar yo'qoladi

---

## Account Limit (T21)

`Telegram/SourceFiles/main/main_domain.h`:
```cpp
static constexpr auto kMaxAccounts = 100;         // asl: 3
static constexpr auto kPremiumMaxAccounts = 100;  // asl: 6
```

---

## Storage Joylari

| Narsa | Joy |
|---|---|
| Global settings (togglelar) | `HKCU\Software\CustomMod\TelegramDesktop` |
| White/Black list | `%AppData%\...\CustomMod\peer_lists.json` |
| Per-peer ghost override | Registry `GhostModePerPeer` group |
| Per-peer antidelete | Registry `AntiDeletePerPeer` group |
| Per-peer antiedit | Registry `AntiEditPerPeer` group |
| Per-Chat names master | Registry `PerPeerNames` group |
| SQLite DB | `%AppData%\...\CustomMod\custom_mod.db` |
| Media | `%AppData%\...\CustomMod\BombMedia\` |
| Branding | `%AppData%\...\CustomMod\branding.json` |

---

## Yangi Helperlar (anonymous namespace, custom_mod_window.cpp)

```cpp
inline QColor AvatarFallbackColor(int idx);  // 7 ta rang palitrasi
void PaintPeerAvatar(QPainter &p, const QRect &rect,
    const QString &peerId, const QString &name,
    Main::Session *session, Ui::PeerUserpicView &view);
```

---

## Kerakli Include lar (custom_mod_window.cpp)

```cpp
#include "custom_db.h"
#include "custom_settings.h"
#include "custom_branding.h"
#include "core/application.h"        // Core::Restart()
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "main/main_session.h"
#include "ui/painter.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "ui/layers/layer_manager.h"
#include "ui/userpic_view.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/themes/window_theme.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_basic.h"
#include "styles/style_custom_mod.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include <QtCore/QTimer>             // Core::Restart() 3s delay uchun
```

---

## Bug Fix Tarixi (muhim)

| T | Muammo | Yechim |
|---|---|---|
| T28 | Restart dan keyin deleted msg yo'qoladi | `processMessagesDeleted` da text+date+isOut ni memory dan olib MarkDeleted ga uzatish |
| T29 | `loadDeletedMessages` da `isEmpty()` crash | `isEmpty()` tekshiruvini qaytarish; date=0 fallback + placeholder qo'shish |
| T30 | Group chatlarda "(matn saqlanmagan)" spam | `msgDate==0` bo'lsa MarkDeleted chaqirmaslik |
| T31 | Cache da date bor lekin olinmayapti | `GetCachedTextAndDate()` qo'shish, delete da cache dan date ham olish |
| T32 | BlackList / global-flag peerlar ham cache ga tushib ketardi | `processMessages` hook da `ShouldAntiEdit\|\|ShouldAntiDelete` → `IsInWhitelist` |
| T33 | **KRITIK**: online real-time xabarlar cache ga tushmaydi (`processMessages` dan o'tmaydi) | Hook `addNewMessage()` funnel ga ko'chirildi |
| T34 | `TryRecordBackgroundDelete` `LIMIT 1` kanal yozuvini olib soxta o'chirilgan ko'rsatardi | Channel peer (`(value>>48)&0xFF==2`) filtrlash, non-channel ni tanlash |
| T35 | `RecordBackgroundEdit` cache bo'sh bo'lsa T32 ni chetlab newText ni cache qilardi | `updateEditedMessage` guard `ShouldAntiEdit` → `ShouldBackgroundCache` |
| T36 | Guruhda o'chirilgan xabar "guruh nomidan" ko'rinardi | Schema v5 `sender_id`; `loadDeletedMessages` da `from=PeerId(senderId)` |
| T37 | Per-Chat AntiDelete yoqilgan, lekin WhiteList'da yo'q peer cache ga tushmaydi | `ShouldBackgroundCache` helper (WhiteList \| Per-Chat override) |
| T38 | Captionsiz media o'chirilsa butunlay yo'qolardi (bo'sh matn skip) | Schema v5 `is_media`; `CacheMessageText` media'da bo'sh matnni ham yozadi; "(media xabar)" placeholder |
| T39 | Archive tab da background media "(empty)" ko'rinardi | `GetAllDeletedMessages` + struct ga `is_media`/`sender_id`; UI "(media xabar)" |
| T40 | `RecordBackgroundEdit` re-cache da sender/media o'chirardi (T36 regressi) | `GetCachedTextAndDate` bilan o'qib, `CacheMessageText` ga qaytadan uzatish |

---

## Keyingi Session Boshlashda Shu Faylni O'qi

1. `DOCs/MEMORY.md` — joriy holat (bu fayl)
2. `DOCs/PRD.md` — to'liq feature talablari
3. `DOCs/NEXT_TASKS.md` — tugallangan va qolgan tasklar
4. Kerakli faylni `Read` qil (katta fayllardan faqat kerakli qism)
