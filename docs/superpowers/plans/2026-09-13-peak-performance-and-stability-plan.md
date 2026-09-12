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

### A1. Kichik ortiqchaliklar — ✅ KOD TAYYOR (`06da91da35`), build kutmoqda

- [x] `ReconcileMediaIndex` -> `Maintenance` navbati (fon oqimi). Oqim
      xavfsizligi: import yo'llari uni allaqachon `crl::async` ichidan
      chaqiradi.
- [x] Global texnik xizmat (reconcile + compaction) jarayon davomida bir
      marta (ilgari har akkaunt uchun).
- [ ] **Tasdiqlash:** 90-soniyadagi ~420 ms blok yo'qolishi;
      `CustomMod Maintenance: CompactActivityHistory` log'da **bir** marta.

### A2. 3-bosqich — bo'sh siklni SQL'siz o'tkazib yuborish (klient tomoni)

Loyiha: spec §8 "3-bosqich". Server protokoliga TEGILMAYDI (`/sync/head`
bekor qilindi — bo'sh `pull` allaqachon arzon, narx klientda).

- [ ] **A2.1 `sync_state` xotira keshi** (`custom_sync_outbox.cpp`):
      `GetState` xotiradan, `SetState` bir xil qiymatni yozmaydi, kesh
      faqat muvaffaqiyatli yozuvdan keyin yangilanadi.
- [ ] **A2.2 Navbat hisoblagichi** (`Outbox::ProbablyEmpty`,
      `ResyncRowCount`): yuqori chegara invarianti — Enqueue oshiradi
      (mutex ostida), o'chirish kamaytirmaydi; `pushPending()` hisoblagich
      0 bo'lsa `Pending()` so'rovini o'tkazib yuboradi, `Pending()` bo'sh
      qaytsa aniq songa qaytaradi.
- [ ] **A2.3 Bo'sh siklni o'tkazib yuborish** (`custom_sync.cpp/.h`,
      `custom_sync_client.cpp/.h`): taymer `onTimer()` orqali;
      `canSkipIdleCycle()` shartlari — WS ulangan va
      `webSocketConnectionId` oxirgi muvaffaqiyatli pull'dagi bilan bir
      xil, `_knownServerSeq <= pull_cursor`, navbat bo'sh,
      `_pendingNotify/_hasMore/_consecutiveFailures` yo'q, oxirgi haqiqiy
      sikl < 30 daqiqa. "Sync now" va WS xabarnomasi o'tkazib
      yuborilmaydi.
- [ ] **A2.4 Kuzatuv logi:** o'tkazib yuborilgan sikllar uchun siyrak
      `LOG` (birinchisi va har 20-si) — tasdiqlash uchun.
- [ ] **Tasdiqlash:** WS ulangan holatda bir necha interval davomida
      `CustomMod SQL` jadvallarida `sync_state`/`sync_outbox` so'rovlari
      deyarli yo'q; boshqa qurilmadan o'zgarish yuborilganda u baribir
      darhol tortiladi (xabarnoma yo'li).

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

- [ ] `CompactActivityHistory` (bitta katta `DELETE ... WHERE id IN
      (SELECT ... LAG ...)`, 427–657 ms) — `LIMIT` li ichki `SELECT` bilan
      ~50 ms lik bo'laklarga, bo'laklar orasida navbatni bo'shatish.
      Eslatma: SQLite'da `DELETE ... LIMIT` odatda kompilyatsiya qilinmagan
      (`SQLITE_ENABLE_UPDATE_DELETE_LIMIT`) — ichki `SELECT id ... LIMIT`
      ishlatilsin.
- [ ] `ActivityCacheLoad` (162–175 ms) — kerak bo'lsa bo'laklash;
      hozirgi qiymat qabul qilinadigan.

### A5. O'lchov kodini tozalash

- [ ] SQL profayler (`sqlite3_trace_v2`) va `PerfScope` registri har
      chaqiriqda mutex oladi. A1–A3 tasdiqlangach: issiq yo'llardagi
      `PerfScope` larni olib tashlash (`history.cpp`, `history_item.cpp`,
      `custom_settings.cpp`, `custom_db.cpp`), profaylerni sozlama/debug
      bayrog'i ortiga yashirish. Stall watchdog'ning bitta `LOG` qatori
      qolishi mumkin (arzon, kelajakda qotish qaytsa darhol ko'rinadi).
      Fayllar ro'yxati: spec §9.

### A6. Eskidan qolgan ochiq ishlar

- [ ] 824 ta legacy `account_id=0` o'chirilgan xabar (154 peer) — har
      akkauntda "eski yozuv, akkaunt noma'lum" bo'lib ko'rinadi
      (`specs/2026-09-11-account-misattribution-incident.md`).
- [ ] S1: media/stories foni miltillashi — `use-qt-rhi=false` sinovi
      (ilova yopiq holda, `experimental_options_rhi_off.json`).
- [ ] `readInboxTill` injected elementlarda o'qish belgisini qo'ymaydi.

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

`A1 tasdiq` -> `A2` -> build + tasdiq -> `A3` -> build + tasdiq -> `A4` ->
`A5` -> `A7 reliz` ; `A6` va `B` parallel.
