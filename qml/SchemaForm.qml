import QtQuick
import QtQuick.Layouts

// A form generated from the kbl JSON Schema for the object at docPointer.
ColumnLayout {
    id: form
    property string docPointer: ""
    property string schemaPointer: docPointer !== "" ? schema.schemaFor(docPointer) : ""
    property int depth: 0
    property string highlight: ""
    signal reveal(Item item)
    spacing: Ui.gap * 1.5

    Repeater {
        model: form.schemaPointer !== "" ? schema.fields(form.schemaPointer) : []
        delegate: SchemaField {
            required property var modelData
            Layout.fillWidth: true
            field: modelData
            pointer: form.docPointer + "/" + modelData.name
            depth: form.depth
            highlight: form.highlight
            onReveal: function (item) { form.reveal(item) }
        }
    }
}
