#pragma once

#include "media/FfmpegPaths.h"
#include "media/MediaInfo.h"
#include "media/Process.h"
#include "subtitles/SubtitleTrack.h"

#include <QObject>
#include <QRectF>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <optional>

class StillFrameProvider;

// The subtitles, drawn by our own overlay rather than the player (Qt only draws text subtitles,
// only while playing, and crashes on DVD pictures). Lists the open video's subtitle streams and
// the subtitle files next to it or loaded by the user; a track is read on the thread pool the
// first time it's picked, then kept. setPosition() picks the cue for the playhead.
class SubtitleController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by EditorController")

    // [{ label, available, note }]; `note` says why an unavailable track can't be shown
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(int activeTrack READ activeTrack NOTIFY activeTrackChanged) // -1: off
    Q_PROPERTY(int loadingTrack READ loadingTrack NOTIFY loadingChanged) // being read, or -1
    // The cue at the playhead: StyledText, or a picture (image://subtitle/<n>) placed at
    // cueRect, given as fractions of the video frame.
    Q_PROPERTY(QString cueText READ cueText NOTIFY cueChanged)
    Q_PROPERTY(QUrl cueImage READ cueImage NOTIFY cueChanged)
    Q_PROPERTY(QRectF cueRect READ cueRect NOTIFY cueChanged)

public:
    SubtitleController(StillFrameProvider *pictures, QObject *parent = nullptr);
    ~SubtitleController() override;

    QVariantList tracks() const;
    int activeTrack() const { return m_active; }
    int loadingTrack() const { return m_loadingTrack; }
    QString cueText() const { return m_cueText; }
    QUrl cueImage() const { return m_cueImage; }
    QRectF cueRect() const { return m_cueRect; }

    // A new video: lists its subtitle streams and sidecar files, and turns on the first sidecar
    // ("<name>.srt"). nullptr clears everything.
    void setMedia(const std::optional<cv::FfmpegPaths> &paths, const cv::MediaInfo *info);

    // -1 turns subtitles off. A track that isn't read yet is read first (loadingTrack meanwhile).
    Q_INVOKABLE void setActiveTrack(int index);
    // V: the next track that can be shown, then off.
    Q_INVOKABLE void cycle();
    // Adds a subtitle file (.srt, .ass, .ssa, .vtt) as a track and turns it on.
    Q_INVOKABLE void loadFile(const QUrl &file);
    Q_INVOKABLE bool isSubtitleFile(const QUrl &file) const;
    // The playhead moved (playing or seeking).
    Q_INVOKABLE void setPosition(double seconds);

signals:
    void tracksChanged();
    void activeTrackChanged();
    void loadingChanged();
    void cueChanged();
    // For the status bar: a track that couldn't be read, or had no subtitles.
    void message(const QString &text);

private:
    struct Track
    {
        QString label;
        cv::SubtitleFormat format = cv::SubtitleFormat::Unsupported;
        int streamIndex = -1; // embedded: its 0:s:N number
        QString filePath;     // a subtitle file
        QString note;         // why it can't be shown
        std::optional<cv::SubtitleTrack> data;
    };

    void load(int index);
    void activate(int index);
    void showCue(int cue);
    void clearCue();

    StillFrameProvider *m_pictures;
    std::optional<cv::FfmpegPaths> m_paths;
    QString m_videoPath;
    QList<Track> m_tracks;
    int m_active = -1;
    int m_loadingTrack = -1;
    quint64 m_generation = 0; // bumped when the video changes, so late results are dropped
    cv::CancelToken m_loadCancel;
    double m_position = 0;
    int m_cue = -1;
    QString m_cueText;
    QUrl m_cueImage;
    QRectF m_cueRect;
    quint64 m_pictureSerial = 0;
};
