#pragma once

#include <QObject>
#include <memory>

class AppController;

struct ReelObservation {
    double targetCenter = 0.0;
    double targetLeft = 0.0;
    double targetRight = 0.0;
    double markerCenter = 0.0;
    double targetVelocity = 0.0;
    double markerVelocity = 0.0;
    int frameWidth = 0;
    qint64 timestampMs = 0;
};

class ReelStrategy {
public:
    explicit ReelStrategy(AppController *app);
    virtual ~ReelStrategy() = default;

    virtual void update(const ReelObservation &obs) = 0;
    virtual void reset() = 0;

protected:
    AppController *m_app = nullptr;
};

std::unique_ptr<ReelStrategy> createReelStrategy(int mode, AppController *app);