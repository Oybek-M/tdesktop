#pragma once

#include <QtCore/QString>

namespace CustomSettings {

struct Values {
    bool ghostMode = true;
    bool bypassRestrictions = true;
    bool offlineDb = true;
    bool antiDelete = true;
    bool antiEdit = true;
    bool spoofMobile = true;
};

void Init();
const Values& Get();
void Set(const QString &id, bool value);

// Helpers for quick access using explicit global namespace
inline bool GhostMode() { return Get().ghostMode; }
inline bool BypassRestrictions() { return Get().bypassRestrictions; }
inline bool OfflineDb() { return Get().offlineDb; }
inline bool AntiDelete() { return Get().antiDelete; }
inline bool AntiEdit() { return Get().antiEdit; }
inline bool SpoofMobile() { return Get().spoofMobile; }

} // namespace CustomSettings
