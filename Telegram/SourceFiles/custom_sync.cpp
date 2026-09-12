#include "custom_sync.h"

#include <algorithm>

#ifndef SYNC_SELFTEST
#include "custom_sync_client.h"
#include "custom_sync_outbox.h"
#include "custom_settings.h"

#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#endif

namespace CustomSync {

// ─── Sof qaror mantiqi (pure function) ──────────────────────────────────────
// Hech qanday timer, tarmoq yoki ma'lumotlar bazasiga bog'lanmagan.

SchedulerDecision NextAction(const SchedulerState &s) {
    // 1. Sync o'chiq — hech qachon ishga tushmaydi (K5)
    if (!s.enabled) {
        return { false, 0 };
    }

    // 2. Sikl hozir ishlayapti — ustma-ust tushishning oldini olish
    if (s.inFlight) {
        return { false, 0 };
    }

    // 3. Ketma-ket xatolar bo'lsa — eksponensial backoff (2^shift, max 300s)
    if (s.consecutiveFailures > 0) {
        const int base = (s.intervalSeconds > 0) ? s.intervalSeconds : 30;
        const int shift = std::min(s.consecutiveFailures - 1, 30);
        qint64 delay = qint64(base) << shift;
        if (delay > kMaxBackoffSeconds || shift >= 30) {
            delay = kMaxBackoffSeconds;
        }
        return { false, int(delay) };
    }

    // 4. Server yana ma'lumot borligini bildirdi yoki sikl davomida WebSocket
    // orqali xabarnoma keldi va tez sikllar chegarasiga yetilmagan — darhol ishga tushirish (runNow = true)
    if ((s.hasMore || s.pendingNotify) && s.catchUpCycles < kMaxCatchUpCycles) {
        return { true, 0 };
    }

    // 5. Oddiy holat yoki catch-up chegarasiga yetilgan — interval bilan
    return { false, s.intervalSeconds };
}

#ifndef SYNC_SELFTEST

namespace {

static Orchestrator *gOrchestrator = nullptr;

// Start tinchligi: main_session.cpp dagi kStartupQuietMs bilan bir xil
// maqsad -- og'ir start tugagach ishlasin.
constexpr auto kStartupQuietSeconds = 90;

} // namespace

Orchestrator::Orchestrator(QObject *parent)
    : QObject(parent)
    , _timer(new QTimer(this))
{
    _timer->setSingleShot(true);
    connect(_timer, &QTimer::timeout, this, &Orchestrator::runCycle);
}

Orchestrator::~Orchestrator() {
    if (gOrchestrator == this) {
        gOrchestrator = nullptr;
    }
}

void Orchestrator::start() {
    if (!CustomSettings::SyncEnabled()) {
        return;
    }
#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
    // Birinchi start() da _client hali yo'q (u runCycle() da yaratiladi) --
    // o'shanda soketni klientning o'zi token olgach ochadi. Bu shart esa
    // "o'chirish -> yoqish" yo'li uchun: klient allaqachon mavjud va
    // tokeni bor, stop() esa soketni yopib qo'ygan edi.
    if (_client) {
        _client->startWebSocket();
    }
#endif
    // Interval CustomSettings::SyncIntervalSeconds() dan har safar o'qiladi
    SchedulerState state;
    state.enabled = true;
    state.inFlight = _inFlight;
    state.consecutiveFailures = _consecutiveFailures;
    state.hasMore = false;
    state.pendingNotify = _pendingNotify;
    state.catchUpCycles = 0;
    state.intervalSeconds = CustomSettings::SyncIntervalSeconds();

    // 2026-09-12: ilova ishga tushgandan keyingi BIRINCHI sikl kamida 90
    // soniya kechiktiriladi. Standart interval 30 soniya, ya'ni ilgari
    // birinchi sinxronizatsiya aynan startning og'ir qismiga tushardi
    // (outbox o'qish + tarmoq + DB yozuv) va bitta SQLite ulanishi uchun
    // chat yuklash bilan raqobatlashardi. Sinxronizatsiya baribir davriy --
    // bir daqiqalik kechikish xatti-harakatni o'zgartirmaydi. Bayroq faqat
    // birinchi marta ishlaydi: "o'chirish -> yoqish" yo'li darhol armlansin.
    static auto sFirstArmAfterLaunch = true;
    auto decision = NextAction(state);
    if (sFirstArmAfterLaunch) {
        sFirstArmAfterLaunch = false;
        decision.runNow = false;
        decision.delaySeconds = std::max(
            decision.delaySeconds,
            kStartupQuietSeconds);
    }
    arm(decision);
}

void Orchestrator::stop() {
    _timer->stop();
#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
    if (_client) {
        _client->stopWebSocket();
    }
#endif
}

void Orchestrator::syncNow() {
    if (!CustomSettings::SyncEnabled()) {
        return;
    }
    if (_inFlight) {
        return;
    }
    _timer->stop();
    runCycle();
}

SyncStatus Orchestrator::status() const {
    SyncStatus s;
    s.lastSuccessAt = _lastSuccessAt;
    s.pendingCount = Outbox::PendingCount();
    s.lastError = _lastError;
    s.consecutiveFailures = _consecutiveFailures;
    s.inFlight = _inFlight;
    return s;
}

void Orchestrator::arm(const SchedulerDecision &d) {
    if (!CustomSettings::SyncEnabled()) {
        _timer->stop();
        return;
    }
    if (d.runNow) {
        // Darhol siklni boshlash (stak to'lib ketmasligi uchun singleShot(0))
        QTimer::singleShot(0, this, &Orchestrator::runCycle);
        return;
    }
    if (d.delaySeconds > 0) {
        _timer->start(d.delaySeconds * 1000);
    }
}

void Orchestrator::runCycle() {
    if (!CustomSettings::SyncEnabled()) {
        return;
    }
    if (_inFlight) {
        return;
    }

    _inFlight = true;
    const auto cycleId = ++_currentCycleId;
    Q_EMIT statusChanged(status());

    // Watchdog: agar tarmoq callback'i hech qachon kelmasa, _inFlight abadiy
    // qotib qolmasligi uchun himoya taymeri (timeout intervaldan hisoblanadi).
    const int interval = CustomSettings::SyncIntervalSeconds();
    const int watchdogTimeoutMs = std::max(60, interval * 3) * 1000;
    QTimer::singleShot(watchdogTimeoutMs, this, [this, cycleId] {
        if (_inFlight && _currentCycleId == cycleId) {
            qWarning() << "CustomSync: cycle" << cycleId
                       << "watchdog fired; clearing inFlight flag and applying backoff";
            onCycleFinished(0, 0, 0, 0, false, QStringLiteral("cycle_timeout"));
        }
    });

    // Avval push, keyin pull (spec §3.4: observed_at iloji boricha yaqin bo'lishi uchun)
    if (!_client) {
        _client = new Client(this);
#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
        connect(_client, &Client::changesAvailable, this, &Orchestrator::onChangesAvailable);
#endif
    }
    _client->pushPending([this, cycleId](int sent, int failed) {
        if (_currentCycleId != cycleId) {
            return;
        }
        // Push xato bo'lsa ham pull baribir urinib ko'riladi
        _client->pullAndMerge([this, cycleId, sent, failed](
                int merged, int rejected, bool hasMore, QString error) {
            if (_currentCycleId != cycleId) {
                return;
            }
            onCycleFinished(sent, failed, merged, rejected, hasMore, error);
        });
    });
}

#ifdef CUSTOM_SYNC_HAS_WEBSOCKETS
void Orchestrator::onChangesAvailable(qint64 seq) {
    Q_UNUSED(seq);
    if (!CustomSettings::SyncEnabled()) {
        return;
    }
    if (_inFlight) {
        // Sikl davomida kelgan xabarnoma — sikl tugagach NextAction orqali darhol tortiladi
        _pendingNotify = true;
        return;
    }
    // Bo'sh turgan paytda xabarnoma kelsa — darhol siklni boshlash
    syncNow();
}
#endif

void Orchestrator::onCycleFinished(
        int pushed, int pushFailed,
        int merged, int rejected,
        bool hasMore, const QString &error) {
    // _inFlight ni tozalash — BARCHA yo'llar (muvaffaqiyat, xatolik, pull natijasi)
    // aynan shu yerga keladi va bayroq faqat shu yerda tozalanadi.
    _inFlight = false;

    Q_UNUSED(pushed);
    Q_UNUSED(pushFailed);
    Q_UNUSED(rejected);

    const bool success = error.isEmpty() && (merged >= 0);
    const bool pendingNotify = _pendingNotify;
    _pendingNotify = false;

    if (success) {
        _consecutiveFailures = 0;
        _lastError.clear();
        _lastSuccessAt = QDateTime::currentSecsSinceEpoch();
        _hasMore = hasMore;
        if (hasMore || pendingNotify) {
            _catchUpCycles++;
        } else {
            _catchUpCycles = 0;
        }
    } else {
        _consecutiveFailures++;
        _lastError = error;
        _hasMore = false;
        _catchUpCycles = 0;
    }

    Q_EMIT statusChanged(status());

    // K5: sync o'chirilgan bo'lsa timer qurollanmaydi
    if (!CustomSettings::SyncEnabled()) {
        return;
    }

    // Har safar rearm bo'lganda interval qayta o'qiladi
    SchedulerState state;
    state.enabled = true;
    state.inFlight = false;
    state.consecutiveFailures = _consecutiveFailures;
    state.hasMore = _hasMore;
    state.pendingNotify = pendingNotify;
    state.catchUpCycles = _catchUpCycles;
    state.intervalSeconds = CustomSettings::SyncIntervalSeconds();
    arm(NextAction(state));
}

Orchestrator *GetOrchestrator() {
    return gOrchestrator;
}

// ─── Tashqi API ──────────────────────────────────────────────────────────────

void Start() {
    // Ilova miqyosida BITTA orkestrator: ikkinchi chaqiruvda yangisi
    // yaratilmaydi. Ammo darhol qaytib ketish MUMKIN EMAS -- Stop() faqat
    // taymerni to'xtatadi, orkestratorni o'chirmaydi, ya'ni UI'dagi
    // "o'chirish -> yoqish" ketma-ketligidan keyin start() qayta
    // chaqirilmasa taymer hech qachon qurollanmaydi va sync ilova qayta
    // ishga tushirilguncha jimgina o'lik qoladi.
    // start() ning o'zi idempotent: sync o'chiq bo'lsa darhol qaytadi,
    // arm() esa taymerni qayta boshlaydi.
    if (!gOrchestrator) {
        gOrchestrator = new Orchestrator(nullptr);
    }
    gOrchestrator->start();
}

void Stop() {
    if (gOrchestrator) {
        gOrchestrator->stop();
    }
}

void SyncNow() {
    if (gOrchestrator) {
        gOrchestrator->syncNow();
    }
}

SyncStatus CurrentSyncStatus() {
    if (gOrchestrator) {
        return gOrchestrator->status();
    }
    return {};
}

#endif // !SYNC_SELFTEST

} // namespace CustomSync
