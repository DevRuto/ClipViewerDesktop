#include "TimeFormat.h"

#include <QRegularExpression>
#include <QStringList>

#include <cmath>

namespace cv::TimeFormat {

namespace {

QString hms(qint64 totalSeconds, bool forceHours)
{
    const qint64 h = totalSeconds / 3600;
    const qint64 m = totalSeconds / 60 % 60;
    const qint64 s = totalSeconds % 60;
    if (h > 0 || forceHours)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QStringLiteral("%1:%2").arg(totalSeconds / 60).arg(s, 2, 10, QChar('0'));
}

} // namespace

QString formatShort(double seconds)
{
    if (!(seconds > 0))
        seconds = 0;
    return hms(static_cast<qint64>(std::floor(seconds)), false);
}

QString format(double seconds)
{
    if (!(seconds > 0))
        seconds = 0;
    const auto ms = static_cast<qint64>(std::llround(seconds * 1000));
    return QStringLiteral("%1.%2").arg(hms(ms / 1000, false)).arg(ms % 1000, 3, 10, QChar('0'));
}

std::optional<double> parse(const QString &text)
{
    static const QRegularExpression wholeField(QStringLiteral("^\\d+$"));
    static const QRegularExpression secondsField(QStringLiteral("^(\\d+\\.?\\d*|\\.\\d+)$"));

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return std::nullopt;
    const QStringList parts = trimmed.split(QChar(':'));
    if (parts.size() > 3)
        return std::nullopt;

    double total = 0;
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const bool isSeconds = i == parts.size() - 1;
        if (!(isSeconds ? secondsField : wholeField).match(parts[i]).hasMatch())
            return std::nullopt;
        const double value = parts[i].toDouble();
        // "1:75" is a typo, not 2:15.
        if (i > 0 && value >= 60)
            return std::nullopt;
        total = total * 60 + value;
    }
    return total;
}

} // namespace cv::TimeFormat
