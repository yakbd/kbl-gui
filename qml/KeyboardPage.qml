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

            // Layers: one row of whole tabs. When they do not fit, the row shows the tabs from `first` on that fit,
            // arrows step one tab (greyed at the end reached), the wheel steps too, and a menu lists them all. No tab
            // is ever cut: the strip lays out only the tabs it shows; a selected tab wider than the strip is elided.
            RowLayout {
                id: layerBar
                Layout.fillWidth: true
                spacing: Ui.gap
                Label {
                    id: layerLabel
                    text: qsTr("Layer")
                    color: Ui.subtle
                }
                ToolButton {
                    objectName: "layerRowBack"
                    visible: layerStrip.overflows
                    enabled: layerStrip.first > 0
                    text: "‹"
                    implicitWidth: 24
                    onClicked: layerStrip.step(-1)
                    Accessible.name: qsTr("Earlier layers")
                }
                Item {
                    id: layerStrip
                    objectName: "layerRow"
                    Layout.fillWidth: true
                    Layout.preferredHeight: tabs.implicitHeight > 0 ? tabs.implicitHeight : 32
                    implicitWidth: 0
                    clip: true
                    readonly property int count: tabRepeater.count
                    readonly property int spacing: 4
                    property int first: 0
                    property int last: count - 1
                    // Overflow from the bar's own width (not the strip's, which the arrows change): all tabs against
                    // the room the bar has without arrows and menu.
                    readonly property real room: layerBar.width - layerLabel.width - layerBar.spacing
                    property real total: 0
                    readonly property bool overflows: total > room + 0.5
                    function widthOf(i) {
                        const t = tabRepeater.itemAt(i)
                        return t ? t.implicitWidth : 0
                    }
                    // The last index from `from` on whose tabs fit whole (at least `from` itself).
                    function lastFrom(from) {
                        let used = 0, i = from
                        for (; i < count; ++i) {
                            const w = widthOf(i) + (i > from ? spacing : 0)
                            if (i > from && used + w > width)
                                break
                            used += w
                        }
                        return Math.max(from, i - 1)
                    }
                    // The first index from which `to` is the last tab that fits.
                    function firstTo(to) {
                        let used = 0, i = to
                        for (; i >= 0; --i) {
                            const w = widthOf(i) + (i < to ? spacing : 0)
                            if (i < to && used + w > width)
                                break
                            used += w
                        }
                        return Math.min(to, i + 1)
                    }
                    function fit() {
                        let sum = 0
                        for (let i = 0; i < count; ++i)
                            sum += widthOf(i) + (i > 0 ? spacing : 0)
                        total = sum
                        if (!overflows) {
                            first = 0
                            last = count - 1
                            return
                        }
                        first = Math.max(0, Math.min(first, count - 1))
                        // Show as many as fit from the end too, then keep the selected tab in view.
                        first = Math.min(first, firstTo(count - 1))
                        const sel = Math.max(0, Math.min(keyboard.layerIndex, count - 1))
                        if (sel < first)
                            first = sel
                        last = lastFrom(first)
                        if (sel > last) {
                            first = firstTo(sel)
                            last = lastFrom(first)
                        }
                    }
                    function step(d) {
                        if (d < 0 && first > 0)
                            first -= 1
                        else if (d > 0 && last < count - 1)
                            first = Math.min(first + 1, firstTo(count - 1))
                        last = lastFrom(first)
                    }
                    onWidthChanged: Qt.callLater(fit)
                    onRoomChanged: Qt.callLater(fit)
                    onCountChanged: Qt.callLater(fit)
                    Connections {
                        target: keyboard
                        function onLayerIndexChanged() { Qt.callLater(layerStrip.fit) }
                    }
                    Row {
                        id: tabs
                        spacing: layerStrip.spacing
                        Repeater {
                            id: tabRepeater
                            model: page.layerList
                            delegate: Button {
                                id: tab
                                objectName: "layerTab"
                                required property var modelData
                                required property int index
                                visible: index >= layerStrip.first && index <= layerStrip.last
                                checkable: true
                                checked: modelData.index === keyboard.layerIndex
                                flat: !checked
                                // implicitWidth is the whole label's; a tab alone and wider than the strip shows its
                                // label elided.
                                readonly property bool squeezed: implicitWidth > layerStrip.width && layerStrip.width > 0
                                width: squeezed ? layerStrip.width : implicitWidth
                                TextMetrics {
                                    id: fullLabel
                                    font: tab.font
                                    text: tab.modelData.label
                                    elide: Text.ElideRight
                                    elideWidth: Math.max(0, layerStrip.width - tab.leftPadding - tab.rightPadding)
                                }
                                text: squeezed ? fullLabel.elidedText : modelData.label
                                onImplicitWidthChanged: Qt.callLater(layerStrip.fit)
                                onClicked: keyboard.layerIndex = modelData.index
                                ToolTip.visible: hovered
                                ToolTip.delay: 500
                                ToolTip.text: (modelData.id !== "" ? qsTr("Layer “%1”: ").arg(modelData.id) : "")
                                              + qsTr("%1, %2 binding(s)").arg(modelData.modifiers).arg(modelData.count)
                                Accessible.name: qsTr("Layer %1").arg(modelData.label)
                            }
                        }
                    }
                    WheelHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onWheel: function (e) {
                            if (e.angleDelta.y !== 0 || e.angleDelta.x !== 0)
                                layerStrip.step((e.angleDelta.y || e.angleDelta.x) > 0 ? -1 : 1)
                        }
                    }
                }
                ToolButton {
                    objectName: "layerRowForward"
                    visible: layerStrip.overflows
                    enabled: layerStrip.last < layerStrip.count - 1
                    text: "›"
                    implicitWidth: 24
                    onClicked: layerStrip.step(1)
                    Accessible.name: qsTr("Later layers")
                }
                ToolButton {
                    id: layerMenuButton
                    visible: layerStrip.overflows
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
