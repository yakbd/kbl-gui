import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Dead keys (SPEC §10): the list on the left, the selected one's composition table on the right.
Item {
    id: page
    objectName: "deadKeysPage"
    property string current: ""
    readonly property var ids: (doc.revision, doc.deadKeyIds())
    readonly property var info: (doc.revision, current ? doc.deadKeyInfo(current) : ({}))
    readonly property var entries: (doc.revision, current ? doc.deadKeyEntries(current) : [])
    property int focusEntry: -1

    onIdsChanged: if (ids.indexOf(current) < 0) current = ids.length > 0 ? ids[0] : ""

    function reveal(pointer) {
        const t = doc.splitPointer(pointer)
        if (t.length >= 2 && ids.indexOf(t[1]) >= 0)
            current = t[1]
        focusEntry = t[2] === "entries" && t.length >= 4 ? parseInt(t[3]) : -1
        if (focusEntry >= 0)
            entryList.positionViewAtIndex(focusEntry, ListView.Contain)
        list.forceActiveFocus()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Ui.pad
        spacing: Ui.pad

        ColumnLayout {
            Layout.preferredWidth: 230
            Layout.fillHeight: true
            spacing: Ui.gap

            Label {
                text: qsTr("Dead keys")
                font.weight: Font.DemiBold
            }
            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 1
                ListView {
                    id: list
                    anchors.fill: parent
                    clip: true
                    model: page.ids
                    currentIndex: page.ids.indexOf(page.current)
                    keyNavigationEnabled: true
                    activeFocusOnTab: true
                    onCurrentIndexChanged: if (currentIndex >= 0) page.current = page.ids[currentIndex]
                    Accessible.name: qsTr("Dead keys")
                    delegate: ItemDelegate {
                        required property string modelData
                        required property int index
                        width: ListView.view.width
                        highlighted: ListView.isCurrentItem
                        onClicked: list.currentIndex = index
                        readonly property var di: (doc.revision, doc.deadKeyInfo(modelData))
                        contentItem: RowLayout {
                            spacing: Ui.gap
                            Text {
                                text: di.display
                                color: Ui.deadKey
                                font.pixelSize: Qt.application.font.pixelSize * 1.4
                                Layout.preferredWidth: 28
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Label {
                                text: modelData
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Label {
                                text: di.count
                                color: Ui.subtle
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        width: parent.width - 24
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        color: Ui.subtle
                        text: qsTr("No dead keys. A dead key waits for the next key and combines with it.")
                    }
                }
            }
            RowLayout {
                spacing: Ui.gap
                TextField {
                    id: newId
                    Layout.fillWidth: true
                    placeholderText: qsTr("new id, e.g. acute")
                    onAccepted: addButton.clicked()
                }
                TextField {
                    id: newSpacing
                    Layout.preferredWidth: 44
                    placeholderText: "´"
                    horizontalAlignment: Text.AlignHCenter
                    onAccepted: addButton.clicked()
                    ToolTip.visible: activeFocus
                    ToolTip.text: qsTr("The character the dead key stands for")
                }
                Button {
                    id: addButton
                    text: qsTr("Add")
                    enabled: newId.text !== "" && newSpacing.text !== ""
                    onClicked: {
                        const err = doc.addDeadKey(newId.text, newSpacing.text)
                        if (err !== "") {
                            addError.text = err
                            return
                        }
                        page.current = newId.text
                        addError.text = ""
                        newId.text = ""
                        newSpacing.text = ""
                    }
                }
            }
            Label {
                id: addError
                visible: text !== ""
                color: Ui.error
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Ui.gap
            visible: page.current !== ""

            RowLayout {
                spacing: Ui.pad
                Label {
                    text: page.info.display || ""
                    color: Ui.deadKey
                    font.pixelSize: Qt.application.font.pixelSize * 2.4
                }
                Label {
                    text: page.current
                    font.pixelSize: Qt.application.font.pixelSize * 1.45
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                Button {
                    text: qsTr("Remove Dead Key")
                    onClicked: doc.removeDeadKey(page.current)
                }
            }

            GridLayout {
                columns: 4
                columnSpacing: Ui.gap
                rowSpacing: Ui.gap
                Layout.fillWidth: true
                Label { text: qsTr("Stands for"); color: Ui.subtle }
                TextField {
                    Layout.preferredWidth: 90
                    text: page.info.spacing || ""
                    enabled: page.info.spacingKind === "text"
                    onEditingFinished: if (text !== page.info.spacing && text !== "")
                                           doc.setValueAt(page.info.pointer + "/spacing", text, qsTr("Edit dead key %1").arg(page.current))
                }
                Label { text: qsTr("Name"); color: Ui.subtle }
                TextField {
                    Layout.fillWidth: true
                    text: page.info.name || ""
                    placeholderText: qsTr("e.g. ACUTE ACCENT")
                    onEditingFinished: {
                        if (text === (page.info.name || ""))
                            return
                        if (text === "")
                            doc.removeAt(page.info.pointer + "/name")
                        else
                            doc.setValueAt(page.info.pointer + "/name", text, qsTr("Rename dead key %1").arg(page.current))
                    }
                }
                Label { text: qsTr("No match"); color: Ui.subtle }
                ComboBox {
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { value: "emitBoth", text: qsTr("Type the dead key's character, then the key (default)") },
                        { value: "emitText", text: qsTr("Type only the next key") },
                        { value: "emitSpacing", text: qsTr("Type only the dead key's character") },
                        { value: "emitNothing", text: qsTr("Type nothing") }
                    ]
                    currentIndex: Math.max(0, indexOfValue(page.info.noMatch || "emitBoth"))
                    onActivated: {
                        if (currentValue === "emitBoth")
                            doc.removeAt(page.info.pointer + "/noMatch", qsTr("Dead key %1: default no-match").arg(page.current))
                        else
                            doc.setValueAt(page.info.pointer + "/noMatch", currentValue, qsTr("Dead key %1: no-match").arg(page.current))
                    }
                }
            }

            SectionHeader { text: qsTr("Compositions (%1)").arg(page.entries.length) }
            RowLayout {
                Layout.fillWidth: true
                spacing: Ui.gap
                Label { text: qsTr("Next key"); color: Ui.subtle; Layout.preferredWidth: 160 }
                Label { text: qsTr("Result"); color: Ui.subtle; Layout.fillWidth: true }
            }
            ListView {
                id: entryList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: page.entries
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                delegate: RowLayout {
                    required property var modelData
                    required property int index
                    width: ListView.view.width - 12
                    spacing: Ui.gap
                    TextField {
                        Layout.preferredWidth: 160
                        text: modelData.base
                        enabled: modelData.baseKind === "text"
                        font.pixelSize: Qt.application.font.pixelSize * 1.2
                        selectByMouse: true
                        onEditingFinished: if (text !== modelData.base)
                                               doc.setDeadKeyEntry(page.current, modelData.index, text, modelData.result)
                        Accessible.name: qsTr("Entry %1 base").arg(index + 1)
                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.color: Ui.accent
                            border.width: 2
                            radius: 4
                            visible: page.focusEntry === index
                        }
                    }
                    Label { text: "→"; color: Ui.subtle }
                    TextField {
                        Layout.fillWidth: true
                        text: modelData.result
                        enabled: modelData.resultKind === "text"
                        font.pixelSize: Qt.application.font.pixelSize * 1.2
                        selectByMouse: true
                        onEditingFinished: if (text !== modelData.result)
                                               doc.setDeadKeyEntry(page.current, modelData.index, modelData.base, text)
                        Accessible.name: qsTr("Entry %1 result").arg(index + 1)
                    }
                    Label {
                        text: modelData.resultKind === "deadKey" ? qsTr("chains to") : ""
                        color: Ui.deadKey
                        visible: text !== ""
                    }
                    ToolButton {
                        text: qsTr("Remove")
                        onClicked: doc.removeDeadKeyEntry(page.current, modelData.index)
                        Accessible.name: qsTr("Remove entry %1").arg(index + 1)
                    }
                }
            }
            RowLayout {
                spacing: Ui.gap
                TextField {
                    id: newBase
                    Layout.preferredWidth: 160
                    placeholderText: qsTr("next key, e.g. e")
                    onAccepted: newResult.forceActiveFocus()
                }
                Label { text: "→"; color: Ui.subtle }
                TextField {
                    id: newResult
                    Layout.fillWidth: true
                    placeholderText: qsTr("result, e.g. é")
                    onAccepted: addEntry.clicked()
                }
                Button {
                    id: addEntry
                    text: qsTr("Add Composition")
                    enabled: newBase.text !== "" && newResult.text !== ""
                    onClicked: {
                        doc.addDeadKeyEntry(page.current, newBase.text, newResult.text)
                        newBase.text = ""
                        newResult.text = ""
                        newBase.forceActiveFocus()
                    }
                }
            }
        }
    }
}
