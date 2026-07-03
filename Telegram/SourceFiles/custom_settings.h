#pragma once

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
};

struct Values {
    bool ghostMode = true;
    bool bypassRestrictions = true;
    bool offlineDb = true;
    bool antiDelete = true;
    bool antiEdit = true;
    bool spoofMobile = true;
    bool storyAnonymousView = true;
    // C22: Dynamic device spoof
    int  spoofDeviceType = 0; // 0=Android, 1=iOS, 2=Windows, 3=Linux
    QString spoofDeviceModel = u"Samsung Galaxy S26 Ultra"_q;
    QString spoofSystemVersion = u"Android 15"_q;
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

[[nodiscard]] bool ShouldAntiDelete(const QString &peerId);
[[nodiscard]] bool ShouldAntiEdit(const QString &peerId);
[[nodiscard]] bool ShouldGhost(const QString &peerId);

// T32+: Background cache (text_cache) ga qaysi peerlar tushishini hal qiladi.
// Performance uchun ATAYLAB cheklangan: faqat WhiteList YOKI Per-Chat override
// orqali AntiDelete/AntiEdit yoqilgan peerlar. Global flag ON bo'lsa ham
// barcha chatlarni cache qilmaymiz (disk/CPU isrofini oldini olish).
[[nodiscard]] bool ShouldBackgroundCache(const QString &peerId);

// Story anonim ko'rish — faqat story view/markRead ni bloklaydi.
// GhostMode dan alohida: xabar o'qildi belgilari bilan aloqasi yo'q.
[[nodiscard]] bool ShouldAnonymousStory(const QString &peerId);

// Peer uchun inson o'qiy oladigan nom qaytaradi.
// Whitelist yoki Blocklist da saqlanganini tekshiradi.
// Topilmasa — bo'sh QString qaytadi (caller o'zi peerId ni ishlata oladi).
[[nodiscard]] QString GetPeerDisplayName(const QString &peerId);

} // namespace CustomSettings
