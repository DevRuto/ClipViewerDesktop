#include "SubtitleStyle.h"

namespace cv {

namespace {

const QString Size = QStringLiteral("size");
const QString Background = QStringLiteral("background");
const QString Position = QStringLiteral("position");

int pick(const QJsonValue &value, const QList<int> &choices, int fallback)
{
    // toInt gives the fallback for non-numbers; a fractional number isn't a choice.
    const int number = value.toInt(fallback);
    return value.isDouble() && value.toDouble() == number && choices.contains(number) ? number : fallback;
}

} // namespace

const QList<int> SubtitleStyle::Sizes{75, 100, 130, 160};
const QStringList SubtitleStyle::Backgrounds{QStringLiteral("box"), QStringLiteral("outline")};
const QList<int> SubtitleStyle::Positions{5, 12, 20};

SubtitleStyle SubtitleStyle::fromJson(const QJsonObject &json)
{
    SubtitleStyle style;
    style.size = pick(json.value(Size), Sizes, style.size);
    const QString background = json.value(Background).toString();
    if (Backgrounds.contains(background))
        style.background = background;
    style.position = pick(json.value(Position), Positions, style.position);
    return style;
}

QJsonObject SubtitleStyle::toJson() const
{
    return {{Size, size}, {Background, background}, {Position, position}};
}

} // namespace cv
