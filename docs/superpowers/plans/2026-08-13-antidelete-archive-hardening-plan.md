# A13 — AntiDelete arxivini mustahkamlash: implementatsiya rejasi

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task.
> Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** AntiDelete arxivi hech qanday ma'lumot yo'qotmasin, va butun
chat o'chirilganda ham xabarlar chat ichida "o'chirilgan" belgisi bilan
ko'rinsin.

**Architecture:** Arxiv o'chirilish hodisasiga tayanishdan voz kechadi —
xabar ko'rilgan/yuklangan zahoti yoziladi, o'chirilish esa faqat belgi
qo'yadi. Ko'rsatish tomonida `loadDeletedMessages()`ning bo'sh chatda
ishlamasligi tuzatiladi. Arxiv yozuvi yangi `custom_archive` modulida
markazlashtiriladi va scroll paytida partiyalab yoziladi.

**Tech Stack:** C++17, Qt 5.15.18, SQLite (sqlite3 C API), CMake,
tdesktop fork (branch `Oybek`).

**Spec:** [`../specs/2026-08-13-antidelete-archive-hardening-design.md`](../specs/2026-08-13-antidelete-archive-hardening-design.md)

---

## Bu loyihaning o'ziga xosligi — DIQQAT BILAN O'QING

**Test freymvorki YO'Q.** tdesktop'da unit-test infratuzilmasi mavjud
emas, build esa ~34 daqiqa oladi va faqat foydalanuvchi ruxsati bilan
ishga tushiriladi. Shuning uchun:

- **Har vazifadan keyin build QILINMAYDI.** Soxta test buyruqlari
  yozmang va bajarmang.
- Har vazifa oxirida **kod darajasidagi tekshiruv**: o'zgartirilgan
  joyni qayta o'qib, (a) qavslar/nuqtali vergullar butun, (b) ishlatilgan
  har bir funksiya/tur mavjud va to'g'ri imzoga ega, (c) `#include`lar
  yetarli, (d) mavjud fayl uslubiga mos (tab bilan otступ,
  `u"..."_q` literal, izohlar o'zbekcha) ekanini tasdiqlang.
- **Yakunda BITTA umumiy build** + qo'lda tekshiruv (Task 12), A6 (Qt6)
  va A11 Task 6 bilan birga.

**Regressiya qoidalari (spec §4 — har vazifada amal qiling):**

1. Gate (`ShouldBackgroundCache` / `ShouldAntiDelete`) `false` bo'lganda
   kod yo'li **hozirgidan bit-darajasida farq qilmasin**.
2. **GhostMode mantig'iga tegilmaydi** — `history.cpp`da 7 ta joyda
   `CustomSettings::GhostMode()` bor, ularga qo'l urmang.
3. **UI oqimi bloklanmasin** — `addOlderSlice`/`addNewerSlice` scroll
   paytidagi issiq yo'l.
4. DB sxemasi orqaga mos bo'lsin — foydalanuvchida 155 MB real ma'lumot
   bor, eski fayl ochilaverishi shart.
5. A11 (story signal) va Activity History'ga tegilmaydi — ular
   `activity_history` jadvalida, bu ish `text_cache` +
   `actioned_messages` bilan cheklanadi.

**Git qoidalari:** commit'larda `Co-Authored-By` **ISHLATILMAYDI**.
Push faqat `origin/Oybek` ga; `upstream` ga hech qachon.

---

## Fayl tuzilmasi

| Fayl | Mas'uliyati | Holat |
|---|---|---|
| `Telegram/SourceFiles/history/history.h` | `_deletedInjectionReady` bayrog'i | Modify |
| `Telegram/SourceFiles/history/history.cpp` | Inject qulfini tuzatish (K1) + slice hook'lari (K2) | Modify |
| `Telegram/SourceFiles/custom_settings.cpp` | `ShouldBackgroundCache` zanjiri (K5.1) + legacy kalit migratsiyasi (K5.2) | Modify |
| `Telegram/SourceFiles/custom_db.h/.cpp` | `is_archived` ustuni, pruning, checkpoint (K6) | Modify |
| `Telegram/SourceFiles/custom_archive.h/.cpp` | **YANGI** — arxiv yozuvining yagona markazi (K2) | Create |
| `Telegram/SourceFiles/data/data_session.cpp` | Mavjud hook'ni helper'ga ko'chirish (K2) | Modify |
| `Telegram/SourceFiles/history/history_item.cpp` | Chiquvchi xabar hook'i (`setRealId`) (K2) | Modify |
| `Telegram/SourceFiles/custom_mod_window.cpp` | Chat holati + sabab + DB statistikasi (K5.3/K6.3) | Modify |
| `Telegram/cmake/telegram_sources.txt` | Yangi fayllarni ro'yxatga qo'shish | Modify |

---

# BOSQICH 1 — K1: Ko'rsatish

> Bu bosqich yolg'iz o'zi eng katta qiymatni beradi: DB'da allaqachon
> saqlangan 104 ta xabar (peer `7815103103`) darhol ko'rinadigan bo'ladi.

### Task 1: Bo'sh chatga inject qilishni yoqish

**Files:**
- Modify: `Telegram/SourceFiles/history/history.h:670-671`
- Modify: `Telegram/SourceFiles/history/history.cpp:170-185` (konstruktor)
- Modify: `Telegram/SourceFiles/history/history.cpp:2181-2191`
- Modify: `Telegram/SourceFiles/history/history.cpp:4584`

**Nega bu ishlaydi (implementator uchun kontekst):**
`insertMessageToBlocks()` (`history.cpp:4295-4301`) **allaqachon** bo'sh
tarixni qo'llab-quvvatlaydi:
```cpp
if (isEmpty()) {
    addNewToBack(item, false);
    return;
}
```
Demak `loadDeletedMessages()` boshidagi `if (isEmpty()) return;` bo'sh
chatni emas, **konstruktor paytidagi chaqiruvni** to'sish uchun qo'yilgan
(`history.cpp:183` — konstruktor ichida chaqiriladi, o'sha paytda
`History` hali owner xaritasida ro'yxatdan o'tmagan). Shuning uchun
shartni "chat bo'shmi" dan "konstruktor tugadimi" ga almashtiramiz.

- [ ] **Step 1: `history.h` ga bayroq qo'shish**

`history.h:670-671` dagi mavjud qatorlardan keyin (`_ghostReadTillId`
ostiga) qo'shing:

```cpp
	MsgId _topMessageId = 0; // CUSTOM GHOST MODE TRACKING
	qint64 _ghostReadTillId = 0; // FAST CACHE
	// CUSTOM A13: konstruktor tugagach true bo'ladi. loadDeletedMessages()
	// shu bayroqqa qaraydi — ilgari isEmpty() ishlatilardi, lekin u bo'sh
	// chatga (butun tarix o'chirilgan holat) inject qilishni ham to'sardi.
	bool _deletedInjectionReady = false;
```

- [ ] **Step 2: konstruktorda bayroqni oxirida yoqish**

`history.cpp:170-185` — konstruktor tanasining OXIRIGA qo'shing.
Diqqat: `loadDeletedMessages()` chaqiruvi **o'z joyida qoladi** va
bayroq undan **keyin** yoqiladi, shunda konstruktordagi chaqiruv
avvalgidek no-op bo'ladi.

```cpp
	loadDeletedMessages();
	updateCommunityRegistration();
	// CUSTOM A13: shu nuqtadan keyin History to'liq qurilgan — inject
	// xavfsiz. Yuqoridagi chaqiruv ataylab no-op bo'lib qoladi.
	_deletedInjectionReady = true;
}
```

- [ ] **Step 3: qulfni almashtirish**

`history.cpp:2181-2186` — mavjud izoh va shartni to'liq almashtiring:

```cpp
void History::loadDeletedMessages() {
	// CUSTOM A13: ilgari `if (isEmpty()) return;` edi — bu butun chat
	// o'chirilganda (blocks bo'sh) saqlangan xabarlarni ham ko'rsatmasdi.
	// Aslida himoya kerak bo'lgan yagona holat — konstruktor paytidagi
	// chaqiruv (insertMessageToBlocks → addNewToBack → addItemToBlock
	// yarim qurilgan History ustida ishlaydi). insertMessageToBlocks()
	// bo'sh tarixni o'zi to'g'ri hal qiladi (addNewToBack).
	if (!_deletedInjectionReady) return;

	const auto peerIdStr = QString::number(peer->id.value);
```

- [ ] **Step 4: `clear()` da Unload holatini himoyalash**

`history.cpp:4584` — `History::clear()` oxiridagi chaqiruv. Unload
(xotiradan bo'shatish) paytida inject qilish tarixni "yuklangan" holatga
noto'g'ri o'tkazadi. Faqat haqiqiy tozalash/o'chirishda inject qilamiz:

```cpp
	// CUSTOM A13: Unload — bu shunchaki xotiradan bo'shatish, inject
	// qilish noto'g'ri bo'lardi. ClearHistory/DeleteChat esa aynan
	// bizga kerak bo'lgan holat (suhbatdosh butun chatni o'chirdi).
	if (type != ClearType::Unload) {
		loadDeletedMessages();
	}
```

- [ ] **Step 5: kod darajasida tekshirish**

`history.h` va `history.cpp`dagi to'rtta joyni qayta o'qing:
1. `_deletedInjectionReady` e'lon qilingan va `private:` bo'limida
   (`_ghostReadTillId` bilan yonma-yon).
2. Konstruktorda bayroq `loadDeletedMessages()` dan KEYIN yoqilgan.
3. `loadDeletedMessages()` boshida `isEmpty()` o'rniga bayroq shartida.
4. `clear()` da `type != ClearType::Unload` qavslar bilan to'g'ri
   o'ralgan.

`ClearType` enum qiymatlari `history.h` da e'lon qilingan — `Unload`
nomi to'g'ri yozilganini tasdiqlang.

- [ ] **Step 6: Commit**

```bash
git add Telegram/SourceFiles/history/history.h Telegram/SourceFiles/history/history.cpp
git commit -m "A13/K1: bo'sh chatga o'chirilgan xabarlarni inject qilishni yoqish"
```

---

# BOSQICH 2 — K5 + K6: To'g'rilik poydevori

> K2 (arxiv qamrovi) bulardan OLDIN kelsa, noto'g'ri gate bilan
> arxivlanadi va pruning uni o'chirib yuboradi. Shuning uchun avval
> poydevor.

### Task 2: `ShouldBackgroundCache()` ustuvorlik zanjirini tuzatish (D3)

**Files:**
- Modify: `Telegram/SourceFiles/custom_settings.cpp:529-538`

- [ ] **Step 1: funksiyani almashtirish**

Mavjud tana:
```cpp
bool ShouldBackgroundCache(const QString &peerId) {
    if (!gInitialized) Init();
    if (peerId.isEmpty()) return false;
    if (IsInBlocklist(peerId)) return false;
    if (IsInWhitelist(peerId)) return true;
    if (HasPerPeerOverride(peerId)) {
        return AntiDeleteForPeer(peerId) || AntiEditForPeer(peerId);
    }
    return false;
}
```

Yangi tana:
```cpp
bool ShouldBackgroundCache(const QString &peerId) {
    if (!gInitialized) Init();
    if (peerId.isEmpty()) return false;
    // A13/D3: ilgari oxirgi bosqich `return false` edi — global
    // antiDelete/antiEdit bayrog'i e'tiborga olinmasdi. Natijada
    // ShouldAntiDelete() true qaytarib turgan chatda fon-cache umuman
    // ishlamasdi va butun-chat o'chirilishida ma'lumot yo'qolardi.
    // Endi zanjir ShouldAntiDelete/ShouldAntiEdit bilan bir xil:
    // Blocklist > Whitelist > per-peer override > global bayroq.
    if (IsInBlocklist(peerId)) return false;
    if (IsInWhitelist(peerId)) return true;
    return AntiDeleteForPeer(peerId) || AntiEditForPeer(peerId);
}
```

**Nega `HasPerPeerOverride` shoxobchasi kerak emas:**
`AntiDeleteForPeer()` (`custom_settings.cpp:353-357`) o'zi per-peer
xaritani tekshiradi va topilmasa global qiymatga tushadi — ya'ni yangi
kod ikkala holatni ham qamrab oladi.

- [ ] **Step 2: kod darajasida tekshirish**

`AntiDeleteForPeer` va `AntiEditForPeer` shu fayl ichida, shu
`namespace CustomSettings` da e'lon qilinganini tasdiqlang
(`custom_settings.cpp:353` va yaqinida `AntiEditForPeer`). Ikkalasi ham
`ShouldBackgroundCache` dan YUQORIDA turishi kerak (C++ e'lon tartibi).
Agar pastda bo'lsa — `custom_settings.h` dagi e'lon yetarli, tekshiring.

- [ ] **Step 3: Commit**

```bash
git add Telegram/SourceFiles/custom_settings.cpp
git commit -m "A13/K5: ShouldBackgroundCache global bayroqni hisobga olsin (D3)"
```

---

### Task 3: O'lik snake_case registry kalitlarini migratsiya qilish (D6)

**Files:**
- Modify: `Telegram/SourceFiles/custom_settings.cpp` — `Init()`, ~200-qator

**Muammo:** registry'da ikkita to'plam bor — `anti_delete`, `anti_edit`,
`ghost_mode`, `bypass_restrictions`, `offline_db` (snake_case, **kod
o'qimaydi**) va `antiDelete`, `antiEdit`, `ghostMode` (camelCase, kod shu
ularni o'qiydi). Foydalanuvchi eski kalitni ko'rib, sozlama yoqilgan deb
o'ylaydi.

- [ ] **Step 1: migratsiya kodini `Init()` ga qo'shish**

`custom_settings.cpp` `Init()` ichida, `gValues.antiDelete = ...`
o'qish qatoridan (~200) **OLDIN** qo'shing:

```cpp
    // A13/D6: eski (snake_case) kalitlarni bir marta ko'chirish.
    // Ular hech qachon o'qilmagan — foydalanuvchini chalg'itadi.
    // Faqat yangi kalit MAVJUD BO'LMASA ko'chiramiz, so'ng eskisini
    // o'chiramiz. Bir marta bajarilgach, keyingi ishga tushirishlarda
    // eski kalitlar yo'q bo'lgani uchun bu blok tekinga ishlaydi.
    {
        const auto migrate = [&](const char *oldKey, const char *newKey) {
            if (settings.contains(oldKey)) {
                if (!settings.contains(newKey)) {
                    settings.setValue(newKey, settings.value(oldKey));
                }
                settings.remove(oldKey);
            }
        };
        migrate("anti_delete", "antiDelete");
        migrate("anti_edit", "antiEdit");
        migrate("ghost_mode", "ghostMode");
        migrate("bypass_restrictions", "bypassRestrictions");
        migrate("offline_db", "offlineDb");
    }
```

- [ ] **Step 2: kod darajasida tekshirish**

1. `settings` o'zgaruvchisi shu qamrovda mavjudligini tasdiqlang
   (`Init()` ichida `QSettings settings("CustomMod", "TelegramDesktop");`
   kabi e'lon bo'lishi kerak) — agar nomi boshqacha bo'lsa, shu nomga
   moslang.
2. Blok `gValues.antiDelete = settings.value("antiDelete", true)...`
   dan OLDIN turganini tasdiqlang — aks holda migratsiya kech qoladi.
3. `migrate` lambda `[&]` bilan olinganini tasdiqlang.

- [ ] **Step 3: Commit**

```bash
git add Telegram/SourceFiles/custom_settings.cpp
git commit -m "A13/K5: o'lik snake_case sozlama kalitlarini migratsiya qilish (D6)"
```

---

### Task 4: Arxivlangan yozuvlarni pruning'dan himoyalash (D4)

**Files:**
- Modify: `Telegram/SourceFiles/custom_db.h` — `CacheMessageText` e'loni
- Modify: `Telegram/SourceFiles/custom_db.cpp:140-148` (sxema migratsiyasi)
- Modify: `Telegram/SourceFiles/custom_db.cpp:801-814` (`PruneStaleCachedText`)
- Modify: `Telegram/SourceFiles/custom_db.cpp:816-853` (`CacheMessageText`)

**Muammo:** `PruneStaleCachedText(30)` 30 kundan eski hamma qatorni
o'chiradi. To'liq arxivga o'tilgach bu arxivni yo'q qiladi.

**Nega `CustomSettings`ni bu yerdan chaqirib bo'lmaydi:** `custom_db.cpp`
ichidagi mavjud izoh (963-1017 oralig'ida) buni aniq aytadi — aylanma
bog'liqlik (circular dependency). Shuning uchun qaror **chaqiruvchi**
tomonda qabul qilinadi va DB'ga `is_archived` bayrog'i sifatida
uzatiladi.

- [ ] **Step 1: sxema migratsiyasi**

`custom_db.cpp` `Init()` ichida, PRAGMA'lardan (140-148) va jadval
yaratishdan KEYIN qo'shing:

```cpp
    // A13/D4: text_cache ga is_archived ustuni. 0 = vaqtinchalik cache
    // (eskirsa o'chsa bo'ladi), 1 = doimiy arxiv (hech qachon
    // o'chirilmaydi). Eski DB fayllari uchun xavfsiz: ustun bo'lmasa
    // qo'shamiz, bo'lsa tegmaymiz.
    {
        bool hasArchivedColumn = false;
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(gDb, "PRAGMA table_info(text_cache)",
                -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (colText(stmt, 1) == u"is_archived"_q) {
                    hasArchivedColumn = true;
                    break;
                }
            }
            sqlite3_finalize(stmt);
        }
        if (!hasArchivedColumn) {
            execSql("ALTER TABLE text_cache "
                    "ADD COLUMN is_archived INTEGER DEFAULT 0");
        }
    }
```

- [ ] **Step 2: `PruneStaleCachedText` ni himoyalash**

`custom_db.cpp:801-814` — SQL qatorini almashtiring:

```cpp
    if (sqlite3_prepare_v2(gDb,
            // A13/D4: arxivlangan qatorlarga TEGMAYMIZ — ular doimiy.
            "DELETE FROM text_cache "
            "WHERE cached_at < ? AND COALESCE(is_archived, 0) = 0",
            -1, &stmt, nullptr) == SQLITE_OK) {
```

- [ ] **Step 3: `CacheMessageText` ga `archived` parametri**

`custom_db.h` dagi e'lonni yangilang (mavjud e'lon ~195-qatorda) —
oxiriga standart qiymatli parametr qo'shing:

```cpp
void CacheMessageText(
    const QString &peerId,
    long long msgId,
    const QString &text,
    bool isOut,
    unsigned int msgDate,
    const QString &senderId,
    bool isMedia,
    bool archived = false);
```

`custom_db.cpp:816-853` dagi ta'rifni ham shunga moslang (standart
qiymat FAQAT e'londa bo'ladi, ta'rifda emas) va INSERT'ni yangilang:

```cpp
    if (sqlite3_prepare_v2(gDb,
            "INSERT OR REPLACE INTO text_cache "
            "(peer_id, msg_id, text, is_out, msg_date, cached_at, "
            "sender_id, is_media, is_archived) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, msgId);
        bindText(stmt, 3, text);
        sqlite3_bind_int(stmt, 4, isOut ? 1 : 0);
        sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(msgDate));
        sqlite3_bind_int64(stmt, 6, QDateTime::currentSecsSinceEpoch());
        bindText(stmt, 7, senderId);
        sqlite3_bind_int(stmt, 8, isMedia ? 1 : 0);
        sqlite3_bind_int(stmt, 9, archived ? 1 : 0);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
```

- [ ] **Step 4: kod darajasida tekshirish**

1. `colText()` yordamchisi `custom_db.cpp:96` da mavjud — `PRAGMA
   table_info` natijasining 1-ustuni (0-indeksli) ustun nomi ekanini
   tasdiqlang.
2. `execSql()` `custom_db.cpp:78` da mavjud.
3. `CacheMessageText`ning **mavjud 2 ta chaqiruvchisi**
   (`custom_db.cpp:926` va `:958`, `RecordBackgroundEdit` ichida)
   o'zgarishsiz ishlashini tasdiqlang — standart parametr tufayli
   ular `archived=false` oladi, ya'ni **hozirgi xatti-harakat
   saqlanadi** (regressiya qoidasi №1).
4. `data_session.cpp:3567` dagi chaqiruv ham o'zgarishsiz qoladi.

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/custom_db.h Telegram/SourceFiles/custom_db.cpp
git commit -m "A13/K6: text_cache ga is_archived ustuni, pruning arxivga tegmasin (D4)"
```

---

### Task 5: WAL checkpoint — to'satdan o'chishda yo'qotmaslik (D5)

**Files:**
- Modify: `Telegram/SourceFiles/custom_db.h` — yangi e'lon
- Modify: `Telegram/SourceFiles/custom_db.cpp` — `Checkpoint()` ta'rifi

**Qaror (spec §K6.2):** `synchronous=NORMAL` **saqlanadi** — `FULL`ga
o'tish har bir yozuvni sekinlashtiradi. Uning o'rniga muntazam
checkpoint.

- [ ] **Step 1: `custom_db.h` ga e'lon qo'shish**

Fayldagi boshqa e'lonlar yonига (masalan `PruneStaleCachedText` e'loni
yaqiniga) qo'shing:

```cpp
// A13/D5: WAL faylini asosiy DB ga ko'chiradi. journal_mode=WAL +
// synchronous=NORMAL da to'satdan tok o'chsa, oxirgi checkpoint'dan
// keyingi tranzaksiyalar yo'qolishi mumkin — muntazam chaqirish shu
// oynani qisqartiradi. Ilova yopilishida va davriy chaqiriladi.
void Checkpoint();
```

- [ ] **Step 2: `custom_db.cpp` ga ta'rif qo'shish**

`PruneStaleCachedText` funksiyasidan keyin qo'shing:

```cpp
void Checkpoint() {
    Init();
    if (!gDb) return;
    execSql("PRAGMA wal_checkpoint(TRUNCATE)");
}
```

- [ ] **Step 3: yopilishda chaqirish**

`CustomDB` da mavjud yopish/tugatish funksiyasini toping:

```bash
grep -n "sqlite3_close\|void Shutdown\|void Close" Telegram/SourceFiles/custom_db.cpp
```

Topilgan yopish funksiyasi ichida, `sqlite3_close(...)` chaqiruvidan
**OLDIN** qo'shing:

```cpp
    // A13/D5: yopishdan oldin WAL ni asosiy faylga ko'chiramiz.
    execSql("PRAGMA wal_checkpoint(TRUNCATE)");
```

Agar bunday funksiya umuman yo'q bo'lsa — bu qadamni o'tkazib yuboring
va Step 4 dagi davriy checkpoint yetarli deb hisoblang; sababini
commit xabarida qayd eting.

- [ ] **Step 4: davriy checkpoint**

`custom_archive.cpp` hali yaratilmagani uchun davriy taymer **Task 6**
da (`CustomArchive::StartMaintenance()` ichida) qo'shiladi. Bu
vazifada faqat `Checkpoint()` funksiyasi va yopilish nuqtasi
tayyorlanadi.

- [ ] **Step 5: kod darajasida tekshirish**

`execSql()` mavjud va `static` ekanini (`custom_db.cpp:78`) hamda
`Checkpoint()` `namespace CustomDB` ichida ekanini tasdiqlang.

- [ ] **Step 6: Commit**

```bash
git add Telegram/SourceFiles/custom_db.h Telegram/SourceFiles/custom_db.cpp
git commit -m "A13/K6: WAL checkpoint funksiyasi va yopilishda chaqiruv (D5)"
```

---

# BOSQICH 3 — K2: Arxiv qamrovi

### Task 6: `custom_archive` moduli va CMake ro'yxati

**Files:**
- Create: `Telegram/SourceFiles/custom_archive.h`
- Create: `Telegram/SourceFiles/custom_archive.cpp`
- Modify: `Telegram/cmake/telegram_sources.txt`

**Nega alohida modul:** arxiv mantig'i bitta joyda tursin, chaqiruv
nuqtalari (`history.cpp`, `data_session.cpp`, `history_item.cpp`)
ingichka bo'lsin. A9 da `custom_upstream.cpp` shu naqsh bilan
qo'shilgan edi.

- [ ] **Step 1: `custom_archive.h` yaratish**

```cpp
#pragma once

// not_null<> uchun. Loyihada bu odatda base/basic_types.h yoki
// prekompilyatsiya qilingan header orqali keladi — `custom_upstream.h`
// va `custom_activity_history.h` qanday qilgan bo'lsa, AYNAN shunday
// qiling (o'sha fayllarni ochib solishtiring, ortiqcha include
// qo'shmang).
#include "base/basic_types.h"

class HistoryItem;

namespace CustomArchive {

// Xabarni doimiy arxivga yozadi, agar shu peer kuzatilayotgan bo'lsa
// (CustomSettings::ShouldBackgroundCache). Idempotent — bir xil xabarni
// qayta berish xavfsiz (INSERT OR REPLACE).
//
// Partiya rejimida (BeginBatch/EndBatch orasida) yozuvlar to'planadi va
// bitta tranzaksiyada saqlanadi — scroll paytida jank bo'lmasligi uchun.
void MaybeArchiveItem(not_null<HistoryItem*> item);

// Partiya boshlanishi/tugashi. Ichma-ich chaqirilishi xavfsiz
// (hisoblagich bilan). EndBatch partiyani DB ga yozadi.
void BeginBatch();
void EndBatch();

// Davriy WAL checkpoint taymerini ishga tushiradi (A13/D5).
// Ilova ishga tushganda bir marta chaqiriladi.
void StartMaintenance();

} // namespace CustomArchive
```

- [ ] **Step 2: `custom_archive.cpp` yaratish**

```cpp
#include "custom_archive.h"

#include "custom_db.h"
#include "custom_settings.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
#include <QtCore/QTimer>
#include <vector>

namespace CustomArchive {
namespace {

struct PendingRow {
	QString peerId;
	long long msgId = 0;
	QString text;
	QString senderId;
	bool isOut = false;
	bool isMedia = false;
	unsigned int msgDate = 0;
};

// Partiya chuqurligi: BeginBatch/EndBatch ichma-ich chaqirilishi mumkin.
int gBatchDepth = 0;
std::vector<PendingRow> gPending;

// Partiya juda kattalashib ketmasin — oraliq yozuv chegarasi.
constexpr auto kFlushThreshold = 200;

void FlushPending() {
	if (gPending.empty()) {
		return;
	}
	// Bitta tranzaksiya — 200 ta alohida yozuv o'rniga.
	CustomDB::ExecRaw("BEGIN");
	for (const auto &row : gPending) {
		CustomDB::CacheMessageText(
			row.peerId,
			row.msgId,
			row.text,
			row.isOut,
			row.msgDate,
			row.senderId,
			row.isMedia,
			true); // archived = true → pruning tegmaydi
	}
	CustomDB::ExecRaw("COMMIT");
	gPending.clear();
}

} // namespace

void MaybeArchiveItem(not_null<HistoryItem*> item) {
	const auto history = item->history();
	if (!history) {
		return;
	}
	const auto peerIdStr = QString::number(history->peer->id.value);
	if (!CustomSettings::ShouldBackgroundCache(peerIdStr)) {
		return; // kuzatilmayotgan chat — hech narsa qilmaymiz
	}
	if (!IsServerMsgId(item->id)) {
		return; // lokal/yuborilayotgan xabar — hali server ID yo'q
	}
	const auto text = item->originalText().text;
	const auto isMedia = (item->media() != nullptr);
	if (text.isEmpty() && !isMedia) {
		return; // saqlashga arzimaydi (mavjud CacheMessageText mantig'i)
	}

	auto row = PendingRow{
		.peerId = peerIdStr,
		.msgId = static_cast<long long>(item->id.bare),
		.text = text,
		.senderId = QString::number(item->from()->id.value),
		.isOut = item->out(),
		.isMedia = isMedia,
		.msgDate = static_cast<unsigned int>(item->date()),
	};

	if (gBatchDepth > 0) {
		gPending.push_back(std::move(row));
		if (gPending.size() >= kFlushThreshold) {
			FlushPending();
		}
		return;
	}
	// Partiyadan tashqarida (real-vaqt yo'li) — bittalab yozamiz.
	CustomDB::CacheMessageText(
		row.peerId, row.msgId, row.text, row.isOut,
		row.msgDate, row.senderId, row.isMedia, true);
}

void BeginBatch() {
	++gBatchDepth;
}

void EndBatch() {
	if (gBatchDepth > 0) {
		--gBatchDepth;
	}
	if (gBatchDepth == 0) {
		FlushPending();
	}
}

void StartMaintenance() {
	// A13/D5: davriy WAL checkpoint — to'satdan tok o'chganda
	// yo'qotish oynasini daqiqalargacha qisqartiradi.
	static QTimer *timer = nullptr;
	if (timer) {
		return;
	}
	timer = new QTimer();
	QObject::connect(timer, &QTimer::timeout, [] {
		CustomDB::Checkpoint();
	});
	timer->start(5 * 60 * 1000); // 5 daqiqa
}

} // namespace CustomArchive
```

- [ ] **Step 3: `CustomDB::ExecRaw` yordamchisini qo'shish**

`custom_archive.cpp` `BEGIN`/`COMMIT` uchun DB'ga to'g'ridan-to'g'ri
murojaat qiladi, lekin `gDb` `custom_db.cpp` ichida `static`. Shuning
uchun `custom_db.h` ga qo'shing:

```cpp
// A13: tashqi modullar uchun oddiy SQL bajaruvchi (BEGIN/COMMIT kabi).
// Natija qaytarmaydigan buyruqlar uchun.
void ExecRaw(const char *sql);
```

`custom_db.cpp` ga ta'rif (`Checkpoint()` yonига):

```cpp
void ExecRaw(const char *sql) {
    Init();
    if (!gDb) return;
    execSql(sql);
}
```

- [ ] **Step 4: CMake ro'yxatiga qo'shish**

`Telegram/cmake/telegram_sources.txt` ni oching va `custom_upstream.cpp`
/ `custom_upstream.h` qatorlarini toping:

```bash
grep -n "custom_upstream\|custom_db\|custom_settings" Telegram/cmake/telegram_sources.txt
```

O'sha guruhga, alifbo tartibini saqlagan holda qo'shing:

```
    custom_archive.cpp
    custom_archive.h
```

Mavjud qatorlar qanday otступ/prefiks bilan yozilgan bo'lsa — **aynan
shunday** yozing (fayldagi naqshga qarang).

- [ ] **Step 5: `StartMaintenance()` ni ishga tushirish**

`CustomUpstream` yoki `CustomActivityHistory` qayerdan ishga
tushirilishini toping:

```bash
grep -rn "CustomActivityHistory::Init\|CustomUpstream::" Telegram/SourceFiles --include=*.cpp | grep -v custom_
```

Topilgan startup joyida, o'sha chaqiruvlar yonига qo'shing:

```cpp
	CustomArchive::StartMaintenance();
```

va faylning `#include` bo'limiga:

```cpp
#include "custom_archive.h"
```

- [ ] **Step 6: kod darajasida tekshirish**

1. `IsServerMsgId()` funksiyasi mavjud va qaysi header'dan kelishini
   tasdiqlang (`data/data_types.h` ehtimoli yuqori) — kerak bo'lsa
   `#include` qo'shing.
2. `item->history()` `not_null<History*>` qaytarsa, `if (!history)`
   sharti keraksiz bo'ladi — imzoni tekshirib, keraksiz bo'lsa olib
   tashlang (kompilyator ogohlantirishi chiqmasin).
3. `item->from()` `nullptr` bo'lishi mumkinmi — `data_session.cpp:3565`
   da xuddi shunday `result->from()->id.value` ishlatilgan, demak
   xavfsiz.
4. Designated initializer (`.peerId = ...`) C++20 xususiyati —
   loyihada boshqa joyda ishlatilganini tasdiqlang
   (`custom_activity_history.cpp` da `PendingStoryMedia` uchun oddiy
   `{ a, b, c }` ishlatilgan). Agar shubha bo'lsa, oddiy tartibli
   initializer'ga o'ting.

- [ ] **Step 7: Commit**

```bash
git add Telegram/SourceFiles/custom_archive.h Telegram/SourceFiles/custom_archive.cpp Telegram/SourceFiles/custom_db.h Telegram/SourceFiles/custom_db.cpp Telegram/cmake/telegram_sources.txt
git commit -m "A13/K2: custom_archive moduli, partiyali yozuv va CMake ro'yxati"
```

---

### Task 7: Scrollback (eski tarix) hook'lari

**Files:**
- Modify: `Telegram/SourceFiles/history/history.cpp` — `addOlderSlice()`
  (~1861) va `addNewerSlice()` (~1915)

**Bu D2 ning asosiy tuzatishi** — serverdan yuklangan eski tarix
birinchi marta arxivlanadi.

- [ ] **Step 1: `#include` qo'shish**

`history.cpp` ning include bo'limiga (boshqa `custom_*` include'lari
yonига) qo'shing:

```cpp
#include "custom_archive.h"
```

- [ ] **Step 2: `addOlderSlice()` oxirini yangilash**

`history.cpp:~1859-1862` — mavjud kod:

```cpp
	checkLocalMessages();
	checkLastMessage();
	loadDeletedMessages();
}
```

Yangi:

```cpp
	checkLocalMessages();
	checkLastMessage();
	// A13/K2: serverdan yangi kelgan eski tarixni doimiy arxivga
	// yozamiz. Partiya rejimi — butun slice bitta tranzaksiyada,
	// scroll paytida jank bo'lmasligi uchun.
	CustomArchive::BeginBatch();
	for (const auto &block : blocks) {
		for (const auto &view : block->messages) {
			CustomArchive::MaybeArchiveItem(view->data());
		}
	}
	CustomArchive::EndBatch();
	loadDeletedMessages();
}
```

- [ ] **Step 3: `addNewerSlice()` oxirini yangilash**

`history.cpp:~1913-1916` — xuddi shu naqsh:

```cpp
	checkLocalMessages();
	checkLastMessage();
	// A13/K2: yangi yuklangan oraliqni doimiy arxivga yozamiz.
	CustomArchive::BeginBatch();
	for (const auto &block : blocks) {
		for (const auto &view : block->messages) {
			CustomArchive::MaybeArchiveItem(view->data());
		}
	}
	CustomArchive::EndBatch();
	loadDeletedMessages();
}
```

- [ ] **Step 4: kod darajasida tekshirish**

1. `blocks` — `std::vector<std::unique_ptr<HistoryBlock>>`; har bir
   blokda `messages` — `std::vector<std::unique_ptr<Element>>`.
   `view->data()` `not_null<HistoryItem*>` qaytaradi. Shu tuzilmani
   `history.cpp:4304-4307` dagi mavjud kod bilan solishtirib
   tasdiqlang (u yerda `block->messages[itemIndex]->data()->date()`
   ishlatilgan) — imzolar mos kelishi shart.
2. `MaybeArchiveItem` ichida gate bor, shuning uchun kuzatilmayotgan
   chatda bu sikl faqat bo'sh aylanish bo'ladi. **Regressiya qoidasi
   №1 bajarilishini tasdiqlang** — DB ga hech narsa yozilmaydi.
3. Bu sikl **butun** `blocks` bo'ylab yuradi, faqat yangi qo'shilgan
   qism bo'ylab emas. Bu ataylab: `CacheMessageText` `INSERT OR
   REPLACE` bo'lgani uchun idempotent, va partiya bitta tranzaksiyada
   ketadi. Agar keyinchalik performans muammosi topilsa, faqat yangi
   slice'ni aylanish optimizatsiyasi qo'shiladi — hozir soddalik
   ustuvor.

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/history/history.cpp
git commit -m "A13/K2: scrollback orqali yuklangan tarixni arxivlash (D2)"
```

---

### Task 8: Chiquvchi xabarlar va real-vaqt yo'lini birlashtirish

**Files:**
- Modify: `Telegram/SourceFiles/history/history_item.cpp:2983-2998`
  (`setRealId`)
- Modify: `Telegram/SourceFiles/data/data_session.cpp:3558-3576`

- [ ] **Step 1: chiquvchi xabar hook'i (`setRealId`)**

`HistoryItem::setRealId(MsgId newId)` — aynan shu nuqtada shu
klientdan yuborilgan xabar server ID sini oladi va to'liq tayyor
bo'ladi.

`history_item.cpp:2998` dagi
`_history->owner().notifyItemIdChange({ fullId(), oldId });`
qatoridan **KEYIN** qo'shing:

```cpp
	// A13/K2: shu klientdan yuborilgan xabar endi server ID ga ega —
	// doimiy arxivga yozamiz. Ilgari arxivga faqat KELGAN xabarlar
	// tushardi (data_session.cpp addNewMessage), yuborilganlar esa
	// butun-chat o'chirilishida yo'qolardi.
	CustomArchive::MaybeArchiveItem(this);
```

Faylning include bo'limiga qo'shing:

```cpp
#include "custom_archive.h"
```

- [ ] **Step 2: real-vaqt hook'ini helper'ga ko'chirish**

`data_session.cpp:3558-3576` dagi mavjud blokni almashtiring:

```cpp
	// T32+ (KRITIK-1): Background AntiEdit/AntiDelete cache MARKAZI.
	// A13/K2: mantiq CustomArchive::MaybeArchiveItem() ga ko'chirildi —
	// gate, bo'sh-matn tekshiruvi va yozuv endi bitta joyda turadi.
	// Xatti-harakat o'zgarmagan, faqat is_archived=1 bo'lib yoziladi
	// (pruning tegmasligi uchun — D4).
	if (result && type == NewMessageType::Unread) {
		CustomArchive::MaybeArchiveItem(result);
	}
	return result;
}
```

Faylning include bo'limiga qo'shing:

```cpp
#include "custom_archive.h"
```

- [ ] **Step 3: kod darajasida tekshirish**

1. `setRealId` ichida `this` — `HistoryItem*`; `MaybeArchiveItem`
   `not_null<HistoryItem*>` kutadi. `not_null` `this` dan bevosita
   qurilishini tasdiqlang (loyihada odatiy).
2. `setRealId` boshidagi `Expects(isSending() || textAppearing());`
   buzilmaganini tasdiqlang — biz faqat funksiya oxiriga qo'shdik.
3. `data_session.cpp` da eski kodda ishlatilgan `peerIdStr`,
   `textValue`, `hasMedia`, `senderIdStr` o'zgaruvchilari endi
   **ishlatilmaydi** — ular olib tashlanganini va "unused variable"
   ogohlantirishi qolmaganini tasdiqlang.
4. `data_session.cpp` da `CustomDB::CacheMessageText` chaqiruvi
   olib tashlangach, `custom_db.h` include'i shu faylda boshqa
   maqsadda hali kerakmi — tekshiring, keraksiz bo'lsa ham
   **olib tashlamang** (boshqa CustomDB chaqiruvlari bor:
   `processMessagesDeleted` va h.k.).

- [ ] **Step 4: Commit**

```bash
git add Telegram/SourceFiles/history/history_item.cpp Telegram/SourceFiles/data/data_session.cpp
git commit -m "A13/K2: chiquvchi xabarlar arxivi va real-vaqt yo'lini birlashtirish"
```

---

# BOSQICH 4 — K3 + K4: Yangi funksiyalar

### Task 9: Custom Window — chat holati, sabab va DB statistikasi (K5.3, K6.3)

**Files:**
- Modify: `Telegram/SourceFiles/custom_settings.h` — yangi e'lon
- Modify: `Telegram/SourceFiles/custom_settings.cpp` — yangi funksiya
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp` — UI

- [ ] **Step 1: sababni qaytaruvchi funksiya (`custom_settings.h`)**

`ShouldBackgroundCache` e'loni yonига qo'shing:

```cpp
// A13/K5.3: chat kuzatilyaptimi va NEGA — foydalanuvchiga ko'rsatish
// uchun. Qaytadi: "Blocklist (o'chirilgan)", "Whitelist (yoqilgan)",
// "Shu chat sozlamasi (yoqilgan/o'chirilgan)" yoki
// "Umumiy sozlama (yoqilgan/o'chirilgan)".
[[nodiscard]] QString TrackingReason(const QString &peerId);
```

- [ ] **Step 2: ta'rif (`custom_settings.cpp`)**

`ShouldBackgroundCache` dan keyin qo'shing:

```cpp
QString TrackingReason(const QString &peerId) {
    if (!gInitialized) Init();
    if (peerId.isEmpty()) {
        return u"Noma'lum chat"_q;
    }
    if (IsInBlocklist(peerId)) {
        return u"Qora ro'yxat — kuzatilmaydi"_q;
    }
    if (IsInWhitelist(peerId)) {
        return u"Oq ro'yxat — kuzatiladi"_q;
    }
    const auto perPeer = gAntiDeletePerPeer.constFind(peerId);
    if (perPeer != gAntiDeletePerPeer.constEnd()) {
        return perPeer.value()
            ? u"Shu chat sozlamasi — kuzatiladi"_q
            : u"Shu chat sozlamasi — kuzatilmaydi"_q;
    }
    return gValues.antiDelete
        ? u"Umumiy sozlama — kuzatiladi"_q
        : u"Umumiy sozlama — kuzatilmaydi"_q;
}
```

- [ ] **Step 3: DB statistikasi (`custom_db.h` + `.cpp`)**

`custom_db.h` ga:

```cpp
// A13/K6.3: DB hajmi (bayt) va arxivdagi xabarlar soni.
[[nodiscard]] qint64 DatabaseSizeBytes();
[[nodiscard]] int ArchivedMessageCount();
```

`custom_db.cpp` ga (`Checkpoint()` yonига):

```cpp
qint64 DatabaseSizeBytes() {
    return QFileInfo(dbFilePath()).size();
}

int ArchivedMessageCount() {
    Init();
    if (!gDb) return 0;
    int result = 0;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT COUNT(*) FROM text_cache "
            "WHERE COALESCE(is_archived, 0) = 1",
            -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}
```

`custom_db.cpp` include bo'limiga `#include <QtCore/QFileInfo>`
qo'shing (agar hali yo'q bo'lsa).

- [ ] **Step 4: Custom Window'ga ko'rsatish**

`custom_mod_window.cpp` da AntiDelete bo'limini toping:

```bash
grep -n "AntiDelete\|antiDelete" Telegram/SourceFiles/custom_mod_window.cpp
```

O'sha bo'limga, A9 da `addToggle` va statuslar qanday qo'shilgan bo'lsa
**shu naqsh bilan**, statik matn qatori qo'shing:

```cpp
	AddSkip(content);
	content->add(object_ptr<Ui::FlatLabel>(
		content,
		u"Arxiv: "_q
			+ QString::number(CustomDB::ArchivedMessageCount())
			+ u" xabar, DB hajmi: "_q
			+ QString::number(CustomDB::DatabaseSizeBytes() / (1024 * 1024))
			+ u" MB"_q,
		st::boxDividerLabel),
		st::boxRowPadding);
```

**Diqqat:** `Ui::FlatLabel`, `st::boxDividerLabel`, `st::boxRowPadding`,
`AddSkip` — bu nomlar shu faylda allaqachon ishlatilganini tasdiqlang
va **aynan mavjud naqshga** moslang. Agar boshqa stil ishlatilgan
bo'lsa — o'shani ishlating, yangi stil ixtiro qilmang.

- [ ] **Step 5: kod darajasida tekshirish**

1. `gAntiDeletePerPeer` — `custom_settings.cpp` ichidagi anonim
   namespace'dagi global; `TrackingReason` shu fayl ichida bo'lgani
   uchun unga kirish bor.
2. `gValues.antiDelete` mavjud maydon (`custom_settings.h`).
3. `dbFilePath()` `custom_db.cpp:72` da `static` — shu fayl ichidan
   chaqirilyapti, muammo yo'q.
4. `TrackingReason` hozircha faqat kelajakdagi per-chat UI uchun —
   agar shu vazifada uni hech qayerdan chaqirmasangiz, kompilyator
   "unused" demaydi (ommaviy funksiya), lekin **spec §K5.3 talabini
   bajarish uchun uni per-chat sozlama oynasida ishlatish kerak**.
   Agar `custom_mod_window.cpp` da per-chat ro'yxat mavjud bo'lsa
   (`GetPerPeerOverrides()` ishlatilgan joy), har bir qator ostiga
   `TrackingReason(peerId)` matnini qo'shing.

- [ ] **Step 6: Commit**

```bash
git add Telegram/SourceFiles/custom_settings.h Telegram/SourceFiles/custom_settings.cpp Telegram/SourceFiles/custom_db.h Telegram/SourceFiles/custom_db.cpp Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "A13/K5-K6: chat kuzatuv sababi va arxiv statistikasi UI da"
```

---

### Task 10: K3 — "Bu chatning butun tarixini arxivla" tugmasi

> ⏸️ **KECHIKTIRILDI (2026-08-13, implementatsiya paytida qabul qilingan
> qaror).**
>
> **Sabab (xotira xavfi):** butun tarixni fon rejimida yuklash — barcha
> xabarlarni `HistoryItem` obyektlari sifatida XOTIRAGA yuklash demak
> (`addOlderSlice()` shunday ishlaydi). 10 000 xabarli chat uchun bu
> sezilarli xotira sarfi; Telegram aynan shuning uchun tarixni
> lazy/bo'lak-bo'lak yuklaydi. Buni to'g'ri qilish uchun alohida,
> xotiraga yuklamaydigan yo'l kerak (masalan MTProto javobini
> `HistoryItem` yaratmasdan to'g'ridan-to'g'ri arxivga yozish) — bu
> alohida dizayn ishi.
>
> **Nega bu maqbul:** K3 — mavjud nuqsonni tuzatish emas, balki QULAYLIK
> funksiyasi. Task 7 (scrollback hook) tufayli foydalanuvchi chatda
> yuqoriga scroll qilgani zahoti tarix avtomatik arxivlanadi — ya'ni
> asosiy himoya allaqachon ishlaydi. Bu tugma faqat "scroll qilmasdan,
> hoziroq himoyala" holatini qulaylashtirardi.
>
> **Qayta ko'rib chiqish sharti:** foydalanuvchi shu qulaylikni aniq
> so'rasa, avval xotiraga yuklamaydigan yondashuv bo'yicha qisqa
> brainstorming qilinadi.
>
> Quyidagi qadamlar KELAJAK uchun saqlanadi, hozir bajarilmaydi.

**Files:**
- Modify: `Telegram/SourceFiles/custom_archive.h/.cpp` — yangi funksiya
- Modify: chat menyusi fayli (Step 1 da aniqlanadi)

- [ ] **Step 1: menyu joyini aniqlash**

```bash
grep -rn "Faollik tarixi" Telegram/SourceFiles --include=*.cpp
```

A11/Activity History uchun menyu bandi shu yerda qo'shilgan
("📋 Faollik tarixi" — screenshot'da ko'ringan). Yangi bandni **aynan
shu naqsh bilan** qo'shing.

- [ ] **Step 2: `custom_archive.h` ga e'lon**

```cpp
// A13/K3: chatning butun tarixini serverdan sahifalab yuklab, doimiy
// arxivga yozadi. Fon rejimida ishlaydi. `done` yakunda chaqiriladi
// (arxivlangan xabarlar soni bilan).
void ArchiveFullHistory(
	not_null<History*> history,
	Fn<void(int archived)> done);
```

`custom_archive.h` boshiga qo'shing:

```cpp
#include "base/functional.h" // Fn<>
class History;
```

- [ ] **Step 3: `custom_archive.cpp` ga ta'rif**

```cpp
void ArchiveFullHistory(
		not_null<History*> history,
		Fn<void(int archived)> done) {
	const auto peerIdStr = QString::number(history->peer->id.value);
	if (!CustomSettings::ShouldBackgroundCache(peerIdStr)) {
		if (done) done(0);
		return;
	}
	// Sahifalab yuklash: har chaqiruvda eng eski yuklangan xabardan
	// oldingi qismni so'raymiz. Yuklangan har bir slice
	// addOlderSlice() orqali keladi va u yerdagi hook (Task 7)
	// avtomatik arxivlaydi — shuning uchun bu yerda faqat
	// yuklashni davom ettiramiz.
	const auto counter = std::make_shared<int>(0);
	const auto step = std::make_shared<Fn<void()>>();
	*step = [=] {
		if (history->loadedAtTop()) {
			if (done) done(*counter);
			return;
		}
		const auto offsetId = history->minMsgId();
		auto &histories = history->owner().histories();
		histories.sendRequest(
			history,
			Data::Histories::RequestType::History,
			[=](Fn<void()> finish) {
			return history->session().api().request(MTPmessages_GetHistory(
				history->peer->input(),
				MTP_int(offsetId),
				MTP_int(0),   // offsetDate
				MTP_int(0),   // addOffset
				MTP_int(50),  // loadCount
				MTP_int(0),   // maxId
				MTP_int(0),   // minId
				MTP_long(0)   // hash
			)).done([=](const MTPmessages_Messages &result) {
				const auto before = history->size();
				// Natijani qayta ishlash — HistoryWidget::messagesReceived
				// naqshi (Step 3a da o'qiladi).
				/* PROCESS_RESULT — Step 3a */
				*counter += (history->size() - before);
				finish();
				crl::on_main([=] { (*step)(); });
			}).fail([=](const MTP::Error &error) {
				finish();
				if (done) done(*counter);
			}).send();
		});
	};
	(*step)();
}
```

- [ ] **Step 3a: natijani qayta ishlash qismini to'ldirish**

Yuqoridagi `/* PROCESS_RESULT — Step 3a */` joyini to'ldirish uchun
**ishlaydigan namunani o'qing**:

`Telegram/SourceFiles/history/history_widget.cpp:4712` —
`messagesReceived(history->peer, result, _firstLoadRequest);`

va o'sha `HistoryWidget::messagesReceived()` funksiyasining tanasi:

```bash
grep -n "void HistoryWidget::messagesReceived" Telegram/SourceFiles/history/history_widget.cpp
```

U `MTPmessages_Messages` ni ochib, `owner().processMessages(...)`
chaqiradi va so'ng `history->addOlderSlice(...)` ga uzatadi. Shu
ketma-ketlikni takrorlang (widget'ga xos qismlarsiz — bizga faqat
`processMessages` + `addOlderSlice` kerak).

**Nega bu yetarli:** `addOlderSlice()` ichida Task 7 da qo'shilgan
arxiv hook'i bor — ya'ni yuklangan har bir slice **avtomatik**
arxivlanadi. Bu vazifada alohida arxivlash kodi yozish shart emas.

**Tasdiqlangan imzolar** (`history_widget.cpp:4700-4714` dan o'qilgan,
taxmin emas):
- `histories.sendRequest(history, type, [=](Fn<void()> finish) {...})`
  — `request()` EMAS, `sendRequest()`
- `history->peer->input()` — **funksiya chaqiruvi**, maydon emas
- `MTPmessages_GetHistory(peerInput, offsetId, offsetDate, addOffset,
  loadCount, maxId, minId, hash)` — 8 ta parametr, shu tartibda
- `.fail([=](const MTP::Error &error) {...})`

`history->size()`, `history->minMsgId()`, `history->loadedAtTop()`
mavjudligini `history.h` dan tasdiqlang. Agar `size()` yoki
`minMsgId()` topilmasa — `blocks` bo'ylab hisoblash yoki
`loadedAtTop()` bilan cheklanish kabi eng yaqin ekvivalentni
ishlating va tanlovingizni izohda yozing.

**Agar natijani qayta ishlash imzosini ishonchli aniqlay olmasangiz —
vazifani BLOCKED deb belgilang** va nima aniqlanmaganini yozing;
taxminiy kod yozib qo'ymang. K3 eng kam kritik komponent (K1+K2
asosiy muammoni allaqachon hal qiladi), shuning uchun uni
bloklangan holda qoldirish maqbul.

- [ ] **Step 4: menyu bandini qo'shish**

Step 1 da topilgan joyga, "Faollik tarixi" naqshi bilan:

```cpp
	AddAction(u"💾 Butun tarixni arxivla"_q, [=] {
		CustomArchive::ArchiveFullHistory(history, [=](int archived) {
			Ui::Toast::Show(u"Arxivlandi: "_q
				+ QString::number(archived) + u" xabar"_q);
		});
	});
```

`AddAction` va `Ui::Toast::Show` — shu fayldagi mavjud naqshga moslang.

- [ ] **Step 5: kod darajasida tekshirish**

1. Step 3a dagi `PROCESS_RESULT` joyi **haqiqiy** kod bilan
   to'ldirilganini (yoki vazifa BLOCKED belgilanganini) tasdiqlang —
   izoh qoldirilmasin.
2. `#include "data/data_histories.h"`, `#include "apiwrap.h"`,
   `#include "main/main_session.h"` kabi kerakli header'lar
   qo'shilganini tasdiqlang (`history_widget.cpp` nimalarni include
   qilganiga qarang).
3. Rekursiv `*step` lambda `std::make_shared` orqali ushlab
   turilganini tasdiqlang — aks holda u o'zini chaqirganda dangling
   bo'ladi.
4. `history` `not_null<History*>` — lambda ichida `[=]` bilan
   olinmoqda. Agar `History` ushbu asinxron zanjir davomida
   o'chirilishi mumkin bo'lsa, `base::weak_ptr` yoki
   `crl::guard` ishlatish kerak — A9 da `crl::guard(content, ...)`
   aynan shu muammo uchun ishlatilgan edi (use-after-free).
   **Bu tekshiruvni albatta bajaring.**

- [ ] **Step 6: Commit**

```bash
git add Telegram/SourceFiles/custom_archive.h Telegram/SourceFiles/custom_archive.cpp
git commit -m "A13/K3: butun tarixni qo'lda arxivlash"
```

---

### Task 11: K4 — Kuzatilayotgan chatlarda media avtomatik saqlash

**Files:**
- Modify: `Telegram/SourceFiles/custom_archive.cpp` — media yuklashni boshlash

**Mavjud infratuzilma:** `data_document.cpp:1046-1075` dagi
`finishLoad()` hook allaqachon **yuklab olingan** faylni
`CustomDB::SaveMediaFile()` orqali doimiy papkaga ko'chiradi. Bizga
faqat **yuklashni boshlash** qismi kerak.

A11 (story media backup) da xuddi shu vazifa hal qilingan — namuna:
`custom_activity_history.cpp` dagi `MaybeBackupStoryMedia()`.

- [ ] **Step 1: `MaybeArchiveItem` ichiga media yuklashni qo'shish**

`MaybeArchiveItem` oxiriga (yozuvdan keyin) qo'shing:

```cpp
	// A13/K4: kuzatilayotgan chatda media avtomatik yuklanadi, shunda
	// butun-chat o'chirilishida ham fayl qo'lda qoladi. Yuklab
	// olingach data_document.cpp:1046-1075 dagi mavjud finishLoad()
	// hook uni doimiy papkaga ko'chiradi.
	if (isMedia) {
		if (const auto media = item->media()) {
			if (const auto document = media->document()) {
				document->save(
					Data::FileOriginMessage(
						item->history()->peer->id,
						item->id),
					QString());
			} else if (const auto photo = media->photo()) {
				photo->load(
					Data::PhotoSize::Large,
					Data::FileOriginMessage(
						item->history()->peer->id,
						item->id));
			}
		}
	}
```

Kerakli include'lar (`custom_archive.cpp` boshiga):

```cpp
#include "data/data_document.h"
#include "data/data_photo.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
```

- [ ] **Step 2: kod darajasida tekshirish**

1. `Data::FileOriginMessage` mavjudligini va konstruktor
   parametrlarini `data/data_file_origin.h` dan tasdiqlang.
   `custom_activity_history.cpp` da `Data::FileOriginStory` shunday
   ishlatilgan — o'sha naqshga qarang.
2. `item->media()` → `Data::Media*`; `media->document()` va
   `media->photo()` mavjudligini `data/data_media_types.h` dan
   tasdiqlang.
3. `document->save(origin, QString())` imzosi
   `custom_activity_history.cpp:90` da aynan shunday ishlatilgan —
   mos kelishini tasdiqlang.
4. **Regressiya:** bu blok `MaybeArchiveItem` ichida, ya'ni gate
   ostida. Kuzatilmayotgan chatda hech qanday yuklash boshlanmaydi.

- [ ] **Step 3: Commit**

```bash
git add Telegram/SourceFiles/custom_archive.cpp
git commit -m "A13/K4: kuzatilayotgan chatlarda media avtomatik saqlash"
```

---

# YAKUN

### Task 12: Build va qo'lda tekshiruv

> ⚠️ **Build faqat foydalanuvchi ruxsati bilan.** Bu vazifa A6 (Qt6
> migratsiyasi) va A11 Task 6 bilan **bitta umumiy build**da bajariladi.

- [ ] **Step 1: build ruxsatini so'rash**

Foydalanuvchidan build boshlashga ruxsat so'rang. Ruxsatsiz
boshlamang.

- [ ] **Step 2: build**

Build buyrug'i va muhit sozlamalari `docs/superpowers/PROJECTS.md` A6
qatorida yozilgan (Qt6 uchun `win.bat qt6`, so'ng
`configure.bat x64 qt6 ...`). O'sha yo'riqnomaga ergashing.

- [ ] **Step 3: kompilyatsiya xatolarini tuzatish**

Har bir xatoni alohida ko'rib chiqing. Yangi kod bilan bog'liq
xatolarni tuzating. A6 (Qt6) bilan bog'liq xatolar alohida —
ularni A6 doirasida hal qiling.

- [ ] **Step 4: qo'lda tekshiruv (spec §6, 10 band)**

Spec'dagi ro'yxatni to'liq bajaring:
1. "Xurshida | V" chatida 104 ta saqlangan xabar "o'chirilgan"
   belgisi bilan ko'rinadimi?
2. Ilovani qayta ishga tushirgach ham ko'rinadimi?
3. Kuzatilayotgan chatda yuqoriga scroll → `text_cache` qatorlari
   ortadimi? (Tekshirish: DB'ni read-only ochib
   `SELECT COUNT(*) FROM text_cache WHERE is_archived=1`.)
4. Shu klientdan xabar yuborish → arxivga tushdimi?
5. "Butun tarixni arxivla" tugmasi ishlaydimi?
6. Media ochmasdan `~/customizationMainFolder/medias/` ga tushdimi?
7. Custom Window'da chat holati/sababi va statistika to'g'rimi?
   Global tugma bilan ham fon-cache ishlayaptimi?
8. 30 kundan eski arxiv yozuvlari saqlanib qoldimi?
9. **Regressiya:** GhostMode, AntiEdit, Activity History (A11),
   self-update, mutual-contact indikatori — avvalgidek ishlayaptimi?
10. **Performans:** uzun chatda tez scroll qilganda jank yo'qmi?

- [ ] **Step 5: hujjatlarni yangilash**

`docs/superpowers/PROJECTS.md` A13 qatorini yakuniy holatga
keltiring, bu reja faylidagi barcha katakchalarni belgilang.

- [ ] **Step 6: Commit**

```bash
git add docs/superpowers/PROJECTS.md docs/superpowers/plans/2026-08-13-antidelete-archive-hardening-plan.md
git commit -m "A13: build va qo'lda tekshiruv yakunlandi"
```
