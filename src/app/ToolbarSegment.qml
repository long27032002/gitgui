import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Một ô của thanh công cụ: biểu tượng + dòng nhãn nhỏ + dòng tiêu đề đậm (+ huy hiệu, nút mũi tên).
Item {
    id: seg
    property string icon: ""
    property string label: ""
    property string title: ""
    property string badgeText: ""
    property color badgeColor: theme.badge
    property bool showChevron: true
    // true: dòng đậm (title) nằm trên, dòng mờ (label) nằm dưới — như nút "Pull origin".
    property bool titleFirst: false
    // Dạng nút tách đôi: bấm vào thân = hành động chính, bấm vào mũi tên = mở menu.
    property bool split: false
    property bool busy: false
    signal clicked()
    signal chevronClicked()

    implicitHeight: 56
    opacity: enabled ? 1.0 : 0.5

    readonly property int chevronAreaWidth: split ? 40 : 0

    Rectangle {
        anchors.fill: parent
        anchors.rightMargin: seg.chevronAreaWidth
        color: bodyArea.pressed ? theme.toolbarPressed : bodyArea.containsMouse ? theme.toolbarHover : "transparent"
    }
    Rectangle {
        visible: seg.split
        anchors.right: parent.right
        width: seg.chevronAreaWidth
        height: parent.height
        color: chevArea.pressed ? theme.toolbarPressed : chevArea.containsMouse ? theme.toolbarHover : "transparent"
    }
    Rectangle {  // vạch ngăn bên phải
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: theme.border
    }
    Rectangle {  // vạch ngăn giữa thân và mũi tên của nút tách đôi
        visible: seg.split
        x: parent.width - seg.chevronAreaWidth
        width: 1
        height: parent.height
        color: theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: seg.split ? seg.chevronAreaWidth + 10 : 12
        spacing: 12

        Item {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            Icon { anchors.centerIn: parent; width: 20; height: 20; kind: seg.icon; color: theme.text }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            Label {
                Layout.fillWidth: true
                text: seg.titleFirst ? seg.title : seg.label
                color: seg.titleFirst ? theme.text : theme.textDim
                font.pixelSize: seg.titleFirst ? 13 : 11
                font.bold: seg.titleFirst
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
            Label {
                Layout.fillWidth: true
                text: seg.titleFirst ? seg.label : seg.title
                color: seg.titleFirst ? theme.textDim : theme.text
                font.pixelSize: seg.titleFirst ? 11 : 13
                font.bold: !seg.titleFirst
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
        }
        Rectangle {
            visible: seg.badgeText !== ""
            radius: height / 2
            color: seg.badgeColor
            implicitHeight: 18
            implicitWidth: badgeLabel.implicitWidth + 14
            Label {
                id: badgeLabel
                anchors.centerIn: parent
                text: seg.badgeText
                color: theme.text
                font.pixelSize: 11
                font.bold: true
            }
        }
        BusyIndicator {
            visible: seg.busy
            running: seg.busy
            implicitWidth: 20
            implicitHeight: 20
        }
        Icon {
            visible: seg.showChevron && !seg.split
            kind: "chevronDown"
            color: theme.text
        }
    }
    Icon {
        visible: seg.split
        anchors.verticalCenter: parent.verticalCenter
        x: parent.width - seg.chevronAreaWidth / 2 - width / 2
        kind: "chevronDown"
        color: theme.text
    }

    MouseArea {
        id: bodyArea
        anchors.fill: parent
        anchors.rightMargin: seg.chevronAreaWidth
        hoverEnabled: true
        enabled: seg.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: seg.clicked()
    }
    MouseArea {
        id: chevArea
        visible: seg.split
        anchors.right: parent.right
        width: seg.chevronAreaWidth
        height: parent.height
        hoverEnabled: true
        enabled: seg.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: seg.chevronClicked()
    }
}
