import QtQuick

// Severity as shape and colour: error = filled circle, warning = triangle, other = hollow circle.
Item {
    property string severity: "info"
    implicitWidth: 12
    implicitHeight: 12
    readonly property color tint: severity === "error" ? Ui.error : severity === "warning" ? Ui.warning
                                  : severity === "accept" ? Ui.subtle : Ui.info
    Rectangle {
        anchors.fill: parent
        visible: parent.severity !== "warning"
        radius: width / 2
        color: parent.severity === "error" ? parent.tint : "transparent"
        border.color: parent.tint
        border.width: 2
    }
    Canvas {
        anchors.fill: parent
        visible: parent.severity === "warning"
        onPaint: {
            const c = getContext("2d")
            c.reset()
            c.fillStyle = parent.tint
            c.beginPath()
            c.moveTo(width / 2, 0)
            c.lineTo(width, height)
            c.lineTo(0, height)
            c.closePath()
            c.fill()
        }
        Component.onCompleted: requestPaint()
    }
}
