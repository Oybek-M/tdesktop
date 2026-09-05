Execute the task described below. It is an implementation job in an
existing C++/Qt repository — not a document to review, summarise or
score. The only output that counts is committed source code.

REPOSITORY
  C:\TBuild\tdesktop
  Branch: Oybek. Never push to a remote named "upstream".

HOW TO REPORT
  Work quietly. Do not narrate steps. Do not paste build logs.
  The final report must be SHORT — the six points at the bottom, a line
  or two each.

STEP 0 — BEFORE YOU WRITE ANYTHING
  1. Read `Telegram/SourceFiles/custom_db.h` — the `PeerKey` struct and
     the signatures of the five functions listed below.
  2. Read `Telegram/SourceFiles/custom_db.cpp` — how it opens the
     database, its `execSql` helper, and the v14 migration block.
  3. Read the plan section:
     `docs/superpowers/plans/2026-07-29-multi-device-sync-02-tdesktop-agent.md`,
     section "## Task 4: Outbox qatlami va enqueue nuqtalari".
  4. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K5  Sync off => ZERO behaviour change. This task is where that rule
      is easiest to break.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop or any server.
  Do NOT start a full tdesktop build (~34 minutes). Verification below
     does not need one.
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real database at `<ArchiveRoot>/db/actioned_messages.db` (ArchiveRoot is a user setting; resolve it via `dbFilePath()` in custom_db.cpp:113).

============================================================
YOUR TASK — PLAN 02, TASK 4
============================================================

Create `Telegram/SourceFiles/custom_sync_outbox.h` / `.cpp`, add the
enqueue calls, and register the new files in `Telegram/CMakeLists.txt`.

This is the first task that touches the main build, so read the
blockers below before writing anything.

------------------------------------------------------------
BLOCKER 1 — you cannot reach the database the way the plan says
------------------------------------------------------------

The plan says to use "o'sha `gDb` ulanishi va `execSql` yordamchisi".
You cannot. Both are `static` in `custom_db.cpp`:

    static sqlite3 *gDb = nullptr;          // line 35
    static bool execSql(const char *sql)    // line 130

Internal linkage — a separate translation unit cannot see either.

Do NOT solve this by opening a second connection to the same file. Two
write connections to one SQLite database produce `SQLITE_BUSY` under
concurrent access, and the failure is intermittent and timing
dependent, which is the worst kind to debug.

Instead expose one narrow accessor from `custom_db.h`:

    struct sqlite3;   // oldindan e'lon -- sqlite3.h sarlavhaga kirmasin

    namespace CustomDB {
    // FAQAT sync outbox uchun. Boshqa modul bu tutqichni olmasin --
    // baza ulanishining egasi custom_db.cpp bo'lib qoladi.
    [[nodiscard]] sqlite3 *RawHandle();
    }

and give `custom_sync_outbox.cpp` its own small statement helper.

**Threading:** the connection is opened with `SQLITE_OPEN_FULLMUTEX`
(`custom_db.cpp` line 248), i.e. serialized mode. SQLite itself
serialises access, so do NOT add your own mutex around it and do NOT
assume single-threaded use. `Pending()` will be called from the sync
worker thread in Task 8.

------------------------------------------------------------
BLOCKER 2 — record_id cannot be computed yet, and faking it is fatal
------------------------------------------------------------

`sync_outbox.record_id` is the primary key, and

    record_id = SHA256(kind || 0 || account_hash || 0 || peer_hash
                       || 0 || msg_id || 0 || occurred_at)

`account_hash` and `peer_hash` are HMACs keyed by material derived from
the master key. **The master key does not exist yet** — the keystore is
Task 5, which comes after this one.

🔴 Do NOT compute `record_id` with an empty, zero or placeholder key
"for now". Those rows would look valid, would be pushed the moment sync
turns on, and would carry ids no other device can ever reproduce. They
would dedup against nothing and silently duplicate every record, and
the only fix afterwards is wiping server state.

The correct behaviour is the one the plan already states for a
different reason: **`Enqueue()` is a no-op when sync is not usable.**
Define "not usable" to include "the master key is unavailable", and
have `Enqueue()` return before touching the database in that case.

So introduce a tiny seam now — something like

    [[nodiscard]] bool KeysAvailable();

returning `false` unconditionally, with a comment saying Task 5 fills
it in. Task 4 therefore ships a complete, exercised SQL layer with
enqueue call sites that are provably inert. That is the intended
state, not a shortcut: it is exactly what rule K5 asks for.

------------------------------------------------------------
CORRECTIONS TO THE PLAN
------------------------------------------------------------

--- 1. `Enqueue` needs `accountId`; so does `OutboxEntry` ---

The plan's signature predates schema v10 and spec 0.12. The v14 table
you are writing to already has the columns:

    Enqueue(kind, accountId, peerId, msgId, occurredAt,
            targetRecordId = {})

`accountId` is `qint64` and comes straight off `PeerKey::accountId` at
every call site. Without it, push time cannot derive the right
`account_hash`, and the resulting `record_id` matches nothing.

Add `accountId` and `targetRecordId` to `OutboxEntry` too, so the
struct mirrors the table.

--- 2. Five enqueue points, not four ---

The plan's table lists four. `media_index` was added later. All five
take a `PeerKey`, so `accountId` is available at each:

| Function (custom_db.cpp)   | Kind          | msg_id |
|---|---|---|
| `MarkDeleted`              | `deleted`     | `msgId` |
| `SaveActionedMessage`, only when `msg.type == "edited"` | `edited` | `msg.msgId` |
| `SaveActivityHistoryEntry` | `activity`    | `DiscriminatorFor(field)` |
| `SaveGhostRead`            | `ghost_read`  | `msgId` |
| `UpsertMediaIndex`         | `media_index` | `entry.msgId` |

Each is ONE line at the very END of the function, after the existing
logic has committed. Do not restructure the capture code — K5.

--- 3. `peer_directory` is DEFERRED, deliberately ---

The plan's revision block lists a sixth point,
`CustomSettings::RememberPeerName()`. Do NOT add it in this task.

Its signature is `RememberPeerName(const QString &peerId, const QString
&name)` — no account context at all, and its four call sites are in
`custom_archive.cpp` and `custom_tab_storage.cpp`. Threading an account
id through them is a real change to unrelated code, and guessing the
account instead would produce a wrong `account_hash`, which is the same
poisoning failure as Blocker 2.

Say in your report that it is deferred. It will be handled when the
push path exists and the right account is unambiguous.

--- 4. The backoff snippet is off by one ---

The plan's comment promises "1s, 2s, 4s…" but its code

    const auto attempts = CurrentAttempts(recordId) + 1;
    const auto delay = std::min<qint64>(300, qint64(1) << std::min(attempts, 9));

gives 2s on the first failure. Use `attempts - 1` for the shift so the
first retry really is 1 second. Keep the 300-second ceiling and keep
`next_retry_at` persisted — surviving a restart is the whole point.

--- 5. CMakeLists entries have no `SourceFiles/` prefix ---

The plan's Step 4 shows `SourceFiles/custom_sync_record.cpp`. That list
uses bare filenames — see `custom_db.cpp` at line 1135. Add the six
files in that style, next to the other `custom_*` entries, keeping the
`.cpp`/`.h` pairing the list already uses.

------------------------------------------------------------
HOW TO VERIFY — without building tdesktop
------------------------------------------------------------

Same technique that verified the v14 migration. Write a scratch script
(your own temp directory, NOT the repo, NOT committed) that:

  1. Extracts the SQL string literals from your `custom_sync_outbox.cpp`
     by parsing the source — concatenating them the way the compiler
     will. Do not retype them by hand; retyping hides exactly the
     concatenation bugs you are looking for.
  2. Creates a scratch SQLite database with the v14 `sync_outbox` and
     `sync_state` DDL (extract that from `custom_db.cpp` the same way).
  3. Executes every extracted statement with representative parameters.
  4. Exercises the real behaviour: insert a row, read it back through
     the `Pending` query, mark it failed twice and assert
     `next_retry_at` grows 1s then 2s, then mark it sent and assert it
     is gone.

That last step is the one that matters. It catches a wrong column
order in a bind, an `UPDATE` whose `WHERE` never matches, and the
backoff off-by-one — none of which a compiler will tell you about.

============================================================
DEFINITION OF DONE
============================================================

  - `custom_sync_outbox.h/.cpp` created; `CustomDB::RawHandle()` added
  - No second SQLite connection; no extra mutex
  - `Enqueue` takes and stores `accountId`; `OutboxEntry` mirrors the table
  - Five one-line enqueue calls, at the end of each function
  - `KeysAvailable()` returns false, and `Enqueue` returns before any
    database write when it does
  - No `record_id` is ever computed from a placeholder key
  - Six files registered in `Telegram/CMakeLists.txt`, bare filenames
  - Your scratch harness ran the extracted SQL and the backoff assertions
  - No full tdesktop build; no scratch files committed
  - One commit, K7 style

============================================================
FINAL REPORT — six short points
============================================================

  1. The backoff sequence your harness measured for the first three
     failures.
  2. `git show --stat HEAD` (stat block only) and the commit subject.
  3. Confirm no second database connection and no added mutex.
  4. Confirm `Enqueue` cannot write while `KeysAvailable()` is false.
  5. Confirm `peer_directory` was deferred and not implemented.
  6. Anything wrong or ambiguous you had to guess at.

OUT OF SCOPE
  - Do not start Task 5 (keystore).
  - Do not implement push, HTTP, or encryption.
  - Do not change any capture logic — only append the enqueue call.
