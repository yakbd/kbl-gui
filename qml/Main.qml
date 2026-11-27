import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    width: Math.min(1280, Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth - 32 : 1280)
    height: Math.min(820, Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight - 48 : 820)
    minimumWidth: 760  // fits a 1024 x 768 screen with the taskbar, as do the defaults above
    minimumHeight: 520
    visible: true
    title: doc.loaded ? (appSettings.platform === "macos" ? "" : (doc.dirty ? "*" : "")) + doc.displayName
                        + (appSettings.platform === "macos" && doc.dirty ? " — " + qsTr("Edited") : "")
                        + " – " + Qt.application.name
                      : Qt.application.name

    readonly property bool mac: appSettings.platform === "macos"
    readonly property bool windows: appSettings.platform === "windows"
    property string statusText: ""
    property var pendingAction: null
    property bool forceClose: false
    property string importedFrom: ""
    property bool showProblems: true

    Component.onCompleted: Ui.p = Qt.binding(function () { return root.palette })

    // ---------------------------------------------------------------------------------------------------------
    // Called from C++ (command line) and from the menus.
    function openPath(path) {
        path = String(path)
        if (cli.needsImport(path)) {
            if (!cli.available) {
                status(qsTr("Opening %1 needs the yakbd tool: %2").arg(appSettings.fileName(path)).arg(cli.problem))
                return
            }
            report.clear()
            status(qsTr("Converting %1…").arg(appSettings.fileName(path)))
            cli.importLayout(path)
            return
        }
        if (doc.open(path)) {
            appSettings.addRecent(path)
            appSettings.lastFolder = appSettings.folderOf(path)
            importedFrom = ""
            report.clear()
            status(qsTr("Opened %1").arg(doc.displayName))
            validateSoon.restart()
        } else {
            status(doc.lastError)
            errorDialog.text = doc.lastError
            errorDialog.open()
        }
    }

    function showPage(name) {
        const i = pages.findIndex(p => p.key === String(name))
        if (i >= 0)
            sidebar.currentIndex = i
    }

    function showOpenDialog() {
        openDialog.open()
    }

    function status(text) {
        statusText = text
    }

    function guard(action) {
        if (!doc.loaded || !doc.dirty) {
            action()
            return
        }
        pendingAction = action
        unsavedDialog.open()
    }

    function saveThen(action) {
        if (doc.filePath === "") {
            pendingAfterSaveAs = action
            saveDialog.open()
            return
        }
        if (doc.save()) {
            status(qsTr("Saved %1").arg(doc.displayName))
            if (action)
                action()
        } else {
            errorDialog.text = doc.lastError
            errorDialog.open()
        }
    }
    property var pendingAfterSaveAs: null

    function navigate(pointer) {
        const t = doc.splitPointer(pointer)
        if (t.length === 0)
            return
        if (t[0] === "hardware" || t[0] === "positions") {
            showPage("keyboard")
            keyboardPage.reveal(pointer)
        } else if (t[0] === "deadKeys") {
            showPage("deadkeys")
            deadKeysPage.reveal(pointer)
        } else if (t[0] === "metadata") {
            showPage("metadata")
            metadataPage.reveal(pointer)
        } else if (t[0] === "settings") {
            showPage("settings")
            settingsPage.reveal(pointer)
        } else if (t[0] === "targets" && t[1] === "windows") {
            showPage("windows")
            windowsPage.reveal(pointer)
        } else if (t[0] === "targets") {
            showPage("targets")
            targetsPage.reveal(pointer)
        } else {
            showPage("json")
            jsonPage.reveal(pointer)
        }
    }

    Connections {
        target: doc
        function onNavigateRequested(pointer) { root.navigate(pointer) }
        function onChanged() {
            if (report.total > 0)
                report.stale = true
            if (doc.loaded && doc.canUndo)
                validateSoon.restart()
        }
    }

    Connections {
        target: cli
        function onFinished(op, ok, message) {
            root.status(message)
            if (op === "import" && ok)
                validateSoon.restart()
            if (op === "pin" && ok)
                validateSoon.restart()
            if (!ok && op !== "validate")
                root.showProblems = true
        }
        function onImported(source) {
            root.importedFrom = source
            appSettings.addRecent(source)
            appSettings.lastFolder = appSettings.folderOf(source)
        }
    }

    // Checks the document with the CLI shortly after it changes, so the Problems panel stays current.
    Timer {
        id: validateSoon
        interval: 600
        onTriggered: {
            if (!doc.loaded || !cli.available)
                return
            if (cli.busy)
                restart()
            else
                cli.validate()
        }
    }

    onClosing: function (close) {
        if (doc.dirty && !forceClose) {
            close.accepted = false
            guard(function () {
                root.forceClose = true
                root.close()
            })
        }
    }

    // ---------------------------------------------------------------------------------------------------------
    // Actions (one definition each; menus and shortcuts use them)
    Action {
        id: newAction
        text: qsTr("&New")
        shortcut: StandardKey.New
        onTriggered: root.guard(function () {
            doc.newDocument()
            report.clear()
            root.importedFrom = ""
            root.status(qsTr("New layout from the template; choose Save As to name it"))
            validateSoon.restart()
        })
    }
    Action {
        id: openAction
        text: qsTr("&Open…")
        shortcut: StandardKey.Open
        onTriggered: root.guard(function () { openDialog.open() })
    }
    Action {
        id: saveAction
        text: qsTr("&Save")
        shortcut: StandardKey.Save
        enabled: doc.loaded
        onTriggered: root.saveThen(null)
    }
    Action {
        id: saveAsAction
        text: qsTr("Save &As…")
        shortcut: StandardKey.SaveAs
        enabled: doc.loaded
        onTriggered: {
            root.pendingAfterSaveAs = null
            saveDialog.open()
        }
    }
    Action {
        id: closeAction
        text: qsTr("&Close")
        shortcut: StandardKey.Close
        enabled: doc.loaded
        onTriggered: root.guard(function () {
            doc.close()
            report.clear()
        })
    }
    Action {
        id: quitAction
        text: root.windows ? qsTr("E&xit") : qsTr("&Quit")
        shortcut: StandardKey.Quit
        onTriggered: root.close()
    }
    Action {
        id: undoAction
        text: doc.undoText ? qsTr("&Undo %1").arg(doc.undoText) : qsTr("&Undo")
        shortcut: StandardKey.Undo
        enabled: doc.canUndo
        onTriggered: doc.undo()
    }
    Action {
        id: redoAction
        text: doc.redoText ? qsTr("&Redo %1").arg(doc.redoText) : qsTr("&Redo")
        shortcut: StandardKey.Redo
        enabled: doc.canRedo
        onTriggered: doc.redo()
    }
    Action {
        id: prefsAction
        text: root.mac ? qsTr("Settings…") : qsTr("&Preferences…")
        shortcut: StandardKey.Preferences
        onTriggered: prefsDialog.open()
    }
    Action {
        id: validateAction
        text: qsTr("&Validate")
        shortcut: "F7"
        enabled: doc.loaded && cli.available && !cli.busy
        onTriggered: {
            root.showProblems = true
            cli.validate()
        }
    }
    Action {
        id: pinAction
        text: qsTr("&Pin Windows Defaults")
        enabled: doc.loaded && cli.available && !cli.busy
        onTriggered: cli.pinDefaults()
    }
    Action {
        id: compileAction
        text: qsTr("&Build Layout DLLs…")
        shortcut: "Ctrl+B"
        enabled: doc.loaded && cli.available && !cli.busy
        onTriggered: compileDialog.open()
    }
    Action {
        id: installerAction
        text: qsTr("Build &Installer…")
        shortcut: "Ctrl+Shift+B"
        enabled: doc.loaded && cli.available && !cli.busy
        onTriggered: installerDialog.open()
    }
    Action {
        id: problemsAction
        text: qsTr("&Problems Panel")
        checkable: true
        checked: root.showProblems
        shortcut: "Ctrl+Shift+M"
        onTriggered: root.showProblems = checked
    }
    Action {
        id: aboutAction
        text: qsTr("&About YAKBD Editor")
        onTriggered: aboutDialog.open()
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            MenuItem { action: newAction }
            MenuItem { action: openAction }
            Menu {
                id: recentMenu
                title: qsTr("Open &Recent")
                enabled: appSettings.recentFiles.length > 0
                Instantiator {
                    model: appSettings.recentFiles
                    delegate: MenuItem {
                        required property string modelData
                        required property int index
                        text: (index < 9 ? "&" + (index + 1) + "  " : "") + appSettings.fileName(modelData)
                        onTriggered: {
                            const path = modelData
                            root.guard(function () { root.openPath(path) })
                        }
                    }
                    onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => recentMenu.removeItem(object)
                }
                MenuSeparator {}
                MenuItem {
                    text: qsTr("&Clear Menu")
                    onTriggered: appSettings.clearRecent()
                }
            }
            MenuSeparator {}
            MenuItem { action: saveAction }
            MenuItem { action: saveAsAction }
            MenuSeparator {}
            MenuItem { action: closeAction }
            MenuSeparator { visible: !root.mac }
            MenuItem { action: quitAction }
        }
        Menu {
            title: qsTr("&Edit")
            MenuItem { action: undoAction }
            MenuItem { action: redoAction }
            MenuSeparator {}
            MenuItem { action: prefsAction }
        }
        Menu {
            title: qsTr("&View")
            MenuItem { action: problemsAction }
            MenuSeparator {}
            Menu {
                title: qsTr("&Appearance")
                MenuItem {
                    text: qsTr("Follow &System")
                    checkable: true
                    checked: appSettings.appearance === "system"
                    onTriggered: appSettings.appearance = "system"
                }
                MenuItem {
                    text: qsTr("&Light")
                    checkable: true
                    checked: appSettings.appearance === "light"
                    onTriggered: appSettings.appearance = "light"
                }
                MenuItem {
                    text: qsTr("&Dark")
                    checkable: true
                    checked: appSettings.appearance === "dark"
                    onTriggered: appSettings.appearance = "dark"
                }
            }
        }
        Menu {
            title: qsTr("&Layout")
            MenuItem { action: validateAction }
            MenuItem { action: pinAction }
            MenuSeparator {}
            MenuItem { action: compileAction }
            MenuItem { action: installerAction }
        }
        Menu {
            title: qsTr("&Help")
            MenuItem {
                text: qsTr("Format &Specification")
                onTriggered: Qt.openUrlExternally("https://github.com/yakbd/kbl-spec/blob/main/SPEC.md")
            }
            MenuItem {
                text: qsTr("Command-Line &Tool")
                onTriggered: Qt.openUrlExternally("https://github.com/yakbd/yakbd-cli#readme")
            }
            MenuSeparator {}
            MenuItem { action: aboutAction }
        }
    }

    // ---------------------------------------------------------------------------------------------------------
    // Dialogs
    readonly property var layoutFilters: [
        qsTr("Keyboard layouts (*.kbl.json *.klc *.xml *.dll)"),
        qsTr("YAKBD layouts (*.kbl.json)"),
        qsTr("MSKLC source files (*.klc)"),
        qsTr("CLDR keyboards (*.xml)"),
        qsTr("Windows layout DLLs (*.dll)"),
        qsTr("All files (*)")
    ]
    FileDialog {
        id: openDialog
        title: qsTr("Open Layout")
        fileMode: FileDialog.OpenFile
        nameFilters: root.layoutFilters
        currentFolder: appSettings.lastFolder ? appSettings.fromLocalFile(appSettings.lastFolder) : ""
        onAccepted: root.openPath(appSettings.toLocalFile(selectedFile))
    }
    FileDialog {
        id: saveDialog
        title: qsTr("Save Layout As")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "kbl.json"
        nameFilters: [qsTr("YAKBD layouts (*.kbl.json)")]
        currentFolder: appSettings.lastFolder ? appSettings.fromLocalFile(appSettings.lastFolder) : ""
        selectedFile: appSettings.fromLocalFile((appSettings.lastFolder || "") + "/" + doc.fileNameForCli())
        onAccepted: {
            let path = appSettings.toLocalFile(selectedFile)
            if (!path.endsWith(".kbl.json"))
                path = path.replace(/\.json$/, "") + ".kbl.json"
            if (doc.saveAs(path)) {
                appSettings.addRecent(path)
                appSettings.lastFolder = appSettings.folderOf(path)
                root.importedFrom = ""
                root.status(qsTr("Saved %1").arg(doc.displayName))
                if (root.pendingAfterSaveAs) {
                    const a = root.pendingAfterSaveAs
                    root.pendingAfterSaveAs = null
                    a()
                }
            } else {
                errorDialog.text = doc.lastError
                errorDialog.open()
            }
        }
        onRejected: root.pendingAfterSaveAs = null
    }
    FolderDialog {
        id: compileDialog
        title: qsTr("Folder for the Layout DLLs")
        currentFolder: appSettings.lastFolder ? appSettings.fromLocalFile(appSettings.lastFolder) : ""
        onAccepted: {
            root.status(qsTr("Building…"))
            cli.compile(appSettings.toLocalFile(selectedFolder))
        }
    }
    FileDialog {
        id: installerDialog
        title: qsTr("Save Installer As")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "exe"
        nameFilters: [qsTr("Windows programs (*.exe)")]
        currentFolder: appSettings.lastFolder ? appSettings.fromLocalFile(appSettings.lastFolder) : ""
        onAccepted: {
            root.status(qsTr("Building the installer…"))
            cli.buildInstaller(appSettings.toLocalFile(selectedFile))
        }
    }
    MessageDialog {
        id: unsavedDialog
        title: qsTr("Unsaved Changes")
        text: qsTr("Do you want to save the changes to “%1”?").arg(doc.displayName)
        informativeText: qsTr("Your changes are lost if you don’t save them.")
        buttons: MessageDialog.Save | MessageDialog.Discard | MessageDialog.Cancel
        onButtonClicked: function (button, role) {
            const action = root.pendingAction
            root.pendingAction = null
            if (button === MessageDialog.Save)
                root.saveThen(action)
            else if (button === MessageDialog.Discard && action)
                action()
        }
    }
    MessageDialog {
        id: errorDialog
        title: qsTr("YAKBD Editor")
        buttons: MessageDialog.Ok
    }
    PreferencesDialog {
        id: prefsDialog
        objectName: "preferencesDialog"
        anchors.centerIn: parent
    }
    AboutDialog {
        id: aboutDialog
        anchors.centerIn: parent
    }

    // ---------------------------------------------------------------------------------------------------------
    // Window content
    readonly property var pages: [
        { key: "keyboard", title: qsTr("Keyboard") },
        { key: "deadkeys", title: qsTr("Dead Keys") },
        { key: "metadata", title: qsTr("Metadata") },
        { key: "settings", title: qsTr("Behaviour") },
        { key: "windows", title: qsTr("Windows Target") },
        { key: "targets", title: qsTr("Other Targets") },
        { key: "json", title: qsTr("JSON") }
    ]

    footer: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Ui.pad
            anchors.rightMargin: Ui.pad
            spacing: Ui.gap * 2
            Label {
                Layout.fillWidth: true
                text: root.statusText
                elide: Text.ElideRight
            }
            BusyIndicator {
                running: cli.busy
                visible: cli.busy
                Layout.preferredHeight: 18
                Layout.preferredWidth: 18
            }
            Label {
                text: cli.available ? qsTr("yakbd %1").arg(cli.version) : qsTr("yakbd not found")
                color: cli.available ? Ui.subtle : Ui.error
                ToolTip.visible: cliStatusArea.containsMouse
                ToolTip.text: cli.available ? appSettings.nativePath(cli.program) : cli.problem
                MouseArea {
                    id: cliStatusArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: prefsDialog.open()
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: !doc.loaded

        ColumnLayout {
            anchors.centerIn: parent
            spacing: Ui.gap * 2
            width: Math.min(parent.width - 64, 460)

            Label {
                text: qsTr("No layout open")
                font.pixelSize: Qt.application.font.pixelSize * 1.6
                font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Ui.subtle
                text: qsTr("Open a .kbl.json layout, or an MSKLC .klc file, a CLDR keyboard or a Windows layout DLL to convert it. New starts from a small QWERTY template.")
            }
            RowLayout {
                spacing: Ui.gap
                Button {
                    action: openAction
                    highlighted: true
                    focus: !doc.loaded
                }
                Button { action: newAction }
            }
            Label {
                visible: appSettings.recentFiles.length > 0
                text: qsTr("Recent")
                font.weight: Font.DemiBold
                Layout.topMargin: Ui.gap
            }
            Repeater {
                model: appSettings.recentFiles.slice(0, 5)
                delegate: ItemDelegate {
                    required property string modelData
                    Layout.fillWidth: true
                    text: appSettings.fileName(modelData)
                    ToolTip.visible: hovered
                    ToolTip.text: appSettings.nativePath(modelData)
                    onClicked: root.openPath(modelData)
                }
            }
            Label {
                visible: !cli.available
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Ui.error
                text: cli.problem + " " + qsTr("Editing works without it; checking, converting and building need it.")
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: doc.loaded

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Pane {
                Layout.fillHeight: true
                Layout.preferredWidth: root.width < 1100 ? 156 : 190
                padding: 0
                background: Rectangle { color: Ui.dark ? Qt.darker(Ui.panel, 1.12) : Qt.darker(Ui.panel, 1.03) }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        Layout.margins: Ui.pad
                        text: (doc.revision, doc.valueAt("/metadata/name")) || doc.displayName
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: Ui.pad
                        Layout.rightMargin: Ui.pad
                        Layout.topMargin: -Ui.gap
                        Layout.bottomMargin: Ui.gap
                        visible: root.importedFrom !== ""
                        text: qsTr("Converted from %1, not saved yet").arg(appSettings.fileName(root.importedFrom))
                        color: Ui.subtle
                        wrapMode: Text.WordWrap
                        font.pixelSize: Qt.application.font.pixelSize * 0.9
                    }
                    ListView {
                        id: sidebar
                        objectName: "sidebar"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: root.pages
                        currentIndex: 0
                        keyNavigationEnabled: true
                        activeFocusOnTab: true
                        clip: true
                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            text: modelData.title
                            highlighted: ListView.isCurrentItem
                            onClicked: sidebar.currentIndex = index
                        }
                        Accessible.name: qsTr("Sections")
                    }
                }
            }

            StackLayout {
                id: stack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: sidebar.currentIndex

                KeyboardPage { id: keyboardPage }
                DeadKeysPage { id: deadKeysPage }
                FormPage {
                    id: metadataPage
                    heading: qsTr("Metadata")
                    note: qsTr("Who made the layout and what it is called. The name is the default display name everywhere.")
                    basePointer: "/metadata"
                }
                FormPage {
                    id: settingsPage
                    heading: qsTr("Behaviour")
                    note: qsTr("How Caps Lock, Shift and normalisation behave on every target (SPEC §5).")
                    basePointer: "/settings"
                }
                FormPage {
                    id: windowsPage
                    heading: qsTr("Windows Target")
                    note: qsTr("Choices for the Windows build. Empty fields use the default rules; Layout › Pin Windows Defaults writes the chosen values into the file. The Defaults tab of the Problems panel lists every choice taken from a rule.")
                    basePointer: "/targets/windows"
                }
                FormPage {
                    id: targetsPage
                    heading: qsTr("Other Targets")
                    note: qsTr("Choices for writing MSKLC .klc files and CLDR keyboards.")
                    basePointer: "/targets"
                    sections: ["klc", "cldr"]
                }
                JsonPage { id: jsonPage }
            }
        }

        // Splitter between the editor and the Problems panel
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            visible: root.showProblems
            color: Ui.capEdge
            MouseArea {
                anchors.fill: parent
                anchors.topMargin: -4
                anchors.bottomMargin: -4
                cursorShape: Qt.SizeVerCursor
                property real startY: 0
                property real startH: 0
                onPressed: function (m) {
                    startY = mapToItem(null, m.x, m.y).y
                    startH = problemsPanel.Layout.preferredHeight
                }
                onPositionChanged: function (m) {
                    const y = mapToItem(null, m.x, m.y).y
                    problemsPanel.Layout.preferredHeight = Math.max(90, Math.min(root.height * 0.7, startH - (y - startY)))
                }
            }
        }
        ProblemsPanel {
            id: problemsPanel
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(210, Math.max(110, root.height * 0.24))
            visible: root.showProblems
        }
    }
}
