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
