import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Any part of the file as JSON, for what the other pages do not cover. Apply replaces that part as one undo step.
Item {
    id: page
    objectName: "jsonPage"
    property string pointer: ""
    property string error: ""
    readonly property string current: (doc.revision, doc.jsonAt(pointer))

    function reveal(p) {
        page.pointer = p.startsWith("/json-view") ? p.slice("/json-view".length) : p
        pointerField.text = page.pointer
        editor.text = page.current
        page.error = ""
        editor.forceActiveFocus()
    }
    onCurrentChanged: if (!editor.activeFocus) editor.text = current
    Component.onCompleted: editor.text = current

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.pad
        spacing: Ui.gap

        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.gap
            Label { text: qsTr("JSON Pointer"); color: Ui.subtle }
            TextField {
                id: pointerField
                Layout.fillWidth: true
                font.family: "monospace"
                placeholderText: qsTr("/ is the whole file, e.g. /hardware/layers/0")
                onAccepted: page.reveal(text)
                Accessible.name: qsTr("JSON Pointer")
            }
            Button {
                text: qsTr("Show")
                onClicked: page.reveal(pointerField.text)
            }
        }
        Label {
            Layout.fillWidth: true
            visible: page.pointer !== "" && !doc.exists(page.pointer)
            text: qsTr("Nothing is at this pointer yet; Apply creates it.")
            color: Ui.subtle
        }
        Label {
            Layout.fillWidth: true
            visible: page.pointer !== ""
            text: schema.describe(page.pointer)
            color: Ui.subtle
            wrapMode: Text.WordWrap
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                id: editor
                font.family: "monospace"
                selectByMouse: true
                wrapMode: TextEdit.NoWrap
                readOnly: page.pointer === ""
                placeholderText: qsTr("Enter a pointer above, or follow a link from another page or the Problems panel")
                Accessible.name: qsTr("JSON text")
            }
        }
        RowLayout {
            spacing: Ui.gap
            Label {
                Layout.fillWidth: true
                text: page.error
                color: Ui.error
                wrapMode: Text.WordWrap
            }
            Button {
                text: qsTr("Revert")
                enabled: editor.text !== page.current
                onClicked: {
                    editor.text = page.current
                    page.error = ""
                }
            }
            Button {
                text: qsTr("Remove")
                enabled: page.pointer !== "" && doc.exists(page.pointer)
                onClicked: doc.removeAt(page.pointer)
            }
            Button {
                text: qsTr("Apply")
                highlighted: true
                enabled: page.pointer !== "" && editor.text !== page.current
                onClicked: page.error = doc.setJsonAt(page.pointer, editor.text, qsTr("Edit %1 as JSON").arg(page.pointer))
            }
        }
    }
}
