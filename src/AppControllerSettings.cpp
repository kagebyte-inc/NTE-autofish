#include "AppController.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

void AppController::completeSetup(const QString &platform, const QString &captureBackend)
{
    const QString normalizedPlatform = platform.trimmed().isEmpty() ? QStringLiteral("Wayland") : platform.trimmed();
    const QString normalizedBackend = captureBackend.trimmed().isEmpty() ? QStringLiteral("PipeWire Portal") : captureBackend.trimmed();

    m_settings.setPlatform(normalizedPlatform);
    m_settings.setCaptureBackend(normalizedBackend);
    m_settings.setSetupComplete(true);
    setLastEvent(QStringLiteral("Setup saved: %1 / %2").arg(normalizedPlatform, normalizedBackend));
}

void AppController::resetSetup()
{
    if (!m_settings.setupComplete()) {
        return;
    }
    m_settings.setSetupComplete(false);
    setLastEvent(QStringLiteral("Setup reset"));
}

void AppController::updateCaptureSetup(const QString &platform, const QString &captureBackend)
{
    const QString normalizedPlatform = platform.trimmed().isEmpty() ? QStringLiteral("Wayland") : platform.trimmed();
    const QString normalizedBackend = captureBackend.trimmed().isEmpty() ? QStringLiteral("PipeWire Portal") : captureBackend.trimmed();

    m_settings.setPlatform(normalizedPlatform);
    m_settings.setCaptureBackend(normalizedBackend);
    setLastEvent(QStringLiteral("Capture path updated: %1 / %2").arg(normalizedPlatform, normalizedBackend));
}

bool AppController::inputReady() const
{
    return m_inputReady;
}

QString AppController::captureFrameSize() const
{
    if (m_captureFrameWidth <= 0 || m_captureFrameHeight <= 0) {
        return QStringLiteral("—");
    }
    return QStringLiteral("%1×%2").arg(m_captureFrameWidth).arg(m_captureFrameHeight);
}

void AppController::refreshDiagnostics()
{
    const bool ready = m_input.ensureOpen();
    if (m_inputReady != ready) {
        m_inputReady = ready;
        emit inputReadyChanged();
    }
}

void AppController::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void AppController::setLastEvent(const QString &event)
{
    if (m_lastEvent == event) {
        return;
    }
    m_lastEvent = event;
    emit lastEventChanged();

    // For release diagnostics: surface error-like last events (e.g. from vision "Start" failures,
    // portal/capture errors, uinput fails) to autofish.log (always, via qWarning) and fishing.log (if enabled).
    const bool looksLikeError = event.contains(QStringLiteral("error"), Qt::CaseInsensitive)
                             || event.contains(QStringLiteral("fail"), Qt::CaseInsensitive)
                             || event.contains(QStringLiteral("unavailable"), Qt::CaseInsensitive)
                             || event.contains(QStringLiteral("rejected"), Qt::CaseInsensitive)
                             || event.contains(QStringLiteral("timeout"), Qt::CaseInsensitive)
                             || event.contains(QStringLiteral("stopped with"), Qt::CaseInsensitive);
    if (looksLikeError) {
        qWarning() << "LastEvent (error):" << event;
        appendFishingLogOnce(event);
    }
}

void AppController::appendRawEvent(const QString &event)
{
    appendLineToLogFile(QStringLiteral("raw-events.log"), event);

    m_rawEventLines.append(event);
    while (m_rawEventLines.size() > 8) {
        m_rawEventLines.removeFirst();
    }

    QString rawEvents = m_rawEventLines.join(QLatin1Char('\n'));
    constexpr qsizetype maxChars = 9000;
    if (rawEvents.size() > maxChars) {
        rawEvents = rawEvents.right(maxChars);
    }

    if (m_rawEvents == rawEvents) {
        return;
    }

    m_rawEvents = rawEvents;
    emit rawEventsChanged();
}

void AppController::appendLineToLogFile(const QString &fileName, const QString &line)
{
    if (!m_settings.writeLogsToFile()) {
        return;
    }

    // Write directly next to the binary (fishing.log, raw-events.log) for easy access in releases.
    QDir logDir(repoRoot());
    QFile file(logDir.filePath(fileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
    stream << line << Qt::endl;
}

void AppController::setFishingEvent(const QString &event)
{
    if (m_fishingEvent == event) {
        return;
    }
    m_fishingEvent = event;
    emit fishingEventChanged();
}

void AppController::appendFishingLog(const QString &event)
{
    const QString line = QStringLiteral("%1  %2")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), event);
    appendLineToLogFile(QStringLiteral("fishing.log"), line);

    m_fishingLogLines.append(line);
    while (m_fishingLogLines.size() > 80) {
        m_fishingLogLines.removeFirst();
    }

    const QString fishingLog = m_fishingLogLines.join(QLatin1Char('\n'));
    if (m_fishingLog == fishingLog) {
        return;
    }

    m_fishingLog = fishingLog;
    emit fishingLogChanged();
}

void AppController::appendFishingLogOnce(const QString &event)
{
    const QString previous = m_fishingLogLines.isEmpty() ? QString() : m_fishingLogLines.constLast();
    if (previous.endsWith(QStringLiteral("  %1").arg(event))) {
        return;
    }
    appendFishingLog(event);
}

void AppController::setReelControl(const QString &event)
{
    if (m_reelControl == event) {
        return;
    }
    m_reelControl = event;
    emit reelControlChanged();
}

void AppController::setUsePidReelControl(bool enabled)
{
    setReelControlMode(enabled ? static_cast<int>(ReelControlMode::Stable)
                               : static_cast<int>(ReelControlMode::Stable));
}

void AppController::setReelControlMode(int mode)
{
    const auto previous = m_settings.reelControlMode();
    m_settings.setReelControlModeInt(mode);
    if (previous == m_settings.reelControlMode()) {
        return;
    }

    QString modeName = QStringLiteral("stable");
    if (m_settings.reelControlMode() == ReelControlMode::ChizukuoPid) {
        modeName = QStringLiteral("chizukuo pid");
    } else if (m_settings.reelControlMode() == ReelControlMode::Stable) {
        modeName = QStringLiteral("stable");
    } else if (m_settings.reelControlMode() == ReelControlMode::Experimental) {
        modeName = QStringLiteral("experimental");
    }

    setReelControl(QStringLiteral("reel mode: %1").arg(modeName));
    appendFishingLog(QStringLiteral("reel mode -> %1").arg(modeName));
}

void AppController::setSaveDebugFrames(bool enabled)
{
    m_settings.setSaveDebugFrames(enabled);
    setLastEvent(QStringLiteral("Debug frame saving %1").arg(enabled ? QStringLiteral("enabled") : QStringLiteral("disabled")));
}

void AppController::setWriteLogsToFile(bool enabled)
{
    m_settings.setWriteLogsToFile(enabled);
    setLastEvent(QStringLiteral("File logging %1").arg(enabled ? QStringLiteral("enabled") : QStringLiteral("disabled")));
}
