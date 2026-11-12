import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cột phải của tab "Xung đột": hoàn tất/huỷ thao tác, chi tiết file và các khối xung đột.
ColumnLayout {
    id: page
    readonly property var cf: repo.conflicts
    spacing: 0

    readonly property var titles: ({
        "merge":       { title: qsTr("Đang merge"),       cont: qsTr("Hoàn tất merge"),       abort: qsTr("Huỷ merge") },
        "rebase":      { title: qsTr("Đang rebase"),      cont: qsTr("Tiếp tục rebase"),      abort: qsTr("Huỷ rebase") },
        "cherry-pick": { title: qsTr("Đang cherry-pick"), cont: qsTr("Tiếp tục cherry-pick"), abort: qsTr("Huỷ cherry-pick") },
        "revert":      { title: qsTr("Đang revert"),      cont: qsTr("Tiếp tục revert"),      abort: qsTr("Huỷ revert") }
    })
    readonly property var info: titles[repo.operation]

    Dialog {
        id: abortDialog
        title: info ? info.abort + "?" : ""
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 440
        contentItem: Label {
            wrapMode: Text.Wrap
            text: qsTr("Mọi việc giải quyết xung đột đã làm sẽ bị bỏ và repo trở về trạng thái trước khi bắt đầu.")
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Huỷ bỏ thao tác"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Quay lại"); highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.abortOperation()
    }

    Dialog {
        id: markDialog
        title: qsTr("File vẫn còn dấu xung đột")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 440
        contentItem: Label {
            wrapMode: Text.Wrap
            text: qsTr("File này vẫn chứa các dòng <<<<<<< / ======= / >>>>>>>. Nếu đánh dấu đã giải quyết, các dòng đó sẽ được commit nguyên trạng.")
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Vẫn đánh dấu"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Quay lại"); highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: cf.markResolved()
    }

    // ---- Dải trạng thái + hoàn tất/huỷ ----
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: bannerRow.implicitHeight + 20
        color: Qt.rgba(0.85, 0.60, 0.15, 0.20)
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }

        RowLayout {
            id: bannerRow
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 12
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            spacing: 8

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.bold: true
                color: theme.text
                text: repo.operation === ""
                      ? qsTr("Có %1 file đang xung đột").arg(repo.conflictCount)
                      : repo.conflictCount > 0
                        ? qsTr("%1 — còn %2 file xung đột cần giải quyết").arg(info.title).arg(repo.conflictCount)
                        : qsTr("%1 — đã giải quyết xong mọi xung đột").arg(info.title)
            }
            Button {
                visible: repo.operation !== ""
                highlighted: repo.canContinueOperation
                enabled: repo.canContinueOperation && !repo.busy
                text: info ? info.cont : ""
                onClicked: repo.continueOperation()
            }
            Button {
                visible: repo.operation !== ""
                enabled: !repo.busy
                text: info ? info.abort : ""
                onClicked: abortDialog.open()
            }
        }
    }

    // ---- Chi tiết file đang chọn ----
    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: cf.selectedPath !== ""
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 14
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: cf.selectedPath
                color: theme.text
                font.bold: true
                font.pixelSize: 14
                elide: Text.ElideMiddle
                textFormat: Text.PlainText
            }
            Label {
                Layout.fillWidth: true
                text: cf.description
                color: theme.textDim
                wrapMode: Text.Wrap
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Button { text: qsTr("Mở bằng editor"); onClicked: cf.openInEditor() }
                Button { text: qsTr("Tải lại file"); enabled: !cf.loading; onClicked: cf.reload() }
                Button { text: cf.mineAction; enabled: !repo.busy; onClicked: cf.resolveWholeFile(true) }
                Button { text: cf.incomingAction; enabled: !repo.busy; onClicked: cf.resolveWholeFile(false) }
            }
        }

        // File không còn dấu xung đột (đã sửa tay) hoặc file nhị phân.
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            visible: !cf.hasBlocks && !cf.loading
            wrapMode: Text.Wrap
            color: theme.text
            text: cf.deleteConflict
                  ? qsTr("Một bên đã xoá file này. Dùng các nút phía trên để giữ file (theo bản có file) hoặc xoá file.")
                  : cf.binary
                  ? qsTr("Đây là file nhị phân nên không chọn được từng khối. Hãy dùng toàn bộ bản của tôi hoặc của họ, hoặc mở bằng ứng dụng phù hợp.")
                  : !cf.hasMarkers
                    ? qsTr("File không còn dấu xung đột. Nếu nội dung đã đúng, hãy đánh dấu là đã giải quyết.")
                    : qsTr("Không đọc được cấu trúc các khối xung đột của file này (dấu bị lồng hoặc thiếu). Hãy mở bằng editor để sửa tay, hoặc dùng toàn bộ một bên.")
        }
        Button {
            Layout.leftMargin: 14
            Layout.topMargin: 8
            visible: !cf.hasBlocks && !cf.loading && !cf.deleteConflict
            text: qsTr("Đánh dấu đã giải quyết")
            enabled: !repo.busy
            highlighted: !cf.hasMarkers
            onClicked: cf.hasMarkers ? markDialog.open() : cf.markResolved()
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: cf.loading
            visible: running
        }

        ListView {
            id: blockList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: cf.hasBlocks
            clip: true
            spacing: 10
            model: cf.blocks
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Frame {
                id: card
                required property int index
                required property int number
                required property string mine
                required property string incoming
                required property string before
                required property string after
                required property string mineLabel
                required property string incomingLabel
                required property int choice

                width: ListView.view.width - 28
                x: 14
                background: Rectangle { color: theme.expander; border.color: theme.border; radius: 4 }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Xung đột #%1").arg(card.number)
                            color: theme.text
                            font.bold: true
                        }
                        Button {
                            text: qsTr("Giữ bản của tôi")
                            highlighted: card.choice === 1
                            onClicked: cf.choose(card.index, card.choice === 1 ? 0 : 1)
                        }
                        Button {
                            text: qsTr("Giữ bản của họ")
                            highlighted: card.choice === 2
                            onClicked: cf.choose(card.index, card.choice === 2 ? 0 : 2)
                        }
                        Button {
                            text: qsTr("Giữ cả hai")
                            highlighted: card.choice === 3
                            onClicked: cf.choose(card.index, card.choice === 3 ? 0 : 3)
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: card.before !== ""
                        text: card.before
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        font.family: "monospace"
                        color: theme.textDim
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        // Bên trái: bản của tôi; bên phải: bản của họ.
                        Repeater {
                            model: [
                                { title: qsTr("Bản của tôi"), label: card.mineLabel, text: card.mine, tint: theme.diffAdd, picked: card.choice === 1 || card.choice === 3 },
                                { title: qsTr("Bản của họ"), label: card.incomingLabel, text: card.incoming, tint: Qt.rgba(0.25, 0.50, 0.85, 0.20), picked: card.choice === 2 || card.choice === 3 }
                            ]
                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                Layout.alignment: Qt.AlignTop
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.title + (modelData.label !== "" ? "  (" + modelData.label + ")" : "")
                                          + (modelData.picked ? "  ✓" : "")
                                    color: theme.text
                                    font.bold: modelData.picked
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: panelText.implicitHeight + 10
                                    color: modelData.tint
                                    border.width: modelData.picked ? 2 : 0
                                    border.color: theme.accent
                                    radius: 3
                                    Label {
                                        id: panelText
                                        x: 6
                                        y: 5
                                        width: parent.width - 12
                                        text: modelData.text === "" ? qsTr("(trống)") : modelData.text
                                        textFormat: Text.PlainText
                                        wrapMode: Text.WrapAnywhere
                                        font.family: "monospace"
                                        color: theme.text
                                        opacity: modelData.text === "" ? 0.5 : 1.0
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: card.after !== ""
                        text: card.after
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        font.family: "monospace"
                        color: theme.textDim
                    }
                }
            }
        }

        // Thanh lưu kết quả của các khối.
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            visible: cf.hasBlocks
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: qsTr("Đã chọn %1/%2 khối").arg(cf.chosenCount).arg(cf.blockCount)
                color: theme.textDim
            }
            Button {
                highlighted: true
                enabled: cf.allChosen && !repo.busy
                text: qsTr("Lưu kết quả và đánh dấu đã giải quyết")
                onClicked: cf.applyChoices()
            }
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: cf.selectedPath === ""
        Label {
            anchors.centerIn: parent
            text: repo.conflictCount === 0 ? qsTr("Không còn xung đột nào cần giải quyết") : qsTr("Chọn một file để giải quyết")
            color: theme.textDim
            font.pixelSize: 14
        }
    }
}
