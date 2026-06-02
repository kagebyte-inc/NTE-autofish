#include "AppController.h"

#include <QDateTime>
#include <QRandomGenerator>

void AppController::updateReelControl()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_reelControlVisible || now - m_lastReelObservationMs > 180) {
        m_reelControlVisible = false;
        m_reelMarkerVelocity = 0.0;
        m_reelTargetVelocity = 0.0;
        resetReelControllerState();
        return;
    }

    const int frameWidth = m_reelFrameWidth > 0 ? m_reelFrameWidth : 1920;
    const double targetWidth = qMax(1.0, m_reelTargetRight - m_reelTargetLeft);
    if (m_reelPulseDirection != 0) {
        if (now < m_reelPulseEndMs) {
            if (m_reelControlMode == ReelControlMode::KagebaitoGuard) {
                const bool insideGreen = m_reelMarkerCenter >= m_reelTargetLeft
                    && m_reelMarkerCenter <= m_reelTargetRight;
                const bool enteredGreen = m_reelPulseDirection > 0
                    ? m_reelMarkerCenter >= m_reelTargetLeft
                    : m_reelMarkerCenter <= m_reelTargetRight;
                const bool crossedTargetCenter = m_reelPulseDirection * (m_reelTargetCenter - m_reelMarkerCenter) <= 0.0;
                const double directionNeed = m_reelTargetCenter - m_reelMarkerCenter;
                const double predictedDirectionNeed = (m_reelTargetCenter + m_reelTargetVelocity * 0.045)
                    - (m_reelMarkerCenter + m_reelMarkerVelocity * 0.045);
                const bool wrongWay = m_reelPulseDirection * directionNeed < -targetWidth * 0.12
                    || m_reelPulseDirection * predictedDirectionNeed < -targetWidth * 0.08;
                const double distanceToGreenEntry = m_reelPulseDirection > 0
                    ? m_reelTargetLeft - m_reelMarkerCenter
                    : m_reelMarkerCenter - m_reelTargetRight;
                const double closingSpeed = m_reelPulseDirection * (m_reelMarkerVelocity - m_reelTargetVelocity);
                const double brakeDistance = qMax(5.0, closingSpeed * (m_experimentalBrakeMs / 1000.0) + qAbs(m_reelMarkerVelocity) * 0.018);
                const bool shouldBrakeBeforeEntry = distanceToGreenEntry > 0.0
                    && closingSpeed > qMax(80.0, targetWidth * 2.0)
                    && distanceToGreenEntry <= brakeDistance;
                if (wrongWay) {
                    m_reelPulseDirection = 0;
                    m_reelPulseEndMs = 0;
                    m_nextReelPulseMs = 0;
                    m_reelGuardSettleUntilMs = 0;
                    setReelDirection(0);
                    if (m_debugMode && now - m_lastReelControlUiMs >= 80) {
                        setReelControl(QStringLiteral("kagebaito guard abort wrong-way  marker %1  target %2")
                                           .arg(m_reelMarkerCenter, 0, 'f', 1)
                                           .arg(m_reelTargetCenter, 0, 'f', 1));
                        m_lastReelControlUiMs = now;
                    }
                    return;
                }
                if (shouldBrakeBeforeEntry) {
                    m_reelPulseDirection = 0;
                    m_reelPulseEndMs = 0;
                    m_reelGuardSettleUntilMs = now + qMax(20, m_experimentalSettleMs);
                    m_nextReelPulseMs = qMax(m_nextReelPulseMs, m_reelGuardSettleUntilMs);
                    const int brakeDirection = closingSpeed > qMax(160.0, targetWidth * 3.0) ? -m_reelDirection : 0;
                    setReelDirection(brakeDirection);
                    if (m_debugMode && now - m_lastReelControlUiMs >= 90) {
                        setReelControl(QStringLiteral("kagebaito guard brake %1  dist %2  speed %3  brake %4")
                                           .arg(brakeDirection == 0 ? QStringLiteral("coast") : QStringLiteral("counter"))
                                           .arg(distanceToGreenEntry, 0, 'f', 1)
                                           .arg(closingSpeed, 0, 'f', 0)
                                           .arg(brakeDistance, 0, 'f', 1));
                        m_lastReelControlUiMs = now;
                    }
                    return;
                }
                if (insideGreen || enteredGreen || crossedTargetCenter) {
                    m_reelPulseDirection = 0;
                    m_reelPulseEndMs = 0;
                    m_reelGuardSettleUntilMs = now + qMax(20, m_experimentalSettleMs);
                    m_nextReelPulseMs = qMax(m_nextReelPulseMs, m_reelGuardSettleUntilMs);
                    setReelDirection(0);
                    if (m_debugMode && now - m_lastReelControlUiMs >= 120) {
                        setReelControl(QStringLiteral("kagebaito guard settle  marker %1  green %2..%3")
                                           .arg(m_reelMarkerCenter, 0, 'f', 1)
                                           .arg(m_reelTargetLeft, 0, 'f', 1)
                                           .arg(m_reelTargetRight, 0, 'f', 1));
                        m_lastReelControlUiMs = now;
                    }
                    return;
                }
            }
            setReelDirection(m_reelPulseDirection);
            return;
        }

        m_reelPulseDirection = 0;
        m_reelPulseEndMs = 0;
        m_nextReelPulseMs = qMax(m_nextReelPulseMs, now + 5);
        setReelDirection(0);
        return;
    }

    if (m_reelControlMode == ReelControlMode::Boundary) {
        const double rawMargin = qBound(6.0, targetWidth * 0.18, 24.0);
        const double margin = qMin(rawMargin, targetWidth * 0.42);
        const double safeLeft = m_reelTargetLeft + margin;
        const double safeRight = m_reelTargetRight - margin;

        double error = 0.0;
        int desiredDirection = 0;
        if (m_reelMarkerCenter < safeLeft) {
            desiredDirection = 1;
            error = safeLeft - m_reelMarkerCenter;
        } else if (m_reelMarkerCenter > safeRight) {
            desiredDirection = -1;
            error = m_reelMarkerCenter - safeRight;
        }

        if (desiredDirection == 0) {
            setReelDirection(0);
            if (m_debugMode && now - m_lastReelControlUiMs >= 180) {
                setReelControl(QStringLiteral("boundary holding  marker %1  safe %2..%3")
                                   .arg(m_reelMarkerCenter, 0, 'f', 1)
                                   .arg(safeLeft, 0, 'f', 1)
                                   .arg(safeRight, 0, 'f', 1));
                m_lastReelControlUiMs = now;
            }
            return;
        }

        setReelDirection(desiredDirection);
        if (m_debugMode && now - m_lastReelControlUiMs >= 160) {
            const QString key = desiredDirection < 0 ? QStringLiteral("A") : QStringLiteral("D");
            setReelControl(QStringLiteral("boundary hold %1  marker %2  safe %3..%4  outside %5")
                               .arg(key)
                               .arg(m_reelMarkerCenter, 0, 'f', 1)
                               .arg(safeLeft, 0, 'f', 1)
                               .arg(safeRight, 0, 'f', 1)
                               .arg(error, 0, 'f', 1));
            m_lastReelControlUiMs = now;
        }
        return;
    }

    if (m_reelControlMode == ReelControlMode::ChizukuoPid) {
        double dt = 0.033;
        if (m_lastReelPidMs > 0 && now > m_lastReelPidMs) {
            dt = qBound(0.001, double(now - m_lastReelPidMs) / 1000.0, 0.100);
        }
        m_lastReelPidMs = now;

        if (m_reelPidFirst) {
            m_reelPidFirst = false;
            m_reelPidPreviousMarker = m_reelMarkerCenter;
            m_reelPidDFiltered = 0.0;
            m_reelPidIntegral = 0.0;
            m_reelPidLastSign = 0;
            m_reelPidSignChangeMs.clear();
            m_reelPidAdaptiveKpScale = 1.0;
        }

        const double error = m_reelTargetCenter - m_reelMarkerCenter;
        const double deadband = 5.0;
        if (qAbs(error) >= deadband) {
            const int currentSign = error > 0.0 ? 1 : -1;
            if (m_reelPidLastSign != 0 && currentSign != m_reelPidLastSign) {
                m_reelPidSignChangeMs.append(now);
                while (m_reelPidSignChangeMs.size() > 8) {
                    m_reelPidSignChangeMs.removeFirst();
                }
            }
            m_reelPidLastSign = currentSign;
        }

        int recentSignChanges = 0;
        for (const qint64 signChangeMs : m_reelPidSignChangeMs) {
            if (now - signChangeMs < 2000) {
                ++recentSignChanges;
            }
        }
        if (recentSignChanges >= 4) {
            m_reelPidAdaptiveKpScale = qMax(0.4, m_reelPidAdaptiveKpScale * 0.95);
            m_reelPidSignChangeMs.clear();
        } else {
            m_reelPidAdaptiveKpScale = qMin(1.0, m_reelPidAdaptiveKpScale + 0.02);
        }

        const double barHalfWidth = qMax(targetWidth * 2.0, frameWidth * 0.32);
        const double distanceScale = qBound(0.0, qAbs(error) / qMax(1.0, barHalfWidth), 1.0);
        const double kp = 0.45 * m_reelPidAdaptiveKpScale * (0.5 + 0.5 * distanceScale);
        const double ki = 0.05;
        const double kd = 0.005;

        const double rawDerivative = -(m_reelMarkerCenter - m_reelPidPreviousMarker) / dt;
        m_reelPidPreviousMarker = m_reelMarkerCenter;
        m_reelPidDFiltered = rawDerivative * 0.25 + m_reelPidDFiltered * 0.75;

        const double outputWithoutIntegral = kp * error + kd * m_reelPidDFiltered;
        if (qAbs(outputWithoutIntegral) < 300.0) {
            if (qAbs(error) >= deadband) {
                m_reelPidIntegral = qBound(-150.0, m_reelPidIntegral + error * dt, 150.0);
            } else {
                m_reelPidIntegral *= 0.90;
            }
        }
        const double output = outputWithoutIntegral + ki * m_reelPidIntegral;

        int desiredDirection = 0;
        if (output > deadband) {
            desiredDirection = 1;
        } else if (output < -deadband) {
            desiredDirection = -1;
        }

        if (desiredDirection != m_reelHumTargetDirection) {
            m_reelHumTargetDirection = desiredDirection;
            const double latencyFactor = qMax(0.3, 1.0 - distanceScale * 0.7);
            int reactionMs = int(QRandomGenerator::global()->bounded(81) + 40);
            reactionMs = qMax(1, int(reactionMs * latencyFactor));
            if (desiredDirection == 0 || desiredDirection == m_reelLastAction) {
                reactionMs = qMax(1, int(reactionMs * 0.3));
            }
            m_reelReactionEndMs = now + reactionMs;
        }

        const int effectiveDirection = now < m_reelReactionEndMs ? m_reelLastAction : m_reelHumTargetDirection;
        if (effectiveDirection == 0) {
            setReelDirection(0);
            m_reelHumPulseState = 0;
            m_reelHumPulseEndMs = 0;
            m_reelLastAction = 0;
            if (now - m_lastReelControlUiMs >= 100) {
                if (!m_debugMode) {
                    return;
                }
                setReelControl(QStringLiteral("chizukuo none  output %1  error %2  kp-scale %3")
                                   .arg(output, 0, 'f', 1)
                                   .arg(error, 0, 'f', 1)
                                   .arg(m_reelPidAdaptiveKpScale, 0, 'f', 2));
                m_lastReelControlUiMs = now;
            }
            return;
        }

        if (m_reelLastAction != 0 && effectiveDirection != m_reelLastAction) {
            setReelDirection(0);
            m_reelHumPulseState = 0;
            m_reelHumPulseEndMs = 0;
        }

        if (m_reelHumPulseState == 0 || now >= m_reelHumPulseEndMs) {
            if (m_reelHumPulseState == 0 || m_reelHumPulseState == 2) {
                const double holdFactor = 1.0 + distanceScale * 0.5;
                const int holdMs = int((QRandomGenerator::global()->bounded(51) + 30) * holdFactor);
                m_reelHumPulseEndMs = now + holdMs;
                m_reelHumPulseState = 1;
                setReelDirection(effectiveDirection);
            } else {
                const double gapFactor = qMax(0.2, 1.0 - distanceScale * 0.8);
                const int gapMs = qMax(1, int((QRandomGenerator::global()->bounded(18) + 8) * gapFactor));
                m_reelHumPulseEndMs = now + gapMs;
                m_reelHumPulseState = 2;
                setReelDirection(0);
            }
        }

        m_reelLastAction = effectiveDirection;
        if (m_debugMode && now - m_lastReelControlUiMs >= 160) {
            const QString key = effectiveDirection < 0 ? QStringLiteral("A") : QStringLiteral("D");
            const QString pulseState = m_reelHumPulseState == 1 ? QStringLiteral("hold") : QStringLiteral("gap");
            setReelControl(QStringLiteral("chizukuo %1 %2  output %3  error %4  kp-scale %5")
                               .arg(pulseState)
                               .arg(key)
                               .arg(output, 0, 'f', 1)
                               .arg(error, 0, 'f', 1)
                               .arg(m_reelPidAdaptiveKpScale, 0, 'f', 2));
            m_lastReelControlUiMs = now;
        }
        return;
    }

    if (m_reelControlMode == ReelControlMode::KagebaitoGuard) {
        double dt = 0.008;
        if (m_lastReelPidMs > 0 && now > m_lastReelPidMs) {
            dt = qBound(0.001, double(now - m_lastReelPidMs) / 1000.0, 0.100);
        }
        m_lastReelPidMs = now;

        if (m_reelPidFirst) {
            m_reelPidFirst = false;
            m_reelPidPreviousMarker = m_reelMarkerCenter;
        }

        const double marginRatio = m_experimentalSafeMarginPercent / 100.0;
        const double margin = qMin(qBound(5.0, targetWidth * marginRatio, 18.0), targetWidth * 0.42);
        const double safeLeft = m_reelTargetLeft + margin;
        const double safeRight = m_reelTargetRight - margin;
        const bool insideActualGreen = m_reelMarkerCenter >= m_reelTargetLeft
            && m_reelMarkerCenter <= m_reelTargetRight;
        if (insideActualGreen && now < m_reelGuardSettleUntilMs) {
            const double targetStoppedSpeed = qMax(80.0, targetWidth * 1.8);
            const double markerFastSpeed = qMax(130.0, targetWidth * 2.6);
            int brakeDirection = 0;
            if (qAbs(m_reelTargetVelocity) < targetStoppedSpeed && qAbs(m_reelMarkerVelocity) > markerFastSpeed) {
                brakeDirection = m_reelMarkerVelocity > 0.0 ? -1 : 1;
            }
            setReelDirection(brakeDirection);
            if (m_debugMode && now - m_lastReelControlUiMs >= 160) {
                setReelControl(QStringLiteral("kagebaito guard latched %1  marker %2  green %3..%4  v %5/%6")
                                   .arg(brakeDirection == 0 ? QStringLiteral("coast") : QStringLiteral("counter"))
                                   .arg(m_reelMarkerCenter, 0, 'f', 1)
                                   .arg(m_reelTargetLeft, 0, 'f', 1)
                                   .arg(m_reelTargetRight, 0, 'f', 1)
                                   .arg(m_reelMarkerVelocity, 0, 'f', 0)
                                   .arg(m_reelTargetVelocity, 0, 'f', 0));
                m_lastReelControlUiMs = now;
            }
            return;
        }

        const double leadSeconds = m_experimentalLeadMs / 1000.0;
        const double predictedMarker = m_reelMarkerCenter + m_reelMarkerVelocity * leadSeconds;
        const double predictedLeft = m_reelTargetLeft + m_reelTargetVelocity * leadSeconds + margin;
        const double predictedRight = m_reelTargetRight + m_reelTargetVelocity * leadSeconds - margin;

        const bool outsideLeft = m_reelMarkerCenter < safeLeft;
        const bool outsideRight = m_reelMarkerCenter > safeRight;
        double error = 0.0;
        int desiredDirection = 0;
        if (insideActualGreen) {
            m_reelGuardSettleUntilMs = qMax(m_reelGuardSettleUntilMs,
                                            now + qBound(20, int(m_experimentalSettleMs * 0.68), 180));
        } else {
            if (outsideLeft || predictedMarker < predictedLeft) {
                desiredDirection = 1;
                error = qMax(safeLeft - m_reelMarkerCenter, predictedLeft - predictedMarker);
            } else if (outsideRight || predictedMarker > predictedRight) {
                desiredDirection = -1;
                error = qMax(m_reelMarkerCenter - safeRight, predictedMarker - predictedRight);
            }
        }

        const double relativeVelocity = m_reelMarkerVelocity - m_reelTargetVelocity;
        const double safeApproachVelocity = qMax(180.0, targetWidth * 6.0);
        if (!outsideLeft && desiredDirection == 1 && relativeVelocity > safeApproachVelocity) {
            desiredDirection = 0;
        } else if (!outsideRight && desiredDirection == -1 && relativeVelocity < -safeApproachVelocity) {
            desiredDirection = 0;
        }

        if (desiredDirection != 0) {
            const double distanceToGreenEntry = desiredDirection > 0
                ? m_reelTargetLeft - m_reelMarkerCenter
                : m_reelMarkerCenter - m_reelTargetRight;
            const double closingSpeed = desiredDirection * (m_reelMarkerVelocity - m_reelTargetVelocity);
            const double brakeDistance = qMax(5.0, closingSpeed * (m_experimentalBrakeMs / 1000.0) + qAbs(m_reelMarkerVelocity) * 0.018);
            if (distanceToGreenEntry > 0.0
                && closingSpeed > qMax(80.0, targetWidth * 2.0)
                && distanceToGreenEntry <= brakeDistance) {
                m_reelGuardSettleUntilMs = now + qMax(20, m_experimentalSettleMs);
                m_nextReelPulseMs = qMax(m_nextReelPulseMs, m_reelGuardSettleUntilMs);
                setReelDirection(0);
                if (m_debugMode && now - m_lastReelControlUiMs >= 160) {
                    setReelControl(QStringLiteral("kagebaito guard pre-brake  dist %1  speed %2  brake %3")
                                       .arg(distanceToGreenEntry, 0, 'f', 1)
                                       .arg(closingSpeed, 0, 'f', 0)
                                       .arg(brakeDistance, 0, 'f', 1));
                    m_lastReelControlUiMs = now;
                }
                return;
            }
        }

        if (desiredDirection == 0 || now < m_nextReelPulseMs) {
            setReelDirection(0);
            if (m_debugMode && now - m_lastReelControlUiMs >= 160) {
                setReelControl(QStringLiteral("kagebaito guard coast  marker %1  safe %2..%3  v %4/%5")
                                   .arg(m_reelMarkerCenter, 0, 'f', 1)
                                   .arg(safeLeft, 0, 'f', 1)
                                   .arg(safeRight, 0, 'f', 1)
                                   .arg(m_reelMarkerVelocity, 0, 'f', 0)
                                   .arg(m_reelTargetVelocity, 0, 'f', 0));
                m_lastReelControlUiMs = now;
            }
            return;
        }

        const bool directionChanged = m_reelLastAction != 0 && desiredDirection != m_reelLastAction;
        const double targetAhead = desiredDirection * (m_reelTargetCenter - m_reelMarkerCenter);
        const double targetAwaySpeed = desiredDirection * (m_reelTargetVelocity - m_reelMarkerVelocity);
        const bool markerOutsideTarget = desiredDirection > 0
            ? m_reelMarkerCenter < m_reelTargetLeft
            : m_reelMarkerCenter > m_reelTargetRight;
        const bool targetEscaped = markerOutsideTarget || targetAhead > targetWidth * 0.32;
        const bool weakClosing = targetAwaySpeed > -qMax(120.0, targetWidth * 2.0);
        const double intensity = qBound(0.0, error / qMax(1.0, targetWidth), 1.0);
        const bool chaseHold = targetEscaped && weakClosing;
        const int chaseMaxPulseMs = qMax(32, m_experimentalMaxPulseMs);
        const int turnMaxPulseMs = qMax(32, qMin(chaseMaxPulseMs, 82));
        const int minGapMs = qBound(0, m_experimentalMinGapMs, 30);
        const int pulseMs = chaseHold
            ? qBound(32, int(38.0 + qMax(0.0, targetAhead) * 0.10 + qMax(0.0, targetAwaySpeed) * 0.055), directionChanged ? turnMaxPulseMs : chaseMaxPulseMs)
            : qBound(8, int((directionChanged ? 8.0 : 10.0) + error * 0.08), 30);
        const int gapMs = chaseHold
            ? qBound(minGapMs, int(10.0 - intensity * 4.0), qMax(minGapMs, 10))
            : qBound(qMax(1, minGapMs), int(28.0 - intensity * 8.0), qMax(qMax(1, minGapMs), 28));
        m_reelPulseDirection = desiredDirection;
        m_reelPulseEndMs = now + pulseMs;
        m_nextReelPulseMs = now + pulseMs + gapMs;
        m_reelLastAction = desiredDirection;
        setReelDirection(desiredDirection);

        if (m_debugMode && now - m_lastReelControlUiMs >= 160) {
            const QString key = desiredDirection < 0 ? QStringLiteral("A") : QStringLiteral("D");
            setReelControl(QStringLiteral("kagebaito guard %1 %2  marker %3  safe %4..%5  error %6  hold %7/%8")
                               .arg(chaseHold ? QStringLiteral("chase") : QStringLiteral("pulse"))
                               .arg(key)
                               .arg(m_reelMarkerCenter, 0, 'f', 1)
                               .arg(safeLeft, 0, 'f', 1)
                               .arg(safeRight, 0, 'f', 1)
                               .arg(error, 0, 'f', 1)
                               .arg(pulseMs)
                               .arg(gapMs));
            m_lastReelControlUiMs = now;
        }
        return;
    }

    double dt = 0.008;
    if (m_lastReelPidMs > 0 && now > m_lastReelPidMs) {
        dt = qBound(0.001, double(now - m_lastReelPidMs) / 1000.0, 0.100);
    }
    m_lastReelPidMs = now;

    if (m_reelPidFirst) {
        m_reelPidFirst = false;
        m_reelPidPreviousMarker = m_reelMarkerCenter;
        m_reelPidDFiltered = 0.0;
        m_reelPidIntegral = 0.0;
    }

    const double error = m_reelTargetCenter - m_reelMarkerCenter;
    const double deadband = qBound(5.0, targetWidth * 0.08, 14.0);
    const double derivative = -(m_reelMarkerCenter - m_reelPidPreviousMarker) / dt;
    m_reelPidPreviousMarker = m_reelMarkerCenter;
    m_reelPidDFiltered = m_reelPidDFiltered * 0.75 + derivative * 0.25;

    const double kp = 0.45;
    const double ki = 0.025;
    const double kd = 0.004;
    const double outputWithoutIntegral = kp * error + kd * m_reelPidDFiltered;
    if (qAbs(error) > deadband && qAbs(outputWithoutIntegral) < 300.0) {
        m_reelPidIntegral = qBound(-150.0, m_reelPidIntegral + error * dt, 150.0);
    } else {
        m_reelPidIntegral *= 0.90;
    }

    const double output = outputWithoutIntegral + ki * m_reelPidIntegral;
    int desiredDirection = 0;
    if (output > deadband) {
        desiredDirection = 1;
    } else if (output < -deadband) {
        desiredDirection = -1;
    }

    if (desiredDirection == 0 || now < m_nextReelPulseMs) {
        setReelDirection(0);
        return;
    }

    const double intensity = qBound(0.0, qAbs(error) / qMax(1.0, frameWidth / 2.0), 1.0);
    const int pulseMs = qBound(12, int(18.0 + qAbs(output) * 0.025 + intensity * 18.0), 52);
    const int gapMs = qBound(5, int(18.0 - intensity * 12.0), 18);
    m_reelPulseDirection = desiredDirection;
    m_reelPulseEndMs = now + pulseMs;
    m_nextReelPulseMs = now + pulseMs + gapMs;
    setReelDirection(desiredDirection);
}
