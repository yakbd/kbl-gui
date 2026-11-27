pragma Singleton
import QtQuick

// Colours of the parts the editor draws itself (the keyboard), derived from the active style's palette so they
// follow the platform, light and dark.
QtObject {
    property var p: null // the window's palette, set by Main.qml

    readonly property bool dark: p ? p.window.hslLightness < 0.5 : false
    readonly property color text: p ? p.text : "#1c1c1e"
    readonly property color subtle: p ? Qt.rgba(text.r, text.g, text.b, 0.62) : "#6c6c70"
    readonly property color faint: p ? Qt.rgba(text.r, text.g, text.b, 0.38) : "#a0a0a6"
    readonly property color accent: p ? p.highlight : "#2f6fd0"
    readonly property color accentText: p ? p.highlightedText : "white"
    // Links take the accent's hue at a lightness that reads on the panel (platform link colours vary a lot).
    readonly property color link: Qt.hsla(accent.hslHue < 0 ? 0.6 : accent.hslHue, 0.7, dark ? 0.72 : 0.38, 1)
    readonly property color surface: p ? p.base : "white"
    readonly property color panel: p ? p.window : "#efeff1"

    // Keyboard
    readonly property color deck: dark ? Qt.darker(panel, 1.25) : Qt.darker(panel, 1.06)
    readonly property color cap: dark ? Qt.lighter(surface, 1.55) : surface
    readonly property color capEdge: dark ? Qt.darker(surface, 1.4) : Qt.darker(panel, 1.28)
    readonly property color capModifier: dark ? Qt.lighter(surface, 1.25) : Qt.darker(panel, 1.02)
    readonly property color capAbsent: Qt.rgba(cap.r, cap.g, cap.b, 0.35)
    readonly property color deadKey: dark ? "#e0a84a" : "#9a5b00"
    readonly property color deadKeyFill: dark ? Qt.rgba(0.88, 0.66, 0.29, 0.16) : Qt.rgba(0.95, 0.72, 0.30, 0.16)

    // Severities: colour is never the only signal (each mark has a shape and a label).
    readonly property color error: dark ? "#ff6b61" : "#c4271b"
    readonly property color warning: dark ? "#f0b43c" : "#9a6400"
    readonly property color info: dark ? "#7fb2f0" : "#2a62b8"
    readonly property color ok: dark ? "#5cc98a" : "#1f7a45"

    readonly property int gap: 8
    readonly property int pad: 12
}
