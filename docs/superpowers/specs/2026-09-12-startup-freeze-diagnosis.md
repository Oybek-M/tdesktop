# 2026-09-12 — Startdagi qotish: tashxis, tuzatish va sync izolyatsiyasi rejasi

**Holat:** asosiy muammo HAL QILINDI va o'lchov bilan tasdiqlandi
(ikki marta: 18:35 va 21:58 startlarida). 2-bosqich bajarildi
(`7ee739b265`), 3–4-bosqichlar rejalashtirilgan.

---

## 1. Muammo

Har ishga tushirishda ilova Windows'ning "not responding" darajasiga
borardi. Foydalanuvchi kuzatuvi: ilgari 9–11 soniya edi (oyna javob
berardi), upstream **v7.2.6 / v7.2.7** merge'idan keyin 30+ soniyaga
chiqdi va oyna butunlay qotadigan bo'ldi.

O'lchangan boshlang'ich holat (18:00 startida, `log.txt`):

```
main thread blocked  1823 ms
main thread blocked   964 ms
main thread blocked 33282 ms
main thread blocked  4598 ms
main thread blocked   434 ms
main thread blocked 21808 ms
main thread blocked  6766 ms
                    -------
                    ~69 soniya
```

---

## 2. Nima ISHLAMADI — va nega buni yozib qo'yish muhim

Uch marta gipoteza qurilib, kod yozildi, build qilindi — va qotish
qolaverdi. Bu bosqichlar behuda emas edi (ikkalasi ham haqiqiy
yaxshilanish), lekin **asosiy sabab ular emasdi**:

| Gipoteza | Commit | Natija |
|---|---|---|
| `GetEditHistory()` har xabar uchun SQL qiladi, reja `idx_am_peer_type` ni tanlab peer'ning barcha `backup` qatorlarini skanerlaydi → O(n²) | `7c8479b441` | To'g'ri yaxshilanish (708 chaqiriq 15 ms), lekin qotish qolди |
| `GetGhostRead()` bo'sh jadvalda ham har chaqiriqda SQL qiladi | `7c8479b441` | To'g'ri yaxshilanish, qotish qoldi |
| Faollik keshi so'rovi sekin | `f8e26268f5` | 374 → 263 ms, qotishga ta'siri kichik |

**Saboq:** startdagi qotishda "qaysi so'rov sekin?" degan savol noto'g'ri
savol edi. To'g'ri savol — **"kim kimni kutyapti?"**

---

## 3. Qanday topildi — o'lchov asbobi

Taxmin qilishni to'xtatib, o'lchaydigan asbob qo'yildi (`27d61ee760`):

1. **SQL profayler** — `sqlite3_trace_v2(gDb, SQLITE_TRACE_PROFILE, ...)`.
   Bajarilgan har bir operatorning vaqti so'rov matni bo'yicha yig'iladi,
   qaysi oqimda bo'lishidan qat'i nazar. Muhim nuqta: PROFILE o'lchovi
   `prepare`–`finalize` oralig'ini qamraydi, ya'ni **qatorlarni qayta
   ishlovchi C++ sikli ham** shu vaqtga kiradi.
2. **Nuqta registri** — `CustomDB::PerfScope` / `PerfNote`. Nomlangan kod
   nuqtalari (13 ta). Muhim: **qulf kutishi ham shu yerga tushadi** — UI
   oqimi fon oqimining SQL'ini kutsa, bu chaqiriq narxida ko'rinadi.
3. **Dump** — `Application` dagi stall watchdog blok tugashini sezgach
   `CustomDB::DumpSqlProfile()` ni chaqiradi: ikkala jadval log'ga chiqadi
   va **nollanadi**, shunda har bir blok o'z alohida hisobini ko'rsatadi.

Foydalanuvchining "balki bir nechta muammo ketma-ket bo'lgani uchun
umumiy hisobda 30+ bo'layotgandir" degan farazi aynan shu dizaynga
sabab bo'ldi — va u **to'g'ri chiqdi**.

Birinchi dump darhol javob berdi (33 282 ms blok):

```
db:EnsurePeerCacheLoaded   28987 ms / atigi 16 chaqiriq   (~1.8 s har biri)
  SQL backup/edited        17870 ms / 8 chaqiriq
  SQL deleted               8697 ms / 8 chaqiriq
```

Va fon oqimlarida ikkita gigant:

```
37927 ms / 1 chaqiriq : SELECT ... FROM activity_history a WHERE a.id = (...)
37592 ms / 1 chaqiriq : SELECT ... FROM media_index ORDER BY peer_id, msg_id
```

---

## 4. Hal qiluvchi dalil

Ikkinchi so'rov (`media_index ORDER BY ...`) `MediaIndexToJson()` dan
keladi, u esa **faqat `ExportFullBackup()` dan** chaqiriladi. Ya'ni
startda to'liq zaxira eksporti ishlayotgan edi. Zaxira arxivlari buni
tasdiqladi:

```
launch 15:39:xx  ->  CustomModBackup_20260912_153958.zip
launch 16:03:57  ->  CustomModBackup_20260912_160402.zip   (+5 s)
launch 18:00:42  ->  CustomModBackup_20260912_180048.zip   (+6 s)
```

`StartAutoBackup()` da: `QTimer::singleShot(5000, RunAutoBackup)` — **har
ishga tushirishda**, oxirgi zaxira qachon qilingani **umuman
tekshirilmasdan** (garchi niyat "har 24 soatda" bo'lsa ham). 62 MB baza
nusxasi + JSON + zip, aynan chatlar yuklanayotgan paytda.

**Raqobatning narxi — bu hujjatdagi eng muhim raqam.** O'sha so'rovlar
bazaning nusxasida (bo'sh mashinada) o'lchandi:

| So'rov | Bo'sh mashinada | Ilovada (start paytida) |
|---|---|---|
| Faollik keshi | 374 ms | **37 927 ms** (≈100×) |
| Peer kesh | 20 ms | **~2 200 ms** (≈110×) |

**So'rovlar sekin emas edi — ular navbatda turgan edi.** Barcha oqimlar
bitta `SQLITE_OPEN_FULLMUTEX` ulanishida ishlaydi, ya'ni har bir SQL
operatori (hatto o'qish ham) bitta mutex'da navbatga turadi.

---

## 5. Tuzatishlar

| Commit | Nima |
|---|---|
| `7c8479b441` | `gEditBackupIds` keshi (GetEditHistory SQL'siz erta qaytadi), `gGhostReadCache` (butun `ghost_reads` xotirada) |
| `f8e26268f5` | **Asosiy:** `RecentBackupExists()` (24 soat), zaxira start+5s → **start+3 daqiqa**, takroriy taymer 24s → 1s; `EnsurePeerCacheLoaded` dan `ORDER BY rowid` (TEMP B-TREE) olib tashlandi; faollik so'rovi `ROW_NUMBER()` ga o'tkazildi |
| `d689e15b8d` | `ReconcileMediaIndex`, `CompactActivityHistoryAsync`, `MediaQuota` papka skaneri → **start+90 soniya** (`kStartupQuietMs`) |
| `009d3c7a21` | Sync birinchi arm'i ≥90 s; `Checkpoint` `TRUNCATE` → `PASSIVE` (TRUNCATE soatiga bir marta) |
| `f143015a1d` | `onChangesAvailable()` start oynasida `syncNow()` ni chetlab o'tmaydi; WS ulangan bo'lsa interval ×10 (≤600 s) |

**Ataylab kechiktirilmagan:** `RestoreDeletedChats` (foydalanuvchiga
ko'rinadigan ish — saqlangan chatlarni ro'yxatga qaytaradi, 376 ms),
`PruneStaleActivityHistory` (~0.3 ms), kvota ogohlantirishi (atomik
o'qish), foydalanuvchi o'zi bosgan "Sync now".

---

## 6. Natija (o'lchangan, 18:35 startida)

```
18:35:15  main thread blocked 1490 ms   <- Scope 0 ms, SQL 28 ms (bizniki emas)
18:35:18  RestoreDeletedChats 376 ms
18:36:49  ReconcileMediaIndex (kechiktirilgan) 538 ms
18:36:49  main thread blocked 444 ms    <- ilgari shu joyda 33 282 ms edi
18:38:13  zaxira O'TKAZIB YUBORILDI (oxirgisi 18:01 da)
```

**~69 soniya → ~1.9 soniya**, ikkita blok. Qolgan 1490 ms bizning kodimiz
emas (oyna yaratish).

---

## 7. "Nega aynan upstream'dan keyin" — tekshiruv

Uchta gipoteza **rad etildi**:

1. **Bizning startdagi ishlar yangi emas.** `CompactActivityHistoryAsync`
   24-avg, `ReconcileMediaIndex` / `RestoreDeletedChats` 14-avg,
   `StartAutoBackup` esa **11-iyun**dan beri. Sentyabrda startga yangi ish
   qo'shilmagan.
2. **Ma'lumot sentyabrda sakramagan.** `activity_history` — 107 707
   qator, shundan 106 588 tasi `status` (last-seen); asosiy qismi 14–24
   avgustda tushgan (14-avg: 17 640, 15-avg: 12 015).
3. **Upstream'ning start kodidagi o'zgarishlari og'irlik qo'shmaydi.**
   `main_account.cpp` / `main_domain.cpp` diffi — logout paytida
   `destroySession()` ichidan qayta kirishdan himoya, ya'ni xavfsizlik
   tuzatishlari.

**Qolgan tushuntirish (gipoteza, mexanizm aniq, o'lchov hali yo'q):**
merge bilan **bir kunlarda** bizning tomondan sync kengaydi:

| Commit | Sana | Nima |
|---|---|---|
| `9e4bdfb0e1` | 07-sen | multi-device kalit almashinuvi + arxiv qabul qilish sync UI ga ulandi |
| `148cc7b719` | **08-sen** | **WebSocket orqali o'zgarish xabarnomalari** |
| `817adfc11e` | 08-sen | sync taymerini qayta armlash |

`148cc7b719` dan keyin: startda WebSocket ulanadi, server xabarnoma
yuborsa `onChangesAvailable()` → `syncNow()` → **darhol sikl**, vaqtdan
qat'i nazar. Ya'ni bitta SQLite ulanishi uchun uchinchi raqobatchi paydo
bo'ldi — aynan start paytida. Bu `f143015a1d` da yopildi.

**Xulosa:** vaqt mos kelgani rost, lekin sabab upstream merge'ining o'zi
emas — o'sha kunlardagi bizning sync/WebSocket ishimiz. Keyingi startda
profayler sync so'rovlarini start oynasida ko'rsatmasa, bu tasdiqlanadi.

---

## 8. Keyingi bosqichlar — sync izolyatsiyasi

Maqsad (foydalanuvchi ta'rifi): **hech bir jarayon boshqasiga xalaqit
qilmasin va bir-birini bloklab qo'ymasin.** Quyidagilar buzilgan narsani
tuzatish emas — bu sinfdagi muammo qaytib kelishini imkonsiz qilish
uchun.

### Avval: nima allaqachon bor (qayta qurmaslik uchun)

| Fikr | Holat |
|---|---|
| Interval sozlanadigan bo'lsin | ✅ `custom_tab_sync.cpp:437`, UI'da, 5–3600 s |
| Ma'lumotni bo'lib-bo'lib yuborish | ✅ push: `SyncPushChunkSize`; pull: `limit=` |
| "O'zgarish bormi?" yengil xabar | ⚠️ WebSocket **push** bor — so'rab turishdan yaxshiroq |

### 2-bosqich — navbat (BAJARILDI, `7ee739b265`)

✅ **Texnik xizmat uchun yagona ketma-ket navbat.**
`CustomDB::Maintenance::Enqueue(name, work)` — keyingi ish faqat
oldingisi tugagach boshlanadi, har birining vaqti log'ga chiqadi
(`CustomMod Maintenance: <nom> took <N> ms`). Navbatga o'tkazildi:
`CompactActivityHistory`, `ActivityCacheLoad`, `MediaQuotaScan`,
`AutoBackup`. Avto-zaxira navbat ichida **sinxron** `ExportFullBackup`
chaqiradi — `ExportFullBackupAsync` o'z oqimini ochib darhol qaytardi va
navbat uni "tugadi" deb o'ylardi.

Navbatga ataylab **olinmadi:** foydalanuvchi o'zi boshlaydigan amallar
(qo'lda eksport/import, `BackfillMediaSha256Async`, media skanerlari);
`ReconcileMediaIndex` (asosiy oqimda, 485 ms — fon oqimiga ko'chirish
oqim taxminlarini o'zgartirardi, ustiga u allaqachon
`CompactActivityHistoryAsync` dan oldin ketma-ket bajariladi).

❌ **"Bo'laklar orasida nafas" — kerak emas ekan.** Tekshiruvda ma'lum
bo'ldi: `pushPending()` har siklda **bitta** bo'lak yuboradi va tarmoq
callback'i orqali qaytadi, ya'ni bo'laklar allaqachon hodisa sikli orqali
nafas oladi.

➡️ **Tranzaksiyaga yig'ish 4-bosqichga ko'chdi.** Sync yo'llarida umuman
tranzaksiya yo'q: bir bo'lakda 50 tagacha (`syncPushChunkSize` standarti)
alohida `DELETE`, har biri o'z tranzaksiyasi va yozuv qulfi bilan. Buni
HOZIR tuzatish xavfli, chunki SQLite tranzaksiyasi **ulanishga** tegishli,
bizda esa ulanish barcha oqimlar orasida umumiy — asosiy oqim tranzaksiya
ochsa, fon oqimining yozuvi ham o'sha tranzaksiyaga tushib qoladi
(tranzaksiya qaytarilsa — yo'qoladi, tasdiqlansa — muddatidan oldin
tasdiqlanadi). Xavfsiz yo'l: avval 4-bosqich (alohida ulanish), keyin
tranzaksiya.

⏳ **Vaqt byudjeti** — hali qilinmadi. Navbat og'ir ishlarning ustma-ust
tushishini yo'q qildi, byudjet esa bitta uzun ishning o'zini bo'laklarga
bo'ladi (masalan compaction `DELETE` ni `LIMIT` bilan sikl qilish).

### 3-bosqich — arzon `seq` tekshiruvi (server tomoni ham kerak)

Hozirgi "bo'sh" sikl ham DB'ga tegadi: `GetState(device_id)`,
`GetState(refresh_token)`, `KeysAvailable()`, `Pending()` — to'rt-besh
kichik so'rov, har intervalda, abadiy.

Taklif: server `GET /api/v1/sync/head` → oxirgi `seq`. Klient uni
**xotiradagi** oxirgi seq bilan solishtiradi; teng bo'lsa va outbox bo'sh
bo'lsa — sikl umuman boshlanmaydi, DB'ga bitta ham so'rov ketmaydi.

⚠️ `customsync-server` loyihasida ham ish talab qiladi.

Ixtiyoriy qo'shimcha: alohida `syncIdleIntervalSeconds` sozlamasi (hozir
bo'sh interval `syncIntervalSeconds × 10`, ≤600 s deb hisoblanadi).

### 4-bosqich — sync uchun alohida DB ulanishi

- ✅ **Nima beradi:** hozir barcha oqimlarning har bir SQL operatori bitta
  `FULLMUTEX` mutex'ida navbatga turadi — hatto ikkita o'qish ham.
  Alohida ulanish bilan sync'ning **o'qishlari** UI o'qishlari bilan
  haqiqiy parallel ketadi (WAL ko'p o'quvchiga ruxsat beradi).
- ❌ **Nima bermaydi:** **yozuvlar baribir navbatda** — WAL'da bir vaqtda
  bitta yozuvchi; ikkinchisi `SQLITE_BUSY` oladi. `busy_timeout` +
  qayta urinish kerak.
- ✅ **Kesh muammosi yo'q:** keshlar C++ tuzilmalari, ularni SQLite emas,
  bizning kodimiz yangilaydi. Boshqa handle'dan yozilsa ham, yozuv o'sha
  C++ funksiyalaridan o'tsa kesh to'g'ri qoladi.
- 💰 **Narxi:** `execSql` va yordamchilar `gDb` ga qattiq bog'langan;
  outbox qatlamini handle qabul qiladigan qilish kerak.

---

## 9. Tozalash kerak bo'lgan narsa

O'lchov kodi (**SQL profayler + `PerfScope` registri**) hali kodda va
har chaqiriqda mutex qulfi oladi. Bir-ikki start tasdiqlangach uni olib
tashlash yoki bayroq ortiga yashirish kerak. Fayllar: `custom_db.cpp`
(`gProfile`, `gScopes`, `DumpSqlProfile`, `PerfNote`), `custom_db.h`
(`PerfScope`), `core/application.cpp` (watchdog dumpi),
`custom_settings.cpp`, `history/history.cpp`, `history/history_item.cpp`.

---

## 10. Umumiy saboqlar

1. **Sekin so'rov ≠ sekin ilova.** Bir xil so'rov bo'sh mashinada 374 ms,
   raqobat ostida 37 927 ms bo'ldi. Har doim ilovaning ICHIDAN o'lchang.
2. **Uch marta noto'g'ri gipoteza — bu asbob qo'yish vaqti kelgani
   signali**, to'rtinchi gipoteza qurish vaqti emas.
3. **Bir necha sabab ustma-ust tushishi mumkin.** Shuning uchun o'lchov
   asbobi bitta joyni emas, barcha nuqtani birdan sanashi kerak.
4. **Davriy texnik xizmat ishi uchun "oxirgi marta qachon bajarilgan?"
   tekshiruvi majburiy.** Bu yerda niyat "24 soatda bir" edi, kod esa
   har startda bajarardi — orada 3 oy o'tdi va hech kim sezmadi.
