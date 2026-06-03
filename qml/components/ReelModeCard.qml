import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

Rectangle {
    id: root
    property string title: ""
    property string caption: ""
    property bool selected: false
    property bool enabled: true
    signal picked()

    radius: Theme.radius
    opacity: enabled ? 1.0 : 0.45
    color: selected && enabled ? "#2231454f" : Theme.cardAlt
    border.color: selected && enabled ? Theme.accent : Theme.border
    border.width: selected ? 2 : 1
    implicitHeight: cardLayout.implicitHeight + 24
    Layout.fillWidth: true

    ColumnLayout {
        id: cardLayout
        anchors.fill: parent
        anchors.margins: 12
        spacing: 4

        Label {
            text: root.title
            color: root.enabled ? Theme.textPrimary : Theme.muted
            font.pixelSize: 14
            font.bold: true
            Layout.fillWidth: true
        }

        SmallCaption {
            visible: root.caption.length > 0
            text: root.caption
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: if (root.enabled) root.picked()
    }
}