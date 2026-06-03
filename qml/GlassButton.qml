import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string variant: "neutral"
    property color primaryColor: "#0ea5e9"
    property color primaryHoverColor: "#38bdf8"
    property color primaryDownColor: "#0284c7"
    property color neutralColor: "#334155"
    property color neutralHoverColor: "#475569"
    property color neutralDownColor: "#1e293b"
    property color dangerColor: "#ef4444"
    property color dangerHoverColor: "#f87171"
    property color dangerDownColor: "#dc2626"
    property color warningColor: "#f59e0b"
    property color warningHoverColor: "#fbbf24"
    property color warningDownColor: "#d97706"
    property color textColor: "#f8fafc"
    property url iconSource: ""
    property int iconSize: 22
    property string tooltipText: text

    readonly property color baseColor: variant === "primary" ? primaryColor :
                                       variant === "danger" ? dangerColor :
                                       variant === "warning" ? warningColor : neutralColor
    readonly property color hoverColor: variant === "primary" ? primaryHoverColor :
                                        variant === "danger" ? dangerHoverColor :
                                        variant === "warning" ? warningHoverColor : neutralHoverColor
    readonly property color downColor: variant === "primary" ? primaryDownColor :
                                       variant === "danger" ? dangerDownColor :
                                       variant === "warning" ? warningDownColor : neutralDownColor

    implicitWidth: iconSource == "" ? 116 : 42
    implicitHeight: 34
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: true

    contentItem: Item {
        implicitWidth: control.iconSource == "" ? label.implicitWidth : control.iconSize
        implicitHeight: Math.max(label.implicitHeight, icon.implicitHeight)

        Text {
            id: label
            anchors.fill: parent
            visible: control.iconSource == ""
            text: control.text
            color: control.enabled ? control.textColor : "#94a3b8"
            font.pixelSize: 14
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Image {
            id: icon
            anchors.centerIn: parent
            visible: control.iconSource != ""
            source: control.iconSource
            width: control.iconSize
            height: control.iconSize
            fillMode: Image.PreserveAspectFit
            opacity: control.enabled ? 1.0 : 0.45
        }
    }

    background: Rectangle {
        radius: 6
        color: !control.enabled ? "#1f2937" :
               control.down ? control.downColor :
               control.hovered ? control.hoverColor : control.baseColor
        border.width: 1
        border.color: control.hovered && control.enabled ? "#55ffffff" : "#22ffffff"
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    ToolTip.visible: hovered && tooltipText.length > 0
    ToolTip.delay: 450
    ToolTip.text: tooltipText
}
