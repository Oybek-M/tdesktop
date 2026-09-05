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
  1. Read `tools/sync-selftest/README.md` for the build command.
  2. Build and run the selftest as it stands. Expected:
     11 record_id, 4 hkdf, 3 account_hash, 3 peer_hash, 3 aes_gcm,
     3 pbkdf2 — all passing, exit code 0. If not, STOP and say so.
  3. Read `Telegram/SourceFiles/custom_sync_outbox.cpp`, function
     `Enqueue` — specifically the `KeysAvailable()` gate and the empty
     `recordId` guard below it.
  4. Read the plan section:
     `docs/superpowers/plans/2026-07-29-multi-device-sync-02-tdesktop-agent.md`,
     section "## Task 5: Kalit saqlash (OS keystore)".
  5. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K5  Sync off => ZERO behaviour change.
  K6  TDD: extend the selftest first, watch it FAIL, then implement.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop or any server. Building and running
     `sync_selftest` is expected and fine.
  Do NOT start a full tdesktop build (~34 minutes).
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real database at `<ArchiveRoot>/db/actioned_messages.db` (ArchiveRoot is a user setting; resolve it via `dbFilePath()` in custom_db.cpp:113).

============================================================
YOUR TASK — PLAN 02, TASK 5
============================================================

Create `Telegram/SourceFiles/custom_sync_keystore.h` / `.cpp`, register
them in `Telegram/CMakeLists.txt`, and extend the selftest to prove the
DPAPI round-trip works.

------------------------------------------------------------
🔴 1. DO NOT TURN `KeysAvailable()` ON IN THIS TASK
------------------------------------------------------------

It is tempting: the keystore is exactly the thing `KeysAvailable()` is
waiting for, and wiring it looks like finishing the job.

It is not. `Enqueue()` still has no `record_id` computation — the
variable is declared, left empty, and a guard below returns early when
it is empty. Flip the flag now and every enqueue call silently returns
at that guard. No error, no log, no rows: the outbox simply never
fills, and the cause sits two files away from the symptom.

The master key does not exist yet either. It is generated during device
enrollment, which is **Task 6**. A keystore with nothing to store is
still correct and still worth having on its own.

So: leave `KeysAvailable()` returning `false`, and update its comment to
point at Task 6 rather than Task 5, so the next person is not sent
looking in the wrong place.

------------------------------------------------------------
🔴 2. THE PLAN'S API CANNOT BE TESTED — RESTRUCTURE IT
------------------------------------------------------------

The plan has `Store()` and `Load()` call `Outbox::SetState()` /
`GetState()` directly. That drags `custom_sync_outbox.cpp` in, which
drags `custom_db.cpp` in, which drags in SQLite, Qt widgets and most of
tdesktop. The selftest links none of that, so Task 5 as written would
ship with **zero** automated verification of the one thing that can
actually go wrong: the DPAPI call.

Split the OS crypto from the persistence. The keystore should know
nothing about the database:

    namespace CustomSync::Keystore {

    [[nodiscard]] bool Available();

    // OS himoyasi ostiga oladi. Windows: DPAPI.
    [[nodiscard]] std::optional<QByteArray> ProtectBytes(
        const QByteArray &plain);

    // Qaytaradi. Blob buzilgan, boshqa foydalanuvchi yoki boshqa
    // mashinada yaratilgan bo'lsa -- bo'sh optional.
    [[nodiscard]] std::optional<QByteArray> UnprotectBytes(
        const QByteArray &blob);

    } // namespace CustomSync::Keystore

Persisting the protected blob — base64 into `sync_state` under
`master_key_protected` — belongs to the caller, in Task 6, where the
key is created. That is better layering anyway: the keystore does OS
crypto, the outbox does storage, and neither needs to know the other
exists.

Keep the plan's Windows body almost verbatim; you are only moving the
two `Outbox::` calls out.

------------------------------------------------------------
3. `Crypt32` IS ALREADY LINKED — do not add it again
------------------------------------------------------------

The plan says to check. It is there:

    cmake/options_win.cmake:145     Crypt32

linked into `common_options` for every Windows target, and delay-loaded
via `/DELAYLOAD:crypt32.dll` (`Telegram/CMakeLists.txt:2481`). Adding a
second `target_link_libraries` entry would be redundant noise.

In `Telegram/CMakeLists.txt` add only the two new source files, bare
filenames, next to the other `custom_sync_*` entries.

The selftest's own `tools/sync-selftest/CMakeLists.txt` already links
`crypt32` (added in Task 2), so adding `custom_sync_keystore.cpp` to its
source list is all it needs.

------------------------------------------------------------
4. Use application entropy, and never change it
------------------------------------------------------------

The plan calls `CryptProtectData` with `nullptr` for `pOptionalEntropy`.
Pass a fixed application-specific byte string instead, and the same one
to `CryptUnprotectData`.

Be honest about what this buys: the entropy sits in our binary, so it is
not a secret. It does mean a blob lifted out of `sync_state` cannot be
unwrapped by generic "dump all DPAPI blobs for this user" tooling
without also pulling the constant out of the executable. That is a low
wall, but it costs one line.

The cost of getting it wrong is high: **changing this value later makes
every already-stored key permanently unreadable.** Put that warning in a
comment directly above the constant.

Note the second parameter of `CryptProtectData` is `szDataDescr` — a
human-readable description, not entropy. The plan's
`L"CustomSync master key"` is correct there; keep it.

------------------------------------------------------------
5. Non-Windows
------------------------------------------------------------

`Available()` returns false, both functions return `std::nullopt`. The
comment in the plan explains the intent — each launch will prompt for
the passphrase instead. Keep that reasoning in the file.

------------------------------------------------------------
HOW TO VERIFY
------------------------------------------------------------

Add a keystore section to `tools/sync-selftest/main.cpp`, guarded by
`#ifdef Q_OS_WIN`, with a count guard in the established style. Four
checks:

  1. Round-trip: protect 32 random bytes, unprotect, bytes match.
  2. Empty input: protecting an empty QByteArray does not crash and
     round-trips to empty.
  3. Tampered blob: flip one byte in the middle of the protected blob;
     `UnprotectBytes` must return an empty optional, not garbage and not
     a crash.
  4. Not-a-blob: pass 16 bytes of junk; must return empty optional.

Check 3 is the one that matters. DPAPI blobs are integrity-protected, so
a correct implementation rejects tampering — but only if you actually
check the `CryptUnprotectData` return value instead of assuming success
and reading `output.pbData`, which on failure is uninitialised.

On non-Windows the section reports "skipped" and does not count as a
failure.

============================================================
DEFINITION OF DONE
============================================================

  - `custom_sync_keystore.h/.cpp` created, no dependency on
    `custom_sync_outbox` or `custom_db`
  - `KeysAvailable()` still returns false; its comment now says Task 6
  - Application entropy used on both protect and unprotect, with the
    "never change this" warning
  - `Crypt32` not added to CMake again; two source files registered
  - Selftest: 4 new keystore checks passing on Windows, plus the
    existing 27 vector checks still passing, exit code 0
  - You watched the new checks FAIL before implementing (K6)
  - No full tdesktop build
  - One commit, K7 style

============================================================
FINAL REPORT — six short points
============================================================

  1. Selftest output: the per-family counts including keystore, and the
     exit code.
  2. `git show --stat HEAD` (stat block only) and the commit subject.
  3. Confirm `KeysAvailable()` still returns false.
  4. Confirm the keystore includes neither `custom_sync_outbox.h` nor
     `custom_db.h`.
  5. What the tampered-blob check did (returned empty / crashed / other).
  6. Anything wrong or ambiguous you had to guess at.

OUT OF SCOPE
  - Do not start Task 6 (HTTP client, enroll, push).
  - Do not generate or store a master key anywhere.
  - Do not write to `sync_state`.
