#include "AppController.h"

#include "InputDevice.h"
#include "VisionService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QImage>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_input(this)
    , m_vision(this)
    , m_fishing(this, this)
    , m_reel(this, &m_fishing, this)
{
    m_reelControlTimer.setInterval(8);
    m_reelControlTimer.setTimerType(Qt::PreciseTimer);

    connect(&m_inputProcess, &QProcess::readyReadStandardOutput, this, &AppController::onInputOutput);
    connect(&m_inputProcess, &QProcess::readyReadStandardError, this, &AppController::onInputErrorOutput);
    connect(&m_inputProcess, &QProcess::finished, this, &AppController::onInputFinished);
    connect(&m_reelControlTimer, &QTimer::timeout, this, [this]() {
        m_reel.tick();
        m_fishing.updateWatchdog();
    });

    connect(&m_settings, &AppSettings::setupChanged, this, &AppController::setupChanged);
    connect(&m_settings, &AppSettings::setupCompleteChanged, this, &AppController::setupCompleteChanged);
    connect(&m_settings, &AppSettings::experienceModeChanged, this, &AppController::experienceModeChanged);
    connect(&m_settings, &AppSettings::debugModeChanged, this, &AppController::debugModeChanged);
    connect(&m_settings, &AppSettings::uiLanguageChanged, this, &AppController::uiLanguageChanged);
    connect(&m_settings, &AppSettings::saveDebugFramesChanged, this, &AppController::saveDebugFramesChanged);
    connect(&m_settings, &AppSettings::writeLogsToFileChanged, this, &AppController::writeLogsToFileChanged);
    connect(&m_settings, &AppSettings::reelControlModeChanged, this, [this]() {
        resetReelControllerState();
        emit reelControlModeChanged();
        emit usePidReelControlChanged();
    });
    connect(&m_settings, &AppSettings::experimentalTuningChanged, this, [this]() {
        resetReelControllerState();
        emit experimentalTuningChanged();
    });
    connect(&m_settings, &AppSettings::fishingTuningChanged, this, &AppController::fishingTuningChanged);

    connect(&m_vision, &VisionService::windowsChanged, this, &AppController::windowsChanged);
    connect(&m_vision, &VisionService::selectedWindowChanged, this, &AppController::selectedWindowChanged);
    connect(&m_vision, &VisionService::windowPickerRequired, this, &AppController::windowPickerRequired);
    connect(&m_vision, &VisionService::statusMessage, this, &AppController::setLastEvent);

    m_reelControlTimer.start();
    refreshDiagnostics();
}

AppController::~AppController()
{
    setReelDirection(0);
    m_input.destroy();
}

AppSettings *AppController::settings() { return &m_settings; }
const AppSettings *AppController::settings() const { return &m_settings; }
InputDevice *AppController::input() { return &m_input; }
VisionService *AppController::vision() { return &m_vision; }
FishingFlowController *AppController::fishing() { return &m_fishing; }
ReelController *AppController::reel() { return &m_reel; }

QString AppController::status() const { return m_status; }
QString AppController::lastEvent() const { return m_lastEvent; }
QString AppController::rawEvents() const { return m_rawEvents; }
QString AppController::fishingEvent() const { return m_fishingEvent; }
QString AppController::fishingLog() const { return m_fishingLog; }
QString AppController::reelControl() const { return m_reelControl; }

bool AppController::usePidReelControl() const
{
    return m_settings.reelControlMode() == ReelControlMode::Stable;
}

int AppController::reelControlMode() const { return m_settings.reelControlModeInt(); }

bool AppController::saveDebugFrames() const { return m_settings.saveDebugFrames(); }

bool AppController::writeLogsToFile() const { return m_settings.writeLogsToFile(); }

bool AppController::setupComplete() const { return m_settings.setupComplete(); }
QString AppController::platform() const { return m_settings.platform(); }
QString AppController::captureBackend() const { return m_settings.captureBackend(); }
QString AppController::experienceMode() const { return m_settings.experienceMode(); }
void AppController::setExperienceMode(const QString &mode) { m_settings.setExperienceMode(mode); }
bool AppController::debugMode() const { return m_settings.debugMode(); }
void AppController::setDebugMode(bool enabled) { m_settings.setDebugMode(enabled); }

int AppController::experimentalLeadMs() const { return m_settings.experimentalLeadMs(); }
void AppController::setExperimentalLeadMs(int value) { m_settings.setExperimentalLeadMs(value); resetReelControllerState(); }
int AppController::experimentalSafeMarginPercent() const { return m_settings.experimentalSafeMarginPercent(); }
void AppController::setExperimentalSafeMarginPercent(int value) { m_settings.setExperimentalSafeMarginPercent(value); resetReelControllerState(); }
int AppController::experimentalSettleMs() const { return m_settings.experimentalSettleMs(); }
void AppController::setExperimentalSettleMs(int value) { m_settings.setExperimentalSettleMs(value); resetReelControllerState(); }
int AppController::experimentalBrakeMs() const { return m_settings.experimentalBrakeMs(); }
void AppController::setExperimentalBrakeMs(int value) { m_settings.setExperimentalBrakeMs(value); resetReelControllerState(); }
int AppController::experimentalMaxPulseMs() const { return m_settings.experimentalMaxPulseMs(); }
void AppController::setExperimentalMaxPulseMs(int value) { m_settings.setExperimentalMaxPulseMs(value); resetReelControllerState(); }
int AppController::experimentalMinGapMs() const { return m_settings.experimentalMinGapMs(); }
void AppController::setExperimentalMinGapMs(int value) { m_settings.setExperimentalMinGapMs(value); resetReelControllerState(); }

int AppController::fishingHookDelayMs() const { return m_settings.fishingHookDelayMs(); }
void AppController::setFishingHookDelayMs(int value) { m_settings.setFishingHookDelayMs(value); }
int AppController::fishingHookJitterMs() const { return m_settings.fishingHookJitterMs(); }
void AppController::setFishingHookJitterMs(int value) { m_settings.setFishingHookJitterMs(value); }
int AppController::fishingEventResetMs() const { return m_settings.fishingEventResetMs(); }
void AppController::setFishingEventResetMs(int value) { m_settings.setFishingEventResetMs(value); }
int AppController::fishingResultEscBaseMs() const { return m_settings.fishingResultEscBaseMs(); }
void AppController::setFishingResultEscBaseMs(int value) { m_settings.setFishingResultEscBaseMs(value); }
int AppController::fishingResultEscJitterMs() const { return m_settings.fishingResultEscJitterMs(); }
void AppController::setFishingResultEscJitterMs(int value) { m_settings.setFishingResultEscJitterMs(value); }
int AppController::fishingRecastBaseMs() const { return m_settings.fishingRecastBaseMs(); }
void AppController::setFishingRecastBaseMs(int value) { m_settings.setFishingRecastBaseMs(value); }
int AppController::fishingRecastJitterMs() const { return m_settings.fishingRecastJitterMs(); }
void AppController::setFishingRecastJitterMs(int value) { m_settings.setFishingRecastJitterMs(value); }
int AppController::fishingAwaitingReelMs() const { return m_settings.fishingAwaitingReelMs(); }
void AppController::setFishingAwaitingReelMs(int value) { m_settings.setFishingAwaitingReelMs(value); }
int AppController::fishingReelLostMs() const { return m_settings.fishingReelLostMs(); }
void AppController::setFishingReelLostMs(int value) { m_settings.setFishingReelLostMs(value); }
int AppController::fishingHookRetryMs() const { return m_settings.fishingHookRetryMs(); }
void AppController::setFishingHookRetryMs(int value) { m_settings.setFishingHookRetryMs(value); }
int AppController::fishingHookVisibleGraceMs() const { return m_settings.fishingHookVisibleGraceMs(); }
void AppController::setFishingHookVisibleGraceMs(int value) { m_settings.setFishingHookVisibleGraceMs(value); }
int AppController::fishingRecoveryBaseMs() const { return m_settings.fishingRecoveryBaseMs(); }
void AppController::setFishingRecoveryBaseMs(int value) { m_settings.setFishingRecoveryBaseMs(value); }
int AppController::fishingRecoveryJitterMs() const { return m_settings.fishingRecoveryJitterMs(); }
void AppController::setFishingRecoveryJitterMs(int value) { m_settings.setFishingRecoveryJitterMs(value); }

QVariantList AppController::windows() const { return m_vision.windows(); }
int AppController::selectedWindowIndex() const { return m_vision.selectedWindowIndex(); }
QString AppController::selectedWindowLabel() const { return m_vision.selectedWindowLabel(); }

QString AppController::appVersion() const
{
#ifdef AUTOFISH_VERSION
    return QStringLiteral(AUTOFISH_VERSION);
#else
    return QStringLiteral("0.0.0");
#endif
}

QString AppController::qtVersion() const { return QString::fromLatin1(qVersion()); }

bool AppController::windowsSetupAvailable() const
{
#ifdef Q_OS_LINUX
    return false;
#else
    return true;
#endif
}

QString AppController::uiLanguage() const { return m_settings.uiLanguage(); }
void AppController::setUiLanguage(const QString &language) { m_settings.setUiLanguage(language); }

void AppController::refreshWindows() { m_vision.refreshWindows(); setStatus(QStringLiteral("idle")); }

void AppController::selectWindow(int index) { m_vision.selectWindow(index); }

void AppController::startCapture()
{
    const QString backend = m_settings.captureBackend();
    if (m_settings.platform() == QStringLiteral("Wayland") || backend == QStringLiteral("PipeWire Portal")) {
        startPortalVision();
        return;
    }
    if (backend == QStringLiteral("X11 Window")) {
        refreshWindows();
        emit windowPickerRequired();
        return;
    }
    startVision();
}

void AppController::startVision()
{
    m_vision.startWatch();
    setStatus(QStringLiteral("watching"));
}

void AppController::startPortalVision()
{
    m_vision.startPortal();
    setStatus(QStringLiteral("watching"));
}

void AppController::stopVision()
{
    m_vision.stop();
    onVisionStopped(0);
}

void AppController::resetExperimentalTuning()
{
    m_settings.resetExperimentalTuning();
    resetReelControllerState();
    setReelControl(QStringLiteral("experimental tuning reset"));
}

void AppController::resetFishingTuning()
{
    m_settings.resetFishingTuning();
    setLastEvent(QStringLiteral("Fishing timing reset"));
}

void AppController::showAboutQt()
{
    QMessageBox::aboutQt(nullptr);
}

void AppController::prepareVisionSession()
{
    m_fishing.resetSession();
    m_reel.resetSession();
    refreshDiagnostics();
    m_captureFrameWidth = 0;
    m_captureFrameHeight = 0;
    emit captureFrameSizeChanged();
}

void AppController::handleVisionLine(const QString &line, bool &frameGeometryChecked)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (m_settings.debugMode() && now - m_lastRawEventUiMs >= 1000) {
            appendRawEvent(line);
            m_lastRawEventUiMs = now;
        }
        return;
    }

    if (m_settings.debugMode() && now - m_lastRawEventUiMs >= 1000) {
        QJsonObject clean = document.object();
        clean.remove("debug_image");  // don't spam raw log with large b64
        appendRawEvent(QJsonDocument(clean).toJson(QJsonDocument::Compact));
        m_lastRawEventUiMs = now;
    }

    const QJsonObject root = document.object();
    if (!frameGeometryChecked) {
        const QJsonObject frame = root.value(QStringLiteral("details")).toObject().value(QStringLiteral("frame")).toObject();
        const int width = frame.value(QStringLiteral("width")).toInt();
        const int height = frame.value(QStringLiteral("height")).toInt();
        if (width > 0 && height > 0) {
            frameGeometryChecked = true;
            if (m_captureFrameWidth != width || m_captureFrameHeight != height) {
                m_captureFrameWidth = width;
                m_captureFrameHeight = height;
                emit captureFrameSizeChanged();
            }
            constexpr int maxRecommendedWidth = 2100;
            constexpr int maxRecommendedHeight = 1250;
            if (width > maxRecommendedWidth || height > maxRecommendedHeight) {
                m_fishing.resetSession();
                resetReelControllerState();
                setStatus(QStringLiteral("error"));
                const QString message = QStringLiteral("Capture is %1x%2. Switch NTE to windowed FullHD before starting autofish.")
                                            .arg(width)
                                            .arg(height);
                setLastEvent(message);
                setFishingEvent(QStringLiteral("Capture too large: use windowed FullHD"));
                appendFishingLog(QStringLiteral("capture rejected: %1x%2, use windowed FullHD").arg(width).arg(height));
                m_vision.stop();
                return;
            }
        }
    }

    if (m_fishing.inspectResultScreen(root)) {
        return;
    }
    m_fishing.inspectFishingEvent(root);
    m_reel.ingestFrame(root);

    if (root.contains(QStringLiteral("debug_image"))) {
        const QString b64 = root.value(QStringLiteral("debug_image")).toString();
        if (!b64.isEmpty()) {
            const QByteArray data = QByteArray::fromBase64(b64.toUtf8());
            QImage img;
            if (img.loadFromData(data, "PNG")) {
                updateDebugOverlay(img);
            }
        }
    }
}

void AppController::onVisionStopped(int exitCode)
{
    setStatus(exitCode == 0 ? QStringLiteral("idle") : QStringLiteral("error"));
    setLastEvent(QStringLiteral("Vision service stopped"));
    setReelDirection(0);
    m_fishing.onVisionStopped();
    m_reel.resetSession();
    m_captureFrameWidth = 0;
    m_captureFrameHeight = 0;
    emit captureFrameSizeChanged();
}

void AppController::notifyReelDirectionChanged(int direction)
{
    if (!m_settings.debugMode()) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastReelControlUiMs < 160) {
        return;
    }
    m_lastReelControlUiMs = now;

    const QString key = direction < 0 ? QStringLiteral("A") : direction > 0 ? QStringLiteral("D") : QStringLiteral("none");
    QString mode = QStringLiteral("stable");
    QString action = QStringLiteral("pulse");
    if (m_settings.reelControlMode() == ReelControlMode::ChizukuoPid) {
        mode = QStringLiteral("chizukuo pid");
        action = QStringLiteral("pulse");
    } else if (m_settings.reelControlMode() == ReelControlMode::Experimental) {
        mode = QStringLiteral("experimental");
        action = QStringLiteral("guard");
    }
    setReelControl(QStringLiteral("%1 %2 %3  target %4  marker %5  error %6")
                       .arg(mode)
                       .arg(action)
                       .arg(key)
                       .arg(m_reel.targetCenter(), 0, 'f', 1)
                       .arg(m_reel.markerCenter(), 0, 'f', 1)
                       .arg(m_reel.targetCenter() - m_reel.markerCenter(), 0, 'f', 1));
}

bool AppController::ensureUinput() { return m_input.ensureOpen(); }
void AppController::setReelDirection(int direction) { m_input.setReelDirection(direction); }
void AppController::sendKeyTap(int keyCode) { m_input.sendKeyTap(keyCode); }

void AppController::resetReelControllerState()
{
    m_reel.resetControllerState();
}

QString AppController::repoRoot() const
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    if (QFileInfo::exists(appDir.absoluteFilePath(QStringLiteral("python/autofish_vision/service.py")))) {
        return appDir.absolutePath();
    }

    const QDir parentDir(appDir.absoluteFilePath(QStringLiteral("..")));
    if (QFileInfo::exists(parentDir.absoluteFilePath(QStringLiteral("python/autofish_vision/service.py")))) {
        return parentDir.absolutePath();
    }

    return parentDir.absolutePath();
}

QString AppController::pythonExecutable() const
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        QDir(repoRoot()).absoluteFilePath(QStringLiteral(".venv/bin/python")),
        appDir.absoluteFilePath(QStringLiteral("../.venv/bin/python")),
        appDir.absoluteFilePath(QStringLiteral("../../.venv/bin/python")),
    };

    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return QStringLiteral("python3");
}

QProcessEnvironment AppController::pythonEnvironment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONPATH"), QDir(repoRoot()).absoluteFilePath(QStringLiteral("python")));
    return env;
}

void AppController::onInputOutput()
{
    m_inputStdoutBuffer += QString::fromUtf8(m_inputProcess.readAllStandardOutput());
    qsizetype newlineIndex = -1;
    while ((newlineIndex = m_inputStdoutBuffer.indexOf(QLatin1Char('\n'))) >= 0) {
        const QString line = m_inputStdoutBuffer.left(newlineIndex).trimmed();
        m_inputStdoutBuffer.remove(0, newlineIndex + 1);
        if (!line.isEmpty()) {
            setLastEvent(line);
        }
    }
}

void AppController::onInputErrorOutput()
{
    const QString output = QString::fromUtf8(m_inputProcess.readAllStandardError()).trimmed();
    if (!output.isEmpty()) {
        setLastEvent(output);
    }
}

void AppController::onInputFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)
    const QString pending = m_inputStdoutBuffer.trimmed();
    if (!pending.isEmpty()) {
        setLastEvent(pending);
        m_inputStdoutBuffer.clear();
    }
    if (exitCode != 0) {
        setLastEvent(QStringLiteral("Input sender exited with code %1").arg(exitCode));
    }
}

QString AppController::debugOverlaySource() const { return m_debugOverlaySource; }

void AppController::updateDebugOverlay(const QImage &img)
{
    if (img.isNull()) return;
    m_debugOverlayImage = img;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(dir);
    const QString path = dir + "/autofish_debug_overlay.png";
    if (img.save(path)) {
        m_debugOverlayVersion++;
        m_debugOverlaySource = QStringLiteral("file://%1?ts=%2").arg(path).arg(m_debugOverlayVersion);
        emit debugOverlaySourceChanged();
    }
}