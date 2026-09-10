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
  1. Read `src/CustomSync.Services/Storage/RetentionPolicy.cs` — Task 2
     shipped the evaluator. Note that it decides `archive_then_delete`
     but nothing acts on that yet.
  2. Read `src/CustomSync.Api/Program.cs` around line 68: how
     `Storage:MediaRoot` is resolved and injected. That is the
     established convention for filesystem roots in this project.
  3. Read `src/CustomSync.Services/MediaService.cs` — how it builds
     paths and guards them.
  4. Read `src/CustomSync.Services/InterchangeService.cs` and the
     `CmxReader`/`CmxWriter` it uses — the archive format already
     exists; you are NOT writing it in this task.
  5. Read one existing test class that touches the filesystem, to copy
     the established style for temp directories and cleanup.
  6. Read "## Task 3: Arxiv target interfeysi va qo'lda yuklab olish"
     and the "## Xavfsizlik tamoyili" section in
     `C:\TBuild\tdesktop\docs\superpowers\plans\2026-07-29-multi-device-sync-04-storage-lifecycle.md`.
  7. Read spec §0.8 in
     `C:\TBuild\tdesktop\docs\superpowers\specs\2026-07-29-multi-device-sync-backend-design.md`.
  8. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code — no hardcoded paths.
  K4  Idempotent where it makes sense; uploading the same archive name
      twice must not corrupt anything.
  K6  TDD: write the failing tests first, watch them fail, implement.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments and XML docs in Uzbek, identifiers in English.
  Do NOT run the server (`dotnet run`). `dotnet test` and
  `dotnet build` are required and fine.
  Do NOT modify anything under `C:\TBuild\tdesktop\docs\sync-protocol\`.
  Do NOT touch the tdesktop repository at all.

============================================================
YOUR TASK — PLAN 04, TASK 3 (ARCHIVE TARGET SEAM)
============================================================

This task defines the seam that the two-phase purge (Task 6) depends
on: **nothing is ever deleted without a verified archive.** Everything
here exists to make that sentence enforceable.

------------------------------------------------------------
🔴 1. THE INTERFACE IS MISSING THE ONE THING THE MANUAL TARGET NEEDS
------------------------------------------------------------

The plan's `IArchiveTarget` has `HealthCheckAsync`, `UploadAsync` and
`DownloadAsync`, and says verification means reading the archive back
and comparing SHA-256.

For the automatic targets that is right. **For the manual target it is
a lie**, and a dangerous one.

`ManualDownloadTarget` writes the archive to a staging directory on the
server's own disk. `DownloadAsync` then reads it back from that same
directory, the checksum matches, and Task 6 concludes "archive
verified, safe to delete". But nobody downloaded anything. The archive
is sitting on the same disk that is about to be purged, and if that
disk is the problem, the archive dies with the data.

The plan is aware of this — it describes a "Xavfsiz saqladim" button —
but it never puts that anywhere the purge logic can see.

So add it to the interface:

    /// <summary>
    /// Checksum tekshiruvi YETARLI emasmi. Qo'lda yuklab olishda arxiv
    /// serverning o'z diskida qoladi, ya'ni uni qayta o'qish "zaxira
    /// xavfsiz joyda" degani EMAS -- buni faqat odam tasdiqlaydi.
    /// </summary>
    bool RequiresExplicitConfirmation { get; }

`ManualDownloadTarget` returns `true`. Any automatic target returns
`false`. Task 6 will refuse to delete when this is true and no
confirmation has been recorded — say in your report that you left that
enforcement to Task 6, because this task adds no deletion.

If you think a different shape expresses this better, implement the one
above and argue in the report.

------------------------------------------------------------
🔴 2. `Path.Combine(stagingRoot, archiveName)` IS A PATH TRAVERSAL
------------------------------------------------------------

The plan's code writes straight to `Path.Combine(stagingRoot,
archiveName)`. If `archiveName` ever contains `..` or an absolute path,
`Path.Combine` happily escapes the staging directory — an absolute
second argument discards the first entirely. This is a server; treat
the name as untrusted even though today's only caller is internal.

Validate: reject any name that is not a plain file name (no directory
separators, no `..`, not rooted), and after combining, verify the
resolved full path is still inside the resolved staging root. Return a
failed `ArchiveUploadResult` rather than throwing.

Test it with `../../etc/passwd` and with a rooted path.

------------------------------------------------------------
🔴 3. WHERE THE STAGING DIRECTORY LIVES — AND WHERE IT MUST NOT
------------------------------------------------------------

`stagingRoot` is a constructor parameter in the plan with no source.
Resolve it the way `Storage:MediaRoot` is already resolved in
`Program.cs` — from `IConfiguration`, as `Storage:ArchiveStagingRoot` —
and inject it. No path literal in the service.

🔴 **It must not sit inside the media root.** If archives are staged
under `Storage:MediaRoot`, they are counted by the storage metrics and
by `storage.quota_total_mb`, so archiving would inflate the very number
that triggered the purge, and a media scan could treat them as blobs.
Fail loudly at startup if the configured staging root resolves to a
path inside the media root, rather than discovering it in production.

------------------------------------------------------------
4. THE UPLOAD PATH ITSELF
------------------------------------------------------------

The plan's `UploadAsync` returns `Success: true` unconditionally and
has no error handling. A full disk, a permission error or a cancelled
request all become an unhandled exception in the middle of a purge.

  - Wrap the write and the read-back; on failure return
    `new ArchiveUploadResult(false, null, null, <sabab>)`.
  - On failure, delete the partial file. A half-written archive that
    looks present is worse than no archive: Task 6 might verify a
    truncated file.
  - Write to a temporary name and rename into place once the write
    succeeds, so a crash mid-write cannot leave a plausible-looking
    archive.
  - Compute SHA-256 by reading the file back from disk, not from the
    input stream. Reading back is what proves what actually landed.

`DownloadAsync` returning null must be the only "not there" signal; do
not throw for a missing file.

------------------------------------------------------------
5. STAGED ARCHIVES ARE THEMSELVES A DISK LEAK
------------------------------------------------------------

This is a storage-management plan, and the manual target's staging
directory grows forever: every archive stays until someone removes it
by hand.

Add a way to enumerate and remove staged archives — for example
`ListStagedAsync()` and `DeleteStagedAsync(location)` on
`ManualDownloadTarget` (not on the interface; this is specific to
manual staging). Do NOT wire any automatic cleanup in this task:
deleting a staged archive before the user has downloaded it destroys
the only copy. Just make it possible, and say in your report that the
retention of staged archives is an open question for Task 6 or 7.

------------------------------------------------------------
6. WHAT NOT TO ADD
------------------------------------------------------------

  - **No purge, no deletion of records or media.** Task 6.
  - No S3, SFTP or Telegram targets. Tasks 4 and 5, deliberately
    deferred — the point of this task is that they become droppable
    later without touching the purge flow.
  - No `.cmx` writing. The format exists; whoever builds the archive
    stream is Task 6's problem.
  - No scheduling or background service. Task 7.
  - No Web UI or endpoint. Task 8, which belongs to plan 03.
  - Do not change `StatsService`, the retention evaluator, or any
    existing endpoint.

------------------------------------------------------------
HOW TO VERIFY
------------------------------------------------------------

Tests first, watched failing (K6). At minimum:

  1. Round trip: upload a stream, `DownloadAsync` returns identical
     bytes, and the returned SHA-256 matches an independently computed
     hash of the original content.
  2. `../../etc/passwd` as `archiveName` is refused, nothing is written
     outside the staging root.
  3. A rooted path as `archiveName` is refused.
  4. `DownloadAsync` on a location that does not exist returns null and
     does not throw.
  5. A failed upload leaves no file behind.
  6. `RequiresExplicitConfirmation` is true for the manual target.
  7. `HealthCheckAsync` creates the staging directory when absent.

Use a temp directory per test and clean it up. Do not write anywhere
under the repository, and do not touch `Storage:MediaRoot`.

Then break your implementation deliberately — a DIFFERENT break from
the obvious one, for example make the traversal check compare the raw
strings instead of the resolved full paths — and confirm a named test
FAILS before restoring it. Report which break and which test.

`dotnet test` must be fully green, all 124 existing tests included.

============================================================
DEFINITION OF DONE
============================================================

  - `IArchiveTarget` created, including `RequiresExplicitConfirmation`
  - `ManualDownloadTarget` created; staging root injected from
    `Storage:ArchiveStagingRoot`, no path literal in the service
  - Startup refuses a staging root inside the media root
  - Path traversal refused, verified against the resolved full path
  - Failed uploads return an error result and leave no partial file
  - Temp-name-then-rename write; checksum computed by reading back
  - Staged archives can be listed and removed; nothing removes them
    automatically
  - Seven tests above, written first and seen failing; you watched a
    deliberate break fail
  - `dotnet test` fully green
  - One commit, K7 style; no scratch files committed

============================================================
FINAL REPORT — seven short points
============================================================

  1. `dotnet test` summary line (total / passed / failed).
  2. Which deliberate break you used and which test caught it.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. How you validate `archiveName`, and how you check the resolved
     path is inside the staging root.
  5. Where `Storage:ArchiveStagingRoot` is read and what happens when
     it is missing or points inside the media root.
  6. What `UploadAsync` does on a mid-write failure.
  7. Anything ambiguous you had to guess at, and whether you agree that
     `RequiresExplicitConfirmation` belongs on the interface rather
     than only on the manual target.

OUT OF SCOPE
  - No capture service (plan 05), no web controller (plan 03).
  - Do not deploy anything and do not run the server.
  - Do not delete any record, media blob or staged archive automatically.
