Execute the task described below. It is an implementation job in an
existing C++/Qt repository — not a document to review, summarise or
score. The only output that counts is committed source code.

REPOSITORY
  C:\TBuild\tdesktop
  Branch: Oybek. Never push to a remote named "upstream".

HOW TO REPORT
  Work quietly. Do not narrate steps. Do not paste build logs.
  The final report must be SHORT — the five points at the bottom, a
  line or two each.

STEP 0 — BEFORE YOU WRITE ANYTHING
  1. Read `Telegram/SourceFiles/custom_db.cpp`, function `RunMigrations()`.
     Read the v10, v12 and v13 blocks specifically. You are adding one
     more block in exactly that style.
  2. Read `Telegram/SourceFiles/custom_db.h` around line 58 — the
     version constant and the comment block above it.
  3. Read the plan section:
     `docs/superpowers/plans/2026-07-29-multi-device-sync-02-tdesktop-agent.md`,
     section "## Task 3: Sxema v6 — outbox va sync_state".
  4. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop or any server.
  Do NOT start a full tdesktop build (~34 minutes). This task does not
     need one — see "HOW TO VERIFY" below.
  Do NOT touch `Telegram/CMakeLists.txt` or anything under
     `docs/sync-protocol/`.
  Do NOT open, modify or delete the user's real database.

============================================================
YOUR TASK — PLAN 02, TASK 3
============================================================

Two files:

  Telegram/SourceFiles/custom_db.h    — bump the version constant
  Telegram/SourceFiles/custom_db.cpp  — add one migration block

------------------------------------------------------------
FOUR CORRECTIONS TO THE PLAN
------------------------------------------------------------

--- 1. The version is 14, not 6 (and not 9) ---

The plan says v6. Its own REVIZIYA block at the top of the file says v9.
Both are stale. `kCurrentSchemaVersion` is currently **13**, so the sync
migration is **v14**.

Versions already taken:

    v10  account_id (account isolation)
    v11  activity_history.source
    v12  story backfill
    v13  actioned_messages.read_at   (A17)
    v14  <- YOURS: sync_outbox + sync_state

Add a line to the comment block above the constant in `custom_db.h`,
matching the existing entries:

    // v14 (sync): sync_outbox + sync_state jadvallari

--- 2. Do NOT write the version stamp inside your block ---

The plan's Step 2 ends with:

    execSql("UPDATE schema_version SET version = 6");

Delete that line. `RunMigrations()` stamps the version **once**, at the
end, from `kCurrentSchemaVersion`:

    UPDATE schema_version SET version = ? WHERE rowid = 1

No existing migration block writes the version itself. Adding one that
does is not just redundant — it hardcodes a number that will be wrong
the moment someone copies the block as a template for v15, and the
mistake is invisible until a user's database silently reports the wrong
version and re-runs migrations it has already applied.

--- 3. `sync_outbox` needs an `account_id` column ---

The plan predates schema v10, which added multi-account isolation, and
predates spec 0.12, which put `account_hash` into `record_id`.

At push time you must compute

    record_id = SHA256(kind || 0 || account_hash || 0 || peer_hash || ...)

and `account_hash` is derived from the account id. If the outbox row
does not record which account the event belongs to, that is
unrecoverable at push time — you would have to guess, and a wrong guess
produces a `record_id` that no other device will ever match. The record
would sync, dedup against nothing, and quietly duplicate.

Add, matching the type used by every other table (v10 block):

    account_id INTEGER NOT NULL DEFAULT 0

and make it part of the primary key alongside `record_id`? **No** —
`record_id` alone stays the primary key, because it already includes
the account. `account_id` is stored so it can be read back, not to
disambiguate.

--- 4. Add `target_record_id TEXT` now ---

Spec 0.13: a `tombstone` must carry its target's `record_id` in
CLEARTEXT, because the server cannot read the encrypted payload.

The plan's outbox has nowhere to put it. Every other kind re-reads its
payload from the existing tables at push time, but a tombstone has no
such row to read from — the whole point is that the thing is gone.

Adding a nullable column now costs nothing. Discovering the gap in
Task 7 costs a v15 migration.

------------------------------------------------------------
THE TABLES
------------------------------------------------------------

Otherwise the plan's DDL is good — keep its shape, its comments and its
index. The outbox deliberately stores only identifying fields and
re-reads the payload at push time, so there is no second copy of the
data that can drift out of step with the first. `next_retry_at` is
persisted so backoff survives an app restart.

`peer_id` is `TEXT` here, matching `activity_history`, `text_cache` and
`media_index`. It is the plain local id — hashing to `peer_hash` happens
at push time, not in the database.

`sync_state` stays a plain `key`/`value` table: cursor, device id,
tokens, last success.

------------------------------------------------------------
HOW TO VERIFY — without building tdesktop
------------------------------------------------------------

The plan's Step 3 says to eyeball the SQL and wait for Task 6. Do
better than that: SQLite is available through Python, so run the real
statements against a throwaway database.

Write a scratch script (do NOT commit it, and do NOT put it in the
repo — use your own temp directory) that:

  1. Creates an in-memory or temp-file SQLite database.
  2. Executes your two `CREATE TABLE` statements and the `CREATE INDEX`,
     copied verbatim from the C++ string literals.
  3. Runs `PRAGMA table_info(sync_outbox)` and prints the columns.
  4. Asserts the expected column set is present.

This catches a stray comma, a missing space at a C++ string-literal
concatenation boundary, or a typo in a column name — all of which
otherwise surface only after a 34-minute build, on a user's machine, as
a failed migration that leaves the database on the old version.

Pay particular attention to string concatenation: in the C++ source the
SQL is split across several `"..."` literals, and a missing trailing
space is the classic way to produce `... NOT NULLDEFAULT 0`.

============================================================
DEFINITION OF DONE
============================================================

  - `kCurrentSchemaVersion` is 14, with a matching comment line
  - One `if (version < 14) { ... }` block, in the style of its neighbours
  - No `UPDATE schema_version` inside the block
  - `sync_outbox` has `account_id` and `target_record_id`
  - You ran the DDL against a scratch SQLite database and it created
    both tables and the index cleanly
  - No full tdesktop build; no scratch files committed
  - One commit, K7 style

============================================================
FINAL REPORT — five short points
============================================================

  1. The `PRAGMA table_info(sync_outbox)` output from your scratch run.
  2. `git show --stat HEAD` (stat block only) and the commit subject.
  3. Confirm no scratch/verification file was committed.
  4. Confirm the version stamp is written only at the end of
     `RunMigrations()`, not inside your block.
  5. Anything wrong or ambiguous you had to guess at.

OUT OF SCOPE
  - Do not start Task 4 (outbox layer and enqueue points). This task
    creates the tables only — nothing writes to them yet.
  - Do not modify any other migration block.
  - Do not touch the user's real `custom_mod.db`.
