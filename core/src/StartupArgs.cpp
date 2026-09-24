#include "StartupArgs.h"

#include <QDir>

namespace cv {

StartupArgs StartupArgs::parse(const QStringList &args)
{
    StartupArgs result;
    for (const QString &arg : args) {
        if (arg.compare(QLatin1String(EditorFlag), Qt::CaseInsensitive) == 0 || arg == QLatin1String("-e"))
            result.page = StartupPage::Editor;
        else if (arg.compare(QLatin1String(ClipsFlag), Qt::CaseInsensitive) == 0)
            result.page = StartupPage::Clips;
        else if (!arg.startsWith(QChar('-')) && result.videoPath.isEmpty())
            result.videoPath = arg;
    }
    if (!result.videoPath.isEmpty())
        result.page = StartupPage::Editor;
    return result;
}

QStringList StartupArgs::makePathsAbsolute(const QStringList &args, const QString &workingDirectory)
{
    const QDir dir(workingDirectory);
    QStringList result;
    result.reserve(args.size());
    for (const QString &arg : args)
        result << (arg.startsWith(QChar('-')) ? arg : QDir::toNativeSeparators(QDir::cleanPath(dir.absoluteFilePath(arg))));
    return result;
}

} // namespace cv
