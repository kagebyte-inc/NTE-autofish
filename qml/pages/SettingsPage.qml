import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

Item {
    id: root
    required property var shell
    anchors.fill: parent
    visible: shell.page === "settings"

    readonly property bool proMode: AppController.experienceMode === "PRO"
    readonly property var sectionKeys: ["general", "capture", "fishing", "reel", "advanced"]

    property int sectionIndex: 0

    function sectionTitle(key) {
        switch (key) {
        case "general": return Theme.t("settings.general")
        case "capture": return Theme.t("settings.capture")
        case "fishing": return Theme.t("fishingMode")
        case "reel": return Theme.t("reelController")
        case "advanced": return Theme.t("settings.advanced")
        }
        return ""
    }

    function selectReelMode(mode) {
        if (AppController.reelControlMode === mode) {
            return
        }
        AppController.reelControlMode = mode
        if (mode === 1) {
            shell.openChizukuoPidDialog()
        } else if (mode === 3) {
            shell.openExperimentalDialog()
        }
    }

    Component.onCompleted: AppController.refreshDiagnostics()

    RowLayout {
        anchors.fill: parent
        spacing: 12

        GlassCard {
            Layout.preferredWidth: 188
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 4

                Repeater {
                    model: root.sectionKeys
                    SettingsNavItem {
                        Layout.fillWidth: true
                        text: root.sectionTitle(modelData)
                        selected: root.sectionIndex === index
                        onActivated: root.sectionIndex = index
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        GlassCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                SectionTitle {
                    text: root.sectionTitle(root.sectionKeys[root.sectionIndex])
                    Layout.fillWidth: true
                }

                ScrollView {
                    id: settingsScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: settingsScroll.availableWidth
                        spacing: 10

                        ColumnLayout {
                            visible: root.sectionIndex === 0
                            spacing: 10
                            Layout.fillWidth: true

                            SmallCaption {
                                text: Theme.t("language")
                                color: Theme.textPrimary
                            }
                            SkinComboBox {
                                Layout.fillWidth: true
                                model: shell.languageNames()
                                currentIndex: shell.languageIndex()
                                onActivated: AppController.uiLanguage = shell.languageCodes[currentIndex]
                                panelColor: Theme.panel
                                borderColor: Theme.border
                                textColor: Theme.textPrimary
                                mutedColor: Theme.muted
                                accentColor: Theme.accent
                            }
                            SmallCaption { text: "v" + AppController.appVersion; color: Theme.muted }

                            SectionTitle { text: Theme.t("settings.system") }
                            SettingsRow {
                                label: Theme.t("settings.input")
                                value: AppController.inputReady ? Theme.t("settings.input.ok") : Theme.t("settings.input.fail")
                                valueColor: AppController.inputReady ? Theme.success : Theme.danger
                            }
                            SettingsRow {
                                label: Theme.t("settings.vision")
                                value: Theme.status(AppController.status)
                                caption: AppController.lastEvent
                            }
                            SettingsRow {
                                label: Theme.t("settings.captureFrame")
                                value: AppController.captureFrameSize
                                caption: AppController.platform + " / " + AppController.captureBackend
                            }
                        }

                        ColumnLayout {
                            visible: root.sectionIndex === 1
                            spacing: 10
                            Layout.fillWidth: true

                            SmallCaption {
                                text: Theme.t("settings.capture.caption")
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            SettingsRow {
                                label: Theme.t("platform")
                                value: AppController.platform
                            }
                            SettingsRow {
                                label: Theme.t("captureBackend")
                                value: AppController.captureBackend
                            }
                            SmallCaption {
                                visible: AppController.status === "watching"
                                text: Theme.t("settings.capture.watchingHint")
                                color: Theme.warning
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            GlassButton {
                                text: Theme.t("settings.capture.change")
                                variant: "neutral"
                                enabled: AppController.status !== "watching"
                                onClicked: shell.openCaptureDialog()
                            }
                        }

                        ColumnLayout {
                            visible: root.sectionIndex === 2
                            spacing: 10
                            Layout.fillWidth: true

                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                InfoButton {
                                    onClicked: shell.openHelp(Theme.t("fishingMode"), Theme.t("fishingMode.help"))
                                }
                            }
                            RowLayout {
                                spacing: 10
                                GlassButton {
                                    text: "EZ"
                                    iconSource: Theme.icon("ez.png")
                                    variant: AppController.experienceMode === "EZ" ? "primary" : "neutral"
                                    onClicked: AppController.experienceMode = "EZ"
                                }
                                GlassButton {
                                    text: "PRO"
                                    iconSource: Theme.icon("pro.png")
                                    variant: proMode ? "warning" : "neutral"
                                    onClicked: AppController.experienceMode = "PRO"
                                }
                            }
                            SmallCaption {
                                text: Theme.t("fishingMode.help")
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }

                            GlassCard {
                                visible: proMode
                                Layout.fillWidth: true
                                implicitHeight: fishingTuningLayout.implicitHeight + 20
                                color: Theme.logBg
                                border.color: Theme.warning

                                ColumnLayout {
                                    id: fishingTuningLayout
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        SectionTitle {
                                            text: Theme.t("fishingTuning.title")
                                            Layout.fillWidth: true
                                        }
                                        GlassButton {
                                            text: Theme.t("reset")
                                            iconSource: Theme.icon("exp_reset.png")
                                            variant: "neutral"
                                            onClicked: AppController.resetFishingTuning()
                                        }
                                    }

                                    SmallCaption {
                                        text: Theme.t("fishingTuning.caption")
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        TuningSpinBox {
                                            label: Theme.t("tuning.hookDelay")
                                            from: 1000; to: 6000; step: 10
                                            currentValue: AppController.fishingHookDelayMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingHookDelayMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.hookJitter")
                                            from: 0; to: 1500; step: 10
                                            currentValue: AppController.fishingHookJitterMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingHookJitterMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.eventReset")
                                            from: 500; to: 8000; step: 10
                                            currentValue: AppController.fishingEventResetMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingEventResetMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.resultEscDelay")
                                            from: 500; to: 5000; step: 10
                                            currentValue: AppController.fishingResultEscBaseMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingResultEscBaseMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.resultEscJitter")
                                            from: 0; to: 1500; step: 10
                                            currentValue: AppController.fishingResultEscJitterMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingResultEscJitterMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.recastDelay")
                                            from: 500; to: 3000; step: 10
                                            currentValue: AppController.fishingRecastBaseMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingRecastBaseMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.recastJitter")
                                            from: 0; to: 800; step: 10
                                            currentValue: AppController.fishingRecastJitterMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingRecastJitterMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.awaitingReelTimeout")
                                            from: 5000; to: 60000; step: 100
                                            currentValue: AppController.fishingAwaitingReelMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingAwaitingReelMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.reelLostTimeout")
                                            from: 500; to: 5000; step: 10
                                            currentValue: AppController.fishingReelLostMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingReelLostMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.hookRetryDelay")
                                            from: 500; to: 5000; step: 10
                                            currentValue: AppController.fishingHookRetryMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingHookRetryMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.hookVisibleGrace")
                                            from: 500; to: 5000; step: 10
                                            currentValue: AppController.fishingHookVisibleGraceMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingHookVisibleGraceMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.recoveryDelay")
                                            from: 200; to: 2000; step: 10
                                            currentValue: AppController.fishingRecoveryBaseMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingRecoveryBaseMs = v }
                                        }
                                        TuningSpinBox {
                                            label: Theme.t("tuning.recoveryJitter")
                                            from: 0; to: 1000; step: 10
                                            currentValue: AppController.fishingRecoveryJitterMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.fishingRecoveryJitterMs = v }
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            visible: root.sectionIndex === 3
                            spacing: 8
                            Layout.fillWidth: true

                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                InfoButton {
                                    onClicked: shell.openHelp(Theme.t("reelController"), Theme.t("reelController.help"))
                                }
                            }

                            SkinComboBox {
                                Layout.fillWidth: true
                                model: shell.reelModeNames()
                                currentIndex: AppController.reelControlMode
                                onActivated: selectReelMode(currentIndex)
                                panelColor: Theme.panel
                                borderColor: Theme.border
                                textColor: Theme.textPrimary
                                mutedColor: Theme.muted
                                accentColor: Theme.accent
                            }

                            GlassCard {
                                visible: AppController.reelControlMode === 3
                                Layout.fillWidth: true
                                implicitHeight: experimentalTuningLayout.implicitHeight + 20
                                color: Theme.logBg
                                border.color: Theme.warning

                                ColumnLayout {
                                    id: experimentalTuningLayout
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        SectionTitle {
                                            text: Theme.t("experimental.title")
                                            Layout.fillWidth: true
                                        }
                                        GlassButton {
                                            text: Theme.t("reset")
                                            iconSource: Theme.icon("exp_reset.png")
                                            variant: "neutral"
                                            onClicked: AppController.resetExperimentalTuning()
                                        }
                                    }

                                    SmallCaption {
                                        text: Theme.t("experimental.caption")
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        TuningSlider {
                                            label: Theme.t("tuning.predictionLead")
                                            from: 0; to: 140; step: 5
                                            currentValue: AppController.experimentalLeadMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.experimentalLeadMs = v }
                                        }
                                        TuningSlider {
                                            label: Theme.t("tuning.safeMargin")
                                            from: 5; to: 40; step: 1
                                            currentValue: AppController.experimentalSafeMarginPercent
                                            suffix: "%"
                                            onValueEdited: function(v) { AppController.experimentalSafeMarginPercent = v }
                                        }
                                        TuningSlider {
                                            label: Theme.t("tuning.insideZoneHold")
                                            from: 0; to: 250; step: 5
                                            currentValue: AppController.experimentalSettleMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.experimentalSettleMs = v }
                                        }
                                        TuningSlider {
                                            label: Theme.t("tuning.brakeLookahead")
                                            from: 0; to: 180; step: 5
                                            currentValue: AppController.experimentalBrakeMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.experimentalBrakeMs = v }
                                        }
                                        TuningSlider {
                                            label: Theme.t("tuning.maxChasePulse")
                                            from: 32; to: 180; step: 2
                                            currentValue: AppController.experimentalMaxPulseMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.experimentalMaxPulseMs = v }
                                        }
                                        TuningSlider {
                                            label: Theme.t("tuning.minPulseGap")
                                            from: 0; to: 30; step: 1
                                            currentValue: AppController.experimentalMinGapMs
                                            suffix: " ms"
                                            onValueEdited: function(v) { AppController.experimentalMinGapMs = v }
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            visible: root.sectionIndex === 4
                            spacing: 10
                            Layout.fillWidth: true

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Switch {
                                    checked: AppController.debugMode
                                    onToggled: AppController.debugMode = checked
                                    palette: shell.palette
                                }
                                SmallCaption {
                                    text: Theme.t("debugMessages")
                                    color: Theme.textPrimary
                                    Layout.fillWidth: true
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Switch {
                                    checked: AppController.saveDebugFrames
                                    onToggled: AppController.saveDebugFrames = checked
                                    palette: shell.palette
                                }
                                SmallCaption {
                                    text: AppController.saveDebugFrames ? Theme.t("savingScreenshots") : Theme.t("notSavingScreenshots")
                                    color: AppController.saveDebugFrames ? Theme.warning : Theme.muted
                                    Layout.fillWidth: true
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Switch {
                                    checked: AppController.writeLogsToFile
                                    onToggled: AppController.writeLogsToFile = checked
                                    palette: shell.palette
                                }
                                SmallCaption {
                                    text: Theme.t("writeLogs")
                                    color: Theme.textPrimary
                                    Layout.fillWidth: true
                                }
                            }
                            SmallCaption {
                                visible: AppController.writeLogsToFile
                                text: Theme.t("settings.logsHint")
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            GlassButton {
                                text: Theme.t("resetSetup")
                                iconSource: Theme.icon("all_reset.png")
                                variant: "danger"
                                onClicked: shell.openResetSetupDialog()
                            }
                        }
                    }
                }
            }
        }
    }
}