#include "custom_branding.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>

namespace CustomBranding {
namespace {

Values gValues;
bool gLoaded = false;
QString gFilePath;

[[nodiscard]] QString ComputeFilePath() {
    const auto dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)
        + QStringLiteral("/CustomMod/");
    QDir().mkpath(dir);
    return dir + QStringLiteral("branding.json");
}

void WriteFile() {
    QJsonObject root;
    root[QStringLiteral("_comment_1")] =
        QStringLiteral("CustomMod branding sozlamalari.");
    root[QStringLiteral("_comment_2")] =
        QStringLiteral("Barcha o'zgartirishlar uchun dasturni RESTART qiling.");
    root[QStringLiteral("_comment_3")] =
        QStringLiteral("iconPath PNG/ICO fayl yo'li. Bo'sh = standart icon.");
    root[QStringLiteral("windowTitle")] = gValues.windowTitle;
    root[QStringLiteral("customModTitle")] = gValues.customModTitle;
    root[QStringLiteral("iconPath")] = gValues.iconPath;

    QFile f(gFilePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

} // namespace

void Load() {
    if (gLoaded) return;
    gLoaded = true;
    gFilePath = ComputeFilePath();

    QFile f(gFilePath);
    if (!f.open(QIODevice::ReadOnly)) {
        // Birinchi ishga tushish — default JSON fayl yaratamiz.
        WriteFile();
        return;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return;
    const auto root = doc.object();

    const auto readString = [&](const QString &key, QString &out) {
        if (root.contains(key)) {
            const auto v = root[key].toString();
            if (!v.isEmpty()) out = v;
        }
    };
    readString(QStringLiteral("windowTitle"), gValues.windowTitle);
    readString(QStringLiteral("customModTitle"), gValues.customModTitle);

    // iconPath bo'sh string ham qabul qilinadi (= standart icon)
    if (root.contains(QStringLiteral("iconPath"))) {
        gValues.iconPath = root[QStringLiteral("iconPath")].toString();
    }
}

const Values& Get() {
    if (!gLoaded) Load();
    return gValues;
}

QString FilePath() {
    if (!gLoaded) Load();
    return gFilePath;
}

void SetWindowTitle(const QString &value) {
    if (!gLoaded) Load();
    gValues.windowTitle = value.isEmpty()
        ? QStringLiteral("Telegram")
        : value;
    WriteFile();
}

void SetCustomModTitle(const QString &value) {
    if (!gLoaded) Load();
    gValues.customModTitle = value.isEmpty()
        ? QStringLiteral("Customizations (By Oybek)")
        : value;
    WriteFile();
}

void SetIconPath(const QString &value) {
    if (!gLoaded) Load();
    gValues.iconPath = value;
    WriteFile();
}

void Save() {
    if (!gLoaded) Load();
    WriteFile();
}

} // namespace CustomBranding
