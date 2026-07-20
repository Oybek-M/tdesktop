# Activity History Log Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist every legitimately-received change to a contact's name, username, profile-photo, and last-seen status into a local SQLite log, and expose that log (current state, last-known-before-hidden value, reconstructed online periods, full change journal) from two UI entry points — Custom Window → Peers tab, and the contact's profile page.

**Architecture:** A new capture module (`custom_activity_history.h/.cpp`) subscribes once per session to the existing `session().changes().peerUpdates(...)` stream (the same mechanism `Api::Updates` already uses), diffs incoming values against the last row stored per peer/field in a new `activity_history` SQLite table (via `custom_db.cpp`, same raw-sqlite3 pattern as `ghost_reads`), and writes a row only on real change. Coverage is gated by a new `CustomSettings::ShouldTrackActivity()` helper following the codebase's existing Blocklist > Whitelist > default priority pattern, backed by two new independent peer lists (Include/Exclude) that mirror the existing Whitelist/Blocklist implementation exactly but are stored separately. Two UI entry points (Peers tab section, profile page button) both open one shared "History Viewer" `Ui::BoxContent`.

**Tech Stack:** C++20 / Qt6, existing `sqlite3` C API via `custom_db.cpp`, `rpl` reactive streams (`Data::Changes::peerUpdates`), existing tdesktop `Ui::` widget toolkit (`Ui::VerticalLayout`, `Ui::SettingsButton`, `Ui::FlatLabel`, `Ui::RoundButton`, `Ui::SlideWrap`, `Ui::GenericBox`).

**Scope note (deviation from the literal spec wording, flag to user before/while presenting this plan):** The spec (`docs/superpowers/specs/2026-07-20-activity-history-log-design.md`, §2.1) says the `photo` field's `new_value` should be "a local file path saved via `SaveMediaFile()`". That is not achievable for v1: `CustomDB::SaveMediaFile()` only copies an *already-local* file — profile photos arrive as remote MTP data, and decoding/downloading them is not currently wired up anywhere in this codebase for `UserProfilePhoto` (only for `Photo`/`Document` media). Task 3 below instead records photo **change events** using `PeerData::userpicPhotoId()` (a stable ID already available synchronously, no download needed) as `new_value`, and `"empty"` when the peer has no photo (`!hasUserpic()`). No thumbnail image is persisted in v1. This is a scope reduction, not a redesign — the table/column shape from the spec is unchanged, only what gets written into `new_value` for `field="photo"` differs from the spec's literal wording.

---

## Implementation Status (2026-07-20)

All 6 code tasks implemented via subagent-driven-development (fresh implementer subagent per task + spec-compliance review + code-quality review, with follow-up fix rounds where reviewers found issues), plus a final cross-cutting review across the whole feature. All approved. **No build has been attempted** — this environment has known disk/RAM constraints that have blocked every build attempt in this project so far (see [[project_custom_window_redesign]] memory). Task 7 (manual regression) remains blocked until a build succeeds.

| Task | Commit(s) | Status |
|---|---|---|
| 1. DB layer | `90d010a93b` | ✅ Implemented, spec-reviewed, code-reviewed |
| 2. Settings layer | `7755eb58b5` | ✅ Implemented, spec-reviewed, code-reviewed |
| 3. Capture module + session hook | `445013b6b9`, fix `f5087bd13a` (DecodeStatusLabel timestamp validation) | ✅ Implemented, spec-reviewed, code-reviewed (2 rounds) |
| 4. Peers tab UI section | `7915a04a12` | ✅ Implemented, spec-reviewed, code-reviewed |
| 6. Shared History Viewer Box | `2567e2b501`, fix `ac463a8f0b` (clarifying comments) | ✅ Implemented, spec-reviewed, code-reviewed (2 rounds) |
| 5. Profile page history button | `a7f751d847` | ✅ Implemented, spec-reviewed, code-reviewed |
| 7. Manual regression pass | — | ⏳ Blocked on a successful build |

Final whole-feature review (integration correctness, cross-file consistency, privacy/scope correctness, build registration completeness): **Approved**.

---

### Task 1: Database layer — `activity_history` table + CRUD

**Files:**
- Modify: `Telegram/SourceFiles/custom_db.h`
- Modify: `Telegram/SourceFiles/custom_db.cpp`

- [ ] **Step 1: Add the struct and function declarations to `custom_db.h`**

Insert this block right before the closing `} // namespace CustomDB` (after the existing `ArchiveStats`/`ClearAllArchive` block, i.e. after line 250 `void ClearAllArchive();`):

```cpp
// ── Activity History Log ──────────────────────────────────────────────────
// Kontaktlarning ism/username/rasm/last-seen o'zgarishlari — faqat ilova
// legal ravishda (joriy maxfiylik sozlamalari asosida) qabul qilgan
// ma'lumot. Yozuv faqat CustomSettings::ShouldTrackActivity() true
// bo'lganda amalga oshiriladi (chaqiruvchi tomonidan tekshiriladi).

struct ActivityHistoryEntry {
    QString field;         // "name" | "username" | "photo" | "status"
    bool hasOldValue = false; // false = bu shu peer/field uchun BIRINCHI yozuv
    QString oldValue;      // hasOldValue=false bo'lsa mazmunsiz (bo'sh)
    QString newValue;
    qint64 observedAt = 0; // unix timestamp (base::unixtime::now())
};

// Yangi yozuv qo'shadi. hasOldValue=false — bu peer/field juftligi uchun
// birinchi marta kuzatilayotganini bildiradi (old_value ustuniga SQL NULL
// yoziladi, "o'zgarish" emas "kuzatish boshlang'ich holati" sifatida).
void SaveActivityHistoryEntry(
    const QString &peerId,
    const QString &field,
    bool hasOldValue,
    const QString &oldValue,
    const QString &newValue,
    qint64 observedAt);

// Shu peer/field uchun eng oxirgi yozilgan qiymatni qaytaradi.
// Qaytish qiymati: true — topildi (outValue to'ldirildi), false — hali
// hech qanday yozuv yo'q (birinchi kuzatish bo'ladi).
bool GetLatestActivityHistoryValue(
    const QString &peerId,
    const QString &field,
    QString &outValue);

// Shu peer uchun BARCHA maydonlar bo'yicha to'liq jurnal, eng yangisidan
// boshlab (observed_at DESC). History Viewer Box shu funksiyani ishlatadi.
[[nodiscard]] QVector<ActivityHistoryEntry> GetActivityHistory(
    const QString &peerId);
```

- [ ] **Step 2: Create the table in `custom_db.cpp`'s `Init()`**

In `custom_db.cpp`, inside `Init()`, right after the existing `text_cache` index (after the line `execSql("CREATE INDEX IF NOT EXISTS idx_tc_cached_at " "ON text_cache(cached_at)");` and before `RunMigrations();`), add:

```cpp
    // Activity History Log: kontaktlarning ism/username/rasm/last-seen
    // o'zgarishlari — faqat ilova legal ravishda qabul qilgan ma'lumot
    // (see custom_activity_history.cpp for the capture logic).
    execSql("CREATE TABLE IF NOT EXISTS activity_history ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "peer_id TEXT NOT NULL, "
            "field TEXT NOT NULL, "
            "old_value TEXT, "
            "new_value TEXT, "
            "observed_at INTEGER NOT NULL)");
    execSql("CREATE INDEX IF NOT EXISTS idx_activity_history_peer "
            "ON activity_history(peer_id, observed_at DESC)");
```

No `kCurrentSchemaVersion` bump is needed — like `ghost_reads` and `text_cache`, this is a brand-new table created unconditionally via `CREATE TABLE IF NOT EXISTS`; the versioned `RunMigrations()` block is only used for `ALTER TABLE` column additions on tables that already existed in older DBs.

- [ ] **Step 3: Implement the three functions in `custom_db.cpp`**

Add this block right after `ArchiveStats GetArchiveStats() { ... }`/`ClearAllArchive()` definitions, at the end of the file before the closing `} // namespace CustomDB` (find it by searching for `void ClearAllArchive()` in the .cpp and inserting after its closing brace):

```cpp
// ---------------------------------------------------------------------------
// Activity History Log
// ---------------------------------------------------------------------------

void SaveActivityHistoryEntry(
        const QString &peerId,
        const QString &field,
        bool hasOldValue,
        const QString &oldValue,
        const QString &newValue,
        qint64 observedAt) {
    Init();
    if (!gDb) return;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "INSERT INTO activity_history "
            "(peer_id, field, old_value, new_value, observed_at) "
            "VALUES (?, ?, ?, ?, ?)",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        bindText(stmt, 2, field);
        if (hasOldValue) {
            bindText(stmt, 3, oldValue);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
        bindText(stmt, 4, newValue);
        sqlite3_bind_int64(stmt, 5, observedAt);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

bool GetLatestActivityHistoryValue(
        const QString &peerId,
        const QString &field,
        QString &outValue) {
    Init();
    if (!gDb) return false;

    bool found = false;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT new_value FROM activity_history "
            "WHERE peer_id = ? AND field = ? "
            "ORDER BY observed_at DESC, id DESC LIMIT 1",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        bindText(stmt, 2, field);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            outValue = colText(stmt, 0);
            found = true;
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

QVector<ActivityHistoryEntry> GetActivityHistory(const QString &peerId) {
    Init();
    QVector<ActivityHistoryEntry> result;
    if (!gDb) return result;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "SELECT field, old_value, new_value, observed_at "
            "FROM activity_history WHERE peer_id = ? "
            "ORDER BY observed_at DESC, id DESC",
            -1, &stmt, nullptr) == SQLITE_OK) {
        bindText(stmt, 1, peerId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ActivityHistoryEntry entry;
            entry.field = colText(stmt, 0);
            entry.hasOldValue = (sqlite3_column_type(stmt, 1) != SQLITE_NULL);
            entry.oldValue = entry.hasOldValue ? colText(stmt, 1) : QString();
            entry.newValue = colText(stmt, 2);
            entry.observedAt = sqlite3_column_int64(stmt, 3);
            result.append(entry);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}
```

- [ ] **Step 4: Build the project (or at minimum compile-check `custom_db.cpp`) and confirm no syntax errors before moving to Task 2.** This codebase has no automated test suite for `custom_*` files — verification here is compile success, consistent with how `ghost_reads`/`text_cache` were added.

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/custom_db.h Telegram/SourceFiles/custom_db.cpp
git commit -m "Activity History Log: add activity_history table + CRUD"
```

---

### Task 2: Settings layer — Include/Exclude lists + global toggle + `ShouldTrackActivity`

**Files:**
- Modify: `Telegram/SourceFiles/custom_settings.h`
- Modify: `Telegram/SourceFiles/custom_settings.cpp`

- [ ] **Step 1: Add the new field to `Values` in `custom_settings.h`**

Right after `bool mutualContactShowInProfile = true;` / `QString mutualContactProfileEmoji = u"🤝"_q;` (end of the mutual-contact block, before the closing `};` of `struct Values`), add:

```cpp
    // Activity History Log: ism/username/rasm/last-seen o'zgarishlarini
    // kuzatish. Include/Exclude ro'yxatlari alohida QHash'larda saqlanadi
    // (pastga qarang) — bu shunchaki global default toggle.
    bool activityHistoryTrackAllContacts = true;
```

- [ ] **Step 2: Add inline getter + function declarations to `custom_settings.h`**

Right after `inline QString MutualContactProfileEmoji() { return Get().mutualContactProfileEmoji; }`, add:

```cpp
inline bool ActivityHistoryTrackAllContacts() { return Get().activityHistoryTrackAllContacts; }
```

Right after the existing `[[nodiscard]] QString GetPeerDisplayName(const QString &peerId);` line (just before the closing `} // namespace CustomSettings`), add:

```cpp
// ── Activity History Log: Include/Exclude ro'yxatlari ───────────────────
// Whitelist/Blocklist bilan bir xil funksiya to'plami va mutual-exclusion
// xatti-harakati, lekin BUTUNLAY MUSTAQIL QHash'larda saqlanadi — bu
// ro'yxatlar Ghost/AntiDelete/AntiEdit uchun ishlatiladigan Whitelist/
// Blocklist bilan aralashtirilmaydi (butunlay boshqa maqsad).

void AddToActivityInclude(const QString &peerId, const QString &displayName);
void RemoveFromActivityInclude(const QString &peerId);
[[nodiscard]] QVector<QPair<QString,QString>> GetActivityInclude();
[[nodiscard]] bool IsInActivityInclude(const QString &peerId);

void AddToActivityExclude(const QString &peerId, const QString &displayName);
void RemoveFromActivityExclude(const QString &peerId);
[[nodiscard]] QVector<QPair<QString,QString>> GetActivityExclude();
[[nodiscard]] bool IsInActivityExclude(const QString &peerId);

// Ustuvorlik: Exclude List (false) > Include List (true, standart holatdan
// qat'iy nazar) > (activityHistoryTrackAllContacts && isContact) > false.
// isContact — chaqiruvchi tomonidan beriladi (bu fayl Data::PeerData ga
// bog'liq emas, boshqa CustomSettings funksiyalari kabi faqat QString bilan
// ishlaydi).
[[nodiscard]] bool ShouldTrackActivity(const QString &peerId, bool isContact);
```

- [ ] **Step 3: Add storage + JSON persistence in `custom_settings.cpp`**

Right after `QHash<int, bool> gWhitelistCategories;` / `QHash<int, bool> gBlocklistCategories;` (the T42 category block), add:

```cpp
// Activity History Log: Include/Exclude — Whitelist/Blocklist'dan mustaqil.
QHash<QString, QString> gActivityInclude;
QHash<QString, QString> gActivityExclude;
```

In `SavePeerLists()`, right before `QFile f(PeerListsFilePath());`, add the serialization for the two new lists:

```cpp
    QJsonArray aInc, aExc;
    for (auto it = gActivityInclude.constBegin(); it != gActivityInclude.constEnd(); ++it) {
        QJsonObject o;
        o[QStringLiteral("id")]   = it.key();
        o[QStringLiteral("name")] = it.value();
        aInc.append(o);
    }
    for (auto it = gActivityExclude.constBegin(); it != gActivityExclude.constEnd(); ++it) {
        QJsonObject o;
        o[QStringLiteral("id")]   = it.key();
        o[QStringLiteral("name")] = it.value();
        aExc.append(o);
    }
    root[QStringLiteral("activity_include")] = aInc;
    root[QStringLiteral("activity_exclude")] = aExc;
```

(`root[...]` assignments must come after `QJsonObject root;` is declared but before `f.open(...)` — insert this block right after the existing `root[QStringLiteral("bl_categories")] = blCats;` line, keeping `QFile f(PeerListsFilePath());` as the following statement.)

In `LoadPeerLists()`, right after the existing `gBlocklistCategories` loading loop (its closing `}`), add:

```cpp
    gActivityInclude.clear();
    for (const auto &v : root[QStringLiteral("activity_include")].toArray()) {
        const auto o  = v.toObject();
        const auto id = o[QStringLiteral("id")].toString();
        if (!id.isEmpty()) gActivityInclude[id] = o[QStringLiteral("name")].toString();
    }

    gActivityExclude.clear();
    for (const auto &v : root[QStringLiteral("activity_exclude")].toArray()) {
        const auto o  = v.toObject();
        const auto id = o[QStringLiteral("id")].toString();
        if (!id.isEmpty()) gActivityExclude[id] = o[QStringLiteral("name")].toString();
    }
```

- [ ] **Step 4: Wire the global toggle's persistence**

In `UpdateValue()`, add a new branch after the mutual-contact ones:

```cpp
    else if (id == "activityHistoryTrackAllContacts") gValues.activityHistoryTrackAllContacts = value;
```

In `Init()`, add after the `mutualContactProfileEmoji` load:

```cpp
    gValues.activityHistoryTrackAllContacts = settings.value(
        "activityHistoryTrackAllContacts", true).toBool();
```

- [ ] **Step 5: Implement the Include/Exclude CRUD + `ShouldTrackActivity`**

Add this block right after the existing `bool AntiEditForPeer(const QString &peerId) { ... }` definition (end of file), before the closing `} // namespace CustomSettings`:

```cpp
// ── Activity History Log: Include List ───────────────────────────────────

void AddToActivityInclude(const QString &peerId, const QString &displayName) {
    if (!gInitialized) Init();
    gActivityInclude[peerId] = displayName;
    gActivityExclude.remove(peerId); // mutual exclusion
    SavePeerLists();
}

void RemoveFromActivityInclude(const QString &peerId) {
    if (!gInitialized) Init();
    gActivityInclude.remove(peerId);
    SavePeerLists();
}

QVector<QPair<QString, QString>> GetActivityInclude() {
    if (!gInitialized) Init();
    QVector<QPair<QString, QString>> result;
    result.reserve(gActivityInclude.size());
    for (auto it = gActivityInclude.constBegin(); it != gActivityInclude.constEnd(); ++it) {
        result.append({ it.key(), it.value() });
    }
    return result;
}

bool IsInActivityInclude(const QString &peerId) {
    if (!gInitialized) Init();
    return gActivityInclude.contains(peerId);
}

// ── Activity History Log: Exclude List ───────────────────────────────────

void AddToActivityExclude(const QString &peerId, const QString &displayName) {
    if (!gInitialized) Init();
    gActivityExclude[peerId] = displayName;
    gActivityInclude.remove(peerId); // mutual exclusion
    SavePeerLists();
}

void RemoveFromActivityExclude(const QString &peerId) {
    if (!gInitialized) Init();
    gActivityExclude.remove(peerId);
    SavePeerLists();
}

QVector<QPair<QString, QString>> GetActivityExclude() {
    if (!gInitialized) Init();
    QVector<QPair<QString, QString>> result;
    result.reserve(gActivityExclude.size());
    for (auto it = gActivityExclude.constBegin(); it != gActivityExclude.constEnd(); ++it) {
        result.append({ it.key(), it.value() });
    }
    return result;
}

bool IsInActivityExclude(const QString &peerId) {
    if (!gInitialized) Init();
    return gActivityExclude.contains(peerId);
}

bool ShouldTrackActivity(const QString &peerId, bool isContact) {
    if (!gInitialized) Init();
    if (peerId.isEmpty()) return false;
    if (IsInActivityExclude(peerId)) return false;
    if (IsInActivityInclude(peerId)) return true;
    return gValues.activityHistoryTrackAllContacts && isContact;
}
```

- [ ] **Step 6: Build/compile-check.**

- [ ] **Step 7: Commit**

```bash
git add Telegram/SourceFiles/custom_settings.h Telegram/SourceFiles/custom_settings.cpp
git commit -m "Activity History Log: Include/Exclude lists + ShouldTrackActivity"
```

---

### Task 3: Capture module — subscribe to `peerUpdates`, diff, write

**Files:**
- Create: `Telegram/SourceFiles/custom_activity_history.h`
- Create: `Telegram/SourceFiles/custom_activity_history.cpp`
- Modify: `Telegram/SourceFiles/main/main_session.cpp`
- Modify: (build file, if this repo lists sources explicitly — see Step 4)

**Context:** This module owns the *capture* side only (subscription + diff + write) and the status-encoding helpers the History Viewer Box (Task 6) will reuse to decode what got written. It does NOT contain any UI code.

- [ ] **Step 1: Create `custom_activity_history.h`**

```cpp
#pragma once

#include <QtCore/QString>

namespace Main {
class Session;
} // namespace Main

namespace Data {
class LastseenStatus;
} // namespace Data

namespace CustomActivityHistory {

// Session yaratilganda BIR MARTA chaqiriladi (main_session.cpp konstruktori
// ichida). Ism/username/rasm/last-seen o'zgarishlarini
// session().changes().peerUpdates(...) orqali kuzatadi va
// CustomSettings::ShouldTrackActivity() true bo'lgan User peerlar uchun
// CustomDB::activity_history jadvaliga yozadi. Obuna session bilan bir xil
// umr ko'radi (session->lifetime() ga bog'langan).
void Init(not_null<Main::Session*> session);

// Xom Data::LastseenStatus qiymatini activity_history uchun ixcham matn
// kodiga aylantiradi: "online:<unixts>" / "offline:<unixts>" /
// "recently" / "within_week" / "within_month" / "long_ago" / "empty".
[[nodiscard]] QString EncodeStatus(const Data::LastseenStatus &status, int32 now);

// EncodeStatus() natijasini inson o'qiy oladigan matnga aylantiradi.
// History Viewer Box (custom_activity_history_box.cpp) shu funksiyani
// ishlatadi.
[[nodiscard]] QString DecodeStatusLabel(const QString &encoded);

} // namespace CustomActivityHistory
```

- [ ] **Step 2: Create `custom_activity_history.cpp`**

```cpp
#include "custom_activity_history.h"

#include "custom_db.h"
#include "custom_settings.h"
#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_lastseen_status.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include <QtCore/QDateTime>

namespace CustomActivityHistory {
namespace {

void RecordField(
        const QString &peerId,
        const QString &field,
        const QString &newValue,
        qint64 observedAt) {
    QString oldValue;
    const auto hadPrevious = CustomDB::GetLatestActivityHistoryValue(
        peerId, field, oldValue);
    if (hadPrevious && oldValue == newValue) {
        return; // haqiqiy o'zgarish yo'q — qayta yozmaymiz
    }
    CustomDB::SaveActivityHistoryEntry(
        peerId, field, hadPrevious, oldValue, newValue, observedAt);
}

} // namespace

QString EncodeStatus(const Data::LastseenStatus &status, int32 now) {
    if (status.isRecently()) {
        return u"recently"_q;
    } else if (status.isWithinWeek()) {
        return u"within_week"_q;
    } else if (status.isWithinMonth()) {
        return u"within_month"_q;
    } else if (status.isLongAgo()) {
        return u"long_ago"_q;
    } else if (status.isOnline(now)) {
        return u"online:"_q + QString::number(status.onlineTill());
    }
    const auto till = status.onlineTill();
    if (till > 0) {
        return u"offline:"_q + QString::number(till);
    }
    return u"empty"_q;
}

QString DecodeStatusLabel(const QString &encoded) {
    if (encoded == u"recently"_q) {
        return u"yaqinda (aniq vaqt yashiringan)"_q;
    } else if (encoded == u"within_week"_q) {
        return u"shu hafta ichida (aniq vaqt yashiringan)"_q;
    } else if (encoded == u"within_month"_q) {
        return u"shu oy ichida (aniq vaqt yashiringan)"_q;
    } else if (encoded == u"long_ago"_q) {
        return u"uzoq vaqt oldin (aniq vaqt yashiringan)"_q;
    } else if (encoded == u"empty"_q || encoded.isEmpty()) {
        return u"noma'lum"_q;
    } else if (encoded.startsWith(u"online:"_q)) {
        const auto ts = encoded.mid(7).toLongLong();
        return u"hozir online (taxminan "_q
            + QDateTime::fromSecsSinceEpoch(ts).toString(u"dd.MM.yyyy HH:mm"_q)
            + u" gacha)"_q;
    } else if (encoded.startsWith(u"offline:"_q)) {
        const auto ts = encoded.mid(8).toLongLong();
        return u"oxirgi marta ko'rilgan: "_q
            + QDateTime::fromSecsSinceEpoch(ts).toString(u"dd.MM.yyyy HH:mm"_q);
    }
    return encoded;
}

void Init(not_null<Main::Session*> session) {
    using Flag = Data::PeerUpdate::Flag;

    session->changes().peerUpdates(
        Flag::Name | Flag::Username | Flag::Photo | Flag::OnlineStatus
    ) | rpl::on_next([=](const Data::PeerUpdate &update) {
        const auto user = update.peer->asUser();
        if (!user) {
            return; // faqat User (shaxsiy chat) kuzatiladi — spec §7
        }
        const auto peerId = QString::number(user->id.value);
        if (!CustomSettings::ShouldTrackActivity(peerId, user->isContact())) {
            return;
        }

        const auto now = base::unixtime::now();

        if (update.flags & Flag::Name) {
            RecordField(peerId, u"name"_q, user->name(), now);
        }
        if (update.flags & Flag::Username) {
            RecordField(peerId, u"username"_q, user->username(), now);
        }
        if (update.flags & Flag::Photo) {
            const auto value = user->hasUserpic()
                ? QString::number(user->userpicPhotoId())
                : u"empty"_q;
            RecordField(peerId, u"photo"_q, value, now);
        }
        if (update.flags & Flag::OnlineStatus) {
            RecordField(
                peerId,
                u"status"_q,
                EncodeStatus(user->lastseen(), now),
                now);
        }
    }, session->lifetime());
}

} // namespace CustomActivityHistory
```

- [ ] **Step 3: Hook into `Main::Session`'s constructor**

In `Telegram/SourceFiles/main/main_session.cpp`, add the include near the other local includes at the top of the file:

```cpp
#include "custom_activity_history.h"
```

Then, in the `Session::Session(...)` constructor body, right after `Expects(_settings != nullptr);` (the first statement in the body, before `_api->requestTermsUpdate();`), add:

```cpp
	CustomActivityHistory::Init(this);
```

This mirrors the existing app-lifetime pattern (`CustomDB::Init()` / `CustomSettings::Init()` called once from `core/application.cpp`) but at session lifetime, since `session->changes()` only exists once a session is constructed. `Api::Updates` (already instantiated in this same constructor's initializer list at `_updates(std::make_unique<Api::Updates>(this))`) uses the identical `session->changes().peerUpdates(...)` mechanism from its own constructor, confirming this is a safe, established point to subscribe.

- [ ] **Step 4: Register the new files with the build**

Check how `custom_db.cpp`/`custom_settings.cpp` are registered (this repo may use a `CMakeLists.txt` file list or a generated sources list). Run:

```bash
grep -rn "custom_settings.cpp" --include=CMakeLists.txt .
```

Add `custom_activity_history.h` and `custom_activity_history.cpp` to the same list, next to the `custom_settings.*` / `custom_db.*` entries, following that file's existing formatting exactly.

- [ ] **Step 5: Build/compile-check.**

- [ ] **Step 6: Commit**

```bash
git add Telegram/SourceFiles/custom_activity_history.h Telegram/SourceFiles/custom_activity_history.cpp Telegram/SourceFiles/main/main_session.cpp Telegram/CMakeLists.txt
git commit -m "Activity History Log: capture module subscribed to peerUpdates"
```

(Adjust the `CMakeLists.txt` path in the `git add` to wherever Step 4 actually found the source list.)

---

### Task 4: Custom Window → Peers tab → "🕒 Activity History" section

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp`

**Context:** `fillPeersTab()` (around line 1557) currently ends with a call to `fillPerChatSection(content, controller);` followed by `Ui::AddSkip(content, st::settingsThumbSkip);` (around line 1615-1616). We add a new `fillActivityHistorySection()` function, following the same file structure as `fillPerChatSection`/the Whitelist-Blocklist section (`fillPeerSection`), and call it after `fillPerChatSection`.

- [ ] **Step 1: Add `fillActivityHistorySection()`**

Add this new function right before `void fillPeersTab(...)` (so it can be called from within it), following the same header/hint/divider structure as `fillPerChatSection`. It takes the same `onRebuild` callback `fillPeersTab` already receives and passes to `fillPeerSection`, so that adding/removing a peer from Include/Exclude immediately refreshes the visible rows (the existing Whitelist/Blocklist section uses the same `onRebuild` mechanism for the same reason — without it, a newly-added row would only appear after the Peers tab is reopened).

Note: this task's rows only have an "➖ remove" button — the "📜 Tarixni ko'rish" button per row is added in Task 6 Step 4, once `MakeHistoryBox` exists. Run Task 6 before Task 5 (the profile button), but Task 4 and Task 6 both touch this same function — apply Task 6 Step 4's row-loop replacement right after finishing this step.

```cpp
// ── Activity History Log: kuzatish qamrovi + kuzatilayotganlar ro'yxati ──
void fillActivityHistorySection(
        not_null<Ui::VerticalLayout*> content,
        not_null<Window::SessionController*> controller,
        Fn<void()> onRebuild) {
    content->add(
        object_ptr<Ui::FlatLabel>(
            content,
            rpl::single(u"🕒 Activity History"_q),
            st::defaultSubsectionTitle),
        st::defaultSubsectionTitlePadding);

    {
        const auto desc = content->add(
            object_ptr<Ui::FlatLabel>(
                content,
                rpl::single(u"Kontaktlarning ism, username, rasm va "
                    "last-seen o'zgarishlarini vaqt bilan saqlaydi — faqat "
                    "ilova legal ravishda qabul qilgan ma'lumot, hech qanday "
                    "maxfiylik cheklovi aylanib o'tilmaydi."_q),
                st::customModHintLabel),
            st::boxRowPadding,
            style::al_justify);
        content->widthValue() | rpl::on_next([=](int w) {
            const auto lw = w
                - st::boxRowPadding.left()
                - st::boxRowPadding.right();
            if (lw > 0) {
                desc->resizeToWidth(lw);
                desc->update();
            }
        }, desc->lifetime());
    }

    // ── Global toggle ─────────────────────────────────────────────
    Ui::AddSkip(content, 8);
    {
        const auto btn = content->add(
            object_ptr<Ui::SettingsButton>(
                content,
                rpl::single(u"Barcha Contact'larni kuzatish"_q),
                st::settingsButtonNoIcon));
        btn->toggleOn(rpl::single(
            CustomSettings::ActivityHistoryTrackAllContacts()));
        btn->toggledValue()
            | rpl::skip(1)
            | rpl::on_next([=](bool on) {
                CustomSettings::Set(u"activityHistoryTrackAllContacts"_q, on);
                Ui::Toast::Show(on
                    ? u"Barcha kontaktlarni kuzatish yoqildi ✓"_q
                    : u"Barcha kontaktlarni kuzatish oʻchirildi"_q);
            }, btn->lifetime());
    }

    // ── Include List ──────────────────────────────────────────────
    Ui::AddSkip(content, 12);
    content->add(
        object_ptr<Ui::FlatLabel>(
            content,
            rpl::single(u"Include List — standart holatdan qat'iy nazar "
                "har doim kuzatiladi:"_q),
            st::customModHintLabel),
        st::boxRowPadding);
    content->add(
        object_ptr<Ui::RoundButton>(
            content,
            rpl::single(u"Chat tanlash — Include"_q),
            st::defaultBoxButton),
        st::boxRowPadding)
    ->addClickHandler([=] {
        if (!gInstance) return;
        gInstance->showBox(Window::PrepareChooseRecipientBox(
            &controller->session(),
            [=](not_null<Data::Thread*> thread) -> bool {
                const auto peer = thread->peer();
                if (!peer->isUser()) {
                    Ui::Toast::Show(
                        u"Faqat shaxsiy chatlar (User) kuzatiladi."_q);
                    return true;
                }
                const auto peerId = QString::number(peer->id.value);
                const auto name = peer->name();
                if (CustomSettings::IsInActivityInclude(peerId)) {
                    Ui::Toast::Show(u"Bu chat allaqachon Include List'da."_q);
                    return true;
                }
                CustomSettings::AddToActivityInclude(peerId, name);
                Ui::Toast::Show(name + u" Include List'ga qoʻshildi."_q);
                if (onRebuild) onRebuild();
                return true;
            },
            rpl::single(u"Include List'ga qoʻshish"_q)));
    });
    for (const auto &e : CustomSettings::GetActivityInclude()) {
        const auto row = content->add(
            object_ptr<Ui::SettingsButton>(
                content,
                rpl::single(u"➖ "_q + e.second),
                st::settingsButtonNoIcon));
        row->addClickHandler([=, peerId = e.first, name = e.second] {
            CustomSettings::RemoveFromActivityInclude(peerId);
            Ui::Toast::Show(name + u" Include List'dan olib tashlandi."_q);
            if (onRebuild) onRebuild();
        });
    }

    // ── Exclude List ──────────────────────────────────────────────
    Ui::AddSkip(content, 12);
    content->add(
        object_ptr<Ui::FlatLabel>(
            content,
            rpl::single(u"Exclude List — hech qachon kuzatilmaydi:"_q),
            st::customModHintLabel),
        st::boxRowPadding);
    content->add(
        object_ptr<Ui::RoundButton>(
            content,
            rpl::single(u"Chat tanlash — Exclude"_q),
            st::defaultBoxButton),
        st::boxRowPadding)
    ->addClickHandler([=] {
        if (!gInstance) return;
        gInstance->showBox(Window::PrepareChooseRecipientBox(
            &controller->session(),
            [=](not_null<Data::Thread*> thread) -> bool {
                const auto peer = thread->peer();
                if (!peer->isUser()) {
                    Ui::Toast::Show(
                        u"Faqat shaxsiy chatlar (User) kuzatiladi."_q);
                    return true;
                }
                const auto peerId = QString::number(peer->id.value);
                const auto name = peer->name();
                if (CustomSettings::IsInActivityExclude(peerId)) {
                    Ui::Toast::Show(u"Bu chat allaqachon Exclude List'da."_q);
                    return true;
                }
                CustomSettings::AddToActivityExclude(peerId, name);
                Ui::Toast::Show(name + u" Exclude List'ga qoʻshildi."_q);
                if (onRebuild) onRebuild();
                return true;
            },
            rpl::single(u"Exclude List'ga qoʻshish"_q)));
    });
    for (const auto &e : CustomSettings::GetActivityExclude()) {
        const auto row = content->add(
            object_ptr<Ui::SettingsButton>(
                content,
                rpl::single(u"➖ "_q + e.second),
                st::settingsButtonNoIcon));
        row->addClickHandler([=, peerId = e.first, name = e.second] {
            CustomSettings::RemoveFromActivityExclude(peerId);
            Ui::Toast::Show(name + u" Exclude List'dan olib tashlandi."_q);
            if (onRebuild) onRebuild();
        });
    }

    Ui::AddSkip(content, st::settingsThumbSkip);
}
```

- [ ] **Step 2: Call it from `fillPeersTab`**

At the end of `fillPeersTab` (right after `fillPerChatSection(content, controller);` and its following `Ui::AddSkip(content, st::settingsThumbSkip);`), add:

```cpp
	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	fillActivityHistorySection(content, controller, onRebuild);
```

(`onRebuild` is already a parameter of `fillPeersTab` itself — see its signature at the top of the function — so this is just forwarding it, the same way `fillPeerSection(content, controller, true, onRebuild)` already does two calls above.)

- [ ] **Step 3: Build/compile-check.**

- [ ] **Step 4: Commit**

```bash
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "Activity History Log: Peers tab Include/Exclude UI section"
```

---

### Task 5: Profile page — "📜 Faollik tarixi" button

**Files:**
- Modify: `Telegram/SourceFiles/info/profile/info_profile_actions.cpp`

**Context:** `ActionsFiller::fillUserActions(not_null<UserData*> user)` (line 3126) is where per-user profile action buttons are added (`addShareContactAction`, `addBlockAction`, etc.), using the `AddActionButton(_wrap, text, toggleOnProducer, callback, icon, style)` template helper (defined at line 1019). The button must only be visible when `CustomSettings::ShouldTrackActivity(peerId, user->isContact())` is true, and must not show for `user->isSelf()`.

- [ ] **Step 1: Add the include**

Near the top of `info_profile_actions.cpp`, add:

```cpp
#include "custom_settings.h"
#include "custom_activity_history_box.h"
```

- [ ] **Step 2: Add `addActivityHistoryAction` to `ActionsFiller`**

Add this private method to the `ActionsFiller` class (declare it alongside the other `add*Action` method declarations in the class body, and define it near `addBlockAction`'s definition):

```cpp
void ActionsFiller::addActivityHistoryAction(not_null<UserData*> user) {
	const auto peerId = QString::number(user->id.value);
	AddActionButton(
		_wrap,
		rpl::single(u"📜 Faollik tarixi"_q),
		rpl::single(CustomSettings::ShouldTrackActivity(
			peerId, user->isContact())),
		[=] {
			_controller->parentController()->show(
				CustomActivityHistory::MakeHistoryBox(
					&user->session(),
					peerId,
					user->name()));
		},
		nullptr);
}
```

- [ ] **Step 3: Call it from `fillUserActions`**

In `ActionsFiller::fillUserActions`, right after `addShareContactAction(user);` and before the `if (!user->isSelf())` block, add:

```cpp
	if (!user->isSelf()) {
		addActivityHistoryAction(user);
	}
```

(Combine with the existing adjacent `if (!user->isSelf())` block that already wraps `addEditContactAction`/`addDeleteContactAction` if you prefer one `if`; keeping it separate here avoids touching that existing block's diff.)

- [ ] **Step 4: Build/compile-check** (this will only fully succeed once `custom_activity_history_box.h`'s `MakeHistoryBox` exists — see Task 6. If executing tasks in order via subagent-driven-development, do Task 6 before Task 5, OR add a temporary forward declaration `namespace CustomActivityHistory { object_ptr<Ui::BoxContent> MakeHistoryBox(Main::Session*, const QString&, const QString&); }` and swap to the real include once Task 6 lands. Prefer reordering: **execute Task 6 before Task 5** to avoid this — the task list above is grouped by "where in the UI", not strict execution order.)

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/info/profile/info_profile_actions.cpp
git commit -m "Activity History Log: profile page history button"
```

---

### Task 6: Shared "History Viewer" Box

**Files:**
- Create: `Telegram/SourceFiles/custom_activity_history_box.h`
- Create: `Telegram/SourceFiles/custom_activity_history_box.cpp`
- Modify: build file (same list as Task 3 Step 4)
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp` (replace the Task 4 stub with the real call)

**Execution order note:** run this task BEFORE Task 5 (profile button) so `MakeHistoryBox` exists when Task 5 wires its click handler. Update Task 4's stub in this task too.

**Context:** The Box shows, for one peer: 1) current state, 2) last-known value before hidden (if currently hidden), 3) reconstructed online periods, 4) full reverse-chronological change log — all sourced from `CustomDB::GetActivityHistory(peerId)`. Online-period reconstruction pairs each `status` row starting with `"online:"` with the next `status` row starting with `"offline:"`, per spec §4.

- [ ] **Step 1: Create `custom_activity_history_box.h`**

```cpp
#pragma once

#include <QtCore/QString>

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class BoxContent;
} // namespace Ui

namespace CustomActivityHistory {

// Ikkala UI joyidan (Peers tab "Tarixni ko'rish" tugmasi va Profil sahifasi
// "Faollik tarixi" tugmasi) chaqiriladigan umumiy "Tarix ko'ruvchi" Box.
// displayName — sarlavhada ko'rsatish uchun (masalan peer->name()).
[[nodiscard]] object_ptr<Ui::BoxContent> MakeHistoryBox(
    not_null<Main::Session*> session,
    const QString &peerId,
    const QString &displayName);

} // namespace CustomActivityHistory
```

- [ ] **Step 2: Create `custom_activity_history_box.cpp`**

```cpp
#include "custom_activity_history_box.h"

#include "custom_activity_history.h"
#include "custom_db.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/widgets/scroll_area.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"
#include <QtCore/QDateTime>

namespace CustomActivityHistory {
namespace {

QString FormatEntryLine(const CustomDB::ActivityHistoryEntry &entry) {
	const auto when = QDateTime::fromSecsSinceEpoch(entry.observedAt)
		.toString(u"dd.MM.yyyy HH:mm"_q);
	const auto fieldLabel = [&] {
		if (entry.field == u"name"_q) return u"Ism"_q;
		if (entry.field == u"username"_q) return u"Username"_q;
		if (entry.field == u"photo"_q) return u"Rasm"_q;
		if (entry.field == u"status"_q) return u"Last-seen"_q;
		return entry.field;
	}();
	const auto valueLabel = (entry.field == u"status"_q)
		? DecodeStatusLabel(entry.newValue)
		: (entry.newValue.isEmpty() ? u"(bo'sh)"_q : entry.newValue);
	if (!entry.hasOldValue) {
		return fieldLabel + u": " + valueLabel + u" (kuzatish boshlandi, " + when + u")"_q;
	}
	const auto oldLabel = (entry.field == u"status"_q)
		? DecodeStatusLabel(entry.oldValue)
		: (entry.oldValue.isEmpty() ? u"(bo'sh)"_q : entry.oldValue);
	return fieldLabel + u": '" + oldLabel + u"' -> '" + valueLabel + u"' (" + when + u")"_q;
}

struct OnlinePeriod {
	qint64 from = 0;
	qint64 to = 0;
};

// Spec §4: T1 (online-observed) dan keyingi birinchi T2 (offline-observed)
// bilan juftlaydi. entries — GetActivityHistory() natijasi (newest-first).
QVector<OnlinePeriod> ReconstructOnlinePeriods(
		const QVector<CustomDB::ActivityHistoryEntry> &entries) {
	// Chronological tartibga o'tkazamiz (eskisidan yangisiga), algoritm
	// shunday ishlashi osonroq.
	QVector<CustomDB::ActivityHistoryEntry> chrono;
	chrono.reserve(entries.size());
	for (auto it = entries.crbegin(); it != entries.crend(); ++it) {
		if (it->field == u"status"_q) chrono.append(*it);
	}

	QVector<OnlinePeriod> result;
	qint64 openFrom = 0;
	for (const auto &e : chrono) {
		if (e.newValue.startsWith(u"online:"_q)) {
			openFrom = e.observedAt; // ketma-ket online — oxirgisi ustun oladi
		} else if (e.newValue.startsWith(u"offline:"_q) && openFrom > 0) {
			const auto till = e.newValue.mid(8).toLongLong();
			result.append({ openFrom, till > 0 ? till : e.observedAt });
			openFrom = 0;
		}
	}
	return result;
}

} // namespace

object_ptr<Ui::BoxContent> MakeHistoryBox(
		not_null<Main::Session*> session,
		const QString &peerId,
		const QString &displayName) {
	return Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"📜 "_q + displayName + u" — Faollik tarixi"_q));

		const auto entries = CustomDB::GetActivityHistory(peerId);
		const auto content = box->verticalLayout();

		// ── 1) Joriy holat + 2) So'nggi ko'ra olgan holatim ─────────
		QString latestStatus;
		const auto hasStatus = CustomDB::GetLatestActivityHistoryValue(
			peerId, u"status"_q, latestStatus);
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Joriy holat: "_q + (hasStatus
					? DecodeStatusLabel(latestStatus)
					: u"noma'lum (hali kuzatilmagan)"_q)),
				st::boxLabel),
			st::boxRowPadding);

		if (hasStatus && (latestStatus == u"recently"_q
				|| latestStatus == u"within_week"_q
				|| latestStatus == u"within_month"_q
				|| latestStatus == u"long_ago"_q)) {
			for (const auto &e : entries) {
				if (e.field == u"status"_q && (e.newValue.startsWith(u"online:"_q)
						|| e.newValue.startsWith(u"offline:"_q))) {
					content->add(
						object_ptr<Ui::FlatLabel>(
							content,
							rpl::single(u"So'nggi ko'ra olgan holatim: "_q
								+ DecodeStatusLabel(e.newValue)),
							st::boxLabel),
						st::boxRowPadding);
					break;
				}
			}
		}

		// ── 3) Online bo'lgan davrlar ────────────────────────────────
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Online bo'lgan davrlar:"_q),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);
		const auto periods = ReconstructOnlinePeriods(entries);
		if (periods.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"(hali ma'lumot yo'q)"_q),
					st::boxLabel),
				st::boxRowPadding);
		}
		for (const auto &p : periods) {
			const auto from = QDateTime::fromSecsSinceEpoch(p.from);
			const auto to = QDateTime::fromSecsSinceEpoch(p.to);
			const auto minutes = std::max<qint64>(0, (p.to - p.from) / 60);
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(
						from.toString(u"dd.MM HH:mm"_q) + u" - "_q
						+ to.toString(u"HH:mm"_q) + u" ("_q
						+ QString::number(minutes) + u" daqiqa)"_q),
					st::boxLabel),
				st::boxRowPadding);
		}

		// ── 4) To'liq o'zgarishlar jurnali ───────────────────────────
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"To'liq o'zgarishlar jurnali:"_q),
				st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);
		if (entries.isEmpty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(u"(hali hech qanday yozuv yo'q)"_q),
					st::boxLabel),
				st::boxRowPadding);
		}
		for (const auto &e : entries) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					rpl::single(FormatEntryLine(e)),
					st::boxLabel),
				st::boxRowPadding);
		}

		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	});
}

} // namespace CustomActivityHistory
```

- [ ] **Step 3: Register the new files with the build** (same as Task 3 Step 4 — add `custom_activity_history_box.h`/`.cpp` to the same source list).

- [ ] **Step 4: Wire the Peers-tab "Tarixni ko'rish" stub from Task 4**

Task 4's section currently only lists Include/Exclude entries with a remove button (no per-entry "view history" button yet — Task 4's Step 1 code above did not include one). Extend the Include/Exclude entry rows in `fillActivityHistorySection` (from Task 4) by adding a second button per row. Replace each of the two `for (const auto &e : CustomSettings::GetActivityInclude())` / `GetActivityExclude()` loops' row-building with:

```cpp
    for (const auto &e : CustomSettings::GetActivityInclude()) {
        const auto row = content->add(
            object_ptr<Ui::SettingsButton>(
                content,
                rpl::single(u"➖ "_q + e.second),
                st::settingsButtonNoIcon));
        row->addClickHandler([=, peerId = e.first, name = e.second] {
            CustomSettings::RemoveFromActivityInclude(peerId);
            Ui::Toast::Show(name + u" Include List'dan olib tashlandi."_q);
            if (onRebuild) onRebuild();
        });
        const auto historyRow = content->add(
            object_ptr<Ui::SettingsButton>(
                content,
                rpl::single(u"📜 Tarixni ko'rish — "_q + e.second),
                st::settingsButtonNoIcon));
        historyRow->addClickHandler([=, peerId = e.first, name = e.second] {
            if (!gInstance) return;
            gInstance->showBox(CustomActivityHistory::MakeHistoryBox(
                &controller->session(), peerId, name));
        });
    }
```

(Apply the same `historyRow` addition to the `GetActivityExclude()` loop.)

- [ ] **Step 5: Build/compile-check the whole feature end-to-end.**

- [ ] **Step 6: Commit**

```bash
git add Telegram/SourceFiles/custom_activity_history_box.h Telegram/SourceFiles/custom_activity_history_box.cpp Telegram/SourceFiles/custom_mod_window.cpp Telegram/CMakeLists.txt
git commit -m "Activity History Log: shared History Viewer Box + wire both UI entry points"
```

---

### Task 7: Full manual regression pass

**Blocked on:** a successful local build. As of 2026-07-20, no build has succeeded in this environment (disk-space exhaustion, then linker OOM from ~15GB RAM — see [project_custom_window_redesign.md] memory). Do not attempt this task until a build succeeds.

- [ ] **Step 1:** Build the app (Release or Debug, whichever succeeds first).
- [ ] **Step 2:** Log in with a test account that has at least 2-3 real contacts.
- [ ] **Step 3:** Open Custom Window → Peers tab, scroll to "🕒 Activity History". Confirm the global toggle reflects the default (ON) and the Include/Exclude sections render empty lists initially.
- [ ] **Step 4:** Add one contact to Include List via "Chat tanlash — Include". Confirm it appears in the list with both "➖" (remove) and "📜 Tarixni ko'rish" buttons.
- [ ] **Step 5:** Trigger a real change on a tracked contact — easiest is to ask a second test account (or the contact themselves, if coordinated) to change their display name or username. Confirm within a few seconds that `custom_activity_history.db`'s `activity_history` table (or the History Viewer Box) shows a new row with the old and new value.
- [ ] **Step 6:** Open the contact's profile page. Confirm the "📜 Faollik tarixi" button is visible (since they're in Include List) and opens the same History Viewer Box content as the Peers-tab button.
- [ ] **Step 7:** Add a different contact to Exclude List. Confirm their profile page does NOT show the "📜 Faollik tarixi" button, and no new rows are written for them even if they change their name/username while Exclude-listed.
- [ ] **Step 8:** Turn OFF "Barcha Contact'larni kuzatish". Confirm a contact who is in neither list stops being tracked (no new rows), while the Include-listed contact from Step 4 keeps being tracked (Include overrides the global toggle, per spec §3.1).
- [ ] **Step 9:** For a contact whose last-seen is currently hidden (`recently`/`within_week`/etc.), open their History Box and confirm "So'nggi ko'ra olgan holatim" shows the last concrete `online:`/`offline:` value ever observed, if one exists in the log.
- [ ] **Step 10:** Restart the app fully. Confirm all Include/Exclude entries and the global toggle persisted (loaded from `peer_lists.json` / `QSettings`), and the activity log itself persisted (loaded from the SQLite file) — open a previously-tracked contact's History Box and confirm old entries are still there.
- [ ] **Step 11:** Update the plan status lines below with the outcome (pass/fail per step, and any bugs found + fixed).

---

## Post-plan note for the user

This plan reduces the spec's "photo" field to change-events only (no thumbnail saved) for v1 — see the Goal section's scope note at the top of this document for why. If you want the actual thumbnail persisted later, that needs a separate small design pass (decoding `stripped_thumb` from the `UserProfilePhoto` MTP object, or triggering an async download) — flag it as a follow-up task if/when you want it, not part of this plan.
