import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

RowLayout {
    id: root
    property string label: ""
    property string value: ""
    property string caption: ""
    property color valueColor: Theme.textPrimary
    default property alias extra: extraSlot.data

    Layout.fillWidth: true
    spacing: 10

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        SmallCaption {
            visible: root.label.length > 0
            text: root.label
            color: Theme.muted
            Layout.fillWidth: true
        }

        Label {
            visible: root.value.length > 0
            text: root.value
            color: root.valueColor
            font.pixelSize: 13
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        SmallCaption {
            visible: root.caption.length > 0
            text: root.caption
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    Item {
        id: extraSlot
        Layout.alignment: Qt.AlignVCenter
    }
}