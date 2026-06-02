import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    property color panelColor: "#e631353b"
    property color panelHoverColor: "#f040444a"
    property color borderColor: "#99c7cbd2"
    property color textColor: "#f2f3f5"
    property color mutedColor: "#c1c6ce"
    property color accentColor: "#4ed4df"

    implicitHeight: 34
    leftPadding: 12
    rightPadding: 34
    hoverEnabled: true

    contentItem: Text {
        text: control.displayText
        color: control.textColor
        font.pixelSize: 13
        font.bold: true
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Canvas {
        x: control.width - width - 12
        y: (control.height - height) / 2
        width: 10
        height: 6

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.moveTo(0, 0)
            ctx.lineTo(width, 0)
            ctx.lineTo(width / 2, height)
            ctx.closePath()
            ctx.fillStyle = control.popup.visible ? control.accentColor : control.mutedColor
            ctx.fill()
        }
    }

    background: Rectangle {
        color: control.hovered || control.popup.visible ? control.panelHoverColor : control.panelColor
        border.color: control.popup.visible ? control.accentColor : control.borderColor
        radius: 3
    }

    delegate: ItemDelegate {
        width: control.width
        height: 30
        text: modelData
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: parent.text
            color: parent.highlighted ? "#ffffff" : control.mutedColor
            font.pixelSize: 13
            font.bold: parent.highlighted
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: parent.highlighted ? "#5b5b63" : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: contentItem.implicitHeight + 8
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }

        background: Rectangle {
            color: "#ef23272d"
            border.color: control.borderColor
            radius: 3
        }
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
