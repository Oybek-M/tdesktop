# Dynamic Device Name + Icon Selector Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

## Status: ✅ IMPLEMENTED va BUILD QILINDI (2026-07-03)

Barcha 6 ta task bajarildi, Release build muvaffaqiyatli yakunlandi
(`53 succeeded, 0 failed`). Haqiqiy implementatsiya rejadagi pseudocode'dan
bir nechta joyda farq qiladi (real kod ustunlik qiladi):

- **UI (`custom_mod_window.cpp`):** platform tanlash 4 ta alohida
  `Ui::RoundButton` orqali amalga oshirildi (`SegmentControl` codebase'da
  yo'q edi). Har bir tugma bosilganda `spoofDeviceType` saqlanadi va
  Device Name / System Version InputField'lari preset qiymat bilan
  to'ldiriladi (lekin foydalanuvchi keyin ularni erkin tahrirlashi
  mumkin). Alohida "Saqlash" tugmasi ikkala matn maydonini birgalikda
  yozadi.
- **Presets:** Android → "Samsung Galaxy S26 Ultra"/"Android 15",
  iOS → "iPhone 17 Pro Max"/"iOS 18", Windows → "PC"/"Windows 11",
  Linux → "PC"/"Linux".
- **`session_private.cpp`:** `deviceModel`/`systemVersion`/`langPackName`
  barchasi `QString` bo'lib chiqdi (loyihada taxmin qilingan
  `std::string` emas) — shuning uchun `.toStdString()` kerak bo'lmadi,
  to'g'ridan-to'g'ri `CustomSettings::SpoofDeviceModel()` va
  `SpoofLangPack()` ishlatildi.

Build muhitidagi qo'shimcha (funksiyaga aloqasi yo'q) muammolar va ularning
yechimi alohida hujjatda: [2026-07-03-windows-build-environment-fixes.md](2026-07-03-windows-build-environment-fixes.md).

**Goal:** "Mobile ko'rinish" (SpoofMobile) rejimida qurilma nomini runtime'da o'zgartirish imkonini berish va to'g'ri qurilma ikonkasini (Android/iOS/Windows/Linux) ta'minlash.

**Architecture:** CustomSettings'ga yangi string/int maydonlar qo'shiladi; UI'da toggle ostida InputField + 4 ta tugmali platform tanlash widgeti chiqadi; `session_private.cpp`'da `langPackName` ham ovverridelanadi — bu icon uchun asosiy yechim.

**Tech Stack:** C++ Qt, `Ui::InputField`, `Ui::RoundButton` / `Ui::Checkbox`, `QSettings`, tdesktop custom MTP fields.

---

## Kontekst

### Muammo
`SpoofMobile()` yoqilganda 3 joyda hardcode qilingan:
- `main_account.cpp:422` — `startMtp()` ichida
- `main_account.cpp:574` — `startMtpForKeys()` ichida
- `mtproto/session_private.cpp:698` — `initConnection` yuborilganda

**Icon muammosi:** `langPackName = "tdesktop"` bo'lgani uchun Telegram serveri bu sessiyani desktop sifatida tasnif qiladi va desktop ikonkasini ko'rsatadi — `system_version = "Android 14"` bo'lsa ham. Yechim: `SpoofMobile()` ON bo'lganda `langPackName`'ni ham ovverridelash.

### langPackName oqimi
```
Lang::GetInstance().langPackName()
  → Session::Options.langPackName   (session.cpp:251)
    → _options->langPackName        (session_private.cpp:679)
      → MTP_string(langPackName)    (session_private.cpp:710, initConnection'da)
```

### Icon-langPack xaritasi (Telegram serveri tekshiradi)
| Platform | langPack  | systemVersion misol    |
|----------|-----------|------------------------|
| Android  | "android" | "Android 15"           |
| iOS      | "ios"     | "iPhone OS 18.2"       |
| Windows  | "tdesktop"| "Windows 11"           |
| Linux    | "tdesktop"| "Ubuntu 24.04"         |

> **Note:** Windows va Linux uchun `langPack = "tdesktop"` qoladi — chunki bu platformalar tdesktop kabi tasniflanadi; ularni farqlash `system_version` matniga qarab.

### O'zgartiriladigan fayllar
| Fayl | Nima | Qator |
|------|------|-------|
| `custom_settings.h` | yangi maydon + helper funksiyalar | Values struct + inline |
| `custom_settings.cpp` | Init/Save/SetString + SpoofLangPack() | ~125-148 |
| `main_account.cpp` | hardcode → helper chaqiruv | 422, 574 |
| `mtproto/session_private.cpp` | hardcode + langPackName override | 697-700, 679 |
| `custom_mod_window.cpp` | UI: InputField + platform buttons | ~483 atrofi |

---

## Task 1: custom_settings.h — Yangi maydonlar va helperlar

**Fayl:** `Telegram/SourceFiles/custom_settings.h`

- [x] **Step 1: Values struct'ga yangi maydonlar qo'shish**

`Values` struct'ni (line 17-25) quyidagicha o'zgartirish:

```cpp
struct Values {
    bool ghostMode = true;
    bool bypassRestrictions = true;
    bool offlineDb = true;
    bool antiDelete = true;
    bool antiEdit = true;
    bool spoofMobile = true;
    bool storyAnonymousView = true;
    // Dynamic device spoof fields (C22)
    int  spoofDeviceType = 0;                              // 0=Android, 1=iOS, 2=Windows, 3=Linux
    QString spoofDeviceModel = u"Samsung Galaxy S26 Ultra"_q;
    QString spoofSystemVersion = u"Android 15"_q;
};
```

- [x] **Step 2: `SetString()` va `SpoofLangPack()` deklaratsiyasi qo'shish**

`Set(const QString &id, bool value);` qatoridan keyin:

```cpp
void SetString(const QString &id, const QString &value);
void SetInt(const QString &id, int value);
```

- [x] **Step 3: Inline helper funksiyalar qo'shish**

`inline bool SpoofMobile() { ... }` dan keyin:

```cpp
inline int     SpoofDeviceType()    { return Get().spoofDeviceType; }
inline QString SpoofDeviceModel()   { return Get().spoofDeviceModel; }
inline QString SpoofSystemVersion() { return Get().spoofSystemVersion; }

// langPackName: "android", "ios", yoki "tdesktop" qaytaradi
[[nodiscard]] QString SpoofLangPack();
```

- [x] **Step 4: Compile tekshirish (optional, keyinroq build bilan)**

> Bu step keyingi tasklarda boshqa fayllar o'zgartirilgandan keyin to'liq tekshiriladi.

---

## Task 2: custom_settings.cpp — Init, Save, SetString, SpoofLangPack

**Fayl:** `Telegram/SourceFiles/custom_settings.cpp`

- [x] **Step 1: `UpdateValue` funksiyasiga spoofDeviceType qo'shish (line ~125)**

Mavjud `UpdateValue` funksiyasida `else if (id == "storyAnonymousView")` dan keyin:

```cpp
else if (id == "spoofDeviceType") gValues.spoofDeviceType = value ? 1 : 0;
```

> **Muhim:** `UpdateValue` faqat `bool` qiymatlar uchun — int/string uchun yangi funksiyalar.

- [x] **Step 2: `UpdateString` va `UpdateInt` qo'shish (namespace ichida, `UpdateValue` dan keyin)**

```cpp
void UpdateString(const QString &id, const QString &value) {
    if (id == "spoofDeviceModel") gValues.spoofDeviceModel = value;
    else if (id == "spoofSystemVersion") gValues.spoofSystemVersion = value;
}

void UpdateInt(const QString &id, int value) {
    if (id == "spoofDeviceType") gValues.spoofDeviceType = value;
}
```

- [x] **Step 3: `Init()` funksiyasida yangi maydonlarni yuklash (line ~137)**

`gValues.storyAnonymousView = settings.value(...)` qatoridan keyin:

```cpp
gValues.spoofDeviceType    = settings.value("spoofDeviceType", 0).toInt();
gValues.spoofDeviceModel   = settings.value("spoofDeviceModel",
    u"Samsung Galaxy S26 Ultra"_q).toString();
gValues.spoofSystemVersion = settings.value("spoofSystemVersion",
    u"Android 15"_q).toString();
```

- [x] **Step 4: `SetString()` va `SetInt()` public funksiyalar (namespace'dan tashqarida)**

```cpp
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
```

- [x] **Step 5: `SpoofLangPack()` funksiyasi (namespace ichida, public)**

```cpp
QString SpoofLangPack() {
    switch (Get().spoofDeviceType) {
    case 0: return u"android"_q;
    case 1: return u"ios"_q;
    default: return u"tdesktop"_q;
    }
}
```

---

## Task 3: main_account.cpp — Hardcoded stringlarni helperlar bilan almashtirish

**Fayl:** `Telegram/SourceFiles/main/main_account.cpp`

### Joylashuv 1: `startMtp()` (line ~421-423)

- [x] **Step 1: Mavjud kodni almashtirish**

```cpp
// ESKI:
if (CustomSettings::SpoofMobile()) {
    fields.deviceModel = u"Samsung Galaxy S26 Ultra"_q;
    fields.systemVersion = u"Android 14"_q;
}

// YANGI:
if (CustomSettings::SpoofMobile()) {
    fields.deviceModel = CustomSettings::SpoofDeviceModel();
    fields.systemVersion = CustomSettings::SpoofSystemVersion();
}
```

### Joylashuv 2: `startMtpForKeys()` (line ~573-575)

- [x] **Step 2: Mavjud kodni almashtirish**

```cpp
// ESKI:
if (CustomSettings::SpoofMobile()) {
    destroyFields.deviceModel = u"Samsung Galaxy S26 Ultra"_q;
    destroyFields.systemVersion = u"Android 14"_q;
}

// YANGI:
if (CustomSettings::SpoofMobile()) {
    destroyFields.deviceModel = CustomSettings::SpoofDeviceModel();
    destroyFields.systemVersion = CustomSettings::SpoofSystemVersion();
}
```

---

## Task 4: session_private.cpp — Hardcode + langPackName override

**Fayl:** `Telegram/SourceFiles/mtproto/session_private.cpp`

- [x] **Step 1: `custom_settings.h` ni include qilish (faylning boshida)**

```cpp
#include "custom_settings.h"
```

> Agar allaqachon include bo'lsa — o'tkazib yuboring.

- [x] **Step 2: Device model/version override (line ~697-700)**

```cpp
// ESKI:
auto deviceModelToUse = deviceModel;
auto systemVersionToUse = systemVersion;
if (CustomSettings::SpoofMobile()) {
    deviceModelToUse = "Samsung Galaxy S26 Ultra";
    systemVersionToUse = "Android 14";
}

// YANGI:
auto deviceModelToUse = deviceModel;
auto systemVersionToUse = systemVersion;
if (CustomSettings::SpoofMobile()) {
    deviceModelToUse = CustomSettings::SpoofDeviceModel().toStdString();
    systemVersionToUse = CustomSettings::SpoofSystemVersion().toStdString();
}
```

> **Eslatma:** `deviceModelToUse` va `systemVersionToUse` — `std::string` tipida, shuning uchun `.toStdString()` ishlatiladi. Agar `QString` bo'lsa — `.toStdString()` kerak emas.

- [x] **Step 3: langPackName override (line ~679 atrofi)**

```cpp
// ESKI:
const auto langPackName = _options->langPackName;

// YANGI:
const auto langPackName = CustomSettings::SpoofMobile()
    ? CustomSettings::SpoofLangPack().toStdString()
    : _options->langPackName;
```

> **Muhim:** `_options->langPackName` tipi `std::string` yoki `QString`'mi — tekshirish kerak. Agar `std::string` bo'lsa: `.toStdString()`. Agar `QString` bo'lsa: `.toStdString()` kerak emas, lekin type mismatch bo'lmasligi uchun `QStringToStdString(CustomSettings::SpoofLangPack())` ishlatish mumkin. Kodni o'qib, tip aniqlansin.

---

## Task 5: custom_mod_window.cpp — UI: Device type selector + InputFields

**Fayl:** `Telegram/SourceFiles/custom_mod_window.cpp`

### Maqsad
Mavjud oddiy `addToggle(u"spoofMobile"_q, ...)` ni kengaytirish:
1. Toggle avvalgiday qoladi
2. Toggle ON bo'lganda yoki birinchi ochilishda: platform buttons (Android/iOS/Windows/Linux) ko'rinadi
3. Device Name InputField
4. System Version InputField (platform tanlanganda auto-filled, lekin tahrirlash mumkin)
5. Kichik eslatma: "O'zgarishlar qayta ulanishdan keyin kuchga kiradi"

### Platform presets (UI va save uchun)

```cpp
struct DevicePreset {
    int type;
    const char* label;
    const char* defaultModel;
    const char* defaultSysVer;
};
static const DevicePreset kPresets[] = {
    { 0, "Android", "Samsung Galaxy S26 Ultra", "Android 15"     },
    { 1, "iOS",     "iPhone 17 Pro",            "iPhone OS 18.2" },
    { 2, "Windows", "PC",                       "Windows 11"     },
    { 3, "Linux",   "PC",                       "Ubuntu 24.04"   },
};
```

### UI kod (mavjud `addToggle` ni almashtirish)

- [x] **Step 1: Mavjud oddiy toggle'ni topish va o'chirish**

```cpp
// O'CHIRILSIN:
addToggle(
    u"spoofMobile"_q,
    u"Mobil qurilma koʻrinishi"_q,
    u"Telegram mobil ilovadan ishlatilayotgandek koʻrinadi."_q);
```

- [x] **Step 2: Yangi kengaytirilgan bo'limni qo'shish (o'sha joyga)**

```cpp
// ── Spoof Mobile kengaytirilgan bo'lim ──────────────────────────────
addToggle(
    u"spoofMobile"_q,
    u"Mobil qurilma koʻrinishi"_q,
    u"Qurilma turi va nomini quyida sozlang."_q);

// Platform type buttons
const auto platformRow = content->add(
    object_ptr<Ui::RpWidget>(content),
    st::boxRowPadding);
// 4 ta tugma: Android | iOS | Windows | Linux
// Har bir tugma bosilganda:
//   1. CustomSettings::SetInt(u"spoofDeviceType"_q, idx)
//   2. Agar systemVersionInput bo'sh yoki avvalgi preset'dan bo'lsa:
//      systemVersionInput->setText(kPresets[idx].defaultSysVer)
//   3. Agar deviceModelInput bo'sh yoki avvalgi preset'dan bo'lsa:
//      deviceModelInput->setText(kPresets[idx].defaultModel)

const auto deviceModelInput = content->add(
    object_ptr<Ui::InputField>(
        content,
        st::defaultInputField,
        rpl::single(u"Masalan: iPhone 17 Pro"_q),
        CustomSettings::SpoofDeviceModel()),
    st::boxRowPadding);

deviceModelInput->changes() | rpl::start_with_next([=] {
    CustomSettings::SetString(
        u"spoofDeviceModel"_q,
        deviceModelInput->getLastText());
}, deviceModelInput->lifetime());

const auto systemVersionInput = content->add(
    object_ptr<Ui::InputField>(
        content,
        st::defaultInputField,
        rpl::single(u"Masalan: Android 15"_q),
        CustomSettings::SpoofSystemVersion()),
    st::boxRowPadding);

systemVersionInput->changes() | rpl::start_with_next([=] {
    CustomSettings::SetString(
        u"spoofSystemVersion"_q,
        systemVersionInput->getLastText());
}, systemVersionInput->lifetime());

// Eslatma
content->add(
    object_ptr<Ui::FlatLabel>(
        content,
        u"O'zgarishlar qayta ulanishdan keyin kuchga kiradi"_q,
        st::boxDividerLabel),
    st::boxRowPadding);
```

> **Eslatma:** Platform buttons uchun `Ui::SegmentControl` yoki oddiy 4 ta `Ui::RoundButton` ishlatilishi mumkin — mavjud UI pattern'ga qarab tanlansin. Eng oddiy yondashuv: 4 ta kichik `Ui::Checkbox` yoki radio button, yoki `Ui::SegmentControl` agar codebase'da mavjud bo'lsa.

- [x] **Step 3: `custom_settings.h` include mavjudligini tekshirish**

Fayl boshida `#include "custom_settings.h"` borligini tekshirish (odatda bor).

---

## Task 6: Tekshirish va build

- [x] **Step 1: Compile**

```
cmake --build . --target Telegram -j8
```

Kutilgan natija: 0 error.

- [x] **Step 2: Qo'lda test**

1. BekGram'ni ishga tushirish
2. Sozlamalar → Custom Mod → "Mobil qurilma ko'rinishi" yoqish
3. Platform: "iOS" tanlash → avtomatik `systemVersion = "iPhone OS 18.2"` to'ldirilishini tekshirish
4. Ixtiyoriy nom yozish (masalan: "iPhone 17 Pro")
5. BekGram'ni yopib, qayta ochish (yoki MTP reconnect)
6. Boshqa Telegram client'da Devices ro'yxatini ochish
7. Kutilgan: iOS icon + yozilgan nom ko'rinishi

- [x] **Step 3: Turli platform'larni tekshirish**

| Platform | langPack | System Version | Ko'rilishi kerak |
|----------|----------|----------------|-----------------|
| Android  | android  | Android 15     | Yashil robot icon |
| iOS      | ios      | iPhone OS 18.2 | iPhone icon       |
| Windows  | tdesktop | Windows 11     | Windows icon      |
| Linux    | tdesktop | Ubuntu 24.04   | Linux icon        |

---

## Muhim eslatmalar

### `deviceModelToUse` tipi haqida
`session_private.cpp`'da quyidagini tekshirish kerak:
```cpp
auto deviceModelToUse = deviceModel;
```
`deviceModel`'ning tipi nima? Agar `std::string` bo'lsa — `SpoofDeviceModel().toStdString()`. Agar `QString` bo'lsa — to'g'ridan to'g'ri `SpoofDeviceModel()`. Mavjud kod:
```cpp
deviceModelToUse = "Samsung Galaxy S26 Ultra";  // string literal → std::string
```
Demak `std::string`. Shuning uchun `SpoofDeviceModel().toStdString()` va `SpoofLangPack().toStdString()` ishlatilsin.

### langPackName tipi haqida
`session_private.cpp:679`:
```cpp
const auto langPackName = _options->langPackName;
```
`Session::Options::langPackName` — `QString` (session.h:46 da `QString langPackName`). Lekin `MTP_string()` `std::string` qabul qiladi. Mavjud kod:
```cpp
MTP_string(langPackName),
```
`langPackName` QString bo'lsa, `MTP_string` uni implicit convert qilishi mumkin. Tekshirish: agar `const auto` deduce qilsa `QString`, u holda override ham `QString` bo'lishi kerak:
```cpp
const auto langPackName = CustomSettings::SpoofMobile()
    ? CustomSettings::SpoofLangPack()
    : _options->langPackName;
```

### Reconnect
Device name o'zgarishlari faqat keyingi MTP ulanishda kuchga kiradi. UI'da bu haqida eslatma bo'lishi kerak (yuqorida qo'shilgan).

### Windows va Linux iconlari
`langPack = "tdesktop"` bilan Windows va Linux iconlari `system_version` string'iga qarab Telegram tomonidan ajratiladi. Agar faqat `langPack` o'zgartirilsa va `system_version = "Windows 11"` bo'lsa, Windows icon chiqadi. Bu Telegram server-side logikasi — biz faqat to'g'ri qiymatlarni yuborsak bo'ldi.
