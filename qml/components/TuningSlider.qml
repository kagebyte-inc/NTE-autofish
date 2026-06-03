import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

ColumnLayout {
    id: root
    property string label: ""
    property int from: 0
    property int to: 100
    property int step: 1
    property int currentValue: 0
    property string suffix: ""
    signal valueEdited(int value)

    Layout.fillWidth: true
    spacing: 4

    Label {
        text: root.label
        color: Theme.textPrimary
        font.pixelSize: 13
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        Slider {
            from: root.from
            to: root.to
            stepSize: root.step
            snapMode: Slider.SnapAlways
            live: true
            value: root.currentValue
            Layout.fillWidth: true
            onMoved: root.valueEdited(Math.round(value))
        }

        Label {
            text: root.currentValue + root.suffix
            color: Theme.muted
            font.pixelSize: 12
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: 52
        }
    }
}