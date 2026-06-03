import QtQuick
import QtQuick.Controls
import Autofish

Rectangle {
    id: root
    property string tooltipText: ""
    signal clicked()

    implicitWidth: 28
    implicitHeight: 28
    radius: 14
    color: Theme.modalHeader
    border.color: Theme.border
    border.width: 1

    Image {
        anchors.centerIn: parent
        source: Theme.icon("info.png")
        width: 18
        height: 18
        fillMode: Image.PreserveAspectFit
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip.visible: mouse.containsMouse && root.tooltipText.length > 0
    ToolTip.delay: 450
    ToolTip.text: root.tooltipText
}