import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import Qt.labs.platform as Platform
import Autofish

ApplicationWindow {
    id: shell
    width: 980
    height: 660
    visible: true
    title: Theme.t("app.title") + " v" + AppController.appVersion

    property string page: "home"
    property int setupPlatformIndex: 0
    property int setupBackendIndex: 0
    property int resetSetupConfirmStep: 0
    readonly property var languageCodes: ["en", "zh_CN", "ja", "ru"]
    readonly property var capturePlatforms: ["Wayland", "X11", "Windows"]
    readonly property var captureBackends: ["PipeWire Portal", "X11 Window", "Win32 Capture"]
    property int captureEditPlatformIndex: 0
    property int captureEditBackendIndex: 0

    property bool modalOpen: chizukuoPidDialog.opened
                             || experimentalDialog.opened
                             || aboutAppDialog.opened
                             || helpDialog.opened
                             || resetSetupDialog.opened
                             || x11WindowDialog.opened
                             || captureDialog.opened

    function openHelp(title, body) {
        helpDialog.title = title
        helpText.text = body
        helpDialog.open()
    }

    function languageIndex() {
        var index = languageCodes.indexOf(AppController.uiLanguage)
        return index < 0 ? 0 : index
    }

    function languageNames() {
        return ["English", "简体中文", "日本語", "Русский"]
    }

    function openResetSetupDialog() {
        resetSetupConfirmStep = 1
        resetSetupDialog.open()
    }

    function openChizukuoPidDialog() {
        chizukuoPidDialog.open()
    }

    function openExperimentalDialog() {
        experimentalDialog.open()
    }

    function syncCaptureEditFromSettings() {
        var platformIndex = capturePlatforms.indexOf(AppController.platform)
        captureEditPlatformIndex = platformIndex < 0 ? 0 : platformIndex
        var backendIndex = captureBackends.indexOf(AppController.captureBackend)
        captureEditBackendIndex = backendIndex < 0 ? captureEditPlatformIndex : backendIndex
    }

    function openCaptureDialog() {
        syncCaptureEditFromSettings()
        captureDialog.open()
    }

    function reelModeNames() {
        void (I18n.generation)
        return [
            Theme.t("reel.experimental"),
            Theme.t("reel.stable"),
            Theme.t("reel.chizukuo")
        ]
    }

    function windowNames() {
        void (I18n.generation)
        if (AppController.windows.length === 0) {
            return [Theme.t("noWindows")]
        }
        var names = []
        for (var i = 0; i < AppController.windows.length; ++i) {
            var item = AppController.windows[i]
            var process = item.process_name || ""
            var title = item.title || item.id || ""
            names.push(process.length > 0 ? process + " - " + title : title)
        }
        return names
    }

    Connections {
        target: AppController
        function onWindowPickerRequired() {
            x11WindowDialog.open()
        }
    }

    color: Theme.bg
    palette.window: Theme.bg
    palette.base: "#1e202c"
    palette.text: Theme.textPrimary
    palette.windowText: Theme.textPrimary
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.accent
    palette.highlightedText: "#ffffff"

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    Platform.MenuBar {
        window: shell

        Platform.Menu {
            title: Theme.t("menu.file")
            Platform.MenuItem {
                text: Theme.t("menu.exit")
                onTriggered: Qt.quit()
            }
        }

        Platform.Menu {
            title: Theme.t("menu.about")
            Platform.MenuItem {
                text: Theme.format("menu.aboutApp", AppController.appVersion)
                onTriggered: aboutAppDialog.open()
            }
            Platform.MenuItem {
                text: Theme.t("menu.aboutQt")
                onTriggered: AppController.showAboutQt()
            }
        }
    }

    AppModal {
        id: chizukuoPidDialog
        title: Theme.t("chizukuo.title")
        preferredWidth: 520
        Label {
            text: Theme.t("chizukuo.body")
            color: Theme.textPrimary
            wrapMode: Text.Wrap
            font.pixelSize: 14
            Layout.fillWidth: true
        }
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            GlassButton {
                text: Theme.t("ok")
                variant: "primary"
                onClicked: chizukuoPidDialog.close()
            }
        }
    }

    AppModal {
        id: experimentalDialog
        title: Theme.t("experimental.warning.title")
        preferredWidth: 520
        frameBorderColor: Theme.warning
        Label {
            text: Theme.t("experimental.warning.body")
            color: Theme.textPrimary
            wrapMode: Text.Wrap
            font.pixelSize: 14
            Layout.fillWidth: true
        }
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            GlassButton {
                text: Theme.t("ok")
                variant: "primary"
                onClicked: experimentalDialog.close()
            }
        }
    }

    AppModal {
        id: aboutAppDialog
        title: Theme.format("menu.aboutApp", AppController.appVersion)
        preferredWidth: 460
        Label {
            readonly property int _i18nGen: I18n.generation
            text: Theme.format("aboutApp.tagline", "Kagebyte")
                  + "\n\n"
                  + Theme.t("aboutApp.affiliation")
                  + "\n\n"
                  + Theme.t("aboutApp.credits")
                  + "\n\n"
                  + Theme.t("aboutApp.warning")
            color: Theme.textPrimary
            wrapMode: Text.Wrap
            font.pixelSize: 14
            Layout.fillWidth: true
        }
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            GlassButton {
                text: Theme.t("ok")
                variant: "primary"
                onClicked: aboutAppDialog.close()
            }
        }
    }

    AppModal {
        id: helpDialog
        preferredWidth: 560
        Label {
            id: helpText
            color: Theme.textPrimary
            wrapMode: Text.Wrap
            font.pixelSize: 14
            Layout.fillWidth: true
        }
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            GlassButton {
                text: Theme.t("ok")
                variant: "primary"
                onClicked: helpDialog.close()
            }
        }
    }

    AppModal {
        id: captureDialog
        title: Theme.t("settings.capture.change")
        preferredWidth: 520
        SmallCaption {
            text: Theme.t("settings.capture.caption")
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        SetupChoice {
            Layout.fillWidth: true
            title: "Wayland"
            caption: Theme.t("platform.wayland.caption")
            iconSource: Theme.icon("wayland.png")
            selected: captureEditPlatformIndex === 0
            onPicked: {
                captureEditPlatformIndex = 0
                captureEditBackendIndex = 0
            }
        }
        SetupChoice {
            Layout.fillWidth: true
            title: "X11"
            caption: Theme.t("platform.x11.caption")
            iconSource: Theme.icon("x11.png")
            selected: captureEditPlatformIndex === 1
            onPicked: {
                captureEditPlatformIndex = 1
                captureEditBackendIndex = 1
            }
        }
        SetupChoice {
            Layout.fillWidth: true
            title: "Windows"
            caption: Theme.t("platform.windows.caption")
            iconSource: Theme.icon("windows.png")
            enabled: AppController.windowsSetupAvailable
            selected: captureEditPlatformIndex === 2
            onPicked: {
                captureEditPlatformIndex = 2
                captureEditBackendIndex = 2
            }
        }
        footer: RowLayout {
            spacing: 10
            Item { Layout.fillWidth: true }
            GlassButton {
                text: Theme.t("cancel")
                variant: "neutral"
                onClicked: captureDialog.close()
            }
            GlassButton {
                text: Theme.t("continue")
                variant: "primary"
                onClicked: {
                    AppController.updateCaptureSetup(
                        capturePlatforms[captureEditPlatformIndex],
                        captureBackends[captureEditBackendIndex]
                    )
                    captureDialog.close()
                }
            }
        }
    }

    AppModal {
        id: resetSetupDialog
        title: resetSetupConfirmStep === 1 ? Theme.t("resetSetup.title1") : Theme.t("resetSetup.title2")
        preferredWidth: 520
        frameBorderColor: resetSetupConfirmStep === 1 ? Theme.warning : Theme.danger
        Label {
            text: resetSetupConfirmStep === 1
                  ? Theme.t("resetSetup.body1")
                  : Theme.t("resetSetup.body2")
            color: Theme.textPrimary
            wrapMode: Text.Wrap
            font.pixelSize: 14
            Layout.fillWidth: true
        }
        footer: RowLayout {
            spacing: 10
            Item { Layout.fillWidth: true }
            GlassButton {
                text: Theme.t("cancel")
                variant: "neutral"
                onClicked: resetSetupDialog.close()
            }
            GlassButton {
                text: resetSetupConfirmStep === 1 ? Theme.t("continue") : Theme.t("reset")
                variant: resetSetupConfirmStep === 1 ? "warning" : "danger"
                onClicked: {
                    if (resetSetupConfirmStep === 1) {
                        resetSetupConfirmStep = 2
                    } else {
                        resetSetupDialog.close()
                        AppController.resetSetup()
                        page = "home"
                    }
                }
            }
        }
        onClosed: resetSetupConfirmStep = 0
    }

    AppModal {
        id: x11WindowDialog
        title: Theme.t("windowPicker")
        preferredWidth: 560
        SmallCaption {
            text: Theme.t("windowPicker.caption")
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        SkinComboBox {
            Layout.fillWidth: true
            model: windowNames()
            currentIndex: AppController.windows.length === 0 ? 0 : AppController.selectedWindowIndex
            enabled: AppController.windows.length > 0
            onActivated: AppController.selectWindow(currentIndex)
            panelColor: Theme.panel
            borderColor: Theme.border
            textColor: Theme.textPrimary
            mutedColor: Theme.muted
            accentColor: Theme.accent
        }
        footer: RowLayout {
            spacing: 10
            GlassButton {
                text: Theme.t("refreshWindows")
                variant: "neutral"
                implicitWidth: 138
                onClicked: AppController.refreshWindows()
            }
            Item { Layout.fillWidth: true }
            GlassButton {
                text: Theme.t("cancel")
                variant: "neutral"
                onClicked: x11WindowDialog.close()
            }
            GlassButton {
                text: Theme.t("start")
                variant: "primary"
                enabled: AppController.windows.length > 0
                onClicked: {
                    x11WindowDialog.close()
                    AppController.startVision()
                }
            }
        }
    }

    SetupPage { shell: shell; anchors.fill: parent }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10
        visible: AppController.setupComplete
        layer.enabled: shell.modalOpen
        layer.effect: MultiEffect {
            blurEnabled: true
            blur: 0.82
            blurMax: 32
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Item { Layout.fillWidth: true }
            SmallCaption {
                text: AppController.platform + " / " + AppController.captureBackend
            }
            GlassButton {
                text: Theme.t("home")
                iconSource: Theme.icon("home.png")
                variant: page === "home" ? "primary" : "neutral"
                onClicked: page = "home"
            }
            GlassButton {
                text: Theme.t("settings")
                iconSource: Theme.icon("settings.png")
                variant: page === "settings" ? "primary" : "neutral"
                onClicked: page = "settings"
            }
            GlassButton {
                text: Theme.t("debug")
                iconSource: Theme.icon("debug.png")
                variant: page === "debug" ? "warning" : "neutral"
                onClicked: page = "debug"
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            HomePage { shell: shell; anchors.fill: parent }
            SettingsPage { shell: shell; anchors.fill: parent }
            DebugPage { shell: shell; anchors.fill: parent }
        }

        SmallCaption {
            text: "v" + AppController.appVersion
            horizontalAlignment: Text.AlignRight
            Layout.fillWidth: true
        }
    }
}