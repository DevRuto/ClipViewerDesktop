# ClipViewer (desktop): app purpose

## What it is

A local **video player with a trim editor**. You open a video on your computer, watch it, pick a
start and end point frame-accurately, and save that part as a new file. Nothing else: no
accounts, no uploading, no cloud, no library or clip browser. It works fully offline.

The main use case is cutting a short moment (a gameplay highlight, a meeting excerpt) out of a
long recording quickly, without quality loss and without a full video editor.

## Playing

- Open a local video from the Open dialog, by dragging it onto the window, or from the command
  line / "Open with" (`ClipViewerDesktop.exe <video>`). Formats: mp4, webm, mov, avi and mkv
  (whatever FFmpeg decodes also works).
- Play/pause, seek by clicking or dragging the timeline, skip 5 s / 10 s, and step one frame at a
  time. The current time is shown to the millisecond.
- Volume and mute (remembered between runs), and full screen.
- Clicking the video plays/pauses. Double-clicking its left or right 30% seeks 10 s back or
  forward, like on common web players.

## Editing (trimming)

- Set the start and end points by dragging the amber handles on the timeline, with I / O at the
  playhead, or by typing a time (`75.5`, `1:15.5` or `0:01:15.500`). Points snap to frames.
- While paused, the picture is the exact frame at the playhead, the same frame the exported
  clip will start with.
- Preview cut plays just the kept range.
- The length, frame count and an estimated file size of the kept range are shown.

## Exporting

- Export writes the kept range to a new MP4 (H.264). The source file is never modified.
- **Smart cut** (default): frame-accurate and near-instant. Only the few frames around each cut
  point are re-encoded; everything in between is copied untouched, so quality and size match the
  source. It needs 8-bit H.264 input; anything else automatically falls back to re-encoding and
  the app says why.
- **Re-encode**: everything is re-encoded (x264 + AAC). Slower, but usually a much smaller file.
- Progress is shown and the export can be cancelled; a cancelled or failed export leaves no
  partial file behind.

## Principles

- **Stable above all.** A bad file, a missing FFmpeg, a failed export or an unwritable settings
  file must produce a clear message, never a crash or a frozen window.
- Fast: opening, seeking and smart-cut exports should feel instant.
- Keyboard-friendly: every common action has a shortcut.
- Cross-platform (Windows first).

## Shortcuts

| Key | Action |
| --- | --- |
| Space / K | Play / pause |
| ← / → | Back / forward 5 s |
| J / L | Back / forward 10 s |
| , / . | Previous / next frame |
| ↑ / ↓ | Volume up / down |
| M | Mute |
| F | Full screen (Esc leaves it) |
| I / O | Set start / end at the playhead |
| Home / End | Jump to start / end point |
| P | Preview cut |
| Ctrl+O | Open |
| Ctrl+E | Export |
