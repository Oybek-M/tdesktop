# CustomMod — Keyingi Tasklar Ro'yxati

**Oxirgi yangilanish:** 2026-10-13 23:45 (o'rnatilgan: `df785ce63a` = A22 A+C, 23:08; A7 = "canary #N" relizlari — kod tayyor, build CustomSync TDLib build'idan KEYIN)

---

## 🟡 2026-10-13 holati — keyingi sessiya shu yerdan boshlaydi

**🆕 23:45 (eng yangi):**
- `df785ce63a` (A22 A+C) 23:08 da O'RNATILDI. Zaxiralar:
  `Release\_tdata-backup-pre-df785ce63a-20261013-20261013-230809`,
  `db\pre-df785ce63a-20261013-20261013-230809`. 23:11 startida bitta chatda
  97 fayl bog'landi + 2 belgi, boshqasida 5 belgi. Start bloki 3780 ms
  bo'ldi (exe almashgandan keyingi birinchi start, Docker yuklamasi
  ostida), iliq startni qayta o'lchash kerak.
- A7: foydalanuvchi "alpha N" ni tanladi. 7.3.x da alpha V2 bilan
  ishlamaydi, uning o'rinbosari — **canary #N**. Kod, manifest v2 va
  `publish.ps1` TAYYOR, dry-run O'TDI. Batafsil pastdagi A7 bo'limida.
- ⚠️ CustomSync: Docker WSL VM yarata olmayapti (HCS
  ERROR_NO_SYSTEM_RESOURCES), shuning uchun Windows qayta yuklanadi. TDLib
  build reboot'dan KEYIN boshlanadi. U tugaguncha tdesktop build YO'Q.

**Upstream:** v7.3.0+v7.3.1 merge qilindi (`9a730f3c41`, 866 commit, T50)
va `origin/Oybek` ga push qilindi. Yangi reliz bor-yo'qligini tekshirish
(repo'da `upstream` remote YO'Q — faqat o'qiydi):
`git ls-remote --tags --refs https://github.com/telegramdesktop/tdesktop.git 'v7.*'`
Merge'da `Telegram/Resources/update/*` (yangilanish kalitlari) — **DOIM
bizniki** (A27).

**✅ `9a730f3c41` o'rnatildi va sinovdan O'TDI** (T50, 2026-10-13 01:07,
`install-staged.sh`). Nusxa `C:\TBuild\staged\9a730f3c41-20261013\` (exe
`7.3.1.0`, 255 MB). Zaxiralar:
`Release\_tdata-backup-pre-9a730f3c41-20261013-20261013-010713` (107 MB),
`db\pre-9a730f3c41-20261013-20261013-010713` (61 MB), eski exe
`Telegram.exe.pre-9a730f3c41-20261013-20261013-010713`.

| Sinov | Natija |
|---|---|
| Start bloki | 1-start 2891 ms (sovuq), 2-start (iliq) **1025 ms** — 7.2.10: 846 ms, 7.2.7 bazasi 1380-1774 ms; bitta o'lchov, keyingi startlarda qayta ko'rish |
| Umumiy ishlash | Foydalanuvchi tekshirdi: ishlayapti |

Start sinovi loglari SAQLANMAGAN: kuzatuvchi
(`C:\TBuild\watch-starttest-v731.sh`) `starttest-20261013` papkasini
yaratmagan (`mkdir -p` yo'q) va nusxalar jimgina yozilmagan — raqamlar
kuzatuvchining stdout'idan. Keyingi kuzatuvchilarda papkani oldindan
yaratish kerak. Build saboqlari (prepare'ni Git
Bash'dan chaqirish, ucrt64 PATH, LNK1102/LNK1180, C: tozalash) — `MEMORY.md`
T50.

**✅ `f829aa2e78` O'RNATILDI (2026-10-13 07:55:44) va start sinovi O'TDI:**
zaxiralar `Release\_tdata-backup-pre-f829aa2e78-20261013-20261013-075544`
(108 MB), `db\pre-f829aa2e78-20261013-20261013-075544` (61 MB). Log nusxasi
`C:\TBuild\starttest-20261013-f829\`. 1-start (exe almashgandan keyin):
blok 2858 ms (7.3.1 ning 1-starti 2891 ms — regressiya emas); chatlar +5/+11 s;
**`start tinchidi` start+20 258 ms** (MediaQuotaScan, GlobalMaintenance x2 —
ikkinchisi bayroq tufayli bo'sh), `ActivityCacheLoad` **+21 s (ilgari +94 s)**,
ReconcileMediaIndex 669 ms fon oqimida, keyin "main thread blocked" YO'Q.
CustomSync qatori yo'q — laptopda `syncEnabled=false` (kutilgan). A22: start
paytidagi 20 ta inject log'ida `order breaks 0`. PC — hali qo'lda o'rnatilmagan.

(Avvalgi holat:) **O'RNATISH UCHUN TAYYOR: `f829aa2e78`** (2026-10-13 03:42, BUILD O'TDI,
SINOVDAN O'TMAGAN) = A27 (o'z V2 kalitlar, `/secure/v2`) + A26 (start
tinchligi signali) + A22 diagnostikasi (`order breaks`). Nusxa
`C:\TBuild\staged\f829aa2e78-20261013\` (SHA256 `e4123aaf0a5dcb39...`),
`install-staged.sh --check` O'TDI (faqat Telegram ochiq edi). Statik
sinov: exe ichida faqat bizning V2 kalitlar, `/secure/v2`, yangi log
satrlari bor. Oraliq nusxalar: `7ece589feb-20261013` (faqat A27),
`dc9c3251cb-20261013` (A27+A26).
**O'rnatish:** foydalanuvchi Telegram'ni TO'LIQ yopadi ->
`bash /c/TBuild/install-staged.sh f829aa2e78-20261013`; PC — qo'lda.
**Sinov (o'rnatgandan keyin):** log'da `start tinchidi -> GlobalMaintenance
/ MediaQuotaScan / CustomSync` ~+20-30 s da; `ActivityCacheLoad` shu
atrofda; "main thread blocked" startdagidan ko'paymagan; chat ochilganda
inject log'ida `order breaks` soni (A22). Self-update to'liq sikli — A7 da.

**⚠️ Build vaqti cheklovi (CustomSync sessiyasidan, 2026-10-13):** bugun
kechqurun foydalanuvchi laptop'da TDLib Docker build'ini ishga tushiradi
(~40-60 daqiqa, ~6 GB RAM) — shu paytda tdesktop build BOSHLANMAYDI.
Boshlanish/tugashni CustomSync sessiyasi xabar qiladi.

**Keyingi tartib:** A26 (start'dagi 90 s kutish; BUILD O'TDI, o'rnatish+sinov
kutilmoqda — `f829aa2e78` da A27 bilan birga) -> A22 -> A21 -> A7 reliz

**A21 holati (2026-10-13 tekshirildi):** Task C UI'si (`7896c40ccf` +
`1c6202bf5b`, Ombor -> "👤 Egasi noma'lum yozuvlar") o'rnatilgan build'da BOR.
Kod ishi qolmagan — qolgani FOYDALANUVCHI qarori: qaysi chat qaysi
akkauntniki. Baza nusxasida (10-13): 30 noaniq peer / 11 173 qator (eng
kattasi `562952328991389` — 6 253); `account_id = 0` jami 14 944 (backup
14 110, deleted 833). Tavsiya: avval kichik peer'da sinash (spec §7).
Tafsilot: `docs/superpowers/specs/2026-09-05-a21-false-positive-deletions.md`.
(avval VPS mirrorlari; A7 dan OLDIN foydalanuvchi
`DesktopPrivate\update-v2\` dagi maxfiy kalitlarni oflayn zaxiralaydi — ✅ 10-13 bajarildi).

**PC holati (09-30):** disk ishlari YOPILDI. E: yangi Toshiba (toza), D:
pending 349 -> 0 (yumshoq xato, sog'lom), arxiv `E:\customizationMainFolder`.
Qolgan (tdesktop'ga bog'liq emas): RAM sinovi, D: -> E: ko'chirish
(foydalanuvchi aytganda). Batafsil: `PC-SETUP-STATE.md` §2026-09-30.

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

## 🟡 A7 — O'z relizlarimiz "canary #N" + self-update to'liq sikli (2026-10-13)

**Qaror (foydalanuvchi, 10-13):** asosiy versiya upstream bilan bir xil
qoladi, relizlar "alpha N" ko'rinishida chiqadi. Iloji bo'lmasa — oxirgi
chora asosiy versiyani oshirish. Hammaga yuboriladi; eski
versiyadagilarga 1 marta qo'lda o'rnatiladi.

**Topilma:** 7.3.x da eski alpha V2 bilan ISHLAMAYDI:
- Packer `-channel` ni `-alpha` bilan qabul qilmaydi;
- launcher alpha'da bo'sh kalit bilan crash bo'ladi va ma'lumotni
  `TelegramForcePortable` ga ko'chiradi.

V2 dagi o'rinbosari — **canary-public #N** (`7.3.1 canary #N`). U
tanlandi, shuning uchun asosiy versiyani oshirish SHART EMAS va
`custom_upstream_checker` o'zgarmaydi.

**Holat:** KOD TAYYOR (10-13): `custom_release.h`, `HttpChecker` canary
feed'i, manifest v2 (`canary-public`) va `publish.ps1`. Dry-run O'TDI
(Packer `td-update-win-x64-7003001-canary-1` ni imzoladi va o'zi
tekshirdi). Qolgani:
1. Build (CustomSync TDLib build'i tugagach), keyin laptop'ga o'rnatish.
   Canary'ga o'tishda Telegram target'i to'liq qayta kompilyatsiya
   BO'LMAYDI: hisoblagich sarlavha faylida.
2. VPS: `/secure/v2` + reliz SSH kaliti (CustomSync sessiyasi) va yarim
   yuklangan 7.2.7 ni tekshirish.
3. PC va akamga #1 ni 1 marta qo'lda o'rnatish.
4. e2e: `kCounter` = 2 -> build -> `release.ps1` (VPS/GitHub'ga chiqarish
   uchun foydalanuvchi ruxsati kerak) -> laptop #1 dan #2 ga o'zi
   yangilanadi.

Tafsilot: `docs/self-update/a7-canary-releases.md`.

## 🟡 A27 — Self-update'ni 7.3.x imzo sxemasiga ko'chirish (2026-10-11; A7 relizidan OLDIN SHART)

**KOD TAYYOR, BUILD O'TDI (2026-10-13, `7ece589feb`):** o'z Ed25519
kalitlarimiz, prefiks `/secure/v2`, `publish.ps1` V2. Qolgani: o'rnatish
(laptop + PC) va to'liq sikl sinovi A7 da. Dizayn, progress va sinov
natijalari — `docs/self-update/a27-v2-signing.md`.

Upstream 7.3.0 yangilanish paketlarini tekshirishni `core/update_keys.cpp`
(`RootPublicKeyPem()`) va `core/update_verify` (`Updates::VerifyUpdate`)
ga ko'chirdi, packer V2 formatiga o'tdi, v1 paketlarni ochish olib
tashlandi. Merge'da `config.h` dagi bizning `UpdatesPublicKey` blokimiz olib
tashlandi (kalit `Telegram/build/_other/packer.cpp` va git tarixida qoldi).
Natija: bizning RSA kalit bilan imzolangan self-update paketlari endi
tekshiruvdan O'TMAYDI.

Hozircha xavfsiz: yangilanish manbai faqat VPS mirror, MtpChecker
o'chirilgan (`core/update_checker.cpp`).

Qilish kerak: yangi sxemani o'rganib, o'z kalitimiz bilan moslash (maxfiy
kalit repo'ga TUSHMAYDI), packer'ni V2 formatiga o'tkazish, VPS mirror'dagi
paket formatini yangilash va test nusxada to'liq yangilanish siklini sinash.
Aniq dizayn — alohida reja.

---

## 🟡 A26 — Start'dagi qat'iy 90 s kutishni "UI bo'shadi" signaliga almashtirish (2026-10-08, g'oya)

Hozir start'dan keyin fon ishlari qat'iy 90 s kutadi: `kStartupQuietMs`
(`main_session.cpp:86`) va `kStartupScanDelayMs` (`custom_media_quota.cpp:23`)
— start paytidagi SQLite raqobatining oldini olish uchun. Foydalanuvchi
(10-07): shu 90 s davomida CustomMod servislari umuman ishlamaydi —
qisqartirish mumkinmi?

**G'oya:** qat'iy vaqt o'rniga "UI bo'shadi" signali (masalan, birinchi
chat ro'yxati chizilgandan keyin asosiy oqim bir necha soniya band
bo'lmasa); 90 s faqat yuqori chegara bo'lib qoladi.

**🟡 BUILD O'TDI (`dc9c3251cb`, 03:32; o'rnatiladigan nusxa `f829aa2e78`), SINOVDAN O'TMAGAN.** O'lchov (7.3.1
iliq start logi): chat ro'yxatlari +4 s / +9 s da yuklangan, lekin
`ActivityCacheLoad` faqat +94 s da (birinchi marta `CompactActivityHistoryAsync`
ichida; `WarmActivityCache()` hech qayerda chaqirilmaydi) — ya'ni har startda
~94 s faollik tarixi umuman yozilmasdi. Yechim: yangi `custom_startup_quiet.{h,cpp}`
— `OnCalm(name, callback)`: callback asosiy oqim RO'YXATDAN O'TGANDAN KEYIN
ketma-ket 8 s tinch bo'lganda (qotish kuzatuvchisi tiki kechikishi < 150 ms)
va startdan kamida 20 s o'tgach chaqiriladi; yuqori chegara — ro'yxatdan
o'tgandan 90 s. Manba: `application.cpp` dagi qotish kuzatuvchisi
(`Start()` + `NoteTick(late)`). Ulangan 3 joy: `main_session.cpp`
(GlobalMaintenance, `crl::guard(this)`), `custom_media_quota.cpp`
(MediaQuotaScan), `custom_sync.cpp` (birinchi sikl signal kelganda DARHOL —
`syncNow()`; ilgari +90 s). Log: `CustomMod Perf: start tinchidi -> <nom>
(start+N ms, kutildi M ms, tinch|yuqori chegara)`.
**Sinov (build'dan keyin):** log'da uchala "start tinchidi" qatori ~+20-30 s
da, `ActivityCacheLoad` ham shu atrofda; "main thread blocked" startdagidan
ko'paymagan; sync birinchi sikli shu paytda.
**Shart:** A25 o'zgarishlari (startdagi holatni yozish) ko'rib chiqilib,
T49 build sinovdan o'tgandan KEYIN. A3.1 (alohida SQLite ulanishi) rejada qoladi.
**Eslatma:** 7.2.10 startidagi 5.2-7.5 s asosiy oqim bloki CustomMod emas —
OpenAL (~3 s) + D3D11 RHI probe (~2 s); S1 (`use-qt-rhi=false`) sinovi
bilan birga tekshiriladi.

## ✅ A25 — Faollik tarixi: startdagi holat yozilmaydi va 3 ta nuqson (2026-10-06; `b0e933957e`, 10-10 sinovdan o'tdi)

**Holat:** 🟡 KOD TAYYOR 2026-10-06 (PC'da, BUILD QILINMAGAN). Tekshirildi
(haqiqiy DB, kontakt `8720525440`). Qilingani: (1) `ScheduleStartupStatusCatchUp`
-- kesh tayyor bo'lgach (10 s so'rov, ~5 daq gacha) kuzatiladigan hamma
foydalanuvchining joriy statusi bir marta; (2) sarlavha faqat
`observed/snapshot` statusdan, lahza alohida "Undan keyingi faollik belgisi"
qatorida, muddati o'tgan online -> "online edi"; (3) `EncodeStatus` da
`kOfflineSkewSeconds = 5`, eski yozuvlar ham shu qoida bilan; qo'shimcha:
online muddati yangilanishi seans DAVOMI deb olinadi (offline kelmasa seans
o'z muddati bilan yopiladi) -- Python simulyatsiyasida 22.09: 65 yozuv -> 32
haqiqiy seans, soxta lahza 0; (4) davrlar yangisi tepada.
**Sinov (build'dan keyin):** kontakt offline bo'lgandan keyin ilovani ochish
-> ~2 daqiqada bazada `offline:<vaqt>` paydo bo'ladi, sarlavha Telegram'dagi
last-seen bilan mos; 22.09 ro'yxati qisqa ulanish guruhlari bo'lib chiqadi.
Startdagi yozuvlar asosiy oqimda -- startdagi qotish (09-12) qaytmaganini
log'da tekshirish. Foydalanuvchi skrinshoti: oyna "hozir online (19:22 gacha)"
deydi, Telegram esa "last seen 21:17".

1. **Startdagi holat yo'qoladi (asosiy).** Ilova 21:18:32 da ochilgan,
   kontakt 21:17 da offline bo'lgan. Holat serverdan start paytida keladi,
   lekin `RecordField` kesh tayyor bo'lmaguncha (start+~90 s) jimgina
   qaytadi va keyin hech kim qayta yozmaydi (story uchun retry bor, status
   uchun yo'q). Natija: ilova yopiq bo'lgan vaqtdagi oxirgi holat HECH
   QACHON bazaga tushmaydi. Tuzatish: kesh tayyor bo'lgach kuzatiladigan
   barcha kontaktlar uchun bir marta `RecordCurrentState` (mavjud funksiya).
2. **Story/rasm nuqtasi "joriy holat" bo'lib qoladi.** Story 19:22 da
   `status = online:<story vaqti>` sifatida yozilgan va eng oxirgi status
   bo'lgani uchun sarlavha "hozir online" deydi. `DecodeStatusLabel`
   sarlavhada manba `story/photo/read/manual` bo'lsa "hozir online" emas,
   "📖 HH:mm da hikoya qo'ygan" deyishi kerak.
3. **"online:<hozir+1>" aslida offline.** Kontakt chiqib ketganda
   `was_online` server/lokal soat farqi tufayli `now` dan 1 s katta keladi,
   `EncodeStatus` uni `online:` deb yozadi (22.09 dagi barcha "aniq lahza"
   qatorlari shu: 09:05:32 online(+5 daq) -> 09:05:40 online:09:05:41).
   Tuzatish: `EncodeStatus` da `onlineTill - now <= 30` -> `offline:`;
   eski ma'lumot uchun `ReconstructOnlinePeriods` ham shu qoidani qo'llasin
   (davr 09:05:32-09:05:41 bo'lib chiqadi, "lahza" emas).
4. **Ro'yxat eskisidan boshlanadi.** Oxirgi 300 yozuv olinadi, lekin
   davrlar eskidan yangiga chiqadi -- foydalanuvchi tepada 22.09 ni ko'radi,
   bugungisini pastda qidiradi. Teskari tartib kerak.

**Muhim kontekst:** VPS Capture (customsync-server Plan 05, statuslarni 24/7
yozadi) kodi tayyor, lekin **deploy to'xtatilgan** (VPS buzilgan, audit
ketmoqda); lokal `sync_state` bo'sh -- ya'ni hozir ilova yopiq paytdagi
bo'shliqni to'ldiradigan hech narsa ishlamayapti. 02-03.10 dagi deyarli
nol yozuv ham ehtimol shu (ilova yopiq bo'lgan -- tasdiqlanmagan).

---

## 🔴 A24 — Scope `setting` qo'llanishidagi ikki kamchilik (2026-09-29)

**Holat:** 🟡 KOD TAYYOR 2026-09-30 (PC'da, BUILD QILINMAGAN). Spec
§3.2.1a va CHANGELOG 2026-09-30. Yo'l-yo'lakay 3-kamchilik topildi va
tuzatildi: startdagi qayta yuborish `occurred_at = hozir` bilan ketardi,
ya'ni eskirgan qurilma ishga tushishi bilan yangiroq o'zgarishni bosib
ketardi. Endi `occurred_at` = kalit versiyasi (`sync_state`
`setting_ver.<key>`), versiya faqat qiymat xeshi o'zgarganda oshadi.
1-qism -> `ApplyRemoteScopeSetting`; 2-qism -> `main_session.cpp` da
`domain().activeValue()` obunasi.

**Sinov (build'dan keyin, 2 qurilma):** A'da WL o'zgartirish -> B pull
qiladi -> B qayta ishga tushadi -> A'da WL B'nikiga QAYTMASLIGI kerak;
log'da eski yozuv uchun `stale_setting`. Ko'p akkauntda akkaunt
almashtirib sozlama o'zgartirish -> outbox `account_id` = ekrandagi.

Asl tavsif:

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

## ✅ A23 — Arxiv papkasi sifatida disk ildizini tanlashga ruxsat bor (2026-09-29; 10-10: 1-2 qadam o'tdi, 3-4 ko'chirish sinalmagan)

**Holat:** 🟡 KOD TAYYOR 2026-09-30 (PC'da, BUILD QILINMAGAN) — laptop
build + sinov kerak. Nima qilindi:
- `NormalizeArchiveRootChoice()`: disk ildizi tanlansa
  `<ildiz>/customizationMainFolder` ga aylanadi.
- `IsSameOrInsidePath()`: joriy arxivning o'zi yoki ichidagi papka
  tanlansa toast bilan rad etiladi (aks holda `MoveTree` cheksiz rekursiya).
- `EnsureArchiveLayout`: ko'chirish faqat `medias`, `db`, `config`,
  `backups`, `bombmedia` bilan cheklandi, keyin eski ildiz bo'sh bo'lsa
  o'chiriladi (`rmdir`, faqat bo'sh papka).

**Sinov (build'dan keyin):** 1) `E:\` tanlash -> dialogda
`E:/customizationMainFolder` ko'rinadi; 2) joriy arxiv ichidagi papkani
tanlash -> rad etiladi; 3) oddiy ko'chirish -> 5 papka o'tadi, eski papka
yo'qoladi; 4) begona fayli bor papkadan ko'chirish -> begona fayl joyida
qoladi.

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

**Holat:** 🟡 2-band uchun A+C KOD YOZILDI (`df785ce63a`, 2026-10-13,
foydalanuvchi "A va C" ni tanladi). BUILD O'TDI (08:21) va 23:08 da
O'RNATILDI. 23:11 startida ishladi: bitta chatda 97 fayl bog'landi + 2
belgi, boshqasida 5 belgi. Qolgani foydalanuvchi sinovi:
- chat `7053823996` da 42 media yozuvi kutiladi (6 tasi fayl bilan, 36
  tasi belgi bilan);
- boshqa qurilmadan rasm o'chirib, rasm qutqarilishini tekshirish. 1-band
(tartib) — diagnostika (`order breaks`) o'rnatilgan, 10-13 startida 0.

**A+C nima qiladi (`df785ce63a`):**
- Qo'shimcha topilma: faylsiz deb yashirilgan 649 ta egasi aniq yozuvdan
  101 tasining fayli arxivda BOR edi (media_index 'present'), faqat yozuvga
  bog'lanmagan — ovozli xabar Telegram keshida fayl emas (`filepath()` bo'sh,
  `TryRescueMedia` "allaqachon arxivda" deb qaytadi), rasm esa o'chirilganda
  ekranda bo'lmagan.
- A/bog'lash: inject va `setDeletedLocally()` fayl topilmasa media_index'dagi
  'present' faylni oladi — FAQAT aniq `account_id` (legacy 0 emas: shaxsiy
  chatlarda msg_id'lar akkauntlar orasida to'qnashadi). Yangi CustomDB:
  `GetMediaIndexForPeer`, `PresentMediaRelPath`, `SetDeletedMediaPath`.
- A/belgi: fayli yo'q, media bo'lgani aniq (`is_media` yoki media_index
  yozuvi) va egasi aniq yozuv — `📎 Rasm/Video/Ovozli xabar/Fayl: nom, N MB
  (hajmi chegaradan katta edi | kvota to'lgan edi | serverdan olib bo'lmadi |
  yuklab olinmoqda | fayl saqlanmagan)`. Legacy (account 0) yozuvlar avvalgidek
  yashirin (A21 da biriktirilgach ko'rinadi).
- C: `CustomArchive::RescueDeletedPhoto()` — o'chirilganda rasm xotirada
  bo'lmasa `photo->load()` (avval Telegram keshi, keyin server), kelganda
  `medias/images/<peer>_<msg>.jpg` ga yoziladi, media_index 'present' (l3) va
  o'chirilgan xabar yozuviga bog'lanadi; 10 daqiqada kelmasa 'missing'
  (`reference_expired`). MarkDeleted() dan KEYIN chaqiriladi.
- Log: `... | media: linked N, placeholder M`.
**Sinov (o'rnatgandan keyin):** `7053823996` chatida avval yashirin bo'lgan
42 ta media xabar endi ko'rinishi kerak (hammasi egasi aniq: 6 tasi arxivdagi
fayli bilan, 36 tasi 📎 belgi bilan — 10-13 nusxasi bo'yicha); log'da `linked`/`placeholder` > 0; biror rasm
xabarni boshqa qurilmadan o'chirib, arxivga `medias/images/` tushishini va
chatda rasm bilan qaytishini tekshirish.

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

**Baza tahlili (2026-10-13, faqat NUSXADA — `db\pre-9a730f3c41-...`, faqat
sanoqlar):**
- `msg_date = 0` gipotezasi RAD ETILDI: o'chirilgan 4066 yozuvdan bor-yo'g'i
  2 tasi (2026-06-07 dagi, mazmunsiz, `account_id = 0`) — ular baribir
  "bo'sh" deb tashlanadi, `currentSecsSinceEpoch()` fallback'i amalda ishlamaydi.
- "O'zim yozgan xabarlar qolib ketadi" — kuchli nomzod: fayli ARXIVLANMAGAN
  media xabarlar. Inject qoidasi (2026-08-27): matn yo'q va fayl diskda yo'q
  -> ko'rsatilmaydi (`skippedEmpty`). `7053823996` chatida 752 o'chirilgan
  yozuvdan 42 tasi shunday (kiruvchi 14, CHIQUVCHI 28 — chiquvchilar ikki
  barobar); butun bazada matnsiz yozuvlar: chiquvchi 381 (366 tasi fayl
  yo'li umuman yo'q media), kiruvchi 640 (595 tasi shunday). Ya'ni
  foydalanuvchi yuborgan rasm/fayl
  o'chirilsa, uning fayli odatda arxivda yo'q va xabar chatga qaytmaydi.
  Nima uchun (kod o'qishdan xulosa, sinovda TASDIQLANMAGAN): arxivga fayl
  faqat YUKLAB OLISH yo'lidan tushadi — L1 (foydalanuvchi ochgan), L2
  `MaybeDownloadMedia` (faqat `ShouldMediaBackup` yoqilgan chatda; fayl
  allaqachon diskda bo'lsa `filepath(true)` sharti bilan darhol qaytadi), L3
  `TryRescueMedia` (o'chirilganda; faqat hujjatlar va faqat media backup
  yoqilgan chatda). Chiquvchi media'ni foydalanuvchi o'z diskidan yuboradi —
  yuklab olish bo'lmaydi, demak arxiv nusxasi ham paydo bo'lmaydi.
  Variantlar (foydalanuvchi qarori kerak, 08-27 qarori "arvohlar" sababli
  qabul qilingan edi): (a) shunday yozuvlar uchun "📎 media (fayl
  arxivlanmagan)" belgisi — faqat `account_id` aniq bo'lganlarda; (b)
  chiquvchi media'ni yuborish paytida arxivlash.
- Tartib chalkashligi (1-band) baza bilan tushuntirilmaydi — `insertMessageToBlocks`
  sana bo'yicha qo'yadi, chat ichida sana va ID monoton. Keyingi qadam:
  inject'dan keyin bloklarda sana tartibi buzilgan qo'shni juftlarni sanab
  log'ga yozish (diagnostika), foydalanuvchi bilan qayta sinash.

---

## Muhim Eslatmalar

- **Build** — 2026-09-26 dan Claude o'zi qiladi, FAQAT laptopda
- **Git** — commit+push ruxsati bor, FAQAT `origin/Oybek`; `upstream` ga hech qachon; `Co-Authored-By` qabul emas
- **Javoblar** — O'zbek lotin tilida
- **MEMORY.md** — kod holati + bug fix tarixi, `DOCs/MEMORY.md`
- **PRD.md** — to'liq feature talablari + known limitations, `DOCs/PRD.md`
- **Model almashtirish** — yangi session da: MEMORY.md → PRD.md → NEXT_TASKS.md o'qi
