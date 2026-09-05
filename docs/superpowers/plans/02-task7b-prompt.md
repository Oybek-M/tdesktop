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
  1. Read `Telegram/SourceFiles/custom_sync_payload.cpp` (Task 7a).
     You are writing its mirror image; the payload keys it emits are
     the keys you consume.
  2. Read `Client::pull` and `ParsePullResponse` in
     `custom_sync_client.h/.cpp`, and `Client::pushPending` — its
     structure is the model for yours.
  3. Read `custom_sync_outbox.h` (`GetState`/`SetState`, `Enqueue`)
     and the five `CustomSync::Outbox::Enqueue` call sites in
     `custom_db.cpp` (grep for them).
  4. Read spec §0.3, §0.13, §0.14, §3.3 and §3.4 in
     `docs/superpowers/specs/2026-07-29-multi-device-sync-backend-design.md`.
  5. Read `tools/sync-selftest/README.md`, section
     "To'liq build'siz sintaksis tekshiruvi" — the include paths there
     are absolute for a reason; use them verbatim.
  6. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K4  Idempotent: merging the same record twice changes nothing.
  K5  Sync off => ZERO behaviour change.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop or any server.
  Do NOT start a full tdesktop build (~34 minutes).
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real database
     (`<ArchiveRoot>/db/actioned_messages.db`; see `dbFilePath()` in
     custom_db.cpp).

============================================================
YOUR TASK — PLAN 02, TASK 7b
============================================================

Add `Client::pullAndMerge(Fn<void(int merged, int rejected, QString error)>)`
plus a merge layer, so records from other devices land in the local
database.

Do NOT add a timer or a polling loop — Task 8 owns scheduling. You
provide the single callable step.

The plan's Task 7 section is thin and predates spec §0.3, §0.13 and
§0.14. The five sections below replace it.

------------------------------------------------------------
🔴 1. THE RETENTION FILTER IS PER KIND, NOT GLOBAL
------------------------------------------------------------

Spec §0.3 requires rejecting records outside the local retention
window, otherwise sync loops forever: pull writes a row, the local
prune deletes it, the next pull brings it back.

The obvious reading — "reject anything whose `occurred_at` is older
than 30 days" — **destroys the application's main feature.** For
`deleted`, `occurred_at` is the MESSAGE's date (see the enqueue call
site: `msgDate > 0 ? msgDate : now`). Deleted-message archives are
kept forever and routinely contain messages from years ago. A global
filter would silently discard nearly all of them.

Only two tables are actually pruned:

| kind | local prune | filter? |
|---|---|---|
| `activity` | `PruneStaleActivityHistory(30)` on `observed_at` | YES |
| `ghost_read` | `PruneStaleGhostReads(30)` on `timestamp` | YES |
| `deleted`, `edited`, `media_index` | never pruned | NO |

For `activity`, `occurred_at` IS the `observed_at` the prune compares
against, so the comparison is direct.

**K1:** do not write `30` in your filter. Both prune functions default
to 30 via a header default argument. Introduce one shared constant in
`custom_db.h`

    constexpr int kActivityRetentionDays = 30;

use it as the default of both prune declarations, and use it in the
filter. One number, one place.

A rejected record is **not an error**: it is not merged, it is counted
as `rejected`, and **the cursor still advances**. If the cursor stalls
on rejected records, sync stops dead.

------------------------------------------------------------
🔴 2. MERGING MUST NOT RE-ENQUEUE WHAT IT MERGED
------------------------------------------------------------

This is the failure you will not notice in testing.

Merge writes through the existing `custom_db.cpp` functions —
`MarkDeleted`, `SaveActionedMessage`, `SaveActivityHistoryEntry`,
`SaveGhostRead`, `UpsertMediaIndex`. Task 4 added an
`Outbox::Enqueue(...)` call to the end of every one of them. So every
record you merge is immediately queued to be pushed back to the server
it just came from.

It is not an infinite loop — `record_id` is deterministic, so the
server answers `duplicate` — but every pull fills the outbox with
useless traffic, and the pushed copy carries this device's
`observed_at` instead of the original.

Add a suppression seam in `custom_sync_outbox`:

    // Merge davomida enqueue'ni o'chiradi. THREAD_LOCAL bo'lishi SHART:
    // merge sync oqimida ishlaydi, foydalanuvchi esa ayni paytda UI
    // oqimida xabar o'chirishi mumkin -- global bayroq o'sha haqiqiy
    // hodisani ham yutib yuborardi.
    class MergeGuard {
    public:
        MergeGuard();
        ~MergeGuard();
    };
    [[nodiscard]] bool MergeInProgress();

backed by a `thread_local int` depth counter (not a bool — nesting must
not clear it early), and make `Enqueue()` return immediately when
`MergeInProgress()` is true. Hold one guard around each record's merge.

------------------------------------------------------------
🔴 3. RESOLVING THE RECORD TO A LOCAL CHAT
------------------------------------------------------------

The envelope carries `account_hash` / `peer_hash` — one-way HMACs. The
real ids come from the **decrypted payload**, per spec §0.14:
`account_id` and `peer_id`, both decimal strings. Task 7a writes them
into every payload.

Order of operations per record:

  1. `Crypto::Open(ContentKey(), record.nonce, record.payload)`.
     Failure => log to the corrupt list (section 5), count as rejected,
     **advance the cursor**, continue. One bad record must never abort
     the batch.
  2. Parse JSON; read `account_id` and `peer_id`.
  3. **Verify** `Crypto::ComputePeerHash(PeerKey(), peer_id) ==
     record.peerHash`. Mismatch means a wrong key or tampering —
     reject, do not merge. For `kind != "activity"` also verify
     `ComputeAccountHash(AccountKey(), account_id) ==
     record.accountHash`; for `activity` that field is the empty
     string by §0.12, so skip it there.
  4. Build `CustomDB::PeerKey{accountId, peerId}` and write.

`accountId` is `account_id.toLongLong()`; `peerId` stays a QString.

------------------------------------------------------------
4. THE FIVE MERGE PATHS — AND WHERE THEY ARE NOT IDEMPOTENT
------------------------------------------------------------

K4 is not free here. A re-pull (cursor reset, reinstall, server replay)
must not double rows. Check each target before writing:

- `activity` -> `SaveActivityHistoryEntry(key, field, hasOldValue,
  oldValue, newValue, occurredAt, source)`. It appends unconditionally.
  Guard with the existing `HasActivityEntryAt(peerId, field,
  observedAt)` — that function exists precisely for this. Pass
  `source` as `u"observed"_q` unless you find a better fit; say in
  your report what you chose.
- `deleted` -> `MarkDeleted(msgId, key, {}, text, msgDate, isOut,
  senderId, isMedia)`. `mediaPath` stays empty — the file is not
  synced by this task. **Read what `MarkDeleted` does on a second
  call for the same msgId** and add a guard if it appends. Say what
  you found.
- `edited` -> `SaveActionedMessage` with `type = "edited"`. Same
  question: does a repeat create a second row? Guard if so.
- `ghost_read` -> `SaveGhostRead(key, msgId)`. Primary key
  `(account_id, peer_id)`, so it upserts. Note it also calls
  `PruneStaleGhostReads` internally — that is fine.
- `media_index` -> `UpsertMediaIndex(key, entry)`. Upsert by PK, safe.
  Fill `MediaIndexEntry` from the payload; `archivedAt` is local
  provenance, not synced — set it to the current time.

Spec §3.4 conflict rule: **smaller `observed_at` wins.** Where a local
row already exists and the incoming record is older, the incoming one
is authoritative. Where the existing write function has no way to
express that, say so rather than inventing a column.

------------------------------------------------------------
5. CURSOR, CORRUPT LIST, AND WHAT `pullAndMerge` RETURNS
------------------------------------------------------------

Cursor lives in `sync_state` under `pull_cursor` (via
`Outbox::GetState`/`SetState`). Read it at the start, write
`nextSince` at the end of a successful batch — **after** processing,
and regardless of how many records were rejected. On a transport
failure do not move it.

Do not loop on `hasMore` inside this function. One batch per call;
Task 8 decides whether to call again.

Corrupt records: append the `record_id` to `sync_state` key
`corrupt_records` (a compact list; cap it so it cannot grow without
bound — say the most recent 50) and `qWarning` once per record. The
user must be able to see that something is being skipped.

Signature:

    void pullAndMerge(Fn<void(int merged, int rejected, QString error)> done);

Gate the whole thing on `Outbox::KeysAvailable()`, exactly as
`pushPending` does — that is what keeps K5 true.

------------------------------------------------------------
6. TOMBSTONE RECEIVE IS OUT OF SCOPE — SKIP IT CLEANLY
------------------------------------------------------------

Applying a tombstone means deleting the local row named by
`target_record_id`. Nothing local is keyed by `record_id`, so this
needs a `record_id -> (kind, account, peer, msg_id)` index table and a
schema v15 migration. That is Task 7c.

Nothing enqueues `tombstone` today, so no such record can exist yet.
Treat an incoming `tombstone` as: counted in `rejected`, cursor
advanced, one `qWarning` naming Task 7c. Do NOT silently drop it and
do NOT invent a partial implementation.

------------------------------------------------------------
HOW TO VERIFY — without building tdesktop
------------------------------------------------------------

**(a) Round-trip harness.** Extend the technique from Task 7a. In your
own temp directory (NOT the repo, NOT committed), a script that:

  1. Builds the four tables in their post-migration shape (Task 7a's
     harness lists them; re-derive from `custom_db.cpp`, do not trust
     a copy).
  2. Takes each payload shape Task 7a emits, feeds it through your
     merge logic's SQL, and asserts the row that lands.
  3. Runs the SAME payload twice and asserts the row count does not
     change (K4). Do this for all five kinds — this is the check that
     catches a missing dedup guard.
  4. Asserts the retention filter: an `activity` record with
     `occurred_at` 40 days old is rejected, and a `deleted` record
     with `occurred_at` 3 years old is **accepted**. Getting the second
     one wrong is the failure described in section 1.

**(b) Syntax check.** `/Zs` at `/W4` on every .cpp you changed, using
the absolute include paths in the selftest README. An official build
uses `/WX`; a warning is a build failure.

Also run the existing selftest unchanged and confirm exit code 0.

============================================================
DEFINITION OF DONE
============================================================

  - `pullAndMerge` added, gated on `KeysAvailable()`, no timer
  - Retention filter is per kind; `kActivityRetentionDays` is the only
    place the number appears
  - `MergeGuard` is thread_local and depth-counted; `Enqueue` honours it
  - Peer/account hashes verified against the payload pre-images
  - All five kinds merge; each non-idempotent path has a guard
  - Cursor advances on rejected and corrupt records, not on transport
    failure
  - `tombstone` skipped with a warning, not silently
  - Harness proved double-merge is a no-op for all five kinds, and
    proved the 3-year-old `deleted` record is accepted
  - `/Zs` clean at `/W4`; selftest exit 0
  - One commit, K7 style; no scratch files committed

============================================================
FINAL REPORT — seven short points
============================================================

  1. Double-merge row counts, per kind, before and after.
  2. What the retention test showed for the 40-day `activity` and the
     3-year `deleted` record.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. For `MarkDeleted` and `SaveActionedMessage`: does a repeat append?
     What guard did you add?
  5. What `source` you passed to `SaveActivityHistoryEntry`, and why.
  6. `/Zs` result per file.
  7. Anything ambiguous you had to guess at — especially anywhere the
     §3.4 "smaller observed_at wins" rule could not be expressed.

OUT OF SCOPE
  - No tombstone application, no schema v15 (Task 7c).
  - No orchestrator, timer or WebSocket (Task 8/9).
  - No media file download.
  - Do not enable sync or change any default.
  - Do not change `occurred_at` at any enqueue site.
