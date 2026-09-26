// Plays clips in each container/codec pair the app claims to support through Qt Multimedia (the
// same backend the player uses) and checks that video frames and audio come out. The clips are
// generated with ffmpeg's lavfi sources; the test skips itself when ffmpeg isn't found, and a row
// skips when the local ffmpeg lacks its encoder. Qt's FFmpeg build has no software AV1 decoder
// (no dav1d), so AV1 only plays with a hardware decoder; those rows skip on machines without one,
// such as the CI runners.
#include "media/FfmpegPaths.h"
#include "media/Process.h"

#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QMediaPlayer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVideoFrame>
#include <QVideoSink>

#include <optional>

using namespace cv;

class PlaybackFormatsTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    std::optional<FfmpegPaths> m_paths;
    QString m_encoders;

    static constexpr int Width = 320;
    static constexpr int Height = 240;
    static constexpr int Seconds = 2;

    bool hasEncoder(const QString &name) const
    {
        return m_encoders.contains(QRegularExpression(QStringLiteral("\\s%1\\s").arg(QRegularExpression::escape(name))));
    }

private slots:
    void initTestCase()
    {
        m_paths = FfmpegPaths::locate();
        if (!m_paths)
            QSKIP("ffmpeg/ffprobe not found (set CLIPVIEWER_FFMPEG_DIR or add them to PATH)");
        QVERIFY(m_dir.isValid());
        m_encoders = QString::fromUtf8(runTool(m_paths->ffmpeg, {"-hide_banner", "-encoders"}, {}));
    }

    void plays_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QString>("videoEncoder");
        QTest::addColumn<QString>("audioEncoder");
        QTest::addColumn<QStringList>("extraArgs");
        QTest::addColumn<bool>("hardwareOnly");

        QTest::newRow("mp4 h264") << "h264.mp4" << "libx264" << "aac" << QStringList{"-pix_fmt", "yuv420p"} << false;
        QTest::newRow("mp4 hevc") << "hevc.mp4" << "libx265" << "aac"
                                  << QStringList{"-pix_fmt", "yuv420p", "-tag:v", "hvc1", "-x265-params", "log-level=error"}
                                  << false;
        QTest::newRow("mp4 hevc 10-bit") << "hevc10.mp4" << "libx265" << "aac"
                                         << QStringList{"-pix_fmt", "yuv420p10le", "-tag:v", "hvc1", "-x265-params",
                                                        "log-level=error"} << false;
        QTest::newRow("mov h264") << "h264.mov" << "libx264" << "aac" << QStringList{"-pix_fmt", "yuv420p"} << false;
        QTest::newRow("mkv h264") << "h264.mkv" << "libx264" << "aac" << QStringList{"-pix_fmt", "yuv420p"} << false;
        QTest::newRow("mkv hevc") << "hevc.mkv" << "libx265" << "libopus"
                                  << QStringList{"-pix_fmt", "yuv420p", "-x265-params", "log-level=error"} << false;
        QTest::newRow("webm vp8") << "vp8.webm" << "libvpx" << "libvorbis" << QStringList{"-b:v", "500k"} << false;
        QTest::newRow("webm vp9") << "vp9.webm" << "libvpx-vp9" << "libopus" << QStringList{"-b:v", "500k"} << false;
        QTest::newRow("webm av1") << "av1.webm" << "libsvtav1" << "libopus" << QStringList{"-pix_fmt", "yuv420p"} << true;
        QTest::newRow("mp4 av1") << "av1.mp4" << "libsvtav1" << "aac" << QStringList{"-pix_fmt", "yuv420p"} << true;
        QTest::newRow("avi mpeg4") << "mpeg4.avi" << "mpeg4" << "libmp3lame" << QStringList{"-q:v", "4"} << false;
    }

    void plays()
    {
        QFETCH(QString, fileName);
        QFETCH(QString, videoEncoder);
        QFETCH(QString, audioEncoder);
        QFETCH(QStringList, extraArgs);
        QFETCH(bool, hardwareOnly);

        for (const QString &encoder : {videoEncoder, audioEncoder})
            if (!hasEncoder(encoder))
                QSKIP(qPrintable(QStringLiteral("this ffmpeg has no %1 encoder").arg(encoder)));

        const QString path = m_dir.filePath(fileName);
        QStringList args{"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                         QStringLiteral("testsrc2=size=%1x%2:rate=30:duration=%3").arg(Width).arg(Height).arg(Seconds),
                         "-f", "lavfi", "-i", QStringLiteral("sine=frequency=440:duration=%1").arg(Seconds),
                         "-c:v", videoEncoder, "-c:a", audioEncoder, "-shortest"};
        args << extraArgs << path;
        runTool(m_paths->ffmpeg, args, {});

        QMediaPlayer player;
        QVideoSink sink;
        QAudioBufferOutput audio;
        player.setVideoSink(&sink);
        player.setAudioBufferOutput(&audio);

        int frames = 0;
        QSize frameSize;
        connect(&sink, &QVideoSink::videoFrameChanged, this, [&](const QVideoFrame &frame) {
            if (!frame.isValid())
                return;
            ++frames;
            frameSize = frame.size();
        });
        qint64 audioUs = 0;
        connect(&audio, &QAudioBufferOutput::audioBufferReceived, this,
                [&](const QAudioBuffer &buffer) { audioUs += buffer.duration(); });
        QString error;
        connect(&player, &QMediaPlayer::errorOccurred, this,
                [&](QMediaPlayer::Error, const QString &message) { error = message; });

        player.setSource(QUrl::fromLocalFile(path));
        QTRY_VERIFY_WITH_TIMEOUT(player.mediaStatus() == QMediaPlayer::LoadedMedia || !error.isEmpty(), 10000);
        if (hardwareOnly && !error.isEmpty())
            QSKIP(qPrintable(QStringLiteral("no hardware decoder for this codec: %1").arg(error)));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(player.hasVideo());
        QVERIFY(player.hasAudio());
        QVERIFY2(qAbs(player.duration() - Seconds * 1000) < 100, qPrintable(QString::number(player.duration())));

        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.mediaStatus() == QMediaPlayer::EndOfMedia || !error.isEmpty(),
                                 Seconds * 1000 + 10000);
        if (hardwareOnly && (!error.isEmpty() || frames == 0))
            QSKIP("no hardware decoder for this codec");
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(frameSize, QSize(Width, Height));
        // Playback may drop a few frames under load, but most of the 30 fps must get through.
        QVERIFY2(frames >= Seconds * 30 * 3 / 4, qPrintable(QStringLiteral("%1 video frames").arg(frames)));
        const double audioSeconds = audioUs / 1e6;
        QVERIFY2(audioSeconds > Seconds * 0.9, qPrintable(QStringLiteral("%1 s of audio").arg(audioSeconds)));

        // A paused seek (what the player does on every scrub) must also produce a frame.
        player.pause();
        frames = 0;
        player.setPosition(Seconds * 1000 / 2);
        QTRY_VERIFY_WITH_TIMEOUT(frames > 0, 5000);
    }
};

QTEST_MAIN(PlaybackFormatsTest)
#include "tst_playbackformats.moc"
