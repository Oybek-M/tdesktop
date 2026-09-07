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
  1. Read `custom_sync_keyshare.h` — `Wrap`, `WrapMasterKey`,
     `UnwrapMasterKey`, `kPassphraseIterations`. Task 12a built all of
     it and it is verified in the selftest.
  2. Read `Outbox::AdoptMasterKey`, `EnsureMasterKeyCreated`,
     `LoadMasterKey` and the fingerprint accessor in
     `custom_sync_outbox.cpp`.
  3. Read `Client::listKeyWraps`, `getKeyWrap`, `createKeyWrap` in
     `custom_sync_client.cpp`.
  4. Read `custom_tab_sync.cpp` end to end — you are extending it, not
     replacing it. Keep its layout idioms.
  5. Read how `ExportFullBackupAsync` uses `crl::async` +
     `crl::on_main` in `custom_db.cpp`.
  6. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K5  Sync off => ZERO behaviour change.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: UI text and code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop. Do NOT run any server.
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real database.
  Do NOT touch the customsync-server repository.

🔴 **Do NOT run `cmake --build` on the Telegram target.** A full build
takes ~34 minutes and ~15 GB of RAM, and the machine runs other heavy
applications; starting one unannounced stalls the user's work. This
task DOES need a full build to be verified -- `custom_tab_sync.cpp`
cannot be checked with `/Zs` -- but **the user runs it**. Finish the
code, say the build is pending, and do not report the task as verified.
Building and running `sync_selftest` is fine (seconds).
Do not touch the `cmake` submodule; it carries a local `/MP6` change.

============================================================
YOUR TASK — TASK 12b (KEY SHARING, UI FLOW)
============================================================

Task 12a shipped the crypto and the three HTTP calls. Nothing calls
them yet. You are wiring the flow into the sync tab so that several
devices end up holding **one** master key.

------------------------------------------------------------
🔴 1. THE ORDER IS THE WHOLE TASK
------------------------------------------------------------

Get this wrong and the user loses their archive. There is no undo.

After a successful `enroll`, **always list the wraps first**:

    listKeyWraps()
      |
      +-- list is NOT empty  -> this device JOINS an existing archive
      |     ask for the passphrase
      |     getKeyWrap(id) -> UnwrapMasterKey -> AdoptMasterKey
      |     NEVER generate a key on this path
      |
      +-- list IS empty      -> this device CREATES the archive
            ask for a new passphrase (twice, must match)
            generate 32 bytes in memory
            WrapMasterKey -> createKeyWrap (POST)
            only after POST SUCCEEDS: AdoptMasterKey

🔴 **Do not call `EnsureMasterKeyCreated()` anywhere in this task.**

It persists the key immediately. If you use it and the POST then fails
— and it will fail with 403 on any device not enrolled with an
`--admin` code — the device is left holding a locally stored key that
was never uploaded. `AdoptMasterKey` refuses to overwrite an existing
key, so that device can now never join the shared archive, and the only
way out is deleting `master_key_protected` by hand.

Generate with `Crypto::RandomBytes(32)`, keep it in a local variable,
and adopt it **only** once the server has the wrap. That ordering is
precisely why 12a added `AdoptMasterKey` as a separate function.

**403 on POST** means the device lacks the admin role. Say that in
plain Uzbek — that a device which creates the archive must be enrolled
with an admin code — rather than showing a raw HTTP error. Do not
retry, and do not fall back to generating a local key.

------------------------------------------------------------
🔴 2. PBKDF2 GOES ON A BACKGROUND THREAD
------------------------------------------------------------

600 000 iterations is seconds of solid CPU. Called from a click handler
it freezes the message pump and Windows paints "Not Responding" over
the app. Every earlier task deferred this; **this is the task where it
becomes real**, and it is the only place in the whole sync feature that
genuinely needs another thread.

    crl::async([=] {
        auto result = KeyShare::UnwrapMasterKey(wrap, passphrase);
        crl::on_main([=] {
            // UI ni FAQAT shu yerda yangilaymiz
        });
    });

Both `WrapMasterKey` and `UnwrapMasterKey` are pure synchronous
functions with no Qt event-loop dependency — 12a wrote them that way
on purpose. Do not add threading inside them.

While it runs: disable the button, show progress text. A user who
clicks twice must not start two derivations.

Do not capture raw widget pointers into the `crl::async` lambda without
a guard — the window can close while it runs. Use the weak-pointer
pattern the neighbouring tabs already use.

------------------------------------------------------------
3. THE PASSPHRASE ITSELF
------------------------------------------------------------

  - Masked input (`Ui::PasswordInput` or the field's password mode).
  - **Never stored anywhere.** Not in settings, not in `sync_state`,
    not in a member that outlives the operation. Clear the field after
    use. Your report must state that a grep of your diff finds no write
    of the passphrase to disk.
  - Never logged. No `qDebug` of it, not even truncated.
  - Creation path asks twice and refuses a mismatch.
  - Show a warning before creating: **if the passphrase is forgotten,
    every record on the server becomes permanently unreadable.**
    Recovery codes exist in the spec (§4.4) but are not built, so today
    the passphrase is the only copy.

Minimum length: enforce something, put the number in a named constant
with a comment, and say the rule in the label rather than only
rejecting after the fact.

------------------------------------------------------------
4. FINGERPRINT — THE ONLY REAL CONFIRMATION
------------------------------------------------------------

Show the current key's fingerprint prominently, and make it easy to
compare: the user reads it on device A and checks device B matches.

Without it a user has only a green checkmark, and a green checkmark is
exactly what a device with the *wrong* key also shows — that is the
failure this whole task exists to remove.

Also add a row saying whether the key is present at all
("Kalit: mavjud" / "Kalit: yo'q — ro'yxatdan o'ting").

------------------------------------------------------------
5. THE ALREADY-HAS-A-DIFFERENT-KEY CASE
------------------------------------------------------------

A device may already hold a locally generated key from an earlier build
while the server holds a wrap made elsewhere. `AdoptMasterKey` will
refuse, correctly.

Detect it and explain it: local key present, server wrap present,
fingerprints differ. Show what that means — this device's own archive
cannot be merged automatically — and stop. Do NOT offer a button that
deletes the local key: that silently destroys everything this device
already pushed.

If you think such an action is needed, say so in your report and leave
it out.

------------------------------------------------------------
6. WHAT NOT TO ADD
------------------------------------------------------------

  - No recovery codes, no email escrow (spec §4.4.1).
  - No wrap deletion UI (`DELETE /keys/wraps/{id}` exists; leave it).
  - No changing an existing passphrase — that means re-wrapping and
    deleting the old wrap, and it is its own task.
  - No unenrol button (still out, same reason as Task 10).

------------------------------------------------------------
HOW TO VERIFY
------------------------------------------------------------

  1. Selftest green before your change and after (exit 0). Your change
     should not touch it.
  2. Do NOT build. Hand the build to the user and say so plainly.
  3. Grep your own diff and confirm: no `EnsureMasterKeyCreated`, no
     passphrase written to `sync_state` or settings, no `Pbkdf2` call
     outside a `crl::async` lambda.

You cannot run tdesktop, so the flow is not exercised. That is
accepted, and it is why the ordering rules above are written as
absolutes rather than suggestions: nobody will catch a mistake here
before it reaches real data.

============================================================
DEFINITION OF DONE
============================================================

  - Wrap list is consulted before any key is created
  - Joining path never generates; creating path adopts only after a
    successful POST
  - `EnsureMasterKeyCreated` not called anywhere in the diff
  - 403 explained as "admin code needed", with no fallback
  - `Pbkdf2` only ever reached through `crl::async`, UI updated via
    `crl::on_main`, buttons disabled while it runs
  - Passphrase masked, never stored, never logged, asked twice on
    creation, minimum length in a named constant
  - Forgotten-passphrase warning shown before creation
  - Fingerprint displayed; mismatch case detected and explained with no
    destructive button offered
  - Selftest exit 0; full build NOT run, handed to the user
  - One commit, K7 style

============================================================
FINAL REPORT — seven short points
============================================================

  1. `git show --stat HEAD` (stat block only) and the commit subject.
  2. The exact branch order your code takes after a successful enrol.
  3. Confirm `EnsureMasterKeyCreated` appears nowhere in the diff, and
     what you call instead.
  4. Where `Pbkdf2` ends up running, and how the UI is updated after.
  5. Your Uzbek wording for the forgotten-passphrase warning and for
     the 403 message.
  6. Confirmation that you did not start a full build.
  7. Anything ambiguous you had to guess at, and whether you think a
     "delete local key" action is needed.

OUT OF SCOPE
  - No WebSocket (Task 9), no capture service (plan 05).
  - Do not change the orchestrator, the retention filter, or any
    enqueue site.
  - Do not enable sync by default or change any default value.
