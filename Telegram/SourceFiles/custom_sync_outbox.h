#pragma once

#include <QtCore/QString>
#include <QtCore/QVector>
#include <optional>

namespace CustomSync {

struct OutboxEntry {
    QString recordId;
    QString kind;
    qint64 accountId = 0;
    QString peerId;       // lokal ochiq peer id
    qint64 msgId = 0;
    qint64 occurredAt = 0;
    qint64 observedAt = 0;
    QString targetRecordId;
    int attempts = 0;
    QString lastError;
    qint64 nextRetryAt = 0;
};

namespace Outbox {

// Kalitlar mavjudligini tekshiradi (Task 5 keystore to'ldiradi).
// Hozircha doim false qaytaradi — soxta record_id yozilishining oldini oladi (K5).
[[nodiscard]] bool KeysAvailable();

// Merge davomida enqueue'ni o'chiradi. THREAD_LOCAL bo'lishi SHART:
// merge sync oqimida ishlaydi, foydalanuvchi esa ayni paytda UI
// oqimida xabar o'chirishi mumkin -- global bayroq o'sha haqiqiy
// hodisani ham yutib yuborardi.
class MergeGuard {
public:
    MergeGuard();
    ~MergeGuard();
    MergeGuard(const MergeGuard &) = delete;
    MergeGuard &operator=(const MergeGuard &) = delete;
};
[[nodiscard]] bool MergeInProgress();

// Navbatga qo'shadi. KeysAvailable() false bo'lsa hech narsa qilmaydi (bazaga tegmaydi).
// Bu funksiya custom_db.cpp dagi capture funksiyalarining OXIRIDA chaqiriladi.
void Enqueue(
    const QString &kind,
    qint64 accountId,
    const QString &peerId,
    qint64 msgId,
    qint64 occurredAt,
    const QString &targetRecordId = {});

// sync_record_map yozuvlar strukturasi (Task 7c)
struct RecordMapEntry {
    QString recordId;
    QString kind;
    qint64 accountId = 0;
    QString peerId;
    qint64 msgId = 0;
    qint64 occurredAt = 0;
};

// sync_record_map jadvaliga yozish (Enqueue va MergeRecord dan chaqiriladi)
void SaveRecordMap(
    const QString &recordId,
    const QString &kind,
    qint64 accountId,
    const QString &peerId,
    qint64 msgId,
    qint64 occurredAt);

// Tombstone uchun target_record_id bo'yicha qidiruv
[[nodiscard]] std::optional<RecordMapEntry> LookupRecordMap(const QString &recordId);

// Producer uchun: (kind, accountId, peerId, occurredAt, msgId) bo'yicha record_id qidiruv
[[nodiscard]] QString FindRecordId(
    const QString &kind,
    qint64 accountId,
    const QString &peerId,
    qint64 occurredAt,
    qint64 msgId = 0);

// Jo'natishga tayyor yozuvlar (next_retry_at <= hozir), eng eskisidan (occurred_at ASC).
[[nodiscard]] QVector<OutboxEntry> Pending(int limit);

void MarkSent(const QString &recordId);

// Manba qator lokal bazada topilmaganda (SourceGone) yozuvni navbatdan o'chirish.
void Drop(const QString &recordId, const QString &reason);

// Eksponensial backoff: 1s, 2s, 4s… maksimum 300s (5 daqiqa).
// next_retry_at diskda saqlanadi — ilova qayta ishga tushsa backoff nolga qaytmaydi.
void MarkFailed(const QString &recordId, const QString &error);

[[nodiscard]] int PendingCount();

// 3-bosqich (2026-09-13): navbat BO'SHLIGINI SQL'siz bilish.
// Xotiradagi hisoblagich HAR DOIM haqiqiy qatorlar sonidan KATTA YOKI
// TENG saqlanadi (Enqueue oshiradi, o'chirishlar kamaytirmaydi), ya'ni
// "0" javobi ishonchli: navbat aniq bo'sh. Eskirgan katta qiymat
// ResyncRowCount() bilan aniq songa qaytariladi.
[[nodiscard]] bool ProbablyEmpty();
void ResyncRowCount();

// sync_state kalit-qiymat qatlami.
[[nodiscard]] QString GetState(const QString &key, const QString &fallback = {});
void SetState(const QString &key, const QString &value);

// Master kalit boshqaruvi:
// Serverdan ochilgan master kalitni O'RNATADI. Lokal kalit allaqachon
// bo'lsa -- FALSE qaytaradi va hech narsani o'zgartirmaydi.
[[nodiscard]] bool AdoptMasterKey(const QByteArray &masterKey);
// Faqat birinchi marta (enroll paytida) yaratiladi, mavjud bo'lsa hech qachon almashtirilmaydi.
// DIQQAT: 2026-09-07 dan beri CHAQIRUVCHISI YO'Q, va bu ataylab.
//
// Ilgari `Client::enroll` uni chaqirardi. Muammo: u kalitni DARHOL
// saqlaydi, POST esa keyin yiqilishi mumkin (admin roli bo'lmasa 403).
// O'shanda qurilma serverga hech qachon yuklanmagan kalit bilan qolardi,
// `AdoptMasterKey` esa uni almashtirishdan bosh tortadi -- ya'ni o'sha
// qurilma umumiy arxivga boshqa qo'shila olmasdi.
//
// To'g'ri tartib (custom_tab_sync.cpp): RandomBytes(32) -> WrapMasterKey
// -> createKeyWrap(POST) -> AdoptMasterKey. Buni enroll'ga qaytarmang.
[[nodiscard]] bool EnsureMasterKeyCreated();
[[nodiscard]] bool LoadMasterKey();
[[nodiscard]] QByteArray MasterKey();
[[nodiscard]] QString KeyFingerprint();
[[nodiscard]] QByteArray ContentKey();
[[nodiscard]] QByteArray PeerKey();
[[nodiscard]] QByteArray AccountKey();
[[nodiscard]] QByteArray MediaKey();

} // namespace Outbox
} // namespace CustomSync
