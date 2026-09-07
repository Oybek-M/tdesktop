Execute the task described below. It is an implementation and
verification job in an existing C++/Qt repository — not a document to
review, summarise or score.

REPOSITORY
  C:\TBuild\tdesktop
  Branch: Oybek. Never push to a remote named "upstream".

HOW TO REPORT
  Work quietly. Do not narrate steps. Do not paste build logs.
  The final report must be SHORT — the seven points at the bottom.

STEP 0 — BEFORE YOU WRITE ANYTHING
  1. Read `Outbox::Enqueue` and `Outbox::KeysAvailable` in
     `custom_sync_outbox.cpp`.
  2. Read `CustomSync::Start` and `Orchestrator::arm` / `runCycle` in
     `custom_sync.cpp`.
  3. Read the five `CustomSync::Outbox::Enqueue` call sites in
     `custom_db.cpp` (grep), and the `if (version < 14)` and
     `if (version < 15)` migration blocks.
  4. Read `dbFilePath()` and `MaybeBackupBeforeMigration()` in
     `custom_db.cpp`.
  5. Read the plan section "## Task 11: Regressiya tekshiruvi (qoida
     K5)" in
     `docs/superpowers/plans/2026-07-29-multi-device-sync-02-tdesktop-agent.md`.
  6. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K5  Sync off => ZERO behaviour change. This task exists to prove it.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments and the checklist in Uzbek, identifiers in
     English.
  Do NOT run tdesktop. Do NOT run any server.
  Do NOT modify anything under `docs/sync-protocol/`.
  🔴 Do NOT open, move, rename or write to the user's real database.
     Read the rule in section 2 before you go anywhere near it.

============================================================
YOUR TASK — PLAN 02, TASK 11
============================================================

The plan's Task 11 is written as a purely manual pass through the
running application. You cannot run tdesktop, so doing it as written is
impossible — and reporting it as done would be a lie.

Split it the way it actually divides:

  - **You** do everything that can be checked without launching the
    app: the migration against a copy of the real database, a static
    K5 audit, the build, the selftest, and one small defensive fix.
  - **You write the manual checklist** that the user runs afterwards.

Do not claim any manual result. If a row can only be confirmed by
looking at the running app, it belongs in the checklist, not in your
report.

------------------------------------------------------------
🔴 1. THE ONE DEFENSIVE FIX — `_inFlight` WATCHDOG
------------------------------------------------------------

`Orchestrator::runCycle()` sets `_inFlight = true` and clears it only
in `onCycleFinished`. Every normal path reaches there. But if a network
callback never fires, the flag stays set **forever**: the timer keeps
firing, every tick returns early, and sync is silently dead until the
application restarts. Nothing in the UI would say why.

Add a watchdog: when a cycle starts, arm a single-shot timer for a
generous timeout (well beyond any real request — derive it from the
interval rather than inventing a second constant). If it fires while
`_inFlight` is still true for that same cycle, log a warning, clear the
flag, count it as a failure so the backoff applies, and rearm.

Guard against the stale-watchdog case: a cycle that finished normally
must not have its watchdog clear the flag belonging to the *next*
cycle. A simple monotonically increasing cycle id captured in the
lambda is enough.

Extend the `NextAction` selftest family only if your change touches it.
It should not — the watchdog lives in the timer wiring, not the policy.

**Leave the second Task 8 follow-up alone.** A large backlog drains one
batch per interval once the catch-up cap is hit, because
`_catchUpCycles` is not reset until `hasMore` goes false. That is safe
behaviour, it is recorded in STATUS.md, and changing pacing during a
regression task is exactly the wrong time.

------------------------------------------------------------
🔴 2. MIGRATION AGAINST A COPY — AND ONLY A COPY
------------------------------------------------------------

Schema v14 and v15 have never run against the user's real data. The
database lives at `<ArchiveRoot>/db/actioned_messages.db`; resolve
`ArchiveRoot` the way `dbFilePath()` does.

**Never open the original.** Not read-only, not with `sqlite3`, not
"just to check". Opening a SQLite database can create or modify `-shm`
and `-wal` sidecars, and this file is the user's irreplaceable archive.

Instead: copy `actioned_messages.db` **together with its `-wal` and
`-shm` files** into your own temp directory, and work only there.
Copying the main file alone gives you a database missing every
committed-but-not-checkpointed change — it will look valid and be
stale, which is worse than an obvious failure.

Then, on the copy:

  1. Record `PRAGMA user_version` / the `schema_version` row, and the
     row count of every existing table, before anything.
  2. Apply the v14 and v15 statements exactly as extracted from
     `custom_db.cpp` — parse them out of the source, do not retype
     them.
  3. Assert: every pre-existing table has the **same row count** as
     before, `sync_outbox`, `sync_state` and `sync_record_map` now
     exist and are empty, and `schema_version` is 15.
  4. Run `PRAGMA integrity_check` and assert it returns `ok`.
  5. Report the actual numbers — table names and counts — not
     "unchanged".

If the real database is not at the expected path, say so and stop.
Do not invent a path or fabricate a database to test against.

------------------------------------------------------------
3. STATIC K5 AUDIT — PROVE IT, DO NOT ASSERT IT
------------------------------------------------------------

For each claim below, quote the line that makes it true. "I reviewed
it and it looks fine" is not a result.

  a. With `syncEnabled == false`, `Outbox::Enqueue()` returns **before
     touching the database**. Follow the short-circuit through
     `KeysAvailable()` and confirm nothing before that point opens a
     statement or reads `sync_state`.
  b. With sync off, no code path reaches `QNetworkAccessManager`.
     Enumerate every entry point that could — the orchestrator, the
     sync tab's buttons, enrolment — and show the gate on each.
  c. `CustomSync::Start()` runs in the `Main::Session` constructor.
     Show that with sync off it performs no file, database or network
     I/O, so application startup is unaffected.
  d. The five capture functions in `custom_db.cpp` call `Enqueue` as
     the **last** statement, after their existing work has committed,
     so an early return inside it cannot skip capture.
  e. `kActivityRetentionDays` is 30 — the same value the code used
     before it became a named constant. Confirm no prune interval
     changed by accident.

------------------------------------------------------------
4. THE MANUAL CHECKLIST YOU WRITE
------------------------------------------------------------

Create `docs/superpowers/plans/02-task11-manual-checklist.md`.

The plan says to put results in the commit message and add no file.
Override that: this checklist gets run again on every device the user
adds — tdesktop, Android, iOS, the capture service — so it needs to
live somewhere it can be reused and ticked off. The *results* still go
in a commit message later.

Cover the plan's three states, in Uzbek, one checkbox per row:

  - Sync off: startup time, deleted messages visible, edit history,
    Activity History window, Ghost Mode, `sync_outbox` stays empty, no
    network activity
  - Sync on but server unreachable: app normal, capture works,
    `sync_outbox` fills, UI shows the error and the backoff, no freeze,
    queue drains when the server returns
  - Key unavailable: capture works, `sync_outbox` fills, push stops,
    nothing is lost

Add a fourth section the plan does not have, because Task 10 shipped
untested UI:

  - **Sinxronizatsiya tab birinchi ko'rish**: the tab opens without
    crashing, every control is reachable, labels are not clipped, the
    toggle survives closing and reopening the window, the status block
    renders with a null orchestrator, and the enrolment confirmation
    box actually appears before any network call.

For each row say **how** to check it, not just what. "Check
`sync_outbox` is empty" is useless without the query and the file path;
give both, and give the read-only way to run it (a copy, per section 2).

------------------------------------------------------------
HOW TO VERIFY YOUR OWN WORK
------------------------------------------------------------

  1. Selftest green before your change (exit 0) and after.
  2. 🔴 **Do NOT run the full build.** `cmake --build` on the Telegram
     target is forbidden without the user asking for it. It takes ~34
     minutes and ~15 GB of RAM, and the machine is running other heavy
     applications; starting one in the background stalls the user's
     work. Building and running `sync_selftest` is fine — that takes
     seconds.

     The watchdog touches `custom_sync.cpp`, so a full build IS needed
     to prove it links — but the **user runs it**. Finish everything
     else, verify what you can with `/Zs`, and stop with a clear line
     saying the build is pending. Do not report the task as verified.
  3. `/Zs` at `/W4 /WX` on `custom_sync.cpp`.
  4. Do not commit the database copy or the harness script.

Do not touch the `cmake` submodule — it carries a deliberate local
`/MP6` change.

============================================================
DEFINITION OF DONE
============================================================

  - `_inFlight` watchdog added, with a cycle id so a stale watchdog
    cannot clear a later cycle's flag
  - Backlog pacing left unchanged
  - Migration verified on a **copy** (with `-wal`/`-shm`); original
    never opened; real row counts reported
  - `PRAGMA integrity_check` returns ok
  - Five K5 claims each backed by a quoted line
  - `02-task11-manual-checklist.md` written, four sections, each row
    saying how to check it
  - `/Zs` clean at `/W4 /WX`; selftest exit 0
  - Full build NOT run; handed to the user instead
  - No manual result claimed as done
  - One commit, K7 style; no harness or database copy committed

============================================================
FINAL REPORT — seven short points
============================================================

  1. Row counts before and after migration, per table, and the
     `integrity_check` result.
  2. The quoted line proving claim (a) — Enqueue returns before any
     database work.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. How the watchdog distinguishes its own cycle from a later one.
  5. `/Zs` result, and confirmation that you did not start a full build.
  6. Anything in the K5 audit that did NOT check out.
  7. Anything ambiguous you had to guess at.

OUT OF SCOPE
  - No WebSocket (Task 9), no key sharing (Task 12).
  - Do not change the scheduler policy, the retention filter, or any
    enqueue site.
  - Do not enable sync or change any default.
  - Do not "fix" anything the audit turns up — report it. A regression
    task that also changes behaviour cannot prove anything.
