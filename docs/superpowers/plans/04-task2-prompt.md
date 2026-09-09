Execute the task described below. It is an implementation job in an
existing .NET 8 repository — not a document to review, summarise or
score. The only output that counts is committed source code.

REPOSITORY
  C:\Users\Oybek\Documents\Projects programming\Telegram\customsync-server
  Branch: Oybek. Remote `origin` is Oybek-M/customsync-server.

HOW TO REPORT
  Work quietly. Do not narrate steps. Do not paste build logs.
  The final report must be SHORT — the seven points at the bottom.

STEP 0 — BEFORE YOU WRITE ANYTHING
  1. Read the `retention.*_days` entries in
     `src/CustomSync.Services/SettingsService.cs` (around line 53-66)
     and the comment above them. Section 1 explains why they matter.
  2. Read `src/CustomSync.Data/Entities/KeyWrapEntity.cs` and
     `MediaBlobEntity.cs` — the entity style you must match.
  3. Read `src/CustomSync.Data/SyncDbContext.cs` — how a DbSet is
     declared and how entities are configured.
  4. Read the two most recent migrations under
     `src/CustomSync.Data/Migrations/` to copy the naming and shape.
  5. Read `src/CustomSync.Services/StatsService.cs` — in particular
     `SummaryAsync`, added in Task 1, and how `SettingsService` is
     injected optionally.
  6. Read the plan sections "## Task 2: Retention siyosatlari" AND the
     REVIZIYA block at the top of
     `C:\TBuild\tdesktop\docs\superpowers\plans\2026-07-29-multi-device-sync-04-storage-lifecycle.md`.
  7. Read spec §0.3 in
     `C:\TBuild\tdesktop\docs\superpowers\specs\2026-07-29-multi-device-sync-backend-design.md`.
  8. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K4  Idempotent: evaluating the same input twice gives the same answer.
  K6  TDD: write the failing tests first, watch them fail, implement.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments and XML docs in Uzbek, identifiers in English.
  Do NOT run the server (`dotnet run`). `dotnet test`, `dotnet build`
  and `dotnet ef migrations add` are required and fine.
  Do NOT modify anything under `C:\TBuild\tdesktop\docs\sync-protocol\`.
  Do NOT touch the tdesktop repository at all.

============================================================
YOUR TASK — PLAN 04, TASK 2 (RETENTION POLICY MODEL)
============================================================

------------------------------------------------------------
🔴 1. A RETENTION MECHANISM ALREADY EXISTS — AND IS DEAD
------------------------------------------------------------

`SettingsService` already seeds a full set of retention settings:

    retention.deleted_days         0   (0 = cheksiz)
    retention.edited_days          0
    retention.activity_days       90
    retention.ghost_read_days      0
    retention.setting_days         0
    retention.peer_directory_days  0
    retention.media_index_days     0

**Nothing reads them.** Grep the repository: they are seeded, exposed
through the settings endpoint, and never enforced. An operator can set
`retention.activity_days = 30` today and nothing happens.

So this task must not introduce a *second*, parallel notion of
retention. Two mechanisms that both decide what to delete is how data
gets deleted twice over, or silently not at all.

**Make the policy evaluator the single path.** The seeded settings
become an implicit, lowest-priority policy synthesised at evaluation
time: for each kind whose `retention.<kind>_days` is greater than zero,
behave as if a policy existed with that `OlderThanDays`, `Action =
delete_only`, and a priority below every stored policy. `0` means
unlimited and synthesises nothing.

If you conclude a different reconciliation is better, implement the one
above anyway and argue your case in the report. What you must NOT do is
leave the settings unreferenced while adding a second mechanism beside
them.

------------------------------------------------------------
🔴 2. TWO RULES THAT PROTECT DATA — BOTH ARE ABSOLUTE
------------------------------------------------------------

**(a) Retention NEVER creates a tombstone.** Spec §0.3. A tombstone is
the user deliberately deleting something, and it propagates to every
device. Retention is local storage management on one server. If
retention emitted tombstones, cleaning up the server's disk would erase
the archive on the user's laptop, phone and capture service — the exact
opposite of what the archive exists for. Nothing in your diff may
create, enqueue or reference a tombstone.

**(b) Server retention must outlast the client's.** tdesktop prunes
`activity_history` at **30 days** (`kActivityRetentionDays`). The
server is the central archive and must keep data **longer** — the
seeded default is 90 days for exactly this reason.

If a policy would delete records younger than the client's own
retention, the data disappears from the server while clients still hold
it, and it then never reaches a device that was offline. That is
permanent, silent data loss across devices.

So: add a **floor**. A configurable minimum (`retention.min_days`,
default 30, read through `SettingsService`) below which a policy's
`OlderThanDays` is refused. The evaluator must not apply such a policy,
and the model must expose why, so Task 8's UI can say so. Do not
silently clamp the value to the floor — a policy the operator wrote as
"7 days" must not quietly behave as "30 days"; it must be reported as
invalid.

------------------------------------------------------------
3. THE MODEL
------------------------------------------------------------

Create `RetentionPolicyEntity` as the plan describes it — the field
list there is good. Match the formatting style of the neighbouring
entities (aligned property names, Uzbek XML docs).

Beyond the plan:

  - Add the `DbSet` to `SyncDbContext` and configure the key the way
    the existing entities are configured.
  - **Add the EF migration.** The plan forgets this entirely; without
    it the table does not exist and every later task fails at runtime.
    Follow the existing migration naming.
  - `Action` is one of the three constants. Reject an unknown action at
    evaluation time rather than treating it as "delete".

------------------------------------------------------------
🔴 4. THE EVALUATOR MUST BE A PURE FUNCTION
------------------------------------------------------------

This is the part that will be wrong if it is written as a service that
queries the database.

Write the decision as a pure, synchronous function with no DbContext,
no I/O and no clock lookup inside it — the caller passes the current
time in. The plan-02 scheduler (`NextAction` in tdesktop's
`custom_sync.cpp`) is the model to copy: policy in a pure function,
plumbing outside it, tests that assert exact answers.

Shape it roughly like:

    public sealed record RetentionCandidate(
        string Kind, string PeerHash, bool HasMedia, DateTime ReceivedAt);

    public sealed record RetentionDecision(
        bool ShouldAct, string Action, string? TargetId,
        string? PolicyId, string Reason);

    public static RetentionDecision Evaluate(
        RetentionCandidate candidate,
        IReadOnlyList<RetentionPolicyEntity> policies,
        IReadOnlyDictionary<string, int> settingsDays,
        int minDays,
        DateTime now);

`Reason` is not decoration — it is what the audit log and the UI show
when an operator asks why a record was or was not selected.

Evaluation order, and nothing else:

    1. Any matching never_delete policy      -> ShouldAct = false
    2. Highest-priority matching valid policy -> its Action applies
    3. Implicit policy from settings (§1)     -> delete_only
    4. Nothing matches                        -> ShouldAct = false

**The default is that nothing is deleted.** Until the operator writes a
policy or sets a non-zero retention day count, every record survives.

Matching: a null/empty `Kind` matches every kind; a null/empty
`PeerHash` matches every peer; `MediaOnly` narrows to candidates with
media. `Enabled = false` never matches. A policy whose `OlderThanDays`
is below the floor never matches and must be reported as invalid.

Ties on `Priority` must be broken deterministically — pick a rule
(e.g. the most specific match, then `PolicyId` ordinal) and say which
in your report. A non-deterministic tie-break makes deletion depend on
row order.

------------------------------------------------------------
5. THE TESTS
------------------------------------------------------------

Write them first and watch them fail (K6). At minimum:

  1. No policies, all settings zero -> nothing is deleted.
  2. `never_delete` wins over a higher-priority `archive_then_delete`
     that also matches.
  3. Highest priority wins among ordinary matching policies, and the
     returned `PolicyId` is the one you expect.
  4. A record younger than `OlderThanDays` is not selected.
  5. A policy with `OlderThanDays` below the floor is refused, is
     reported as invalid, and does NOT fall back to the floor value.
  6. The implicit settings policy applies when no stored policy matches,
     and loses to any stored policy that does.
  7. `Enabled = false` never matches.
  8. An unknown `Action` string does not delete.

These are pure-function tests: no database, no fixture. Put them in
their own test class.

Then break your implementation deliberately — a DIFFERENT break from
the obvious one, for example make `never_delete` respect `Priority`
instead of outranking everything — and confirm a named test FAILS
before restoring it. Report which break and which test.

`dotnet test` must be fully green, all 115 existing tests included.

------------------------------------------------------------
6. WHAT NOT TO ADD
------------------------------------------------------------

  - **No deletion.** Nothing in this task removes a row. Deletion is
    Task 6 and it is two-phase: archive, verify, only then delete.
  - No archive targets (Tasks 3-5), no scheduling (Task 7).
  - No Web UI (Task 8, and it belongs to plan 03).
  - No endpoint for managing policies unless you need one to test;
    CRUD belongs with the UI work.
  - Do not change `StatsService`, `SummaryAsync` or any existing
    endpoint's response shape.
  - Do not seed example policies. An empty policy table means "keep
    everything", and that is the correct state for a server nobody has
    configured yet.

============================================================
DEFINITION OF DONE
============================================================

  - `RetentionPolicyEntity` created, DbSet added, configured, and an
    EF migration generated
  - `retention.min_days` added to the seeded settings, default 30
  - Evaluation is a pure function taking `now` as a parameter
  - Seeded `retention.<kind>_days` participate as the lowest-priority
    implicit policy — they are no longer dead configuration
  - `never_delete` outranks everything regardless of priority
  - A policy under the floor is refused and reported, never clamped
  - Deterministic tie-break, stated in the report
  - Nothing creates or references a tombstone
  - Eight tests above, written first and seen failing; you watched a
    deliberate break fail
  - `dotnet test` fully green
  - One commit, K7 style; no scratch files committed

============================================================
FINAL REPORT — seven short points
============================================================

  1. `dotnet test` summary line (total / passed / failed).
  2. Which deliberate break you used and which test caught it.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. Your evaluation order in code, and the tie-break rule you chose.
  5. How the seeded `retention.<kind>_days` settings feed the
     evaluator, and what happens when one of them is 0.
  6. The migration file name, and confirmation that `dotnet build`
     succeeds with it.
  7. Anything ambiguous you had to guess at, and whether you agree that
     a policy below the floor should be refused rather than clamped.

OUT OF SCOPE
  - No capture service (plan 05), no web controller (plan 03).
  - Do not deploy anything and do not run the server.
  - Do not delete any data, anywhere, for any reason.
