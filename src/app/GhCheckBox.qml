import QtQuick
import QtQuick.Controls

// Ô tích kiểu GitHub Desktop: nền xanh nhạt khi bật, gạch ngang khi chọn một phần.
CheckBox {
    id: cb
    tristate: true
    padding: 0
    spacing: 0
    implicitWidth: 18
    implicitHeight: 18
    // Trạng thái do git quyết định: bấm chỉ phát tín hiệu, không tự đổi.
    nextCheckState: function() { return cb.checkState }

    contentItem: Item { implicitWidth: 0; implicitHeight: 0 }
    indicator: Rectangle {
        implicitWidth: 16
        implicitHeight: 16
        x: (cb.width - width) / 2
        y: (cb.height - height) / 2
        radius: 3
        color: cb.checkState === Qt.Unchecked ? "transparent" : theme.accent
        border.width: 1
        border.color: cb.checkState === Qt.Unchecked ? theme.inputBorder : theme.accent
        opacity: cb.enabled ? 1.0 : 0.45

        Label {
            visible: cb.checkState === Qt.Checked
            anchors.centerIn: parent
            text: "✓"
            color: theme.window
            font.bold: true
            font.pixelSize: 11
        }
        Rectangle {
            visible: cb.checkState === Qt.PartiallyChecked
            anchors.centerIn: parent
            width: 8; height: 2
            color: theme.window
        }
    }
}
