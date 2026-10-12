#include "custom_startup_quiet.h"

#include "base/call_delayed.h"
#include "base/debug_log.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace CustomStartupQuiet {
namespace {

// Startdan shuncha vaqt o'tmaguncha signal berilmaydi: start'ning eng og'ir
// qismi (chatlar, xabar ob'ektlari, media) odatda shu ichida tugaydi. O'lchov
// (2026-10-13, 7.3.1, iliq start): ikkala akkaunt chat ro'yxati +4 s va +9 s
// da yuklangan.
constexpr auto kMinSinceStartMs = crl::time(20 * 1000);

// Asosiy oqim shuncha vaqt KETMA-KET tinch bo'lishi kerak.
constexpr auto kCalmMs = crl::time(8 * 1000);

// Kuzatuvchi taymeri (250 ms) shundan ko'p kechiksa, o'sha tik "band".
// Oddiy holatda kechikish Windows taymer aniqligi doirasida (~16 ms).
constexpr auto kBusyLateMs = crl::time(150);

// Yuqori chegara (ro'yxatdan o'tgan paytdan): ilgarigi qat'iy kutish.
constexpr auto kMaxWaitMs = crl::time(90 * 1000);

struct Waiter {
	const char *name = nullptr;
	crl::time registeredAt = 0;
	Fn<void()> callback;
};

// crl::now() jarayon boshidan sanaladi, ya'ni 0 ham haqiqiy qiymat --
// shuning uchun alohida bayroq / optional.
bool gStarted = false;
crl::time gStartedAt = 0;
std::optional<crl::time> gCalmSince; // yo'q -- oxirgi tik band edi
std::vector<Waiter> gWaiters;

void Check(crl::time now) {
	if (gWaiters.empty()) {
		return;
	}
	auto ready = std::vector<Fn<void()>>();
	for (auto i = gWaiters.begin(); i != gWaiters.end();) {
		const auto calmFor = gCalmSince
			? (now - std::max(*gCalmSince, i->registeredAt))
			: crl::time(0);
		const auto calm = (now - gStartedAt >= kMinSinceStartMs)
			&& (calmFor >= kCalmMs);
		const auto waited = now - i->registeredAt;
		if (!calm && waited < kMaxWaitMs) {
			++i;
			continue;
		}
		LOG(("CustomMod Perf: start tinchidi -> %1 (start+%2 ms, "
			"kutildi %3 ms, %4)"
			).arg(i->name
			).arg(now - gStartedAt
			).arg(waited
			).arg(calm ? u"tinch"_q : u"yuqori chegara"_q));
		ready.push_back(std::move(i->callback));
		i = gWaiters.erase(i);
	}
	// Ro'yxatdan chiqarilgandan KEYIN chaqiriladi: callback ichida yangi
	// OnCalm() bo'lsa ham iterator buzilmaydi.
	for (auto &callback : ready) {
		callback();
	}
}

} // namespace

void Start() {
	if (!gStarted) {
		gStarted = true;
		gStartedAt = crl::now();
	}
}

void NoteTick(crl::time late) {
	const auto now = crl::now();
	// Tizim uyqusidan keyingi ulkan kechikish ham "band" -- uyg'onishda
	// tarmoq qayta ulanadi va farqlar yuklanadi.
	if (late >= kBusyLateMs) {
		gCalmSince = std::nullopt;
	} else if (!gCalmSince) {
		gCalmSince = now;
	}
	Check(now);
}

void OnCalm(const char *name, Fn<void()> callback) {
	if (!gStarted) {
		// Kuzatuvchi ishga tushmagan (bo'lmasligi kerak) -- eski qat'iy
		// kutish, ish baribir yo'qolmasin.
		LOG(("CustomMod Perf: start kuzatuvchisi yo'q, %1 qat'iy %2 ms"
			).arg(name
			).arg(kMaxWaitMs));
		base::call_delayed(kMaxWaitMs, std::move(callback));
		return;
	}
	gWaiters.push_back({ name, crl::now(), std::move(callback) });
}

} // namespace CustomStartupQuiet
