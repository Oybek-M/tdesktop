# CustomMod — Keyingi Tasklar Ro'yxati

**Oxirgi yangilanish:** 2026-10-14 23:55 (o'rnatilgan: `efde411217` = A7 "7.3.1 canary #1", 23:51 — start sinovi O'TDI)

---

## 🟡 2026-10-13 holati — keyingi sessiya shu yerdan boshlaydi

**🆕 10-16 11:25 (eng yangi): canary #5 CHIQDI** (laptop 11:26 da yuklab oldi, exe SHA mos; o'rnatish Telegram qayta ochilganda) (`e98ab68c6a`,
`staged/e98ab68c6a-20261016`, exe SHA `ab5f7205eb1adf83...`), 3 mirror OK.
CustomSync xabari bo'yicha tuzatishlar: (1) merge'dan keyin ochiq tarix
yangilanadi (`RequestHistoryReload`, 1.5 s debounce, qurilgan xabarlarga
`restoreFromCustomDB()` qayta qo'llanadi); (2) sync orqali kelgan tahrir
'backup' qatorini ham yozadi -> TAHRIRLANDI bloki ko'rinadi; (3) rad
etilgan yozuvlar soni log'ga yoziladi. "log.txt band" (tez qayta ochish)
upstream xatti-harakati, o'zgartirilmadi; rad etilgan yozuvda cursor
surilishi ataylab (qayta urinish foyda bermaydi). **Foydalanuvchi sinovi
kerak:** Telegram yopiq holda boshqa qurilmadan peer'ga yozish, tahrirlash,
o'chirish -> Telegram'ni ochish -> qayta ochmasdan O'CHIRILDI va
TAHRIRLANDI 1 ko'rinishi.

**10-16 08:00: canary #4 CHIQDI** (`e1ea5976d9`,
`staged/e1ea5976d9-20261016`, exe SHA `994f3a6c8c5faa64...`). /secure
uchun yangi login/parol exe'da (header mos, exe'da yangi URL 1 ta, eski
login 0 ta; qiymatlar chop etilmadi). vps-secure, vps-pub va github OK, SHA
mos. GitHub endi **Release asset** (A32 1-qism, `74037eba40`): tag =
paket nomi, git tarixi o'smaydi. Laptop'da Telegram 07:50 da yopilgan,
08:33 da #4 yuklandi va o'rnatildi, 08:36 da `canary: #4` bilan
ishga tushdi, exe SHA mos — **laptop #4 da (e2e #3 -> #4 O'TDI)**. **Qolganlari:** (1) PC va akamga #4 ni 1 marta qo'lda o'rnatish
(`install-staged.sh e1ea5976d9-20261016`, Telegram yopiq holda);
(2) uchala mashina #4 da bo'lgach CustomSync'ga yozish, shunda eski login
o'chiriladi; (3) ✅ A32 2-qism (10-16, ruxsat bilan): tdesktop-releases
`main` = 1 ta orphan commit `ab87705` (README, current4, v2/current6),
force-push qilindi; #4 Release asset joyida. To'liq eski tarix zaxirasi:
`C:\TBuild\backups\tdesktop-releases-mirror-20261016.git` (mirror, fsck ok,
12 commit, 423 MiB). GitHub `size` hali ~433 MB ko'rsatadi: eski obyektlar
GitHub'ning o'z GC'sigacha qoladi (zudlik bilan kerak bo'lsa GitHub
Support'dan purge so'raladi).

**10-15 21:15:** canary #3 (`b307fe98e4`) foydalanuvchi
ruxsati bilan CHIQARILDI: vps-secure, vps-pub, github — uchalasi OK, SHA
mos (GitHub `a05dc48`). 1-urinishda ikkala VPS mirror sftp "Connection
closed" (exit 255) bilan yiqildi; `release.ps1` ni qayta ishga tushirish
o'tdi. Laptop (#2) 21:18 da `Canary feed offers 7003001 #3` ni topdi,
21:19 da yuklab ochdi: `tupdates/temp/ready` bor, `version` = canary #3,
exe SHA staged build bilan mos. 21:46 da foydalanuvchi qayta ishga
tushirdi: log'da `canary: #3`, exe SHA mos, `Update Error` yo'q —
**e2e #2 -> #3 O'TDI**. PC va
akam hali #1/#2 ni ham o'rnatmagan: foydalanuvchi keyinroq eng yangisini
1 marta qo'lda o'rnatadi. **Keyingisi — canary #4:** CustomSync 21:52 da
"secrets yangilandi" deb yozdi: `vps-mirror-secrets.txt` da yangi login va
parol (LF, BOM'siz), `pub_prefix` o'zgarmadi. O'tish davrida eski login
ham ishlaydi (ikkalasi 403 = auth o'tdi, katalog yopiq; parolsiz 401).
Qadamlar (foydalanuvchi ruxsati bilan): kCounter = 4 -> build (configure
`CMAKE_CONFIGURE_DEPENDS` orqali o'zi qayta ishlaydi) ->
`gen/custom_update_auth.h` yangi login bilan mosligini tekshirish
(qiymatni chop etmay) -> dry-run -> ruxsat bilan reliz. #3 klientlar #4 ni
eski login bilan oladi. Eski loginni o'chirish laptop, PC va akam #4+ ga
o'tgach, egasining buyrug'i bilan bo'ladi; o'tganlarini CustomSync'ga
yozish kerak.

**🆕 A32 — GitHub mirror hajmi (10-15 21:50; faqat tahlil, HALI
BOSHLANMAGAN):** `Oybek-M/tdesktop-releases` PRIVATE: klientlar undan
o'qiy olmaydi, u faqat zaxira/arxiv. Hajmi ~375 MB, 12 commit: 7.0.7
(test, keyin o'chirilgan), 7.0.9 ×2, 7.1.1 ×3, canary #2, canary #3 ×2.
Har paket ~57 MB. GitHub 50 MB dan kattasiga ogohlantiradi, 100 MB dan
kattasini push'da rad etadi, paket esa upstream bilan o'sib boradi.
GitHub Release'lar yo'q. Taklif:
1. `publish.ps1` GitHub qadamini Release asset'ga o'tkazish
   (`gh release create v7003001-canary-N <paket> --repo ...`). Asset 2 GiB
   gacha bo'lishi mumkin, trafik cheklovi yo'q, git tarixi o'smaydi.
   Git'da faqat kichik `current6` qoladi.
2. Eski paket blob'larini tarixdan tozalash (orphan branch yoki
   filter-repo + force-push). Bu qaytarib bo'lmaydigan amal, FAQAT
   egasining aniq ruxsati bilan.
3. Git LFS'da saqlash/trafik kvotasi bor, Release asset'dan afzalligi yo'q.

**10-15 19:55:** canary #3 A31 bilan BUILD O'TDI (19:47):
`staged/b307fe98e4-20261015` (SHA256 `5164804557376427...`). Tekshirildi:
`gen/custom_update_auth.h` secrets bilan mos, exe'dagi yangilanish URL'i #2
dagi bilan bir xil (parol chop etilmadi). Dry-run O'TDI
(`td-update-win-x64-7003001-canary-3`). **Reliz foydalanuvchi ruxsatini
kutmoqda.** CustomSync'ga "tayyor" yozildi: parol almashtirish (o'tish davri
bilan) endi ularda; yangi parol kelgach — canary #4 (yangi parol bilan).
`a9389507ce` build'i (A31'siz) ISHLATILMAYDI.

**⏸ 10-15 09:15 — PAUZA (tarix):**
- Kodda tayyor, push qilingan: A28-A30 (`a9389507ce`, kCounter = 3) va
  **A31** (`b307fe98e4`): `/secure` login:parol manbadan olib tashlandi.
  Endi CMake configure paytida `DesktopPrivate/vps-mirror-secrets.txt`
  (`secure_user=`, `secure_pass=`) dan `out/Telegram/gen/custom_update_auth.h`
  ga yozadi; faqat localstorage.cpp o'qiydi.
- Build: `a9389507ce` (#3, A31'siz) 09:02 da boshlangan, pauza paytida
  link bosqichida edi — natija `C:/TBuild/build-gentle.log` da. UNI CHIQARMANG.
- **Keyingi qadamlar:**
  1. A31 bilan qayta build (CMakeLists o'zgargani uchun configure qayta
     ishlaydi; localstorage.cpp + link).
  2. exe'da prefiks oldingi bilan bir xilligini tekshirish: secrets
     faylidan o'qib, faqat MOSLIK SONINI chiqarish, parolni chop etmaslik.
  3. `staged/b307fe98e4-20261015` ga saqlash → `release.ps1 -DryRun` →
     foydalanuvchi ruxsati bilan #3 relizi (`*>&1` ishlatsa ham endi
     yiqilmaydi).
  4. CustomSync'ga xabar berish: "build fayldan o'qiydi" — shundan keyin
     ular parolni o'tish davri bilan almashtiradi (yangi parol qo'shiladi,
     eskisi hamma yangi build'ga o'tguncha qoladi).
- Foydalanuvchida: PC va akamning kompyuteriga canary #2 ni qo'lda
  o'rnatish (`staged/80489011e3-20261015`); A22 sinovi; A21.

**10-15 08:45:** ✅ **A7 e2e O'TDI** — canary #2 VPS'ga
chiqarildi (secure + pub, SHA tasdiqlandi; GitHub mirror qo'lda,
tdesktop-releases `09010a7`), laptop #1 08:24 da topdi, yukladi, foydalanuvchi
"Update Telegram" bosdi -> 08:28 `canary: #2`. Sync ulandi (08:22):
`pull_cursor` = 93, capture'ning 22 deleted + 3 edited yozuvi arxivda, outbox 0.
⚠️ Upstream log satri `/secure` URL'idagi basic-auth login:parolni yozardi —
log nusxalarida yashirildi, kod `b2d0bb5568` da tuzatildi (keyingi canary'da).
Parol suhbat tarixiga ham tushdi — almashtirish foydalanuvchi qarori
(yangi parol build'ga kiritiladi, nginx o'tish davrida ikkalasini qabul qiladi).
Yangi tasklar (CustomSync UX topilmalari + baza zaxirasi):
- **A28:** Sync tab'idagi diagnostika bloki tab qayta ochilguncha
  yangilanmaydi ("Lokal master kalit: yo'q", kalit esa bor).
- **A29:** muvaffaqiyatli ro'yxatdan o'tgach tugmani qayta bosish
  `invalid_or_used_code` ni ko'rsatib, holat yozuvini almashtiradi.
- **A30:** `MaybeBackupBeforeMigration` versiyani o'qiy olmasa (eski jarayon
  yopilayotgan payt) `premigrate-v0` nusxasini oladi — 10-15 08:27 da
  62 MB ortiqcha nusxa; baza sog'lom (v17, quick_check ok).

**10-15 01:10:** CustomSync: sync serveri egasi p4 apply qilgach ochiladi
(`https://sync.2007.uz`). O'rnatilgan build'da `CUSTOM_SYNC_HAS_WEBSOCKETS` BOR
(exe'da `/ws/notify`) — ro'yxatdan o'tishni hozirgi build bilan qilsa
bo'ladi (admin kodi egasi ekranida, 10 daqiqa; keyin arxiv paroli;
02-task11-manual-checklist §4). Xavfsizlik tavsiyasi bajarildi
(`80489011e3`): WebSocket token'i URL o'rniga `Authorization: Bearer`
sarlavhasida. Shu commit = **canary #2** (`kCounter` = 2), BUILD O'TDI:
`staged/80489011e3-20261015`. Qo'lda O'RNATILMAYDI — A7 e2e'da VPS
orqali #1 -> #2 yangilanish bo'lib chiqadi.

**10-14 23:55:** canary #1 O'RNATILDI (23:51). Zaxiralar:
`Release\_tdata-backup-pre-efde411217-20261014-20261014-235131` (103 MB),
`db\pre-efde411217-20261014-20261014-235131` (61 MB). 23:52 startida:
`Launched version: 7003001, ..., canary: #1` ✅; `start tinchidi` +20 s ✅;
`Update Error: could not get current version 203` — KUTILGAN (VPS'da
`/secure/v2/current6` hali yo'q, 404; canary 10–15 daqiqada qayta so'raydi).
Start bloki 3571 ms (exe almashgandan keyingi birinchi start) — iliq
startni qayta o'lchash kerak. Log nusxalari: `C:\TBuild\starttest-20261014-a7\`.

**10-14 09:30:**
- A7 canary #1 BUILD O'TDI (09:22, inkremental ~10 daqiqa). Nusxa:
  `C:\TBuild\staged\efde411217-20261014\` (SHA256 `5f9e30dc4f132fdd...`,
  `--check` O'TDI; exe ichida manifest v2 va canary feed kodi bor).
  O'rnatish uchun foydalanuvchi Telegram'ni yopishi kerak:
  `bash /c/TBuild/install-staged.sh efde411217-20261014`. Sinov: log'da
  `Launched version: 7003001, ..., canary: #1`, Settings'da
  "7.3.1 canary #1".
- VPS: reliz kaliti (`~/.ssh/customsync-release`, host `customsync-vps`)
  serverda hali RUXSAT ETILMAGAN (`Permission denied (publickey)`).
  `.pub` kalit VPS'ning `authorized_keys` iga qo'shilmaguncha
  `release.ps1` yuklay olmaydi. `/secure/v2` papkasini skript o'zi
  yaratadi (`mkdir -p`).
- TDLib build tugadi (CustomSync, 10-14 00:35) — build cheklovi yo'q.

**23:45 (10-13):**
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
tekshirdi). BUILD O'TDI (10-14 09:22): `staged\efde411217-20261014`.
Qolgani:
1. ✅ Laptop'ga o'rnatildi (10-14 23:51), log'da `canary: #1`.
2. VPS: reliz SSH kaliti hali ruxsat etilmagan (`Permission denied`) —
   `customsync-release.pub` ni `authorized_keys` ga qo'shish (CustomSync
   sessiyasi yoki foydalanuvchi). Shu bilan birga yarim yuklangan 7.2.7 ni
   tekshirish.
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

**Holat (10-16 tekshirildi):** kod `0c1a158c32` da, `b19d83c2a1` merge orqali
v7.2.10 build'iga kirgan (10-10 o'rnatilgan) va hozirgi canary'larda bor —
faqat foydalanuvchi sinovi qoldi. Avvalgi yozuv: KOD TAYYOR 2026-09-30. Spec
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
