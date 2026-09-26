#pragma once

#include "media/ReencodeOptions.h"
#include "subtitles/SubtitleStyle.h"

#include <QJsonObject>
#include <QString>

namespace cv {

// User preferences, stored as camelCase JSON. Missing or invalid fields fall back to the defaults;
// keys this build doesn't know are kept when saving, so older and newer builds can share the file.
struct AppSettings
{
    double volume = 1.0; // 0-1
    bool muted = false;
    // Export mode: smart cut (true) or re-encode.
    bool smartCut = true;
    // Settings for re-encoding (the menu next to the export mode).
    ReencodeOptions reencode;
    // Where the last export went; the save dialog starts there. Empty: next to the source video.
    QString lastExportFolder;
    // Where the last saved frame (PNG) went. Empty: next to the source video.
    QString lastFrameFolder;
    // Colour palette name (Theme.qml); the UI falls back to Graphite for a name it doesn't know.
    // Any palette goes with any style.
    QString theme = QStringLiteral("graphite");
    // Shape of the controls (Theme.qml): graphite, material, fluent or adwaita. The UI falls back to
    // Graphite for a name it doesn't know.
    QString style = QStringLiteral("graphite");
    // Smaller controls and bars.
    bool compact = false;
    // Keeps the window above other windows.
    bool alwaysOnTop = false;
    // Clicking the video plays/pauses, a double-click on an edge seeks (PlayerClickGesture).
    bool clickControls = true;
    // The double-click on an edge; off leaves every click a plain play/pause.
    bool edgeDoubleClick = true;
    // Seek steps in whole seconds: the short one for Left/Right, the long one for J/L and the
    // double-click on an edge.
    int shortSkip = 5; // 1-60
    int longSkip = 10; // 1-600
    // Start playing as soon as a video opens, instead of showing its first frame.
    bool autoplay = false;
    // Subtitle size, background and position.
    SubtitleStyle subtitles;

    // %LOCALAPPDATA%\ClipViewerDesktop\settings.json on Windows.
    static QString defaultPath();

    // Reads the settings, or returns the defaults if the file is missing or unreadable.
    static AppSettings load(const QString &path);

    // Writes the settings, replacing the file only once the new one is complete. Returns false if
    // it couldn't be written.
    bool save(const QString &path) const;

    bool operator==(const AppSettings &other) const;

private:
    QJsonObject m_unknown; // fields from the file this build doesn't use
};

} // namespace cv
