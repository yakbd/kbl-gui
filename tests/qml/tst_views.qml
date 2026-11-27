import QtQuick
import QtQuick.Controls
import QtTest
import Yakbd

Item {
    id: host
    width: 1200
    height: 760

    KeyboardPage {
        id: keyboardPage
        anchors.fill: parent
        visible: tests.page === "keyboard"
    }
    DeadKeysPage {
        id: deadKeysPage
        anchors.fill: parent
        visible: tests.page === "deadkeys"
    }
    FormPage {
        id: metadataPage
        anchors.fill: parent
        visible: tests.page === "metadata"
        heading: "Metadata"
        basePointer: "/metadata"
    }

    TestCase {
        id: tests
        name: "Views"
        when: windowShown
        property string page: "keyboard"

        function init() {
            verify(doc.open(testData + "/latin-altgr-deadkeys.kbl.json"))
            page = "keyboard"
        }

        function test_keyboardShowsLayers() {
            const view = findChild(keyboardPage, "keyboardView")
            verify(view)
            verify(view.rows.length >= 5)
            compare(keyboardPage.layerList.length, 8)
            keyboardPage.layerIndex = 4
            compare(view.layerIndex, 4)
        }

        function test_arrowKeysEnterAndEdit() {
            const view = findChild(keyboardPage, "keyboardView")
            view.forceActiveFocus()
            view.selected = "KeyQ"
            keyClick(Qt.Key_Right)
            compare(view.selected, "KeyW")
            keyClick(Qt.Key_Down)
            verify(view.selected === "KeyS" || view.selected === "KeyA", view.selected)
            view.selected = "KeyS"
            keyClick(Qt.Key_Return)
            const field = findChild(keyboardPage, "bindingText")
            verify(field)
            tryVerify(() => field.activeFocus)
            compare(field.text, "s")
            field.selectAll()
            keyClick(Qt.Key_X)
            keyClick(Qt.Key_Tab) // focus leaves: the edit is committed
            compare(doc.bindingAt(0, "KeyS").value, "x")
            verify(doc.dirty)
            doc.undo()
            compare(doc.bindingAt(0, "KeyS").value, "s")
            verify(!doc.dirty)
        }

        function test_codePointsField() {
            keyboardPage.selected = "KeyE"
            const cp = findChild(keyboardPage, "bindingCodePoints")
            compare(cp.text, "U+0065")
            cp.forceActiveFocus()
            cp.text = "U+0065 U+0301"
            cp.editingFinished()
            compare(doc.bindingAt(0, "KeyE").value, "é")
        }

        function test_navigateFromPointer() {
            keyboardPage.reveal("/hardware/layers/5/bindings/KeyZ")
            compare(keyboardPage.layerIndex, 5)
            compare(keyboardPage.selected, "KeyZ")
        }

        function test_deadKeysPage() {
            page = "deadkeys"
            deadKeysPage.reveal("/deadKeys/grave/entries/1")
            compare(deadKeysPage.current, "grave")
            compare(deadKeysPage.focusEntry, 1)
            verify(deadKeysPage.entries.length > 1)
        }

        function test_generatedForm() {
            page = "metadata"
            metadataPage.reveal("/metadata/name")
            compare(metadataPage.highlight, "/metadata/name")
            wait(100)
            // the focused editor is the name field; typing and leaving it changes the document
            const before = doc.valueAt("/metadata/name")
            verify(before.length > 0)
            keyClick(Qt.Key_End)
            keyClick(Qt.Key_Exclam)
            keyClick(Qt.Key_Tab)
            compare(doc.valueAt("/metadata/name"), before + "!")
            doc.undo()
            compare(doc.valueAt("/metadata/name"), before)
        }
    }
}
