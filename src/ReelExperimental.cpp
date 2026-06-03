#include "ReelController.h"

#include "AppController.h"
#include "AppSettings.h"

#include <QDateTime>
#include <QString>

void ReelController::tickExperimental()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double targetWidth = qMax(1.0, m_targetRight - m_targetLeft);

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
            m_app->setReelControl(QStringLiteral("experimental latched %1  marker %2  green %3..%4  v %5/%6")
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
                m_app->setReelControl(QStringLiteral("experimental pre-brake  dist %1  speed %2  brake %3")
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
            m_app->setReelControl(QStringLiteral("experimental coast  marker %1  safe %2..%3  v %4/%5")
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
        m_app->setReelControl(QStringLiteral("experimental %1 %2  marker %3  safe %4..%5  error %6  hold %7/%8")
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
}