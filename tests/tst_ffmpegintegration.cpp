// Runs real exports on clips generated with ffmpeg's lavfi sources. Skips when ffmpeg/ffprobe
// can't be found (set CLIPVIEWER_FFMPEG_DIR or add them to PATH).
#include "media/ClipExporter.h"
#include "media/FrameGrabber.h"
#include "media/MediaProbe.h"
#include "media/SubtitleExtractor.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <mutex>
#include <optional>

using namespace cv;

class FfmpegIntegrationTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    std::optional<FfmpegPaths> m_paths;

    QString samplePath() const { return m_dir.filePath("sample.mp4"); }
    QString mpegPath() const { return m_dir.filePath("sample-mpeg4.mkv"); }
    QString output(const QString &name) const { return m_dir.filePath(name); }

    QString writeFile(const QString &name, const QByteArray &contents) const
    {
        QFile file(output(name));
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size())
            qFatal("Can't write %s", qPrintable(file.fileName()));
        return file.fileName();
    }

    // The sample's video with two cues muxed in as a subtitle track of the given codec. No audio:
    // ffmpeg 6.x's mkv muxer shifts every stream by the AAC priming (23 ms), cues included.
    QString withSubtitles(const QString &name, const QString &codec) const
    {
        const QString srt = writeFile("cues.srt", "1\n00:00:01,000 --> 00:00:02,500\nHello\n\n"
                                                  "2\n00:00:03,000 --> 00:00:04,000\n<i>World</i>\n");
        runTool(paths().ffmpeg,
                {"-hide_banner", "-loglevel", "error", "-y", "-i", samplePath(), "-i", srt, "-map", "0:v", "-map", "1",
                 "-c", "copy", "-c:s", codec, output(name)},
                {});
        return output(name);
    }

    static void verifyCues(const SubtitleTrack &track)
    {
        QCOMPARE(track.cues.size(), 2);
        QCOMPARE(track.cues[0].start, 1.0);
        QCOMPARE(track.cues[0].end, 2.5);
        QCOMPARE(track.cues[0].text, "Hello");
        QCOMPARE(track.cues[1].start, 3.0);
        QCOMPARE(track.cues[1].text, "<i>World</i>");
    }
    const FfmpegPaths &paths() const { return *m_paths; }

    // MD5 of every decoded video frame, in order.
    QStringList frameHashes(const QStringList &input) const
    {
        QStringList args{"-hide_banner", "-nostdin", "-loglevel", "error"};
        args << input << "-map" << "0:v:0" << "-f" << "framemd5" << "-";
        const QString out = QString::fromUtf8(runTool(paths().ffmpeg, args, {}));
        QStringList hashes;
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty() && !trimmed.startsWith('#'))
                hashes << trimmed.mid(trimmed.lastIndexOf(',') + 1).trimmed();
        }
        return hashes;
    }

    QStringList partsFolders(const QString &baseName) const
    {
        return QDir(m_dir.path())
            .entryList({"." + baseName + ".parts-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    }

private slots:
    void initTestCase()
    {
        m_paths = FfmpegPaths::locate();
        if (!m_paths)
            QSKIP("ffmpeg/ffprobe not found (set CLIPVIEWER_FFMPEG_DIR or add them to PATH)");
        QVERIFY(m_dir.isValid());

        // Keyframe every second (with B-frames), so a smart cut has a head, a copied middle and a tail.
        runTool(paths().ffmpeg,
                {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                 "testsrc2=size=320x240:rate=30:duration=10", "-f", "lavfi", "-i", "sine=frequency=440:duration=10",
                 "-c:v", "libx264", "-g", "30", "-sc_threshold", "0", "-pix_fmt", "yuv420p", "-c:a", "aac",
                 "-shortest", samplePath()},
                {});
        // Same picture in a codec smart cut can't handle.
        runTool(paths().ffmpeg,
                {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                 "testsrc2=size=320x240:rate=30:duration=4", "-c:v", "mpeg4", mpegPath()},
                {});
    }

    void probe_readsSample()
    {
        const MediaInfo info = MediaProbe(paths()).probe(samplePath());
        QVERIFY(qAbs(info.duration - 10) < 0.1);
        QCOMPARE(info.videoCodec, "h264");
        QCOMPARE(info.width, 320);
        QCOMPARE(info.height, 240);
        QVERIFY(qAbs(info.frameRate - 30) < 0.01);
        QCOMPARE(info.audioCodec, "aac");
    }

    void probe_nonVideoFile_throws()
    {
        QFile file(output("not-a-video.mp4"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello");
        file.close();
        QVERIFY_THROWS_EXCEPTION(FfmpegError, MediaProbe(paths()).probe(file.fileName()));
    }

    void export_reencode_isFrameAccurate()
    {
        QList<double> reports;
        const ExportResult result = ClipExporter(paths()).exportClip(
            {samplePath(), output("reencode.mp4"), 2.5, 6, ExportMode::Reencode}, [&](double p) { reports << p; });

        const MediaInfo info = MediaProbe(paths()).probe(output("reencode.mp4"));
        QVERIFY(qAbs(info.duration - 3.5) < 0.1);
        QCOMPARE(reports.last(), 1.0);
        QVERIFY(result.fallbackReason.isEmpty());
    }

    void export_smartCut_copiesWholeGopsAndIsFrameAccurate()
    {
        std::mutex gate;
        QList<double> reports;
        // Keyframes every second: re-encodes 2.5-3 and 7-7.5, copies 3-7.
        const ExportResult result = ClipExporter(paths()).exportClip(
            {samplePath(), output("smart.mp4"), 2.5, 7.5, ExportMode::SmartCut}, [&](double p) {
                std::lock_guard lock(gate);
                reports << p;
            });

        QVERIFY(result.fallbackReason.isEmpty());
        QCOMPARE(reports.last(), 1.0);
        const QStringList source = frameHashes({"-ss", "2.5", "-i", samplePath(), "-t", "5"});
        const QStringList clip = frameHashes({"-i", output("smart.mp4")});
        QCOMPARE(source.size(), 150);
        QCOMPARE(clip.size(), source.size());
        // The copied frames (3 s to 7 s) are the source's own, in the right place.
        QCOMPARE(clip.mid(15, 120), source.mid(15, 120));
        QVERIFY(clip.mid(0, 15) != source.mid(0, 15)); // re-encoded, so not bit-identical

        const MediaInfo info = MediaProbe(paths()).probe(output("smart.mp4"));
        QCOMPARE(info.videoCodec, "h264");
        QCOMPARE(info.audioCodec, "aac");
        QVERIFY(qAbs(info.duration - 5) < 0.1);
        QVERIFY(partsFolders("smart").isEmpty());
    }

    void export_smartCut_onKeyframes_copiesEverything()
    {
        ClipExporter(paths()).exportClip({samplePath(), output("smart-keyframes.mp4"), 3, 7, ExportMode::SmartCut});
        const QStringList source = frameHashes({"-ss", "3", "-i", samplePath(), "-t", "4"});
        QCOMPARE(source.size(), 120);
        QCOMPARE(frameHashes({"-i", output("smart-keyframes.mp4")}), source);
    }

    void export_smartCut_insideOneGop_encodesExactRange()
    {
        ClipExporter(paths()).exportClip({samplePath(), output("smart-short.mp4"), 4.2, 4.8, ExportMode::SmartCut});
        QCOMPARE(frameHashes({"-i", output("smart-short.mp4")}).size(), 18);
    }

    void export_smartCut_unsupportedCodec_fallsBackToReencode()
    {
        const ExportResult result =
            ClipExporter(paths()).exportClip({mpegPath(), output("smart-mpeg4.mp4"), 1, 3, ExportMode::SmartCut});
        QVERIFY(result.fallbackReason.contains("mpeg4"));
        const MediaInfo info = MediaProbe(paths()).probe(output("smart-mpeg4.mp4"));
        QCOMPARE(info.videoCodec, "h264");
        QVERIFY(qAbs(info.duration - 2) < 0.1);
    }

    void export_reencode_uprightClip_scalesShortSideAndLimitsFrameRate()
    {
        const QString upright = output("upright60.mp4");
        runTool(paths().ffmpeg,
                {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                 "testsrc2=size=720x1280:rate=60:duration=1", "-f", "lavfi", "-i", "sine=duration=1", "-c:v",
                 "libx264", "-preset", "ultrafast", "-c:a", "aac", "-shortest", upright},
                {});
        ExportRequest request{upright, output("upright-480p30.mp4"), 0, 1, ExportMode::Reencode};
        request.reencode.maxHeight = 480;
        request.reencode.maxFrameRate = 30;
        request.reencode.audioBitrate = 0;
        ClipExporter(paths()).exportClip(request);

        const MediaInfo info = MediaProbe(paths()).probe(request.outputPath);
        QCOMPARE(info.width, 480);
        QVERIFY2(qAbs(info.height - 853) <= 1 && info.height % 2 == 0, qPrintable(QString::number(info.height)));
        QVERIFY(qAbs(info.frameRate - 30) < 0.01);
        QVERIFY(!info.hasAudio());
    }

    void export_reencode_neverScalesUp()
    {
        ExportRequest request{samplePath(), output("no-upscale.mp4"), 0, 1, ExportMode::Reencode};
        request.reencode.maxHeight = 1080;
        request.reencode.maxFrameRate = 60; // above the source's 30
        ClipExporter(paths()).exportClip(request);

        const MediaInfo info = MediaProbe(paths()).probe(request.outputPath);
        QCOMPARE(info.width, 320);
        QCOMPARE(info.height, 240);
        QVERIFY(qAbs(info.frameRate - 30) < 0.01);
        QCOMPARE(info.audioCodec, "aac");
    }

    void export_cancelled_deletesPartialOutput()
    {
        CancelToken cancel;
        ExportRequest request{samplePath(), output("cancelled.mp4"), 0, 10, ExportMode::Reencode};
        request.reencode.preset = "veryslow";
        QVERIFY_THROWS_EXCEPTION(OperationCancelled, ClipExporter(paths()).exportClip(request, [&](double p) {
            if (p > 0)
                cancel.cancel();
        }, cancel));
        QVERIFY(!QFile::exists(output("cancelled.mp4")));
    }

    void export_smartCut_cancelled_deletesOutputAndParts()
    {
        CancelToken cancel;
        // The first report comes when the first part finishes, before the final mux.
        QVERIFY_THROWS_EXCEPTION(OperationCancelled,
                                 ClipExporter(paths()).exportClip(
                                     {samplePath(), output("smart-cancelled.mp4"), 0.5, 9.5, ExportMode::SmartCut},
                                     [&](double p) {
                                         if (p > 0 && p < 1)
                                             cancel.cancel();
                                     },
                                     cancel));
        QVERIFY(!QFile::exists(output("smart-cancelled.mp4")));
        QVERIFY(partsFolders("smart-cancelled").isEmpty());
    }

    void grabFrame_returnsImageOfVideoSize()
    {
        const QImage frame = FrameGrabber(paths()).grab(samplePath(), 3.5);
        QCOMPARE(frame.size(), QSize(320, 240));
    }

    void grabFrame_scalesDownToMaxWidth()
    {
        QCOMPARE(FrameGrabber(paths()).grab(samplePath(), 1, 160).width(), 160);
    }

    void grabFrame_fullSize_keepsOddSize()
    {
        const QString odd = output("odd.mkv");
        runTool(paths().ffmpeg,
                {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                 "testsrc=size=321x241:rate=10:duration=1", "-c:v", "ffv1", odd},
                {});
        QCOMPARE(FrameGrabber(paths()).grab(odd, 0.5, 0).size(), QSize(321, 241));
    }

    void subtitles_extractTextTrack_mkv()
    {
        const QString mkv = withSubtitles("subs.mkv", "srt");
        QCOMPARE(MediaProbe(paths()).probe(mkv).subtitleStreams().value(0).codec, "subrip");
        verifyCues(SubtitleExtractor(paths()).extract(mkv, 0, SubtitleFormat::Text, {}));
    }

    void subtitles_extractTextTrack_mp4MovText()
    {
        const QString mp4 = withSubtitles("subs.mp4", "mov_text");
        QCOMPARE(MediaProbe(paths()).probe(mp4).subtitleStreams().value(0).codec, "mov_text");
        verifyCues(SubtitleExtractor(paths()).extract(mp4, 0, SubtitleFormat::Text, {}));
    }

    void subtitles_loadAssFile()
    {
        const QString ass = writeFile(
            "styled.ass",
            "[Script Info]\nScriptType: v4.00+\n\n[V4+ Styles]\n"
            "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, "
            "Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, "
            "MarginL, MarginR, MarginV, Encoding\n"
            "Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1"
            "\n\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
            "Dialogue: 0,0:00:01.00,0:00:03.50,Default,,0,0,0,,{\\i1}Hello{\\i0} there\\Nsecond line\n");
        const SubtitleTrack track = SubtitleExtractor(paths()).loadFile(ass, {});
        QCOMPARE(track.cues.size(), 1);
        QCOMPARE(track.cues[0].start, 1.0);
        QCOMPARE(track.cues[0].end, 3.5);
        QCOMPARE(track.cues[0].text, "<i>Hello</i> there<br>second line");
    }

    void subtitles_badInput_throwsInsteadOfCrashing()
    {
        CancelToken cancelled;
        cancelled.cancel();
        const QString mkv = withSubtitles("subs-cancel.mkv", "srt");
        QVERIFY_THROWS_EXCEPTION(OperationCancelled,
                                 SubtitleExtractor(paths()).extract(mkv, 0, SubtitleFormat::Text, cancelled));
        // A stream that isn't there, a missing file, a video passed off as subtitles
        QVERIFY_THROWS_EXCEPTION(FfmpegError, SubtitleExtractor(paths()).extract(mkv, 5, SubtitleFormat::Text, {}));
        QVERIFY_THROWS_EXCEPTION(MediaError, SubtitleExtractor(paths()).loadFile(output("missing.srt"), {}));
        QVERIFY(SubtitleExtractor(paths()).loadFile(writeFile("fake.srt", "not subtitles"), {}).isEmpty());
    }

    void grabFrame_pastEnd_returnsNull()
    {
        QVERIFY(FrameGrabber(paths()).grab(samplePath(), 60).isNull());
    }
};

QTEST_GUILESS_MAIN(FfmpegIntegrationTest)
#include "tst_ffmpegintegration.moc"
