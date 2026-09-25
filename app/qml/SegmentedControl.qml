import QtQuick
import QtQuick.Controls.Basic

// A choice between a few options: one sunken track with a highlight that slides to the picked one.
// `model` is a list of { label, value, icon?, toolTip? }. A click emits activated(value); the owner
// sets `value` from its setting, so the control never breaks that binding. `fill` stretches the
// segments evenly across the control's width.
Rectangle {
    id: root

    property var model: []
    property var value
    property bool fill: false
    signal activated(var value)

    readonly property int currentIndex: model.findIndex(option => option.value === value)
    readonly property var current: currentIndex >= 0 ? model[currentIndex] : null

    implicitWidth: segments.implicitWidth + 2 * inset
    implicitHeight: 32
    radius: Theme.radius
    color: Theme.sunken
    border.color: Theme.controlBorder

    readonly property int inset: 3

    Rectangle {
        id: highlight
        // repeater.count makes this re-evaluate once the delegates exist
        readonly property Item target: repeater.count > 0 && root.currentIndex >= 0
            ? repeater.itemAt(root.currentIndex) : null
        property bool animate: false

        visible: target !== null
        x: target ? segments.x + target.x : 0
        y: root.inset
        width: target ? target.width : 0
        height: root.height - 2 * root.inset
        radius: Theme.radius - 2
        color: root.enabled ? Theme.accentSoft : Theme.raised
        border.color: root.enabled ? Theme.accentLine : Theme.controlBorder

        Behavior on x { enabled: highlight.animate; NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on width { enabled: highlight.animate; NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        // Slide on changes, not into place when the control first appears.
        Component.onCompleted: Qt.callLater(() => animate = true)
    }

    Row {
        id: segments
        x: root.inset
        y: root.inset
        height: root.height - 2 * root.inset

        Repeater {
            id: repeater
            model: root.model

            delegate: Item {
                id: segment
                required property var modelData
                required property int index
                readonly property bool selected: index === root.currentIndex

                width: root.fill ? (root.width - 2 * root.inset) / Math.max(1, repeater.count) : content.implicitWidth + 24
                height: segments.height

                ToolTip.visible: (modelData.toolTip ?? "") !== "" && area.containsMouse
                ToolTip.delay: 600
                ToolTip.text: modelData.toolTip ?? ""

                Row {
                    id: content
                    anchors.centerIn: parent
                    spacing: 6
                    readonly property color foreground: !root.enabled ? Theme.disabledText
                        : segment.selected ? Theme.accentText
                        : area.containsMouse ? Theme.text
                        : Theme.text2

                    Icon {
                        visible: (segment.modelData.icon ?? "") !== ""
                        name: segment.modelData.icon ?? ""
                        size: 15
                        color: content.foreground
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: segment.modelData.label
                        color: content.foreground
                        font.family: Theme.font
                        font.pixelSize: 13
                        font.weight: Font.Medium // the same for all, so picking one doesn't resize it
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on color { ColorAnimation { duration: 80 } }
                    }
                }

                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        if (!segment.selected)
                            root.activated(segment.modelData.value)
                    }
                }
            }
        }
    }
}
