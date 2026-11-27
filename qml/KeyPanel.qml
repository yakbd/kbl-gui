import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Properties of the selected key: what it does in the current layer, its position data, and every layer at a
// glance. Text fields commit on Enter or when they lose focus; each commit is one undo step.
Pane {
    id: panel
    property int layerIndex: 0
    property string position: ""
    signal selectLayer(int index)
    padding: Ui.pad

    readonly property var binding: (doc.revision, position ? doc.bindingAt(layerIndex, position) : ({}))
    readonly property var pos: (doc.revision, position ? doc.positionInfo(position) : ({}))
    readonly property var allLayers: (doc.revision, position ? doc.bindingsForPosition(position) : [])
    readonly property var layerInfo: (doc.revision, doc.layers()[layerIndex] || ({}))
    readonly property string kindChoice: {
        const k = binding.kind || "unbound"
        return k === "action" ? "text" : k
    }

    function focusBinding() {
        if (valueField.visible)
            valueField.forceActiveFocus()
        else
            kindBox.forceActiveFocus()
    }

    function codePoints(s) {
        const out = []
        for (const ch of s)
            out.push("U+" + ch.codePointAt(0).toString(16).toUpperCase().padStart(4, "0"))
        return out.join(" ")
    }
    function fromCodePoints(s) {
        let out = ""
        for (const part of s.trim().split(/[\s,]+/)) {
            if (part === "")
                continue
            const m = /^(?:U\+|0x)?([0-9A-Fa-f]{1,6})$/.exec(part)
            if (!m)
                return null
            const v = parseInt(m[1], 16)
            if (v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF))
                return null
            out += String.fromCodePoint(v)
        }
        return out
    }

    background: Rectangle {
        color: Ui.surface
        radius: 8
        border.color: Ui.capEdge
        border.width: 1
    }

    Label {
        anchors.centerIn: parent
        visible: panel.position === ""
        width: parent.width - 2 * Ui.pad
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: Ui.subtle
        text: qsTr("Select a key to see and change what it types.\nArrow keys move between keys; Enter edits.")
    }

    ScrollView {
        id: panelScroll
        anchors.fill: parent
        visible: panel.position !== ""
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: panelScroll.availableWidth
            spacing: Ui.gap

            Label {
                text: panel.position
                font.pixelSize: Qt.application.font.pixelSize * 1.45
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                elide: Text.ElideRight
            }
            Label {
                visible: !panel.pos.exists
                text: qsTr("Not declared in positions; it has no scan code.")
                color: Ui.warning
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }

            SectionHeader { text: qsTr("In layer %1").arg(panel.layerInfo.label || "") }

            Label { text: qsTr("Binding"); color: Ui.subtle }
            ComboBox {
                id: kindBox
                objectName: "bindingKind"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                textRole: "text"
                valueRole: "value"
                model: [
                    { value: "text", text: qsTr("Types text") },
                    { value: "deadKey", text: qsTr("Dead key") },
                    { value: "key", text: qsTr("Key from the keys list") },
                    { value: "nothing", text: qsTr("Explicitly nothing") },
                    { value: "unbound", text: qsTr("No binding (falls back)") }
                ]
                currentIndex: Math.max(0, indexOfValue(panel.kindChoice))
                enabled: ["text", "deadKey", "key", "nothing", "unbound", "action"].indexOf(panel.binding.kind || "unbound") >= 0
                onActivated: {
                    const v = currentValue
                    if (v === "unbound" || v === "nothing")
                        doc.setBinding(panel.layerIndex, panel.position, v, "")
                    else if (v === "deadKey" && doc.deadKeyIds().length > 0)
                        doc.setBinding(panel.layerIndex, panel.position, v, doc.deadKeyIds()[0])
                    else if (v === "key" && doc.keyIds().length > 0)
                        doc.setBinding(panel.layerIndex, panel.position, v, doc.keyIds()[0])
                    else if (v === "text")
                        valueField.forceActiveFocus()
                }
                Accessible.name: qsTr("Binding kind")
            }

            Label {
                visible: !kindBox.enabled
                text: qsTr("This binding (%1) is edited in the JSON page.").arg(panel.binding.kind || "")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                color: Ui.subtle
            }
            Button {
                visible: !kindBox.enabled
                text: qsTr("Show in JSON")
                onClicked: doc.navigate("/json-view" + panel.binding.pointer)
            }

            // Text
            Label {
                visible: valueField.visible
                text: qsTr("Text")
                color: Ui.subtle
            }
            TextField {
                id: valueField
                objectName: "bindingText"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: kindBox.enabled && (kindBox.currentValue === "text" || kindBox.currentValue === "unbound")
                text: panel.binding.kind === "text" || panel.binding.kind === "action" ? panel.binding.value : ""
                placeholderText: panel.binding.fallback ? qsTr("Caps Lock fallback: %1").arg(panel.binding.fallback)
                                                        : qsTr("Type the character(s)")
                font.pixelSize: Qt.application.font.pixelSize * 1.3
                selectByMouse: true
                onEditingFinished: {
                    if (text !== (panel.binding.kind === "text" || panel.binding.kind === "action" ? panel.binding.value : ""))
                        doc.setBinding(panel.layerIndex, panel.position, "text", text)
                }
                Accessible.name: qsTr("Text the key types")
            }
            Label {
                visible: valueField.visible
                text: qsTr("Code points")
                color: Ui.subtle
            }
            TextField {
                id: cpField
                objectName: "bindingCodePoints"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: valueField.visible
                text: panel.codePoints(valueField.text)
                placeholderText: qsTr("U+00E6 U+0301")
                font.family: "monospace"
                selectByMouse: true
                property bool bad: false
                color: bad ? Ui.error : palette.text
                onTextEdited: bad = panel.fromCodePoints(text) === null
                onEditingFinished: {
                    const t = panel.fromCodePoints(text)
                    if (t === null) {
                        bad = true
                        return
                    }
                    bad = false
                    if (t !== valueField.text)
                        doc.setBinding(panel.layerIndex, panel.position, "text", t)
                }
                Accessible.name: qsTr("Code points of the text")
                ToolTip.visible: bad && activeFocus
                ToolTip.text: qsTr("Write code points as U+XXXX, separated by spaces")
            }

            // Dead key or key reference
            Label {
                visible: refBox.visible
                text: kindBox.currentValue === "deadKey" ? qsTr("Dead key") : qsTr("Key")
                color: Ui.subtle
            }
            ComboBox {
                id: refBox
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: kindBox.enabled && (kindBox.currentValue === "deadKey" || kindBox.currentValue === "key")
                editable: kindBox.currentValue === "key"
                model: kindBox.currentValue === "deadKey" ? (doc.revision, doc.deadKeyIds()) : (doc.revision, doc.keyIds())
                currentIndex: model.indexOf(panel.binding.value)
                onActivated: doc.setBinding(panel.layerIndex, panel.position, kindBox.currentValue, currentText)
                onAccepted: doc.setBinding(panel.layerIndex, panel.position, kindBox.currentValue, editText)
                Accessible.name: qsTr("Referenced id")
            }
            Label {
                visible: refBox.visible && refBox.count === 0
                text: kindBox.currentValue === "deadKey" ? qsTr("The layout has no dead keys yet; add one on the Dead Keys page.")
                                                         : qsTr("The layout has no keys list; type an id (a single letter or digit is an implied key).")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                color: Ui.subtle
            }
            Button {
                visible: kindBox.currentValue === "deadKey" && panel.binding.kind === "deadKey"
                text: qsTr("Open Dead Key “%1”").arg(panel.binding.value)
                onClicked: doc.navigate("/deadKeys/" + panel.binding.value)
            }

            SectionHeader { text: qsTr("Position") }
            GridLayout {
                columns: 2
                columnSpacing: Ui.gap
                rowSpacing: Ui.gap
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Label { text: qsTr("Scan code"); color: Ui.subtle }
                TextField {
                    objectName: "scanCode"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: panel.pos.scanCode || ""
                    placeholderText: qsTr("e.g. 1E or E01D")
                    font.family: "monospace"
                    validator: RegularExpressionValidator { regularExpression: /^$|^([0-9A-Fa-f]{2}|[Ee][01][0-9A-Fa-f]{2})$/ }
                    onEditingFinished: {
                        const p = "/positions/" + panel.position + "/scanCode"
                        if (text === (panel.pos.scanCode || ""))
                            return
                        if (text === "")
                            doc.removeAt(p, qsTr("Clear scan code of %1").arg(panel.position))
                        else
                            doc.setValueAt(p, text, qsTr("Set scan code of %1").arg(panel.position))
                    }
                }
                Label { text: qsTr("Name"); color: Ui.subtle }
                TextField {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: panel.pos.name || ""
                    placeholderText: qsTr("shown by key-name lists")
                    onEditingFinished: {
                        const p = "/positions/" + panel.position + "/name"
                        if (text === (panel.pos.name || ""))
                            return
                        if (text === "")
                            doc.removeAt(p, qsTr("Clear name of %1").arg(panel.position))
                        else
                            doc.setValueAt(p, text, qsTr("Rename %1").arg(panel.position))
                    }
                }
                Item { width: 1; height: 1 }
                CheckBox {
                    text: qsTr("Number-pad key (Num Lock)")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    clip: true
                    checked: !!panel.pos.numLock
                    onToggled: {
                        const p = "/positions/" + panel.position + "/numLock"
                        if (checked)
                            doc.setValueAt(p, true, qsTr("Num Lock on %1").arg(panel.position))
                        else
                            doc.removeAt(p, qsTr("No Num Lock on %1").arg(panel.position))
                    }
                }
            }

            SectionHeader { text: qsTr("All layers") }
            Repeater {
                model: panel.allLayers
                delegate: ItemDelegate {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    highlighted: modelData.layer === panel.layerIndex
                    onClicked: panel.selectLayer(modelData.layer)
                    contentItem: RowLayout {
                        spacing: Ui.gap
                        Label {
                            text: modelData.layerLabel
                            color: Ui.subtle
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            elide: Text.ElideRight
                        }
                        Label {
                            text: modelData.kind === "unbound" ? (modelData.fallback ? modelData.fallback : "—") : modelData.display
                            color: modelData.kind === "deadKey" ? Ui.deadKey : modelData.kind === "unbound" ? Ui.faint : Ui.text
                            font.italic: modelData.kind === "unbound" && !!modelData.fallback
                            font.pixelSize: Qt.application.font.pixelSize * 1.15
                        }
                    }
                }
            }
        }
    }
}
