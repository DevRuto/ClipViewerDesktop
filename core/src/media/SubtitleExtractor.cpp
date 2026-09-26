#include "SubtitleExtractor.h"

#include "subtitles/DvdSubtitle.h"
#include "subtitles/SrtParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace cv {

namespace {

// A subtitle file bigger than this isn't one (a film's .srt is well under 1 MB).
constexpr qint64 MaxSubtitleFileBytes = 32 << 20;
// A picture cue with no stop time and nothing after it
constexpr double FallbackCueSeconds = 5;

const QStringList &subtitleSuffixes()
{
    static const QStringList suffixes{QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("ssa"),
                                      QStringLiteral("vtt")};
    return suffixes;
}

double jsonSeconds(const QJsonObject &object, const QString &name)
{
    bool ok = false;
    const double value = object.value(name).toString().toDouble(&ok);
    return ok && std::isfinite(value) ? value : std::nan("");
}

} // namespace

SubtitleTrack SubtitleExtractor::extract(const QString &videoPath, int index, SubtitleFormat format,
                                         const CancelToken &cancel) const
{
    const QString stream = QStringLiteral("0:s:%1").arg(index);
    switch (format) {
    case SubtitleFormat::Text: {
        const QByteArray srt = runTool(m_paths.ffmpeg,
                                       {QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                                        QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-i"),
                                        videoPath, QStringLiteral("-map"), stream, QStringLiteral("-f"),
                                        QStringLiteral("srt"), QStringLiteral("-")},
                                       cancel);
        return SrtParser::parse(srt);
    }
    case SubtitleFormat::DvdPicture: {
        const QByteArray json = runTool(
            m_paths.ffprobe,
            {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-select_streams"),
             QStringLiteral("s:%1").arg(index), QStringLiteral("-show_entries"),
             QStringLiteral("stream=extradata,width,height:packet=pts_time,duration_time,data"),
             QStringLiteral("-show_data"), QStringLiteral("-of"), QStringLiteral("json"), videoPath},
            cancel);
        cancel.throwIfCancelled();
        return parsePictureTrack(json);
    }
    case SubtitleFormat::Unsupported:
        break;
    }
    throw MediaError(QStringLiteral("This subtitle format can't be shown"));
}

SubtitleTrack SubtitleExtractor::loadFile(const QString &path, const CancelToken &cancel) const
{
    const QFileInfo info(path);
    if (!info.isFile())
        throw MediaError(QStringLiteral("Subtitle file not found: %1").arg(path));
    if (info.size() > MaxSubtitleFileBytes)
        throw MediaError(QStringLiteral("%1 is too large for a subtitle file").arg(info.fileName()));

    if (info.suffix().compare(QLatin1String("srt"), Qt::CaseInsensitive) == 0) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            throw MediaError(QStringLiteral("Can't read %1: %2").arg(info.fileName(), file.errorString()));
        return SrtParser::parse(file.readAll());
    }
    const QByteArray srt = runTool(m_paths.ffmpeg,
                                   {QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                                    QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-i"), path,
                                    QStringLiteral("-f"), QStringLiteral("srt"), QStringLiteral("-")},
                                   cancel);
    return SrtParser::parse(srt);
}

bool SubtitleExtractor::isSubtitleFile(const QString &path)
{
    return subtitleSuffixes().contains(QFileInfo(path).suffix().toLower());
}

QStringList SubtitleExtractor::sidecarFiles(const QString &videoPath)
{
    const QFileInfo video(videoPath);
    const QString base = video.completeBaseName();
    QStringList exact, others;
    const QFileInfoList entries = video.dir().entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : entries) {
        const QString name = entry.fileName();
        if (!isSubtitleFile(name) || !name.startsWith(base + QLatin1Char('.'), Qt::CaseInsensitive))
            continue;
        if (entry.completeBaseName().compare(base, Qt::CaseInsensitive) == 0
            && entry.suffix().compare(QLatin1String("srt"), Qt::CaseInsensitive) == 0)
            exact << entry.absoluteFilePath();
        else
            others << entry.absoluteFilePath();
    }
    return exact + others;
}

SubtitleTrack SubtitleExtractor::parsePictureTrack(const QByteArray &ffprobeJson)
{
    const QJsonObject root = QJsonDocument::fromJson(ffprobeJson).object();
    const QJsonObject stream = root.value(QLatin1String("streams")).toArray().at(0).toObject();
    const DvdSubtitle::Palette palette = DvdSubtitle::parsePalette(
        QString::fromLatin1(DvdSubtitle::parseHexDump(stream.value(QLatin1String("extradata")).toString())));

    SubtitleTrack track;
    track.palette = palette.colors;
    track.canvas = palette.canvas;
    if (track.canvas.isEmpty()) {
        const int width = stream.value(QLatin1String("width")).toInt();
        const int height = stream.value(QLatin1String("height")).toInt();
        track.canvas = width > 0 && height > 0 ? QSize(width, height) : QSize(720, 480);
    }

    struct Packet
    {
        double pts;
        double duration;
        std::optional<SubtitleCue> cue;
    };
    QList<Packet> packets;
    for (const QJsonValue &value : root.value(QLatin1String("packets")).toArray()) {
        const QJsonObject packet = value.toObject();
        const double pts = jsonSeconds(packet, QStringLiteral("pts_time"));
        if (std::isnan(pts))
            continue;
        const QByteArray data = DvdSubtitle::parseHexDump(packet.value(QLatin1String("data")).toString());
        packets << Packet{pts, jsonSeconds(packet, QStringLiteral("duration_time")), DvdSubtitle::readCue(data, pts)};
    }
    std::stable_sort(packets.begin(), packets.end(), [](const Packet &a, const Packet &b) { return a.pts < b.pts; });

    for (qsizetype i = 0; i < packets.size(); ++i) {
        if (!packets[i].cue)
            continue; // an empty packet: it only clears the screen, which the previous cue's end covers
        SubtitleCue cue = *packets[i].cue;
        if (std::isnan(cue.end)) {
            // Until the next packet (usually the empty one that clears it), else the packet's own duration
            const Packet &p = packets[i];
            if (i + 1 < packets.size())
                cue.end = packets[i + 1].pts;
            else if (p.duration > 0)
                cue.end = p.pts + p.duration;
            else
                cue.end = cue.start + FallbackCueSeconds;
        }
        track.cues << cue;
    }
    track.normalize();
    return track;
}

} // namespace cv
