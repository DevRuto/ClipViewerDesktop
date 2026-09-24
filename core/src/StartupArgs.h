#pragma once

#include <QString>
#include <QStringList>

namespace cv {

// The command line: an optional video path to open ("Open with", drag onto the exe). Flags
// (anything starting with '-') are ignored.
struct StartupArgs
{
    QString videoPath; // empty when none was given

    static StartupArgs parse(const QStringList &args);

    // Makes relative video paths absolute, so the arguments still mean the same thing when handed
    // to an already running instance with a different working directory.
    static QStringList makePathsAbsolute(const QStringList &args, const QString &workingDirectory);

    bool operator==(const StartupArgs &) const = default;
};

} // namespace cv
