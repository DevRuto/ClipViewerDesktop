#pragma once

#include <QString>

#include <optional>

// Formats and parses the timestamps shown in the player and the trim editor. Times are
// seconds as doubles throughout the app.
namespace cv::TimeFormat {

// Whole seconds, like a video player: "m:ss", or "h:mm:ss" at an hour or longer.
QString formatShort(double seconds);

// "m:ss.fff", or "h:mm:ss.fff" at an hour or longer. Rounds to the millisecond, so 1.4999999 s
// (e.g. 45 frames at 30 fps) shows as 1.500.
QString format(double seconds);

// Accepts plain seconds ("75.5"), "m:ss(.fff)" or "h:mm:ss(.fff)". Returns nothing if the text
// isn't a non-negative time; minutes and seconds after the first field must be below 60.
std::optional<double> parse(const QString &text);

} // namespace cv::TimeFormat
