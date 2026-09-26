import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// The settings popup (the gear in the top bar). Everything here is saved in settings.json.
Popup {
    id: root

    required property EditorController editor

    padding: 16
    width: 340
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    background: Rectangle {
        radius: Theme.radius
        color: Theme.popup
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Icon { name: "settings"; size: 16; color: Theme.text2 }
            Text {
                Layout.fillWidth: true
                text: "Settings"
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "Click controls"
                color: Theme.text2
                font.pixelSize: 12
                font.weight: Font.Medium
            }
            SegmentedControl {
                Layout.fillWidth: true
                fill: true
                model: [
                    { label: "On", value: true },
                    { label: "Off", value: false }
                ]
                value: root.editor.clickControls
                onActivated: value => root.editor.clickControls = value
            }
            Text {
                Layout.fillWidth: true
                text: "Click the video to play or pause. Double-click its left or right edge to skip 10 s."
                color: Theme.text3
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }
    }
}
