#pragma once

#include <QtCore/QString>
#include <QtCore/QObject>

#ifndef SYNC_SELFTEST
class QTimer;
#endif

namespace CustomSync {

// ─── Sof qaror mantiqi (pure function) ──────────────────────────────────────
// Selftestda tekshiriladi. Timer, tarmoq, bazaga bog'lanmagan.

struct SchedulerState {
    bool enabled = false;        // CustomSettings::SyncEnabled()
    bool inFlight = false;       // hozir sikl ishlayapti
    int consecutiveFailures = 0;
    bool hasMore = false;        // server yana yozuvlar borligini bildirdi
    int catchUpCycles = 0;       // ketma-ket tez sikllar soni
    int intervalSeconds = 0;     // CustomSettings::SyncIntervalSeconds()
};

struct SchedulerDecision {
    bool runNow = false;         // darhol siklni boshlash
    int delaySeconds = 0;        // runNow false bo'lsa — timer oraliq soniyalarda
};

[[nodiscard]] SchedulerDecision NextAction(const SchedulerState &s);

inline constexpr int kMaxCatchUpCycles = 5;
inline constexpr int kMaxBackoffShift = 9;
inline constexpr int kMaxBackoffSeconds = 300;

// ─── Tashqi API (Lifecycle va Status) ────────────────────────────────────────

void Start();     // ilova ishga tushganda bir marta
void Stop();      // yopilishda
void SyncNow();   // qo'lda, UI tugmasidan (Task 10)

struct SyncStatus {
    qint64 lastSuccessAt = 0;
    int pendingCount = 0;
    QString lastError;
    int consecutiveFailures = 0;
    bool inFlight = false;
};

[[nodiscard]] SyncStatus CurrentSyncStatus();

#ifndef SYNC_SELFTEST
class Client;

// Orkestrator sinfi — faqat asosiy (GUI) oqimda ishlaydi (QTimer).
// Hech qanday ikkinchi oqim yoki mutex yo'q.
class Orchestrator : public QObject {
    Q_OBJECT

public:
    explicit Orchestrator(QObject *parent = nullptr);
    ~Orchestrator() override;

    void start();
    void stop();
    void syncNow();
    [[nodiscard]] SyncStatus status() const;

Q_SIGNALS:
    void statusChanged(const SyncStatus &status);

private:
    void arm(const SchedulerDecision &d);
    void runCycle();
    void onCycleFinished(int pushed, int pushFailed,
                         int merged, int rejected,
                         bool hasMore, const QString &error);

    QTimer *_timer = nullptr;
    // BITTA uzoq yashovchi klient. Har siklda yangisini yaratish xato edi:
    // access token Client ichida yashaydi, ya'ni yangi nusxa har safar
    // /devices/refresh ga borardi. Refresh token esa BIR MARTALIK -- server
    // eskisini o'ldiradi -- shuning uchun bu har 30 soniyada bitta ortiqcha
    // so'rov va bitta token rotatsiyasi demak edi. TLS ulanishi ham qayta
    // ishlatilmasdi.
    Client *_client = nullptr;
    bool _inFlight = false;
    bool _hasMore = false;
    int _catchUpCycles = 0;
    int _consecutiveFailures = 0;
    qint64 _lastSuccessAt = 0;
    QString _lastError;
};

[[nodiscard]] Orchestrator *GetOrchestrator();
#endif

} // namespace CustomSync
