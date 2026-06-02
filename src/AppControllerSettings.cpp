#include "AppController.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QTextStream>

void AppController::completeSetup(const QString &platform, const QString &captureBackend)
{
    const QString normalizedPlatform = platform.trimmed().isEmpty() ? QStringLiteral("Wayland") : platform.trimmed();
    const QString normalizedBackend = captureBackend.trimmed().isEmpty() ? QStringLiteral("PipeWire Portal") : captureBackend.trimmed();

    bool changed = false;
    if (m_platform != normalizedPlatform) {
        m_platform = normalizedPlatform;
        saveUiSetting(QStringLiteral("platform"), m_platform);
        changed = true;
    }
    if (m_captureBackend != normalizedBackend) {
        m_captureBackend = normalizedBackend;
        saveUiSetting(QStringLiteral("captureBackend"), m_captureBackend);
        changed = true;
    }
    if (!m_setupComplete) {
        m_setupComplete = true;
        saveUiSetting(QStringLiteral("setupComplete"), m_setupComplete);
        emit setupCompleteChanged();
    }
    if (changed) {
        emit setupChanged();
    }
    setLastEvent(QStringLiteral("Setup saved: %1 / %2").arg(m_platform, m_captureBackend));
}

void AppController::resetSetup()
{
    if (!m_setupComplete) {
        return;
    }
    m_setupComplete = false;
    saveUiSetting(QStringLiteral("setupComplete"), m_setupComplete);
    emit setupCompleteChanged();
    setLastEvent(QStringLiteral("Setup reset"));
}

void AppController::setExperienceMode(const QString &mode)
{
    const QString normalized = mode == QStringLiteral("PRO") ? QStringLiteral("PRO") : QStringLiteral("EZ");
    if (m_experienceMode == normalized) {
        return;
    }
    m_experienceMode = normalized;
    saveUiSetting(QStringLiteral("experienceMode"), m_experienceMode);
    emit experienceModeChanged();
}

void AppController::setDebugMode(bool enabled)
{
    if (m_debugMode == enabled) {
        return;
    }
    m_debugMode = enabled;
    saveUiSetting(QStringLiteral("debugMode"), m_debugMode);
    emit debugModeChanged();
}

void AppController::setUiLanguage(const QString &language)
{
    const QString normalized = language == QStringLiteral("ru")
        || language == QStringLiteral("ja")
        || language == QStringLiteral("zh_CN")
        ? language
        : QStringLiteral("en");
    if (m_uiLanguage == normalized) {
        return;
    }

    m_uiLanguage = normalized;
    saveUiSetting(QStringLiteral("uiLanguage"), m_uiLanguage);
    emit uiLanguageChanged();
}

void AppController::setExperimentalLeadMs(int value)
{
    const int bounded = qBound(0, value, 140);
    if (m_experimentalLeadMs == bounded) {
        return;
    }
    m_experimentalLeadMs = bounded;
    saveUiSetting(QStringLiteral("experimentalLeadMs"), bounded);
    resetReelControllerState();
    emit experimentalTuningChanged();
}

void AppController::setExperimentalSafeMarginPercent(int value)
{
    const int bounded = qBound(5, value, 40);
    if (m_experimentalSafeMarginPercent == bounded) {
        return;
    }
    m_experimentalSafeMarginPercent = bounded;
    saveUiSetting(QStringLiteral("experimentalSafeMarginPercent"), bounded);
    resetReelControllerState();
    emit experimentalTuningChanged();
}

void AppController::setExperimentalSettleMs(int value)
{
    const int bounded = qBound(0, value, 250);
    if (m_experimentalSettleMs == bounded) {
        return;
    }
    m_experimentalSettleMs = bounded;
    saveUiSetting(QStringLiteral("experimentalSettleMs"), bounded);
    resetReelControllerState();
    emit experimentalTuningChanged();
}

void AppController::setExperimentalBrakeMs(int value)
{
    const int bounded = qBound(0, value, 180);
    if (m_experimentalBrakeMs == bounded) {
        return;
    }
    m_experimentalBrakeMs = bounded;
    saveUiSetting(QStringLiteral("experimentalBrakeMs"), bounded);
    resetReelControllerState();
    emit experimentalTuningChanged();
}

void AppController::setExperimentalMaxPulseMs(int value)
{
    const int bounded = qBound(32, value, 180);
    if (m_experimentalMaxPulseMs == bounded) {
        return;
    }
    m_experimentalMaxPulseMs = bounded;
    saveUiSetting(QStringLiteral("experimentalMaxPulseMs"), bounded);
    resetReelControllerState();
    emit experimentalTuningChanged();
}

void AppController::setExperimentalMinGapMs(int value)
{
    const int bounded = qBound(0, value, 30);
    if (m_experimentalMinGapMs == bounded) {
        return;
    }
    m_experimentalMinGapMs = bounded;
    saveUiSetting(QStringLiteral("experimentalMinGapMs"), bounded);
    resetReelControllerState();
    emit experimentalTuningChanged();
}

void AppController::resetExperimentalTuning()
{
    m_experimentalLeadMs = 55;
    m_experimentalSafeMarginPercent = 20;
    m_experimentalSettleMs = 110;
    m_experimentalBrakeMs = 85;
    m_experimentalMaxPulseMs = 115;
    m_experimentalMinGapMs = 4;

    saveUiSetting(QStringLiteral("experimentalLeadMs"), m_experimentalLeadMs);
    saveUiSetting(QStringLiteral("experimentalSafeMarginPercent"), m_experimentalSafeMarginPercent);
    saveUiSetting(QStringLiteral("experimentalSettleMs"), m_experimentalSettleMs);
    saveUiSetting(QStringLiteral("experimentalBrakeMs"), m_experimentalBrakeMs);
    saveUiSetting(QStringLiteral("experimentalMaxPulseMs"), m_experimentalMaxPulseMs);
    saveUiSetting(QStringLiteral("experimentalMinGapMs"), m_experimentalMinGapMs);

    resetReelControllerState();
    setReelControl(QStringLiteral("experimental tuning reset"));
    emit experimentalTuningChanged();
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
    if (!m_writeLogsToFile) {
        return;
    }

    QDir logsDir(repoRoot());
    if (!logsDir.mkpath(QStringLiteral("logs"))) {
        return;
    }

    QFile file(logsDir.filePath(QStringLiteral("logs/%1").arg(fileName)));
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

void AppController::setReelControl(const QString &event)
{
    if (m_reelControl == event) {
        return;
    }

    m_reelControl = event;
    emit reelControlChanged();
}

void AppController::loadUiSettings()
{
    QSettings settings(QStringLiteral("kagebaito"), QStringLiteral("autofish"));
    const QString systemLocale = QLocale::system().name();
    QString defaultLanguage = QStringLiteral("en");
    if (systemLocale.startsWith(QStringLiteral("ru"))) {
        defaultLanguage = QStringLiteral("ru");
    } else if (systemLocale.startsWith(QStringLiteral("ja"))) {
        defaultLanguage = QStringLiteral("ja");
    } else if (systemLocale.startsWith(QStringLiteral("zh"))) {
        defaultLanguage = QStringLiteral("zh_CN");
    }

    m_setupComplete = settings.value(QStringLiteral("ui/setupComplete"), false).toBool();
    m_platform = settings.value(QStringLiteral("ui/platform"), QStringLiteral("Wayland")).toString();
    m_captureBackend = settings.value(QStringLiteral("ui/captureBackend"), QStringLiteral("PipeWire Portal")).toString();
    m_experienceMode = settings.value(QStringLiteral("ui/experienceMode"), QStringLiteral("EZ")).toString();
    if (m_experienceMode != QStringLiteral("PRO")) {
        m_experienceMode = QStringLiteral("EZ");
    }
    m_debugMode = settings.value(QStringLiteral("ui/debugMode"), false).toBool();
    m_uiLanguage = settings.value(QStringLiteral("ui/uiLanguage"), defaultLanguage).toString();
    if (m_uiLanguage != QStringLiteral("ru")
        && m_uiLanguage != QStringLiteral("ja")
        && m_uiLanguage != QStringLiteral("zh_CN")) {
        m_uiLanguage = QStringLiteral("en");
    }
    m_saveDebugFrames = settings.value(QStringLiteral("ui/saveDebugFrames"), false).toBool();
    m_writeLogsToFile = settings.value(QStringLiteral("ui/writeLogsToFile"), false).toBool();
    m_reelControlMode = static_cast<ReelControlMode>(
        qBound(static_cast<int>(ReelControlMode::Boundary),
               settings.value(QStringLiteral("ui/reelControlMode"), static_cast<int>(ReelControlMode::LegacyPid)).toInt(),
               static_cast<int>(ReelControlMode::KagebaitoGuard)));
    m_experimentalLeadMs = qBound(0, settings.value(QStringLiteral("ui/experimentalLeadMs"), 55).toInt(), 140);
    m_experimentalSafeMarginPercent = qBound(5, settings.value(QStringLiteral("ui/experimentalSafeMarginPercent"), 20).toInt(), 40);
    m_experimentalSettleMs = qBound(0, settings.value(QStringLiteral("ui/experimentalSettleMs"), 110).toInt(), 250);
    m_experimentalBrakeMs = qBound(0, settings.value(QStringLiteral("ui/experimentalBrakeMs"), 85).toInt(), 180);
    m_experimentalMaxPulseMs = qBound(32, settings.value(QStringLiteral("ui/experimentalMaxPulseMs"), 115).toInt(), 180);
    m_experimentalMinGapMs = qBound(0, settings.value(QStringLiteral("ui/experimentalMinGapMs"), 4).toInt(), 30);
}

void AppController::saveUiSetting(const QString &key, const QVariant &value)
{
    QSettings settings(QStringLiteral("kagebaito"), QStringLiteral("autofish"));
    settings.setValue(QStringLiteral("ui/%1").arg(key), value);
}

void AppController::setUsePidReelControl(bool enabled)
{
    setReelControlMode(enabled ? static_cast<int>(ReelControlMode::LegacyPid)
                               : static_cast<int>(ReelControlMode::Boundary));
}

void AppController::setReelControlMode(int mode)
{
    const int boundedMode = qBound(static_cast<int>(ReelControlMode::Boundary),
                                   mode,
                                   static_cast<int>(ReelControlMode::KagebaitoGuard));
    const auto nextMode = static_cast<ReelControlMode>(boundedMode);
    if (m_reelControlMode == nextMode) {
        return;
    }

    m_reelControlMode = nextMode;
    saveUiSetting(QStringLiteral("reelControlMode"), boundedMode);
    resetReelControllerState();

    QString modeName = QStringLiteral("boundary");
    if (m_reelControlMode == ReelControlMode::ChizukuoPid) {
        modeName = QStringLiteral("chizukuo pid");
    } else if (m_reelControlMode == ReelControlMode::LegacyPid) {
        modeName = QStringLiteral("stable");
    } else if (m_reelControlMode == ReelControlMode::KagebaitoGuard) {
        modeName = QStringLiteral("experimental");
    }

    setReelControl(QStringLiteral("reel mode: %1").arg(modeName));
    appendFishingLog(QStringLiteral("reel mode -> %1").arg(modeName));
    emit reelControlModeChanged();
    emit usePidReelControlChanged();
}

void AppController::setSaveDebugFrames(bool enabled)
{
    if (m_saveDebugFrames == enabled) {
        return;
    }

    m_saveDebugFrames = enabled;
    saveUiSetting(QStringLiteral("saveDebugFrames"), m_saveDebugFrames);
    setLastEvent(QStringLiteral("Debug frame saving %1").arg(enabled ? QStringLiteral("enabled") : QStringLiteral("disabled")));
    emit saveDebugFramesChanged();
}

void AppController::setWriteLogsToFile(bool enabled)
{
    if (m_writeLogsToFile == enabled) {
        return;
    }

    m_writeLogsToFile = enabled;
    saveUiSetting(QStringLiteral("writeLogsToFile"), m_writeLogsToFile);
    setLastEvent(QStringLiteral("File logging %1").arg(enabled ? QStringLiteral("enabled") : QStringLiteral("disabled")));
    emit writeLogsToFileChanged();
}
