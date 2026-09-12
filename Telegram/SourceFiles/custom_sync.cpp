#include "custom_sync.h"

#include <algorithm>

#ifndef SYNC_SELFTEST
#include "custom_sync_client.h"
#include "custom_sync_outbox.h"
#include "custom_settings.h"

#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDebug>

#include "base/debug_log.h"
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

// WebSocket ulangan bo'lsa taymer intervali shuncha barobar uzayadi
// (yuqori chegara bilan). Sabab: soket ulangan ekan, server o'zgarishni
// O'ZI itaradi -- qisqa intervalli so'rab turish shunchaki ortiqcha
// DB + tarmoq ishi. Taymer bu holatda faqat ZAXIRA yo'l: soket jimgina
// uzilib qolsa yoki xabarnoma yo'qolsa.
constexpr auto kWsIdleMultiplier = 10;
constexpr auto kWsIdleMaxSeconds = 600;

// Siklni o'tkazib yuborish ruxsat etiladigan eng uzun oraliq. Xabarnoma
// tizimida kutilmagan xato bo'lsa ham (masalan server xabarnomani
// yubormasa) ma'lumot shundan ortiq eskirmaydi.
constexpr auto kMaxIdleSkipSeconds = qint64(30 * 60);

// Ilova ishga tushgandan beri o'tgan vaqt (birinchi start() da qo'yiladi).
QElapsedTimer gSinceStart;

[[nodiscard]] bool InStartupQuietWindow() {
    return gSinceStart.isValid()
        && (gSinceStart.elapsed() < qint64(kStartupQuietSeconds) * 1000);
}

[[nodiscard]] int EffectiveIntervalSeconds(Client *client) {
    const auto base = CustomSettings::SyncIntervalSeconds();
    if (!client || !client->webSocketConnected()) {
        return base;
    }
    return std::min(base * kWsIdleMultiplier, kWsIdleMaxSeconds);
}

} // namespace

Orchestrator::Orchestrator(QObject *parent)
    : QObject(parent)
    , _timer(new QTimer(this))
{
    _timer->setSingleShot(true);
    connect(_timer, &QTimer::timeout, this, &Orchestrator::onTimer);
}

Orchestrator::~Orchestrator() {
    if (gOrchestrator == this) {
        gOrchestrator = nullptr;
    }
}

void Orchestrator::start() {
    if (!gSinceStart.isValid()) {
        gSinceStart.start();
    }
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
    state.intervalSeconds = EffectiveIntervalSeconds(_client);

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

void Orchestrator::onTimer() {
    // Faqat TAYMER yo'li bo'sh siklni o'tkazib yubora oladi. Foydalanuvchi
    // bosgan "Sync now" va WS xabarnomasi to'g'ridan-to'g'ri runCycle() ga
    // boradi -- ular hech qachon o'tkazib yuborilmaydi.
    if (canSkipIdleCycle()) {
        ++_skippedCycles;
        if (_skippedCycles == 1 || (_skippedCycles % 20) == 0) {
            LOG(("CustomMod Sync: bo'sh sikl o'tkazib yuborildi "
                "(jami %1, tarmoq va DB so'rovisiz)").arg(_skippedCycles));
        }
        rearmIdle();
        return;
    }
    runCycle();
}

// Bo'sh siklni butunlay (tarmoq ham, DB ham yo'q) o'tkazib yuborish mumkinmi.
//
// Mantiq: server boshqa qurilma yozuv yuborganda WebSocket orqali seq
// bilan xabar beradi (o'zimizning yozuvlarimiz uchun bermaydi --
// NotifyOthersAsync). Demak:
//   - soket ulangan VA oxirgi muvaffaqiyatli pull'dan beri UZILMAGAN,
//   - xabarnomalardagi eng katta seq bizning cursor'dan katta emas,
//   - yuboriladigan navbat aniq bo'sh,
// bo'lsa -- serverda ham, bizda ham yangi narsa yo'q, sikl behuda.
// Kafolat buzilishi mumkin bo'lgan har holatda false: soket yo'q/uzilgan,
// kutilayotgan xabarnoma, qo'shimcha sahifa, xatolar ketma-ketligi, yoki
// oxirgi haqiqiy sikl kMaxIdleSkipSeconds dan eski.
bool Orchestrator::canSkipIdleCycle() const {
    if (!_client || !_client->webSocketConnected()) {
        return false;
    }
    if (_pendingNotify || _hasMore || _consecutiveFailures > 0) {
        return false;
    }
    if (_client->webSocketConnectionId() != _pulledWsConnectionId) {
        return false;
    }
    const auto now = QDateTime::currentSecsSinceEpoch();
    if (_lastFullCycleAt == 0
            || (now - _lastFullCycleAt) >= kMaxIdleSkipSeconds) {
        return false;
    }
    // GetState endi xotiradan o'qiydi (Outbox holat keshi).
    const auto cursor = Outbox::GetState(
        QStringLiteral("pull_cursor"),
        QStringLiteral("0")).toLongLong();
    if (_knownServerSeq > cursor) {
        return false;
    }
    return Outbox::ProbablyEmpty();
}

void Orchestrator::rearmIdle() {
    if (!CustomSettings::SyncEnabled()) {
        return;
    }
    SchedulerState state;
    state.enabled = true;
    state.inFlight = false;
    state.consecutiveFailures = _consecutiveFailures;
    state.hasMore = false;
    state.pendingNotify = false;
    state.catchUpCycles = 0;
    state.intervalSeconds = EffectiveIntervalSeconds(_client);
    arm(NextAction(state));
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
    _cycleWsConnectionId = _client->webSocketConnectionId();
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
    // Seq ni ESLAB qolamiz: bo'sh siklni o'tkazib yuborish qarori
    // (canSkipIdleCycle) shu raqamni cursor bilan solishtiradi.
    _knownServerSeq = std::max(_knownServerSeq, seq);
    if (!CustomSettings::SyncEnabled()) {
        return;
    }
    if (_inFlight || InStartupQuietWindow()) {
        // Sikl davomida kelgan xabarnoma — sikl tugagach NextAction orqali
        // darhol tortiladi.
        //
        // 2026-09-12: start tinchligi oynasida ham shu yo'ldan boramiz.
        // Ilgari bu yerdan to'g'ridan-to'g'ri syncNow() chaqirilardi va u
        // taymerni butunlay chetlab o'tardi -- ya'ni startda server bitta
        // xabarnoma yuborsa, sikl chatlar yuklanayotgan paytda ishga
        // tushib, bitta SQLite ulanishi uchun raqobatga kirardi. Endi
        // bayroq qo'yiladi, armlangan taymer esa oyna tugashi bilan uni
        // NextAction orqali darhol tortadi -- ya'ni xabarnoma YO'QOLMAYDI,
        // faqat kechikadi.
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
        _lastFullCycleAt = _lastSuccessAt;
        _pulledWsConnectionId = _cycleWsConnectionId;
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
    state.intervalSeconds = EffectiveIntervalSeconds(_client);
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
