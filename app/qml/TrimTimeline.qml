import QtQuick

// The trim timeline: the whole video as a track, the kept range highlighted between two draggable
// amber handles, and the playhead. Pressing near a handle drags it; pressing anywhere else scrubs.
// It only reports what the user did; the owner snaps the times and moves the values back in.
Item {
    id: root

    property double duration: 0
    property double position: 0
    property double trimStart: 0
    property double trimEnd: 0

    signal seekRequested(double seconds)
    signal trimStartDragged(double seconds)
    signal trimEndDragged(double seconds)
    signal scrubStarted()
    signal scrubFinished()

    readonly property int handleWidth: 10
    readonly property real trackLeft: handleWidth
    readonly property real trackWidth: Math.max(1, width - 2 * handleWidth)

    implicitHeight: 56

    function xFor(seconds) {
        return duration > 0 ? trackLeft + seconds / duration * trackWidth : trackLeft
    }
    function secondsAt(x) {
        return duration > 0 ? Math.max(0, Math.min(duration, (x - trackLeft) / trackWidth * duration)) : 0
    }

    // Track
    Rectangle {
        id: track
        x: root.trackLeft
        width: root.trackWidth
        y: 12
        height: parent.height - 24
        radius: Theme.radius
        color: Theme.sunken
        border.width: 1
        border.color: Theme.border
    }

    // Kept range
    Rectangle {
        x: root.xFor(root.trimStart)
        width: Math.max(0, root.xFor(root.trimEnd) - x)
        y: track.y
        height: track.height
        color: Theme.accentSoft
        border.width: 1
        border.color: Theme.accentLine
        visible: root.duration > 0
    }

    // Handles
    Repeater {
        model: root.duration > 0 ? 2 : 0
        delegate: Rectangle {
            required property int index
            readonly property bool isStart: index === 0
            x: isStart ? root.xFor(root.trimStart) - root.handleWidth : root.xFor(root.trimEnd)
            y: track.y - 4
            width: root.handleWidth
            height: track.height + 8
            radius: 3
            color: dragArea.dragging === (isStart ? "start" : "end") ? Theme.accentHover : Theme.accent

            Column {
                anchors.centerIn: parent
                spacing: 2
                Repeater {
                    model: 3
                    Rectangle { width: 2; height: 2; radius: 1; color: Theme.accentInk }
                }
            }
        }
    }

    // Playhead
    Item {
        visible: root.duration > 0
        x: root.xFor(root.position)
        height: parent.height

        Rectangle {
            x: -1
            width: 2
            y: 4
            height: parent.height - 8
            color: Theme.text
        }
        Rectangle {
            x: -5
            y: 0
            width: 10
            height: 10
            radius: 5
            color: Theme.text
        }
    }

    MouseArea {
        id: dragArea

        property string dragging: "" // "start", "end", "playhead" or ""

        anchors.fill: parent
        enabled: root.duration > 0
        hoverEnabled: true
        preventStealing: true
        cursorShape: dragging === "start" || dragging === "end" || nearHandle(mouseX) !== ""
            ? Qt.SizeHorCursor : Qt.PointingHandCursor

        function nearHandle(x) {
            const toStart = Math.abs(x - (root.xFor(root.trimStart) - root.handleWidth / 2))
            const toEnd = Math.abs(x - (root.xFor(root.trimEnd) + root.handleWidth / 2))
            const reach = root.handleWidth
            if (toStart > reach && toEnd > reach)
                return ""
            return toStart <= toEnd ? "start" : "end"
        }

        function apply(x) {
            if (dragging === "start")
                root.trimStartDragged(root.secondsAt(x + root.handleWidth / 2))
            else if (dragging === "end")
                root.trimEndDragged(root.secondsAt(x - root.handleWidth / 2))
            else
                root.seekRequested(root.secondsAt(x))
        }

        onPressed: mouse => {
            dragging = nearHandle(mouse.x) || "playhead"
            if (dragging === "playhead")
                root.scrubStarted()
            apply(mouse.x)
        }
        onPositionChanged: mouse => {
            if (pressed)
                apply(mouse.x)
        }
        function finish() {
            if (dragging === "playhead")
                root.scrubFinished()
            dragging = ""
        }

        onReleased: finish()
        onCanceled: finish()
    }
}
