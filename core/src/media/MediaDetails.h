#pragma once

#include "MediaInfo.h"

#include <QList>
#include <QString>

namespace cv {

// The media info panel's text: a row per fact about the file, then one per stream.
namespace MediaDetails {

struct Row
{
    QString label; // "Container", "Video", "Audio 2", …
    QString value; // "h264 (High) · 1920×1080 · 59.94 fps · …"

    bool operator==(const Row &) const = default;
};

QList<Row> describe(const MediaInfo &info);

// "12.3 MB"
QString formatBytes(double bytes);
// "192 kbit/s", "8.2 Mbit/s"
QString formatBitRate(qint64 bitsPerSecond);

} // namespace MediaDetails

} // namespace cv
