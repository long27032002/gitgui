import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cột phải của tab "Thay đổi": tên file, chế độ xem (chưa stage / đã stage), hành động và diff.
ColumnLayout {
    id: detail
    spacing: 0

    function esc(t) { return t.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;") }
    readonly property int slash: repo.selectedPath.lastIndexOf("/")
    readonly property string dirName: slash >= 0 ? repo.selectedPath.substring(0, slash + 1) : ""
    readonly property string fileName: slash >= 0 ? repo.selectedPath.substring(slash + 1) : repo.selectedPath

    // Tiêu đề: đường dẫn (thư mục mờ, tên file sáng) + các nút
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 40
        color: theme.sidebar
        visible: repo.selectedPath !== ""
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 12
            spacing: 8

            Label {
                Layout.fillWidth: true
                textFormat: Text.StyledText
                elide: Text.ElideMiddle
                color: theme.text
                font.pixelSize: 13
                text: "<font color=\"" + theme.textDim + "\">" + detail.esc(detail.dirName) + "</font>" + detail.esc(detail.fileName)
            }

            StatusIcon { status: repo.selectedStatus }
        }
    }

    // Menu bấm phải: bỏ một dòng hoặc cả khối (chỉ khi diff chọn được và file chưa bị xoá).
    Menu {
        id: diffMenu
        property int row: -1
        property int kind: 0
        MenuItem {
            text: qsTr("Bỏ dòng này…")
            visible: diffMenu.kind === 2 || diffMenu.kind === 3
            height: visible ? implicitHeight : 0
            onTriggered: { lineDialog.message = qsTr("Bỏ dòng thay đổi này khỏi file \"%1\"?").arg(repo.selectedPath); lineDialog.open() }
        }
        MenuItem {
            text: qsTr("Bỏ cả khối này…")
            onTriggered: { lineDialog.message = qsTr("Bỏ toàn bộ khối thay đổi này khỏi file \"%1\"?").arg(repo.selectedPath); lineDialog.open() }
        }
    }
    DiscardDialog {
        id: lineDialog
        confirmText: qsTr("Bỏ")
        onConfirmed: repo.discardDiffRow(diffMenu.row)
    }

    DiffView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: repo.selectedPath !== ""
        model: repo.diff
        onRowToggled: function(row) { repo.toggleDiffRow(row) }
        onRowContextRequested: function(row, kind) {
            if (!repo.diffSelectable || repo.selectedStatus === "D" || (kind !== 1 && kind !== 2 && kind !== 3))
                return
            diffMenu.row = row
            diffMenu.kind = kind
            diffMenu.popup()
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: repo.selectedPath === ""
        Label {
            anchors.centerIn: parent
            text: repo.changeCount === 0 ? qsTr("Không có thay đổi nào ở nhánh này") : qsTr("Chọn một file để xem thay đổi")
            color: theme.textDim
            font.pixelSize: 14
        }
    }
}
