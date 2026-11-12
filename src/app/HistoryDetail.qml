import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Đồ thị lịch sử chiếm vùng làm việc chính; phần dưới hiển thị chi tiết commit đã chọn.
ColumnLayout {
    id: detail
    readonly property var hist: repo.history
    signal commitContextRequested(string hash, string shortHash)
    spacing: 0

    SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: Qt.Vertical
        handle: Rectangle { implicitHeight: 5; color: theme.border }

        ColumnLayout {
            SplitView.preferredHeight: 310
            SplitView.minimumHeight: 150
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                height: 32
                color: theme.sidebar
                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    Label { text: qsTr("Đồ thị"); Layout.preferredWidth: 105; Layout.leftMargin: 10; color: theme.textDim; font.pixelSize: 11; font.bold: true }
                    Label { text: qsTr("Thông điệp commit"); Layout.fillWidth: true; color: theme.textDim; font.pixelSize: 11; font.bold: true }
                    Label { text: qsTr("Tác giả"); Layout.preferredWidth: 145; color: theme.textDim; font.pixelSize: 11; font.bold: true }
                    Label { text: qsTr("Ngày"); Layout.preferredWidth: 145; color: theme.textDim; font.pixelSize: 11; font.bold: true }
                    Label { text: qsTr("Mã"); Layout.preferredWidth: 78; Layout.rightMargin: 10; color: theme.textDim; font.pixelSize: 11; font.bold: true }
                }
            }

            ListView {
                id: graphList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: detail.hist.commits
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                onAtYEndChanged: if (atYEnd && count > 0) detail.hist.loadMore()

                delegate: ItemDelegate {
                    id: graphRow
                    width: ListView.view.width
                    height: 34
                    padding: 0
                    highlighted: index >= detail.hist.rangeStart && index <= detail.hist.rangeEnd

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        acceptedModifiers: Qt.NoModifier
                        onTapped: detail.hist.selectCommit(index)
                    }
                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        acceptedModifiers: Qt.ShiftModifier
                        onTapped: detail.hist.extendSelection(index)
                    }
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: {
                            if (index < detail.hist.rangeStart || index > detail.hist.rangeEnd)
                                detail.hist.selectCommit(index)
                            detail.commitContextRequested(model.hash, model.shortHash)
                        }
                    }

                    background: Rectangle {
                        color: graphRow.highlighted ? theme.rowSelected : graphRow.hovered ? theme.rowHover : "transparent"
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }
                    }

                    contentItem: RowLayout {
                        spacing: 0
                        Canvas {
                            Layout.preferredWidth: 100
                            Layout.fillHeight: true
                            property int nodeLane: model.graphNodeLane
                            property var edges: model.graphEdges
                            property var verticalLanes: model.graphVerticalLanes
                            function laneColor(lane) {
                                var colors = ["#38bdf8", "#f59e0b", "#22c55e", "#d946ef", "#fb7185", "#a3e635", "#818cf8"]
                                return colors[lane % colors.length]
                            }
                            onNodeLaneChanged: requestPaint()
                            onEdgesChanged: requestPaint()
                            onVerticalLanesChanged: requestPaint()
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.lineWidth = 2
                                ctx.lineCap = "round"
                                var centerY = height / 2
                                var xFor = function(lane) { return 10 + lane * 14 }
                                for (var v = 0; v < verticalLanes.length; ++v) {
                                    var lane = verticalLanes[v]
                                    ctx.strokeStyle = laneColor(lane)
                                    ctx.beginPath()
                                    ctx.moveTo(xFor(lane), 0)
                                    ctx.lineTo(xFor(lane), height)
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
                                    if (targetLane === nodeLane)
                                        ctx.lineTo(toX, height)
                                    else
                                        ctx.bezierCurveTo(fromX, centerY + 8, toX, height - 8, toX, height)
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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Rectangle {
                                    visible: model.refs !== ""
                                    radius: 3
                                    color: theme.rowSelected
                                    Layout.maximumWidth: 175
                                    implicitWidth: Math.min(175, graphRefs.implicitWidth + 12)
                                    Layout.preferredHeight: visible ? 19 : 0
                                    Label {
                                        id: graphRefs
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
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
                                    font.bold: graphRow.highlighted
                                    textFormat: Text.PlainText
                                }
                            }
                            Label {
                                Layout.preferredWidth: 145
                                text: model.author
                                elide: Text.ElideRight
                                color: theme.textDim
                                font.pixelSize: 11
                                textFormat: Text.PlainText
                            }
                            Label {
                                Layout.preferredWidth: 145
                                text: model.dateText
                                elide: Text.ElideRight
                                color: theme.textDim
                                font.pixelSize: 11
                            }
                            Label {
                                Layout.preferredWidth: 78
                                Layout.rightMargin: 10
                                text: model.shortHash
                                color: theme.textDim
                                font.family: "monospace"
                                font.pixelSize: 10
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: graphList.count === 0 && !detail.hist.loading
                    text: qsTr("Chưa có commit nào")
                    color: theme.textDim
                }
            }
        }

        ColumnLayout {
            SplitView.fillHeight: true
            SplitView.minimumHeight: 140
            spacing: 0

            Flickable {
                id: messageView
                Layout.fillWidth: true
                Layout.preferredHeight: 84
                visible: detail.hist.selectedHash !== ""
                clip: true
                contentWidth: width
                contentHeight: messageText.implicitHeight + 16
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                TextEdit {
                    id: messageText
                    x: 14
                    y: 8
                    width: messageView.width - 28
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    text: detail.hist.message
                    textFormat: TextEdit.PlainText
                    color: theme.text
                    selectionColor: theme.button
                    font.pixelSize: 13
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: theme.border; visible: detail.hist.selectedHash !== "" }

            ListView {
                id: fileList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(150, Math.max(36, count * 32))
                visible: detail.hist.selectedHash !== ""
                clip: true
                model: detail.hist.files
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: fileRow
                    width: ListView.view.width
                    height: 32
                    padding: 0
                    highlighted: detail.hist.selectedFile === model.path
                    onClicked: detail.hist.selectFile(index)
                    background: Rectangle {
                        color: fileRow.highlighted ? theme.rowSelected : fileRow.hovered ? theme.rowHover : "transparent"
                    }
                    contentItem: RowLayout {
                        spacing: 10
                        StatusIcon { Layout.leftMargin: 14; status: model.status }
                        Label {
                            Layout.fillWidth: true
                            Layout.rightMargin: 12
                            elide: Text.ElideMiddle
                            textFormat: Text.PlainText
                            color: theme.text
                            font.pixelSize: 13
                            text: model.origPath !== "" ? model.origPath + " → " + model.path : model.path
                        }
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: theme.border; visible: detail.hist.selectedHash !== "" }

            DiffView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: detail.hist.selectedHash !== ""
                model: detail.hist.diff
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: detail.hist.selectedHash === ""
                Label {
                    anchors.centerIn: parent
                    text: qsTr("Chọn một commit để xem thay đổi")
                    color: theme.textDim
                    font.pixelSize: 14
                }
            }
        }
    }
}
