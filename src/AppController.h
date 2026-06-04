#pragma once

#include "AppSettings.h"
#include "FishingFlowController.h"
#include "InputDevice.h"
#include "ReelController.h"
#include "VisionService.h"

#include <QObject>
#include <QProcess>
#include <QList>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QImage>

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString lastEvent READ lastEvent NOTIFY lastEventChanged)
    Q_PROPERTY(QString rawEvents READ rawEvents NOTIFY rawEventsChanged)
    Q_PROPERTY(QString fishingEvent READ fishingEvent NOTIFY fishingEventChanged)
    Q_PROPERTY(QString fishingLog READ fishingLog NOTIFY fishingLogChanged)
    Q_PROPERTY(QString reelControl READ reelControl NOTIFY reelControlChanged)
    Q_PROPERTY(bool usePidReelControl READ usePidReelControl WRITE setUsePidReelControl NOTIFY usePidReelControlChanged)
    Q_PROPERTY(int reelControlMode READ reelControlMode WRITE setReelControlMode NOTIFY reelControlModeChanged)
    Q_PROPERTY(bool saveDebugFrames READ saveDebugFrames WRITE setSaveDebugFrames NOTIFY saveDebugFramesChanged)
    Q_PROPERTY(bool writeLogsToFile READ writeLogsToFile WRITE setWriteLogsToFile NOTIFY writeLogsToFileChanged)
    Q_PROPERTY(bool setupComplete READ setupComplete NOTIFY setupCompleteChanged)
    Q_PROPERTY(QString platform READ platform NOTIFY setupChanged)
    Q_PROPERTY(QString captureBackend READ captureBackend NOTIFY setupChanged)
    Q_PROPERTY(QString experienceMode READ experienceMode WRITE setExperienceMode NOTIFY experienceModeChanged)
    Q_PROPERTY(bool debugMode READ debugMode WRITE setDebugMode NOTIFY debugModeChanged)
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
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(int selectedWindowIndex READ selectedWindowIndex NOTIFY selectedWindowChanged)
    Q_PROPERTY(QString selectedWindowLabel READ selectedWindowLabel NOTIFY selectedWindowChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(bool windowsSetupAvailable READ windowsSetupAvailable CONSTANT)
    Q_PROPERTY(QString uiLanguage READ uiLanguage WRITE setUiLanguage NOTIFY uiLanguageChanged)
    Q_PROPERTY(bool inputReady READ inputReady NOTIFY inputReadyChanged)
    Q_PROPERTY(QString captureFrameSize READ captureFrameSize NOTIFY captureFrameSizeChanged)
    Q_PROPERTY(QString debugOverlaySource READ debugOverlaySource NOTIFY debugOverlaySourceChanged)
    Q_PROPERTY(bool pythonSetupInProgress READ pythonSetupInProgress NOTIFY pythonSetupInProgressChanged)
    Q_PROPERTY(QString pythonSetupMessage READ pythonSetupMessage NOTIFY pythonSetupMessageChanged)

public:
    using ReelControlMode = AppSettings::ReelControlMode;

    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    AppSettings *settings();
    const AppSettings *settings() const;
    InputDevice *input();
    VisionService *vision();
    FishingFlowController *fishing();
    ReelController *reel();

    QString status() const;
    QString lastEvent() const;
    QString rawEvents() const;
    QString fishingEvent() const;
    QString fishingLog() const;
    QString reelControl() const;
    bool usePidReelControl() const;
    void setUsePidReelControl(bool enabled);
    int reelControlMode() const;
    void setReelControlMode(int mode);
    bool saveDebugFrames() const;
    void setSaveDebugFrames(bool enabled);
    bool writeLogsToFile() const;
    void setWriteLogsToFile(bool enabled);
    bool setupComplete() const;
    QString platform() const;
    QString captureBackend() const;
    QString experienceMode() const;
    void setExperienceMode(const QString &mode);
    bool debugMode() const;
    void setDebugMode(bool enabled);
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
    QVariantList windows() const;
    int selectedWindowIndex() const;
    QString selectedWindowLabel() const;
    QString appVersion() const;
    QString qtVersion() const;
    bool windowsSetupAvailable() const;
    QString uiLanguage() const;
    void setUiLanguage(const QString &language);

    Q_INVOKABLE void refreshWindows();
    Q_INVOKABLE void selectWindow(int index);
    Q_INVOKABLE void startCapture();
    Q_INVOKABLE void startVision();
    Q_INVOKABLE void startPortalVision();
    Q_INVOKABLE void stopVision();
    Q_INVOKABLE void completeSetup(const QString &platform, const QString &captureBackend);
    Q_INVOKABLE void resetSetup();
    Q_INVOKABLE void resetExperimentalTuning();
    Q_INVOKABLE void resetFishingTuning();
    Q_INVOKABLE void updateCaptureSetup(const QString &platform, const QString &captureBackend);
    Q_INVOKABLE void refreshDiagnostics();
    Q_INVOKABLE void showAboutQt();

    bool inputReady() const;
    QString captureFrameSize() const;

    QString debugOverlaySource() const;
    void updateDebugOverlay(const QImage &img);

    QString repoRoot() const;
    QString pythonExecutable() const;
    QProcessEnvironment pythonEnvironment() const;

    bool validateVisionPython(QString *errorMessage = nullptr) const;

    bool pythonSetupInProgress() const;
    QString pythonSetupMessage() const;

    void ensureVisionPythonEnvironment();

    void prepareVisionSession();
    void handleVisionLine(const QString &line, bool &frameGeometryChecked);
    void onVisionStopped(int exitCode);
    void notifyReelDirectionChanged(int direction);

    void setStatus(const QString &status);
    void setLastEvent(const QString &event);
    void appendRawEvent(const QString &event);
    void appendLineToLogFile(const QString &fileName, const QString &line);
    void setFishingEvent(const QString &event);
    void appendFishingLog(const QString &event);
    void setReelControl(const QString &event);
    bool ensureUinput();
    void setReelDirection(int direction);
    void sendKeyTap(int keyCode);
    void resetReelControllerState();
    void appendFishingLogOnce(const QString &event);

signals:
    void statusChanged();
    void lastEventChanged();
    void rawEventsChanged();
    void fishingEventChanged();
    void fishingLogChanged();
    void reelControlChanged();
    void usePidReelControlChanged();
    void reelControlModeChanged();
    void saveDebugFramesChanged();
    void writeLogsToFileChanged();
    void setupCompleteChanged();
    void setupChanged();
    void experienceModeChanged();
    void debugModeChanged();
    void experimentalTuningChanged();
    void fishingTuningChanged();
    void windowsChanged();
    void selectedWindowChanged();

    void pythonSetupInProgressChanged();
    void pythonSetupMessageChanged();
    void uiLanguageChanged();
    void inputReadyChanged();
    void captureFrameSizeChanged();
    void debugOverlaySourceChanged();
    void windowPickerRequired();

private slots:
    void onInputOutput();
    void onInputErrorOutput();
    void onInputFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onVenvCreated(int exitCode);
    void onPipInstalled(int exitCode);
    void onVenvSetupFailed(const QString &reason);
private:
    QString m_status = QStringLiteral("idle");
    QString m_lastEvent = QStringLiteral("No events yet");
    QString m_rawEvents;
    QStringList m_rawEventLines;
    QString m_fishingEvent;
    QString m_fishingLog;
    QStringList m_fishingLogLines;
    QString m_reelControl = QStringLiteral("No reel control yet");
    QString m_inputStdoutBuffer;

    qint64 m_lastRawLogMs = 0;
    qint64 m_lastReelControlUiMs = 0;
    bool m_inputReady = false;
    int m_captureFrameWidth = 0;
    int m_captureFrameHeight = 0;

    QImage m_debugOverlayImage;
    QString m_debugOverlaySource;
    int m_debugOverlayVersion = 0;
    qint64 m_lastDebugOverlayUpdateMs = 0;

    AppSettings m_settings;
    InputDevice m_input;
    VisionService m_vision;
    FishingFlowController m_fishing;
    ReelController m_reel;
    QProcess m_inputProcess;
    QTimer m_reelControlTimer;

    // Automatic vision venv setup on startup
    QProcess m_venvCreateProcess;
    QProcess m_pipInstallProcess;
    bool m_pythonSetupInProgress = false;
    QString m_pythonSetupMessage;
    bool m_pythonEnvReady = false;
};
