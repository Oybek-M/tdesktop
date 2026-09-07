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
  1. Read `custom_sync.h` and `custom_sync.cpp` in full: `NextAction`,
     `Orchestrator::arm`, `runCycle`, `onCycleFinished`, the watchdog
     at line 138, and the long-lived `_client` member.
  2. Read `custom_sync_client.h` — the comment about which thread
     `QNetworkAccessManager` lives on, and the private `_accessToken`
     and `_refreshing` members.
  3. Read `Client::enroll` and the refresh path in
     `custom_sync_client.cpp`: where the access token comes from and
     when it is replaced.
  4. Read `Telegram/CMakeLists.txt` — how sources and libraries are
     added.
  5. Read `tools/sync-selftest/README.md`, including the section on
     compiling a single file without a full build.
  6. This prompt overrides the plan wherever they disagree.

NON-NEGOTIABLE RULES
  K1  No configuration literals in code.
  K5  Sync off => ZERO behaviour change.
  K6  TDD: write the policy test first, watch it fail, then implement.
  K7  ONE commit. Imperative subject, WHY in the body, no
      "Key changes:" list, no Co-Authored-By trailer.
  Language: code comments in Uzbek, identifiers in English.
  Do NOT run tdesktop. Do NOT run any server.
  Do NOT modify anything under `docs/sync-protocol/`.
  Do NOT touch the user's real database.
  Do NOT touch the customsync-server repository.

🔴 **Do NOT run `cmake --build` on the Telegram target.** A full build
takes ~34 minutes (~8 incremental) and ~15 GB of RAM, and the machine
runs other heavy applications. The user runs it.

You CAN and MUST compile your own files — see VERIFICATION. A one-file
compile takes about a minute and is explicitly allowed.

🔴 **Do NOT touch the `cmake` submodule.** It carries a deliberate
local `/MP6` change, and a submodule edit does not travel with a push.
This matters here: `cmake/external/qt/CMakeLists.txt` is the obvious
place to add a Qt module and it is exactly the file you must not edit.
Section 1 says where the linkage goes instead.

============================================================
YOUR TASK — PLAN 02, TASK 9 (WEBSOCKET NOTIFICATIONS)
============================================================

Sync already works on a 30-second timer. This task cuts the latency to
about a second by letting the server say "there are changes, pull now".

It is an **optimisation over polling, never a replacement.** Every line
you write must keep that true.

------------------------------------------------------------
1. THE MODULE IS BUILT — THE LINKAGE IS NOT
------------------------------------------------------------

The plan's Step 1 asks you to check whether Qt WebSockets exists. It
does; that check is already done:

    C:/TBuild/Libraries/win64/Qt-6.11.1/lib/Qt6WebSockets.lib
    C:/TBuild/Libraries/win64/Qt-6.11.1/lib/cmake/Qt6WebSockets/

So do NOT skip the task. But nothing in tdesktop links it yet, and the
build is a static, non-packaged Qt (`DESKTOP_APP_USE_PACKAGED=OFF`),
so the module will not appear on its own.

Add the linkage in `Telegram/CMakeLists.txt`, NOT in the submodule:

    find_package(Qt6 COMPONENTS WebSockets QUIET)
    if (TARGET Qt6::WebSockets)
        target_link_libraries(Telegram PRIVATE Qt6::WebSockets)
        target_compile_definitions(Telegram PRIVATE CUSTOM_SYNC_HAS_WEBSOCKETS)
    endif()

Guard every WebSocket include and every use with
`#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS`. A build on a machine without the
module must still compile and must still sync on the timer. Say in your
report that you verified the file compiles with the macro **undefined**
as well as defined.

------------------------------------------------------------
🔴 2. THE SERVER CONTRACT — READ IT, DO NOT GUESS IT
------------------------------------------------------------

The server side is already written, and it is deployed nowhere yet, so
you cannot probe it. These are the facts, taken from its source:

  - Endpoint: `GET /ws/notify`
  - Auth: the JWT goes in the **`access_token` query parameter**. The
    server accepts a query token only for paths starting with `/ws`;
    everywhere else it wants the `Authorization` header. Use the query
    parameter here.
  - The server pushes exactly this text frame:
        {"type":"changes","seq":<int64>}
  - It is sent to every connected device **except the one that pushed**.
    You do not need a self-notification guard; do not add one.
  - The client sends **nothing**. The server reads only to notice a
    close frame.
  - It fires from one place only: after a successful push applies
    records.

Scheme: derive `ws://` from `http://` and `wss://` from `https://` on
the configured server URL. K1 — no hardcoded host, no hardcoded scheme.

Treat `seq` as **logging only**. The local `pull_cursor` stays the
single source of truth for what to fetch; a notification is a hint that
something exists, not a statement about what. Do not use `seq` to skip
or shortcut a pull.

------------------------------------------------------------
🔴 3. A NOTIFICATION DURING A CYCLE MUST NOT BE LOST
------------------------------------------------------------

This is the defect this task is most likely to ship.

`syncNow()` is a no-op while `_inFlight` is true. So a notification
arriving mid-cycle — which is the *common* case, because the other
device pushed while we were talking to the same server — does nothing,
and the change waits for the next interval. The feature would then be
silently useless in exactly the situation it exists for.

Keep a `_pendingNotify` flag. Set it when a notification arrives while
a cycle is in flight. In `onCycleFinished`, if it is set, clear it and
schedule an immediate cycle instead of the normal interval.

Fold this into the pure policy function rather than the timer wiring:
add `pendingNotify` to `SchedulerState`, and make `NextAction` return
`runNow` for it the same way it already does for `hasMore`. Then it is
testable, and the existing catch-up cap keeps a burst of notifications
from starving the UI — reuse that cap, do not add a second one.

------------------------------------------------------------
4. WHERE THE SOCKET LIVES, AND WHY
------------------------------------------------------------

Put it in `Client`, not in `Orchestrator`.

`Client` already owns `_accessToken`, `_refreshing` and the refresh
logic. The socket URL needs a **fresh** token, and a socket closed
because the token expired must reconnect only after a refresh.
Splitting that across two classes means either exposing the token
through a new accessor or duplicating the refresh state — both worse.

`Client` emits a Qt signal, e.g.:

    void changesAvailable(qint64 seq);

`Orchestrator` connects it to its notify handler. That keeps the
orchestrator thin, which is the same reason the scheduler policy was
extracted as a pure function.

`QWebSocket` is GUI-thread-only, exactly like `QNetworkAccessManager`.
No `crl::async`, no second thread, no mutex. The existing comment in
`custom_sync_client.h` explains why; the same reasoning applies here.

------------------------------------------------------------
5. RECONNECTION — AND THE TWO WAYS TO GET IT WRONG
------------------------------------------------------------

Exponential backoff, 1s doubling to a 60s ceiling, reset on a
successful open. Put the bounds in named constants with a comment.

Two failure modes to handle explicitly:

  a. **A 401/403 close is not a network problem.** Reconnecting with
     the same dead token in a loop hammers the server and never
     succeeds. On an auth-shaped failure, do not reconnect directly:
     let the next normal cycle refresh the token, and try again after
     that. Say in your report how you distinguish the two cases.

  b. **A flapping socket must not drive the sync loop.** Connecting and
     disconnecting must never itself trigger a cycle. Only an actual
     `{"type":"changes"}` frame does.

Ignore any frame you do not recognise — log once and move on. A future
server version may add message types, and an unknown type is not an
error.

------------------------------------------------------------
🔴 6. K5 — THE SOCKET IS PART OF "NO NETWORK WHEN OFF"
------------------------------------------------------------

Open the socket only when ALL of these hold:

    CustomSettings::SyncEnabled()  &&  device_id is set  &&  a valid
    access token exists

Toggling sync off must close it. Task 11's audit proved that with sync
off no code path reaches `QNetworkAccessManager`; your change must keep
that statement true for `QWebSocket` too, and your report must say
which line enforces it.

Do not open the socket from `CustomSync::Start()` unconditionally —
that runs during `Main::Session` construction.

------------------------------------------------------------
7. WHAT NOT TO ADD
------------------------------------------------------------

  - No message sending from the client. The channel is one-way.
  - No heartbeat/ping of your own. `QWebSocket` handles protocol-level
    pings; an application ping is a second timer with a second way to
    be wrong.
  - No UI toggle for WebSocket. It is an implementation detail of sync.
  - No change to the interval, the backoff or the catch-up cap.

If you think the sync tab should show the socket state, say so in your
report and leave it out — Task 10's tab is still visually unverified.

------------------------------------------------------------
HOW TO VERIFY
------------------------------------------------------------

  1. Selftest green (exit 0) BEFORE your change, so a later failure is
     attributable.
  2. Extend the `NextAction` family with the `pendingNotify` cases and
     assert exact numbers:
       - pendingNotify, not in flight, under the cap -> runs immediately
       - pendingNotify while disabled -> never runs
       - pendingNotify while inFlight -> does not run
       - pendingNotify at the catch-up cap -> falls back to the interval
     Update the count guard.
  3. Break your implementation deliberately — a DIFFERENT break from
     the one you would expect, for example make `pendingNotify` lose to
     `hasMore` instead of being checked alongside it — and confirm a
     named case FAILS before restoring it. Report which break and which
     case caught it.
  4. `/Zs` at `/W4 /WX` on `custom_sync.cpp`.
  5. `custom_sync_client.cpp` cannot be checked with `/Zs`. Compile it
     the way `tools/sync-selftest/README.md` documents, with
     `msbuild ... /p:SelectedFiles=<your file> /t:ClCompile`.

     This is REQUIRED, not optional, for every .cpp you touch that
     `/Zs` cannot handle. Three consecutive full builds were wasted on
     2026-09-07 because a delegate skipped this step.
  6. Do NOT run the full build. Hand it to the user and say so.

You cannot test against a live server — none is deployed. That is
accepted, and it is why the contract in section 2 is written as fact
rather than as something to discover.

============================================================
DEFINITION OF DONE
============================================================

  - `Qt6::WebSockets` linked from `Telegram/CMakeLists.txt`; the
    `cmake` submodule untouched
  - Everything guarded by `CUSTOM_SYNC_HAS_WEBSOCKETS`; compiles with
    the macro undefined too
  - Socket lives in `Client`, emits a signal the orchestrator consumes
  - Query-parameter auth; scheme derived from the server URL
  - `pendingNotify` in `SchedulerState`, decided by `NextAction`,
    reusing the existing catch-up cap
  - Backoff 1s -> 60s in named constants; auth failures handled
    differently from network failures
  - No socket while sync is off, unenrolled, or tokenless
  - Selftest exit 0 with the new cases and an updated count guard; you
    watched a deliberate break fail
  - `/Zs` clean at `/W4 /WX`; every file `/Zs` cannot handle compiled
    with the single-file MSBuild command
  - Full build NOT run
  - One commit, K7 style; no scratch files committed

============================================================
FINAL REPORT — seven short points
============================================================

  1. The selftest line for the scheduler family and the exit code.
  2. Which deliberate break you used and which case caught it.
  3. `git show --stat HEAD` (stat block only) and the commit subject.
  4. How a notification arriving mid-cycle reaches a pull, in order.
  5. How you tell an auth close from a network close, and what each
     does.
  6. `/Zs` result per file, and the single-file compile result per file
     that `/Zs` could not handle.
  7. Anything ambiguous you had to guess at, and whether you think the
     sync tab should show the socket state.

OUT OF SCOPE
  - No capture service (plan 05), no Android work.
  - Do not change the scheduler interval, backoff or retention filter.
  - Do not enable sync or change any default.
  - Do not change `occurred_at` at any enqueue site.
  - Do not modify the key-sharing flow from Task 12.
