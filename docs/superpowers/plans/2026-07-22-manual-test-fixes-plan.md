# Manual-Test Batch (2026-07-22) — Bug Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. (If you are not running inside a superpowers-enabled agent, simply execute the tasks below in order — each is fully self-contained.)

**Goal:** Fix the 5 concrete, already-diagnosed issues found while manually testing the Activity History Log feature: a suspected performance regression on first-start, plain-text Include/Exclude rows, a backup/import gap, missing DB pruning, and a log-noise question — WITHOUT touching the two items that need separate treatment (offline-first startup needs its own brainstorm/spec cycle; upstream v7.0.2 sync is explicitly deferred by the project owner until this batch lands).

**Architecture:** All changes are localized to 3 existing files already used by the Activity History Log feature — `Telegram/SourceFiles/custom_db.h/.cpp` (data layer), `Telegram/SourceFiles/custom_activity_history.cpp` (capture module, temporary diagnostic only), and `Telegram/SourceFiles/custom_mod_window.cpp` (Peers tab UI). No new files, no new tables, no schema version bump. The performance fix reuses an existing in-file caching pattern (`EnsurePeerCacheLoaded` / `gDeletedCache`) already present in `custom_db.cpp` for a different table — Task 2 replicates that pattern for `activity_history`.

**Tech Stack:** C++17, Qt 6 (Widgets), sqlite3 C API (raw, no ORM), the project's own `rpl::` reactive-signal library, MSVC (Visual Studio, Release x64 configuration — this project has no automated test suite for `custom_*.cpp` files, so every task ends with a manual build + manual verification step instead of a unit test).

**Scope note (why 2 items from the research doc are excluded):** `docs/superpowers/plans/2026-07-22-manual-test-findings.md` recorded 9 items total. This plan covers items 3, 4, 5, 6 (renumbered as Tasks 2–6 below) plus the crash fix's build-confirmation (already committed separately as `0515f02c0f`, not repeated here) and the perf investigation (item 1, split into a diagnostic Task 1 + conditional fix Task 2). Item 2 (offline-first startup) is **excluded** — it is a major architectural feature the user explicitly said needs its own `research + brainstorm + superpowers` cycle first, not a task in a bug-fix plan. Item 9 (native update-checker research) is **excluded** — it's a research-only question with no code to plan. Item 8 (upstream v7.0.2 sync) is **excluded** — the user explicitly said this should happen only *after* this batch is done, and it's a merge-task, not a feature-implementation task (same shape as `docs/superpowers/plans/2026-07-11-upstream-sync-552-commits.md`).

**Durable project rules that apply to every task below:** commit only to the local `Oybek` branch; never push to the `upstream` remote; never open a PR against `telegramtdesktop/tdesktop`. Pushing to `origin/Oybek` is fine but not required by this plan — leave that decision to whoever runs it.

---

### Task 1: Diagnostic instrumentation for the first-start performance regression

**Context:** The user reports first-start-after-logout time regressed from ~7-8s to ~11-12s after the Activity History Log feature shipped. `custom_activity_history.cpp`'s `Init()` subscribes to `session->changes().peerUpdates(Flag::Name | Flag::Username | Flag::Photo | Flag::OnlineStatus)`, and its handler calls `RecordField()` for every matching flag on every fired update — each call does a **synchronous SQLite SELECT** via `CustomDB::GetLatestActivityHistoryValue()`, with **no in-memory cache** at all (unlike this file's sibling tables, which do cache — see Task 2). A fresh login re-syncs the entire contact list into a clean in-memory session, which is very likely to fire these peerUpdate flags for every contact — meaning up to 4 synchronous DB round-trips per contact, every single first-start. This is a strong, code-verified hypothesis, but it has **not been measured**. Do not skip straight to a fix — gather the numbers first.

**Files:**
- Modify: `Telegram/SourceFiles/custom_activity_history.cpp`

- [ ] **Step 1: Add temporary timing instrumentation**

Current top of file (lines 1-14):
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
```

Replace with:
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
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

namespace CustomActivityHistory {
namespace {

// TASK 1 diagnostika (2026-07-22 perf-regression tekshiruvi): vaqtinchalik
// hisoblagich. Task 1 tugagach OLIB TASHLANADI — Task 2'ning birinchi
// qadami shu bloklarni revert qiladi.
struct ActivityDiagStats {
	int callCount = 0;
	qint64 totalUs = 0;
};
ActivityDiagStats gDiagStats;
```

Current `RecordField` (lines 16-29):
```cpp
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
```

Replace with:
```cpp
void RecordField(
		const QString &peerId,
		const QString &field,
		const QString &newValue,
		qint64 observedAt) {
	QElapsedTimer timer;
	timer.start();

	QString oldValue;
	const auto hadPrevious = CustomDB::GetLatestActivityHistoryValue(
		peerId, field, oldValue);
	if (hadPrevious && oldValue == newValue) {
		gDiagStats.callCount++;
		gDiagStats.totalUs += timer.nsecsElapsed() / 1000;
		return; // haqiqiy o'zgarish yo'q — qayta yozmaymiz
	}
	CustomDB::SaveActivityHistoryEntry(
		peerId, field, hadPrevious, oldValue, newValue, observedAt);
	gDiagStats.callCount++;
	gDiagStats.totalUs += timer.nsecsElapsed() / 1000;
}
```

Current end of `Init()` (the last line before its closing brace, inside `void Init(not_null<Main::Session*> session) {`):
```cpp
void Init(not_null<Main::Session*> session) {
	using Flag = Data::PeerUpdate::Flag;

	session->changes().peerUpdates(
```

Replace with (adds a one-shot 15-second report right before the subscription is set up):
```cpp
void Init(not_null<Main::Session*> session) {
	using Flag = Data::PeerUpdate::Flag;

	// TASK 1 diagnostika: 15 soniyadan keyin bir marta natijani log'ga
	// yozadi. Olib tashlanadi — Task 2'ga qarang.
	QTimer::singleShot(15000, [] {
		qDebug() << "[ActivityHistoryDiag] calls=" << gDiagStats.callCount
			<< " totalUs=" << gDiagStats.totalUs
			<< " avgUs=" << (gDiagStats.callCount
				? gDiagStats.totalUs / gDiagStats.callCount : 0);
	});

	session->changes().peerUpdates(
```

- [ ] **Step 2: Build**

Build the `Telegram` project in Visual Studio (Release x64). Expected: 0 errors (this is a pure addition, no signature changes).

- [ ] **Step 3: Run the real test**

Fully log out of the account inside the app, then log back in (this is the exact "first-start after logout" scenario the user reported). Do not touch anything else. Wait at least 15 seconds after the login screen disappears.

- [ ] **Step 4: Read the result**

Open `out/Release/log.txt` (or the console if running from Visual Studio's debugger) and find the line starting with `[ActivityHistoryDiag]`. Record the three numbers: `calls`, `totalUs`, `avgUs`.

- [ ] **Step 5: Commit the diagnostic (temporary, will be reverted in Task 2)**

```bash
git add Telegram/SourceFiles/custom_activity_history.cpp
git commit -m "temp: activity history perf diagnostic instrumentation (Task 1)"
```

- [ ] **Step 6: Decide**

- If `calls` is roughly proportional to the account's contact count (dozens to thousands) **and** `totalUs` is at least ~500,000 (0.5s) — the hypothesis is confirmed. Proceed to Task 2.
- If `calls` is small (under ~50) or `totalUs` is negligible (under ~50,000) — the hypothesis is **wrong**. Do NOT apply Task 2. Instead: `git revert HEAD` to remove the diagnostic commit, and report the actual measured numbers back to the project owner — the real regression cause is elsewhere and is out of scope for this plan.

---

### Task 2: Replace per-call SQLite reads with an in-memory cache (conditional on Task 1)

**Only do this task if Task 1's Step 6 confirmed the hypothesis.**

**Context:** `custom_db.cpp` already solves this exact class of problem for a different table. `EnsurePeerCacheLoaded()` (lines 290-339) lazily loads a peer's `actioned_messages` rows into an in-memory `QHash` on first touch, guarded by `gCacheMutex`, so repeat lookups for the same peer never hit SQLite again. `GetLatestActivityHistoryValue()` currently does the opposite — a fresh `sqlite3_prepare_v2`/`step`/`finalize` round-trip on **every single call**, once per field per contact, with no caching at all. This task adds the same lazy-cache pattern for `activity_history`.

**Files:**
- Modify: `Telegram/SourceFiles/custom_activity_history.cpp` (revert Task 1's diagnostic)
- Modify: `Telegram/SourceFiles/custom_db.cpp`

- [ ] **Step 1: Revert Task 1's diagnostic instrumentation**

```bash
git revert --no-edit HEAD
```

(This assumes Task 1's commit is the most recent one. If other commits landed in between, instead manually undo the 3 edits from Task 1 Step 1 in `custom_activity_history.cpp`.)

- [ ] **Step 2: Add the in-memory cache globals**

Current (`custom_db.cpp` lines 33-46):
```cpp
// In-memory caches for O(1) lookups, populated lazily per-peer (see
// EnsurePeerCacheLoaded()) rather than in bulk for the whole archive at
// startup — with 200k+ rows a full-table load blocked the UI for seconds.
// gCacheMutex guards both caches against concurrent reads/writes from
// background threads (e.g. MTProto callbacks vs. UI thread).
static QMutex gCacheMutex;
static QHash<QString, QSet<long long>> gDeletedCache;
static QHash<QString, QHash<long long, QString>> gEditedCache;
// Which peers have already had their entries loaded from the DB into the
// caches above (via EnsurePeerCacheLoaded()). A peer can also be "loaded"
// implicitly by live writes (SaveMessage()/MarkEdited()) before it's ever
// explicitly loaded; EnsurePeerCacheLoaded() merges rather than overwrites,
// so that's safe either way.
static QSet<QString> gLoadedPeers;
```

Replace with (keeps every existing line unchanged, only appends two new globals at the end):
```cpp
// In-memory caches for O(1) lookups, populated lazily per-peer (see
// EnsurePeerCacheLoaded()) rather than in bulk for the whole archive at
// startup — with 200k+ rows a full-table load blocked the UI for seconds.
// gCacheMutex guards both caches against concurrent reads/writes from
// background threads (e.g. MTProto callbacks vs. UI thread).
static QMutex gCacheMutex;
static QHash<QString, QSet<long long>> gDeletedCache;
static QHash<QString, QHash<long long, QString>> gEditedCache;
// Which peers have already had their entries loaded from the DB into the
// caches above (via EnsurePeerCacheLoaded()). A peer can also be "loaded"
// implicitly by live writes (SaveMessage()/MarkEdited()) before it's ever
// explicitly loaded; EnsurePeerCacheLoaded() merges rather than overwrites,
// so that's safe either way.
static QSet<QString> gLoadedPeers;
// Activity History Log: peerId -> field -> latest new_value. Same lazy
// per-peer-load pattern as gDeletedCache/gEditedCache above (see
// EnsureActivityCacheLoaded below), added to fix a first-start performance
// regression — see Task 1/2 of docs/superpowers/plans/2026-07-22-manual-test-fixes-plan.md.
static QHash<QString, QHash<QString, QString>> gActivityLatestCache;
static QSet<QString> gActivityLoadedPeers;
```

- [ ] **Step 3: Add `EnsureActivityCacheLoaded`**

Add this new `static` function immediately before `SaveActivityHistoryEntry`'s definition (search for `void SaveActivityHistoryEntry(` in `custom_db.cpp`, insert directly above it):

```cpp
// Loads one peer's latest-value-per-field into gActivityLatestCache on
// first touch, unless already loaded — mirrors EnsurePeerCacheLoaded()
// above, applied to the activity_history table.
static void EnsureActivityCacheLoaded(const QString &peerId) {
	{
		QMutexLocker locker(&gCacheMutex);
		if (gActivityLoadedPeers.contains(peerId)) return;
	}
	Init();
	if (!gDb) return;

	QHash<QString, QString> latest;
	sqlite3_stmt *stmt = nullptr;
	if (sqlite3_prepare_v2(gDb,
			"SELECT field, new_value FROM activity_history "
			"WHERE peer_id = ? ORDER BY observed_at ASC, id ASC",
			-1, &stmt, nullptr) == SQLITE_OK) {
		bindText(stmt, 1, peerId);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			// ASC tartib — oxirgi yozilgan qator eng yangi qiymat, shuning
			// uchun hash'ga ustma-ust yozilaveradi va oxiri eng yangisi qoladi.
			latest[colText(stmt, 0)] = colText(stmt, 1);
		}
		sqlite3_finalize(stmt);
	}

	QMutexLocker locker(&gCacheMutex);
	if (gActivityLoadedPeers.contains(peerId)) return; // race guard
	gActivityLatestCache[peerId] = latest;
	gActivityLoadedPeers.insert(peerId);
}
```

- [ ] **Step 4: Rewrite `GetLatestActivityHistoryValue` to read from the cache**

Current (`custom_db.cpp`):
```cpp
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
```

Replace with:
```cpp
bool GetLatestActivityHistoryValue(
        const QString &peerId,
        const QString &field,
        QString &outValue) {
    EnsureActivityCacheLoaded(peerId);
    QMutexLocker locker(&gCacheMutex);
    const auto peerIt = gActivityLatestCache.constFind(peerId);
    if (peerIt == gActivityLatestCache.constEnd()) return false;
    const auto fieldIt = peerIt->constFind(field);
    if (fieldIt == peerIt->constEnd()) return false;
    outValue = fieldIt.value();
    return true;
}
```

- [ ] **Step 5: Update the cache on every write in `SaveActivityHistoryEntry`**

Current (`custom_db.cpp`):
```cpp
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
```

Replace with:
```cpp
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

    // Keep the in-memory cache in sync so repeat lookups this session see
    // the new value without another SQLite round-trip.
    {
        QMutexLocker locker(&gCacheMutex);
        gActivityLatestCache[peerId][field] = newValue;
        gActivityLoadedPeers.insert(peerId);
    }
}
```

- [ ] **Step 6: Clear the new cache on full restore, alongside the existing ones**

Current (`custom_db.cpp`):
```cpp
void LoadRestoreCache() {
    Init();
    QMutexLocker locker(&gCacheMutex);
    gDeletedCache.clear();
    gEditedCache.clear();
    gLoadedPeers.clear();
}
```

Replace with:
```cpp
void LoadRestoreCache() {
    Init();
    QMutexLocker locker(&gCacheMutex);
    gDeletedCache.clear();
    gEditedCache.clear();
    gLoadedPeers.clear();
    gActivityLatestCache.clear();
    gActivityLoadedPeers.clear();
}
```

- [ ] **Step 7: Build**

Build the `Telegram` project in Visual Studio (Release x64). Expected: 0 errors.

- [ ] **Step 8: Manually verify the fix**

Repeat the same test as Task 1 Step 3 (full logout, then login, time it with a stopwatch from pressing Enter to the app being fully usable). Confirm the elapsed time is back down near the ~7-8s baseline the user reported before this regression. This is a subjective timing check (no automated benchmark exists for this project) — if it's still slow, say so plainly rather than claiming success; the remaining regression would need a fresh Task-1-style diagnostic round to find the next bottleneck (out of scope for this plan).

- [ ] **Step 9: Commit**

```bash
git add Telegram/SourceFiles/custom_db.cpp
git commit -m "perf: cache Activity History latest-value lookups in memory

GetLatestActivityHistoryValue did a fresh SQLite round-trip on every
call, once per field per contact, with no caching (unlike the
sibling actioned_messages caches in this file). A fresh login re-syncs
the whole contact list and fires this path for every contact, which is
the likely source of the first-start performance regression. Add a
lazy per-peer in-memory cache mirroring the existing
EnsurePeerCacheLoaded/gDeletedCache pattern."
```

---

### Task 3: Upgrade Include/Exclude list rows to the rich avatar+name+ID pattern

**Context:** `fillActivityHistorySection()` in `custom_mod_window.cpp` currently renders each Include/Exclude entry as a plain-text `Ui::SettingsButton` (`➖ <name>`) that IS the delete action, followed by a separate `📜 Tarixni ko'rish — <name>` button. The White/Black List section (`fillPeerSection`, its `state->addEntry` lambda, lines 1138-1260) instead renders a custom row with a circular avatar, a name label, an "ID: <id>" label, and a dedicated delete button. This task extracts that visual pattern into a small reusable function and uses it for Include/Exclude, without touching `fillPeerSection` itself (it already works, don't risk regressing it).

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp`

- [ ] **Step 1: Add a forward declaration**

Current (`custom_mod_window.cpp`, near the top with the other forward declarations):
```cpp
void fillActivityHistorySection(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	Fn<void()> onRebuild);
```

Add immediately above it:
```cpp
// Avatar + ism + ID + o'chirish tugmasi bilan bitta peer qatorini quradi
// va uni |content| ga qo'shadi. White/Black List'dagi fillPeerSection's
// state->addEntry bilan bir xil vizual pattern, lekin SlideWrap/entryWraps
// holat boshqaruvisiz — chaqiruvchi butun bo'limni onRebuild() orqali
// to'liq qayta quradi (Include/Exclude List shu tarzda ishlaydi).
void AddAvatarPeerRow(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	const QString &peerId,
	const QString &name,
	Fn<void()> onDelete);
void fillActivityHistorySection(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	Fn<void()> onRebuild);
```

- [ ] **Step 2: Add the function definition**

Insert this new function immediately before `void fillActivityHistorySection(` at line 1563 (search for that exact line — it's the *definition*, not the forward declaration you just edited):

```cpp
void AddAvatarPeerRow(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		const QString &peerId,
		const QString &name,
		Fn<void()> onDelete) {
	constexpr int kRowH    = 56;
	constexpr int kAvSize  = 38;
	constexpr int kPadL = 14;
	constexpr int kPadR = 12;
	constexpr int kGap  = 12;
	constexpr int kDelBtnW = 76;

	const auto row = content->add(object_ptr<Ui::RpWidget>(content));
	row->setFixedHeight(kRowH);

	// Avatar circle — real userpic agar peer cache da bo'lsa,
	// aks holda fallback (harf + rang).
	const auto av = Ui::CreateChild<Ui::RpWidget>(row);
	av->setFixedSize(kAvSize, kAvSize);
	const auto userpicView = std::make_shared<Ui::PeerUserpicView>();
	const auto session = &controller->session();
	av->paintRequest() | rpl::on_next([=](QRect) {
		Painter p(av);
		PaintPeerAvatar(
			p,
			QRect(0, 0, kAvSize, kAvSize),
			peerId,
			name,
			session,
			*userpicView);
	}, av->lifetime());

	const auto nameLabel = Ui::CreateChild<Ui::FlatLabel>(
		row,
		rpl::single(name.isEmpty() ? peerId : name),
		st::boxLabel);
	const auto idLabel = Ui::CreateChild<Ui::FlatLabel>(
		row,
		rpl::single(u"ID: "_q + peerId),
		st::customModHintLabel);
	const auto delBtn = Ui::CreateChild<Ui::RoundButton>(
		row,
		rpl::single(u"Oʻchirish"_q),
		st::attentionBoxButton);
	delBtn->setFixedWidth(kDelBtnW);
	av->show();
	nameLabel->show();
	idLabel->show();
	delBtn->show();

	row->paintRequest() | rpl::on_next([=](QRect) {
		Painter p(row);
		p.fillRect(kPadL + kAvSize + kGap, kRowH - 1,
			row->width() - kPadL - kAvSize - kGap - kPadR, 1,
			st::shadowFg->c);
	}, row->lifetime());

	const auto layoutRow = [=](int w) {
		const auto avY = (kRowH - kAvSize) / 2;
		av->move(kPadL, avY);
		av->update();
		const auto textX = kPadL + kAvSize + kGap;
		const auto textW = w - textX - kGap - kDelBtnW - kPadR;
		if (textW <= 0) return;
		nameLabel->resizeToWidth(textW);
		nameLabel->move(textX, 10);
		nameLabel->update();
		idLabel->resizeToWidth(textW);
		idLabel->move(textX, 10 + nameLabel->height() + 2);
		idLabel->update();
		const auto btnY = (kRowH - st::defaultBoxButton.height) / 2;
		delBtn->move(w - kPadR - kDelBtnW, btnY);
	};
	row->widthValue() | rpl::on_next(layoutRow, row->lifetime());
	if (content->width() > 0) layoutRow(content->width());

	if (onDelete) {
		delBtn->addClickHandler([=] {
			delBtn->setDisabled(true);
			onDelete();
		});
	}
}

```

- [ ] **Step 3: Use it for the Include List**

Current (`custom_mod_window.cpp`, inside `fillActivityHistorySection`, the Include List loop):
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

Replace with:
```cpp
	for (const auto &e : CustomSettings::GetActivityInclude()) {
		const auto peerId = e.first;
		const auto name = e.second;
		AddAvatarPeerRow(content, controller, peerId, name, [=] {
			CustomSettings::RemoveFromActivityInclude(peerId);
			Ui::Toast::Show(name + u" Include List'dan olib tashlandi."_q);
			if (onRebuild) onRebuild();
		});
		const auto historyRow = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(u"📜 Tarixni ko'rish — "_q + name),
				st::settingsButtonNoIcon));
		historyRow->addClickHandler([=] {
			if (!gInstance) return;
			gInstance->showBox(CustomActivityHistory::MakeHistoryBox(
				&controller->session(), peerId, name));
		});
	}
```

- [ ] **Step 4: Use it for the Exclude List**

Current (`custom_mod_window.cpp`, the Exclude List loop, same shape as Include's original):
```cpp
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

Replace with:
```cpp
	for (const auto &e : CustomSettings::GetActivityExclude()) {
		const auto peerId = e.first;
		const auto name = e.second;
		AddAvatarPeerRow(content, controller, peerId, name, [=] {
			CustomSettings::RemoveFromActivityExclude(peerId);
			Ui::Toast::Show(name + u" Exclude List'dan olib tashlandi."_q);
			if (onRebuild) onRebuild();
		});
		const auto historyRow = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(u"📜 Tarixni ko'rish — "_q + name),
				st::settingsButtonNoIcon));
		historyRow->addClickHandler([=] {
			if (!gInstance) return;
			gInstance->showBox(CustomActivityHistory::MakeHistoryBox(
				&controller->session(), peerId, name));
		});
	}
```

- [ ] **Step 5: Build**

Build the `Telegram` project in Visual Studio (Release x64). Expected: 0 errors.

- [ ] **Step 6: Manually verify**

Open the Custom Window → Peers tab → 🕒 Activity History section. Add 1-2 contacts to the Include List and 1 to the Exclude List. Confirm each now renders with a circular avatar (real photo or letter fallback), the contact's name, and "ID: <number>" — matching the visual style of the White/Black List rows above. Click a row's "Oʻchirish" button and confirm the entry disappears and a toast shows. Click "📜 Tarixni ko'rish" and confirm the History Viewer Box still opens correctly.

- [ ] **Step 7: Commit**

```bash
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "fix: render Activity History Include/Exclude rows with avatar+name+ID

Previously these rendered as plain-text buttons, unlike White/Black
List's rich row style. Extract that row pattern into a shared
AddAvatarPeerRow() helper and use it for both lists."
```

---

### Task 4: Fix Activity History data missing from backup Import

**Context:** `ExportFullBackup()` copies the whole DB file, so `activity_history` is already included in every export. `ImportFullBackup()` does NOT do a raw file replace — it `ATTACH`es the source DB and runs explicit per-table merge `INSERT` statements for only `actioned_messages` and `ghost_reads` (see `custom_db.cpp` around lines 1415-1439). `activity_history` has no merge branch at all, so it is silently dropped on every import. History rows are immutable point-in-time records (not a single mutable value like `ghost_reads`), so the correct merge semantic is **append-only, skip exact duplicates** — the same rule already used for `actioned_messages`.

**Files:**
- Modify: `Telegram/SourceFiles/custom_db.cpp`

- [ ] **Step 1: Add the merge branch**

Current (`custom_db.cpp`, inside `ImportFullBackup`, between the `actioned_messages` merge and `COMMIT`):
```cpp
        // ghost_reads is a genuine single-value-per-peer table (unlike
        // actioned_messages), so "newest timestamp wins" is the correct
        // merge rule here, not append.
        execSql(
            "INSERT OR REPLACE INTO main.ghost_reads (peer_id, msg_id, timestamp) "
            "SELECT imp.peer_id, imp.msg_id, imp.timestamp FROM import_db.ghost_reads imp "
            "WHERE NOT EXISTS ("
            "  SELECT 1 FROM main.ghost_reads cur "
            "  WHERE cur.peer_id = imp.peer_id AND cur.timestamp >= imp.timestamp"
            ")");
        execSql("COMMIT");
```

Replace with:
```cpp
        // ghost_reads is a genuine single-value-per-peer table (unlike
        // actioned_messages), so "newest timestamp wins" is the correct
        // merge rule here, not append.
        execSql(
            "INSERT OR REPLACE INTO main.ghost_reads (peer_id, msg_id, timestamp) "
            "SELECT imp.peer_id, imp.msg_id, imp.timestamp FROM import_db.ghost_reads imp "
            "WHERE NOT EXISTS ("
            "  SELECT 1 FROM main.ghost_reads cur "
            "  WHERE cur.peer_id = imp.peer_id AND cur.timestamp >= imp.timestamp"
            ")");
        // activity_history rows are immutable point-in-time records (like
        // actioned_messages, not like ghost_reads), so append-only merge is
        // correct here too — skip rows that already exist identically.
        // If importing an older backup predating this table, import_db
        // simply has no such table and this statement fails silently
        // (execSql only logs via qDebug, does not abort the transaction) —
        // same tolerance the ghost_reads/actioned_messages merges already have.
        execSql(
            "INSERT INTO main.activity_history "
            "(peer_id, field, old_value, new_value, observed_at) "
            "SELECT imp.peer_id, imp.field, imp.old_value, imp.new_value, imp.observed_at "
            "FROM import_db.activity_history imp "
            "WHERE NOT EXISTS ("
            "  SELECT 1 FROM main.activity_history cur "
            "  WHERE cur.peer_id = imp.peer_id AND cur.field = imp.field "
            "    AND cur.observed_at = imp.observed_at "
            "    AND IFNULL(cur.old_value,'') = IFNULL(imp.old_value,'') "
            "    AND cur.new_value = imp.new_value"
            ")");
        execSql("COMMIT");
```

- [ ] **Step 2: Build**

Build the `Telegram` project in Visual Studio (Release x64). Expected: 0 errors.

- [ ] **Step 3: Manually verify**

1. In the running app, add a contact to the Activity History Include List and let at least one real change (or restart the app once, which re-records the current baseline for online-status) get recorded — confirm via "📜 Tarixni ko'rish" that at least one entry exists.
2. Use the existing Backup/Export button in the Custom Window to export a full backup to a folder.
3. Close the app. Using any SQLite browser (e.g. "DB Browser for SQLite") or the `sqlite3` command-line tool, open `%APPDATA%\...\CustomMod\actioned_messages.db` (the live DB — confirm the exact path by checking what `dbFilePath()` returns in `custom_db.cpp`, it's a function in the same file) and run `DELETE FROM activity_history;` then close the browser/tool.
4. Reopen the app, open the same contact's History Viewer Box, and confirm it is now empty (proves the deletion worked and nothing auto-repopulates it).
5. Use the existing Backup/Import button, pick the backup folder from step 2, run the import (merge mode, not full-replace).
6. Reopen the same contact's History Viewer Box and confirm the entries from step 1 are back.

- [ ] **Step 4: Commit**

```bash
git add Telegram/SourceFiles/custom_db.cpp
git commit -m "fix: include activity_history in backup Import merge

ExportFullBackup already copied it via the whole-file copy, but
ImportFullBackup's per-table merge only covered actioned_messages and
ghost_reads, silently dropping activity_history on every import. Add
an append-only merge branch, matching actioned_messages' semantics
since history rows are immutable point-in-time records."
```

---

### Task 5: Add pruning/auto-cleanup for the `activity_history` table

**Context:** Every other growing table in this DB has an automatic prune: `PruneStaleGhostReads(days=30)` and `PruneStaleCachedText(days=30)`, both triggered by a static save-counter inside their respective `Save*` function (see `SaveGhostRead`, lines 364-370). `activity_history` has no such mechanism and will grow unbounded. Unlike `ghost_reads`/`text_cache` (which are short-lived caches), Activity History's entire purpose is **long-term** tracking, so a 30-day retention would defeat the feature — this plan uses **365 days** as the default retention window. This number is a judgment call, not a user-specified requirement — flag it to the project owner as adjustable.

**Files:**
- Modify: `Telegram/SourceFiles/custom_db.h`
- Modify: `Telegram/SourceFiles/custom_db.cpp`

- [ ] **Step 1: Add the declaration**

Current (`custom_db.h`, end of the "Activity History Log" section):
```cpp
// Shu peer uchun BARCHA maydonlar bo'yicha to'liq jurnal, eng yangisidan
// boshlab (observed_at DESC). History Viewer Box shu funksiyani ishlatadi.
[[nodiscard]] QVector<ActivityHistoryEntry> GetActivityHistory(
    const QString &peerId);

} // namespace CustomDB
```

Replace with:
```cpp
// Shu peer uchun BARCHA maydonlar bo'yicha to'liq jurnal, eng yangisidan
// boshlab (observed_at DESC). History Viewer Box shu funksiyani ishlatadi.
[[nodiscard]] QVector<ActivityHistoryEntry> GetActivityHistory(
    const QString &peerId);

// Delete activity_history entries older than |days| days (default 365 —
// this table is a long-term history log, unlike ghost_reads/text_cache's
// 30-day caches, so a much longer retention is used by default). Called
// automatically by SaveActivityHistoryEntry(); safe to call manually.
void PruneStaleActivityHistory(int days = 365);

} // namespace CustomDB
```

- [ ] **Step 2: Implement it**

Add this new function immediately after `SaveActivityHistoryEntry`'s closing brace in `custom_db.cpp` (search for `void SaveActivityHistoryEntry(`, find its matching closing `}`, insert right after):

```cpp
// Delete activity_history entries older than |days| days. Same pattern as
// PruneStaleGhostReads, but observed_at is a unix-timestamp INTEGER column
// (not a formatted TEXT timestamp like ghost_reads.timestamp), so the
// cutoff is computed and bound as an int64 instead of a string.
void PruneStaleActivityHistory(int days) {
    Init();
    if (!gDb) return;

    const qint64 cutoff =
        QDateTime::currentDateTime().addDays(-days).toSecsSinceEpoch();
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(gDb,
            "DELETE FROM activity_history WHERE observed_at < ?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, cutoff);
        sqlite3_step(stmt);
        const int removed = sqlite3_changes(gDb);
        sqlite3_finalize(stmt);
        if (removed > 0) {
            qDebug() << "PruneStaleActivityHistory: removed" << removed
                     << "entries older than" << days << "days.";
        }
    }
}
```

- [ ] **Step 3: Trigger it periodically from `SaveActivityHistoryEntry`**

Current (`custom_db.cpp`, the start of `SaveActivityHistoryEntry`, already modified by Task 2 if that task ran — apply this on top of whichever version is currently in the file):
```cpp
void SaveActivityHistoryEntry(
        const QString &peerId,
        const QString &field,
        bool hasOldValue,
        const QString &oldValue,
        const QString &newValue,
        qint64 observedAt) {
    Init();
    if (!gDb) return;
```

Replace with:
```cpp
void SaveActivityHistoryEntry(
        const QString &peerId,
        const QString &field,
        bool hasOldValue,
        const QString &oldValue,
        const QString &newValue,
        qint64 observedAt) {
    Init();
    // Prune once every 50 saves — same pattern as SaveGhostRead/CacheMessageText.
    static int sActivitySaveCount = 0;
    if (++sActivitySaveCount % 50 == 0) {
        PruneStaleActivityHistory(365);
    }
    if (!gDb) return;
```

- [ ] **Step 4: Build**

Build the `Telegram` project in Visual Studio (Release x64). Expected: 0 errors.

- [ ] **Step 5: Verify by reading, not by waiting**

There is no UI trigger for this (matches the user's ask, which was for automatic cleanup, not a manual button) and waiting for 50 real saves to happen naturally isn't practical to verify by hand. Instead: re-read the diff for `PruneStaleActivityHistory` and confirm the SQL and the `qint64`/`toSecsSinceEpoch()` cutoff match `ActivityHistoryEntry::observedAt`'s type and `SaveActivityHistoryEntry`'s `sqlite3_bind_int64(stmt, 5, observedAt)` binding (both `custom_db.h`/`custom_db.cpp`) — i.e. confirm the column really is an integer unix timestamp, not a formatted string, so `PruneStaleGhostReads`'s string-based cutoff pattern was correctly NOT copied verbatim.

- [ ] **Step 6: Commit**

```bash
git add Telegram/SourceFiles/custom_db.h Telegram/SourceFiles/custom_db.cpp
git commit -m "feat: add automatic pruning for activity_history table

Every other growing CustomMod table (ghost_reads, text_cache) already
auto-prunes; activity_history had none and would grow unbounded.
Add PruneStaleActivityHistory(days=365) triggered every 50 saves,
same pattern as PruneStaleGhostReads/PruneStaleCachedText. 365 days
(not 30) because this table is a long-term history log by design,
not a short-lived cache — adjust the default if that's wrong."
```

---

### Task 6: Log-noise research finding (no code change)

**Context:** The user's `log.txt` was dominated by two repeated lines and asked whether they could be reduced. This task documents the finding — no code should be changed.

**Finding:** Both lines come from **official upstream Telegram Desktop code**, not from any CustomMod addition:
- `Telegram/SourceFiles/data/data_session.cpp:966` — `LOG(("API Warning: not loaded minimal channel applied."));` — fires whenever a "minimal" (partial/stub) channel object arrives from the server before its full data has been loaded locally. This is routine, expected behavior any time channels are referenced before being fully fetched (e.g. scrolling chat lists, forwarding, lazy-loading) — not a bug, not something this fork introduced.
- `Telegram/SourceFiles/history/history.cpp:3543` (`History::unknownMessageDeleted`) — fires when a delete-message update references a message ID the local session hasn't loaded yet. Also routine official behavior.

Both use the official `LOG(...)` macro (`Telegram/lib_base/base/debug_log.h:23`), which writes to the main log file **unconditionally** (no `#ifdef DEBUG` guard) — so this is not a "Debug build verbosity" artifact either, it's just how upstream Telegram Desktop always logs these two conditions.

**Recommendation: do not modify either line.** Reasons: (1) they are core protocol-handling code shared byte-for-byte with upstream, unrelated to any CustomMod feature; (2) silencing a real (if usually harmless) protocol warning risks hiding a genuinely broken state in some future edge case; (3) patching official files here adds unnecessary merge-conflict risk for the pending v7.0.2 upstream sync (Task 8 in the research doc, deferred). If `log.txt` size/readability is the real concern, the practical fix is log **rotation/retention** (keep last N lines or last N MB) at the log-writer level, not suppressing specific warnings — but no such request was made, so this plan does not add one either.

- [ ] **Step 1: No action required.** Mark this task complete after reading the finding above — there is nothing to build, verify, or commit.

---

### Task 7: Full manual regression pass

**Files:** none (verification only)

- [ ] **Step 1: Crash fix confirmation** (already committed separately as `0515f02c0f`, not part of this plan's diff) — open a contact's profile page and click "📜 Faollik tarixi". Confirm the app does NOT crash and the History Viewer Box opens.
- [ ] **Step 2: Performance** — if Task 2 ran, confirm first-start-after-logout feels back to its ~7-8s baseline (see Task 2 Step 8). If Task 2 did NOT run (Task 1 ruled out the hypothesis), skip this and note that the regression remains unexplained.
- [ ] **Step 3: Include/Exclude UI** — confirm rows show avatar+name+ID and the delete/history buttons both work (see Task 3 Step 6).
- [ ] **Step 4: Backup/Import** — confirm the full export→wipe→import cycle from Task 4 Step 3 restores Activity History entries correctly.
- [ ] **Step 5: Build one final time** in Visual Studio (Release x64), confirm 0 errors, 0 new warnings introduced by this plan's changes.
- [ ] **Step 6: Report back** to the project owner with: which tasks ran, the Task 1/2 timing numbers either way, and any issues found during Step 1-4 that weren't expected.
