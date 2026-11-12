import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cột trái của tab "Xung đột": danh sách file đang xung đột.
ColumnLayout {
    readonly property var cf: repo.conflicts
    spacing: 0

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 40
        color: theme.sidebar
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }
        Label {
            anchors.centerIn: parent
            text: repo.conflictCount === 1 ? qsTr("1 file xung đột") : qsTr("%1 file xung đột").arg(repo.conflictCount)
            color: theme.text
            font.pixelSize: 13
        }
    }

    ListView {
        id: fileList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: cf.files
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        delegate: ItemDelegate {
            id: row
            width: ListView.view.width
            height: 52
            padding: 0
            highlighted: cf.selectedPath === model.path
            onClicked: cf.select(index)
            background: Rectangle {
                color: row.highlighted ? theme.rowSelected : row.hovered ? theme.rowHover : "transparent"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }
            }
            contentItem: RowLayout {
                spacing: 10
                StatusIcon { Layout.leftMargin: 12; status: "U" }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 12
                    spacing: 1
                    Label {
                        Layout.fillWidth: true
                        text: model.path
                        elide: Text.ElideMiddle
                        color: theme.text
                        font.pixelSize: 13
                        font.bold: true
                        textFormat: Text.PlainText
                    }
                    Label {
                        Layout.fillWidth: true
                        text: model.description
                        elide: Text.ElideRight
                        color: theme.textDim
                        font.pixelSize: 12
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: fileList.count === 0
            text: qsTr("Không còn file xung đột")
            color: theme.textDim
        }
    }
}
