#include "AppController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QVariantMap>

#include <linux/input.h>

void AppController::refreshWindows()
{
    QProcess process;
    process.setProcessEnvironment(pythonEnvironment());
    process.setWorkingDirectory(repoRoot());
    process.start(pythonExecutable(),
                  {QStringLiteral("-m"), QStringLiteral("autofish_vision.service"),
                   QStringLiteral("--list-windows")});

    if (!process.waitForStarted(1500)) {
        setStatus(QStringLiteral("error"));
        setLastEvent(QStringLiteral("Failed to start window scanner"));
        return;
    }

    if (!process.waitForFinished(4000)) {
        process.kill();
        setStatus(QStringLiteral("error"));
        setLastEvent(QStringLiteral("Window scanner timed out"));
        return;
    }

    const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (!stderrText.isEmpty()) {
        setLastEvent(stderrText);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(stdoutText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setStatus(QStringLiteral("error"));
        setLastEvent(stdoutText.isEmpty() ? QStringLiteral("Window scanner returned no JSON") : stdoutText);
        return;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("event")).toString() != QStringLiteral("window_list")) {
        setStatus(QStringLiteral("error"));
        setLastEvent(stdoutText);
        return;
    }

    QVariantList windows;
    const QJsonArray array = root.value(QStringLiteral("details")).toObject().value(QStringLiteral("windows")).toArray();
    for (const QJsonValue &value : array) {
        windows.append(value.toObject().toVariantMap());
    }

    m_windows = windows;
    emit windowsChanged();

    if (m_windows.isEmpty()) {
        m_selectedWindowIndex = -1;
    } else if (m_selectedWindowIndex < 0 || m_selectedWindowIndex >= m_windows.size()) {
        m_selectedWindowIndex = 0;
    }
    emit selectedWindowChanged();

    setStatus(QStringLiteral("idle"));
    setLastEvent(QStringLiteral("Found %1 windows").arg(m_windows.size()));
}

void AppController::selectWindow(int index)
{
    if (index < -1 || index >= m_windows.size() || m_selectedWindowIndex == index) {
        return;
    }

    m_selectedWindowIndex = index;
    emit selectedWindowChanged();
}

void AppController::startVision()
{
    if (m_visionProcess.state() != QProcess::NotRunning) {
        setLastEvent(QStringLiteral("Vision service is already running"));
        return;
    }

    m_hookActionTimer.stop();
    m_resultCloseTimer.stop();
    m_recastTimer.stop();
    m_resultScreenLatched = false;
    m_resultScreenAbsentFrames = 0;
    m_lastReelObservationMs = 0;
    m_reelControlVisible = false;
    m_frameGeometryChecked = false;
    transitionFishingFlowState(FishingFlowState::Waiting);
    m_hookRetryCount = 0;
    ensureUinput();

    QString windowId;
    QString windowTitle = QStringLiteral("NTE");
    if (m_selectedWindowIndex >= 0 && m_selectedWindowIndex < m_windows.size()) {
        const QVariantMap window = m_windows.at(m_selectedWindowIndex).toMap();
        windowId = window.value(QStringLiteral("id")).toString();
        windowTitle = window.value(QStringLiteral("title")).toString();
    }

    QStringList arguments = {QStringLiteral("-m"), QStringLiteral("autofish_vision.service"),
                             QStringLiteral("--watch"), QStringLiteral("--window-title"), windowTitle};
    if (!windowId.isEmpty()) {
        arguments << QStringLiteral("--window-id") << windowId;
    }
    if (m_saveDebugFrames) {
        arguments << QStringLiteral("--debug-dir") << QStringLiteral("debug")
                  << QStringLiteral("--debug-every") << QStringLiteral("30");
    }
    arguments << QStringLiteral("--interval") << QStringLiteral("0.005");

    m_visionProcess.setProcessEnvironment(pythonEnvironment());
    m_visionProcess.setWorkingDirectory(repoRoot());
    m_visionProcess.start(pythonExecutable(), arguments);

    if (!m_visionProcess.waitForStarted(1500)) {
        setStatus(QStringLiteral("error"));
        setLastEvent(QStringLiteral("Failed to start vision service"));
        return;
    }

    setStatus(QStringLiteral("watching"));
    setLastEvent(QStringLiteral("Vision service started"));
}

void AppController::startPortalVision()
{
    if (m_visionProcess.state() != QProcess::NotRunning) {
        setLastEvent(QStringLiteral("Vision service is already running"));
        return;
    }

    m_hookActionTimer.stop();
    m_resultCloseTimer.stop();
    m_recastTimer.stop();
    m_resultScreenLatched = false;
    m_resultScreenAbsentFrames = 0;
    m_lastReelObservationMs = 0;
    m_reelControlVisible = false;
    m_frameGeometryChecked = false;
    transitionFishingFlowState(FishingFlowState::Waiting);
    m_hookRetryCount = 0;
    ensureUinput();

    m_visionProcess.setProcessEnvironment(pythonEnvironment());
    m_visionProcess.setWorkingDirectory(repoRoot());
    QStringList arguments = {QStringLiteral("-m"), QStringLiteral("autofish_vision.service"),
                             QStringLiteral("--watch"), QStringLiteral("--portal")};
    if (m_saveDebugFrames) {
        arguments << QStringLiteral("--debug-dir") << QStringLiteral("debug")
                  << QStringLiteral("--debug-every") << QStringLiteral("30");
    }
    arguments << QStringLiteral("--interval") << QStringLiteral("0.005");
    m_visionProcess.start(pythonExecutable(), arguments);

    if (!m_visionProcess.waitForStarted(1500)) {
        setStatus(QStringLiteral("error"));
        setLastEvent(QStringLiteral("Failed to start portal vision service"));
        return;
    }

    setStatus(QStringLiteral("watching"));
    setLastEvent(QStringLiteral("Opening portal picker"));
}

void AppController::stopVision()
{
    if (m_visionProcess.state() == QProcess::NotRunning) {
        setStatus(QStringLiteral("idle"));
        return;
    }

    m_visionProcess.terminate();
    if (!m_visionProcess.waitForFinished(1500)) {
        m_visionProcess.kill();
    }

    setStatus(QStringLiteral("idle"));
    setLastEvent(QStringLiteral("Vision service stopped"));
    m_hookActionTimer.stop();
    m_manualResultScreenTimer.stop();
    m_resultCloseTimer.stop();
    m_recastTimer.stop();
    setReelDirection(0);
    m_resultScreenLatched = false;
    m_resultScreenAbsentFrames = 0;
    m_hookBlueTriggerFrames = 0;
    m_frameGeometryChecked = false;
    transitionFishingFlowState(FishingFlowState::Waiting);
    m_hookRetryCount = 0;
}

void AppController::simulateBite()
{
    setLastEvent(QStringLiteral("{\"event\":\"bite\",\"confidence\":1.0,\"source\":\"ui\"}"));
    setFishingEvent(m_debugMode ? QStringLiteral("fish_on_hook detected (simulated)")
                                : QStringLiteral("Hook! Reason: simulated"));
    transitionFishingFlowState(FishingFlowState::HookScheduled, QStringLiteral("simulated hook"));
    scheduleHookAction();
    m_fishingEventResetTimer.start();
}

void AppController::simulateResultScreen()
{
    m_manualResultScreenTimer.start(3000);
    setLastEvent(QStringLiteral("Manual result test armed: Alt-Tab to the game, Esc/F sequence starts in 3000 ms"));
    setFishingEvent(QStringLiteral("manual result test armed: switch focus to game"));
    m_fishingEventResetTimer.start();
}

void AppController::onVisionOutput()
{
    m_stdoutBuffer += QString::fromUtf8(m_visionProcess.readAllStandardOutput());

    qsizetype newlineIndex = -1;
    while ((newlineIndex = m_stdoutBuffer.indexOf(QLatin1Char('\n'))) >= 0) {
        const QString line = m_stdoutBuffer.left(newlineIndex).trimmed();
        m_stdoutBuffer.remove(0, newlineIndex + 1);
        if (!line.isEmpty()) {
            handleVisionLine(line);
        }
    }

    const QString pending = m_stdoutBuffer.trimmed();
    if (!pending.isEmpty()) {
        setLastEvent(pending);
    }
}

void AppController::onVisionErrorOutput()
{
    const QString output = QString::fromUtf8(m_visionProcess.readAllStandardError()).trimmed();
    if (!output.isEmpty()) {
        setLastEvent(output);
    }
}

void AppController::onVisionFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)
    setStatus(exitCode == 0 ? QStringLiteral("idle") : QStringLiteral("error"));
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

void AppController::handleVisionLine(const QString &line)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_debugMode && now - m_lastRawEventUiMs >= 1000) {
        appendRawEvent(line);
        m_lastRawEventUiMs = now;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }

    const QJsonObject root = document.object();
    if (!inspectFrameGeometry(root)) {
        return;
    }

    if (inspectResultScreen(root)) {
        return;
    }
    inspectFishingEvent(root);
    inspectReelControl(root);
}

bool AppController::inspectFrameGeometry(const QJsonObject &root)
{
    if (m_frameGeometryChecked) {
        return true;
    }

    const QJsonObject frame = root.value(QStringLiteral("details")).toObject().value(QStringLiteral("frame")).toObject();
    const int width = frame.value(QStringLiteral("width")).toInt();
    const int height = frame.value(QStringLiteral("height")).toInt();
    if (width <= 0 || height <= 0) {
        return true;
    }

    m_frameGeometryChecked = true;
    constexpr int maxRecommendedWidth = 2100;
    constexpr int maxRecommendedHeight = 1250;
    if (width <= maxRecommendedWidth && height <= maxRecommendedHeight) {
        return true;
    }

    m_hookActionTimer.stop();
    m_resultCloseTimer.stop();
    m_recastTimer.stop();
    resetReelControllerState();
    setStatus(QStringLiteral("error"));

    const QString message = QStringLiteral("Capture is %1x%2. Switch NTE to windowed FullHD before starting autofish.")
                                .arg(width)
                                .arg(height);
    setLastEvent(message);
    setFishingEvent(QStringLiteral("Capture too large: use windowed FullHD"));
    appendFishingLog(QStringLiteral("capture rejected: %1x%2, use windowed FullHD").arg(width).arg(height));

    if (m_visionProcess.state() != QProcess::NotRunning) {
        m_visionProcess.terminate();
    }
    return false;
}

QString AppController::repoRoot() const
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    return appDir.absoluteFilePath(QStringLiteral(".."));
}

QString AppController::pythonExecutable() const
{
    const QString venvPython = QDir(repoRoot()).absoluteFilePath(QStringLiteral(".venv/bin/python"));
    return QFileInfo::exists(venvPython) ? venvPython : QStringLiteral("python3");
}

QProcessEnvironment AppController::pythonEnvironment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONPATH"), QDir(repoRoot()).absoluteFilePath(QStringLiteral("python")));
    return env;
}
