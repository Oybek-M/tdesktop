Execute the task described below. It is an implementation job in an
existing C++/Qt repository — not a document to review, summarise or
score. The only output that counts is committed source code.

REPOSITORY
  C:\TBuild\tdesktop
  Branch: Oybek. Never push to a remote named "upstream".

HOW TO REPORT
  Work quietly. Do not narrate steps. Do not paste build logs.
  The final report must be SHORT — the five points at the bottom, a
  line or two each. Long reports are not useful here.

STEP 0 — READ BEFORE YOU WRITE ANYTHING
  1. The plan section:
     `docs/superpowers/plans/2026-07-29-multi-device-sync-02-tdesktop-agent.md`,
     section "## Task 1: Mustaqil test dasturi va `record_id`".
  2. The contract you must match: `docs/sync-protocol/test-vectors.json`.
  3. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K6  TDD: build the selftest first, watch it FAIL, then implement.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop or any server. Building and running the small
     `sync_selftest` binary is expected and fine — it prints and exits.
  Do NOT start a full tdesktop build. It takes ~34 minutes and is
     not needed for this task.
  Do NOT touch `Telegram/CMakeLists.txt`. The new files stay out of
     the main build until plan 02 Task 4. That is what keeps Task 1
     cheap.

============================================================
YOUR TASK — PLAN 02, TASK 1
============================================================

Create four files:

  tools/sync-selftest/CMakeLists.txt
  tools/sync-selftest/main.cpp
  Telegram/SourceFiles/custom_sync_record.h
  Telegram/SourceFiles/custom_sync_record.cpp

Plus an empty stub pair `custom_sync_crypto.h/.cpp` (filled in Task 2),
because the selftest compiles both translation units.

------------------------------------------------------------
FIVE THINGS THE PLAN GETS WRONG — READ CAREFULLY
------------------------------------------------------------

The plan was written in July, before spec 0.12. Following its code
literally produces a selftest that either fails or, worse, PASSES
WITHOUT CHECKING ANYTHING.

--- 1. `ComputeRecordId` takes FIVE arguments, not four ---

The plan's signature omits `accountHash`. The real formula (spec 0.12):

  record_id = hex(SHA256(
      kind || 0x00 || account_hash || 0x00 || peer_hash || 0x00 ||
      msg_id || 0x00 || occurred_at))

`accountHash` goes SECOND, right after `kind`. Numbers are decimal
strings with the sign preserved (`-42` and `42` are different records).
The 0x00 separator appears between fields but NOT after the last one.

`account_hash` is the EMPTY STRING when and only when
`kind == "activity"` — last-seen bypass is an objective fact about the
person being watched, so it merges across accounts. Every other kind
carries a real account_hash. The vectors cover both directions.

The .NET reference implementation is
`customsync-server/src/CustomSync.Core/RecordId.cs` if you want to
compare byte for byte.

--- 2. The JSON is NOT shaped the way main.cpp assumes ---

This is the dangerous one. The plan writes:

    for (const auto &item : root.value(u"record_id"_qs).toArray()) {
        const auto in = entry.value(u"input"_qs).toObject();
        ... entry.value(u"expected"_qs).toString()

Every part of that is wrong:

  - `record_id` is an OBJECT, not an array. `.toArray()` on an object
    returns an EMPTY array, so the loop body never runs, `gFailures`
    stays 0, and the program prints "Barcha vektorlar mos keldi" having
    verified NOTHING.
  - The cases live under `record_id.cases`.
  - There is no `input` / `expected` nesting. Each case is flat.

A real case looks exactly like this:

    {
      "kind": "deleted",
      "account_hash": "8ce7fd6f2c871df09e218375ad4bb5c4",
      "peer_hash": "cbcd16f7c84f024ee6791c08453e35e0",
      "msg_id": 395278,
      "occurred_at": 1787000000,
      "record_id": "25cc97f881e1b7dde2de8b11599e608476d8054e853a92f8d9b6619d6941cc0c"
    }

So iterate `root["record_id"]["cases"]` and read the flat fields.

REQUIRED GUARD: after the loop, assert that you actually checked
**11 cases**. If the count is 0, or anything other than 11, print an
error and return non-zero. A vector runner that silently checks nothing
is worse than no runner at all — it reports green forever.

--- 3. `toDouble()` corrupts one of the vectors ---

The plan reads `msg_id` with `qint64(in.value("msg_id").toDouble())`.

One vector has `msg_id = -5190442718973336697` (an avatar, stored as
`-photo_id`). That is far beyond 2^53, so the double round-trip changes
the value, the decimal string changes, and the SHA256 comes out wrong.
You would spend an hour hunting a hash mismatch that is really a JSON
parsing bug.

Use `QJsonValue::toInteger()` (Qt 6, exact qint64) everywhere you read
`msg_id`, `occurred_at` and `observed_at` — in `main.cpp` AND in
`FromJson`. Likewise when WRITING, pass `qint64` to `QJsonValue`, not
`double`. Delete the plan's "msg_id va JSON" note about revisiting this
in Task 2; fix it now.

--- 4. Two record kinds are missing ---

The plan's `Kind` namespace lists six. The protocol has eight — add:

    MediaIndex = "media_index"   // spec 0.4
    Tombstone  = "tombstone"     // spec 0.3

Both appear in the vectors, so omitting them means those cases cannot
be exercised.

--- 5. The `Record` struct is missing two fields ---

Add to `struct Record`:

    QString accountHash;      // 0.12, "" faqat kind=="activity" uchun
    QString targetRecordId;   // 0.13, faqat kind=="tombstone" uchun

Carry both through `ToJson` / `FromJson`.

`target_record_id` must travel in CLEARTEXT alongside the encrypted
payload (spec 0.13). The server cannot read `payload`, so a tombstone
whose target is only inside the ciphertext deletes nothing — the
deletion silently fails to propagate. Emit the field only for
tombstones; leave it out otherwise.

------------------------------------------------------------
WIRE FORMAT — snake_case (confirmed 2026-09-02)
------------------------------------------------------------

`ToJson` / `FromJson` keep the plan's snake_case keys:

    record_id, kind, account_hash, peer_hash, msg_id, occurred_at,
    observed_at, device_id, nonce, payload, target_record_id, media

The server was serving camelCase over HTTP until 2026-09-02; it now
serves snake_case, matching the spec, the `.cmx` format and this code.
Do not "helpfully" switch to camelCase.

`nonce` and `payload` are base64 strings. `media` is an array of
`{hash, size, nonce}`.

------------------------------------------------------------
OTHER NOTES
------------------------------------------------------------

- `u"..."_qs` may not compile on Qt 6.11. Use `QStringLiteral(...)`.
- `DiscriminatorFor` is NOT covered by any vector. Keep it as the plan
  writes it, but do not claim it is verified.
- Build environment:
    Qt      C:/TBuild/Libraries/win64/Qt-6.11.1
    OpenSSL C:/TBuild/Libraries/win64/openssl3
  Configure the selftest with `qt-cmake.bat` from that Qt's `bin/`,
  the same way `docs/self-update/qtwebsockets-module.md` does it.
- Task 1 needs only `Qt6::Core`. OpenSSL linkage can wait for Task 2,
  but leaving it in the CMakeLists now is harmless.

============================================================
DEFINITION OF DONE
============================================================

  - `sync_selftest <path-to-test-vectors.json>` exits 0
  - It reports **11 of 11** record_id cases checked, by count
  - You watched it FAIL before writing custom_sync_record.cpp (K6)
  - `Telegram/CMakeLists.txt` untouched; no full tdesktop build
  - One commit, K7 style

Sanity check you can do by hand: kind `activity` with account_hash `""`,
peer_hash `cbcd16f7c84f024ee6791c08453e35e0`, msg_id 0,
occurred_at 1787000002 must produce
`eea4dc5e11301c50d7460fc9ebc7f86be92f9dbf3b95ab661a004f4c57a3c919`.

If you cannot reach 11/11, STOP and report which case fails with its
expected and actual hash. Do not adjust the vectors — they are the
cross-platform contract and .NET already matches them.

============================================================
FINAL REPORT — five short points
============================================================

  1. Selftest output: how many cases checked, how many passed.
  2. `git show --stat HEAD` (stat block only) and the commit subject.
  3. Confirm `Telegram/CMakeLists.txt` was not modified.
  4. The exact cmake configure command that worked.
  5. Anything wrong or ambiguous you had to guess at.

OUT OF SCOPE
  - Do not start Task 2 (crypto). Stubs only.
  - Do not add third-party libraries.
  - Do not modify anything under `docs/sync-protocol/`.
