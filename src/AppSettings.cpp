#include "AppSettings.h"

#include <QLocale>
#include <QSettings>

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    load();
}

void AppSettings::load()
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
               settings.value(QStringLiteral("ui/reelControlMode"), static_cast<int>(ReelControlMode::KagebaitoGuard)).toInt(),
               static_cast<int>(ReelControlMode::KagebaitoGuard)));
    m_experimentalLeadMs = qBound(0, settings.value(QStringLiteral("ui/experimentalLeadMs"), 55).toInt(), 140);
    m_experimentalSafeMarginPercent = qBound(5, settings.value(QStringLiteral("ui/experimentalSafeMarginPercent"), 20).toInt(), 40);
    m_experimentalSettleMs = qBound(0, settings.value(QStringLiteral("ui/experimentalSettleMs"), 110).toInt(), 250);
    m_experimentalBrakeMs = qBound(0, settings.value(QStringLiteral("ui/experimentalBrakeMs"), 85).toInt(), 180);
    m_experimentalMaxPulseMs = qBound(32, settings.value(QStringLiteral("ui/experimentalMaxPulseMs"), 115).toInt(), 180);
    m_experimentalMinGapMs = qBound(0, settings.value(QStringLiteral("ui/experimentalMinGapMs"), 4).toInt(), 30);
    m_fishingHookDelayMs = qBound(1000, settings.value(QStringLiteral("ui/fishingHookDelayMs"), 3000).toInt(), 6000);
    m_fishingHookJitterMs = qBound(0, settings.value(QStringLiteral("ui/fishingHookJitterMs"), 500).toInt(), 1500);
    m_fishingEventResetMs = qBound(500, settings.value(QStringLiteral("ui/fishingEventResetMs"), 2000).toInt(), 8000);
    m_fishingResultEscBaseMs = qBound(500, settings.value(QStringLiteral("ui/fishingResultEscBaseMs"), 2000).toInt(), 5000);
    m_fishingResultEscJitterMs = qBound(0, settings.value(QStringLiteral("ui/fishingResultEscJitterMs"), 500).toInt(), 1500);
    m_fishingRecastBaseMs = qBound(500, settings.value(QStringLiteral("ui/fishingRecastBaseMs"), 1200).toInt(), 3000);
    m_fishingRecastJitterMs = qBound(0, settings.value(QStringLiteral("ui/fishingRecastJitterMs"), 250).toInt(), 800);
    m_fishingAwaitingReelMs = qBound(5000, settings.value(QStringLiteral("ui/fishingAwaitingReelMs"), 15000).toInt(), 60000);
    m_fishingReelLostMs = qBound(500, settings.value(QStringLiteral("ui/fishingReelLostMs"), 1800).toInt(), 5000);
    m_fishingHookRetryMs = qBound(500, settings.value(QStringLiteral("ui/fishingHookRetryMs"), 1500).toInt(), 5000);
    m_fishingHookVisibleGraceMs = qBound(500, settings.value(QStringLiteral("ui/fishingHookVisibleGraceMs"), 2200).toInt(), 5000);
    m_fishingRecoveryBaseMs = qBound(200, settings.value(QStringLiteral("ui/fishingRecoveryBaseMs"), 800).toInt(), 2000);
    m_fishingRecoveryJitterMs = qBound(0, settings.value(QStringLiteral("ui/fishingRecoveryJitterMs"), 350).toInt(), 1000);
}

void AppSettings::save(const QString &key, const QVariant &value)
{
    QSettings settings(QStringLiteral("kagebaito"), QStringLiteral("autofish"));
    settings.setValue(QStringLiteral("ui/%1").arg(key), value);
}

bool AppSettings::setupComplete() const { return m_setupComplete; }
void AppSettings::setSetupComplete(bool complete)
{
    if (m_setupComplete == complete) return;
    m_setupComplete = complete;
    save(QStringLiteral("setupComplete"), complete);
    emit setupCompleteChanged();
}

QString AppSettings::platform() const { return m_platform; }
void AppSettings::setPlatform(const QString &platform)
{
    if (m_platform == platform) return;
    m_platform = platform;
    save(QStringLiteral("platform"), platform);
    emit platformChanged();
    emit setupChanged();
}

QString AppSettings::captureBackend() const { return m_captureBackend; }
void AppSettings::setCaptureBackend(const QString &backend)
{
    if (m_captureBackend == backend) return;
    m_captureBackend = backend;
    save(QStringLiteral("captureBackend"), backend);
    emit captureBackendChanged();
    emit setupChanged();
}

QString AppSettings::experienceMode() const { return m_experienceMode; }
void AppSettings::setExperienceMode(const QString &mode)
{
    const QString normalized = mode == QStringLiteral("PRO") ? QStringLiteral("PRO") : QStringLiteral("EZ");
    if (m_experienceMode == normalized) return;
    m_experienceMode = normalized;
    save(QStringLiteral("experienceMode"), normalized);
    emit experienceModeChanged();
}

bool AppSettings::debugMode() const { return m_debugMode; }
void AppSettings::setDebugMode(bool enabled)
{
    if (m_debugMode == enabled) return;
    m_debugMode = enabled;
    save(QStringLiteral("debugMode"), enabled);
    emit debugModeChanged();
}

QString AppSettings::uiLanguage() const { return m_uiLanguage; }
void AppSettings::setUiLanguage(const QString &language)
{
    const QString normalized = language == QStringLiteral("ru")
        || language == QStringLiteral("ja")
        || language == QStringLiteral("zh_CN")
        ? language
        : QStringLiteral("en");
    if (m_uiLanguage == normalized) return;
    m_uiLanguage = normalized;
    save(QStringLiteral("uiLanguage"), normalized);
    emit uiLanguageChanged();
}

bool AppSettings::saveDebugFrames() const { return m_saveDebugFrames; }
void AppSettings::setSaveDebugFrames(bool enabled)
{
    if (m_saveDebugFrames == enabled) return;
    m_saveDebugFrames = enabled;
    save(QStringLiteral("saveDebugFrames"), enabled);
    emit saveDebugFramesChanged();
}

bool AppSettings::writeLogsToFile() const { return m_writeLogsToFile; }
void AppSettings::setWriteLogsToFile(bool enabled)
{
    if (m_writeLogsToFile == enabled) return;
    m_writeLogsToFile = enabled;
    save(QStringLiteral("writeLogsToFile"), enabled);
    emit writeLogsToFileChanged();
}

AppSettings::ReelControlMode AppSettings::reelControlMode() const { return m_reelControlMode; }
int AppSettings::reelControlModeInt() const { return static_cast<int>(m_reelControlMode); }
void AppSettings::setReelControlModeInt(int mode)
{
    setReelControlMode(static_cast<ReelControlMode>(qBound(static_cast<int>(ReelControlMode::Boundary),
                                                           mode,
                                                           static_cast<int>(ReelControlMode::KagebaitoGuard))));
}
void AppSettings::setReelControlMode(ReelControlMode mode)
{
    if (m_reelControlMode == mode) return;
    m_reelControlMode = mode;
    save(QStringLiteral("reelControlMode"), static_cast<int>(mode));
    emit reelControlModeChanged();
}

int AppSettings::experimentalLeadMs() const { return m_experimentalLeadMs; }
void AppSettings::setExperimentalLeadMs(int value)
{
    const int bounded = qBound(0, value, 140);
    if (m_experimentalLeadMs == bounded) return;
    m_experimentalLeadMs = bounded;
    save(QStringLiteral("experimentalLeadMs"), bounded);
    emit experimentalTuningChanged();
}

int AppSettings::experimentalSafeMarginPercent() const { return m_experimentalSafeMarginPercent; }
void AppSettings::setExperimentalSafeMarginPercent(int value)
{
    const int bounded = qBound(5, value, 40);
    if (m_experimentalSafeMarginPercent == bounded) return;
    m_experimentalSafeMarginPercent = bounded;
    save(QStringLiteral("experimentalSafeMarginPercent"), bounded);
    emit experimentalTuningChanged();
}

int AppSettings::experimentalSettleMs() const { return m_experimentalSettleMs; }
void AppSettings::setExperimentalSettleMs(int value)
{
    const int bounded = qBound(0, value, 250);
    if (m_experimentalSettleMs == bounded) return;
    m_experimentalSettleMs = bounded;
    save(QStringLiteral("experimentalSettleMs"), bounded);
    emit experimentalTuningChanged();
}

int AppSettings::experimentalBrakeMs() const { return m_experimentalBrakeMs; }
void AppSettings::setExperimentalBrakeMs(int value)
{
    const int bounded = qBound(0, value, 180);
    if (m_experimentalBrakeMs == bounded) return;
    m_experimentalBrakeMs = bounded;
    save(QStringLiteral("experimentalBrakeMs"), bounded);
    emit experimentalTuningChanged();
}

int AppSettings::experimentalMaxPulseMs() const { return m_experimentalMaxPulseMs; }
void AppSettings::setExperimentalMaxPulseMs(int value)
{
    const int bounded = qBound(32, value, 180);
    if (m_experimentalMaxPulseMs == bounded) return;
    m_experimentalMaxPulseMs = bounded;
    save(QStringLiteral("experimentalMaxPulseMs"), bounded);
    emit experimentalTuningChanged();
}

int AppSettings::experimentalMinGapMs() const { return m_experimentalMinGapMs; }
void AppSettings::setExperimentalMinGapMs(int value)
{
    const int bounded = qBound(0, value, 30);
    if (m_experimentalMinGapMs == bounded) return;
    m_experimentalMinGapMs = bounded;
    save(QStringLiteral("experimentalMinGapMs"), bounded);
    emit experimentalTuningChanged();
}

void AppSettings::resetExperimentalTuning()
{
    m_experimentalLeadMs = 55;
    m_experimentalSafeMarginPercent = 20;
    m_experimentalSettleMs = 110;
    m_experimentalBrakeMs = 85;
    m_experimentalMaxPulseMs = 115;
    m_experimentalMinGapMs = 4;
    save(QStringLiteral("experimentalLeadMs"), m_experimentalLeadMs);
    save(QStringLiteral("experimentalSafeMarginPercent"), m_experimentalSafeMarginPercent);
    save(QStringLiteral("experimentalSettleMs"), m_experimentalSettleMs);
    save(QStringLiteral("experimentalBrakeMs"), m_experimentalBrakeMs);
    save(QStringLiteral("experimentalMaxPulseMs"), m_experimentalMaxPulseMs);
    save(QStringLiteral("experimentalMinGapMs"), m_experimentalMinGapMs);
    emit experimentalTuningChanged();
}

int AppSettings::fishingHookDelayMs() const { return m_fishingHookDelayMs; }
void AppSettings::setFishingHookDelayMs(int value)
{
    const int bounded = qBound(1000, value, 6000);
    if (m_fishingHookDelayMs == bounded) return;
    m_fishingHookDelayMs = bounded;
    save(QStringLiteral("fishingHookDelayMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingHookJitterMs() const { return m_fishingHookJitterMs; }
void AppSettings::setFishingHookJitterMs(int value)
{
    const int bounded = qBound(0, value, 1500);
    if (m_fishingHookJitterMs == bounded) return;
    m_fishingHookJitterMs = bounded;
    save(QStringLiteral("fishingHookJitterMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingEventResetMs() const { return m_fishingEventResetMs; }
void AppSettings::setFishingEventResetMs(int value)
{
    const int bounded = qBound(500, value, 8000);
    if (m_fishingEventResetMs == bounded) return;
    m_fishingEventResetMs = bounded;
    save(QStringLiteral("fishingEventResetMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingResultEscBaseMs() const { return m_fishingResultEscBaseMs; }
void AppSettings::setFishingResultEscBaseMs(int value)
{
    const int bounded = qBound(500, value, 5000);
    if (m_fishingResultEscBaseMs == bounded) return;
    m_fishingResultEscBaseMs = bounded;
    save(QStringLiteral("fishingResultEscBaseMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingResultEscJitterMs() const { return m_fishingResultEscJitterMs; }
void AppSettings::setFishingResultEscJitterMs(int value)
{
    const int bounded = qBound(0, value, 1500);
    if (m_fishingResultEscJitterMs == bounded) return;
    m_fishingResultEscJitterMs = bounded;
    save(QStringLiteral("fishingResultEscJitterMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingRecastBaseMs() const { return m_fishingRecastBaseMs; }
void AppSettings::setFishingRecastBaseMs(int value)
{
    const int bounded = qBound(500, value, 3000);
    if (m_fishingRecastBaseMs == bounded) return;
    m_fishingRecastBaseMs = bounded;
    save(QStringLiteral("fishingRecastBaseMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingRecastJitterMs() const { return m_fishingRecastJitterMs; }
void AppSettings::setFishingRecastJitterMs(int value)
{
    const int bounded = qBound(0, value, 800);
    if (m_fishingRecastJitterMs == bounded) return;
    m_fishingRecastJitterMs = bounded;
    save(QStringLiteral("fishingRecastJitterMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingAwaitingReelMs() const { return m_fishingAwaitingReelMs; }
void AppSettings::setFishingAwaitingReelMs(int value)
{
    const int bounded = qBound(5000, value, 60000);
    if (m_fishingAwaitingReelMs == bounded) return;
    m_fishingAwaitingReelMs = bounded;
    save(QStringLiteral("fishingAwaitingReelMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingReelLostMs() const { return m_fishingReelLostMs; }
void AppSettings::setFishingReelLostMs(int value)
{
    const int bounded = qBound(500, value, 5000);
    if (m_fishingReelLostMs == bounded) return;
    m_fishingReelLostMs = bounded;
    save(QStringLiteral("fishingReelLostMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingHookRetryMs() const { return m_fishingHookRetryMs; }
void AppSettings::setFishingHookRetryMs(int value)
{
    const int bounded = qBound(500, value, 5000);
    if (m_fishingHookRetryMs == bounded) return;
    m_fishingHookRetryMs = bounded;
    save(QStringLiteral("fishingHookRetryMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingHookVisibleGraceMs() const { return m_fishingHookVisibleGraceMs; }
void AppSettings::setFishingHookVisibleGraceMs(int value)
{
    const int bounded = qBound(500, value, 5000);
    if (m_fishingHookVisibleGraceMs == bounded) return;
    m_fishingHookVisibleGraceMs = bounded;
    save(QStringLiteral("fishingHookVisibleGraceMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingRecoveryBaseMs() const { return m_fishingRecoveryBaseMs; }
void AppSettings::setFishingRecoveryBaseMs(int value)
{
    const int bounded = qBound(200, value, 2000);
    if (m_fishingRecoveryBaseMs == bounded) return;
    m_fishingRecoveryBaseMs = bounded;
    save(QStringLiteral("fishingRecoveryBaseMs"), bounded);
    emit fishingTuningChanged();
}

int AppSettings::fishingRecoveryJitterMs() const { return m_fishingRecoveryJitterMs; }
void AppSettings::setFishingRecoveryJitterMs(int value)
{
    const int bounded = qBound(0, value, 1000);
    if (m_fishingRecoveryJitterMs == bounded) return;
    m_fishingRecoveryJitterMs = bounded;
    save(QStringLiteral("fishingRecoveryJitterMs"), bounded);
    emit fishingTuningChanged();
}

void AppSettings::resetFishingTuning()
{
    m_fishingHookDelayMs = 3000;
    m_fishingHookJitterMs = 500;
    m_fishingEventResetMs = 2000;
    m_fishingResultEscBaseMs = 2000;
    m_fishingResultEscJitterMs = 500;
    m_fishingRecastBaseMs = 1200;
    m_fishingRecastJitterMs = 250;
    m_fishingAwaitingReelMs = 15000;
    m_fishingReelLostMs = 1800;
    m_fishingHookRetryMs = 1500;
    m_fishingHookVisibleGraceMs = 2200;
    m_fishingRecoveryBaseMs = 800;
    m_fishingRecoveryJitterMs = 350;
    save(QStringLiteral("fishingHookDelayMs"), m_fishingHookDelayMs);
    save(QStringLiteral("fishingHookJitterMs"), m_fishingHookJitterMs);
    save(QStringLiteral("fishingEventResetMs"), m_fishingEventResetMs);
    save(QStringLiteral("fishingResultEscBaseMs"), m_fishingResultEscBaseMs);
    save(QStringLiteral("fishingResultEscJitterMs"), m_fishingResultEscJitterMs);
    save(QStringLiteral("fishingRecastBaseMs"), m_fishingRecastBaseMs);
    save(QStringLiteral("fishingRecastJitterMs"), m_fishingRecastJitterMs);
    save(QStringLiteral("fishingAwaitingReelMs"), m_fishingAwaitingReelMs);
    save(QStringLiteral("fishingReelLostMs"), m_fishingReelLostMs);
    save(QStringLiteral("fishingHookRetryMs"), m_fishingHookRetryMs);
    save(QStringLiteral("fishingHookVisibleGraceMs"), m_fishingHookVisibleGraceMs);
    save(QStringLiteral("fishingRecoveryBaseMs"), m_fishingRecoveryBaseMs);
    save(QStringLiteral("fishingRecoveryJitterMs"), m_fishingRecoveryJitterMs);
    emit fishingTuningChanged();
}