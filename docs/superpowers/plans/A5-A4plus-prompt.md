# A5 + A4+ — o'lchov kodini tozalash va startdagi 727 ms blok (Gemini uchun prompt)

## 0. 🖥️ AVVAL: qaysi kompyuterdasiz (MAJBURIY)

Loyiha laptop va PC'da olib boriladi. Bu promptdagi yo'llar (`C:\TBuild\...`)
**laptop'niki**.

1. `hostname` ni aniqlang.
2. `<tdesktop>\docs\MACHINES.md` jadvalidan shu kompyuter yo'llarini oling
   (`<tdesktop>` = `C:\TBuild\tdesktop` yoki `D:\Oybek\Telegram\tdesktop`,
   qaysi biri mavjud bo'lsa; `git -C <yo'l> remote -v` -> `Oybek-M/tdesktop`).
3. Kompyuter jadvalda yo'q bo'lsa — yo'llarni topib jadvalga yozing va
   hisobotda ayting. Taxmin qilmang.
4. Ishni boshlashdan oldin: `git fetch origin && git status -sb` —
   `Oybek` branch'da, `origin/Oybek` bilan bir xil bo'lishi shart (boshqa
   kompyuterda qilingan ish bo'lishi mumkin: avval `git pull --ff-only`).

Kontekst (o'qing): `<tdesktop>\docs\superpowers\plans\2026-09-13-peak-performance-and-stability-plan.md`
(A4, A5), `<tdesktop>\docs\superpowers\specs\2026-09-12-startup-freeze-diagnosis.md` §9–10.

## 1. Qat'iy qoidalar

- **Build QILMANG** va ilovani ishga tushirmang — build'ni foydalanuvchi
  qo'lda qiladi (~55 daqiqa). Shuning uchun kompilyatsiya xatosiz
  bo'lishini o'zingiz sinchiklab tekshiring: include'lar, ishlatilmay
  qolgan o'zgaruvchi/funksiya (`-Werror` bo'lmasa ham MSVC ogohlantiradi),
  `#ifdef` bloklari, imzolar `.h` va `.cpp` da bir xil.
- Ilova ishlab turgan bo'lishi mumkin — **CustomMod bazasiga yozmang**.
  O'lchash kerak bo'lsa bazaning NUSXASI ustida (`sqlite3` CLI).
- Izohlar o'zbekcha, "nega" ni tushuntiradi, apostrof oddiy ASCII `'`.
- Commit: `Co-Authored-By` YO'Q. Push faqat `origin Oybek` (upstream'ga
  hech qachon). Faqat o'zgartirgan fayllaringizni stage qiling —
  `cmake` submodule "modified" ko'rinadi, unga TEGMANG.
- Upstream fayllarini (bizning `custom_*` bo'lmagan kod) keraksiz
  o'zgartirmang — keyingi upstream merge'da konflikt bo'ladi.

## 2. Vazifa A4+ — startdagi 727 ms blok (avval shu)

### Dalil (log, 09-15 10:21, build 09-14 00:26)

```
10:21:01  main thread blocked 1671 ms          <- oyna yaratilishi, bizniki emas
10:21:03  RestoreDeletedChats took 608 ms      <- 1-akkaunt, ASOSIY OQIM
10:21:04  PruneStaleActivityHistory took 126 ms <- ASOSIY OQIM
10:21:04  main thread blocked 727 ms           <- 608 + 126
10:21:09  RestoreDeletedChats 202 peers, 17 restored, 50 ms   <- 2-akkaunt
```

Ikkala chaqiriq `main/main_session.cpp` da `chatsListLoadedEvents` obunasi
ichida (`timed(...)` bloklari, ~230 va ~274-qatorlar).

### Tahlil (tekshiring, ko'r-ko'rona qabul qilmang)

1. **`RestoreDeletedChats` (custom_archive.cpp ~613).** Uning ichki
   taymeri (`perfTimer`) sikl vaqtini o'lchaydi va 1-akkauntda 50 ms dan
   kam chiqqan (log qatori yo'q). Ya'ni 608 ms deyarli butunlay
   `CustomDB::GetPeersWithDeletedMessages()` ichidagi
   `EnsurePeersWithDeletedLoaded()` da (custom_db.cpp ~1001):
   ```sql
   SELECT DISTINCT account_id, peer_id FROM actioned_messages WHERE type = 'deleted'
   ```
   `actioned_messages` da `type` bilan boshlanadigan indeks YO'Q (mavjud:
   `(peer_id, type)`, `(peer_id, msg_id)`, `(timestamp DESC)`,
   `(account_id, peer_id, msg_id)`) — ~270 ming qatorli to'liq skan.
   09-12 profayleri shu so'rovni 401 ms deb ko'rsatgan.
   Ikkinchi akkauntda kesh allaqachon to'la — shuning uchun 50 ms.
2. **`PruneStaleActivityHistory`** (custom_db.cpp ~3924) — oddiy
   `DELETE ... WHERE observed_at < ?`. Izohda "~0.3 ms" deyilgan, lekin
   o'lchov 126 ms. UI uchun zarur emas.

### Talablar

1. **O'lchang (bazaning nusxasida).** Laptop'da baza yo'li:
   `CustomSettings::ArchiveRoot()` ostida; MACHINES.md yoki kodga qarab
   toping. `EXPLAIN QUERY PLAN` va `.timer on` bilan so'rovni indekssiz
   va `CREATE INDEX ... ON actioned_messages(type, account_id, peer_id)`
   bilan solishtiring. Natijani hisobotga yozing.
2. **Indeks.** Qamrovchi indeks `(type, account_id, peer_id)` qo'shing.
   Qaysi yo'l bilan — o'lchovga qarab tanlang va sababini yozing:
   - (a) sxema migratsiyasi `v17` (`kCurrentSchemaVersion`). DIQQAT:
     migratsiya `Init()` da, asosiy oqimda, va undan oldin ~62 MB
     pre-migration zaxira nusxasi olinadi — bu bir martalik start
     qotishi. Faqat indeks yaratish nusxa bilan birga < ~1 s bo'lsa.
   - (b) sxemani ko'tarmasdan `CREATE INDEX IF NOT EXISTS` ni
     `CustomDB::Maintenance::Enqueue` orqali fon oqimida, startda darhol
     (90 s kutmasdan). Idempotent.
   Qaysi biri bo'lmasin, `EnsurePeersWithDeletedLoaded` ning mantiqi
   (unite, MarkDeleted bilan poyga) o'zgarmasin.
3. **`RestoreDeletedChats` ni asosiy oqimdan chiqarish.** Peer ro'yxatini
   (DB so'rovi) fon oqimida oling, keyin natija bilan siklni asosiy oqimda
   bajaring (`History`, `owner.history()`, `refreshChatListEntry` —
   faqat asosiy oqim). Session yopilib qolishi mumkin: qaytishda
   `crl::on_main(session, ...)` yoki `base::make_weak` bilan himoya.
   Mavjud izohlarni saqlang (nega kechiktirilMAYDI — foydalanuvchiga
   ko'rinadigan ish; faqat DB qismi fonga o'tadi).
   DIQQAT: `GetDeletedMessages` (issiq yo'l) ham `EnsurePeersWithDeletedLoaded`
   ni chaqiradi. Kesh tayyor bo'lmaganda u "bo'sh" QAYTARMASLIGI shart —
   aks holda o'chirilgan xabarlar ko'rinmay qoladi. Indeks tufayli
   blokirovkali yuklash arzon bo'lishi kerak — o'lchov bilan tasdiqlang.
4. **`PruneStaleActivityHistory`** ni `Maintenance::Enqueue` orqali fonga
   o'tkazing (startda darhol, 90 s kutmasdan). Izohdagi "~0.3 ms" ni
   o'lchovga moslang.
5. `timed(...)` log qatorlari qolsin — build'dan keyin tasdiqlash shu
   qatorlar orqali.

## 3. Vazifa A5 — o'lchov kodini issiq yo'llardan olish

### Hozirgi holat

- `CustomDB::PerfScope` (RAII, `custom_db.h` ~53) har chaqiriqda
  `PerfNote` -> `QMutexLocker(&gProfMutex)` + `QHash` yangilash. 09-12
  o'lchovida bir blokda: `db:GetGhostRead` 4717, `settings:ShouldAntiDelete`
  2599, `history:loadDeletedMessages` 2341, `db:EnsurePeerCacheLoaded`
  1710 marta chaqirilgan.
- Foydalanish joylari:
  - `custom_settings.cpp` ~941, ~949, ~957 (`ShouldAntiDelete`,
    `ShouldAntiEdit`, `ShouldGhost`)
  - `custom_db.cpp` ~1036, ~1110, ~1118, ~1203, ~1580
  - `history/history.cpp` ~2221, ~2663, ~2679, ~2685 va ~2234 dagi
    LOKAL `struct PerfScope` (statik hisoblagich + har 200-chaqiriqda LOG)
  - `history/history_item.cpp` ~3359
- SQL profayler: `custom_db.cpp` ~319 `sqlite3_trace_v2(SQLITE_TRACE_PROFILE)`
  — HAR so'rovda mutex + `QString::fromUtf8(sql)` + hash.
- Stall watchdog: `core/application.cpp` `StartMainThreadStallWatch()` —
  blok tugaganda `CustomDB::DumpSqlProfile(...)`; >10 daqiqa kechikish
  uyqu deb hisoblanib `ResetSqlProfile()`.

### Talablar

1. **Profilingni bayroq ortiga oling.** Bitta joyda aniqlanadigan
   `CustomDB::ProfilingEnabled()` — jarayon boshida bir marta o'qiladi
   (masalan `CUSTOMMOD_PROFILE=1` muhit o'zgaruvchisi), keyin oddiy
   `static const bool`. Yoqilmagan bo'lsa:
   - `sqlite3_trace_v2` UMUMAN o'rnatilmaydi;
   - `PerfScope` konstruktor/destruktori taymer ham ishga tushirmaydi,
     mutex ham olmaydi (bitta bool tekshiruvi);
   - `DumpSqlProfile` hech narsa qilmaydi.
   Sozlama UI'si kerak emas. Qanday yoqilishini izohda va hisobotda yozing.
2. **Eng issiq yo'llardagi `PerfScope` larni butunlay olib tashlang:**
   `settings:ShouldAntiDelete/ShouldAntiEdit/ShouldGhost`,
   `db:GetGhostRead`, `db:IsDeletedLocally`, `db:EnsurePeerCacheLoaded`,
   `item:restoreFromCustomDB`, `history:*` va `history.cpp` dagi lokal
   `struct PerfScope` (+ uning `QElapsedTimer` include'i endi kerak
   bo'lmasa). Ular o'z vazifasini bajardi (T43). Qolganlarini (kam
   chaqiriladigan) bayroq ortida qoldirish mumkin — nimani qoldirganingizni
   ro'yxat qiling.
3. **Qoladigan arzon diagnostika:** stall watchdog'ning `main thread
   blocked` LOG qatori va uyqu qatori; `TimedStep`/`timed` (startda bir
   marta); `CustomMod Maintenance: X took N ms`. Bular doimiy qolsin —
   kelajakda qotish qaytsa darhol ko'rinadi.
4. Spec §9 va reja A5 bandini yangilang: nima olib tashlandi, nima qoldi,
   profiling qanday yoqiladi.

## 4. Tekshirish (build'siz)

- `git grep -n "PerfScope\|PerfNote"` natijasini hisobotga qo'ying va
  har qolgan joyni asoslang.
- `git diff --stat` va har fayl bo'yicha o'zingiz qayta o'qing:
  ishlatilmay qolgan `#include`, static funksiya, o'zgaruvchi yo'q.
- Build'dan keyin FOYDALANUVCHI tekshiradigan kutilgan log (hisobotda
  yozing):
  - startda `main thread blocked` faqat oyna yaratilishi (~1.5 s),
    `RestoreDeletedChats`/`PruneStaleActivityHistory` dan keyingi ~727 ms
    blok YO'Q;
  - `CustomMod Scope` / `CustomMod SQL` jadvallari log'da YO'Q
    (profiling o'chiq);
  - `CUSTOMMOD_PROFILE=1` bilan ishga tushirilsa jadvallar yana chiqadi.

## 5. Tugatish

- Mantiqan ajratilgan commit'lar (masalan: A4+ indeks, A4+ fonga
  o'tkazish, A5 profiling bayrog'i, docs). Har commit xabari "nega" ni
  aytadi.
- Reja faylida A4+/A5 bandlarini belgilang: "KOD TAYYOR (`<hash>`), build
  kutmoqda".
- `<tdesktop>\docs\superpowers\PROJECTS.md` dagi 2026-09-14 bo'limining
  "Keyingi qadam" qatorini yangilang.
- Push `origin Oybek`. Hisobotda: o'lchov natijalari, tanlangan indeks
  yo'li va sababi, commit'lar, qolgan `PerfScope` ro'yxati, kutilgan log.
