import QtQuick
import QtQuick.Controls

// One key of the keyboard picture.
Item {
    id: cap
    property var keyData: ({})
    property real unit: 48
    property bool selected: false
    signal clicked()
    signal doubleClicked()

    readonly property string kind: keyData.kind || "unbound"
    readonly property bool dead: kind === "deadKey"
    readonly property bool modifier: !!keyData.modifier
    readonly property bool hasFallback: kind === "unbound" && !!keyData.fallback
    readonly property string shown: modifier && kind === "unbound" ? shortName(keyData.id) : (keyData.label || "")

    width: unit * (keyData.width || 1)
    height: unit
    opacity: keyData.present || modifier ? 1 : 0.55

    Accessible.role: Accessible.Button
    Accessible.name: keyData.id + (shown ? ": " + shown : "")

    function shortName(id) {
        const names = {
            ShiftLeft: qsTr("Shift"), ShiftRight: qsTr("Shift"), ControlLeft: qsTr("Ctrl"),
            ControlRight: qsTr("Ctrl"), AltLeft: qsTr("Alt"), AltRight: qsTr("AltGr"), MetaLeft: qsTr("Meta"),
            MetaRight: qsTr("Meta"), CapsLock: qsTr("Caps"), ContextMenu: qsTr("Menu")
        }
        return names[id] || id
    }

    // Key body: a top face over a darker edge, the keycap's only depth cue.
    Rectangle {
        id: edge
        anchors.fill: parent
        radius: Math.max(4, cap.unit * 0.12)
        color: cap.selected ? Ui.accent : Ui.capEdge
    }
    Rectangle {
        id: face
        anchors.fill: parent
        anchors.bottomMargin: cap.selected ? 2 : Math.max(2, cap.unit * 0.06)
        anchors.margins: cap.selected ? 2 : 0
        radius: edge.radius - (cap.selected ? 1 : 0)
        color: cap.dead ? Ui.deadKeyFill
             : cap.modifier ? Ui.capModifier
             : !cap.keyData.present ? Ui.capAbsent : Ui.cap
        border.width: cap.dead ? 1 : 0
        border.color: Ui.deadKey

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: cap.unit * 0.04
            width: parent.width - 6
            horizontalAlignment: Text.AlignHCenter
            text: cap.hasFallback ? cap.keyData.fallback : cap.shown
            color: cap.dead ? Ui.deadKey : cap.hasFallback ? Ui.faint : cap.modifier ? Ui.subtle : Ui.text
            font.pixelSize: cap.modifier || text.length > 2 ? cap.unit * 0.24 : cap.unit * 0.40
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 7
            font.italic: cap.hasFallback
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
        Text {
            // Position name, small, top left: what the key is, whatever it types.
            x: 4
            y: 2
            visible: cap.unit >= 30 && !cap.modifier
            width: parent.width - 8
            elide: Text.ElideRight
            text: cap.keyData.id ? cap.keyData.id.replace(/^Key|^Digit/, "") : ""
            color: Ui.subtle
            font.pixelSize: Math.max(8, Math.min(11, cap.unit * 0.17))
            textFormat: Text.PlainText
        }
        Text {
            // Marks: a dead key, an explicit "nothing", a reference to a key.
            anchors.right: parent.right
            anchors.rightMargin: 4
            y: 2
            visible: text !== "" && cap.unit >= 36
            text: cap.kind === "nothing" ? qsTr("none") : cap.kind === "key" ? qsTr("key")
                : cap.kind === "codeUnits" ? qsTr("units") : cap.kind === "action" ? "" : ""
            color: cap.dead ? Ui.deadKey : Ui.faint
            font.pixelSize: Math.max(8, cap.unit * 0.16)
            textFormat: Text.PlainText
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: cap.clicked()
        onDoubleClicked: cap.doubleClicked()
        ToolTip.visible: containsMouse && cap.unit < 60
        ToolTip.delay: 600
        ToolTip.text: cap.keyData.id + (cap.shown ? " → " + cap.shown : "")
    }
}
