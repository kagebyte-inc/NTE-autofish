#include "ReelController.h"

#include "AppController.h"
#include "AppSettings.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QString>

void ReelController::tickChizukuoPid()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double targetWidth = qMax(1.0, m_targetRight - m_targetLeft);
    const int frameWidth = m_frameWidth > 0 ? m_frameWidth : 1920;

    // NOTE: This is an adaptation of Chizu's PIDController + humanization pulsing from their _handle_struggling.
    // The core adaptive PID math (sign changes, distance scale, EMA D, conditional I) is ported.
    // However, due to differences in architecture (separate Python vision service sending events vs tight monolithic Python loop,
    // uinput vs pydirectinput, our elements detection of green_target/yellow_marker vs their direct HSV on bar ROI),
    // this mode often does not perform as well as in the original. Stable and Experimental
    // are custom implementations tuned for this codebase. Kept for reference only.
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
}