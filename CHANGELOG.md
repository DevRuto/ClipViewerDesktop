# Changelog

All notable changes to ClipViewer Desktop are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.6.1] - 2026-09-26

### Added

- Each run is logged to `logs\app.log` in `%LOCALAPPDATA%\ClipViewerDesktop`, keeping the last
  four runs. A crash writes a stack trace to the log, plus a minidump on Windows, and the next
  start shows a notice. The README explains where to find them.
- Tests that play every supported format (mp4, mov, mkv, webm, avi; H.264, HEVC, VP8, VP9, AV1,
  MPEG-4) through Qt Multimedia.

### Fixed

- The subtitle tests no longer fail with ffmpeg 6.x.

## [0.6.0] - 2026-09-26

### Added

- Playback speed control ([ and ]).
- Audio and subtitle track menu (B and V).
- Subtitles drawn by the app: `.srt`, `.ass`, `.ssa` and `.vtt` files next to the video, loaded
  from the menu or dropped on the window, embedded text tracks and DVD picture subtitles.
- Subtitle settings: size, background (box or outline) and position, saved in the settings, and a
  per-video timing offset (G and H).
- Always-on-top toggle (Ctrl+T), kept in the settings.
- Save the current frame as a full-size PNG (Ctrl+S).
- Media info panel with each stream's codec, size, bitrate and language.
- Aspect ratio, zoom and rotation for the view (A, Z, R).
- Hotkeys section and new screenshots in the README.

### Changed

- The tracks button is always shown, and a menu button's tooltip is hidden while its menu is open.
- Only one popup is open at a time.
- Linux CI tests against the same pinned FFmpeg build as the release.

### Fixed

- Playback errors show in the status bar.
- Pinning the window on top no longer removes the title bar.
- Garbled characters in the UI.
- Picture subtitle tracks no longer crash Qt's FFmpeg backend.

## [0.5.0] - 2026-09-25

### Added

- Frame preview when hovering over the timeline or the progress line in hidden-controls mode.
- Buttons to set the trim start and end to the current time.
- Re-encode options: quality, encoder speed, resolution limit, frame rate limit and audio.

### Changed

- The export mode is now a segmented switch, and the re-encode options live in a settings popup
  next to it.
- Each trim button sits next to its time field, grouped under a small floating label, with the
  set-start button to the left of the start field.

## [0.4.0] - 2026-09-25

### Changed

- Scrubbing the timeline or dragging a trim handle shows the live video, one seek at a time.

## [0.3.0] - 2026-09-24

### Added

- Theme switcher with three dark palettes (Graphite, Slate, Plum) and three light ones (Paper,
  Mist, Sage). The chosen theme is saved in the settings.
- MIT license.
- Code signing note in the README.

## [0.2.0] - 2026-09-24

### Added

- Ctrl+H hides the UI controls and shows only the video.
- While the controls are hidden, the time shows briefly over the video on play, pause and seek,
  with a progress line along the bottom. The progress line stays visible and can be dragged to
  seek.
- The app version shows in the title bar, taken from the git tag.
- Screenshots of the player and the trim editor in the README.

### Changed

- The app opens as a plain player; the trim controls are behind an Edit mode toggle (E).
- The status bar is hidden when there's no message, and messages can be dismissed.
- The cursor over the video is the normal arrow; the hand is kept for the progress line.
- The version comes from git tags instead of a number in `CMakeLists.txt`.
- CI test workflows run only on branch pushes that change build or test inputs.

## [0.1.0] - 2026-09-24

First release.

### Added

- Video player with a frame-accurate trim editor built on Qt Quick and Qt Multimedia.
- Export of the trimmed range to a new MP4, by smart cut (stream copy with re-encoded edges) or a
  full re-encode, using FFmpeg as an external process.
- Paused frames decoded by FFmpeg, so the still at the start point matches the exported clip's
  first frame.
- Release builds bundle a pinned, checksum-verified FFmpeg; Windows packages leave out unused Qt
  parts.
- CMake presets for Linux and macOS (untested on real machines).
- GitHub Actions: tests on Windows and Linux, and installers plus portable packages for every
  platform when a version tag is pushed.
- README with build and packaging steps.
- Qt 6.12.0, installed with a pinned aqtinstall commit until 3.4.0 is released.

### Removed

- The set-at-playhead buttons from the trim bar.

[Unreleased]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.6.1...HEAD
[0.6.1]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.6.0...v0.6.1
[0.6.0]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/DevRuto/ClipViewerDesktop/releases/tag/v0.1.0
