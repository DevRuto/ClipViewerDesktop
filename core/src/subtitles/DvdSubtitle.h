#pragma once

#include "SubtitleTrack.h"

#include <QByteArray>
#include <QImage>
#include <QRect>
#include <QRgb>
#include <QSize>
#include <QString>

#include <array>
#include <optional>

// DVD subtitles (ffmpeg's dvd_subtitle): each packet is a "subpicture unit", a small 4-colour
// run-length-encoded picture with control commands saying when to show it, where, and which of
// the track's 16 palette colours (and transparencies) its 4 colours use. Nothing here reads
// outside the packet, however it's damaged.
namespace cv::DvdSubtitle {

struct Palette
{
    std::array<QRgb, 16> colors{};
    QSize canvas; // "size:" from the track header; empty when it isn't given
    bool hasColors = false;
};

struct Picture
{
    QImage image;   // ARGB32
    QRect position; // on the track's canvas
};

// The track header ffmpeg keeps as extradata, in the VobSub .idx style:
// "size: 720x480\npalette: 000000, ffffff, …". Without a palette, a grey ramp.
Palette parsePalette(const QString &header);

// The bytes of an ffprobe -show_data hex dump ("00000000: 0898 0879 ...  ascii").
QByteArray parseHexDump(const QString &dump);

// The cue for a packet shown at `pts` seconds, with the packet kept for render(). Its end is the
// stop command's time, or NaN when the packet has none (the caller then ends it at the next
// packet). Nothing for a packet that shows no picture or is malformed.
std::optional<SubtitleCue> readCue(const QByteArray &packet, double pts);

// Decodes a packet's picture. Nothing if it's malformed or entirely transparent.
std::optional<Picture> render(const QByteArray &packet, const std::array<QRgb, 16> &palette);

} // namespace cv::DvdSubtitle
