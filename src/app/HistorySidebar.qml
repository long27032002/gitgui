import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cột trái của tab "Lịch sử": danh sách commit; Shift+bấm chọn đoạn; bấm phải mở menu.
ColumnLayout {
    id: side
    readonly property var hist: repo.history
    spacing: 0

    TextEdit { id: clipboardHelper; visible: false }

    // Chỉ dùng cho smoke test.
    function smokeOpen() {
        squashDialog.open()
        branchFromCommitDialog.open()
        chooseBranchFromCommitDialog.open()
        commitMenu.popup()
    }

    function openCommitMenu(hash, shortHash) {
        commitMenu.hash = hash
        commitMenu.shortHash = shortHash
        commitMenu.popup()
    }

    property string branchCommitHash: ""
    property string pendingBranchCommitName: ""

    function requestBranchFromCommit(name, hash, stashChanges) {
        if (repo.changeCount > 0 && stashChanges === undefined) {
            pendingBranchCommitName = name
            branchCommitHash = hash
            chooseBranchFromCommitDialog.open()
        } else {
            repo.createBranchFromCommit(name, hash, stashChanges === true)
        }
    }

    Dialog {
        id: squashDialog
        title: qsTr("Gom commit")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 540
        readonly property var plan: repo.squashPlan
        readonly property bool loaded: plan.loaded === true
        readonly property bool valid: loaded && plan.ok === true
        onAboutToShow: repo.prepareSquash()
        onPlanChanged: if (valid) messageField.text = plan.message

        contentItem: ColumnLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: !squashDialog.loaded ? qsTr("Đang kiểm tra…")
                      : !squashDialog.valid ? squashDialog.plan.error
                      : qsTr("Gom %1 commit đã chọn thành một commit duy nhất. Nội dung cuối cùng của dự án không đổi.")
                        .arg(squashDialog.plan.count)
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 130
                visible: squashDialog.valid
                TextArea {
                    id: messageField
                    wrapMode: TextEdit.Wrap
                    placeholderText: qsTr("Message cho commit sau khi gom")
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                visible: squashDialog.valid && squashDialog.plan.pushed === true
                color: theme.warning
                font.bold: true
                text: qsTr("Các commit này đã được push. Gom chúng sẽ viết lại lịch sử; sau đó bạn cần Force push để đồng bộ.")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                visible: squashDialog.valid && squashDialog.plan.dirty === true
                color: theme.danger
                text: qsTr("Đang có thay đổi chưa commit. Vì phải áp lại các commit phía trên, hãy commit hoặc cất tạm trước.")
            }
        }
        footer: DialogButtonBox {
            Button {
                text: qsTr("Gom commit")
                highlighted: true
                enabled: squashDialog.valid && side.hist.canSquashSelection && squashDialog.plan.dirty !== true
                         && messageField.text.trim() !== "" && !repo.busy
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.squashSelected(messageField.text)
    }

    Dialog {
        id: branchFromCommitDialog
        title: qsTr("Tạo nhánh từ commit")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 420
        onAboutToShow: { branchNameField.clear(); branchNameField.forceActiveFocus() }
        contentItem: ColumnLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Nhánh mới sẽ bắt đầu từ commit %1 và sau khi tạo app sẽ chuyển sang nhánh đó.").arg(commitMenu.shortHash)
            }
            TextField {
                id: branchNameField
                Layout.fillWidth: true
                placeholderText: qsTr("Tên nhánh mới")
                onAccepted: branchFromCommitDialog.accept()
            }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Tạo nhánh"); highlighted: true; enabled: branchNameField.text.trim() !== ""; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: side.requestBranchFromCommit(branchNameField.text, branchCommitHash)
    }

    Dialog {
        id: chooseBranchFromCommitDialog
        title: qsTr("Bạn đang có thay đổi chưa commit")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 460
        contentItem: Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Chọn cách xử lý thay đổi trước khi chuyển sang nhánh mới từ commit đã chọn. Thay đổi được mang theo có thể không áp dụng được nếu trùng với nội dung của commit.")
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Cất tạm"); highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Mang theo"); DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.createBranchFromCommit(side.pendingBranchCommitName, side.branchCommitHash, true)
        onApplied: { repo.createBranchFromCommit(side.pendingBranchCommitName, side.branchCommitHash, false); close() }
    }

    Menu {
        id: commitMenu
        property string hash: ""
        property string shortHash: ""
        MenuItem {
            text: side.hist.rangeSize > 1 ? qsTr("Cherry-pick %1 commit đã chọn vào nhánh hiện tại").arg(side.hist.rangeSize)
                                            : qsTr("Cherry-pick commit này vào nhánh hiện tại")
            enabled: side.hist.rangeSize >= 1 && !repo.busy
            onTriggered: repo.cherryPickSelected()
        }
        MenuItem {
            text: qsTr("Tạo nhánh từ commit này…")
            enabled: !repo.busy
            onTriggered: { side.branchCommitHash = commitMenu.hash; branchFromCommitDialog.open() }
        }
        MenuItem {
            text: qsTr("Revert commit này")
            enabled: side.hist.rangeSize === 1 && !repo.busy
            onTriggered: repo.revertCommit(commitMenu.hash)
        }
        MenuItem {
            text: side.hist.rangeSize > 1 ? qsTr("Gom %1 commit đã chọn…").arg(side.hist.rangeSize)
                                           : qsTr("Gom commit… (Shift+bấm để chọn đoạn)")
            enabled: side.hist.rangeSize > 1 && !repo.busy
            onTriggered: squashDialog.open()
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Sao chép mã commit")
            onTriggered: {
                clipboardHelper.text = commitMenu.hash
                clipboardHelper.selectAll()
                clipboardHelper.copy()
            }
        }
    }

    ListView {
        id: commitList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: side.hist.commits
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        onAtYEndChanged: if (atYEnd && count > 0) side.hist.loadMore()

        delegate: ItemDelegate {
            id: commitRow
            width: ListView.view.width
            height: 52
            padding: 0
            highlighted: index >= side.hist.rangeStart && index <= side.hist.rangeEnd

            // Bấm: chọn một commit. Shift+bấm: mở rộng thành đoạn. Bấm phải: menu.
            TapHandler {
                acceptedButtons: Qt.LeftButton
                acceptedModifiers: Qt.NoModifier
                onTapped: side.hist.selectCommit(index)
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                acceptedModifiers: Qt.ShiftModifier
                onTapped: side.hist.extendSelection(index)
            }
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: {
                    if (index < side.hist.rangeStart || index > side.hist.rangeEnd)
                        side.hist.selectCommit(index)
                    commitMenu.hash = model.hash
                    commitMenu.shortHash = model.shortHash
                    commitMenu.popup()
                }
            }

            background: Rectangle {
                color: commitRow.highlighted ? theme.rowSelected : commitRow.hovered ? theme.rowHover : "transparent"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }
            }
            contentItem: RowLayout {
                spacing: 0
                Canvas {
                    id: graph
                    Layout.preferredWidth: 94
                    Layout.fillHeight: true
                    property int nodeLane: model.graphNodeLane
                    property int laneCount: model.graphLaneCount
                    property var edges: model.graphEdges
                    property var verticalLanes: model.graphVerticalLanes
                    function laneColor(lane) {
                        var colors = ["#38bdf8", "#f59e0b", "#22c55e", "#d946ef", "#fb7185", "#a3e635", "#818cf8"]
                        return colors[lane % colors.length]
                    }
                    onNodeLaneChanged: requestPaint()
                    onLaneCountChanged: requestPaint()
                    onEdgesChanged: requestPaint()
                    onVerticalLanesChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.lineWidth = 2
                        ctx.lineCap = "round"
                        var centerY = height / 2
                        var xFor = function(lane) { return 9 + lane * 13 }
                        for (var v = 0; v < verticalLanes.length; ++v) {
                            var liveLane = verticalLanes[v]
                            ctx.strokeStyle = laneColor(liveLane)
                            ctx.beginPath()
                            ctx.moveTo(xFor(liveLane), 0)
                            ctx.lineTo(xFor(liveLane), height)
                            ctx.stroke()
                        }
                        ctx.strokeStyle = laneColor(nodeLane)
                        ctx.beginPath()
                        ctx.moveTo(xFor(nodeLane), 0)
                        ctx.lineTo(xFor(nodeLane), centerY)
                        ctx.stroke()
                        for (var e = 0; e < edges.length; ++e) {
                            var targetLane = edges[e].toLane
                            var fromX = xFor(nodeLane)
                            var toX = xFor(targetLane)
                            ctx.strokeStyle = laneColor(nodeLane)
                            ctx.beginPath()
                            ctx.moveTo(fromX, centerY)
                            if (targetLane === nodeLane) {
                                ctx.lineTo(toX, height)
                            } else {
                                ctx.bezierCurveTo(fromX, centerY + 11, toX, height - 11, toX, height)
                            }
                            ctx.stroke()
                        }
                        ctx.beginPath()
                        ctx.arc(xFor(nodeLane), centerY, model.isMerge ? 4.5 : 4, 0, Math.PI * 2)
                        ctx.fillStyle = laneColor(nodeLane)
                        ctx.fill()
                        if (model.isMerge) {
                            ctx.beginPath()
                            ctx.arc(xFor(nodeLane), centerY, 1.8, 0, Math.PI * 2)
                            ctx.fillStyle = theme.sidebar
                            ctx.fill()
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 10
                    spacing: 2
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        Rectangle {
                            id: refsBadge
                            visible: model.refs !== ""
                            radius: 3
                            color: theme.rowSelected
                            Layout.maximumWidth: 126
                            implicitWidth: Math.min(126, refsText.implicitWidth + 10)
                            Layout.preferredHeight: visible ? 17 : 0
                            Label {
                                id: refsText
                                anchors.fill: parent
                                anchors.leftMargin: 5
                                anchors.rightMargin: 5
                                text: model.refs.replace("HEAD -> ", "")
                                elide: Text.ElideRight
                                color: theme.accent
                                font.pixelSize: 10
                                verticalAlignment: Text.AlignVCenter
                                textFormat: Text.PlainText
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: (model.isMerge ? "⑂ " : "") + model.subject
                            elide: Text.ElideRight
                            color: theme.text
                            font.pixelSize: 12
                            font.bold: commitRow.highlighted
                            textFormat: Text.PlainText
                        }
                        Label {
                            text: model.shortHash
                            color: theme.textDim
                            font.family: "monospace"
                            font.pixelSize: 10
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: model.author + "  •  " + model.dateText
                        elide: Text.ElideRight
                        color: theme.textDim
                        font.pixelSize: 10
                        textFormat: Text.PlainText
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: commitList.count === 0 && !side.hist.loading
            text: qsTr("Chưa có commit nào")
            color: theme.textDim
        }
    }

    BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        running: side.hist.loading
        visible: running
        implicitHeight: 24
        implicitWidth: 24
    }
}
