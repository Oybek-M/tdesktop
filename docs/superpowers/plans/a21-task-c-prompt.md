# Task C — A21: noaniq legacy yozuvlarni QO'LDA biriktirish UI'si

You are implementing one self-contained task in the CustomMod fork of
Telegram Desktop (C++/Qt6). Read this whole prompt before writing code.

---

## 1. Background — why this task exists

`actioned_messages` stores AntiDelete records. Every read filters with
`account_id IN (0, ?)` (`custom_db.cpp:158`, `kAccountFilterSql`, 20 call
sites). Rows written before the v10 migration carry `account_id = 0`, and
**zero matches every account** — so a deletion recorded under account A is
injected into account B's chat, where the user knows they deleted nothing.

Two of three fixes are already merged (commit `9fa32cc7c3`):

- **A** — schema **v16** assigned an owner to the 11 783 legacy rows whose
  peer appears under exactly one account. Certain, no guessing.
- **B** — the remaining rows render a distinct marker in the chat
  (`history.cpp:2324`, `legacyMarker`) reading
  `—— O'CHIRILDI ——  ⚠ eski yozuv, akkaunt noma'lum`.

**Your task is C.** 14 970 legacy rows remain. Of those, **11 148 rows
across 29 peers** are ambiguous: the peer appears under two or more
accounts, so no rule can pick the owner. Only the user knows.

Measured on the live database (2026-09-05):

| peer_id | accounts | legacy rows |
|---|---|---|
| 562953420233555 | 9 | 85 |
| 562951956045168 | 6 | 91 |
| 562951352165303 | 6 | 60 |
| **1334067829** | **5** | **445** |
| 1474449522 | 5 | 239 |
| 562952015747750 | 4 | 28 |
| 6700270485 | 3 | 34 |
| 281479796986935 | 2 | 8 |
| …21 more | 2 | — |

`1334067829` is the chat the user actually reported. 29 items is small
enough to resolve by hand — that is the whole point of this task.

### 1.1. What success looks like — read this before you verify anything

**Do not judge this task by what changes in the chat window.** Measured
on 2026-09-07, of the 217 legacy `deleted` rows in peer `1334067829`:

| | rows |
|---|---|
| has text — can be injected into the chat | 23 |
| has a media file that still exists on disk | 0 |
| **no content at all — never injected** | **194** |

A separate rule (`history.cpp`, added 2026-08-27) refuses to inject a
record with no text and no surviving media file, and `looksForeign()`
filters more of what is left. So roughly 89% of these rows are already
invisible, and assigning them an owner will not visibly change the chat.

That is expected. **This task is about data correctness, not about the
chat rendering.** Verify it with SQL against a copy of the database and
through the Ombor tab's own list, never by looking for messages to
disappear from a conversation.

**Do NOT invent a heuristic.** Attribution by timestamp was already
measured and rejected: the accounts' activity windows nearly all run from
2026-08-27/28 to today, so timestamps separate nothing. Attribution by
message-id monotonicity already exists (`history.cpp`, `looksForeign()`)
and is deliberately conservative. Your job is to ask the user, not guess.

---

## 2. What to build

A new collapsible section in the **Ombor** tab listing every ambiguous
peer. For each: the chat name, how many legacy rows it holds, and one
button per candidate account. Pressing a button assigns those rows to
that account. A "Tegmang" option leaves them at 0.

### 2.1. Data layer — `custom_db.h` / `custom_db.cpp`

Add near the other media/maintenance declarations:

```cpp
// A21-C: legacy (account_id = 0) yozuvlari bir necha akkauntda uchraydigan
// peer'lar. Egasini qoida bilan aniqlab bo'lmaydi -- foydalanuvchi tanlaydi.
struct AmbiguousLegacyPeer {
    QString peerId;
    int legacyCount = 0;            // shu peer'dagi account_id = 0 qatorlar
    QVector<qint64> candidates;     // noldan farqli account_id'lar
    QVector<int> candidateCounts;   // har nomzodning qatorlari (candidates bilan bir tartibda)
};

[[nodiscard]] QVector<AmbiguousLegacyPeer> GetAmbiguousLegacyPeers();

// Tanlangan peer'ning BARCHA account_id = 0 qatorlarini accountId ga
// biriktiradi. Nechta qator o'zgargani qaytadi.
int AssignLegacyRows(const QString &peerId, qint64 accountId);
```

Implementation notes, all mandatory:

- Follow the existing raw-sqlite3 style in this file: `Init()`, `if (!gDb)
  return {}`, `sqlite3_prepare_v2` / `bindText` / `colText` /
  `sqlite3_finalize`. Do **not** introduce QtSql.
- `GetAmbiguousLegacyPeers()`: peers having `COUNT(DISTINCT account_id) >= 2`
  among non-zero rows, **and** at least one `account_id = 0` row. Order by
  `legacyCount DESC` so the worst offenders come first.
- Read the full result set into a `QVector` before issuing any UPDATE —
  never UPDATE while a SELECT statement is open on the same connection.
  `ReconcileMediaIndex()` (`custom_db.cpp`, ~line 1641) is the reference.
- `AssignLegacyRows()` must wrap its UPDATE in `execSql("BEGIN")` /
  `execSql("COMMIT")` and return `sqlite3_changes(gDb)`.
- After a successful assign, call `CustomDB::ClearCaches()` if such a
  function exists (grep for the cache invalidation used by
  `PermanentlyDeleteMessage`); the in-memory `gDeletedCache` /
  `gLoadedPeers` must not keep serving the pre-assign view. If no such
  helper exists, add the minimal invalidation the file already implies —
  do not leave a stale cache.

### 2.2. UI — `custom_tab_storage.cpp`

Add a new collapsible section, **closed by default**, after section `s3`
("🔍 Indekslash va tuzatish"):

```cpp
const auto s3b = AddCollapsibleSection(
    content,
    u"👤 Egasi noma'lum yozuvlar"_q,
    false);
```

Contents:

1. A `st::customModHintLabel` explaining the situation in two sentences:
   these records predate account tracking; assigning them hides them from
   the other accounts. If the list is empty, say so and stop.
2. For each `AmbiguousLegacyPeer`, in order:
   - A `st::defaultSubsectionTitle` line:
     `CustomSettings::GetPeerDisplayName(peerId)` and, when that returns
     empty, the raw `peerId`; plus the legacy row count.
   - One `Ui::SettingsButton` per candidate account, labelled with the
     account id and its row count for that peer, e.g.
     `u"→ 1474449522 (%1 ta yozuv shu akkauntda)"_q`.
   - Pressing it opens `Ui::MakeConfirmBox` naming the chat, the target
     account and the exact number of rows, and warning that the records
     will stop appearing in the other accounts. Only on confirm call
     `AssignLegacyRows()`.
   - After the call, `Ui::Toast::Show` the number of rows moved and
     rebuild the tab. The tab already receives a rebuild callback —
     `fillStorageTab(content, window, rebuildArchive)`; follow how the
     existing buttons in this file trigger a refresh rather than
     inventing a new mechanism.

Style/pattern reference: the sha256 and scan buttons in section `s3` of
this same file show the exact idiom for button + confirm + toast +
disable-while-running.

---

## 3. Hard constraints

- **Do not modify the v15 or v16 migration blocks** in `custom_db.cpp`,
  and do not bump `kCurrentSchemaVersion` (currently 16, `custom_db.h:68`).
  This task adds no schema.
- **Do not touch `docs/sync-protocol/`** or anything under
  `Telegram/SourceFiles/custom_sync*` — another session owns those.
- **Do not run a build.** The user builds manually and reports results.
- **Do not touch the user's real database** at
  `<ArchiveRoot>/db/actioned_messages.db`. `ArchiveRoot` is a user setting;
  resolve it via `dbFilePath()` (`custom_db.cpp:113`) if you need the path
  for documentation. If you want to test SQL, copy the file first.
- Comments in **Uzbek**, explaining *why*, not *what*. Plain ASCII
  apostrophes (`'`) only — never `’`.
- **Do not add `lang_keys` or `.strings` entries.** UI text is written
  inline as `u"..."_q`, the way the rest of `custom_tab_storage.cpp` does.
- Commit to the current branch. **No `Co-Authored-By` trailer.**
  Never push to `upstream`.

---

## 4. Correctness requirements

The assignment is a one-way write to 11 148 rows of the user's AntiDelete
history. Treat it accordingly:

1. **Never assign to an account that is not a candidate for that peer.**
   The button set is the whitelist.
2. **Never touch rows with `account_id <> 0`.** Every UPDATE must carry
   `AND account_id = 0`.
3. **Never assign across peers.** Every UPDATE must carry `AND peer_id = ?`.
4. The confirm box must state the row count that will change, and it must
   be the number your own query produced — not an estimate.
5. Assignment must be **idempotent**: pressing the same button twice
   changes 0 rows the second time and must not error.

## 5. Definition of done

- `GetAmbiguousLegacyPeers()` returns 29 entries on the live data shape
  described in §1, ordered by `legacyCount` descending, `1334067829`
  showing 445 rows and 5 candidates.
- The Ombor tab renders the section; each peer lists exactly its own
  candidate accounts.
- A confirmed assign moves exactly the stated rows, the toast reports the
  same number, and reopening the tab no longer lists that peer.
- `SELECT COUNT(*) FROM actioned_messages WHERE account_id = 0` drops by
  exactly the assigned amount, and no row's `account_id` changed from a
  non-zero value. Check this on a copy, not the live database.
- The code compiles as part of the existing `custom_db` / `custom_tab_*`
  targets with no new CMake entries.

## 6. Report back

State: the files you changed, the exact SQL of both new queries, how you
invalidated the cache after an assign, and anything you found that
contradicts this prompt. If something here is wrong, say so rather than
working around it silently.
