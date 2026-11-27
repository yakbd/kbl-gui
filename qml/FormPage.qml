import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A page of fields generated from the schema, for one object of the file (or several, as sections).
Item {
    id: page
    property string heading: ""
    property string note: ""
    property string basePointer: ""
    property var sections: []
    property string highlight: ""

    function reveal(pointer) {
        highlight = ""
        highlight = pointer
    }

    function scrollTo(item) {
        const y = item.mapToItem(content, 0, 0).y
        const flick = scroll.contentItem
        flick.contentY = Math.max(0, Math.min(y - 40, flick.contentHeight - flick.height))
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            id: content
            width: Math.min(scroll.availableWidth, 900)
            x: Ui.pad * 2
            spacing: Ui.gap

            Item { Layout.preferredHeight: Ui.pad }
            Label {
                text: page.heading
                font.pixelSize: Qt.application.font.pixelSize * 1.45
                font.weight: Font.DemiBold
            }
            Label {
                text: page.note
                color: Ui.subtle
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.maximumWidth: 720
                Layout.bottomMargin: Ui.gap
            }
            SchemaForm {
                visible: page.sections.length === 0
                Layout.fillWidth: true
                Layout.rightMargin: Ui.pad * 4
                docPointer: page.basePointer
                highlight: page.highlight
                onReveal: function (item) { page.scrollTo(item) }
            }
            Repeater {
                model: page.sections
                delegate: ColumnLayout {
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.rightMargin: Ui.pad * 4
                    spacing: Ui.gap
                    SectionHeader {
                        text: modelData
                        Layout.topMargin: Ui.pad
                    }
                    SchemaForm {
                        Layout.fillWidth: true
                        docPointer: page.basePointer + "/" + modelData
                        highlight: page.highlight
                        onReveal: function (item) { page.scrollTo(item) }
                    }
                }
            }
            Item { Layout.preferredHeight: Ui.pad * 2 }
        }
    }
}
