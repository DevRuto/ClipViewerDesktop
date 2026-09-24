#pragma once

#include "FfmpegPaths.h"
#include "MediaInfo.h"
#include "Process.h"

#include <QByteArray>
#include <QJsonObject>

#include <optional>

namespace cv {

// Reads stream and format information from a video file using ffprobe.
class MediaProbe
{
public:
    explicit MediaProbe(FfmpegPaths paths) : m_paths(std::move(paths)) {}

    // Blocks; call from a worker thread. Throws FfmpegError, MediaError or OperationCancelled.
    MediaInfo probe(const QString &path, const CancelToken &cancel = {}) const;

    // Builds a MediaInfo from ffprobe's -show_format -show_streams JSON.
    static MediaInfo parse(const QString &path, const QByteArray &ffprobeJson);

    // Parses ffprobe rationals like "30000/1001"; "0/0" means unknown.
    static std::optional<double> parseRate(const QString &value);

    // A field as text, whether ffprobe wrote it as a string or a number.
    static QString field(const QJsonObject &object, const QString &name);

private:
    FfmpegPaths m_paths;
};

} // namespace cv
