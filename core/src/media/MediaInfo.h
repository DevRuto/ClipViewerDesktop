#pragma once

#include <QString>

namespace cv {

// What ffprobe reports about a local video file.
struct MediaInfo
{
    QString path;
    double duration = 0; // seconds
    qint64 sizeBytes = 0;
    QString containerFormat;
    QString videoCodec;
    int width = 0;
    int height = 0;
    double frameRate = 0; // 0 when unknown
    QString audioCodec;   // empty when the file has no audio

    bool hasAudio() const { return !audioCodec.isEmpty(); }

    // Length of one frame in seconds; falls back to 30 fps when the rate is unknown.
    double frameDuration() const { return 1.0 / (frameRate > 0 ? frameRate : 30.0); }
};

} // namespace cv
