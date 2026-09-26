#pragma once

#include "SubtitleTrack.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace cv::SrtParser {

// Parses SubRip (.srt) data. Never throws: a cue with a broken timestamp is skipped, and
// anything unreadable gives an empty track.
SubtitleTrack parse(const QByteArray &data);

// The file's text: by its byte-order mark if it has one, else UTF-8 if it's valid UTF-8, else
// the system codepage (older .srt files are often Windows-1252 or similar).
QString decode(const QByteArray &data);

// A cue's lines as StyledText: <i>, <b> and <u> kept, other tags (e.g. <font>) and ASS-style
// {\...} overrides removed, the rest escaped, line breaks as <br>.
QString toStyledText(const QString &text);

// "00:01:02,345" (also with '.' or ':' before the milliseconds, and without hours) in seconds.
std::optional<double> parseTimestamp(QStringView text);

} // namespace cv::SrtParser
