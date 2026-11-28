import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The CLI's report: problems, losses and defaulted choices, plus the CLI's own output. Entries that point into
// the file open the matching field (Enter or double-click).
Pane {
    id: panel
    padding: 0
    background: Rectangle { color: Ui.panel }

    readonly property var filters: ["problems", "losses", "defaults", "all"]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header strip: section tabs drawn here (not TabBar) so they look the same in every style and scheme.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Ui.dark ? Qt.darker(Ui.panel, 1.15) : Qt.darker(Ui.panel, 1.04)
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Ui.capEdge
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Ui.gap
                spacing: 2

                Repeater {
                    id: tabs
                    property int currentIndex: 0
                    model: [
                        { title: qsTr("Problems"), count: report.problemCount, alert: report.errorCount > 0 },
                        { title: qsTr("Losses"), count: report.lossCount },
                        { title: qsTr("Defaults"), count: report.defaultCount },
                        { title: qsTr("All"), count: 0 },
                        { title: qsTr("Output"), count: 0 }
                    ]
                    delegate: AbstractButton {
                        id: tab
                        required property var modelData
                        required property int index
                        readonly property bool current: tabs.currentIndex === index
                        Layout.fillHeight: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.role: Accessible.PageTab
                        Accessible.name: modelData.title
                        onClicked: {
                            tabs.currentIndex = index
                            if (index < 4)
                                report.filter = panel.filters[index]
                        }
                        contentItem: RowLayout {
                            spacing: 6
                            Label {
                                text: tab.modelData.title
                                color: tab.current ? Ui.text : Ui.subtle
                                font.weight: tab.current ? Font.DemiBold : Font.Normal
                            }
                            Rectangle {
                                visible: tab.modelData.count > 0
                                implicitWidth: Math.max(18, countLabel.implicitWidth + 10)
                                implicitHeight: 17
                                radius: 8.5
                                color: tab.modelData.alert ? Ui.error : Qt.rgba(Ui.text.r, Ui.text.g, Ui.text.b, 0.12)
                                Label {
                                    id: countLabel
                                    anchors.centerIn: parent
                                    text: tab.modelData.count
                                    color: tab.modelData.alert ? "white" : Ui.text
                                    font.pixelSize: Qt.application.font.pixelSize * 0.82
                                }
                            }
                        }
                        leftPadding: 10
                        rightPadding: 10
                        background: Item {
                            Rectangle {
                                anchors.fill: parent
                                anchors.topMargin: 4
                                anchors.bottomMargin: 1
                                radius: 4
                                color: tab.hovered && !tab.current ? Qt.rgba(Ui.text.r, Ui.text.g, Ui.text.b, 0.06) : "transparent"
                                border.width: tab.visualFocus ? 2 : 0
                                border.color: Ui.accent
                            }
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width - 8
                                height: 2
                                radius: 1
                                visible: tab.current
                                color: Ui.accent
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Label {
                    visible: report.stale || cli.busy
                    text: cli.busy ? qsTr("checking…") : qsTr("changed since the last check")
                    color: Ui.subtle
                    rightPadding: Ui.gap
                    Layout.fillWidth: true
                    Layout.preferredWidth: implicitWidth
                    Layout.minimumWidth: 0
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
                Label {
                    visible: report.total > 0 && !report.stale && !cli.busy
                    text: report.source === "import" ? qsTr("conversion report") : qsTr("from yakbd %1").arg(report.source)
                    color: Ui.subtle
                    rightPadding: Ui.gap
                    // Gives way to the picker and Validate in a narrow window.
                    Layout.fillWidth: true
                    Layout.preferredWidth: implicitWidth
                    Layout.minimumWidth: 0
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
                // The target picker: what Validate checks and what Layout › Build builds.
                ComboBox {
                    id: targetPicker
                    objectName: "targetPicker"
                    visible: cli.targets.length > 1
                    flat: true
                    model: cli.targets
                    textRole: "label"
                    valueRole: "name"
                    currentIndex: cli.targets.findIndex(t => t.name === cli.target)
                    onActivated: function (i) {
                        cli.setTarget(cli.targets[i].name)
                        currentIndex = Qt.binding(() => cli.targets.findIndex(t => t.name === cli.target))
                    }
                    implicitContentWidthPolicy: ComboBox.WidestText
                    Layout.preferredHeight: 28
                    ToolTip.visible: hovered
                    ToolTip.delay: 700
                    ToolTip.text: qsTr("Target: what Validate checks and Layout › Build builds")
                    Accessible.name: qsTr("Target")
                }
                Button {
                    objectName: "validateButton"
                    text: qsTr("Validate")
                    flat: true
                    enabled: doc.loaded && cli.available && !cli.busy
                    onClicked: cli.validate()
                    Layout.rightMargin: Ui.gap
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex === 4 ? 1 : 0

            ListView {
                id: list
                objectName: "problemsList"
                clip: true
                model: report
                activeFocusOnTab: true
                keyNavigationEnabled: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                Keys.onReturnPressed: if (currentItem) currentItem.go()
                Keys.onEnterPressed: if (currentItem) currentItem.go()
                Accessible.name: qsTr("Report entries")

                delegate: ItemDelegate {
                    id: row
                    required property int index
                    required property string severity
                    required property string entryClass
                    required property string kind
                    required property string message
                    required property string location
                    required property string pointer
                    required property string detail
                    required property bool navigable
                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    function go() {
                        if (navigable)
                            doc.navigate(pointer)
                    }
                    onClicked: list.currentIndex = index
                    onDoubleClicked: go()
                    ToolTip.visible: hovered && detail !== ""
                    ToolTip.delay: 700
                    ToolTip.text: detail
                    Accessible.name: severity + ": " + message

                    contentItem: RowLayout {
                        spacing: Ui.gap
                        SeverityMark {
                            severity: row.severity
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Label {
                            text: row.severity
                            color: Ui.subtle
                            Layout.preferredWidth: 58
                        }
                        Label {
                            text: row.message
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Label {
                            text: row.kind
                            color: Ui.subtle
                            Layout.preferredWidth: 220
                            elide: Text.ElideMiddle
                        }
                        Label {
                            text: row.location
                            color: row.navigable ? Ui.link : Ui.subtle
                            font.underline: row.navigable && row.hovered
                            Layout.preferredWidth: 240
                            elide: Text.ElideMiddle
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    color: Ui.subtle
                    text: !cli.available ? qsTr("Checking needs the yakbd tool (Preferences).")
                        : report.total === 0 ? qsTr("Not checked yet. Validate (F7) runs yakbd on the open layout.")
                        : report.filter === "problems" ? qsTr("No problems. The layout builds for %1.").arg(cli.targetLabel(cli.reportTarget || "windows"))
                        : report.filter === "losses" ? qsTr("Nothing is lost.")
                        : qsTr("Nothing to show.")
                }
            }

            ScrollView {
                TextArea {
                    readOnly: true
                    text: cli.log
                    font.family: "monospace"
                    wrapMode: TextEdit.NoWrap
                    selectByMouse: true
                    placeholderText: qsTr("The commands the editor runs and what yakbd prints.")
                    Accessible.name: qsTr("Output of yakbd")
                }
            }
        }
    }
}
