#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

class AppSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool setupComplete READ setupComplete WRITE setSetupComplete NOTIFY setupCompleteChanged)
    Q_PROPERTY(QString platform READ platform WRITE setPlatform NOTIFY platformChanged)
    Q_PROPERTY(QString captureBackend READ captureBackend WRITE setCaptureBackend NOTIFY captureBackendChanged)
    Q_PROPERTY(QString experienceMode READ experienceMode WRITE setExperienceMode NOTIFY experienceModeChanged)
    Q_PROPERTY(bool debugMode READ debugMode WRITE setDebugMode NOTIFY debugModeChanged)
    Q_PROPERTY(QString uiLanguage READ uiLanguage WRITE setUiLanguage NOTIFY uiLanguageChanged)
    Q_PROPERTY(bool saveDebugFrames READ saveDebugFrames WRITE setSaveDebugFrames NOTIFY saveDebugFramesChanged)
    Q_PROPERTY(bool writeLogsToFile READ writeLogsToFile WRITE setWriteLogsToFile NOTIFY writeLogsToFileChanged)
    Q_PROPERTY(int reelControlMode READ reelControlModeInt WRITE setReelControlModeInt NOTIFY reelControlModeChanged)
    Q_PROPERTY(int experimentalLeadMs READ experimentalLeadMs WRITE setExperimentalLeadMs NOTIFY experimentalTuningChanged)
    Q_PROPERTY(int experimentalSafeMarginPercent READ experimentalSafeMarginPercent WRITE setExperimentalSafeMarginPercent NOTIFY experimentalTuningChanged)
    Q_PROPERTY(int experimentalSettleMs READ experimentalSettleMs WRITE setExperimentalSettleMs NOTIFY experimentalTuningChanged)
    Q_PROPERTY(int experimentalBrakeMs READ experimentalBrakeMs WRITE setExperimentalBrakeMs NOTIFY experimentalTuningChanged)
    Q_PROPERTY(int experimentalMaxPulseMs READ experimentalMaxPulseMs WRITE setExperimentalMaxPulseMs NOTIFY experimentalTuningChanged)
    Q_PROPERTY(int experimentalMinGapMs READ experimentalMinGapMs WRITE setExperimentalMinGapMs NOTIFY experimentalTuningChanged)
    Q_PROPERTY(int fishingHookDelayMs READ fishingHookDelayMs WRITE setFishingHookDelayMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingHookJitterMs READ fishingHookJitterMs WRITE setFishingHookJitterMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingEventResetMs READ fishingEventResetMs WRITE setFishingEventResetMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingResultEscBaseMs READ fishingResultEscBaseMs WRITE setFishingResultEscBaseMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingResultEscJitterMs READ fishingResultEscJitterMs WRITE setFishingResultEscJitterMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingRecastBaseMs READ fishingRecastBaseMs WRITE setFishingRecastBaseMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingRecastJitterMs READ fishingRecastJitterMs WRITE setFishingRecastJitterMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingAwaitingReelMs READ fishingAwaitingReelMs WRITE setFishingAwaitingReelMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingReelLostMs READ fishingReelLostMs WRITE setFishingReelLostMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingHookRetryMs READ fishingHookRetryMs WRITE setFishingHookRetryMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingHookVisibleGraceMs READ fishingHookVisibleGraceMs WRITE setFishingHookVisibleGraceMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingRecoveryBaseMs READ fishingRecoveryBaseMs WRITE setFishingRecoveryBaseMs NOTIFY fishingTuningChanged)
    Q_PROPERTY(int fishingRecoveryJitterMs READ fishingRecoveryJitterMs WRITE setFishingRecoveryJitterMs NOTIFY fishingTuningChanged)

public:
    enum class ReelControlMode {
        Experimental = 0,    // Advanced custom VSH/experimental guard logic with prediction, brake, settle (the "Experimental" controller)
        Stable = 1,          // Simple PID + basic pulse (the "Stable" controller)
        ChizukuoPid = 2,     // Adapted from Chizu (PID + humanized pulsing). May not perform well due to architecture differences (vision service rate, input model, detection). Kept for reference/comparison.
    };
    Q_ENUM(ReelControlMode)

    explicit AppSettings(QObject *parent = nullptr);

    void load();
    void save(const QString &key, const QVariant &value);

    bool setupComplete() const;
    void setSetupComplete(bool complete);
    QString platform() const;
    void setPlatform(const QString &platform);
    QString captureBackend() const;
    void setCaptureBackend(const QString &backend);
    QString experienceMode() const;
    void setExperienceMode(const QString &mode);
    bool debugMode() const;
    void setDebugMode(bool enabled);
    QString uiLanguage() const;
    void setUiLanguage(const QString &language);
    bool saveDebugFrames() const;
    void setSaveDebugFrames(bool enabled);
    bool writeLogsToFile() const;
    void setWriteLogsToFile(bool enabled);
    ReelControlMode reelControlMode() const;
    void setReelControlMode(ReelControlMode mode);
    int reelControlModeInt() const;
    void setReelControlModeInt(int mode);
    int experimentalLeadMs() const;
    void setExperimentalLeadMs(int value);
    int experimentalSafeMarginPercent() const;
    void setExperimentalSafeMarginPercent(int value);
    int experimentalSettleMs() const;
    void setExperimentalSettleMs(int value);
    int experimentalBrakeMs() const;
    void setExperimentalBrakeMs(int value);
    int experimentalMaxPulseMs() const;
    void setExperimentalMaxPulseMs(int value);
    int experimentalMinGapMs() const;
    void setExperimentalMinGapMs(int value);
    void resetExperimentalTuning();
    int fishingHookDelayMs() const;
    void setFishingHookDelayMs(int value);
    int fishingHookJitterMs() const;
    void setFishingHookJitterMs(int value);
    int fishingEventResetMs() const;
    void setFishingEventResetMs(int value);
    int fishingResultEscBaseMs() const;
    void setFishingResultEscBaseMs(int value);
    int fishingResultEscJitterMs() const;
    void setFishingResultEscJitterMs(int value);
    int fishingRecastBaseMs() const;
    void setFishingRecastBaseMs(int value);
    int fishingRecastJitterMs() const;
    void setFishingRecastJitterMs(int value);
    int fishingAwaitingReelMs() const;
    void setFishingAwaitingReelMs(int value);
    int fishingReelLostMs() const;
    void setFishingReelLostMs(int value);
    int fishingHookRetryMs() const;
    void setFishingHookRetryMs(int value);
    int fishingHookVisibleGraceMs() const;
    void setFishingHookVisibleGraceMs(int value);
    int fishingRecoveryBaseMs() const;
    void setFishingRecoveryBaseMs(int value);
    int fishingRecoveryJitterMs() const;
    void setFishingRecoveryJitterMs(int value);
    void resetFishingTuning();

signals:
    void setupCompleteChanged();
    void platformChanged();
    void captureBackendChanged();
    void setupChanged();
    void experienceModeChanged();
    void debugModeChanged();
    void uiLanguageChanged();
    void saveDebugFramesChanged();
    void writeLogsToFileChanged();
    void reelControlModeChanged();
    void experimentalTuningChanged();
    void fishingTuningChanged();

private:
    bool m_setupComplete = false;
    QString m_platform = QStringLiteral("Wayland");
    QString m_captureBackend = QStringLiteral("PipeWire Portal");
    QString m_experienceMode = QStringLiteral("EZ");
    bool m_debugMode = false;
    QString m_uiLanguage = QStringLiteral("en");
    bool m_saveDebugFrames = false;
    bool m_writeLogsToFile = false;
    ReelControlMode m_reelControlMode = ReelControlMode::Experimental;
    int m_experimentalLeadMs = 55;
    int m_experimentalSafeMarginPercent = 20;
    int m_experimentalSettleMs = 110;
    int m_experimentalBrakeMs = 85;
    int m_experimentalMaxPulseMs = 115;
    int m_experimentalMinGapMs = 4;
    int m_fishingHookDelayMs = 3000;
    int m_fishingHookJitterMs = 500;
    int m_fishingEventResetMs = 2000;
    int m_fishingResultEscBaseMs = 2000;
    int m_fishingResultEscJitterMs = 500;
    int m_fishingRecastBaseMs = 1200;
    int m_fishingRecastJitterMs = 250;
    int m_fishingAwaitingReelMs = 15000;
    int m_fishingReelLostMs = 1800;
    int m_fishingHookRetryMs = 1500;
    int m_fishingHookVisibleGraceMs = 2200;
    int m_fishingRecoveryBaseMs = 800;
    int m_fishingRecoveryJitterMs = 350;
};