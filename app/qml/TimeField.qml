import QtQuick
import QtQuick.Controls.Basic

// An editable timestamp. Shows `value` formatted; on Enter or focus loss it calls `commit(text)`,
// which returns false for text that isn't a time, and then goes back to showing the value.
TextField {
    id: field

    property double value
    property var format: seconds => seconds.toFixed(3)
    property var commit: text => false

    implicitWidth: 104
    implicitHeight: 32
    horizontalAlignment: TextInput.AlignHCenter
    font.family: Theme.monoFont
    font.pixelSize: 13
    color: Theme.text
    selectionColor: Theme.accentLine
    selectedTextColor: Theme.text
    selectByMouse: true

    text: format(value)

    function restore() {
        text = Qt.binding(() => format(value))
    }

    onEditingFinished: {
        commit(text)
        restore()
        focus = false
    }
    Keys.onEscapePressed: {
        restore()
        focus = false
    }

    background: Rectangle {
        radius: Theme.radius
        color: Theme.sunken
        border.width: 1
        border.color: field.activeFocus ? Theme.accentLine
            : field.hovered ? Theme.controlBorderHover : Theme.controlBorder
    }
}
