# CustomMod — Product Requirements Document (PRD)

**Loyiha:** Telegram Desktop uchun maxsus modifikatsiya  
**Muallif:** Oybek  
**Oxirgi yangilanish:** 2026-06-10 (T33–T38, schema v5)

---

## 1. Maqsad

Telegram Desktop dasturini maxsus funksiyalar bilan kengaytirish:
- Maxfiylik (Ghost Mode, anonim story ko'rish)
- Xabar arxivi (o'chirilgan, tahrirlangan xabarlar — background da ham)
- Peer-based boshqaruv (White/Black list, Per-Chat settings)
- Zaxira nusxa tizimi (laptop ko'chirishni qo'llab-quvvatlash)
- UI orqali real-time boshqaruv
- Branding (nom va icon o'zgartirish)

---

## 2. Asosiy Funksiyalar (Features)

### F1 — Ghost Mode
- **Nima qiladi:** Online holat, yozmoqda belgisi va "xabar o'qildi" bildirishnomasini yashiradi
- **Qayerda:** `apiwrap.cpp` — `sendReadRequest`, `sendBotReadRequest`
- **API:** `CustomSettings::ShouldGhost(peerId)`
- **UI toggle:** General Tab → "Privacy & Ghost Mode"

### F2 — Story Anonim Ko'rish (GhostMode dan alohida)
- **Nima qiladi:** Hikoyalarni ko'rganingiz egasiga bildirilmaydi
- **Qayerda:** `data/data_stories.cpp` — 3 joy
- **API:** `CustomSettings::ShouldAnonymousStory(peerId)`
- **UI toggle:** General Tab → "Hikoyalarni anonim ko'rish"
- **Muhim:** GhostMode dan mustaqil

### F3 — Anti-Delete
- **Nima qiladi:** O'chirilgan xabarlarni arxivda saqlaydi, ko'rinishda qoldiradi
- **Qayerda:** `history/history_item.cpp`, `data/data_session.cpp`
- **API:** `CustomSettings::ShouldAntiDelete(peerId)`
- **Storage:** SQLite `actioned_messages` jadvali
- **Background:** Chat ochilmagan bo'lsa ham ishlaydi (T27+T28+T31)

### F4 — Anti-Edit
- **Nima qiladi:** Tahrirlangan xabarning avvalgi matnini saqlaydi
- **Qayerda:** `history/history_item.cpp`, `data/data_session.cpp`
- **API:** `CustomSettings::ShouldAntiEdit(peerId)`
- **Storage:** SQLite `actioned_messages` jadvali
- **Background:** Chat ochilmagan bo'lsa ham ishlaydi (T27)

### F5 — Bypass Restrictions
- **Nima qiladi:** Cheklangan chatlarda nusxalash va yuborish imkonini beradi
- **API:** `CustomSettings::BypassRestrictions()`

### F6 — Spoof Mobile
- **Nima qiladi:** Mobil qurilmadan ishlatilayotgandek ko'rinadi
- **API:** `CustomSettings::SpoofMobile()`

### F7 — Offline DB
- **Nima qiladi:** Xabarlar va medialarni qurilmada saqlaydi
- **API:** `CustomSettings::OfflineDb()`

### F8 — White List
- **Nima qiladi:** Bu ro'yxatdagi chatlar uchun barcha funksiyalar DOIM yoqiq
- **Priority:** Blocklist dan past, Global flagdan yuqori
- **Storage:** `<AppData>/CustomMod/peer_lists.json`

### F9 — Black List
- **Nima qiladi:** Bu ro'yxatdagi chatlar uchun barcha funksiyalar DOIM o'chirilgan
- **Priority:** ENG YUQORI — barcha narsani bekor qiladi
- **Storage:** `<AppData>/CustomMod/peer_lists.json`

### F10 — Archive Tab
- **Nima qiladi:** O'chirilgan va tahrirlangan xabarlarni ko'rsatadi
- **Yangilash:** "🔄 Yangilash" tugmasi bosilganda tab qayta quriladi

### F11 — Zaxira Nusxa (Backup/Restore v2)
- **Export:** DB + media + peer_lists + branding + registry + manifest
- **Import:** To'liq tiklash + 3 soniyadan keyin auto-restart
- **Qayerda:** `custom_db.cpp` — `ExportFullBackup`, `ImportFullBackup`
- **Maqsad:** Laptop ko'chirishda barcha ma'lumotlarni xavfsiz o'tkazish

### F12 — LayerManager (T18)
- **Nima qiladi:** "Chat tanlash" dialog Custom Window ICHIDA ochiladi
- **Qayerda:** `CustomModWindow::showBox()`, `_layerManager`

### F13 — Per-Chat Settings (T19)
- **Nima qiladi:** Har bir chat uchun Ghost / Anti-Delete / Anti-Edit alohida ON/OFF
- **Qayerda:** Peers tab oxirida "Per-Chat Sozlamalar" section
- **Priority:** Blocklist > Whitelist > Per-peer override > Global
- **Storage:** Registry groups: `GhostModePerPeer`, `AntiDeletePerPeer`, `AntiEditPerPeer`, `PerPeerNames`

### F14 — Real Peer Avatar (T20)
- **Nima qiladi:** Peers va Per-Chat panellarida haqiqiy avatar ko'rsatadi
- **Qayerda:** `custom_mod_window.cpp` — `PaintPeerAvatar()` helper
- **Fallback:** Harf + rang (peer session da yuklanmagan bo'lsa)

### F15 — Account Limit Unlock (T21)
- **Nima qiladi:** Bir vaqtda 100 ta account ishlata olish
- **Qayerda:** `main/main_domain.h` — `kMaxAccounts = 100`, `kPremiumMaxAccounts = 100`

### F16 — Branding (T22–T24)
- **Nima qiladi:** Window title va icon ni JSON orqali o'zgartirish
- **Qayerda:** `custom_branding.h/.cpp`, `application.cpp`, `main_window.cpp`
- **UI:** General Tab → "Branding" section (3 input + file picker + save)
- **Storage:** `%AppData%/CustomMod/branding.json`

### F17 — Background AntiEdit (T27+T33+T35+T40)
- **Nima qiladi:** Chat ochilmagan/ilova online bo'lsa ham edit larni qayd qiladi
- **Mexanizm:**
  1. `addNewMessage()` (T33 — barcha yo'llar markazi) → xabar text_cache ga yoziladi (agar `ShouldBackgroundCache`)
  2. `updateEditedMessage()` → existing==nullptr → guard `ShouldBackgroundCache` (T35) → `RecordBackgroundEdit()` → cache dan eski matn olinib, actioned_messages ga yoziladi
  3. `RecordBackgroundEdit` re-cache da `GetCachedTextAndDate` orqali sender/media saqlanadi — T40
- **Storage:** SQLite `text_cache` jadvali

### F18 — Background AntiDelete (T28+T30+T31+T33+T34+T36+T38+T39)
- **Nima qiladi:** Chat ochilmagan/ilova online bo'lsa ham delete larni qayd qiladi
- **Mexanizm:**
  1. Xabar kelganda → `addNewMessage()` (T33) → `text_cache` ga yoziladi (`ShouldBackgroundCache` bo'lsa); captionsiz media ham `is_media` bilan (T38)
  2. Delete update kelganda → memory da yo'q → `GetCachedTextAndDate()` → text+date+sender+media cache dan
  3. Non-channel delete da `TryRecordBackgroundDelete` kanal yozuvlarini filtrlaydi (T34)
  4. `msgDate > 0` bo'lsa → `MarkDeleted()` (sender_id + is_media bilan) → actioned_messages ga
  5. `msgDate == 0` (hech qachon yetib kelmagan) → skip (spam yo'q)
- **Ko'rsatish:** guruhda haqiqiy yuboruvchidan ko'rinadi (T36 `sender_id`); media → "(media xabar)" (T38/T39); Archive tab da ham media to'g'ri (T39)
- **Cheklov:** Faqat WhiteList YOKI Per-Chat AntiDelete/AntiEdit override bo'lgan peerlar (T37 `ShouldBackgroundCache`)

---

## 3. Priority Zanjiri (Should* helpers)

```
Blocklist (false) → Whitelist (true) → Per-peer override → Global flag
```

Per-peer override faqat Per-Chat Settings paneli orqali qo'shilgan chatlar uchun ishlaydi.

---

## 4. UI Tuzilmasi — Custom Window

```
CustomModWindow (Ui::RpWidget, standalone window)
├── CustomTabBar: [General | Peers | Archive | About]
├── Panel 0: fillGeneralTab
│   ├── "Privacy & Ghost Mode"
│   │   ├── Ghost Mode (toggle)
│   │   ├── Hikoyalarni anonim ko'rish (toggle)
│   │   └── Mobil qurilma ko'rinishi (toggle)
│   ├── "Cheklovlar"
│   │   └── Cheklangan chatda nusxalash va yuborish (toggle)
│   ├── "Xabarlar tarixi"
│   │   ├── Anti-Delete (toggle)
│   │   ├── Anti-Edit (toggle)
│   │   └── Offline xabar bazasi (toggle)
│   └── "Branding" (T24)
│       ├── Window title input
│       ├── Mod window title input
│       ├── Icon path input + file picker + clear
│       └── Saqlash tugmasi + Toast
├── Panel 1: fillPeersTab
│   ├── White List section
│   │   ├── "Chat tanlash" button (window ichida ochiladi)
│   │   ├── ID orqali qo'shish (input + button)
│   │   ├── "Barchasini tozalash" button
│   │   └── Entry rows (avatar + nom + ID + O'chirish)
│   ├── Black List section (xuddi shunday)
│   └── Per-Chat Sozlamalar section
│       ├── "Chat tanlash — Per-Chat" button
│       ├── Count label + empty wrap
│       └── Entry: header (avatar+nom+ID+delete) + Ghost/Delete/Edit toggle
├── Panel 2: fillArchiveTab (rebuild-able)
│   ├── "🔄 Yangilash" button
│   ├── 🗑 O'chirilgan xabarlar section
│   └── ✏ Tahrirlangan xabarlar section
└── Panel 3: fillAboutTab
    ├── Arxiv holati (dinamik, real-time yangilanadi)
    ├── 📦 Zaxira nusxa (Export/Import — 7 element)
    ├── 📊 Arxiv boshqaruvi
    └── ⚠️ XAVFLI HUDUD — tozalash tugmalari
```

---

## 5. Storage

| Ma'lumot | Joy | Format |
|---|---|---|
| Global settings | Registry: `HKCU\Software\CustomMod\TelegramDesktop` | QSettings |
| White/Black list | `<AppData>/CustomMod/peer_lists.json` | JSON |
| Per-peer ghost override | Registry: `...\GhostModePerPeer\` | QSettings group |
| Per-peer anti-delete | Registry: `...\AntiDeletePerPeer\` | QSettings group |
| Per-peer anti-edit | Registry: `...\AntiEditPerPeer\` | QSettings group |
| Per-Chat names | Registry: `...\PerPeerNames\` | QSettings group |
| O'chirilgan/Tahrirlangan xabarlar | SQLite `actioned_messages` | custom_db |
| Xabar text cache | SQLite `text_cache` | custom_db |
| Media fayllar | `<AppData>/CustomMod/BombMedia/` | File system |
| Branding | `<AppData>/CustomMod/branding.json` | JSON |
| Window geometry | Registry | QSettings |

---

## 6. Pending / Keyingi Tasklar

### NEXT-7: Runtime test (HIGH)
- Barcha T18–T31 o'zgarishlarni build qilib tekshirish
- Background AntiDelete: chat ochmasdan xabar kelganda va o'chirilganda ishlayaptimiyu?
- Branding UI: title va icon o'zgartirish ishlayaptimi?
- Import/Export: laptop ko'chirish ssenariysini to'liq test qilish

### NEXT-9: Per-Chat "Barchasini tozalash" tugmasi (OPTIONAL)
- White/Black list da bor, Per-Chat da yo'q
- Foydalanuvchi 10+ chat qo'shsa, har birini alohida o'chirish noqulay

### NEXT-10: Background AntiDelete — cache da date saqlanmagan eski xabarlar (OPTIONAL)
- Hozir: `msgDate == 0` bo'lsa skip (spam yo'q)
- Yaxshilash: agar date yo'q bo'lsa ham, xabar o'chirilganini alohida jadvalda qayd qilish imkoniyati
- Limit: faqat SelectedList da bo'lsa

---

## 7. Known Limitations

1. **JSON real-time** — faqat UI orqali o'zgartirilganda real-time; fayl tashqaridan edit qilinsa restart kerak
2. **Registry path:** `HKCU\Software\CustomMod\TelegramDesktop` — App ID o'zgartirilsa barcha sozlamalar yo'qoladi
3. **Background AntiDelete/AntiEdit** — faqat WhiteList YOKI Per-Chat override (AntiDelete/AntiEdit) bo'lgan peerlar uchun (T37 `ShouldBackgroundCache`)
4. **text_cache da date 0** bo'lsa (xabar umuman yetib kelmagan — ilova o'chiq edi) — delete ko'rinmaydi. Media (captionsiz) endi `is_media` bilan saqlanadi (T38)
5. **sender_id eski yozuvlarda yo'q** — v5 dan oldin saqlangan o'chirilgan guruh xabarlari hali ham "guruh nomidan" ko'rinadi (faqat yangi yozuvlar to'g'ri)
6. **Per-Chat "Barchasini tozalash"** — yo'q (NEXT-9 da rejalashtirilgan)
7. **Avatar** — session da yuklanmagan peerlar uchun harf+rang fallback (haqiqiy rasm bor peerlar uchun to'g'ri ishlaydi)
