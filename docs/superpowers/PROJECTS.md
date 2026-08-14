# Loyiha yo'nalishlari — holat jadvali

Bu fayl qaysi ish **hozir faol**, qaysi biri **to'xtatib qo'yilgan** va
qaysi biri **hali muhokama bosqichida** ekanini ko'rsatadi. Yangi
sessiya boshlanganda birinchi shu yerga qarang.

Oxirgi yangilanish: 2026-08-14

> 🚨 **YO'L O'ZGARDI (2026-08-14):** build daraxti
> `...\Projects programming\Telegram\Telegram\` dan **`C:\TBuild\`** ga
> ko'chirildi. Repo endi **`C:\TBuild\tdesktop`**, kutubxonalar
> `C:\TBuild\Libraries`, maxfiy kalitlar `C:\TBuild\DesktopPrivate`.
> **Sabab:** upstream `prepare.py` bo'sh joyli yo'llarni qo'llab-quvvatlamaydi
> (42 ta tirnoqsiz `%LIBS_DIR%`/`%THIRDPARTY_DIR%` ishlatilishi), bizning
> eski yo'lda esa "Projects **programming**" bor edi. Rasmiy hujjat ham
> aynan shunday qisqa yo'lni tavsiya qiladi (`D:\TBuild`). Qolgan 23 ta
> loyiha o'z joyida qoldi — faqat shu daraxt ko'chdi.

---

## ⚡ Umumiy ustuvorlik tartibi (2026-08-13 qayta rejalashtirildi)

Foydalanuvchi limit tugashi sababli bu yerda aniq bosqichlarga bo'lib
qo'yildi — boshqa Claude sessiyasida ham davom ettirish mumkin bo'lsin
uchun.

1. **1-bosqich — mayda, tez bitadigan tasklar.** Hozircha alohida
   build talab qilmaydigan ochiq kod-darajasidagi ish yo'q (A11'ning
   qolgan qismi ham build talab qiladi — pastga qarang). Agar shu
   bosqichda yangi mayda task paydo bo'lsa, avval shular yopiladi.
2. **2-bosqich — A6 (Qt 5.15.18 → Qt 6.5+ migratsiyasi).** To'liq
   yakunlanadi.
3. **BITTA umumiy build+qo'lda tekshiruv** — 1- va 2-bosqichdagi
   barcha o'zgarishlarni (jumladan **A11 Task 6**: story-signal
   feature'ning build+qo'lda tekshiruvi) **bitta build siklida**
   birga qamrab oladi, alohida-alohida build qilinmaydi (resurs
   tejash uchun).
4. **3-bosqich — Track C: `customsync-server`.** Yangi, mustaqil
   repo. Joylashuv **kelishildi**: top-level yangi papka
   `C:\Users\Oybek\Documents\Projects programming\customsync-server`
   (2-tavsiya emas — ikkalasi ham `Telegram\Telegram\` ichiga
   joylashgan tdesktop'ning o'z build-scaffold'i, `DesktopPrivate`
   maxfiy kalitlari bilan bir joyda — shu sabab rad etildi). Boshlash
   tartibi: `01a` backend poydevori → ... (to'liq ro'yxat pastda, C
   bo'limida).

> ⚠️ **Implement qilishni (Qt6 migratsiya ham, customsync-server ham)
> boshlashdan oldin har safar foydalanuvchidan alohida ruxsat so'rash
> SHART.** Bu hujjat yangilanishi hali implement boshlanganini
> anglatmaydi — faqat rejani qayd etadi.

> 🆕 **2026-08-13, ish oxirida:** yangi kritik xato **A13** (AntiDelete
> matnli xabarda ishlamadi) topildi — yuqoridagi bosqichlar rejasiga hali
> kiritilmagan/joylashtirilmagan. **Keyingi sessiyada birinchi navbatda
> user'dan so'rang:** A13 investigatsiyasi A6 build'idan OLDIN qilinsinmi
> (A12'da bo'lgani kabi tartib) yoki A6 build davom etaversinmi — bu
> qaror hali qabul qilinmagan.

---

## A — tdesktop CustomMod ⚡ FAOL

Hozirgi asosiy ish. Boshqa yo'nalishlar bunga aralashmasligi kerak.

**Repo:** `Telegram/tdesktop`, branch `Oybek`
**Holat:** ✅ **v7.0.9 build qilindi, chiqarildi va real-world
self-update sinovi muvaffaqiyatli o'tdi** (2026-08-08). Birinchi
urinishda update-apply ishlamagan edi — ildiz sabab (`Packer.exe`ning
`release-staging\` prefiksini noto'g'ri arxivlashi) topilib tuzatildi,
paket qayta chiqarildi, qayta sinovda versiya haqiqatan v7.0.7'dan
v7.0.9'ga o'zgardi. Tafsilot:
[`../self-update/HANDOFF.md`](../self-update/HANDOFF.md) §0.1.

**Ochiq vazifalar:**

| № | Vazifa | Holat |
|---|---|---|
| A1 | To'liq qo'lda tekshiruv | ✅ 2026-08-01, kritik muammo yo'q |
| A2 | Upstream v7.0.5 → v7.0.7 sync | ✅ 2026-08-01, `2e61fdcbc2` (162 commit, 2 konflikt) |
| A3 | Log shovqinini kamaytirish (`API Warning: not loaded minimal channel applied.`) | 🟡 2026-08-09: root cause topildi (`data_session.cpp:966`, rasmiy kod, `LOG` shartsiz yoziladi — `DEBUG_LOG`ga o'tkazish mumkin edi), lekin user tuzatmaslikni tanladi — kod o'zgarishsiz qoldi. Past ustuvor, kerak bo'lsa keyinroq qaytiladi. |
| A4 | Upstream v7.0.7 → v7.0.9 sync (to'liq `dev→SafeWall→Customizations↔Oybek` zanjiri orqali) | ✅ 2026-08-06, 100 commit, 1 konflikt (`lib_ui` submodule) |
| A5 | Build v7.0.9 + haqiqiy self-update sinovi | ✅ 2026-08-08: build+reliz+update-apply hammasi tasdiqlandi (2 marta chiqarildi — `publish.ps1`dagi path bug tuzatilgach) |
| A6 | Qt 5.15.18 → Qt 6.11.1 ga o'tish | 🔨 **Build'gacha bo'lgan barcha tayyorgarlik ishlari yakunlandi (2026-08-13).** Brainstorming orqali tasdiqlangan, spec: `docs/superpowers/specs/2026-08-13-qt6-migration-design.md`. Investigatsiya natijasi: rasmiy `tdesktop`/`desktop-app` build tizimi Qt6'ni allaqachon to'liq qo'llab-quvvatlaydi (`Telegram\build\prepare\win.bat qt6`), bu YANGI/experimental ish emas — Windows'da rasmiy default hamon Qt5 (Windows 7 moslik uchun), Qt6 ixtiyoriy `qt6` bayrog'i bilan. Bajarildi: (1) `lib_ui` submodule Qt5-fork (`Oybek-M/lib_ui`)dan rasmiy `desktop-app/lib_ui`ga qaytarildi (commit `cd9d356ddd`) — bu Qt6'gacha Qt5 build'ni vaqtincha buzadi, kutilgan holat; (2) butun `SourceFiles` bo'ylab Qt5-only API sweep qilindi (`QRegExp`, `QTextCodec`, `qAsConst`, `QStringRef`, `QDesktopWidget` va h.k. — topilmadi); (3) 3 ta `QNativeGestureEvent::pos()/globalPos()` chaqiruvi `position()/globalPosition()`ga o'zgartirildi (commit `914c1b1a95`: `info_media_grid_zoom.cpp`, `editor_paint.cpp`, `media_view_overlay_widget.cpp`). **Build boshlash urinishi (2026-08-13, natija: hali real boshlanmagan):**
User "BUILD uchun tayyorman" dedi, keyin darhol "build qilishni real
boshlamay tur" deb to'xtatdi (chunki A13 kritik xatosi paydo bo'ldi,
pastga qarang) — **shuning uchun `win.bat qt6` hali HAQIQIY ishga
tushmagan**, faqat muhit sozlash urinishlari bo'ldi:
- 2 marta `cmd.exe /c "..."` Bash orqali chaqirildi — MSYS/Git-Bash
  quote-mangling sababli **ikkalasi ham no-op** (banner chiqib, hech
  narsa bajarmay darhol exit 0 bilan tugadi). **Xulosa: Bash tool orqali
  to'g'ridan-to'g'ri `cmd.exe /c "murakkab && zanjir"` ishlatmang** —
  buning o'rniga alohida `.bat` fayl yozib, uni chaqiring.
- 3-urinish (PowerShell orqali alohida `.bat` fayl chaqirilib) — **haqiqiy
  xato topildi**: `vcvars64.bat -vcvars_ver=14.44` ishlamadi —
  `[ERROR:vcvars.bat] Toolset directory for version '14.44' was not
  found.` Sabab: `docs/building-win.md`dagi Windows-7-moslik uchun maxsus
  `v144.4` toolset komponenti o'rnatilmagan (faqat standart `v145` bor).
  **User qarori (tasdiqlangan, AskUserQuestion orqali):** bizga Windows 7
  moslik kerak emas — `-vcvars_ver=14.44` bayrog'ini olib tashlab, oddiy
  `vcvars64.bat` (standart toolset) ishlatiladi. **Bu fix hali .bat faylga
  yozilmagan** — keyingi sessiyada shu o'zgarishni qilib, keyin qayta
  urinish kerak.

**Ishlatiladigan skript (qayta yaratish kerak, chunki avvalgisi
sessiya-specific temp papkada edi va yo'qolgan bo'lishi mumkin) — TUZATILGAN
holatda (`-vcvars_ver=14.44` OLIB TASHLANGAN):**
```bat
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo VCVARS_FAILED
    exit /b 1
)
cd /d "C:\Users\Oybek\Documents\Projects programming\Telegram\Telegram"
tdesktop\Telegram\build\prepare\win.bat qt6
echo PREPARE_EXIT_CODE=%errorlevel%
```
Ishga tushirish: **PowerShell orqali** (Bash/cmd.exe orqali EMAS —
yuqoridagi quote-mangling muammosi), `run_in_background: true` bilan,
masalan: `& 'C:\...\run_qt6_build.bat' *> 'C:\...\qt6_build.log'`.
Uzoq (soatlab) jarayon — user signal berguncha ishga tushirilmasin.

**BUILD URINISHLARI 2026-08-14 (⏸️ TO'XTATILDI — user ofisga ketdi):**
Progress: **20/32 bosqich muvaffaqiyatli** (cache'da), **21/32 `libvpx`da
bloklangan**.

Yo'lda topilgan va TUZATILGAN muammolar (hammasi bir xil naqsh — upstream
faqat ARM64 uchun to'g'ri sozlagan, x64 uchun unutgan):
1. `-vcvars_ver=14.44` — v144.4 toolset o'rnatilmagan → bayroq olib
   tashlandi (Windows 7 moslik bizga kerak emas).
2. Disk 31 GB yetmasdi → `out/Telegram/` (42.9 GB oraliq artefaktlar) va
   eski `Telegram.pdb` o'chirildi → 77 GB.
3. `win.bat` `python %FullScriptPath%prepare.py` ni **tirnoqsiz**
   chaqiradi → `prepare.py` to'g'ridan-to'g'ri, tirnoq bilan chaqirilyapti.
4. `NUMBER_OF_PROCESSORS` cmd.exe himoyalagan (sinovda tasdiqlandi:
   `set` ishlamaydi, bola jarayonda ham tizim qiymati qaytadi) →
   `prepare.py` ga `TDESKTOP_BUILD_JOBS` qo'shildi (cache-kalitiga
   ta'sir qilmaydigan yo'l bilan: `environment` dict'ga EMAS,
   `modifiedEnv` ga). Hozir 6 ta job (16 yadro / 15 GB RAM da to'liq
   parallellik OS ni muzlatadi).
5. `prepare.py` fon rejimida `getch()` bilan
   `"(r)ebuild, rebuild (a)ll, (s)kip..."` savolini berib qotib qolardi →
   `silent` argumenti qo'shildi (skriptning o'z mexanizmi).
6. Bo'sh joyli yo'l → `C:\TBuild` ga ko'chirildi (yuqoridagi ogohlantirish).
7. `lzma` + `breakpad`: `ToolsetProp` bo'sh qolardi → v145 majburlandi.

⛔ **HAL QILINMAGAN BLOKER — `v143` toolset yo'qligi (3-marta chiqdi):**
`lzma`, `breakpad`, `libvpx` — uchalasi ham
`error MSB8020: Platform Toolset = 'v143' cannot be found` beradi.
Mashinada faqat VS2026 bor (MSVC 14.51 = v145); `vswhere` yagona
instansiyani ko'rsatadi, VS2022 papkalari bo'sh qoldiq. lzma/breakpad
uchun v145 majburlash yordam berdi, lekin **libvpx uchun ishlamadi**:
uning `configure`idagi `all_platforms` ro'yxatida faqat
`arm64-win64-vs17-v145` bor, `x86_64-win64-vs17-v145` yo'q
(`Unrecognized toolchain` xatosi). Mexanizmning o'zi bor —
`gen_msvs_vcxproj.sh` `platform_toolset` ni target nomining 4-maydonidan
oladi — faqat oq ro'yxatga kiritilmagan. Sinab ko'rilgan `-v145`
o'zgarishi **orqaga qaytarildi** (kodda ishlamaydigan qiymat qolmasin).

**User qarori (2026-08-14):** 1-variant — **VS2022 v143 build tools
o'rnatish** (Visual Studio Installer → "MSVC v143 - VS2022 C++ x64/x86
build tools", ~2-3 GB) — yoki boshqa yo'l topish. Ish **vaqtincha
to'xtatildi**.

**Qayta boshlashda:** (1) v143 o'rnatilgach build skriptini qayta ishga
tushirish (skript matni pastda), (2) `configure.bat x64 qt6 -D
TDESKTOP_API_ID=... -D TDESKTOP_API_HASH=...`, (3) to'liq build,
(4) A11 Task 6 va A13 bilan birga qo'lda tekshiruv.

**Ishlaydigan build skripti** (`C:\TBuild` uchun, PowerShell orqali
`run_in_background: true` bilan chaqiriladi):
```bat
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set TDESKTOP_BUILD_JOBS=6
set PYTHONUNBUFFERED=1
cd /d "C:\TBuild"
python -u "C:\TBuild\tdesktop\Telegram\build\prepare\prepare.py" qt6 silent
echo [A6] EXIT=%errorlevel%
``` |
| A7 | VS2022→VS2026 ko'chishi bilan bog'liq build-muhit tuzatishlari | ✅ 2026-08-08 — toolset, QT env var, api_id/api_hash, `DESKTOP_APP_DISABLE_AUTOUPDATE` qayta yoqildi. Tafsilot: `docs/self-update/HANDOFF.md` §2.6 |
| A8 | `publish.ps1` Packer path bug (release-staging prefiksi) | ✅ 2026-08-08 — tuzatildi, v7.0.9 qayta chiqarildi, sinovdan o'tdi. Tafsilot: `docs/self-update/HANDOFF.md` §0.1 |
| A9 | Upstream (rasmiy) versiya tekshiruvchisi — Custom Window'da rasmiy tdesktop'da yangi reliz bor-yo'qligini avto/qo'lda bildirish | ✅ 2026-08-09: to'liq implement, build va real-muhitda qo'lda sinov (7/7 band) muvaffaqiyatli. Spec: `docs/superpowers/specs/2026-08-08-upstream-update-checker-design.md`, reja: `docs/superpowers/plans/2026-08-08-upstream-update-checker-plan.md` (5/5 task ✅, yakuniy code review APPROVED) |
| A10 | "Bitta tugma" sync+build+publish pipeline — foydalanuvchi mavjud bo'lganda, A9 yangilanish borligini bildirgach, sync→build→3 mirror'ga reliz ketma-ketligini bitta buyruq/tugma bilan boshlash (hozirgi 4-5 qo'lda qadam o'rniga) | 🕓 Keyingi tasklarga qo'shildi (2026-08-08). **Ataylab yarim-avtomatik** — to'liq unattended emas: build resurs-qoidasi ("build oldidan doim so'rash"), merge-konflikt qarorlari va reliz oldidan tekshiruv hali inson ishtirokini talab qiladi. A9 asosida keladi, undan keyin brainstorming qilinadi. |
| A11 | Story post-vaqti — yashirilgan/berkitilgan last-seen'ni bilvosita aniqlash uchun qo'shimcha signal: kuzatilayotgan user story qo'yganda, uni ochmasdan/ko'rmasdan story'ning qo'yilgan vaqti mavjud "Activity History" arxiviga yoziladi (fallback signal) + ixtiyoriy story media (foto/video) zaxirasi. | 🔨 2026-08-09: 5/6 task implement+review qilindi va push qilindi (Task 1-5: settings maydoni, story-vaqt signali hook, media-backup logikasi, Custom Window toggle, History Box formatlash). **Faqat Task 6 (build + qo'lda tekshiruv) qoldi** — bu alohida emas, A6 (Qt6) bilan **bitta umumiy build**da qilinadi (2026-08-13 qayta rejalashtirildi, yuqoridagi ustuvorlik bo'limiga qarang). Spec: `docs/superpowers/specs/2026-08-09-story-activity-signal-design.md`, reja: `docs/superpowers/plans/2026-08-09-story-activity-signal-plan.md`. Xavfsizlik: mavjud "Hikoyalarni anonim ko'rish" (`ShouldAnonymousStory`) real ikkinchi akkountdan tekshirilib, to'g'ri ishlayotgani tasdiqlandi (§5, spec ichida). |
| A13 | 🔴 **KRITIK — butun CHAT o'chirilganda AntiDelete deyarli hech narsani saqlay olmadi (ILDIZ SABAB TOPILDI 2026-08-13):** suhbatdosh ("Xurshida \| V", peer `7815103103`) **butun chatni** o'chirdi. **Ikkita mustaqil nuqson aniqlandi.** **(1) KO'RSATISH nuqsoni:** `history.cpp:2186` — `History::loadDeletedMessages()` boshida `if (isEmpty()) return;` bor. Butun chat o'chirilganda `blocks` bo'shaydi → `isEmpty()`=true → funksiya darhol chiqib ketadi → **DB'da SAQLANGAN xabarlar ham ko'rinmay qoladi**. `CustomDB::GetDeletedMessages()`ning butun kodbazada boshqa chaqiruvchisi YO'Q (alohida viewer yo'q) — demak ma'lumot bor, lekin foydalanuvchiga umuman yetib bormaydi. **(2) ARXIV QAMROVI nuqsoni:** `text_cache` jadvaliga yozish butun kodbazada FAQAT bitta joyda — `data_session.cpp:3558-3576`, `Session::addNewMessage()` ichida, `type == NewMessageType::Unread` sharti bilan. Ya'ni faqat ilova ishlab turganda real-vaqtda kelgan xabarlar arxivlanadi; serverdan scrollback orqali yuklangan eski tarix HECH QACHON arxivlanmaydi (`history.cpp`da bitta ham `CustomDB::` YOZISH hook'i yo'q — faqat o'qish). Natijada butun-chat o'chirilishida faqat (a) xotirada turgan va (b) real-vaqt cache'dagi xabarlar qutqariladi. **DB dalili:** 2026-08-13T17:02:16 da bitta partiyada atigi **90 ta xabar** saqlangan, hammasi o'sha kunning 15:24–17:01 oralig'idan (~1.5 soatlik "dum"), holbuki chat tarixi kamida 2026-07-17 gacha borardi; shu peer uchun `text_cache`da jami 71 qator (bir oyga siyrak tarqalgan). **(3) Yon topilma (bu holatga sabab emas, lekin yashirin nuqson):** `ShouldBackgroundCache()` global `antiDelete` bayrog'ini HISOBGA OLMAYDI (faqat Whitelist yoki per-peer override'da true qaytaradi), `ShouldAntiDelete()` esa oladi — ya'ni faqat global tugmaga tayangan foydalanuvchida AntiDelete "yoqilgan" ko'rinadi, lekin fon-cache umuman ishlamaydi. **(4) Sozlama holati (registry):** global `antiDelete` = **false**; `anti_delete: true` (snake_case) — eski/o'lik kalit, kod uni o'qimaydi (`custom_settings.cpp:200` faqat `antiDelete`ni o'qiydi). Faol qamrov Whitelist-kategoriya (User) orqali ta'minlanmoqda. | 🔨 **KOD YOZILDI, BUILD KUTILMOQDA (2026-08-13).** Phase 1 (root cause) yakunlandi, spec (`specs/2026-08-13-antidelete-archive-hardening-design.md`) va reja (`plans/2026-08-13-antidelete-archive-hardening-plan.md`) yozildi, **12 vazifadan 10 tasi implement qilindi va push qilindi**. Bajarilgan: K1 ko'rsatish tuzatildi (`_deletedInjectionReady` bayrog'i — `isEmpty()` qulfi o'rniga; `History::clear()` da `Unload` holati himoyalandi); K5 sozlama zanjiri to'g'rilandi (D3) + o'lik snake_case kalitlar migratsiyasi (D6); K6 `text_cache.is_archived` ustuni (v6 migratsiya, mavjud `RunMigrations` naqshi bilan), pruning arxivga tegmaydi (D4), `Checkpoint()` + davriy WAL checkpoint (D5); K2 yangi `custom_archive` moduli (partiyali yozuv, bitta tranzaksiya) + scrollback hook'lari (`addOlderSlice`/`addNewerSlice`, faqat `createItems()` qaytargan yangi xabarlar) + chiquvchi xabarlar hook'i (`HistoryItem::setRealId`) + real-vaqt yo'li helper'ga birlashtirildi; K4 media avtomatik yuklash; K5.3/K6.3 `TrackingReason()` va arxiv statistikasi UI. **Task 10 (K3 — "butun tarixni arxivla" tugmasi) ATAYLAB KECHIKTIRILDI** — xotira xavfi (butun tarixni `HistoryItem` sifatida xotiraga yuklash); sabab reja faylida batafsil. **Muhim trade-off:** `ShouldBackgroundCache()` ilgari global bayroqni ATAYLAB e'tiborsiz qoldirardi (disk tejash uchun) — endi hisobga oladi, ya'ni global tugma yoqilsa barcha chatlar cache'ga tushadi. Hozirgi sozlamada global `antiDelete=false`, shuning uchun darhol ta'siri yo'q. **Qolgan:** Task 12 — build + 10 bandlik qo'lda tekshiruv (A6 va A11 Task 6 bilan bitta build'da). | 🔬 **Phase 1 (root cause) tafsiloti:** Dalillar: kod o'qildi (`history.cpp`, `data_session.cpp`, `custom_db.cpp`, `custom_settings.cpp`) + real DB (`%APPDATA%\TelegramDesktop\CustomMod\actioned_messages.db`, 155 MB) read-only rejimda so'rovlar bilan tekshirildi + Windows registry sozlamalari o'qildi. **Yaxshi xabar:** o'sha chatning 98 ta xabari (jumladan oxirgi 90 talik suhbat) DB'da SAQLANGAN va tiklanishi mumkin — faqat ko'rsatish yo'li bloklangan. **Keyingi qadam:** tuzatish yondashuvi bo'yicha user qarori kutilmoqda (Phase 3/4 hali boshlanmagan). |
| A13-eski | 🔴 (Dastlabki, aniqlashtirilgunga qadar bo'lgan tavsif — arxiv uchun saqlandi) **AntiDelete matnli xabarda ishlamadi:** shaxsiy (1-on-1) chatda suhbatdosh MATN xabar yozdi va o'chirdi — CustomMod tdesktop (Windows) buni SAQLAB QOLA OLMADI (AntiDelete kutilgan natijani bermadi), garchi bu funksiya aynan shunday holat uchun mavjud. Parallel: user mobil qurilmadan "aka messenger" deb atalgan boshqa Telegram klient/akkountga ham kirgan edi — **o'sha klient xabarni ushlab qololgan/saqlab qolgan** ("aka messenger" nima ekani aniq emas — boshqa qurilmadagi boshqa Telegram klientmi yoki oila a'zosining ilovasimi, keyingi sessiyada aniqlashtirish kerak). Qo'shimcha aniqlangan tafsilot: kompyuter keyinroq **to'g'ridan-to'g'ri o'chirilgan** (ilovani quit qilmasdan, kompyuterni butunlay o'chirish) — LEKIN bu o'chirish chatdagi yozishmadan **ancha vaqt keyin** sodir bo'lgan (ya'ni bevosita ketma-ketlikda emas). Bu detal muhim gipotezani tekshirish uchun: agar AntiDelete faqat xotirada saqlab, faqat "clean quit"da diskka yozsa — istalgan hard-kill (hatto ancha keyin bo'lsa ham) shu oraliqdagi barcha saqlanmagan yozuvlarni yo'qotgan bo'lardi; agar darhol DB'ga yozsa — bu hodisa buni tushuntira olmaydi va boshqa root cause qidirish kerak. | 🆕 **YANGI (2026-08-13), investigatsiya BOSHLANMAGAN.** `superpowers:systematic-debugging` chaqirilgan edi, lekin Phase 1 boshlanishidan oldin user to'xtatib, docs-update so'radi (model Sonnet→Opus almashtirilmoqda). **Muhim kod-kontekst (tasdiqlanmagan, faqat eslatma):** AntiDelete/AntiEdit CustomMod'ning OLDIN implement qilingan (bu sessiyada yozilmagan) funksiyasi. Ma'lum bog'liq kod: `data_document.cpp`dagi `finishLoad()` hook (`CustomSettings::AntiDelete()` bilan gate qilingan) — LEKIN bu MEDIA/document fayllar uchun, matnli xabarlar uchun EMAS — matnli-xabar AntiDelete alohida kod yo'lidan borishi kerak (hali topilmagan/tasdiqlanmagan). Memory'da eslatma bor: "O'chirilgan/tahrirlangan xabarlar restart'da saqlash: `addOlderSlice`/`addNewerSlice`ga `loadDeletedMessages()` hook qo'shilgan" — bu DB-asosidagi, restart-dan keyin ham saqlanadigan tuzilma borligini ko'rsatadi, lekin write-path (xabar o'chirilishini ushlab qolish/DB'ga yozish) qayerda ekani hali tekshirilmagan. **Keyingi qadam:** systematic-debugging Phase 1 — `log.txt`ni tekshirish, matnli-xabar AntiDelete kod yo'lini topish (`custom_*.cpp` fayllar ichida qidirish, ehtimol `history.cpp`/`data_session.cpp`dagi message-delete update handler'iga bog'liq), aniq reproduksiya shartlarini so'rash. |
| A12 | 🔴 **KRITIK — moliyaviy ta'sirli crash:** Telegram Stars sovg'a qilish (gift) jarayonida ilova qulaydi. Reprodutsiya (2026-08-09, foydalanuvchi tomonidan): (1) CustomMod client'da stars sotib olib boshqa userga gift qilishga urinilganda — Visa kartadan pul yechildi, asosiy oynaga qaytganda **crash**, qayta ishga tushirilganda stars **noto'g'ri o'z profiliga** kredit bo'lgan (gift qilinmagan). (2) Xuddi shu urinish **rasmiy (official, tuzatilmagan) tdesktop client'da qaytarilgan** — yana Visa'dan pul yechildi, gift qabul qiluvchi chatida 100 stars paydo bo'lgani zahoti yana **crash** (to'liq yopilish), lekin qayta ishga tushirilganda bu safar gift **to'g'ri yetib borgan**. | ⏸️ **PAUZA QILINDI (2026-08-09).** `telegramdesktop/tdesktop` GitHub issue'lari tekshirildi (`gh search issues`) — aniq mos crash-report topilmadi. Eng yaqin: [#29972](https://github.com/telegramdesktop/tdesktop/issues/29972) "Concurrency issues in Telegram marketplace purchases" (stars yechilib gift boshqa/noto'g'ri joyga borishi) — lekin bu **server-tomon (MTProto) race condition** deb topilib, maintainer "bu tdesktop-specific emas, bugs.telegram.org'ga" deb yopgan, hech qanday fix/PR yo'q. User qarori: agar mavjud ma'lumot topilmasa, o'zimiz resurs sarflab debug qilmaymiz — bu ehtimol upstream/server xatosi, rasmiy tomondan tuzatilsa keyingi sync orqali o'zi keladi. **Keyinroq qayta ko'rib chiqish mumkin** agar: (a) user yana takrorlasa va aniqroq repro/crash-log to'plansa, (b) GitHub'da keyinchalik tegishli issue/fix paydo bo'lsa. |

**v7.0.7 bilan kelgan yangi bog'liqliklar:** `Telegram/ThirdParty/libcbor`
va `libfido2` (FIDO2/passkey). Build muammosiz o'tdi.

**v7.0.9 sync bilan kelgan muhim o'zgarish:** `Customizations` branch'i
eski (Saidjon-davri) tarixdan tozalanib, `Oybek`ga tekislandi —
force-push qilindi. `Telegram/lib_ui` submodule'i endi rasmiy
`desktop-app/lib_ui` o'rniga bizning fork'imizga (`Oybek-M/lib_ui`)
ishora qiladi — bizning Qt5 moslik patch'imiz uchun (rasmiyda yo'q,
faqat fork'da). Tafsilot: `docs/self-update/HANDOFF.md` §2.5.

---

## B — Self-update mexanizmi ✅ Windows tayyor, Linux/macOS bloklangan

Har rebuild'dan keyin ilovani fleshka/cloud orqali qo'lda tarqatish
muammosini hal qiladi. Bir nechta desktop qurilma va boshqa
foydalanuvchilar (masalan aka) paydo bo'lganda bu muammo keskinlashadi.

**Holat (2026-08-08):** Windows uchun **to'liq ishlab turibdi,
production'da, va endi to'liq real-world update-apply sinovidan
o'tgan** (v7.0.7 → v7.0.9, haqiqiy "Check for Updates" tugmasi orqali,
fayl almashtirish bosqichigacha tasdiqlangan — ilgari faqat signature
tekshiruvi sinalgan edi). Yo'lda topilgan `publish.ps1` path bug'i
(§0.1) tuzatildi. Kirish nuqtasi:
[`../self-update/HANDOFF.md`](../self-update/HANDOFF.md).

Keyingi versiyani chiqarish bitta buyruq: `.\tools\publish\release.ps1`
(tdesktop repo ichida, `out\Release` build qilingandan keyin).

**Linux/macOS (Task 5-6):** kod ko'rib chiqilgan, qo'shimcha
o'zgartirish kerak emasligi tasdiqlangan, lekin build/sinov qadamlari
**Linux/macOS mashina yo'qligi sababli bloklangan** — Packer.exe'ning
chiqish fayl nomi compile-time aniqlanadi, cross-compile qilib
bo'lmaydi. Mashina paydo bo'lganda davom ettiriladi.

**A yo'nalishiga bog'liqligi:** yo'q. Update server oddiy statik fayl
(nginx) — B yo'nalishiga ham bog'liq emas, mustaqil bajarilishi mumkin.

---

## C — Multi-device sync ekotizimi (5 app) ⏸️ NAVBATDA (3-bosqich)

Spec va planlar **to'liq yozilgan**, implement boshlanmagan.
2026-08-13: foydalanuvchi bilan ustuvorlik qayta rejalashtirildi —
Track A/Qt (A6) to'liq va bitta build bilan yakunlangach boshlanadi
(yuqoridagi "Umumiy ustuvorlik tartibi" bo'limiga qarang).

**Repo joylashuvi — KELISHILDI (2026-08-13):** yangi top-level papka
```
C:\Users\Oybek\Documents\Projects programming\customsync-server
```
(Foydalanuvchi taklif qilgan ikkita variant — `...\Telegram\` va
`...\Telegram\Telegram\` — rad etildi: ikkalasi ham aslida
tdesktop'ning o'z C++ build-scaffold'i, `DesktopPrivate` maxfiy
signing-kalitlari bilan bir joyda, umumiy "Telegram loyihalari" uyi
emas. `Telegram BOTs\` konvensiyasiga o'xshab, mustaqil top-level
papka tanlandi.)

**Hujjatlar:**
- Spec: [`specs/2026-07-29-multi-device-sync-backend-design.md`](specs/2026-07-29-multi-device-sync-backend-design.md)
- Planlar: [`plans/2026-07-29-multi-device-sync-00-index.md`](plans/2026-07-29-multi-device-sync-00-index.md) va undan keyingi 6 ta fayl

**Muhim:** bu ish **tdesktop repozitoriysida bajarilmaydi.** Backend,
web app va capture service **alohida papkada** (yuqorida) yaratiladi.

> ⚠️ **Implement qilishni boshlashdan oldin foydalanuvchidan yakuniy
> ruxsat so'rash SHART** — papka joylashuvi kelishilgan bo'lsa ham,
> boshlash vaqtini alohida tasdiqlaydi.

**Boshlash tartibi (foydalanuvchi ruxsatidan keyin, A6+build
yakunlangach):**
1. `01a` — backend poydevori
2. `01b` — backend sync yadrosi
3. `02` — tdesktop sync agenti (bu qism tdesktop repo'sida)
4. `03` — web controller
5. `04` — storage lifecycle
6. `05` — always-on TDLib capture service

---

## D — Mobil klientlar 🕓 KEYINGA QOLDIRILGAN

`tmobile-android` (exteraGram fork) va `tmobile-ios` (Telegram-iOS fork).

**Holat:** hali muhokama qilinmagan. Foydalanuvchi aniq aytdi —
tafsilotlarni keyin batafsil muhokama qilamiz.

**Allaqachon ma'lum bo'lgan cheklovlar** (C yo'nalishi muhokamasida
aniqlangan, muhokama boshlanganda esga olinsin):
- iOS ilovani to'xtatadi va Telegram o'chirish hodisasi uchun push
  yubormaydi → iOS'da capture tabiatan to'liq bo'lmaydi, u amalda
  asosan **viewer** bo'lib qoladi
- Samsung Note 10+ batareya boshqaruvi Android fon xizmatini o'ldiradi
- iOS uchun macOS + Xcode kerak (foydalanuvchida bor)

---

## Doimiy qoidalar

- Build ishga tushirishdan **oldin doim so'rash** — ~34 daqiqa oladi va
  boshqa og'ir ilovalar bilan raqobatlashadi
- Commit + push faqat `origin/Oybek` ga; `upstream` ga **hech qachon**
- Commit'larda `Co-Authored-By` trailer **ishlatilmaydi**
