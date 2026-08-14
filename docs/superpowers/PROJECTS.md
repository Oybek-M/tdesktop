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

## 🔴 HOZIRGI ISH — Qt6 build'idan keyingi 4 ta muammo (2026-08-14)

**Qt6 build MUVAFFAQIYATLI** (`Telegram.exe` 219.5 MB, 14.08 13:20,
`EXIT=0`). Foydalanuvchi uni ishlab turgan nusxaga ko'chirdi
(`C:\Users\Oybek\Pictures\Release\`, eskisi `Telegram.exe.qt5-zaxira`
sifatida saqlangan — orqaga qaytish 5 soniya). Real foydalanishda
**4 ta muammo** topildi. Tartib foydalanuvchi bilan kelishilgan:
3 → 2 → 4 → 1.

| № | Muammo | Holat |
|---|---|---|
| **3** | Birinchi start sekin, bo'limlarga (Unread/Personal) birinchi o'tishda qisqa freez | ✅ **TEKSHIRILDI — regressiya emas.** Gipoteza "A13 arxiv hook'i DB'ga ko'p yozyapti" **rad etildi**: DB 150 MB (ertalabki 155 dan oshmagan), WAL atigi 0.4 MB. Kod yo'li ham buni tasdiqlaydi — `MaybeArchiveItem` `addNewMessage`da faqat `NewMessageType::Unread` uchun, `addOlderSlice`da esa chat OCHILGANDA ishlaydi; startup'da dialoglar `Existing` turida keladi. Haqiqiy sabab — **Qt6 keshlarini bir martalik qayta qurish** (shrift/glif/ikonka keshlari Qt versiyasi o'zgarganda bekor bo'ladi). Foydalanuvchi ikkinchi start'ni sinab ko'rdi: **tezroq ochildi** → gipoteza tasdiqlandi. Qo'shimcha performance yaxshilash **oxiriga surildi** (user qarori).<br><br>**2026-08-14 (2-bosqich, kod yozildi, build kutilmoqda):** o'z kodimizdagi issiq yo'l topildi — `GetDeletedMessages()` `History::loadDeletedMessages()` dan, u esa `addOlderSlice`/`addNewerSlice` ichidan chaqiriladi, ya'ni **har bir scroll bo'lagida bitta SQLite so'rovi**, chatlarning aksariyatida esa o'chirilgan xabar umuman yo'q. `custom_db.cpp` ga `gPeersWithDeleted` filtri qo'shildi: bitta `DISTINCT` so'rov (`idx_am_peer_type` indeksidan foydalanadi) bilan bir marta to'ldiriladi, keyin peer to'plamda bo'lmasa so'rov **umuman ketmaydi**. Yolg'on-manfiy bo'lishi mumkin emas: barcha insert nuqtalari (`SaveActionedMessage` + `MarkDeleted` ning UPDATE yo'li) va tozalash nuqtalari (`LoadRestoreCache`/`ClearDeletedArchive`/`ClearAllArchive`) filtrni yangilaydi. `GetPeersWithDeletedMessages()` ham shu keshdan foydalanadi → `RestoreDeletedChats` birinchi yuklashdan keyin bepul. |
| **2** | DB'dan tiklangan chat ("Xurshida \| V", peer `7815103103`) asosiy chatList'da ko'rinmaydi — faqat qidiruvdan topib ochgandan keyin paydo bo'ladi | ✅ **TUZATILDI (kod yozildi, build kutilmoqda).** Ildiz sabab: `loadDeletedMessages()` faqat `History` obyekti YUKLANGANDA ishlaydi, `History` esa chat ochilganda yaratiladi. Yechim (K1b): `CustomDB::GetPeersWithDeletedMessages()` (arzon DISTINCT so'rov) + `CustomArchive::RestoreDeletedChats()` — har peer uchun History yaratib inject qiladi, so'ng `refreshChatListEntry()` bilan ro'yxatga qo'shadi; `folderKnown()` false bo'lsa `requestDialogEntry()` (aks holda `Expects(folderKnown())` yiqilardi). Chaqiruv `chatsListLoadedEvents()` ga bog'langan, sessiya konstruktoriga EMAS.<br><br>**2026-08-14 qayta tekshiruvda TOPILGAN XATO (tuzatildi):** obunada `rpl::take(1)` `filter`dan OLDIN turgan edi. Bu oqim **arxiv papkasi uchun ham otiladi** (`folder != nullptr`), shuning uchun birinchi hodisa arxivniki bo'lsa `take(1)` o'shani yutib obunani tugatardi va asosiy ro'yxat hodisasi hech qachon kelmasdi → fix umuman ishlamas edi. Endi tartib `filter` → `take(1)` → `on_next`, ya'ni kodbazadagi boshqa `chatsListLoadedEvents` obunachilari bilan bir xil (`boxes/peer_list_controllers.cpp:452`). |
| **4** | "🔔 Rasmiy versiya tekshiruvi" (A9) ishlamaydi: **"Tekshirib bo'lmadi: Connection closed"** | ⏳ **NAVBATDA.** Birinchi gipoteza (`cmake/external/qt/CMakeLists.txt` dagi `if (QT_VERSION GREATER 6)` — `6.11.1` satr, CMake uni 0 deb hisoblaydi → TLS plagini ulanmay qoladi) **TEKSHIRILDI VA RAD ETILDI**: `Telegram.vcxproj` da `qschannelbackend`, `qnetworklistmanager`, `Qt6EntryPoint`, `qwindows` — hammasi ULANGAN. **2026-08-14: ildiz sabab topildi va tuzatildi (build kutilmoqda).** Dalil zanjiri: (a) xato matni `"Connection closed"` = `QNetworkReply::RemoteHostClosedError` — ya'ni TCP **va** TLS o'rnatildi, so'ng server ulanishni yopdi; TLS backend yo'q bo'lganda xato `"TLS initialization failed"` bo'lardi; (b) `prepare.py:1693` — Windows Qt6 `-openssl linked` bilan quriladi, demak TLS backend joyida; (c) Qt5 → Qt6 da o'zgargan yagona tegishli standart: `QNetworkRequest::Http2AllowedAttribute` Qt5'da `false`, **Qt6'da `true`**. Ya'ni ALPN orqali `h2` kelishildi va o'sha seans uzildi. Yechim: `custom_upstream_checker.cpp` da HTTP/2 atayin o'chirildi (GitHub API HTTP/1.1 ni to'liq qo'llaydi — yo'qotish yo'q). Qo'shimcha: **20s transfer timeout** (ilgari javob kelmasa `finished` otilmay, `QNetworkAccessManager` hech qachon o'chirilmasdi — sizib ketish) va xato matniga **xato kodi + HTTP status** qo'shildi, shunda qayta yiqilsa bitta ishga tushirishning o'zi sababni ko'rsatadi. **Muhim:** bu self-update'ni BUZMAYDI — A9 rasmiy tdesktop relizlarini GitHub'dan tekshiradi, o'z yangilanish kanalimiz (3 mirror) esa mutlaqo alohida mexanizm. |
| **1** | Story ko'rayotganda orqa fon bug'i: blur ↔ shaffof holat juda tez almashadi | ⏳ **OXIRGI va YAGONA qolgan.** Ehtimol Qt6 render farqi (RHI/OpenGL). Eng noaniq va qimmat — shuning uchun oxirga qo'yilgan. |

### ✅ 2026-08-14 18:09 build — 2, 3, 4 REAL SINOVDAN O'TDI

Foydalanuvchi tasdiqladi: tiklangan chat qidiruvsiz chatList'da paydo bo'ldi (2),
rasmiy versiya tekshiruvi ishladi (4), scroll **sezilarli silliqroq** (3).
Crash yo'q. Faqat 1-muammo qoldi.

**Build yo'lidagi 4 ta to'siq** (hammasi hal qilindi, tafsilot commit'larda):
Debug konfiguratsiyasi → `dnsapi.lib` yo'qligi (LNK1120) → `QT` user env
5.15.18 bo'lgani (CMake Qt5/Qt6 to'qnashuvi, 22×MSB8066) → disk yetmasligi
(LNK1180, tasodifiy Debug build 22 GB egallagan edi).

### 🔴 Crash: 10 MB dan katta media (topildi va tuzatildi — `bfe0789969`)

`log.txt:206` → `Assertion Failed! "!_filename.isEmpty() || (_fullSize <=
Storage::kMaxFileInMemory)" file_download.cpp:114`.

`DocumentData::save()` ga **bo'sh maqsad nomi** = "xotiraga yukla".
Keshlanmaydigan hujjat uchun `save()` `LoadToFileOnly` ni tanlaydi
(`data_document.cpp:1292`), `FileLoader` esa bo'sh nomni faqat fayl
≤ 10 MB bo'lgandagina qabul qiladi. Yo'l: `addOlderSlice` →
`MaybeArchiveItem` → `MaybeDownloadMedia`. Saved Messages'da katta
fayllar ko'p → deyarli har safar yiqilardi. Xuddi shu xato A11 story
backup'da ham bor edi (`custom_activity_history.cpp`) — ikkalasi tuzatildi.

**Ataylab qilingan scope kamayishi:** >10 MB media endi avtomatik
arxivlanmaydi. MUHIM: `DocumentData::finishLoad()` (`data_document.cpp:1064`)
hook'i saqlanib qoldi — ya'ni foydalanuvchi **ochgan/yuklagan** media
istalgan hajmda arxivlanaveradi. Yo'qolgan yagona narsa — hech qachon
ochilmagan katta faylni oldindan yuklash.

---

## 🔵 NAVBATDAGI ISH: Katta media backup (spec YOZILMAGAN)

Yuqoridagi scope kamayishini yopish uchun. **User bilan kelishilgan
qarorlar** (spec shu asosda yoziladi):

**Uch qatlamli dizayn** — bir-birini almashtirmaydi, to'ldiradi:
1. *Bepul bazaviy* — `finishLoad` hook'i (ALLAQACHON ISHLAYDI)
2. *Asosiy* — ro'yxatdagi chatlarda oldindan yuklash, **haqiqiy maqsad
   fayl yo'li bilan** (bo'sh nom = crash, yuqoriga qarang)
3. *Oxirgi imkoniyat* — o'chirish aniqlanganda urinib ko'rish.
   Ishonchsiz (`file_reference` eskiradi), lekin urinish tekin

**Ro'yxatga bog'lanish** (user qarori — alohida ro'yxat SHAKLLANTIRILMAYDI):
```
YOQILADI: chat White List'da  YOKI  per-chat "Media Backup" toggle
HECH QACHON: Black List'da  YOKI  Saved Messages (peer->isSelf())
```
🔴 **`ShouldAntiDelete()` zanjiriga ERGASHMAYDI** — unda global bayroq
bor, unga ergashsa global AntiDelete yoqilganda 981 ta peer'dan
(asosan botlar) video yuklardi. White List'da atigi 5 ta chat bor,
global bayroq esa boshqa narsa — bu ikkisi chalkashtirilmasin.

Per-chat toggle "Individual sozlamalar" bo'limiga tushadi (Ghost Mode /
Anti-Delete / Anti-Edit yoniga 4-qator).

**Sozlamalar** (ikkalasi ham Custom Window'da o'zgartiriladi):
| Sozlama | Default |
|---|---|
| Bitta fayl chegarasi | 100 MB |
| Umumiy kvota | 10 GB |
| Kvota to'lganda | Hech narsa O'CHIRILMAYDI; muammo hal bo'lmaguncha har ishga tushishda alert |

**Texnik tuzoq:** faylni to'g'ridan-to'g'ri arxiv papkasiga yuklasak,
`finishLoad` uni YANA `SaveMediaFile()` bilan nusxalaydi (fayl o'zini
o'ziga). Manba arxiv daraxti ichida bo'lsa hook'ni o'tkazib yuborish kerak.
Shu bilan birga o'sha hook'dagi xato ham tuzatiladi: u faqat **global**
`AntiDelete()` ni tekshiradi, per-peer emas — ya'ni global o'chiq, chat
whitelist'da bo'lsa media saqlanmaydi.

**Export/import talabi (user):** bularning hammasi Archive tab'idagi
import/export'ga qo'shilishi kerak, chunki Track C (customsync-server)
uchun kerak bo'ladi. **HAL QILINMAGAN SAVOL** — eksportga fayllar
kiradimi yoki faqat indeks? Tavsiya: ajratish. *Indeks* (yo'l, hajm,
hash, peer, msgId, sana) — doim eksport, bir necha MB, server orqali
bemalol sinxronlanadi. *Fayllar* — ixtiyoriy, alohida; blob storage
alohida infratuzilma talab qiladi va Track C ni bloklamasligi kerak.

### Yangi versiyani boshqa qurilmalarga tarqatish (user savoli, 2026-08-14)

Hozir barcha qurilmalarda `v7.0.9`, rasmiyda ham shu. **Versiyani
ko'tarmasdan tarqatib bo'lmaydi** — self-update versiya raqamini
solishtiradi, teng bo'lsa yangilanish taklif qilinmaydi.

Versiya `Telegram/build/version` faylida, `release.ps1` o'shandan oladi.
**Ehtiyot bo'lish kerak bo'lgan joy:** oddiy `7.0.10` qilinsa, rasmiy
tdesktop keyinchalik haqiqiy `7.0.10` chiqarganda ma'no to'qnashuvi va
upstream sync'da version-fayl konflikti bo'ladi. tdesktop'da bu holat
uchun alpha/beta hisoblagichi bor — asosiy versiyani saqlab, faqat
fork-build raqamini oshirish mumkin. **Aniq mexanizm reliz vaqtida
`version` fayli va `set_version.py` o'qilib tanlanadi — hozircha taxmin
qilinmadi.**

**Tartib:** avval 4 ta muammo tuzatiladi, keyin versiya ko'tariladi va
`release.ps1` bilan 3 mirror'ga chiqariladi.

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

✅ **YAKUNLANDI 2026-08-14 13:20** — `prepare` 32/32, `configure` va
to'liq build `EXIT=0`, `Telegram.exe` 219.5 MB. Yo'lda 9 ta to'siq
yengildi; ulardan uchtasi Claude'ning o'z xatolari edi (toolset
nomuvofiqligi, `lib_ui` ni master uchiga qadash, `data_session.h`
include). Batafsil ro'yxat quyida.

**Kelajak uchun muhim bilim:**
- Build skripti shart: `set QT=6.11.1` (aks holda CMake qayta
  generatsiyada Qt5 so'raydi), `set TDESKTOP_BUILD_JOBS=6`, `silent`
  argumenti, `-vcvars_ver` YO'Q, `prepare.py` to'liq yo'l bilan.
- Configure: `configure.bat qt6 -G "Visual Studio 18 2026" -Ax64 -Tv145 -D ...`
  — `-Ax64` **qo'shib** yozilishi shart (`-A x64` bo'lsa `run_cmake.py`
  dagi `arg == 'x64'` shartiga ilashib xato beradi). `-G` berilishi
  `run_cmake.py` ning qattiq qadalgan `-T v143` blokini chetlab o'tadi —
  shu sabab `cmake_helpers` submodule'ini patch qilish SHART BO'LMADI.
- `qt_6.11.1` (manba+build, 30 GB) o'chirilgan; `Qt-6.11.1` (SDK,
  4.1 GB) saqlanadi — build aynan shuni ishlatadi (`QT_DIR`).
- Qt5 fayllari (8.2 GB) **ataylab saqlangan** — Qt6 to'liq sinovdan
  o'tguncha orqaga qaytish yo'li.
- Disk: to'liq build uchun kamida ~20 GB, xavfsizi 30-35 GB kerak.

**Yo'lda yengilgan to'siqlar (tarix):**
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

✅ **BLOKER YECHILDI (2026-08-14, ~07:30):** foydalanuvchi **MSVC v143 —
VS 2022 C++ x64/x86 build tools (v14.44-17.14)** ni o'rnatdi (VS Installer →
Modify → Individual components). Aynan `14.44` tanlandi, chunki
`docs/building-win.md` aynan shu versiyani talab qiladi
(`-vcvars_ver=14.44`). Tasdiqlangan holat:
- `VC\Tools\MSVC\` da endi **14.44.35207** (1647 fayl) va 14.51.36231 bor
- MSBuild PlatformToolsets: `v170 → v143`, `v180 → v145`
- `vcvars64.bat` ishlaydi (`Environment initialized for: 'x64'`)

✅ **SINOVDAN O'TDI (2026-08-14 ~07:40):** `libvpx` bosqichi alohida
ishga tushirilib, **muvaffaqiyatli qurildi** —
`prepare.py qt6 silent libvpx` → `Build succeeded` (Debug|x64 va
Release|x64), `[INSTALL] .../local/lib/x64/vpxmt.lib`, `EXIT=0`.
`MSB8020` umuman chiqmadi. **Bloker aniq va to'liq yopildi.**

(Eslatma: bundan oldingi sinov `VCVARS_FAILED` bergan edi — sabab v143
emas, balki sinov VS Installer o'rnatishni tugatmasdan turib ishga
tushirilgani. Dars: VS komponenti o'rnatilgach, `Get-Process setup`
bo'shaganini kutish kerak.)

**KEYINGI QADAMLAR (aniq tartibda):**
1. **To'liq prepare:** qolgan bosqichlar (libvpx'gacha bo'lgan 20 tasi va
   endi libvpx ham cache'da — qayta qurilmaydi) — pastdagi build skripti
   bilan.
4. `configure.bat x64 qt6 -D TDESKTOP_API_ID=... -D TDESKTOP_API_HASH=...`
5. To'liq loyiha build'i (~34+ daqiqa).
6. **Qo'lda tekshiruv** — uchta ish birga: A6 (Qt6 regressiyalari),
   **A11 Task 6** (story signal), **A13** (10 bandlik ro'yxat,
   `specs/2026-08-13-antidelete-archive-hardening-design.md` §6).

**Ixtiyoriy:** v14.44 o'rnatilgach build skriptiga `-vcvars_ver=14.44`
bayrog'ini qaytarish mumkin — shunda muhit `docs/building-win.md` bilan
aynan mos keladi (Windows 7 mosligi ham qaytadi). Hozir usiz ishlayapti.

⛔ **Eski bloker tavsifi (tarix uchun) — `v143` toolset yo'qligi (3-marta chiqqan edi):**
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

**User qarori (2026-08-14):** 1-variant tanlandi va **bajarildi** —
v14.44 o'rnatildi (yuqoriga qarang).

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
