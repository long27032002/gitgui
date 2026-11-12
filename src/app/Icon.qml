import QtQuick

// Biểu tượng vẽ bằng Canvas (nét đơn sắc), cùng phong cách với thanh công cụ của GitHub Desktop.
Canvas {
    id: icon
    property string kind: ""
    property color color: theme.text
    property real lineWidth: 1.5

    implicitWidth: 16
    implicitHeight: 16
    onColorChanged: requestPaint()
    onKindChanged: requestPaint()
    onWidthChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)
        ctx.strokeStyle = color
        ctx.fillStyle = color
        ctx.lineWidth = lineWidth * width / 16
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        var s = width / 16
        function P(x, y) { return [x * s, y * s] }
        function line(pts) {
            ctx.beginPath()
            ctx.moveTo(pts[0][0] * s, pts[0][1] * s)
            for (var i = 1; i < pts.length; ++i)
                ctx.lineTo(pts[i][0] * s, pts[i][1] * s)
            ctx.stroke()
        }
        function circle(x, y, r, fill) {
            ctx.beginPath()
            ctx.arc(x * s, y * s, r * s, 0, Math.PI * 2)
            if (fill) ctx.fill(); else ctx.stroke()
        }
        switch (kind) {
        case "repo":
            ctx.strokeRect(3.5 * s, 1.5 * s, 9 * s, 13 * s)
            line([[3.5, 11], [12.5, 11]])
            line([[6, 13.2], [6, 14.5], [7.2, 13.7], [8.4, 14.5], [8.4, 13.2]])
            break
        case "branch":
            circle(4.5, 3.2, 1.8, false)
            circle(4.5, 12.8, 1.8, false)
            circle(11.5, 5.2, 1.8, false)
            line([[4.5, 5], [4.5, 11]])
            ctx.beginPath()
            ctx.moveTo(11.5 * s, 7 * s)
            ctx.quadraticCurveTo(11.5 * s, 9.6 * s, 4.5 * s, 9.6 * s)
            ctx.stroke()
            break
        case "arrowDown":
            line([[8, 2.5], [8, 13]])
            line([[3.8, 9], [8, 13.2], [12.2, 9]])
            break
        case "arrowUp":
            line([[8, 13.5], [8, 3]])
            line([[3.8, 7], [8, 2.8], [12.2, 7]])
            break
        case "sync":
            ctx.beginPath()
            ctx.arc(8 * s, 8 * s, 5 * s, Math.PI * 1.15, Math.PI * 0.2, false)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(8 * s, 8 * s, 5 * s, Math.PI * 0.15, Math.PI * 1.2, false)
            ctx.stroke()
            line([[10.6, 1.8], [12.8, 4.4], [9.6, 5.4]])
            line([[5.4, 14.2], [3.2, 11.6], [6.4, 10.6]])
            break
        case "chevronDown":
            line([[3.5, 6], [8, 10.5], [12.5, 6]])
            break
        case "chevronRight":
            line([[6, 3.5], [10.5, 8], [6, 12.5]])
            break
        case "stash":
            ctx.strokeRect(2.5 * s, 4.5 * s, 11 * s, 9 * s)
            line([[4.5, 2.5], [11.5, 2.5]])
            line([[2.5, 8], [13.5, 8]])
            break
        case "person":
            circle(7, 5, 2.4, false)
            ctx.beginPath()
            ctx.arc(7 * s, 14 * s, 5 * s, Math.PI * 1.15, Math.PI * 1.85, false)
            ctx.stroke()
            line([[12.5, 3], [12.5, 8]])
            line([[10, 5.5], [15, 5.5]])
            break
        case "close":
            line([[4, 4], [12, 12]])
            line([[12, 4], [4, 12]])
            break
        }
    }
}
