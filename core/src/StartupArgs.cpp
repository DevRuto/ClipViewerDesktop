#include "StartupArgs.h"

#include <QDir>

namespace cv {

StartupArgs StartupArgs::parse(const QStringList &args)
{
    StartupArgs result;
    for (const QString &arg : args) {
        if (!arg.startsWith(QChar('-'))) {
            result.videoPath = arg;
            break;
        }
    }
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
