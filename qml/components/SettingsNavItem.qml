import QtQuick
import QtQuick.Controls
import Autofish

Rectangle {
    id: root
    property string text: ""
    property bool selected: false
    signal activated()

    height: 36
    radius: Theme.radius
    color: selected ? "#2231454f" : "transparent"
    border.color: selected ? Theme.accent : "transparent"
    border.width: selected ? 1 : 0

    Label {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        text: root.text
        color: root.selected ? Theme.textPrimary : Theme.muted
        font.pixelSize: 13
        font.bold: root.selected
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activated()
    }
}