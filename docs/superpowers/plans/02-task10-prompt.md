Execute the task described below. It is an implementation job in an
existing C++/Qt repository — not a document to review, summarise or
score. The only output that counts is committed source code.

REPOSITORY
  C:\TBuild\tdesktop
  Branch: Oybek. Never push to a remote named "upstream".

HOW TO REPORT
  Work quietly. Do not narrate steps. Do not paste build logs.
  The final report must be SHORT — the seven points at the bottom.

STEP 0 — BEFORE YOU WRITE ANYTHING
  1. Read `Telegram/SourceFiles/custom_tab_system.cpp` end to end. It is
     the model for what you are writing: same file shape, same helpers,
     same visual vocabulary. Copy its idioms rather than inventing.
  2. Read `custom_tab_common.h` — the shared includes and the
     `fill*Tab` declarations near line 140.
  3. Read `custom_mod_window.cpp` lines 20–60 and 88–152: the two
     `std::array<..., 7>` members, the `CustomTabBar` name list, and
     the `fill*Tab(makeInner(N), ...)` calls.
  4. Read `custom_sync.h` — `SyncStatus`, `Start/Stop/SyncNow`,
     `GetOrchestrator()` and the `statusChanged` signal.
  5. Read `Client::enroll` in `custom_sync_client.cpp` and
     `Outbox::EnsureMasterKeyCreated` in `custom_sync_outbox.cpp`.
  6. Read the sync fields in `custom_settings.h` (around line 72) and
     how a neighbouring tab writes settings.
  7. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K5  Sync off => ZERO behaviour change.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: UI text and code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop. Do NOT run any server.
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real database.

⚠️ THIS TASK IS THE EXCEPTION ON BUILDS. Every earlier task banned the
full build. This one requires it — see the verification section. It
takes ~34 minutes and competes for RAM; do not start anything else
heavy while it runs, and do not touch the `cmake` submodule, which
carries a deliberate local `/MP6` change.

============================================================
YOUR TASK — PLAN 02, TASK 10
============================================================

Add a "Sinxronizatsiya" tab to the CustomMod window so the feature can
actually be turned on, enrolled, watched and diagnosed.

Right now sync is fully implemented and completely unreachable:
`syncEnabled` defaults to false and nothing in the UI can change it.

Files:
  - Create `Telegram/SourceFiles/custom_tab_sync.cpp`
  - Declare `fillSyncTab(...)` in `custom_tab_common.h`
  - Register the tab in `custom_mod_window.cpp`
  - Add the .cpp to `Telegram/CMakeLists.txt` (bare filename)

The window currently has 7 tabs (indices 0–6). You are adding index 7.
That means **three** edits in `custom_mod_window.cpp`: both
`std::array<..., 7>` become 8, the `CustomTabBar` name list gets an
entry, and a `fillSyncTab(makeInner(7), ...)` call joins the others.
Miss one of the array sizes and you get an out-of-bounds write that
will not always crash immediately.

------------------------------------------------------------
🔴 1. A SECOND DEVICE GENERATES A DIFFERENT KEY — SAY SO
------------------------------------------------------------

This is the most important thing in this task.

`Outbox::EnsureMasterKeyCreated()` makes a **random** master key and
protects it with DPAPI. There is no passphrase, no escrow, no key
sharing — that work is deliberately deferred.

So if the user enrols a second device against the same account, that
device creates its **own, different** master key. Everything then goes
wrong quietly:

  - its `peer_hash` values differ, so its `record_id` values differ:
    nothing dedups against the first device's records
  - records pulled from the first device fail to decrypt, and Task 7b
    counts every one of them as rejected or corrupt

Nothing crashes. The user sees a green "connected" state and silently
gets two disconnected archives.

**The UI must not let that happen unnoticed.** Before enrolling, show a
confirmation box (`Ui::ConfirmBox`, the pattern is in the neighbouring
tabs) stating plainly that key sharing between devices is not
implemented yet and this device will keep its own archive. Require the
user to confirm.

And surface the symptom afterwards: the rejected/corrupt counters
already exist (`sync_state` key `corrupt_records`, and
`pullAndMerge`'s `rejected` count). If corrupt records are being
recorded, show a visible warning row — "N ta yozuv deshifrlanmadi,
kalit mos kelmasligi mumkin" — not a number buried in a debug log.

Do NOT implement passphrase wrapping to fix this. It is a protocol
feature, not a UI one.

------------------------------------------------------------
🔴 2. PBKDF2 AND THE UI THREAD
------------------------------------------------------------

`Crypto::Pbkdf2` runs 600 000 or 2 000 000 iterations — seconds of
solid CPU. Called from a button handler it freezes the message pump and
Windows paints "Not Responding" over the app.

In the current code nothing on the enrol path calls it — the master key
is random bytes plus DPAPI — so **if you write this tab as specified,
you will not touch PBKDF2 at all.** That is the correct outcome.

The rule matters for whoever adds passphrase support later: that call
belongs in `crl::async` with the result posted back through
`crl::on_main`, the way `ExportFullBackupAsync` already does it. If you
find yourself calling `Pbkdf2` anywhere in this task, stop — you have
drifted out of scope.

Everything else here is cheap and belongs on the main thread, where the
orchestrator already lives.

------------------------------------------------------------
3. WHAT THE TAB CONTAINS
------------------------------------------------------------

**Sozlamalar**

  - Toggle: "Sinxronizatsiya yoqilgan" -> `syncEnabled`. On enable call
    `CustomSync::Start()`, on disable `CustomSync::Stop()`. Both are
    idempotent.
  - Input: server URL -> `syncServerUrl`. Trim it. Reject a value that
    is not `http://` or `https://` with a toast rather than storing
    something the client will silently fail on.
  - Number input: interval seconds -> `syncIntervalSeconds`. The
    orchestrator re-reads this at every rearm, so no restart is needed.
    Clamp to a sane range and say what the range is in the label.

🔴 **K5:** with the toggle off, nothing on this tab may touch the
network, and the timer must not be armed. Do not call `SyncNow()` or
`enroll` from any code path that runs while sync is disabled.

**Ro'yxatdan o'tish (enrollment)**

Two inputs — one-time code and device name — and a button that calls
`Client::enroll(serverUrl, code, deviceName, done)` after the
confirmation box from section 1.

The button must be disabled while a request is in flight, and the
result must be reported: success shows the device id, failure shows the
server's error text verbatim, not a generic "xatolik".

Show enrolment state by reading `Outbox::GetState("device_id")` — empty
means not enrolled. Hide or disable push/pull controls until it is set.

**Holat (status)**

Subscribe to `Orchestrator::statusChanged` and render `SyncStatus`:

  - `lastSuccessAt` as a readable local time, or "hali muvaffaqiyat
    bo'lmagan" when 0
  - `pendingCount` — records waiting in the outbox
  - `consecutiveFailures` and `lastError` — show the error text, and
    say the retry is backing off rather than leaving it looking stuck
  - `inFlight` — a simple "sinxronlanmoqda…" indicator

Also a "Hozir sinxronlash" button calling `CustomSync::SyncNow()`. It
is a no-op while a cycle is in flight; reflect that in the button's
enabled state instead of letting the click do nothing silently.

`GetOrchestrator()` returns null when `Start()` has not run. Handle
that — do not dereference it blind.

**Diagnostika**

  - The corrupt-records warning from section 1.
  - A read-only line showing the pull cursor (`sync_state` key
    `pull_cursor`) — it is the fastest way to tell "never synced" from
    "synced and idle" when something looks wrong.

------------------------------------------------------------
4. THINGS NOT TO ADD
------------------------------------------------------------

  - **No "qurilmani ajratish" (unenrol) button.** Clearing `device_id`
    and `refresh_token` is easy; the danger is that the obvious
    implementation also clears `master_key_protected`, and then every
    record already on the server becomes permanently undecryptable. If
    you think the tab needs it, say so in your report and leave it out.
  - No passphrase, escrow or key export.
  - No media upload/download controls.
  - No WebSocket toggle (Task 9 is not written yet).

------------------------------------------------------------
HOW TO VERIFY
------------------------------------------------------------

`/Zs` does not work on this file — `custom_tab_common.h` pulls in most
of tdesktop's header tree, the same reason `custom_db.cpp` was never
syntax-checked that way. So this task needs the real build.

  1. Run the existing selftest first and confirm it is green (exit 0)
     **before** you change anything, so a later failure is
     attributable.
  2. Make your changes.
  3. Full build:
     `cmake --build C:/TBuild/tdesktop/out --config Release`
     It takes ~34 minutes. It must finish with zero errors. Report the
     first error verbatim if it does not, and stop.
  4. Confirm `Telegram.exe` was relinked (its timestamp is newer than
     the objects) — a build that skips the link step because of an
     earlier failure can otherwise look like success.
  5. Run the selftest again — it must still exit 0.

You cannot run tdesktop, so the tab is not visually verified. That is
accepted. In exchange, be conservative: copy layout idioms from
`custom_tab_system.cpp` exactly rather than inventing new widget
arrangements that nobody will look at before the user does.

============================================================
DEFINITION OF DONE
============================================================

  - `custom_tab_sync.cpp` created, `fillSyncTab` declared in
    `custom_tab_common.h`, registered in CMakeLists
  - `custom_mod_window.cpp`: **both** arrays sized 8, tab name added,
    `fillSyncTab(makeInner(7), ...)` called
  - Enable/disable toggle wired to `Start()` / `Stop()`
  - Server URL validated; interval clamped and labelled
  - Enrolment behind a confirmation box that states the second-device
    key problem in plain Uzbek
  - Status rendered from `statusChanged`; null orchestrator handled
  - Corrupt-record count surfaced as a visible warning
  - `Pbkdf2` not called anywhere in this task
  - No unenrol button
  - Full build clean; `Telegram.exe` relinked; selftest exit 0
  - One commit, K7 style

============================================================
FINAL REPORT — seven short points
============================================================

  1. Selftest exit code before and after your change.
  2. Full build result, and the `Telegram.exe` timestamp versus the
     object timestamps.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. The exact Uzbek wording of your second-device confirmation box.
  5. What the tab shows when `GetOrchestrator()` is null.
  6. Confirm `Pbkdf2` appears nowhere in your diff.
  7. Anything ambiguous you had to guess at, and whether you think the
     unenrol button should exist.

OUT OF SCOPE
  - No WebSocket (Task 9), no regression sweep (Task 11).
  - Do not enable sync by default or change any default value.
  - Do not change `occurred_at` at any enqueue site.
  - Do not modify the orchestrator's scheduling logic.
