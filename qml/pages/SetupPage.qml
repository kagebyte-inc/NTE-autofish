import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import Autofish

Item {
    id: root
    required property var shell
    anchors.fill: parent
    visible: !AppController.setupComplete

    layer.enabled: shell.modalOpen
    layer.effect: MultiEffect {
        blurEnabled: true
        blur: 0.82
        blurMax: 32
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.setupOverlay
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: 28

        ColumnLayout {
            Layout.preferredWidth: Math.min(390, parent.width * 0.42)
            Layout.maximumWidth: 390
            Layout.minimumWidth: 320
            Layout.fillHeight: true
            spacing: 18

            Image {
                source: Theme.icon("autofish-icon.png")
                Layout.preferredWidth: 240
                Layout.preferredHeight: 240
                Layout.alignment: Qt.AlignHCenter
                fillMode: Image.PreserveAspectFit
            }

            Label {
                text: Theme.t("app.title")
                color: Theme.textPrimary
                font.pixelSize: 34
                font.bold: true
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            Label {
                text: Theme.t("firstLaunch")
                color: Theme.accentHover
                font.pixelSize: 18
                font.bold: true
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            SmallCaption {
                text: Theme.t("firstLaunch.caption")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            SmallCaption {
                text: Theme.t("language")
                color: Theme.textPrimary
            }

            RowLayout {
                id: languageRow
                Layout.fillWidth: true
                spacing: 10

                Image {
                    source: Theme.icon("lang.png")
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    Layout.alignment: Qt.AlignVCenter
                    fillMode: Image.PreserveAspectFit
                }

                SkinComboBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 220
                    model: shell.languageNames()
                    currentIndex: shell.languageIndex()
                    onActivated: AppController.uiLanguage = shell.languageCodes[currentIndex]
                    panelColor: Theme.panel
                    borderColor: Theme.border
                    textColor: Theme.textPrimary
                    mutedColor: Theme.muted
                    accentColor: Theme.accent
                }
            }

            Item { Layout.fillHeight: true }

            GlassButton {
                text: Theme.t("continue")
                variant: "primary"
                implicitWidth: 150
                onClicked: {
                    var platforms = ["Wayland", "X11", "Windows"]
                    var backends = ["PipeWire Portal", "X11 Window", "Win32 Capture"]
                    AppController.completeSetup(platforms[shell.setupPlatformIndex], backends[shell.setupBackendIndex])
                    shell.page = "home"
                }
            }
        }

        GlassCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 420

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 16

                SectionTitle { text: Theme.t("platform") }

                SetupChoice {
                    Layout.fillWidth: true
                    title: "Wayland"
                    caption: Theme.t("platform.wayland.caption")
                    iconSource: Theme.icon("wayland.png")
                    selected: shell.setupPlatformIndex === 0
                    onPicked: {
                        shell.setupPlatformIndex = 0
                        shell.setupBackendIndex = 0
                    }
                }

                SetupChoice {
                    Layout.fillWidth: true
                    title: "X11"
                    caption: Theme.t("platform.x11.caption")
                    iconSource: Theme.icon("x11.png")
                    selected: shell.setupPlatformIndex === 1
                    onPicked: {
                        shell.setupPlatformIndex = 1
                        shell.setupBackendIndex = 1
                    }
                }

                SetupChoice {
                    Layout.fillWidth: true
                    title: "Windows"
                    caption: Theme.t("platform.windows.caption")
                    iconSource: Theme.icon("windows.png")
                    enabled: AppController.windowsSetupAvailable
                    selected: shell.setupPlatformIndex === 2
                    onPicked: {
                        shell.setupPlatformIndex = 2
                        shell.setupBackendIndex = 2
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}