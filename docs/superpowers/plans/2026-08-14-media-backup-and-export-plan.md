# Katta media backup + indeks va eksport — implementation plan

> **Agent ishchilar uchun:** bu rejani vazifa-vazifa bajarish uchun
> `superpowers:subagent-driven-development` yoki `superpowers:executing-plans`
> ishlatiladi. Qadamlar `- [ ]` checkbox sintaksisida.

**Maqsad:** 10 MB dan katta media'ni arxivlash imkonini qaytarish, saqlanmagan
media haqida iz qoldiradigan indeks qo'shish, va eksport/import'ni Track C
(customsync-server) ga tayyorlash.

**Spec:** `docs/superpowers/specs/2026-08-14-media-backup-and-export-design.md`

**Arxitektura:** Uch qatlamli arxivlash (L1 mavjud hook, L2 oldindan yuklash,
L3 qutqaruv urinishi) + `media_index` SQLite jadvali + ikkita alohida eksport
arxivi (indeks doim, media ixtiyoriy va chat bo'yicha tanlanadigan).

**Sinov usuli:** bu kodbazada bu yo'llar uchun avtomat test infratuzilmasi yo'q
(SQLite + Qt tarmoq + UI). Loyihaning o'rnatilgan naqshiga amal qilamiz:
implement → **bitta build** → qo'lda sinov (T1–T15). Shuning uchun quyidagi
vazifalarda "testni yoz" qadami emas, **kompilyatsiya + qo'lda tekshiruv**
qadamlari bor. Bu ataylab qilingan, TDD dan chekinish sifatida qayd etilgan.

---

## ⚠️ Boshlashdan oldin

Build **eng oxirida bitta marta** qilinadi (Vazifa 15). Oraliq build'lar
qilinmaydi — har biri ~6 daqiqa va foydalanuvchi kompyuterini band qiladi.
Build'ni boshlashdan oldin **doim foydalanuvchidan so'ralsin**.

---

## Faza 0 — Mavjud xatoni tuzatish (poydevor)

### Vazifa 1: Kategoriya aniq ro'yxat yozuvini bosib ketmasin

**Muammo (2026-08-14 da topilgan, HAQIQIY va FAOL):**
`IsInBlocklist()` avval aniq ro'yxatni, so'ng **kategoriyani** tekshiradi.
`ShouldAntiDelete()` ustuvorligi Blocklist > Whitelist. Foydalanuvchining
5 ta kanali White List'da, lekin Black List'da "Kanallar" kategoriyasi
yoqilgan → **o'sha kanallar uchun AntiDelete amalda o'chiq**.

Aniq, bitta chat uchun qilingan qaror keng kategoriya qoidasidan ustun
bo'lishi kerak.

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/custom_settings.cpp:461` (`IsInBlocklist`)
- O'zgartirish: `Telegram/SourceFiles/custom_settings.cpp:425` (`IsInWhitelist`)

- [ ] **Qadam 1: `IsInBlocklist` da aniq whitelist yozuvini hurmat qilish**

```cpp
bool IsInBlocklist(const QString &peerId) {
    if (!gInitialized) Init();
    if (gBlocklist.contains(peerId)) return true;
    // 2026-08-14: ANIQ whitelist yozuvi kategoriya blokidan USTUN.
    // Ilgari "Kanallar" kategoriyasi Black List'da yoqilgan bo'lsa,
    // White List'ga qo'lda qo'shilgan kanal ham bloklanardi — ya'ni
    // foydalanuvchining aniq qarori jimgina bekor qilinardi.
    if (gWhitelist.contains(peerId)) return false;
    const auto type = GetPeerType(peerId);
    if (type != PeerType::Unknown) {
        const auto it = gBlocklistCategories.constFind(static_cast<int>(type));
        if (it != gBlocklistCategories.constEnd() && it.value()) return true;
    }
    return false;
}
```

- [ ] **Qadam 2: `IsInWhitelist` ga simmetrik himoya**

Aniq blocklist yozuvi whitelist kategoriyasidan ustun bo'lsin:

```cpp
bool IsInWhitelist(const QString &peerId) {
    if (!gInitialized) Init();
    if (gWhitelist.contains(peerId)) return true;
    if (gBlocklist.contains(peerId)) return false; // aniq blok > kategoriya
    const auto type = GetPeerType(peerId);
    ...
}
```

- [ ] **Qadam 3: Kompilyatsiya tekshiruvi**

Bu vazifada build qilinmaydi. Faqat sintaksisni ko'z bilan tekshiring —
build Vazifa 15 da.

- [ ] **Qadam 4: Commit**

```bash
git commit -am "fix(settings): aniq peer yozuvi kategoriya qoidasidan ustun bo'lsin"
```

---

## Faza 1 — Poydevor: indeks va sozlamalar

### Vazifa 2: `media_index` jadvali (schema v6 → v7)

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/custom_db.cpp` (`RunMigrations`, `kCurrentSchemaVersion`)

- [ ] **Qadam 1: `kCurrentSchemaVersion` ni 7 ga ko'tarish**

- [ ] **Qadam 2: v7 migratsiyasini qo'shish**

```sql
CREATE TABLE IF NOT EXISTS media_index (
    peer_id     TEXT    NOT NULL,
    msg_id      INTEGER NOT NULL,
    kind        TEXT    NOT NULL,
    file_name   TEXT,
    rel_path    TEXT,
    size        INTEGER NOT NULL DEFAULT 0,
    sha256      TEXT,
    msg_date    INTEGER,
    archived_at INTEGER,
    layer       TEXT,
    status      TEXT    NOT NULL,
    reason      TEXT,
    PRIMARY KEY (peer_id, msg_id)
);
CREATE INDEX IF NOT EXISTS idx_mi_status ON media_index(status);
CREATE INDEX IF NOT EXISTS idx_mi_peer   ON media_index(peer_id);
```

Migratsiya mavjud versiyalangan naqshga amal qilsin (`version >=
kCurrentSchemaVersion` bo'lsa o'tkazib yuborish) — shunda eski build
yangi bazani ocha oladi (orqaga moslik, A13 da tekshirilgan).

- [ ] **Qadam 3: Commit**

### Vazifa 3: `CustomDB` indeks API

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/custom_db.h`, `custom_db.cpp`

- [ ] **Qadam 1: Struct va funksiyalarni e'lon qilish**

```cpp
struct MediaIndexEntry {
    QString peerId;
    long long msgId = 0;
    QString kind;        // image | video | voice | file
    QString fileName;
    QString relPath;
    long long size = 0;
    QString sha256;
    unsigned int msgDate = 0;
    unsigned int archivedAt = 0;
    QString layer;       // l1 | l2 | l3
    QString status;      // present | pending | missing
    QString reason;
};

void UpsertMediaIndex(const MediaIndexEntry &entry);
void SetMediaIndexStatus(
    const QString &peerId,
    long long msgId,
    const QString &status,
    const QString &reason);
[[nodiscard]] bool HasMediaIndexEntry(const QString &peerId, long long msgId);

// Eksport tanlash oynasi uchun: media'si BOR chatlar va ularning hajmi.
struct MediaPeerSummary {
    QString peerId;
    int fileCount = 0;
    long long totalBytes = 0;
};
[[nodiscard]] QVector<MediaPeerSummary> GetMediaPeerSummaries();

// Kvota uchun: barcha 'present' yozuvlar yig'indisi.
[[nodiscard]] long long TotalArchivedMediaBytes();
```

- [ ] **Qadam 2: Implementatsiya**

`UpsertMediaIndex` — `INSERT ... ON CONFLICT(peer_id, msg_id) DO UPDATE`.
`GetMediaPeerSummaries` — `SELECT peer_id, COUNT(*), SUM(size) FROM
media_index WHERE status='present' GROUP BY peer_id ORDER BY SUM(size) DESC`.

Barcha funksiyalar boshida `Init()`, `gDb` null tekshiruvi — fayldagi
mavjud naqsh bilan bir xil.

- [ ] **Qadam 3: Commit**

### Vazifa 4: Yangi sozlamalar

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/custom_settings.h`, `custom_settings.cpp`

- [ ] **Qadam 1: `Values` struct'iga maydonlar**

```cpp
int mediaBackupMaxFileMb = 100;   // 10 – 2048
int mediaBackupQuotaGb = 10;      // 1 – 500
```

- [ ] **Qadam 2: Getter'lar va setter'lar**

Mavjud naqsh (`UpstreamCheckIntervalMinutes` / `SetInt`) bilan bir xil.
Setter'larda qiymat chegaralarga qisilsin (`std::clamp`).

- [ ] **Qadam 3: Per-chat "Media Backup" override**

`PerPeerEntry` ga `bool mediaBackupEnabled` maydoni qo'shiladi va
`SetMediaBackupForPeer` / `MediaBackupForPeer` funksiyalari — mavjud
`SetAntiDeleteForPeer` naqshi bo'yicha.

⚠️ `GetPerPeerOverrides()` ni ishlatadigan barcha joylar (asosan
`custom_mod_window.cpp`) yangi maydon bilan buzilmasligini tekshiring.

- [ ] **Qadam 4: Commit**

### Vazifa 5: `ShouldMediaBackup()` — ro'yxatga bog'lanish qoidasi

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/custom_settings.h`, `custom_settings.cpp`

- [ ] **Qadam 1: Funksiyani yozish**

```cpp
// 🔴 DIQQAT: bu funksiya ShouldAntiDelete() zanjiriga ATAYLAB
// ERGASHMAYDI. O'sha zanjirning oxirgi bo'g'ini — GLOBAL bayroq — bu
// yerda XAVFLI: 2026-08-14 dagi baza tozalashida global AntiDelete
// tufayli 981 ta peer kuzatilayotgani aniqlangan edi (asosan botlar).
// AntiDelete uchun bu arzon (chatiga bir necha KB matn), media backup
// uchun esa halokatli — har bir botdan video yuklardik.
//
// Shuning uchun faqat ANIQ a'zolik: White List yoki per-chat toggle.
[[nodiscard]] bool ShouldMediaBackup(const QString &peerId) {
    if (IsInBlocklist(peerId)) return false;
    if (IsInWhitelist(peerId)) return true;
    return MediaBackupForPeer(peerId);
}
```

⚠️ `peer->isSelf()` tekshiruvi bu yerda EMAS — `peerId` string'idan
Saved Messages ekanini aniqlab bo'lmaydi. U chaqiruv joyida
(`custom_archive.cpp`) qilinadi, Vazifa 7 ga qarang.

- [ ] **Qadam 2: Commit**

### Vazifa 6: Kvota hisoblagichi

**Fayllar:**
- Yaratish: `Telegram/SourceFiles/custom_media_quota.h`, `custom_media_quota.cpp`
- O'zgartirish: `Telegram/CMakeLists.txt` (yangi fayllarni ro'yxatga qo'shish)

- [ ] **Qadam 1: API**

```cpp
namespace CustomMediaQuota {

// Ishga tushishda bir marta chaqiriladi — joriy hajmni hisoblaydi.
void Init();

[[nodiscard]] long long UsedBytes();
[[nodiscard]] long long LimitBytes();   // CustomSettings dan
[[nodiscard]] bool IsFull();

// Yangi fayl qo'shilgach chaqiriladi (qayta skanerlashsiz).
void AddBytes(long long bytes);

} // namespace CustomMediaQuota
```

- [ ] **Qadam 2: Implementatsiya**

`Init()` — `CustomDB::TotalArchivedMediaBytes()` dan boshlang'ich qiymat
oladi (papkani rekursiv skanerlashdan tezroq). Agar indeks bo'sh bo'lsa
(birinchi ishga tushish), `medias/` papkasini bir marta skanerlaydi va
indeksni to'ldiradi.

- [ ] **Qadam 3: CMakeLists.txt ga qo'shish**

Mavjud `custom_archive.cpp` yozuvi yonига, bir xil naqshda.

- [ ] **Qadam 4: Commit**

---

## Faza 2 — Arxivlash qatlamlari

### Vazifa 7: L2 — haqiqiy fayl yo'li bilan oldindan yuklash

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/custom_archive.cpp` (`MaybeDownloadMedia`)

- [ ] **Qadam 1: Maqsad yo'lini hisoblovchi yordamchi**

```cpp
// medias/<turi>/<peerId>_<msgId>_<xavfsiz nom>
// SaveMediaFile() bilan bir xil papka sxemasi.
[[nodiscard]] QString ArchiveTargetPath(
    not_null<HistoryItem*> item,
    not_null<DocumentData*> document);
```

Fayl nomi tozalansin (Windows'da taqiqlangan belgilar: `\ / : * ? " < > |`).

- [ ] **Qadam 2: `MaybeDownloadMedia` ni qayta yozish**

```cpp
void MaybeDownloadMedia(not_null<HistoryItem*> item) {
    const auto media = item->media();
    if (!media) return;

    const auto peer = item->history()->peer;
    if (peer->isSelf()) {
        // Saved Messages: foydalanuvchidan boshqa hech kim o'chira
        // olmaydi — AntiDelete'ning tahdid modeli bu yerda qo'llanmaydi.
        return;
    }
    const auto peerIdStr = QString::number(peer->id.value);
    const auto origin = Data::FileOriginMessage(item->fullId());

    if (const auto document = media->document()) {
        if (!document->filepath(true).isEmpty() || document->loading()) {
            return;
        }
        if (!CustomSettings::ShouldMediaBackup(peerIdStr)) {
            // L2 yoqilmagan — 10 MB gacha bo'lgan eski xatti-harakat.
            if (document->size <= Storage::kMaxFileInMemory) {
                document->save(origin, QString());
            }
            return;
        }
        const auto maxBytes = qint64(
            CustomSettings::MediaBackupMaxFileMb()) * 1024 * 1024;
        if (document->size > maxBytes) {
            RecordPending(item, document, u"too_large"_q);
            return;
        }
        if (CustomMediaQuota::IsFull()) {
            RecordPending(item, document, u"quota_full"_q);
            return;
        }
        // 🔴 Bo'sh nom EMAS — aynan shu 2026-08-14 crash'iga olib kelgan.
        document->save(origin, ArchiveTargetPath(item, document));
        RecordPresent(item, document);
    } else if (const auto photo = media->photo()) {
        photo->load(Data::PhotoSize::Large, origin);
    }
}
```

- [ ] **Qadam 3: `RecordPending` / `RecordPresent` yordamchilari**

Ikkalasi ham `CustomDB::UpsertMediaIndex()` ni chaqiradi; `RecordPresent`
qo'shimcha ravishda `CustomMediaQuota::AddBytes()`.

- [ ] **Qadam 4: Commit**

### Vazifa 8: `finishLoad` hook'ining ikkita tuzatishi

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/data/data_document.cpp:1056-1067`

- [ ] **Qadam 1: Ikki marta nusxalash tuzog'ini yopish**

L2 faylni to'g'ridan-to'g'ri arxivga yuklaydi, shunda `_loader->fileName()`
arxiv yo'lining O'ZI bo'ladi va `SaveMediaFile()` uni o'ziga nusxalaydi.

```cpp
const auto cachePath = _loader->fileName();
const auto archiveRoot = QDir::homePath() + "/customizationMainFolder";
if (cachePath.startsWith(archiveRoot)) {
    // Fayl allaqachon arxivda (L2 to'g'ridan-to'g'ri shu yerga yukladi)
    // — o'zini o'ziga nusxalashning hojati yo'q.
} else if (!cachePath.isEmpty() && shouldSave) {
    CustomDB::SaveMediaFile(cachePath, "file");
}
```

- [ ] **Qadam 2: Per-peer xatosini tuzatish**

Koddagi izoh "no per-peer id available here" deb da'vo qiladi — bu
**noto'g'ri**. `FileLoader::fileOrigin()` mavjud (`file_download.h:98`).

```cpp
// Origin xabarga bog'liq bo'lsa — per-peer qoidani ishlatamiz.
// Aks holda (avatar, sticker set va h.k.) global bayroqqa qaytamiz.
bool shouldSave = CustomSettings::AntiDelete();
const auto origin = _loader->fileOrigin();
if (v::is<Data::FileOriginMessage>(origin.data)) {
    const auto &msgId = v::get<Data::FileOriginMessage>(origin.data);
    shouldSave = CustomSettings::ShouldAntiDelete(
        QString::number(msgId.peer.value));
}
```

- [ ] **Qadam 3: Eskirgan izohni yangilash**

`data_document.cpp:1062-1063` dagi "no per-peer id available here"
jumlasini olib tashlang — u endi noto'g'ri va kelajakdagi o'quvchini
chalg'itadi.

- [ ] **Qadam 4: Commit**

### Vazifa 9: L3 — o'chirish paytida qutqarish urinishi

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/history/history_item.cpp` (`setDeletedLocally()` yo'li)

⚠️ **Avval o'qing:** `setDeletedLocally()` ning joriy implementatsiyasini
toping va u media bilan nima qilayotganini tushuning — u allaqachon
"live cache path" dan nusxalashga urinadi (`data_document.cpp:1057`
izohiga qarang). L3 o'sha mavjud yo'lni kengaytiradi, yangi mexanizm
qurmaydi.

- [ ] **Qadam 1: Media arxivda yo'qligini tekshirish**

`CustomDB::HasMediaIndexEntry(peerId, msgId)` va status `present` emasligi.

- [ ] **Qadam 2: Yuklashga urinish**

Chegara va kvota tekshiruvlari L2 dagidek. Muvaffaqiyatsizlikda
`SetMediaIndexStatus(..., "missing", "reference_expired")`.

**Ochiq e'tirof (kodda ham izoh bo'lsin):** bu ishonchsiz. Xabar
o'chirilgach `file_reference` tez orada yaroqsiz bo'ladi. L3 kafolat
emas, bonus — urinish tekin.

- [ ] **Qadam 3: Commit**

### Vazifa 10: Kvota to'lganda ogohlantirish

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/main/main_session.cpp` (mavjud startup hook'lari yonида)

- [ ] **Qadam 1: Ishga tushishda tekshirish**

`CustomArchive::StartMaintenance()` yonida `CustomMediaQuota::Init()` va
kvota to'la bo'lsa box ko'rsatish.

**Toast EMAS, box** — foydalanuvchi tasdiqlashi kerak. Matnda: joriy
hajm / kvota, "Sozlamalarni ochish" tugmasi.

Muammo hal bo'lmaguncha **har ishga tushishda** takrorlanadi (foydalanuvchi
talabi). Hech qanday fayl o'chirilmaydi.

- [ ] **Qadam 2: Commit**

---

## Faza 3 — Eksport / import

### Vazifa 11: Ikkita alohida arxiv + manifest v3

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/custom_db.h`, `custom_db.cpp:1310` (`ExportFullBackup`)

- [ ] **Qadam 1: `ExportOptions` struct**

```cpp
struct ExportOptions {
    bool includeAllMedia = false;
    QVector<QString> mediaPeerIds;  // includeAllMedia false bo'lganda
};
```

Bo'sh `mediaPeerIds` + `includeAllMedia == false` → **faqat indeks**.

- [ ] **Qadam 2: `ExportFullBackup` ni ikki bosqichga bo'lish**

Bosqich 1 (`CustomModBackup_<stamp>.zip`): baza, `peer_lists.json`,
`branding.json`, `settings.reg`, `manifest.json`. Media papkasi **YO'Q**.

Bosqich 2 (`CustomModMedia_<stamp>.zip`, faqat tanlansa): `medias/` dan
tanlangan peer'larning fayllari (indeksdagi `rel_path` bo'yicha).

**Nima uchun ikkita fayl:** ZIP yaratilgach unga qo'shib bo'lmaydi.
Ikkitasi bilan asosiysi soniyalarda tayyor bo'lib, media fonda davom etadi.

- [ ] **Qadam 3: Manifest v3**

```cpp
manifest["version"] = 3;
manifest["mediaIncluded"] = includeAny;
manifest["mediaArchive"] = mediaZipName;   // yoki bo'sh
manifest["mediaTotalBytes"] = totalBytes;
```

`mediaIncluded` "media yo'q edi" va "media ataylab chiqarildi" holatlarini
ajratadi — v2 dagi `hasMedia` buni ajrata olmasdi.

⚠️ **Orqaga moslik:** `ImportFullBackup` v2 manifest'ini ham qabul
qilaverishi shart (eski zaxiralar). `version < 3` bo'lsa — eski xatti-harakat.

- [ ] **Qadam 4: Commit**

### Vazifa 12: Import — media arxivini alohida qabul qilish

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/custom_db.cpp` (`ImportFullBackup`)

- [ ] **Qadam 1: Asosiy arxivni import qilgandan keyin indeksni moslashtirish**

Indeksda `status='present'` bo'lgan, lekin fayli topilmagan yozuvlar
`missing` ga o'tkaziladi — bazada yolg'on ma'lumot qolmasligi uchun.

- [ ] **Qadam 2: Media arxivini alohida import qilish funksiyasi**

```cpp
bool ImportMediaArchive(const QString &zipPath);
```

Fayllarni joyiga qo'yadi va tegishli indeks yozuvlarini `present` ga
qaytaradi. Asosiy import'dan **keyin**, istalgan vaqtda chaqirilishi mumkin.

- [ ] **Qadam 3: Mavjud `fullReplace` semantikasi o'zgarmasin**

Merge / to'liq almashtirish xatti-harakati hozirgidek qolsin.

- [ ] **Qadam 4: Commit**

---

## Faza 4 — UI

### Vazifa 13: Custom Window sozlamalari

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/custom_mod_window.cpp`

- [ ] **Qadam 1: General yoki Archive tab'iga ikkita raqamli maydon**

"Bitta fayl uchun chegara (MB)" — default 100, "Umumiy kvota (GB)" —
default 10. Mavjud raqamli maydon naqshini ishlating (upstream check
interval maydoni).

Yonida joriy holat: "Ishlatilgan: 3.2 GB / 10 GB".

- [ ] **Qadam 2: "Individual sozlamalar" ga 4-qator**

Ghost Mode / Anti-Delete / Anti-Edit yoniga **"Media Backup"** toggle'i.

- [ ] **Qadam 3: Commit**

### Vazifa 14: Eksport tanlash oynasi

**Fayllar:**
- O'zgartirish: `Telegram/SourceFiles/custom_mod_window.cpp` (Archive tab, eksport tugmasi)

- [ ] **Qadam 1: Tanlash oynasi**

Eksport bosilganda box ochiladi. Ro'yxat `CustomDB::GetMediaPeerSummaries()`
dan — ya'ni **faqat media'si bor chatlar** ko'rinadi (odatda 3-7 qator,
yuzlab emas).

```
☑ Hammasi (12.4 GB)
  ☑ Мубина)🎀      2.1 GB   340 fayl
  ☐ Amor Fati❤️‍🔥   8.2 GB  1204 fayl
──────────────────────────
Tanlangan: 3.2 GB
```

- "Hammasi" belgilansa → `includeAllMedia = true`
- Alohida belgilansa → `mediaPeerIds`
- Hech nima belgilanmasa → faqat indeks

Yig'indi tanlov o'zgarganda darhol yangilansin — foydalanuvchi ZIP
hajmini oldindan bilsin.

- [ ] **Qadam 2: Oxirgi tanlovni eslab qolish**

`CustomSettings` ga saqlanadi, keyingi eksport bir bosishda bo'lsin.

- [ ] **Qadam 3: Fon rejimi va progress**

Mavjud `ExportFullBackupAsync` ishlatiladi. Media bosqichi alohida
navbatga qo'yiladi — asosiy arxiv tayyor bo'lgach foydalanuvchi ishini
davom ettiraveradi. **Bekor qilish tugmasi bo'lsin** (media bosqichi
uzoq davom etadi).

- [ ] **Qadam 4: Commit**

---

## Faza 5 — Build va sinov

### Vazifa 15: Bitta build + qo'lda sinov

- [ ] **Qadam 1: Foydalanuvchidan build uchun ruxsat so'rash**

Build ~6 daqiqa va kompyuterni band qiladi. **Hech qachon so'ramasdan
boshlamang.**

- [ ] **Qadam 2: VS2026 da Release / x64 → Build**

`C:\TBuild\tdesktop\out\Telegram.sln`. **Rebuild / Clean QILMANG.**
Muhit: `QT=6.11.1` bo'lishi shart (VS ni qayta ochish kerak bo'lishi mumkin).

- [ ] **Qadam 3: Sinov jadvali**

| № | Sinov | Kutilayotgan natija |
|---|---|---|
| T1 | White List'dagi chatda 50 MB video (ochilmagan) | Arxivga tushadi, indeks `present` |
| T2 | Xuddi shu, 500 MB video | Yuklanmaydi, indeks `pending`/`too_large`, **crash yo'q** |
| T3 | White List'da bo'lmagan chat, global AntiDelete YOQIQ | Media yuklanmaydi |
| T4 | Saved Messages | Hech qachon yuklanmaydi |
| T5 | Black List'dagi chat | Hech qachon yuklanmaydi |
| T6 | Kvota to'lgan holatda qayta ishga tushirish | Box chiqadi, fayllar o'chmaydi |
| T7 | Kvota to'lganda media ochish (L1) | Baribir arxivlanadi |
| T8 | Eksport, hech nima belgilanmagan | Bitta ZIP, MB'lar, `mediaIncluded: false` |
| T9 | Eksport, "Hammasi" belgilangan | Ikkita ZIP, asosiysi tez tayyor |
| T10 | Eksport, 1 ta chat belgilangan | Media ZIP faqat o'sha chat fayllari |
| T11 | Faqat asosiy arxivni import | Indeks tiklanadi, fayli yo'qlar `missing` |
| T12 | Media arxivini keyinroq import | Yozuvlar `present` ga qaytadi |
| T13 | L2 yuklagan fayl | Ikki marta nusxalanmaydi (Vazifa 8) |
| T14 | Global AntiDelete O'CHIQ, chat White List'da, media ochish | Arxivlanadi |
| T15 | **5 ta whitelist kanal** (Vazifa 1) | AntiDelete ishlaydi — Black List kategoriyasi ularni bloklamaydi |
| T16 | **A11 story backup** (eski `A11 Task 6`) | Kichik story arxivlanadi; katta story `pending`, crash yo'q |

- [ ] **Qadam 4: Natijalarni `PROJECTS.md` ga yozish**

---

## Eslatmalar

- **Eski zaxiralar:** v2 manifest bilan yaratilgan zaxiralar import
  qilinaverishi shart (Vazifa 11, Qadam 3)
- **DB orqaga mosligi:** v7 migratsiyasi eski build'ni buzmasligi kerak —
  A13 da v5→v6 uchun tekshirilgan naqsh
- **Track C:** indeks `sha256` bilan tayyor bo'ladi, blob sinxronizatsiyasi
  esa keyinroq mustaqil qo'shiladi va bu ishni bloklamaydi
