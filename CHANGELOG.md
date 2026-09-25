# Changelog

All notable changes to ClipViewer Desktop are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/DevRuto/ClipViewerDesktop/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/DevRuto/ClipViewerDesktop/releases/tag/v0.1.0
