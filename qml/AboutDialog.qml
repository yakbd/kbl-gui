import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    title: qsTr("About YAKBD Editor")
    modal: true
    standardButtons: Dialog.Close
    width: 480
    padding: Ui.pad * 2

    ColumnLayout {
        width: parent.width
        spacing: Ui.gap
        RowLayout {
            spacing: Ui.pad
            Image {
                source: "icons/yakbd.svg"
                sourceSize.width: 56
                sourceSize.height: 56
            }
            ColumnLayout {
                spacing: 2
                Label {
                    text: qsTr("YAKBD Editor")
                    font.pixelSize: Qt.application.font.pixelSize * 1.4
                    font.weight: Font.DemiBold
                }
                Label {
                    text: qsTr("Version %1 (preview)").arg(appSettings.version)
                    color: Ui.subtle
                }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Edits .kbl.json keyboard layouts. Checking, converting and building are done by the yakbd command-line tool.")
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Ui.subtle
            text: qsTr("yakbd: %1\nQt %2, style %3").arg(cli.available ? cli.version : qsTr("not found")).arg(appSettings.qtVersion).arg(appSettings.style)
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.StyledText
            text: qsTr("MIT-0 licence. Uses Qt under the GNU LGPL v3, dynamically linked; see the NOTICE file. <a href=\"https://github.com/yakbd\">github.com/yakbd</a>")
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
    }
}
