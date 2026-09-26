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
        "scissors": "M9 6 A3 3 0 1 1 3 6 A3 3 0 1 1 9 6 M9 18 A3 3 0 1 1 3 18 A3 3 0 1 1 9 18 M8.1 8.1 L12 12 M20 4 L8.1 15.9 M14.5 14.5 L20 20",
        "palette": "M12 3 A9 9 0 1 0 12 21 A2 2 0 0 0 13.5 17.7 A2 2 0 0 1 15 14.5 L17 14.5 A4 4 0 0 0 21 10.5 C21 6.4 17 3 12 3 Z M7.5 11 L7.5 11 M10.5 7.5 L10.5 7.5 M15 8 L15 8",
        "markStart": "M10 5 L6 5 L6 19 L10 19 M15 4 L15 20",
        "markEnd": "M14 5 L18 5 L18 19 L14 19 M9 4 L9 20",
        "close":"M6 6 L18 18 M18 6 L6 18",
        "zap": "M13 2 L4 14 L11 14 L10 22 L20 10 L13 10 Z",
        "reencode": "M3 12 A9 9 0 0 1 12 3 A9.75 9.75 0 0 1 18.74 5.74 L21 8 M21 3 L21 8 L16 8 M21 12 A9 9 0 0 1 12 21 A9.75 9.75 0 0 1 5.26 18.26 L3 16 M8 16 L3 16 L3 21",
        "sliders": "M21 4 L14 4 M10 4 L3 4 M21 12 L12 12 M8 12 L3 12 M21 20 L16 20 M12 20 L3 20 M14 2 L14 6 M8 10 L8 14 M16 18 L16 22",
        "captions": "M5 5 L19 5 Q21 5 21 7 L21 17 Q21 19 19 19 L5 19 Q3 19 3 17 L3 7 Q3 5 5 5 Z M7 15 L11 15 M14 15 L17 15 M7 11 L9 11 M12 11 L17 11",
        "pin": "M12 17 L12 22 M5 17 L19 17 M7 17 L9 10 L8 7 L8 3 L16 3 L16 7 L15 10 L17 17",
        "camera": "M14.5 4 L9.5 4 L7.5 7 L4 7 Q2 7 2 9 L2 18 Q2 20 4 20 L20 20 Q22 20 22 18 L22 9 Q22 7 20 7 L16.5 7 Z M15 13 A3 3 0 1 1 9 13 A3 3 0 1 1 15 13",
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
