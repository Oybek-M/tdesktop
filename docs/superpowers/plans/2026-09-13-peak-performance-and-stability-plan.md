# Peak performance va barqarorlik — ish rejasi (2026-09-13)

**Manba:** [`specs/2026-09-12-startup-freeze-diagnosis.md`](../specs/2026-09-12-startup-freeze-diagnosis.md)
**Maqsad (foydalanuvchi ta'rifi):** loyihani yana peak performance'ga
chiqarish va stabil holatga o'tkazish; **hech bir jarayon boshqasiga
xalaqit qilmasin va bir-birini bloklab qo'ymasin.** Parallel yoki undan
keyin — `customsync-server` ishini davom ettirish.

**Stabil qaytish nuqtasi:** git tag `custommod-stable-20260913`
(`d054d03a7b`, build 09-12 23:40, sinov 09-13 00:17 — bloklar 1453 / 420 /
492 ms). Biror bosqich narsani buzsa — shu tegga qayting.

## Umumiy qoidalar (har vazifaga tegishli)

- Build'ni FAQAT foydalanuvchi ishga tushiradi. Commit/push faqat
  `origin/Oybek` ga, `Co-Authored-By` yo'q.
- Har o'zgarishdan keyin **o'lchov** bilan tasdiqlash: `log.txt` dagi
  `main thread blocked`, `CustomMod Scope/SQL`, `CustomMod Maintenance`
  qatorlari. Gipoteza qurishdan oldin jadvalga qarang (spec §10).
- Sinov: ilovani tray'dan **to'liq** yoping (Quit), keyin oching; eski
  jarayon tirik bo'lsa yangi binar yuklanmaydi.
- Izohlar o'zbekcha, "nega" ni tushuntiradi.

---

## A-faza — tdesktop (CustomMod)

### A1. Kichik ortiqchaliklar — ✅ TASDIQLANDI (build 09-14 00:26, sinov 00:47)

- [x] `ReconcileMediaIndex` -> `Maintenance` navbati (fon oqimi). Oqim
      xavfsizligi: import yo'llari uni allaqachon `crl::async` ichidan
      chaqiradi.
- [x] Global texnik xizmat (reconcile + compaction) jarayon davomida bir
      marta (ilgari har akkaunt uchun).
- [x] **Tasdiqlash:** 90-soniyadagi ~420 ms blok yo'qolishi;
      `CustomMod Maintenance: CompactActivityHistory` log'da **bir** marta.
      09-14 00:47 sinovi: 90-soniyada blok YO'Q; navbat ketma-ket —
      MediaQuotaScan 22 ms -> ReconcileMediaIndex 567 ms (endi fonda) ->
      CompactActivityHistory 481 ms (bir marta) -> ActivityCacheLoad 176 ms.
- [ ] **Kuzatish:** start bloki 3451 ms bo'ldi (09-13 da 1453 ms). Scope
      0 ms, SQL 30 ms — bizning kod emas; log'da til yuklangandan OpenAL'gacha
      2 s bo'shliq (Qt/audio init). Build'dan keyingi birinchi (sovuq)
      ishga tushirish bo'lishi mumkin — keyingi startda qayta o'lchansin.

### A2. 3-bosqich — bo'sh siklni SQL'siz o'tkazib yuborish (klient tomoni) — ✅ KOD build'da, tasdiq DEPLOY'GACHA KUTADI

Loyiha: spec §8 "3-bosqich". Server protokoliga TEGILMAYDI (`/sync/head`
bekor qilindi — bo'sh `pull` allaqachon arzon, narx klientda).

- [x] **A2.1 `sync_state` xotira keshi** (`custom_sync_outbox.cpp`):
      `GetState` xotiradan, `SetState` bir xil qiymatni yozmaydi, kesh
      faqat muvaffaqiyatli yozuvdan keyin yangilanadi.
- [x] **A2.2 Navbat hisoblagichi** (`Outbox::ProbablyEmpty`,
      `ResyncRowCount`): yuqori chegara invarianti — Enqueue oshiradi
      (mutex ostida), o'chirish kamaytirmaydi; `pushPending()` hisoblagich
      0 bo'lsa `Pending()` so'rovini o'tkazib yuboradi, `Pending()` bo'sh
      qaytsa aniq songa qaytaradi.
- [x] **A2.3 Bo'sh siklni o'tkazib yuborish** (`custom_sync.cpp/.h`,
      `custom_sync_client.cpp/.h`): taymer `onTimer()` orqali;
      `canSkipIdleCycle()` shartlari — WS ulangan va
      `webSocketConnectionId` oxirgi muvaffaqiyatli pull'dagi bilan bir
      xil, `_knownServerSeq <= pull_cursor`, navbat bo'sh,
      `_pendingNotify/_hasMore/_consecutiveFailures` yo'q, oxirgi haqiqiy
      sikl < 30 daqiqa. "Sync now" va WS xabarnomasi o'tkazib
      yuborilmaydi.
- [x] **A2.4 Kuzatuv logi:** o'tkazib yuborilgan sikllar uchun siyrak
      `LOG` (birinchisi va har 20-si) — tasdiqlash uchun.
- [ ] **Tasdiqlash (deploy'dan keyin):** bu kompyuterda sync yoqilmagan
      (`syncEnabled` va server manzili yo'q), server ham hali deploy
      qilinmagan — ya'ni sync kodi hozir umuman ishlamaydi va uni real
      sinab bo'lmaydi. Tasdiqlash server deploy'i va klient ulanganidan
      keyin. WS ulangan holatda bir necha interval davomida
      `CustomMod SQL` jadvallarida `sync_state`/`sync_outbox` so'rovlari
      deyarli yo'q; boshqa qurilmadan o'zgarish yuborilganda u baribir
      darhol tortiladi (xabarnoma yo'li). Log'da
      `CustomMod Sync: bo'sh sikl o'tkazib yuborildi (jami N, ...)` qatori
      ko'rinishi kerak (birinchisi va har 20-si).

### A3. 4-bosqich — sync uchun alohida SQLite ulanishi

Nima uchun: hozir barcha oqimlarning har bir SQL operatori bitta
`FULLMUTEX` ulanish mutex'ida navbatga turadi. Alohida ulanish sync
o'qishlarini UI o'qishlari bilan parallel qiladi (WAL), va xavfsiz
tranzaksiyalarga yo'l ochadi.

- [ ] **A3.1** `CustomDB::OpenAuxConnection()` — xuddi shu fayl,
      `PRAGMA busy_timeout` (masalan 2000 ms), `journal_mode=WAL` allaqachon
      faylda. Sxema migratsiyalari faqat asosiy `gDb` da, aux esa
      `Init()` DAN KEYIN ochiladi.
- [ ] **A3.2** Outbox qatlami (`sync_outbox`, `sync_state`,
      `sync_record_map`, `pending_tombstones`) aux ulanishga o'tadi.
      `CustomDB::RawHandle()` ni sync'dan olib tashlash.
- [ ] **A3.3** `SQLITE_BUSY` siyosati: busy_timeout + bitta qayta urinish;
      muvaffaqiyatsiz yozuv log'ga, jimgina yutilmaydi.
- [ ] **A3.4** Push natijalari (`MarkSent/MarkFailed/Drop`) bitta
      tranzaksiyada — faqat aux ulanishda xavfsiz (spec §8 2-bosqich:
      umumiy ulanishda boshqa oqim yozuvi tranzaksiyaga "tushib qolardi").
- [ ] **A3.5 Ochiq masala:** `MergeRecord` yozuvlari `CustomDB` API
      (`SaveActionedMessage` va h.k.) orqali `gDb` ga ketadi, ular keshlarni
      ham yangilaydi. Merge to'plamini tranzaksiyaga olish uchun yoki bu
      funksiyalar handle qabul qilishi, yoki merge tranzaksiyasiz qolishi
      kerak. Qaror A3.1–A3.4 o'lchovidan keyin.
- [ ] **Tasdiqlash:** sync sikli paytida UI o'qishlari (`db:*` Scope)
      kutish vaqti ko'rsatmasligi; `SQLITE_BUSY` log'da yo'q.

### A4. Vaqt byudjeti — uzun texnik xizmat ishlarini bo'laklash

- [x] `CompactActivityHistory` peer bo'yicha bo'laklandi (2026-09-16).
      Oyna `PARTITION BY peer_id, field` bo'lgani uchun peerlar mustaqil —
      natija bir xil qoladi, lekin bitta statement mutexni 5 ms dan ortiq
      ushlamaydi. Bo'laklar ~50 ms budjet bilan tranzaksiyaga guruhlanadi
      (har peer uchun alohida COMMIT WAL'ni ortiqcha yozardi).
      Bazaning nusxasida tekshirildi: 55 246 qator, 753 peer —
      bitta statement 133.8 ms -> eng uzuni 4.8 ms, o'chirilgan qatorlar
      to'plami AYNAN bir xil (ID'lar bo'yicha solishtirildi).
- [x] `ActivityCacheLoad` o'lchandi (2026-09-16, B3): bazaning nusxasida (55 249 qator)
      cold 157.3 ms, warm o'rtacha 132.3 ms (< 150 ms, min 116 ms, max 188 ms).
      Per-peer bo'laklash sinovi (753 ta alohida so'rov) 208 ms oldi, ya'ni
      oyna funksiyali yagona SELECT ancha tezroq va fonda `Maintenance::Enqueue`
      orqali asosiy oqimni bloklamasdan ishlaydi. Hozirgi kod saqlab qolindi.

### A4+. Startdagi 727 ms blokni yo'qotish — KOD TAYYOR (`4a477c8abd`, `c27bdccb60`), build kutmoqda

- [x] **A4+.1 Indeks:** `actioned_messages(type, account_id, peer_id)` qamrovchi indeksi (sxema v17). `EnsurePeersWithDeletedLoaded` so'rovi 110 ms dan 0.47 ms ga (234x) tushdi; SCAN o'rniga SEARCH COVERING INDEX (`4a477c8abd`).
- [x] **A4+.2 `RestoreDeletedChats` DB so'rovi fonga:** `GetPeersWithDeletedMessages` `crl::async` da olinadi, UI tiklash sikli `crl::on_main(session, ...)` da bajariladi. Asosiy oqim bloklanmaydi (`c27bdccb60`).
- [x] **A4+.3 `PruneStaleActivityHistory` fonga:** `CustomDB::Maintenance::Enqueue` orqali fon oqimida darhol bajariladi, asosiy oqimdan 126 ms blok olib tashlandi (`c27bdccb60`).

### A5. O'lchov kodini tozalash — KOD TAYYOR (`6eb4b30075`), build kutmoqda

- [x] SQL profayler (`sqlite3_trace_v2`) va `PerfScope` registri `CustomDB::ProfilingEnabled()` (`CUSTOMMOD_PROFILE=1`) bayrog'i ortiga olindi. Standart holatda trace ulanmaydi, mutex va taymerlar ishlamaydi.
- [x] Eng issiq yo'llardagi `PerfScope` lar butunlay olib tashlandi (`settings:ShouldAntiDelete/Edit/Ghost`, `db:EnsurePeerCacheLoaded`, `db:IsDeletedLocally`, `db:GetGhostRead`, `item:restoreFromCustomDB`, `history:loadDeletedMessages`, `history:inboxReadTillId`, `history:lastAvailableMessage`, `history:unreadCount`, lokal `struct PerfScope`).
- [x] Doimiy diagnostika qoldi: stall watchdog (`main thread blocked N ms`), `Maintenance took N ms`, `timed()` loglari. Fonga qarang: spec §9.

### A6. Eskidan qolgan ochiq ishlar

- [x] **B2 Legacy `account_id=0`:** DB nusxasida o'lchandi (2026-09-16). `actioned_messages` da
      14 944 qator (1072 peer, 833 deleted, 14 110 backup). O'lchov ko'rsatdiki,
      qatorlarning faqat 4.08 % i (88 peer, 610 qator) da egasi aniq; qolgan 95.92 % i
      (14 334 qator) ko'p akkauntli (60 peer / 11 173 qator) yoki dalilsiz (1581 peer /
      3161 qator) bo'lgani uchun ularni biror akkauntga yozish ma'lumot yo'qotadi.
      Xavfsiz mustaqil tozalash skripti tayyorlandi: `tools/maintenance/cleanup_legacy_account_zero.py`
      (dry-run, auto-backup, Telegram yopiqligini tekshirish, JSON undo log).
      Jonli bazaga yozilmadi. `kAccountFilterSql` filtri legacy qatorlar
      butunlay 0 bo'lmaguncha olib tashlanmaydi (`ee4b9ec9dc`).
      **Tekshiruvda topilgan va tuzatilgan (`2af8bc7c1a`):** `text_cache` va
      `media_index` kaliti `(account_id, peer_id, msg_id)`. Legacy qator
      ko'chirilganda nishon qator allaqachon mavjud bo'lsa, `UPDATE OR REPLACE`
      MAVJUD (to'g'ri egali, yangiroq) qatorni jimgina o'chirardi va undo log
      uni bilmagani uchun yo'qotish qaytarilmas edi. Skriptning eski nusxasi
      jonli bazaning nusxasida ishga tushirilganda 1126 ta `text_cache` va
      138 ta `media_index` qatori yo'q qilindi. Endi to'qnashgan qatorlar
      TEGILMAYDI va alohida sanaladi; undo log COMMIT'dan oldin yoziladi;
      Telegram ishlab tursa yozish har qanday yo'l uchun rad etiladi
      (eski tekshiruv faqat laptop yo'lini bilardi, PC'da jonli bazaga
      ogohlantirish bilan yozib yuborardi) -- nusxada ataylab sinash uchun
      `--copy`. Tuzatilgan skript nusxada boshdan-oxir sinaldi: 18 325 qator
      yangilandi, 1264 tasi to'qnashuv sababli o'tkazib yuborildi, birorta
      qator yo'qolmadi, `--undo` bazani aynan tiklaydi.
- [ ] S1: media/stories foni miltillashi — `use-qt-rhi=false` sinovi
      (ilova yopiq holda, `experimental_options_rhi_off.json`).
- [x] **B1 `readInboxTill` va inject qilingan elementlar:** Muammo kod va jonli log
      orqali tasdiqlandi: inject qilingan elementlar `WithLocalFlag` tufayli `isRegular() == false`
      bo'ladi va `_clientSideMessages` ga qo'shilmaydi. Chatda undan oldingi server xabari
      bo'lmasa (tarixi o'chirilgan chat), `readInboxTill(item)` yuqoriga qarab server xabari
      topolmaydi va `App Error: Can't read history till unknown local message.` xatosi
      to'xtovsiz yoziladi (bugungi `log.txt` da 217 marta kuzatildi).
      Tuzatildi (`837ce8f234`): `data_histories.cpp` da `isDeletedLocally()` holati
      tekshirilib, soxta xato logi to'xtatildi, mahalliy unread holati xavfsiz tozalanadi,
      Ghost Mode va `Expects(IsServerMsgId)` buzilmasligi ta'minlandi.
      **Tekshiruvda topilgan va tuzatilgan (`3c1556847e`):** `setUnreadCount()` da
      `Expects(folderKnown())` bor, tiklangan chatda esa papka noma'lum bo'lishi
      mumkin (shuning uchun `RestoreDeletedChats` `requestDialogEntry()` ishlatadi) --
      ya'ni yangi tarmoq aynan xavfsiz bo'lishi kerak joyda ilovani yiqitishi mumkin edi.
      Endi `folderKnown()` sharti qo'yildi; `setUnreadMark()` va
      `updateChatListEntry()` da bunday shart yo'q, shuning uchun badge baribir tozalanadi.

### A7. Reliz

- [ ] A1–A2 (kamida) build + tasdiqlangach versiya reliz qilinadi.
- [ ] Avval **VPS mirrorlarini tekshirish**: 09-11 dagi 7.2.7 relizi
      yarim yo'lda to'xtatilgan (GitHub mirror 7.1.1 da qolgan, VPS
      holati tekshirilmagan) — yarim yuklangan paket qolmaganiga ishonch
      hosil qilish.
- [ ] `tools/publish/release.ps1` (avval `-DryRun`).

---

## B-faza — customsync-server (parallel)

Kanonik holat: `customsync-server/PROGRESS.md` (2026-09-11: 135 test,
0 warning) va `tdesktop/docs/sync-protocol/STATUS.md`.

- **Hozirgi nuqta:** plan 04 (storage lifecycle) 3/6 — **keyingisi plan 04
  Task 6 (ikki fazali o'chirish)**, tartib 3 -> 6 -> 7. Batafsil —
  PROGRESS.md §2.
- **Bu rejaning serverga ta'siri: YO'Q.** A2 va A3 faqat klient tomonida;
  protokol o'zgarmaydi, `CHANGELOG.md` ga yozuv kerak emas. `/sync/head`
  endpoint'i ataylab qilinmadi (spec §8, 3-bosqich).
- Server ishi odatda **alohida sessiyada** olib boriladi — bir vaqtda ikki
  sessiya bir repo'da ishlasa, har biri faqat o'z fayllarini commit qiladi.

---

## Tartib

~~`A1 tasdiq` -> `A2` -> build + tasdiq -> `A3` -> build + tasdiq -> `A4` ->
`A5` -> `A7 reliz`~~

**Qayta tartiblandi (09-14):** sync hozir hech kimda yoqilmagan va server
deploy qilinmagan, shuning uchun A2 tasdig'i va A3 (sync uchun alohida
ulanish) foydalanuvchiga hozir hech narsa bermaydi va sinab ham bo'lmaydi.
Ular server deploy'i bosqichiga ko'chadi. Hozirgi tartib:

`A5` (o'lchov kodini issiq yo'llardan olish — har bir foydalanuvchiga
foyda) -> build + start qayta o'lchovi -> `A7 reliz` ; `A4` faqat o'lchov
blok ko'rsatsa ; `A2 tasdiq` + `A3` — deploy'dan keyin ; `A6` va `B`
parallel.
