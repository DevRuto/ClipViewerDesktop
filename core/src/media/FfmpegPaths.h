#pragma once

#include <QString>

#include <optional>

namespace cv {

// Resolved locations of the ffmpeg and ffprobe executables.
struct FfmpegPaths
{
    static constexpr auto DirectoryEnvVar = "CLIPVIEWER_FFMPEG_DIR";

    QString ffmpeg;
    QString ffprobe;

    // Where the app installs its own FFmpeg: %LOCALAPPDATA%\ClipViewerDesktop\ffmpeg on Windows.
    // Per-user and writable, unlike the app folder.
    static QString managedDirectory();

    // Finds ffmpeg/ffprobe, in order: configuredDirectory, the CLIPVIEWER_FFMPEG_DIR environment
    // variable, an ffmpeg/ (or ffmpeg/bin/) folder next to the app, managedDirectory(), then PATH.
    // Both tools must be in the same directory.
    static std::optional<FfmpegPaths> locate(const QString &configuredDirectory = {});

    // The tools in directory, or nothing unless both are there.
    static std::optional<FfmpegPaths> inDirectory(const QString &directory);
};

} // namespace cv
