import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Layers across the top, the keyboard in the middle, the selected key's properties on the right.
Item {
    id: page
    objectName: "keyboardPage"
    property alias layerIndex: keyboard.layerIndex
    property alias selected: keyboard.selected
    readonly property var layerList: (doc.revision, doc.layers())

    function reveal(pointer) {
        const t = doc.splitPointer(pointer)
        if (t[0] === "hardware" && t[1] === "layers" && t.length >= 3) {
            keyboard.layerIndex = Math.min(parseInt(t[2]), Math.max(0, layerList.length - 1))
            if (t[3] === "bindings" && t.length >= 5)
                keyboard.selected = t[4]
        } else if (t[0] === "positions" && t.length >= 2) {
            keyboard.selected = t[1]
        }
        keyboard.forceActiveFocus()
    }

    Connections {
        target: doc
        function onLoadedChanged() {
            keyboard.layerIndex = 0
            keyboard.selected = ""
        }
        function onFilePathChanged() {
            if (keyboard.layerIndex >= page.layerList.length)
                keyboard.layerIndex = 0
        }
    }

    // Wide: keyboard and key panel side by side. Narrow (small screens): the panel docks below the keyboard.
    readonly property bool wide: width >= 560

    GridLayout {
        anchors.fill: parent
        anchors.margins: Ui.pad
        columns: page.wide ? 2 : 1
        columnSpacing: Ui.pad
        rowSpacing: Ui.gap

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: page.wide
            spacing: Ui.gap

            // Layers: one row. When they do not fit, the row scrolls (wheel or drag) and a menu lists them all.
            RowLayout {
                Layout.fillWidth: true
                spacing: Ui.gap
                Label {
                    text: qsTr("Layer")
                    color: Ui.subtle
                }
                // When the row overflows, arrows show that layers lie beyond either end (greyed at the end reached).
                ToolButton {
                    objectName: "layerRowBack"
                    visible: layerRow.overflows
                    enabled: !layerRow.atXBeginning
                    text: "\u2039"
                    implicitWidth: 24
                    onClicked: layerRow.scrollBy(-1)
                    Accessible.name: qsTr("Earlier layers")
                }
                ListView {
                    id: layerRow
                    objectName: "layerRow"
                    Layout.fillWidth: true
                    Layout.preferredHeight: contentItem.childrenRect.height > 0 ? contentItem.childrenRect.height : 32
                    implicitWidth: 0
                    orientation: ListView.Horizontal
                    spacing: 4
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: page.layerList
                    currentIndex: keyboard.layerIndex
                    highlightFollowsCurrentItem: false
                    onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                    onWidthChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                    onCountChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                    readonly property bool overflows: contentWidth > width + 1
                    function scrollBy(step) {
                        let i = step < 0 ? indexAt(contentX + 1, 1) : indexAt(contentX + width - 2, 1)
                        if (i < 0)
                            i = step < 0 ? indexAt(contentX + spacing + 1, 1) : indexAt(contentX + width - spacing - 2, 1)
                        positionViewAtIndex(Math.max(0, Math.min(count - 1, i + step)), step < 0 ? ListView.Beginning : ListView.End)
                    }
                    delegate: Button {
                        required property var modelData
                        checkable: true
                        checked: modelData.index === keyboard.layerIndex
                        flat: !checked
                        text: modelData.label
                        onClicked: keyboard.layerIndex = modelData.index
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: (modelData.id !== "" ? qsTr("Layer “%1”: ").arg(modelData.id) : "")
                                      + qsTr("%1, %2 binding(s)").arg(modelData.modifiers).arg(modelData.count)
                        Accessible.name: qsTr("Layer %1").arg(modelData.label)
                    }
                    WheelHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onWheel: function (e) {
                            layerRow.contentX = Math.max(0, Math.min(layerRow.contentWidth - layerRow.width,
                                                                     layerRow.contentX - e.angleDelta.y))
                        }
                    }
                }
                ToolButton {
                    objectName: "layerRowForward"
                    visible: layerRow.overflows
                    enabled: !layerRow.atXEnd
                    text: "\u203a"
                    implicitWidth: 24
                    onClicked: layerRow.scrollBy(1)
                    Accessible.name: qsTr("Later layers")
                }
                ToolButton {
                    id: layerMenuButton
                    visible: layerRow.overflows
                    text: qsTr("All Layers…")
                    onClicked: layerMenu.popup(layerMenuButton, 0, layerMenuButton.height)
                    Accessible.name: qsTr("All layers")
                    Menu {
                        id: layerMenu
                        Instantiator {
                            model: page.layerList
                            delegate: MenuItem {
                                required property var modelData
                                text: modelData.label + (modelData.id !== "" ? "  (" + modelData.id + ")" : "")
                                checkable: true
                                checked: modelData.index === keyboard.layerIndex
                                onTriggered: keyboard.layerIndex = modelData.index
                            }
                            onObjectAdded: (index, object) => layerMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) => layerMenu.removeItem(object)
                        }
                    }
                }
            }
            Label {
                visible: page.layerList.length === 0
                text: qsTr("This layout has no hardware layers. Add them in the JSON page (hardware.layers).")
                color: Ui.subtle
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            KeyboardView {
                id: keyboard
                objectName: "keyboardView"
                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight
                visible: page.layerList.length > 0
                onEditRequested: panel.focusBinding()
                focus: true
            }

            Flow {  // the legend wraps on narrow windows instead of widening the page
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 10
                spacing: Ui.gap * 2
                visible: page.layerList.length > 0
                Repeater {
                    model: [
                        { mark: "a", label: qsTr("types text") },
                        { mark: "´", label: qsTr("dead key"), dead: true },
                        { mark: "a", label: qsTr("from the layer without Caps Lock"), fallback: true },
                        { mark: "", label: qsTr("no binding in this layer") }
                    ]
                    delegate: RowLayout {
                        required property var modelData
                        spacing: 6
                        Rectangle {
                            width: 22
                            height: 20
                            radius: 4
                            color: modelData.dead ? Ui.deadKeyFill : Ui.cap
                            border.color: modelData.dead ? Ui.deadKey : Ui.capEdge
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: modelData.mark
                                color: modelData.dead ? Ui.deadKey : modelData.fallback ? Ui.faint : Ui.text
                                font.italic: !!modelData.fallback
                            }
                        }
                        Label {
                            text: modelData.label
                            color: Ui.subtle
                        }
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }

        KeyPanel {
            id: panel
            Layout.preferredWidth: page.wide ? Math.max(220, Math.min(330, page.width * 0.3)) : -1
            Layout.fillWidth: !page.wide
            Layout.fillHeight: true
            Layout.minimumHeight: page.wide ? 0 : 200
            layerIndex: keyboard.layerIndex
            position: keyboard.selected
            onSelectLayer: function (i) { keyboard.layerIndex = i }
        }
    }
}
