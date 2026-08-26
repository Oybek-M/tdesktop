# Sxema v10 — akkaunt ajratmasi + media tuzatishlari

> **Agent uchun:** vazifalar ketma-ket bajariladi. Har vazifa oxirida
> commit. Build faqat 7-vazifada, oldin **foydalanuvchidan so'ralsin**.

**Maqsad:** 12 ta akkaunt bitta bazaga yozganda ma'lumot aralashmasin;
saqlangan media placeholder'da ko'rinsin.

**Tashxis:** [`../specs/2026-08-26-multi-account-db-isolation-design.md`](../specs/2026-08-26-multi-account-db-isolation-design.md)

**Asosiy qaror:** global "joriy akkaunt" o'zgaruvchisi **ISHLATILMAYDI**
— tdesktop barcha akkauntlarni bir vaqtda ishlatadi va fon akkauntlari
ham yozadi. Akkaunt ID'si chaqiruv joyidan **kompilyator majburlagan
holda** uzatiladi.

---

## Vazifa 1: `PeerKey` turi va `Key()` yordamchilari

**Fayllar:** `Telegram/SourceFiles/custom_db.h`, `custom_db.cpp`

- [ ] **1.1** `custom_db.h` ga qo'shing (namespace `CustomDB` ichiga,
      mavjud struct'lardan oldin):

```cpp
// Akkaunt + peer juftligi. Nima uchun alohida tur: 12 ta akkaunt
// bitta bazaga yozadi, fon akkauntlari ham. Bitta QString peerId
// yetarli emas edi — 2026-08-26 da aralashuv xatosi shundan chiqqan.
// Standart qiymat ATAYLAB yo'q: kompilyator har chaqiruv joyini
// yangilashga majbur qilsin, aks holda unutilgan joy jimgina 0 yozadi.
struct PeerKey {
    qint64 accountId = 0;
    QString peerId;

    [[nodiscard]] bool operator<(const PeerKey &other) const {
        return (accountId != other.accountId)
            ? (accountId < other.accountId)
            : (peerId < other.peerId);
    }
    [[nodiscard]] bool operator==(const PeerKey &other) const {
        return accountId == other.accountId && peerId == other.peerId;
    }
};
```

- [ ] **1.2** Yordamchilarni **alohida** faylga yozing —
      `custom_db.h` MTProto/Data turlarini include qilmaydi va
      qilmasligi ham kerak.

Yangi fayl `Telegram/SourceFiles/custom_peer_key.h`:

```cpp
#pragma once

#include "custom_db.h"

class HistoryItem;
class PeerData;
namespace Main { class Session; }

namespace CustomDB {

[[nodiscard]] PeerKey Key(not_null<PeerData*> peer);
[[nodiscard]] PeerKey Key(not_null<HistoryItem*> item);
[[nodiscard]] PeerKey Key(Main::Session &session, PeerId peerId);

} // namespace CustomDB
```

Yangi fayl `Telegram/SourceFiles/custom_peer_key.cpp`:

```cpp
#include "custom_peer_key.h"

#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"

namespace CustomDB {

PeerKey Key(not_null<PeerData*> peer) {
    return Key(peer->session(), peer->id);
}

PeerKey Key(not_null<HistoryItem*> item) {
    return Key(item->history()->session(), item->history()->peer->id);
}

PeerKey Key(Main::Session &session, PeerId peerId) {
    return PeerKey{
        .accountId = qint64(session.userId().bare),
        .peerId = QString::number(peerId.value),
    };
}

} // namespace CustomDB
```

- [ ] **1.3** `Telegram/CMakeLists.txt` da `custom_db.cpp` yonига
      `custom_peer_key.cpp` va `custom_peer_key.h` qo'shing.

- [ ] **1.4** Commit: `feat: PeerKey turi - akkaunt + peer juftligi`

---

## Vazifa 2: Sxema v10 migratsiyasi

**Fayl:** `custom_db.cpp` (`RunMigrations()`), `custom_db.h`

- [ ] **2.1** `custom_db.h`: `kCurrentSchemaVersion` ni **10** ga.

- [ ] **2.2** `RunMigrations()` oxiriga (v9 blokidan keyin):

```cpp
    // v9 -> v10: akkaunt ajratmasi.
    //
    // Nima uchun: tdesktop 12 akkauntni bir vaqtda ishlatadi va
    // FON akkauntlari ham bazaga yozadi. Kalit (peer_id, msg_id)
    // edi, ya'ni ikki akkauntning bir odam bilan yozishmasi bitta
    // chelakka tushardi. 2026-08-26 da Akam chatidagi 218 yozuvdan
    // 201 tasi boshqa akkauntniki bo'lib chiqdi.
    //
    // account_id = 0 -> eski, egasi NOMA'LUM yozuv. Ular
    // O'CHIRILMAYDI (qaysi akkauntniki ekanini retroaktiv aniqlab
    // bo'lmaydi), o'qishda esa ID-diapazon tekshiruvidan o'tadi.
    if (version < 10) {
        execSql("ALTER TABLE actioned_messages ADD COLUMN account_id INTEGER NOT NULL DEFAULT 0");
        execSql("ALTER TABLE text_cache        ADD COLUMN account_id INTEGER NOT NULL DEFAULT 0");
        execSql("ALTER TABLE media_index       ADD COLUMN account_id INTEGER NOT NULL DEFAULT 0");
        execSql("ALTER TABLE ghost_reads       ADD COLUMN account_id INTEGER NOT NULL DEFAULT 0");
        // activity_history: ustun PROVENANCE uchun qo'shiladi, lekin
        // o'qishda FILTRLANMAYDI - spec 0.13 ga qarang. Faollik
        // kuzatilayotgan odam haqidagi fakt; akkauntlarga bo'lish
        // last-seen bypass qamrovini buzadi.
        execSql("ALTER TABLE activity_history  ADD COLUMN account_id INTEGER NOT NULL DEFAULT 0");

        execSql("CREATE INDEX IF NOT EXISTS idx_am_acc_peer_msg "
                "ON actioned_messages(account_id, peer_id, msg_id)");
        execSql("CREATE INDEX IF NOT EXISTS idx_tc_acc_peer_msg "
                "ON text_cache(account_id, peer_id, msg_id)");
        execSql("CREATE INDEX IF NOT EXISTS idx_mi_acc_peer_msg "
                "ON media_index(account_id, peer_id, msg_id)");
    }
```

- [ ] **2.3** Commit: `feat: sxema v10 - account_id ustuni`

---

## Vazifa 3: Yozish yo'llari — `PeerKey` ni uzatish

`const QString &peerId` parametrini `const PeerKey &key` ga
almashtiring. **Standart qiymat qo'ymang** — kompilyator xatosi
yo'l ko'rsatuvchi ro'yxat vazifasini bajaradi.

- [ ] **3.1** Quyidagi funksiyalar imzosini o'zgartiring
      (`custom_db.h` + `custom_db.cpp`):

```
MarkDeleted, SaveActionedMessage (ActionedMessage ga accountId maydoni),
CacheMessageText, SaveGhostRead, ResetGhostRead, GetGhostRead,
ScheduleUserDelete, IsUserDeletePending, ClearUserDeletePending,
PermanentlyDeleteMessage, IsDeletedLocally, GetDeletedMessages,
GetOriginalTextBeforeEdit, GetEditHistory, GetCachedText,
GetCachedTextAndDate, GetSavedMediaPath, GetArchivedMediaPath,
RecordBackgroundEdit, UpsertMediaIndex, SetMediaIndexStatus,
SaveActivityHistoryEntry, GetLatestActivityHistoryValue,
GetActivityHistory
```

Har `INSERT`/`UPDATE` ga `account_id` bind qiling; har `SELECT`/
`DELETE` ga `AND account_id IN (0, :acc)` qo'shing.

🔴 **ISTISNO — `activity_history`:** `SaveActivityHistoryEntry` ga
`account_id` **yoziladi**, lekin `GetActivityHistory` va
`GetLatestActivityHistoryValue` da `account_id` filtri
**QO'SHILMAYDI**. Sabab: spec §0.13.

- [ ] **3.2** Kesh tuzilmalarini yangilang: `gDeletedCache`,
      `gPeersWithDeleted` endi `QString` emas, `PeerKey` bo'yicha
      kalitlanadi (`std::map<PeerKey, ...>` — `operator<` 1.1 da bor).

- [ ] **3.3** Kompilyator ko'rsatgan har bir chaqiruv joyini
      to'g'irlang. Manba jadvali:

| Fayl | Akkaunt manbai |
|---|---|
| `data/data_session.cpp` | `Key(*_session, peerId)` |
| `history/history_item.cpp` | `Key(this)` |
| `history/history.cpp` | `Key(session(), peer->id)` |
| `history/view/history_view_element.cpp` | `Key(item)` |
| `data/data_histories.cpp` | `Key(item)` yoki `Key(_owner->session(), peerId)` |
| `data/data_document.cpp` | `Key(item)` |
| `apiwrap.cpp` | `Key(item)` |
| `custom_archive.cpp` | `Key(item)` |
| `custom_activity_history.cpp` | `Key(peer)` |
| `main/main_session.cpp` | `Key(*this, ...)` |

⚠️ `custom_mod_window.cpp` (22 chaqiruv) — asosan statistika va
tozalash. U yerda **joriy faol sessiya** ishlatiladi; agar sessiya
mavjud bo'lmasa (oyna sessiyasiz ochilgan), akkauntsiz variant
qoladi. Har bir joyni alohida ko'ring, ko'r-ko'rona almashtirmang.

- [ ] **3.4** Commit: `feat: CustomDB yozish/oqish yollari akkauntga boglandi`

---

## Vazifa 4: Eski yozuvlar uchun ID-diapazon tekshiruvi

**Fayl:** `history/history.cpp` (`loadDeletedMessages()`)

`account_id = 0` yozuvlar hali aralashgan. Ularni qo'yishdan oldin
tekshiring.

- [ ] **4.1** `loadDeletedMessages()` da, sikldan **oldin**, chatdagi
      haqiqiy server xabarlaridan eng kichik ID va uning sanasini
      toping:

```cpp
    // 2026-08-26: eski (account_id=0) yozuvlar qaysi akkauntniki
    // ekani noma'lum. Shaxsiy chatda xabar ID'lari akkaunt bo'yicha
    // MONOTON o'sadi, shuning uchun "ID kichikroq, lekin sanasi
    // kechroq" holati mumkin emas - bunday yozuv boshqa akkauntning
    // ID-fazosidan. Ma'lumot o'chirilmaydi, faqat ko'rsatilmaydi.
    auto minRealId = MsgId(0);
    auto minRealDate = TimeId(0);
    for (const auto &block : blocks) {
        for (const auto &message : block->messages) {
            const auto real = message->data();
            if (!real->isRegular()) continue;
            if (!minRealId || real->id < minRealId) {
                minRealId = real->id;
                minRealDate = real->date();
            }
        }
    }
```

- [ ] **4.2** Siklda, `owner().message(...)` tekshiruvidan keyin:

```cpp
        if (msg.accountId == 0
            && minRealId
            && MsgId(msg.msgId) < minRealId
            && TimeId(effectiveDate) > minRealDate) {
            continue; // boshqa akkauntning ID-fazosi
        }
```

⚠️ `effectiveDate` allaqachon yuqorida hisoblanadi — bu tekshiruvni
**undan keyin** qo'ying.

- [ ] **4.3** `DeletedMessage` struct'iga `qint64 accountId = 0;`
      qo'shing va `GetDeletedMessages()` da to'ldiring.

- [ ] **4.4** Commit: `fix: eski yozuvlar uchun ID-diapazon tekshiruvi`

---

## Vazifa 5: Media №1 — saqlangan faylni placeholder'ga biriktirish

**Fayl:** `history/history.cpp` (`loadDeletedMessages()`)

Hozir doim `MTP_messageMediaEmpty()` uzatiladi, ya'ni diskda turgan
fayl ham ko'rinmaydi.

- [ ] **5.1** `makeMessage()` chaqiruvidan oldin faylni toping:

```cpp
    // 2026-08-26: ilgari media DOIM MTP_messageMediaEmpty() edi -
    // diskda fayl turgan bo'lsa ham "(media xabar)" chiqardi.
    QString localPath = msg.mediaPath;
    if (localPath.isEmpty() || !QFile::exists(localPath)) {
        localPath = CustomDB::GetArchivedMediaPath(key, msg.msgId);
    }
```

- [ ] **5.2** `localPath` bo'sh bo'lmasa, lokal hujjat yarating va
      uni item'ga biriktiring. tdesktop'da tayyor yo'l:
      `owner().documentFromWeb` EMAS, balki
      `Data::Session::processDocument` ham EMAS — lokal fayl uchun
      `owner().document(...)` bilan yangi `DocumentData` yaratib,
      `setLocation(Core::FileLocation(localPath))` chaqiring, so'ng
      `item->setMedia(...)`.

      🔴 **Aniq API nomlarini kod ichidan tekshiring** — bu joy
      versiyaga bog'liq. `Storage::LocalImageLocation` va
      `DocumentData::setLocation` ni grep qiling.

- [ ] **5.3** Matn: fayl topilsa `"(media xabar)"` o'rniga fayl nomi
      ko'rsatilsin; topilmasa eski matn qoladi.

- [ ] **5.4** Commit: `fix: saqlangan media placeholder da korsatiladi`

---

## Vazifa 6: Media №2 va №3

- [ ] **6.1** **Nisbiy yo'l.** `MarkDeleted()` ga kelgan `mediaPath`
      `CustomSettings::ArchiveRoot()` bilan boshlansa, uni nisbiy
      qilib saqlang. O'qishda ildiz bilan birlashtiring.
      Migratsiya v10 blokiga qo'shing:

```cpp
        // Eski absolyut yo'llar: arxiv ildizi 2026-08-15 da ko'chgan,
        // baza yangilanmagan - 17 yozuvdan 6 tasi buzilgan edi.
        execSql("UPDATE actioned_messages "
                "SET media_path = replace(media_path, "
                "  'C:/Users/Oybek/customizationMainFolder/', '') "
                "WHERE media_path LIKE 'C:/Users/Oybek/customizationMainFolder/%'");
```

⚠️ Ildiz yo'li **qattiq kodlanmasin** — `ArchiveRoot()` dan oling va
so'rovni parametr bilan quring. Yuqoridagi satr faqat naqsh.

- [ ] **6.2** **Qamrov.** `data/data_session.cpp`,
      `processMessagesDeleted()`: hozir `MarkDeleted(...)` ga doim
      bo'sh `QString()` uzatiladi. Uning o'rniga:

```cpp
            auto mediaPath = QString();
            if (isMedia) {
                mediaPath = CustomDB::GetArchivedMediaPath(key, messageId.v);
            }
```

va uni `MarkDeleted()` ga bering.

- [ ] **6.3** Commit: `fix: media_path nisbiy + xotirada yoq xabarlar uchun qamrov`

---

## Vazifa 7: Build va tekshirish

- [ ] **7.1** 🔴 **Foydalanuvchidan build uchun ruxsat so'rang.**
      ~34 daqiqa oladi.

- [ ] **7.2** Build. Xato bo'lsa tuzating va qayta urinmang —
      har xatoni alohida hal qiling.

- [ ] **7.3** Foydalanuvchi `exe` ni ko'chirgach, bazani tekshiring
      (tashxis hujjati §7 dagi 3 ta so'rov):

```sql
SELECT COUNT(*) FROM actioned_messages
WHERE account_id = 0 AND timestamp > date('now');            -- kutilgan 0

SELECT COUNT(*) FROM actioned_messages
WHERE media_path LIKE 'C:/%' OR media_path LIKE '/%';        -- kutilgan 0

SELECT COUNT(*) FROM media_index WHERE account_id = 0
  AND archived_at > date('now');                              -- kutilgan 0
```

- [ ] **7.4** Qo'lda: Akam chatini oching — yangi arvoh **paydo
      bo'lmasin**. Ikkinchi akkauntga o'ting — u yerda ham o'zining
      yozuvlarigina ko'rinsin.

- [ ] **7.5** Qo'lda: Faollik tarixi **ikkala akkauntda ham bir xil
      to'liq** ro'yxatni ko'rsatsin (birlashish ishlayotganining
      isboti).

- [ ] **7.6** `docs/sync-protocol/STATUS.md` da sxema qatorini v10 ga
      yangilang. Commit + push `origin/Oybek`.

---

## Eslatmalar

- `Co-Authored-By` **ishlatilmaydi**
- Push faqat `origin/Oybek`, `upstream` ga **hech qachon**
- `git add -A` merge'dan keyin **ishlatilmaydi** (submodullarni
  jimgina muzlatib qo'yadi)
- `docs/` ni commit qilishdan oldin `git status` ni tekshiring —
  `customsync-server` sessiyasi ham shu papkaga yozadi
