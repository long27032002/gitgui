import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: win
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 520
    visible: true
    color: theme.window
    // Bảng màu cho mọi control QML (QPalette của ứng dụng không luôn được các style áp dụng).
    palette {
        window: theme.window
        windowText: theme.text
        base: theme.input
        alternateBase: theme.rowHover
        text: theme.text
        button: theme.toolbarHover
        buttonText: theme.text
        highlight: theme.button
        highlightedText: theme.onAccent
        placeholderText: theme.textDim
        toolTipBase: theme.expander
        toolTipText: theme.text
        light: theme.border
        midlight: theme.border
        mid: theme.inputBorder
        dark: theme.border
        shadow: theme.border
        link: theme.accent
    }
    title: repo.hasRepo ? qsTr("%1 — GitGui").arg(repo.repoName) : qsTr("GitGui")

    // Độ rộng sidebar; ô "Repository hiện tại" của thanh công cụ rộng bằng nó như GitHub Desktop.
    property int sidebarWidth: 341

    // Hành động chuyển/tạo nhánh đang chờ người dùng chọn cách xử lý thay đổi chưa commit.
    property string pendingAction: ""   // "switch" | "create"
    property string pendingName: ""
    property string menuBranch: ""      // nhánh đang được thao tác trong menu "⋯"

    readonly property string remoteName: repo.upstream.indexOf("/") > 0 ? repo.upstream.split("/")[0] : "origin"
    // Hành động chính của ô đồng bộ: publish | pull | push | fetch
    readonly property string syncKind: !repo.hasUpstream ? "publish" : repo.behind > 0 ? "pull" : repo.ahead > 0 ? "push" : "fetch"

    function setTab(i) { tabs.current = i }

    // Dùng khi chụp ảnh giao diện: mở một hộp thoại/popup theo tên.
    function openDialog(name) {
        menuBranch = repo.branch
        switch (name) {
        case "merge": mergeDialog.open(); break
        case "rebase": rebaseDialog.open(); break
        case "forcepush": forcePushDialog.open(); break
        case "create": createDialog.open(); break
        case "choose": pendingAction = "switch"; pendingName = "main"; chooseDialog.open(); break
        case "branches": branchPopup.open(); break
        case "repos": repoPopup.open(); break
        case "clone": cloneDialog.open(); break
        case "newrepo": newRepoDialog.open(); break
        case "discard": discardAllDialog.open(); break
        }
    }

    function syncPrimary() {
        if (syncKind === "pull") repo.pull()
        else if (syncKind === "fetch") repo.fetch()
        else repo.push()
    }

    function requestSwitch(name) {
        if (repo.changeCount > 0) {
            pendingAction = "switch"
            pendingName = name
            chooseDialog.open()
        } else {
            repo.switchBranch(name, false)
        }
    }

    function requestCreate(name) {
        if (name.trim() === "")
            return
        if (repo.changeCount > 0) {
            pendingAction = "create"
            pendingName = name.trim()
            chooseDialog.open()
        } else {
            repo.createBranch(name, false)
        }
    }

    // Chỉ dùng cho smoke test: mở mọi hộp thoại để lộ lỗi binding/thuộc tính thiếu.
    function smokeOpenDialogs() {
        menuBranch = repo.branch
        mergeDialog.open()
        rebaseDialog.open()
        forcePushDialog.open()
        createDialog.open()
        chooseDialog.open()
        renameDialog.open()
        deleteDialog.open()
        remoteDeleteDialog.open()
        forceDeleteDialog.open()
        repoPopup.open()
        branchPopup.open()
        cloneDialog.open()
        newRepoDialog.open()
        discardAllDialog.open()
        historySidebar.smokeOpen()
    }

    function runPending(stash) {
        if (pendingAction === "switch")
            repo.switchBranch(pendingName, stash)
        else if (pendingAction === "create")
            repo.createBranch(pendingName, stash)
        pendingAction = ""
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Chọn thư mục repository")
        onAccepted: repo.openRepoUrl(selectedFolder)
    }

    // ---- Hộp thoại ----

    // Giống GitHub Desktop: hỏi xử lý thay đổi chưa commit khi đổi nhánh.
    Dialog {
        id: chooseDialog
        title: qsTr("Bạn đang có thay đổi chưa commit")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 460
        contentItem: Label {
            wrapMode: Text.Wrap
            text: win.pendingAction === "create"
                  ? qsTr("Bạn muốn xử lý các thay đổi này thế nào khi tạo nhánh \"%1\"?\n\n"
                         + "• Cất tạm: để lại ở nhánh hiện tại, khôi phục được khi quay lại.\n"
                         + "• Mang theo: các thay đổi đi cùng sang nhánh mới.").arg(win.pendingName)
                  : qsTr("Bạn muốn xử lý các thay đổi này thế nào khi chuyển sang nhánh \"%1\"?\n\n"
                         + "• Cất tạm: để lại ở nhánh hiện tại, khôi phục được khi quay lại.\n"
                         + "• Mang theo: các thay đổi đi cùng sang nhánh mới (có thể thất bại nếu xung đột).").arg(win.pendingName)
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Cất tạm thay đổi"); highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Mang theo sang nhánh mới"); DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: win.runPending(true)
        onApplied: { win.runPending(false); close() }
        onRejected: win.pendingAction = ""
    }

    Dialog {
        id: createDialog
        title: qsTr("Tạo nhánh mới")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 400
        onAboutToShow: { createField.clear(); createField.forceActiveFocus() }
        contentItem: ColumnLayout {
            spacing: 8
            Label { text: qsTr("Nhánh mới sẽ được tạo từ vị trí hiện tại của \"%1\".").arg(repo.branch); wrapMode: Text.Wrap; Layout.fillWidth: true }
            TextField {
                id: createField
                Layout.fillWidth: true
                placeholderText: qsTr("Tên nhánh, ví dụ: feature/dang-nhap")
                onAccepted: createDialog.accept()
            }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Tạo nhánh"); highlighted: true; enabled: createField.text.trim() !== ""; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: win.requestCreate(createField.text)
    }

    Dialog {
        id: renameDialog
        title: qsTr("Đổi tên nhánh")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 400
        onAboutToShow: { renameField.text = win.menuBranch; renameField.selectAll(); renameField.forceActiveFocus() }
        contentItem: TextField {
            id: renameField
            onAccepted: renameDialog.accept()
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Đổi tên"); highlighted: true; enabled: renameField.text.trim() !== ""; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.renameBranch(win.menuBranch, renameField.text)
    }

    Dialog {
        id: deleteDialog
        title: qsTr("Xoá nhánh?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 400
        contentItem: Label { wrapMode: Text.Wrap; text: qsTr("Xoá nhánh \"%1\"?").arg(win.menuBranch) }
        footer: DialogButtonBox {
            Button { text: qsTr("Xoá"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.deleteBranch(win.menuBranch)
    }

    Dialog {
        id: remoteDeleteDialog
        title: qsTr("Xoá nhánh trên remote?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 440
        contentItem: Label {
            wrapMode: Text.Wrap
            text: qsTr("Xoá nhánh %1 trên remote? Thao tác này xoá nhánh khỏi máy chủ và không thể hoàn tác bằng GitGui.").arg(win.menuBranch)
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Xoá trên remote"); highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.deleteRemoteBranch(win.menuBranch)
    }

    // Nhánh chưa merge: phải xác nhận lần hai vì xoá sẽ mất commit.
    Dialog {
        id: forceDeleteDialog
        title: qsTr("Nhánh chứa commit chưa merge")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 460
        contentItem: Label {
            wrapMode: Text.Wrap
            text: qsTr("Nhánh \"%1\" có commit chưa được merge vào nhánh nào khác. "
                       + "Xoá bắt buộc sẽ làm mất các commit này (chỉ còn khôi phục được bằng git reflog trong một thời gian).")
                  .arg(repo.pendingForceDelete)
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Xoá bắt buộc"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Huỷ"); highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.confirmForceDelete()
        onRejected: repo.cancelForceDelete()
    }

    // ---- Merge / rebase / force push ----
    Dialog {
        id: mergeDialog
        title: qsTr("Merge vào nhánh hiện tại")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 500
        property int mode: 0
        readonly property bool loaded: repo.branchCompare.loaded === true
        readonly property int incoming: repo.branchCompare.incoming === undefined ? 0 : repo.branchCompare.incoming
        onAboutToShow: { mode = 0; repo.prepareBranchAction(win.menuBranch) }

        contentItem: ColumnLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.bold: true
                text: qsTr("Merge \"%1\" vào \"%2\"").arg(win.menuBranch).arg(repo.branch)
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                opacity: 0.8
                text: !mergeDialog.loaded ? qsTr("Đang kiểm tra…")
                      : mergeDialog.incoming === 0 ? qsTr("Không có commit mới để merge — nhánh này đã nằm trong nhánh hiện tại.")
                      : qsTr("%1 commit mới sẽ được đưa vào nhánh hiện tại.").arg(mergeDialog.incoming)
            }
            RadioButton {
                text: qsTr("Tạo merge commit")
                checked: mergeDialog.mode === 0
                onClicked: mergeDialog.mode = 0
            }
            Label { Layout.leftMargin: 28; Layout.fillWidth: true; wrapMode: Text.Wrap; opacity: 0.65; font.pixelSize: 12
                    text: qsTr("Giữ nguyên lịch sử của cả hai nhánh, luôn tạo một merge commit.") }
            RadioButton {
                text: qsTr("Squash — gom thành một thay đổi")
                checked: mergeDialog.mode === 1
                onClicked: mergeDialog.mode = 1
            }
            Label { Layout.leftMargin: 28; Layout.fillWidth: true; wrapMode: Text.Wrap; opacity: 0.65; font.pixelSize: 12
                    text: qsTr("Đưa mọi thay đổi vào vùng stage để bạn tự viết message và commit. Không tạo liên kết lịch sử với nhánh kia.") }
            RadioButton {
                text: qsTr("Chỉ fast-forward")
                checked: mergeDialog.mode === 2
                onClicked: mergeDialog.mode = 2
            }
            Label { Layout.leftMargin: 28; Layout.fillWidth: true; wrapMode: Text.Wrap; opacity: 0.65; font.pixelSize: 12
                    text: qsTr("Chỉ di chuyển nhánh hiện tại tới cuối nhánh kia; báo lỗi nếu hai nhánh đã phân kỳ.") }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Merge"); highlighted: true; enabled: mergeDialog.loaded && mergeDialog.incoming > 0 && !repo.busy; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.mergeBranch(win.menuBranch, mode)
    }

    Dialog {
        id: rebaseDialog
        title: qsTr("Rebase nhánh hiện tại")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 500
        readonly property bool loaded: repo.branchCompare.loaded === true
        readonly property int localCount: repo.branchCompare.local === undefined ? 0 : repo.branchCompare.local
        readonly property int pushedCount: repo.branchCompare.pushed === undefined ? 0 : repo.branchCompare.pushed
        onAboutToShow: repo.prepareBranchAction(win.menuBranch)

        contentItem: ColumnLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.bold: true
                text: qsTr("Rebase \"%1\" lên \"%2\"").arg(repo.branch).arg(win.menuBranch)
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: !rebaseDialog.loaded ? qsTr("Đang kiểm tra…")
                      : qsTr("%1 commit của nhánh hiện tại sẽ được phát lại (tạo mới) trên đầu \"%2\".")
                        .arg(rebaseDialog.localCount).arg(win.menuBranch)
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                visible: rebaseDialog.loaded && rebaseDialog.pushedCount > 0
                color: "#d99a2b"
                font.bold: true
                text: qsTr("%1 commit trong số đó đã được push. Rebase sẽ viết lại chúng; sau đó bạn cần Force push để đồng bộ.")
                      .arg(rebaseDialog.pushedCount)
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                visible: repo.changeCount > 0
                color: "#d64545"
                text: qsTr("Đang có thay đổi chưa commit. Hãy commit hoặc cất tạm trước khi rebase.")
            }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Rebase"); highlighted: true; enabled: rebaseDialog.loaded && repo.changeCount === 0 && !repo.busy; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Huỷ"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.rebaseOnto(win.menuBranch)
    }

    Dialog {
        id: forcePushDialog
        title: qsTr("Force push — ghi đè lịch sử trên remote")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 560
        readonly property bool loaded: repo.forcePushInfo.loaded === true
        readonly property bool failed: loaded && repo.forcePushInfo.error !== undefined && repo.forcePushInfo.error !== ""
        onAboutToShow: repo.prepareForcePush()
        onRejected: repo.cancelForcePush()

        contentItem: ColumnLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: !forcePushDialog.loaded ? qsTr("Đang kiểm tra remote…")
                      : forcePushDialog.failed ? repo.forcePushInfo.error
                      : qsTr("Lịch sử của \"%1\" trên remote sẽ bị thay bằng lịch sử local. %2 commit trên remote sẽ bị ghi đè:")
                        .arg(repo.forcePushInfo.upstream).arg(repo.forcePushInfo.count)
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(160, commitsCol.implicitHeight + 8)
                visible: forcePushDialog.loaded && !forcePushDialog.failed && repo.forcePushInfo.count > 0
                clip: true
                Column {
                    id: commitsCol
                    width: parent.width
                    Repeater {
                        model: repo.forcePushInfo.commits
                        delegate: Label {
                            required property string modelData
                            text: modelData
                            font.family: "monospace"
                            elide: Text.ElideRight
                            width: commitsCol.width
                            textFormat: Text.PlainText
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                visible: forcePushDialog.loaded && !forcePushDialog.failed
                color: "#d99a2b"
                text: qsTr("Những người khác đang dùng nhánh này sẽ bị ảnh hưởng và app không hoàn tác được thao tác này. "
                           + "Để an toàn, git chỉ ghi đè nếu remote vẫn đúng như danh sách trên (--force-with-lease); "
                           + "nếu có người vừa push thêm, thao tác sẽ bị từ chối.")
            }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Force push"); enabled: forcePushDialog.loaded && !forcePushDialog.failed && !repo.busy; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: qsTr("Huỷ"); highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: repo.confirmForcePush()
    }

    Connections {
        target: repo
        function onPendingForceDeleteChanged() {
            if (repo.pendingForceDelete !== "")
                forceDeleteDialog.open()
        }
    }

    // ---- Chọn nhánh ----
    Popup {
        id: branchPopup
        parent: branchButton
        y: branchButton.height
        width: 340
        height: Math.min(460, win.height - 140)
        padding: 8
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: theme.sidebar; border.color: theme.inputBorder; radius: 6 }
        onOpened: { branchFilter.clear(); branchFilter.forceActiveFocus() }

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            TextField {
                id: branchFilter
                Layout.fillWidth: true
                placeholderText: qsTr("Tìm nhánh…")
            }

            ListView {
                id: branchList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: repo.branches
                ScrollBar.vertical: ScrollBar {}

                section.property: "isRemote"
                section.delegate: Label {
                    required property string section
                    width: ListView.view.width
                    leftPadding: 8
                    topPadding: 6
                    bottomPadding: 2
                    text: section === "true" ? qsTr("Nhánh trên remote") : qsTr("Nhánh local")
                    font.bold: true
                    opacity: 0.7
                }

                delegate: ItemDelegate {
                    id: branchRow
                    readonly property bool matches: branchFilter.text === ""
                        || model.name.toLowerCase().indexOf(branchFilter.text.toLowerCase()) >= 0
                    visible: matches
                    height: matches ? implicitHeight : 0
                    width: ListView.view.width
                    enabled: !repo.busy
                    onClicked: {
                        branchPopup.close()
                        if (!model.isCurrent)
                            win.requestSwitch(model.name)
                    }

                    contentItem: RowLayout {
                        spacing: 6
                        Label { text: model.isCurrent ? "✓" : ""; Layout.preferredWidth: 14 }
                        Label {
                            Layout.fillWidth: true
                            text: model.name
                            elide: Text.ElideMiddle
                            font.bold: model.isCurrent
                            textFormat: Text.PlainText
                        }
                        ToolButton {
                            visible: !(model.isCurrent && model.isRemote)
                            text: "⋯"
                            onClicked: {
                                win.menuBranch = model.name
                                branchMenu.isCurrent = model.isCurrent
                                branchMenu.isRemote = model.isRemote
                                branchMenu.popup()
                            }
                        }
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Tạo nhánh mới…")
                enabled: !repo.busy
                onClicked: { branchPopup.close(); createDialog.open() }
            }
        }
    }

    Menu {
        id: branchMenu
        property bool isCurrent: false
        property bool isRemote: false
        MenuItem {
            text: qsTr("Merge vào nhánh hiện tại…")
            visible: !branchMenu.isCurrent
            height: visible ? implicitHeight : 0
            onTriggered: { branchPopup.close(); mergeDialog.open() }
        }
        MenuItem {
            text: qsTr("Rebase nhánh hiện tại lên đây…")
            visible: !branchMenu.isCurrent
            height: visible ? implicitHeight : 0
            onTriggered: { branchPopup.close(); rebaseDialog.open() }
        }
        MenuItem {
            text: qsTr("Đổi tên…")
            visible: !branchMenu.isRemote
            height: visible ? implicitHeight : 0
            onTriggered: { branchPopup.close(); renameDialog.open() }
        }
        MenuItem {
            text: qsTr("Xoá nhánh…")
            visible: !branchMenu.isRemote && !branchMenu.isCurrent
            height: visible ? implicitHeight : 0
            onTriggered: { branchPopup.close(); deleteDialog.open() }
        }
        MenuItem {
            text: qsTr("Xoá nhánh trên remote…")
            visible: branchMenu.isRemote
            height: visible ? implicitHeight : 0
            onTriggered: { branchPopup.close(); remoteDeleteDialog.open() }
        }
    }

    CloneDialog { id: cloneDialog }
    NewRepoDialog { id: newRepoDialog }
    DiscardDialog {
        id: discardAllDialog
        confirmText: qsTr("Bỏ tất cả")
        message: qsTr("Bỏ TẤT CẢ thay đổi chưa commit (%1 file) và đưa working tree về như ở commit gần nhất? File mới sẽ bị xoá.").arg(repo.changeCount)
        onConfirmed: repo.discardAll()
    }

    // ---- Chọn repository (gần đây) ----
    Popup {
        id: repoPopup
        parent: repoSeg
        y: repoSeg.height
        width: win.sidebarWidth
        height: Math.min(460, win.height - 140)
        padding: 8
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: theme.sidebar; border.color: theme.inputBorder; radius: 6 }
        onOpened: { repoFilter.clear(); repoFilter.forceActiveFocus() }

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            TextField {
                id: repoFilter
                Layout.fillWidth: true
                placeholderText: qsTr("Tìm repository…")
                selectByMouse: true
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: repo.recentRepos
                ScrollBar.vertical: ScrollBar {}
                section.property: "parentDir"

                delegate: ItemDelegate {
                    id: recentRow
                    readonly property bool matches: repoFilter.text === ""
                        || model.name.toLowerCase().indexOf(repoFilter.text.toLowerCase()) >= 0
                    visible: matches
                    height: matches ? 40 : 0
                    width: ListView.view.width
                    padding: 0
                    onClicked: { repoPopup.close(); repo.openRepo(model.path) }
                    background: Rectangle { color: recentRow.hovered ? theme.rowHover : "transparent"; radius: 4 }
                    contentItem: RowLayout {
                        spacing: 8
                        Icon { Layout.leftMargin: 8; kind: "repo"; color: theme.textDim }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                Layout.fillWidth: true
                                text: model.name
                                color: theme.text
                                font.bold: true
                                elide: Text.ElideRight
                                textFormat: Text.PlainText
                            }
                            Label {
                                Layout.fillWidth: true
                                text: model.parentDir
                                color: theme.textDim
                                font.pixelSize: 11
                                elide: Text.ElideMiddle
                                textFormat: Text.PlainText
                            }
                        }
                        ToolButton {
                            visible: recentRow.hovered
                            implicitWidth: 28
                            contentItem: Icon { kind: "close"; color: theme.textDim }
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Bỏ khỏi danh sách")
                            onClicked: repo.removeRecent(model.path)
                        }
                    }
                }

                section.delegate: Label {
                    required property string section
                    width: ListView.view.width
                    leftPadding: 8
                    topPadding: 6
                    bottomPadding: 2
                    text: section
                    color: theme.textDim
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                    textFormat: Text.PlainText
                }

                Label {
                    anchors.centerIn: parent
                    visible: parent.count === 0
                    text: qsTr("Chưa có repository nào gần đây")
                    color: theme.textDim
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Button { Layout.fillWidth: true; text: qsTr("Mở…"); onClicked: { repoPopup.close(); folderDialog.open() } }
                Button { Layout.fillWidth: true; text: qsTr("Clone…"); onClicked: { repoPopup.close(); cloneDialog.open() } }
                Button { Layout.fillWidth: true; text: qsTr("Tạo mới…"); onClicked: { repoPopup.close(); newRepoDialog.open() } }
            }
        }
    }

    Menu {
        id: syncMenu
        MenuItem { text: qsTr("Fetch %1").arg(win.remoteName); enabled: repo.canSync && !repo.busy; onTriggered: repo.fetch() }
        MenuItem { text: qsTr("Pull %1").arg(win.remoteName); enabled: repo.canSync && repo.hasUpstream && !repo.busy; onTriggered: repo.pull() }
        MenuItem {
            text: repo.hasUpstream ? qsTr("Push %1").arg(win.remoteName) : qsTr("Publish nhánh")
            enabled: repo.canSync && !repo.busy
            onTriggered: repo.push()
        }
        MenuItem {
            text: qsTr("Force push…")
            visible: repo.diverged
            height: visible ? implicitHeight : 0
            enabled: repo.canSync && !repo.busy
            onTriggered: forcePushDialog.open()
        }
    }

    // ---- Thanh menu ----
    menuBar: MenuBar {
        background: Rectangle {
            color: theme.toolbar
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }
        }
        Menu {
            title: qsTr("&File")
            Action { text: qsTr("Mở repository…"); shortcut: "Ctrl+O"; onTriggered: folderDialog.open() }
            Action { text: qsTr("Repository gần đây…"); shortcut: "Ctrl+Shift+O"; onTriggered: repoPopup.open() }
            Action { text: qsTr("Clone repository…"); shortcut: "Ctrl+Shift+C"; onTriggered: cloneDialog.open() }
            Action { text: qsTr("Tạo repository mới…"); shortcut: "Ctrl+N"; onTriggered: newRepoDialog.open() }
            MenuSeparator {}
            Action { text: qsTr("Thoát"); shortcut: "Ctrl+Q"; onTriggered: Qt.quit() }
        }
        Menu {
            title: qsTr("&Edit")
            Action {
                text: qsTr("Chọn / bỏ chọn tất cả file thay đổi")
                shortcut: "Ctrl+Shift+A"
                enabled: repo.hasRepo && repo.changeCount > 0 && !repo.busy
                onTriggered: repo.toggleAll()
            }
            MenuSeparator {}
            Action {
                text: qsTr("Bỏ tất cả thay đổi…")
                enabled: repo.hasRepo && repo.changeCount > 0 && !repo.busy
                onTriggered: discardAllDialog.open()
            }
            Action {
                text: qsTr("Hoàn tác việc bỏ thay đổi")
                shortcut: "Ctrl+Z"
                enabled: repo.canUndoDiscard && !repo.busy
                onTriggered: repo.undoDiscard()
            }
        }
        Menu {
            title: qsTr("&View")
            Action { text: qsTr("Thay đổi"); shortcut: "Ctrl+1"; enabled: repo.hasRepo; onTriggered: win.setTab(0) }
            Action { text: qsTr("Lịch sử"); shortcut: "Ctrl+2"; enabled: repo.hasRepo; onTriggered: win.setTab(1) }
            Action {
                text: qsTr("Xung đột")
                shortcut: "Ctrl+3"
                enabled: repo.conflictCount > 0 || repo.operation !== ""
                onTriggered: win.setTab(2)
            }
            MenuSeparator {}
            Action {
                text: theme.dark ? qsTr("Chuyển sang giao diện sáng") : qsTr("Chuyển sang giao diện tối")
                onTriggered: theme.toggle()
            }
            MenuSeparator {}
            Menu {
                title: qsTr("Ngôn ngữ")
                MenuItem {
                    text: "Tiếng Việt"
                    checkable: true
                    checked: language.language === "vi"
                    onTriggered: language.setLanguage("vi")
                }
                MenuItem {
                    text: "English"
                    checkable: true
                    checked: language.language === "en"
                    onTriggered: language.setLanguage("en")
                }
            }
        }
        Menu {
            title: qsTr("&Repository")
            Action { text: qsTr("Fetch"); shortcut: "Ctrl+Shift+T"; enabled: repo.canSync && !repo.busy; onTriggered: repo.fetch() }
            Action { text: qsTr("Pull"); shortcut: "Ctrl+Shift+P"; enabled: repo.canSync && repo.hasUpstream && !repo.busy; onTriggered: repo.pull() }
            Action { text: qsTr("Push"); shortcut: "Ctrl+P"; enabled: repo.canSync && !repo.busy; onTriggered: repo.push() }
            Action { text: qsTr("Force push…"); enabled: repo.diverged && !repo.busy; onTriggered: forcePushDialog.open() }
            MenuSeparator {}
            Action { text: qsTr("Làm mới"); shortcut: "F5"; enabled: repo.hasRepo && !repo.busy; onTriggered: repo.refresh() }
            Action { text: qsTr("Mở thư mục repository"); enabled: repo.hasRepo; onTriggered: repo.revealRepoFolder() }
            Action { text: qsTr("Mở .gitignore"); enabled: repo.hasRepo; onTriggered: repo.openGitignore() }
        }
        Menu {
            title: qsTr("&Branch")
            Action { text: qsTr("Chuyển nhánh…"); shortcut: "Ctrl+B"; enabled: repo.hasRepo; onTriggered: branchPopup.open() }
            Action { text: qsTr("Tạo nhánh mới…"); shortcut: "Ctrl+Shift+N"; enabled: repo.hasRepo && !repo.busy; onTriggered: createDialog.open() }
            Action {
                text: qsTr("Đổi tên nhánh hiện tại…")
                enabled: repo.canSync && !repo.busy
                onTriggered: { win.menuBranch = repo.branch; renameDialog.open() }
            }
        }
        Menu {
            title: qsTr("&Help")
            Action { text: qsTr("Giới thiệu GitGui"); onTriggered: aboutDialog.open() }
        }
    }

    Dialog {
        id: aboutDialog
        title: qsTr("Giới thiệu")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 420
        standardButtons: Dialog.Ok
        contentItem: Label {
            wrapMode: Text.Wrap
            text: qsTr("GitGui — ứng dụng quản lý repository Git viết bằng Qt/QML, mô phỏng trải nghiệm của GitHub Desktop cho Linux.\n\nMọi thao tác chạy qua git có sẵn trên máy (SSH agent cho push/pull).")
        }
    }

    // ---- Thanh công cụ ba ô ----
    header: Rectangle {
        height: 56
        color: theme.toolbar
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }

        Row {
            anchors.fill: parent
            ToolbarSegment {
                id: repoSeg
                width: win.sidebarWidth
                height: parent.height
                icon: "repo"
                label: qsTr("Repository hiện tại")
                title: repo.hasRepo ? repo.repoName : qsTr("Chưa mở repository")
                onClicked: repoPopup.opened ? repoPopup.close() : repoPopup.open()
            }
            ToolbarSegment {
                id: branchButton
                width: 330
                height: parent.height
                visible: repo.hasRepo
                icon: "branch"
                label: qsTr("Nhánh hiện tại")
                title: repo.branch
                onClicked: branchPopup.opened ? branchPopup.close() : branchPopup.open()
            }
            ToolbarSegment {
                id: syncSeg
                width: 360
                height: parent.height
                visible: repo.hasRepo
                enabled: repo.canSync
                split: true
                titleFirst: true
                busy: repo.busy && repo.activity !== ""
                icon: win.syncKind === "pull" ? "arrowDown" : (win.syncKind === "fetch" ? "sync" : "arrowUp")
                title: repo.activity !== "" ? repo.activity
                       : win.syncKind === "publish" ? qsTr("Publish nhánh")
                       : win.syncKind === "pull" ? qsTr("Pull %1").arg(win.remoteName)
                       : win.syncKind === "push" ? qsTr("Push %1").arg(win.remoteName)
                       : qsTr("Fetch %1").arg(win.remoteName)
                label: win.syncKind === "publish" ? qsTr("Đẩy nhánh này lên remote") : repo.lastFetchedText
                badgeText: win.syncKind === "pull" ? "↓ " + repo.behind
                           : (win.syncKind === "push" ? "↑ " + repo.ahead : "")
                onClicked: win.syncPrimary()
                onChevronClicked: syncMenu.popup(syncSeg, syncSeg.width - 220, syncSeg.height)
            }
        }

        // Bên phải: thông báo ngắn và nút Force push khi lịch sử đã lệch khỏi remote
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            Label {
                anchors.verticalCenter: parent.verticalCenter
                visible: repo.notice !== ""
                text: repo.notice
                color: theme.textDim
                elide: Text.ElideRight
                width: Math.min(implicitWidth, 360)
                textFormat: Text.PlainText
            }
            Button {
                visible: repo.canCancel
                enabled: repo.canCancel
                text: qsTr("Huỷ lệnh mạng")
                onClicked: repo.cancelNetworkOperation()
            }
            Button {
                visible: repo.canUndoDiscard
                text: qsTr("↶ Hoàn tác việc bỏ thay đổi")
                enabled: !repo.busy
                ToolTip.visible: hovered
                ToolTip.text: repo.lastDiscardText
                onClicked: repo.undoDiscard()
            }
            Button {
                visible: repo.diverged
                text: qsTr("Force push…")
                enabled: repo.canSync && !repo.busy
                onClicked: forcePushDialog.open()
            }
        }
    }

    // ---- Thanh báo lỗi ----
    Rectangle {
        id: errorBanner
        z: 10
        visible: repo.errorMessage !== ""
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: visible ? errorRow.implicitHeight + 16 : 0
        color: Qt.rgba(0.85, 0.25, 0.25, 0.94)

        RowLayout {
            id: errorRow
            anchors.fill: parent
            anchors.margins: 8
            Label {
                Layout.fillWidth: true
                text: repo.errorMessage
                color: "white"
                wrapMode: Text.Wrap
            }
            ToolButton {
                contentItem: Icon { kind: "close"; color: "white" }
                onClicked: repo.clearError()
            }
        }
    }

    // ---- Màn hình chờ khi chưa mở repo ----
    Item {
        anchors.fill: parent
        visible: !repo.hasRepo

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 14
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Chưa mở repository nào")
                color: theme.text
                font.pixelSize: 22
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Chọn một thư mục chứa repo Git để xem và commit thay đổi.")
                color: theme.textDim
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                Button { text: qsTr("Mở repository…"); highlighted: true; onClicked: folderDialog.open() }
                Button { text: qsTr("Clone từ URL…"); onClicked: cloneDialog.open() }
                Button { text: qsTr("Tạo repository mới…"); onClicked: newRepoDialog.open() }
            }
        }
    }

    // ---- Nội dung: sidebar (tab) + vùng chi tiết ----
    SplitView {
        anchors.fill: parent
        visible: repo.hasRepo
        orientation: Qt.Horizontal
        handle: Rectangle {
            implicitWidth: 1
            implicitHeight: 1
            color: SplitHandle.pressed ? theme.accent : theme.border
        }

        Rectangle {
            id: sidebar
            SplitView.preferredWidth: 341
            SplitView.minimumWidth: 260
            SplitView.maximumWidth: 560
            color: theme.sidebar
            onWidthChanged: win.sidebarWidth = width

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Dải tab: Thay đổi | Lịch sử | (Xung đột)
                Item {
                    id: tabs
                    Layout.fillWidth: true
                    implicitHeight: 40
                    property int current: 0
                    readonly property bool conflictsVisible: repo.conflictCount > 0 || repo.operation !== ""
                    readonly property var entries: conflictsVisible
                        ? [ { text: qsTr("Thay đổi"), badge: repo.changeCount, tint: theme.badge },
                            { text: qsTr("Lịch sử"), badge: 0, tint: theme.badge },
                            { text: qsTr("Xung đột"), badge: repo.conflictCount, tint: theme.conflict } ]
                        : [ { text: qsTr("Thay đổi"), badge: repo.changeCount, tint: theme.badge },
                            { text: qsTr("Lịch sử"), badge: 0, tint: theme.badge } ]

                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }
                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: tabs.entries
                            delegate: Item {
                                required property int index
                                required property var modelData
                                width: tabs.width / tabs.entries.length
                                height: tabs.height
                                readonly property bool active: tabs.current === index

                                Rectangle {
                                    anchors.fill: parent
                                    color: tabMouse.containsMouse && !parent.active ? theme.rowHover : "transparent"
                                }
                                Row {
                                    anchors.centerIn: parent
                                    spacing: 8
                                    Label {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.text
                                        color: parent.parent.active ? theme.text : theme.textDim
                                        font.bold: parent.parent.active
                                        font.pixelSize: 13
                                    }
                                    Rectangle {
                                        visible: modelData.badge > 0
                                        anchors.verticalCenter: parent.verticalCenter
                                        radius: 9
                                        color: modelData.tint
                                        implicitHeight: 18
                                        implicitWidth: Math.max(18, badgeText.implicitWidth + 10)
                                        width: implicitWidth
                                        height: implicitHeight
                                        Label {
                                            id: badgeText
                                            anchors.centerIn: parent
                                            text: modelData.badge
                                            color: theme.text
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }
                                }
                                Rectangle {  // gạch chân của tab đang mở
                                    visible: parent.active
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 3
                                    color: theme.button
                                }
                                MouseArea {
                                    id: tabMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: tabs.current = index
                                }
                            }
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: tabs.current
                    ChangesSidebar {}
                    HistorySidebar { id: historySidebar }
                    ConflictSidebar {}
                }
            }
        }

        Rectangle {
            SplitView.fillWidth: true
            color: theme.content

            StackLayout {
                anchors.fill: parent
                currentIndex: tabs.current
                ChangesDetail {}
                HistoryDetail {
                    id: historyDetail
                    onCommitContextRequested: historySidebar.openCommitMenu(hash, shortHash)
                }
                ConflictDetail {}
            }
        }
    }

    Connections {
        target: repo
        function onSquashMergeStaged() { tabs.current = 0 }
    }

    // Có conflict mới (vd sau khi pull): tự chuyển sang tab Xung đột; hết conflict thì quay về Thay đổi.
    Connections {
        target: repo
        property int lastCount: 0
        function onStatusChanged() {
            if (repo.conflictCount > 0 && lastCount === 0)
                tabs.current = 2
            else if (!tabs.conflictsVisible && tabs.current === 2)
                tabs.current = 0
            lastCount = repo.conflictCount
        }
    }
}
