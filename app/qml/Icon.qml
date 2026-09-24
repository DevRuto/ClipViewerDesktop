import QtQuick
import QtQuick.Shapes

// A 24x24 stroked line icon (Lucide-style), scaled to `size`. `name` picks the path.
Item {
    id: root

    property string name
    property color color: Theme.text
    property real size: 16

    implicitWidth: size
    implicitHeight: size

    readonly property var paths: ({
        "play": "M7 4 L19 12 L7 20 Z",
        "pause": "M8 5 L8 19 M16 5 L16 19",
        "stepBack": "M17 5 L9 12 L17 19 M6 5 L6 19",
        "stepForward": "M7 5 L15 12 L7 19 M18 5 L18 19",
        "skipBack": "M11 6 L5 12 L11 18 M19 6 L13 12 L19 18",
        "skipForward": "M13 6 L19 12 L13 18 M5 6 L11 12 L5 18",
        "open": "M3 7 L3 19 L21 19 L21 9 L12 9 L10 6 L3 6 Z",
        "export": "M12 3 L12 15 M7 10 L12 15 L17 10 M4 17 L4 21 L20 21 L20 17",
        "preview": "M4 6 L4 18 M20 6 L20 18 M8 8 L16 12 L8 16 Z",
        "close": "M6 6 L18 18 M18 6 L6 18",
        "volume": "M4 9 L4 15 L8 15 L13 19 L13 5 L8 9 Z M16.5 9.5 Q18 12 16.5 14.5 M19 7 Q22.5 12 19 17",
        "muted": "M4 9 L4 15 L8 15 L13 19 L13 5 L8 9 Z M16 9 L21 15 M21 9 L16 15",
        "fullscreen": "M4 9 L4 4 L9 4 M15 4 L20 4 L20 9 M20 15 L20 20 L15 20 M9 20 L4 20 L4 15",
        "exitFullscreen": "M9 4 L9 9 L4 9 M20 9 L15 9 L15 4 M15 20 L15 15 L20 15 M4 15 L9 15 L9 20"
    })

    Shape {
        anchors.centerIn: parent
        width: 24
        height: 24
        scale: root.size / 24
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.color
            strokeWidth: 2
            fillColor: root.name === "play" ? root.color : "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg { path: root.paths[root.name] ?? "" }
        }
    }
}
