import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Tạo repository mới: tên, nơi lưu, nhánh đầu tiên, README và .gitignore (tuỳ chọn).
Dialog {
    id: dlg
    property string parentDir: repo.homeDir

    title: qsTr("Tạo repository mới")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 540

    onAboutToShow: {
        nameField.clear()
        branchField.text = "main"
        readmeBox.checked = true
        ignoreBox.currentIndex = 0
        repo.clearError()
        nameField.forceActiveFocus()
    }

    FolderDialog {
        id: pickFolder
        title: qsTr("Chọn thư mục chứa repository")
        onAccepted: dlg.parentDir = repo.localPath(selectedFolder)
    }

    // Tạo xong (không lỗi) thì đóng; repo được mở tự động.
    Connections {
        target: repo
        function onCreatingChanged() {
            if (!repo.creating && dlg.visible && repo.errorMessage === "")
                dlg.close()
        }
    }

    contentItem: ColumnLayout {
        spacing: 8

        Label { text: qsTr("Tên"); color: theme.textDim; font.pixelSize: 12 }
        TextField {
            id: nameField
            Layout.fillWidth: true
            selectByMouse: true
            placeholderText: qsTr("ten-du-an")
        }

        Label { text: qsTr("Thư mục chứa"); color: theme.textDim; font.pixelSize: 12 }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                Layout.fillWidth: true
                selectByMouse: true
                text: dlg.parentDir
                onTextEdited: dlg.parentDir = text
            }
            Button { text: qsTr("Chọn…"); onClicked: pickFolder.open() }
        }

        Label { text: qsTr("Nhánh đầu tiên"); color: theme.textDim; font.pixelSize: 12 }
        TextField { id: branchField; Layout.fillWidth: true; selectByMouse: true }

        CheckBox {
            id: readmeBox
            checked: true
            text: qsTr("Tạo file README.md")
        }

        Label { text: qsTr("Mẫu .gitignore"); color: theme.textDim; font.pixelSize: 12 }
        ComboBox {
            id: ignoreBox
            Layout.fillWidth: true
            model: [qsTr("(Không tạo)")].concat(repo.gitignoreTemplates)
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: theme.textDim
            font.pixelSize: 12
            text: qsTr("Nếu tạo README hoặc .gitignore, app sẽ tự tạo commit \"Initial commit\" (cần đã cấu hình user.name và user.email cho git).")
        }
        Label {
            Layout.fillWidth: true
            visible: repo.errorMessage !== ""
            wrapMode: Text.Wrap
            color: theme.danger
            text: repo.errorMessage
        }
    }

    footer: DialogButtonBox {
        Button {
            text: repo.creating ? qsTr("Đang tạo…") : qsTr("Tạo repository")
            highlighted: true
            enabled: !repo.creating && nameField.text.trim() !== ""
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: repo.createRepository(dlg.parentDir, nameField.text, branchField.text, readmeBox.checked,
                                             ignoreBox.currentIndex > 0 ? ignoreBox.currentText : "")
        }
        Button {
            text: qsTr("Đóng")
            enabled: !repo.creating
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
