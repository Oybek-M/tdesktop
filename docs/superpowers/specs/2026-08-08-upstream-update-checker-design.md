# Upstream (rasmiy) versiya tekshiruvchisi — Design

**Sana:** 2026-08-08
**Qamrov:** CustomMod client'ning o'zi rasmiy `telegramdesktop/tdesktop`da yangi reliz chiqqanini avtomatik/qo'lda tekshirib, foydalanuvchiga bildirishi.
**Maqsad:** Foydalanuvchi asosan faqat CustomMod client'ni ishlatadi va rasmiy GitHub'ni qo'lda kuzatib bora olmaydi — bu funksiya "sync qilish vaqti keldimi?" degan savolga javob beradi, sync/build/reliz jarayonining o'zini emas.

---

## 1. Umumiy tamoyillar

- Bu funksiya faqat **bildiradi** — hech qanday avtomatik sync/build/reliz qilmaydi (qasddan chegaralangan qamrov, YAGNI). Yangi rasmiy versiya topilgach, sync/build/reliz jarayoni hozirgacha bo'lgani kabi qo'lda (yoki kelajakda alohida "bitta tugma" funksiyasi bilan) boshlanadi.
- Solishtirish manbai — GitHub'ning ochiq API'si (`api.github.com/repos/telegramdesktop/tdesktop/releases/latest`), to'g'ridan-to'g'ri client'dan, autentifikatsiyasiz (60 so'rov/soat limit yetarli, kamdan-kam tekshiramiz).
- Kod konventsiyasi — mavjud `CustomSettings`/`CustomDB` namespace + erkin funksiyalar uslubiga mos (class emas).
- Tarmoq qatlami — `core/update_checker.cpp`da allaqachon ishlatilayotgan `QNetworkAccessManager`/`QNetworkRequest` pattern'ining kichik, mustaqil nusxasi (mavjud update-checker infratuzilmasiga bog'liq emas, faqat uslubi bir xil).

---

## 2. Yangi modul: `core/upstream_update_checker.h` / `.cpp`

```cpp
namespace CustomUpstream {

struct CheckResult {
	bool checked = false;      // hech bo'lmasa bir marta tekshirilganmi (ilova ishga tushgandan beri)
	bool hasNewer = false;     // rasmiy versiya biznikidan yangimi
	QString localVersion;      // masalan "7.0.9" (Core::App().settings() emas, AppVersionStr)
	QString latestVersion;     // GitHub javobidagi tag_name'dan olingan, masalan "7.0.9"
	QString releaseUrl;        // GitHub release sahifasi (html_url)
	QDateTime checkedAt;
	QString error;             // bo'sh emas bo'lsa — tarmoq/parse xatosi (masalan "internet yo'q")
};

// Ilova ishga tushganda 1 marta chaqiriladi (Core::Application startup'da,
// CustomDB::Init() kabi boshqa Init() chaqiruvlari qatorida). Sozlamalarda
// yoqilgan bo'lsa (CustomSettings::UpstreamCheckEnabled()), auto-timer'ni
// CustomSettings::UpstreamCheckIntervalMinutes() asosida boshlaydi.
void Init();

// Qo'lda ("Hozir tekshirish" tugmasi) yoki auto-timer orqali chaqiriladi.
// Tarmoq so'rovi asinxron — natija callback orqali qaytadi (UI thread'da).
void CheckNow(std::function<void(CheckResult)> callback = nullptr);

// Custom Window ochilganda darhol ko'rsatish uchun keshlangan oxirgi natija
// (tarmoq so'rovisiz — bo'sh CheckResult{} agar hali hech qachon
// tekshirilmagan bo'lsa).
CheckResult LastResult();

} // namespace CustomUpstream
```

### 2.1 Versiya solishtirish

`GET https://api.github.com/repos/telegramdesktop/tdesktop/releases/latest` javobidagi `tag_name` (masalan `"v7.0.9"`) dan raqamli qismlar (`major.minor.patch`) regex bilan ajratiladi (boshidagi `"v"` harfi bo'lsa olib tashlanadi). So'ng `Telegram/SourceFiles/core/version.h`dagi `AppVersion` allaqachon ishlatayotgan formula bilan solishtiriladi: `major*1000000 + minor*1000 + patch`. Bizning fork'ning versiyasi (`AppVersion`) sync jarayoni tufayli har doim upstream tag'lariga mos qilib qo'yiladi — shuning uchun alohida "asos versiya" belgisi kerak emas, `AppVersionStr`ning o'zi to'g'ridan-to'g'ri taqqoslash asosi bo'ladi.

GitHub'ning `/releases/latest` endpoint'i draft va pre-release'larni avtomatik chetlab o'tadi (qo'shimcha filtrlash kerak emas).

### 2.2 Xatolarni boshqarish

Tarmoq so'rovi muvaffaqiyatsiz bo'lsa (internet yo'q, GitHub mavjud emas, 403/429 va h.k.) — `CheckResult.error` to'ldiriladi, `checked = false` qoladi, UI'da "tekshirib bo'lmadi" holatini ko'rsatadi. Hech qanday xato oynasi/bildirishnoma chiqmaydi (jim xato, chunki bu fon jarayoni — asosiy update-checker'ning shovqin qilmaslik falsafasiga mos).

---

## 3. Sozlamalar (`custom_settings.h` / `.cpp`ga qo'shimcha)

`Values` struct'iga yangi maydonlar:

```cpp
bool upstreamCheckEnabled = true;
int upstreamCheckIntervalMinutes = 1440;  // standart: kunlik (24*60)
QString upstreamLastKnownVersion;          // oxirgi marta FOYDALANUVCHIGA bildirilgan versiya
qint64 upstreamLastCheckedAt = 0;          // unix timestamp
```

Mos helper funksiyalar, mavjud toggle'lar bilan bir xil uslubda:

```cpp
inline bool UpstreamCheckEnabled() { return Get().upstreamCheckEnabled; }
inline int  UpstreamCheckIntervalMinutes() { return Get().upstreamCheckIntervalMinutes; }
void SetUpstreamCheckEnabled(bool enabled);
void SetUpstreamCheckIntervalMinutes(int minutes);
```

`upstreamLastKnownVersion` — takroriy bildirishnoma chiqmasligi uchun: bir xil rasmiy versiya haqida faqat **bir marta** toast chiqadi (birinchi topilganda), keyingi avto-tekshiruvlar xuddi shu versiyani qayta-qayta bildirmaydi (Custom Window'dagi status baribir doimiy ko'rinib turadi).

---

## 4. UI — Custom Window, General tab, yangi bo'lim

`custom_mod_window.cpp`ga (mavjud bo'limlar qatoriga) yangi bo'lim qo'shiladi:

- **Doimiy status qatori:** `"Siz asoslangan: 7.0.9  |  Rasmiy so'nggi: 7.0.9"` — `CustomUpstream::LastResult()`dan to'ldiriladi. Holatlar:
  - Hali tekshirilmagan → `"Rasmiy so'nggi: — (hali tekshirilmagan)"`.
  - Tekshirilmoqda → `"tekshirilmoqda..."`.
  - Tekshirib bo'lmadi → `"tekshirib bo'lmadi (internet yo'qmi?)"`.
  - Yangilanish yo'q → oddiy matn.
  - **Yangilanish bor** → qator ajralib turadi (rang/belgi orqali) + **"GitHub'da ko'rish"** tugmasi (`releaseUrl`ni standart brauzerda ochadi, mavjud `UrlClickHandler::Open()` pattern'i orqali).
- **"Hozir tekshirish"** tugmasi — bosilganda `CustomUpstream::CheckNow(...)` chaqiradi, natija kelgach status qatori yangilanadi.
- **"Avtomatik tekshirish"** toggle — `CustomSettings::UpstreamCheckEnabled()`ga bog'langan, o'zgarganda `CustomUpstream`ning ichki timer'i qayta ishga tushadi/to'xtaydi.
- **Chastota tanlovi** (dropdown, faqat auto-tekshirish yoqilganda ko'rinadi): **Soatlik** (60) / **Kunlik** (1440, standart) / **Haftalik** (10080) / **Boshqa...** — oxirgisi tanlansa, daqiqada qiymat kiritiladigan raqamli maydon (`QLineEdit` + validator, min 15 daqiqa — GitHub API'ni haddan tashqari tez-tez urmaslik uchun pastki chegara) paydo bo'ladi.
- **"Oxirgi tekshiruv: <sana/vaqt>"** — kichik, xira matn, `upstreamLastCheckedAt`dan.

---

## 5. Bildirishnoma (toast)

Auto-tekshiruv yangi versiya topsa **va** bu versiya `upstreamLastKnownVersion`dan farq qilsa: mavjud Windows toast-bildirishnoma mexanizmi orqali (rasmiy Telegram'ning "yangilanish tayyor" bildirishnomasi ishlatgan yo'l bilan bir xil infratuzilma) qisqa xabar chiqadi — masalan "Rasmiy Telegram Desktop 7.1.0 chiqdi (siz 7.0.9'dasiz)". Bosilsa, Custom Window'ning tegishli bo'limini ochadi (yoki to'g'ridan-to'g'ri GitHub'ni).

Qo'lda tekshiruvda ("Hozir tekshirish" tugmasi) — toast chiqmaydi, faqat status qatori yangilanadi (foydalanuvchi allaqachon ekranda, alohida bildirishnoma keraksiz).

---

## 6. Chegaralar (qasddan qamrovga kiritilmagan)

- Sync/build/reliz jarayonini avtomatlashtirish — **bu spec doirasida emas** (foydalanuvchi bilan muhokama qilindi: build resurs-qoidasi, merge-konflikt qarorlari va test bosqichi hali inson ishtirokini talab qiladi). Kelajakda alohida "bitta tugma bilan sync+build+publish" funksiyasi sifatida ko'rib chiqilishi mumkin.
- Linux/macOS uchun bu funksiya kerak emas (hozircha faqat Windows client faol ishlatiladi).
- Beta/pre-release rasmiy versiyalarni kuzatish — kiritilmagan (`/releases/latest` ularni allaqachon chetlab o'tadi).
