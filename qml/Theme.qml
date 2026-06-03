pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#0f111a"
    readonly property color card: "#d2161822"
    readonly property color cardAlt: "#cc1a1d2a"
    readonly property color border: "#26ffffff"
    readonly property color textPrimary: "#f8fafc"
    readonly property color muted: "#94a3b8"
    readonly property color veryMuted: "#64748b"
    readonly property color accent: "#0ea5e9"
    readonly property color accentHover: "#38bdf8"
    readonly property color success: "#10b981"
    readonly property color warning: "#f59e0b"
    readonly property color danger: "#ef4444"
    readonly property color panel: "#242836"
    readonly property color logBg: "#141824"
    readonly property color setupOverlay: "#090b10"
    readonly property color modalHeader: "#202838"
    readonly property int radius: 8

    function icon(fileName: string): url {
        return Qt.resolvedUrl("assets/icons/" + fileName)
    }

    function t(key: string): string {
        void (I18n.generation)
        return I18n.textFor(key)
    }

    function format(key: string, arg: string): string {
        void (I18n.generation)
        return I18n.format(key, arg)
    }

    function status(value: string): string {
        void (I18n.generation)
        return I18n.statusText(value)
    }
}