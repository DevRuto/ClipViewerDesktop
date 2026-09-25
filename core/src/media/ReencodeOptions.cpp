#include "ReencodeOptions.h"

namespace cv {

namespace {

const QString Crf = QStringLiteral("crf");
const QString Preset = QStringLiteral("preset");
const QString MaxHeight = QStringLiteral("maxHeight");
const QString MaxFrameRate = QStringLiteral("maxFrameRate");
const QString AudioBitrate = QStringLiteral("audioBitrate");

int pick(const QJsonValue &value, const QList<int> &choices, int fallback)
{
    // toInt gives the fallback for non-numbers; a fractional number isn't a choice.
    const int number = value.toInt(fallback);
    return value.isDouble() && value.toDouble() == number && choices.contains(number) ? number : fallback;
}

} // namespace

const QList<int> ReencodeOptions::Crfs{16, 18, 23, 28};
const QStringList ReencodeOptions::Presets{QStringLiteral("veryfast"), QStringLiteral("medium"),
                                           QStringLiteral("slow")};
const QList<int> ReencodeOptions::MaxHeights{0, 1080, 720, 480};
const QList<int> ReencodeOptions::MaxFrameRates{0, 60, 30};
const QList<int> ReencodeOptions::AudioBitrates{320, 192, 128, 0};

ReencodeOptions ReencodeOptions::fromJson(const QJsonObject &json)
{
    ReencodeOptions options;
    options.crf = pick(json.value(Crf), Crfs, options.crf);
    const QString preset = json.value(Preset).toString();
    if (Presets.contains(preset))
        options.preset = preset;
    options.maxHeight = pick(json.value(MaxHeight), MaxHeights, options.maxHeight);
    options.maxFrameRate = pick(json.value(MaxFrameRate), MaxFrameRates, options.maxFrameRate);
    options.audioBitrate = pick(json.value(AudioBitrate), AudioBitrates, options.audioBitrate);
    return options;
}

QJsonObject ReencodeOptions::toJson() const
{
    return {{Crf, crf},
            {Preset, preset},
            {MaxHeight, maxHeight},
            {MaxFrameRate, maxFrameRate},
            {AudioBitrate, audioBitrate}};
}

} // namespace cv
