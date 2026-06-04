import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

GlassCard {
    id: root
    required property var shell
    anchors.fill: parent
    visible: shell.page === "debug"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            SectionTitle {
                text: Theme.t("debugStream")
                Layout.fillWidth: true
            }
            SmallCaption {
                text: AppController.writeLogsToFile ? "fishing.log, raw-events.log" : ""
                color: Theme.muted
            }
        }

        Label {
            visible: !AppController.debugMode
            text: Theme.t("debugDisabled")
            color: Theme.muted
            font.pixelSize: 14
        }

        TextArea {
            visible: AppController.debugMode
            text: AppController.reelControl
            readOnly: true
            wrapMode: Text.Wrap
            color: AppController.reelControl.indexOf("holding") >= 0 ? Theme.success : Theme.textPrimary
            selectedTextColor: Theme.bg
            selectionColor: Theme.accent
            background: Rectangle {
                color: Theme.logBg
                border.color: Theme.border
                radius: 6
            }
            Layout.fillWidth: true
            Layout.preferredHeight: 70
        }

        TextArea {
            visible: AppController.debugMode
            text: AppController.rawEvents.length > 0
                  ? AppController.rawEvents
                  : Theme.t("placeholder.raw")
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
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        SectionTitle {
            visible: AppController.debugMode && AppController.debugOverlaySource.length > 0
            text: "Live OpenCV zones / debug overlay (for tuning detect)"
            Layout.fillWidth: true
        }

        Image {
            visible: AppController.debugMode && AppController.debugOverlaySource.length > 0
            source: AppController.debugOverlaySource
            Layout.preferredWidth: 400
            Layout.preferredHeight: 225
            fillMode: Image.PreserveAspectFit
            cache: false
            smooth: true
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.color: Theme.accent
                border.width: 1
            }
        }
    }
}
