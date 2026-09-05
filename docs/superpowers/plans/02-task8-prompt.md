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
  1. Read `custom_sync_client.h` in full — especially the comment
     above `class Client` about which thread `QNetworkAccessManager`
     lives on.
  2. Read `Client::pushPending` and `Client::pullAndMerge` in
     `custom_sync_client.cpp`. Both are already asynchronous: they
     return immediately and their callbacks fire later.
  3. Read `Outbox::Pending`, `MarkSent`, `MarkFailed` and
     `PendingCount` in `custom_sync_outbox.cpp`.
  4. Read the sync fields in `custom_settings.h` (around line 72) and
     the accessors below them.
  5. Read the plan section:
     `docs/superpowers/plans/2026-07-29-multi-device-sync-02-tdesktop-agent.md`,
     "## Task 8: Orkestrator".
  6. Read `tools/sync-selftest/README.md` and
     `tools/sync-selftest/CMakeLists.txt`.
  7. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K5  Sync off => ZERO behaviour change.
  K6  TDD: write the policy test first, watch it fail, then implement.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop or any server.
  Do NOT start a full tdesktop build (~34 minutes).
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real database.

============================================================
YOUR TASK — PLAN 02, TASK 8
============================================================

Create `Telegram/SourceFiles/custom_sync.h` / `.cpp`: the orchestrator
that decides when to push and pull, plus the status it exposes for the
Task 10 UI.

------------------------------------------------------------
🔴 1. DO NOT PUT THIS ON A BACKGROUND THREAD
------------------------------------------------------------

The plan says to run the loop inside `crl::async`. **Do not.** It was
written before the client existed and it is wrong for three reasons.

**It buys nothing.** `Client::push` and `Client::pull` are already
non-blocking — they hand the request to `QNetworkAccessManager` and
return. Nothing in a tick blocks: encrypting a few kilobytes with
AES-GCM and reading a handful of SQLite rows is microseconds, not the
kind of work `ExportFullBackupAsync` was built for (that one walks the
whole archive and copies files).

**It breaks `QNetworkAccessManager`.** It is not thread-safe and lives
on the GUI thread — `custom_sync_client.h` says so in its own comment.
Calling `push()` from a `crl::async` lambda is undefined behaviour that
will usually look like a request that silently never fires.

**It introduces a data race.** Merge writes through `custom_db.cpp`,
whose `gPendingWrites` buffer (line 87) is guarded by **no mutex at
all** — `gCacheMutex` covers the caches, not that vector. Today merge
runs in a network callback on the GUI thread, the same thread as
capture, so there is no race. Move the loop to a worker and there is.

So: a `base::Timer` (or `QTimer`) on the main thread. Callbacks land on
the main thread too, so the whole loop is single-threaded and needs no
locking.

⚠️ One real exception, out of scope but worth knowing: PBKDF2 at
600 000 / 2 000 000 iterations takes seconds. It runs during enrollment
and unlock, never in a tick. If Task 10 calls it straight from a
button handler the UI will freeze — that is the place that genuinely
needs `crl::async`. Mention it in your report; do not fix it here.

------------------------------------------------------------
🔴 2. TICKS MUST NOT OVERLAP
------------------------------------------------------------

`Outbox::Pending()` selects rows whose `next_retry_at` has passed.
Nothing marks a row as in flight. If a tick fires while the previous
push is still waiting on the network, the second tick selects **the
same rows** and sends them again.

The server dedups on `record_id`, so no data is corrupted — but the
device uploads everything twice, and every log and counter lies about
it.

Keep an `_inFlight` flag, set before starting a cycle and cleared in
the completion callback **on every path, including errors and early
returns**. A tick that finds the flag set does nothing and waits for
the next one. A leaked flag stops sync permanently and looks like a
dead network, so make sure there is exactly one place that clears it.

------------------------------------------------------------
3. WHAT ONE CYCLE DOES
------------------------------------------------------------

Push first, then pull. Pushing first means a record this device just
captured reaches the server before we ask what changed, which keeps
`observed_at` as close to the real observation as possible (spec §3.4:
the smallest `observed_at` wins).

    pushPending  ->  pullAndMerge  ->  update status, clear _inFlight

If push fails, still attempt the pull — a broken upload should not
block downloads.

**`hasMore`:** `pullAndMerge` handles exactly one batch. When the
server says there is more, schedule the next cycle immediately rather
than after the full interval, but cap consecutive fast cycles (a
`kMaxCatchUpCycles`-style bound) so a large backlog cannot starve the
UI. After the cap, fall back to the normal interval.

**Interval** comes from `CustomSettings::SyncIntervalSeconds()`. Read
it every time you rearm, not once at startup, so changing the setting
takes effect without a restart.

------------------------------------------------------------
4. FAILURE BACKOFF — SEPARATE FROM THE PER-RECORD ONE
------------------------------------------------------------

`Outbox::MarkFailed` backs off individual records. That does nothing
for a server that is down: the timer still fires every 30 seconds and
every cycle fails the same way.

Add a cycle-level backoff: on consecutive failures multiply the delay
(same shape as the outbox — double each time, ceiling at 5 minutes),
and reset it to the normal interval on the first success. Keep the
consecutive-failure count and the last error in the status.

------------------------------------------------------------
5. LIFECYCLE AND STATUS
------------------------------------------------------------

    namespace CustomSync {
    void Start();     // ilova ishga tushganda bir marta
    void Stop();      // yopilishda
    void SyncNow();   // qo'lda, UI tugmasidan (Task 10)
    }

`Start()` must be idempotent and must create exactly one orchestrator
for the whole application — not one per account. tdesktop creates and
destroys `Main::Session` objects on account switches; a per-session
orchestrator would multiply. Find where the other CustomMod startup
work happens (`main/main_session.cpp` around line 215 calls
`CustomDB::PruneStaleActivityHistory()`) and place the call so it runs
once; say in your report where you put it and why it cannot run twice.

🔴 **K5:** when `CustomSettings::SyncEnabled()` is false the timer must
never be armed and no network call may happen. Toggling the setting on
starts it; toggling it off stops it. Check the flag at arm time, not
only at `Start()`, or enabling sync would require a restart.

`SyncNow()` runs a cycle immediately unless one is in flight. It must
work even when the interval timer is armed — do not let it queue a
second concurrent cycle.

Status to expose (Task 10 renders it, do not build UI here):

    lastSuccessAt (qint64), pendingCount (int), lastError (QString),
    consecutiveFailures (int), inFlight (bool)

Use Qt signals on the orchestrator's QObject to notify changes.
`pendingCount` comes from `Outbox::PendingCount()`.

------------------------------------------------------------
🔴 6. HOW TO VERIFY — MAKE THE POLICY TESTABLE
------------------------------------------------------------

You cannot run tdesktop, and a timer-driven class that touches the
network is untestable as written. So do not write it that way.

Extract the decision into a **pure function** with no timer, no
network and no database:

    struct SchedulerState {
        bool enabled = false;
        bool inFlight = false;
        int consecutiveFailures = 0;
        bool hasMore = false;
        int catchUpCycles = 0;
        int intervalSeconds = 0;
    };

    struct SchedulerDecision {
        bool runNow = false;
        int delaySeconds = 0;   // runNow false bo'lsa qayta qurollash
    };

    [[nodiscard]] SchedulerDecision NextAction(const SchedulerState &s);

Put it in `custom_sync.h/.cpp` with no dependency on `Client`,
`custom_db` or Qt beyond QtCore, add `custom_sync.cpp` to
`tools/sync-selftest/CMakeLists.txt`, and test it in `main.cpp` in the
established style — a count guard, and a per-family result line.

At least these cases, and assert the exact numbers:

  1. disabled -> never runs
  2. inFlight -> never runs, whatever else is true
  3. clean state -> runs after `intervalSeconds`
  4. `hasMore` and under the cap -> runs immediately
  5. `hasMore` and at the cap -> falls back to `intervalSeconds`
  6. failures 1,2,3 -> the delay doubles
  7. failures large -> capped at 300 seconds
  8. failure count reset -> back to `intervalSeconds`

Then break your implementation deliberately (for example remove the
`inFlight` check) and confirm the test FAILS before restoring it. Say
in your report which break you used and which case caught it.

The timer wiring around `NextAction` stays untested — that is the
accepted trade. Keep that wiring as thin as you can so there is little
left to be wrong.

**Syntax check.** `/Zs` at `/W4 /WX` on every .cpp you changed, using
the absolute include paths in the selftest README.

============================================================
DEFINITION OF DONE
============================================================

  - `custom_sync.h/.cpp` created, registered in `Telegram/CMakeLists.txt`
    (bare filenames) and in the selftest's CMakeLists
  - Main thread only; no `crl::async`, no mutex, no second thread
  - `_inFlight` set once and cleared on every completion path
  - Push then pull; pull still attempted after a push failure
  - `hasMore` fast-path with a bounded catch-up count
  - Cycle-level backoff, doubling to a 300-second ceiling, reset on
    success
  - Interval re-read from settings at every rearm
  - Timer never armed while `SyncEnabled()` is false
  - `Start()` idempotent and application-wide, not per-session
  - `NextAction` is pure and covered by 8 selftest cases with a count
    guard; you watched a deliberate break fail
  - `/Zs` clean at `/W4 /WX`; selftest exit 0
  - One commit, K7 style; no scratch files committed

============================================================
FINAL REPORT — seven short points
============================================================

  1. The selftest output line for the scheduler family, and exit code.
  2. Which deliberate break you used and which case caught it.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. Where you called `Start()` and why it cannot run twice.
  5. Every path that clears `_inFlight` — list them.
  6. `/Zs` result per file.
  7. Anything ambiguous you had to guess at, plus your view on the
     PBKDF2-on-the-UI-thread note in section 1.

OUT OF SCOPE
  - No WebSocket (Task 9), no UI (Task 10).
  - No media upload or download.
  - Do not enable sync or change any default.
  - Do not move any existing work onto another thread.
  - Do not change `occurred_at` at any enqueue site.
