// Subtitles for the overlay: the .srt parser, cue lookup, the DVD subtitle decoder and the
// extractor's parsing. No ffmpeg needed; tst_ffmpegintegration extracts real tracks.
#include "media/MediaInfo.h"
#include "media/SubtitleExtractor.h"
#include "subtitles/DvdSubtitle.h"
#include "subtitles/SrtParser.h"
#include "subtitles/SubtitleStyle.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStringConverter>
#include <QTemporaryDir>
#include <QTest>

using namespace cv;

namespace {

// A DVD subpicture unit: a 4x3 picture at (10, 20), shown from its packet's time for 2.048 s
// (a stop delay of 180 x 1024/90000 s). Rows 0 and 2 are the top field, row 1 the bottom one:
//   row 0: colour 1, 1, 2, 2   (codes 0x9, 0xA: run 2)
//   row 1: colour 3 x 4        (code 0x13: run 4)
//   row 2: colour 1 to the end (code 0x0001: run 0 = rest of the line)
// Colour c uses palette entry c; colour 0 is transparent, colour 3 half transparent.
QByteArray samplePacket(bool withStop = true)
{
    QByteArray p = QByteArray::fromHex("0000" "0008" // size (patched below), control sequence offset
                                       "9a0001"      // top field (offset 4)
                                       "13");        // bottom field (offset 7)
    const QByteArray next = withStop ? QByteArray::fromHex("0020") : QByteArray::fromHex("0008");
    p += QByteArray::fromHex("0000") + next // first sequence at 8: delay 0
         + QByteArray::fromHex("033210"     // colours 3,2,1,0 -> palette 3,2,1,0
                               "048ff0"     // alpha: 8, 15, 15, 0
                               "0500a00d014016" // x 10-13, y 20-22
                               "0600040007" // fields at 4 and 7
                               "01ff");     // start
    if (withStop)
        p += QByteArray::fromHex("00b4002002ff"); // second sequence at 32: stop after 180 ticks
    p[0] = static_cast<char>(p.size() >> 8);
    p[1] = static_cast<char>(p.size() & 0xff);
    return p;
}

std::array<QRgb, 16> samplePalette()
{
    std::array<QRgb, 16> palette{};
    for (int i = 0; i < 16; ++i)
        palette[i] = qRgb(i * 10, 100, 200);
    return palette;
}

// The way ffprobe -show_data prints bytes
QString hexDump(const QByteArray &data)
{
    QString out = QStringLiteral("\n");
    for (qsizetype line = 0; line < data.size(); line += 16) {
        const QByteArray chunk = data.mid(line, 16);
        QString hex;
        for (qsizetype i = 0; i < chunk.size(); i += 2)
            hex += QString::fromLatin1(chunk.mid(i, 2).toHex()) + QLatin1Char(' ');
        out += QStringLiteral("%1: %2 %3\n").arg(line, 8, 16, QLatin1Char('0')).arg(hex.leftJustified(40), QString(16, '.'));
    }
    return out;
}

} // namespace

class SubtitlesTest : public QObject
{
    Q_OBJECT

private slots:
    // ---- SRT ----

    void srt_parsesCues()
    {
        const SubtitleTrack track = SrtParser::parse("1\r\n00:00:01,000 --> 00:00:02,500\r\nHello\r\nworld\r\n\r\n"
                                                     "2\r\n00:01:00,250 --> 00:01:02,000 X1:10 X2:20\r\nBye\r\n");
        QCOMPARE(track.cues.size(), 2);
        QCOMPARE(track.cues[0].start, 1.0);
        QCOMPARE(track.cues[0].end, 2.5);
        QCOMPARE(track.cues[0].text, "Hello<br>world");
        QCOMPARE(track.cues[1].start, 60.25);
        QCOMPARE(track.cues[1].text, "Bye");
    }

    void srt_brokenTimestamp_skipsOnlyThatCue()
    {
        const SubtitleTrack track = SrtParser::parse("1\n00:00:01,000 --> 00:00:02,000\nA\n\n"
                                                     "2\n00:00:0x,000 --> nonsense\nB\n\n"
                                                     "3\n00:00:05,000 --> 00:00:06,000\nC\n");
        QCOMPARE(track.cues.size(), 2);
        QCOMPARE(track.cues[0].text, "A");
        QCOMPARE(track.cues[1].text, "C");
    }

    void srt_missingBlankLine_stillSplitsCues()
    {
        const SubtitleTrack track = SrtParser::parse("1\n00:00:01,000 --> 00:00:02,000\nA\n"
                                                     "2\n00:00:03,000 --> 00:00:04,000\nB\n");
        QCOMPARE(track.cues.size(), 2);
        QCOMPARE(track.cues[0].text, "A");
        QCOMPARE(track.cues[1].text, "B");
    }

    void srt_emptyOrGarbage_givesNoCues()
    {
        QVERIFY(SrtParser::parse({}).isEmpty());
        QVERIFY(SrtParser::parse("\n\n\n").isEmpty());
        QVERIFY(SrtParser::parse(QByteArray("\x00\xff\x13 binary --> junk", 22)).isEmpty());
        // A cue that ends before it starts, and one with no text
        QVERIFY(SrtParser::parse("1\n00:00:05,000 --> 00:00:01,000\nBackwards\n\n2\n00:00:01,000 --> 00:00:02,000\n\n")
                    .isEmpty());
    }

    void srt_markup_keepsOnlySimpleStyles()
    {
        QCOMPARE(SrtParser::toStyledText("<font color=\"red\">Hi</font> <I>there</I> & <3"),
                 "Hi <i>there</i> &amp; &lt;3");
        QCOMPARE(SrtParser::toStyledText("{\\an8}{\\i1}Top{\\i0}\\Nline <b >bold</ b>"), "Top<br>line <b>bold</b>");
        QCOMPARE(SrtParser::toStyledText("<script>alert(1)</script>"), "alert(1)");
    }

    void srt_encodings()
    {
        // UTF-8 with a byte-order mark
        QCOMPARE(SrtParser::decode("\xef\xbb\xbf" "Caf\xc3\xa9"), QStringLiteral("Café"));
        // UTF-16 LE with a byte-order mark
        QStringEncoder utf16(QStringConverter::Utf16LE, QStringEncoder::Flag::WriteBom);
        const QByteArray wide = utf16(QStringLiteral("00:00:01,000 --> 00:00:02,000\nÜber\n"));
        QCOMPARE(SrtParser::parse(wide).cues.value(0).text, QStringLiteral("Über"));
        // Not UTF-8: the system codepage
        const QByteArray latin1("Caf\xe9");
        QStringDecoder system(QStringConverter::System);
        QCOMPARE(SrtParser::decode(latin1), system(latin1));
    }

    void srt_timestamps()
    {
        QCOMPARE(SrtParser::parseTimestamp(u"01:02:03,456").value_or(-1), 3723.456);
        QCOMPARE(SrtParser::parseTimestamp(u"1:02:03.5").value_or(-1), 3723.5);
        QCOMPARE(SrtParser::parseTimestamp(u" 02:03,04 ").value_or(-1), 123.04);
        QVERIFY(!SrtParser::parseTimestamp(u"00:61:00,000"));
        QVERIFY(!SrtParser::parseTimestamp(u"soon"));
        QVERIFY(!SrtParser::parseTimestamp(u""));
    }

    // ---- Cue lookup ----

    void cueAt_findsTheShowingCue()
    {
        SubtitleTrack track;
        track.cues = {{1, 2, "a", {}}, {3, 6, "b", {}}, {4, 5, "c", {}}, {10, 11, "d", {}}};
        QCOMPARE(track.cueAt(0.5), -1);
        QCOMPARE(track.cueAt(1), 0);
        QCOMPARE(track.cueAt(1.999), 0);
        QCOMPARE(track.cueAt(2), -1);    // the end is exclusive
        QCOMPARE(track.cueAt(3.5), 1);
        QCOMPARE(track.cueAt(4.5), 2);   // overlapping: the later start wins
        QCOMPARE(track.cueAt(5.5), 1);   // and the earlier one shows again after it
        QCOMPARE(track.cueAt(10.5), 3);
        QCOMPARE(track.cueAt(99), -1);
        QCOMPARE(SubtitleTrack{}.cueAt(1), -1);
    }

    // ---- DVD subtitles ----

    void dvd_readCue_timing()
    {
        const auto cue = DvdSubtitle::readCue(samplePacket(), 100);
        QVERIFY(cue.has_value());
        QCOMPARE(cue->start, 100.0);
        QVERIFY(qAbs(cue->end - 102.048) < 1e-9);
        QVERIFY(cue->isPicture());

        const auto open = DvdSubtitle::readCue(samplePacket(false), 7);
        QVERIFY(open.has_value());
        QVERIFY(std::isnan(open->end));
    }

    void dvd_render_decodesBothFields()
    {
        const auto picture = DvdSubtitle::render(samplePacket(), samplePalette());
        QVERIFY(picture.has_value());
        QCOMPARE(picture->position, QRect(10, 20, 4, 3));
        const QImage &image = picture->image;
        const QRgb c1 = qRgba(10, 100, 200, 255), c2 = qRgba(20, 100, 200, 255), c3 = qRgba(30, 100, 200, 136);
        const QList<QList<QRgb>> expected{{c1, c1, c2, c2}, {c3, c3, c3, c3}, {c1, c1, c1, c1}};
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 4; ++x)
                QCOMPARE(image.pixel(x, y), expected[y][x]);
    }

    void dvd_emptyOrTransparent_givesNothing()
    {
        QVERIFY(!DvdSubtitle::readCue(QByteArray::fromHex("0002"), 1)); // the "clear" packets between cues
        QByteArray invisible = samplePacket();
        invisible[16] = 0; // the alpha command's values: all four colours transparent
        invisible[17] = 0;
        QVERIFY(!DvdSubtitle::render(invisible, samplePalette()));
    }

    void dvd_damagedPackets_neverCrash()
    {
        // Every truncation of a valid packet, then random bytes; in a debug build QByteArray
        // asserts on any read outside the packet.
        const QByteArray packet = samplePacket();
        for (qsizetype n = 0; n <= packet.size(); ++n) {
            DvdSubtitle::readCue(packet.left(n), 0);
            DvdSubtitle::render(packet.left(n), samplePalette());
        }
        QRandomGenerator random(1234);
        for (int i = 0; i < 3000; ++i) {
            QByteArray junk(random.bounded(1, 200), Qt::Uninitialized);
            for (char &c : junk)
                c = static_cast<char>(random.bounded(256));
            if (i % 2 == 0 && junk.size() >= 4) {
                junk[2] = 0; // point the control sequence inside the packet more often
                junk[3] = static_cast<char>(random.bounded(static_cast<int>(junk.size())));
            }
            DvdSubtitle::readCue(junk, 0);
            DvdSubtitle::render(junk, samplePalette());
        }
        // A control sequence that points back at an earlier one
        QByteArray loop = samplePacket(false);
        loop[10] = 0;
        loop[11] = 4;
        DvdSubtitle::readCue(loop, 0);
    }

    void dvd_parsePalette()
    {
        const auto palette = DvdSubtitle::parsePalette(
            "size: 720x480\npalette: df0000, 000000, 0000dd, e1e1e1, deb6d1, afdbd7, aee4b2, dfe400, de00d3, "
            "de7900, 8a00e0, 00e37d, 00007a, 9665a1, 8aab62, cdcdcd\n");
        QVERIFY(palette.hasColors);
        QCOMPARE(palette.canvas, QSize(720, 480));
        QCOMPARE(palette.colors[0], qRgb(0xdf, 0, 0));
        QCOMPARE(palette.colors[15], qRgb(0xcd, 0xcd, 0xcd));

        const auto fallback = DvdSubtitle::parsePalette("palette: 1, 2, 3\nsize: huge");
        QVERIFY(!fallback.hasColors);
        QVERIFY(fallback.canvas.isEmpty());
        QCOMPARE(fallback.colors[15], qRgb(255, 255, 255));
    }

    void dvd_parseHexDump()
    {
        const QString dump = "\n00000000: 0898 0879 0000 0003 0003 0003 0003 0003  ...y............\n"
                             "00000010: ff00 9808 9102 ffff                      ........\n";
        QCOMPARE(DvdSubtitle::parseHexDump(dump), QByteArray::fromHex("08980879000000030003000300030003ff0098089102ffff"));
        QCOMPARE(DvdSubtitle::parseHexDump(hexDump(samplePacket())), samplePacket());
        QVERIFY(DvdSubtitle::parseHexDump("garbage").isEmpty());
    }

    void extractor_parsePictureTrack()
    {
        const QString header = QString::fromLatin1(QByteArray("size: 640x360\npalette: " + QByteArray("000000, ").repeated(15)
                                                              + "ffffff\n"));
        const auto packet = [](const char *pts, const char *duration, const QByteArray &data) {
            QJsonObject object{{"data", hexDump(data)}};
            if (pts)
                object["pts_time"] = pts;
            if (duration)
                object["duration_time"] = duration;
            return object;
        };
        const QJsonObject root{
            {"packets", QJsonArray{packet("1.000000", "0.5", samplePacket()), packet("5.000000", "3.0", samplePacket(false)),
                                   packet("6.500000", nullptr, QByteArray::fromHex("0002")),
                                   packet(nullptr, nullptr, samplePacket())}},
            {"streams", QJsonArray{QJsonObject{{"width", 720}, {"height", 480}, {"extradata", hexDump(header.toLatin1())}}}}};
        const QByteArray json = QJsonDocument(root).toJson();
        const SubtitleTrack track = SubtitleExtractor::parsePictureTrack(json);
        QCOMPARE(track.canvas, QSize(640, 360));
        QCOMPARE(track.palette[15], qRgb(255, 255, 255));
        QCOMPARE(track.cues.size(), 2); // the clear packet and the one without a time are dropped
        QCOMPARE(track.cues[0].start, 1.0);
        QVERIFY(qAbs(track.cues[0].end - 3.048) < 1e-9); // its stop command
        QCOMPARE(track.cues[1].start, 5.0);
        QCOMPARE(track.cues[1].end, 6.5); // no stop: until the next packet

        QVERIFY(SubtitleExtractor::parsePictureTrack("not json").isEmpty());
    }

    // ---- Files and formats ----

    void sidecarFiles_belongToTheVideo()
    {
        QTemporaryDir dir;
        for (const char *name : {"movie.mp4", "movie.srt", "Movie.en.srt", "movie.ass", "moviex.srt", "other.srt",
                                 "movie.txt"}) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QStringList names;
        for (const QString &path : SubtitleExtractor::sidecarFiles(dir.filePath("movie.mp4")))
            names << QFileInfo(path).fileName();
        QCOMPARE(names, (QStringList{"movie.srt", "movie.ass", "Movie.en.srt"}));
    }

    void style_onlyListedChoicesSurvive()
    {
        const SubtitleStyle style = SubtitleStyle::fromJson(
            QJsonObject{{"size", 160}, {"background", "outline"}, {"position", 20}});
        QCOMPARE(style.size, 160);
        QCOMPARE(style.background, "outline");
        QCOMPARE(style.position, 20);
        QCOMPARE(SubtitleStyle::fromJson(style.toJson()), style);

        const SubtitleStyle junk = SubtitleStyle::fromJson(
            QJsonObject{{"size", 999}, {"background", "neon"}, {"position", 12.5}});
        QCOMPARE(junk, SubtitleStyle{});
        QCOMPARE(SubtitleStyle::fromJson({}), SubtitleStyle{});
    }

    void subtitleFormats()
    {
        QCOMPARE(subtitleFormat("subrip"), SubtitleFormat::Text);
        QCOMPARE(subtitleFormat("mov_text"), SubtitleFormat::Text);
        QCOMPARE(subtitleFormat("dvd_subtitle"), SubtitleFormat::DvdPicture);
        QCOMPARE(subtitleFormat("hdmv_pgs_subtitle"), SubtitleFormat::Unsupported);
        QCOMPARE(subtitleFormat("some_future_codec"), SubtitleFormat::Unsupported);
        QVERIFY(SubtitleExtractor::isSubtitleFile("a/b/Film.SRT"));
        QVERIFY(!SubtitleExtractor::isSubtitleFile("film.mp4"));
    }
};

QTEST_GUILESS_MAIN(SubtitlesTest)
#include "tst_subtitles.moc"
