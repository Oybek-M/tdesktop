# Custom Mod Improvements — Design Spec
Date: 2026-05-19

## Scope

8 ta funksionallikni to'liq va muammosiz implement qilish. Hozirgi ishlaydigan featurelarga salbiy ta'sir qilmagan holda.

---

## 1. Unified Peer Tracking Logic

**Muammo:** AntiDelete, AntiEdit, GhostMode hammasi alohida global flag bilan ishlaydi. Whitelist API bor lekin wiring yo'q.

**Yechim:** `custom_settings.h/cpp` da uchta unified helper:

```cpp
bool ShouldAntiDelete(const QString &peerId);
bool ShouldAntiEdit(const QString &peerId);
bool ShouldGhost(const QString &peerId);
```

Har birining logikasi:
- Global flag ON → `true` (barcha peer lar uchun)
- Global flag OFF, whitelist bo'sh → `false`
- Global flag OFF, whitelist bor → `peerId` whitelist da bormi tekshir

**Wiring joylari:**
- `data_session.cpp` → `processNonChannelMessagesDeleted` va `processMessagesDeleted` → `ShouldAntiDelete(peerIdStr)` 
- `history_item.cpp` → `applyEdition` ichida → `ShouldAntiEdit(peerIdStr)`
- `data/data_stories.cpp` → `sendMarkAsReadRequest` → `ShouldGhost(peerIdStr)`
- `data_histories.cpp` → read history blocking → `ShouldGhost(peerIdStr)` (mavjud, tekshirish kerak)

---

## 2. PeerID Whitelist UI

**Hozirgi holat:** Faqat qo'lda ID kiritish. `AntiDeleteForPeer`, `AntiEditForPeer` set/get bor lekin UI to'liq emas.

**Yangi UI (settings_main.cpp):**

### Whitelist bo'limi (har 3 feature uchun alohida yoki bitta):
- "Faqat tanlangan chatlar uchun ishlash" toggle
- "Chat qo'shish" tugmasi → `ChooseRecipientBox` (native Telegram dialog) → peer ID va nom avtomatik olinadi
- "ID kiritish" tugmasi → manual input (mavjud pattern)
- Ro'yxat: `[PeerName] [PeerID] [X o'chirish]`

### Whitelist turi:
**Bitta umumiy whitelist** — AntiDelete, AntiEdit, Ghost uchun ham ishlatiladi. Peer whitelist ga qo'shilganda uchala feature ham shu peer uchun aktiv bo'ladi. Alohida whitelist yo'q — soddalik va kichik DB uchun.

**Yangi funksiyalar custom_settings.h:**
```cpp
// Whitelist management
void AddToWhitelist(const QString &peerId, const QString &peerName);
void RemoveFromWhitelist(const QString &peerId);
QVector<QPair<QString, QString>> GetWhitelist(); // {peerId, peerName}
bool IsInWhitelist(const QString &peerId);
```

---

## 3. Forward Bypass — Kaskad Yondashuvi

**Muammo:** `noForwards` flag bo'lganda yoki file reference muddati o'tganda media forward ishlamaydi.

**Kaskad (prioritet tartibida):**

```
1. Native MTPmessages_ForwardMessages
   ↓ (agar rad etilsa: FLOOD_WAIT, CHAT_FORWARDS_RESTRICTED, etc.)
2. SendExistingDocument/Photo (file reference bilan)
   ↓ (agar file reference muddati o'tgan)
3. CustomDB::FindSavedMediaPath(peerId, msgId) → lokal fayldan re-upload
   ↓ (agar lokal fayl ham yo'q)
4. Telegram keshidan (doc->filepath(true)) re-upload
   ↓ (agar keshda ham yo'q)
5. Toast: "Media topilmadi, faqat matn yuborildi"
```

**Yangi funksiya custom_db.h:**
```cpp
QString FindSavedMediaPath(const QString &peerId, long long msgId);
// actioned_messages.mediaPath ni qaytaradi
```

**Implement joyi:** `apiwrap.cpp` → `forwardMessages()` ichida, `BypassRestrictions()` true bo'lganda.

---

## 4. Media Saqlash — To'liq va Bitta Joyda

**Muammo:**
- Fotolar saqlanmaydi (`data_photo.cpp` da hook yo'q)
- `~/Downloads/Telegram_AntiDelete/` va `customizationMainFolder/medias/` — ikki xil joy
- Backup ga faqat `customizationMainFolder` kiradi

**Yechim:**
- **Bitta joy:** `customizationMainFolder/medias/{images,videos,voices,files}/`
- `setDeletedLocally()` ham shu joyga nusxalash (`~/Downloads/` o'rniga)
- `data_photo.cpp` ga hook: `PhotoData` yuklab bo'linganda `ShouldAntiDelete(peerId)` bo'lsa `SaveMediaFile(path, "image")`
- `data_document.cpp` — mavjud, faqat peer check qo'shamiz

**Trigger vaqti:**
- `setDeletedLocally()` chaqirilganda (xabar o'chirilganda) — bu asosiy trigger
- `data_document.cpp` da mavjud `DocumentData::save()` hook — `data_photo.cpp` da xuddi shu pattern: `PhotoData` ning yuklash tugash callbacki. Aniq hook nomi Sprint 3 da `data_photo.cpp` o'qib aniqlanadi (`data_document.cpp` dagi pattern takrorlanadi).

---

## 5. Story Anonim Ko'rish

**Hozirgi holat:** Global `GhostMode()` tekshiriladi → `sendMarkAsReadRequest` skip qilinadi.

**Yangi holat:**
```cpp
// data/data_stories.cpp:
if (ShouldGhost(peerIdStr)) { return; }
```
`ShouldGhost` whitelist ham tekshiradi — peer whitelist da bo'lsa global ghost off bo'lsa ham ishlaydi.

---

## 6. Last Seen (Tasdiqlash)

Hozir to'liq ishlaydi (`api_updates.cpp`):
- `updateOnline()` → GhostMode bo'lsa `offline=true` yuboradi
- `sendAction()` → GhostMode bo'lsa online signal chiqmaydi

**Cheklov:** `updateOnline()` global MTProto call, peer argumenti yo'q — per-peer online status hiding arxitektura darajasida mumkin emas. Shuning uchun:
- **Global GhostMode ON** → hamma uchun online ko'rinmaydi (hozirgi kabi)
- **Per-peer whitelist** → faqat read receipts va story views uchun ishlaydi (peer konteksti bor joyda)
- Last seen per-peer hiding: **scope dan tashqarida**, alohida spec kerak bo'ladi

---

## 7. Import/Export To'liq

**Hozirgi holat:** `ExportFullBackup` / `ImportFullBackup` bor, ishlaydi.

**To'ldirishlar:**
- `~/Downloads/Telegram_AntiDelete/` ham backup ga kirsin (4-punkt hal qilgach bu yo'q bo'ladi — bitta joy)
- Toast matni: "Qayta ishga tushiring" → "Arxiv yangilandi"
- Import dan so'ng UI reload to'g'ri ishlashini tekshirish

---

## 8. Muammo va Bug tekshiruvlar

**`gDeletedCache` race condition:**
- `QMutex gCacheMutex` qo'shamiz `custom_db.cpp` ga
- `MarkDeleted`, `PermanentlyDeleteMessage`, `IsDeletedLocally`, `LoadRestoreCache` — hammasi mutex bilan

**Duplicate entries:**
- `MarkDeleted` da `INSERT OR IGNORE` + `UPDATE` atomik bo'lmagan → `INSERT OR REPLACE` ga o'tkazish yoki `BEGIN TRANSACTION` qo'shish

**`gPendingWrites` bo'sh flush:**
- `FlushPendingWrites` bo'sh bo'lganda ham timer ishlaydi → `if (gPendingWrites.empty()) return;` guard qo'shamiz

---

## Implement Tartibi

```
Sprint 1: ShouldAntiDelete/AntiEdit/Ghost helpers + Wiring (data_session, history_item, data_stories)
Sprint 2: Whitelist UI (native chat tanlash + manual ID)
Sprint 3: Media bitta joyga ko'chirish + foto saqlash hook
          [Sprint 4 ning cascade step 3 si Sprint 3 ga bog'liq — step 3 ishlamasa
           cascade step 5 (toast fallback) ishlaydi, hech narsa buzilamaydi]
Sprint 4: Forward kaskad (native → SendExisting → re-upload)
Sprint 5: Bug tuzatishlar (mutex, INSERT OR REPLACE, toast matni)
Sprint 6: Tekshiruvlar (Story anon, Import/Export, Last seen confirm)
```

---

## Fayllar

| Fayl | O'zgarish |
|---|---|
| `custom_settings.h/cpp` | ShouldAntiDelete/Edit/Ghost helpers, AddToWhitelist, GetWhitelist |
| `custom_db.h/cpp` | FindSavedMediaPath, QMutex, INSERT OR REPLACE |
| `data_session.cpp` | ShouldAntiDelete wiring |
| `history_item.cpp` | ShouldAntiEdit wiring, setDeletedLocally media path |
| `data/data_stories.cpp` | ShouldGhost wiring |
| `data/data_photo.cpp` | SaveMediaFile hook |
| `apiwrap.cpp` | Forward cascade |
| `settings/sections/settings_main.cpp` | Whitelist UI |
