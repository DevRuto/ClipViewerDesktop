import QtQuick
import QtQuick.Controls.Basic

// The app's button. Default: raised and outlined. `flat`: the amber main action (`danger` makes it
// red, for destructive actions). `quiet`: borderless, for toolbars. `checked` (with checkable)
// shows the accent, for segmented choices.
Button {
    id: control

    property string iconName
    property bool quiet: false
    property bool danger: false
    property string toolTip

    implicitHeight: Theme.controlHeight
    padding: 8
    leftPadding: text ? 12 : 8
    rightPadding: text ? 12 : 8
    font.family: Theme.font
    font.pixelSize: Theme.fontSize
    font.weight: control.flat ? Font.DemiBold : Theme.buttonWeight
    focusPolicy: Qt.NoFocus // keep keyboard shortcuts with the window

    ToolTip.visible: toolTip !== "" && hovered
    ToolTip.delay: 600
    ToolTip.text: toolTip

    readonly property color foreground: !enabled ? Theme.disabledText
        : flat ? (danger ? Theme.text : Theme.accentInk)
        : checked ? Theme.accentText
        : Theme.text

    contentItem: Row {
        spacing: 6
        Icon {
            visible: control.iconName !== ""
            name: control.iconName
            color: control.foreground
            size: 16
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: control.text !== ""
            text: control.text
            font: control.font
            color: control.foreground
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    background: Rectangle {
        radius: Theme.cornerFor(height)
        color: {
            if (!control.enabled)
                return control.flat ? Theme.raised : (control.quiet ? Theme.clear : Theme.buttonFill)
            if (control.flat) {
                const base = control.danger ? Theme.danger : Theme.accent
                return control.down ? Qt.darker(base, 1.1) : control.hovered ? Qt.lighter(base, 1.08) : base
            }
            if (control.checked)
                return Theme.accentSoft
            if (control.down)
                return Theme.raisedPressed
            if (control.hovered)
                return Theme.raisedHover
            return control.quiet ? Theme.clear : Theme.buttonFill
        }
        border.width: control.flat || control.quiet || !Theme.buttonBorder ? 0 : 1
        border.color: control.checked ? Theme.accentLine
            : control.hovered ? Theme.controlBorderHover : Theme.controlBorder

        Behavior on color { ColorAnimation { duration: 80 } }

        // Fluent: a darker line along the bottom of outlined buttons
        Rectangle {
            visible: Theme.bottomStroke && !control.flat && !control.quiet && !control.checked
            x: parent.radius
            y: parent.height - 1
            width: parent.width - 2 * parent.radius
            height: 1
            color: Theme.controlBorderHover
        }
    }
}
