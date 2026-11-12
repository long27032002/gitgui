import QtQuick
import QtQuick.Controls

// Hiển thị DiffModel theo kiểu GitHub Desktop: hai cột số dòng (cũ/mới), cột dấu +/−, nền xanh/đỏ,
// mã được tô màu cú pháp. Ở diff chọn được, cột số dòng của dòng ĐƯỢC ĐƯA VÀO COMMIT có nền xanh dương
// đậm (mặc định là tất cả); bấm vào cột số dòng (hoặc dấu +/−) để bỏ/chọn lại dòng đó, bấm vào đầu
// khối @@ để bỏ/chọn cả khối. Phát `rowToggled`.
ListView {
    id: view
    signal rowToggled(int row)
    // Bấm phải vào một hàng (dùng cho menu "Bỏ dòng/khối này").
    signal rowContextRequested(int row, int kind)

    readonly property int gutterWidth: 48
    readonly property int signWidth: 22
    readonly property int codeX: gutterWidth * 2 + signWidth
    readonly property int rowMinHeight: 21
    readonly property string mono: "monospace"

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    delegate: Item {
        id: row
        required property int index
        required property string text
        required property int kind          // 0 thông tin, 1 đầu khối @@, 2 thêm, 3 xoá, 4 ngữ cảnh
        required property bool selectable
        required property int checkState
        required property int oldNumber
        required property int newNumber
        required property string html

        readonly property bool isChange: kind === 2 || kind === 3
        readonly property bool selected: selectable && checkState === 2
        // Dòng chọn được mà đã bị loại khỏi commit: cột số dòng xám. Diff chỉ xem: tô nhạt theo +/−.
        readonly property color gutterBg: selected ? theme.gutterSelected
                                        : (selectable && isChange) ? theme.gutter
                                        : kind === 2 ? theme.gutterAdd
                                        : kind === 3 ? theme.gutterDel
                                        : theme.gutter
        readonly property color numberColor: selected ? theme.gutterSelectedText : theme.gutterText
        readonly property string codeText: (isChange || kind === 4) && text.length > 0 ? text.substring(1) : text

        width: ListView.view.width
        height: Math.max(code.implicitHeight + 3, view.rowMinHeight)

        // Nền phần mã
        Rectangle {
            x: view.codeX
            width: parent.width - view.codeX
            height: parent.height
            color: row.kind === 2 ? theme.diffAdd
                 : row.kind === 3 ? theme.diffDel
                 : row.kind === 1 ? theme.hunk
                 : theme.content
        }
        // Hai cột số dòng
        Rectangle {
            x: 0
            width: view.gutterWidth * 2 + view.signWidth
            height: parent.height
            color: row.kind === 1 ? theme.hunk : row.gutterBg
        }
        Label {
            x: 0
            width: view.gutterWidth - 8
            height: parent.height
            visible: row.oldNumber > 0
            text: row.oldNumber
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            color: row.numberColor
            font.family: view.mono
            font.pixelSize: 12
            font.bold: row.selected
        }
        Label {
            x: view.gutterWidth
            width: view.gutterWidth - 8
            height: parent.height
            visible: row.newNumber > 0
            text: row.newNumber
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            color: row.numberColor
            font.family: view.mono
            font.pixelSize: 12
            font.bold: row.selected
        }
        // Dấu +/− (hoặc "⋯" ở đầu khối)
        Label {
            x: view.gutterWidth * 2
            width: view.signWidth
            height: parent.height
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: row.kind === 2 ? "+" : row.kind === 3 ? "−" : ""
            color: row.numberColor
            font.family: view.mono
            font.pixelSize: 13
            font.bold: true
        }
        Label {  // biểu tượng mở rộng ở đầu khối
            visible: row.kind === 1
            x: 0
            width: view.gutterWidth * 2 + view.signWidth
            height: parent.height
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: row.selected ? "●" : row.checkState === 1 ? "◐" : (row.selectable ? "○" : "⋯")
            color: row.selected ? theme.gutterSelectedText : theme.gutterText
            font.pixelSize: 12
        }

        Label {
            id: code
            x: view.codeX + 6
            width: parent.width - view.codeX - 12
            anchors.verticalCenter: parent.verticalCenter
            text: row.html !== "" ? row.html : row.codeText
            textFormat: row.html !== "" ? Text.StyledText : Text.PlainText
            wrapMode: Text.WrapAnywhere
            color: row.kind === 1 || row.kind === 0 ? theme.textDim : theme.text
            font.family: view.mono
            font.pixelSize: 12
            font.italic: row.kind === 0
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: view.rowContextRequested(row.index, row.kind)
        }

        MouseArea {
            x: 0
            width: view.codeX
            height: parent.height
            enabled: row.selectable
            hoverEnabled: row.selectable
            cursorShape: row.selectable ? Qt.PointingHandCursor : Qt.ArrowCursor
            ToolTip.visible: containsMouse && row.selectable
            ToolTip.delay: 600
            ToolTip.text: row.kind === 1 ? qsTr("Bấm để chọn/bỏ cả khối này khỏi commit")
                          : row.selected ? qsTr("Bấm để loại dòng này khỏi commit")
                                         : qsTr("Bấm để đưa dòng này vào commit")
            onClicked: view.rowToggled(row.index)
        }
    }
}
