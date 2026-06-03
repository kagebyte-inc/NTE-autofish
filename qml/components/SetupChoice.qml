import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

Rectangle {
    id: root
    property string title: ""
    property string caption: ""
    property url iconSource: ""
    property bool selected: false
    property bool enabled: true
    signal picked()

    radius: Theme.radius
    opacity: enabled ? 1.0 : 0.45
    color: selected && enabled ? "#2231454f" : Theme.cardAlt
    border.color: selected && enabled ? Theme.accent : Theme.border
    border.width: selected ? 2 : 1
    implicitHeight: 104

    RowLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 14

        Image {
            source: root.iconSource
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignVCenter
            fillMode: Image.PreserveAspectFit
            opacity: root.selected ? 1.0 : 0.78
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 6

            Label {
                text: root.title
                color: root.enabled ? Theme.textPrimary : Theme.muted
                font.pixelSize: 15
                font.bold: true
                Layout.fillWidth: true
            }

            SmallCaption {
                text: root.caption
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: if (root.enabled) root.picked()
    }
}