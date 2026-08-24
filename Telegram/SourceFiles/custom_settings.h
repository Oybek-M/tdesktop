#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QVector>

namespace CustomSettings {

// Per-Chat Settings paneli uchun entry struct.
struct PerPeerEntry {
    QString peerId;
    QString displayName;
    bool ghostEnabled;
    bool antiDeleteEnabled;
    bool antiEditEnabled;
    bool mediaBackupEnabled;  // 2026-08-14
};

struct Values {
    bool ghostMode = true;
    bool bypassRestrictions = true;
    bool offlineDb = true;
    bool antiDelete = true;
    bool antiEdit = true;
    bool spoofMobile = true;
    bool storyAnonymousView = true;
    // C22: Dynamic device spoof.
    // NOTE: spoofDeviceType itself is NOT sent to Telegram's MTProto layer —
    // the server infers the device icon from the spoofDeviceModel/
    // spoofSystemVersion strings, not a separate type code. This field only
    // drives the preset buttons in custom_mod_window.cpp (which pre-fill
    // those two strings); it has no other effect.
    int  spoofDeviceType = 0; // 0=Android, 1=iOS, 2=Windows, 3=Linux
    QString spoofDeviceModel = u"Samsung Galaxy S26 Ultra"_q;
    QString spoofSystemVersion = u"Android 15"_q;
    // Mutual-Contact Indikatori: mutual_contact bo'lgan peerlar ismi yoniga
    // emoji qo'shish, 4 mustaqil joy uchun mustaqil toggle+emoji.
    bool mutualContactShowInChatList = true;
    QString mutualContactChatListEmoji = u"🤝"_q;
    bool mutualContactShowInContactsList = true;
    QString mutualContactContactsListEmoji = u"🤝"_q;
    bool mutualContactShowInProfile = true;
    QString mutualContactProfileEmoji = u"🤝"_q;
    bool mutualContactShowInMembersList = true;
    QString mutualContactMembersListEmoji = u"🤝"_q;
    // Activity History Log: ism/username/rasm/last-seen o'zgarishlarini
    // kuzatish. Include/Exclude ro'yxatlari alohida QHash'larda saqlanadi
    // (pastga qarang) — bu shunchaki global default toggle.
    bool activityHistoryTrackAllContacts = true;
    // ── Upstream (rasmiy) versiya tekshiruvchisi ─────────────────────────
    bool upstreamCheckEnabled = true;
    int upstreamCheckIntervalMinutes = 1440;  // standart: kunlik

    // Katta media backup (2026-08-14). Ikkalasi ham Custom Window'da
    // o'zgartiriladi; chegaralar UpdateInt() da qisiladi.
    //
    // Yuqori chegara 4096 MB — Telegram Premium 4 GB gacha fayl
    // yuborishga ruxsat beradi, ya'ni undan kattasi bo'lishi mumkin emas.
    int mediaBackupMaxFileMb = 100;     // 10 – 4096
    // Kvota MB'da saqlanadi (GB'da emas) — foydalanuvchi 5.5 GB kabi
    // kasrli qiymat kirita olishi uchun. UI GB'da ko'rsatadi.
    int mediaBackupQuotaMb = 10240;    // 1024 – 512000 (1 GB – 500 GB)
    QString upstreamLastKnownVersion;         // oxirgi FOYDALANUVCHIGA bildirilgan versiya
    QString upstreamEtag;                     // GitHub ETag — shartli so'rov uchun (304 limitdan yeyilmaydi)
    qint64 upstreamLastCheckedAt = 0;         // unix timestamp (soniya)
    // ── Story media zaxirasi (A11) ───────────────────────────────────────
    // Story vaqt-kuzatuvidan MUSTAQIL, alohida tugma — disk-sarflovchi
    // funksiya, shuning uchun standart holatda O'CHIRILGAN (opt-in).
    bool storyMediaBackupEnabled = false;
};

void Init();
const Values& Get();
void Set(const QString &id, bool value);
void SetString(const QString &id, const QString &value);
void SetInt(const QString &id, int value);

// Global helpers
inline bool GhostMode() { return Get().ghostMode; }
inline bool BypassRestrictions() { return Get().bypassRestrictions; }
inline bool OfflineDb() { return Get().offlineDb; }
inline bool AntiDelete() { return Get().antiDelete; }
inline bool AntiEdit() { return Get().antiEdit; }
inline bool SpoofMobile() { return Get().spoofMobile; }
inline bool StoryAnonymousView() { return Get().storyAnonymousView; }
inline int     SpoofDeviceType()    { return Get().spoofDeviceType; }
inline QString SpoofDeviceModel()   { return Get().spoofDeviceModel; }
inline QString SpoofSystemVersion() { return Get().spoofSystemVersion; }
[[nodiscard]] QString SpoofLangPack();

inline bool    MutualContactShowInChatList()     { return Get().mutualContactShowInChatList; }
inline QString MutualContactChatListEmoji()      { return Get().mutualContactChatListEmoji; }
inline bool    MutualContactShowInContactsList() { return Get().mutualContactShowInContactsList; }
inline QString MutualContactContactsListEmoji()  { return Get().mutualContactContactsListEmoji; }
inline bool    MutualContactShowInProfile()      { return Get().mutualContactShowInProfile; }
inline QString MutualContactProfileEmoji()       { return Get().mutualContactProfileEmoji; }
inline bool    MutualContactShowInMembersList()  { return Get().mutualContactShowInMembersList; }
inline QString MutualContactMembersListEmoji()   { return Get().mutualContactMembersListEmoji; }

inline bool ActivityHistoryTrackAllContacts() { return Get().activityHistoryTrackAllContacts; }

inline bool    UpstreamCheckEnabled()          { return Get().upstreamCheckEnabled; }
inline int     UpstreamCheckIntervalMinutes()  { return Get().upstreamCheckIntervalMinutes; }
inline int     MediaBackupMaxFileMb()          { return Get().mediaBackupMaxFileMb; }
inline int     MediaBackupQuotaMb()            { return Get().mediaBackupQuotaMb; }
inline QString UpstreamLastKnownVersion()      { return Get().upstreamLastKnownVersion; }
inline QString UpstreamEtag()                  { return Get().upstreamEtag; }
inline qint64  UpstreamLastCheckedAt()         { return Get().upstreamLastCheckedAt; }
inline bool    StoryMediaBackupEnabled()       { return Get().storyMediaBackupEnabled; }
// qint64 generic Set/SetInt(int) orqali saqlanolmaydi (32-bit chegara) —
// shuning uchun alohida, per-peer setter'lar kabi maxsus funksiya.
void SetUpstreamLastCheckedAt(qint64 timestamp);

// Per-chat Ghost Mode override (legacy C12).
[[nodiscard]] bool GhostModeForPeer(const QString &peerId);
void SetGhostModeForPeer(const QString &peerId, bool enabled);
void ResetGhostModeForPeer(const QString &peerId);

// Per-chat Anti-Delete override (legacy C14).
[[nodiscard]] bool AntiDeleteForPeer(const QString &peerId);
void SetAntiDeleteForPeer(const QString &peerId, bool enabled);
void ResetAntiDeleteForPeer(const QString &peerId);

// Per-chat Anti-Edit override (NEXT-6).
[[nodiscard]] bool AntiEditForPeer(const QString &peerId);
void SetAntiEditForPeer(const QString &peerId, bool enabled);
void ResetAntiEditForPeer(const QString &peerId);

// ── Per-Chat Settings boshqaruvi (UI panel uchun) ────────────────────────
// AddPerPeerOverride: peerni ro'yxatga qo'shadi va joriy global holatini
// per-peer override sifatida yozadi (keyin alohida o'zgartiriladi).
void AddPerPeerOverride(const QString &peerId, const QString &displayName);
void RemovePerPeerOverride(const QString &peerId);
void ClearAllPerPeerOverrides();
[[nodiscard]] bool HasPerPeerOverride(const QString &peerId);
[[nodiscard]] QVector<PerPeerEntry> GetPerPeerOverrides();

// ── Arxiv ildizi — YAGONA manba (2026-08-15) ─────────────────────────────
//
// Ilgari yo'llar 14 xil joyda alohida hisoblanardi va ikkita ildizga
// bo'linib ketgan edi (~/customizationMainFolder va %APPDATA%/CustomMod).
// Endi AntiDelete va zaxira uchun saqlanadigan HAMMA narsa bitta ildiz
// ostida:
//
//   <ildiz>/medias/{images,videos,voices,files}
//   <ildiz>/db/actioned_messages.db
//   <ildiz>/config/{peer_lists.json, branding.json}
//   <ildiz>/backups/
//
// Standart ildiz — ~/customizationMainFolder. Foydalanuvchi uni Custom
// Window orqali o'zgartirishi mumkin.
//
// DIQQAT: ArchiveRoot() Init() ni TALAB QILMAYDI — u faqat QSettings'ga
// qaraydi. Aks holda CustomDB::Init() -> ArchiveRoot() ->
// CustomSettings::Init() zanjirida rekursiya xavfi tug'ilardi.
[[nodiscard]] QString ArchiveRoot();
[[nodiscard]] QString DefaultArchiveRoot();
[[nodiscard]] QString ArchiveMediasDir();   // <ildiz>/medias
[[nodiscard]] QString ArchiveDbDir();       // <ildiz>/db
[[nodiscard]] QString ArchiveConfigDir();   // <ildiz>/config
[[nodiscard]] QString ArchiveBackupsDir();  // <ildiz>/backups

// Ildizni o'zgartiradi (fayllarni KO'CHIRMAYDI — buni chaqiruvchi
// MoveArchiveRoot() orqali qiladi). Bo'sh qiymat standartga qaytaradi.
void SetArchiveRoot(const QString &path);

// Ildiz o'zgartirilganda mavjud ma'lumotlarni ko'chirishni belgilaydi.
// Ko'chirishning O'ZI keyingi ishga tushishda, baza OCHILMASDAN OLDIN
// bajariladi (EnsureArchiveLayout) — ilova ishlayotganda SQLite fayli
// ochiq bo'lgani uchun ko'chirish xavfli.
void ScheduleArchiveRootMove(const QString &fromPath);

// Kerakli sub-papkalarni yaratadi va eski joylashuvdan (AppData'dagi
// baza/sozlama/zaxiralar) bir marta ko'chiradi. Baza OCHILMASDAN OLDIN
// chaqirilishi shart. Takroriy chaqiruv zararsiz.
void EnsureArchiveLayout();

// ── Platformadan mustaqil sozlama almashinuvi (2026-08-14) ───────────────
// Eksport/import formati boshqa ilovalarimizda va serverda ham
// ishlatilishi kerak. Windows registry dump (settings.reg) u yerda
// o'qilmaydi, shuning uchun JSON kanonik shakl bo'ladi.
//
// Maydonlar ro'yxati QO'LDA yuritilmaydi — butun QSettings daraxti
// (allKeys) dump qilinadi. Aks holda har yangi sozlama qo'shilganda bu
// yerni yangilash esdan chiqib, eksport jimgina to'liqsiz bo'lib qolardi.
[[nodiscard]] QJsonObject ExportToJson();

// JSON dan tiklaydi va Init() ni qayta ishga tushiradi.
// Noma'lum kalitlar saqlanadi (kelajakdagi versiya bilan moslik uchun).
void ImportFromJson(const QJsonObject &json);

// ── Unified Peer Whitelist ────────────────────────────────────────────────
// Peers in this list: Ghost + AntiDelete + AntiEdit are ALWAYS ON,
// regardless of global flags. (Lower priority than BlockList.)

void AddToWhitelist(const QString &peerId, const QString &displayName);
void RemoveFromWhitelist(const QString &peerId);
[[nodiscard]] QVector<QPair<QString,QString>> GetWhitelist();
[[nodiscard]] bool IsInWhitelist(const QString &peerId);

// ── Unified Peer Blocklist ────────────────────────────────────────────────
// Peers in this list: Ghost + AntiDelete + AntiEdit are ALWAYS OFF,
// regardless of global flags. HIGHEST PRIORITY — overrides whitelist.

void AddToBlocklist(const QString &peerId, const QString &displayName);
void RemoveFromBlocklist(const QString &peerId);
void ClearBlocklist();
[[nodiscard]] QVector<QPair<QString,QString>> GetBlocklist();
[[nodiscard]] bool IsInBlocklist(const QString &peerId);

void ClearWhitelist();

// ── Peer type detection ───────────────────────────────────────────────────
// peerId — QString::number(peer->id.value) formatidagi raqamli ID.
// Telegram PeerId encoding: (value>>48)&0xFF → 0=User, 1=Group, 2=Channel
enum class PeerType { User = 0, Group = 1, Channel = 2, Unknown = 3 };
[[nodiscard]] PeerType GetPeerType(const QString &peerId);

// ── Kategoriya (Category) bo'yicha whitelist/blocklist ───────────────────
// PeerType ga mos kategoriya yoqilgan bo'lsa, IsInWhitelist/IsInBlocklist
// shu turdagi barcha peerlar uchun true qaytaradi.
// Saqlash: peer_lists.json — "wl_categories" / "bl_categories".
[[nodiscard]] bool IsWhitelistCategoryEnabled(PeerType type);
void SetWhitelistCategory(PeerType type, bool enabled);
[[nodiscard]] bool IsBlocklistCategoryEnabled(PeerType type);
void SetBlocklistCategory(PeerType type, bool enabled);

// ── Unified "should track" helpers ───────────────────────────────────────
// Priority order (highest → lowest):
//   1. BlockList      → always returns false  (never track)
//   2. WhiteList      → always returns true   (always track)
//   3. Per-peer override (SetGhostModeForPeer, SetAntiDeleteForPeer, ...)
//   4. Global flag (e.g. AntiDelete())    (user-configured default)
//
// Use these everywhere instead of the bare global flags.

// ── Media backup (2026-08-14) ────────────────────────────────────────────
// Per-chat override. DIQQAT: AntiDeleteForPeer() dan farqli o'laroq,
// override yo'q bo'lsa global bayroqqa QAYTMAYDI — false qaytaradi.
[[nodiscard]] bool MediaBackupForPeer(const QString &peerId);
void SetMediaBackupForPeer(const QString &peerId, bool enabled);
void ResetMediaBackupForPeer(const QString &peerId);

// Katta media oldindan yuklab olinadimi. Zanjir ShouldAntiDelete() dan
// FARQ QILADI — global bayroq hisobga OLINMAYDI (izoh .cpp da).
//   Black List (veto) > White List > per-chat override
// Saved Messages istisnosi chaqiruv joyida (peer->isSelf()).
[[nodiscard]] bool ShouldMediaBackup(const QString &peerId);

[[nodiscard]] bool ShouldAntiDelete(const QString &peerId);
[[nodiscard]] bool ShouldAntiEdit(const QString &peerId);
[[nodiscard]] bool ShouldGhost(const QString &peerId);

// T32+: Background cache (text_cache) ga qaysi peerlar tushishini hal qiladi.
// A13/D3 (2026-08-13) — XATTI-HARAKAT O'ZGARDI. Ilgari bu funksiya global
// antiDelete/antiEdit bayrog'ini ATAYLAB e'tiborsiz qoldirardi (disk/CPU
// tejash uchun: faqat WhiteList yoki Per-Chat override'li peerlar cache'ga
// tushardi). Amalda bu tuzoq bo'lib chiqdi: ShouldAntiDelete() true qaytarib,
// foydalanuvchiga "AntiDelete yoqilgan" deb ko'rinardi, lekin fon-cache
// ishlamagani uchun suhbatdosh butun chatni o'chirsa ma'lumot yo'qolardi.
// Endi zanjir ShouldAntiDelete/ShouldAntiEdit bilan bir xil:
//   Blocklist > Whitelist > Per-Chat override > global bayroq.
// Ya'ni global bayroq YOQILGAN bo'lsa barcha chatlar cache'ga tushadi —
// disk sarfi ortadi, lekin "yoqilgan" degani haqiqatan ishlaydi degani.
// Disk'ni cheklash kerak bo'lsa — global bayroqni o'chirib, Whitelist yoki
// Per-Chat override ishlating.
[[nodiscard]] bool ShouldBackgroundCache(const QString &peerId);

// A13/K5.3: ShouldBackgroundCache bilan bir xil zanjir, lekin natija o'rniga
// SABABNI qaytaradi ("Oq ro'yxat — kuzatiladi", "Umumiy sozlama —
// kuzatilmaydi", ...). Custom Window'da ko'rsatiladi.
[[nodiscard]] QString TrackingReason(const QString &peerId);

// Story anonim ko'rish — faqat story view/markRead ni bloklaydi.
// GhostMode dan alohida: xabar o'qildi belgilari bilan aloqasi yo'q.
[[nodiscard]] bool ShouldAnonymousStory(const QString &peerId);

// Peer uchun inson o'qiy oladigan nom qaytaradi.
// Whitelist yoki Blocklist da saqlanganini tekshiradi.
// Topilmasa — bo'sh QString qaytadi (caller o'zi peerId ni ishlata oladi).
[[nodiscard]] QString GetPeerDisplayName(const QString &peerId);

// 2026-08-24: yengil nom keshi. GetPeerDisplayName() ilgari faqat
// White/Black List'ga qarardi, shuning uchun ro'yxatlarda bo'lmagan
// oddiy chatlar eksport ro'yxatida "ID 620565940" bo'lib chiqardi.
// Sessiyadan olish ham har doim ishlamaydi: peerLoaded() faqat shu
// seansda yuklangan peer'ni topadi.
//
// Yechim — nomni ARXIVLASH PAYTIDA eslab qolamiz (o'shanda peer aniq
// ma'lum) va keyin doim shu keshdan o'qiymiz.
// AddPerPeerOverride() dan farqi: bu FAQAT nom yozadi, hech qanday
// per-peer sozlama yaratmaydi.
void RememberPeerName(const QString &peerId, const QString &name);

// ── Activity History Log: Include/Exclude ro'yxatlari ───────────────────
// Whitelist/Blocklist bilan bir xil funksiya to'plami va mutual-exclusion
// xatti-harakati, lekin BUTUNLAY MUSTAQIL QHash'larda saqlanadi — bu
// ro'yxatlar Ghost/AntiDelete/AntiEdit uchun ishlatiladigan Whitelist/
// Blocklist bilan aralashtirilmaydi (butunlay boshqa maqsad).

void AddToActivityInclude(const QString &peerId, const QString &displayName);
void RemoveFromActivityInclude(const QString &peerId);
[[nodiscard]] QVector<QPair<QString,QString>> GetActivityInclude();
[[nodiscard]] bool IsInActivityInclude(const QString &peerId);

void AddToActivityExclude(const QString &peerId, const QString &displayName);
void RemoveFromActivityExclude(const QString &peerId);
[[nodiscard]] QVector<QPair<QString,QString>> GetActivityExclude();
[[nodiscard]] bool IsInActivityExclude(const QString &peerId);

// Ustuvorlik: Exclude List (false) > Include List (true, standart holatdan
// qat'iy nazar) > (activityHistoryTrackAllContacts && isContact) > false.
// isContact — chaqiruvchi tomonidan beriladi (bu fayl Data::PeerData ga
// bog'liq emas, boshqa CustomSettings funksiyalari kabi faqat QString bilan
// ishlaydi).
[[nodiscard]] bool ShouldTrackActivity(const QString &peerId, bool isContact);

} // namespace CustomSettings
