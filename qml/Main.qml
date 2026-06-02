import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform
import Autofish

ApplicationWindow {
    id: window
    width: 980
    height: 660
    visible: true
    title: i18n.format("aboutApp.body", AppController.appVersion)

    property color bg: "#0f111a"
    property color card: "#d2161822"
    property color cardAlt: "#cc1a1d2a"
    property color border: "#26ffffff"
    property color textPrimary: "#f8fafc"
    property color muted: "#94a3b8"
    property color veryMuted: "#64748b"
    property color accent: "#0ea5e9"
    property color accentHover: "#38bdf8"
    property color success: "#10b981"
    property color warning: "#f59e0b"
    property color danger: "#ef4444"
    property string page: "home"
    property int setupPlatformIndex: 0
    property int setupBackendIndex: 0
    property int resetSetupConfirmStep: 0
    property var languageCodes: ["en", "zh_CN", "ja", "ru"]

    function openHelp(title, body) {
        helpDialog.title = title
        helpText.text = body
        helpDialog.open()
    }

    function centerPopup(popup) {
        popup.x = Math.round((window.width - popup.width) / 2)
        popup.y = Math.round((window.height - popup.height) / 2)
    }

    function languageIndex() {
        var index = languageCodes.indexOf(AppController.uiLanguage)
        return index < 0 ? 0 : index
    }

    function languageNames() {
        return [
            "English",
            "简体中文",
            "日本語",
            "Русский"
        ]
    }

    function reelModeNames() {
        return [
            i18n.textFor("reel.boundary"),
            i18n.textFor("reel.chizukuo"),
            i18n.textFor("reel.stable"),
            i18n.textFor("reel.experimental")
        ]
    }

    color: bg
    palette.window: bg
    palette.base: "#1e202c"
    palette.text: textPrimary
    palette.windowText: textPrimary
    palette.buttonText: textPrimary
    palette.highlight: accent
    palette.highlightedText: "#ffffff"

    Rectangle {
        anchors.fill: parent
        color: bg
    }

    I18n {
        id: i18n
        language: AppController.uiLanguage
    }

    Platform.MenuBar {
        id: nativeMenuBar
        window: window

        Platform.Menu {
            title: i18n.textFor("menu.file")

            Platform.MenuItem {
                text: i18n.textFor("menu.exit")
                onTriggered: Qt.quit()
            }
        }

        Platform.Menu {
            title: i18n.textFor("menu.about")

            Platform.MenuItem {
                text: i18n.format("menu.aboutApp", AppController.appVersion)
                onTriggered: aboutAppDialog.open()
            }

            Platform.MenuItem {
                text: i18n.textFor("menu.aboutQt")
                onTriggered: AppController.showAboutQt()
            }
        }
    }

    Dialog {
        id: chizukuoPidDialog
        title: i18n.textFor("chizukuo.title")
        modal: true
        width: Math.min(parent.width - 48, 520)
        palette: window.palette
        onAboutToShow: centerPopup(chizukuoPidDialog)

        header: DialogTitleBar {
            dialog: chizukuoPidDialog
            title: chizukuoPidDialog.title
        }

        background: Rectangle {
            color: cardAlt
            border.color: border
            radius: 8
        }

        contentItem: Label {
            text: i18n.textFor("chizukuo.body")
            color: textPrimary
            wrapMode: Text.Wrap
            font.pixelSize: 14
        }

        footer: DialogButtonBox {
            background: Rectangle {
                color: "transparent"
            }

            GlassButton {
                text: i18n.textFor("ok")
                variant: "primary"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: experimentalDialog
        title: i18n.textFor("experimental.warning.title")
        modal: true
        width: Math.min(parent.width - 48, 520)
        palette: window.palette
        onAboutToShow: centerPopup(experimentalDialog)

        header: DialogTitleBar {
            dialog: experimentalDialog
            title: experimentalDialog.title
        }

        background: Rectangle {
            color: cardAlt
            border.color: warning
            radius: 8
        }

        contentItem: Label {
            text: i18n.textFor("experimental.warning.body")
            color: textPrimary
            wrapMode: Text.Wrap
            font.pixelSize: 14
        }

        footer: DialogButtonBox {
            background: Rectangle { color: "transparent" }

            GlassButton {
                text: i18n.textFor("ok")
                variant: "primary"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: aboutAppDialog
        title: i18n.format("menu.aboutApp", AppController.appVersion)
        modal: true
        width: Math.min(parent.width - 48, 420)
        palette: window.palette
        onAboutToShow: centerPopup(aboutAppDialog)

        header: DialogTitleBar {
            dialog: aboutAppDialog
            title: aboutAppDialog.title
        }

        background: Rectangle {
            color: cardAlt
            border.color: border
            radius: 8
        }

        contentItem: Label {
            text: i18n.format("aboutApp.body", AppController.appVersion)
            color: textPrimary
            wrapMode: Text.Wrap
            font.pixelSize: 14
        }

        footer: DialogButtonBox {
            background: Rectangle { color: "transparent" }

            GlassButton {
                text: i18n.textFor("ok")
                variant: "primary"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: helpDialog
        modal: true
        width: Math.min(parent.width - 48, 560)
        palette: window.palette
        onAboutToShow: centerPopup(helpDialog)

        header: DialogTitleBar {
            dialog: helpDialog
            title: helpDialog.title
        }

        background: Rectangle {
            color: cardAlt
            border.color: border
            radius: 8
        }

        contentItem: Label {
            id: helpText
            color: textPrimary
            wrapMode: Text.Wrap
            font.pixelSize: 14
        }

        footer: DialogButtonBox {
            background: Rectangle {
                color: "transparent"
            }

            GlassButton {
                text: i18n.textFor("ok")
                variant: "primary"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: resetSetupDialog
        title: resetSetupConfirmStep === 1 ? i18n.textFor("resetSetup.title1") : i18n.textFor("resetSetup.title2")
        modal: true
        width: Math.min(parent.width - 48, 520)
        palette: window.palette
        onAboutToShow: centerPopup(resetSetupDialog)

        header: DialogTitleBar {
            dialog: resetSetupDialog
            title: resetSetupDialog.title
        }

        background: Rectangle {
            color: cardAlt
            border.color: resetSetupConfirmStep === 1 ? warning : danger
            radius: 8
        }

        contentItem: Label {
            text: resetSetupConfirmStep === 1
                  ? i18n.textFor("resetSetup.body1")
                  : i18n.textFor("resetSetup.body2")
            color: textPrimary
            wrapMode: Text.Wrap
            font.pixelSize: 14
        }

        footer: RowLayout {
            spacing: 10

            Item { Layout.fillWidth: true }

            GlassButton {
                text: i18n.textFor("cancel")
                variant: "neutral"
                onClicked: resetSetupDialog.close()
            }

            GlassButton {
                text: resetSetupConfirmStep === 1 ? i18n.textFor("continue") : i18n.textFor("reset")
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

    component GlassCard: Rectangle {
        color: card
        border.color: border
        radius: 8
    }

    component SmallCaption: Label {
        color: muted
        font.pixelSize: 12
    }

    component SectionTitle: Label {
        color: textPrimary
        font.pixelSize: 16
        font.bold: true
    }

    component InfoButton: Rectangle {
        id: infoButton
        signal clicked()

        implicitWidth: 28
        implicitHeight: 28
        radius: 14
        color: "#202838"
        border.color: border
        border.width: 1

        Label {
            anchors.centerIn: parent
            text: "i"
            color: accentHover
            font.pixelSize: 15
            font.bold: true
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: infoButton.clicked()
        }
    }

    component DialogTitleBar: Rectangle {
        id: titleBar
        property var dialog
        property string title: ""

        implicitHeight: 36
        color: "#202838"
        border.color: border
        border.width: 0

        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            text: titleBar.title
            color: textPrimary
            font.pixelSize: 14
            font.bold: true
            elide: Text.ElideRight
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeAllCursor

            property real pressWindowX: 0
            property real pressWindowY: 0
            property real pressDialogX: 0
            property real pressDialogY: 0

            onPressed: function(mouse) {
                var point = mapToItem(window.contentItem, mouse.x, mouse.y)
                pressWindowX = point.x
                pressWindowY = point.y
                pressDialogX = titleBar.dialog.x
                pressDialogY = titleBar.dialog.y
            }

            onPositionChanged: function(mouse) {
                if (!pressed || !titleBar.dialog) {
                    return
                }

                var point = mapToItem(window.contentItem, mouse.x, mouse.y)
                var nextX = pressDialogX + point.x - pressWindowX
                var nextY = pressDialogY + point.y - pressWindowY
                titleBar.dialog.x = Math.max(0, Math.min(window.width - titleBar.dialog.width, nextX))
                titleBar.dialog.y = Math.max(0, Math.min(window.height - titleBar.dialog.height, nextY))
            }
        }
    }

    component TuningSlider: RowLayout {
        id: tuningSlider
        property string label: ""
        property int from: 0
        property int to: 100
        property int step: 1
        property int currentValue: 0
        property string suffix: ""
        property int labelWidth: 118
        property int valueWidth: 50
        signal valueEdited(int value)

        Layout.fillWidth: true
        spacing: 10

        Label {
            text: tuningSlider.label
            color: textPrimary
            font.pixelSize: 13
            elide: Text.ElideRight
            Layout.preferredWidth: tuningSlider.labelWidth
        }

        Slider {
            from: tuningSlider.from
            to: tuningSlider.to
            stepSize: tuningSlider.step
            snapMode: Slider.SnapAlways
            live: true
            value: tuningSlider.currentValue
            Layout.fillWidth: true
            onMoved: tuningSlider.valueEdited(Math.round(value))
        }

        Label {
            text: tuningSlider.currentValue + tuningSlider.suffix
            color: muted
            font.pixelSize: 12
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: tuningSlider.valueWidth
        }
    }

    component SetupChoice: Rectangle {
        id: choice
        property string title: ""
        property string caption: ""
        property bool selected: false
        signal picked()

        radius: 8
        color: selected ? "#2231454f" : cardAlt
        border.color: selected ? accent : border
        border.width: selected ? 2 : 1
        implicitHeight: 86

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 6

            Label {
                text: choice.title
                color: textPrimary
                font.pixelSize: 15
                font.bold: true
                Layout.fillWidth: true
            }

            SmallCaption {
                text: choice.caption
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: choice.picked()
        }
    }

    component ModeCard: Rectangle {
        id: modeCard
        property string mode: "EZ"
        property string title: ""
        property string caption: ""
        property color stripeColor: accent
        property bool selected: AppController.experienceMode === mode

        radius: 8
        color: selected ? "#252c3b" : card
        border.color: selected ? stripeColor : border
        border.width: selected ? 2 : 1
        implicitHeight: 146

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 4
            radius: 2
            color: modeCard.stripeColor
            opacity: modeCard.selected ? 1.0 : 0.45
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: modeCard.title
                    color: textPrimary
                    font.pixelSize: 22
                    font.bold: true
                    Layout.fillWidth: true
                }
                Rectangle {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 26
                    radius: 13
                    color: modeCard.selected ? modeCard.stripeColor : "#263241"
                    Label {
                        anchors.centerIn: parent
                        text: modeCard.selected ? i18n.textFor("active") : modeCard.mode
                        color: "#ffffff"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            SmallCaption {
                text: modeCard.caption
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Item {
                Layout.fillHeight: true
            }

            GlassButton {
                text: modeCard.selected ? i18n.textFor("selected") : modeCard.mode
                variant: modeCard.mode === "EZ" ? "primary" : "neutral"
                enabled: !modeCard.selected
                onClicked: AppController.experienceMode = modeCard.mode
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: !AppController.setupComplete

        Rectangle {
            anchors.fill: parent
            color: "#090b10"
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 36
            spacing: 28

            ColumnLayout {
                Layout.preferredWidth: Math.min(390, parent.width * 0.42)
                Layout.fillHeight: true
                spacing: 18

                Label {
                    text: i18n.textFor("app.title")
                    color: textPrimary
                    font.pixelSize: 34
                    font.bold: true
                    Layout.fillWidth: true
                }

                Label {
                    text: i18n.textFor("firstLaunch")
                    color: accentHover
                    font.pixelSize: 18
                    font.bold: true
                }

                SmallCaption {
                    text: i18n.textFor("firstLaunch.caption")
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                Item { Layout.fillHeight: true }

                GlassButton {
                    text: i18n.textFor("continue")
                    variant: "primary"
                    implicitWidth: 150
                    onClicked: {
                        var platforms = ["Wayland", "X11", "Windows"]
                        var backends = ["PipeWire Portal", "X11 Window", "Win32 Capture"]
                        AppController.completeSetup(platforms[setupPlatformIndex], backends[setupBackendIndex])
                        page = "home"
                    }
                }
            }

            GlassCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 16

                    SectionTitle { text: i18n.textFor("platform") }

                    SetupChoice {
                        Layout.fillWidth: true
                        title: "Wayland"
                        caption: i18n.textFor("platform.wayland.caption")
                        selected: setupPlatformIndex === 0
                        onPicked: {
                            setupPlatformIndex = 0
                            setupBackendIndex = 0
                        }
                    }

                    SetupChoice {
                        Layout.fillWidth: true
                        title: "X11"
                        caption: i18n.textFor("platform.x11.caption")
                        selected: setupPlatformIndex === 1
                        onPicked: {
                            setupPlatformIndex = 1
                            setupBackendIndex = 1
                        }
                    }

                    SetupChoice {
                        Layout.fillWidth: true
                        title: "Windows"
                        caption: i18n.textFor("platform.windows.caption")
                        selected: setupPlatformIndex === 2
                        onPicked: {
                            setupPlatformIndex = 2
                            setupBackendIndex = 2
                        }
                    }

                    SectionTitle { text: i18n.textFor("captureBackend") }

                    SkinComboBox {
                        Layout.preferredWidth: 260
                        model: ["PipeWire Portal", "X11 Window", "Win32 Capture"]
                        currentIndex: setupBackendIndex
                        onActivated: setupBackendIndex = currentIndex
                        panelColor: "#242836"
                        borderColor: border
                        textColor: textPrimary
                        mutedColor: muted
                        accentColor: accent
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10
        visible: AppController.setupComplete

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item { Layout.fillWidth: true }

            SmallCaption {
                text: AppController.platform + " / " + AppController.captureBackend
            }

            GlassButton {
                text: i18n.textFor("home")
                variant: page === "home" ? "primary" : "neutral"
                implicitWidth: 82
                onClicked: page = "home"
            }

            GlassButton {
                text: i18n.textFor("settings")
                variant: page === "settings" ? "primary" : "neutral"
                implicitWidth: 96
                onClicked: page = "settings"
            }

            GlassButton {
                text: i18n.textFor("debug")
                variant: page === "debug" ? "warning" : "neutral"
                implicitWidth: 82
                onClicked: page = "debug"
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                anchors.fill: parent
                spacing: 14
                visible: page === "home"

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
                                SmallCaption { text: i18n.textFor("state") }
                                Label {
                                    text: i18n.statusText(AppController.status)
                                    color: AppController.status === "watching" ? success : textPrimary
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
                                SmallCaption { text: i18n.textFor("currentEvent") }
                                Label {
                                    text: i18n.fishingEventText(AppController.fishingEvent)
                                    color: AppController.fishingEvent.indexOf("fish_on_hook") >= 0
                                           || AppController.fishingEvent.indexOf("Hook!") >= 0 ? success : textPrimary
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

                            SectionTitle { text: i18n.textFor("fishingControls") }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                GlassButton {
                                    text: i18n.textFor("start")
                                    variant: "primary"
                                    enabled: AppController.status !== "watching"
                                    onClicked: {
                                        if (AppController.platform === "Wayland"
                                            || AppController.captureBackend === "PipeWire Portal") {
                                            AppController.startPortalVision()
                                        } else {
                                            AppController.startVision()
                                        }
                                    }
                                }

                                GlassButton {
                                    text: i18n.textFor("stop")
                                    variant: "danger"
                                    enabled: AppController.status === "watching"
                                    onClicked: AppController.stopVision()
                                }

                                GlassButton {
                                    text: i18n.textFor("simulateBite")
                                    variant: "neutral"
                                    implicitWidth: 128
                                    visible: AppController.experienceMode === "PRO"
                                    onClicked: AppController.simulateBite()
                                }

                                GlassButton {
                                    text: i18n.textFor("resultTest")
                                    variant: "neutral"
                                    implicitWidth: 118
                                    visible: AppController.experienceMode === "PRO"
                                    onClicked: AppController.simulateResultScreen()
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
                                    text: i18n.fishingLogText(AppController.fishingLog)
                                    readOnly: true
                                    wrapMode: TextEdit.NoWrap
                                    color: textPrimary
                                    selectedTextColor: bg
                                    selectionColor: accent
                                    background: Rectangle {
                                        color: "#141824"
                                        border.color: border
                                        radius: 6
                                    }
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                spacing: 14
                visible: page === "settings"

                GlassCard {
                    Layout.preferredWidth: 220
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        SectionTitle { text: i18n.textFor("runtime") }
                        SmallCaption { text: i18n.textFor("runtime.caption") ; wrapMode: Text.Wrap ; Layout.fillWidth: true }

                        SmallCaption {
                            text: i18n.textFor("language")
                            color: textPrimary
                        }

                        SkinComboBox {
                            Layout.fillWidth: true
                            model: languageNames()
                            currentIndex: languageIndex()
                            onActivated: AppController.uiLanguage = languageCodes[currentIndex]
                            panelColor: "#242836"
                            borderColor: border
                            textColor: textPrimary
                            mutedColor: muted
                            accentColor: accent
                        }

                        GlassButton {
                            text: i18n.textFor("resetSetup")
                            variant: "neutral"
                            implicitWidth: 128
                            onClicked: {
                                resetSetupConfirmStep = 1
                                resetSetupDialog.open()
                            }
                        }
                    }
                }

                GlassCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            SectionTitle {
                                text: i18n.textFor("fishingMode")
                                Layout.fillWidth: true
                            }

                            InfoButton {
                                onClicked: openHelp(
                                    i18n.textFor("fishingMode"),
                                    i18n.textFor("fishingMode.help")
                                )
                            }
                        }

                        RowLayout {
                            spacing: 10
                            GlassButton {
                                text: "EZ"
                                variant: AppController.experienceMode === "EZ" ? "primary" : "neutral"
                                implicitWidth: 82
                                onClicked: AppController.experienceMode = "EZ"
                            }
                            GlassButton {
                                text: "PRO"
                                variant: AppController.experienceMode === "PRO" ? "warning" : "neutral"
                                implicitWidth: 82
                                onClicked: AppController.experienceMode = "PRO"
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            SectionTitle {
                                text: i18n.textFor("reelController")
                                Layout.fillWidth: true
                            }

                            InfoButton {
                                onClicked: openHelp(
                                    i18n.textFor("reelController"),
                                    i18n.textFor("reelController.help")
                                )
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            SkinComboBox {
                                id: reelModeBox
                                model: reelModeNames()
                                currentIndex: AppController.reelControlMode
                                onActivated: {
                                    AppController.reelControlMode = currentIndex
                                    if (currentIndex === 1) {
                                        chizukuoPidDialog.open()
                                    } else if (currentIndex === 3) {
                                        experimentalDialog.open()
                                    }
                                }
                                Layout.preferredWidth: 250
                                panelColor: "#242836"
                                borderColor: border
                                textColor: textPrimary
                                mutedColor: muted
                                accentColor: accent
                            }
                        }

                        GlassCard {
                            visible: AppController.reelControlMode === 3
                            Layout.fillWidth: true
                            implicitHeight: experimentalTuningLayout.implicitHeight + 20
                            color: "#141824"
                            border.color: warning

                            ColumnLayout {
                                id: experimentalTuningLayout
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true

                                    SectionTitle {
                                        text: i18n.textFor("experimental.title")
                                        Layout.fillWidth: true
                                    }

                                    GlassButton {
                                        text: i18n.textFor("reset")
                                        variant: "neutral"
                                        implicitWidth: 88
                                        onClicked: AppController.resetExperimentalTuning()
                                    }
                                }

                                SmallCaption {
                                    text: i18n.textFor("experimental.caption")
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 14
                                    rowSpacing: 6

                                    TuningSlider {
                                        label: i18n.textFor("tuning.predictionLead")
                                        from: 0
                                        to: 140
                                        step: 5
                                        currentValue: AppController.experimentalLeadMs
                                        suffix: " ms"
                                        onValueEdited: function(value) { AppController.experimentalLeadMs = value }
                                    }

                                    TuningSlider {
                                        label: i18n.textFor("tuning.safeMargin")
                                        from: 5
                                        to: 40
                                        step: 1
                                        currentValue: AppController.experimentalSafeMarginPercent
                                        suffix: "%"
                                        onValueEdited: function(value) { AppController.experimentalSafeMarginPercent = value }
                                    }

                                    TuningSlider {
                                        label: i18n.textFor("tuning.insideZoneHold")
                                        from: 0
                                        to: 250
                                        step: 5
                                        currentValue: AppController.experimentalSettleMs
                                        suffix: " ms"
                                        onValueEdited: function(value) { AppController.experimentalSettleMs = value }
                                    }

                                    TuningSlider {
                                        label: i18n.textFor("tuning.brakeLookahead")
                                        from: 0
                                        to: 180
                                        step: 5
                                        currentValue: AppController.experimentalBrakeMs
                                        suffix: " ms"
                                        onValueEdited: function(value) { AppController.experimentalBrakeMs = value }
                                    }

                                    TuningSlider {
                                        label: i18n.textFor("tuning.maxChasePulse")
                                        from: 32
                                        to: 180
                                        step: 2
                                        currentValue: AppController.experimentalMaxPulseMs
                                        suffix: " ms"
                                        onValueEdited: function(value) { AppController.experimentalMaxPulseMs = value }
                                    }

                                    TuningSlider {
                                        label: i18n.textFor("tuning.minPulseGap")
                                        from: 0
                                        to: 30
                                        step: 1
                                        currentValue: AppController.experimentalMinGapMs
                                        suffix: " ms"
                                        onValueEdited: function(value) { AppController.experimentalMinGapMs = value }
                                    }
                                }
                            }
                        }

                        SectionTitle { text: i18n.textFor("debug") }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Switch {
                                checked: AppController.debugMode
                                onToggled: AppController.debugMode = checked
                                palette: window.palette
                            }

                            SmallCaption {
                                text: i18n.textFor("debugMessages")
                                color: textPrimary
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Switch {
                                checked: AppController.saveDebugFrames
                                onToggled: AppController.saveDebugFrames = checked
                                palette: window.palette
                            }

                            SmallCaption {
                                text: AppController.saveDebugFrames ? i18n.textFor("savingScreenshots") : i18n.textFor("notSavingScreenshots")
                                color: AppController.saveDebugFrames ? warning : muted
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Switch {
                                checked: AppController.writeLogsToFile
                                onToggled: AppController.writeLogsToFile = checked
                                palette: window.palette
                            }

                            SmallCaption {
                                text: i18n.textFor("writeLogs")
                                color: textPrimary
                                Layout.fillWidth: true
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }

            GlassCard {
                anchors.fill: parent
                visible: page === "debug"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        SectionTitle { text: i18n.textFor("debugStream") ; Layout.fillWidth: true }
                        SmallCaption {
                            text: AppController.writeLogsToFile ? "logs/fishing.log, logs/raw-events.log" : ""
                            color: muted
                        }
                    }

                    Label {
                        visible: !AppController.debugMode
                        text: i18n.textFor("debugDisabled")
                        color: muted
                        font.pixelSize: 14
                    }

                    TextArea {
                        visible: AppController.debugMode
                        text: AppController.reelControl
                        readOnly: true
                        wrapMode: Text.Wrap
                        color: AppController.reelControl.indexOf("holding") >= 0 ? success : textPrimary
                        selectedTextColor: bg
                        selectionColor: accent
                        background: Rectangle { color: "#141824"; border.color: border; radius: 6 }
                        Layout.fillWidth: true
                        Layout.preferredHeight: 70
                    }

                    TextArea {
                        visible: AppController.debugMode
                        text: i18n.rawEventsText(AppController.rawEvents)
                        readOnly: true
                        wrapMode: TextEdit.NoWrap
                        color: textPrimary
                        selectedTextColor: bg
                        selectionColor: accent
                        background: Rectangle { color: "#141824"; border.color: border; radius: 6 }
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }
            }
        }

        SmallCaption {
            text: "v" + AppController.appVersion
            horizontalAlignment: Text.AlignRight
            Layout.fillWidth: true
        }
    }
}
