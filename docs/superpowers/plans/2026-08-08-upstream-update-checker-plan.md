# Upstream (rasmiy) versiya tekshiruvchisi Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Custom Window'ga rasmiy `telegramdesktop/tdesktop`da yangi reliz chiqqanini avto/qo'lda tekshiradigan va bildiradigan bo'lim qo'shish — sync/build/reliz jarayonining o'zini emas.

**Architecture:** Yangi `CustomUpstream` namespace (`custom_upstream_checker.h/.cpp`) GitHub'ning ochiq API'siga (`api.github.com/.../releases/latest`) `QNetworkAccessManager` orqali so'rov yuboradi, `tag_name`ni `AppVersionStr` bilan solishtiradi, natijani keshlaydi va `base::Timer` bilan davriy (sozlanadigan interval) auto-tekshiruv qiladi. Sozlamalar mavjud `CustomSettings::Values`ga qo'shiladi (xuddi boshqa CustomMod toggle'lari kabi, `QSettings("CustomMod","TelegramDesktop")` orqali saqlanadi). UI — `custom_mod_window.cpp`ning General tab'iga yangi bo'lim, mavjud `fillActivityHistorySection`/spoof-preset bo'limlaridagi bir xil widget pattern'lari (`Ui::SettingsButton` toggle, `Ui::RoundButton` preset tugmalari, `Ui::SlideWrap` shartli ko'rinish, `Ui::Toast::Show` fikr-mulohaza) bilan.

**Tech Stack:** C++17, Qt 5.15 (QtNetwork, QtCore), tdesktop'ning `rpl`/`base::Timer`/`Ui::*` infratuzilmasi, CMake (qo'lda ro'yxatlangan fayllar).

**Muhim — build siyosati:** Bu loyihada build ~30-60 daqiqa oladi va **har doim ishga tushirishdan oldin foydalanuvchidan ruxsat so'ralishi shart** (og'ir CPU/RAM, boshqa ilovalar bilan raqobatlashadi). Shu sababli bu rejada **faqat bitta build-va-tekshirish bosqichi bor — eng oxirida, Task 5da**. Task 1-4 faqat kod yozadi, build qilmaydi.

---

## Fayl tuzilishi

| Fayl | Maqsad |
|---|---|
| `Telegram/SourceFiles/custom_upstream_checker.h` (yangi) | `CustomUpstream` namespace — public interfeys: `CheckResult`, `Init()`, `UpdateAutoTimer()`, `CheckNow()`, `LastResult()` |
| `Telegram/SourceFiles/custom_upstream_checker.cpp` (yangi) | GitHub API so'rovi, versiya solishtirish, auto-timer, natija keshi |
| `Telegram/SourceFiles/custom_settings.h` (o'zgartiriladi) | `Values`ga 4 ta yangi maydon + getter'lar + 1 ta maxsus setter e'loni |
| `Telegram/SourceFiles/custom_settings.cpp` (o'zgartiriladi) | `Init()` o'qishi, `UpdateValue`/`UpdateInt`/`UpdateString` yangi branch'lari, maxsus setter |
| `Telegram/SourceFiles/custom_mod_window.cpp` (o'zgartiriladi) | Yangi `fillUpstreamCheckSection()` funksiyasi + `fillGeneralTab()`ga chaqiruv |
| `Telegram/SourceFiles/core/application.cpp` (o'zgartiriladi) | Startup'da `CustomUpstream::Init()` chaqiruvi |
| `Telegram/CMakeLists.txt` (o'zgartiriladi) | Yangi 2 faylni build ro'yxatiga qo'shish |

---

### Task 1: `CustomSettings`ga yangi maydonlar

**Files:**
- Modify: `Telegram/SourceFiles/custom_settings.h`
- Modify: `Telegram/SourceFiles/custom_settings.cpp`

- [ ] **Step 1: `Values` struct'iga yangi maydonlarni qo'shish**

`custom_settings.h`da, `Values` struct'ining oxiriga (`activityHistoryTrackAllContacts` maydonidan keyin, struct yopilishidan oldin):

```cpp
    bool activityHistoryTrackAllContacts = true;
    // ── Upstream (rasmiy) versiya tekshiruvchisi ─────────────────────────
    bool upstreamCheckEnabled = true;
    int upstreamCheckIntervalMinutes = 1440;  // standart: kunlik
    QString upstreamLastKnownVersion;         // oxirgi FOYDALANUVCHIGA bildirilgan versiya
    qint64 upstreamLastCheckedAt = 0;         // unix timestamp (soniya)
};
```

- [ ] **Step 2: Getter'larni va maxsus setter e'lonini qo'shish**

Xuddi shu faylda, `inline bool ActivityHistoryTrackAllContacts()` qatoridan keyin (81-qator atrofida):

```cpp
inline bool ActivityHistoryTrackAllContacts() { return Get().activityHistoryTrackAllContacts; }

inline bool    UpstreamCheckEnabled()          { return Get().upstreamCheckEnabled; }
inline int     UpstreamCheckIntervalMinutes()  { return Get().upstreamCheckIntervalMinutes; }
inline QString UpstreamLastKnownVersion()      { return Get().upstreamLastKnownVersion; }
inline qint64  UpstreamLastCheckedAt()         { return Get().upstreamLastCheckedAt; }
// qint64 generic Set/SetInt(int) orqali saqlanolmaydi (32-bit chegara) —
// shuning uchun alohida, per-peer setter'lar kabi maxsus funksiya.
void SetUpstreamLastCheckedAt(qint64 timestamp);
```

- [ ] **Step 3: `Init()`da yangi qiymatlarni o'qish**

`custom_settings.cpp`da, `Init()` ichida `gValues.activityHistoryTrackAllContacts = settings.value(...)` qatoridan keyin (222-223 qatorlar atrofida):

```cpp
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
```

- [ ] **Step 4: `UpdateValue`/`UpdateInt`/`UpdateString`ga yangi branch'lar qo'shish**

`UpdateValue` funksiyasida (159-172 qatorlar), oxirgi `else if` dan keyin:

```cpp
void UpdateValue(const QString &id, bool value) {
    if (id == "ghostMode") gValues.ghostMode = value;
    // ... mavjud branch'lar o'zgarishsiz ...
    else if (id == "activityHistoryTrackAllContacts") gValues.activityHistoryTrackAllContacts = value;
    else if (id == "upstreamCheckEnabled") gValues.upstreamCheckEnabled = value;
}
```

`UpdateInt` funksiyasida (183-185 qatorlar):

```cpp
void UpdateInt(const QString &id, int value) {
    if (id == "spoofDeviceType") gValues.spoofDeviceType = value;
    else if (id == "upstreamCheckIntervalMinutes") gValues.upstreamCheckIntervalMinutes = value;
}
```

`UpdateString` funksiyasida (174-181 qatorlar):

```cpp
void UpdateString(const QString &id, const QString &value) {
    if (id == "spoofDeviceModel") gValues.spoofDeviceModel = value;
    // ... mavjud branch'lar o'zgarishsiz ...
    else if (id == "mutualContactMembersListEmoji") gValues.mutualContactMembersListEmoji = value;
    else if (id == "upstreamLastKnownVersion") gValues.upstreamLastKnownVersion = value;
}
```

- [ ] **Step 5: Maxsus setter'ni implement qilish**

`custom_settings.cpp`da, `SetInt()` funksiyasidan keyin (285-289 qatorlar atrofida):

```cpp
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
```

- [ ] **Step 6: Commit**

```bash
git add Telegram/SourceFiles/custom_settings.h Telegram/SourceFiles/custom_settings.cpp
git commit -m "feat(settings): add upstream update checker settings fields"
```

---

### Task 2: `CustomUpstream` tekshiruv moduli

**Files:**
- Create: `Telegram/SourceFiles/custom_upstream_checker.h`
- Create: `Telegram/SourceFiles/custom_upstream_checker.cpp`

- [ ] **Step 1: Header faylini yozish**

`Telegram/SourceFiles/custom_upstream_checker.h`:

```cpp
#pragma once

#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <functional>

namespace CustomUpstream {

struct CheckResult {
	bool checked = false;      // hech bo'lmasa bir marta muvaffaqiyatli tekshirilganmi
	bool hasNewer = false;     // rasmiy versiya biznikidan yangimi
	QString localVersion;      // masalan "7.0.9" (Core::AppVersionStr)
	QString latestVersion;     // GitHub javobidagi tag_name'dan, "v" prefiksisiz
	QString releaseUrl;        // GitHub release sahifasi (html_url)
	QDateTime checkedAt;
	QString error;             // bo'sh emas bo'lsa — tarmoq/parse xatosi
};

// Ilova ishga tushganda 1 marta chaqiriladi (core/application.cpp'dan).
void Init();

// Sozlamalar o'zgarganda (auto-check yoq/o'chir, interval) qayta chaqiriladi —
// eski timer'ni to'xtatib, kerak bo'lsa yangisini boshlaydi.
void UpdateAutoTimer();

// Qo'lda ("Hozir tekshirish" tugmasi) yoki auto-timer orqali chaqiriladi.
// Tarmoq so'rovi asinxron — natija callback orqali qaytadi (UI thread'da).
void CheckNow(std::function<void(CheckResult)> callback = nullptr);

// Custom Window ochilganda darhol ko'rsatish uchun keshlangan oxirgi natija
// (tarmoq so'rovisiz — bo'sh CheckResult{} agar hali hech qachon
// tekshirilmagan bo'lsa).
CheckResult LastResult();

} // namespace CustomUpstream
```

- [ ] **Step 2: Implementatsiya faylini yozish**

`Telegram/SourceFiles/custom_upstream_checker.cpp`:

```cpp
#include "custom_upstream_checker.h"

#include "custom_settings.h"
#include "core/version.h"
#include "base/timer.h"
#include "ui/toast/toast.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <algorithm>

namespace CustomUpstream {
namespace {

constexpr auto kGithubLatestReleaseUrl
	= "https://api.github.com/repos/telegramdesktop/tdesktop/releases/latest";
constexpr auto kMinIntervalMinutes = 15;

CheckResult gLastResult;
std::unique_ptr<base::Timer> gAutoTimer;

int VersionCode(int major, int minor, int patch) {
	return major * 1000000 + minor * 1000 + patch;
}

// "v7.0.9" yoki "7.0.9" -> 7000009. Mos kelmasa std::nullopt.
std::optional<int> ParseVersionCode(const QString &raw) {
	auto text = raw;
	if (text.startsWith(u"v"_q)) {
		text = text.mid(1);
	}
	static const auto expr = QRegularExpression(u"^(\\d+)\\.(\\d+)\\.(\\d+)"_q);
	const auto match = expr.match(text);
	if (!match.hasMatch()) {
		return std::nullopt;
	}
	return VersionCode(
		match.captured(1).toInt(),
		match.captured(2).toInt(),
		match.captured(3).toInt());
}

void RunCheck(std::function<void(CheckResult)> callback, bool notifyIfNewer) {
	auto result = CheckResult();
	result.localVersion = QString::fromUtf8(AppVersionStr);

	const auto manager = new QNetworkAccessManager();
	auto request = QNetworkRequest(
		QUrl(QString::fromUtf8(kGithubLatestReleaseUrl)));
	request.setRawHeader("User-Agent", "CustomMod-tdesktop-UpstreamChecker");
	const auto reply = manager->get(request);

	QObject::connect(reply, &QNetworkReply::finished, [=]() mutable {
		result.checkedAt = QDateTime::currentDateTime();

		if (reply->error() != QNetworkReply::NoError) {
			result.error = reply->errorString();
		} else {
			const auto data = reply->readAll();
			const auto obj = QJsonDocument::fromJson(data).object();
			const auto tag = obj.value(u"tag_name"_q).toString();
			const auto url = obj.value(u"html_url"_q).toString();
			const auto latestCode = ParseVersionCode(tag);
			const auto localCode = ParseVersionCode(result.localVersion);

			if (tag.isEmpty() || !latestCode || !localCode) {
				result.error = u"GitHub javobini tahlil qilib bo'lmadi"_q;
			} else {
				result.checked = true;
				result.latestVersion = tag.startsWith(u"v"_q)
					? tag.mid(1)
					: tag;
				result.releaseUrl = url;
				result.hasNewer = (*latestCode > *localCode);

				CustomSettings::SetUpstreamLastCheckedAt(
					result.checkedAt.toSecsSinceEpoch());

				if (result.hasNewer && notifyIfNewer) {
					const auto known = CustomSettings::UpstreamLastKnownVersion();
					if (known != result.latestVersion) {
						CustomSettings::SetString(
							u"upstreamLastKnownVersion"_q,
							result.latestVersion);
						Ui::Toast::Show(
							u"Rasmiy Telegram Desktop "_q
								+ result.latestVersion
								+ u" chiqdi (siz "_q
								+ result.localVersion
								+ u"'dasiz)"_q);
					}
				}
			}
		}

		gLastResult = result;
		reply->deleteLater();
		manager->deleteLater();
		if (callback) {
			callback(result);
		}
	});
}

} // namespace

void UpdateAutoTimer() {
	gAutoTimer = nullptr;
	if (!CustomSettings::UpstreamCheckEnabled()) {
		return;
	}
	const auto minutes = std::max(
		CustomSettings::UpstreamCheckIntervalMinutes(),
		kMinIntervalMinutes);
	gAutoTimer = std::make_unique<base::Timer>([] {
		RunCheck(nullptr, true);
	});
	gAutoTimer->callEach(crl::time(minutes) * 60 * 1000);
}

void Init() {
	UpdateAutoTimer();
}

void CheckNow(std::function<void(CheckResult)> callback) {
	RunCheck(std::move(callback), false);
}

CheckResult LastResult() {
	return gLastResult;
}

} // namespace CustomUpstream
```

- [ ] **Step 3: Commit**

```bash
git add Telegram/SourceFiles/custom_upstream_checker.h Telegram/SourceFiles/custom_upstream_checker.cpp
git commit -m "feat(upstream): add GitHub-based official release checker module"
```

---

### Task 3: Startup hook + CMake ro'yxatga olish

**Files:**
- Modify: `Telegram/SourceFiles/core/application.cpp`
- Modify: `Telegram/CMakeLists.txt`

- [ ] **Step 1: `application.cpp`ga include qo'shish**

`Telegram/SourceFiles/core/application.cpp`da, 11-qatordagi `#include "custom_settings.h"` dan keyin:

```cpp
#include "custom_branding.h"
#include "custom_db.h"
#include "custom_settings.h"
#include "custom_upstream_checker.h"
```

- [ ] **Step 2: `Application::run()`da chaqiruv qo'shish**

Xuddi shu faylda, 274-277 qatorlar atrofida:

```cpp
void Application::run() {
	CustomDB::Init();
	CustomSettings::Init();
	CustomUpstream::Init();
	CustomBranding::Load();
```

- [ ] **Step 3: `CMakeLists.txt`ga yangi fayllarni qo'shish**

`Telegram/CMakeLists.txt`da, 1103-1104 qatorlar (`custom_settings.cpp` / `custom_settings.h`) dan keyin:

```
    custom_settings.cpp
    custom_settings.h
    custom_upstream_checker.cpp
    custom_upstream_checker.h
    custom_activity_history.cpp
```

- [ ] **Step 4: Commit**

```bash
git add Telegram/SourceFiles/core/application.cpp Telegram/CMakeLists.txt
git commit -m "feat(upstream): wire checker into app startup and build"
```

---

### Task 4: Custom Window UI bo'limi

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp`

- [x] **Step 1: `#include` qo'shish**

Fayl boshidagi `#include "ui/toast/toast.h"` qatoridan keyin:

```cpp
#include "ui/toast/toast.h"
#include "custom_upstream_checker.h"
```

(Agar `<QtGui/QDesktopServices>` va `<QtCore/QUrl>` hali include qilinmagan bo'lsa, ularni ham shu yerga qo'shing — "GitHub'da ko'rish" tugmasi uchun kerak.)

- [x] **Step 2: Forward declaration qo'shish**

280-283 qatorlar atrofidagi `fillActivityHistorySection` e'lonidan keyin:

```cpp
void fillActivityHistorySection(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	Fn<void()> onRebuild);
void fillUpstreamCheckSection(not_null<Ui::VerticalLayout*> content);
```

- [x] **Step 3: `fillUpstreamCheckSection` funksiyasini yozish**

> **Amalda kiritilgan qo'shimcha tuzatish (code-quality review, 2026-08-08):** `CheckNow(applyResult)` chaqiruvi `CheckNow(crl::guard(content, applyResult))`ga o'zgartirildi — asinxron callback Custom Window yopilgandan keyin kelsa, `linkWrap`/`freqWrap`ga dangling pointer orqali murojaat qilish xavfi (use-after-free) shu orqali oldi olindi. Shuningdek `#include <algorithm>` aniq qo'shildi (`std::max` uchun). Commit `d724ac4792`.

`fillGeneralTab` funksiyasidan OLDIN (masalan 480-qator atrofida, `fillGeneralTab`ning ta'rifidan oldin) yangi funksiya qo'shiladi:

```cpp
void fillUpstreamCheckSection(not_null<Ui::VerticalLayout*> content) {
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"🔔 Rasmiy versiya tekshiruvi"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	{
		const auto desc = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(u"Rasmiy telegramdesktop/tdesktop'da yangi "
					"reliz chiqqanini tekshiradi va bildiradi. Hech qanday "
					"avtomatik yangilash yoki build qilmaydi — faqat "
					"xabardor qiladi."_q),
				st::customModHintLabel),
			st::boxRowPadding,
			style::al_justify);
		content->widthValue() | rpl::on_next([=](int w) {
			const auto lw = w
				- st::boxRowPadding.left()
				- st::boxRowPadding.right();
			if (lw > 0) {
				desc->resizeToWidth(lw);
				desc->update();
			}
		}, desc->lifetime());
	}

	Ui::AddSkip(content, 8);

	// ── Status qatori ────────────────────────────────────────────────
	auto statusText = std::make_shared<rpl::variable<QString>>(
		u"Hali tekshirilmagan"_q);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			statusText->value(),
			st::boxLabel),
		st::boxRowPadding);

	// ── GitHub'da ko'rish tugmasi (faqat yangilanish bo'lsa ko'rinadi) ──
	const auto linkWrap = content->add(
		object_ptr<Ui::SlideWrap<Ui::RoundButton>>(
			content,
			object_ptr<Ui::RoundButton>(
				content,
				rpl::single(u"🔗 GitHub'da ko'rish"_q),
				st::defaultBoxButton)),
		st::boxRowPadding);
	const auto releaseUrl = std::make_shared<QString>();
	linkWrap->entity()->addClickHandler([=] {
		if (!releaseUrl->isEmpty()) {
			QDesktopServices::openUrl(QUrl(*releaseUrl));
		}
	});
	linkWrap->toggle(false, anim::type::instant);

	const auto applyResult = [=](CustomUpstream::CheckResult r) {
		if (!r.checked) {
			*statusText = r.error.isEmpty()
				? u"Hali tekshirilmagan"_q
				: (u"Tekshirib bo'lmadi: "_q + r.error);
			linkWrap->toggle(false, anim::type::normal);
			return;
		}
		const auto base = u"Siz asoslangan: "_q + r.localVersion
			+ u"  |  Rasmiy so'nggi: "_q + r.latestVersion;
		*statusText = r.hasNewer
			? (base + u"  —  YANGILANISH BOR!"_q)
			: base;
		*releaseUrl = r.releaseUrl;
		linkWrap->toggle(r.hasNewer, anim::type::normal);
	};
	applyResult(CustomUpstream::LastResult());

	// ── "Hozir tekshirish" tugmasi ──────────────────────────────────
	Ui::AddSkip(content, 4);
	content->add(
		object_ptr<Ui::RoundButton>(
			content,
			rpl::single(u"🔄 Hozir tekshirish"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		*statusText = u"Tekshirilmoqda..."_q;
		CustomUpstream::CheckNow(applyResult);
	});

	// ── Avtomatik tekshirish toggle ───────────────────────────────────
	Ui::AddSkip(content, 12);
	const auto autoBtn = content->add(
		object_ptr<Ui::SettingsButton>(
			content,
			rpl::single(u"Avtomatik tekshirish"_q),
			st::settingsButtonNoIcon));
	autoBtn->toggleOn(rpl::single(CustomSettings::UpstreamCheckEnabled()));

	// ── Chastota tanlovi (auto yoqilgandagina ko'rinadi) ──────────────
	const auto freqWrap = content->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			content,
			object_ptr<Ui::VerticalLayout>(content)),
		style::margins(0, 0, 0, 0));
	const auto freqForm = freqWrap->entity();

	freqForm->add(
		object_ptr<Ui::FlatLabel>(
			freqForm,
			rpl::single(u"Chastota:"_q),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	struct Preset { QString label; int minutes; };
	const Preset kPresets[3] = {
		{ u"Soatlik"_q, 60 },
		{ u"Kunlik"_q, 1440 },
		{ u"Haftalik"_q, 10080 },
	};
	const auto isPreset = [](int minutes) {
		return minutes == 60 || minutes == 1440 || minutes == 10080;
	};

	const auto customWrap = freqForm->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			freqForm,
			object_ptr<Ui::VerticalLayout>(freqForm)),
		style::margins(0, 0, 0, 0));
	const auto customForm = customWrap->entity();
	const auto customInput = customForm->add(
		object_ptr<Ui::InputField>(
			customForm,
			st::defaultInputField,
			rpl::single(u"Daqiqada (min 15)"_q),
			QString::number(CustomSettings::UpstreamCheckIntervalMinutes())),
		st::boxRowPadding);
	customForm->add(
		object_ptr<Ui::RoundButton>(
			customForm,
			rpl::single(u"💾 Saqlash"_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		bool ok = false;
		const auto parsed = customInput->getLastText().trimmed().toInt(&ok);
		const auto clamped = std::max(ok ? parsed : 1440, 15);
		CustomSettings::SetInt(u"upstreamCheckIntervalMinutes"_q, clamped);
		CustomUpstream::UpdateAutoTimer();
		Ui::Toast::Show(
			u"Chastota saqlandi: "_q + QString::number(clamped) + u" daqiqa"_q);
	});
	customWrap->toggle(
		!isPreset(CustomSettings::UpstreamCheckIntervalMinutes()),
		anim::type::instant);

	for (const auto &preset : kPresets) {
		const auto minutes = preset.minutes;
		freqForm->add(
			object_ptr<Ui::RoundButton>(
				freqForm,
				rpl::single(preset.label),
				st::defaultBoxButton),
			st::boxRowPadding)
		->addClickHandler([=] {
			CustomSettings::SetInt(u"upstreamCheckIntervalMinutes"_q, minutes);
			CustomUpstream::UpdateAutoTimer();
			customWrap->toggle(false, anim::type::normal);
			Ui::Toast::Show(u"Chastota saqlandi."_q);
		});
	}
	freqForm->add(
		object_ptr<Ui::RoundButton>(
			freqForm,
			rpl::single(u"Boshqa..."_q),
			st::defaultBoxButton),
		st::boxRowPadding)
	->addClickHandler([=] {
		customWrap->toggle(true, anim::type::normal);
	});

	freqWrap->toggle(CustomSettings::UpstreamCheckEnabled(), anim::type::instant);

	autoBtn->toggledValue()
		| rpl::skip(1)
		| rpl::on_next([=](bool on) {
			CustomSettings::Set(u"upstreamCheckEnabled"_q, on);
			CustomUpstream::UpdateAutoTimer();
			freqWrap->toggle(on, anim::type::normal);
			Ui::Toast::Show(on
				? u"Avtomatik tekshirish yoqildi"_q
				: u"Avtomatik tekshirish o'chirildi"_q);
		}, autoBtn->lifetime());

	// ── Oxirgi tekshiruv vaqti ──────────────────────────────────────
	Ui::AddSkip(content, 8);
	{
		const auto lastAt = CustomSettings::UpstreamLastCheckedAt();
		const auto text = (lastAt > 0)
			? (u"Oxirgi tekshiruv: "_q
				+ QDateTime::fromSecsSinceEpoch(lastAt)
					.toString(u"yyyy-MM-dd HH:mm"_q))
			: u"Oxirgi tekshiruv: hali yo'q"_q;
		content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(text),
				st::customModHintLabel),
			st::boxRowPadding);
	}
}
```

- [x] **Step 4: `fillGeneralTab`ga chaqiruvni ulash**

`fillGeneralTab` funksiyasining oxirida (927-928 qatorlar atrofida, "Saqlash" tugmasi bloki tugagandan keyin, funksiya yopilishidan oldin):

```cpp
	->addClickHandler([=] {
		CustomBranding::SetWindowTitle(titleInput->getLastText().trimmed());
		CustomBranding::SetCustomModTitle(modInput->getLastText().trimmed());
		CustomBranding::SetIconPath(iconInput->getLastText().trimmed());
		Ui::Toast::Show(u"Saqlandi! O'zgartirishlar uchun dasturni qayta yoqing."_q);
	});

	Ui::AddSkip(content, st::settingsThumbSkip);
	Ui::AddDivider(content);
	Ui::AddSkip(content, st::settingsThumbSkip);
	fillUpstreamCheckSection(content);

	Ui::AddSkip(content, st::settingsThumbSkip);
}
```

- [x] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "feat(upstream): add Custom Window UI section for upstream checker"
```

✅ Bajarildi va push qilindi: commit `6119f121b3` (implementatsiya) + `d724ac4792` (code-quality review'dagi CRITICAL/IMPORTANT topilmalarni tuzatish). Spec compliance ✅ va code quality ✅ ikkalasi ham tasdiqlangan.

---

### Task 5: Build va qo'lda tekshiruv (bitta build, oxirida)

**Files:** yo'q (faqat build + qo'lda sinov)

- [ ] **Step 1: Build uchun ruxsat so'rash**

Build boshlashdan oldin foydalanuvchidan **aniq ruxsat so'rang** (loyihaning qat'iy qoidasi). Ruxsat berilgach:

```powershell
cmake --build out --config Release --target Telegram
```

- [ ] **Step 2: CMake xatolarini tuzatish (agar bo'lsa)**

Eng ehtimoliy xato manbalari:
- `QtNetwork` linklanmagan bo'lsa — `Telegram/CMakeLists.txt`da boshqa `#include <QtNetwork/...>` ishlatuvchi fayllar (masalan `core/update_checker.cpp`) allaqachon shu targetga tegishli bo'lgani uchun qo'shimcha `target_link_libraries` shart emas; agar linker xatosi chiqsa, `update_checker.cpp` qaysi CMake target ostida ekanini tekshiring va `custom_upstream_checker.cpp` xuddi shu target ichida ekanini tasdiqlang.
- Xato matnini o'qing, aniq qatorga qarang — taxmin qilmasdan tuzating.

- [ ] **Step 3: Ilovani ishga tushirish va qo'lda tekshirish**

```powershell
& "out\Release\Telegram.exe" -debug
```

Tekshirish ro'yxati:
1. Custom Window → General tab ochilganda, "🔔 Rasmiy versiya tekshiruvi" bo'limi ko'rinadimi — status "Hali tekshirilmagan" deb ko'rsatadimi?
2. "🔄 Hozir tekshirish" bosilganda — status "Tekshirilmoqda..." ga o'zgaradimi, so'ng bir necha soniyada haqiqiy natijaga (masalan "Siz asoslangan: 7.0.9 | Rasmiy so'nggi: 7.0.9") almashadimi?
3. Agar rasmiyda yangi versiya bo'lsa — status "YANGILANISH BOR!" ko'rsatib, "🔗 GitHub'da ko'rish" tugmasi paydo bo'ladimi va bosilganda brauzerda to'g'ri release sahifasi ochiladimi?
4. "Avtomatik tekshirish" toggle'ni o'chirib-yoqib ko'ring — "Chastota" bo'limi mos ravishda yashirinib/ko'rinib turadimi?
5. "Boshqa..." tugmasini bosib, "5" kiritib "Saqlash" bosing — Toast "Chastota saqlandi: 5 daqiqa" chiqishi, lekin haqiqiy qiymat 15 daqiqaga cheklanishi kerak (min 15 qoidasi) — `Get-Content "$env:APPDATA\..."` orqali emas, shunchaki UI xatti-harakatidan kuzating (yoki 5 daqiqadan keyin auto-check qayta ishga tushishini kutib ko'ring — amalda esa 15 daqiqada ishlaydi).
6. Dasturni yoping va qayta oching — "Oxirgi tekshiruv" vaqti va oldingi status saqlanib qolganmi (QSettings orqali persist bo'lganini tasdiqlaydi)?
7. Internetni vaqtincha o'chirib "Hozir tekshirish" bosing — "Tekshirib bo'lmadi: ..." xabari chiqib, dastur qulamayotganini tasdiqlang.

- [ ] **Step 4: Yakuniy commit**

```bash
git add -A
git commit -m "test(upstream): manual verification of upstream update checker feature"
```

(Agar Step 1-4 davomida tuzatishlar kiritilgan bo'lsa, ularni ham shu yoki alohida commit'larga kiriting.)

---

## O'z-o'zini ko'rib chiqish (self-review)

**Spec qamrovi:**
- §2 (modul interfeysi) → Task 2 ✅
- §2.1 (versiya solishtirish) → Task 2, `ParseVersionCode`/`VersionCode` ✅
- §2.2 (xato boshqaruvi) → Task 2, `RunCheck`dagi `result.error` yo'li; Task 4 Step 3 "Tekshirib bo'lmadi" holati ✅
- §3 (sozlamalar) → Task 1 ✅
- §4 (UI — status, tugmalar, toggle, chastota, oxirgi tekshiruv) → Task 4 ✅
- §5 (bildirishnoma) → Task 2, `RunCheck`dagi `notifyIfNewer` yo'li (qo'lda tekshiruvda `false`, auto'da `true`, va faqat `upstreamLastKnownVersion`dan farq qilganda) ✅
- §6 (chegaralar) → sync/build/publish avtomatlashtirilmagan, kod hech qayerda git/build buyruqlarini chaqirmaydi ✅

**Moslik izohi (spec'dan bitta ongli chetlanish):** Spec §5'da "mavjud Windows toast-bildirishnoma mexanizmi" deyilgan edi, lekin tekshiruv shuni ko'rsatdiki — rasmiy tdesktop'ning o'zi ham real update-ready holatini faqat **in-app** (Settings > Advanced ichida) ko'rsatadi, alohida Windows OS toast ishlatmaydi (`settings_advanced.cpp`, `State::Ready`). Shu sababli bu rejada ham mavjud, allaqachon ishlatilayotgan `Ui::Toast::Show()` (in-app toast, `custom_mod_window.cpp`da activity-history toggle'i kabi joylarda hozir ham ishlatiladi) tanlandi — bu ham spec'ning asosiy maqsadini ("GitHub'ni qo'lda kuzatmasdan bildirish") to'liq bajaradi, lekin yangi, sinalmagan OS-darajasidagi notification kodini yozish xavfini oldini oladi.

**Placeholder skanerlash:** Yo'q — har bir qadamda to'liq kod bor, "TODO"/"keyinroq qo'shiladi" yo'q.

**Tur mosligi:** `CheckResult`, `CustomUpstream::*` funksiya imzolari Task 2 (header) va Task 4 (chaqiruvlar)da bir xil; `CustomSettings::Upstream*()` getter'lari Task 1 va Task 2/4'dagi chaqiruvlarda bir xil nomlanishda.
