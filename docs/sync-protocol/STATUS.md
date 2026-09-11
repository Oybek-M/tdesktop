# Loyihalar holati — HAR SESSIYA SHU YERDAN BOSHLANADI

Oxirgi yangilanish: **2026-09-11**

> ✅ **2026-08-26 dagi ko'p akkauntli aralashuv xatosi HAL QILINDI.**
> Protokol tomoni: spec §0.12 (`record_id` ga `account_hash`),
> `test-vectors.json` qayta generatsiya qilindi, .NET tasdiqladi.
> tdesktop tomoni: sxema v10 (`account_id`) qurildi.
> Tashxis: [`../superpowers/specs/2026-08-26-multi-account-db-isolation-design.md`](../superpowers/specs/2026-08-26-multi-account-db-isolation-design.md)

> Bu fayl bir nechta loyiha o'rtasida "kim nimani bajardi" ni
> ko'rsatadi. Sessiya boshida o'qing, oxirida yangilang.

---

## Umumiy holat

| Loyiha | Holat | Keyingi qadam |
|---|---|---|
| **tdesktop** (CustomMod) | 🟡 Plan 02 dan 10/11 task kodda tayyor; **to'liq build KUTILMOQDA** | Build (uyda) → qo'lda regressiya → Task 9 |
| **customsync-server** | 🟡 01a, 01b ✅; **plan 04 jarayonda (3/6)** | 04 Task 6 (ikki fazali o'chirish) |
| **server-controller** | ⚪ boshlanmagan | 01a/01b tugagach |
| **tmobile-android** | ⚪ muhokama qilinmagan | — |
| **tmobile-ios** | ⚪ muhokama qilinmagan | — |

---

## Ikki mashina o'rtasida ishlash (office laptop ↔ uy PC)

2026-09-07 da kerak bo'ldi va yana kerak bo'ladi. Ikkala repo endi
GitHub'da, ya'ni odatiy yo'l — `push` / `pull`:

| Repo | Remote | Branch |
|---|---|---|
| tdesktop (CustomMod) | `Oybek-M/tdesktop` (`origin`) | `Oybek` |
| customsync-server | `Oybek-M/customsync-server` (`origin`) | `Oybek` |

🔴 **`upstream` ga (`telegramdesktop/tdesktop`) hech qachon push
qilinmaydi.** `saidjon` remote'iga ham.

### Push bilan KETMAYDIGAN narsalar

1. **`cmake` submodule'idagi `/MP6` o'zgarishi.** U commit qilinmagan
   va submodule detached HEAD'da turadi. Patch:
   `Projects programming/Telegram/cmake-MP6.patch`. Yangi mashinada
   build'dan OLDIN:

       cd C:/TBuild/tdesktop/cmake && git apply ../../cmake-MP6.patch

   Nima uchun kerak: raqamsiz `/MP` barcha yadroni oladi, har
   `cl.exe` `/O2` bilan 1-2 GB yeydi. 15 GB RAM to'lib OS muzlaydi.
   Yadro/RAM boshqacha bo'lsa raqamni moslang.

2. **`out/` build daraxti** — repo'da yo'q, yangi mashinada CMake
   qaytadan konfiguratsiya qilinadi va birinchi build to'liq bo'ladi
   (~34 daqiqa; inkremental build ~8 daqiqa).

3. **Foydalanuvchi bazasi** (`<ArchiveRoot>/db/actioned_messages.db`)
   — mashinaga bog'liq va ko'chirilmaydi. Ikki mashinada ikki alohida
   arxiv bo'ladi; ularni birlashtirish aynan shu loyihaning (sync)
   maqsadi.

4. **Master kalit** — DPAPI bilan himoyalangan, mashinadan chiqmaydi.
   Yangi mashina Task 12 oqimi orqali umumiy arxivga QO'SHILADI
   (parol so'raladi), yangi kalit YARATMAYDI.

### Remote'siz zaxira (agar internet bo'lmasa)

    git bundle create ../repo-YYYY-MM-DD.bundle --all
    # boshqa mashinada:
    git clone repo-YYYY-MM-DD.bundle repo

Bitta fayl, to'liq tarix bilan. `git bundle verify` tekshiradi.

---

## tdesktop — protokolga tegishli holat

| Nima | Holat |
|---|---|
| `qtwebsockets` moduli | ✅ qurildi va sinovdan o'tdi (2026-08-25) |
| Sxema versiyasi | **v15** kodda. v14 = `sync_outbox` + `sync_state`, v15 = `sync_record_map`. Migratsiya jonli bazaning NUSXASIDA tekshirildi (2026-09-07), `integrity_check` = ok. 🔴 Keyingi bo'sh versiya — **v16** |
| `account_id` (ko'p akkaunt) | ✅ **BOR** — 5 ta jadvalda. `activity_history` ataylab FILTRLANMAYDI (spec §0.13) |
| `media_index` jadvali | ✅ mavjud, **3 087** yozuv (2026-09-07 da o'lchandi) |
| `sha256` maydoni | ❌ **BO'SH** — hisoblash hali yozilmagan |
| `sync_outbox` / `sync_state` | ✅ v14 da qo'shildi (Task 3/4) |
| `CustomSync` moduli | ✅ mavjud — `custom_sync.{h,cpp}` orkestrator (Task 8 + 11) |
| `PeerNameCache` | ✅ mavjud (`peer_directory` ning lokal proyeksiyasi) |
| Retention (activity) | ✅ 30 kun, tozalash ishlaydi |

### tdesktop'da 2026-08-25 da tuzatilgan (Track C ga ta'sir qiladi)

| Xato | Nima uchun protokolga tegishli |
|---|---|
| Rasmlar arxivlanmasdi | `media_index` da `image` yozuvlari faqat 08-24 dan bor — undan oldingi rasmlar YO'Q. Sync ularni topa olmaydi |
| Qo'lda yuklangan media indekslanmasdi | L1 yo'li `media_index` ga yozmasdi — `GetArchivedMediaPath()` ularni ko'rmasdi |
| Placeholder qayta arxivlanardi | ASL MATN o'rniga marker saqlanardi. ✅ TASDIQLANDI: 12 -> 0, haqiqiy 7 ta xabar tegilmadi, chat ochilgach yangi buzilgan paydo bo'lmadi |

⚠️ **Sync uchun muhim:** eski (2026-08-24 dan oldingi) rasmlar
arxivda YO'Q. Ular `media_index` da ham yo'q, ya'ni sync ularni
"yo'qolgan" deb ham ko'rsata olmaydi. Bu qabul qilingan
yo'qotish — tiklab bo'lmaydi.

### 🔴 Plan 02 da ataylab qoldirilgan / ochiq bandlar

1. **`peer_directory` enqueue nuqtasi.** `CustomSettings::RememberPeerName(peerId, name)`
   da akkaunt konteksti yo'q, to'rtta chaqiruvchisi bor
   (`custom_archive.cpp` ×3, `custom_tab_storage.cpp`). Akkauntni
   taxmin qilish noto'g'ri `account_hash` beradi, ya'ni hech bir qurilma
   takrorlay olmaydigan `record_id`. Push yo'li paydo bo'lganda hal qilinadi.

2. ✅ **Payload QURILADI** (Task 7a, 2026-09-05). `custom_sync_payload.cpp`
   har bir `kind` uchun mazmunni lokal jadvallardan o'qiydi va
   `BuildStatus { Ok, SourceGone, Unsupported }` qaytaradi.
   `SourceGone` da yozuv `Outbox::Drop` qilinadi. **Seam saqlandi**:
   bo'sh payload hech qachon jo'natilmaydi — aks holda server
   `created` qaytarardi, `MarkSent` yozuvni o'chirardi va `record_id`
   deterministik bo'lgani uchun hodisani qayta yuborib bo'lmasdi.

3. ✅ **`custom_sync_client.cpp` KOMPILYATSIYA QILINDI** (2026-09-05).
   To'liq build o'tdi: beshala `custom_sync_*.obj` va `custom_db.obj`
   qurildi, `moc_custom_sync_client.cpp` generatsiya qilindi,
   `Telegram.exe` linklandi. Ungacha bitta kompilyatsiya xatosi topilib
   tuzatilgan edi (`Crypto::Seal` `QByteArray` qaytaradi, `optional`
   emas) -- u selftest'ga kirmagani uchun sezilmay qolgandi.

4. **`Outbox::Enqueue` hozircha inert.** `KeysAvailable()` doim `false`
   qaytaradi, chunki master kalit Task 6 (enrollment) da paydo bo'ladi. Task 6 da uni
   `true` ga o'tkazayotgan odam **`record_id` hisoblashini ham qo'shishi
   shart** — aks holda bo'sh `record_id` bilan `INSERT OR REPLACE`
   hamma yozuvni bitta `''` kalitiga uradi. Kodda ikkinchi qulf bor
   (bo'sh `recordId` da darhol qaytadi), lekin u xatoni yashiradi emas,
   to'xtatadi xolos.

5. 🔴 **`edited` uchun `occurred_at` ikki tahrirni ajratmaydi.**
   Enqueue nuqtasi `msg.msgDate` ni beradi -- xabarning ASL sanasi, u
   tahrirda o'zgarmaydi. Ya'ni bir xabar ikki marta tahrirlansa ikkala
   hodisa bir xil `record_id` oladi va ikkinchisi dedup'da yo'qoladi.
   Lokal tahrir vaqtiga o'tish YECHIM EMAS: `occurred_at` aynan shu
   sabab deterministik -- ikki qurilma bir hodisani ko'rganda bir xil
   `record_id` chiqarishi kerak, lokal soat esa har qurilmada boshqa.
   To'g'ri yechim Telegram'ning tahrir sanasini talab qiladi,
   `ActionedMessage` da esa bunday maydon yo'q. Alohida ish sifatida
   qoldirildi (2026-09-05 da Task 7a tayyorlanayotganda topildi).

6. 🔴 **Tombstone qabul qilish -- Task 7c.** 7b da kelgan tombstone
   ogohlantirish bilan o'tkazib yuboriladi va **cursor undan o'tib
   ketadi**, ya'ni u boshqa qayta o'qilmaydi. Hozir hech narsa
   tombstone enqueue qilmaydi, shuning uchun yo'qotish yo'q. 7c
   `record_id -> lokal qator` indeksini (sxema **v15**) qo'shadi;
   o'shanda `pull_cursor` ni bir marta 0 ga qaytarish kerak bo'lishi
   mumkin, aks holda 7b davrida kelgan tombstone'lar qo'llanmay
   qoladi.

7. 🔴 **Task 8 uchun: merge QAYSI OQIMDA ishlaydi?** Hozir
   `pullAndMerge` QNetworkAccessManager callback'ida, ya'ni GUI
   oqimida bajariladi -- capture bilan bir xil oqim, poyga yo'q.
   Ammo `MergeGuard` izohida "merge sync oqimida ishlaydi" deyilgan.
   Agar orkestrator merge'ni boshqa oqimga ko'chirsa,
   `custom_db.cpp` dagi `gPendingWrites` (87-qator) **hech qanday
   mutex bilan himoyalanmagan** -- `gCacheMutex` faqat keshlarni
   qoplaydi. `MergeGuard` ning o'zi thread_local, ya'ni to'g'ri;
   muammo faqat gPendingWrites da. (2026-09-05, Task 7c tekshiruvida
   topildi.)

8. **Task 8 dan qolgan ikkita quyruq (Task 11 uchun):**
   - `_inFlight` uchun **watchdog yo'q**. `pushPending` yoki
     `pullAndMerge` callback'i otilmasa bayroq abadiy qoladi va sync
     jimgina o'ladi. `QNetworkReply` odatda `finished` ni chiqaradi,
     ya'ni ehtimollik past -- lekin timeout arzon.
   - **Katta backlog sekin oqadi.** Cap'ga yetgach `_catchUpCycles`
     `hasMore=false` bo'lmaguncha tiklanmaydi, ya'ni har intervalda
     bitta paket.

## Plan 02 — task holati (2026-09-07)

| Task | Nima | Holat |
|---|---|---|
| 1–6 | selftest, kripto, sxema, outbox, keystore, HTTP klient | ✅ |
| 7a | payload qurish | ✅ tekshirildi |
| 7b | pull va lokal merge | ✅ tekshirildi |
| 7c | tombstone + `sync_record_map` (v15) | ✅ tekshirildi |
| 8 | orkestrator (`NextAction` + timer) | ✅ tekshirildi, 1 ta xato tuzatildi |
| 10 | Sinxronizatsiya tab (UI, indeks 7) | ✅ kod tayyor |
| 11 | regressiya audit + `_inFlight` watchdog | ✅ kod tayyor |
| 12a | kalit ulashish kriptosi va transporti | ✅ tekshirildi |
| 12b | kalit ulashish UI oqimi | ✅ kod tayyor |
| 9 | WebSocket bildirishnomalari | ✅ kod tayyor, build kutilmoqda |

✅ **Plan 02 ning 11 ta task'i ham kodda tugadi** (2026-09-08).
WebSocket kechikishni 30 soniyadan ~1 soniyagacha tushiradi, lekin
polling asosiy yo'l bo'lib qoladi: modul bo'lmagan build ham,
soket uzilgan holat ham to'g'ri ishlaydi.

🔴 **Task 9 dan keyin TO'LIQ QAYTA BUILD kerak (~34 daqiqa, ~8 emas).**
`Telegram/CMakeLists.txt` ga target darajasidagi
`CUSTOM_SYNC_HAS_WEBSOCKETS` qo'shildi — bu Telegram target'idagi
HAMMA faylni qayta kompilyatsiya qilishga majbur qiladi.

### Task 9 tekshiruvida topilgani (2026-09-08)

Kod to'g'ri, lekin delegate'ning **tekshiruvi yaroqsiz edi**: u
bitta faylni kompilyatsiya qildi va "muvaffaqiyat" oldi, ammo
`.vcxproj` 09-07 15:37 dan, uning `CMakeLists.txt` tahriri esa
09-08 12:13 dan edi. MSBuild CMake'ni qayta yurgizmaydi, ya'ni
makro aniqlanmagan holda butun WebSocket kodi preprotsessorda
tashlab yuborilgan — kompilyator uni umuman ko'rmagan.

Qayta generatsiyadan keyin tasdiqlandi: makro `.vcxproj` da (8 ta
joyda), `Qt6WebSockets.lib` linkda, ikkala fayl makro BILAN toza
kompilyatsiya bo'ladi, va AUTOMOC `changesAvailable` signalini
yaratadi (aks holda bu LINK bosqichida chiqadigan xato bo'lardi).
Tuzoq `../../tools/sync-selftest/README.md` ga yozildi.

Har bir delegate ishi hisobotiga ishonilmasdan qayta tekshirildi:
SQL manbadan dasturiy ravishda ajratib olinib Python `sqlite3` da
yurgizildi, selftest mustaqil qurildi, va har implementatsiya
**ataylab buzilib** testlar bo'sh emasligi isbotlandi (keyin
tiklandi).

### Task 8 da topilgan va tuzatilgan xato

`runCycle()` har siklda `new Client(this)` yaratardi. Access token
`Client` ichida yashaydi, ya'ni har sikl `/devices/refresh` ga
borardi; refresh token esa BIR MARTALIK — server eskisini o'ldiradi.
Natija: har 30 soniyada ortiqcha so'rov va token rotatsiyasi, TLS
ulanish qayta ishlatilmaydi. Yechim: `Orchestrator` ning uzoq
yashovchi `_client` a'zosi (`219e6cd52b`).

---

## ✅ TO'LIQ BUILD O'TDI — 2026-09-07, 18:54

    13 succeeded, 0 failed, 43 up-to-date, 1 skipped
    07:48 daqiqa

Bitta build to'rtta ishni qopladi: Task 11 watchdog, Task 12a, Task
12b va tdesktop-customization tomonidagi o'zgarishlar.

`Telegram.exe` **qayta linklandi**: 235 447 808 bayt, 18:54 —
build tugagan vaqt bilan mos. (Ungacha qo'lda to'xtatilgan build
2 097 152 baytlik kesilgan fayl qoldirgan edi.)

🔴 **Endi kod kompilyatsiya bo'ldi, lekin HALI ISHLATILMADI.**
Sinxronizatsiya tab'i, watchdog va kalit ulashish oqimi ekranda hech
kim tomonidan ko'rilmagan.

### Build'dan keyin, shu tartibda (BAJARILMAGAN)

1. `../superpowers/plans/02-task11-manual-checklist.md` — 4 bo'lim.
   4-bo'lim Sinxronizatsiya tab'ini **birinchi marta ko'z bilan
   ko'rish**: kod kompilyatsiya bo'ldi, lekin uni hech kim ekranda
   ochmagan.
2. `C4505 'CustomDB::bindKey': unreferenced function` — bir qatorli
   tozalash. Hozir zararsiz, lekin rasmiy build `/WX` bilan quriladi.
3. Task 9 (WebSocket).

### `custom_tab_sync.cpp` — uchta kompilyatsiya xatosi

Uchtasi ketma-ket **uchta to'liq build**ni yedi, chunki kompilyator
har safar faqat birinchisini ko'rsatadi:

| Xato | Sabab | Commit |
|---|---|---|
| C3536 `submitBtn` | lambda o'z initsializatoridagi o'zgaruvchini ushlagan (2 joyda) | `4cd89e94fe` |
| C2672 `VerticalLayout::add` | `Ui::PasswordInput` -> `MaskedInputField : RpWidgetBase<QLineEdit>`, `RpWidget` avlodi EMAS — `add()` ning SFINAE sharti rad etadi | `7a9f232500` |
| C2039 `AddPasswordField` | `Settings::` emas, `Settings::CloudPassword::` | `20a3e7aa08` |

🔴 **Sabab — jarayondagi bo'shliq.** Faylni yozgan delegate uni
hech qachon kompilyatsiya qila olmagan: `/Zs` bu faylda ishlamaydi
(butun header daraxtini tortadi), to'liq build esa taqiqlangan.
Ya'ni birinchi kompilyator tekshiruvi foydalanuvchining build'i
bo'lgan.

**Bo'shliq yopildi:** bitta fayl ~1 daqiqada kompilyatsiya qilinadi,
usul `../../tools/sync-selftest/README.md` da (`12576ab667`). Link
bosqichi bajarilmaydi, `Telegram.exe` ga tegilmaydi.
`custom_tab_sync.cpp` shu yo'l bilan toza (EXIT=0) deb tasdiqlandi.
Keyingi UI vazifalarida delegate ham, tekshiruvchi ham shuni
ishlatadi.

---

## Kalit ulashish (Task 12) — blokerdan chiqildi

Ilgari har qurilma TASODIFIY master kalit yaratardi: ikki qurilma
bir-birining yozuvini ocha olmasdi va dedup ishlamasdi. Loyihaning
asosiy maqsadi (bitta akkaunt, 4 sessiya — tdesktop + Android + iOS +
capture xizmati) usiz mumkin emas edi.

Server tomoni ilgaridan tayyor (`KeyEndpoints.cs`,
`/api/v1/keys/wraps` GET/POST/DELETE). Ish faqat klientda edi.

Yangi oqim, `enroll` dan keyin:

    listKeyWraps()
      |- ro'yxat BO'SH EMAS -> QO'SHILISH: parol so'raladi,
      |    getKeyWrap -> UnwrapMasterKey -> AdoptMasterKey
      |    (bu yo'lda kalit HECH QACHON generatsiya qilinmaydi)
      +- ro'yxat BO'SH      -> YARATISH: RandomBytes(32) ->
           WrapMasterKey -> POST -> faqat POST MUVAFFAQIYATIDAN
           KEYIN AdoptMasterKey

POST `admin` rolini talab qiladi: 1-qurilma `--admin` kodi bilan
enroll bo'lib o'ramni yaratadi, qolganlari oddiy kod bilan faqat
o'qiydi. 403 "admin kod kerak" deb tushuntiriladi, lokal kalitga
qaytish YO'Q.

🔴 **`Outbox::EnsureMasterKeyCreated()` endi CHAQIRILMAYDI, va bu
ataylab.** U kalitni darhol diskka yozadi; POST keyin muvaffaqiyatsiz
bo'lsa (admin kodisiz qurilmada 403 kafolatlangan), qurilma
yuklanmagan kalit bilan qolib ketadi — va `AdoptMasterKey` mavjud
kalit ustiga yozishdan bosh tortgani uchun u boshqa hech qachon
umumiy arxivga qo'shila olmaydi. Yagona chiqish yo'li
`master_key_protected` ni qo'lda o'chirish. Ogohlantirish
`custom_sync_outbox.h` da deklaratsiya ustiga yozilgan
(`752e3c0fd2`) — uni `enroll` ga qaytarmang.

PBKDF2 (600 000 iteratsiya, sekundlar) `crl::async` da ishlaydi,
natija `crl::on_main` orqali qaytadi — aks holda Windows ilova
ustiga "Not Responding" chizadi. Parol hech qayerda saqlanmaydi va
log'ga tushmaydi.

**Hali yo'q:** tiklash kodlari va email escrow (spec §4.4), o'ramni
o'chirish UI, parolni almashtirish. Bugun parol — kalitning yagona
nusxasi, va UI shuni ochiq aytadi.

---

## Ghost-read tuzatishi uchun aniq boshlang'ich nuqta

Task 11 migratsiya tekshiruvi jonli bazaning nusxasidan raqamlarni
berdi: `actioned_messages` 42 490, `activity_history` 160 611,
`media_index` 3 087 — va **`ghost_reads` = 0 ta qator**.

Bu tasodif emas, izlanishni juda toraytiradi. Yozish yo'li faqat
**bitta** shartga bog'liq:

    data_histories.cpp:776 va 786
    if (CustomSettings::ShouldGhost(QString::number(peer->id.value))) {
        CustomDB::SaveGhostRead(...);

O'qish tomoni esa keng ulangan (`history.cpp` da 4 ta joy,
`api_updates.cpp` da 1 ta). Ya'ni mexanizm bor, lekin **hech qachon
yozilmayapti**.

Ikki ehtimol — tuzatishdan OLDIN qaysi biri ekanini aniqlash kerak:

1. Ghost rejimi yoqilgan, lekin `ShouldGhost()` false qaytaryapti
   (peer id formati mos kelmayapti?) — unda tuzatish shu yerda.
2. Ghost rejimi o'chiq — unda o'qilgan holat serverga ketadi va
   sabab butunlay boshqa joyda; avval shuni tekshirish kerak, aks
   holda noto'g'ri joyni tuzatamiz.

🔴 **Ochiq savol (javob kutilmoqda):** Ghost Mode yoqilganmi, va u
global sozlamami yoki har chat uchun alohidami?

---

## tdesktop'da Track C uchun qolgan ishlar

1. ✅ Sxema v10 (`account_id`) — qurildi.
2. ✅ Sxema v14/v15 — `sync_outbox`, `sync_state`, `sync_record_map`.
3. ✅ `CustomSync` moduli — Task 9 dan boshqasi.
4. **`sha256` hisoblash** — `media_index` ga to'ldirish (yangi
   fayllar + mavjud 3 087 ta uchun backfill). Hali boshlanmagan.
5. **`peer_directory` enqueue nuqtasi** — quyidagi "ataylab
   qoldirilgan ishlar" 1-bandiga qarang.

---

## customsync-server — implement holati

**Repo:** https://github.com/Oybek-M/customsync-server (public, MIT)
**Papka:** `Projects programming\Telegram\customsync-server`
**Branch:** `Oybek` — `dotnet test`: **135 test, hammasi o'tadi**

🔴 **Aniq holat va keyingi qadam shu loyihaning `PROGRESS.md`
faylida.** Quyidagisi faqat qisqacha.

| Plan | Holat |
|---|---|
| **01a** — backend poydevori | ✅ 7 task + rejadan tashqari 6b (rol avtorizatsiyasi) |
| **01b** — sync yadrosi | ✅ 9 task'ning hammasi, deploy fayllari bilan |
| 02 — tdesktop agenti | ✅ 11 task'ning hammasi kodda; v7.2.6 merge'idan keyin build o'tdi |
| 03 — web controller | ⚪ boshlanmagan |
| **04** — storage lifecycle | 🟡 Task 1-3 ✅ (har birida tekshiruvda xato topilib tuzatildi). Keyingisi: 6 → 7. Task 4-5 keyinga, 8 → plan 03 |
| 05 — capture xizmati | ⚪ 04 dan keyin. `libtdjson` tahlili `PROGRESS.md` da |
| 06 — reliz boshqaruvi | 🟡 Task 1-4 bajarilgan (2026-09-03) |

**Kelishilgan tartib (2026-09-09):** `04 → 05 → 03 → read_at → TO'LIQ DEPLOY`.
Deploy oxirida — foydalanuvchining ongli qarori. Xavfi: klient va server
hali hech qachon gaplashmagan, deploy'gacha plan 02 ning to'rt qismi
(regressiya 2-3-bo'lim, watchdog, kalit ulashish, WebSocket) sinab bo'lmaydi.
Batafsil: customsync-server `PROGRESS.md` §2.

### 01b da nima ishlaydi

`seq` cursor (commit tartibi kafolati) · push/pull · tombstone (ikki
yo'nalishli) · kontent-adresli media + kvota · kalit o'ramlari + rate
limiting · keyset pagination + statistika · WebSocket bildirishnoma ·
`.cmx` import/eksport · platformalararo kripto vektorlari.

### 🔴 Plan 02 (tdesktop agenti) boshlashdan oldin bilish shart

- **`record_id` formulasi o'zgargan** — spec §0.12, `account_hash`
  qo'shildi. `activity` kind uchun u **bo'sh satr**.
- **`peer_hash` formulasi o'zgarmagan.**
- **`tombstone` push qilganda `target_record_id` OCHIQ maydonda ham
  yuborilishi shart** — spec §0.13. Server `payload` ni o'qiy olmaydi.
- **`sha256` ochiq matn ustidan**, shifrlashdan OLDIN (§0.5).
- ✅ **Sxema v14** (`sync_outbox` + `sync_state`) va **v15**
  (`sync_record_map`) qo'llangan. Keyingi bo'sh versiya — **v16**.
- Beshala vektor oilasi .NET da tasdiqlangan — C++ tomoni ham
  `test-vectors.json` ga qarshi tekshirilishi shart.

## Kelishilgan qarorlar (qisqacha)

To'liq matn: spec §0.

| № | Qaror |
|---|---|
| 0.1 | `qtwebsockets` alohida quriladi — Qt qayta qurilmaydi ✅ |
| 0.2 | tdesktop sxemasi — sync uchun v14/v15 qo'shildi ✅ |
| 0.3 | Retention: mijozda qabul filtri + serverda uzunroq + tombstone |
| 0.4 | `media_index` sync'ga to'liq kiritiladi |
| 0.5 | `sha256` majburiy, **ochiq matn** ustidan |
| 0.6 | Yangi kind'lar: `media_index`, `tombstone`; manfiy `msg_id` |
| 0.7 | Yagona custom eksport formati (`.cmx`), qo'lda ochish yo'li bilan |
| 0.8 | Arxiv ildizi sozlanadi — yo'llar kodda saqlanmaydi |
| 0.9 | Kvota: mijozda ham, serverda ham |
| 0.10 | `peer_directory` va `PeerNameCache` birlashtirildi |
| 0.11 | Plan 06 — reliz boshqaruvi API orqali |
| 0.13 | `tombstone` nishoni OCHIQ maydonda (server payload'ni ocholmaydi) |
| 0.14 | Payload ochiq `account_id` + `peer_id` olib yuradi — `peer_hash` teskarilanmaydi (2026-09-05) |

---

## 🔴 Buzilmaydigan qoidalar

1. **Local-first.** Server o'chsa mijozlar to'liq ishlashda davom etadi.
2. **Imzo lokalda.** Maxfiy kalitlar serverga hech qachon chiqmaydi.
3. **`test-vectors.json` — kontrakt.** Har platforma uni qayta hosil
   qila olishi shart.
4. **Retention tombstone yaratmaydi.** Lokal tozalash global
   o'chirish emas.
5. **`sha256` shifrlashdan OLDIN.** Aks holda dedup butunlay buziladi.
6. **Og'ir build'ni agent boshlamaydi.** `cmake --build` Telegram
   nishoni uchun ~34 daqiqa va ~15 GB RAM oladi; laptopda boshqa katta
   ilovalar ishlab turadi. Uni FAQAT foydalanuvchi yurgizadi
   (2026-09-07 da kelishildi). Agentga ruxsat berilgani: `sync_selftest`
   (soniyalar), `/Zs` sintaksis tekshiruvi, va bitta faylni
   kompilyatsiya qilish (`tools/sync-selftest/README.md`).

7. **`activity` kind akkauntlar bo'ylab BIRLASHGAN qoladi.** Boshqa
   hamma narsa akkaunt bo'yicha ajratiladi, lekin faollik tarixi
   kuzatilayotgan odam haqidagi obyektiv fakt — kim kuzatganiga
   bog'liq emas. Akkauntlarga bo'lib tashlash last-seen bypass
   qamrovini **buzadi**; aksincha, ko'p akkaunt uni **yaxshilaydi**.
