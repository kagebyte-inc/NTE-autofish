import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

RowLayout {
    id: root
    property string label: ""
    property int from: 0
    property int to: 100
    property int step: 1
    property int currentValue: 0
    property string suffix: ""
    signal valueEdited(int value)

    Layout.fillWidth: true
    spacing: 10

    Label {
        text: root.label
        color: Theme.textPrimary
        font.pixelSize: 13
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }

    SpinBox {
        id: spin
        from: root.from
        to: root.to
        stepSize: root.step
        value: root.currentValue
        editable: true
        Layout.preferredWidth: Math.max(96, contentItem.implicitWidth + leftPadding + rightPadding + 36)
        onValueModified: root.valueEdited(value)

        contentItem: TextInput {
            text: spin.valueFromText(spin.textFromValue(spin.value, spin.locale), spin.locale)
            font.pixelSize: 13
            font.bold: true
            color: Theme.textPrimary
            selectionColor: Theme.accent
            selectedTextColor: Theme.bg
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            readOnly: !spin.editable
            validator: spin.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly
        }

        up.indicator: Rectangle {
            implicitWidth: 18
            implicitHeight: parent.height / 2 - 1
            color: spin.up.pressed ? Theme.panel : Theme.cardAlt
            border.color: Theme.border
            Text {
                anchors.centerIn: parent
                text: "▲"
                font.pixelSize: 7
                color: Theme.muted
            }
        }

        down.indicator: Rectangle {
            implicitWidth: 18
            implicitHeight: parent.height / 2 - 1
            y: parent.height / 2 + 1
            color: spin.down.pressed ? Theme.panel : Theme.cardAlt
            border.color: Theme.border
            Text {
                anchors.centerIn: parent
                text: "▼"
                font.pixelSize: 7
                color: Theme.muted
            }
        }

        background: Rectangle {
            color: spin.activeFocus ? Theme.panel : Theme.logBg
            border.color: spin.activeFocus ? Theme.accent : Theme.border
            radius: 4
        }
    }

    Label {
        text: root.suffix
        visible: root.suffix.length > 0
        color: Theme.muted
        font.pixelSize: 12
        Layout.preferredWidth: root.suffix.length > 0 ? 28 : 0
    }
}