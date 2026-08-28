#pragma once

#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QVector>
#include <QtCore/QHashFunctions>
#include <functional>

class HistoryItem;

namespace CustomDB {

// Akkaunt + peer juftligi. Nima uchun alohida tur: 12 ta akkaunt
// bitta bazaga yozadi, fon akkauntlari ham. Bitta QString peerId
// yetarli emas edi -- 2026-08-26 da aralashuv xatosi shundan chiqqan.
// Standart qiymat ATAYLAB yo'q: kompilyator har chaqiruv joyini
// yangilashga majbur qilsin, aks holda unutilgan joy jimgina 0 yozadi.
struct PeerKey {
    qint64 accountId = 0;
    QString peerId;

    [[nodiscard]] bool operator<(const PeerKey &other) const {
        return (accountId != other.accountId)
            ? (accountId < other.accountId)
            : (peerId < other.peerId);
    }
    [[nodiscard]] bool operator==(const PeerKey &other) const {
        return accountId == other.accountId && peerId == other.peerId;
    }
};

[[nodiscard]] inline size_t qHash(const PeerKey &key, size_t seed = 0) {
    return qHashMulti(seed, key.accountId, key.peerId);
}

struct ActionedMessage {
    qint64 accountId = 0;
    QString peerId;
    long long msgId = 0;
    QString type; // "deleted", "edited", "backup"
    QString originalText;
    QString newText;
    QString mediaPath;
    bool isOut = false;
    unsigned int msgDate = 0;
    QDateTime timestamp;
    QString senderId;       // v5: guruhda haqiqiy yuboruvchi peer id (bo'sh = noma'lum)
    bool isMedia = false;   // v5: media xabar (matn bo'sh bo'lsa ham o'chirilganini biламiz)
};

// E19: Schema version constant — increment when adding new columns/tables.
// RunMigrations() is called inside Init() automatically.
// v11 (A16 §1): activity_history ga source ustuni
constexpr int kCurrentSchemaVersion = 11;

void Init();

// E19: Run any pending schema migrations up to kCurrentSchemaVersion.
// Called automatically by Init(); safe to call multiple times.
void RunMigrations();

// Startup: load all deleted/edited records into fast in-memory caches
void LoadRestoreCache();

// Fast O(1) lookups used in HistoryItem constructor
bool IsDeletedLocally(const PeerKey &key, long long msgId);
QString GetOriginalTextBeforeEdit(const PeerKey &key, long long msgId);

// Returns all saved pre-edit text versions for a message, oldest first.
// Each entry is the text *before* that edit was applied.
// Empty if the message was never edited (or not tracked).
QVector<QString> GetEditHistory(const PeerKey &key, long long msgId);

void SaveGhostRead(const PeerKey &key, long long msgId);
long long GetGhostRead(const PeerKey &key);

// E22: Remove a peer's ghost-read record (call when ghost mode is disabled for that peer).
void ResetGhostRead(const PeerKey &key);

// 2026-08-26: custom_settings.cpp dagi "shu peer uchun override'larni
// butunlay tozalash" amallari hech qanday sessiya/akkaunt konteksti
// bilan chaqirilmaydi (UI'da akkaunt tanlovchisi yo'q). ResetGhostRead()
// bo'sh (accountId=0) PeerKey bilan chaqirilsa faqat egasi noma'lum
// eski qatorlarni tozalaydi -- haqiqiy akkauntlarning yozuvi qolib
// ketadi. Bu funksiya barcha akkauntlar bo'yicha tozalaydi.
void ResetGhostReadForPeerAllAccounts(const QString &peerId);

// E22: Delete ghost_reads entries older than |days| days (default 30).
// Called automatically by SaveGhostRead(); safe to call manually.
void PruneStaleGhostReads(int days = 30);
void MarkDeleted(
    long long msgId,
    const PeerKey &key,
    const QString &mediaPath = QString(),
    const QString &originalText = QString(),
    unsigned int msgDate = 0,
    bool isOut = false,
    const QString &senderId = QString(),  // v5: guruhda haqiqiy yuboruvchi
    bool isMedia = false);                // v5: media xabar belgisi

// User-initiated delete support:
// Call before sending delete request to server so the message is truly removed.
// Removes all DB records for the message and prevents re-saving on server ACK.
void ScheduleUserDelete(const PeerKey &key, long long msgId);

// Returns true if the user themselves initiated deletion of this message.
bool IsUserDeletePending(const PeerKey &key, long long msgId);

// Clear the pending flag after processMessagesDeleted has handled the message.
void ClearUserDeletePending(const PeerKey &key, long long msgId);

// Permanently erase all records for a message from DB + in-memory caches.
void PermanentlyDeleteMessage(const PeerKey &key, long long msgId);
void SaveActionedMessage(const ActionedMessage &msg);
QString GetMessageHistory(long long msgId, const PeerKey &key);

struct DeletedMessage {
    long long msgId;
    QString mediaPath;
    bool isOut;
    unsigned int date;
    QString text;
    QString senderId;       // v5: guruhda haqiqiy yuboruvchi (bo'sh = noma'lum)
    bool isMedia = false;   // v5: media xabar edi
    qint64 accountId = 0;   // v10: 0 = egasi noma'lum (v10 dan oldingi yozuv)
};
QVector<DeletedMessage> GetDeletedMessages(const PeerKey &key);

// Cross-chat deleted messages archive: returns up to |limit| most recent deleted
// messages across all peers, sorted newest-first. Suitable for the archive viewer.
struct DeletedMessageWithPeer {
    QString peerId;
    long long msgId = 0;
    QString text;
    QString mediaPath;
    bool isOut = false;
    unsigned int date = 0;
    QString senderId;       // v5: guruhda haqiqiy yuboruvchi
    bool isMedia = false;   // v5: media xabar (mediaPath bo'lmasa ham)
};
QVector<DeletedMessageWithPeer> GetAllDeletedMessages(int limit = 300);

// Arxivdagi (media_index) faylning to'liq yo'li, yoki bo'sh.
// GetSavedMediaPath() dan farqi — u faqat O'CHIRILGAN xabarlarga
// qaraydi, bu esa butun L2 arxivini qamrab oladi.
[[nodiscard]] QString GetArchivedMediaPath(
    const PeerKey &key,
    long long msgId);

// A13/K1b: arxivda o'chirilgan xabari bor peer'lar ro'yxati (faqat ID lar).
// Ishga tushishda chat ro'yxatini tiklash uchun — matn/media yuklamaydi,
// shuning uchun startup'da arzon.
[[nodiscard]] QVector<QString> GetPeersWithDeletedMessages(qint64 accountId);

// ── Media indeks (schema v7) ─────────────────────────────────────────────
// Har bir media xabar shu yerda qayd etiladi — hatto fayl yuklanmagan
// bo'lsa ham. Shunday qilib "bunday fayl bor edi, lekin saqlanmadi"
// ma'lumoti yo'qolmaydi.
//
// status: present — fayl arxivda bor
//         pending — hozircha yo'q, lekin olish mumkin (chegara/kvota)
//         missing — yo'q va endi olib bo'lmaydi (reference eskirgan)
struct MediaIndexEntry {
    QString peerId;
    long long msgId = 0;
    QString kind;          // image | video | voice | file
    QString fileName;
    QString relPath;       // arxiv ildizidan nisbiy yo'l
    long long size = 0;
    QString sha256;        // hozircha bo'sh — 7.1-eslatmaga qarang
    unsigned int msgDate = 0;
    unsigned int archivedAt = 0;
    QString layer;         // l1 | l2 | l3
    QString status;
    QString reason;        // too_large | quota_full | reference_expired ...
};

void UpsertMediaIndex(const PeerKey &key, const MediaIndexEntry &entry);
void SetMediaIndexStatus(
    const PeerKey &key,
    long long msgId,
    const QString &status,
    const QString &reason);
[[nodiscard]] bool HasPresentMediaIndexEntry(
    const PeerKey &key,
    long long msgId);

// Eksport tanlash oynasi uchun: media'si BOR chatlar, hajmi bo'yicha
// kamayish tartibida. Foydalanuvchi qaysi chatni eksportga qo'shishni
// hajmiga qarab hal qiladi.
struct MediaPeerSummary {
    QString peerId;
    int fileCount = 0;
    long long totalBytes = 0;
};
[[nodiscard]] QVector<MediaPeerSummary> GetMediaPeerSummaries();

// Kvota uchun: barcha 'present' yozuvlar hajmi yig'indisi.
[[nodiscard]] long long TotalArchivedMediaBytes();

// Import'dan keyin: 'present' deb belgilangan, lekin fayli topilmagan
// yozuvlarni 'missing' ga o'tkazadi (bazada yolg'on ma'lumot qolmasin).
// archiveRoot — ~/customizationMainFolder. Nechta yozuv o'zgargani qaytadi.
int ReconcileMediaIndex(const QString &archiveRoot);

// Bir martalik BACKFILL skaneri: media_index v7 da paydo bo'lgan, ya'ni
// undan OLDIN arxivlangan fayllar indeksda yo'q va shuning uchun
// eksportga tushmaydi. Bu funksiya medias/ daraxtini aylanib chiqib,
// indeksda yo'q har bir fayl uchun yozuv qo'shadi.
//
// Fayl nomi sxemalari (tarixiy tartibda):
//   <peerId>_<msgId>_<nom>  — L2 (2026-08-14+), ikkalasi ham aniqlanadi
//   <msgId>_<nom>           — setDeletedLocally() zaxira yo'li
//   <nom>                   — SaveMediaFile(), hech qanday ma'lumot yo'q
//
// Aniqlab bo'lmaganlar peerId="0" ostida, sintetik msgId bilan
// saqlanadi — ular ham eksportga tusha olishi uchun (dalillar
// yo'qolmasin). Takroriy chaqiruv xavfsiz: mavjud yozuvlar tegilmaydi.
//
// Nechta YANGI yozuv qo'shilgani qaytadi.
int ScanArchiveMedia(const QString &archiveRoot);
void ScanArchiveMediaAsync(std::function<void(int added)> callback);

// ── Eski fayllarni TUZATISH (2026-08-15) ─────────────────────────────────
//
// 2026-08-15 dagi kengaytma tuzatishi faqat YANGI arxivlanadigan
// fayllarga ta'sir qiladi. Diskda esa `<peerId>_<msgId>_file` ko'rinishida,
// kengaytmasiz fayllar qolgan, va bir qismi noto'g'ri papkada
// (yumaloq videolar medias/voices/ da — eski `isVideoMessage() -> "voice"`
// tasnifi qoldig'i). Eksport diskdan nusxalagani uchun bu buzuqlik
// zaxiralarga ham o'tadi.
//
// Bu funksiya turni fayl NOMIDAN emas, MAZMUNIDAN aniqlaydi
// (QMimeDatabase::MatchContent — magic-byte sniffing), chunki eski
// fayllar uchun DocumentData endi mavjud emas.
//
// Mavjud kengaytma HECH QACHON almashtirilmaydi — faqat yo'q bo'lsa
// qo'shiladi. Mazmuni aniqlanmagan fayllarga TEGILMAYDI.
struct RepairReport {
    int scanned = 0;
    int extensionAdded = 0;  // kengaytma qo'shildi
    int movedFolder = 0;     // to'g'ri papkaga ko'chirildi
    int unknown = 0;         // mazmuni aniqlanmadi — tegilmadi
    int failed = 0;          // fayl tizimi xatosi
};

// dryRun = true bo'lsa hech narsa o'zgarmaydi, faqat hisobot qaytadi.
// Foydalanuvchiga avval shu hisobot ko'rsatiladi.
RepairReport RepairArchiveMedia(const QString &archiveRoot, bool dryRun);
void RepairArchiveMediaAsync(
    bool dryRun,
    std::function<void(RepairReport)> callback);

// C11: per-message edit history — all recorded versions across all chats.
struct EditRecord {
    QString peerId;
    long long msgId = 0;
    QString originalText; // text before this edit
    QString newText;      // text after this edit (may be empty if not captured)
    unsigned int msgDate = 0;
    QDateTime editedAt;   // when the edit was recorded locally
};
QVector<EditRecord> GetAllEditedMessages(int limit = 300);

void SaveMessage(HistoryItem *item);

// Flush any pending batched SaveMessage() writes immediately.
// Called automatically on a 100ms timer; also safe to call manually (e.g. before export).
void FlushPendingWrites();

// Start a daily auto-backup timer. Call once at app startup.
// Backups are saved to <AppData>/CustomMod/AutoBackups/ and the 3 most recent are kept.
void StartAutoBackup();

// Hot-reload support: register a callback invoked after a successful ImportFullBackup().
// The callback should refresh all open history views (call loadDeletedMessages on each peer).
using ReloadCallback = std::function<void()>;
void SetImportReloadCallback(ReloadCallback cb);

void ExportDatabase(const QString &targetPath);
void ImportDatabase(const QString &sourcePath);

// Reports export progress as a human-readable stage label + percent (0-100).
// May be called from a background thread (see ExportFullBackupAsync) — the
// async wrapper marshals it to the main thread before invoking it, but
// direct ExportFullBackup() callers are responsible for their own thread
// safety if they pass one.
using ExportProgressCallback = std::function<void(const QString &stage, int percent)>;

// ── Eksport formati v3 (2026-08-14) ─────────────────────────────────────
//
// Format boshqa ilovalarimizda va serverda ham ishlatilishi kerak,
// shuning uchun ikkita PLATFORMADAN MUSTAQIL artefakt qo'shildi:
//
//   settings.json  — barcha sozlamalar (kanonik). settings.reg Windows
//                    registry dump'i bo'lib, serverda o'qib bo'lmaydi;
//                    u faqat orqaga moslik uchun saqlanadi.
//   index.json     — media indeksi JSON'da. Server SQLite drayveri va
//                    bizning ichki sxemamizni bilishi shart bo'lmasin.
//
// Chiqish IKKITA alohida arxiv:
//   CustomModBackup_<stamp>.zip — baza + sozlamalar + indeks (MB'lar)
//   CustomModMedia_<stamp>.zip  — media fayllar (GB'lar), ixtiyoriy
//
// Nima uchun ikkita: ZIP yaratilgach unga qo'shib bo'lmaydi. Ajratish
// bilan asosiy arxiv soniyalarda tayyor bo'ladi va darhol ishlatsa
// bo'ladi, media esa fonda davom etadi. Track C uchun ham shu bo'linish
// kerak — indeks doim sinxronlanadi, bloblar alohida.
struct ExportOptions {
	// true  — barcha media eksport qilinadi (mediaPeerIds e'tiborsiz)
	// false — faqat mediaPeerIds dagi chatlar media'si
	// false + bo'sh ro'yxat — media umuman yo'q, faqat indeks
	bool includeAllMedia = false;
	QVector<QString> mediaPeerIds;
};

struct ExportResult {
	QString mainZipPath;   // bo'sh = muvaffaqiyatsiz
	QString mediaZipPath;  // bo'sh = media eksport qilinmadi
	long long mediaBytes = 0;
};

// Synchronous — does real disk I/O and shells out to PowerShell to zip the
// result, which can take seconds to tens of seconds for large archives.
// Prefer ExportFullBackupAsync() from UI code; this is kept for callers
// that already run off the main thread (e.g. the async wrapper itself).
ExportResult ExportFullBackup(
	const QString &targetDir,
	const ExportOptions &options = ExportOptions(),
	const ExportProgressCallback &onProgress = nullptr);

// Media indeksini JSON massiv sifatida qaytaradi (index.json mazmuni).
// Alohida ochiq, chunki server/boshqa ilovalar uni to'g'ridan-to'g'ri
// so'rashi mumkin.
[[nodiscard]] QJsonArray MediaIndexToJson();

// Full restore: imports DB + media from a previously exported backup.
// sourcePath may be a .zip file produced by ExportFullBackup(), or a plain folder.
// fullReplace=false (default): MERGE into the existing archive (see the
//   E28/Vazifa 2.3 comment at the top of ImportFullBackup()'s definition) —
//   nothing already on this device is deleted.
// fullReplace=true: clears ALL existing CustomMod archive data first (via
//   ClearAllArchive()), then imports — the backup becomes the sole source
//   of truth. Destructive; callers must confirm with the user first.
// Calls LoadRestoreCache() after restoring so in-memory caches are refreshed.
// Returns true on success.
// Synchronous, same caveat as ExportFullBackup() above — prefer
// ImportFullBackupAsync() from UI code.
bool ImportFullBackup(const QString &sourcePath, bool fullReplace = false);

// Async wrappers: run the synchronous export/import on a background thread
// (crl::async) and deliver the result back on the main thread (crl::on_main),
// so callers never block the UI thread. Safe to call from the UI thread.
// onProgress (export only) is marshaled to the main thread automatically.
using ExportResultCallback = std::function<void(const ExportResult &result)>;
void ExportFullBackupAsync(
	const QString &targetDir,
	const ExportOptions &options,
	ExportResultCallback callback,
	const ExportProgressCallback &onProgress = nullptr);

using ImportResultCallback = std::function<void(bool success)>;
void ImportFullBackupAsync(
	const QString &sourcePath,
	bool fullReplace,
	ImportResultCallback callback);

// Media arxivini alohida, asosiy import'dan KEYIN istalgan vaqtda
// tiklaydi. Fayllarni joyiga qo'yadi, so'ng ReconcileMediaIndex() orqali
// media_index yozuvlarini 'present' ga qaytaradi.
bool ImportMediaArchive(const QString &zipPath);
void ImportMediaArchiveAsync(
	const QString &zipPath,
	ImportResultCallback callback);

QString SaveMediaFile(const QString &sourcePath, const QString &type); // "image", "video", "voice", "file"

// ── Background AntiEdit cache (T27) ───────────────────────────────────────
// Tray/minimize holatda ham AntiEdit ishlashi uchun.
// Telegram MTProto update kelganda — agar peer da AntiEdit yoqilgan bo'lsa,
// xabar matnini cache ga yozamiz. Edit kelganda — cache dan eski matnni
// olib, oddiy applyEdition logikasini DB darajasida bajaramiz.

// Yangi xabar kelganda chaqiriladi (agar ShouldBackgroundCache(peer)).
// v5: senderId — guruhda haqiqiy yuboruvchi; isMedia — media xabar (matnsiz ham
// cache ga tushadi, shunda background delete da o'chirilganini bilib qolamiz).
// A13/D4: archived=true bo'lsa yozuv DOIMIY arxiv sifatida belgilanadi va
// PruneStaleCachedText() unga tegmaydi. Standart false — mavjud
// chaqiruvchilar xatti-harakati o'zgarmaydi.
void CacheMessageText(
    const PeerKey &key,
    long long msgId,
    const QString &text,
    bool isOut,
    unsigned int msgDate,
    const QString &senderId = QString(),
    bool isMedia = false,
    bool archived = false);

// Cache dan oldingi matnni qaytaradi (bo'lmasa, bo'sh string).
QString GetCachedText(const PeerKey &key, long long msgId);

// Cache dan text + msg_date + sender + media birga qaytaradi.
// outDate — saqlangan sana (0 bo'lsa topilmagan yoki saqlanmagan).
// outSenderId / outIsMedia — v5 ustunlari (ixtiyoriy, nullptr o'tkazsa o'qilmaydi).
QString GetCachedTextAndDate(
    const PeerKey &key,
    long long msgId,
    unsigned int &outDate,
    QString *outSenderId = nullptr,
    bool *outIsMedia = nullptr);

// Background edit qayd qilish — applyEdition o'rniga.
// HistoryItem mavjud bo'lmaganda chaqiriladi (chat ochilmagan).
// Cache dan eski matnni o'qib, actioned_messages ga 'edited' yozadi.
// Qaytarish: true — saqlandi, false — eski matn cache da yo'q.
bool RecordBackgroundEdit(
    const PeerKey &key,
    long long msgId,
    const QString &newText,
    bool isOut,
    unsigned int msgDate);

// Eski cache yozuvlarini tozalash. Avtomatik chaqiriladi (har CacheMessageText da).
// A13/D4: is_archived=1 bo'lgan (doimiy arxiv) qatorlarga TEGMAYDI.
void PruneStaleCachedText(int days = 30);

// A13/D5: WAL faylini asosiy DB ga ko'chiradi — to'satdan tok o'chganda
// yo'qotish oynasini qisqartirish uchun. CustomArchive davriy chaqiradi.
void Checkpoint();

// A13: tashqi modullar uchun oddiy SQL bajaruvchi (BEGIN/COMMIT kabi).
// Natija qaytarmaydigan buyruqlar uchun — gDb static bo'lgani sababli kerak.
void ExecRaw(const char *sql);

// A13/K6.3: Custom Window'da ko'rsatiladigan arxiv statistikasi.
[[nodiscard]] qint64 DatabaseSizeBytes();
[[nodiscard]] int ArchivedMessageCount();

// T28: Background AntiDelete — chat ochilmagan, HistoryItem yo'q.
// text_cache jadvalida msgId bo'yicha qidirib, agar topilsa AntiDelete
// yoqilgan peer bo'lsa — 'deleted' yozuvi yaratamiz.
// Faqat non-channel xabarlar uchun (channel/group da peerId allaqachon ma'lum).
void TryRecordBackgroundDelete(long long msgId);

// Sprint 4: Return the locally-saved media path for a deleted message (peerId+msgId),
// or an empty string if no media was saved. Used as a last-resort fallback in
// forward cascade before sending text-only.
QString GetSavedMediaPath(const PeerKey &key, long long msgId);

// Archive management
struct ArchiveStats {
    int deletedCount = 0;
    int editedCount = 0;
};
ArchiveStats GetArchiveStats();
void ClearDeletedArchive();
void ClearEditedArchive();
void ClearAllArchive();

// ── Activity History Log ──────────────────────────────────────────────────
// Kontaktlarning ism/username/rasm/last-seen o'zgarishlari — faqat ilova
// legal ravishda (joriy maxfiylik sozlamalari asosida) qabul qilgan
// ma'lumot. Yozuq faqat CustomSettings::ShouldTrackActivity() true
// bo'lganda amalga oshiriladi (chaqiruvchi tomonidan tekshiriladi).

struct ActivityHistoryEntry {
    QString field;         // "name" | "username" | "photo" | "status"
    bool hasOldValue = false; // false = bu shu peer/field uchun BIRINCHI yozuv
    QString oldValue;      // hasOldValue=false bo'lsa mazmunsiz (bo'sh)
    QString newValue;
    qint64 observedAt = 0; // unix timestamp (base::unixtime::now())
};

// Yangi yozuv qo'shadi. hasOldValue=false — bu peer/field juftligi uchun
// birinchi marta kuzatilayotganini bildiradi (old_value ustuniga SQL NULL
// yoziladi, "o'zgarish" emas "kuzatish boshlang'ich holati" sifatida).
// 2026-08-26: PeerKey oladi, lekin faqat account_id ni YOZISH uchun
// (provenance). O'qish tarafi (Get*) hamon faqat QString peerId oladi va
// akkaunt bo'yicha FILTRLAMAYDI -- spec 0.13. Faollik tarixi kuzatilayotgan
// odam haqidagi obyektiv fakt, kim kuzatganiga bog'liq emas.
void SaveActivityHistoryEntry(
    const PeerKey &key,
    const QString &field,
    bool hasOldValue,
    const QString &oldValue,
    const QString &newValue,
    qint64 observedAt,
    const QString &source = u"observed"_q);

// A16 §1: Berilgan peer, field va observed_at bo'yicha yozuv borligini tekshiradi (dublikat oldini olish).
// Akkaunt bo'yicha filtrlanmaydi (activity_history akkauntlar aro birlashgan).
[[nodiscard]] bool HasActivityEntryAt(
    const QString &peerId,
    const QString &field,
    qint64 observedAt);

// Shu peer/field uchun eng oxirgi yozilgan qiymatni qaytaradi.
// Qaytish qiymati: true — topildi (outValue to'ldirildi), false — hali
// hech qanday yozuv yo'q (birinchi kuzatish bo'ladi).
bool GetLatestActivityHistoryValue(
    const QString &peerId,
    const QString &field,
    QString &outValue);

// Shu peer uchun BARCHA maydonlar bo'yicha to'liq jurnal, eng yangisidan
// boshlab (observed_at DESC). History Viewer Box shu funksiyani ishlatadi.
// limit — nechta ENG SO'NGGI yozuv qaytarilsin. Cheklov SHART: eng faol
// kontaktda 13 333 qator bor edi va Faollik tarixi oynasi har qator uchun
// alohida widget yaratadi. Butun tarix kerak bo'lsa limit oshiriladi.
[[nodiscard]] QVector<ActivityHistoryEntry> GetActivityHistory(
    const QString &peerId,
    int limit = 300);

// Delete activity_history entries older than |days| days.
// SaveActivityHistoryEntry() soatiga bir marta chaqiradi; ishga tushishda
// ham bir marta chaqiriladi (main_session.cpp).
// 2026-08-24: muddat 365 -> 30 kun (foydalanuvchi qarori). Last-seen
// yozuvlari kuniga ~10 600 ta; 365 kunda jadval ~3.9 mln qatorga yetardi.
// 30 kunda ~320 ming qatorda barqarorlashadi.
void PruneStaleActivityHistory(int days = 30);

// 2026-08-24: faollik keshi FON oqimida yuklanadi — startup'ni bloklamaydi.
// Tayyor bo'lmaguncha RecordField() status yozuvini o'tkazib yuborishi
// SHART, aks holda har kontakt uchun soxta "kuzatish boshlandi" yozuvi
// paydo bo'ladi (kesh bo'sh ko'ringani uchun).
// Faollik tarixidan ma'nosiz qatorlarni o'chiradi (last-seen shovqini va
// 60 soniyadan qisqa online davrlar — qurilma ulanishlari). Nechta qator
// o'chirilgani qaytadi. ~6 soniya oladi — Async variantini ishlating.
int CompactActivityHistory();
void CompactActivityHistoryAsync();

[[nodiscard]] bool IsActivityCacheReady();
void WarmActivityCache();

} // namespace CustomDB
