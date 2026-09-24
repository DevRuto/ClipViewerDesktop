#pragma once

#include <QString>
#include <QStringList>

namespace cv {

// Which window a launch asks for.
enum class StartupPage {
    Clips,  // the clip browser, the app's main page
    Editor, // the trim editor, optionally with a video to open
};

// The command line: no arguments opens the clip browser; --editor (or -e) opens the editor;
// --clips the browser; a file path opens that video in the editor ("Open with").
struct StartupArgs
{
    static constexpr auto EditorFlag = "--editor";
    static constexpr auto ClipsFlag = "--clips";

    StartupPage page = StartupPage::Clips;
    QString videoPath; // empty when none was given

    static StartupArgs parse(const QStringList &args);

    // Makes relative video paths absolute, so the arguments still mean the same thing when handed
    // to an already running instance with a different working directory.
    static QStringList makePathsAbsolute(const QStringList &args, const QString &workingDirectory);

    bool operator==(const StartupArgs &) const = default;
};

} // namespace cv
