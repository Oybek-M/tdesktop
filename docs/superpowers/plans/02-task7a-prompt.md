Execute the task described below. It is an implementation job in an
existing C++/Qt repository — not a document to review, summarise or
score. The only output that counts is committed source code.

REPOSITORY
  C:\TBuild\tdesktop
  Branch: Oybek. Never push to a remote named "upstream".

HOW TO REPORT
  Work quietly. Do not narrate steps. Do not paste build logs.
  The final report must be SHORT — the seven points at the bottom, a
  line or two each.

STEP 0 — BEFORE YOU WRITE ANYTHING
  1. Read `Telegram/SourceFiles/custom_sync_client.cpp`, the anonymous
     namespace just above `Client::pushPending` — the `BuildPayload`
     stub and the comment explaining why it returns `nullopt`.
  2. Read `Telegram/SourceFiles/custom_sync_outbox.h` — `OutboxEntry`
     and the `Outbox::` API. This is everything you get as input.
  3. Read `Telegram/SourceFiles/custom_db.cpp`: the `execSql` helper,
     `CustomDB::RawHandle()`, and the five `CustomSync::Outbox::Enqueue`
     call sites (grep for them — they are one line each, at the end of
     their functions).
  4. Read spec §0.4, §0.14 and §3.2 in
     `docs/superpowers/specs/2026-07-29-multi-device-sync-backend-design.md`.
     §0.14 was added today and is the reason this task exists in this
     shape.
  5. Read `tools/sync-selftest/README.md`, section
     "To'liq build'siz sintaksis tekshiruvi".
  6. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K4  Idempotent: building a payload twice gives byte-identical JSON.
  K5  Sync off => ZERO behaviour change.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop or any server.
  Do NOT start a full tdesktop build (~34 minutes). The verification
     below does not need one.
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real database at `<ArchiveRoot>/db/actioned_messages.db` (ArchiveRoot is a user setting; resolve it via `dbFilePath()` in custom_db.cpp:113).

============================================================
YOUR TASK — PLAN 02, TASK 7a
============================================================

The plan's "Task 7" covers both push payloads and pull/merge. It is
split. **You are doing the push half only.** Pull and merge are 7b and
depend on this landing first — merge reads exactly the fields you are
about to write.

Create `Telegram/SourceFiles/custom_sync_payload.h` / `.cpp`, register
them in `Telegram/CMakeLists.txt`, and make `custom_sync_client.cpp`
use them instead of its stub.

Right now `BuildPayload()` returns `nullopt` unconditionally, so
`pushPending()` builds records, throws every one away, and reports
`(0, 0)`. Nothing has ever been sent. Your job is to make the content
real.

------------------------------------------------------------
🔴 1. EVERY PAYLOAD CARRIES `account_id` AND `peer_id`
------------------------------------------------------------

Spec §0.14, added 2026-09-05. The plan predates it and its payload
table is therefore incomplete.

The record envelope identifies its owner only as `account_hash` and
`peer_hash`. Those are HMACs — **not reversible**. A receiving device
must write the row into a real `PeerKey{accountId, peerId}`, and no
amount of work on the envelope will recover one from a hash.

So every payload object you build starts with two fields:

    "account_id": "<decimal string>"
    "peer_id":    "<decimal string>"

**Decimal strings, not JSON numbers.** They must be byte-identical to
the pre-images used for the hashes — `QString::number(entry.accountId)`
and `entry.peerId` exactly as stored. Anything else silently breaks the
integrity check 7b will perform (`HMAC(peer_key, peer_id) == peer_hash`).

This holds for `kind == "activity"` too, even though its `account_hash`
is the empty string by §0.12. The hash is empty; the payload field is
not. 7b needs it to know which account's database to write.

Payload is E2E encrypted, so this reveals nothing to the server.

------------------------------------------------------------
🔴 2. YOU CANNOT REACH THE DATA THROUGH `custom_db.h`
------------------------------------------------------------

Four of the six kinds need to read one specific row back, and
`custom_db.h` exposes no single-row getter for any of them. What it has
is peer-wide vectors — `GetDeletedMessages(key)` returns every deleted
message in the chat. Calling that once per outbox entry is O(n·m) over
a table with thousands of rows.

Do NOT add getters to `custom_db.h`. That file is 146 KB, used
everywhere, and every edit to it is a K5 risk.

Use the accessor Task 4 already added for exactly this purpose:

    sqlite3 *db = CustomDB::RawHandle();

and write your own prepared statements in `custom_sync_payload.cpp`,
with a small local statement helper. `custom_sync_outbox.cpp` already
does precisely this — copy its shape, including how it handles a null
handle and how it finalises statements on every path.

**Threading:** the connection is `SQLITE_OPEN_FULLMUTEX` (serialized).
SQLite serialises for you. Do NOT add a mutex. Do NOT open a second
connection.

------------------------------------------------------------
3. THE TABLES, AS THEY ACTUALLY EXIST AFTER MIGRATIONS
------------------------------------------------------------

`custom_db.cpp` shows a baseline `CREATE TABLE` near line 285 and then
mutates it across migrations. The baseline is NOT what you are querying.
The final column sets are:

    actioned_messages
      id, peer_id, msg_id, type, original_text, new_text, media_path,
      is_out, msg_date, timestamp, notes, sender_id, is_media,
      account_id, read_at

    activity_history
      id, peer_id, field, old_value, new_value, observed_at,
      account_id, source

    media_index
      account_id, peer_id, msg_id, kind, file_name, rel_path, size,
      sha256, msg_date, archived_at, layer, status, reason
      PRIMARY KEY (account_id, peer_id, msg_id)

    ghost_reads
      account_id, peer_id, msg_id, timestamp
      PRIMARY KEY (account_id, peer_id)

Note `activity_history` has **no msg_id column**. That matters below.

------------------------------------------------------------
4. WHAT EACH KIND'S PAYLOAD CONTAINS AND WHERE IT COMES FROM
------------------------------------------------------------

All six get `account_id` and `peer_id` as above, in addition to what is
listed here. JSON keys are snake_case. Serialise with
`QJsonDocument::Compact` so the bytes are stable (K4).

--- `deleted` ---  spec §3.2: `{text, sender_id, is_out, is_media}`

    SELECT original_text, sender_id, is_out, is_media
    FROM actioned_messages
    WHERE account_id = ? AND peer_id = ? AND msg_id = ? AND type = 'deleted'
    ORDER BY id DESC LIMIT 1

`original_text` maps to the payload key `text`, not `original_text`.
`is_out` and `is_media` are INTEGER in SQLite and booleans in JSON.

--- `edited` ---  spec §3.2: `{old_text, new_text, is_out}`

    SELECT original_text, new_text, is_out
    FROM actioned_messages
    WHERE account_id = ? AND peer_id = ? AND msg_id = ? AND type = 'edited'
    ORDER BY id DESC LIMIT 1

`original_text` -> `old_text`, `new_text` -> `new_text`.

⚠️ A message edited twice has two rows here and you take the newest.
That is the best available answer, but it is a workaround, not a fix —
see section 6. Do not try to fix it in this task.

--- `activity` ---  spec §3.2: `{field, old_value, has_old_value, new_value}`

🔴 This is the one that will cost you time if you skim it.

`activity_history` has no `msg_id`. The outbox stores
`msg_id = DiscriminatorFor(field)` — the first 8 bytes of SHA-256 over
the field name. **That is one-way; you cannot read the field name out
of it.**

Resolve it forward instead. `occurred_at` for this kind is the row's
`observed_at` (see the enqueue call site), so:

    SELECT field, old_value, new_value
    FROM activity_history
    WHERE peer_id = ? AND observed_at = ?

then walk the rows and keep the one where
`CustomSync::DiscriminatorFor(field) == entry.msgId`. Usually there is
exactly one row; there can be several when two fields changed in the
same second, which is why the comparison exists.

Note the query filters on `peer_id` and `observed_at` only — **not
`account_id`**. `activity_history` is deliberately merged across
accounts (spec §0.13, and the comment at the `account_id` migration
says so explicitly). Adding an `account_id` filter here would silently
miss rows written by another account.

`has_old_value` is `old_value IS NOT NULL` — read it from the column's
null-ness with `sqlite3_column_type(...) == SQLITE_NULL`, not from
whether the string is empty. An empty old name is a real value.

--- `ghost_read` ---  spec §3.2: `{}` — metadata is enough

No database read at all. The payload is just the two §0.14 fields. The
message id already travels in the envelope.

--- `media_index` ---  spec §0.4

    SELECT kind, file_name, rel_path, size, sha256, status, reason,
           layer, msg_date
    FROM media_index
    WHERE account_id = ? AND peer_id = ? AND msg_id = ?

Payload keys: `kind, file_name, rel_path, size, sha256, status, reason,
layer, msg_date`.

`sha256` is empty for most rows today — spec §0.5 backfill has not run.
Emit it as an empty string rather than omitting the key; a stable key
set keeps K4 honest.

⚠️ The payload key `kind` here is the MEDIA kind (image/video/voice/
file), not the record kind. They collide by name and mean different
things. Do not "fix" one into the other.

--- `tombstone` ---  spec §0.3: `{target_record_id}`

From `entry.targetRecordId`, no database read. Note nothing enqueues
this kind yet, so it is unreachable in practice — implement it anyway,
it is four lines and 7b deletes by it.

------------------------------------------------------------
🔴 5. A MISSING SOURCE ROW MUST NOT WEDGE THE OUTBOX
------------------------------------------------------------

The row can be gone by the time push runs — the user cleared the
archive, or a prune removed it. If you return "cannot build" for that,
`pushPending` leaves the entry in the outbox and retries it on every
cycle, forever, for a record whose content no longer exists.

So the return type has to distinguish three outcomes, not two:

    enum class BuildStatus {
        Ok,          // payload qurildi
        SourceGone,  // manba qator yo'q -- hech qachon qurilmaydi
        Unsupported, // kind hali qo'llab-quvvatlanmaydi
    };

    struct BuildResult {
        BuildStatus status = BuildStatus::Unsupported;
        QByteArray json;
    };

    [[nodiscard]] BuildResult Build(const OutboxEntry &entry);

and `pushPending` handles them differently:

  - `Ok`          -> encrypt and send, as now.
  - `SourceGone`  -> remove from the outbox and log it. Add
                     `Outbox::Drop(recordId, reason)` for this —
                     the SQL is the same DELETE that `MarkSent` uses,
                     but calling it `MarkSent` would be a lie in the
                     logs and in the next person's head.
  - `Unsupported` -> leave it queued, exactly as today.

Do not silently swallow `SourceGone`. Count it, and surface the count
through `pushPending`'s existing callback or a qDebug line — a device
dropping records is something the user must be able to notice.

------------------------------------------------------------
6. ONE KNOWN DEFECT — REPORT IT, DO NOT FIX IT
------------------------------------------------------------

While reading the enqueue sites you will notice that `edited` uses
`msg.msgDate` as `occurred_at`. `msgDate` is the message's original
date and does not change when the message is edited, so two edits of
one message produce the same `(kind, account, peer, msg_id,
occurred_at)` tuple and therefore the **same `record_id`** — the second
edit dedups into the first and never reaches the server.

The obvious fix — use the local edit time — is wrong: `occurred_at` is
what makes two devices that saw the same event agree on a `record_id`,
and local wall-clock time differs per device. The real fix needs
Telegram's edit date, which `ActionedMessage` does not currently carry.

That is a separate change. Mention it in your report and move on.

------------------------------------------------------------
HOW TO VERIFY — without building tdesktop
------------------------------------------------------------

Two independent checks. Both are required.

**(a) The SQL harness.** Same technique that verified Task 3 and
Task 4. A scratch script in your own temp directory (NOT the repo, NOT
committed) that:

  1. Parses `custom_sync_payload.cpp` and extracts the SQL string
     literals, concatenating them the way the compiler will. Do not
     retype them by hand — retyping hides exactly the concatenation
     bugs you are looking for.
  2. Creates a scratch SQLite database with the four tables in their
     final shape (section 3 above).
  3. Inserts representative rows, including the awkward ones: a
     `deleted` row with `is_media = 1` and empty text, an
     `activity_history` row with `old_value` SQL NULL, and **two**
     `activity_history` rows sharing one `observed_at` with different
     `field` values.
  4. Runs each extracted statement with the parameters your C++ binds,
     in the same order, and asserts the values that come back.
  5. For the two-rows-same-second case, reimplements
     `DiscriminatorFor` in Python (`int.from_bytes(sha256(field)
     .digest()[:8], "big", signed=True)`) and asserts that it selects
     the right one of the two rows.

Step 5 is the one that matters. It is the only check that proves the
discriminator match works, and getting it wrong means activity records
carry another field's values — which no compiler and no crash will
ever tell you about.

**(b) Syntax check.** `custom_sync_payload.cpp` cannot go in the
selftest (it needs SQLite and `custom_db`), so compile it standalone
with the `/Zs` recipe in `tools/sync-selftest/README.md`. Include
`custom_sync_client.cpp` in the same check, since you changed it. Both
must be clean at `/W4` — an official build uses `/WX` and a warning
there is a build failure.

Also run the existing selftest to confirm you broke nothing:
27 vector checks plus the keystore checks, exit code 0.

============================================================
DEFINITION OF DONE
============================================================

  - `custom_sync_payload.h/.cpp` created, registered in
    `Telegram/CMakeLists.txt` with bare filenames
  - No getters added to `custom_db.h`; no second connection; no mutex
  - All six kinds build a payload; all six include `account_id` and
    `peer_id` as decimal strings
  - `activity` resolves its field through `DiscriminatorFor`, and its
    query does NOT filter on `account_id`
  - `has_old_value` comes from SQL NULL, not from an empty string
  - Three-way `BuildResult`; `Outbox::Drop` added; `SourceGone` entries
    leave the outbox and are counted
  - The stub in `custom_sync_client.cpp` is gone
  - Harness ran the extracted SQL, including the two-fields-one-second
    discriminator case
  - `/Zs` clean at `/W4` for both changed .cpp files
  - Selftest still exits 0
  - No full tdesktop build; no scratch files committed
  - One commit, K7 style

============================================================
FINAL REPORT — seven short points
============================================================

  1. The JSON your harness produced for one `activity` record, verbatim.
  2. What the two-fields-one-second case selected, and how you knew it
     was right.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. Confirm `custom_db.h` gained nothing.
  5. Confirm the `activity` query has no `account_id` filter, and say
     in one line why.
  6. What `/Zs` reported for both files.
  7. Anything wrong or ambiguous you had to guess at — including
     whether you agree with the section 6 defect analysis.

OUT OF SCOPE
  - Do not implement pull or merge. That is Task 7b: retention
    filtering per kind, the merge-must-not-re-enqueue guard, peer
    resolution via §0.14, and tombstone application.
  - Do not enable sync, generate a master key, or change any default.
  - Do not add the `peer_directory` enqueue point (still deferred).
  - Do not change `occurred_at` at any enqueue site.
