import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// The subtitle settings popup: size, background and position (saved, see cv::SubtitleStyle) and
// the timing offset for the open video.
Popup {
    id: root

    required property EditorController editor

    padding: 16
    width: 380
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    background: PopupBackground {}

    // key: the setting's name in editor.subtitleStyle
    readonly property var settings: [
        {
            key: "size", title: "Size",
            options: [
                { label: "Small", value: 75 },
                { label: "Normal", value: 100 },
                { label: "Large", value: 130 },
                { label: "Huge", value: 160 }
            ]
        },
        {
            key: "background", title: "Background",
            options: [
                { label: "Box", value: "box", toolTip: "Text on a dark box" },
                { label: "Outline", value: "outline", toolTip: "Outlined text, no box" }
            ]
        },
        {
            key: "position", title: "Position",
            options: [
                { label: "Low", value: 5 },
                { label: "Middle", value: 12 },
                { label: "High", value: 20 }
            ]
        }
    ]

    readonly property double delay: editor.subtitles.delay

    function formatDelay(seconds) {
        return (seconds > 0 ? "+" : "") + seconds.toFixed(1) + " s"
    }

    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Icon { name: "captions"; size: 16; color: Theme.text2 }
            Text {
                Layout.fillWidth: true
                text: "Subtitle settings"
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            AppButton {
                quiet: true
                text: "Reset"
                implicitHeight: 26
                enabled: !root.editor.subtitleStyleIsDefault || root.delay !== 0
                toolTip: "Back to the defaults"
                onClicked: {
                    root.editor.resetSubtitleStyle()
                    root.editor.subtitles.delay = 0
                }
            }
        }

        Repeater {
            model: root.settings

            delegate: ColumnLayout {
                id: setting
                required property var modelData
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: setting.modelData.title
                    color: Theme.text2
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
                SegmentedControl {
                    Layout.fillWidth: true
                    fill: true
                    model: setting.modelData.options
                    value: root.editor.subtitleStyle[setting.modelData.key]
                    onActivated: value => root.editor.setSubtitleStyleOption(setting.modelData.key, value)
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "Timing"
                color: Theme.text2
                font.pixelSize: 12
                font.weight: Font.Medium
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                AppButton {
                    text: "Earlier"
                    toolTip: "Show the subtitles 0.1 s earlier (G)"
                    onClicked: root.editor.subtitles.delay = root.delay - 0.1
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: root.delay === 0 ? "In sync" : root.formatDelay(root.delay)
                    color: root.delay === 0 ? Theme.text2 : Theme.accentText
                    font.family: Theme.monoFont
                }
                AppButton {
                    text: "Later"
                    toolTip: "Show the subtitles 0.1 s later (H)"
                    onClicked: root.editor.subtitles.delay = root.delay + 0.1
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: "Size and position also apply to DVD subtitles, which keep their own look. Timing is for this video only."
            color: Theme.text3
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
