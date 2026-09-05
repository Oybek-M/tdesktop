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
  1. Read `MergeRecord` and `Client::pullAndMerge` in
     `custom_sync_client.cpp` (Task 7b). The tombstone branch there is
     the stub you are replacing.
  2. Read the v14 migration block in `custom_db.cpp` and
     `kCurrentSchemaVersion` in `custom_db.h`.
  3. Read `Outbox::Enqueue` in `custom_sync_outbox.cpp`, and
     `MergeGuard` / `MergeInProgress`.
  4. Read spec §0.3 and §0.13 in
     `docs/superpowers/specs/2026-07-29-multi-device-sync-backend-design.md`.
  5. Read `tools/sync-selftest/README.md`, section
     "To'liq build'siz sintaksis tekshiruvi" — use those include paths
     verbatim, they are absolute for a reason.
  6. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K4  Idempotent: applying the same tombstone twice changes nothing.
  K5  Sync off => ZERO behaviour change.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop or any server.
  Do NOT start a full tdesktop build (~34 minutes).
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real database
     (`<ArchiveRoot>/db/actioned_messages.db`).

============================================================
YOUR TASK — PLAN 02, TASK 7c
============================================================

Make tombstones work: a schema v15 index that maps `record_id` back to
a local row, application of incoming tombstones, and one producer.

------------------------------------------------------------
1. SCHEMA v15 — `sync_record_map`
------------------------------------------------------------

`record_id` is a hash of metadata; nothing local is keyed by it. Without
an index, a tombstone naming `target_record_id` cannot be resolved to
anything. Add, in a `if (version < 15)` block following the v14 block's
style, and bump `kCurrentSchemaVersion` to 15:

    CREATE TABLE IF NOT EXISTS sync_record_map (
        record_id   TEXT PRIMARY KEY,
        kind        TEXT    NOT NULL,
        account_id  INTEGER NOT NULL DEFAULT 0,
        peer_id     TEXT    NOT NULL,
        msg_id      INTEGER NOT NULL DEFAULT 0,
        occurred_at INTEGER NOT NULL DEFAULT 0)

plus an index on `(kind, account_id, peer_id, occurred_at)` — the
producer in section 4 needs the reverse lookup, and without it that is
a full scan.

`occurred_at` is in the table for one reason: `activity` rows carry a
one-way discriminator as `msg_id`, so the only way back to the
`activity_history` row is `(peer_id, observed_at)` plus a discriminator
comparison. Do not drop the column as redundant.

**Populate it from exactly two places**, both of which already know
every field:

  - `Outbox::Enqueue()` — local capture. Write the map row next to the
    outbox row. Note `Enqueue` returns early when sync is off or during
    a merge; that is correct, the map is only meaningful when syncing.
  - `MergeRecord()` — incoming records, after a successful merge.
    `MergeGuard` suppresses `Enqueue`, so merge must write the map
    itself or half the map goes missing.

Use `INSERT OR REPLACE` — re-observing the same event must not fail.

------------------------------------------------------------
🔴 2. APPLYING A TOMBSTONE — AND THE CACHES BEHIND THE TABLES
------------------------------------------------------------

Replace the `TombstoneSkipped` branch. Look `target_record_id` up in
`sync_record_map`, then delete the row it names:

| kind | how to delete |
|---|---|
| `deleted` | `DELETE FROM actioned_messages WHERE account_id=? AND peer_id=? AND msg_id=? AND type='deleted'` |
| `edited` | same, `type='edited'` |
| `ghost_read` | `CustomDB::ResetGhostRead(key)` |
| `media_index` | `DELETE FROM media_index WHERE account_id=? AND peer_id=? AND msg_id=?` |
| `activity` | see below |

Do NOT use `PermanentlyDeleteMessage()`. It removes every record for a
message id regardless of type, so a tombstone for one `edited` entry
would also destroy the `deleted` archive entry for the same message.

🔴 **`custom_db.cpp` keeps in-memory caches in front of these tables**
— the restore cache, the activity cache (`gActivityCache`,
`gActivityLoadedPeers`) and others. Deleting rows with raw SQL leaves
those caches holding data that no longer exists, and the UI keeps
showing a "deleted" message that is gone from disk until the next
restart. Find which caches cover each table you touch and invalidate
them. Say in your report exactly which ones you found and what you did.
If a cache has no invalidation entry point, add a narrow one rather
than reaching into its internals from another file.

**`activity` needs a new entry point.** `DeleteActivityEntry(id)`
refuses rows with `source == 'observed'`, and Task 7b merges incoming
activity rows with exactly that source — so the existing function can
never delete what sync wrote. Its guard protects against the user
destroying system-observed facts by hand; a tombstone is a different
thing, a deliberate deletion already made on another device.

Add a separate function for the sync path, e.g.
`DeleteActivityEntryForSync(qint64 id)`, with a comment saying why the
`source` guard is deliberately absent. Do not weaken
`DeleteActivityEntry`.

To find the row: `SELECT id, field FROM activity_history WHERE
peer_id = ? AND observed_at = ?` (no `account_id` filter — spec §0.13),
then match `DiscriminatorFor(field) == map.msg_id`, exactly as
`custom_sync_payload.cpp` does.

**Target not in the map** (§0.13, point 3): the tombstone may arrive
before the record it kills. Do not discard it. Keep the
`target_record_id` in a capped list in `sync_state`
(`pending_tombstones`, same shape as `corrupt_records`), and in
`MergeRecord` check that list **before** merging any record — if the
incoming `record_id` is on it, skip the merge, remove it from the list,
and count the record as rejected. That mirrors what the server does.

Applying a tombstone whose target row is already gone is a success, not
an error (K4).

------------------------------------------------------------
3. ONE-OFF CURSOR RESET
------------------------------------------------------------

Task 7b advanced `pull_cursor` past every tombstone it skipped, so
those records will never be pulled again. On first run after this task
ships, reset the cursor once:

    if (Outbox::GetState("tombstone_backfill_done") != "1") {
        Outbox::SetState("pull_cursor", "0");
        Outbox::SetState("tombstone_backfill_done", "1");
    }

Run it inside `pullAndMerge`, after the `KeysAvailable()` gate and
before reading the cursor. Re-pulling already-merged records is
harmless — every merge path is idempotent as of 7b, and that is exactly
what those guards were for.

------------------------------------------------------------
4. ONE PRODUCER — AND WHY ONLY ONE
------------------------------------------------------------

Add tombstone enqueueing to exactly one place: the user deleting a
single activity entry, `custom_activity_history_box.cpp:351`, where
`DeleteActivityEntry(e.id)` already returns whether it removed a row.

When it returns true, resolve that row's `record_id` from
`sync_record_map` (kind `activity`, the peer, the entry's
`observed_at`) and enqueue:

    Outbox::Enqueue(Kind::Tombstone, accountId, peerId,
                    DiscriminatorFor(targetRecordId),
                    QDateTime::currentSecsSinceEpoch(),
                    targetRecordId);

The `msg_id` is the discriminator of the target id — spec §0.6. If the
map has no row, do not enqueue: the record was never synced, so there
is nothing on the server to delete.

**Deliberately NOT producers:**

  - `PermanentlyDeleteMessage()` — its three call sites are in
    `data_histories.cpp` and `data_session.cpp`, i.e. Telegram's own
    delete flow, not archive management. Enqueueing there would mean
    that deleting a message in Telegram wipes it from every other
    device's archive — the precise outcome the archive exists to
    prevent.
  - `ClearDeletedArchive()` / `ClearEditedArchive()` /
    `ClearAllArchive()` — propagating a bulk wipe to every device is a
    product decision, not a mechanical one, and it is unrecoverable if
    wrong.
  - Retention pruning — spec §0.3 is explicit: local cleanup is not a
    global delete.

State in your report that these were considered and left out.

------------------------------------------------------------
HOW TO VERIFY — without building tdesktop
------------------------------------------------------------

**(a) SQL harness**, same technique as 7a/7b, in your own temp
directory (NOT the repo, NOT committed). Extract the SQL from your
source rather than retyping it, build the tables in their
post-migration shape, then prove:

  1. Map round trip: an enqueued record produces a map row; looking up
     its `record_id` returns the right `(kind, account, peer, msg_id,
     occurred_at)`.
  2. Each of the five deletes removes the intended row **and nothing
     else** — in particular, a tombstone for an `edited` entry must
     leave the `deleted` row for the same `msg_id` untouched. Assert
     both rows exist before and only the right one after.
  3. `activity` deletion picks the correct row when two rows share one
     `observed_at` and differ by `field` (reimplement
     `DiscriminatorFor` in Python:
     `int.from_bytes(sha256(field).digest()[:8], "big", signed=True)`).
  4. Applying the same tombstone twice leaves the same state (K4).
  5. An unresolved tombstone lands in `pending_tombstones`, and a
     later record with that id is not merged.

Check 2 is the one that matters — it is the difference between this
task and silent archive loss.

**(b) Syntax check.** `/Zs` at `/W4` on every .cpp you changed. An
official build uses `/WX`.

Also run the existing selftest unchanged and confirm exit code 0.

============================================================
DEFINITION OF DONE
============================================================

  - `kCurrentSchemaVersion` is 15; migration is additive, guarded by
    `if (version < 15)`, and uses `IF NOT EXISTS`
  - `sync_record_map` written from both `Enqueue` and `MergeRecord`
  - All five kinds delete correctly; `PermanentlyDeleteMessage` unused
  - Every cache in front of a table you delete from is invalidated
  - `DeleteActivityEntryForSync` added; `DeleteActivityEntry` unchanged
  - Unresolved tombstones persist and are applied on late arrival
  - One-off cursor reset, guarded by its own `sync_state` flag
  - Exactly one producer; the rejected candidates named in the report
  - Harness proved the edited/deleted isolation and double-apply
  - `/Zs` clean at `/W4`; selftest exit 0
  - One commit, K7 style; no scratch files committed

============================================================
FINAL REPORT — seven short points
============================================================

  1. Before/after row counts for the edited-vs-deleted isolation test.
  2. What the double-apply test showed.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. Which in-memory caches you found in front of the affected tables,
     and how you invalidated each.
  5. How the late-arriving-record case behaved.
  6. `/Zs` result per file.
  7. Anything ambiguous you had to guess at, and whether you agree with
     the producer scoping in section 4.

OUT OF SCOPE
  - No orchestrator, timer or WebSocket (Task 8/9).
  - No media file download or upload.
  - Do not enable sync or change any default.
  - Do not change `occurred_at` at any enqueue site.
  - Do not add bulk-clear tombstone producers.
