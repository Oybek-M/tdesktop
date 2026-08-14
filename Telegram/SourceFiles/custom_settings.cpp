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

// T42: Category-based whitelist/blocklist. Key=static_cast<int>(PeerType), Value=enabled.
QHash<int, bool> gWhitelistCategories;
QHash<int, bool> gBlocklistCategories;

// Activity History Log: Include/Exclude — Whitelist/Blocklist'dan mustaqil.
QHash<QString, QString> gActivityInclude;
QHash<QString, QString> gActivityExclude;

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
    QJsonObject wlCats, blCats;
    for (auto it = gWhitelistCategories.constBegin(); it != gWhitelistCategories.constEnd(); ++it) {
        wlCats[QString::number(it.key())] = it.value();
    }
    for (auto it = gBlocklistCategories.constBegin(); it != gBlocklistCategories.constEnd(); ++it) {
        blCats[QString::number(it.key())] = it.value();
    }

    QJsonArray aInc, aExc;
    for (auto it = gActivityInclude.constBegin(); it != gActivityInclude.constEnd(); ++it) {
        QJsonObject o;
        o[QStringLiteral("id")]   = it.key();
        o[QStringLiteral("name")] = it.value();
        aInc.append(o);
    }
    for (auto it = gActivityExclude.constBegin(); it != gActivityExclude.constEnd(); ++it) {
        QJsonObject o;
        o[QStringLiteral("id")]   = it.key();
        o[QStringLiteral("name")] = it.value();
        aExc.append(o);
    }

    QJsonObject root;
    root[QStringLiteral("whitelist")] = wl;
    root[QStringLiteral("blacklist")] = bl;
    root[QStringLiteral("wl_categories")] = wlCats;
    root[QStringLiteral("bl_categories")] = blCats;
    root[QStringLiteral("activity_include")] = aInc;
    root[QStringLiteral("activity_exclude")] = aExc;

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

    gWhitelistCategories.clear();
    const auto wlCats = root[QStringLiteral("wl_categories")].toObject();
    for (auto it = wlCats.constBegin(); it != wlCats.constEnd(); ++it) {
        bool ok = false;
        const int key = it.key().toInt(&ok);
        if (ok) gWhitelistCategories[key] = it.value().toBool();
    }

    gBlocklistCategories.clear();
    const auto blCats = root[QStringLiteral("bl_categories")].toObject();
    for (auto it = blCats.constBegin(); it != blCats.constEnd(); ++it) {
        bool ok = false;
        const int key = it.key().toInt(&ok);
        if (ok) gBlocklistCategories[key] = it.value().toBool();
    }

    gActivityInclude.clear();
    for (const auto &v : root[QStringLiteral("activity_include")].toArray()) {
        const auto o  = v.toObject();
        const auto id = o[QStringLiteral("id")].toString();
        if (!id.isEmpty()) gActivityInclude[id] = o[QStringLiteral("name")].toString();
    }

    gActivityExclude.clear();
    for (const auto &v : root[QStringLiteral("activity_exclude")].toArray()) {
        const auto o  = v.toObject();
        const auto id = o[QStringLiteral("id")].toString();
        if (!id.isEmpty()) gActivityExclude[id] = o[QStringLiteral("name")].toString();
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
    else if (id == "mutualContactShowInChatList") gValues.mutualContactShowInChatList = value;
    else if (id == "mutualContactShowInContactsList") gValues.mutualContactShowInContactsList = value;
    else if (id == "mutualContactShowInProfile") gValues.mutualContactShowInProfile = value;
    else if (id == "mutualContactShowInMembersList") gValues.mutualContactShowInMembersList = value;
    else if (id == "activityHistoryTrackAllContacts") gValues.activityHistoryTrackAllContacts = value;
    else if (id == "upstreamCheckEnabled") gValues.upstreamCheckEnabled = value;
    else if (id == "storyMediaBackupEnabled") gValues.storyMediaBackupEnabled = value;
}

void UpdateString(const QString &id, const QString &value) {
    if (id == "spoofDeviceModel") gValues.spoofDeviceModel = value;
    else if (id == "spoofSystemVersion") gValues.spoofSystemVersion = value;
    else if (id == "mutualContactChatListEmoji") gValues.mutualContactChatListEmoji = value;
    else if (id == "mutualContactContactsListEmoji") gValues.mutualContactContactsListEmoji = value;
    else if (id == "mutualContactProfileEmoji") gValues.mutualContactProfileEmoji = value;
    else if (id == "mutualContactMembersListEmoji") gValues.mutualContactMembersListEmoji = value;
    else if (id == "upstreamLastKnownVersion") gValues.upstreamLastKnownVersion = value;
}

void UpdateInt(const QString &id, int value) {
    if (id == "spoofDeviceType") gValues.spoofDeviceType = value;
    else if (id == "upstreamCheckIntervalMinutes") gValues.upstreamCheckIntervalMinutes = value;
}

} // namespace

void Init() {
    if (gInitialized) return;

    QSettings settings("CustomMod", "TelegramDesktop");

    // A13/D6: eski (snake_case) kalitlarni bir marta ko'chirish.
    // Ular hech qachon o'qilmagan — registry da "anti_delete: true"
    // turib, kod esa "antiDelete" ni o'qigani uchun foydalanuvchi
    // sozlama yoqilgan deb o'ylardi. Faqat yangi kalit MAVJUD
    // BO'LMASA ko'chiramiz, so'ng eskisini o'chiramiz — bir marta
    // bajarilgach keyingi ishga tushirishlarda bu blok tekinga ishlaydi.
    {
        const auto migrate = [&](const char *oldKey, const char *newKey) {
            if (settings.contains(oldKey)) {
                if (!settings.contains(newKey)) {
                    settings.setValue(newKey, settings.value(oldKey));
                }
                settings.remove(oldKey);
            }
        };
        migrate("anti_delete", "antiDelete");
        migrate("anti_edit", "antiEdit");
        migrate("ghost_mode", "ghostMode");
        migrate("bypass_restrictions", "bypassRestrictions");
        migrate("offline_db", "offlineDb");
    }

    gValues.ghostMode = settings.value("ghostMode", true).toBool();
    gValues.bypassRestrictions = settings.value("bypassRestrictions", true).toBool();
    gValues.offlineDb = settings.value("offlineDb", true).toBool();
    gValues.antiDelete = settings.value("antiDelete", true).toBool();
    gValues.antiEdit = settings.value("antiEdit", true).toBool();
    gValues.spoofMobile = settings.value("spoofMobile", true).toBool();
    gValues.storyAnonymousView = settings.value("storyAnonymousView", true).toBool();
    gValues.spoofDeviceType    = settings.value("spoofDeviceType", 0).toInt();
    gValues.spoofDeviceModel   = settings.value("spoofDeviceModel",
        u"Samsung Galaxy S26 Ultra"_q).toString();
    gValues.spoofSystemVersion = settings.value("spoofSystemVersion",
        u"Android 15"_q).toString();
    gValues.mutualContactShowInChatList = settings.value(
        "mutualContactShowInChatList", true).toBool();
    gValues.mutualContactChatListEmoji = settings.value(
        "mutualContactChatListEmoji", u"🤝"_q).toString();
    gValues.mutualContactShowInContactsList = settings.value(
        "mutualContactShowInContactsList", true).toBool();
    gValues.mutualContactContactsListEmoji = settings.value(
        "mutualContactContactsListEmoji", u"🤝"_q).toString();
    gValues.mutualContactShowInProfile = settings.value(
        "mutualContactShowInProfile", true).toBool();
    gValues.mutualContactProfileEmoji = settings.value(
        "mutualContactProfileEmoji", u"🤝"_q).toString();
    gValues.mutualContactShowInMembersList = settings.value(
        "mutualContactShowInMembersList", true).toBool();
    gValues.mutualContactMembersListEmoji = settings.value(
        "mutualContactMembersListEmoji", u"🤝"_q).toString();

    gValues.activityHistoryTrackAllContacts = settings.value(
        "activityHistoryTrackAllContacts", true).toBool();

    gValues.upstreamCheckEnabled = settings.value(
        "upstreamCheckEnabled", true).toBool();
    gValues.upstreamCheckIntervalMinutes = settings.value(
        "upstreamCheckIntervalMinutes", 1440).toInt();
    gValues.upstreamLastKnownVersion = settings.value(
        "upstreamLastKnownVersion", QString()).toString();
    gValues.upstreamLastCheckedAt = settings.value(
        "upstreamLastCheckedAt", qint64(0)).toLongLong();

    gValues.storyMediaBackupEnabled = settings.value(
        "storyMediaBackupEnabled", false).toBool();

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

void SetString(const QString &id, const QString &value) {
    UpdateString(id, value);
    QSettings settings("CustomMod", "TelegramDesktop");
    settings.setValue(id, value);
}

void SetInt(const QString &id, int value) {
    UpdateInt(id, value);
    QSettings settings("CustomMod", "TelegramDesktop");
    settings.setValue(id, value);
}

void SetUpstreamLastCheckedAt(qint64 timestamp) {
    gValues.upstreamLastCheckedAt = timestamp;
    QSettings settings("CustomMod", "TelegramDesktop");
    settings.setValue("upstreamLastCheckedAt", timestamp);
}

QString SpoofLangPack() {
    switch (Get().spoofDeviceType) {
    case 0: return u"android"_q;
    case 1: return u"ios"_q;
    default: return u"tdesktop"_q;
    }
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
    if (gWhitelist.contains(peerId)) return true;
    // 2026-08-14: ANIQ yozuv KATEGORIYAdan ustun. Foydalanuvchi bitta
    // chatni qo'lda Black List'ga qo'shgan bo'lsa, White List'dagi
    // kategoriya (masalan "Guruhlar") uni qaytarib yoqmasligi kerak.
    if (gBlocklist.contains(peerId)) return false;
    const auto type = GetPeerType(peerId);
    if (type != PeerType::Unknown) {
        const auto it = gWhitelistCategories.constFind(static_cast<int>(type));
        if (it != gWhitelistCategories.constEnd() && it.value()) return true;
    }
    return false;
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
    if (gBlocklist.contains(peerId)) return true;
    // 2026-08-14 (HAQIQIY XATO, foydalanuvchiga ta'sir qilgan): ANIQ
    // White List yozuvi KATEGORIYA blokidan ustun bo'lishi kerak.
    //
    // Ilgari: foydalanuvchining 5 ta kanali White List'da edi, lekin
    // Black List'da "Kanallar / superguruhlar" kategoriyasi yoqilgan.
    // GetPeerType() ularni Channel deb aniqlagani uchun bu funksiya true
    // qaytarardi va ShouldAntiDelete() (Blocklist > Whitelist) ularni
    // o'chirib qo'yardi — ya'ni foydalanuvchining aniq qarori jimgina
    // bekor qilinardi.
    //
    // AddToBlocklist() da gWhitelist.remove() bor, ya'ni ANIQ yozuvlar
    // bir-birini istisno qiladi; kategoriya uchun esa bunday himoya
    // yo'q edi. Shu bo'shliq to'ldirildi.
    if (gWhitelist.contains(peerId)) return false;
    const auto type = GetPeerType(peerId);
    if (type != PeerType::Unknown) {
        const auto it = gBlocklistCategories.constFind(static_cast<int>(type));
        if (it != gBlocklistCategories.constEnd() && it.value()) return true;
    }
    return false;
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

// ── T42: Peer type detection ──────────────────────────────────────────────

PeerType GetPeerType(const QString &peerId) {
    bool ok = false;
    const qint64 value = peerId.toLongLong(&ok);
    if (!ok) return PeerType::Unknown;
    switch (static_cast<int>((value >> 48) & 0xFF)) {
        case 0: return PeerType::User;
        case 1: return PeerType::Group;
        case 2: return PeerType::Channel;
        default: return PeerType::Unknown;
    }
}

bool IsWhitelistCategoryEnabled(PeerType type) {
    if (!gInitialized) Init();
    const auto it = gWhitelistCategories.constFind(static_cast<int>(type));
    return it != gWhitelistCategories.constEnd() && it.value();
}

void SetWhitelistCategory(PeerType type, bool enabled) {
    if (!gInitialized) Init();
    gWhitelistCategories[static_cast<int>(type)] = enabled;
    if (enabled) {
        gBlocklistCategories[static_cast<int>(type)] = false; // mutual exclusion
    }
    SavePeerLists();
}

bool IsBlocklistCategoryEnabled(PeerType type) {
    if (!gInitialized) Init();
    const auto it = gBlocklistCategories.constFind(static_cast<int>(type));
    return it != gBlocklistCategories.constEnd() && it.value();
}

void SetBlocklistCategory(PeerType type, bool enabled) {
    if (!gInitialized) Init();
    gBlocklistCategories[static_cast<int>(type)] = enabled;
    if (enabled) {
        gWhitelistCategories[static_cast<int>(type)] = false; // mutual exclusion
    }
    SavePeerLists();
}

// ── Unified "should track" helpers ────────────────────────────────────────
// Priority: Blocklist (false) > Whitelist (true) > Global flag

bool ShouldAntiDelete(const QString &peerId) {
    if (!gInitialized) Init();
    if (!peerId.isEmpty() && IsInBlocklist(peerId)) return false;
    if (!peerId.isEmpty() && IsInWhitelist(peerId)) return true;
    return AntiDeleteForPeer(peerId);
}

bool ShouldAntiEdit(const QString &peerId) {
    if (!gInitialized) Init();
    if (!peerId.isEmpty() && IsInBlocklist(peerId)) return false;
    if (!peerId.isEmpty() && IsInWhitelist(peerId)) return true;
    return AntiEditForPeer(peerId);
}

bool ShouldGhost(const QString &peerId) {
    if (!gInitialized) Init();
    if (!peerId.isEmpty() && IsInBlocklist(peerId)) return false;
    if (!peerId.isEmpty() && IsInWhitelist(peerId)) return true;
    return GhostModeForPeer(peerId);
}

bool ShouldBackgroundCache(const QString &peerId) {
    if (!gInitialized) Init();
    if (peerId.isEmpty()) return false;
    // A13/D3: ilgari oxirgi bosqich `return false` edi — global
    // antiDelete/antiEdit bayrog'i e'tiborga olinmasdi. Natijada
    // ShouldAntiDelete() true qaytarib turgan chatda fon-cache umuman
    // ishlamasdi va butun-chat o'chirilishida ma'lumot yo'qolardi.
    // Endi zanjir ShouldAntiDelete/ShouldAntiEdit bilan bir xil:
    // Blocklist > Whitelist > per-peer override > global bayroq.
    // (AntiDeleteForPeer o'zi per-peer xaritani tekshirib, topilmasa
    // global qiymatga tushadi — shu sabab alohida HasPerPeerOverride
    // shoxobchasi kerak emas.)
    if (IsInBlocklist(peerId)) return false;
    if (IsInWhitelist(peerId)) return true;
    return AntiDeleteForPeer(peerId) || AntiEditForPeer(peerId);
}

QString TrackingReason(const QString &peerId) {
    if (!gInitialized) Init();
    // A13/K5.3: foydalanuvchi "nega bu chat kuzatilmayapti?" degan savolga
    // taxmin qilmasdan javob topsin — ShouldBackgroundCache bilan AYNAN
    // bir xil zanjir, faqat natija o'rniga sababni qaytaradi.
    if (peerId.isEmpty()) {
        return u"Noma'lum chat"_q;
    }
    if (IsInBlocklist(peerId)) {
        return u"Qora ro'yxat — kuzatilmaydi"_q;
    }
    if (IsInWhitelist(peerId)) {
        return u"Oq ro'yxat — kuzatiladi"_q;
    }
    const auto perPeer = gAntiDeletePerPeer.constFind(peerId);
    if (perPeer != gAntiDeletePerPeer.constEnd()) {
        return perPeer.value()
            ? u"Shu chat sozlamasi — kuzatiladi"_q
            : u"Shu chat sozlamasi — kuzatilmaydi"_q;
    }
    return gValues.antiDelete
        ? u"Umumiy sozlama — kuzatiladi"_q
        : u"Umumiy sozlama — kuzatilmaydi"_q;
}

bool ShouldAnonymousStory(const QString &peerId) {
    if (!gInitialized) Init();
    if (!peerId.isEmpty() && IsInBlocklist(peerId)) return false;
    if (!peerId.isEmpty() && IsInWhitelist(peerId)) return true;
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

void ClearAllPerPeerOverrides() {
    if (!gInitialized) Init();
    // Ghost DB reset — har bir peer uchun
    for (const QString &peerId : gPerPeerNames.keys()) {
        CustomDB::ResetGhostRead(peerId);
    }
    gPerPeerNames.clear();
    gGhostPerPeer.clear();
    gAntiDeletePerPeer.clear();
    gAntiEditPerPeer.clear();

    QSettings settings("CustomMod", "TelegramDesktop");
    settings.remove("PerPeerNames");
    settings.remove("GhostModePerPeer");
    settings.remove("AntiDeletePerPeer");
    settings.remove("AntiEditPerPeer");
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

// ── Activity History Log: Include List ───────────────────────────────────

void AddToActivityInclude(const QString &peerId, const QString &displayName) {
    if (!gInitialized) Init();
    gActivityInclude[peerId] = displayName;
    gActivityExclude.remove(peerId); // mutual exclusion
    SavePeerLists();
}

void RemoveFromActivityInclude(const QString &peerId) {
    if (!gInitialized) Init();
    gActivityInclude.remove(peerId);
    SavePeerLists();
}

QVector<QPair<QString, QString>> GetActivityInclude() {
    if (!gInitialized) Init();
    QVector<QPair<QString, QString>> result;
    result.reserve(gActivityInclude.size());
    for (auto it = gActivityInclude.constBegin(); it != gActivityInclude.constEnd(); ++it) {
        result.append({ it.key(), it.value() });
    }
    return result;
}

bool IsInActivityInclude(const QString &peerId) {
    if (!gInitialized) Init();
    return gActivityInclude.contains(peerId);
}

// ── Activity History Log: Exclude List ───────────────────────────────────

void AddToActivityExclude(const QString &peerId, const QString &displayName) {
    if (!gInitialized) Init();
    gActivityExclude[peerId] = displayName;
    gActivityInclude.remove(peerId); // mutual exclusion
    SavePeerLists();
}

void RemoveFromActivityExclude(const QString &peerId) {
    if (!gInitialized) Init();
    gActivityExclude.remove(peerId);
    SavePeerLists();
}

QVector<QPair<QString, QString>> GetActivityExclude() {
    if (!gInitialized) Init();
    QVector<QPair<QString, QString>> result;
    result.reserve(gActivityExclude.size());
    for (auto it = gActivityExclude.constBegin(); it != gActivityExclude.constEnd(); ++it) {
        result.append({ it.key(), it.value() });
    }
    return result;
}

bool IsInActivityExclude(const QString &peerId) {
    if (!gInitialized) Init();
    return gActivityExclude.contains(peerId);
}

bool ShouldTrackActivity(const QString &peerId, bool isContact) {
    if (!gInitialized) Init();
    if (peerId.isEmpty()) return false;
    if (IsInActivityExclude(peerId)) return false;
    if (IsInActivityInclude(peerId)) return true;
    return gValues.activityHistoryTrackAllContacts && isContact;
}

} // namespace CustomSettings
