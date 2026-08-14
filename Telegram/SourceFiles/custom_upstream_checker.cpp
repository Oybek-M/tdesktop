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

	// A9/Qt6: HTTP/2 ni ATAYIN o'chiramiz.
	//
	// Qt5'da Http2AllowedAttribute standart holda `false` edi, Qt6'da esa
	// `true`. Qt6'ga o'tgach bu tekshiruv "Connection closed"
	// (RemoteHostClosedError) bilan yiqila boshladi — ya'ni TCP+TLS o'rnatildi,
	// so'ng server ulanishni yopdi. Bu TLS backend yo'qligining alomati EMAS
	// (u holda "TLS initialization failed" bo'lardi va build'da OpenSSL
	// `-openssl-linked` bilan ulangan) — bu ALPN orqali kelishilgan h2
	// seansining uzilishi. GitHub API HTTP/1.1 ni to'liq qo'llab-quvvatlaydi,
	// shuning uchun uni majburlash xavfsiz va yo'qotishsiz.
	request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

	// Javob kelmay qolsa QNetworkAccessManager cheksiz kutardi va `manager`
	// hech qachon o'chirilmasdi (finished otilmaydi) — 20 soniyalik chegara.
	request.setTransferTimeout(20 * 1000);

	const auto reply = manager->get(request);

	QObject::connect(reply, &QNetworkReply::finished, [=]() mutable {
		result.checkedAt = QDateTime::currentDateTime();

		if (reply->error() != QNetworkReply::NoError) {
			// Diagnostika: qayta yiqilsa, matnning o'zi sababni ko'rsatsin —
			// xato kodi va (agar javob kelgan bo'lsa) HTTP statusi bilan.
			const auto status = reply->attribute(
				QNetworkRequest::HttpStatusCodeAttribute);
			result.error = reply->errorString()
				+ u" [kod "_q
				+ QString::number(int(reply->error()))
				+ (status.isValid()
					? (u", HTTP "_q + QString::number(status.toInt()))
					: QString())
				+ u"]"_q;
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
