#include "MediaProbe.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

#include <cmath>

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
    QList<StreamInfo> streams;
    for (const QJsonValue &value : root.value(QLatin1String("streams")).toArray()) {
        const QJsonObject stream = value.toObject();
        streams << parseStream(stream);
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

    info.bitRate = field(format, QStringLiteral("bit_rate")).toLongLong();
    info.containerFormat = field(format, QStringLiteral("format_name"));
    info.containerName = field(format, QStringLiteral("format_long_name"));
    info.streams = streams;
    info.videoCodec = field(video, QStringLiteral("codec_name"));
    info.width = video.value(QLatin1String("width")).toInt();
    info.height = video.value(QLatin1String("height")).toInt();
    info.frameRate = parseRate(field(video, QStringLiteral("avg_frame_rate")))
                         .value_or(parseRate(field(video, QStringLiteral("r_frame_rate"))).value_or(0));
    // ffprobe writes the pixel aspect as "4:3", and "0:1" when it's unknown
    info.sampleAspectRatio =
        parseRate(field(video, QStringLiteral("sample_aspect_ratio")).replace(QLatin1Char(':'), QLatin1Char('/')))
            .value_or(1);
    if (!std::isfinite(info.sampleAspectRatio) || info.sampleAspectRatio <= 0)
        info.sampleAspectRatio = 1;
    info.rotation = parseRotation(video);
    if (haveAudio) {
        info.audioCodec = field(audio, QStringLiteral("codec_name"));
        if (info.audioCodec.isEmpty())
            info.audioCodec = QStringLiteral("unknown");
    }
    return info;
}

StreamInfo MediaProbe::parseStream(const QJsonObject &stream)
{
    const QJsonObject tags = stream.value(QLatin1String("tags")).toObject();
    StreamInfo info;
    info.type = field(stream, QStringLiteral("codec_type"));
    info.codec = field(stream, QStringLiteral("codec_name"));
    info.profile = field(stream, QStringLiteral("profile"));
    info.language = field(tags, QStringLiteral("language"));
    if (info.language == QLatin1String("und"))
        info.language.clear();
    info.title = field(tags, QStringLiteral("title")).trimmed();
    // Matroska keeps the bitrate in a "BPS" tag instead
    info.bitRate = field(stream, QStringLiteral("bit_rate")).toLongLong();
    if (info.bitRate <= 0)
        info.bitRate = std::max(0LL, field(tags, QStringLiteral("BPS")).toLongLong());
    info.coverArt =
        stream.value(QLatin1String("disposition")).toObject().value(QLatin1String("attached_pic")).toInt() == 1;
    info.width = stream.value(QLatin1String("width")).toInt();
    info.height = stream.value(QLatin1String("height")).toInt();
    info.frameRate = parseRate(field(stream, QStringLiteral("avg_frame_rate"))).value_or(0);
    info.pixelFormat = field(stream, QStringLiteral("pix_fmt"));
    info.sampleRate = field(stream, QStringLiteral("sample_rate")).toInt();
    info.channels = stream.value(QLatin1String("channels")).toInt();
    info.channelLayout = field(stream, QStringLiteral("channel_layout"));
    return info;
}

int MediaProbe::parseRotation(const QJsonObject &stream)
{
    double degrees = 0;
    bool found = false;
    for (const QJsonValue &value : stream.value(QLatin1String("side_data_list")).toArray()) {
        const QJsonObject data = value.toObject();
        if (data.value(QLatin1String("side_data_type")).toString() == QLatin1String("Display Matrix")) {
            // Counter-clockwise, so a phone video held upright usually says -90
            degrees = -data.value(QLatin1String("rotation")).toDouble();
            found = true;
            break;
        }
    }
    if (!found)
        degrees = field(stream.value(QLatin1String("tags")).toObject(), QStringLiteral("rotate")).toDouble();
    if (!std::isfinite(degrees))
        return 0;
    // To the nearest quarter turn, in [0, 360)
    const int quarters = static_cast<int>(std::lround(degrees / 90)) % 4;
    return (quarters + 4) % 4 * 90;
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
