import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Appearance and the command-line tool. Changes apply at once (no OK needed), as desktop preference windows do.
Dialog {
    id: dlg
    title: appSettings.platform === "macos" ? qsTr("Settings") : qsTr("Preferences")
    modal: true
    standardButtons: Dialog.Close
    width: 560
    padding: Ui.pad * 2

    onOpened: appearanceGroup.forceActiveFocus()

    ColumnLayout {
        width: parent.width
        spacing: Ui.gap

        Label {
            text: qsTr("Appearance")
            font.weight: Font.DemiBold
        }
        ColumnLayout {
            id: appearanceGroup
            spacing: 2
            RadioButton {
                text: qsTr("Follow the system")
                checked: appSettings.appearance === "system"
                onClicked: appSettings.appearance = "system"
                focus: true
            }
            RadioButton {
                text: qsTr("Light")
                checked: appSettings.appearance === "light"
                onClicked: appSettings.appearance = "light"
            }
            RadioButton {
                text: qsTr("Dark")
                checked: appSettings.appearance === "dark"
                onClicked: appSettings.appearance = "dark"
            }
        }

        Label {
            text: qsTr("Command-line tool")
            font.weight: Font.DemiBold
            Layout.topMargin: Ui.pad
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Ui.subtle
            text: qsTr("The editor asks yakbd to check, convert and build layouts. It uses the copy next to the editor, or the one on the PATH, unless you choose one here.")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.gap
            TextField {
                id: cliField
                Layout.fillWidth: true
                text: appSettings.cliPath
                placeholderText: qsTr("automatic")
                onEditingFinished: appSettings.cliPath = text
                Accessible.name: qsTr("Path of yakbd")
            }
            Button {
                text: qsTr("Choose…")
                onClicked: cliDialog.open()
            }
            Button {
                text: qsTr("Automatic")
                enabled: appSettings.cliPath !== ""
                onClicked: {
                    appSettings.cliPath = ""
                    cliField.text = ""
                }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: cli.available ? qsTr("Using yakbd %1 at %2").arg(cli.version).arg(appSettings.nativePath(cli.program))
                                : cli.problem
            color: cli.available ? Ui.ok : Ui.error
        }
    }

    FileDialog {
        id: cliDialog
        title: qsTr("Choose the yakbd Program")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            appSettings.cliPath = appSettings.toLocalFile(selectedFile)
            cliField.text = appSettings.cliPath
        }
    }
}
