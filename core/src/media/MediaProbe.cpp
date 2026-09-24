#include "MediaProbe.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

namespace cv {

MediaInfo MediaProbe::probe(const QString &path, const CancelToken &cancel) const
{
    if (!QFileInfo(path).isFile())
        throw MediaError(QStringLiteral("Video file not found: %1").arg(path));

    const QByteArray json = runTool(
        m_paths.ffprobe,
        {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-print_format"), QStringLiteral("json"),
         QStringLiteral("-show_format"), QStringLiteral("-show_streams"), path},
        cancel);
    return parse(path, json);
}

MediaInfo MediaProbe::parse(const QString &path, const QByteArray &ffprobeJson)
{
    const QJsonObject root = QJsonDocument::fromJson(ffprobeJson).object();

    QJsonObject video, audio;
    bool haveVideo = false, haveAudio = false;
    for (const QJsonValue &value : root.value(QLatin1String("streams")).toArray()) {
        const QJsonObject stream = value.toObject();
        const QString type = stream.value(QLatin1String("codec_type")).toString();
        // Cover art in mp3/mp4 shows up as a single-frame video stream; skip it.
        const bool isCoverArt =
            stream.value(QLatin1String("disposition")).toObject().value(QLatin1String("attached_pic")).toInt() == 1;
        if (type == QLatin1String("video") && !haveVideo && !isCoverArt) {
            video = stream;
            haveVideo = true;
        } else if (type == QLatin1String("audio") && !haveAudio) {
            audio = stream;
            haveAudio = true;
        }
    }
    if (!haveVideo)
        throw MediaError(QStringLiteral("File has no video stream"));

    const QJsonObject format = root.value(QLatin1String("format")).toObject();
    MediaInfo info;
    info.path = path;

    bool ok = false;
    info.duration = field(format, QStringLiteral("duration")).toDouble(&ok);
    if (!ok)
        info.duration = field(video, QStringLiteral("duration")).toDouble();
    info.sizeBytes = field(format, QStringLiteral("size")).toLongLong(&ok);
    if (!ok)
        info.sizeBytes = QFileInfo(path).size();

    info.containerFormat = field(format, QStringLiteral("format_name"));
    info.videoCodec = field(video, QStringLiteral("codec_name"));
    info.width = video.value(QLatin1String("width")).toInt();
    info.height = video.value(QLatin1String("height")).toInt();
    info.frameRate = parseRate(field(video, QStringLiteral("avg_frame_rate")))
                         .value_or(parseRate(field(video, QStringLiteral("r_frame_rate"))).value_or(0));
    if (haveAudio) {
        info.audioCodec = field(audio, QStringLiteral("codec_name"));
        if (info.audioCodec.isEmpty())
            info.audioCodec = QStringLiteral("unknown");
    }
    return info;
}

std::optional<double> MediaProbe::parseRate(const QString &value)
{
    const QStringList parts = value.split(QChar('/'));
    if (parts.size() != 2)
        return std::nullopt;
    bool okNum = false, okDen = false;
    const double num = parts[0].toDouble(&okNum);
    const double den = parts[1].toDouble(&okDen);
    if (!okNum || !okDen || num == 0 || den == 0)
        return std::nullopt;
    return num / den;
}

QString MediaProbe::field(const QJsonObject &object, const QString &name)
{
    const QJsonValue value = object.value(name);
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toDouble(), 'g', 17);
    return {};
}

} // namespace cv
