# CustomMod — Keyingi Tasklar Ro'yxati

**Oxirgi yangilanish:** 2026-09-27 (sync scope/edit_date kodi build kutmoqda; upstream v7.2.9 chiqqan; A22 ochiq)

---

## 🟡 2026-09-27 holati — keyingi sessiya shu yerdan boshlaydi

**Upstream:** rasmiy `telegramdesktop/tdesktop` da **v7.2.8 va v7.2.9**
chiqqan, bizda **7.2.7** (oxirgi merge `9b081b2a43`, 2026-09-10).
Repo'da `upstream` remote YO'Q — tekshirish faqat o'qiydi:
`git ls-remote --tags --refs https://github.com/telegramdesktop/tdesktop.git 'v7.*'`

**Build QILINMAGAN kod (09-27, push qilingan):**

| Commit | Nima |
|---|---|
| `a6f0874e77` | docs(sync): `edited` uchun `edit_date`, scope setting kalitlari protokoli |
| `d4cc460156` | `edited` xabarlar `edit_date` bilan yoziladi, fon tahrir nomuvofiqligi tuzatildi |
| `7b6079f81d` | scope sozlamalari (WL/BL, AntiDelete/AntiEdit) outbox'ga chiqadi va qabul qilinadi |
| `a06ed12373` | docs(sync): activity `msg_id = DiscriminatorFor(field)`, `status` kodlashi (§3.2.2, `userStatusEmpty -> long_ago`), test vektorlari |
| `a119ef83cb` | activity kuzatuv sozlamalari (`scope.activity_*`) outbox'ga chiqadi va qabul qilinadi |

**Tartib (kelishilgan):** laptopdagi ish tugab push qilinguncha PC'da
tdesktop kodiga tegilmaydi -> **upstream v7.2.8+v7.2.9 merge** -> laptop
build (yuqoridagi commitlar ham shunga kiradi) -> sinov -> A22 -> A21 ->
A7 reliz (avval VPS mirrorlari).

**PC holati:** E: diski almashtirildi; D: da **349 pending sektor**
topildi — muhim ma'lumotlar yangi E: ga nusxalanadi. Batafsil:
`PC-SETUP-STATE.md` §2026-09-27.

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

## 🔴 A24 — Scope `setting` qo'llanishidagi ikki kamchilik (2026-09-29)

**Holat:** ochiq, kichik, disk ishlaridan keyin asosiy ish bilan birga.
Spec §3.2.1a (qoida) va CHANGELOG 2026-09-29.

1. **`ApplyScopeSetting` `occurred_at` ni solishtirmaydi** (`custom_sync_client.cpp`,
   `Kind::Setting` shoxobchasi) — pull tartibida oxirgisi g'olib, eskirgan qiymat
   yangisini bosishi mumkin. Tuzatish: har `key` uchun oxirgi qo'llangan
   `occurred_at` (va teng bo'lsa `record_id`) ni saqlash (`sync_state` yoki alohida
   jadval), kichigini tashlab yuborish. Lokal o'zgarish ham shu vaqtni yangilasin.
2. **`gActiveAccountId` = oxirgi yaratilgan sessiya** (`main_session.cpp:203`),
   ekrandagi akkaunt emas. Global semantika tufayli ma'lumot yo'qolmaydi, lekin
   `account_id` ma'nosiz. Tuzatish: aktiv akkaunt o'zgarishiga obuna bo'lish
   (`Core::App().domain().activeChanges()`) yoki sozlama yozuvini hamma kirgan
   akkauntlar uchun yuborish.

---

## 🔴 A23 — Arxiv papkasi sifatida disk ildizini tanlashga ruxsat bor (2026-09-29)

**Holat:** ochiq, kod o'zgarishi kichik, PRIORITET O'RTA (ma'lumot xavfi).

2026-09-29 da PC'da arxiv "📁 Papkani o'zgartirish" orqali ko'chirilganda
foydalanuvchi papka o'rniga `E:\` ni tanladi -> `archiveRootPath = E:/`,
arxiv (`medias`, `db`, ...) disk ildiziga tushdi. Ko'chirishning o'zi
to'g'ri o'tdi, lekin:

- `MoveTree(from, to)` (`custom_settings.cpp`) manba papkadagi **HAMMA**
  narsani ko'chiradi. Ildiz arxiv bo'lsa, keyingi "Ko'chirish" butun
  diskni (boshqa papkalar, `System Volume Information`, `$RECYCLE.BIN`)
  yangi papkaga sudraydi.

**Tuzatish:** `custom_tab_storage.cpp` dagi tanlovdan keyin
`QDir(chosen).isRoot()` bo'lsa rad etish yoki avtomatik
`<chosen>/customizationMainFolder` ga aylantirish; `MoveTree` ga faqat
ma'lum arxiv papkalarini (`medias`, `db`, `config`, `backups`,
`bombmedia`) ko'chirish cheklovi.

PC'da qo'lda tuzatildi: papkalar `E:\customizationMainFolder` ga
ko'chirildi, reestr `E:/customizationMainFolder`.

---

## 🔴 Ochiq Tasklar — 2026-09-26

### A22 — Tiklangan xabarlar tartibidagi qoldiq chalkashlik

**Holat:** ochiq, PRIORITET PAST (resurs bo'lganda ko'riladi).

T47 tuzatishidan keyin qo'lda sinov **90% mos** keldi: `7053823996` chatida
iyul-avgust o'chirilgan xabarlari chiqdi va boshqa chatga o'tib qaytilganda
joyida qoldi. Qolgan muammolar:

1. **Ba'zan tartib hamon chalkash.**
2. **Ba'zida foydalanuvchi o'zi yozgan xabarlar qolib ketadi.**

**MUHIM — log dalili (taxmin qilmaslik uchun):** 14:42 seansida log'da
`reinserted` va `window` sonlari **0 bo'lib qoldi**, ya'ni T47 tuzatishi
bu seansda **umuman ishga tushmadi**. Chat ochilganda `clear(Unload)`
chaqirilmagan (inject qilingan xabarning `mainView()` bor edi, shuning
uchun `isReadyFor()` true qaytargan) va bloklar qayta qurilmagan.
Demak sinovdagi yaxshilanishni T47 ga bog'lab bo'lmaydi va qoldiq
chalkashlik dastlabki nuqsonning o'zi bo'lishi ehtimoli yuqori.

**Tekshirishni qaydan boshlash kerak:**
- `History::insertMessageToBlocks()` — SANA bo'yicha qo'yadi, Telegram esa
  bloklarni msg_id tartibida saqlaydi. Sana va ID bir-biriga mos kelmasa
  (ayniqsa `msg_date` 0 bo'lib `currentSecsSinceEpoch()` fallback ishlaganda)
  tartib buziladi. `actioned_messages` da `msg_date = 0` bo'lgan yozuvlarni
  sanab ko'rish kerak.
- `is_out = 1` yozuvlar: `fromId = session().userPeerId()` yo'li
  (`history.cpp`, inject bloki) — "o'zim yozgan xabarlar qolib ketadi"
  shikoyati shu shoxobcha bilan bog'liq bo'lishi mumkin.
- `skipped: empty 36` — shu chatda 36 yozuv mazmunsiz deb tashlangan;
  ular yo'qolgan xabarlarning bir qismi bo'lishi mumkin.

**Sinov usuli:** log'da `reinserted`/`window` sonlarini kuzatish. Ular 0
bo'lsa tuzatish yo'li umuman bosilmagan degani.

---

## Muhim Eslatmalar

- **Build** — 2026-09-26 dan Claude o'zi qiladi, FAQAT laptopda
- **Git** — commit+push ruxsati bor, FAQAT `origin/Oybek`; `upstream` ga hech qachon; `Co-Authored-By` qabul emas
- **Javoblar** — O'zbek lotin tilida
- **MEMORY.md** — kod holati + bug fix tarixi, `DOCs/MEMORY.md`
- **PRD.md** — to'liq feature talablari + known limitations, `DOCs/PRD.md`
- **Model almashtirish** — yangi session da: MEMORY.md → PRD.md → NEXT_TASKS.md o'qi
