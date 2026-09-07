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

    // 4. Server yana ma'lumot borligini bildirdi va tez sikllar chegarasiga
    // yetilmagan — darhol ishga tushirish (runNow = true)
    if (s.hasMore && s.catchUpCycles < kMaxCatchUpCycles) {
        return { true, 0 };
    }

    // 5. Oddiy holat yoki catch-up chegarasiga yetilgan — interval bilan
    return { false, s.intervalSeconds };
}

#ifndef SYNC_SELFTEST

namespace {

static Orchestrator *gOrchestrator = nullptr;

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
    // Interval CustomSettings::SyncIntervalSeconds() dan har safar o'qiladi
    SchedulerState state;
    state.enabled = true;
    state.inFlight = _inFlight;
    state.consecutiveFailures = _consecutiveFailures;
    state.hasMore = false;
    state.catchUpCycles = 0;
    state.intervalSeconds = CustomSettings::SyncIntervalSeconds();
    arm(NextAction(state));
}

void Orchestrator::stop() {
    _timer->stop();
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
    Q_EMIT statusChanged(status());

    // Avval push, keyin pull (spec §3.4: observed_at iloji boricha yaqin bo'lishi uchun)
    auto *client = new Client(this);
    client->pushPending([this, client](int sent, int failed) {
        // Push xato bo'lsa ham pull baribir urinib ko'riladi
        client->pullAndMerge([this, client, sent, failed](
                int merged, int rejected, bool hasMore, QString error) {
            client->deleteLater();
            onCycleFinished(sent, failed, merged, rejected, hasMore, error);
        });
    });
}

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

    if (success) {
        _consecutiveFailures = 0;
        _lastError.clear();
        _lastSuccessAt = QDateTime::currentSecsSinceEpoch();
        _hasMore = hasMore;
        if (hasMore) {
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
    state.catchUpCycles = _catchUpCycles;
    state.intervalSeconds = CustomSettings::SyncIntervalSeconds();
    arm(NextAction(state));
}

Orchestrator *GetOrchestrator() {
    return gOrchestrator;
}

// ─── Tashqi API ──────────────────────────────────────────────────────────────

void Start() {
    // Idempotent: ikkinchi chaqiruvda hech narsa qilmaydi.
    // Ilova miqyosida bitta nusxa.
    if (gOrchestrator) {
        return;
    }
    gOrchestrator = new Orchestrator(nullptr);
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
