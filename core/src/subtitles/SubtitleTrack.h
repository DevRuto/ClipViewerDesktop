#pragma once

#include <QByteArray>
#include <QList>
#include <QRgb>
#include <QSize>
#include <QString>

#include <array>

namespace cv {

// One subtitle: text (from .srt and other text formats) or a picture (DVD subtitles).
struct SubtitleCue
{
    double start = 0; // seconds
    double end = 0;
    // Text as the small rich-text subset QML's StyledText shows: <i>, <b>, <u> and <br>, with
    // everything else escaped. Empty for a picture.
    QString text;
    // A picture cue's encoded packet, decoded only when it's shown (see DvdSubtitle::render):
    // a film's worth of decoded pictures would take hundreds of MB.
    QByteArray picture;

    bool isPicture() const { return !picture.isEmpty(); }
};

struct SubtitleTrack
{
    QList<SubtitleCue> cues; // sorted by start
    // Picture tracks: the frame the pictures are placed on (e.g. 720x480 for a DVD) and the
    // track's 16 colours.
    QSize canvas;
    std::array<QRgb, 16> palette{};

    bool isEmpty() const { return cues.isEmpty(); }

    // The cue showing at `seconds`, or -1. When cues overlap, the one that started last.
    int cueAt(double seconds) const;

    // Sorts the cues by start and drops those that end before they start.
    void normalize();
};

} // namespace cv
