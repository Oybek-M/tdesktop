# CustomMod — Keyingi Tasklar Ro'yxati

**Oxirgi yangilanish:** 2026-06-28 (NEXT-9 + NEXT-10 + T42 tugallandi)

---

## ✅ Tugallangan Tasklar

| # | Task | Fayl(lar) |
|---|---|---|
| T1 | Style file + CMakeLists + header skeleton | `style_custom_mod.h`, `CMakeLists.txt` |
| T2 | CustomTabBar widget | `custom_mod_window.cpp` |
| T3 | CustomModWindow klass + singleton | `custom_mod_window.cpp` |
| T4 | fillGeneralTab | `custom_mod_window.cpp` |
| T5 | fillPeerSection + fillPeersTab | `custom_mod_window.cpp` |
| T6 | fillArchiveTab + MakeWordDiff | `custom_mod_window.cpp` |
| T7 | fillAboutTab | `custom_mod_window.cpp` |
| T8 | settings_main.cpp — Customizations tugmasi | `settings/sections/settings_main.cpp` |
| T9 | Story anonim ko'rish — GhostMode dan ajratish | `data/data_stories.cpp`, `custom_settings.h/.cpp` |
| T10 | General tab — storyAnonymousView toggle + label tuzatmalar | `custom_mod_window.cpp` |
| T11 | Peers tab — real-time display fix, gInstance raise, label fix | `custom_mod_window.cpp` |
| T12 | Archive tab — onRefresh parametr, Yangilash tugmasi, label tuzatmalar | `custom_mod_window.cpp` |
| T13 | About tab — onArchiveChanged parametr, dinamik stats, real-time | `custom_mod_window.cpp` |
| T14 | settings_main.cpp — keywords kengaytirish (story, whitelist, blacklist...) | `settings/sections/settings_main.cpp` |
| T15 | Archive tab — peer nomi ko'rsatish (GetPeerDisplayName helper) | `custom_settings.h/.cpp`, `custom_mod_window.cpp` |
| T16 | Hidden panel resize fix — `_inners` array, showEvent, switchTab, update() | `custom_mod_window.cpp` |
| T17 | FlatLabel wrap fix — `customModHintLabel` style (minWidth:1px) | `custom_mod.style`, `custom_mod_window.cpp` |
| T18 | **NEXT-5: LayerManager** — "Chat tanlash" custom window ichida ochiladi | `custom_mod_window.cpp` |
| T19 | **NEXT-6: Per-Chat Settings** — har bir chat uchun Ghost/Delete/Edit toggle | `custom_settings.h/.cpp`, `custom_mod_window.cpp` |
| T20 | **NEXT-8: Real peer avatar** — `peerLoaded()` + `paintUserpic` + fallback | `custom_mod_window.cpp` |
| T21 | **Account limit unlock** — kMaxAccounts 3→100, kPremiumMaxAccounts 6→100 | `main/main_domain.h` |
| T22 | **Branding** — JSON orqali title + icon o'zgartirish | `custom_branding.h/.cpp`, `main_window.cpp`, `custom_mod_window.cpp`, `application.cpp`, `CMakeLists.txt` |
| T23 | **Branding qo'llanma** — user darajadagi qadam-baqadam yo'riqnoma | `DOCs/BRANDING_QOLLANMA.md` |
| T24 | **Branding UI** — General tab da Branding sektsiyasi (3 input + file picker + save) | `custom_branding.h/.cpp`, `custom_mod_window.cpp` |
| T25 | **Full Backup v2** — JSON sozlamalar + Registry + Manifest + auto-restart | `custom_db.cpp`, `custom_mod_window.cpp` |
| T26 | **Backup qo'llanma** — user darajadagi yo'riqnoma (laptop ko'chirish) | `DOCs/BACKUP_QOLLANMA.md` |
| T27 | **Background AntiEdit** — text_cache table + processMessages/updateEditedMessage hook | `custom_db.h/.cpp`, `data/data_session.cpp` |
| T28 | **Bug fix: restart dan keyin xabar yo'qoldi** — MarkDeleted ga text+date+isOut + non-channel item topilmasa cache fallback + cache yana AntiDelete uchun | `custom_db.h/.cpp`, `data/data_session.cpp` |
| T29 | **Bug fix: loadDeletedMessages isEmpty() skip** — chat ochilganda blocks bo'sh bo'lsa ham inject + date fallback + "(matn saqlanmagan)" placeholder | `history/history.cpp` |
| T30 | **Bug fix: "(matn saqlanmagan)" spam group chatlarda** — `msgDate==0` bo'lsa `MarkDeleted` chaqirmaslik (memory da yo'q xabarlar DB ga yozilmasin) | `data/data_session.cpp` |
| T31 | **Cache date fallback** — `GetCachedTextAndDate` qo'shildi: delete kelganda cache dan text + date birga olinadi, `msgDate==0` endi cache ni ham tekshiradi | `custom_db.h/.cpp`, `data/data_session.cpp` |
| T32 | **Bug fix: WhiteList-only caching** — `processMessages` cache hook da `ShouldAntiEdit\|\|ShouldAntiDelete` → `IsInWhitelist` ga almashtirish; global ON bo'lganda faqat WhiteList peerlar cache ga tushadi | `data/data_session.cpp` |
| T33 | **KRITIK: cache hook noto'g'ri joyda** — online real-time xabarlar (`updateNewMessage`/`updateShortMessage`) `processMessages` dan o'tmaydi. Hook `addNewMessage()` funnel ga ko'chirildi → barcha yo'llar qamrab olindi | `data/data_session.cpp` |
| T34 | **Bug fix: msg_id channel to'qnashuvi** — `TryRecordBackgroundDelete` `LIMIT 1` non-channel delete da kanal yozuvini olib soxta "O'CHIRILDI" ko'rsatardi. Endi channel peerlar (`(value>>48)&0xFF==2`) filtrlana­di | `custom_db.cpp` |
| T35 | **Bug fix: RecordBackgroundEdit T32 ni chetlab o'tardi** — `updateEditedMessage` guard `ShouldAntiEdit` → `ShouldBackgroundCache` | `data/data_session.cpp` |
| T36 | **Feature: haqiqiy yuboruvchi (sender_id)** — guruhda o'chirilgan xabar "guruh nomidan" emas, haqiqiy yuboruvchidan ko'rinadi. Schema v5: `sender_id` ustun | `custom_db.h/.cpp`, `data/data_session.cpp`, `history/history.cpp` |
| T37 | **Feature: Per-Chat + cache uyg'unligi** — `ShouldBackgroundCache` helper: WhiteList YOKI Per-Chat override (AntiDelete/AntiEdit) bo'lgan peerlar cache ga tushadi | `custom_settings.h/.cpp` |
| T38 | **Feature: media xabar background delete** — caption­siz media ham cache ga tushadi (`is_media` ustun, schema v5). O'chirilsa "(media xabar)" placeholder | `custom_db.h/.cpp`, `data/data_session.cpp`, `history/history.cpp` |
| T39 | **Bug fix: Archive tab media placeholder** — `GetAllDeletedMessages` + `DeletedMessageWithPeer` ga `is_media`/`sender_id`; Archive tab da fayl saqlanmagan media "(media xabar)" ko'rsatadi ("(empty)" emas) | `custom_db.h/.cpp`, `custom_mod_window.cpp` |
| T40 | **Bug fix: RecordBackgroundEdit sender/media yo'qotardi** — re-cache (`INSERT OR REPLACE`) avvalgi `sender_id`/`is_media` ni o'chirib, T36 ni edit→delete da buzardi. Endi `GetCachedTextAndDate` orqali o'qib, saqlab qoladi | `custom_db.cpp` |
| T41 | **Bug fix: timestamp overlap o'chirilgan xabarda** — `validateText()` guard'idagi `!isDeletedLocally()` har safar `setTextWithLinks` ni qayta chaqirib skip block ni o'chirardi → `10:22 PM` matn ustiga tushardi. Yangi `Flag::DeletedMarkerApplied (0x4000)`: marker bir marta qo'llanadi, keyin guard no-op → skip block saqlanadi | `history/view/history_view_element.h/.cpp` |
| T42 | **Feature: Category-based White/Black List** — Peer type bo'yicha guruh tanlash (Users/Groups/Channels). `PeerType` enum, `GetPeerType()`, `Is/SetWhitelistCategory()`, `Is/SetBlocklistCategory()`. `IsInWhitelist/IsInBlocklist` endi kategoriya ham tekshiradi. Storage: `peer_lists.json` — `wl_categories`/`bl_categories`. UI: `fillPeerSection` da 3 ta toggle. | `custom_settings.h/.cpp`, `custom_mod_window.cpp` |
| T43 | **NEXT-9: Per-Chat "Barchasini tozalash"** — Per-Chat State ga `entryWraps` qo'shildi. "Barchasini tozalash" tugmasi `ClearAllPerPeerOverrides()` chaqiradi. | `custom_settings.h/.cpp`, `custom_mod_window.cpp` |
| T44 | **NEXT-10: Background AntiDelete — msgDate==0 WhiteList fallback** — WhiteList peerda date yo'q bo'lsa joriy vaqt + "(matn saqlanmagan)" placeholder. Faqat WhiteList peerlar uchun (Global ON bo'lsa skip). | `data/data_session.cpp` |

---

## 🔜 Keyingi Tasklar (Navbat tartibida)

### NEXT-7: Runtime test (HIGH)
- **Online real-time delete (T33)** — chat ochiq EMAS, ilova online, WhiteList chatdan xabar kelib o'chirilsa saqlanadimi? (eng muhim — T33 dan oldin ishlamasdi)
- Background AntiDelete: chat ochmasdan xabar kelib o'chirilganda saqlanadimi? (T28+T31)
- "(matn saqlanmagan)" spam yo'qoldimi group chatlarda? (T30)
- WhiteList-only caching: BlackList/global peerlardan xabar cache ga tushmaydimi? (T32)
- **Channel to'qnashuvi (T34)** — bir xil msg_id li kanal+user yozuvida soxta "O'CHIRILDI" chiqmaydimi?
- **Sender_id (T36)** — guruhda o'chirilgan xabar haqiqiy yuboruvchidan ko'rinadimi?
- **Per-Chat cache (T37)** — WhiteList'da yo'q, lekin Per-Chat AntiDelete yoqilgan peer cache ga tushadimi?
- **Media delete (T38)** — captionsiz rasm/video o'chirilsa "(media xabar)" ko'rinadimi?
- **Schema v5 migration** — eski DB (v4) ochilganda `sender_id`/`is_media` ustunlari muammosiz qo'shiladimi?
- Branding UI: title + icon saqlash/yuklash ishlayaptimi? (T24)
- Import/Export: laptop ko'chirish ssenariysini to'liq test (T25)
- Restart dan keyin per-peer settings saqlanadimi? (T19)
- **Timestamp overlap (T41)** — o'chirilgan xabarda `10:22 PM` endi matn ustiga tushmayaptimiw?

### ~~NEXT-8: Real peer avatar~~ ✅ T20 da tugadi

### ~~NEXT-9: Per-Chat "Barchasini tozalash"~~ ✅ T43 da tugadi

### ~~NEXT-10: Background AntiDelete — msgDate==0 fallback~~ ✅ T44 da tugadi

---

## Muhim Eslatmalar

- **Build va Git** — foydalanuvchi o'zi qiladi, Claude teginmaydi
- **Javoblar** — O'zbek lotin tilida
- **MEMORY.md** — kod holati + bug fix tarixi, `DOCs/MEMORY.md`
- **PRD.md** — to'liq feature talablari + known limitations, `DOCs/PRD.md`
- **Model almashtirish** — yangi session da: MEMORY.md → PRD.md → NEXT_TASKS.md o'qi
