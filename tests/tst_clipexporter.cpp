// ClipExporter's argument building and parsing, SmartCutPlan and MediaProbe's JSON parsing. No
// ffmpeg needed; tst_ffmpegintegration runs the real thing.
#include "media/ClipExporter.h"
#include "media/MediaProbe.h"
#include "media/SmartCutPlan.h"

#include <QTemporaryFile>
#include <QTest>

using namespace cv;

namespace {

// Whether args contains expected as a contiguous run.
bool containsSequence(const QStringList &args, const QStringList &expected)
{
    for (qsizetype i = 0; i + expected.size() <= args.size(); ++i)
        if (args.mid(i, expected.size()) == expected)
            return true;
    return false;
}

#define VERIFY_SEQUENCE(args, ...)                                                                                     \
    QVERIFY2(containsSequence(args, QStringList{__VA_ARGS__}),                                                         \
             qPrintable(QStringLiteral("Expected [%1] in [%2]")                                                        \
                            .arg(QStringList{__VA_ARGS__}.join(' '), (args).join(' '))))

constexpr double Tol = 0.5 / 30;
const QList<double> Keys{0, 4, 8, 12, 16};

} // namespace

class ClipExporterTest : public QObject
{
    Q_OBJECT

private slots:
    void buildArguments_reencode_usesSettings()
    {
        ExportRequest request{"in.mkv", "out.mp4", 1.5, 4, ExportMode::Reencode};
        request.crf = 23;
        request.preset = "slow";
        const QStringList args = ClipExporter::buildArguments(request);

        VERIFY_SEQUENCE(args, "-hide_banner", "-nostdin", "-nostats", "-loglevel", "error", "-y");
        VERIFY_SEQUENCE(args, "-ss", "1.500", "-i", "in.mkv", "-t", "2.500");
        VERIFY_SEQUENCE(args, "-c:v", "libx264", "-preset", "slow", "-crf", "23");
        VERIFY_SEQUENCE(args, "-c:a", "aac");
        VERIFY_SEQUENCE(args, "-movflags", "+faststart");
        QVERIFY(!args.contains("copy"));
        QCOMPARE(args.last(), "out.mp4");
    }

    void exportRequest_defaultsToVeryfastCrf18()
    {
        const ExportRequest request{"in.mp4", "out.mp4", 0, 1, ExportMode::Reencode};
        QCOMPARE(request.preset, "veryfast");
        QCOMPARE(request.crf, 18);
    }

    void copySegmentArguments_dropsPacketsOutsideSegmentByPts()
    {
        const QStringList args = ClipExporter::copySegmentArguments("in.mkv", {4, 10, true}, 0.01);
        VERIFY_SEQUENCE(args, "-ss", "4.000", "-i", "in.mkv", "-t", "7.000");
        VERIFY_SEQUENCE(args, "-c:v", "copy", "-bsf:v", R"(noise=drop=lt(pts*tb\,-0.010)+gte(pts*tb\,5.990))");
        VERIFY_SEQUENCE(args, "-f", "mpegts");
    }

    void encodeSegmentArguments_startsExactlyAndStopsHalfAFrameEarly()
    {
        const QStringList args = ClipExporter::encodeSegmentArguments("in.mkv", "yuvj420p", {2.5, 4, false}, 0.01);
        VERIFY_SEQUENCE(args, "-ss", "2.500", "-i", "in.mkv", "-t", "1.490");
        VERIFY_SEQUENCE(args, "-c:v", "libx264");
        VERIFY_SEQUENCE(args, "-pix_fmt", "yuvj420p");
    }

    void parseKeyframes_keepsKeyframesRelativeToFileStart()
    {
        const QString csv = "10.500000,K__\n10.550000,___\r\n12.500000,K_\nN/A,K__\n12.500000,K__\n";
        QCOMPARE(ClipExporter::parseKeyframes(csv, 10.5), (QList<double>{0.0, 2.0}));
    }

    void parseProgressSeconds()
    {
        QCOMPARE(ClipExporter::parseProgressSeconds("out_time_us=2500000"), 2.5);
        QCOMPARE(ClipExporter::parseProgressSeconds("out_time_us=0"), 0.0);
        QCOMPARE(ClipExporter::parseProgressSeconds("out_time_us=N/A"), -1.0);
        QCOMPARE(ClipExporter::parseProgressSeconds("out_time=00:00:02.500000"), -1.0);
        QCOMPARE(ClipExporter::parseProgressSeconds("progress=continue"), -1.0);
    }

    void formatSeconds()
    {
        QCOMPARE(ClipExporter::formatSeconds(1.5), "1.500");
        QCOMPARE(ClipExporter::formatSeconds(0), "0.000");
        QCOMPARE(ClipExporter::formatSeconds(1.0 / 3), "0.333333");
        QCOMPARE(ClipExporter::formatSeconds(-0.01), "-0.010");
        QCOMPARE(ClipExporter::formatSeconds(12.0625), "12.0625");
    }

    void validate_rejectsBadRanges()
    {
        QTemporaryFile input;
        QVERIFY(input.open());
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 ClipExporter::validate({input.fileName(), "out.mp4", 5, 5, ExportMode::SmartCut}));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 ClipExporter::validate({input.fileName(), "out.mp4", -1, 5, ExportMode::SmartCut}));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 ClipExporter::validate({input.fileName(), input.fileName(), 0, 5, ExportMode::SmartCut}));
        ClipExporter::validate({input.fileName(), "out.mp4", 0, 5, ExportMode::SmartCut});
    }

    // ---- SmartCutPlan ----

    void plan_betweenKeyframes_encodesHeadAndTail_copiesMiddle()
    {
        QCOMPARE(SmartCutPlan::create(Keys, 2.5, 13, Tol),
                 (QList<SmartCutSegment>{{2.5, 4, false}, {4, 12, true}, {12, 13, false}}));
    }

    void plan_onKeyframes_copiesEverything()
    {
        QCOMPARE(SmartCutPlan::create(Keys, 4, 12, Tol), (QList<SmartCutSegment>{{4, 12, true}}));
    }

    void plan_keyframeWithinHalfAFrame_countsAsOnTheCutPoint()
    {
        QCOMPARE(SmartCutPlan::create(Keys, 4.01, 11.99, Tol), (QList<SmartCutSegment>{{4.01, 11.99, true}}));
    }

    void plan_oneKeyframeInRange_encodesEverything()
    {
        QCOMPARE(SmartCutPlan::create(Keys, 3, 7, Tol), (QList<SmartCutSegment>{{3, 7, false}}));
    }

    void plan_noKeyframeInRange_encodesEverything()
    {
        QCOMPARE(SmartCutPlan::create(Keys, 1, 3, Tol), (QList<SmartCutSegment>{{1, 3, false}}));
    }

    void plan_unorderedKeyframes_areSorted()
    {
        QCOMPARE(SmartCutPlan::create({8, 0, 4}, 1, 9, Tol),
                 (QList<SmartCutSegment>{{1, 4, false}, {4, 8, true}, {8, 9, false}}));
    }

    // ---- MediaProbe::parse ----

    void probeParse_readsStreams()
    {
        const QByteArray json = R"({
            "streams": [
                { "codec_type": "video", "codec_name": "mjpeg", "disposition": { "attached_pic": 1 } },
                { "codec_type": "video", "codec_name": "h264", "width": 1920, "height": 1080,
                  "avg_frame_rate": "60000/1001", "r_frame_rate": "60/1" },
                { "codec_type": "audio", "codec_name": "aac" }
            ],
            "format": { "format_name": "mov,mp4", "duration": "12.500000", "size": "1234" }
        })";
        const MediaInfo info = MediaProbe::parse("clip.mp4", json);
        QCOMPARE(info.videoCodec, "h264");
        QCOMPARE(info.width, 1920);
        QCOMPARE(info.height, 1080);
        QVERIFY(qAbs(info.frameRate - 59.94) < 0.01);
        QCOMPARE(info.duration, 12.5);
        QCOMPARE(info.sizeBytes, 1234);
        QCOMPARE(info.audioCodec, "aac");
        QVERIFY(info.hasAudio());
    }

    void probeParse_noVideo_throws()
    {
        QVERIFY_THROWS_EXCEPTION(MediaError,
                                 MediaProbe::parse("a.mp3", R"({ "streams": [ { "codec_type": "audio" } ] })"));
    }

    void parseRate()
    {
        QCOMPARE(MediaProbe::parseRate("30/1").value_or(-1), 30.0);
        QVERIFY(!MediaProbe::parseRate("0/0").has_value());
        QVERIFY(!MediaProbe::parseRate("").has_value());
    }
};

QTEST_GUILESS_MAIN(ClipExporterTest)
#include "tst_clipexporter.moc"
