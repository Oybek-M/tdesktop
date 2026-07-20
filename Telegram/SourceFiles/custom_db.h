#pragma once

#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <functional>

class HistoryItem;

namespace CustomDB {

struct ActionedMessage {
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
// v5: actioned_messages + text_cache ga sender_id va is_media ustunlari qo'shildi.
constexpr int kCurrentSchemaVersion = 5;

void Init();

// E19: Run any pending schema migrations up to kCurrentSchemaVersion.
// Called automatically by Init(); safe to call multiple times.
void RunMigrations();

// Startup: load all deleted/edited records into fast in-memory caches
void LoadRestoreCache();

// Fast O(1) lookups used in HistoryItem constructor
bool IsDeletedLocally(const QString &peerId, long long msgId);
QString GetOriginalTextBeforeEdit(const QString &peerId, long long msgId);

// Returns all saved pre-edit text versions for a message, oldest first.
// Each entry is the text *before* that edit was applied.
// Empty if the message was never edited (or not tracked).
QVector<QString> GetEditHistory(const QString &peerId, long long msgId);

void SaveGhostRead(const QString &peerId, long long msgId);
long long GetGhostRead(const QString &peerId);

// E22: Remove a peer's ghost-read record (call when ghost mode is disabled for that peer).
void ResetGhostRead(const QString &peerId);

// E22: Delete ghost_reads entries older than |days| days (default 30).
// Called automatically by SaveGhostRead(); safe to call manually.
void PruneStaleGhostReads(int days = 30);
void MarkDeleted(
    long long msgId,
    const QString &peerId,
    const QString &mediaPath = QString(),
    const QString &originalText = QString(),
    unsigned int msgDate = 0,
    bool isOut = false,
    const QString &senderId = QString(),  // v5: guruhda haqiqiy yuboruvchi
    bool isMedia = false);                // v5: media xabar belgisi

// User-initiated delete support:
// Call before sending delete request to server so the message is truly removed.
// Removes all DB records for the message and prevents re-saving on server ACK.
void ScheduleUserDelete(const QString &peerId, long long msgId);

// Returns true if the user themselves initiated deletion of this message.
bool IsUserDeletePending(const QString &peerId, long long msgId);

// Clear the pending flag after processMessagesDeleted has handled the message.
void ClearUserDeletePending(const QString &peerId, long long msgId);

// Permanently erase all records for a message from DB + in-memory caches.
void PermanentlyDeleteMessage(const QString &peerId, long long msgId);
void SaveActionedMessage(const ActionedMessage &msg);
QString GetMessageHistory(long long msgId, const QString &peerId);

struct DeletedMessage {
    long long msgId;
    QString mediaPath;
    bool isOut;
    unsigned int date;
    QString text;
    QString senderId;       // v5: guruhda haqiqiy yuboruvchi (bo'sh = noma'lum)
    bool isMedia = false;   // v5: media xabar edi
};
QVector<DeletedMessage> GetDeletedMessages(const QString &peerId);

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

// Full backup: copies DB + entire media folder into targetDir.
// Returns the path of the exported directory on success, empty string on failure.
// Synchronous — does real disk I/O and shells out to PowerShell to zip the
// result, which can take seconds to tens of seconds for large archives.
// Prefer ExportFullBackupAsync() from UI code; this is kept for callers
// that already run off the main thread (e.g. the async wrapper itself).
QString ExportFullBackup(
	const QString &targetDir,
	const ExportProgressCallback &onProgress = nullptr);

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
using ExportResultCallback = std::function<void(const QString &resultPathOrEmpty)>;
void ExportFullBackupAsync(
	const QString &targetDir,
	ExportResultCallback callback,
	const ExportProgressCallback &onProgress = nullptr);

using ImportResultCallback = std::function<void(bool success)>;
void ImportFullBackupAsync(
	const QString &sourcePath,
	bool fullReplace,
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
void CacheMessageText(
    const QString &peerId,
    long long msgId,
    const QString &text,
    bool isOut,
    unsigned int msgDate,
    const QString &senderId = QString(),
    bool isMedia = false);

// Cache dan oldingi matnni qaytaradi (bo'lmasa, bo'sh string).
QString GetCachedText(const QString &peerId, long long msgId);

// Cache dan text + msg_date + sender + media birga qaytaradi.
// outDate — saqlangan sana (0 bo'lsa topilmagan yoki saqlanmagan).
// outSenderId / outIsMedia — v5 ustunlari (ixtiyoriy, nullptr o'tkazsa o'qilmaydi).
QString GetCachedTextAndDate(
    const QString &peerId,
    long long msgId,
    unsigned int &outDate,
    QString *outSenderId = nullptr,
    bool *outIsMedia = nullptr);

// Background edit qayd qilish — applyEdition o'rniga.
// HistoryItem mavjud bo'lmaganda chaqiriladi (chat ochilmagan).
// Cache dan eski matnni o'qib, actioned_messages ga 'edited' yozadi.
// Qaytarish: true — saqlandi, false — eski matn cache da yo'q.
bool RecordBackgroundEdit(
    const QString &peerId,
    long long msgId,
    const QString &newText,
    bool isOut,
    unsigned int msgDate);

// Eski cache yozuvlarini tozalash. Avtomatik chaqiriladi (har CacheMessageText da).
void PruneStaleCachedText(int days = 30);

// T28: Background AntiDelete — chat ochilmagan, HistoryItem yo'q.
// text_cache jadvalida msgId bo'yicha qidirib, agar topilsa AntiDelete
// yoqilgan peer bo'lsa — 'deleted' yozuvi yaratamiz.
// Faqat non-channel xabarlar uchun (channel/group da peerId allaqachon ma'lum).
void TryRecordBackgroundDelete(long long msgId);

// Sprint 4: Return the locally-saved media path for a deleted message (peerId+msgId),
// or an empty string if no media was saved. Used as a last-resort fallback in
// forward cascade before sending text-only.
QString GetSavedMediaPath(const QString &peerId, long long msgId);

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
void SaveActivityHistoryEntry(
    const QString &peerId,
    const QString &field,
    bool hasOldValue,
    const QString &oldValue,
    const QString &newValue,
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
[[nodiscard]] QVector<ActivityHistoryEntry> GetActivityHistory(
    const QString &peerId);

} // namespace CustomDB
