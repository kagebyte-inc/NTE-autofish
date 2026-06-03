#include "ReelController.h"

#include "AppController.h"
#include "AppSettings.h"
#include "FishingFlowController.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>

ReelController::ReelController(AppController *controller, FishingFlowController *fishing, QObject *parent)
    : QObject(parent)
    , m_app(controller)
    , m_fishing(fishing)
{
}

void ReelController::resetSession()
{
    m_visible = false;
    m_lastObservationMs = 0;
    m_lastSeenMs = 0;
    resetControllerState();
}

void ReelController::resetControllerState()
{
    m_pulseEndMs = 0;
    m_nextPulseMs = 0;
    m_guardSettleUntilMs = 0;
    m_pulseDirection = 0;
    m_lastPidMs = 0;
    m_pidIntegral = 0.0;
    m_pidPreviousMarker = 0.0;
    m_pidDFiltered = 0.0;
    m_pidFirst = true;
    m_pidLastSign = 0;
    m_pidSignChangeMs.clear();
    m_pidAdaptiveKpScale = 1.0;
    m_reactionEndMs = 0;
    m_humPulseEndMs = 0;
    m_humPulseState = 0;
    m_humTargetDirection = 0;
    m_lastAction = 0;
    m_app->setReelDirection(0);
}

void ReelController::ingestFrame(const QJsonObject &root)
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
    if (m_lastObservationMs > 0 && now > m_lastObservationMs && now - m_lastObservationMs < 250) {
        const double dt = double(now - m_lastObservationMs) / 1000.0;
        const double markerVelocity = (markerCenter - m_markerCenter) / dt;
        const double targetVelocity = (targetCenter - m_targetCenter) / dt;
        m_markerVelocity = m_markerVelocity * 0.55 + markerVelocity * 0.45;
        m_targetVelocity = m_targetVelocity * 0.55 + targetVelocity * 0.45;
    } else {
        m_markerVelocity = 0.0;
        m_targetVelocity = 0.0;
    }

    m_visible = true;
    m_fishing->enterReeling();
    m_previousTargetCenter = m_targetCenter;
    m_previousMarkerCenter = m_markerCenter;
    m_previousObservationMs = m_lastObservationMs;
    m_targetCenter = targetCenter;
    m_targetLeft = targetLeft;
    m_targetRight = targetRight;
    m_markerCenter = markerCenter;
    m_frameWidth = frameWidth;
    m_lastObservationMs = now;
    m_lastSeenMs = now;
    m_fishing->noteReelSeen(now);

    tick();

    if (m_app->settings()->debugMode() && m_lastObservationMs - m_lastControlUiMs >= 180) {
        QString modeName = QStringLiteral("stable");
        if (m_app->settings()->reelControlMode() == AppSettings::ReelControlMode::Experimental) {
            modeName = QStringLiteral("experimental");
        } else if (m_app->settings()->reelControlMode() == AppSettings::ReelControlMode::ChizukuoPid) {
            modeName = QStringLiteral("chizukuo pid (adapted, may not work well)");
        }

        m_app->setReelControl(QStringLiteral("%1 target %2  marker %3  error %4  v %5/%6")
                           .arg(modeName)
                           .arg(m_targetCenter, 0, 'f', 1)
                           .arg(m_markerCenter, 0, 'f', 1)
                           .arg(m_targetCenter - m_markerCenter, 0, 'f', 1)
                           .arg(m_markerVelocity, 0, 'f', 0)
                           .arg(m_targetVelocity, 0, 'f', 0));
        m_lastControlUiMs = m_lastObservationMs;
    }
}

void ReelController::tick()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_visible || now - m_lastObservationMs > 180) {
        m_visible = false;
        m_markerVelocity = 0.0;
        m_targetVelocity = 0.0;
        resetControllerState();
        return;
    }

    const int frameWidth = m_frameWidth > 0 ? m_frameWidth : 1920;
    const double targetWidth = qMax(1.0, m_targetRight - m_targetLeft);
    if (m_pulseDirection != 0) {
        if (now < m_pulseEndMs) {
            if (m_app->settings()->reelControlMode() == AppSettings::ReelControlMode::Experimental) {
                const bool insideGreen = m_markerCenter >= m_targetLeft
                    && m_markerCenter <= m_targetRight;
                const bool enteredGreen = m_pulseDirection > 0
                    ? m_markerCenter >= m_targetLeft
                    : m_markerCenter <= m_targetRight;
                const bool crossedTargetCenter = m_pulseDirection * (m_targetCenter - m_markerCenter) <= 0.0;
                const double directionNeed = m_targetCenter - m_markerCenter;
                const double predictedDirectionNeed = (m_targetCenter + m_targetVelocity * 0.045)
                    - (m_markerCenter + m_markerVelocity * 0.045);
                const bool wrongWay = m_pulseDirection * directionNeed < -targetWidth * 0.12
                    || m_pulseDirection * predictedDirectionNeed < -targetWidth * 0.08;
                const double distanceToGreenEntry = m_pulseDirection > 0
                    ? m_targetLeft - m_markerCenter
                    : m_markerCenter - m_targetRight;
                const double closingSpeed = m_pulseDirection * (m_markerVelocity - m_targetVelocity);
                const double brakeDistance = qMax(5.0, closingSpeed * (m_app->settings()->experimentalBrakeMs() / 1000.0) + qAbs(m_markerVelocity) * 0.018);
                const bool shouldBrakeBeforeEntry = distanceToGreenEntry > 0.0
                    && closingSpeed > qMax(80.0, targetWidth * 2.0)
                    && distanceToGreenEntry <= brakeDistance;
                if (wrongWay) {
                    m_pulseDirection = 0;
                    m_pulseEndMs = 0;
                    m_nextPulseMs = 0;
                    m_guardSettleUntilMs = 0;
                    m_app->setReelDirection(0);
                    if (m_app->settings()->debugMode() && now - m_lastControlUiMs >= 80) {
                        m_app->setReelControl(QStringLiteral("experimental abort wrong-way  marker %1  target %2")
                                           .arg(m_markerCenter, 0, 'f', 1)
                                           .arg(m_targetCenter, 0, 'f', 1));
                        m_lastControlUiMs = now;
                    }
                    return;
                }
                if (shouldBrakeBeforeEntry) {
                    m_pulseDirection = 0;
                    m_pulseEndMs = 0;
                    m_guardSettleUntilMs = now + qMax(20, m_app->settings()->experimentalSettleMs());
                    m_nextPulseMs = qMax(m_nextPulseMs, m_guardSettleUntilMs);
                    const int brakeDirection = closingSpeed > qMax(160.0, targetWidth * 3.0) ? -m_app->input()->reelDirection() : 0;
                    m_app->setReelDirection(brakeDirection);
                    if (m_app->settings()->debugMode() && now - m_lastControlUiMs >= 90) {
                        m_app->setReelControl(QStringLiteral("experimental brake %1  dist %2  speed %3  brake %4")
                                           .arg(brakeDirection == 0 ? QStringLiteral("coast") : QStringLiteral("counter"))
                                           .arg(distanceToGreenEntry, 0, 'f', 1)
                                           .arg(closingSpeed, 0, 'f', 0)
                                           .arg(brakeDistance, 0, 'f', 1));
                        m_lastControlUiMs = now;
                    }
                    return;
                }
                if (insideGreen || enteredGreen || crossedTargetCenter) {
                    m_pulseDirection = 0;
                    m_pulseEndMs = 0;
                    m_guardSettleUntilMs = now + qMax(20, m_app->settings()->experimentalSettleMs());
                    m_nextPulseMs = qMax(m_nextPulseMs, m_guardSettleUntilMs);
                    m_app->setReelDirection(0);
                    if (m_app->settings()->debugMode() && now - m_lastControlUiMs >= 120) {
                        m_app->setReelControl(QStringLiteral("experimental settle  marker %1  green %2..%3")
                                           .arg(m_markerCenter, 0, 'f', 1)
                                           .arg(m_targetLeft, 0, 'f', 1)
                                           .arg(m_targetRight, 0, 'f', 1));
                        m_lastControlUiMs = now;
                    }
                    return;
                }
            }
            m_app->setReelDirection(m_pulseDirection);
            return;
        }

        m_pulseDirection = 0;
        m_pulseEndMs = 0;
        m_nextPulseMs = qMax(m_nextPulseMs, now + 5);
        m_app->setReelDirection(0);
        return;
    }

    // Dispatch to mode-specific implementation (extracted to separate .cpp files
    // to keep this file manageable).
    const auto mode = m_app->settings()->reelControlMode();
    switch (mode) {
    case AppSettings::ReelControlMode::Experimental:
        tickExperimental();
        break;
    case AppSettings::ReelControlMode::Stable:
        tickStable();
        break;
    case AppSettings::ReelControlMode::ChizukuoPid:
        tickChizukuoPid();
        break;
    }
}
