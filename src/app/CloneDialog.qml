import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Clone repository từ URL, có thanh tiến độ và nút huỷ. Clone xong, repo được mở tự động.
Dialog {
    id: dlg
    property string parentDir: repo.homeDir
    property bool nameEdited: false

    title: qsTr("Clone repository")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 560
    closePolicy: cloner.running ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    onAboutToShow: {
        urlField.clear()
        nameField.clear()
        nameEdited = false
        cloner.clearError()
        urlField.forceActiveFocus()
    }

    FolderDialog {
        id: pickFolder
        title: qsTr("Chọn thư mục chứa bản clone")
        onAccepted: dlg.parentDir = repo.localPath(selectedFolder)
    }

    Connections {
        target: cloner
        function onFinished(path) { dlg.close() }
    }

    contentItem: ColumnLayout {
        spacing: 8

        Label { text: qsTr("URL của repository"); color: theme.textDim; font.pixelSize: 12 }
        TextField {
            id: urlField
            Layout.fillWidth: true
            enabled: !cloner.running
            selectByMouse: true
            placeholderText: qsTr("https://github.com/user/repo.git  hoặc  git@github.com:user/repo.git")
            onTextChanged: if (!dlg.nameEdited) nameField.text = cloner.suggestName(text)
        }

        Label { text: qsTr("Thư mục chứa bản clone"); color: theme.textDim; font.pixelSize: 12 }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                Layout.fillWidth: true
                enabled: !cloner.running
                selectByMouse: true
                text: dlg.parentDir
                onTextEdited: dlg.parentDir = text
            }
            Button { text: qsTr("Chọn…"); enabled: !cloner.running; onClicked: pickFolder.open() }
        }

        Label { text: qsTr("Tên thư mục"); color: theme.textDim; font.pixelSize: 12 }
        TextField {
            id: nameField
            Layout.fillWidth: true
            enabled: !cloner.running
            selectByMouse: true
            onTextEdited: dlg.nameEdited = true
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WrapAnywhere
            color: theme.textDim
            font.pixelSize: 12
            visible: nameField.text !== ""
            text: qsTr("Sẽ clone vào: %1/%2").arg(dlg.parentDir).arg(nameField.text)
        }

        ProgressBar {
            Layout.fillWidth: true
            visible: cloner.running
            indeterminate: cloner.percent < 0
            from: 0
            to: 100
            value: cloner.percent
        }
        Label {
            Layout.fillWidth: true
            visible: cloner.running
            text: cloner.stage + (cloner.percent >= 0 ? "  " + cloner.percent + "%" : "")
            color: theme.textDim
        }
        Label {
            Layout.fillWidth: true
            visible: cloner.errorMessage !== ""
            wrapMode: Text.Wrap
            color: theme.danger
            text: cloner.errorMessage
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: theme.textDim
            font.pixelSize: 12
            text: qsTr("Repo riêng tư qua SSH cần khoá đã nạp sẵn trong ssh-agent (ssh-add). Qua HTTPS cần credential helper của git.")
        }
    }

    footer: DialogButtonBox {
        Button {
            text: cloner.running ? qsTr("Đang clone…") : qsTr("Clone")
            highlighted: true
            enabled: !cloner.running && urlField.text.trim() !== "" && nameField.text.trim() !== ""
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: cloner.start(urlField.text, dlg.parentDir, nameField.text)
        }
        Button {
            text: cloner.running ? qsTr("Huỷ clone") : qsTr("Đóng")
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: cloner.running ? cloner.cancel() : dlg.close()
        }
    }
}
