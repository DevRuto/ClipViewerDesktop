import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia
import ClipViewer

// The player and trim editor window. Playback is Qt Multimedia's MediaPlayer; everything else (probing, the
// trim range, the paused still, export) goes through `editor` (EditorController).
ApplicationWindow {
    id: window

    required property EditorController editor

    // The playhead in seconds: the player's position while playing, otherwise the frame we sought to.
    property double position: 0
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    // P (preview cut) plays the kept range and stops at its end.
    property bool previewing: false
    property bool resumeAfterScrub: false
    // While dragging the playhead (or a trim handle, paused) the live video follows the mouse; the exact
    // still comes on release.
    property bool scrubbing: false
    // One player seek at a time while scrubbing: a paused seek decodes from the previous keyframe, and a
    // new seek drops the unfinished one, so seeking on every mouse move shows nothing until it stops.
    property bool scrubSeekBusy: false
    property int scrubSeekPending: -1 // ms, the latest position asked for while a seek was busy
    readonly property bool fullScreen: visibility === Window.FullScreen
    // The window opens as a plain player; edit mode (E, or the toggle top right) adds the trim controls.
    property bool editMode: false
    // Ctrl+H: only the video is shown (an export's progress bar still appears)
    property bool controlsHidden: false

    width: 1180
    height: 760
    minimumWidth: 760
    minimumHeight: 520
    visible: true
    color: Theme.background
    title: (editor.hasMedia ? editor.fileName + " — " : "") + "ClipViewer " + Qt.application.version
    font.family: Theme.font
    font.pixelSize: 13

    // The palette follows the saved setting (Theme falls back to Graphite for an unknown name).
    Binding { target: Theme; property: "name"; value: window.editor.theme }

    // ---- Playback commands ----

    function seekTo(seconds) {
        if (!editor.hasMedia)
            return
        const t = editor.snap(Math.max(0, Math.min(editor.duration, seconds)))
        position = t
        if (scrubbing)
            scrubSeek(Math.round(t * 1000))
        else
            player.position = Math.round(t * 1000)
        if (!playing && !scrubbing)
            editor.requestStill(t)
        flashOsd()
    }

    // Scrubbing pauses, so each position shows a frame. An ffmpeg still per mouse move lags behind,
    // so the old still is hidden and the live video shown until the drag ends.
    function beginScrub() {
        resumeAfterScrub = playing
        previewing = false
        if (playing)
            player.pause()
        scrubbing = true
        editor.clearStill()
    }

    function scrubSeek(ms) {
        if (scrubSeekBusy) {
            scrubSeekPending = ms
            return
        }
        scrubSeekPending = -1
        if (ms === player.position)
            return // no new frame would come to end the seek
        scrubSeekBusy = true
        scrubSeekTimeout.restart()
        player.position = ms
    }

    // A frame arrived (or the seek timed out): start the latest waiting seek.
    function scrubSeekDone() {
        if (!scrubSeekBusy)
            return
        scrubSeekBusy = false
        scrubSeekTimeout.stop()
        if (scrubbing && scrubSeekPending >= 0)
            scrubSeek(scrubSeekPending)
    }

    function endScrub() {
        if (!scrubbing)
            return
        scrubbing = false
        scrubSeekBusy = false
        scrubSeekPending = -1
        scrubSeekTimeout.stop()
        if (resumeAfterScrub) {
            player.position = Math.round(position * 1000) // a waiting seek may not have run
            play()
        } else {
            seekTo(position)
        }
    }

    // While the controls are hidden, play/pause and seeks briefly show the time over the video.
    function flashOsd() {
        if (controlsHidden)
            osdTimer.restart()
    }

    function play() {
        if (!editor.hasMedia)
            return
        if (position >= editor.duration - editor.frameDuration)
            seekTo(0)
        editor.clearStill()
        player.play()
    }

    function pause() {
        player.pause()
        previewing = false
        seekTo(player.position / 1000) // snap, and show the exact frame
    }

    function togglePlay() {
        previewing = false
        if (playing)
            pause()
        else
            play()
        flashOsd()
    }

    function stepFrames(frames) {
        if (playing)
            player.pause()
        previewing = false
        seekTo(position + frames * editor.frameDuration)
    }

    function skip(seconds) {
        seekTo(position + seconds)
    }

    function previewCut() {
        if (!editor.hasMedia)
            return
        seekTo(editor.trimStart)
        editor.clearStill()
        previewing = true
        player.play()
    }

    function toggleEditMode() {
        if (editMode && previewing) {
            previewing = false
            pause()
        }
        editMode = !editMode
    }

    function toggleFullScreen() {
        visibility = fullScreen ? Window.Windowed : Window.FullScreen
    }

    function changeVolume(delta) {
        editor.muted = false
        editor.volume = Math.max(0, Math.min(1, editor.volume + delta))
    }

    function showOpenDialog() {
        openDialog.open()
    }

    function exportClip() {
        if (!editMode || !editor.hasMedia || editor.exporting)
            return
        saveDialog.selectedFile = editor.suggestedExportUrl()
        saveDialog.open()
    }

    MediaPlayer {
        id: player
        source: window.editor.source
        videoOutput: videoOutput
        audioOutput: AudioOutput {
            volume: window.editor.volume
            muted: window.editor.muted
        }

        onSourceChanged: {
            window.position = 0
            window.previewing = false
            if (source.toString() !== "") {
                pause() // loads the first frame without playing
                window.editor.requestStill(0)
            }
        }
        onPositionChanged: {
            if (!window.playing)
                return
            window.position = player.position / 1000
            if (window.previewing && window.position >= window.editor.trimEnd) {
                window.previewing = false
                player.pause()
                window.seekTo(window.editor.trimEnd)
            }
        }
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.EndOfMedia) {
                window.previewing = false
                window.seekTo(window.editor.duration - window.editor.frameDuration)
            }
        }
        onErrorOccurred: (error, errorString) => console.warn("Playback error:", errorString)
    }

    FileDialog {
        id: openDialog
        title: "Open video"
        nameFilters: ["Videos (*.mp4 *.webm *.mov *.avi *.mkv)", "All files (*)"]
        onAccepted: window.editor.openFile(selectedFile)
    }

    FileDialog {
        id: saveDialog
        title: "Export clip"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "mp4"
        nameFilters: ["MP4 video (*.mp4)"]
        onAccepted: window.editor.exportTo(selectedFile)
    }

    // ---- Shortcuts (as in the ClipViewer web player) ----

    Shortcut { sequences: ["Space", "K"]; onActivated: window.togglePlay() }
    Shortcut { sequence: "Left"; onActivated: window.skip(-5) }
    Shortcut { sequence: "Right"; onActivated: window.skip(5) }
    Shortcut { sequence: "J"; onActivated: window.skip(-10) }
    Shortcut { sequence: "L"; onActivated: window.skip(10) }
    Shortcut { sequence: ","; onActivated: window.stepFrames(-1) }
    Shortcut { sequence: "."; onActivated: window.stepFrames(1) }
    Shortcut { sequence: "E"; onActivated: window.toggleEditMode() }
    Shortcut { sequence: "I"; enabled: window.editMode; onActivated: window.editor.setStartHere(window.position) }
    Shortcut { sequence: "O"; enabled: window.editMode; onActivated: window.editor.setEndHere(window.position) }
    Shortcut { sequence: "P"; enabled: window.editMode; onActivated: window.previewCut() }
    // Home/End jump to the trim points in edit mode, and to the ends of the video otherwise.
    Shortcut { sequence: "Home"; onActivated: window.seekTo(window.editMode ? window.editor.trimStart : 0) }
    Shortcut {
        sequence: "End"
        onActivated: window.seekTo(window.editMode ? window.editor.trimEnd : window.editor.duration - window.editor.frameDuration)
    }
    Shortcut { sequence: "Ctrl+O"; onActivated: window.showOpenDialog() }
    Shortcut { sequence: "Ctrl+E"; enabled: window.editMode; onActivated: window.exportClip() }
    Shortcut { sequence: "M"; onActivated: window.editor.muted = !window.editor.muted }
    Shortcut { sequence: "Up"; onActivated: window.changeVolume(0.05) }
    Shortcut { sequence: "Down"; onActivated: window.changeVolume(-0.05) }
    Shortcut { sequence: "F"; onActivated: window.toggleFullScreen() }
    Shortcut { sequence: "Ctrl+H"; onActivated: window.controlsHidden = !window.controlsHidden }
    Shortcut { sequence: "Esc"; enabled: window.fullScreen; onActivated: window.toggleFullScreen() }

    Timer { id: osdTimer; interval: 1500 }
    // In case a seek never shows a frame, so scrubbing can't get stuck.
    Timer { id: scrubSeekTimeout; interval: 1000; onTriggered: window.scrubSeekDone() }

    DropArea {
        anchors.fill: parent
        onDropped: drop => {
            if (drop.hasUrls)
                window.editor.openFile(drop.urls[0])
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Top bar ----
        Rectangle {
            Layout.fillWidth: true
            visible: !window.fullScreen && !window.controlsHidden
            implicitHeight: 48
            color: Theme.chrome

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 12
                spacing: 12

                AppButton {
                    quiet: true
                    iconName: "open"
                    text: "Open"
                    toolTip: "Open a video (Ctrl+O)"
                    onClicked: window.showOpenDialog()
                }
                Text {
                    text: window.editor.fileName
                    color: Theme.text
                    font.weight: Font.Medium
                    elide: Text.ElideMiddle
                    Layout.maximumWidth: 420
                }
                Text {
                    text: window.editor.infoText
                    color: Theme.text3
                    font.family: Theme.monoFont
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                AppButton {
                    id: themeButton
                    quiet: true
                    iconName: "palette"
                    checked: themeMenu.visible
                    toolTip: "Theme"
                    onClicked: themeMenu.visible ? themeMenu.close() : themeMenu.open()

                    Popup {
                        id: themeMenu
                        y: themeButton.height + 6
                        x: themeButton.width - width
                        padding: 4
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                        background: Rectangle {
                            radius: Theme.radius
                            color: Theme.popup
                            border.color: Theme.border
                        }

                        // One row per palette, with a swatch: its background with its accent in the middle
                        Component {
                            id: themeItem
                            AppButton {
                                id: item
                                required property string modelData
                                readonly property var colors: Theme.palettes[modelData]
                                width: 160
                                quiet: true
                                leftPadding: 36
                                text: colors.label
                                checked: Theme.name === modelData
                                onClicked: {
                                    window.editor.theme = modelData
                                    themeMenu.close()
                                }

                                Rectangle {
                                    x: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 16
                                    height: 16
                                    radius: 8
                                    color: item.colors.background
                                    border.color: item.colors.controlBorderHover
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: item.colors.accent
                                    }
                                }
                            }
                        }

                        contentItem: Column {
                            spacing: 2
                            Repeater { model: Theme.darkNames; delegate: themeItem }
                            Rectangle { width: parent.width; height: 1; color: Theme.border }
                            Repeater { model: Theme.lightNames; delegate: themeItem }
                        }
                    }
                }
                AppButton {
                    iconName: "scissors"
                    text: "Edit"
                    checked: window.editMode // not checkable, so a click can't break the binding
                    toolTip: window.editMode ? "Back to the player (E)" : "Trim and export (E)"
                    onClicked: window.toggleEditMode()
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        // ---- Video ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.sunken

            VideoOutput {
                id: videoOutput
                anchors.fill: parent
                visible: window.editor.hasMedia
            }

            Connections {
                target: videoOutput.videoSink
                function onVideoFrameChanged() { window.scrubSeekDone() }
            }

            // The exact frame, decoded by ffmpeg, while paused (see CLAUDE.md, "Paused frames").
            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                source: window.editor.stillSource
                visible: !window.playing && window.editor.hasMedia && status === Image.Ready
                cache: false
                asynchronous: false
                smooth: true
            }

            MouseArea {
                anchors.fill: parent
                enabled: window.editor.hasMedia
                onPressed: mouse => {
                    // 0: toggle; 1/2: a double-click on an edge undoes the first click's toggle and seeks.
                    const action = window.editor.videoClick(mouse.x / width)
                    window.togglePlay()
                    if (action !== 0)
                        window.skip(action === 1 ? -10 : 10)
                }
            }

            // Time overlay while the controls are hidden (see flashOsd)
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 16
                width: osdRow.implicitWidth + 24
                height: osdRow.implicitHeight + 14
                radius: 6
                color: Qt.alpha(Theme.chrome, 0.8)
                opacity: window.controlsHidden && window.editor.hasMedia && osdTimer.running ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 150 } }

                Row {
                    id: osdRow
                    anchors.centerIn: parent
                    spacing: 8
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: window.playing ? "play" : "pause"
                        size: 16
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: window.editor.formatTime(window.position)
                        color: Theme.text
                        font.family: Theme.monoFont
                        font.pixelSize: 16
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "/ " + window.editor.formatTime(window.editor.duration)
                        color: Theme.text3
                        font.family: Theme.monoFont
                        font.pixelSize: 16
                    }
                }
            }

            // Progress line along the bottom while the controls are hidden; click or drag it to seek.
            // The hit area is taller than the line, which thickens on hover and shows a preview.
            Item {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 14
                visible: window.controlsHidden && window.editor.hasMedia

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: progressArea.containsMouse || progressArea.pressed ? 6 : 3
                    color: Qt.alpha(Theme.chrome, 0.6)
                    Behavior on height { NumberAnimation { duration: 100 } }

                    Rectangle {
                        height: parent.height
                        width: window.editor.duration > 0
                            ? parent.width * Math.min(1, window.position / window.editor.duration) : 0
                        color: Theme.accent
                    }
                }

                MouseArea {
                    id: progressArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    readonly property bool hovering: containsMouse && !pressed

                    function secondsAt(x) {
                        return Math.max(0, Math.min(1, x / width)) * window.editor.duration
                    }
                    onPressed: mouse => {
                        window.beginScrub()
                        window.seekTo(secondsAt(mouse.x))
                    }
                    onPositionChanged: mouse => {
                        if (pressed)
                            window.seekTo(secondsAt(mouse.x))
                        else
                            window.editor.requestThumbnail(secondsAt(mouse.x))
                    }
                    onHoveringChanged: {
                        if (hovering)
                            window.editor.requestThumbnail(secondsAt(mouseX))
                        else
                            window.editor.clearThumbnail()
                    }
                    onReleased: window.endScrub()
                    onCanceled: window.endScrub()
                }

                HoverPreview {
                    visible: progressArea.hovering
                    source: window.editor.thumbnailSource
                    time: window.editor.formatShortTime(progressArea.secondsAt(progressArea.mouseX))
                    pointerX: progressArea.mouseX
                }
            }

            // Empty state
            Column {
                anchors.centerIn: parent
                spacing: 14
                visible: !window.editor.hasMedia

                Icon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    name: "open"
                    size: 40
                    color: Theme.text3
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: window.editor.loading ? "Opening…" : "Drop a video here, or open one"
                    color: Theme.text2
                    font.pixelSize: 15
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "mp4 · webm · mov · avi · mkv"
                    color: Theme.text3
                    font.family: Theme.monoFont
                    font.pixelSize: 12
                }
                AppButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    flat: true
                    text: "Open video…"
                    enabled: window.editor.ffmpegFound && !window.editor.loading
                    onClicked: window.showOpenDialog()
                }
            }
        }

        // ---- Controls ----
        Rectangle {
            Layout.fillWidth: true
            visible: !window.controlsHidden
            implicitHeight: controls.implicitHeight + 24
            color: Theme.surface

            Rectangle { width: parent.width; height: 1; color: Theme.border }

            ColumnLayout {
                id: controls
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    spacing: 4
                    Layout.fillWidth: true

                    AppButton { quiet: true; iconName: "stepBack"; toolTip: "Previous frame (,)"; enabled: window.editor.hasMedia; onClicked: window.stepFrames(-1) }
                    AppButton {
                        quiet: true
                        iconName: window.playing ? "pause" : "play"
                        toolTip: window.playing ? "Pause (Space)" : "Play (Space)"
                        enabled: window.editor.hasMedia
                        onClicked: window.togglePlay()
                    }
                    AppButton { quiet: true; iconName: "stepForward"; toolTip: "Next frame (.)"; enabled: window.editor.hasMedia; onClicked: window.stepFrames(1) }

                    Text {
                        Layout.leftMargin: 8
                        text: window.editor.formatTime(window.position)
                        color: Theme.text
                        font.family: Theme.monoFont
                    }
                    Text {
                        text: "/ " + window.editor.formatTime(window.editor.duration)
                        color: Theme.text3
                        font.family: Theme.monoFont
                    }

                    Item { Layout.fillWidth: true }

                    AppButton {
                        quiet: true
                        iconName: window.editor.muted || window.editor.volume === 0 ? "muted" : "volume"
                        toolTip: window.editor.muted ? "Unmute (M)" : "Mute (M)"
                        enabled: window.editor.hasMedia && window.editor.hasAudio
                        onClicked: window.editor.muted = !window.editor.muted
                    }
                    Slider {
                        id: volumeSlider
                        implicitWidth: 96
                        enabled: window.editor.hasMedia && window.editor.hasAudio
                        from: 0
                        to: 1
                        value: window.editor.muted ? 0 : window.editor.volume
                        focusPolicy: Qt.NoFocus
                        onMoved: {
                            window.editor.muted = false
                            window.editor.volume = value
                        }
                        background: Rectangle {
                            x: volumeSlider.leftPadding
                            y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                            width: volumeSlider.availableWidth
                            height: 4
                            radius: 2
                            color: Theme.raised
                            Rectangle {
                                width: volumeSlider.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: volumeSlider.enabled ? Theme.text2 : Theme.disabledText
                            }
                        }
                        handle: Rectangle {
                            x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                            y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                            width: 12
                            height: 12
                            radius: 6
                            color: volumeSlider.enabled ? Theme.text : Theme.disabledText
                        }
                    }
                    AppButton {
                        quiet: true
                        iconName: window.fullScreen ? "exitFullscreen" : "fullscreen"
                        toolTip: window.fullScreen ? "Exit full screen (F)" : "Full screen (F)"
                        onClicked: window.toggleFullScreen()
                    }

                    AppButton {
                        Layout.leftMargin: 8
                        visible: window.editMode
                        iconName: "preview"
                        text: "Preview cut"
                        toolTip: "Play the kept range (P)"
                        enabled: window.editor.hasMedia
                        onClicked: window.previewCut()
                    }
                }

                TrimTimeline {
                    Layout.fillWidth: true
                    duration: window.editor.duration
                    position: window.position
                    trimStart: window.editor.trimStart
                    trimEnd: window.editor.trimEnd
                    trimming: window.editMode
                    thumbnailSource: window.editor.thumbnailSource
                    formatTime: seconds => window.editor.formatShortTime(seconds)

                    onHoverMoved: seconds => window.editor.requestThumbnail(seconds)
                    onHoverEnded: window.editor.clearThumbnail()
                    onSeekRequested: seconds => window.seekTo(seconds)
                    onScrubStarted: window.beginScrub()
                    onScrubFinished: window.endScrub()
                    // A paused handle drag follows the handle like a scrub; while playing it only moves the handle.
                    onHandleDragStarted: {
                        if (!window.playing)
                            window.beginScrub()
                    }
                    onHandleDragFinished: window.endScrub()
                    onTrimStartDragged: seconds => {
                        window.editor.trimStart = seconds
                        if (!window.playing)
                            window.seekTo(window.editor.trimStart)
                    }
                    onTrimEndDragged: seconds => {
                        window.editor.trimEnd = seconds
                        if (!window.playing)
                            window.seekTo(window.editor.trimEnd)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: window.editMode
                    spacing: 8

                    AppButton {
                        iconName: "markStart"
                        toolTip: "Set start to the current time (I)"
                        enabled: window.editor.hasMedia
                        onClicked: window.editor.setStartHere(window.position)
                    }
                    Text { text: "Start"; color: Theme.text2 }
                    TimeField {
                        enabled: window.editor.hasMedia
                        value: window.editor.trimStart
                        format: seconds => window.editor.formatTime(seconds)
                        commit: text => window.editor.setTrimStartText(text)
                    }

                    Text { text: "End"; color: Theme.text2; Layout.leftMargin: 12 }
                    TimeField {
                        enabled: window.editor.hasMedia
                        value: window.editor.trimEnd
                        format: seconds => window.editor.formatTime(seconds)
                        commit: text => window.editor.setTrimEndText(text)
                    }
                    AppButton {
                        iconName: "markEnd"
                        toolTip: "Set end to the current time (O)"
                        enabled: window.editor.hasMedia
                        onClicked: window.editor.setEndHere(window.position)
                    }

                    Text {
                        Layout.leftMargin: 12
                        Layout.fillWidth: true
                        text: window.editor.clipSummary
                        color: Theme.text3
                        font.family: Theme.monoFont
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    // Export mode, and the re-encode settings next to it
                    SegmentedControl {
                        value: window.editor.smartCut
                        model: [
                            { label: "Smart cut", value: true, icon: "zap",
                              toolTip: "Frame-accurate and near-instant: re-encodes only the ends, copies the rest" },
                            { label: "Re-encode", value: false, icon: "reencode",
                              toolTip: "Re-encode everything (x264): slower, usually a smaller file" }
                        ]
                        onActivated: value => window.editor.smartCut = value
                    }
                    AppButton {
                        id: reencodeButton
                        Layout.leftMargin: -4
                        iconName: "sliders"
                        checked: reencodeMenu.visible
                        toolTip: "Re-encode settings"
                        // Opening the settings picks re-encoding, which is what they apply to.
                        onClicked: {
                            if (reencodeMenu.visible) {
                                reencodeMenu.close()
                            } else {
                                window.editor.smartCut = false
                                reencodeMenu.open()
                            }
                        }

                        // Marks settings changed from the defaults
                        Rectangle {
                            visible: !window.editor.reencodeIsDefault
                            x: reencodeButton.width - width - 4
                            y: 4
                            width: 6
                            height: 6
                            radius: 3
                            color: Theme.accent
                        }

                        ReencodeSettings {
                            id: reencodeMenu
                            editor: window.editor
                            x: reencodeButton.width - width
                            y: -reencodeMenu.height - 8
                        }
                    }

                    AppButton {
                        flat: true
                        iconName: "export"
                        text: "Export…"
                        toolTip: "Export the kept range (Ctrl+E)"
                        enabled: window.editor.hasMedia && !window.editor.exporting
                        onClicked: window.exportClip()
                    }
                }
            }
        }

        // ---- Status bar: only while there's a message or an export ----
        Rectangle {
            Layout.fillWidth: true
            visible: window.editor.exporting
                || (!window.fullScreen && !window.controlsHidden && window.editor.status.length > 0)
            implicitHeight: 34
            color: window.editor.ffmpegFound ? Theme.chrome : Theme.warningSoft

            Rectangle { width: parent.width; height: 1; color: Theme.border }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: window.editor.exporting
                        ? "Exporting… " + Math.round(window.editor.exportProgress * 100) + "%"
                        : window.editor.status
                    color: window.editor.ffmpegFound ? Theme.text2 : Theme.warning
                    elide: Text.ElideRight
                }

                ProgressBar {
                    id: progress
                    visible: window.editor.exporting
                    value: window.editor.exportProgress
                    implicitWidth: 220
                    implicitHeight: 6
                    background: Rectangle { radius: 3; color: Theme.raised }
                    contentItem: Item {
                        Rectangle {
                            width: progress.visualPosition * parent.width
                            height: parent.height
                            radius: 3
                            color: Theme.accent
                        }
                    }
                }
                AppButton {
                    visible: window.editor.exporting
                    quiet: true
                    iconName: "close"
                    text: "Cancel"
                    implicitHeight: 26
                    onClicked: window.editor.cancelExport()
                }
                // The missing-FFmpeg warning stays; other messages can be dismissed
                AppButton {
                    visible: !window.editor.exporting && window.editor.ffmpegFound
                    quiet: true
                    iconName: "close"
                    toolTip: "Dismiss"
                    implicitHeight: 26
                    onClicked: window.editor.clearStatus()
                }
            }
        }
    }
}
