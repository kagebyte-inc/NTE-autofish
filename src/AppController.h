#pragma once

#include <QObject>
#include <QProcess>
#include <QList>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantList>

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
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(int selectedWindowIndex READ selectedWindowIndex NOTIFY selectedWindowChanged)
    Q_PROPERTY(QString selectedWindowLabel READ selectedWindowLabel NOTIFY selectedWindowChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString uiLanguage READ uiLanguage WRITE setUiLanguage NOTIFY uiLanguageChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

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
    QVariantList windows() const;
    int selectedWindowIndex() const;
    QString selectedWindowLabel() const;
    QString appVersion() const;
    QString qtVersion() const;
    QString uiLanguage() const;
    void setUiLanguage(const QString &language);

    Q_INVOKABLE void refreshWindows();
    Q_INVOKABLE void selectWindow(int index);
    Q_INVOKABLE void startVision();
    Q_INVOKABLE void startPortalVision();
    Q_INVOKABLE void stopVision();
    Q_INVOKABLE void simulateBite();
    Q_INVOKABLE void simulateResultScreen();
    Q_INVOKABLE void completeSetup(const QString &platform, const QString &captureBackend);
    Q_INVOKABLE void resetSetup();
    Q_INVOKABLE void resetExperimentalTuning();
    Q_INVOKABLE void showAboutQt();

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
    void windowsChanged();
    void selectedWindowChanged();
    void uiLanguageChanged();

private slots:
    void onVisionOutput();
    void onVisionErrorOutput();
    void onVisionFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onInputOutput();
    void onInputErrorOutput();
    void onInputFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void updateReelControl();
    void sendHookAction();
    void sendRecastAction();
    void sendResultCloseAction();
    void sendManualResultScreenAction();
    void resetFishingEvent();

private:
    enum class FishingFlowState {
        Waiting,
        HookScheduled,
        AwaitingReel,
        Reeling,
        ResultClosing,
        Recasting,
    };

    enum class ReelControlMode {
        Boundary = 0,
        ChizukuoPid = 1,
        LegacyPid = 2,
        KagebaitoGuard = 3,
    };

    void setStatus(const QString &status);
    void setLastEvent(const QString &event);
    void appendRawEvent(const QString &event);
    void appendLineToLogFile(const QString &fileName, const QString &line);
    void setFishingEvent(const QString &event);
    void appendFishingLog(const QString &event);
    void setReelControl(const QString &event);
    void loadUiSettings();
    void saveUiSetting(const QString &key, const QVariant &value);
    void handleVisionLine(const QString &line);
    bool inspectFrameGeometry(const QJsonObject &root);
    void triggerResultScreenFlow(const QString &source);
    bool inspectResultScreen(const QJsonObject &root);
    void inspectFishingEvent(const QJsonObject &root);
    void inspectReelControl(const QJsonObject &root);
    void scheduleHookAction();
    void transitionFishingFlowState(FishingFlowState state, const QString &reason = {});
    void updateFishingFlowWatchdog();
    void retryHookActionOrCleanup();
    void scheduleFOnlyRecovery(const QString &source);
    void resetReelControllerState();
    bool ensureUinput();
    void destroyUinput();
    void sendKeyTap(int keyCode);
    void setKeyDown(int keyCode, bool down);
    void sendUinputEvent(unsigned short type, unsigned short code, int value);
    void setReelDirection(int direction);
    QString repoRoot() const;
    QString pythonExecutable() const;
    QProcessEnvironment pythonEnvironment() const;

    QString m_status = QStringLiteral("idle");
    QString m_lastEvent = QStringLiteral("No events yet");
    QString m_rawEvents = QStringLiteral("No raw vision events yet");
    QStringList m_rawEventLines;
    QString m_fishingEvent = QStringLiteral("No fishing events yet");
    QString m_fishingLog = QStringLiteral("No fishing decisions yet");
    QStringList m_fishingLogLines;
    QString m_reelControl = QStringLiteral("No reel control yet");
    QString m_stdoutBuffer;
    QString m_inputStdoutBuffer;
    QVariantList m_windows;
    int m_selectedWindowIndex = -1;
    bool m_fishHookedLatched = false;
    int m_fishHookedAbsentFrames = 0;
    int m_hookBlueTriggerFrames = 0;
    bool m_resultScreenLatched = false;
    int m_resultScreenAbsentFrames = 0;
    FishingFlowState m_fishingFlowState = FishingFlowState::Waiting;
    ReelControlMode m_reelControlMode = ReelControlMode::LegacyPid;
    bool m_saveDebugFrames = false;
    bool m_writeLogsToFile = false;
    bool m_setupComplete = false;
    QString m_platform = QStringLiteral("Wayland");
    QString m_captureBackend = QStringLiteral("PipeWire Portal");
    QString m_experienceMode = QStringLiteral("EZ");
    bool m_debugMode = false;
    QString m_uiLanguage = QStringLiteral("en");
    int m_experimentalLeadMs = 55;
    int m_experimentalSafeMarginPercent = 20;
    int m_experimentalSettleMs = 110;
    int m_experimentalBrakeMs = 85;
    int m_experimentalMaxPulseMs = 115;
    int m_experimentalMinGapMs = 4;
    bool m_reelControlVisible = false;
    double m_reelTargetCenter = 0.0;
    double m_reelTargetLeft = 0.0;
    double m_reelTargetRight = 0.0;
    double m_reelMarkerCenter = 0.0;
    double m_reelTargetVelocity = 0.0;
    double m_reelMarkerVelocity = 0.0;
    double m_previousReelTargetCenter = 0.0;
    double m_previousReelMarkerCenter = 0.0;
    int m_reelFrameWidth = 0;
    qint64 m_lastReelObservationMs = 0;
    qint64 m_previousReelObservationMs = 0;
    qint64 m_lastVisionEventUiMs = 0;
    qint64 m_lastRawEventUiMs = 0;
    qint64 m_lastIgnoredResultLogMs = 0;
    qint64 m_lastReelControlUiMs = 0;
    qint64 m_fishingFlowStateEnteredMs = 0;
    qint64 m_lastHookSeenMs = 0;
    qint64 m_lastHookFiredMs = 0;
    qint64 m_lastReelSeenMs = 0;
    int m_hookRetryCount = 0;
    bool m_frameGeometryChecked = false;
    qint64 m_reelPulseEndMs = 0;
    qint64 m_nextReelPulseMs = 0;
    qint64 m_reelGuardSettleUntilMs = 0;
    qint64 m_lastReelPidMs = 0;
    double m_reelPidIntegral = 0.0;
    double m_reelPidPreviousMarker = 0.0;
    double m_reelPidDFiltered = 0.0;
    bool m_reelPidFirst = true;
    int m_reelPidLastSign = 0;
    QList<qint64> m_reelPidSignChangeMs;
    double m_reelPidAdaptiveKpScale = 1.0;
    qint64 m_reelReactionEndMs = 0;
    qint64 m_reelHumPulseEndMs = 0;
    int m_reelHumPulseState = 0;
    int m_reelHumTargetDirection = 0;
    int m_reelLastAction = 0;
    int m_reelDirection = 0;
    int m_reelPulseDirection = 0;
    int m_uinputFd = -1;
    bool m_keyADown = false;
    bool m_keyDDown = false;
    QProcess m_visionProcess;
    QProcess m_inputProcess;
    QTimer m_fishingEventResetTimer;
    QTimer m_hookActionTimer;
    QTimer m_manualResultScreenTimer;
    QTimer m_resultCloseTimer;
    QTimer m_recastTimer;
    QTimer m_reelControlTimer;
};
