#include "SrtParser.h"

#include <QRegularExpression>
#include <QStringConverter>
#include <QStringList>

#include <algorithm>

namespace cv::SrtParser {

namespace {

bool isTimingLine(QStringView line)
{
    return line.contains(QLatin1String("-->"));
}

bool isNumber(QStringView line)
{
    const QStringView trimmed = line.trimmed();
    return !trimmed.isEmpty() && std::all_of(trimmed.begin(), trimmed.end(), [](QChar c) { return c.isDigit(); });
}

} // namespace

QString decode(const QByteArray &data)
{
    if (const auto encoding = QStringConverter::encodingForData(data)) {
        QStringDecoder decoder(*encoding); // drops the byte-order mark
        return decoder(data);
    }
    // Stateless, so bytes cut off at the end count as invalid rather than waiting for more
    QStringDecoder utf8(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    const QString text = utf8(data);
    if (!utf8.hasError())
        return text;
    QStringDecoder system(QStringConverter::System);
    return system(data);
}

std::optional<double> parseTimestamp(QStringView text)
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*(?:(\\d{1,3}):)?(\\d{1,2}):(\\d{1,2})(?:[,.](\\d{1,3}))?\\s*$"));
    const QRegularExpressionMatch m = pattern.matchView(text);
    if (!m.hasMatch())
        return std::nullopt;
    const int hours = m.capturedView(1).isEmpty() ? 0 : m.capturedView(1).toInt();
    const int minutes = m.capturedView(2).toInt();
    const int seconds = m.capturedView(3).toInt();
    if (minutes >= 60 || seconds >= 60)
        return std::nullopt;
    // "1,5" is 1.5 s: the fraction's digits are the leading ones of the milliseconds
    const QString fraction = m.captured(4).leftJustified(3, QLatin1Char('0'));
    return hours * 3600.0 + minutes * 60.0 + seconds + fraction.toInt() / 1000.0;
}

QString toStyledText(const QString &text)
{
    static const QRegularExpression assOverride(QStringLiteral("\\{\\\\[^}]*\\}"));
    static const QRegularExpression tag(QStringLiteral("<\\s*(/?)\\s*([a-zA-Z]+)[^>]*>"));

    QString plain = text;
    plain.remove(assOverride);
    plain.replace(QLatin1String("\\N"), QLatin1String("\n"));

    QString out;
    qsizetype last = 0;
    for (const QRegularExpressionMatch &m : tag.globalMatch(plain)) {
        out += plain.mid(last, m.capturedStart() - last).toHtmlEscaped();
        const QString name = m.captured(2).toLower();
        if (name == QLatin1String("i") || name == QLatin1String("b") || name == QLatin1String("u"))
            out += QLatin1Char('<') + m.captured(1) + name + QLatin1Char('>');
        last = m.capturedEnd();
    }
    out += plain.mid(last).toHtmlEscaped();

    QStringList lines = out.split(QLatin1Char('\n'));
    for (QString &line : lines)
        line = line.trimmed();
    lines.removeAll(QString());
    return lines.join(QLatin1String("<br>"));
}

SubtitleTrack parse(const QByteArray &data)
{
    QString text = decode(data);
    text.replace(QLatin1String("\r\n"), QLatin1String("\n")).replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = text.split(QLatin1Char('\n'));

    SubtitleTrack track;
    qsizetype i = 0;
    while (i < lines.size()) {
        const QString &line = lines[i++];
        if (!isTimingLine(line))
            continue; // an index number, a blank line, or junk before the first cue

        // "00:00:01,000 --> 00:00:02,500", possibly followed by position hints
        const qsizetype arrow = line.indexOf(QLatin1String("-->"));
        const auto start = parseTimestamp(QStringView(line).left(arrow));
        const auto end = parseTimestamp(line.mid(arrow + 3).trimmed().section(QLatin1Char(' '), 0, 0));

        // The text runs to a blank line, or to the next cue if the blank line is missing.
        QStringList body;
        while (i < lines.size() && !lines[i].trimmed().isEmpty() && !isTimingLine(lines[i]))
            body << lines[i++];
        if (i < lines.size() && isTimingLine(lines[i]) && !body.isEmpty() && isNumber(body.last()))
            body.removeLast(); // that was the next cue's index

        if (!start || !end)
            continue; // a broken timestamp: skip this cue, keep the rest
        const QString styled = toStyledText(body.join(QLatin1Char('\n')));
        if (styled.isEmpty())
            continue;
        track.cues << SubtitleCue{*start, *end, styled, {}};
    }
    track.normalize();
    return track;
}

} // namespace cv::SrtParser
