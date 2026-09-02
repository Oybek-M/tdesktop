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
  1. Read `tools/sync-selftest/README.md`. It has the exact build
     command, including two environment traps that will cost you time
     if you rediscover them yourself.
  2. Build and run the selftest as it stands. Expected right now:
     `11/11 holat`, roundtrip checks pass, exit code 0.
     If that is not what you get, STOP and say so.
  3. Read the plan section:
     `docs/superpowers/plans/2026-07-29-multi-device-sync-02-tdesktop-agent.md`,
     section "## Task 2: Kriptografik primitivlar".
  4. Read `docs/sync-protocol/test-vectors.json` — the actual contract.
  5. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K6  TDD: extend the selftest first, watch it FAIL, then implement.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop or any server. Building and running
     `sync_selftest` is expected and fine.
  Do NOT start a full tdesktop build (~34 minutes, not needed).
  Do NOT touch `Telegram/CMakeLists.txt`.
  Do NOT modify anything under `docs/sync-protocol/`. The vectors are
     the cross-platform contract; .NET already matches them.

============================================================
YOUR TASK — PLAN 02, TASK 2
============================================================

Fill in the two stub files created in Task 1:

  Telegram/SourceFiles/custom_sync_crypto.h
  Telegram/SourceFiles/custom_sync_crypto.cpp

and extend `tools/sync-selftest/main.cpp` to verify the four remaining
vector families against them.

The plan's API design (Step 3, the header) is GOOD — implement it as
written. The plan's TEST code (Step 1) is almost entirely wrong. Details
below.

------------------------------------------------------------
THE PLAN'S TEST CODE DOES NOT MATCH THE VECTOR FILE
------------------------------------------------------------

The plan was written in July against an imagined vector layout. Every
JSON path in its Step 1 is wrong. Copying it gives you empty loops that
check nothing and report success — the same failure mode Task 1's case
counter exists to prevent.

Here is what is actually in the file.

--- `hkdf` — an object, NOT an array of cases ---

    "hkdf": {
      "master_key_hex": "000102...1e1f",
      "derived": {
        "customsync-content-v1": "ef4be29d...",
        "customsync-media-v1":   "7547ea67...",
        "customsync-peer-v1":    "b6d7bc75...",
        "customsync-account-v1": "8761ba1a..."
      }
    }

So: one master key, and `derived` is a MAP from info-string to expected
hex. Iterate the keys of `derived`. There is no `salt_hex` field and no
`expected_key_hex` field.

**The salt is 32 ZERO BYTES**, always, for every derivation. It is not
in the file because it never varies. "No salt" and "32 zero bytes" are
NOT the same thing to every library — that difference is a classic
silent interop break, which is exactly why the plan's header comment
insists the salt be passed explicitly. Pass `QByteArray(32, '\0')`.

**There are FOUR info strings, not three.** Spec section 5's diagram
shows only three (content, media, peer); `customsync-account-v1` was
added by spec 0.12. The vector file is authoritative.

--- `pbkdf2` — key name and field names both differ ---

The plan reads `root["pbkdf2_hmac_sha256"]` with `input` /
`expected_key_hex`. Reality:

    "pbkdf2": {
      "cases": [
        { "password": "parol123",
          "salt_hex": "000102030405060708090a0b0c0d0e0f",
          "iterations": 600000,
          "kek_hex": "8a9685a5..." }
      ]
    }

Key is `pbkdf2`, cases are under `.cases`, fields are flat, and the
expected value is `kek_hex`. `password` is a plain UTF-8 string (one
case contains spaces and a hyphen), `salt_hex` is hex.

One case uses 2,000,000 iterations and will take a second or two. That
is expected — do NOT lower it to make the run faster.

--- `account_hash` and `peer_hash` — missing from the plan entirely ---

The plan predates spec 0.12 and does not test these at all. Add both.

    account_hash: HMAC-SHA256(account_key, account_id)[0..16] -> lowercase hex
    peer_hash:    HMAC-SHA256(peer_key,    peer_id   )[0..16] -> lowercase hex

where

    account_key = HkdfSha256(master, zeros32, "customsync-account-v1", 32)
    peer_key    = HkdfSha256(master, zeros32, "customsync-peer-v1",    32)

and `master` is `hkdf.master_key_hex` from the same file. Both families
have 3 cases under `.cases`, with fields `account_id` / `account_hash`
and `peer_id` / `peer_hash`.

`account_id` and `peer_id` are **DECIMAL STRINGS**, not numbers — hash
the string bytes, not an integer. One case in each family is `"0"`.

The first 16 BYTES of the HMAC, hex-encoded, gives 32 hex characters.

--- `aes_gcm` — the plan never checks the vectors at all ---

The plan only does a self-roundtrip and a tamper test. Both are worth
keeping, but they would pass even if your GCM were subtly incompatible
with every other platform. The file has three exact cases:

    { "name": "bo'sh matn",
      "key_hex": "ef4be29d...",       <- this is the content key
      "nonce_hex": "000102030405060708090a0b",
      "plaintext_hex": "",
      "ciphertext_hex": "",
      "tag_hex": "e3c8ef0e702aafd77ff79dbcda2c0520" }

Check them in BOTH directions: encrypt plaintext and compare ciphertext
and tag independently, then decrypt and recover the plaintext.

**The tag trap.** `ciphertext_hex` does NOT contain the tag — they are
separate fields. Some libraries append the tag to the ciphertext; if
you compare a combined buffer against `ciphertext_hex` everything fails
and it looks like a key problem.

Our own `Seal()` returns `ciphertext || tag` combined, and that is
CORRECT — it is the wire format (spec: `payload` is base64 of
"ciphertext+tag"). Do not change the API. In the test, split the last
16 bytes off and compare the two halves separately.

Note the empty-plaintext case: ciphertext is empty, tag is not. Make
sure your code handles a zero-length input without shortcutting.

------------------------------------------------------------
REQUIRED COUNT GUARDS
------------------------------------------------------------

Like Task 1's `checkedCases != 11` guard, assert the count for each
family and return non-zero if it is wrong:

    hkdf          4 derived keys
    account_hash  3 cases
    peer_hash     3 cases
    aes_gcm       3 cases  (each checked encrypt AND decrypt)
    pbkdf2        3 cases

An empty loop that reports success is worse than no test at all. This
has already happened once in this plan.

------------------------------------------------------------
OTHER NOTES
------------------------------------------------------------

- The .NET reference implementation of the same primitives is in
  `customsync-server/src/CustomSync.Core/CryptoPrimitives.cs`, and the
  equivalent test is `tests/CustomSync.Tests/CryptoVectorTests.cs`.
  Useful if a value disagrees and you need a second opinion.
- `KeyFingerprint` is NOT covered by any vector. Implement it per the
  spec — SHA256("customsync-fingerprint-v1" || master_key)[0..8] — but
  do not claim it is verified.
- `RandomBytes` must come from `RAND_bytes`, not `qrand`/`std::rand`.
  Check its return value; a silent failure here produces predictable
  keys.
- OpenSSL is already linked by `tools/sync-selftest/CMakeLists.txt`
  (`OpenSSL::Crypto`), so no build changes are needed.
- Use `QStringLiteral(...)`, not `u"..."_qs`.
- Keep the plan's `Open()` returning `std::optional` and returning
  empty on tag mismatch. A corrupt record must not crash the app.

============================================================
DEFINITION OF DONE
============================================================

  - `sync_selftest <test-vectors.json>` exits 0
  - It reports, by count: 11 record_id, 4 hkdf, 3 account_hash,
    3 peer_hash, 3 aes_gcm, 3 pbkdf2 — all passing
  - The tamper test still shows a modified tag is REJECTED
  - You watched the new checks FAIL before implementing (K6)
  - `Telegram/CMakeLists.txt` untouched; no full tdesktop build
  - One commit, K7 style

If any vector does not match, STOP and report which one, with expected
and actual hex. Do not edit the vector file.

============================================================
FINAL REPORT — five short points
============================================================

  1. Selftest output: the per-family counts, and the exit code.
  2. `git show --stat HEAD` (stat block only) and the commit subject.
  3. Confirm `Telegram/CMakeLists.txt` and `docs/sync-protocol/` were
     not modified.
  4. Whether the empty-plaintext AES case needed any special handling.
  5. Anything wrong or ambiguous you had to guess at.

OUT OF SCOPE
  - Do not start Task 3 (schema migration).
  - Do not add third-party libraries; OpenSSL has HKDF, PBKDF2, HMAC
    and AES-GCM.
