import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One field of a generated form. An empty field is absent from the file, so the format's default (or, for the
// Windows target, the CLI's default rule) applies; the description comes from the schema.
ColumnLayout {
    id: f
    property var field: ({})
    property string pointer: ""
    property int depth: 0
    property string highlight: ""
    signal reveal(Item item)
    spacing: 4

    readonly property string kind: field.kind || "json"
    readonly property bool present: (doc.revision, doc.exists(pointer))
    readonly property var value: (doc.revision, doc.valueAt(pointer))
    readonly property bool highlighted: highlight !== "" && (highlight === pointer || highlight.startsWith(pointer + "/"))
                                        && kind !== "object"
    readonly property string label: field.name + (field.required ? " *" : "")

    onHighlightedChanged: if (highlighted) {
        f.reveal(f)
        focusTimer.start()
    }
    Timer {
        id: focusTimer
        interval: 50
        onTriggered: {
            const item = editorLoader.item
            if (item && item.forceActiveFocus)
                item.forceActiveFocus()
        }
    }

    function commitText(text, numeric) {
        if (text === "") {
            if (present)
                doc.removeAt(pointer, qsTr("Clear %1").arg(field.name))
            return
        }
        const v = numeric ? Number(text) : text
        if (present && value === v)
            return
        doc.setValueAt(pointer, v, qsTr("Set %1").arg(field.name))
    }

    // Nested object: a sub-heading and its own fields.
    Loader {
        Layout.fillWidth: true
        active: f.kind === "object" && f.depth < 2
        visible: active
        sourceComponent: ColumnLayout {
            spacing: Ui.gap
            SectionHeader { text: f.field.name }
            Label {
                visible: !!f.field.description
                text: f.field.description || ""
                color: Ui.subtle
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Loader {
                Layout.fillWidth: true
                Layout.leftMargin: Ui.pad
                Component.onCompleted: setSource(Qt.resolvedUrl("SchemaForm.qml"),
                                                 { docPointer: f.pointer, schemaPointer: f.field.schema, depth: f.depth + 1 })
                onLoaded: {
                    item.docPointer = Qt.binding(() => f.pointer)
                    item.schemaPointer = Qt.binding(() => f.field.schema)
                    item.depth = f.depth + 1
                    item.highlight = Qt.binding(() => f.highlight)
                    item.reveal.connect(f.reveal)
                }
            }
        }
    }

    RowLayout {
        visible: !(f.kind === "object" && f.depth < 2)
        Layout.fillWidth: true
        spacing: Ui.pad

        Label {
            text: f.label
            Layout.preferredWidth: 190
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 6
            elide: Text.ElideRight
            font.weight: f.present ? Font.DemiBold : Font.Normal
            color: f.highlighted ? Ui.accent : palette.text
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Loader {
                id: editorLoader
                Layout.fillWidth: true
                sourceComponent: {
                    switch (f.kind) {
                    case "string": return textEditor
                    case "integer":
                    case "number": return numberEditor
                    case "boolean": return boolEditor
                    case "enum": return enumEditor
                    case "enumSet": return setEditor
                    case "stringList": return listEditor
                    default: return jsonEditor
                    }
                }
            }
            Label {
                visible: !!f.field.description
                text: f.field.description || ""
                color: Ui.subtle
                font.pixelSize: Qt.application.font.pixelSize * 0.92
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }

    Component {
        id: textEditor
        TextField {
            text: f.present && typeof f.value === "string" ? f.value : ""
            placeholderText: f.present ? "" : qsTr("not set")
            selectByMouse: true
            readonly property bool bad: f.field.pattern && text !== "" && !(new RegExp(f.field.pattern)).test(text)
            color: bad ? Ui.error : palette.text
            onEditingFinished: f.commitText(text, false)
            Accessible.name: f.field.name
            ToolTip.visible: bad && activeFocus
            ToolTip.text: qsTr("Expected the pattern %1").arg(f.field.pattern)
        }
    }
    Component {
        id: numberEditor
        TextField {
            text: f.present && typeof f.value === "number" ? String(f.value) : ""
            placeholderText: f.present ? "" : (f.field.minimum !== undefined ? qsTr("not set (%1–%2)").arg(f.field.minimum).arg(f.field.maximum)
                                                                           : qsTr("not set"))
            selectByMouse: true
            validator: RegularExpressionValidator {
                regularExpression: f.kind === "integer" ? /^$|^-?\d+$/ : /^$|^-?\d+(\.\d+)?([eE][+-]?\d+)?$/
            }
            onEditingFinished: f.commitText(text, true)
            Accessible.name: f.field.name
        }
    }
    Component {
        id: boolEditor
        ComboBox {
            model: [qsTr("Not set (default)"), qsTr("Yes (true)"), qsTr("No (false)")]
            currentIndex: !f.present ? 0 : f.value === true ? 1 : 2
            onActivated: function (i) {
                if (i === 0)
                    doc.removeAt(f.pointer, qsTr("Clear %1").arg(f.field.name))
                else
                    doc.setValueAt(f.pointer, i === 1, qsTr("Set %1").arg(f.field.name))
            }
            Accessible.name: f.field.name
        }
    }
    Component {
        id: enumEditor
        ComboBox {
            model: [qsTr("Not set (default)")].concat(f.field.options || [])
            currentIndex: f.present ? Math.max(0, (f.field.options || []).indexOf(f.value) + 1) : 0
            onActivated: function (i) {
                if (i === 0)
                    doc.removeAt(f.pointer, qsTr("Clear %1").arg(f.field.name))
                else
                    doc.setValueAt(f.pointer, f.field.options[i - 1], qsTr("Set %1").arg(f.field.name))
            }
            Accessible.name: f.field.name
        }
    }
    Component {
        id: setEditor
        Flow {
            spacing: Ui.gap
            readonly property var chosen: (doc.revision, f.present ? JSON.parse(doc.jsonAt(f.pointer) || "[]") : [])
            Repeater {
                model: f.field.options || []
                delegate: CheckBox {
                    required property string modelData
                    text: modelData
                    checked: parent.chosen.indexOf(modelData) >= 0
                    onToggled: {
                        let list = (f.field.options || []).filter(o => o === modelData ? checked : parent.chosen.indexOf(o) >= 0)
                        doc.setValueAt(f.pointer, list, qsTr("Set %1").arg(f.field.name))
                    }
                }
            }
            Button {
                visible: f.present
                flat: true
                text: qsTr("Use default")
                onClicked: doc.removeAt(f.pointer, qsTr("Clear %1").arg(f.field.name))
            }
            Label {
                visible: !f.present
                text: qsTr("not set: the default applies")
                color: Ui.subtle
                topPadding: 8
            }
        }
    }
    Component {
        id: listEditor
        TextField {
            text: (doc.revision, f.present ? JSON.parse(doc.jsonAt(f.pointer) || "[]").join(", ") : "")
            placeholderText: qsTr("not set; comma-separated")
            selectByMouse: true
            onEditingFinished: {
                const items = text.split(",").map(s => s.trim()).filter(s => s !== "")
                if (items.length === 0) {
                    if (f.present)
                        doc.removeAt(f.pointer, qsTr("Clear %1").arg(f.field.name))
                } else if (JSON.stringify(items) !== (f.present ? JSON.stringify(JSON.parse(doc.jsonAt(f.pointer))) : "")) {
                    doc.setValueAt(f.pointer, items, qsTr("Set %1").arg(f.field.name))
                }
            }
            Accessible.name: f.field.name
        }
    }
    Component {
        id: jsonEditor
        RowLayout {
            spacing: Ui.gap
            Label {
                Layout.fillWidth: true
                text: f.present ? (doc.revision, doc.jsonAt(f.pointer)).replace(/\s+/g, " ").slice(0, 120) : qsTr("not set")
                color: f.present ? palette.text : Ui.subtle
                elide: Text.ElideRight
                font.family: f.present ? "monospace" : Qt.application.font.family
            }
            Button {
                text: f.present ? qsTr("Edit as JSON") : qsTr("Add as JSON")
                onClicked: doc.navigate("/json-view" + f.pointer)
            }
        }
    }
}
