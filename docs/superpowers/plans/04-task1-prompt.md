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
  1. Read `src/CustomSync.Services/StatsService.cs` IN FULL. Most of
     this task already exists there. Section 1 says what that means.
  2. Read `src/CustomSync.Data/Entities/RecordEntity.cs` and
     `MediaBlobEntity.cs` — the exact field names you may use.
  3. Read `src/CustomSync.Services/SettingsService.cs` — how
     configuration is read from the `server_settings` table (rule K1).
  4. Read `tests/CustomSync.Tests/Fixtures/DatabaseFixture.cs` and one
     existing test class to copy the established style.
  5. Read `src/CustomSync.Api/Endpoints/StatsEndpoints.cs` — the
     existing `/api/v1/stats/storage` endpoint and its authorization.
  6. Read the plan section "## Task 1: Xotira metrikasi va o'sish
     prognozi" in
     `C:\TBuild\tdesktop\docs\superpowers\plans\2026-07-29-multi-device-sync-04-storage-lifecycle.md`,
     including the REVIZIYA block at the top of that file.
  7. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code — config lives in
      `server_settings` (`SettingsService`).
  K4  Idempotent reads: calling the metric twice changes nothing.
  K6  TDD: write the failing test first, watch it fail, then implement.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments and XML docs in Uzbek, identifiers in English.
  Do NOT run the server (`dotnet run`). `dotnet test` and
  `dotnet build` are required and fine.
  Do NOT modify anything under `C:\TBuild\tdesktop\docs\sync-protocol\`.
  Do NOT touch the tdesktop repository at all.
  Do NOT add migrations or change any entity in this task.

============================================================
YOUR TASK — PLAN 04, TASK 1 (STORAGE METRICS + GROWTH)
============================================================

------------------------------------------------------------
🔴 1. DO NOT CREATE `StorageMetricsService` — EXTEND WHAT EXISTS
------------------------------------------------------------

The plan tells you to create
`src/CustomSync.Services/Storage/StorageMetricsService.cs` with
`SummaryAsync` and `ByPeerAsync`. **That plan text is out of date.**
`StatsService` already implements both, with the same queries:

  - `StorageAsync()` returns
    `StorageStat(RecordCount, RecordBytes, MediaCount, MediaBytes)` —
    identical aggregation to the plan's `SummaryAsync`, including the
    `count == 0` guards that avoid `SumAsync` on an empty set.
  - `PeersAsync()` returns
    `PeerStat(PeerHash, RecordCount, TotalBytes, LastOccurredAt)` —
    this IS the plan's `ByPeerAsync`, and better: it also carries
    `LastOccurredAt` and supports ordering.

Creating a second service would give the project two implementations of
the same aggregation that will drift apart, and `/api/v1/stats/storage`
would keep using the old one.

So:

  - **Add** `SummaryAsync(long? diskCapacityBytes, CancellationToken)`
    to `StatsService`, returning a new `StorageSummary` record that
    carries the four existing numbers plus `TotalBytes`,
    `DailyGrowthBytes` and `DaysUntilFull`.
  - **Do NOT** re-create `ByPeerAsync`. Say in your report that
    `PeersAsync` already covers it.
  - **Do NOT** change `StorageAsync` or `StorageStat`. The existing
    `/api/v1/stats/storage` endpoint keeps its current response shape;
    breaking it is out of scope and there is no client to migrate.

If, having read the file, you are convinced a separate class is right
after all, do NOT create it — say so in your report with your reasoning
and implement it as instructed above.

------------------------------------------------------------
🔴 2. THE `null` VS `0` RULE — THIS IS THE POINT OF THE TASK
------------------------------------------------------------

`DaysUntilFull` is `int?`. It is **null** when the answer does not
exist:

  - growth is zero — a server that is not growing never fills
  - `diskCapacityBytes` was not supplied

It is `0` only when the disk is genuinely already full or over
capacity.

Returning `0` for "no growth" would render in the panel as an urgent
"disk full today" warning on an idle server. That is the single most
likely defect in this task, and both required tests target it.

------------------------------------------------------------
🔴 3. TWO DEFECTS IN THE PLAN'S OWN GROWTH CODE — FIX BOTH
------------------------------------------------------------

The plan's `DailyGrowthAsync` is:

    var cutoff = DateTime.UtcNow.AddDays(-7);
    var recent = await db.Records.Where(r => r.ReceivedAt >= cutoff)
                   .SumAsync(r => (long?)r.PayloadSize, ct) ?? 0;
    return recent / 7;

**(a) It always divides by 7, even on a server younger than 7 days.**
A server that has been collecting for 2 days reports 2/7 of its real
growth rate — so `DaysUntilFull` comes out roughly 3.5x too
optimistic, exactly when the operator most needs a true number.

Divide by the **observed** window instead: the number of days between
the oldest record still inside the window and now, clamped to at least
1 day and at most 7. Derive the oldest timestamp from the data
(`MIN(ReceivedAt)`), not from a guess.

**(b) It ignores media entirely.** Growth counts only
`Records.PayloadSize`, while `MediaBlobs.Size` is normally the larger
consumer — that is the whole reason this plan exists. Include media
uploaded inside the same window; `MediaBlobEntity.UploadedAt` is the
timestamp to use.

Both fixes must be visible in the returned `DailyGrowthBytes`.

------------------------------------------------------------
4. WINDOW LENGTH AND DISK CAPACITY ARE CONFIGURATION (K1)
------------------------------------------------------------

Do not hardcode `7` or any disk size in the service.

  - The growth window length is a setting, e.g.
    `storage.growth_window_days`, default 7, read through
    `SettingsService`. Follow the naming and default-handling style the
    existing settings already use — read them before inventing a key.
  - `diskCapacityBytes` stays a **parameter** of `SummaryAsync`. The
    caller resolves it. Do not read it inside the service and do not
    give it a hardcoded fallback: an invented disk size produces a
    confident, wrong "days until full".

A short constant explaining *why* 7 is the default (shorter windows
swing on a single quiet day, longer ones hide growth that just
started) belongs in a comment, not as a magic number in the query.

------------------------------------------------------------
5. THE TESTS
------------------------------------------------------------

Write them FIRST and watch them fail (K6). At minimum:

  1. Empty database → `TotalBytes` 0, `DailyGrowthBytes` 0,
     `DaysUntilFull` **null**, and no divide-by-zero.
  2. Zero growth with a real `diskCapacityBytes` → `DaysUntilFull`
     **null** (not 0).
  3. Records inside the window, server younger than the window →
     growth reflects the **observed** days, not a division by 7.
     Assert the exact number.
  4. Media inside the window contributes to growth. Assert the exact
     number, and assert it differs from the records-only figure.
  5. Disk already over capacity → `DaysUntilFull` is 0, not negative.

Then break your implementation deliberately — for example make
`DaysUntilFull` return 0 instead of null when growth is zero — and
confirm a named test FAILS before restoring it. Report which break you
used and which test caught it.

`dotnet test` must end with every test passing, and you must not have
broken any of the existing 105.

------------------------------------------------------------
6. WHAT NOT TO ADD
------------------------------------------------------------

  - **No Web UI.** Plan 04 Task 8 and the `.vue` files belong to plan
    03, which is not written yet. Nothing under `web/` in this task.
  - No retention policy, no purge, no archive targets — Tasks 2-7.
  - No new endpoint unless you need one; if you add one, it is a GET
    under the existing `/api/v1/stats` group with the same
    authorization as its neighbours, and it must not replace
    `/storage`.
  - No entity changes, no migrations.
  - Do not touch anything to do with tombstones. Server-side retention
    never creates them (spec §0.3) — that rule matters in Task 2, and
    nothing in this task should approach it.

============================================================
DEFINITION OF DONE
============================================================

  - `SummaryAsync` added to `StatsService`; no second metrics service
  - `StorageAsync`, `StorageStat` and `PeersAsync` unchanged
  - `DaysUntilFull` null on zero growth and on missing capacity; 0 only
    when actually full
  - Growth divides by the observed window, clamped to 1..window
  - Growth includes media bytes in the window
  - Window length read from settings; no hardcoded disk size anywhere
  - Five tests above, written first and seen failing; you watched a
    deliberate break fail
  - `dotnet test` fully green, existing tests included
  - One commit, K7 style; no scratch files committed

============================================================
FINAL REPORT — seven short points
============================================================

  1. `dotnet test` summary line (total / passed / failed).
  2. Which deliberate break you used and which test caught it.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. The exact growth formula you shipped, including how you clamp the
     window.
  5. The settings key you added and its default.
  6. Confirmation that `StorageAsync`, `StorageStat` and `PeersAsync`
     are untouched.
  7. Anything ambiguous you had to guess at, and whether you agree that
     extending `StatsService` was right.

OUT OF SCOPE
  - No capture service (plan 05), no web controller (plan 03).
  - Do not deploy anything and do not run the server.
  - Do not change any existing endpoint's response shape.
