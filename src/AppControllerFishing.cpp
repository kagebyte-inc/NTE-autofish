#include "AppController.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>

#include <linux/input.h>

void AppController::sendHookAction()
{
    if (!ensureUinput()) {
        setLastEvent(QStringLiteral("Failed to initialize /dev/uinput for F"));
        appendFishingLog(QStringLiteral("hook F failed: /dev/uinput unavailable"));
        return;
    }

    m_lastHookFiredMs = QDateTime::currentMSecsSinceEpoch();
    sendKeyTap(KEY_F);
    transitionFishingFlowState(FishingFlowState::AwaitingReel, QStringLiteral("hook F sent"));
    setLastEvent(QStringLiteral("Sent F through uinput"));
    appendFishingLog(QStringLiteral("hook F sent"));
}

void AppController::triggerResultScreenFlow(const QString &source)
{
    if (m_resultCloseTimer.isActive() || m_recastTimer.isActive()) {
        return;
    }

    m_resultScreenLatched = true;
    m_resultScreenAbsentFrames = 0;
    transitionFishingFlowState(FishingFlowState::ResultClosing, QStringLiteral("result %1").arg(source));
    m_hookActionTimer.stop();
    m_fishHookedLatched = false;
    m_fishHookedAbsentFrames = 0;
    m_hookBlueTriggerFrames = 0;
    setReelDirection(0);

    const int escDelayMs = 2000 + int(QRandomGenerator::global()->bounded(1001)) - 500;
    m_resultCloseTimer.start(escDelayMs);
    setLastEvent(QStringLiteral("Result screen %1: scheduled Esc in %2 ms").arg(source).arg(escDelayMs));
    setFishingEvent(QStringLiteral("result screen %1: Esc scheduled").arg(source));
    appendFishingLog(QStringLiteral("result accepted (%1), Esc in %2 ms").arg(source).arg(escDelayMs));
    m_fishingEventResetTimer.start();
}

void AppController::sendResultCloseAction()
{
    if (!ensureUinput()) {
        setLastEvent(QStringLiteral("Failed to initialize /dev/uinput for result Esc"));
        return;
    }

    sendKeyTap(KEY_ESC);
    const int recastDelayMs = 1200 + int(QRandomGenerator::global()->bounded(501));
    m_recastTimer.start(recastDelayMs);
    setLastEvent(QStringLiteral("Sent Esc for result screen, scheduled F in %1 ms").arg(recastDelayMs));
    setFishingEvent(QStringLiteral("result screen: Esc -> F"));
    appendFishingLog(QStringLiteral("Esc sent for result, F in %1 ms").arg(recastDelayMs));
    m_fishingEventResetTimer.start();
}

bool AppController::inspectResultScreen(const QJsonObject &root)
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
        const bool resultAllowed = m_fishingFlowState == FishingFlowState::Reeling
            || m_fishingFlowState == FishingFlowState::AwaitingReel;
        if (!resultAllowed) {
            if (now - m_lastIgnoredResultLogMs >= 1000) {
                appendFishingLog(QStringLiteral("result ignored: state gate blocked it"));
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

void AppController::inspectFishingEvent(const QJsonObject &root)
{
    if (m_fishingFlowState == FishingFlowState::ResultClosing
        || m_fishingFlowState == FishingFlowState::Recasting
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
        if (m_fishingFlowState == FishingFlowState::AwaitingReel
            && m_hookRetryCount < 1
            && m_lastHookFiredMs > 0
            && m_lastHookSeenMs - m_lastHookFiredMs >= 1500) {
            retryHookActionOrCleanup();
            return;
        }

        if (name == QStringLiteral("hook_blue_trigger")) {
            ++m_hookBlueTriggerFrames;
            if (m_hookBlueTriggerFrames < 3) {
                fishHookedVisible = true;
                m_fishHookedAbsentFrames = 0;
                appendFishingLog(QStringLiteral("blue hook seen %1/3").arg(m_hookBlueTriggerFrames));
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
        transitionFishingFlowState(FishingFlowState::HookScheduled, QStringLiteral("hook accepted"));
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
        setFishingEvent(m_debugMode
                            ? QStringLiteral("fish_on_hook detected by %1 at %2").arg(source, bboxText)
                            : QStringLiteral("Hook! Reason: %1").arg(source));
        appendFishingLog(m_debugMode
                             ? QStringLiteral("hook accepted by %1 at %2").arg(source, bboxText)
                             : QStringLiteral("hook accepted by %1").arg(source));
        m_fishingEventResetTimer.start();
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

void AppController::inspectReelControl(const QJsonObject &root)
{
    const QJsonObject details = root.value(QStringLiteral("details")).toObject();
    const QJsonArray elements = details.value(QStringLiteral("elements")).toArray();

    bool hasTarget = false;
    bool hasMarker = false;
    bool fishHookedPromptVisible = false;
    double targetCenter = 0.0;
    double targetLeft = 0.0;
    double targetRight = 0.0;
    double markerCenter = 0.0;
    int frameWidth = 0;

    for (const QJsonValue &value : elements) {
        const QJsonObject element = value.toObject();
        const QString name = element.value(QStringLiteral("name")).toString();
        if (name == QStringLiteral("fish_hooked_prompt")) {
            fishHookedPromptVisible = true;
            continue;
        }
        if (name != QStringLiteral("reel_green_target") && name != QStringLiteral("reel_yellow_marker")) {
            continue;
        }

        const QJsonArray bbox = element.value(QStringLiteral("bbox")).toArray();
        if (bbox.size() != 4) {
            continue;
        }
        const double center = bbox.at(0).toDouble() + bbox.at(2).toDouble() / 2.0;
        const QJsonObject elementDetails = element.value(QStringLiteral("details")).toObject();
        const int candidateFrameWidth = elementDetails.value(QStringLiteral("frame_width")).toInt();
        if (candidateFrameWidth > 0) {
            frameWidth = candidateFrameWidth;
        }

        if (name == QStringLiteral("reel_green_target")) {
            targetCenter = center;
            targetLeft = bbox.at(0).toDouble();
            targetRight = bbox.at(0).toDouble() + bbox.at(2).toDouble();
            hasTarget = true;
        } else {
            markerCenter = center;
            hasMarker = true;
        }
    }

    if (fishHookedPromptVisible) {
        return;
    }

    if (!hasTarget || !hasMarker) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastReelObservationMs > 0 && now > m_lastReelObservationMs && now - m_lastReelObservationMs < 250) {
        const double dt = double(now - m_lastReelObservationMs) / 1000.0;
        const double markerVelocity = (markerCenter - m_reelMarkerCenter) / dt;
        const double targetVelocity = (targetCenter - m_reelTargetCenter) / dt;
        m_reelMarkerVelocity = m_reelMarkerVelocity * 0.55 + markerVelocity * 0.45;
        m_reelTargetVelocity = m_reelTargetVelocity * 0.55 + targetVelocity * 0.45;
    } else {
        m_reelMarkerVelocity = 0.0;
        m_reelTargetVelocity = 0.0;
    }

    const bool enteredReeling = m_fishingFlowState != FishingFlowState::Reeling;
    m_reelControlVisible = true;
    transitionFishingFlowState(FishingFlowState::Reeling);
    m_hookRetryCount = 0;
    if (enteredReeling) {
        appendFishingLog(QStringLiteral("state -> Reeling"));
    }
    m_previousReelTargetCenter = m_reelTargetCenter;
    m_previousReelMarkerCenter = m_reelMarkerCenter;
    m_previousReelObservationMs = m_lastReelObservationMs;
    m_reelTargetCenter = targetCenter;
    m_reelTargetLeft = targetLeft;
    m_reelTargetRight = targetRight;
    m_reelMarkerCenter = markerCenter;
    m_reelFrameWidth = frameWidth;
    m_lastReelObservationMs = now;
    m_lastReelSeenMs = now;

    updateReelControl();

    if (m_debugMode && m_lastReelObservationMs - m_lastReelControlUiMs >= 180) {
        QString modeName = QStringLiteral("boundary");
        if (m_reelControlMode == ReelControlMode::ChizukuoPid) {
            modeName = QStringLiteral("chizukuo pid");
        } else if (m_reelControlMode == ReelControlMode::LegacyPid) {
            modeName = QStringLiteral("kagebaito pid");
        }

        setReelControl(QStringLiteral("%1 target %2  marker %3  error %4  v %5/%6")
                           .arg(modeName)
                           .arg(m_reelTargetCenter, 0, 'f', 1)
                           .arg(m_reelMarkerCenter, 0, 'f', 1)
                           .arg(m_reelTargetCenter - m_reelMarkerCenter, 0, 'f', 1)
                           .arg(m_reelMarkerVelocity, 0, 'f', 0)
                           .arg(m_reelTargetVelocity, 0, 'f', 0));
        m_lastReelControlUiMs = m_lastReelObservationMs;
    }
}

void AppController::scheduleHookAction()
{
    if (m_hookActionTimer.isActive()) {
        return;
    }

    const int jitterMs = int(QRandomGenerator::global()->bounded(1001)) - 500;
    const int delayMs = 3000 + jitterMs;
    m_hookActionTimer.start(delayMs);
    if (m_fishingFlowState != FishingFlowState::HookScheduled) {
        transitionFishingFlowState(FishingFlowState::HookScheduled, QStringLiteral("hook scheduled"));
    }
    setLastEvent(QStringLiteral("Scheduled F in %1 ms").arg(delayMs));
    appendFishingLog(QStringLiteral("hook F scheduled in %1 ms").arg(delayMs));
}

void AppController::transitionFishingFlowState(FishingFlowState state, const QString &reason)
{
    if (m_fishingFlowState == state) {
        if (!reason.isEmpty()) {
            m_fishingFlowStateEnteredMs = QDateTime::currentMSecsSinceEpoch();
        }
        return;
    }

    m_fishingFlowState = state;
    m_fishingFlowStateEnteredMs = QDateTime::currentMSecsSinceEpoch();

    if (!reason.isEmpty()) {
        appendFishingLog(QStringLiteral("state -> %1 (%2)")
                             .arg([state]() {
                                 switch (state) {
                                 case FishingFlowState::Waiting:
                                     return QStringLiteral("Waiting");
                                 case FishingFlowState::HookScheduled:
                                     return QStringLiteral("HookScheduled");
                                 case FishingFlowState::AwaitingReel:
                                     return QStringLiteral("AwaitingReel");
                                 case FishingFlowState::Reeling:
                                     return QStringLiteral("Reeling");
                                 case FishingFlowState::ResultClosing:
                                     return QStringLiteral("ResultClosing");
                                 case FishingFlowState::Recasting:
                                     return QStringLiteral("Recasting");
                                 }
                                 return QStringLiteral("Unknown");
                             }())
                             .arg(reason));
    }
}

void AppController::updateFishingFlowWatchdog()
{
    if (m_status != QStringLiteral("watching")
        || m_resultCloseTimer.isActive()
        || m_recastTimer.isActive()) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 awaitingReelTimeoutMs = 15000;
    constexpr qint64 reelLostTimeoutMs = 1800;

    if (m_fishingFlowState == FishingFlowState::AwaitingReel
        && now - m_fishingFlowStateEnteredMs >= awaitingReelTimeoutMs) {
        retryHookActionOrCleanup();
        return;
    }

    if (m_fishingFlowState == FishingFlowState::Reeling
        && m_lastReelSeenMs > 0
        && now - m_lastReelSeenMs >= reelLostTimeoutMs) {
        appendFishingLog(QStringLiteral("reel lost for %1 ms -> F-only recovery").arg(now - m_lastReelSeenMs));
        scheduleFOnlyRecovery(QStringLiteral("reel watchdog"));
    }
}

void AppController::retryHookActionOrCleanup()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 hookVisibleGraceMs = 2200;

    if (m_hookRetryCount < 1 && m_lastHookSeenMs > 0 && now - m_lastHookSeenMs <= hookVisibleGraceMs) {
        if (!ensureUinput()) {
            appendFishingLog(QStringLiteral("hook retry failed: /dev/uinput unavailable"));
            scheduleFOnlyRecovery(QStringLiteral("hook retry failed"));
            return;
        }

        ++m_hookRetryCount;
        m_lastHookFiredMs = now;
        transitionFishingFlowState(FishingFlowState::AwaitingReel, QStringLiteral("hook retry F"));
        sendKeyTap(KEY_F);
        setLastEvent(QStringLiteral("Retried F after hook watchdog"));
        setFishingEvent(QStringLiteral("hook watchdog: retried F"));
        appendFishingLog(QStringLiteral("hook watchdog retry F (%1/1)").arg(m_hookRetryCount));
        m_fishingEventResetTimer.start();
        return;
    }

    appendFishingLog(QStringLiteral("hook/reel watchdog timeout -> F-only recovery"));
    scheduleFOnlyRecovery(QStringLiteral("hook watchdog"));
}

void AppController::scheduleFOnlyRecovery(const QString &source)
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
    m_lastReelSeenMs = 0;
    m_lastReelObservationMs = 0;
    m_reelControlVisible = false;
    resetReelControllerState();

    transitionFishingFlowState(FishingFlowState::Recasting, source);
    const int delayMs = 800 + int(QRandomGenerator::global()->bounded(701));
    m_recastTimer.start(delayMs);
    setLastEvent(QStringLiteral("%1: F-only recovery in %2 ms").arg(source).arg(delayMs));
    setFishingEvent(QStringLiteral("%1: F-only recovery").arg(source));
    appendFishingLog(QStringLiteral("%1 -> F-only recovery, F in %2 ms").arg(source).arg(delayMs));
    m_fishingEventResetTimer.start();
}

void AppController::resetReelControllerState()
{
    m_reelPulseEndMs = 0;
    m_nextReelPulseMs = 0;
    m_reelGuardSettleUntilMs = 0;
    m_reelPulseDirection = 0;
    m_lastReelPidMs = 0;
    m_reelPidIntegral = 0.0;
    m_reelPidPreviousMarker = 0.0;
    m_reelPidDFiltered = 0.0;
    m_reelPidFirst = true;
    m_reelPidLastSign = 0;
    m_reelPidSignChangeMs.clear();
    m_reelPidAdaptiveKpScale = 1.0;
    m_reelReactionEndMs = 0;
    m_reelHumPulseEndMs = 0;
    m_reelHumPulseState = 0;
    m_reelHumTargetDirection = 0;
    m_reelLastAction = 0;
    setReelDirection(0);
}

void AppController::sendRecastAction()
{
    if (!ensureUinput()) {
        setLastEvent(QStringLiteral("Failed to initialize /dev/uinput for recast F"));
        appendFishingLog(QStringLiteral("recast F failed: /dev/uinput unavailable"));
        return;
    }

    transitionFishingFlowState(FishingFlowState::Recasting);
    sendKeyTap(KEY_F);
    transitionFishingFlowState(FishingFlowState::Waiting);
    m_lastReelObservationMs = 0;
    m_reelControlVisible = false;
    m_resultScreenLatched = false;
    m_resultScreenAbsentFrames = 0;
    m_hookBlueTriggerFrames = 0;
    m_hookRetryCount = 0;
    m_lastHookSeenMs = 0;
    m_lastHookFiredMs = 0;
    m_lastReelSeenMs = 0;
    setLastEvent(QStringLiteral("Sent F to recast/recover"));
    appendFishingLog(QStringLiteral("recast F sent; state -> Waiting"));
}

void AppController::sendManualResultScreenAction()
{
    triggerResultScreenFlow(QStringLiteral("manual GUI calibration"));
}

void AppController::resetFishingEvent()
{
    setFishingEvent(QStringLiteral("No fishing events yet"));
}
