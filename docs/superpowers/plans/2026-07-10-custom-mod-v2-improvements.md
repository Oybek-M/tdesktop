# BekGram CustomMod v2.0 — To'liq Yaxsilishtirish Plani

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Sana**: 2026-07-10  
**Muallif**: Arxitektura Agent (Opus)  
**Status**: Rejalashtirish  
**Prioritet**: HIGH (5 ta aniq muhim muammo)

---

## Qisqacha Fikr

Ushbu plan BekGram CustomMod (tdesktop fork)'dagi **8 ta o'zaro bog'liq muammoni** hal qiladi:
1. **UI thread freeze** backup/restore paytida (1–30 soniya, ma'lumotlar hajmiga qarab)
2. **Startup freeze** cache yuklashda (210k+ qator SQLite'dan sinxron)
3. **Ma'lumot yo'qolishi** multi-device scenariyada (merge o'rniga overwrite)
4. **Rasm hook'i yo'q** (faqat hujjatlar saqlanadi)
5. **Media yo'llar ortiqcha** (eski `/Telegram_AntiDelete/` vs yangi `/customizationMainFolder`)
6. **Forward bypass kaskadi** — implement qilinmagan
7. **UI/UX yakunlash** — tort bo'shliq, kuch iyerarhiyasi, xavf operatsiyalari uchun reng
8. **Backup integratsiyasi** — media avtomatik qo'shilmaydi

**Umumiy arxitektura yechimi:**
- Asinxron operatsiyalar `crl::async` / `crl::on_main` orqali (tdesktop'da allaqachon ishlatiladi)
- Lazy/per-peer caching to'liq jadvalni o'qish o'rniga
- Merge-first strategiya import uchun aniq foydalanuvchi tanlashi bilan
- Thread-safe SQLite FULLMUTEX flagi bilan
- Foto/hujjat/ovoz uchun unified media pipeline, indekslash bilan

**Baholash**: ~3–4 hafta parallel ishlab chiqish paytida (2 xodim)

---

## Maqsad

BekGram production'da ishlatilish ishonchliligini pasaytiradigan muammolarni bartaraf etish:
- Asosiy: **UI freeze yo'q backup/restore/startup paytida**
- Ikkinchi: **Multi-device'da ma'lumot yo'qolmaydi**
- Uchinchi: **Xususiyat to'liqligini ta'minlash**: media arxiv + forward bypass

---

## Arxitektura Tasviri

```
Telegram/SourceFiles/
├─ custom_db.h/cpp (400 → 600 qator)
│  ├─ LoadRestoreCache() → LazyPerPeerCache
│  ├─ ExportFullBackup() → crl::async orqali
│  ├─ ImportFullBackup() → asinxron + merge strategiyasi
│  ├─ MergeDatabase() [YANGI]
│  └─ FindSavedMediaPath() [YANGI] forward bypass uchun
│
├─ custom_settings.h/cpp (200 qator)
│  ├─ Init() → startAsyncCacheLoader()
│  └─ GetCachedValue() → per-peer ni lazy yukla
│
├─ custom_mod_window.cpp (1900 → 2100 qator)
│  ├─ Backup tugmasi (1687–1699) → asinxron + progress
│  ├─ Tiklash tugmasi (1707–1734) → asinxron + merge UI
│  ├─ UI audit: tab spacing, tugma iyerarhiyasi
│  └─ Xavf operatsiyalari styling (qizil chegara)
│
├─ data/data_photo.cpp [YANGI HOOK]
│  ├─ photoLoaded() callback → unified yo'liga saqla
│
├─ data/data_document.cpp (1057 → yo'li o'zgartirilgan)
│  ├─ documentLoaded() → ~/customizationMainFolder
│
├─ apiwrap.cpp (forwardMessages kaskadi) [YANGI]
│  └─ Fallback zanjiri: native → SendExisting → CustomDB
│
└─ db/db_singleton.h/cpp [YANGI]
   └─ Thread-safe per-peer cache manager
```

Database Schema (custom_db.cpp):
```
actioned_messages:
  - peer_id (TEXT, INDEX!)
  - msg_id (INTEGER)
  - type (TEXT: 'deleted', 'edited', 'backup')
  - timestamp (TEXT)
  - original_text (TEXT)
  - media_path (TEXT) [YANGI]
  - media_type (TEXT) [YANGI: 'photo', 'document', 'voice']
```

---

## Texnika Stack

| Komponent | Hozir | Yangi | Sabab |
|---|---|---|---|
| Threading | Yo'q (sinxron) | `crl::async` + `crl::on_main` | tdesktop'ning native infra, zero-copy tasks |
| Cache | To'liq jadvali QHash | Per-peer lazy LRU | O(1) startup, o'sib boradi |
| SQLite | WAL | WAL + FULLMUTEX + per-peer INDEX | Thread-safe, tez so'rovlar |
| UI Progress | Yo'q | `Ui::RoundButton::setEnabled(false)` + Toast | UI blocklash yo'q |
| Media yo'li | Ortiqcha | `~/customizationMainFolder/medias/{type}/` | Bitta ishonch manbai |
| Merge Strategiyasi | Overwrite | Timestamp asosida + aniq UX | Ma'lumotni saqlash |
| Forward | Faqat native | Kaskadi: native → SendExisting → CustomDB | Stable fallback |

---

## Implement Tartibi — Bog'lanishi Bo'yicha

### Fase 1: Threading va Cache Infratuzilmasi (1–3 kun)

#### Vazifa 1.1: Asinxron Backup/Export Progress Bilan

**Fayllar**: `custom_db.h/cpp`, `custom_mod_window.cpp`, yangi `custom_async_worker.h/cpp`

**Hozirgi holat** (`custom_db.cpp:1072–1165`):
```cpp
QString ExportFullBackup(const QString &targetDir) {
    Init();
    if (!gDb) return {};
    FlushPendingWrites();
    
    // ... DB copy (sinxron)
    QProcess regProc;
    regProc.start("cmd.exe", {"/c", regCmd});
    regProc.waitForFinished(10000);  // ← UI BLOK QILADI
    // ... ZIP (sinxron)
    QProcess zipProc;
    zipProc.start("powershell.exe", {...});
    zipProc.waitForFinished(30000);  // ← MUHIM NUQTA
}
```

**Yangi imzo**:
```cpp
// custom_db.h
struct ExportProgressCallback {
    std::function<void(int percent, const QString &status)> onProgress;
    std::function<void(const QString &error)> onError;
    std::function<void(const QString &path)> onSuccess;
};

void ExportFullBackupAsync(
    const QString &targetDir,
    ExportProgressCallback callback);
```

**Implement** (`custom_db.cpp`):
```cpp
void ExportFullBackupAsync(
    const QString &targetDir,
    ExportProgressCallback callback) {
    
    // Background thread'da ishga tushir tdesktop infra orqali
    crl::async([] {
        // 0–10%: DB copy (sinxron, lekin alohida thread'da)
        callback.onProgress(5, u"DB nusxalanmoqda..."_q);
        // BLOK operatsiyalar UI'ni ta'sir qilmaydi
        
        // 10–30%: Media copy
        callback.onProgress(20, u"Media nusxalanmoqda..."_q);
        CopyDirRecursive(src, dst);
        
        // 30–70%: Registry export
        callback.onProgress(40, u"Parametrlar exportlanmoqda..."_q);
        #ifdef Q_OS_WIN
        QProcess regProc;
        regProc.start("cmd.exe", {...});
        regProc.waitForFinished(10000);  // ← Hozir background'da!
        #endif
        
        // 70–100%: ZIP
        callback.onProgress(80, u"Arxivlanmoqda..."_q);
        QProcess zipProc;
        zipProc.start("powershell.exe", {...});
        zipProc.waitForFinished(30000);  // ← Lekin background'da!
        
        const QString result = stageDir + ".zip";
        
        // Main thread'ga natija qaytarish
        crl::on_main([result, callback] {
            callback.onSuccess(result);
        });
    });
}
```

**UI chaqirish** (`custom_mod_window.cpp:1687`):
```cpp
// Eski (FREEZE 30 soniyaga):
->addClickHandler([=] {
    const auto dir = QFileDialog::getExistingDirectory(...);
    if (dir.isEmpty()) return;
    const auto result = CustomDB::ExportFullBackup(dir);  // ← SYNC
    if (result.isEmpty()) {
        Ui::Toast::Show(u"Eksport bajarilmadi."_q);
    }
});

// Yangi (UI BLOK YO'Q):
->addClickHandler([=] {
    const auto dir = QFileDialog::getExistingDirectory(...);
    if (dir.isEmpty()) return;
    
    // Tugmani o'chirish, progress ko'rsatish
    button->setEnabled(false);
    auto progressLabel = new Ui::FlatLabel(parent, 
        rpl::single(u"0% — tayyorlanmoqda..."_q),
        st::labelDefaults);
    
    CustomDB::ExportProgressCallback cb;
    cb.onProgress = [button, progressLabel](int pct, const QString &status) {
        progressLabel->setText(QString("%1% — %2").arg(pct).arg(status));
    };
    cb.onSuccess = [button, progressLabel](const QString &path) {
        button->setEnabled(true);
        Ui::Toast::Show(u"✅ Eksport tayyar: "_q + path);
    };
    cb.onError = [button](const QString &err) {
        button->setEnabled(true);
        Ui::Toast::Show(u"❌ Xato: "_q + err);
    };
    
    CustomDB::ExportFullBackupAsync(dir, cb);
});
```

**Tekshirish ro'yxati**:
- [ ] `custom_async_worker.h` yaratish `ExportProgressCallback` struct bilan
- [ ] Export logikasini `crl::async {}` blokga o'tkazish
- [ ] Har bir bosqichda progress callback'lar (DB, media, registry, ZIP)
- [ ] UI tugmasini update qilish (disable + progress label)
- [ ] 1 GB+ media'da sinash (< 100 ms UI latency bo'lishi kerak)
- [ ] Toast xabarlarini update qilish (emoji qo'shish)

**Vaqt**: ~4 soat

---

#### Vazifa 1.2: Asinxron Import Merge Strategiyasi Bilan (A Variantini Tanlash)

**Tavsiya**: **A Varianti (Lazy Per-Peer)** o'rniga to'liq jadvalni yuklagandan

**Asoslash**:
- **A Varianti**: Startup < 100ms. Chat ochishda per-peer yuklash. 99% foydalanuvchi 5–20 chat ocha → 10x xotira va vaqt tejamligi.
- **B Varianti**: Barcha 210k background'da yuklash → startup tez ko'rinadi, lekin 50+ MB xotira doimiy (vs 2–5 MB lazy).

**Tanlash**: A Varianti (lazy per-peer).

**Fayllar**: `custom_db.h/cpp`, `custom_settings.cpp`, yangi `db/db_peer_cache.h/cpp`

**Hozirgi holat** (`custom_db.cpp:256`):
```cpp
void LoadRestoreCache() {
    Init();
    if (!gDb) return;
    
    // TO'LIQ jadvali SINXRON startup'da yuklash
    // 210,000 qator → SELECT peer_id, msg_id FROM actioned_messages
    // → QHash insert × 210k → 2–5 soniya sekin diskda
    
    QHash<QString, QSet<long long>> newDeleted;
    // ... 210k qatorni to'ldirish
}
```

**Yangi arxitektura**:
```cpp
// custom_db.h
class PerPeerCache {
private:
    struct CacheEntry {
        QSet<long long> deletedIds;
        QHash<long long, QString> editedTexts;
        bool loaded = false;
        QDateTime loadedAt;
    };
    
    QHash<QString, CacheEntry> mCache;
    QMutex mMutex;
    
public:
    // Specific peer'uchun on-demand yukla
    void ensureLoadedPeer(const QString &peerId);
    bool isDeletedLocally(const QString &peerId, long long msgId);
    QString getOriginalText(const QString &peerId, long long msgId);
};
```

**Implement** (`custom_db.cpp`):
```cpp
// YANGI: Per-peer lazy yuklash
void PerPeerCache::ensureLoadedPeer(const QString &peerId) {
    QMutexLocker locker(&mMutex);
    auto &entry = mCache[peerId];
    if (entry.loaded) return;
    
    // FAQAT bu peer uchun yuklash
    // SELECT ... WHERE peer_id = ? ORDER BY rowid ASC
    sqlite3_stmt *stmt = nullptr;
    const auto query = "SELECT msg_id, type, original_text FROM actioned_messages WHERE peer_id = ? ORDER BY rowid ASC";
    
    if (sqlite3_prepare_v2(gDb, query.toStdString().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    
    bindText(stmt, 1, peerId);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const long long msgId = sqlite3_column_int64(stmt, 0);
        const QString type = colText(stmt, 1);
        
        if (type == "deleted") {
            entry.deletedIds.insert(msgId);
        } else if (type == "backup" || type == "edited") {
            if (!entry.editedTexts.contains(msgId)) {
                entry.editedTexts[msgId] = colText(stmt, 2);
            }
        }
    }
    sqlite3_finalize(stmt);
    
    entry.loaded = true;
    entry.loadedAt = QDateTime::currentDateTime();
}

bool PerPeerCache::isDeletedLocally(const QString &peerId, long long msgId) {
    QMutexLocker locker(&mMutex);
    auto &entry = mCache[peerId];
    if (!entry.loaded) {
        locker.unlock();
        ensureLoadedPeer(peerId);
        locker.relock();
    }
    return entry.deletedIds.contains(msgId);
}

// Startup paytida: hechnarsa yuklash yo'q
void LoadRestoreCache() {
    Init();
    {
        QMutexLocker locker(&gCacheMutex);
        gDeletedCache.clear();  // Bo'sh qoldirish!
        gEditedCache.clear();
    }
    // Shu hammasi!
    // Birinchi har bir peer'uchun murojaat paytida avtomatik yuklash
    // ensureLoadedPeer() orqali
}
```

**Database**: peer_id ustuniga INDEX borligini tekshirish:

```cpp
// custom_db.cpp:createSchema() yoki Init()
// Agar yo'q bo'lsa qo'shish:
if (sqlite3_prepare_v2(gDb, 
    "CREATE INDEX IF NOT EXISTS idx_peer_id ON actioned_messages(peer_id)",
    -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}
```

**Tekshirish ro'yxati**:
- [ ] `db/db_peer_cache.h` yaratish `PerPeerCache` classikasi bilan
- [ ] `ensureLoadedPeer(peerId)` implement qilish per-peer SELECT bilan
- [ ] `peer_id` ustuniga INDEX qo'shish (schema'da tekshirish)
- [ ] `LoadRestoreCache()` update qilish bo'sh qilish uchun
- [ ] `IsDeletedLocally()` / `GetOriginalText()` update qilish `ensureLoadedPeer()` chaqirish uchun
- [ ] Sinash: startup < 100 ms bo'lishi kerak
- [ ] 5 ta boshqa chat ishga tushirish, har biri < 50 ms yuklash bo'lishi kerak

**Vaqt**: ~6 soat

---

### Fase 2: Media Asosi (4–6 kun)

#### Vazifa 2.1: Media Yo'llarini Yagona Qilish

**Fayllar**: `data/data_photo.cpp`, `data/data_document.cpp`, `custom_db.h/cpp`

**Muammo** (`data_document.cpp:1057`):
```cpp
// Eski (o'zgarish kerak):
const auto backupDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) 
                     + "/Telegram_AntiDelete/";
QFile::copy(cachePath, backupDir + QString::number(id) + "_" + fileName);
```

**Yangi** (yagona):
```cpp
// ASOSI: ~/customizationMainFolder/medias/
// TUZILISHI:
//   ~/customizationMainFolder/medias/
//   ├── photo/
//   │   └── {peerId}_{msgId}_{timestamp}.jpg
//   ├── document/
//   │   └── {peerId}_{msgId}_{filename}
//   ├── voice/
//   │   └── {peerId}_{msgId}_{timestamp}.ogg
//   ├── video/
//   │   └── {peerId}_{msgId}_{filename}
//   └── manifest.json
//       {
//         "version": 2,
//         "hasMedia": true,
//         "counts": { "photo": 124, "document": 456, ... }
//       }
```

**Helper function** (`custom_db.h`):
```cpp
namespace CustomDB {
    enum class MediaType {
        Photo,
        Document,
        Voice,
        Video,
        Unknown
    };
    
    QString GetMediaDir();  // ~/customizationMainFolder/medias
    QString GetMediaTypeDir(MediaType type);  // ~/customizationMainFolder/medias/photo
    QString GetMediaPath(const QString &peerId, long long msgId, 
                        MediaType type, const QString &filename);
    MediaType DetectMediaType(const QString &mimeType);
}
```

**Implement** (`custom_db.cpp`):
```cpp
QString CustomDB::GetMediaDir() {
    return QDir::homePath() + "/customizationMainFolder/medias";
}

QString CustomDB::GetMediaTypeDir(MediaType type) {
    const auto baseDir = GetMediaDir();
    switch (type) {
        case MediaType::Photo: return baseDir + "/photo";
        case MediaType::Document: return baseDir + "/document";
        case MediaType::Voice: return baseDir + "/voice";
        case MediaType::Video: return baseDir + "/video";
        default: return baseDir + "/unknown";
    }
}

QString CustomDB::GetMediaPath(
    const QString &peerId,
    long long msgId,
    MediaType type,
    const QString &filename) {
    
    const auto dir = GetMediaTypeDir(type);
    QDir().mkpath(dir);
    
    // Format: {peerId}_{msgId}_{sanitized_filename}
    const auto baseName = QString("%1_%2_%3")
        .arg(peerId)
        .arg(msgId)
        .arg(filename.isEmpty() ? QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") : filename);
    
    return dir + "/" + baseName;
}
```

**Update `data_document.cpp:1057`**:
```cpp
// ESKI:
const auto backupDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) 
                     + "/Telegram_AntiDelete/";
QFile::copy(cachePath, backupDir + QString::number(id) + "_" + fileName);

// YANGI:
const auto mediaPath = CustomDB::GetMediaPath(
    peerId,  // kontekstdan olish kerak
    msgId,
    CustomDB::MediaType::Document,
    fileName);
QFile::copy(cachePath, mediaPath);

// Shuningdek actioned_messages.media_path va media_type satrlariga qo'shish
CustomDB::SaveActionedMessage(peerId, msgId, "backup", 
                              originalText, mediaPath, "document");
```

**Tekshirish ro'yxati**:
- [ ] `GetMediaDir()` va `GetMediaTypeDir()` yaratish
- [ ] `MediaType` enum qo'shish
- [ ] `data_document.cpp:1057` yo'lni update qilish
- [ ] `media_path` va `media_type` ustunlarini schema'ga qo'shish (yoki ALTER TABLE)
- [ ] `manifest.json` builder yaratish statistika uchun
- [ ] Fayllarni yagona papkaga nusxalashni sinash

**Vaqt**: ~3 soat

---

#### Vazifa 2.2: Rasm Saqlash Hook'i Qo'shish

**Fayllar**: `data/data_photo.cpp`

**Hozirgi**: Hook yo'q, rasmlar AntiDelete'da saqlanmaydi

**Kerak**: Rasm yuklashning callback'i bo'lgan joyni topish (`photoLoaded`) va CustomDB'ga saqlash qo'shish.

**Implement** (`custom_db.cpp`):
```cpp
void CustomDB::SaveMediaFromPhoto(
    const QString &peerId,
    long long msgId,
    const QString &localPath,
    const QString &fileName) {
    
    if (localPath.isEmpty()) return;
    
    const auto destPath = GetMediaPath(peerId, msgId, MediaType::Photo, fileName);
    if (!QFile::copy(localPath, destPath)) {
        qDebug() << "CustomDB::SaveMediaFromPhoto failed:" << destPath;
        return;
    }
    
    // DB'da saqlash
    SaveActionedMessage(peerId, msgId, "backup", "", destPath, "photo");
}
```

**Hook qo'shish joyi**: `data_photo.cpp`'da `photoLoaded()` callback'i yoki shunga o'xshash joyda:
```cpp
// data_photo.cpp'da, photoLoaded() callback'ining ichida:
CustomDB::SaveMediaFromPhoto(peerId, msgId, cacheFilePath, fileName);
```

**Tekshirish ro'yxati**:
- [ ] `data_photo.cpp`'ni o'qib `photoLoaded()` callback'i to'g'ri joyini topish
- [ ] `CustomDB::SaveMediaFromPhoto()` `custom_db.cpp`'ga qo'shish
- [ ] `photoLoaded()`'dan to'g'ri parametrlar bilan chaqirish
- [ ] Sinash: chat'dan rasm yuklab, `~/customizationMainFolder/medias/photo/` papkasida faylni tekshirish

**Vaqt**: ~2 soat

---

#### Vazifa 2.3: Import uchun Merge Strategiyasini Implement Qilish (Strategiya Tanlash)

**Tavsiya**: **Timestamp asosida merge** (yangi ustun oldiradi, barcha ma'lumotlar saqlanadi)

**Asoslash**:
- **Overwrite (hozirgi)**: Sodda, lekin 6–7 kunlik ma'lumot yo'qoladi
- **Union**: Hammasini saqla, lekin konfliktlar hal qilinmagan (dublikatlar)
- **Timestamp**: Bitta xabar uchun — eng yangi versiyasi olinadi (yangi), lekin BAR databasedagi barcha xabarlar ikkala backup'dan qo'shiladi

**Algoritm**:
1. Import qilinayotgan ZIP'ni ochish, vaqtinchalik DB chiqarish
2. `ATTACH DATABASE` uni qilish
3. Har bir (peer_id, msg_id, type) uchun:
   - Hozirgi DB'da bor bo'lsa → timestamp solishtirish → yangi ALMASHTIRILADI
   - Hozirgi DB'da yo'q bo'lsa → qo'shish (INSERT OR IGNORE)
4. JSON (peer_lists.json) uchun → oq/qora listlarni UNION qilish
5. Registry uchun → per-peer override'larni merge qilish

**Implement** (`custom_db.cpp`):
```cpp
bool ImportFullBackup(const QString &sourcePath) {
    Init();
    if (!gDb) return false;
    
    // Bosqich 1: Source DB'ni chiqarish yoki joylash
    const QString sourceDb = (sourcePath.endsWith(".zip"))
        ? ExtractZipToTemp(sourcePath, "actioned_messages.db")
        : sourcePath + "/actioned_messages.db";
    
    if (!QFile::exists(sourceDb)) {
        qDebug() << "ImportFullBackup: source DB topilmadi:" << sourceDb;
        return false;
    }
    
    // Bosqich 2: ATTACH source database
    sqlite3_stmt *stmt = nullptr;
    const QString attachQuery = QString(
        "ATTACH DATABASE '%1' AS import_db").arg(sourceDb);
    
    if (sqlite3_prepare_v2(gDb, attachQuery.toStdString().c_str(), 
                          -1, &stmt, nullptr) != SQLITE_OK) {
        qDebug() << "ATTACH failed";
        return false;
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    // Bosqich 3: Timestamp solishtirish bilan ma'lumotni merge qilish
    // import_db'dan barcha yozuvlarni olish
    stmt = nullptr;
    const char *mergeQuery = R"(
        INSERT OR REPLACE INTO main.actioned_messages
        (peer_id, msg_id, type, timestamp, original_text, media_path, media_type)
        SELECT 
            imp.peer_id, imp.msg_id, imp.type, imp.timestamp, imp.original_text,
            imp.media_path, imp.media_type
        FROM import_db.actioned_messages imp
        WHERE NOT EXISTS (
            SELECT 1 FROM main.actioned_messages cur
            WHERE cur.peer_id = imp.peer_id
              AND cur.msg_id = imp.msg_id
              AND cur.type = imp.type
              AND cur.timestamp >= imp.timestamp
        )
    )";
    
    if (sqlite3_prepare_v2(gDb, mergeQuery, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Bosqich 4: Media fayllarini nusxalash (agar backup media bo'lsa)
    const QString sourceMediaDir = (sourcePath.endsWith(".zip"))
        ? ExtractZipToTemp(sourcePath, "customizationMainFolder/medias")
        : sourcePath + "/customizationMainFolder/medias";
    
    if (QDir(sourceMediaDir).exists()) {
        MergeMediaDirs(sourceMediaDir, GetMediaDir());
    }
    
    // Bosqich 5: JSON fayllarini merge qilish
    MergePeerLists(
        sourcePath + "/peer_lists.json",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) 
            + "/CustomMod/peer_lists.json");
    
    // Bosqich 6: Detach qilish
    sqlite3_prepare_v2(gDb, "DETACH DATABASE import_db", 
                       -1, &stmt, nullptr);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    // Cache invalidate qilish — qayta yuklash kerak
    {
        QMutexLocker locker(&gCacheMutex);
        gDeletedCache.clear();
        gEditedCache.clear();
    }
    
    return true;
}

// Helper: Media papkalarini merge qilish
void MergeMediaDirs(const QString &source, const QString &dest) {
    QDir sourceDir(source);
    QDir destDir(dest);
    
    if (!destDir.exists()) {
        QDir().mkpath(dest);
    }
    
    for (const auto &typeDir : sourceDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString typeSource = source + "/" + typeDir;
        const QString typeDest = dest + "/" + typeDir;
        
        QDir().mkpath(typeDest);
        
        for (const auto &file : QDir(typeSource).entryList(QDir::Files)) {
            const QString srcFile = typeSource + "/" + file;
            const QString destFile = typeDest + "/" + file;
            
            // Agar dest mavjud va yangi bo'lsa o'zgartirmang
            if (!QFile::exists(destFile) || 
                QFileInfo(srcFile).lastModified() > QFileInfo(destFile).lastModified()) {
                QFile::copy(srcFile, destFile);
            }
        }
    }
}
```

**UI strategiyani tanlash uchun** (`custom_mod_window.cpp:1720`):
```cpp
// Eski (faqat overwrite):
const auto reply = QMessageBox::warning(dialogParent,
    u"Zaxiradan tiklash"_q,
    u"Bu amal JORIY arxivni O'CHIRIB,\n"
    "importlanayotgan bilan almashtiriladi.\n"
    "Davom etasizmi?"_q,
    QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

// Yangi (strategiya tanlash):
const auto dialog = new Ui::ConfirmBox(
    parent,
    object_ptr<Ui::VerticalLayout>(parent,
        object_ptr<Ui::FlatLabel>(parent, 
            u"Tiklash usuli:"_q, st::labelDefaults),
        object_ptr<Ui::RadioButton>(parent, 
            u"Birlashtirish (tafsiya) — hozirgi va yangi ma'lumotlarni saqlash"_q,
            true),  // Asosiy
        object_ptr<Ui::RadioButton>(parent,
            u"⚠️ Almashtrish (xavfli) — joriy arxivni o'chirish"_q,
            false)));

int strategy = MERGE;  // 0 = MERGE, 1 = OVERWRITE
// ...
```

**Tekshirish ro'yxati**:
- [ ] `ATTACH DATABASE` logikasi `ImportFullBackup()`'ga qo'shish
- [ ] Merge query timestamp solishtirish bilan implement qilish
- [ ] `MergeMediaDirs()` helper yaratish
- [ ] `MergePeerLists()` helper yaratish
- [ ] `custom_mod_window.cpp`'dagi UI strategiya tanlashi bilan update qilish
- [ ] Sinash: 1 haftalik ma'lumotli backup import qilish, hozirgi 6 kunlik ma'lumot qolish tekshirish

**Vaqt**: ~5 soat

---

### Fase 3: Advanced Xususiyatlar (7–10 kun)

#### Vazifa 3.1: Forward Bypass Kaskadini Implement Qilish

**Fayllar**: `apiwrap.cpp`, `custom_db.h/cpp`

**Arxitektura**:
```
forwardMessages(toChat, messages)
  ↓
[1] Sinash: MTPmessages_ForwardMessages (NATIVE)
    ├─ Muvaffaq? Qaytarish
    └─ Xato (FLOOD_WAIT, CHAT_FORWARDS_RESTRICTED, file reference expired)?
         ↓
[2] Sinash: SendExistingDocument/Photo (file reference bilan)
    ├─ Muvaffaq? Qaytarish
    └─ File reference muddati o'tgan yoki noto'g'ri?
         ↓
[3] Sinash: CustomDB::FindSavedMediaPath() + qayta upload qilish
    ├─ Fayl topilgan? Upload va yuborish
    └─ Topilmagan?
         ↓
[4] Sinash: Telegram cache'i (doc->filepath(true))
    ├─ Topilgan? Upload va yuborish
    └─ Topilmagan?
         ↓
[5] FALLBACK: Faqat matn yuborish (original_text DB'dan)
         ↓
Toast: "❌ Media topilmadi, faqat matn yuborildi"
```

**Helper function** (`custom_db.h/cpp`):
```cpp
// custom_db.h
struct SavedMediaInfo {
    QString path;
    QString mimeType;
    long long fileSize;
    CustomDB::MediaType type;
};

std::optional<SavedMediaInfo> FindSavedMediaPath(
    const QString &peerId,
    long long msgId);
```

**Implement** (`custom_db.cpp`):
```cpp
std::optional<SavedMediaInfo> CustomDB::FindSavedMediaPath(
    const QString &peerId,
    long long msgId) {
    
    Init();
    if (!gDb) return {};
    
    sqlite3_stmt *stmt = nullptr;
    const char *query = R"(
        SELECT media_path, media_type FROM actioned_messages
        WHERE peer_id = ? AND msg_id = ?
        ORDER BY rowid DESC LIMIT 1
    )";
    
    if (sqlite3_prepare_v2(gDb, query, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }
    
    bindText(stmt, 1, peerId);
    sqlite3_bind_int64(stmt, 2, msgId);
    
    std::optional<SavedMediaInfo> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const QString path = colText(stmt, 0);
        const QString typeStr = colText(stmt, 1);
        
        if (!path.isEmpty() && QFile::exists(path)) {
            SavedMediaInfo info;
            info.path = path;
            info.fileSize = QFileInfo(path).size();
            
            // Extension'dan yoki type field'dan MIME type'ni aniqlash
            if (typeStr == "photo") {
                info.mimeType = "image/jpeg";
            } else if (typeStr == "document") {
                info.mimeType = MimeTypeFromExtension(path);
            } else if (typeStr == "voice") {
                info.mimeType = "audio/ogg";
            } else if (typeStr == "video") {
                info.mimeType = "video/mp4";
            }
            
            info.type = (typeStr == "photo") ? MediaType::Photo :
                       (typeStr == "document") ? MediaType::Document :
                       (typeStr == "voice") ? MediaType::Voice :
                       (typeStr == "video") ? MediaType::Video : MediaType::Unknown;
            
            result = info;
        }
    }
    
    sqlite3_finalize(stmt);
    return result;
}

QString MimeTypeFromExtension(const QString &filePath) {
    const auto ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "pdf") return "application/pdf";
    if (ext == "docx") return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";
    if (ext == "mp3") return "audio/mpeg";
    if (ext == "m4a") return "audio/mp4";
    if (ext == "ogg") return "audio/ogg";
    if (ext == "mp4") return "video/mp4";
    // ... qo'shimcha
    return "application/octet-stream";
}
```

**Update `apiwrap.cpp`** (taxminiy joylashuv):
```cpp
// ESKI (faqat native forward):
void ApiWrap::forwardMessages(
    const HistoryItemsList &items,
    Data::Folder *toFolder,
    std::shared_ptr<Ui::Toast::Instance> &&toast) {
    
    auto ids = ranges::views::transform(items, [](const auto &item) {
        return item->id;
    }) | ranges::to_vector;
    
    // Native API chaqirish
    request(MTPmessages_ForwardMessages(
        MTP_flags(0),
        // ...
    )).done([this, toast](const MTPUpdates &result) {
        // Muvaffaq
        applyUpdates(result);
    }).fail([this, toast](const MTP::Error &error) {
        // XATO — fallback yo'q!
        if (error.type() == qstr("FLOOD_WAIT")) {
            // ...
        }
    }).send();
}

// YANGI (kaskadi bilan):
void ApiWrap::forwardMessages(
    const HistoryItemsList &items,
    Data::Folder *toFolder,
    std::shared_ptr<Ui::Toast::Instance> &&toast) {
    
    auto ids = ranges::views::transform(items, [](const auto &item) {
        return item->id;
    }) | ranges::to_vector;
    
    // [1] Native forward'ni sinash
    request(MTPmessages_ForwardMessages(
        MTP_flags(0),
        // ...
    )).done([this, toast](const MTPUpdates &result) {
        applyUpdates(result);
        // Muvaffaq!
    }).fail([this, items, toast](const MTP::Error &error) {
        // [2] Xato boshqarish — fallback'ni sinash
        tryForwardFallback(items, error, toast);
    }).send();
}

void ApiWrap::tryForwardFallback(
    const HistoryItemsList &items,
    const MTP::Error &error,
    std::shared_ptr<Ui::Toast::Instance> &&toast) {
    
    // Agar xato tiklash mumkin bo'lsa (FLOOD_WAIT, FORWARDS_RESTRICTED), [2–5] ni sinash
    if (error.type() == qstr("FLOOD_WAIT") ||
        error.type() == qstr("CHAT_FORWARDS_RESTRICTED") ||
        error.type() == qstr("FILE_REFERENCE_EXPIRED")) {
        
        for (const auto &item : items) {
            if (const auto media = item->media()) {
                // [3] Saqlangan media'ni topish
                const auto savedMedia = CustomDB::FindSavedMediaPath(
                    QString::number(item->chatId()),
                    item->id);
                
                if (savedMedia) {
                    // [3a] CustomDB'dan qayta upload qilish
                    uploadAndForwardMedia(item, savedMedia.value(), toast);
                } else if (const auto doc = media->document()) {
                    // [4] Telegram cache'ni sinash
                    const auto cachePath = doc->filepath(true);
                    if (!cachePath.isEmpty()) {
                        uploadAndForwardMedia(item, cachePath, toast);
                    } else {
                        // [5] Fallback: faqat matn
                        forwardAsTextOnly(item, toast);
                    }
                }
            } else {
                // Media yo'q, faqat matni forward qilish
                forwardAsTextOnly(item, toast);
            }
        }
    }
}

void ApiWrap::forwardAsTextOnly(
    const HistoryItem *item,
    std::shared_ptr<Ui::Toast::Instance> &&toast) {
    
    // Agar tahrir qilingan bo'lsa CustomDB'dan original matn olish
    const auto originalText = CustomDB::GetOriginalTextBeforeEdit(
        QString::number(item->chatId()),
        item->id);
    
    const auto text = originalText.isEmpty() 
        ? item->plainText() 
        : originalText;
    
    // Oddiy xabar sifatida yuborish
    sendMessage(toChat, text);
    
    // Foydalanuvchiga xabar
    Ui::Toast::Show(u"❌ Media topilmadi, faqat matn yuborildi"_q);
}
```

**Tekshirish ro'yxati**:
- [ ] `FindSavedMediaPath()` `custom_db.cpp`'ga qo'shish
- [ ] `MimeTypeFromExtension()` implement qilish
- [ ] `SavedMediaInfo` struct `custom_db.h`'ga qo'shish
- [ ] `apiwrap.cpp`'ni kaskadi logikasi bilan update qilish
- [ ] `tryForwardFallback()`, `uploadAndForwardMedia()`, `forwardAsTextOnly()` yaratish
- [ ] Sinash: eski xabar forward'ni sinash, file ref expired bo'lgan
- [ ] Toast'larning to'g'ri ko'rsatilishini tekshirish

**Vaqt**: ~6 soat

---

#### Vazifa 3.2: UI/UX Audit va Yaxsilishtirish

**Fayllar**: `custom_mod_window.cpp`, `custom_settings.cpp`

**Hozirgi muammolar** (ScreenShot'lardan):
1. About tab juda tort (kichik padding elementlar orasida)
2. Matn tort, statistika vizual jihatdan ajralib turmasligi
3. "XAVFLI HUDUD" (xavf operatsiyalari) vizual ogohlantiruvi yo'q (qizil chegara/fon)
4. Tugmalar kichik, kuch iyerarhiyasi
5. Qismlar juda shunga o'xshash ko'rinadi

**Nima o'zgarishi kerak**:

**1) Spacing va Typography** (`custom_mod_window.cpp`):
```cpp
// Eski (tort):
content->add(labelStat, st::boxRowPadding);  // padding 12px
labelStat->setText(u"O'chirilgan: 167,224"_q);

// Yangi (kengash):
auto statRow = object_ptr<Ui::VerticalLayout>(content);
statRow->add(object_ptr<Ui::FlatLabel>(content,
    u"O'chirilgan"_q,
    st::labelDefaults));  // "O'chirilgan" — kulrang label
statRow->add(object_ptr<Ui::FlatLabel>(content,
    u"167,224"_q,
    st::labelBold));  // "167,224" — qalin, katta shrift
statRow->add(spacing(8px));

content->add(std::move(statRow), 
    st::boxRowPadding);  // Statistika blok 20px margin bilan
```

**2) Xavf Operatsiyalari Styling** (`custom_mod_window.cpp:1720–1734`):
```cpp
// Eski (oddiy tugma):
content->add(
    object_ptr<Ui::RoundButton>(content,
        u"📥 Zaxira nusxadan tiklash"_q,
        st::defaultBoxButton),
    st::boxRowPadding);

// Yangi (qizil chegara + ogohlantiruv):
auto importBtn = object_ptr<Ui::RoundButton>(content,
    u"📥 Zaxiradan tiklash"_q,
    st::dangerBoxButton);  // Custom style: qizil chegara, engil qizil fon

importBtn->setStyleSheet(R"(
    QPushButton {
        border: 2px solid #e74c3c;
        border-radius: 8px;
        background-color: #fadbd8;
        color: #c0392b;
        font-weight: bold;
        padding: 10px 20px;
    }
    QPushButton:hover {
        background-color: #f5b7b1;
    }
    QPushButton:pressed {
        background-color: #fadbd8;
    }
)");

content->add(std::move(importBtn), st::boxRowPadding);
```

**3) Stats Section Qayta Dizayni** (`custom_mod_window.cpp`, ~1600–1650):
```cpp
// Yangi tuzilma:
auto statsLayout = object_ptr<Ui::VerticalLayout>(content);

// Sarlavha
statsLayout->add(object_ptr<Ui::FlatLabel>(content,
    u"📊 Arxiv"_q,
    st::labelLargeDefaults));  // Qalin, 16pt

// Grid statistika
auto statsGrid = object_ptr<Ui::GridLayout>(content);
statsGrid->setRowSpacing(12);
statsGrid->setColumnSpacing(24);

// Har bir statistika: emoji + raqam + label
const auto addStat = [&](const QString &emoji, 
                         const QString &number,
                         const QString &label) {
    auto statCol = object_ptr<Ui::VerticalLayout>(content);
    statCol->add(object_ptr<Ui::FlatLabel>(content,
        emoji + " " + number,
        st::labelBold));  // Katta raqam
    statCol->add(object_ptr<Ui::FlatLabel>(content,
        label,
        st::labelSmallDefaults));  // Kichik kulrang label
    statsGrid->addWidget(std::move(statCol));
};

addStat(u"🗑️"_q, u"167,224"_q, u"O'chirilgan");
addStat(u"✏️"_q, u"43,045"_q, u"Tahrirlangan");
addStat(u"📷"_q, u"12,456"_q, u"Rasmlar");
addStat(u"📄"_q, u"34,782"_q, u"Hujjatlar");

statsLayout->add(std::move(statsGrid));

// Ajratuvchi
statsLayout->add(object_ptr<Ui::PlainShadow>(content), 
                 object_ptr<Ui::LayoutData>());

content->add(std::move(statsLayout), st::boxRowPadding);
```

**4) Tab Visual Ajratish**:
```cpp
// Tanlangan/tanlanmagan qism uchun style'ni update qilish
// st::boxTabsContainer yoki custom style:
//   - Tanlangan qism: qalin, rang asosidagi pastki chegara (ko'k)
//   - Tanlanmagan qism: kulrang, chegara yo'q
```

**Tekshirish ro'yxati**:
- [ ] About qismdagi padding'ni oshirish (12px o'rniga 20px)
- [ ] Stats qismini grid layout bilan qayta qilish
- [ ] Xavf tugmalari uchun qizil chegara/fon qo'shish
- [ ] Tipografiyani yaxshilash (sarlavhalar qalin, raqamlar kattaroq)
- [ ] Har bir statistika uchun emoji qo'shish
- [ ] Turli ekran o'lchamlarida sinash (mobile, tablet, desktop)
- [ ] Solishtirish uchun before/after skrinshotni olish

**Vaqt**: ~4 soat

---

### Fase 4: Integratsiya va Tekshirish (11–14 kun)

#### Vazifa 4.1: Backup Export'da Media'ni Qo'shish

**Fayllar**: `custom_db.cpp` (funksiya allaqachon Vazifa 1.1'da update qilingan)

**Kerak**:
- `ExportFullBackup()` → media papkasi ~/customizationMainFolder/medias endi unified (Vazifa 2.1'dan keyin)
- manifest.json'ni media ma'lumoti bilan update qilish

**ExportFullBackup'da Yangi** (~1092-qator):
```cpp
// 2) Media papkasi (yaxshilangan, endi unified):
const QString mediaSrc = QDir::homePath() + "/customizationMainFolder";
if (QDir(mediaSrc).exists()) {
    if (!CopyDirRecursive(mediaSrc, stageDir + "/customizationMainFolder")) {
        QDir(stageDir).removeRecursively();
        return {};
    }
}

// YANGI: manifest.json'ni media statistikasi bilan yaratish
{
    const QString manifestPath = stageDir + "/manifest.json";
    QJsonObject manifest;
    manifest["version"] = 2;
    manifest["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    manifest["hasMedia"] = true;
    
    // Statistikani hisoblash
    QJsonObject counts;
    counts["deleted"] = countMessagesWithType("deleted");
    counts["edited"] = countMessagesWithType("edited");
    counts["photos"] = QDir(stageDir + "/customizationMainFolder/medias/photo").entryList(QDir::Files).count();
    counts["documents"] = QDir(stageDir + "/customizationMainFolder/medias/document").entryList(QDir::Files).count();
    counts["voices"] = QDir(stageDir + "/customizationMainFolder/medias/voice").entryList(QDir::Files).count();
    
    manifest["counts"] = counts;
    
    // Yozish
    QFile file(manifestPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(manifest).toJson());
        file.close();
    }
}
```

**Tekshirish ro'yxati**:
- [ ] Vazifa 2.1 tugallanganligi tekshirish (unified paths)
- [ ] Backup manifest.json'ni counts bilan update qilish
- [ ] Import paytida manifest.json'ni tekshirish va "X rasm, Y hujjat importlanmoqda" degan xabar ko'rsatish
- [ ] Sinash: backup → tiklash → barcha media ko'chirilishini tekshirish

**Vaqt**: ~1 soat

---

#### Vazifa 4.2: Tekshirish va Tasdiqash Suite

**Nima tekshirilishi kerak**:

| Stsena | Sinash | Muvaffaqiyat Mezoni |
|---|---|---|
| **Startup** | Ilovani ishga tushirish, CustomMod oynasini ochish | < 100 ms birinchi UI javobi, < 500 ms to'liq rendering |
| **Backup export** | 10 GB media bilan export | < 200 ms UI freeze, progress har 100 ms'da update |
| **Backup import** | 5 GB backup import | < 200 ms UI freeze, merge ishlaydi (hozirgi + import qilingan) |
| **Per-peer cache** | 10 ta turli chat ochish | Har biri < 50 ms yuklash, xotira footprint < 10 MB |
| **Photo save hook** | Rasm yuklab chat'ga | Fayl ~/customizationMainFolder/medias/photo/ da ko'rinadi |
| **Document path** | Hujjat yuklab chat'ga | Fayl ~/customizationMainFolder/medias/document/'da, eski path'da emas |
| **Forward cascade** | Eski xabar forward'ni sinash | Fallback ishlaydi, media yo'q bo'lsa matn yuboriladi |
| **UI styling** | CustomMod oynasini ochish | Tugmalar ko'rinadi, spacing OK, xavf tugmasida qizil chegara |

**Tekshirish ro'yxati**:
- [ ] Har bir stsena uchun test script yaratish
- [ ] Haqiqiy ma'lumotlar bilan ishga tushirish (agar eski backup bo'lsa 210k xabar bilan)
- [ ] Memory/CPU profile qilish (Qt Creator Profiler tools)
- [ ] UI uchun before/after skrinshotlarini olish
- [ ] Dokumentatsiya: talablar → realizatsiya mapping

**Vaqt**: ~3 soat

---

## Muhim Implement Tartibi

```
Kun    | Vazifa                          | Bog'lanishi  | Soat | Xodim
-------|----------------------------------|--------------|------|----------
1–2    | Vazifa 1.1 (asinxron export)     | Yo'q         | 4    | A
1–2    | Vazifa 1.2 (lazy cache)          | Yo'q         | 6    | B
3      | Vazifa 2.1 (media unifikatsiya)  | Yo'q         | 3    | A
3      | Vazifa 2.2 (rasm hook)           | Vazifa 2.1   | 2    | B
4      | Vazifa 2.3 (merge strategiyasi)  | Vazifa 1.1   | 5    | A
5      | Vazifa 3.1 (forward kaskadi)     | Vazifa 2.1   | 6    | B
6      | Vazifa 3.2 (UI audit)            | Yo'q         | 4    | A
7      | Vazifa 4.1 (backup manifest)     | Vazifa 2.1   | 1    | B
7      | Vazifa 4.2 (tekshirish)          | Hammasi      | 3    | A+B
```

---

## Fayl O'zgarishlari Xulasasi

| Fayl | Status | Qatorlar | Izoh |
|---|---|---|---|
| `custom_db.h` | Modify | +50 | Yangi enum, struct, function signatures |
| `custom_db.cpp` | Modify | +300 | Asinxron export, merge, per-peer cache |
| `custom_async_worker.h` | Create | 100 | Callback struct, progress enum |
| `custom_settings.cpp` | Modify | +30 | startAsyncCacheLoader() chaqirish |
| `custom_mod_window.cpp` | Modify | +100 | Asinxron tugmalar, UI qayta dizayni |
| `data/data_photo.cpp` | Modify | +5 | Hook chaqirish photoLoaded()'da |
| `data/data_document.cpp` | Modify | +2 | Unified yo'lga update |
| `apiwrap.cpp` | Modify | +100 | Forward kaskadi logikasi |
| `db/db_peer_cache.h` | Create | 80 | PerPeerCache class |
| `db/db_peer_cache.cpp` | Create | 120 | ensureLoadedPeer() impl |
| **Jami** | — | ~792 | +3 yangi fayl, 9 modified |

---

## Muhim Eslatmalar va Tavsiyalar

### #2 Multi-Device Merge Strategiyasi

**Tanlash**: **Timestamp asosida merge** o'rniga overwrite.

**Sabablar**:
1. **Overwrite (hozirgi)** — ma'lumot yo'qolishiga olib keladi. Real foydalanuvchi 6 kunlik ishni yo'qotdi.
2. **Timestamp** — BARCHA ma'lumotlarni saqlaydi, konfliktlar tiniq hal qilinadi (yangi ustun oldiradi).
3. **UX** — foydalanuvchiga aniq tanlov berish dialog orqali ("Merge vs Replace").

**Realizatsiya**: INSERT OR REPLACE with timestamp'dagi WHERE sharti (Vazifa 2.3'ga qarang).

---

### #3 Startup Freeze Yechimi: A Varianti (Lazy Per-Peer)

**Tanlash**: **A Varianti (Lazy per-peer cache)** o'rniga to'liq jadvali asinxron.

**Asoslash**:
- **A Varianti**: 
  - Startup: < 100 ms (bo'sh)
  - Per-peer load: < 50 ms (peer_id'dagi INDEX)
  - Xotira: 2–5 MB (faqat ochilgan chatlar)
  - 1M+ xabar'da masshtabnashuvga tayyor
  
- **B Varianti (Full async background)**:
  - Startup: 1–3 soniya sekinroq ko'rinadi
  - Xotira: 50+ MB doimiy
  - Katta arxivlarda masshtabnashuvga tayyor emas

Foydalanuvchi butun shu fikrni taklif qilgan ("birinchi ochilayotgan chat'ning ma'lumotini DB'dan o'qish kerak").

**Realizatsiya**: `PerPeerCache::ensureLoadedPeer()` with INDEX (Vazifa 1.2'ga qarang).

---

### Threading Modeli

Barcha asinxron operatsiyalar **tdesktop'ning built-in `crl::async` + `crl::on_main`**'dan foydalanish:

```cpp
// Background thread:
crl::async([] {
    // I/O, CPU-heavy ish bu yerda
    // UI'ga KIRMA!
    const auto result = doHeavyWork();
    
    // Main thread'ga qaytarish:
    crl::on_main([result] {
        // UI'ni result bilan update qilish
        showToast(result);
    });
});
```

**Afzalliklari**:
- Tashqi bog'lantirib yo'q (tdesktop'da built-in)
- Xavf safe (Rust-style Result)
- Event loop'ga integratsiya (qo'lda thread boshqarish yo'q)
- Loyihaning barcha bo'limida ishlatiladi (window/, ui/, data/)

---

### Database Thread Safety

**Hozirgi SQL thread safety**:
```cpp
// custom_db.cpp
sqlite3_open_v2(dbPath, &gDb, 
    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
    nullptr);  // ← FULLMUTEX YO'Q!
```

**O'zgarishi kerak**:
```cpp
sqlite3_open_v2(dbPath, &gDb, 
    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
    nullptr);
sqlite3_config(SQLITE_CONFIG_SERIALIZED);  // FULLMUTEX ni yoqish
```

Yoki ochashda:
```cpp
sqlite3_db_config(gDb, SQLITE_DBCONFIG_ENABLE_FTS5, 1, nullptr);
// SQLite bu connection uchun thread-safe
```

---

### Merge'dan Oldin Kontrol Nuqtalari

Main'ga o'tashddan oldin:

1. **Performance**:
   - [ ] Startup < 100 ms
   - [ ] Backup export < 200 ms UI freeze (10 GB'da)
   - [ ] Per-peer cache < 50 ms load
   - [ ] Xotira < 20 MB (baseline + caches)

2. **Ma'lumot Integralliligi**:
   - [ ] Merge testi: backup → tiklash → ikkala dataset'ni tekshirish
   - [ ] Import paytida ma'lumot yo'qolmashi
   - [ ] Media fayllar to'g'ri DB'ga ko'chirilish va bog'lanish

3. **UX**:
   - [ ] Progress indicatorlar ishlashi
   - [ ] Toast xabarlar to'g'ri ko'rsatilishi
   - [ ] Xavf tugmalarida qizil chegara ko'rinishi
   - [ ] Turli ekran o'lchamlarida visual to'qonuq yo'q

4. **Kod Sifati**:
   - [ ] Compiler ogohlantirlari yo'q
   - [ ] Barcha catch bloklar to'ldirilgan (silent xatolar yo'q)
   - [ ] Murakkab satsallarda izohlar
   - [ ] tdesktop kod stiliga uygunlik

---

## Taxminiy Timeline

- **Fase 1 (1–3 kun)**: Threading + Cache = 10 soat
- **Fase 2 (4–6 kun)**: Media Asosi = 10 soat
- **Fase 3 (7–10 kun)**: Advanced Xususiyatlar = 10 soat
- **Fase 4 (11–14 kun)**: Tekshirish + Tasdiqash = 4 soat
- **Buffer (10%)**: ~4 soat

**Jami**: 38 soat (~1 hafta 1 xodim uchun, ~3–4 kun 2 xodim parallel ishlaganda)

---

## Adreslar va Manbalar

- **tdesktop threading**: `/tdesktop/base/crl/crl_async.h`, misollar window/, ui/'da
- **SQLite thread safety**: https://www.sqlite.org/threadsafe.html (SERIALIZED rejimi)
- **Qt fayl operatsiyalari**: `QFile::copy()`, `QDir::mkpath()`, `QJsonDocument`
- **Custom DB schema**: `custom_db.cpp:createSchema()`
- **UI controls**: `Ui::RoundButton`, `Ui::FlatLabel`, `Ui::Toast`

---

**Plan realizatsiyaga tayyorlangan. Vazifa 1.1 (asinxron export) va Vazifa 1.2 (lazy cache) bilan parallel ravishda boshlang.**
