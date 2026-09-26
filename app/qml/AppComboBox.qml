import QtQuick
import QtQuick.Controls.Basic

// A dropdown for a choice between many options, styled like the fields. `model` is a list of
// { label, value, font? }; `font` draws that row in the given family (a font picker's preview).
// Like SegmentedControl, the owner sets `value` from its setting and a pick emits chosen(value), so
// the control never breaks that binding. The list scrolls past `maxPopupHeight`.
ComboBox {
    id: control

    property var value
    property int maxPopupHeight: 320
    signal chosen(var value)

    textRole: "label"
    valueRole: "value"
    currentIndex: indexOf(value)
    // A pick sets currentIndex itself; bind it to `value` again
    onActivated: index => {
        chosen(model[index].value)
        currentIndex = Qt.binding(() => indexOf(value))
    }

    function indexOf(v) {
        return model.findIndex(option => option.value === v)
    }

    implicitHeight: Theme.controlHeight
    leftPadding: 10
    rightPadding: 32
    font.family: Theme.font
    font.pixelSize: Theme.fontSize
    focusPolicy: Qt.NoFocus // keep keyboard shortcuts with the window

    readonly property var current: currentIndex >= 0 ? model[currentIndex] : null

    contentItem: Text {
        text: control.displayText
        font.family: control.current?.font ?? control.font.family
        font.pixelSize: control.font.pixelSize
        color: control.enabled ? Theme.text : Theme.disabledText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Icon {
        x: control.width - width - 10
        anchors.verticalCenter: parent.verticalCenter
        name: "chevron-down"
        size: 16
        color: control.enabled ? Theme.text2 : Theme.disabledText
        rotation: control.popup.visible ? 180 : 0
    }

    background: Rectangle {
        radius: Theme.radius
        color: Theme.fieldFill
        border.width: 1
        border.color: control.popup.visible ? Theme.accentLine
            : control.hovered ? Theme.controlBorderHover : Theme.controlBorder

        // Fluent: a darker bottom edge
        Rectangle {
            visible: Theme.bottomStroke
            x: parent.radius
            width: parent.width - 2 * parent.radius
            height: 1
            y: parent.height - height
            color: Theme.fieldBottom
        }
    }

    delegate: ItemDelegate {
        id: row
        required property var modelData
        required property int index
        width: ListView.view.width
        implicitHeight: Theme.controlHeight
        leftPadding: 10
        rightPadding: 10
        highlighted: control.highlightedIndex === index
        focusPolicy: Qt.NoFocus

        contentItem: Text {
            text: row.modelData.label
            font.family: row.modelData.font ?? control.font.family
            font.pixelSize: control.font.pixelSize
            color: row.index === control.currentIndex ? Theme.accentText : Theme.text
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Math.max(0, Theme.popupRadius - 4)
            color: row.index === control.currentIndex ? Theme.accentSoft
                : row.highlighted || row.hovered ? Theme.raisedHover : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, control.maxPopupHeight)
        padding: 4
        background: PopupBackground {}
        onOpened: list.positionViewAtIndex(Math.max(0, control.currentIndex), ListView.Center)

        contentItem: ListView {
            id: list
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
