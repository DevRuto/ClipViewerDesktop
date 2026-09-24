# CLAUDE.md

This file guides Claude Code when working in this repository (the `main` branch).

## Project

A local **video player with a trim editor**, in C++ and Qt. Open a video, watch it, set start and
end points frame-accurately, export that range to a new MP4. The keyboard shortcuts are defined in
`app/qml/Main.qml`.

**Scope: a video player with video-edit functionality, nothing else.** No server, accounts,
upload, clip browser or library. Don't add network code or features outside playing and editing
local videos unless the user asks.

**Stability is the top priority.** A bad file, a missing FFmpeg, a failed export or an
unwritable settings file must show a clear message, never crash or freeze the window. In practice:

- Nothing blocking on the UI thread. Probes, stills and exports run on the thread pool
  (`runInBackground` in `EditorController.cpp`); results come back through a `QPointer` guard.
- Exceptions never cross a thread or the QML boundary. Worker lambdas catch everything and return
  a result struct.
- Every long operation is cancellable (`CancelToken`) and cleans up after itself (partial output,
  the smart cut's parts folder).
- Validate input from QML, files and settings (clamp, fall back to defaults).
- After a change, build, run `.\dev.ps1 test`, and launch the app on a real video, as well as a
  missing file and a non-video file.

This branch is a rewrite of an earlier .NET/Avalonia app, which is on `avalonia` in the same repo
(`ClipViewerDesktop.Core/Media/*.cs` there is the reference for the ffmpeg logic). This branch
started with no history of its own (orphan branch); don't merge `avalonia` into it.

## Stack

- **C++20, Qt 6.10** (built and tested with 6.10.3 + MinGW 13.1 on Windows). CMake + Ninja.
- **Qt Quick / QML** UI with **Qt Quick Controls (Basic style)**, restyled as **Graphite**:
  near-black neutral surfaces, one amber accent, 6 px corners, Geist / Geist Mono fonts (embedded,
  OFL, loaded in `main.cpp`).
  - Colours are tokens in the `Theme` singleton (`app/qml/Theme.qml`): `background`, `surface`,
    `raised`, `border`, `text`/`text2`/`text3`, `accent`, … Use them in views, never literal
    colours. Grey text uses `text2`/`text3`, not `opacity`. Monospace is `Theme.monoFont`.
  - Buttons are `AppButton`. The default is raised and outlined. `flat` is the amber main action
    (`flat` + `danger` for a destructive one), `quiet` is borderless (toolbars), and `checkable`
    + `checked` gives a segmented choice. Buttons don't take focus, so keyboard shortcuts keep
    working.
  - Icons are `Icon { name: "play" }`: 24×24 stroked SVG paths in `Icon.qml`. Add new ones there.
- **Qt Multimedia** (`MediaPlayer` + `VideoOutput`, FFmpeg backend) for playback. Unlike the old
  LibVLC version, QML can draw over the video.
- **FFmpeg as an external process** (`QProcess`, `core/src/media/Process.h`) for probing
  (ffprobe), exporting and still frames. Qt's bundled FFmpeg libraries are only for playback.
- **Qt Test** for tests.

## Layout

- `core/`: the `cvcore` static library. It has no QML or widgets (QtCore, plus QtGui for
  `QImage`), so it can be tested headless. Namespace `cv`.
  - `TimeFormat`: timestamps.
  - `StartupArgs`: the command line, an optional video path.
  - `PlayerClickGesture`: click to play/pause, double-click an edge to seek.
  - `AppSettings`: `settings.json` in `%LOCALAPPDATA%\ClipViewerDesktop`. Unknown keys survive
    saving.
  - `media/`: `Process` (`runProcess`/`runTool`, `CancelToken`, error types), `FfmpegPaths`,
    `MediaProbe`, `FrameGrabber`, `SmartCutPlan` and `ClipExporter`.
- `app/`: the executable (`ClipViewerDesktop.exe`), a QML module with URI `ClipViewer`.
  - `src/EditorController`: all player/editor state and commands exposed to QML (`QML_ELEMENT`,
    passed in as the `editor` required property). `src/StillFrameProvider`: serves the paused
    still frame as `image://still/<n>`. `src/main.cpp`: fonts, style, startup.
  - `qml/Main.qml`: the window, which owns playback (`MediaPlayer`), the shortcuts and the layout.
    `TrimTimeline.qml`, `TimeField.qml`, `AppButton.qml`, `Icon.qml`, `Theme.qml`.
  - `assets/`: fonts and icons.
- `tests/`: one Qt Test executable per `tst_*.cpp`. `tst_ffmpegintegration` generates sample
  clips with ffmpeg's `lavfi` sources and checks real exports frame by frame (framemd5). It
  skips itself when ffmpeg isn't found.

## Commands

The toolchain is Qt 6.10.3 (MinGW) in `C:\Qt\6.10.3\mingw_64` and MinGW 13.1 in
`C:\Qt\Tools\mingw1310_64`, installed with aqtinstall. CMake and Ninja come from pip. To
reinstall:

```powershell
pip install --user aqtinstall cmake ninja
python -m aqt install-qt windows desktop 6.10.3 win64_mingw -m qtmultimedia qtshadertools qtimageformats -O C:\Qt
python -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O C:\Qt
```

`dev.ps1` puts the toolchain on PATH and wraps CMake (set `QT_ROOT`/`MINGW_ROOT` to override the
locations):

- Build: `.\dev.ps1 build` (Debug, `build\debug`); `-Config release` for Release.
- Test: `.\dev.ps1 test`.
- Run: `.\dev.ps1 run -- <video path>`.
- Package: `.\dev.ps1 dist -Config release` gives a runnable folder in `dist\bin`, with Qt's DLLs,
  QML modules and plugins (via `qt_generate_deploy_qml_app_script`) and FFmpeg in `bin\ffmpeg`.
  The release presets set `CLIPVIEWER_BUNDLE_FFMPEG`, and `cmake/BundleFfmpeg.cmake` downloads a
  pinned build at configure time and checks its SHA-256 (gyan.dev on Windows, Martin Riedl's builds
  on Linux/macOS). `CLIPVIEWER_FFMPEG_DIR` bundles a local copy instead. To bump the version,
  update the version, URLs and hashes together. On Windows the deploy step leaves out Qt parts the
  app never loads (translations, `opengl32sw.dll`, the non-Basic Controls styles, the QML debug and
  other unused plugins; see `app/CMakeLists.txt`). If the app starts using one, remove it from
  that list.
- Linux/macOS use the `unix-debug` / `unix-release` presets, with Qt found through `QT_ROOT`.
- Plain CMake also works: `cmake --preset debug`, `cmake --build --preset debug`,
  `ctest --preset debug`. The presets (`CMakePresets.json`) set the compiler and the PATH for
  builds and tests, but not for running the exe outside `dev.ps1`: put
  `C:\Qt\6.10.3\mingw_64\bin` on PATH first.
- Qt logs go to the debugger on Windows. Set `QT_FORCE_STDERR_LOGGING=1` to see QML warnings
  and errors on the console.

## Smart cut gotchas (verified in the .NET app, ported as-is; don't undo these)

These live in `ClipExporter` + `SmartCutPlan`, and `tst_ffmpegintegration` checks them. Parts
are MPEG-TS files in a hidden `.<name>.parts-<guid>` folder next to the output, cut in parallel
(one `std::async` per part), joined with the concat demuxer, then muxed with the audio.

- **Copied part is cut by pts, not by -ss/-t:** on B-frame streams ffmpeg seeks a bit before
  `-ss` (so a stream copy starts a whole GOP early), and `-t` cuts by decode time (letting in the
  next keyframe). The `noise=drop=lt(pts*tb,…)+gte(pts*tb,…)` bitstream filter drops everything
  outside the segment. Its pts are relative to the `-ss` value.
- **Copy must end on a keyframe**, or B-frames before the cut lose their references; hence the
  re-encoded tail. Assumes closed GOPs (x264/OBS/ShadowPlay defaults).
- **Encoded parts seek exactly** to the trim point or the keyframe's probed time. An off-grid
  seek skews the timestamps x264 gets and corrupts the joins. They stop half a frame before their
  end instead of relying on a float-exact `-t`.
- **MPEG-TS, not raw .h264, for the parts:** raw H.264 loses timestamps and frames were dropped at
  the joins. TS also carries each part's SPS/PPS in-band.
- **Audio is copied with a two-step seek** (input seek 5 s early, then an output seek), because
  an input seek alone snaps audio to the video keyframe (~0.5 s early). The copy's start offset
  (packet boundary + AAC priming) is lost when muxing to MP4, so it's restored with `-itsoffset`.
  Checked in sync to within 0.4 ms by cross-correlating against the source.
- Keyframe times come from packet flags (`ffprobe -show_entries packet=pts_time,flags`) in 20 s
  windows at each end, converted from absolute to `-ss` time by subtracting `format.start_time`.
- Re-encode mode is x264 `veryfast` CRF 18 + AAC. `fast` CRF 20 gave the same quality in twice
  the time. The AMD hardware encoder and parallel chunked encodes were both slower.
- Numbers passed to ffmpeg go through `ClipExporter::formatSeconds` (invariant, 3–6 decimals).

## Playback and paused frames

- The playhead and trim points snap to the frame grid (`EditorController::snap`, constant frame
  rate assumed).
- **Paused frames come from ffmpeg:** while paused, `Main.qml` shows the still decoded by
  `FrameGrabber` over the video (latest request wins). It uses the same `-ss` input seek as the
  export's re-encoded parts, so the still at the start point is exactly the exported clip's first
  frame. Qt Multimedia's own paused seeks haven't been checked for frame accuracy. If they turn
  out exact, the still could go, but compare against the export first.
- `MediaPlayer` state stays in QML. `window.position` is the playhead: the player's position
  while playing, and the frame we sought to while paused.
- Clicking the video: `EditorController::videoClick` returns the `PlayerClickGesture` action.
  A double-click on an edge undoes the first click's toggle, then seeks 10 s.

## Qt pitfalls hit so far

- **moc and raw string literals:** a `R"(...)"` literal containing backslashes made moc write an
  empty `.moc` file, which fails later at link time ("undefined reference to vtable"). Use
  ordinary escaped strings in files with `Q_OBJECT`.
- **QML_ELEMENT headers:** the generated type registration includes them by file name, so
  `app/src` is on the include path (`target_include_directories`).
- **`runProcess` and cancellation:** a short ffmpeg run can exit within one poll, so a cancel
  requested from the output callback is checked again after the last output is read.
- `QProcess` blocking calls (`waitFor*`) are only used on worker threads.

## Roadmap (not done yet)

- Release builds bundle FFmpeg. A dev build without FFmpeg shows a warning in the status bar.
  An in-app download (the .NET app fetched gyan.dev's build into
  `%LOCALAPPDATA%\ClipViewerDesktop\ffmpeg`, which `FfmpegPaths` still checks) is only needed if
  the bundled copy ever goes.
- One running copy per user: later launches hand their video path to it
  (`StartupArgs::makePathsAbsolute` is ready for that).
- Remember the window's size and position; a recent files list.
- A playback speed control; a thumbnail preview when hovering over the timeline.
- Check the full format list (mp4 H.264/HEVC, mov, mkv, webm VP8/VP9/AV1, avi) against Qt
  Multimedia's FFmpeg backend.
- Try the macOS/Linux builds on real machines. The presets, deployment and FFmpeg bundling are in
  place, but only Windows has been built and run. Sign and notarize the macOS bundle.

## Conventions

- Match the surrounding style: 4-space indent, `m_` members, `QStringLiteral` for literals, a
  short comment on anything non-obvious, and no comments restating the code.
- Keep UI-free logic in `core/` with a test. `EditorController` should stay thin glue.
- Never modify the source video. Exports go to a new file (validated in `ClipExporter::validate`).
- Commit as you go, in small logical commits.
