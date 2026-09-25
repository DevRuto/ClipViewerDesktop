import QtQuick

// The preview shown above a seek bar while hovering it: the frame (once one has loaded) and the
// hovered time. Centred on pointerX, kept within the parent's width.
Rectangle {
    id: root

    property url source
    property string time
    property real pointerX: 0

    readonly property int padding: 4

    z: 1
    width: Math.max(thumbnail.visible ? thumbnail.width : 0, timeText.implicitWidth) + 2 * padding
    height: (thumbnail.visible ? thumbnail.height + padding : 0) + timeText.implicitHeight + 2 * padding
    x: Math.max(0, Math.min((parent ? parent.width : 0) - width, pointerX - width / 2))
    y: -height - 4
    radius: Theme.radius
    color: Theme.popup
    border.width: 1
    border.color: Theme.border

    Image {
        id: thumbnail
        x: root.padding
        y: root.padding
        width: 192
        height: implicitWidth > 0 ? Math.round(width * implicitHeight / implicitWidth) : 108
        source: root.source
        visible: status === Image.Ready
        fillMode: Image.PreserveAspectFit
        cache: false
        asynchronous: false
        smooth: true
    }

    Text {
        id: timeText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.padding
        text: root.time
        color: Theme.text
        font.family: Theme.monoFont
        leftPadding: 4
        rightPadding: 4
    }
}
