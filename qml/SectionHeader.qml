import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A group title inside a panel or form.
ColumnLayout {
    property alias text: title.text
    Layout.fillWidth: true
    Layout.topMargin: Ui.gap * 1.5
    spacing: 4
    Label {
        id: title
        font.weight: Font.DemiBold
        Layout.fillWidth: true
        elide: Text.ElideRight
        Accessible.role: Accessible.Heading
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Ui.capEdge
        opacity: 0.6
    }
}
