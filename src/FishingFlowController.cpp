
#include "FishingFlowController.h"

#include "AppController.h"
#include "AppSettings.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>

#include <linux/input.h>

FishingFlowController::FishingFlowController(AppController *controller, QObject *parent)
    : QObject(parent)
    , m_app(controller)
{
    m_eventResetTimer.setSingleShot(true);
    m_hookActionTimer.setSingleShot(true);
    m_resultCloseTimer.setSingleShot(true);
    m_recastTimer.setSingleShot(true);

    connect(&m_eventResetTimer, &QTimer::timeout, this, &FishingFlowController::resetFishingEvent);
    connect(&m_hookActionTimer, &QTimer::timeout, this, &FishingFlowController::sendHookAction);
    connect(&m_resultCloseTimer, &QTimer::timeout, this, &FishingFlowController::sendResultCloseAction);
    connect(&m_recastTimer, &QTimer::timeout, this, &FishingFlowController::sendRecastAction);
    connect(m_app->settings(), &AppSettings::fishingTuningChanged, this, &FishingFlowController::applyTimingSettings);
    applyTimingSettings();
    m_stateEnteredMs = QDateTime::currentMSecsSinceEpoch();
}

void FishingFlowController::applyTimingSettings()
{
    m_eventResetTimer.setInterval(m_app->settings()->fishingEventResetMs());
}

FishingFlowController::State FishingFlowController::state() const { return m_state; }

void FishingFlowController::resetSession()
{
    m_hookActionTimer.stop();
    m_resultCloseTimer.stop();
    m_recastTimer.stop();
    m_resultScreenLatched = false;
    m_resultScreenAbsentFrames = 0;
    m_fishHookedLatched = false;
    m_fishHookedAbsentFrames = 0;
    m_hookBlueTriggerFrames = 0;
    m_hookRetryCount = 0;
    m_lastHookSeenMs = 0;
    m_lastHookFiredMs = 0;
    transitionState(State::Waiting);
}

void FishingFlowController::onVisionStopped()
{
    resetSession();
}

void FishingFlowController::enterReeling()
{
    const bool entered = m_state != State::Reeling;
    transitionState(State::Reeling);
    m_hookRetryCount = 0;
    if (entered) {
        m_app->appendFishingLog(QStringLiteral("state -> Reeling"));
    }
}

void FishingFlowController::sendHookAction()
{
    if (!m_app->input()->ensureOpen()) {
        m_app->setLastEvent(QStringLiteral("Failed to initialize input backend for F: %1").arg(m_app->input()->lastError()));
        m_app->appendFishingLog(QStringLiteral("hook F failed: input backend unavailable"));
        return;
    }

    m_lastHookFiredMs = QDateTime::currentMSecsSinceEpoch();
    m_app->sendKeyTap(KEY_F);
    transitionState(State::AwaitingReel, QStringLiteral("hook F sent"));
    m_app->setLastEvent(QStringLiteral("Sent F through %1").arg(m_app->input()->backendName()));
    m_app->appendFishingLog(QStringLiteral("hook F sent"));
}

void FishingFlowController::triggerResultScreenFlow(const QString &source)
{
    if (m_resultCloseTimer.isActive() || m_recastTimer.isActive()) {
        return;
    }

    m_resultScreenLatched = true;
    m_resultScreenAbsentFrames = 0;
    transitionState(State::ResultClosing, QStringLiteral("result %1").arg(source));
    m_hookActionTimer.stop();
    m_fishHookedLatched = false;
    m_fishHookedAbsentFrames = 0;
    m_hookBlueTriggerFrames = 0;
    m_app->setReelDirection(0);

    const auto *settings = m_app->settings();
    const int escJitter = settings->fishingResultEscJitterMs();
    const int escDelayMs = settings->fishingResultEscBaseMs()
        + int(QRandomGenerator::global()->bounded(escJitter * 2 + 1)) - escJitter;
    m_resultCloseTimer.start(escDelayMs);
    m_app->setLastEvent(QStringLiteral("Result screen %1: scheduled Esc in %2 ms").arg(source).arg(escDelayMs));
    m_app->setFishingEvent(QStringLiteral("result screen %1: Esc scheduled").arg(source));
    m_app->appendFishingLog(QStringLiteral("result accepted (%1), Esc in %2 ms").arg(source).arg(escDelayMs));
    m_eventResetTimer.start();
}

void FishingFlowController::sendResultCloseAction()
{
    if (!m_app->input()->ensureOpen()) {
        m_app->setLastEvent(QStringLiteral("Failed to initialize input backend for result Esc: %1").arg(m_app->input()->lastError()));
        return;
    }

    m_app->sendKeyTap(KEY_ESC);
    const auto *settings = m_app->settings();
    const int recastJitter = settings->fishingRecastJitterMs();
    const int recastDelayMs = settings->fishingRecastBaseMs()
        + int(QRandomGenerator::global()->bounded(recastJitter * 2 + 1)) - recastJitter;
    m_recastTimer.start(recastDelayMs);
    m_app->setLastEvent(QStringLiteral("Sent Esc for result screen, scheduled F in %1 ms").arg(recastDelayMs));
    m_app->setFishingEvent(QStringLiteral("result screen: Esc -> F"));
    m_app->appendFishingLog(QStringLiteral("Esc sent for result, F in %1 ms").arg(recastDelayMs));
    m_eventResetTimer.start();
}

bool FishingFlowController::inspectResultScreen(const QJsonObject &root)
{
    const QJsonObject details = root.value(QStringLiteral("details")).toObject();
    const QJsonArray elements = details.value(QStringLiteral("elements")).toArray();
    bool resultVisible = false;

    for (const QJsonValue &value : elements) {
        const QJsonObject element = value.toObject();
        if (element.value(QStringLiteral("name")).toString() != QStringLiteral("fishing_result_screen")) {
            continue;
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool resultAllowed = m_state == State::Reeling
            || m_state == State::AwaitingReel;
        if (!resultAllowed) {
            if (now - m_lastIgnoredResultLogMs >= 1000) {
                m_app->appendFishingLog(QStringLiteral("result ignored: state gate blocked it"));
                m_lastIgnoredResultLogMs = now;
            }
            return false;
        }

        resultVisible = true;
        m_resultScreenAbsentFrames = 0;
        if (m_resultScreenLatched) {
            return true;
        }

        triggerResultScreenFlow(QStringLiteral("detected"));
        return true;
    }

    if (!resultVisible) {
        ++m_resultScreenAbsentFrames;
        if (m_resultScreenAbsentFrames >= 10) {
            m_resultScreenLatched = false;
        }
    }

    return false;
}

void FishingFlowController::inspectFishingEvent(const QJsonObject &root)
{
    if (m_state == State::ResultClosing
        || m_state == State::Recasting
        || m_resultCloseTimer.isActive()
        || m_recastTimer.isActive()) {
        return;
    }

    const QJsonObject details = root.value(QStringLiteral("details")).toObject();
    const QJsonArray elements = details.value(QStringLiteral("elements")).toArray();
    bool fishHookedVisible = false;
    for (const QJsonValue &value : elements) {
        const QJsonObject element = value.toObject();
        const QString name = element.value(QStringLiteral("name")).toString();
        if (name != QStringLiteral("fish_hooked_prompt") && name != QStringLiteral("hook_blue_trigger")) {
            continue;
        }

        const QJsonObject elementDetails = element.value(QStringLiteral("details")).toObject();
        if (elementDetails.value(QStringLiteral("state")).toString() != QStringLiteral("fish_on_hook")) {
            continue;
        }

        m_lastHookSeenMs = QDateTime::currentMSecsSinceEpoch();
        if (m_state == State::AwaitingReel
            && m_hookRetryCount < 1
            && m_lastHookFiredMs > 0
            && m_lastHookSeenMs - m_lastHookFiredMs >= m_app->settings()->fishingHookRetryMs()) {
            retryHookActionOrCleanup();
            return;
        }

        if (name == QStringLiteral("hook_blue_trigger")) {
            ++m_hookBlueTriggerFrames;
            if (m_hookBlueTriggerFrames < 3) {
                fishHookedVisible = true;
                m_fishHookedAbsentFrames = 0;
                m_app->appendFishingLog(QStringLiteral("blue hook seen %1/3").arg(m_hookBlueTriggerFrames));
                return;
            }
        } else {
            m_hookBlueTriggerFrames = 0;
        }

        fishHookedVisible = true;
        m_fishHookedAbsentFrames = 0;
        if (m_fishHookedLatched) {
            return;
        }
        m_fishHookedLatched = true;
        transitionState(State::HookScheduled, QStringLiteral("hook accepted"));
        scheduleHookAction();

        const QJsonArray bbox = element.value(QStringLiteral("bbox")).toArray();
        const QString bboxText = bbox.size() == 4
            ? QStringLiteral("[%1, %2, %3, %4]")
                  .arg(bbox.at(0).toInt())
                  .arg(bbox.at(1).toInt())
                  .arg(bbox.at(2).toInt())
                  .arg(bbox.at(3).toInt())
            : QStringLiteral("unknown bbox");
        const QString source = name == QStringLiteral("hook_blue_trigger")
            ? QStringLiteral("blue trigger")
            : QStringLiteral("prompt");
        m_app->setFishingEvent(m_app->settings()->debugMode()
                            ? QStringLiteral("fish_on_hook detected by %1 at %2").arg(source, bboxText)
                            : QStringLiteral("Hook! Reason: %1").arg(source));
        m_app->appendFishingLog(m_app->settings()->debugMode()
                             ? QStringLiteral("hook accepted by %1 at %2").arg(source, bboxText)
                             : QStringLiteral("hook accepted by %1").arg(source));
        m_eventResetTimer.start();
        return;
    }

    if (!fishHookedVisible) {
        m_hookBlueTriggerFrames = 0;
        ++m_fishHookedAbsentFrames;
        if (m_fishHookedAbsentFrames >= 8) {
            m_fishHookedLatched = false;
        }
    }
}

void FishingFlowController::scheduleHookAction()
{
    if (m_hookActionTimer.isActive()) {
        return;
    }

    const auto *settings = m_app->settings();
    const int jitterMs = settings->fishingHookJitterMs();
    const int delayMs = settings->fishingHookDelayMs()
        + int(QRandomGenerator::global()->bounded(jitterMs * 2 + 1)) - jitterMs;
    m_hookActionTimer.start(delayMs);
    if (m_state != State::HookScheduled) {
        transitionState(State::HookScheduled, QStringLiteral("hook scheduled"));
    }
    m_app->setLastEvent(QStringLiteral("Scheduled F in %1 ms").arg(delayMs));
    m_app->appendFishingLog(QStringLiteral("hook F scheduled in %1 ms").arg(delayMs));
}

void FishingFlowController::transitionState(State state, const QString &reason)
{
    if (m_state == state) {
        if (!reason.isEmpty()) {
            m_stateEnteredMs = QDateTime::currentMSecsSinceEpoch();
        }
        return;
    }

    m_state = state;
    m_stateEnteredMs = QDateTime::currentMSecsSinceEpoch();

    if (!reason.isEmpty()) {
        m_app->appendFishingLog(QStringLiteral("state -> %1 (%2)")
                             .arg([state]() {
                                 switch (state) {
                                 case State::Waiting:
                                     return QStringLiteral("Waiting");
                                 case State::HookScheduled:
                                     return QStringLiteral("HookScheduled");
                                 case State::AwaitingReel:
                                     return QStringLiteral("AwaitingReel");
                                 case State::Reeling:
                                     return QStringLiteral("Reeling");
                                 case State::ResultClosing:
                                     return QStringLiteral("ResultClosing");
                                 case State::Recasting:
                                     return QStringLiteral("Recasting");
                                 }
                                 return QStringLiteral("Unknown");
                             }())
                             .arg(reason));
    }
}

void FishingFlowController::updateWatchdog()
{
    if (m_app->status() != QStringLiteral("watching")
        || m_resultCloseTimer.isActive()
        || m_recastTimer.isActive()) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto *settings = m_app->settings();
    const qint64 awaitingReelTimeoutMs = settings->fishingAwaitingReelMs();
    const qint64 reelLostTimeoutMs = settings->fishingReelLostMs();

    if (m_state == State::AwaitingReel
        && now - m_stateEnteredMs >= awaitingReelTimeoutMs) {
        retryHookActionOrCleanup();
        return;
    }

    if (m_state == State::Reeling
        && m_lastReelSeenMs > 0
        && now - m_lastReelSeenMs >= reelLostTimeoutMs) {
        m_app->appendFishingLog(QStringLiteral("reel lost for %1 ms -> F-only recovery").arg(now - m_lastReelSeenMs));
        scheduleFOnlyRecovery(QStringLiteral("reel watchdog"));
    }
}

void FishingFlowController::retryHookActionOrCleanup()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 hookVisibleGraceMs = m_app->settings()->fishingHookVisibleGraceMs();

    if (m_hookRetryCount < 1 && m_lastHookSeenMs > 0 && now - m_lastHookSeenMs <= hookVisibleGraceMs) {
        if (!m_app->input()->ensureOpen()) {
            m_app->appendFishingLog(QStringLiteral("hook retry failed: input backend unavailable"));
            scheduleFOnlyRecovery(QStringLiteral("hook retry failed"));
            return;
        }

        ++m_hookRetryCount;
        m_lastHookFiredMs = now;
        transitionState(State::AwaitingReel, QStringLiteral("hook retry F"));
        m_app->sendKeyTap(KEY_F);
        m_app->setLastEvent(QStringLiteral("Retried F after hook watchdog"));
        m_app->setFishingEvent(QStringLiteral("hook watchdog: retried F"));
        m_app->appendFishingLog(QStringLiteral("hook watchdog retry F (%1/1)").arg(m_hookRetryCount));
        m_eventResetTimer.start();
        return;
    }

    m_app->appendFishingLog(QStringLiteral("hook/reel watchdog timeout -> F-only recovery"));
    scheduleFOnlyRecovery(QStringLiteral("hook watchdog"));
}

void FishingFlowController::scheduleFOnlyRecovery(const QString &source)
{
    if (m_recastTimer.isActive()) {
        return;
    }

    m_hookActionTimer.stop();
    m_resultScreenLatched = false;
    m_resultScreenAbsentFrames = 0;
    m_fishHookedLatched = false;
    m_fishHookedAbsentFrames = 0;
    m_hookBlueTriggerFrames = 0;
    m_hookRetryCount = 0;
    m_lastHookSeenMs = 0;
    m_lastHookFiredMs = 0;
    m_app->reel()->resetSession();
    m_app->resetReelControllerState();

    transitionState(State::Recasting, source);
    const auto *settings = m_app->settings();
    const int recoveryJitter = settings->fishingRecoveryJitterMs();
    const int delayMs = settings->fishingRecoveryBaseMs()
        + int(QRandomGenerator::global()->bounded(recoveryJitter * 2 + 1)) - recoveryJitter;
    m_recastTimer.start(delayMs);
    m_app->setLastEvent(QStringLiteral("%1: F-only recovery in %2 ms").arg(source).arg(delayMs));
    m_app->setFishingEvent(QStringLiteral("%1: F-only recovery").arg(source));
    m_app->appendFishingLog(QStringLiteral("%1 -> F-only recovery, F in %2 ms").arg(source).arg(delayMs));
    m_eventResetTimer.start();
}

void FishingFlowController::sendRecastAction()
{
    if (!m_app->input()->ensureOpen()) {
        m_app->setLastEvent(QStringLiteral("Failed to initialize input backend for recast F: %1").arg(m_app->input()->lastError()));
        m_app->appendFishingLog(QStringLiteral("recast F failed: input backend unavailable"));
        return;
    }

    transitionState(State::Recasting);
    m_app->sendKeyTap(KEY_F);
    transitionState(State::Waiting);
    m_app->reel()->resetSession();
    m_resultScreenLatched = false;
    m_resultScreenAbsentFrames = 0;
    m_hookBlueTriggerFrames = 0;
    m_hookRetryCount = 0;
    m_lastHookSeenMs = 0;
    m_lastHookFiredMs = 0;
    m_app->setLastEvent(QStringLiteral("Sent F to recast/recover"));
    m_app->appendFishingLog(QStringLiteral("recast F sent; state -> Waiting"));
}

void FishingFlowController::resetFishingEvent()
{
    m_app->setFishingEvent({});
}

void FishingFlowController::noteReelSeen(qint64 timestampMs)
{
    m_lastReelSeenMs = timestampMs;
}
