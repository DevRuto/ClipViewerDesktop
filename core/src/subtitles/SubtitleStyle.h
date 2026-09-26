#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace cv {

// How subtitles are drawn (the subtitle settings popup). Each field is one of a fixed list of
// choices; fromJson replaces anything else with the default.
struct SubtitleStyle
{
    // Percent of the normal size (text is about a twentieth of the picture's height). Also scales
    // DVD picture subtitles.
    int size = 100;
    // Text on a dark "box", or with an "outline" and no box. DVD pictures bring their own look.
    QString background = QStringLiteral("box");
    // Distance of the text's bottom from the picture's bottom, in percent of its height. DVD
    // pictures move up by the difference from the default.
    int position = 5;

    static const QList<int> Sizes;
    static const QStringList Backgrounds;
    static const QList<int> Positions;

    static SubtitleStyle fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

    bool operator==(const SubtitleStyle &) const = default;
};

} // namespace cv
