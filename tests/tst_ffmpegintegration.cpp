// Runs real exports on clips generated with ffmpeg's lavfi sources. Skips when ffmpeg/ffprobe
// can't be found (set CLIPVIEWER_FFMPEG_DIR or add them to PATH).
#include "media/ClipExporter.h"
#include "media/FrameGrabber.h"
#include "media/MediaProbe.h"

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

    void export_cancelled_deletesPartialOutput()
    {
        CancelToken cancel;
        ExportRequest request{samplePath(), output("cancelled.mp4"), 0, 10, ExportMode::Reencode};
        request.preset = "veryslow";
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

    void grabFrame_pastEnd_returnsNull()
    {
        QVERIFY(FrameGrabber(paths()).grab(samplePath(), 60).isNull());
    }
};

QTEST_GUILESS_MAIN(FfmpegIntegrationTest)
#include "tst_ffmpegintegration.moc"
