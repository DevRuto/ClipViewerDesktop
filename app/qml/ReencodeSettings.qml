import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// The re-encode settings popup: each setting is a row of presets (the values are cv::ReencodeOptions'
// choices), with a line under it describing the picked one.
Popup {
    id: root

    required property EditorController editor

    padding: 16
    width: 380
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    background: PopupBackground {}

    // key: the setting's name in editor.reencode
    readonly property var settings: [
        {
            key: "crf", title: "Quality",
            options: [
                { label: "Best", value: 16, hint: "CRF 16 · near-lossless, large file" },
                { label: "High", value: 18, hint: "CRF 18 · looks like the source (default)" },
                { label: "Good", value: 23, hint: "CRF 23 · x264's default, about half the size" },
                { label: "Small", value: 28, hint: "CRF 28 · visibly softer, for sharing" }
            ]
        },
        {
            key: "preset", title: "Encoding speed",
            options: [
                { label: "Fast", value: "veryfast", hint: "x264 veryfast · quickest export (default)" },
                { label: "Balanced", value: "medium", hint: "x264 medium · slower, a bit smaller" },
                { label: "Slow", value: "slow", hint: "x264 slow · slowest, smallest file" }
            ]
        },
        {
            key: "maxHeight", title: "Resolution",
            options: [
                { label: "Original", value: 0, hint: "Keeps the source size (default)" },
                { label: "1080p", value: 1080, hint: "Scales down to 1080p; never scales up" },
                { label: "720p", value: 720, hint: "Scales down to 720p; never scales up" },
                { label: "480p", value: 480, hint: "Scales down to 480p; never scales up" }
            ]
        },
        {
            key: "maxFrameRate", title: "Frame rate",
            options: [
                { label: "Original", value: 0, hint: "Keeps the source frame rate (default)" },
                { label: "60 fps", value: 60, hint: "Drops frames above 60 fps; slower video is unchanged" },
                { label: "30 fps", value: 30, hint: "Drops frames above 30 fps; slower video is unchanged" }
            ]
        },
        {
            key: "audioBitrate", title: "Audio",
            options: [
                { label: "320k", value: 320, hint: "AAC 320 kbit/s" },
                { label: "192k", value: 192, hint: "AAC 192 kbit/s (default)" },
                { label: "128k", value: 128, hint: "AAC 128 kbit/s" },
                { label: "None", value: 0, hint: "Leaves the audio out" }
            ]
        }
    ]

    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Icon { name: "reencode"; size: 16; color: Theme.text2 }
            Text {
                Layout.fillWidth: true
                text: "Re-encode settings"
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            AppButton {
                quiet: true
                text: "Reset"
                implicitHeight: 26
                enabled: !root.editor.reencodeIsDefault
                toolTip: "Back to the defaults"
                onClicked: root.editor.resetReencode()
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
                    id: choice
                    Layout.fillWidth: true
                    fill: true
                    model: setting.modelData.options
                    value: root.editor.reencode[setting.modelData.key]
                    onActivated: value => root.editor.setReencodeOption(setting.modelData.key, value)
                }
                Text {
                    Layout.fillWidth: true
                    text: choice.current ? choice.current.hint : ""
                    color: Theme.text3
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: "Output is H.264 + AAC in MP4. Smart cut copies the video as it is, so these only apply to re-encoding."
            color: Theme.text3
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
