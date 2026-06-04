import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

RowLayout {
    id: root
    required property var shell
    anchors.fill: parent
    spacing: 14
    visible: shell.page === "home"

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            GlassCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 96
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    SmallCaption { text: Theme.t("state") }
                    Label {
                        text: Theme.status(AppController.status)
                        color: AppController.status === "watching" ? Theme.success : Theme.textPrimary
                        font.pixelSize: 24
                        font.bold: true
                    }
                }
            }

            GlassCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 96
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    SmallCaption { text: Theme.t("currentEvent") }
                    Label {
                        text: AppController.fishingEvent.length > 0
                              ? AppController.fishingEvent
                              : Theme.t("placeholder.event")
                        color: AppController.fishingEvent.indexOf("fish_on_hook") >= 0
                               || AppController.fishingEvent.indexOf("Hook!") >= 0 ? Theme.success : Theme.textPrimary
                        font.pixelSize: 15
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        GlassCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                SectionTitle { text: Theme.t("fishingControls") }

                SmallCaption {
                    visible: AppController.pythonSetupInProgress || AppController.pythonSetupMessage.length > 0
                    text: AppController.pythonSetupMessage
                    color: AppController.pythonSetupInProgress ? Theme.warning : Theme.muted
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    GlassButton {
                        text: Theme.t("start")
                        variant: "primary"
                        enabled: AppController.status !== "watching" && !AppController.pythonSetupInProgress
                        onClicked: AppController.startCapture()
                    }

                    GlassButton {
                        text: Theme.t("stop")
                        variant: "danger"
                        enabled: AppController.status === "watching"
                        onClicked: AppController.stopVision()
                    }

                    Item { Layout.fillWidth: true }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                    TextArea {
                        text: AppController.fishingLog.length > 0
                              ? AppController.fishingLog
                              : Theme.t("placeholder.log")
                        readOnly: true
                        wrapMode: TextEdit.NoWrap
                        color: Theme.textPrimary
                        selectedTextColor: Theme.bg
                        selectionColor: Theme.accent
                        background: Rectangle {
                            color: Theme.logBg
                            border.color: Theme.border
                            radius: 6
                        }
                    }
                }
            }
        }
    }
}