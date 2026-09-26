#pragma once

#include <QList>
#include <QString>

namespace cv {

// How the app can show a subtitle stream: text (converted to SRT by ffmpeg) or DVD pictures
// (decoded by DvdSubtitle). Blu-ray PGS and anything unknown can't be shown yet.
enum class SubtitleFormat { Text, DvdPicture, Unsupported };

SubtitleFormat subtitleFormat(const QString &codec);

// One stream of a file as ffprobe reports it. Fields that don't apply to its type stay empty/0.
struct StreamInfo
{
    QString type;     // ffprobe's codec_type: "video", "audio", "subtitle", "data", "attachment"
    QString codec;    // codec_name
    QString profile;  // e.g. "High", "LC"
    QString language; // ISO 639-2 tag, e.g. "eng"; empty when missing or "und"
    QString title;
    qint64 bitRate = 0; // bits per second; 0 when unknown
    bool coverArt = false; // a still picture attached to the file, not a video track

    // Video
    int width = 0;
    int height = 0;
    double frameRate = 0;
    QString pixelFormat;

    // Audio
    int sampleRate = 0;
    int channels = 0;
    QString channelLayout; // e.g. "stereo", "5.1"
};

// What ffprobe reports about a local video file.
struct MediaInfo
{
    QString path;
    double duration = 0; // seconds
    qint64 sizeBytes = 0;
    qint64 bitRate = 0; // overall, bits per second; 0 when unknown
    QString containerFormat;
    QString containerName; // format_long_name, e.g. "QuickTime / MOV"
    QString videoCodec;
    int width = 0;
    int height = 0;
    double frameRate = 0; // 0 when unknown
    double sampleAspectRatio = 1; // pixel width / height
    int rotation = 0;     // clockwise degrees the picture is turned for display: 0, 90, 180 or 270
    QString audioCodec;   // empty when the file has no audio
    QList<StreamInfo> streams; // every stream, in file order

    bool hasAudio() const { return !audioCodec.isEmpty(); }

    // Length of one frame in seconds; falls back to 30 fps when the rate is unknown.
    double frameDuration() const { return 1.0 / (frameRate > 0 ? frameRate : 30.0); }

    // Width / height of the picture as shown: with the pixel aspect and rotation applied.
    double displayAspect() const
    {
        if (width <= 0 || height <= 0)
            return 16.0 / 9;
        const double aspect = width * sampleAspectRatio / height;
        return rotation % 180 == 0 ? aspect : 1 / aspect;
    }

    // The subtitle streams, in file order (ffmpeg's 0:s:N numbering).
    QList<StreamInfo> subtitleStreams() const;
};

} // namespace cv
