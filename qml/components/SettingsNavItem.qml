import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

Rectangle {
    id: root
    property string text: ""
    property bool selected: false
    property url iconSource: ""
    signal activated()

    height: 36
    radius: Theme.radius
    color: selected ? "#2231454f" : "transparent"
    border.color: selected ? Theme.accent : "transparent"
    border.width: selected ? 1 : 0

    RowLayout {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Image {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            visible: root.iconSource != ""
            source: root.iconSource
            fillMode: Image.PreserveAspectFit
            opacity: root.selected ? 1.0 : 0.75

        }

        Label {
            Layout.fillWidth: true
            text: root.text
            color: root.selected ? Theme.textPrimary : Theme.muted
            font.pixelSize: 13
            font.bold: root.selected
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activated()
    }
}