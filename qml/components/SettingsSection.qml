import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Autofish

GlassCard {
    id: root
    property string title: ""
    property string caption: ""
    property bool collapsible: false
    property bool expanded: true
    default property alias content: bodyColumn.data

    Layout.fillWidth: true
    implicitHeight: headerLayout.implicitHeight + (root.expanded ? bodyColumn.implicitHeight + 28 : 8)

    ColumnLayout {
        id: headerLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            SectionTitle {
                text: root.title
                Layout.fillWidth: true
            }

            GlassButton {
                visible: root.collapsible
                text: root.expanded ? "−" : "+"
                variant: "neutral"
                implicitWidth: 34
                implicitHeight: 28
                onClicked: root.expanded = !root.expanded
            }
        }

        SmallCaption {
            visible: root.caption.length > 0
            text: root.caption
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    ColumnLayout {
        id: bodyColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headerLayout.bottom
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.bottomMargin: 14
        spacing: 10
        visible: root.expanded
    }
}