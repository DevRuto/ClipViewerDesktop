#include "MediaDetails.h"

#include "TimeFormat.h"

#include <QHash>
#include <QStringList>

#include <cmath>

namespace cv::MediaDetails {

namespace {

QString codecText(const StreamInfo &stream)
{
    QString codec = stream.codec.isEmpty() ? QStringLiteral("unknown codec") : stream.codec;
    if (!stream.profile.isEmpty())
        codec += QStringLiteral(" (%1)").arg(stream.profile);
    return codec;
}

QString streamText(const StreamInfo &stream)
{
    QStringList parts{codecText(stream)};
    if (stream.type == QLatin1String("video")) {
        if (stream.width > 0 && stream.height > 0)
            parts << QStringLiteral("%1×%2").arg(stream.width).arg(stream.height);
        if (stream.frameRate > 0 && !stream.coverArt)
            parts << QStringLiteral("%1 fps").arg(QString::number(stream.frameRate, 'g', 4));
        if (!stream.pixelFormat.isEmpty())
            parts << stream.pixelFormat;
    } else if (stream.type == QLatin1String("audio")) {
        if (stream.sampleRate > 0)
            parts << QStringLiteral("%1 kHz").arg(QString::number(stream.sampleRate / 1000.0, 'g', 4));
        if (!stream.channelLayout.isEmpty())
            parts << stream.channelLayout;
        else if (stream.channels > 0)
            parts << QStringLiteral("%1 ch").arg(stream.channels);
    }
    if (stream.bitRate > 0)
        parts << formatBitRate(stream.bitRate);
    if (!stream.language.isEmpty())
        parts << stream.language;
    if (!stream.title.isEmpty())
        parts << QStringLiteral("“%1”").arg(stream.title);
    return parts.join(QStringLiteral(" · "));
}

QString streamLabel(const StreamInfo &stream)
{
    if (stream.coverArt)
        return QStringLiteral("Cover art");
    if (stream.type == QLatin1String("video"))
        return QStringLiteral("Video");
    if (stream.type == QLatin1String("audio"))
        return QStringLiteral("Audio");
    if (stream.type == QLatin1String("subtitle"))
        return QStringLiteral("Subtitles");
    if (stream.type == QLatin1String("attachment"))
        return QStringLiteral("Attachment");
    return QStringLiteral("Data");
}

} // namespace

QList<Row> describe(const MediaInfo &info)
{
    QList<Row> rows;
    rows << Row{QStringLiteral("Container"), info.containerName.isEmpty() ? info.containerFormat : info.containerName};
    rows << Row{QStringLiteral("Duration"), TimeFormat::format(info.duration)};
    rows << Row{QStringLiteral("Size"), formatBytes(static_cast<double>(info.sizeBytes))};
    if (info.bitRate > 0)
        rows << Row{QStringLiteral("Bit rate"), formatBitRate(info.bitRate)};
    if (info.rotation != 0)
        rows << Row{QStringLiteral("Rotation"), QStringLiteral("%1°").arg(info.rotation)};

    // Numbered only when there's more than one of a kind: "Audio 1", "Audio 2"
    QHash<QString, int> total, seen;
    for (const StreamInfo &stream : info.streams)
        ++total[streamLabel(stream)];
    for (const StreamInfo &stream : info.streams) {
        const QString label = streamLabel(stream);
        const int n = ++seen[label];
        rows << Row{total[label] > 1 ? QStringLiteral("%1 %2").arg(label).arg(n) : label, streamText(stream)};
    }
    return rows;
}

QString formatBytes(double bytes)
{
    if (bytes >= 1 << 30)
        return QStringLiteral("%1 GB").arg(bytes / (1 << 30), 0, 'f', 2);
    if (bytes >= 1 << 20)
        return QStringLiteral("%1 MB").arg(bytes / (1 << 20), 0, 'f', 1);
    if (bytes >= 1 << 10)
        return QStringLiteral("%1 KB").arg(std::round(bytes / (1 << 10)));
    return QStringLiteral("%1 B").arg(bytes);
}

QString formatBitRate(qint64 bitsPerSecond)
{
    if (bitsPerSecond >= 1000000)
        return QStringLiteral("%1 Mbit/s").arg(bitsPerSecond / 1e6, 0, 'f', 1);
    return QStringLiteral("%1 kbit/s").arg(std::llround(bitsPerSecond / 1e3));
}

} // namespace cv::MediaDetails
