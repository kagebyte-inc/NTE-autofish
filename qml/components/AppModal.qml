import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

Item {
    id: modal
    property bool opened: false
    property string title: ""
    property real preferredWidth: 520
    property color frameBorderColor: Theme.border
    default property alias content: contentColumn.data
    property alias footer: footerColumn.data
    signal closed()

    anchors.fill: parent
    visible: opened
    z: 1000

    function centerPanel() {
        panel.x = Math.round((modal.width - panel.width) / 2)
        panel.y = Math.round((modal.height - panel.height) / 2)
    }

    function open() {
        opened = true
        Qt.callLater(centerPanel)
    }

    function close() {
        if (!opened) {
            return
        }
        opened = false
        closed()
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
    }

    Rectangle {
        id: panel
        width: Math.min(modal.width - 48, modal.preferredWidth)
        height: Math.min(panelLayout.implicitHeight, modal.height - 48)
        radius: Theme.radius
        color: Theme.cardAlt
        border.color: modal.frameBorderColor
        clip: true

        ColumnLayout {
            id: panelLayout
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                color: Theme.modalHeader

                Label {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    text: modal.title
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SizeAllCursor
                    drag.target: panel
                    drag.minimumX: 0
                    drag.minimumY: 0
                    drag.maximumX: Math.max(0, modal.width - panel.width)
                    drag.maximumY: Math.max(0, modal.height - panel.height)
                }
            }

            ColumnLayout {
                id: contentColumn
                Layout.fillWidth: true
                Layout.margins: 16
                spacing: 8
            }

            ColumnLayout {
                id: footerColumn
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16
                spacing: 8
            }
        }

        Connections {
            target: modal
            function onWidthChanged() {
                panel.x = Math.max(0, Math.min(modal.width - panel.width, panel.x))
            }
            function onHeightChanged() {
                panel.y = Math.max(0, Math.min(modal.height - panel.height, panel.y))
            }
        }
    }
}