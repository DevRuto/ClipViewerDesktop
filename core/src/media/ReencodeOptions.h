#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace cv {

// Settings for a Reencode export. Each field is one of a fixed list of choices (the UI offers
// presets, never free-form values); fromJson replaces anything else with the default.
struct ReencodeOptions
{
    // x264 CRF; lower is higher quality.
    int crf = 18;
    // x264 preset. On a 5-minute 720p60 clip, veryfast at CRF 18 matched the quality of fast at
    // CRF 20 in about half the time.
    QString preset = QStringLiteral("veryfast");
    // Limit on the short side in pixels (720 is 1280x720, or 720x1280 upright); never scales up.
    // 0 keeps the source size.
    int maxHeight = 0;
    // Limit in frames per second; a slower source is left alone. 0 keeps the source rate.
    int maxFrameRate = 0;
    // AAC bitrate in kbit/s; 0 leaves the audio out.
    int audioBitrate = 192;

    static const QList<int> Crfs;
    static const QStringList Presets;
    static const QList<int> MaxHeights;
    static const QList<int> MaxFrameRates;
    static const QList<int> AudioBitrates;

    static ReencodeOptions fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

    bool operator==(const ReencodeOptions &) const = default;
};

} // namespace cv
