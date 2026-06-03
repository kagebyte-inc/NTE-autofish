#include "ReelController.h"

#include "AppController.h"
#include "AppSettings.h"

#include <QDateTime>
#include <QString>

void ReelController::tickStable()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double targetWidth = qMax(1.0, m_targetRight - m_targetLeft);
    const int frameWidth = m_frameWidth > 0 ? m_frameWidth : 1920;

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

    // This is the "Stable" implementation (custom, from scratch for our detection + input model).
    // Simple PID + basic intensity-modulated pulse (hold/gap). Not a direct port of Chizu's.
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