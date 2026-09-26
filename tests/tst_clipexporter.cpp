// ClipExporter's argument building and parsing, SmartCutPlan, MediaProbe's JSON parsing and the
// media info text. No
// ffmpeg needed; tst_ffmpegintegration runs the real thing.
#include "media/ClipExporter.h"
#include "media/MediaDetails.h"
#include "media/MediaProbe.h"
#include "media/SmartCutPlan.h"

#include <QJsonDocument>
#include <QJsonObject>
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
        request.reencode.crf = 23;
        request.reencode.preset = "slow";
        const QStringList args = ClipExporter::buildArguments(request);

        VERIFY_SEQUENCE(args, "-hide_banner", "-nostdin", "-nostats", "-loglevel", "error", "-y");
        VERIFY_SEQUENCE(args, "-ss", "1.500", "-i", "in.mkv", "-t", "2.500");
        VERIFY_SEQUENCE(args, "-map", "0:v:0", "-map", "0:a:0?");
        VERIFY_SEQUENCE(args, "-c:v", "libx264", "-preset", "slow", "-crf", "23");
        VERIFY_SEQUENCE(args, "-c:a", "aac", "-b:a", "192k");
        VERIFY_SEQUENCE(args, "-movflags", "+faststart");
        QVERIFY(!args.contains("copy"));
        QVERIFY(!args.contains("-vf")); // original size and frame rate
        QCOMPARE(args.last(), "out.mp4");
    }

    void buildArguments_reencode_scalesLimitsFrameRateAndSetsAudio()
    {
        ExportRequest request{"in.mkv", "out.mp4", 0, 4, ExportMode::Reencode};
        request.reencode.maxHeight = 720;
        request.reencode.maxFrameRate = 30;
        request.reencode.audioBitrate = 128;
        const QStringList args = ClipExporter::buildArguments(request, 60);
        VERIFY_SEQUENCE(args, "-vf",
                        "fps=30,scale=w='if(gte(iw,ih),-2,min(720,iw))':h='if(gte(iw,ih),min(720,ih),-2)'");
        VERIFY_SEQUENCE(args, "-c:a", "aac", "-b:a", "128k");
    }

    void buildArguments_reencode_leavesSlowerFrameRateAlone()
    {
        ExportRequest request{"in.mkv", "out.mp4", 0, 4, ExportMode::Reencode};
        request.reencode.maxFrameRate = 30;
        QVERIFY(!ClipExporter::buildArguments(request, 29.97).contains("-vf"));
        QVERIFY(!ClipExporter::buildArguments(request, 0).contains("-vf")); // unknown
    }

    void buildArguments_reencode_noAudio()
    {
        ExportRequest request{"in.mkv", "out.mp4", 0, 4, ExportMode::Reencode};
        request.reencode.audioBitrate = 0;
        const QStringList args = ClipExporter::buildArguments(request);
        QVERIFY(args.contains("-an"));
        QVERIFY(!args.contains("0:a:0?"));
        QVERIFY(!args.contains("-c:a"));
    }

    void exportRequest_defaultsToVeryfastCrf18()
    {
        const ExportRequest request{"in.mp4", "out.mp4", 0, 1, ExportMode::Reencode};
        QCOMPARE(request.reencode.preset, "veryfast");
        QCOMPARE(request.reencode.crf, 18);
        QCOMPARE(request.reencode.maxHeight, 0);
        QCOMPARE(request.reencode.maxFrameRate, 0);
        QCOMPARE(request.reencode.audioBitrate, 192);
    }

    void reencodeOptions_jsonRoundTrip()
    {
        ReencodeOptions options;
        options.crf = 28;
        options.preset = "medium";
        options.maxHeight = 480;
        options.maxFrameRate = 60;
        options.audioBitrate = 0;
        QCOMPARE(ReencodeOptions::fromJson(options.toJson()), options);
    }

    void reencodeOptions_valuesOutsideTheChoices_fallBack()
    {
        const QJsonObject json{{"crf", 0},          {"preset", "placebo"}, {"maxHeight", 1080.5},
                               {"maxFrameRate", "30"}, {"audioBitrate", 64}};
        QCOMPARE(ReencodeOptions::fromJson(json), ReencodeOptions{});
        QCOMPARE(ReencodeOptions::fromJson({}), ReencodeOptions{});
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

    void probeParse_readsStreamDetails()
    {
        const QByteArray json = R"({
            "streams": [
                { "codec_type": "video", "codec_name": "h264", "profile": "High", "width": 1920, "height": 1080,
                  "avg_frame_rate": "30/1", "pix_fmt": "yuv420p", "bit_rate": "8200000", "sample_aspect_ratio": "4:3",
                  "side_data_list": [ { "side_data_type": "Display Matrix", "rotation": -90 } ] },
                { "codec_type": "audio", "codec_name": "aac", "profile": "LC", "sample_rate": "48000", "channels": 2,
                  "channel_layout": "stereo", "tags": { "language": "eng", "BPS": "192000" } },
                { "codec_type": "audio", "codec_name": "opus", "channels": 6,
                  "tags": { "language": "und", "title": " Commentary " } },
                { "codec_type": "subtitle", "codec_name": "subrip", "tags": { "language": "fre" } }
            ],
            "format": { "format_name": "matroska,webm", "format_long_name": "Matroska / WebM",
                        "duration": "90", "size": "2048", "bit_rate": "8500000" }
        })";
        const MediaInfo info = MediaProbe::parse("clip.mkv", json);
        QCOMPARE(info.rotation, 90);
        QCOMPARE(info.sampleAspectRatio, 4.0 / 3);
        QCOMPARE(info.displayAspect(), 1080 / (1920 * 4.0 / 3)); // turned sideways
        QCOMPARE(info.streams.size(), 4);
        QCOMPARE(info.streams[1].bitRate, 192000);
        QCOMPARE(info.streams[2].language, QString());
        QCOMPARE(info.streams[2].title, "Commentary");

        using Row = MediaDetails::Row;
        QCOMPARE(MediaDetails::describe(info),
                 (QList<Row>{{"Container", "Matroska / WebM"},
                             {"Duration", "1:30.000"},
                             {"Size", "2 KB"},
                             {"Bit rate", "8.5 Mbit/s"},
                             {"Rotation", "90°"},
                             {"Video", "h264 (High) · 1920×1080 · 30 fps · yuv420p · 8.2 Mbit/s"},
                             {"Audio 1", "aac (LC) · 48 kHz · stereo · 192 kbit/s · eng"},
                             {"Audio 2", "opus · 6 ch · “Commentary”"},
                             {"Subtitles", "subrip · fre"}}));
    }

    void probeParse_rotation()
    {
        const auto rotation = [](const char *json) { return MediaProbe::parseRotation(QJsonDocument::fromJson(json).object()); };
        QCOMPARE(rotation(R"({})"), 0);
        QCOMPARE(rotation(R"({ "side_data_list": [ { "side_data_type": "Display Matrix", "rotation": 90 } ] })"), 270);
        QCOMPARE(rotation(R"({ "side_data_list": [ { "side_data_type": "Display Matrix", "rotation": -180 } ] })"), 180);
        QCOMPARE(rotation(R"({ "tags": { "rotate": "90" } })"), 90);
        QCOMPARE(rotation(R"({ "tags": { "rotate": "nonsense" } })"), 0);
    }

    void subtitleTracks_onlyTextOnesCanBeShown()
    {
        MediaInfo info;
        info.streams = {{"video", "h264"}, {"subtitle", "subrip"}, {"audio", "aac"}, {"subtitle", "dvd_subtitle"},
                        {"data", "bin_data"}, {"subtitle", "ass"}};
        QVERIFY(info.canShowSubtitleTrack(0, 3));
        QVERIFY(!info.canShowSubtitleTrack(1, 3)); // bitmap
        QVERIFY(info.canShowSubtitleTrack(2, 3));
        QVERIFY(!info.canShowSubtitleTrack(3, 3));
        QVERIFY(!info.canShowSubtitleTrack(-1, 3));
        // The player lists a different number: only safe when every track is text
        QVERIFY(!info.canShowSubtitleTrack(0, 2));
        info.streams[3].codec = "mov_text";
        QVERIFY(info.canShowSubtitleTrack(0, 2));
        QVERIFY(!MediaInfo::isTextSubtitleCodec("hdmv_pgs_subtitle"));
        QVERIFY(!MediaInfo::isTextSubtitleCodec("some_future_codec"));
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
