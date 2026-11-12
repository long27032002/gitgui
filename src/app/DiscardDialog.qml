import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Hộp thoại xác nhận trước khi bỏ thay đổi. Luôn nhắc rằng có thể hoàn tác ngay sau đó.
Dialog {
    id: dlg
    property string message: ""
    property string confirmText: qsTr("Bỏ thay đổi")
    signal confirmed()

    title: qsTr("Bỏ thay đổi?")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 480

    contentItem: ColumnLayout {
        spacing: 10
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: dlg.message
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: theme.textDim
            font.pixelSize: 12
            text: qsTr("App sẽ sao lưu nội dung hiện tại trước khi bỏ; bạn có thể hoàn tác ngay sau đó bằng nút Hoàn tác trên thanh công cụ.")
        }
    }
    footer: DialogButtonBox {
        Button { text: dlg.confirmText; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        Button { text: qsTr("Huỷ"); highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
    }
    onAccepted: confirmed()
}
