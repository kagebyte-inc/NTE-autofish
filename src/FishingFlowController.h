#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

class AppController;

class FishingFlowController final : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Waiting,
        HookScheduled,
        AwaitingReel,
        Reeling,
        ResultClosing,
        Recasting,
    };

    explicit FishingFlowController(AppController *controller, QObject *parent = nullptr);

    State state() const;
    void resetSession();
    void onVisionStopped();
    void enterReeling();
    void applyTimingSettings();
    bool inspectResultScreen(const QJsonObject &root);
    void inspectFishingEvent(const QJsonObject &root);
    void updateWatchdog();
    void noteReelSeen(qint64 timestampMs);

private slots:
    void sendHookAction();
    void sendResultCloseAction();
    void sendRecastAction();
    void resetFishingEvent();

private:
    void triggerResultScreenFlow(const QString &source);
    void scheduleHookAction();
    void transitionState(State state, const QString &reason = {});
    void retryHookActionOrCleanup();
    void scheduleFOnlyRecovery(const QString &source);

    AppController *m_app = nullptr;
    State m_state = State::Waiting;
    bool m_fishHookedLatched = false;
    int m_fishHookedAbsentFrames = 0;
    int m_hookBlueTriggerFrames = 0;
    bool m_resultScreenLatched = false;
    int m_resultScreenAbsentFrames = 0;
    qint64 m_stateEnteredMs = 0;
    qint64 m_lastHookSeenMs = 0;
    qint64 m_lastHookFiredMs = 0;
    qint64 m_lastReelSeenMs = 0;
    qint64 m_lastIgnoredResultLogMs = 0;
    int m_hookRetryCount = 0;

    QTimer m_eventResetTimer;
    QTimer m_hookActionTimer;
    QTimer m_resultCloseTimer;
    QTimer m_recastTimer;
};