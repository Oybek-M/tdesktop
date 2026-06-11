#include "custom_settings.h"
#include "custom_db.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>

namespace CustomSettings {
namespace {

Values gValues;
bool gInitialized = false;

// Legacy per-peer overrides (C12 / C14) + NEXT-6 anti-edit.
QHash<QString, bool> gGhostPerPeer;
QHash<QString, bool> gAntiDeletePerPeer;
QHash<QString, bool> gAntiEditPerPeer;

// Per-Chat Settings master list: peerId → displayName.
// Faqat Per-Chat Settings UI orqali qo'shilgan peerlar shu ro'yxatda.
QHash<QString, QString> gPerPeerNames;

// Unified whitelist: always ON for these peers (lower priority than blocklist).
QHash<QString, QString> gWhitelist;

// Unified blocklist: always OFF for these peers (highest priority).
QHash<QString, QString> gBlocklist;

// ── JSON peer lists ────────────────────────────────────────────────────────
// Fayl manzili: <AppData>/CustomMod/peer_lists.json
// Format:
// {
//   "whitelist": [ {"id": "123", "name": "Ism"}, ... ],
//   "blacklist": [ {"id": "456", "name": "Ism"}, ... ]
// }

[[nodiscard]] QString PeerListsFilePath() {
    const auto dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)
        + QStringLiteral("/CustomMod/");
    QDir().mkpath(dir);
    return dir + QStringLiteral("peer_lists.json");
}

void SavePeerLists() {
    QJsonArray wl, bl;
    for (auto it = gWhitelist.constBegin(); it != gWhitelist.constEnd(); ++it) {
        QJsonObject o;
        o[QStringLiteral("id")]   = it.key();
        o[QStringLiteral("name")] = it.value();
        wl.append(o);
    }
    for (auto it = gBlocklist.constBegin(); it != gBlocklist.constEnd(); ++it) {
        QJsonObject o;
        o[QStringLiteral("id")]   = it.key();
        o[QStringLiteral("name")] = it.value();
        bl.append(o);
    }
    QJsonObject root;
    root[QStringLiteral("whitelist")] = wl;
    root[QStringLiteral("blacklist")] = bl;

    QFile f(PeerListsFilePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

void LoadPeerLists() {
    QFile f(PeerListsFilePath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    const auto root = doc.object();

    gWhitelist.clear();
    for (const auto &v : root[QStringLiteral("whitelist")].toArray()) {
        const auto o  = v.toObject();
        const auto id = o[QStringLiteral("id")].toString();
        if (!id.isEmpty()) gWhitelist[id] = o[QStringLiteral("name")].toString();
    }

    gBlocklist.clear();
    for (const auto &v : root[QStringLiteral("blacklist")].toArray()) {
        const auto o  = v.toObject();
        const auto id = o[QStringLiteral("id")].toString();
        if (!id.isEmpty()) gBlocklist[id] = o[QStringLiteral("name")].toString();
    }
}

void UpdateValue(const QString &id, bool value) {
    if (id == "ghostMode") gValues.ghostMode = value;
    else if (id == "bypassRestrictions") gValues.bypassRestrictions = value;
    else if (id == "offlineDb") gValues.offlineDb = value;
    else if (id == "antiDelete") gValues.antiDelete = value;
    else if (id == "antiEdit") gValues.antiEdit = value;
    else if (id == "spoofMobile") gValues.spoofMobile = value;
    else if (id == "storyAnonymousView") gValues.storyAnonymousView = value;
}

} // namespace

void Init() {
    if (gInitialized) return;

    QSettings settings("CustomMod", "TelegramDesktop");
    gValues.ghostMode = settings.value("ghostMode", true).toBool();
    gValues.bypassRestrictions = settings.value("bypassRestrictions", true).toBool();
    gValues.offlineDb = settings.value("offlineDb", true).toBool();
    gValues.antiDelete = settings.value("antiDelete", true).toBool();
    gValues.antiEdit = settings.value("antiEdit", true).toBool();
    gValues.spoofMobile = settings.value("spoofMobile", true).toBool();
    gValues.storyAnonymousView = settings.value("storyAnonymousView", true).toBool();

    // Per-peer ghost overrides
    settings.beginGroup("GhostModePerPeer");
    for (const QString &key : settings.childKeys()) {
        gGhostPerPeer[key] = settings.value(key).toBool();
    }
    settings.endGroup();

    // Per-peer anti-delete overrides
    settings.beginGroup("AntiDeletePerPeer");
    for (const QString &key : settings.childKeys()) {
        gAntiDeletePerPeer[key] = settings.value(key).toBool();
    }
    settings.endGroup();

    // Per-peer anti-edit overrides (NEXT-6)
    settings.beginGroup("AntiEditPerPeer");
    for (const QString &key : settings.childKeys()) {
        gAntiEditPerPeer[key] = settings.value(key).toBool();
    }
    settings.endGroup();

    // Per-Chat Settings master list (NEXT-6)
    settings.beginGroup("PerPeerNames");
    for (const QString &key : settings.childKeys()) {
        gPerPeerNames[key] = settings.value(key).toString();
    }
    settings.endGroup();

    // Whitelist va Blocklist — JSON fayldan yuklanadi.
    // Fayl: <AppData>/CustomMod/peer_lists.json
    LoadPeerLists();

    gInitialized = true;

    // Pre-load restore cache if tracking is active for any peer.
    if (gValues.antiDelete || gValues.antiEdit
            || !gWhitelist.isEmpty() || !gBlocklist.isEmpty()) {
        CustomDB::LoadRestoreCache();
    }

    CustomDB::StartAutoBackup();
}

const Values& Get() {
    if (!gInitialized) Init();
    return gValues;
}

void Set(const QString &id, bool value) {
    UpdateValue(id, value);
    QSettings settings("CustomMod", "TelegramDesktop");
    settings.setValue(id, value);
}

// ── Legacy per-peer Ghost Mode (C12) ─────────────────────────────────────

bool GhostModeForPeer(const QString &peerId) {
    if (!gInitialized) Init();
    const auto it = gGhostPerPeer.constFind(peerId);
    return (it != gGhostPerPeer.constEnd()) ? it.value() : gValues.ghostMode;
}

void SetGhostModeForPeer(const QString &peerId, bool enabled) {
    if (!gInitialized) Init();
    gGhostPerPeer[peerId] = enabled;
    QSettings settings("CustomMod", "TelegramDesktop");
    settings.beginGroup("GhostModePerPeer");
    settings.setValue(peerId, enabled);
    settings.endGroup();
    if (!enabled) {
        CustomDB::ResetGhostRead(peerId);
    }
}

void ResetGhostModeForPeer(const QString &peerId) {
    if (!gInitialized) Init();
    gGhostPerPeer.remove(peerId);
    QSettings settings("CustomMod", "TelegramDesktop");
    settings.beginGroup("GhostModePerPeer");
    settings.remove(peerId);
    settings.endGroup();
    CustomDB::ResetGhostRead(peerId);
}

// ── Legacy per-peer Anti-Delete (C14) ────────────────────────────────────

bool AntiDeleteForPeer(const QString &peerId) {
    if (!gInitialized) Init();
    const auto it = gAntiDeletePerPeer.constFind(peerId);
    return (it != gAntiDeletePerPeer.constEnd()) ? it.value() : gValues.antiDelete;
}

void SetAntiDeleteForPeer(const QString &peerId, bool enabled) {
    if (!gInitialized) Init();
    gAntiDeletePerPeer[peerId] = enabled;
    QSettings settings("CustomMod", "TelegramDesktop");
    settings.beginGroup("AntiDeletePerPeer");
    settings.setValue(peerId, enabled);
    settings.endGroup();
}

void ResetAntiDeleteForPeer(const QString &peerId) {
    if (!gInitialized) Init();
    gAntiDeletePerPeer.remove(peerId);
    QSettings settings("CustomMod", "TelegramDesktop");
    settings.beginGroup("AntiDeletePerPeer");
    settings.remove(peerId);
    settings.endGroup();
}

// ── Whitelist ─────────────────────────────────────────────────────────────

void AddToWhitelist(const QString &peerId, const QString &displayName) {
    if (!gInitialized) Init();
    gWhitelist[peerId] = displayName;
    gBlocklist.remove(peerId); // mutual exclusion
    SavePeerLists();
}

void RemoveFromWhitelist(const QString &peerId) {
    if (!gInitialized) Init();
    gWhitelist.remove(peerId);
    SavePeerLists();
}

QVector<QPair<QString, QString>> GetWhitelist() {
    if (!gInitialized) Init();
    QVector<QPair<QString, QString>> result;
    result.reserve(gWhitelist.size());
    for (auto it = gWhitelist.constBegin(); it != gWhitelist.constEnd(); ++it) {
        result.append({ it.key(), it.value() });
    }
    return result;
}

bool IsInWhitelist(const QString &peerId) {
    if (!gInitialized) Init();
    return gWhitelist.contains(peerId);
}

// ── Blocklist ─────────────────────────────────────────────────────────────

void AddToBlocklist(const QString &peerId, const QString &displayName) {
    if (!gInitialized) Init();
    gBlocklist[peerId] = displayName;
    gWhitelist.remove(peerId); // mutual exclusion
    SavePeerLists();
}

void RemoveFromBlocklist(const QString &peerId) {
    if (!gInitialized) Init();
    gBlocklist.remove(peerId);
    SavePeerLists();
}

QVector<QPair<QString, QString>> GetBlocklist() {
    if (!gInitialized) Init();
    QVector<QPair<QString, QString>> result;
    result.reserve(gBlocklist.size());
    for (auto it = gBlocklist.constBegin(); it != gBlocklist.constEnd(); ++it) {
        result.append({ it.key(), it.value() });
    }
    return result;
}

bool IsInBlocklist(const QString &peerId) {
    if (!gInitialized) Init();
    return gBlocklist.contains(peerId);
}

void ClearWhitelist() {
    if (!gInitialized) Init();
    gWhitelist.clear();
    SavePeerLists();
}

void ClearBlocklist() {
    if (!gInitialized) Init();
    gBlocklist.clear();
    SavePeerLists();
}

// ── Unified "should track" helpers ────────────────────────────────────────
// Priority: Blocklist (false) > Whitelist (true) > Global flag

bool ShouldAntiDelete(const QString &peerId) {
    if (!gInitialized) Init();
    if (!peerId.isEmpty() && gBlocklist.contains(peerId)) return false;
    if (!peerId.isEmpty() && gWhitelist.contains(peerId)) return true;
    // Per-peer override (NEXT-6): Blocklist/Whitelist dan keyin tekshiriladi.
    return AntiDeleteForPeer(peerId);
}

bool ShouldAntiEdit(const QString &peerId) {
    if (!gInitialized) Init();
    if (!peerId.isEmpty() && gBlocklist.contains(peerId)) return false;
    if (!peerId.isEmpty() && gWhitelist.contains(peerId)) return true;
    // Per-peer override (NEXT-6): Blocklist/Whitelist dan keyin tekshiriladi.
    return AntiEditForPeer(peerId);
}

bool ShouldGhost(const QString &peerId) {
    if (!gInitialized) Init();
    if (!peerId.isEmpty() && gBlocklist.contains(peerId)) return false;
    if (!peerId.isEmpty() && gWhitelist.contains(peerId)) return true;
    return GhostModeForPeer(peerId);
}

bool ShouldBackgroundCache(const QString &peerId) {
    if (!gInitialized) Init();
    if (peerId.isEmpty()) return false;
    // BlackList — hech qachon cache qilmaymiz.
    if (gBlocklist.contains(peerId)) return false;
    // WhiteList — har doim cache qilamiz.
    if (gWhitelist.contains(peerId)) return true;
    // Per-Chat override: foydalanuvchi shu chat uchun AntiDelete yoki AntiEdit
    // ni aniq yoqgan bo'lsa — cache qilamiz. Ro'yxat baribir cheklangan
    // (faqat qo'lda qo'shilgan chatlar), shuning uchun performance saqlanadi.
    if (HasPerPeerOverride(peerId)) {
        return AntiDeleteForPeer(peerId) || AntiEditForPeer(peerId);
    }
    // Global flag ON bo'lsa ham — qolgan barcha chatlarni cache qilmaymiz.
    return false;
}

bool ShouldAnonymousStory(const QString &peerId) {
    if (!gInitialized) Init();
    if (!peerId.isEmpty() && gBlocklist.contains(peerId)) return false;
    if (!peerId.isEmpty() && gWhitelist.contains(peerId)) return true;
    return gValues.storyAnonymousView;
}

// ── Per-chat Anti-Edit override (NEXT-6) ─────────────────────────────────

bool AntiEditForPeer(const QString &peerId) {
    if (!gInitialized) Init();
    const auto it = gAntiEditPerPeer.constFind(peerId);
    return (it != gAntiEditPerPeer.constEnd()) ? it.value() : gValues.antiEdit;
}

void SetAntiEditForPeer(const QString &peerId, bool enabled) {
    if (!gInitialized) Init();
    gAntiEditPerPeer[peerId] = enabled;
    QSettings settings("CustomMod", "TelegramDesktop");
    settings.beginGroup("AntiEditPerPeer");
    settings.setValue(peerId, enabled);
    settings.endGroup();
}

void ResetAntiEditForPeer(const QString &peerId) {
    if (!gInitialized) Init();
    gAntiEditPerPeer.remove(peerId);
    QSettings settings("CustomMod", "TelegramDesktop");
    settings.beginGroup("AntiEditPerPeer");
    settings.remove(peerId);
    settings.endGroup();
}

// ── Per-Chat Settings boshqaruvi (NEXT-6) ────────────────────────────────

void AddPerPeerOverride(const QString &peerId, const QString &displayName) {
    if (!gInitialized) Init();
    gPerPeerNames[peerId] = displayName;
    // Agar avvaldan override yo'q bo'lsa — joriy global qiymat bilan boshlash.
    if (!gGhostPerPeer.contains(peerId))
        gGhostPerPeer[peerId] = gValues.ghostMode;
    if (!gAntiDeletePerPeer.contains(peerId))
        gAntiDeletePerPeer[peerId] = gValues.antiDelete;
    if (!gAntiEditPerPeer.contains(peerId))
        gAntiEditPerPeer[peerId] = gValues.antiEdit;

    QSettings settings("CustomMod", "TelegramDesktop");
    settings.beginGroup("PerPeerNames");
    settings.setValue(peerId, displayName);
    settings.endGroup();
    settings.beginGroup("GhostModePerPeer");
    settings.setValue(peerId, gGhostPerPeer[peerId]);
    settings.endGroup();
    settings.beginGroup("AntiDeletePerPeer");
    settings.setValue(peerId, gAntiDeletePerPeer[peerId]);
    settings.endGroup();
    settings.beginGroup("AntiEditPerPeer");
    settings.setValue(peerId, gAntiEditPerPeer[peerId]);
    settings.endGroup();
}

void RemovePerPeerOverride(const QString &peerId) {
    if (!gInitialized) Init();
    gPerPeerNames.remove(peerId);
    gGhostPerPeer.remove(peerId);
    gAntiDeletePerPeer.remove(peerId);
    gAntiEditPerPeer.remove(peerId);

    QSettings settings("CustomMod", "TelegramDesktop");
    auto removeKey = [&](const char *group) {
        settings.beginGroup(group);
        settings.remove(peerId);
        settings.endGroup();
    };
    removeKey("PerPeerNames");
    removeKey("GhostModePerPeer");
    removeKey("AntiDeletePerPeer");
    removeKey("AntiEditPerPeer");

    // Ghost reset: DB ham tozalansin.
    CustomDB::ResetGhostRead(peerId);
}

bool HasPerPeerOverride(const QString &peerId) {
    if (!gInitialized) Init();
    return gPerPeerNames.contains(peerId);
}

QVector<PerPeerEntry> GetPerPeerOverrides() {
    if (!gInitialized) Init();
    QVector<PerPeerEntry> result;
    result.reserve(gPerPeerNames.size());
    for (auto it = gPerPeerNames.constBegin(); it != gPerPeerNames.constEnd(); ++it) {
        const auto &id = it.key();
        result.append({
            id,
            it.value(),
            GhostModeForPeer(id),
            AntiDeleteForPeer(id),
            AntiEditForPeer(id)
        });
    }
    return result;
}

QString GetPeerDisplayName(const QString &peerId) {
    if (!gInitialized) Init();
    if (peerId.isEmpty()) return QString();
    // Whitelist dan qidirish (avval)
    auto it = gWhitelist.constFind(peerId);
    if (it != gWhitelist.constEnd() && !it.value().isEmpty()) {
        return it.value();
    }
    // Blocklist dan qidirish
    it = gBlocklist.constFind(peerId);
    if (it != gBlocklist.constEnd() && !it.value().isEmpty()) {
        return it.value();
    }
    return QString();
}

} // namespace CustomSettings
