import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// The settings popup (the gear in the top bar). Everything here is saved in settings.json.
Popup {
    id: root

    required property EditorController editor

    padding: 16
    width: 360
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    background: Rectangle {
        radius: Theme.radius
        color: Theme.popup
        border.color: Theme.border
    }

    readonly property var onOff: [
        { label: "On", value: true },
        { label: "Off", value: false }
    ]

    // key: the editor property the setting reads and writes; needs: a setting that must be on
    readonly property var settings: [
        {
            key: "shortSkip", title: "Skip with ← / →",
            options: [
                { label: "1 s", value: 1 },
                { label: "2 s", value: 2 },
                { label: "5 s", value: 5 },
                { label: "10 s", value: 10 }
            ]
        },
        {
            key: "longSkip", title: "Skip with J / L",
            options: [
                { label: "5 s", value: 5 },
                { label: "10 s", value: 10 },
                { label: "30 s", value: 30 },
                { label: "60 s", value: 60 }
            ]
        },
        {
            key: "autoplay", title: "When a video opens",
            options: [
                { label: "Show the first frame", value: false },
                { label: "Play", value: true }
            ]
        },
        {
            key: "clickControls", title: "Click the video to play or pause",
            options: onOff
        },
        {
            key: "edgeDoubleClick", title: "Double-click an edge to skip", needs: "clickControls",
            options: onOff,
            note: "The left or right edge skips as far as J / L."
        }
    ]

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

        Repeater {
            model: root.settings

            delegate: ColumnLayout {
                id: setting
                required property var modelData
                Layout.fillWidth: true
                spacing: 6
                enabled: !modelData.needs || root.editor[modelData.needs]

                Text {
                    text: setting.modelData.title
                    color: setting.enabled ? Theme.text2 : Theme.text3
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
                SegmentedControl {
                    Layout.fillWidth: true
                    fill: true
                    model: setting.modelData.options
                    value: root.editor[setting.modelData.key]
                    onActivated: value => root.editor[setting.modelData.key] = value
                }
                Text {
                    Layout.fillWidth: true
                    visible: !!setting.modelData.note
                    text: setting.modelData.note || ""
                    color: Theme.text3
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
