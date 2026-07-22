# Manual Test Findings (2026-07-22) — Research Notes & Task List

**Date:** 2026-07-22
**Status:** Research recorded. NOT authorized for implementation yet — user must approve each item before work starts (they may need to shut their laptop down).
**Source:** User's manual regression pass over Activity History Log (Task 7 of `2026-07-20-activity-history-log-plan.md`) surfaced these items in one batch.

---

## 1. Performance regression: first-start-after-logout time (7-8s → 11-12s)

**Report:** After a prior optimization pass, first-start (after logout, pressing Enter to login) took ~7-8s to reach a fully usable state. It has regressed to ~11-12s. Network conditions are unchanged/still good.

**Status:** Root cause NOT verified. An unverified hypothesis exists — `CustomActivityHistory::Init(session)` (added this session for the Activity History Log feature, hooked in `main_session.cpp`) subscribes to `session().changes().peerUpdates(...)`, which could in theory fire once per changed field per contact during the initial post-login full-contact-list sync, and `SaveActivityHistoryEntry` hits SQLite synchronously each time. **This has not been confirmed against actual code** — need to check whether `Data::Session`'s initial sync path actually fires bulk `Name|Username|Photo|OnlineStatus` peerUpdates, and whether `custom_activity_history.cpp`'s handler runs synchronously on that path.

**Next steps (not yet done):** profile/trace first-start with and without the Activity History capture module active; check if `SaveActivityHistoryEntry` is called during initial sync and how many times; check `custom_db.cpp` SQLite write pattern (transaction batching, `PRAGMA synchronous`, etc.) for other Save* functions already tuned for this concern.

**Task:** #2 in task list.

---

## 2. Offline-first startup (Unigram-style) — full research + design needed

**Report:** User wants the app to open fully into the last-known state with **zero internet connection**, the way Unigram does (user has personally verified this works in Unigram). The project already maintains an "Offline Database" for other CustomMod features; user believes it could be extended to serve as the source of truth for the initial UI render, decoupling first-paint from network availability.

**Explicitly requested by user:** `research + brainstorm + superpowers qilib ko'rish kerak hamda to'liq implement qilish kerak` — this is NOT a quick fix. Must go through `superpowers:brainstorming` → spec → `superpowers:writing-plans` → plan → (likely) `superpowers:subagent-driven-development`, same as the Activity History Log feature.

**Status:** Not started. This is the largest item in this batch — should probably be its own dedicated session/branch of work once approved, kept separate from the smaller bug fixes below.

**Task:** #3 in task list.

---

## 3. Include/Exclude list rows: plain text instead of rich avatar+name+ID UI

**Report (screenshot-confirmed):** In CustomWindow → Peers tab → "🕒 Activity History" section, Include/Exclude List rows render as plain text buttons (e.g. `➖ Muslimaxon Maxmudjonova`), unlike the White/Black List and Per-Chat sections in the same tab, which show a circular avatar, name, and peer ID (e.g. "Akam | Home", ID: 1334067829).

**Root cause (confirmed by reading code):** `custom_mod_window.cpp`'s `fillActivityHistorySection` builds Include/Exclude rows as plain `Ui::SettingsButton` text rows. The rich pattern already exists elsewhere in the same file — `fillPeerSection`'s `state->addEntry` lambda (White/Black List row builder, ~lines 1138-1235) — and should be replicated:

- Row container: fixed-height (`kRowH = 56`) custom `Ui::RpWidget`, not a `Ui::SettingsButton`.
- Avatar: `Ui::CreateChild<Ui::RpWidget>(row)`, fixed size `kAvSize = 38`, painted via `PaintPeerAvatar(p, rect, peerId, name, session, userpicView)` — draws the real cached userpic if available, else a letter+color fallback. Needs a `std::make_shared<Ui::PeerUserpicView>()` kept alive per-row.
- Name label: `Ui::CreateChild<Ui::FlatLabel>(row, rpl::single(name.isEmpty() ? peerId : name), st::boxLabel)`.
- ID label: `Ui::CreateChild<Ui::FlatLabel>(row, rpl::single(u"ID: "_q + peerId), st::customModHintLabel)`.
- Delete button: `Ui::CreateChild<Ui::RoundButton>(row, rpl::single(u"Oʻchirish"_q), st::attentionBoxButton)`, fixed width `kDelBtnW = 76`.
- All child widgets need explicit `->show()` — Qt does not auto-show new children of an already-visible parent.
- Bottom separator painted manually via `row->paintRequest()` + `p.fillRect(...)`.
- Layout driven by a `layoutRow(int w)` lambda bound to `row->widthValue() | rpl::on_next(...)`, plus an immediate manual call if the parent already has a nonzero width (subscriptions don't fire retroactively for `w=0` at construction time).
- Delete button click handler calls `CustomSettings::RemoveFromWhitelist/Blocklist` (for the reference pattern) — for Include/Exclude this would call the equivalent Include/Exclude removal functions in `custom_settings.cpp`.

**Next steps (not yet done):** extract/refactor this row-builder into a shared helper (used by 3 call sites now: Whitelist, Blocklist, Include, Exclude) rather than copy-pasting a 4th time, OR duplicate it minimally for Include/Exclude first and refactor later — decision needed at implementation time.

**Task:** #4 in task list.

---

## 4. Activity History not covered by Backup Import; needs pruning/auto-cleanup

**Report:** Two related questions: (a) is Activity History data included in Backup (Import/Export)? (b) DB needs manual or automatic pruning or it will grow unbounded and hang the app.

**4a. Backup/Export — confirmed gap (read `custom_db.cpp` in full):**
- All CustomMod tables (`schema_version`, `ghost_reads`, `actioned_messages`, `text_cache`, `activity_history`) live in ONE SQLite file (`dbFilePath()`).
- `ExportFullBackup()` does a raw whole-file copy — `activity_history` **is** included in exports automatically, no gap there.
- `ImportFullBackup()` does **NOT** do a raw file replace by default. It `ATTACH`es the source DB and runs explicit per-table `INSERT ... WHERE NOT EXISTS` merge statements for exactly two tables: `actioned_messages` (append-only merge) and `ghost_reads` (newest-timestamp-wins merge). **`activity_history` is not in this list — it is silently NOT restored/merged on import**, whether the import is a merge-import or `fullReplace`. (`text_cache` is also intentionally excluded, presumably as designed since it's just a cache — but `activity_history` is real user-facing data and this exclusion looks unintentional.)
- **Fix needed:** add an explicit merge branch for `activity_history` in `ImportFullBackup()`. Suggest a newest-`observedAt`-wins merge per `(peerId, field, observedAt)` or straightforward `INSERT ... WHERE NOT EXISTS` (append-only, same as `actioned_messages`) since history entries are immutable point-in-time records, not mutable state like `ghost_reads` — append-only merge is likely the correct semantic (no "wins" conflict makes sense for a log of historical values).

**4b. Pruning — not implemented yet, established pattern to follow:**
- `custom_db.cpp` already has `PruneStaleGhostReads(int days = 30)` and `PruneStaleCachedText(int days = 30)`, each triggered periodically via a save-counter check inside the corresponding `Save*` function, e.g. `SaveGhostRead`'s `if (++sSaveCount % 50 == 0) PruneStaleGhostReads(30);`.
- **Fix needed:** add `PruneStaleActivityHistory(int days = N)` following the same shape, triggered from `SaveActivityHistoryEntry`. Retention window (30 days? longer, since this is a "history log" whose whole point is long-term tracking?) needs a decision — flagging this as a question for the user rather than assuming 30 days is right for this specific table.
- Consider also exposing a manual "tozalash" (clear) button in the Peers tab's Activity History section, similar to the "Barchasini tozalash" pattern already used for Whitelist/Blocklist in the same file.

**Task:** #5 (backup fix) and #6 (pruning) in task list.

---

## 5. CRASH on "Faollik tarixi" profile button — ROOT-CAUSED AND FIXED

**Report:** Clicking "Faollik tarixi" on a contact's profile page crashed the app completely. User attached `log.txt` (1134 lines) and said it's fine to also clean up/reduce excessive logging.

**Investigation:**
- Read `log.txt` in full — contains only normal startup/RPC-warning noise (`API Warning: not loaded minimal channel applied.`, `History::unknownMessageDeleted`), ends abruptly mid-line with **no crash/exception trace at all**. No crash-dump file exists anywhere in `out/Release` or `out/Release/tdata` for this portable dev build (an unrelated official Telegram Desktop install's dump folder in `AppData/Roaming` was found and correctly excluded as not belonging to this build).
- Since no direct evidence (dump/trace) was available, root cause was found via **static pattern comparison** (`superpowers:systematic-debugging` Phase 2): compared the crashing method `ActionsFiller::addActivityHistoryAction` (`info_profile_actions.cpp`) against its known-working sibling `addBlockAction`.
- **Root cause:** `addActivityHistoryAction`'s button click lambda referenced the member `_controller` directly (`_controller->parentController()->show(...)`). Under C++ `[=]` capture semantics, referencing a class member captures `this` (the `ActionsFiller*`), **not** a value-copy of the member. `ActionsFiller` is a short-lived stack-local object inside `SetupActions()`, destroyed once that function returns — long before the user actually clicks the button. The lambda therefore dereferences a dangling `this` when invoked: a classic use-after-free.
- `addBlockAction` avoids this correctly by copying `_controller->parentController()` into a **local variable** before constructing the lambda, and having the lambda capture/use that local instead of touching the member.

**Fix applied:** copied the pattern — `const auto controller = _controller->parentController();` added before the `AddActionButton(...)` call, lambda body changed from `_controller->parentController()->show(...)` to `controller->show(...)`.

**Status:** Fix applied and **committed** (`0515f02c0f` on `Oybek`). **Push failed** — this environment currently has no network route to github.com (`Could not resolve host`); push should be retried once connectivity is available.

**Caveat:** this diagnosis rests entirely on static code-pattern analysis, not a live debugger/crash-dump confirmation, since no dump was available. The reasoning is strong and well-precedented (matches a known-working sibling exactly) but should be flagged as such — ask the user to re-test the profile button after the next build to confirm the crash is actually gone.

**Task:** #1 in task list (marked essentially done, pending user's build confirmation).

---

## 5b. Log noise — not yet investigated

**Report:** User said it's fine to clean up `log.txt` and reduce excessive logging if a source can be found.

**Status:** Not started. `log.txt` is dominated by repeated `API Warning: not loaded minimal channel applied.` and `History::unknownMessageDeleted` lines. Need to grep the codebase for these exact log strings, determine why they fire so frequently, and decide whether reducing verbosity is safe (i.e. not masking something the log was meant to surface).

**Task:** #7 in task list.

---

## 6. Upstream v7.0.2 sync (deferred until items above are fixed)

**Report:** Official upstream v7.0.2 contains a fix for the "Two finger swipe to reply" bug this project previously could not fix itself. User wants this synced in, explicitly **after** the bugs above are resolved — similar in nature to the prior 552-commit upstream sync (`2026-07-11-upstream-sync-552-commits.md`).

**Status:** Not started — correctly deferred per user's own sequencing. Must follow the durable project rule: merge into `Oybek` only, **never** push or PR to the `upstream` remote.

**Task:** #8 in task list.

---

## 7. Research question: native "Check for updates" — separate report requested

**Report:** User asked, as a standalone research question (explicitly requested to be answered in a **separate message**, not bundled with fixes): official tdesktop has Settings → Advanced → "Check for updates" (with an "update automatically" option) that lets the app check its own update feed and self-update. Why doesn't this feature work/appear in this custom fork, and could it be enabled?

**Status:** Not started. Needs investigation into `Telegram/SourceFiles/core/update_checker.*` (or equivalent), relevant build flags (e.g. `TDESKTOP_DISABLE_AUTOUPDATE`, `OFFICIAL_TARGET`), whether this custom build's CMake/build config defines/undefines them, and — separately from "can it be turned on" — whether it's even sound for a fork to point at Telegram's own official update feed (signing mismatch, update-feed ownership are likely blockers). To be reported to the user as its own message per their explicit request.

**Task:** #9 in task list.

---

## Governing constraint

Per the user's explicit instruction: **do not implement anything from this list beyond what was needed to diagnose the already-reported crash** (item 5, already fixed and committed) **without asking first** — the user may need to shut their laptop down and wants to approve each piece of work before it starts.
