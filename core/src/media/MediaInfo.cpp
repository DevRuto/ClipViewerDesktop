#include "MediaInfo.h"

#include <QStringList>

#include <algorithm>

namespace cv {

bool MediaInfo::canShowSubtitleTrack(int index, int playerTrackCount) const
{
    QList<const StreamInfo *> subtitles;
    for (const StreamInfo &stream : streams) {
        if (stream.type == QLatin1String("subtitle"))
            subtitles << &stream;
    }
    if (index < 0 || index >= playerTrackCount)
        return false;
    if (subtitles.size() != playerTrackCount)
        return std::all_of(subtitles.cbegin(), subtitles.cend(),
                           [](const StreamInfo *s) { return isTextSubtitleCodec(s->codec); });
    return isTextSubtitleCodec(subtitles[index]->codec);
}

bool MediaInfo::isTextSubtitleCodec(const QString &codec)
{
    // A list of the text ones rather than the bitmap ones, so a codec we don't know stays off.
    static const QStringList text{
        QStringLiteral("subrip"),   QStringLiteral("srt"),      QStringLiteral("ass"),
        QStringLiteral("ssa"),      QStringLiteral("mov_text"), QStringLiteral("webvtt"),
        QStringLiteral("text"),     QStringLiteral("microdvd"), QStringLiteral("subviewer"),
        QStringLiteral("subviewer1"), QStringLiteral("sami"),   QStringLiteral("realtext"),
        QStringLiteral("mpl2"),     QStringLiteral("pjs"),      QStringLiteral("vplayer"),
        QStringLiteral("stl"),      QStringLiteral("jacosub"),  QStringLiteral("ttml")};
    return text.contains(codec);
}

} // namespace cv
