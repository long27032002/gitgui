import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cột trái của tab "Thay đổi": chọn tất cả, danh sách file, thay đổi đã cất, khung commit.
ColumnLayout {
    id: side
    spacing: 0
    property bool stashOpen: false
    property bool coAuthorsOpen: false
    // Bản nháp được giữ lại khi bật "sửa commit gần nhất" để khôi phục nếu bỏ chọn.
    property string draftSummary: ""
    property string draftDescription: ""
    property string draftCoAuthors: ""

    // Menu bấm phải trên một file: bỏ thay đổi, bỏ qua (.gitignore)
    Menu {
        id: fileMenu
        property string targetPath: ""
        property string targetStatus: ""
        readonly property string ext: targetPath.lastIndexOf(".") > targetPath.lastIndexOf("/") + 1
                                      ? targetPath.substring(targetPath.lastIndexOf(".") + 1) : ""
        readonly property bool untracked: targetStatus === "?"
        readonly property bool inFolder: targetPath.indexOf("/") > 0

        MenuItem {
            text: qsTr("Bỏ thay đổi…")
            enabled: fileMenu.targetStatus !== "U"
            onTriggered: {
                discardDialog.paths = [fileMenu.targetPath]
                discardDialog.message = fileMenu.untracked
                    ? qsTr("Xoá file mới \"%1\"?").arg(fileMenu.targetPath)
                    : qsTr("Bỏ mọi thay đổi của file \"%1\" và đưa nó về như ở commit gần nhất?").arg(fileMenu.targetPath)
                discardDialog.open()
            }
        }
        MenuSeparator { visible: fileMenu.untracked; height: visible ? implicitHeight : 0 }
        MenuItem {
            text: qsTr("Bỏ qua file này (.gitignore)")
            visible: fileMenu.untracked
            height: visible ? implicitHeight : 0
            onTriggered: repo.ignoreFile(fileMenu.targetPath)
        }
        MenuItem {
            text: qsTr("Bỏ qua mọi file *.%1 (.gitignore)").arg(fileMenu.ext)
            visible: fileMenu.untracked && fileMenu.ext !== ""
            height: visible ? implicitHeight : 0
            onTriggered: repo.ignoreExtension(fileMenu.targetPath)
        }
        MenuItem {
            text: qsTr("Bỏ qua thư mục chứa file này (.gitignore)")
            visible: fileMenu.untracked && fileMenu.inFolder
            height: visible ? implicitHeight : 0
            onTriggered: repo.ignoreFolder(fileMenu.targetPath)
        }
    }
    DiscardDialog {
        id: discardDialog
        property var paths: []
        onConfirmed: repo.discardFiles(paths)
    }

    Dialog {
        id: discardSavedDialog
        title: qsTr("Xoá thay đổi đã cất tạm?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 420
        contentItem: Label {
            wrapMode: Text.Wrap
            text: qsTr("Các thay đổi đã cất khi bạn rời nhánh này sẽ bị xoá vĩnh viễn và không thể khôi phục.")
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Xoá"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.discardSavedChanges()
    }

    // ---- Dòng "N file thay đổi" kèm ô chọn tất cả ----
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 40
        color: theme.sidebar
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10
            GhCheckBox {
                checkState: repo.selectAllState
                enabled: repo.changeCount > 0 && !repo.busy
                onClicked: repo.toggleAll()
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: repo.changeCount === 1 ? qsTr("1 file thay đổi") : qsTr("%1 file thay đổi").arg(repo.changeCount)
                color: theme.text
                font.pixelSize: 13
            }
            Item { Layout.preferredWidth: 18 }  // cân đối với ô tích bên trái
        }
    }

    // ---- Danh sách file ----
    ListView {
        id: fileList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: repo.changes
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        delegate: ItemDelegate {
            id: row
            width: ListView.view.width
            height: 36
            padding: 0
            highlighted: repo.selectedPath === model.path
            onClicked: repo.selectRow(index)
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: {
                    fileMenu.targetPath = model.path
                    fileMenu.targetStatus = model.status
                    fileMenu.popup()
                }
            }

            function esc(t) { return t.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;") }
            readonly property int slash: model.path.lastIndexOf("/")
            readonly property string fileName: slash >= 0 ? model.path.substring(slash + 1) : model.path
            readonly property string dirName: slash >= 0 ? model.path.substring(0, slash + 1) : ""

            background: Rectangle {
                color: row.highlighted ? theme.rowSelected : row.hovered ? theme.rowHover : "transparent"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }
            }
            contentItem: RowLayout {
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10
                GhCheckBox {
                    Layout.leftMargin: 12
                    checkState: model.checkState
                    enabled: !repo.busy && model.status !== "U"
                    onClicked: repo.toggleChecked(index)
                }
                Label {
                    Layout.fillWidth: true
                    textFormat: Text.StyledText
                    elide: Text.ElideMiddle
                    color: theme.text
                    font.pixelSize: 13
                    text: "<font color=\"" + theme.textDim + "\">" + row.esc(row.dirName) + "</font>" + row.esc(row.fileName)
                }
                StatusIcon {
                    Layout.rightMargin: 12
                    status: model.status
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: fileList.count === 0 && !repo.busy
            text: qsTr("Không có thay đổi nào")
            color: theme.textDim
        }
    }

    // ---- Thay đổi đã cất tạm ----
    Rectangle {
        Layout.fillWidth: true
        visible: repo.hasSavedChanges
        implicitHeight: stashCol.implicitHeight
        color: theme.sidebar
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: theme.border }

        ColumnLayout {
            id: stashCol
            width: parent.width
            spacing: 0
            ItemDelegate {
                Layout.fillWidth: true
                implicitHeight: 40
                padding: 0
                background: Rectangle { color: parent.hovered ? theme.rowHover : "transparent" }
                onClicked: side.stashOpen = !side.stashOpen
                contentItem: RowLayout {
                    spacing: 10
                    Icon { Layout.leftMargin: 12; kind: "stash"; color: theme.modified }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Thay đổi đã cất tạm")
                        color: theme.text
                        font.bold: true
                        font.pixelSize: 13
                    }
                    Icon { Layout.rightMargin: 12; kind: side.stashOpen ? "chevronDown" : "chevronRight"; color: theme.text }
                }
            }
            RowLayout {
                visible: side.stashOpen
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.bottomMargin: 10
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: theme.textDim
                    font.pixelSize: 12
                    text: qsTr("Cất khi bạn rời nhánh này.")
                }
                Button {
                    text: qsTr("Khôi phục")
                    highlighted: true
                    enabled: !repo.busy
                    onClicked: repo.restoreSavedChanges()
                }
                Button {
                    text: qsTr("Xoá")
                    enabled: !repo.busy
                    onClicked: discardSavedDialog.open()
                }
            }
        }
    }

    // ---- Khung commit ----
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: commitCol.implicitHeight + 24
        color: theme.sidebar
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: theme.border }

        ColumnLayout {
            id: commitCol
            x: 12
            y: 12
            width: parent.width - 24
            spacing: 10

            // Commit gần nhất (chưa push) có thể hoàn tác nhanh.
            RowLayout {
                Layout.fillWidth: true
                visible: repo.canUndoCommit
                spacing: 6
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    color: theme.textDim
                    font.pixelSize: 12
                    textFormat: Text.PlainText
                    text: qsTr("Commit gần nhất: %1").arg(repo.lastCommitSubject)
                }
                Button {
                    text: qsTr("Hoàn tác")
                    flat: true
                    enabled: !repo.busy
                    onClicked: repo.undoLastCommit()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Bỏ commit này nhưng giữ nguyên thay đổi để sửa lại")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Rectangle {  // ảnh đại diện: chữ cái đầu của user.name
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: 16
                    color: theme.badge
                    Label {
                        anchors.centerIn: parent
                        text: repo.userName !== "" ? repo.userName.charAt(0).toUpperCase() : "?"
                        color: theme.text
                        font.bold: true
                        font.pixelSize: 14
                    }
                }
                TextField {
                    id: summaryField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Tóm tắt (bắt buộc)")
                    implicitHeight: 34
                    leftPadding: 10
                    color: theme.text
                    placeholderTextColor: theme.textDim
                    selectByMouse: true
                    background: Rectangle {
                        radius: 4
                        color: theme.input
                        border.width: 1
                        border.color: summaryField.activeFocus ? theme.accent : theme.inputBorder
                    }
                }
            }

            // Mô tả + biểu tượng co-author
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 112
                radius: 4
                color: theme.input
                border.width: 1
                border.color: descriptionField.activeFocus ? theme.accent : theme.inputBorder

                ScrollView {
                    anchors.fill: parent
                    anchors.bottomMargin: 30
                    clip: true
                    TextArea {
                        id: descriptionField
                        placeholderText: qsTr("Mô tả")
                        placeholderTextColor: theme.textDim
                        color: theme.text
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        background: null
                    }
                }
                ToolButton {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 2
                    width: 30
                    height: 28
                    background: Rectangle { color: parent.hovered ? theme.rowHover : "transparent"; radius: 4 }
                    contentItem: Icon {
                        kind: "person"
                        color: side.coAuthorsOpen ? theme.accent : theme.button
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Thêm đồng tác giả (Co-authored-by)")
                    onClicked: {
                        side.coAuthorsOpen = !side.coAuthorsOpen
                        if (side.coAuthorsOpen)
                            coAuthorsField.forceActiveFocus()
                    }
                }
            }

            // Dòng đồng tác giả
            Rectangle {
                visible: side.coAuthorsOpen
                Layout.fillWidth: true
                implicitHeight: 34
                radius: 4
                color: theme.input
                border.width: 1
                border.color: coAuthorsField.activeFocus ? theme.accent : theme.inputBorder
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 8
                    Label { text: qsTr("Đồng tác giả"); color: theme.textDim; font.pixelSize: 12 }
                    TextField {
                        id: coAuthorsField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Tên <email>, Tên khác <email>")
                        placeholderTextColor: theme.textDim
                        color: theme.text
                        selectByMouse: true
                        background: null
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: repo.canAmend
                spacing: 8
                GhCheckBox {
                    id: amendBox
                    tristate: false
                    // GhCheckBox mặc định giữ nguyên trạng thái; ô này là bật/tắt thường.
                    nextCheckState: function() { return amendBox.checkState === Qt.Checked ? Qt.Unchecked : Qt.Checked }
                onToggled: {
                    if (checked) {
                        side.draftSummary = summaryField.text
                        side.draftDescription = descriptionField.text
                        side.draftCoAuthors = coAuthorsField.text
                        const p = repo.lastCommitParts()
                        summaryField.text = p.subject
                        descriptionField.text = p.body
                        coAuthorsField.text = p.coAuthors
                        side.coAuthorsOpen = p.coAuthors !== ""
                    } else {
                        summaryField.text = side.draftSummary
                        descriptionField.text = side.draftDescription
                        coAuthorsField.text = side.draftCoAuthors
                    }
                }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Sửa (amend) commit gần nhất")
                    color: theme.text
                    font.pixelSize: 13
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: amendBox.toggle()
                    }
                }
            }

            Button {
                id: commitButton
                Layout.fillWidth: true
                implicitHeight: 34
                enabled: (repo.includedCount > 0 || amendBox.checked) && summaryField.text.trim() !== "" && !repo.busy
                         && repo.operation === "" && repo.conflictCount === 0
                background: Rectangle {
                    radius: 4
                    color: !commitButton.enabled ? theme.badge
                         : commitButton.down ? theme.toolbarPressed
                         : commitButton.hovered ? theme.buttonHover : theme.button
                }
                contentItem: Label {
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    textFormat: Text.RichText
                    elide: Text.ElideRight
                    color: commitButton.enabled ? theme.onAccent : theme.textDim
                    font.pixelSize: 13
                    text: amendBox.checked ? qsTr("Sửa commit <b>%1</b>").arg(repo.lastCommitSubject.replace(/&/g, "&amp;").replace(/</g, "&lt;"))
                                           : qsTr("Commit vào <b>%1</b>").arg(repo.branch.replace(/&/g, "&amp;").replace(/</g, "&lt;"))
                }
                ToolTip.visible: hovered && (repo.operation !== "" || repo.conflictCount > 0)
                ToolTip.text: qsTr("Đang có merge/rebase hoặc xung đột chưa xong — hoàn tất ở tab Xung đột trước.")
                onClicked: repo.commit(summaryField.text, descriptionField.text,
                                       side.coAuthorsOpen ? coAuthorsField.text : "", amendBox.checked)
            }
        }
    }

    Connections {
        target: repo
        function onCommitted() {
            summaryField.clear()
            descriptionField.clear()
            coAuthorsField.clear()
            amendBox.checked = false
        }
        // Vừa hoàn tác commit: điền lại message cũ để người dùng sửa và commit lại.
        function onCommitUndone(subject, body, coAuthors) {
            summaryField.text = subject
            descriptionField.text = body
            coAuthorsField.text = coAuthors
            side.coAuthorsOpen = coAuthors !== ""
        }
        function onStatusChanged() {
            if (!repo.canAmend && amendBox.checked)
                amendBox.checked = false  // commit đã được push/đổi: không còn sửa được
        }
    }
}
