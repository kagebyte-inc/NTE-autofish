import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string skin: "secondary"
    property color textColor: "#f8fafc"
    property color disabledTextColor: "#94a3b8"
    property int borderLeft: skin === "primary" ? 74 : 18
    property int borderRight: skin === "primary" ? 22 : 18
    property int borderTop: skin === "primary" ? 22 : 14
    property int borderBottom: skin === "primary" ? 22 : 14
    readonly property int skinState: !enabled ? 3 : down ? 2 : hovered ? 1 : 0

    implicitWidth: skin === "primary" ? 170 : 124
    implicitHeight: skin === "primary" ? 42 : 34
    leftPadding: skin === "primary" ? 42 : 18
    rightPadding: 18
    topPadding: 0
    bottomPadding: 0
    flat: true
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        color: control.enabled ? control.textColor : control.disabledTextColor
        font.pixelSize: 14
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: BorderImage {
        source: "assets/nte/btn_" + control.skin + "_" + control.skinState + ".png"
        border.left: control.borderLeft
        border.right: control.borderRight
        border.top: control.borderTop
        border.bottom: control.borderBottom
        horizontalTileMode: BorderImage.Stretch
        verticalTileMode: BorderImage.Stretch
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
