# ClipViewer Desktop

A local video player with a trim editor. You can open a video, set the start and end points to
the exact frame, and export that range to a new MP4. It's written in C++20 with Qt 6 (Qt Quick).
Export and still frames use FFmpeg, run as an external program.

## Requirements

- Qt 6.10 (MinGW 64-bit), with the Qt Multimedia, Qt Shader Tools and Qt Image Formats modules
- MinGW 13.1
- CMake 3.25 or later, and Ninja
- FFmpeg (`ffmpeg` and `ffprobe`), needed at runtime and by the integration tests

The CMake presets expect Qt in `C:\Qt\6.10.3\mingw_64` and MinGW in `C:\Qt\Tools\mingw1310_64`.
One way to install them:

```powershell
pip install --user aqtinstall cmake ninja
python -m aqt install-qt windows desktop 6.10.3 win64_mingw -m qtmultimedia qtshadertools qtimageformats -O C:\Qt
python -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O C:\Qt
```

If your toolchain is somewhere else, override `QT_ROOT` / `MINGW_ROOT` in a
`CMakeUserPresets.json` that inherits from the `debug` or `release` preset.

## Development

```powershell
cmake --preset debug             # configure into build\debug
cmake --build --preset debug     # build
ctest --preset debug             # run the tests
```

The presets put Qt and MinGW on PATH for building and testing, but not for running the exe.
Add Qt's `bin` folder to PATH before you start the app:

```powershell
$env:PATH = "C:\Qt\6.10.3\mingw_64\bin;$env:PATH"
.\build\debug\ClipViewerDesktop.exe path\to\video.mp4
```

Qt sends its log to the debugger on Windows. Set `QT_FORCE_STDERR_LOGGING=1` to print QML
warnings and errors to the console.

The code is split into three parts: `core/` is the UI-free library, with the media and FFmpeg
logic. `app/` is the Qt Quick executable. `tests/` holds the Qt Test suites.

## Production build

```powershell
cmake --preset release
cmake --build --preset release
cmake --install build\release --prefix dist
```

`dist\bin` is then a self-contained folder: `ClipViewerDesktop.exe` along with Qt's DLLs, QML
modules and plugins. You can zip it and ship it as is.

FFmpeg is not bundled. The app looks for `ffmpeg.exe` and `ffprobe.exe` in this order:

1. `ffmpeg\` or `ffmpeg\bin\` next to the exe
2. `%LOCALAPPDATA%\ClipViewerDesktop\ffmpeg`
3. PATH

For a release that works on its own, copy an FFmpeg build into `dist\bin\ffmpeg\`. Without
FFmpeg, the app still starts and plays videos, but it can't export and shows a warning in the
status bar.
