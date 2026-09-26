#include "DvdSubtitle.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace cv::DvdSubtitle {

namespace {

// Pictures larger than this are treated as corrupt (a DVD frame is 720x576).
constexpr int MaxSide = 4096;

quint16 read16(const QByteArray &data, qsizetype pos)
{
    return static_cast<quint16>((static_cast<quint8>(data[pos]) << 8) | static_cast<quint8>(data[pos + 1]));
}

// Reads 4-bit values from a byte range, stopping (returning -1) at its end.
class NibbleReader
{
public:
    NibbleReader(const QByteArray &data, qsizetype start, qsizetype end)
        : m_data(data), m_pos(start * 2), m_end(end * 2) {}

    int next()
    {
        if (m_pos >= m_end)
            return -1;
        const quint8 byte = static_cast<quint8>(m_data[m_pos / 2]);
        return (m_pos++ % 2 == 0) ? byte >> 4 : byte & 0x0f;
    }

    void alignToByte()
    {
        if (m_pos % 2)
            ++m_pos;
    }

private:
    const QByteArray &m_data;
    qsizetype m_pos; // in nibbles
    qsizetype m_end;
};

// Decodes one field (every other row, starting at firstRow) of the run-length-encoded picture
// into `pixels` (a 2-bit colour index per pixel).
void decodeField(const QByteArray &data, qsizetype offset, int width, int height, int firstRow, QList<quint8> &pixels)
{
    if (offset < 4 || offset >= data.size())
        return;
    NibbleReader reader(data, offset, data.size());
    for (int y = firstRow; y < height; y += 2) {
        int x = 0;
        while (x < width) {
            // A code is 1-4 nibbles: the more leading zero bits, the longer the run it can hold.
            int code = reader.next();
            if (code < 0)
                return;
            for (int threshold : {0x4, 0x10, 0x40}) {
                if (code >= threshold)
                    break;
                const int n = reader.next();
                if (n < 0)
                    return;
                code = (code << 4) | n;
            }
            int run = code >> 2;
            if (run == 0)
                run = width - x; // to the end of the line
            run = std::min(run, width - x);
            std::fill_n(pixels.begin() + static_cast<qsizetype>(y) * width + x, run, static_cast<quint8>(code & 3));
            x += run;
        }
        reader.alignToByte();
    }
}

} // namespace

Palette parsePalette(const QString &header)
{
    Palette palette;
    for (const QString &rawLine : header.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;
        const QString key = line.left(colon).trimmed().toLower();
        const QString value = line.mid(colon + 1).trimmed();
        if (key == QLatin1String("size")) {
            const QStringList parts = value.split(QLatin1Char('x'));
            bool okW = false, okH = false;
            const int w = parts.value(0).trimmed().toInt(&okW);
            const int h = parts.value(1).trimmed().toInt(&okH);
            if (parts.size() == 2 && okW && okH && w > 0 && h > 0 && w <= MaxSide && h <= MaxSide)
                palette.canvas = QSize(w, h);
        } else if (key == QLatin1String("palette")) {
            const QStringList colors = value.split(QLatin1Char(','));
            if (colors.size() < 16)
                continue;
            bool allOk = true;
            std::array<QRgb, 16> parsed{};
            for (int i = 0; i < 16; ++i) {
                bool ok = false;
                parsed[i] = 0xff000000u | (colors[i].trimmed().toUInt(&ok, 16) & 0xffffffu);
                allOk = allOk && ok;
            }
            if (allOk) {
                palette.colors = parsed;
                palette.hasColors = true;
            }
        }
    }
    if (!palette.hasColors) {
        // No palette: a grey ramp, so the text still shows (light on dark)
        for (int i = 0; i < 16; ++i)
            palette.colors[i] = qRgb(i * 17, i * 17, i * 17);
    }
    return palette;
}

QByteArray parseHexDump(const QString &dump)
{
    // Each line: 8 hex digits of offset, ": ", up to 8 groups of 4 hex digits, then the ASCII view.
    QByteArray bytes;
    for (const QString &line : dump.split(QLatin1Char('\n'))) {
        const qsizetype colon = line.indexOf(QLatin1String(": "));
        if (colon < 0)
            continue;
        const QStringList groups = line.mid(colon + 2, 39).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString &group : groups) {
            if (group.size() % 2 != 0)
                break;
            const QByteArray chunk = QByteArray::fromHex(group.toLatin1());
            if (chunk.size() * 2 != group.size())
                break; // not hex: the ASCII view of a short line
            bytes += chunk;
        }
    }
    return bytes;
}

namespace {

// What a packet's control sequences say.
struct Control
{
    double startMs = 0;
    double stopMs = std::numeric_limits<double>::quiet_NaN();
    std::array<int, 4> colorIndex{3, 2, 1, 0};
    std::array<int, 4> alpha{0, 15, 15, 15};
    QRect position;
    qsizetype topField = -1;
    qsizetype bottomField = -1;
};

// Nothing if the packet is malformed or never shows its picture.
std::optional<Control> readControl(const QByteArray &packet)
{
    const qsizetype size = packet.size();
    if (size < 4 || read16(packet, 0) == 0)
        return std::nullopt; // too short, or 32-bit offsets (HD DVD), which aren't supported

    Control control;
    bool shown = false;
    int x1 = -1, x2 = -1, y1 = -1, y2 = -1;

    // Control sequences: [delay:16][next:16] commands... 0xff; the last one points at itself.
    qsizetype seq = read16(packet, 2);
    for (int guard = 0; guard < 64 && seq + 4 <= size; ++guard) {
        const double delayMs = read16(packet, seq) * 1024.0 / 90.0;
        const qsizetype next = read16(packet, seq + 2);
        qsizetype pos = seq + 4;
        bool done = false;
        while (!done && pos < size) {
            const quint8 command = static_cast<quint8>(packet[pos++]);
            switch (command) {
            case 0x00: // forced start (menus, forced subtitles)
            case 0x01: // start
                control.startMs = delayMs;
                shown = true;
                break;
            case 0x02: // stop
                control.stopMs = delayMs;
                break;
            case 0x03: // palette entries for the 4 colours
            case 0x04: { // their transparency
                if (pos + 2 > size)
                    return std::nullopt;
                const quint8 a = static_cast<quint8>(packet[pos]), b = static_cast<quint8>(packet[pos + 1]);
                (command == 0x03 ? control.colorIndex : control.alpha) = {b & 0x0f, b >> 4, a & 0x0f, a >> 4};
                pos += 2;
                break;
            }
            case 0x05: { // coordinates, 12 bits each, inclusive
                if (pos + 6 > size)
                    return std::nullopt;
                const auto at = [&](int i) { return static_cast<int>(static_cast<quint8>(packet[pos + i])); };
                x1 = (at(0) << 4) | (at(1) >> 4);
                x2 = ((at(1) & 0x0f) << 8) | at(2);
                y1 = (at(3) << 4) | (at(4) >> 4);
                y2 = ((at(4) & 0x0f) << 8) | at(5);
                pos += 6;
                break;
            }
            case 0x06: // where the two fields' picture data starts
                if (pos + 4 > size)
                    return std::nullopt;
                control.topField = read16(packet, pos);
                control.bottomField = read16(packet, pos + 2);
                pos += 4;
                break;
            case 0x07: { // colour/contrast changes within the picture: skipped
                if (pos + 2 > size)
                    return std::nullopt;
                const qsizetype length = read16(packet, pos);
                if (length < 2)
                    return std::nullopt;
                pos += length;
                break;
            }
            default: // 0xff ends the sequence; anything else is unknown, so stop reading it
                done = true;
                break;
            }
        }
        if (next <= seq)
            break;
        seq = next;
    }

    if (!shown || x1 < 0 || x2 < x1 || y2 < y1 || control.topField < 0 || control.bottomField < 0)
        return std::nullopt;
    if (x2 - x1 + 1 > MaxSide || y2 - y1 + 1 > MaxSide)
        return std::nullopt;
    control.position = QRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    return control;
}

} // namespace

std::optional<SubtitleCue> readCue(const QByteArray &packet, double pts)
{
    const auto control = readControl(packet);
    if (!control || !std::isfinite(pts))
        return std::nullopt;
    SubtitleCue cue;
    cue.start = pts + control->startMs / 1000;
    cue.end = std::isnan(control->stopMs) ? control->stopMs : pts + control->stopMs / 1000;
    cue.picture = packet;
    return cue;
}

std::optional<Picture> render(const QByteArray &packet, const std::array<QRgb, 16> &palette)
{
    const auto control = readControl(packet);
    if (!control)
        return std::nullopt;
    const int width = control->position.width();
    const int height = control->position.height();

    QList<quint8> pixels(static_cast<qsizetype>(width) * height, 0);
    decodeField(packet, control->topField, width, height, 0, pixels);
    decodeField(packet, control->bottomField, width, height, 1, pixels);

    std::array<QRgb, 4> rgba{};
    for (int c = 0; c < 4; ++c) {
        const QRgb color = palette[control->colorIndex[c]];
        rgba[c] = control->alpha[c] == 0 ? 0u : qRgba(qRed(color), qGreen(color), qBlue(color), control->alpha[c] * 17);
    }

    QImage image(width, height, QImage::Format_ARGB32);
    if (image.isNull())
        return std::nullopt;
    bool anyVisible = false;
    for (int y = 0; y < height; ++y) {
        auto *row = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            row[x] = rgba[pixels[static_cast<qsizetype>(y) * width + x]];
            anyVisible = anyVisible || qAlpha(row[x]) != 0;
        }
    }
    if (!anyVisible)
        return std::nullopt;
    return Picture{image, control->position};
}

} // namespace cv::DvdSubtitle
