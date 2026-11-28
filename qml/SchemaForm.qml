import QtQuick
import QtQuick.Layouts

// A form generated from the kbl JSON Schema for the object at docPointer. The fields follow the schema of the
// file's format version (schema.version); the list is replaced only when it really changes, so that a version
// change made by an edit in this form keeps the fields (and the focus) in place.
ColumnLayout {
    id: form
    property string docPointer: ""
    property string schemaPointer: docPointer !== "" ? (schema.version, schema.schemaFor(docPointer)) : ""
    property int depth: 0
    property string highlight: ""
    property var fieldList: []
    signal reveal(Item item)
    spacing: Ui.gap * 1.5

    function refresh() {
        const f = form.schemaPointer !== "" ? schema.fields(form.schemaPointer) : []
        if (JSON.stringify(f) !== JSON.stringify(form.fieldList))
            form.fieldList = f
    }
    onSchemaPointerChanged: refresh()
    Component.onCompleted: refresh()
    Connections {
        target: schema
        function onVersionChanged() { form.refresh() }
    }

    Repeater {
        model: form.fieldList
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
