#pragma once

#include <QJsonObject>
#include <QObject>

class AppController;
class FishingFlowController;

class ReelController final : public QObject
{
    Q_OBJECT

public:
    explicit ReelController(AppController *controller,
                          FishingFlowController *fishing,
                          QObject *parent = nullptr);

    void resetSession();
    void resetControllerState();
    void ingestFrame(const QJsonObject &root);
    void tick();

    double targetCenter() const { return m_targetCenter; }
    double markerCenter() const { return m_markerCenter; }

private:
    AppController *m_app = nullptr;
    FishingFlowController *m_fishing = nullptr;
    bool m_visible = false;
    double m_targetCenter = 0.0;
    double m_targetLeft = 0.0;
    double m_targetRight = 0.0;
    double m_markerCenter = 0.0;
    double m_targetVelocity = 0.0;
    double m_markerVelocity = 0.0;
    double m_previousTargetCenter = 0.0;
    double m_previousMarkerCenter = 0.0;
    int m_frameWidth = 0;
    qint64 m_lastObservationMs = 0;
    qint64 m_previousObservationMs = 0;
    qint64 m_lastControlUiMs = 0;
    qint64 m_lastSeenMs = 0;
    qint64 m_pulseEndMs = 0;
    qint64 m_nextPulseMs = 0;
    qint64 m_guardSettleUntilMs = 0;
    qint64 m_lastPidMs = 0;
    double m_pidIntegral = 0.0;
    double m_pidPreviousMarker = 0.0;
    double m_pidDFiltered = 0.0;
    bool m_pidFirst = true;
    int m_pidLastSign = 0;
    QList<qint64> m_pidSignChangeMs;
    double m_pidAdaptiveKpScale = 1.0;
    qint64 m_reactionEndMs = 0;
    qint64 m_humPulseEndMs = 0;
    int m_humPulseState = 0;
    int m_humTargetDirection = 0;
    int m_lastAction = 0;
    int m_pulseDirection = 0;
};