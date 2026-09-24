#include "FfmpegPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace cv {

namespace {

QString executableName(const char *name)
{
#ifdef Q_OS_WIN
    return QString::fromLatin1(name) + QStringLiteral(".exe");
#else
    return QString::fromLatin1(name);
#endif
}

} // namespace

QString FfmpegPaths::managedDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("ClipViewerDesktop/ffmpeg"));
}

std::optional<FfmpegPaths> FfmpegPaths::locate(const QString &configuredDirectory)
{
    QStringList candidates{configuredDirectory, qEnvironmentVariable(DirectoryEnvVar)};
    if (QCoreApplication::instance()) {
        const QDir appDir(QCoreApplication::applicationDirPath());
        candidates << appDir.filePath(QStringLiteral("ffmpeg")) << appDir.filePath(QStringLiteral("ffmpeg/bin"));
    }
    candidates << managedDirectory();
    candidates << qEnvironmentVariable("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);

    for (const QString &dir : std::as_const(candidates)) {
        if (dir.trimmed().isEmpty())
            continue;
        if (auto paths = inDirectory(dir))
            return paths;
    }
    return std::nullopt;
}

std::optional<FfmpegPaths> FfmpegPaths::inDirectory(const QString &directory)
{
    const QDir dir(directory);
    const QString ffmpeg = dir.filePath(executableName("ffmpeg"));
    const QString ffprobe = dir.filePath(executableName("ffprobe"));
    if (QFileInfo(ffmpeg).isFile() && QFileInfo(ffprobe).isFile())
        return FfmpegPaths{QDir::toNativeSeparators(ffmpeg), QDir::toNativeSeparators(ffprobe)};
    return std::nullopt;
}

} // namespace cv
