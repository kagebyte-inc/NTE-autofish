#include "AppController.h"

#include <QDateTime>
#include <QMessageBox>

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    loadUiSettings();

    m_fishingEventResetTimer.setSingleShot(true);
    m_fishingEventResetTimer.setInterval(2000);
    m_hookActionTimer.setSingleShot(true);
    m_manualResultScreenTimer.setSingleShot(true);
    m_resultCloseTimer.setSingleShot(true);
    m_recastTimer.setSingleShot(true);
    m_reelControlTimer.setInterval(8);
    m_reelControlTimer.setTimerType(Qt::PreciseTimer);

    connect(&m_visionProcess, &QProcess::readyReadStandardOutput,
            this, &AppController::onVisionOutput);
    connect(&m_visionProcess, &QProcess::readyReadStandardError,
            this, &AppController::onVisionErrorOutput);
    connect(&m_visionProcess, &QProcess::finished,
            this, &AppController::onVisionFinished);
    connect(&m_inputProcess, &QProcess::readyReadStandardOutput,
            this, &AppController::onInputOutput);
    connect(&m_inputProcess, &QProcess::readyReadStandardError,
            this, &AppController::onInputErrorOutput);
    connect(&m_inputProcess, &QProcess::finished,
            this, &AppController::onInputFinished);
    connect(&m_fishingEventResetTimer, &QTimer::timeout,
            this, &AppController::resetFishingEvent);
    connect(&m_hookActionTimer, &QTimer::timeout,
            this, &AppController::sendHookAction);
    connect(&m_manualResultScreenTimer, &QTimer::timeout,
            this, &AppController::sendManualResultScreenAction);
    connect(&m_resultCloseTimer, &QTimer::timeout,
            this, &AppController::sendResultCloseAction);
    connect(&m_recastTimer, &QTimer::timeout,
            this, &AppController::sendRecastAction);
    connect(&m_reelControlTimer, &QTimer::timeout, this, [this]() {
        updateReelControl();
        updateFishingFlowWatchdog();
    });
    m_fishingFlowStateEnteredMs = QDateTime::currentMSecsSinceEpoch();
    m_reelControlTimer.start();
}

AppController::~AppController()
{
    setReelDirection(0);
    destroyUinput();
}

QString AppController::status() const
{
    return m_status;
}

QString AppController::lastEvent() const
{
    return m_lastEvent;
}

QString AppController::rawEvents() const
{
    return m_rawEvents;
}

QString AppController::fishingEvent() const
{
    return m_fishingEvent;
}

QString AppController::fishingLog() const
{
    return m_fishingLog;
}

QString AppController::reelControl() const
{
    return m_reelControl;
}

bool AppController::usePidReelControl() const
{
    return m_reelControlMode == ReelControlMode::LegacyPid;
}

int AppController::reelControlMode() const
{
    return static_cast<int>(m_reelControlMode);
}

bool AppController::saveDebugFrames() const
{
    return m_saveDebugFrames;
}

bool AppController::writeLogsToFile() const
{
    return m_writeLogsToFile;
}

bool AppController::setupComplete() const
{
    return m_setupComplete;
}

QString AppController::platform() const
{
    return m_platform;
}

QString AppController::captureBackend() const
{
    return m_captureBackend;
}

QString AppController::experienceMode() const
{
    return m_experienceMode;
}

bool AppController::debugMode() const
{
    return m_debugMode;
}

int AppController::experimentalLeadMs() const
{
    return m_experimentalLeadMs;
}

int AppController::experimentalSafeMarginPercent() const
{
    return m_experimentalSafeMarginPercent;
}

int AppController::experimentalSettleMs() const
{
    return m_experimentalSettleMs;
}

int AppController::experimentalBrakeMs() const
{
    return m_experimentalBrakeMs;
}

int AppController::experimentalMaxPulseMs() const
{
    return m_experimentalMaxPulseMs;
}

int AppController::experimentalMinGapMs() const
{
    return m_experimentalMinGapMs;
}

QVariantList AppController::windows() const
{
    return m_windows;
}

int AppController::selectedWindowIndex() const
{
    return m_selectedWindowIndex;
}

QString AppController::selectedWindowLabel() const
{
    if (m_selectedWindowIndex < 0 || m_selectedWindowIndex >= m_windows.size()) {
        return QStringLiteral("No window selected");
    }

    const QVariantMap window = m_windows.at(m_selectedWindowIndex).toMap();
    const QString process = window.value(QStringLiteral("process_name")).toString();
    const QString title = window.value(QStringLiteral("title")).toString();
    if (process.isEmpty()) {
        return title;
    }
    return QStringLiteral("%1 - %2").arg(process, title);
}

QString AppController::appVersion() const
{
#ifdef AUTOFISH_VERSION
    return QStringLiteral(AUTOFISH_VERSION);
#else
    return QStringLiteral("0.0.0");
#endif
}

QString AppController::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString AppController::uiLanguage() const
{
    return m_uiLanguage;
}

void AppController::showAboutQt()
{
    QMessageBox::aboutQt(nullptr);
}
