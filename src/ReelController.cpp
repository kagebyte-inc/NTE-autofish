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
        QString modeName = QStringLiteral("boundary");
        if (m_app->settings()->reelControlMode() == AppSettings::ReelControlMode::ChizukuoPid) {
            modeName = QStringLiteral("chizukuo pid");
        } else if (m_app->settings()->reelControlMode() == AppSettings::ReelControlMode::LegacyPid) {
            modeName = QStringLiteral("kagebaito pid");
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
            if (m_app->settings()->reelControlMode() == AppSettings::ReelControlMode::KagebaitoGuard) {
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
                        m_app->setReelControl(QStringLiteral("kagebaito guard abort wrong-way  marker %1  target %2")
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
                        m_app->setReelControl(QStringLiteral("kagebaito guard brake %1  dist %2  speed %3  brake %4")
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
                        m_app->setReelControl(QStringLiteral("kagebaito guard settle  marker %1  green %2..%3")
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

    if (m_app->settings()->reelControlMode() == AppSettings::ReelControlMode::Boundary) {
        const double rawMargin = qBound(6.0, targetWidth * 0.18, 24.0);
        const double margin = qMin(rawMargin, targetWidth * 0.42);
        const double safeLeft = m_targetLeft + margin;
        const double safeRight = m_targetRight - margin;

        double error = 0.0;
        int desiredDirection = 0;
        if (m_markerCenter < safeLeft) {
            desiredDirection = 1;
            error = safeLeft - m_markerCenter;
        } else if (m_markerCenter > safeRight) {
            desiredDirection = -1;
            error = m_markerCenter - safeRight;
        }

        if (desiredDirection == 0) {
            m_app->setReelDirection(0);
            if (m_app->settings()->debugMode() && now - m_lastControlUiMs >= 180) {
                m_app->setReelControl(QStringLiteral("boundary holding  marker %1  safe %2..%3")
                                   .arg(m_markerCenter, 0, 'f', 1)
                                   .arg(safeLeft, 0, 'f', 1)
                                   .arg(safeRight, 0, 'f', 1));
                m_lastControlUiMs = now;
            }
            return;
        }

        m_app->setReelDirection(desiredDirection);
        if (m_app->settings()->debugMode() && now - m_lastControlUiMs >= 160) {
            const QString key = desiredDirection < 0 ? QStringLiteral("A") : QStringLiteral("D");
            m_app->setReelControl(QStringLiteral("boundary hold %1  marker %2  safe %3..%4  outside %5")
                               .arg(key)
                               .arg(m_markerCenter, 0, 'f', 1)
                               .arg(safeLeft, 0, 'f', 1)
                               .arg(safeRight, 0, 'f', 1)
                               .arg(error, 0, 'f', 1));
            m_lastControlUiMs = now;
        }
        return;
    }

    if (m_app->settings()->reelControlMode() == AppSettings::ReelControlMode::ChizukuoPid) {
        double dt = 0.033;
        if (m_lastPidMs > 0 && now > m_lastPidMs) {
            dt = qBound(0.001, double(now - m_lastPidMs) / 1000.0, 0.100);
        }
        m_lastPidMs = now;

        if (m_pidFirst) {
            m_pidFirst = false;
            m_pidPreviousMarker = m_markerCenter;
            m_pidDFiltered = 0.0;
            m_pidIntegral = 0.0;
            m_pidLastSign = 0;
            m_pidSignChangeMs.clear();
            m_pidAdaptiveKpScale = 1.0;
        }

        const double error = m_targetCenter - m_markerCenter;
        const double deadband = 5.0;
        if (qAbs(error) >= deadband) {
            const int currentSign = error > 0.0 ? 1 : -1;
            if (m_pidLastSign != 0 && currentSign != m_pidLastSign) {
                m_pidSignChangeMs.append(now);
                while (m_pidSignChangeMs.size() > 8) {
                    m_pidSignChangeMs.removeFirst();
                }
            }
            m_pidLastSign = currentSign;
        }

        int recentSignChanges = 0;
        for (const qint64 signChangeMs : m_pidSignChangeMs) {
            if (now - signChangeMs < 2000) {
                ++recentSignChanges;
            }
        }
        if (recentSignChanges >= 4) {
            m_pidAdaptiveKpScale = qMax(0.4, m_pidAdaptiveKpScale * 0.95);
            m_pidSignChangeMs.clear();
        } else {
            m_pidAdaptiveKpScale = qMin(1.0, m_pidAdaptiveKpScale + 0.02);
        }

        const double barHalfWidth = qMax(targetWidth * 2.0, frameWidth * 0.32);
        const double distanceScale = qBound(0.0, qAbs(error) / qMax(1.0, barHalfWidth), 1.0);
        const double kp = 0.45 * m_pidAdaptiveKpScale * (0.5 + 0.5 * distanceScale);
        const double ki = 0.05;
        const double kd = 0.005;

        const double rawDerivative = -(m_markerCenter - m_pidPreviousMarker) / dt;
        m_pidPreviousMarker = m_markerCenter;
        m_pidDFiltered = rawDerivative * 0.25 + m_pidDFiltered * 0.75;

        const double outputWithoutIntegral = kp * error + kd * m_pidDFiltered;
        if (qAbs(outputWithoutIntegral) < 300.0) {
            if (qAbs(error) >= deadband) {
                m_pidIntegral = qBound(-150.0, m_pidIntegral + error * dt, 150.0);
            } else {
                m_pidIntegral *= 0.90;
            }
        }
        const double output = outputWithoutIntegral + ki * m_pidIntegral;

        int desiredDirection = 0;
        if (output > deadband) {
            desiredDirection = 1;
        } else if (output < -deadband) {
            desiredDirection = -1;
        }

        if (desiredDirection != m_humTargetDirection) {
            m_humTargetDirection = desiredDirection;
            const double latencyFactor = qMax(0.3, 1.0 - distanceScale * 0.7);
            int reactionMs = int(QRandomGenerator::global()->bounded(81) + 40);
            reactionMs = qMax(1, int(reactionMs * latencyFactor));
            if (desiredDirection == 0 || desiredDirection == m_lastAction) {
                reactionMs = qMax(1, int(reactionMs * 0.3));
            }
            m_reactionEndMs = now + reactionMs;
        }

        const int effectiveDirection = now < m_reactionEndMs ? m_lastAction : m_humTargetDirection;
        if (effectiveDirection == 0) {
            m_app->setReelDirection(0);
            m_humPulseState = 0;
            m_humPulseEndMs = 0;
            m_lastAction = 0;
            if (now - m_lastControlUiMs >= 100) {
                if (!m_app->settings()->debugMode()) {
                    return;
                }
                m_app->setReelControl(QStringLiteral("chizukuo none  output %1  error %2  kp-scale %3")
                                   .arg(output, 0, 'f', 1)
                                   .arg(error, 0, 'f', 1)
                                   .arg(m_pidAdaptiveKpScale, 0, 'f', 2));
                m_lastControlUiMs = now;
            }
            return;
        }

        if (m_lastAction != 0 && effectiveDirection != m_lastAction) {
            m_app->setReelDirection(0);
            m_humPulseState = 0;
            m_humPulseEndMs = 0;
        }

        if (m_humPulseState == 0 || now >= m_humPulseEndMs) {
            if (m_humPulseState == 0 || m_humPulseState == 2) {
                const double holdFactor = 1.0 + distanceScale * 0.5;
                const int holdMs = int((QRandomGenerator::global()->bounded(51) + 30) * holdFactor);
                m_humPulseEndMs = now + holdMs;
                m_humPulseState = 1;
                m_app->setReelDirection(effectiveDirection);
            } else {
                const double gapFactor = qMax(0.2, 1.0 - distanceScale * 0.8);
                const int gapMs = qMax(1, int((QRandomGenerator::global()->bounded(18) + 8) * gapFactor));
                m_humPulseEndMs = now + gapMs;
                m_humPulseState = 2;
                m_app->setReelDirection(0);
            }
        }

        m_lastAction = effectiveDirection;
        if (m_app->settings()->debugMode() && now - m_lastControlUiMs >= 160) {
            const QString key = effectiveDirection < 0 ? QStringLiteral("A") : QStringLiteral("D");
            const QString pulseState = m_humPulseState == 1 ? QStringLiteral("hold") : QStringLiteral("gap");
            m_app->setReelControl(QStringLiteral("chizukuo %1 %2  output %3  error %4  kp-scale %5")
                               .arg(pulseState)
                               .arg(key)
                               .arg(output, 0, 'f', 1)
                               .arg(error, 0, 'f', 1)
                               .arg(m_pidAdaptiveKpScale, 0, 'f', 2));
            m_lastControlUiMs = now;
        }
        return;
    }

    if (m_app->settings()->reelControlMode() == AppSettings::ReelControlMode::KagebaitoGuard) {
        double dt = 0.008;
        if (m_lastPidMs > 0 && now > m_lastPidMs) {
            dt = qBound(0.001, double(now - m_lastPidMs) / 1000.0, 0.100);
        }
        m_lastPidMs = now;

        if (m_pidFirst) {
            m_pidFirst = false;
            m_pidPreviousMarker = m_markerCenter;
        }

        const double marginRatio = m_app->settings()->experimentalSafeMarginPercent() / 100.0;
        const double margin = qMin(qBound(5.0, targetWidth * marginRatio, 18.0), targetWidth * 0.42);
        const double safeLeft = m_targetLeft + margin;
        const double safeRight = m_targetRight - margin;
        const bool insideActualGreen = m_markerCenter >= m_targetLeft
            && m_markerCenter <= m_targetRight;
        if (insideActualGreen && now < m_guardSettleUntilMs) {
            const double targetStoppedSpeed = qMax(80.0, targetWidth * 1.8);
            const double markerFastSpeed = qMax(130.0, targetWidth * 2.6);
            int brakeDirection = 0;
            if (qAbs(m_targetVelocity) < targetStoppedSpeed && qAbs(m_markerVelocity) > markerFastSpeed) {
                brakeDirection = m_markerVelocity > 0.0 ? -1 : 1;
            }
            m_app->setReelDirection(brakeDirection);
            if (m_app->settings()->debugMode() && now - m_lastControlUiMs >= 160) {
                m_app->setReelControl(QStringLiteral("kagebaito guard latched %1  marker %2  green %3..%4  v %5/%6")
                                   .arg(brakeDirection == 0 ? QStringLiteral("coast") : QStringLiteral("counter"))
                                   .arg(m_markerCenter, 0, 'f', 1)
                                   .arg(m_targetLeft, 0, 'f', 1)
                                   .arg(m_targetRight, 0, 'f', 1)
                                   .arg(m_markerVelocity, 0, 'f', 0)
                                   .arg(m_targetVelocity, 0, 'f', 0));
                m_lastControlUiMs = now;
            }
            return;
        }

        const double leadSeconds = m_app->settings()->experimentalLeadMs() / 1000.0;
        const double predictedMarker = m_markerCenter + m_markerVelocity * leadSeconds;
        const double predictedLeft = m_targetLeft + m_targetVelocity * leadSeconds + margin;
        const double predictedRight = m_targetRight + m_targetVelocity * leadSeconds - margin;

        const bool outsideLeft = m_markerCenter < safeLeft;
        const bool outsideRight = m_markerCenter > safeRight;
        double error = 0.0;
        int desiredDirection = 0;
        if (insideActualGreen) {
            m_guardSettleUntilMs = qMax(m_guardSettleUntilMs,
                                            now + qBound(20, int(m_app->settings()->experimentalSettleMs() * 0.68), 180));
        } else {
            if (outsideLeft || predictedMarker < predictedLeft) {
                desiredDirection = 1;
                error = qMax(safeLeft - m_markerCenter, predictedLeft - predictedMarker);
            } else if (outsideRight || predictedMarker > predictedRight) {
                desiredDirection = -1;
                error = qMax(m_markerCenter - safeRight, predictedMarker - predictedRight);
            }
        }

        const double relativeVelocity = m_markerVelocity - m_targetVelocity;
        const double safeApproachVelocity = qMax(180.0, targetWidth * 6.0);
        if (!outsideLeft && desiredDirection == 1 && relativeVelocity > safeApproachVelocity) {
            desiredDirection = 0;
        } else if (!outsideRight && desiredDirection == -1 && relativeVelocity < -safeApproachVelocity) {
            desiredDirection = 0;
        }

        if (desiredDirection != 0) {
            const double distanceToGreenEntry = desiredDirection > 0
                ? m_targetLeft - m_markerCenter
                : m_markerCenter - m_targetRight;
            const double closingSpeed = desiredDirection * (m_markerVelocity - m_targetVelocity);
            const double brakeDistance = qMax(5.0, closingSpeed * (m_app->settings()->experimentalBrakeMs() / 1000.0) + qAbs(m_markerVelocity) * 0.018);
            if (distanceToGreenEntry > 0.0
                && closingSpeed > qMax(80.0, targetWidth * 2.0)
                && distanceToGreenEntry <= brakeDistance) {
                m_guardSettleUntilMs = now + qMax(20, m_app->settings()->experimentalSettleMs());
                m_nextPulseMs = qMax(m_nextPulseMs, m_guardSettleUntilMs);
                m_app->setReelDirection(0);
                if (m_app->settings()->debugMode() && now - m_lastControlUiMs >= 160) {
                    m_app->setReelControl(QStringLiteral("kagebaito guard pre-brake  dist %1  speed %2  brake %3")
                                       .arg(distanceToGreenEntry, 0, 'f', 1)
                                       .arg(closingSpeed, 0, 'f', 0)
                                       .arg(brakeDistance, 0, 'f', 1));
                    m_lastControlUiMs = now;
                }
                return;
            }
        }

        if (desiredDirection == 0 || now < m_nextPulseMs) {
            m_app->setReelDirection(0);
            if (m_app->settings()->debugMode() && now - m_lastControlUiMs >= 160) {
                m_app->setReelControl(QStringLiteral("kagebaito guard coast  marker %1  safe %2..%3  v %4/%5")
                                   .arg(m_markerCenter, 0, 'f', 1)
                                   .arg(safeLeft, 0, 'f', 1)
                                   .arg(safeRight, 0, 'f', 1)
                                   .arg(m_markerVelocity, 0, 'f', 0)
                                   .arg(m_targetVelocity, 0, 'f', 0));
                m_lastControlUiMs = now;
            }
            return;
        }

        const bool directionChanged = m_lastAction != 0 && desiredDirection != m_lastAction;
        const double targetAhead = desiredDirection * (m_targetCenter - m_markerCenter);
        const double targetAwaySpeed = desiredDirection * (m_targetVelocity - m_markerVelocity);
        const bool markerOutsideTarget = desiredDirection > 0
            ? m_markerCenter < m_targetLeft
            : m_markerCenter > m_targetRight;
        const bool targetEscaped = markerOutsideTarget || targetAhead > targetWidth * 0.32;
        const bool weakClosing = targetAwaySpeed > -qMax(120.0, targetWidth * 2.0);
        const double intensity = qBound(0.0, error / qMax(1.0, targetWidth), 1.0);
        const bool chaseHold = targetEscaped && weakClosing;
        const int chaseMaxPulseMs = qMax(32, m_app->settings()->experimentalMaxPulseMs());
        const int turnMaxPulseMs = qMax(32, qMin(chaseMaxPulseMs, 82));
        const int minGapMs = qBound(0, m_app->settings()->experimentalMinGapMs(), 30);
        const int pulseMs = chaseHold
            ? qBound(32, int(38.0 + qMax(0.0, targetAhead) * 0.10 + qMax(0.0, targetAwaySpeed) * 0.055), directionChanged ? turnMaxPulseMs : chaseMaxPulseMs)
            : qBound(8, int((directionChanged ? 8.0 : 10.0) + error * 0.08), 30);
        const int gapMs = chaseHold
            ? qBound(minGapMs, int(10.0 - intensity * 4.0), qMax(minGapMs, 10))
            : qBound(qMax(1, minGapMs), int(28.0 - intensity * 8.0), qMax(qMax(1, minGapMs), 28));
        m_pulseDirection = desiredDirection;
        m_pulseEndMs = now + pulseMs;
        m_nextPulseMs = now + pulseMs + gapMs;
        m_lastAction = desiredDirection;
        m_app->setReelDirection(desiredDirection);

        if (m_app->settings()->debugMode() && now - m_lastControlUiMs >= 160) {
            const QString key = desiredDirection < 0 ? QStringLiteral("A") : QStringLiteral("D");
            m_app->setReelControl(QStringLiteral("kagebaito guard %1 %2  marker %3  safe %4..%5  error %6  hold %7/%8")
                               .arg(chaseHold ? QStringLiteral("chase") : QStringLiteral("pulse"))
                               .arg(key)
                               .arg(m_markerCenter, 0, 'f', 1)
                               .arg(safeLeft, 0, 'f', 1)
                               .arg(safeRight, 0, 'f', 1)
                               .arg(error, 0, 'f', 1)
                               .arg(pulseMs)
                               .arg(gapMs));
            m_lastControlUiMs = now;
        }
        return;
    }

    double dt = 0.008;
    if (m_lastPidMs > 0 && now > m_lastPidMs) {
        dt = qBound(0.001, double(now - m_lastPidMs) / 1000.0, 0.100);
    }
    m_lastPidMs = now;

    if (m_pidFirst) {
        m_pidFirst = false;
        m_pidPreviousMarker = m_markerCenter;
        m_pidDFiltered = 0.0;
        m_pidIntegral = 0.0;
    }

    const double error = m_targetCenter - m_markerCenter;
    const double deadband = qBound(5.0, targetWidth * 0.08, 14.0);
    const double derivative = -(m_markerCenter - m_pidPreviousMarker) / dt;
    m_pidPreviousMarker = m_markerCenter;
    m_pidDFiltered = m_pidDFiltered * 0.75 + derivative * 0.25;

    const double kp = 0.45;
    const double ki = 0.025;
    const double kd = 0.004;
    const double outputWithoutIntegral = kp * error + kd * m_pidDFiltered;
    if (qAbs(error) > deadband && qAbs(outputWithoutIntegral) < 300.0) {
        m_pidIntegral = qBound(-150.0, m_pidIntegral + error * dt, 150.0);
    } else {
        m_pidIntegral *= 0.90;
    }

    const double output = outputWithoutIntegral + ki * m_pidIntegral;
    int desiredDirection = 0;
    if (output > deadband) {
        desiredDirection = 1;
    } else if (output < -deadband) {
        desiredDirection = -1;
    }

    if (desiredDirection == 0 || now < m_nextPulseMs) {
        m_app->setReelDirection(0);
        return;
    }

    const double intensity = qBound(0.0, qAbs(error) / qMax(1.0, frameWidth / 2.0), 1.0);
    const int pulseMs = qBound(12, int(18.0 + qAbs(output) * 0.025 + intensity * 18.0), 52);
    const int gapMs = qBound(5, int(18.0 - intensity * 12.0), 18);
    m_pulseDirection = desiredDirection;
    m_pulseEndMs = now + pulseMs;
    m_nextPulseMs = now + pulseMs + gapMs;
    m_app->setReelDirection(desiredDirection);
}
