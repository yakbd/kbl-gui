import QtQuick
import QtQuick.Controls

// The physical keyboard for one layer: each key shows what it types there. Arrow keys move the selection,
// Enter edits the selected key.
FocusScope {
    id: view
    property int layerIndex: 0
    property string selected: ""
    readonly property var rows: (doc.revision, doc.keyboardRows(layerIndex))
    readonly property real maxUnits: {
        let m = 1
        for (const r of rows) {
            let w = 0
            for (const k of r)
                w += k.width
            m = Math.max(m, w + (r.length - 1) * 0.08)
        }
        return m
    }
    readonly property real unit: Math.max(20, Math.min(64, (width - 2 * Ui.pad) / maxUnits))
    signal editRequested()

    implicitHeight: column.implicitHeight + 2 * Ui.pad
    activeFocusOnTab: true
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Keyboard")

    function locate(id) {
        for (let r = 0; r < rows.length; ++r)
            for (let c = 0; c < rows[r].length; ++c)
                if (rows[r][c].id === id)
                    return { row: r, col: c }
        return null
    }
    function centre(r, c) {
        let x = 0
        for (let i = 0; i < c; ++i)
            x += rows[r][i].width + 0.08
        return x + rows[r][c].width / 2
    }
    function move(dr, dc) {
        if (rows.length === 0)
            return
        let at = locate(selected)
        if (!at) {
            selected = rows[Math.min(1, rows.length - 1)][0].id
            return
        }
        if (dc !== 0) {
            const c = Math.max(0, Math.min(rows[at.row].length - 1, at.col + dc))
            selected = rows[at.row][c].id
            return
        }
        const r = Math.max(0, Math.min(rows.length - 1, at.row + dr))
        if (r === at.row)
            return
        const x = centre(at.row, at.col)
        let best = 0, bestD = 1e9
        for (let c = 0; c < rows[r].length; ++c) {
            const d = Math.abs(centre(r, c) - x)
            if (d < bestD) {
                bestD = d
                best = c
            }
        }
        selected = rows[r][best].id
    }

    Keys.onLeftPressed: move(0, -1)
    Keys.onRightPressed: move(0, 1)
    Keys.onUpPressed: move(-1, 0)
    Keys.onDownPressed: move(1, 0)
    Keys.onReturnPressed: editRequested()
    Keys.onEnterPressed: editRequested()

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: Ui.deck
        border.color: view.activeFocus ? Ui.accent : "transparent"
        border.width: view.activeFocus ? 2 : 0
    }

    Column {
        id: column
        x: Ui.pad
        y: Ui.pad
        spacing: view.unit * 0.08
        Repeater {
            model: view.rows
            delegate: Row {
                id: rowItem
                required property var modelData
                required property int index
                spacing: view.unit * 0.08
                topPadding: index === 1 ? view.unit * 0.18 : 0
                Repeater {
                    model: rowItem.modelData
                    delegate: KeyCap {
                        required property var modelData
                        unit: view.unit
                        keyData: modelData
                        selected: modelData.id === view.selected
                        onClicked: {
                            view.selected = modelData.id
                            view.forceActiveFocus()
                        }
                        onDoubleClicked: {
                            view.selected = modelData.id
                            view.editRequested()
                        }
                    }
                }
            }
        }
    }
}
