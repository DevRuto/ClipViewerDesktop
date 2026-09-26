#include "MediaInfo.h"

#include <QStringList>

namespace cv {

SubtitleFormat subtitleFormat(const QString &codec)
{
    if (codec == QLatin1String("dvd_subtitle"))
        return SubtitleFormat::DvdPicture;
    // A list of the text ones rather than the picture ones, so a codec we don't know stays off.
    static const QStringList text{
        QStringLiteral("subrip"),     QStringLiteral("srt"),      QStringLiteral("ass"),
        QStringLiteral("ssa"),        QStringLiteral("mov_text"), QStringLiteral("webvtt"),
        QStringLiteral("text"),       QStringLiteral("microdvd"), QStringLiteral("subviewer"),
        QStringLiteral("subviewer1"), QStringLiteral("sami"),     QStringLiteral("realtext"),
        QStringLiteral("mpl2"),       QStringLiteral("pjs"),      QStringLiteral("vplayer"),
        QStringLiteral("stl"),        QStringLiteral("jacosub"),  QStringLiteral("ttml")};
    return text.contains(codec) ? SubtitleFormat::Text : SubtitleFormat::Unsupported;
}

QList<StreamInfo> MediaInfo::subtitleStreams() const
{
    QList<StreamInfo> subtitles;
    for (const StreamInfo &stream : streams) {
        if (stream.type == QLatin1String("subtitle"))
            subtitles << stream;
    }
    return subtitles;
}

} // namespace cv
