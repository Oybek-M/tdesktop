#pragma once

#include <QtCore/QString>

// CustomMod branding tizimi.
//
// JSON fayli: %AppData%/CustomMod/branding.json
// Birinchi ishga tushishda default qiymatlar bilan avtomatik yaratiladi.
//
// FAYL FORMATI:
// {
//   "windowTitle":    "Telegram",                    // Main window title
//   "customModTitle": "Customizations (By Oybek)",   // Custom window title
//   "iconPath":       ""                             // PNG/ICO fayl yo'li (bo'sh=standart)
// }
//
// HAR BIR MAYDONNING TA'SIRI:
//
// | Maydon          | Qachon ta'sir qiladi               |
// |-----------------|------------------------------------|
// | windowTitle     | DARHOL (yangi xabar/chat tanlanda) |
// | customModTitle  | KEYINGI custom window ochilganda   |
// | iconPath        | DASTUR RESTART qilingandan keyin   |
//
// REBUILD KERAK BO'LADIGAN NARSALAR (JSON da YO'Q):
// 1. Executable nomi (Telegram.exe → MyApp.exe)
//    → CMakeLists.txt va Windows resource (.rc) faylida
// 2. Windows taskbar Application ID
//    → launcher.cpp: setApplicationName() — REGISTRY PATH ham o'zgaradi,
//       barcha sozlamalar yo'qoladi! Qilmaslik tavsiya etiladi.

namespace CustomBranding {

struct Values {
    QString windowTitle = QStringLiteral("Telegram");
    QString customModTitle = QStringLiteral("Customizations (By Oybek)");
    QString iconPath;
};

// JSON fayldan yuklaydi. Birinchi marta default fayl yaratadi.
// Application start vaqtida bir marta chaqirilishi kerak.
void Load();

// Qiymatlarni qaytaradi. Agar Load() chaqirilmagan bo'lsa — chaqiradi.
[[nodiscard]] const Values& Get();

// Qaysi fayldan o'qildi (debug uchun).
[[nodiscard]] QString FilePath();

// UI orqali o'zgartirish uchun setterlar (Custom Settings dan).
// Har bir setter darhol JSON ga yozadi.
void SetWindowTitle(const QString &value);
void SetCustomModTitle(const QString &value);
void SetIconPath(const QString &value);

// Barcha qiymatlarni JSON ga yozadi (setterlarsiz).
void Save();

} // namespace CustomBranding
