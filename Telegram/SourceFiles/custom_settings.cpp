#include "custom_settings.h"

namespace CustomSettings {
namespace {

Values gValues;
bool gInitialized = false;

void UpdateValue(const QString &id, bool value) {
    if (id == "ghostMode") gValues.ghostMode = value;
    else if (id == "bypassRestrictions") gValues.bypassRestrictions = value;
    else if (id == "offlineDb") gValues.offlineDb = value;
    else if (id == "antiDelete") gValues.antiDelete = value;
    else if (id == "antiEdit") gValues.antiEdit = value;
    else if (id == "spoofMobile") gValues.spoofMobile = value;
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

    gInitialized = true;
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

} // namespace CustomSettings
