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
  1. Read `tools/sync-selftest/README.md` for the build command.
  2. Build and run the selftest. Expected: 11 record_id, 4 hkdf,
     3 account_hash, 3 peer_hash, 3 aes_gcm, 3 pbkdf2, 4 keystore —
     all passing, exit 0. If not, STOP and say so.
  3. Read `Telegram/SourceFiles/custom_sync_outbox.cpp` — `KeysAvailable()`,
     `Enqueue()` and the two guards inside it.
  4. Read `Telegram/SourceFiles/custom_sync_keystore.h`.
  5. Read the plan section:
     `docs/superpowers/plans/2026-07-29-multi-device-sync-02-tdesktop-agent.md`,
     section "## Task 6: HTTP klient — enroll va push".
  6. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code. Tunables go in CustomSettings.
  K2  Never block the UI thread. All network calls asynchronous.
  K5  Sync off => ZERO behaviour change.
  K6  TDD: extend the selftest first, watch it FAIL, then implement.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.

  🔴 Do NOT run any server. Not the CustomSync backend, not a mock, not
     `dotnet run`. This is an absolute rule in this project.
  🔴 Do NOT start the full tdesktop build (~34 minutes). The plan's
     Step 4 says to ask the user first — that decision is the user's,
     not yours. Report that it is pending instead.
  🔴 Do NOT perform the plan's Step 5 (manual end-to-end check). Every
     part of it needs a running server and a running tdesktop. Leave it
     to the user and say so.
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real `custom_mod.db`.

============================================================
YOUR TASK — PLAN 02, TASK 6
============================================================

Settings, an HTTP client, master-key creation, and the wiring that
finally makes the outbox live.

------------------------------------------------------------
🔴 THE ONE THING THAT MATTERS: FOUR CHANGES, OR NONE
------------------------------------------------------------

Tasks 4 and 5 deliberately left the outbox inert. `KeysAvailable()`
returns `false`, and `Enqueue()` has a second guard that returns when
`recordId` is empty.

This task turns it on. Those four pieces must land TOGETHER:

  1. A master key exists and is loadable.
  2. `Enqueue()` actually computes `record_id`.
  3. `KeysAvailable()` returns true only when 1 and 2 can both succeed.
  4. The client can push what the outbox produces.

Doing 3 without 2 is the trap. Every enqueue would hit the empty-
`recordId` guard and return silently: no error, no log, no rows, and
the outbox stays permanently empty while everything *looks* wired up.
The symptom appears in `custom_db.cpp`; the cause sits in a different
file behind a guard that is doing its job correctly.

Doing 2 without 1 is worse. A `record_id` computed from a placeholder
key looks valid, pushes fine, and produces ids no other device can ever
reproduce — dedup matches nothing and every record silently duplicates.
The only cleanup afterwards is wiping server state.

`KeysAvailable()` should be, in effect:

    CustomSettings::SyncEnabled()
        && !SyncServerUrl().isEmpty()
        && master key successfully loaded

------------------------------------------------------------
MASTER KEY — create once, never regenerate
------------------------------------------------------------

On successful enrollment, if and only if no key is stored yet:

  - 32 bytes from `Crypto::RandomBytes(32)` (which uses `RAND_bytes`)
  - `Keystore::ProtectBytes()` it
  - base64 into `Outbox::SetState("master_key_protected", ...)`

🔴 If `master_key_protected` already exists, DO NOT overwrite it. Every
record ever pushed is derived from that key; replacing it orphans the
entire history — the old records become undecryptable and their
`record_id`s unreproducible. Re-enrolment must reuse the existing key.

🔴 Write this in a comment, because it is a real and dangerous gap:
**a second device will generate a DIFFERENT master key today.** Sharing
one key across devices needs the passphrase-wrap flow
(`/api/v1/keys/wraps`, already implemented server-side) plus a
passphrase prompt, which is a later task. Until then, enrolling a second
device produces two devices that cannot read each other's records and
whose `record_id`s never match — and nothing warns you. Say this
explicitly in your report.

Cache the derived keys (`content`, `peer`, `account`) after the first
load. `Enqueue()` runs inside capture paths; a DPAPI call plus three
HKDF derivations per captured event is not acceptable there.

------------------------------------------------------------
MAKE `record_id` COMPUTABLE FROM THE SELFTEST
------------------------------------------------------------

Put the composition in a pure function, in `custom_sync_record.h/.cpp`
(no database, no settings, no network):

    [[nodiscard]] QString ComputeRecordIdFor(
        const QByteArray &masterKey,
        const QString &kind,
        qint64 accountId,
        const QString &peerId,
        qint64 msgId,
        qint64 occurredAt);

It derives `account_key`/`peer_key` via HKDF, computes the two hashes,
and calls the existing `ComputeRecordId`. Remember spec 0.12:
`account_hash` is the EMPTY STRING when `kind == "activity"`, and a
real hash for every other kind.

`Enqueue()` then calls this with the cached master key.

Why a separate pure function: it is the single piece whose failure
poisons everything downstream, and it is the only part of this task the
selftest can reach. Anything living in `custom_sync_outbox.cpp` drags
in SQLite and cannot be tested.

------------------------------------------------------------
THE SERVER CONTRACT — verified against the running code
------------------------------------------------------------

**All JSON is snake_case, in both directions.** The server changed to
this on 2026-09-02 specifically so the wire, the `.cmx` format and this
client all agree. Do not use camelCase.

    POST /api/v1/devices/enroll          (no Authorization header)
      -> {"code": "...", "name": "...", "platform": "..."}
      200 {"device_id", "refresh_token", "access_token", "expires_at"}
      400 {"error": "invalid_or_used_code"}

    POST /api/v1/devices/refresh         (no Authorization header)
      -> {"device_id": "...", "refresh_token": "..."}
      200 {"refresh_token", "access_token", "expires_at"}
      401 on a bad or already-rotated token

    POST /api/v1/sync/push               Bearer
      -> {"records": [ ...record objects... ]}
      200 {"results": [{"record_id", "status", "seq", "message"}]}
      400 {"error": "batch_too_large", "max": N, "received": M}

    GET  /api/v1/sync/pull?since=&limit= Bearer
      200 {"records": [...], "next_since": N, "has_more": bool}

    HEAD /api/v1/media/{hash}            Bearer  -> 200 exists / 404 not
    PUT  /api/v1/media/{hash}            Bearer  -> raw encrypted bytes
                                         413 too large, 507 quota full
    GET  /api/v1/media/{hash}            Bearer  -> raw bytes

`status` is one of: `created`, `duplicate`, `superseded`, `error`.
`duplicate` and `superseded` are SUCCESS — remove the row from the
outbox. Only `error` is retryable.

The record object on the wire is exactly what `ToJson()` already
produces. Do not build a second serializer.

------------------------------------------------------------
CORRECTIONS TO THE PLAN
------------------------------------------------------------

--- 1. `mediaUpload` has a `nonce` parameter the server never sees ---

The plan declares
`mediaUpload(hash, encrypted, nonce, done)`. The PUT body is the
encrypted blob and nothing else — the server reads `request.Body`
straight into a buffer. The nonce travels inside the record, in
`media[].nonce`. Drop the parameter.

Related, and easy to get backwards: **the hash is over the PLAINTEXT,
before encryption** (spec 0.5). Hashing the ciphertext would break
dedup across devices, because each device encrypts with a different
nonce and would upload the same photo under a different hash every time.

--- 2. Refresh-token rotation is a one-shot ---

`/devices/refresh` rotates the refresh token: the old one dies the
moment the response is generated. If you use the new access token and
save the new refresh token afterwards — or crash in between — the
device is permanently locked out and needs a fresh enrollment code.

Persist the new refresh token FIRST, then continue.

Refresh proactively, on a margin before `expires_at`, rather than
waiting for a 401. `expires_at` is ISO-8601 UTC; parse with
`Qt::ISODateWithMs`.

--- 3. Do not hardcode the batch size ---

The server caps a push batch (currently 500) and answers 400
`batch_too_large` with the real `max`. K1: put the client's chunk size
in `CustomSettings`, and on `batch_too_large` adopt the `max` the server
reported instead of failing the rows.

--- 4. Settings: add a fourth value ---

Beyond the plan's `syncEnabled`, `syncServerUrl`, `syncIntervalSeconds`,
add the push chunk size. Follow the existing `custom_settings.cpp`
pattern exactly — `Init()`, the update function, and the getter.

`syncEnabled` defaults to **false**, and nothing may touch the network
until it is explicitly on. That is rule K5 and it is the whole reason
the default exists.

--- 5. Threading ---

`QNetworkAccessManager` is not thread-safe and must stay on one thread.
`Enqueue()` is called from the database path. Keep the `Client` on the
GUI thread and make sure no reply handler touches the outbox from
another thread.

------------------------------------------------------------
HOW TO VERIFY — no server, no full build
------------------------------------------------------------

Add ONE new selftest section, `record_id_from_master`, with a count
guard in the established style.

Using `hkdf.master_key_hex` from `test-vectors.json` as the master key,
call `ComputeRecordIdFor(...)` and check it reproduces the published
`record_id` for at least these three cases from the vector file:

  - a `deleted` case  (real account_hash path)
  - an `activity` case (empty account_hash path)
  - the `media_index` case with `msg_id = -5190442718973336697`

You will need the `account_id` and `peer_id` **decimal strings** whose
hashes appear in the vectors — `account_hash.cases` and
`peer_hash.cases` give you both sides of that mapping.

This is worth the effort: it proves the client's whole hashing chain —
master key, HKDF, both HMACs, the empty-`account_hash` rule, negative
`msg_id` — produces exactly the ids the server and .NET already agree
on. It is the only part of Task 6 that can be verified without a
network, and it is the part whose failure is silent and expensive.

The HTTP paths cannot be tested here. Structure `Client` so the
response parsing is a separate free function taking a `QByteArray`, so a
later task can test it without a socket.

============================================================
DEFINITION OF DONE
============================================================

  - Four settings added, `syncEnabled` defaulting to false
  - `custom_sync_client.h/.cpp` created and registered in
    `Telegram/CMakeLists.txt` (bare filenames)
  - Master key created on first enrol only; existing key never replaced
  - `ComputeRecordIdFor()` pure, in `custom_sync_record.*`
  - `Enqueue()` computes `record_id`; `KeysAvailable()` gated on
    settings AND a loadable key
  - Selftest: new `record_id_from_master` section passing, all 31
    existing checks still passing, exit 0
  - You watched the new checks FAIL before implementing (K6)
  - No server run, no full build, no manual end-to-end attempt
  - One commit, K7 style

============================================================
FINAL REPORT — seven short points
============================================================

  1. Selftest output: per-family counts and exit code.
  2. `git show --stat HEAD` (stat block only) and the commit subject.
  3. What exactly makes `KeysAvailable()` true now.
  4. Where the master key is created, and what stops it being replaced.
  5. Confirm you did not run a server, a full build, or the manual test.
  6. Confirm the second-device key-sharing gap is documented in code.
  7. Anything wrong or ambiguous you had to guess at.

OUT OF SCOPE
  - Do not start Task 7 (pull and local merge).
  - Do not implement the passphrase key-wrap flow.
  - Do not add UI. Settings values only; the settings screen is Task 10.
