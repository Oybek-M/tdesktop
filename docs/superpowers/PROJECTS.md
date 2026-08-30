# Loyiha yo'nalishlari — holat jadvali

Bu fayl qaysi ish **hozir faol**, qaysi biri **to'xtatib qo'yilgan** va
qaysi biri **hali muhokama bosqichida** ekanini ko'rsatadi. Yangi
sessiya boshlanganda birinchi shu yerga qarang.

Oxirgi yangilanish: 2026-08-28

---

# ✅ TDESKTOP TOMONI TOZA (2026-08-30)

**Barcha ochiq ishlar yopildi, build va sinovdan o'tdi.**

Build: 2026-08-30 15:11 (`Pictures\Release\Telegram.exe`).
Jonli baza sxemasi: **v12**, `integrity: ok`.

| Ish | Holat |
|---|---|
| **A16 §1** — story vaqti `status` shkalasiga | ✅ 92 nuqta, 📖 belgisi bilan |
| **A16 §2** — qo'lda yozuv + `🗑 O'chirish` | ✅ o'chirish faqat retroaktiv yozuvlarda chiqadi |
| **A16 §3** — tez tugma + vaqtinchalik bufer | ✅ |
| **A15** — bot token | ✅ savol yopildi: `BOT_METHOD_INVALID`. Kod ogohlantirish bilan saqlandi |
| Sxema **v11** (`source`) va **v12** (story backfill) | ✅ ikkala migratsiya toza o'tdi |
| Kompaksiya xatosi | ✅ shovqin filtri retroaktiv yozuvlarni yeb qo'yardi — tuzatildi va isbotlandi (23 → 0) |

Track C uchun keyingi bo'sh sxema versiyasi — **v13**.

## Yaqinda YOPILGAN ishlar

- ✅ **Sxema v10 — akkaunt izolyatsiyasi** (build o'tdi, sinovdan o'tdi)
- ✅ **Arvoh yozuvlar** — Akam chatida 218 → 15
  (`specs/2026-08-27-legacy-ghost-rows-followup.md`)
- ✅ **Baza buzilishi tuzatildi** — `EnsureArchiveLayout` begona WAL/SHM
  ni ko'chirardi; migratsiyadan oldin avtomatik zaxira qo'shildi

---

# ARXIV — TRACK C (2026-08-26 dagi yozuv)

**Track A (tdesktop) YOPILDI.** v7.1.1 chiqarildi, uchala mirror'da,
self-update ishlaydi va real sinovdan o'tdi.

## Track C — customsync-server

> WARNING: Spec va planlar **2026-07-31** da yozilgan — deyarli BIR OY
> oldin. O'shandan beri ko'p narsa o'zgardi (Qt6, v7.1.1, media
> arxivi, media_index, kvota, eksport formati v3). **Implement
> boshlashdan OLDIN ularni birga qayta ko'rib chiqish kerak** —
> foydalanuvchi shuni aniq so'radi.

**Hujjatlar:**
- Spec: `specs/2026-07-29-multi-device-sync-backend-design.md`
- Index: `plans/2026-07-29-multi-device-sync-00-index.md`
- 01a backend poydevori / 01b sync yadrosi / 02 tdesktop agenti
  03 web controller / 04 storage lifecycle / 05 capture service

**Papka (HAQIQIY):** `Projects programming\Telegram\customsync-server`
— alohida repo, tdesktop git tarixiga kirmaydi.

🟢 **Track C FAOL implementatsiyada** (2026-08-28). 01a tugagan,
01b Task 1-5 tugagan, keyingi qadam 01b Task 6 (WebSocket).
**Aniq holat: o'sha loyihaning `PROGRESS.md` faylida** — bu yerdagi
yozuvlar eskirishi mumkin.

**Boshlash tartibi:** 01a -> 01b -> 02 -> 03 -> 04 -> 05

## Qayta ko'rib chiqishda albatta hisobga olinadigan yangiliklar

| Nima | Nega Track C ga ta'sir qiladi |
|---|---|
| `media_index` (schema v8) | Sync uchun tayyor tuzilma — peer/msg/hajm/holat |
| Eksport formati v3 | `settings.json` + `index.json`, platformadan mustaqil — ATAYLAB Track C uchun shunday qilingan |
| Kvota tizimi | Serverga ham kerak bo'ladi |
| Arxiv ildizi sozlanadi | Sync yo'llari qattiq kodlanmasin |
| Reliz API g'oyasi | Server vazifalariga qo'shildi (pastga qarang) |

## tdesktop tomonida Track C DAVOMIDA qilinadigan 6 ta ish

Pastdagi "TRACK C DAVOMIDA" bo'limiga qarang. Ular Track C ni
bloklamaydi, lekin server ishlayotganda tdesktop'da baribir
o'zgarish kerak bo'ladi — shuning uchun birga qilinadi.

---

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

## 📚 ARXIV — umumiy ustuvorlik tartibi (2026-08-13)

> ESKIRGAN: 1- va 2-bosqich bajarildi, Track A yopildi.
> Joriy reja fayl boshida.

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
   `C:\Users\Oybek\Documents\Projects programming\Telegram\customsync-server`
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

## 📚 ARXIV (2026-08-14) — Qt6 build'idan keyingi 4 ta muammo

> Bu bo'lim TUGAGAN ishning tarixi. Joriy holat yuqorida.

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

## 🟡 QOLGAN ISHLAR: Story bo'limi (2 ta, ikkalasi ham story bilan bog'liq)

### S1. Story viewer UI bug (= eski "1-muammo")

**Alomat:** story ko'rayotganda orqa fon blur ↔ shaffof holat o'rtasida
juda tez almashadi (miltillaydi).

**Ma'lum:** Qt5 build'da bunday bo'lmagan — ya'ni Qt6 regressiyasi.
Ehtimol RHI/OpenGL render yo'lidagi farq. Hech qanday tekshiruv hali
o'tkazilmagan, gipoteza ham yo'q.

**Boshlash nuqtasi:** `media/stories/` ostidagi fon chizish kodi va
`media_view_overlay_opengl.cpp` / `media_view_overlay_rhi.cpp` dagi Qt6
farqlari. Avval `log.txt` ni tekshirish kerak — miltillash paytida
render warning'lari chiqayotgan bo'lishi mumkin.

### S2. A11 story avtomatik yuklab olish — SINALMAGAN

Story media backup (`CustomActivityHistory::MaybeBackupStoryMedia`)
implement qilingan, lekin **hech qachon real sinovdan o'tkazilmagan**
(TaskList'dagi "A11 Task 6" hamon pending).

**Diqqat — 2026-08-14 da o'zgardi:** shu funksiyaga ham 10 MB chegarasi
qo'yildi (crash tuzatishi bilan birga, `custom_activity_history.cpp`).
Ya'ni **10 MB dan katta story videolari endi backup qilinmaydi**.
Sinovda buni hisobga oling — kichik story ishlashi, katta story esa
jimgina o'tkazib yuborilishi kerak (crash EMAS).

Sinov: `~/customizationMainFolder/medias/` ostida story fayllari
paydo bo'ladimi; Custom Window'dagi "Story media backup" toggle'i
haqiqatan ta'sir qiladimi.

**Bog'liqlik:** katta story'larni ham saqlash — bu quyidagi "Katta media
backup" ishining bir qismi (bir xil muammo, bir xil yechim: haqiqiy
maqsad fayl yo'li). Ikkalasini birga rejalashtirish mantiqiy.

---

## 📚 ARXIV (2026-08-15) — o'sha kungi boshlanish nuqtasi

> ESKIRGAN. Joriy boshlanish nuqtasi fayl BOSHIDA.

## ✅ 2026-08-15 kech — R1, R2, R2b, A11, versiyalash BAJARILDI

Kod tayyor, **BUILD QILINMAGAN**. Bitta build'da sinaladi.

| Ish | Natija |
|---|---|
| R1 | `RepairArchiveMedia()` — mazmundan MIME (MatchContent), kengaytma qo'shish, papka tuzatish, indeks yangilash. UI'da **dry-run + tasdiq**. |
| R2 | Shaxsiy chat nomlari sessiyadan olinadi |
| R2b | Eksport ro'yxatida ID nom ostida kichik shriftda |
| A11 | Story backup L2 ga o'tkazildi — endi katta story'lar ham saqlanadi, indeksga tushadi va eksport qilinadi |
| Versiya | **Kod kerak emas edi** — mexanizm mavjud. `docs/self-update/alpha-releases.md`. About tab'da to'liq versiya ko'rsatiladi. |

## 🆕 TRACK C GA QO'SHILDI — reliz yuklashni API ga o'tkazish

**Foydalanuvchi taklifi (2026-08-25).** `customsync-server` ning
vazifalari ro'yxatiga qo'shildi.

### Muammo (real hodisa, 2026-08-25)

Bitta reliz chiqarish uchun skript IKKI MARTA ishga tushirildi:

  1-yurish: vps-pub OK, github OK, **vps-secure FAILED**
            (`Connection reset ... scp: Couldn't send packet: Broken pipe`)
  2-yurish: vps-secure OK, vps-pub OK, **github FAILED** (clone uzildi)

Ikkalasi birga to'liq qamradi, lekin bu tasodif. Skript har yurishda
faqat O'SHA yurishni biladi — umumiy holatni bilmaydi va "FAILED"
deb yozadi, holbuki mirror allaqachon to'g'ri bo'lishi mumkin.

### Nega SSH/scp yomon

| Hozirgi | API bilan |
|---|---|
| 52 MB bitta scp seansida; uzilsa HAMMASI boshidan | Bo'laklab yuklash, uzilgan joydan davom |
| SSH kaliti kerak — faqat bitta odam chiqara oladi | Token bilan, ruxsat berilganlar ham |
| "broken pipe" — sabab noaniq | Aniq HTTP status + xato matni |
| Mirror holati qo'lda tekshiriladi | `GET /releases` — bir so'rovda |
| Uchta mirror = uchta alohida mantiq | Bitta API, tarqatishni server bajaradi |
| Idempotentlik yo'q — qayta yurish hammasini qayta yuklaydi | Checksum bo'yicha "allaqachon bor" javobi |

### Taxminiy shakl

```
POST /api/releases            reliz yaratish (versiya, platforma)
PUT  /api/releases/:id/blob   bo'laklab yuklash (resumable)
POST /api/releases/:id/publish  imzoni tekshirib, mirrorlarga tarqatish
GET  /api/releases            holat: qaysi mirror qaysi versiyada
```

**Muhim:** imzo LOKALDA qoladi. Server faqat tayyor, imzolangan
paketni qabul qiladi va tarqatadi — maxfiy kalit hech qachon
serverga chiqmaydi (`packer_private.h`, `alpha_private.h`).

Bu A10 (bitta tugmali pipeline) bilan tabiiy birlashadi: A10 ning
"3 mirror'ga chiqarish" qadami shu API chaqiruviga aylanadi.

---

## 📋 TRACK C DAVOMIDA tdesktop'da qilinadigan ishlar (5 ta)

Foydalanuvchi qarori: bular Track C ni BLOKLAMAYDI, lekin Track C
ishlayotganda tdesktop tomonida baribir o'zgarish kerak bo'ladi —
shuning uchun birga qilinadi.

| № | Ish | Holat |
|---|---|---|
| 1 | **Custom Window UX qayta dizayni** | Qaror qabul qilingan: mantiqiy guruhlar bo'yicha alohida tablar, barcha bo'limlar `SlideWrap` bilan YOPIQ holatda. 75 ta tushuntirish matni qisqartiriladi. |
| 2 | **S1 — story fonining miltillashi** | GL/RHI yo'liga qamalgan. Vaqtinchalik yechim bor (OpenGL o'chirish), ya'ni bloklamaydi. Eng noaniq ish. |
| 3 | **Priority lazy loader** | Tanlangan chat yuqori prioritetga, chat almashganda prioritet ko'chadi. Hozircha shoshilinch emas — og'ir yuk fonga ko'chirilgan. |
| 4 | **A10 — bitta tugmali sync+build+publish** | A9 ustiga quriladi. Har upstream sync'da qo'lda 4-5 qadam bajarilmoqda. |
| 5 | **A14 — o'qilgan vaqt orqali faollik** 🆕 | Pastda batafsil. Foydalanuvchi 2026-08-25 da topdi. |
| 6 | **Reliz yuklashni API ga o'tkazish** 🆕 | SSH/scp o'rniga `customsync-server` API. Yuqorida batafsil. |
| 7 | **A15 — bot token orqali kirish** 🆕 | Foydalanuvchi 2026-08-26 da so'radi. **v10 dan KEYIN.** Pastda batafsil. |
| 8 | **A16 — faollik kuzatuvidagi bo'shliqlar** | 🟢 **DEYARLI TUGADI** (2026-08-29). §3 va §2 sinovdan o'tdi, §1 backfill (v12) va o'chirish imkoni qurildi — sinov qoldi. 3 ta bo'shliq: story vaqti `status` ga tushmaydi, qo'lda yozuv qo'shish yo'q, ro'yxatga o'z vaqtida qo'sha olmaslik. **Qaror qabul qilingan:** tez tugma + vaqtinchalik bufer (`activityBufferMinutes`, standart 10, 1–120 daqiqa, Faollik tarixi bo'limida sozlanadi). Batafsil: [`specs/2026-08-28-a16-activity-capture-gaps.md`](specs/2026-08-28-a16-activity-capture-gaps.md) |

**Qo'shimcha (2026-08-26 da qo'shildi) — ENG USTUVOR:**
**Sxema v10 — `account_id`** + 3 ta media tuzatishi. 12 ta akkaunt
bitta bazaga yozadi, `account_id` ustuni yo'q → bir akkauntning
o'chirilgan xabarlari boshqasining chatida arvoh bo'lib chiqadi.
To'liq tashxis va reja:
`specs/2026-08-26-multi-account-db-isolation-design.md`.
Track C ga ham tegadi (`record_id` da akkaunt yo'q).

**Qo'shimcha (2026-08-25 da qo'shildi):**
**Startup 14.55 soniya** — sababi hali NOMA'LUM, log yordam bermaydi
(2s dan 14s gacha jim bo'shliq). Profiler yoki bosqichma-bosqich
vaqt o'lchash kodi kerak.

**Strukturaviy qarz:** media arxivi mantig'i UCHTA yo'lda alohida
yozilgan (L1 `finishLoad`, L2 `MaybeDownloadMedia`, L3
`TryRescueMedia`). Har tuzatish uchalasiga alohida qo'llanishi
kerak bo'lyapti va bir necha marta unutilgan. Birlashtirish kerak.

---

## 💡 A15 — bot token orqali kirish (2026-08-26)

**Foydalanuvchi so'radi.** Unigram va boshqa ba'zi forklarda mavjud.
**Tartib qarori: v10 (`account_id`) dan KEYIN** — sababi pastda.

### Holat: tekshirilmagan g'oya, implement qilinmagan

### Nima allaqachon tayyor

MTProto metodi sxemada bor — `mtproto/scheme/api.tl:2298`:

```
auth.importBotAuthorization#67a3ff2c flags:int api_id:int
    api_hash:string bot_auth_token:string = auth.Authorization;
```

Ya'ni `MTPauth_ImportBotAuthorization` allaqachon generatsiya
qilinadi. Kodbazada **hech qayerda ishlatilmagan**, `self()->isBot()`
tekshiruvi ham **umuman yo'q**.

### Login qismi arzon

`intro_qr.cpp:496` naqshi tayyor: bitta MTP so'rov →
`finish(authorization)` → `Step::createSession()`. Yangi
`BotTokenWidget : Step` shu naqshni takrorlaydi — taxminan **250
satr** + start ekranida havola. Sessiya yaratish, DC boshqaruvi va
ko'p akkaunt qo'llab-quvvatlashi umumiy koddan bepul keladi.

### 🔴 Asosiy risk: muammo logindan KEYIN

tdesktop UI'si butunlay "self = odam" taxminiga qurilgan.

**Hal qiluvchi noma'lum:** `messages.getDialogs` bot uchun ishlaydimi.
Agar ishlamasa — chat ro'yxati **bo'sh** qoladi va "to'liq klient"
g'oyasi qulaydi. Bu tekshirilmagan.

| Nima | Bot akkauntda |
|---|---|
| `messages.getDialogs` | ❓ **NOMA'LUM — birinchi tekshiriladigan narsa** |
| Tarixiy xabarlar | Bot chatga qo'shilgunga qadar bo'lganini olmaydi |
| Guruh xabarlari | Privacy mode yoqiq bo'lsa faqat o'ziga murojaatlar |
| `messages.getMessages` | ✅ ID bo'yicha istalgan xabar (HTTP Bot API'dan ustunligi) |
| Kontakt, story, last-seen | ❌ yo'q → Ghost Mode, Faollik tarixi, Mutual-contact **ma'nosiz** |
| AntiDelete / MediaBackup | Nazariy jihatdan bot chatlarida ishlashi mumkin |

### Nima uchun v10 dan KEYIN

Bot login **yana bitta akkaunt** qo'shadi. Baza hozir `account_id`
siz va 12 akkaunt allaqachon aralashib ketgan
(`specs/2026-08-26-multi-account-db-isolation-design.md`). Bot
rejimini v10 dan oldin qo'shsak, muammo kattalashadi.

### Nima uchun Unigram'da bor

Unigram TDLib ustiga qurilgan, TDLib esa
`checkAuthenticationBotToken` ni tayyor beradi. tdesktop TDLib emas —
o'zining MTProto qatlamini ishlatadi, shuning uchun bizda qo'lda
yozish kerak. Plus/Telegraph'da borligi **tasdiqlanmadi** (Android
forklari, kodi ko'rilmagan).

### Qadamlar

1. v10 tugagach.
2. **Arzon tekshiruv (~30 daqiqa).** BotFather'dan bir martalik token
   olib, bot nomidan `messages.getDialogs` va `messages.getHistory`
   ni chaqirib ko'rish. Bu spekulyatsiyani tugatadi.
3. Natijaga qarab qaror:
   - Ishlasa → to'liq "bot akkaunt" rejimi asosli
   - Ishlamasa → cheklangan rejim (faqat bot chatlari +
     `getMessages` orqali arxivlash), UI'da ochiq aytilgan holda

### Track C bilan aloqasi

Bot akkaunt ham sync'da **alohida akkaunt** sifatida ko'rinadi
(`account_id` bo'yicha), lekin `activity_history` unga tegishli emas
— bot last-seen kuzata olmaydi. Plan 05 (TDLib capture service) bilan
chalkashtirmaslik kerak: u **foydalanuvchi akkaunti** bilan kiradi,
bot bilan emas — bot boshqa odamlarning chatlarini ko'ra olmaydi.

---

## 💡 YANGI G'OYA — o'qilgan vaqt orqali faollikni aniqlash (A14)

**Foydalanuvchi topdi (2026-08-25).** Track C davomida tdesktop
tomonida qilinadigan ishlar ro'yxatiga **5-band** sifatida qo'shildi.

### Muammo

Kontakt last-seen'ni yashirsa, "Faollik tarixi" faqat bilvosita
signallarga tayanadi: online/offline o'tishlari va story qo'yilgan
vaqt. Bu yetarli emas — yashirgan odam ko'pincha umuman ko'rinmaydi.

### Kuzatuv

Bitta kontaktda ikkita MUSTAQIL manba bir-biriga mos kelmadi:

| Manba | Vaqt |
|---|---|
| Faollik tarixi (last-seen) | 24.08 **18:31** |
| Xabar o'qilgan vaqti (kontekst menyu) | 24.08 **19:35** |

Ya'ni odam 19:35 da FAOL bo'lgan, lekin last-seen buni
ko'rsatmagan. **O'qilgan vaqt — mustaqil va aniqroq signal.**

### Nega bu ishlaydi

O'qilgan vaqt boshqa maxfiylik sozlamasi ostida: foydalanuvchi
last-seen'ni yashirishi mumkin, lekin o'qilgan vaqt (agar u
alohida yashirilmagan bo'lsa) ochiq qoladi. Ikkalasi bir xil
tugma emas.

### Texnik boshlanish nuqtasi

`MTPmessages_GetOutboxReadDate` — `api/api_who_reacted.cpp:290`
da allaqachon ishlatiladi (kontekst menyudagi "yesterday at 7:35 PM"
aynan shundan). Ya'ni MTProto so'rovi tayyor, uni faollik tarixiga
ulash kerak.

### Reja (taxminiy)

1. Kuzatilayotgan kontaktga yuborgan CHIQUVCHI xabarlarimiz uchun
   `GetOutboxReadDate` so'raladi (kontakt ro'yxatda bo'lsa).
2. Natija `activity_history` ga yangi `field='read'` sifatida
   yoziladi — mavjud tuzilma o'zgarmaydi.
3. Faollik tarixi oynasida alohida qator: "Xabar o'qildi: HH:MM".
4. Online davrlarni qayta qurishda ham hisobga olinadi — o'qilgan
   vaqt = o'sha lahzada FAOL bo'lgan.

### Ehtiyot bo'lish kerak bo'lgan joylar

- **So'rov chastotasi.** Har xabar uchun alohida so'rov yubormaslik
  kerak — flood-limit xavfi. Faqat kuzatilayotgan 8 kishi va faqat
  yangi o'qilgan xabarlar uchun.
- **Ghost Mode bilan ziddiyat yo'q** — bu BIZNING xabarimiz qachon
  o'qilganini so'raydi, biz hech narsani o'qilgan deb belgilamaymiz.
- **Premium/maxfiylik cheklovi.** Kontakt o'qilgan vaqtni yashirgan
  bo'lsa so'rov bo'sh qaytadi — bu normal, xato emas.

---

## 🔍 2026-08-25 — bypass-forward media zanjiri: 4 ta bo'shliq

Sinov ketma-ketligi (foydalanuvchi topdi, har biri oldingisini
inkor qildi — shuning uchun gipoteza emas, DALIL bilan borildi):

1. "Yangi klientda media ketmadi" → men "Telegram keshida yo'q"
   dedim. **Noto'g'ri.**
2. Foydalanuvchi media'ni to'liq ko'rib forward qildi — baribir
   ketmadi. Demak ko'rish yetarli emas.
3. DB dalili: forward qilingan xabar (`t.me/mubinam_1/597`)
   `media_index` da `present`, diskda fayl BOR. Ammo
   `actioned_messages` da 0 qator.

**1-bo'shliq:** bypass-forward zanjiri arxivni umuman ko'rmasdi.
Manba faqat (a) Telegram'ning o'z fayli, (b) O'CHIRILGAN xabar
media'si edi. `GetArchivedMediaPath()` qo'shildi.

Keyingi sinov: avtomatik yuklangan media ishladi, QO'LDA
yuklangani ishlamadi. `data_document.cpp` `finishLoad()` da yana
uchta xato:

**2-bo'shliq:** darvoza faqat `ShouldAntiDelete()` ni tekshirardi.
`ShouldMediaBackup()` ataylab boshqa zanjirdan boradi, ya'ni
"Media Backup" yoqilgan chat AntiDelete qamrovida bo'lmasa —
qo'lda yuklangan media arxivlanmasdi.

**3-bo'shliq:** tur `"file"` deb qotirilgan — video
`medias/files/` ga tushardi. 2026-08-15 tasnif tuzatishi bu
joyni qamramagan.

**4-bo'shliq:** `SaveMediaFile()` `media_index` ga yozmaydi va
fayl nomida peer/msg yo'q → `(peerId, msgId)` bo'yicha topib
bo'lmasdi.

**Dars:** bir xil mantiq (media arxivi) uchta yo'lda — L1
(`finishLoad`), L2 (`MaybeDownloadMedia`), L3 (`TryRescueMedia`) —
alohida-alohida yozilgan. Tuzatish bittasiga qo'llanib,
qolganlari unutilgan. Yangi tuzatish kiritganda **uchalasini ham**
tekshiring.

Ikkala yo'l ham sinovdan o'tdi. Eski fayllar uchun "Eski media
fayllarni indekslash" tugmasi bosildi — 60 ta fayl indekslandi.

---

## 🔴 2026-08-24 — DARS: Qt6 configure self-update'ni O'CHIRIB QO'YGAN

Birinchi 7.1.1 relizi chiqarilgach ma'lum bo'ldi: build'da
**"Check for Updates" bo'limi umuman yo'q** edi. Ya'ni o'sha paketni
o'rnatgan qurilma boshqa hech qachon o'zi yangilana olmasdi —
bir tomonlama tuzoq.

Sabab:

```
DESKTOP_APP_DISABLE_AUTOUPDATE = ON     ← self-update UI o'chiq
DESKTOP_APP_SPECIAL_TARGET     = ""     ← Packer/Updater qurilmaydi
```

`configure.bat qt6 ...` bu bayroqlarni standart holatiga qaytargan.

⚠️ **BU XATO IKKINCHI MARTA TAKRORLANDI.**
`docs/self-update/HANDOFF.md` §2.6 da u allaqachon yozilgan va
"har safar CMake keshi tozalanganda eslab qolish kerak" deb
ogohlantirilgan edi. Qt6 migratsiyasida hujjat o'qilmadi.

**Qoida — CMake qayta konfiguratsiya qilinganda HAR SAFAR tekshiring:**

```bash
grep -E "DISABLE_AUTOUPDATE|API_ID" out/CMakeCache.txt
# DESKTOP_APP_DISABLE_AUTOUPDATE:BOOL=OFF  <- OFF bo'lishi SHART
```

Tuzatish (mavjud sozlamalarni saqlab):
`cmake out -D DESKTOP_APP_DISABLE_AUTOUPDATE=OFF`

Natija: `Updater.vcxproj` va `Packer.vcxproj` qayta yaratildi,
40 daqiqalik build, "Version and updates" bo'limi qaytdi.
Yangi `Packer.exe` 7 MB -> 21.9 MB (endi Qt6 ga qarshi qurilgan),
imzo tekshiruvi o'tdi.

**Dars ikkinchisi:** relizni chiqarishdan OLDIN build'da self-update
UI borligini ko'z bilan tekshirish kerak. Birinchi safar bu qadam
o'tkazib yuborildi va ishlamaydigan paket tarqatildi.

---

## ✅ 2026-08-24 — kod bosqichi TUGADI, reliz kutilmoqda

Build 19:01 (`build-2026-08-24-photos`). Barcha sinovlar o'tdi.

### Sinov natijalari

| Sinov | Natija |
|---|---|
| Fon tozalashi | ✅ 82 043 qator o'chdi, schema v8, 2 ta indeks yaratildi |
| Faollik tarixi | ✅ tez ochiladi, qisqa ulanishlar guruhlangan |
| Bo'sh AntiDelete yozuvi | ✅ yo'qoldi |
| Bypass forward emoji | ✅ `ð` o'rniga 📌🕐🔑🔗 |
| Rasm arxivi | ✅ `medias/images/` 31 → 50 fayl, indeksda 49 `present` |
| Rasmiy versiya tekshiruvi | ✅ |
| Eksport ro'yxatida nomlar | ✅ (yangi indekslanganlar uchun) |

### 🔴 YANGI TASK — startup 14.55 soniya

Foydalanuvchi sekundomer bilan o'lchadi: **lokal parol O'CHIRILGAN**
holatda, Enter bosilgandan chatga kira olishgacha **14.55 soniya**.

⚠️ **Claude'ning dastlabki xulosasi NOTO'G'RI edi.** Log'da 2s va 14s
oralig'ida hech narsa yo'q edi va men buni "parol kutilmoqda" deb
izohladim. Parol olib tashlangach ham vaqt o'zgarmadi — demak sabab
boshqa. Log 1 soniyalik aniqlikda yozgani chalg'itdi.

**Ma'lum:** ilovaning o'z init'i ~2s (font, RHI, sozlamalar), keyin
12 soniyalik JIM bo'shliq, undan keyin hamma narsa bir soniyada.
Bo'shliq ichida bironta log yozuvi yo'q.

**Keyingi qadam:** bu bo'shliqda nima ishlayotganini aniqlash kerak.
Log yordam bermaydi — profiler yoki bosqichma-bosqich vaqt o'lchash
kodi kerak. Ko'r-ko'rona gipoteza tuzmang.

### Reliz uchun tayyorgarlik (tekshirilgan)

Kalitlar, `Packer.exe`, `Updater.exe`, SSH host, `gh` auth — hammasi
joyida. Packer 08-08 dan bo'lsa ham **muammo emas**: `publish.ps1`
versiyani `version.h` dan o'zi o'qib `-version` argumenti sifatida
beradi (compile-time emas).

Tartib: `.\tools\publish\release.ps1 -DryRun` → so'ng argumentsiz.

---

## 🚀 2026-08-23 — upstream v7.1.1 ga o'tildi, build va 1-guruh sinovi o'tdi

**670 commit** merge qilindi (`v7.0.9` → **`v7.1.1`**), 10 ta konflikt hal
qilindi. Xavfsiz qaytish nuqtasi: `pre-merge-7.1.1` tegi.
Build: 16:53, `Telegram.exe` 234.8 MB, **57 succeeded / 0 failed**.

### 🔴 Yo'lda qilingan JIDDIY xato — dars

Merge'dan keyin `git add -A` ishlatildi. Git submodule ko'rsatkichlarini
hal qilinmagan qoldirgan edi, `add -A` esa **12 tasining hammasini**
jimgina merge-oldi holatda muzlatdi. Natijada manba kod v7.1.1 niki,
kutubxonalar esa v7.0.9 davridan bo'lib qoldi.

Build 1000+ xato bilan yiqildi. Chalg'ituvchi tomoni: `git submodule
update` "sinxron" deb ko'rsatardi — indeks va ish daraxti mos edi,
lekin **ikkalasi ham noto'g'ri** edi.

**Dars: merge'dan keyin `git add -A` ISHLATMANG.** Submodule holatini
`git ls-tree -r HEAD | grep ^160000` bilan tegga solishtirib tekshiring.

Ikkinchi dars: 1083 qatorlik xato ro'yxatida haqiqiy sabab **2-3
qatorlarda** edi (`Custom build ... exited with code 1` +
`struct 'CallButton' already defined`). Qolgani kaskad. Error List esa
faqat bitta IntelliSense (`E0070`) xatosini ko'rsatardi — **doim
Output'ga qarang, Error List'ga emas.**

### ✅ 1-guruh sinovi (merge konfliktlari) — 6/6

| Sinov | Natija |
|---|---|
| T-M1 bypass forward | ✅ **avvalgidan yaxshi** — ilgari ko'p holatda `pending` da qotardi |
| T-M2 aralash forward | ✅ |
| T-M3 oddiy forward (bypass o'chiq) | ✅ upstream'ning sof yo'li buzilmagan |
| T-M4 AntiEdit | ✅ |
| T-M5 AntiDelete ko'rsatish | ✅ |
| T-M6 Ghost Mode | ✅ |

### Topilgan va tuzatilgan 3 ta muammo (`679ecb7ba8`, BUILD KUTILMOQDA)

1. **Bypass izohida `ð`** — emoji UTF-8 baytlari sifatida `u"..."`
   (UTF-16) literaliga yozilgan edi. Tuzatildi; butun daraxt
   skanerlandi, bizning kodda boshqa shunday joy yo'q.
2. **GitHub rate limit (HTTP 403)** — ETag + `If-None-Match` qo'shildi
   (304 javob GitHub limitidan hisoblanmaydi), 403/429 uchun tushunarli
   matn. Tuzoq: `upstreamLastKnownVersion` endi har javobda yoziladi,
   shuning uchun bildirishnoma sharti uchun eski qiymat **yozishdan
   oldin** o'qiladi.
3. **Bo'sh AntiDelete xabari** — ildiz sabab DB dalili bilan aniqlandi
   va u **allaqachon yopilgan** (A13, 2026-08-13). Oyma-oy bo'sh yozuv
   ulushi: 05 → 11.5%, 06 → 5.3%, 07 → 0.13%, **08 → 0** (1388 tadan).
   Ko'rinayotgani — 11 ta eski qoldiq. Tuzatish ko'rsatish darajasida.

### Build muhiti

`MaxConcurrentBuilds = 8` (VS registry), `/MP6`
(`cmake/options_win.cmake` — **lokal o'zgarish**, submodule yangilansa
yo'qoladi). `/MP6 → /MP8` keyingi rejalashtirilgan to'liq build'da
qilinadi: `/MP` kompilyator bayrog'i, uni o'zgartirish barcha `.obj`
larni bekor qiladi.

---

## ✅ 2026-08-23 — 13:14 build'i bir haftalik real foydalanishda tasdiqlandi

Diskdagi dalil (`Pictures/customizationMainFolder/medias`, jami 1022 MB):

| Papka | Fayl | Xulosa |
|---|---|---|
| `avatars/` | **312** | ✅ Avatar tuzatishi ishlaydi (ilgari **0** edi) |
| `stories/` | **21** | ✅ A11 story backup ishlaydi (T16 amalda tasdiqlandi) |
| `videos/` | 245 | ✅ |
| `voices/` | 311 | ⚠️ ichida hamon **8 ta video** — o'sha eski 8 talik |

⚠️ **R1 tuzatishi build ichida, lekin tugma hech qachon bosilmagan.**
`voices/` dagi 8 ta video (752, 769, 774, 775, 776, 777, 782, 783)
2026-08-16 dagilarning aynan o'zi. About tab →
"🔍 Eski media fayllarni indekslash va tuzatish" bosilishi kerak.

**Sinov natijasi kelmagan bandlar:** T2, T6 (kvota alerti), T7,
T11–T14, kvota input'ining saqlanishi.

**Yangi kuzatuv (tekshirilmagan):** About tab 11:55 da "0.3 GB
ishlatilgan" ko'rsatgan edi, diskda esa o'sha payt 925 MB bor edi.
Agar hisoblagich hamon diskdagidan sezilarli kam ko'rsatsa — bu
alohida xato (indeksda yo'q fayllar sanalmayapti).

---

## 🧪 2026-08-16 BUILD SINOVI — natijalar va topilgan 3 ta xato

Build teg: `build-2026-08-16-media-backup`. 27 commit `origin/Oybek` ga
push qilindi.

| Sinov | Natija |
|---|---|
| T4, T5, T8, T9, T10, T15 | ✅ |
| T1 | ✅ (ehtimol, avvalgi sinovdan eslab qolingan) |
| R1, R2, R3 | ✅ (R1 90%, R2 95%) |
| T2, T6, T7 | ⏸️ T6 bloklagan (kvota xatosi, pastga qarang) |
| T11, T12, T13, T14, T16 | ❓ sinash usuli noaniq edi — quyida tushuntirildi |
| Avatar backup | ❌ umuman ishlamadi |

### Topilgan va TUZATILGAN 3 ta xato (`4038803f62`, build kutilmoqda)

**1. Kvota (T6 ni bloklagan).** `UpdateInt()` kvotani 1024 MB ga
qisardi. Foydalanuvchi 0.3 GB kiritsa registry'ga `307` yozilar, lekin
xotirada `1024` qolardi → UI qayta "1.0" ko'rsatardi va ogohlantirish
hech qachon chiqmasdi (0.3 GB ishlatilgan < 1 GB kvota). Dalil:
registry'da `mediaBackupQuotaMb = 307`, UI'da 1.0.

Pastki chegara **100 MB** ga tushirildi. Qo'shimcha: `SetInt()` endi
qisilgan qiymatni yozadi — registry bilan xotira ajralib ketmaydi.

**2. Avatar backup — noto'g'ri API.** `owner().photo(id)` noma'lum ID
uchun **bo'sh `PhotoData`** yaratadi (fayl joylashuvisiz), ya'ni
`load()` hech narsa yuklamaydi va `loaded()` hech qachon `true`
bo'lmaydi. To'liq `PhotoData` faqat profil rasmi ochilgan yoki
`UserFull` kelgan bo'lsa mavjud. **Diskdagi dalil:**
`medias/avatars/` yaratilgan, lekin **bo'sh** (0 fayl).

Endi `PeerData::userpicCloudImage(view)` ishlatiladi — ilova
ko'rsatayotgan rasmning aynan o'zi, yuklashni ham o'zi boshlaydi.
Rasm ko'pincha allaqachon keshda bo'ladi, shuning uchun darhol bir
marta tekshiriladi (aks holda `downloaderTaskFinished` otilmasdi).

**3. R1 to'qnashuvi — 8 ta yolg'on "xato".** `voices/` da 8 ta video
qolgan edi; diskni tekshirish ko'rsatdiki **hammasi `videos/` dagi
nusxaning ayni o'zi** (bayt-hajmi teng). Sabab: tasnif tuzatilgach
yangi kod faylni to'g'ri papkaga yozdi, eski nusxa esa joyida qoldi;
R1 esa nom to'qnashuvida faylga tegmasdi. Endi hajmlar teng bo'lsa
manba nusxasi o'chiriladi va indeksdan olib tashlanadi.

> "Turi aniqlanmadi: 5" — bu **xato emas**. `files/` dagi `.tgs`
> (gzip) va `.xlsx` (zip) kabi fayllar; R1 `files/` ga ataylab
> tegmaydi. Diskda tekshirilgan: `voices/`/`videos/`/`images/` da
> aniqlanmagan fayl **yo'q**.

### T3 — bu xato EMAS, ataylab qilingan

White List'da bo'lmagan chatda global AntiDelete yoqiq bo'lsa ham media
oldindan yuklanmaydi. `ShouldMediaBackup()` **ataylab**
`ShouldAntiDelete()` zanjiriga ergashmaydi: global bayroq yoqilsa
**981 ta peer** (asosan botlar va kanallar) media yuklay boshlardi va
kvota bir necha soatda to'lardi. Media backup — qimmat amal, AntiDelete
esa arzon (faqat matn), shuning uchun ular alohida boshqariladi.

O'zgartirish kerak bo'lsa: chatni White List'ga qo'shing yoki o'sha
chatning "Media Backup" toggle'ini yoqing.

### Sinash usuli noaniq bo'lganlar — qanday tekshiriladi

| № | Qanday sinaladi |
|---|---|
| T11/T12 | Eksport (Hammasi yoqiq) → 2 ta ZIP. Avval **faqat asosiy** ZIP'ni import → Archive tab'da yozuvlar bor, lekin fayllar `missing`. Keyin **media ZIP**'ni import → o'sha yozuvlar `present` ga qaytadi. |
| T13 | White List chatda **ochilmagan** video yonidan scroll (L2 yuklaydi), keyin o'sha videoni **ochish** (L1 hook). `medias/videos/` da fayl **bitta** bo'lishi kerak, ikkita emas. |
| T14 | Global AntiDelete'ni **o'chiring**, chat White List'da qolsin, o'sha chatda media **oching**. Arxivga tushishi kerak. |
| T16 | "Story media backup" toggle'ini yoqing, kuzatilayotgan kontakt story qo'yguncha kuting. `medias/stories/` da fayl paydo bo'ladi. Kutish talab qiladi — sinov navbatning oxirida turishi mantiqiy. |

---

### S1 — LOKALLASHTIRILDI (2026-08-15, foydalanuvchi sinovi)

**Natija: OpenGL o'chirilganda miltillash BUTUNLAY yo'qoladi.**

Bu bug'ni **GL/RHI render yo'liga** qamab qo'yadi. Umumiy chizish
mantig'i (`media/stories/`) sof — u har ikkala yo'lda ham bir xil
ishlaydi va rasterda muammo yo'q.

**Qidiruv maydoni:**
- `media/view/media_view_overlay_opengl.cpp`
- `media/view/media_view_overlay_rhi.cpp`
- (`media_view_overlay_raster.cpp` — SOG'LOM, solishtirish uchun etalon)

**Ish gipotezasi (hali tekshirilmagan):** "shaffof" holat GL kadri
fonni umuman chizmaganini bildiradi. Qt6'ga o'tishda
`QOpenGLWidget`/RHI ning kadrlararo bufer saqlash semantikasi
o'zgargan — tdesktop qisman qayta chizishga tayanayotgan bo'lsa,
ba'zi kadrlar tozalangan buferda qolib ketadi.

**Keyingi qadam:** raster va GL yo'llarida fon chizish ketma-ketligini
solishtirish — GL yo'lida fon har kadrda chiziladimi yoki faqat
"o'zgardi" bo'lganda.

**Vaqtinchalik yechim mavjud:** OpenGL ni o'chirish. Ya'ni S1 hech
narsani BLOKLAMAYDI — build va tarqatish undan mustaqil davom etadi.

---

### S1 — eski yozuv (diagnostika qadami, BAJARILDI)

Kodni qidirish to'xtatildi — bu **kuzatuv talab qiladigan** diagnostika,
ko'r-ko'rona gipoteza tuzish tokenni behuda sarflaydi.

**Birinchi va eng arzon tajriba:** tdesktop Settings → Advanced →
apparat tezlashtirish (OpenGL) toggle'ini **o'chirib**, story'ni qayta
ko'ring.

- Miltillash **to'xtasa** → muammo GL/RHI render yo'lida. Qidiruv
  maydoni `media_view_overlay_opengl.cpp` / `_rhi.cpp` gacha
  toraydi — bu juda katta yutuq.
- Miltillash **qolsa** → muammo umumiy chizish mantig'ida
  (`media/stories/`), GL bilan bog'liq emas.

Ikkinchi qadam: miltillash paytida `log.txt` da render ogohlantirishlari
chiqayotganini tekshirish.

Bu ikki dalilsiz kodga kirishmang.

---

## 🔵 Eski reja (R1/R2/R3 tavsifi)

Build 18:15 da 0 xato bilan o'tdi, arxiv ildizi konsolidatsiyasi
**ishladi** (baza/config/backups/bombmedia `customizationMainFolder` ga
ko'chdi, AppData bo'shadi). Quyidagilar QOLDI:

### R1 (KRITIK) — Mavjud fayllarning kengaytmasi va papkasi noto'g'ri

**Dalil:** `medias/voices/` da 30 tadan 19 tasi, `medias/videos/` da
68 tadan 43 tasi `<peerId>_<msgId>_file` ko'rinishida — kengaytmasiz.
`files/` toza (52/52).

**Sabab (aniqlangan, taxmin emas):** 2026-08-15 dagi tuzatish faqat
YANGI arxivlanadigan fayllarga ta'sir qiladi. Diskdagi eski fayllar
o'z nomi bilan qoladi va eksport ularni shundayligicha nusxalaydi.
Ya'ni xato "qaytmagan" — u hech qachon ketmagan.

Bundan tashqari `voices/` ichida VIDEO fayllar ham bor (eski
`isVideoMessage() -> "voice"` tasnifi qoldig'i).

**Yechim — TUZATISH (repair) bosqichi.** Eski fayllar uchun
`DocumentData` endi mavjud emas, shuning uchun tur fayl NOMIDAN emas,
**MAZMUNIDAN** aniqlanadi:

```cpp
QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent)
```

Bu magic-byte'lar bo'yicha sniffing qiladi — nom qanday bo'lishidan
qat'i nazar mp4/ogg/webm/jpeg ni to'g'ri aniqlaydi.

Har bir fayl uchun:
1. Mazmundan MIME aniqlanadi
2. Kengaytma yo'q yoki MIME ga zid bo'lsa → to'g'ri kengaytma bilan
   qayta nomlanadi (`MediaExtensionFor()` dagi jadval qayta ishlatiladi
   — uni MIME'dan kengaytmaga aylantiruvchi qismini alohida funksiyaga
   ajratish kerak)
3. MIME turi papkaga zid bo'lsa (video `voices/` da) → to'g'ri
   sub-papkaga KO'CHIRILADI
4. `media_index` dagi `rel_path` va `file_name` yangilanadi

Mavjud "🔍 Eski media fayllarni indekslash" tugmasiga qo'shiladi —
skaner + tuzatish bitta amal. Tugma matni ham yangilanadi.

⚠️ Fayl qayta nomlanganda indeks YANGILANISHI SHART, aks holda
`ReconcileMediaIndex()` ularni `missing` deb belgilab qo'yadi.

### R2 — Eksport ro'yxatida shaxsiy chatlar ID bilan chiqmoqda

`GetPeerDisplayName()` faqat White/Black List va per-peer nomlarga
qaraydi. Ro'yxatda bo'lmagan shaxsiy chatlar uchun nom yo'q →
"ID 620565940" ko'rinadi (3 ta shunday).

**Yechim:** nomni `Data::Session` dan olish (Custom Window'da sessiya
mavjud). Foydalanuvchi so'ragan ko'rinish: nom tepada, ostida kichik
shriftda ID. `Ui::SettingsButton` bir qatorli — ikki qatorli ko'rinish
uchun alohida widget kerak bo'ladi; agar qimmat bo'lsa, birinchi
bosqichda nomni to'g'ri chiqarish yetarli.

### R3 — Ildizni o'zgartirishni sinash (hali sinalmagan)

Kod yozilgan, lekin foydalanuvchi sinamagan. Sinashdan OLDIN to'liq
zaxira olish tavsiya etiladi. Ko'chirish mantig'i: faqat maqsadda
mavjud BO'LMAGAN fayllar ko'chiriladi, manba esa faqat muvaffaqiyatli
nusxadan keyin o'chiriladi.

---

⚠️ **2026-08-15 da aniqlandi: backfill skaneri build'ga TUSHMAGAN.**
`Telegram.exe` 00:15 da qurilgan, skaner commit'i esa 00:40 da yozilgan.
Ya'ni 00:15 dagi "0 xato" build undan OLDINGI holatniki.

1. **Avval BUILD** (Release/x64) — 2 ta fayl qayta kompilyatsiya
   bo'ladi: `custom_db.cpp`, `custom_mod_window.cpp`.
2. `out\Release\Telegram.exe` → `C:\Users\Oybek\Pictures\Release\` ga
   ko'chirish (avval ilovadan to'liq chiqish). Sinovdan oldin exe
   vaqti oxirgi commit'dan KEYIN ekanini tekshiring.
2. Sinov — birinchi navbatda shu uchtasi:
   - **T15:** 5 ta whitelist kanalda AntiDelete endi ishlaydimi
     (kategoriya xatosi tuzatilgan)
   - **T2:** White List'dagi chatda katta video yonidan scroll —
     **crash bo'lmasligi kerak**
   - **Archive tab:** "🔍 Eski media fayllarni indekslash" → keyin
     oynani yopib qayta ochib, eksport ro'yxati to'lganini ko'rish
3. Qolgan sinovlar: reja faylining Vazifa 15 jadvali (T1–T16).

**Holat:** 13 ta commit, hammasi LOKAL — push qilinmagan (user
xohishi bo'yicha keyin birga yuboriladi). Build ✅, sinov ⏳.

⚠️ Kod kompilyatsiya bo'lgani uning TO'G'RI ishlashini bildirmaydi.
Bu o'zgarishlar arxivlash, kvota, eksport va import yo'llariga tegdi —
ularning hech biri hali bir marta ham ishlatilmagan.

---

## ✅ Katta media backup — KOD YOZILDI (2026-08-14 kechqurun)

Spec: `docs/superpowers/specs/2026-08-14-media-backup-and-export-design.md`
Reja: `docs/superpowers/plans/2026-08-14-media-backup-and-export-plan.md`

**14 ta koddagi vazifa bajarildi, 9 ta commit. BUILD QILINMAGAN.**
Qolgan yagona ish — Vazifa 15: build + T1–T16 qo'lda sinov.

Muhim qarorlar va tuzoqlar uchun commit xabarlarini o'qing — har biri
nima uchun shunday qilinganini tushuntiradi.

Eng muhim uchtasi:
1. `ShouldMediaBackup()` ATAYLAB `ShouldAntiDelete()` zanjiriga
   ergashmaydi (global bayroq → 981 peer xavfi).
2. Indeks holati `finishLoad()` da tasdiqlanadi — `save()` yuklashni
   faqat boshlaydi, shuning uchun darhol `present` yozish indeksni
   yolg'onchi qilardi va bu Track C ga tarqalardi.
3. Eksport formati umumlashtirildi: `settings.json` + `index.json`
   (platformadan mustaqil), PowerShell bog'liqligi `ZipDirectory()` da
   yakkalandi.

### Yo'l-yo'lakay topilgan va tuzatilgan HAQIQIY xato

`IsInBlocklist()` kategoriya tekshiruvi aniq White List yozuvini bosib
ketardi. Foydalanuvchining 5 ta kanali White List'da, Black List'da esa
"Kanallar" kategoriyasi yoqilgan → **o'sha kanallarda AntiDelete amalda
o'chiq edi**. Endi aniq yozuv kategoriyadan ustun (T15 shuni sinaydi).

---

## 🔵 Eski reja matni (ma'lumot uchun)

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
