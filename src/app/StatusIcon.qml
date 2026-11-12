import QtQuick
import QtQuick.Controls

// Ô vuông viền màu với ký hiệu bên trong, giống biểu tượng trạng thái file của GitHub Desktop.
Rectangle {
    id: root
    property string status: "M"

    readonly property color tint: {
        switch (status) {
        case "A":
        case "?": return theme.added
        case "D": return theme.deleted
        case "R": return theme.renamed
        case "U": return theme.conflict
        default:  return theme.modified
        }
    }

    implicitWidth: 16
    implicitHeight: 16
    radius: 3
    color: "transparent"
    border.width: 1.5
    border.color: tint

    // Sửa đổi: chấm tròn ở giữa.
    Rectangle {
        visible: root.status === "M" || root.status === "T" || root.status === "C"
        anchors.centerIn: parent
        width: 5; height: 5; radius: 2.5
        color: root.tint
    }
    // Thêm mới / chưa theo dõi: dấu cộng.
    Item {
        visible: root.status === "A" || root.status === "?"
        anchors.centerIn: parent
        width: 8; height: 8
        Rectangle { anchors.centerIn: parent; width: 8; height: 1.5; color: root.tint }
        Rectangle { anchors.centerIn: parent; width: 1.5; height: 8; color: root.tint }
    }
    // Xoá: dấu trừ.
    Rectangle {
        visible: root.status === "D"
        anchors.centerIn: parent
        width: 8; height: 1.5
        color: root.tint
    }
    // Đổi tên và conflict: ký tự.
    Label {
        visible: root.status === "R" || root.status === "U"
        anchors.centerIn: parent
        text: root.status === "R" ? "→" : "!"
        color: root.tint
        font.bold: true
        font.pixelSize: 10
    }
}
