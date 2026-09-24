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

    // ---- Playback commands ----

    function seekTo(seconds) {
        if (!editor.hasMedia)
            return
        const t = editor.snap(Math.max(0, Math.min(editor.duration, seconds)))
        position = t
        player.position = Math.round(t * 1000)
        if (!playing)
            editor.requestStill(t)
        flashOsd()
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
            // The hit area is taller than the line, which thickens on hover.
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

                    function seekToMouse(x) {
                        window.seekTo(Math.max(0, Math.min(1, x / width)) * window.editor.duration)
                    }
                    function finishScrub() {
                        if (window.resumeAfterScrub)
                            window.play()
                    }

                    // Paused while dragging, as on the timeline, so each position shows its exact frame
                    onPressed: mouse => {
                        window.resumeAfterScrub = window.playing
                        window.previewing = false
                        if (window.playing)
                            player.pause()
                        seekToMouse(mouse.x)
                    }
                    onPositionChanged: mouse => {
                        if (pressed)
                            seekToMouse(mouse.x)
                    }
                    onReleased: finishScrub()
                    onCanceled: finishScrub()
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

                    onSeekRequested: seconds => window.seekTo(seconds)
                    onScrubStarted: {
                        window.resumeAfterScrub = window.playing
                        window.previewing = false
                        if (window.playing)
                            player.pause()
                    }
                    onScrubFinished: {
                        if (window.resumeAfterScrub)
                            window.play()
                    }
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

                    Text {
                        Layout.leftMargin: 12
                        Layout.fillWidth: true
                        text: window.editor.clipSummary
                        color: Theme.text3
                        font.family: Theme.monoFont
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    // Export mode, as a segmented pair
                    Row {
                        spacing: 0
                        AppButton {
                            text: "Smart cut"
                            checkable: true
                            checked: window.editor.smartCut
                            toolTip: "Frame-accurate and near-instant: re-encodes only the ends, copies the rest"
                            onClicked: window.editor.smartCut = true
                        }
                        AppButton {
                            text: "Re-encode"
                            checkable: true
                            checked: !window.editor.smartCut
                            toolTip: "Re-encode everything (x264): slower, usually a smaller file"
                            onClicked: window.editor.smartCut = false
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
