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
  1. Read `custom_sync_crypto.h` in full. Every primitive you need
     already exists and is verified against `test-vectors.json`:
     `Pbkdf2`, `Seal`, `Open`, `RandomBytes`, `KeyFingerprint`.
  2. Read `Outbox::EnsureMasterKeyCreated`, `LoadMasterKey` and
     `MasterKey` in `custom_sync_outbox.cpp`.
  3. Read `custom_sync_keystore.h` — DPAPI protect/unprotect.
  4. Read `Client::enroll` and the parser functions in
     `custom_sync_client.cpp` — you are adding three calls in the same
     shape.
  5. Read spec §4.4 ("Qulf ochish usullari (key wrapping)") and §4.4.1
     in `docs/superpowers/specs/2026-07-29-multi-device-sync-backend-design.md`.
  6. Read `tools/sync-selftest/README.md` and its `CMakeLists.txt`.
  7. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K5  Sync off => ZERO behaviour change.
  K6  TDD: extend the selftest first, watch it FAIL, then implement.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop or any server.
  Do NOT start a full tdesktop build (~34 minutes). Not needed here.
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real database.
  Do NOT touch the customsync-server repository. Its side is DONE.

============================================================
YOUR TASK — TASK 12a (KEY SHARING, CLIENT CRYPTO + TRANSPORT)
============================================================

This is a new task, added after plan 02 task 10, and it is the highest
priority in the project.

**Why it exists.** Every device currently calls
`Outbox::EnsureMasterKeyCreated()`, which generates a **random** master
key. Two devices therefore hold two different keys. Their `peer_hash`
values differ, so their `record_id` values differ, so nothing dedups —
and records pulled from the other device never decrypt. The whole point
of the product is several devices sharing one archive, and today that
is impossible.

The fix is the passphrase wrap the spec already designs in §4.4:
`KEK = PBKDF2(passphrase, salt, iterations)` and
`wrapped = AES-GCM(KEK, nonce, masterKey)`. The server stores the wrap
and never sees the passphrase or the key.

You are doing the crypto and the transport. The UI flow is 12b.

Create `Telegram/SourceFiles/custom_sync_keyshare.h` / `.cpp`, add
three calls to `Client`, and cover it in the selftest.

------------------------------------------------------------
1. THE SERVER SIDE ALREADY EXISTS — MATCH IT EXACTLY
------------------------------------------------------------

Do not design an endpoint. These are live in
`customsync-server/src/CustomSync.Api/Endpoints/KeyEndpoints.cs`:

    GET    /api/v1/keys/wraps          list (any authenticated device)
    GET    /api/v1/keys/wraps/{id}     one wrap (rate limited)
    POST   /api/v1/keys/wraps          create  -- ADMIN ROLE ONLY
    DELETE /api/v1/keys/wraps/{id}     admin only

**JSON is snake_case** on this server (it is configured with
`JsonNamingPolicy.SnakeCaseLower`). So POST sends:

    { "wrap_type": "passphrase", "label": "...", "salt": "<base64>",
      "nonce": "<base64>", "wrapped_key": "<base64>", "iterations": 600000 }

and GET `/{id}` returns `wrap_id`, `wrap_type`, `label`, `salt`,
`nonce`, `wrapped_key`, `iterations` — the three byte fields as
**base64**, not hex. Getting this wrong produces a wrap that decodes to
garbage and fails only at unwrap time with an unhelpful error.

POST returns `{ "wrap_id": "..." }`.

**Role matters.** An enrolled device gets role `device` by default;
`admin` comes from an enrolment code minted with `--admin`. So POST
will return 403 on an ordinary device. That is correct behaviour, not a
bug — only the first device creates the wrap. Surface the 403 as a
clear error string rather than a generic failure, because 12b needs to
explain it to the user.

------------------------------------------------------------
2. THE API TO WRITE
------------------------------------------------------------

    namespace CustomSync::KeyShare {

    struct Wrap {
        QString wrapId;
        QString wrapType;   // "passphrase" | "recovery" | "email"
        QString label;
        QByteArray salt;
        QByteArray nonce;
        QByteArray wrappedKey;
        int iterations = 0;
    };

    // Master kalitni paroldan chiqarilgan KEK bilan o'raydi.
    // salt va nonce har chaqiruvda YANGI tasodifiy qiymat bo'ladi.
    [[nodiscard]] Wrap WrapMasterKey(
        const QByteArray &masterKey,
        const QString &passphrase,
        const QString &label);

    // Ochadi. Parol noto'g'ri bo'lsa -- bo'sh optional (AES-GCM tegi
    // mos kelmaydi). Bu yagona to'g'ri xato signali.
    [[nodiscard]] std::optional<QByteArray> UnwrapMasterKey(
        const Wrap &wrap,
        const QString &passphrase);

    } // namespace CustomSync::KeyShare

Keep this file free of `custom_db`, `Client` and Qt beyond QtCore, so
the selftest can link it — the same discipline `custom_sync_crypto.cpp`
follows.

`iterations` for `wrap_type == "passphrase"` is 600 000 per spec §4.4.
It is a **protocol constant**, not a tunable: put it in a named
constant with a comment saying it must match what other platforms use,
and read it back from the wrap when unwrapping rather than assuming it
(§4.4.1 uses 2 000 000 for email escrow, and that path will exist
later).

Salt is 16 bytes, nonce is 12 bytes — both from `Crypto::RandomBytes`.

On `Client`, add three calls in the existing async shape:

    void listKeyWraps(Fn<void(bool ok, QVector<Wrap> wraps, QString error)>);
    void getKeyWrap(const QString &wrapId, Fn<void(bool ok, Wrap wrap, QString error)>);
    void createKeyWrap(const Wrap &wrap, Fn<void(bool ok, QString wrapId, QString error)>);

plus free `Parse...` functions next to the existing ones, so the
parsing is testable without a network.

------------------------------------------------------------
🔴 3. NEVER GENERATE A KEY WHEN A WRAP EXISTS
------------------------------------------------------------

This is the rule the whole task exists to enforce.

Add to `Outbox`:

    // Serverdan ochilgan master kalitni O'RNATADI. Lokal kalit allaqachon
    // bo'lsa -- FALSE qaytaradi va hech narsani o'zgartirmaydi.
    [[nodiscard]] bool AdoptMasterKey(const QByteArray &masterKey);

`AdoptMasterKey` must refuse to overwrite an existing
`master_key_protected`. Overwriting it would make every record this
device already pushed permanently undecryptable — there is no undo, and
the user would not find out until much later.

`EnsureMasterKeyCreated()` already refuses to overwrite; leave that
behaviour exactly as it is. Do NOT make it consult the server — 12b
decides the order (list wraps first, generate only if the list is
empty), and putting a network call inside it would break K5 and the
selftest at once.

⚠️ Note in your report, do not fix: two devices enrolling at the same
moment could both see an empty wrap list and both generate a key. For a
single-user setup this is unlikely, and the real defence is 12b's UI
order, not a lock here.

------------------------------------------------------------
🔴 4. PBKDF2 DOES NOT RUN ON THE UI THREAD
------------------------------------------------------------

600 000 iterations is seconds of solid CPU. Called from a button
handler it freezes the message pump and Windows paints "Not
Responding".

Earlier tasks said this was hypothetical. **It is real now** — you are
writing the first code that actually calls `Pbkdf2`.

So `WrapMasterKey` and `UnwrapMasterKey` must be plain synchronous
functions with no Qt event-loop dependency, and their contract must say
in a comment that callers run them off the main thread via `crl::async`
and post the result back with `crl::on_main`. 12b does the wiring;
`ExportFullBackupAsync` is the pattern in this codebase.

Do not add threading inside these functions. A pure synchronous
function is what makes them testable in the selftest.

------------------------------------------------------------
5. FINGERPRINT — CHEAP AND WORTH IT
------------------------------------------------------------

`Crypto::KeyFingerprint` already exists. Expose the current key's
fingerprint through `Outbox` so 12b can print it on every device. Two
devices showing the same short string is the only way a user can
actually confirm the sharing worked; without it they are trusting a
green checkmark.

------------------------------------------------------------
HOW TO VERIFY
------------------------------------------------------------

Add `custom_sync_keyshare.cpp` to `tools/sync-selftest/CMakeLists.txt`
and a section to `main.cpp` in the established style — a count guard
and a per-family result line. Required cases:

  1. Round trip: wrap a known 32-byte key, unwrap it, bytes match.
  2. Wrong passphrase -> empty optional. Not a crash, not garbage.
  3. Tampered `wrappedKey` (flip one byte) -> empty optional.
  4. Tampered `salt` -> empty optional.
  5. Two wraps of the same key with the same passphrase have
     **different** salt and nonce, and both unwrap to the same key.
     This is the one that catches a hardcoded or reused salt.
  6. `iterations` is read from the wrap, not assumed: build a wrap with
     a deliberately different iteration count and confirm unwrapping
     honours it.
  7. Parsing: feed `ParseKeyWrapResponse` a literal snake_case JSON
     body with base64 fields and assert every field, including that
     base64 — not hex — was decoded.

Case 5 and case 7 are the ones that matter. A reused salt silently
destroys the security property, and a hex/base64 mix-up is the single
most likely way this task fails in the field.

Then break your implementation deliberately — for example make the
salt a fixed constant — and confirm the suite FAILS before restoring
it. Say which break you used and which case caught it.

**Syntax check.** `/Zs` at `/W4 /WX` on every .cpp you changed, using
the absolute include paths in the selftest README.

Note case 1–6 will take a few seconds each: 600 000 PBKDF2 iterations
are not free. That is expected; do not lower the count to speed the
test up.

============================================================
DEFINITION OF DONE
============================================================

  - `custom_sync_keyshare.h/.cpp` created, registered in
    `Telegram/CMakeLists.txt` and in the selftest CMakeLists
  - No dependency on `custom_db`, `Client` or Qt beyond QtCore
  - Three `Client` calls plus free parser functions
  - snake_case request/response, base64 byte fields
  - 403 from POST surfaced as a distinct, readable error
  - `Outbox::AdoptMasterKey` added and refuses to overwrite
  - `EnsureMasterKeyCreated` behaviour unchanged; no network inside it
  - Key fingerprint exposed
  - `iterations` read from the wrap, never assumed
  - 7 selftest cases with a count guard; you watched a break fail
  - `/Zs` clean at `/W4 /WX`; selftest exit 0
  - One commit, K7 style; no scratch files committed

============================================================
FINAL REPORT — seven short points
============================================================

  1. The selftest line for the keyshare family, and exit code.
  2. Which break you used and which case caught it.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. Confirm salt and nonce are fresh per wrap, and say where they come
     from.
  5. What `AdoptMasterKey` does when a local key already exists.
  6. `/Zs` result per file.
  7. Anything ambiguous you had to guess at.

OUT OF SCOPE
  - No UI (12b): no passphrase prompt, no enrolment flow change.
  - Do not call these from `enroll` or the orchestrator yet.
  - No recovery codes, no email escrow (spec §4.4.1) — leave the
    2 000 000 path unbuilt but do not hardcode 600 000 on unwrap.
  - Do not touch the server repository.
  - Do not enable sync or change any default.
