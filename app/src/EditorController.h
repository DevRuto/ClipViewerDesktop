#pragma once

#include "AppSettings.h"
#include "PlayerClickGesture.h"
#include "SubtitleController.h"
#include "media/ClipExporter.h"
#include "media/FfmpegPaths.h"
#include "media/MediaInfo.h"
#include "media/Process.h"

#include <QCache>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaMetaData>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <optional>

class StillFrameProvider;

// The player and trim editor's state and commands. Playback itself lives in QML (MediaPlayer); this
// holds the probed file, the trim range (snapped to the frame grid), the ffmpeg still shown while
// paused, the export and the saved settings. All ffmpeg work runs on the thread pool; results are
// posted back to the UI thread.
class EditorController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created in main.cpp")

    Q_PROPERTY(bool ffmpegFound READ ffmpegFound CONSTANT)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY mediaChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QUrl source READ source NOTIFY mediaChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY mediaChanged)
    Q_PROPERTY(QString infoText READ infoText NOTIFY mediaChanged)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY mediaChanged)
    // The media info panel: [{ label, value }], see cv::MediaDetails
    Q_PROPERTY(QVariantList mediaDetails READ mediaDetails NOTIFY mediaChanged)
    // Width / height of the picture as shown (pixel aspect and rotation applied)
    Q_PROPERTY(double displayAspect READ displayAspect NOTIFY mediaChanged)
    Q_PROPERTY(double duration READ duration NOTIFY mediaChanged)
    Q_PROPERTY(double frameDuration READ frameDuration NOTIFY mediaChanged)
    Q_PROPERTY(double trimStart READ trimStart WRITE setTrimStart NOTIFY trimChanged)
    Q_PROPERTY(double trimEnd READ trimEnd WRITE setTrimEnd NOTIFY trimChanged)
    Q_PROPERTY(QString clipSummary READ clipSummary NOTIFY trimChanged)
    Q_PROPERTY(bool smartCut READ smartCut WRITE setSmartCut NOTIFY smartCutChanged)
    // The re-encode settings as { crf, preset, maxHeight, maxFrameRate, audioBitrate }; see cv::ReencodeOptions.
    Q_PROPERTY(QVariantMap reencode READ reencode NOTIFY reencodeChanged)
    Q_PROPERTY(bool reencodeIsDefault READ reencodeIsDefault NOTIFY reencodeChanged)
    // The subtitle settings as { size, background, position }; see cv::SubtitleStyle.
    Q_PROPERTY(QVariantMap subtitleStyle READ subtitleStyle NOTIFY subtitleStyleChanged)
    Q_PROPERTY(bool subtitleStyleIsDefault READ subtitleStyleIsDefault NOTIFY subtitleStyleChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(bool compact READ compact WRITE setCompact NOTIFY compactChanged)
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged)
    // Click to play/pause, double-click an edge to seek; off leaves clicks on the video alone.
    Q_PROPERTY(bool clickControls READ clickControls WRITE setClickControls NOTIFY clickControlsChanged)
    Q_PROPERTY(bool edgeDoubleClick READ edgeDoubleClick WRITE setEdgeDoubleClick NOTIFY edgeDoubleClickChanged)
    // Seek steps in seconds: Left/Right, and J/L plus the double-click on an edge.
    Q_PROPERTY(int shortSkip READ shortSkip WRITE setShortSkip NOTIFY shortSkipChanged)
    Q_PROPERTY(int longSkip READ longSkip WRITE setLongSkip NOTIFY longSkipChanged)
    Q_PROPERTY(bool autoplay READ autoplay WRITE setAutoplay NOTIFY autoplayChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportProgressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool crashNotice READ crashNotice NOTIFY crashNoticeChanged)
    Q_PROPERTY(QUrl stillSource READ stillSource NOTIFY stillChanged)
    Q_PROPERTY(QUrl thumbnailSource READ thumbnailSource NOTIFY thumbnailChanged)
    Q_PROPERTY(SubtitleController *subtitles READ subtitles CONSTANT)

public:
    EditorController(StillFrameProvider *stills, StillFrameProvider *thumbnails, StillFrameProvider *subtitlePictures,
                     QObject *parent = nullptr);
    ~EditorController() override;

    bool ffmpegFound() const { return m_paths.has_value(); }
    bool hasMedia() const { return m_info.has_value(); }
    bool loading() const { return m_loading; }
    QUrl source() const;
    QString fileName() const;
    QString infoText() const;
    bool hasAudio() const { return m_info && m_info->hasAudio(); }
    QVariantList mediaDetails() const;
    double displayAspect() const { return m_info ? m_info->displayAspect() : 16.0 / 9; }
    double duration() const { return m_info ? m_info->duration : 0; }
    double frameDuration() const { return m_info ? m_info->frameDuration() : 1 / 30.0; }
    double trimStart() const { return m_trimStart; }
    double trimEnd() const { return m_trimEnd; }
    void setTrimStart(double seconds);
    void setTrimEnd(double seconds);
    QString clipSummary() const;
    bool smartCut() const { return m_settings.smartCut; }
    void setSmartCut(bool value);
    QVariantMap reencode() const { return m_settings.reencode.toJson().toVariantMap(); }
    bool reencodeIsDefault() const { return m_settings.reencode == cv::ReencodeOptions{}; }
    QVariantMap subtitleStyle() const { return m_settings.subtitles.toJson().toVariantMap(); }
    bool subtitleStyleIsDefault() const { return m_settings.subtitles == cv::SubtitleStyle{}; }
    double volume() const { return m_settings.volume; }
    void setVolume(double value);
    bool muted() const { return m_settings.muted; }
    void setMuted(bool value);
    QString theme() const { return m_settings.theme; }
    void setTheme(const QString &name);
    QString style() const { return m_settings.style; }
    void setStyle(const QString &name);
    bool compact() const { return m_settings.compact; }
    void setCompact(bool value);
    bool alwaysOnTop() const { return m_settings.alwaysOnTop; }
    void setAlwaysOnTop(bool value);
    bool clickControls() const { return m_settings.clickControls; }
    void setClickControls(bool value);
    bool edgeDoubleClick() const { return m_settings.edgeDoubleClick; }
    void setEdgeDoubleClick(bool value);
    int shortSkip() const { return m_settings.shortSkip; }
    void setShortSkip(int seconds);
    int longSkip() const { return m_settings.longSkip; }
    void setLongSkip(int seconds);
    bool autoplay() const { return m_settings.autoplay; }
    void setAutoplay(bool value);
    bool exporting() const { return m_exporting; }
    double exportProgress() const { return m_exportProgress; }
    QString status() const { return m_status; }
    QUrl stillSource() const { return m_stillSource; }
    QUrl thumbnailSource() const { return m_thumbnailSource; }
    SubtitleController *subtitles() { return &m_subtitles; }

    // Opens a local video (path or file:// URL): probes it, then hands it to the player.
    Q_INVOKABLE void openFile(const QString &pathOrUrl);
    // Rounds to the nearest frame of the open video.
    Q_INVOKABLE double snap(double seconds) const;
    Q_INVOKABLE QString formatTime(double seconds) const;
    Q_INVOKABLE QString formatShortTime(double seconds) const;
    // Sets the start/end from typed text; returns false if the text isn't a time.
    Q_INVOKABLE bool setTrimStartText(const QString &text);
    Q_INVOKABLE bool setTrimEndText(const QString &text);
    // I / O: start or end here. Setting the start past the end moves the end to the video's end
    // (and vice versa), so the range never collapses.
    Q_INVOKABLE void setStartHere(double position);
    Q_INVOKABLE void setEndHere(double position);
    // Decodes the exact frame at `seconds` with ffmpeg and shows it as stillSource (the latest
    // request wins). clearStill hides it again, e.g. when playback starts.
    Q_INVOKABLE void requestStill(double seconds);
    Q_INVOKABLE void clearStill();
    // The timeline's hover preview: a small frame near `seconds` as thumbnailSource. One grab runs
    // at a time and only the latest request waits behind it, so the preview keeps up with the
    // mouse; frames are cached per video. clearThumbnail hides it when the mouse leaves.
    Q_INVOKABLE void requestThumbnail(double seconds);
    Q_INVOKABLE void clearThumbnail();
    // What a click at x (0-1 across the video) does: 0 toggle play, 1 undo toggle and seek back,
    // 2 undo toggle and seek forward. See PlayerClickGesture. Always 0 with edgeDoubleClick off.
    Q_INVOKABLE int videoClick(double x);
    // "<last export folder, or the source's>/<name>_clip.mp4".
    Q_INVOKABLE QUrl suggestedExportUrl() const;
    Q_INVOKABLE void exportTo(const QUrl &destination);
    // "<last frame folder, or the source's>/<name>_<time>.png".
    Q_INVOKABLE QUrl suggestedFrameUrl(double seconds) const;
    // Saves the full-size frame at `seconds` as a PNG and reports the result in the status bar.
    Q_INVOKABLE void saveFrame(double seconds, const QUrl &destination);
    Q_INVOKABLE void cancelExport();
    // Copies the media info panel's text.
    Q_INVOKABLE void copyMediaDetails() const;
    Q_INVOKABLE void clearStatus() { setStatus({}); }
    // "The app crashed last time", shown in the status bar until dismissed.
    bool crashNotice() const { return m_crashNotice; }
    void showCrashNotice();
    Q_INVOKABLE void dismissCrashNotice();
    // Opens the folder with the log files and crash dumps in the file manager.
    Q_INVOKABLE void openLogFolder() const;
    // MediaPlayer.onErrorOccurred: shows the player's message in the status bar.
    Q_INVOKABLE void reportPlaybackError(const QString &message);
    // Sets one re-encode setting; a value that isn't one of its choices falls back to the default.
    Q_INVOKABLE void setReencodeOption(const QString &key, const QVariant &value);
    Q_INVOKABLE void resetReencode();
    // Sets one subtitle setting; a value that isn't one of its choices falls back to the default.
    Q_INVOKABLE void setSubtitleStyleOption(const QString &key, const QVariant &value);
    Q_INVOKABLE void resetSubtitleStyle();
    // A MediaPlayer audio track as "Track 2 · English · Commentary".
    Q_INVOKABLE QString trackLabel(const QMediaMetaData &track, int index) const;

signals:
    void mediaChanged();
    void loadingChanged();
    void trimChanged();
    void smartCutChanged();
    void reencodeChanged();
    void subtitleStyleChanged();
    void volumeChanged();
    void mutedChanged();
    void themeChanged();
    void styleChanged();
    void compactChanged();
    void alwaysOnTopChanged();
    void clickControlsChanged();
    void edgeDoubleClickChanged();
    void shortSkipChanged();
    void longSkipChanged();
    void autoplayChanged();
    void exportingChanged();
    void exportProgressChanged();
    void statusChanged();
    void crashNoticeChanged();
    void stillChanged();
    void thumbnailChanged();

private:
    void setStatus(const QString &status);
    void setReencode(const cv::ReencodeOptions &options);
    void setSubtitleStyle(const cv::SubtitleStyle &style);
    void setTrimRange(double start, double end);
    double minClip() const;
    void saveSettings(); // debounced
    void writeSettings();
    double thumbnailStep() const;
    void grabThumbnail(qint64 key);
    void showThumbnail(const QImage &frame);

    StillFrameProvider *m_stills;
    std::optional<cv::FfmpegPaths> m_paths;
    std::optional<cv::MediaInfo> m_info;
    bool m_loading = false;
    quint64 m_openGeneration = 0;
    double m_trimStart = 0;
    double m_trimEnd = 0;
    cv::AppSettings m_settings;
    QTimer m_settingsSave;
    bool m_exporting = false;
    double m_exportProgress = 0;
    cv::CancelToken m_exportCancel;
    QString m_status;
    bool m_crashNotice = false;
    QUrl m_stillSource;
    quint64 m_stillGeneration = 0;
    cv::CancelToken m_stillCancel;
    cv::CancelToken m_saveFrameCancel; // only cancelled on exit
    StillFrameProvider *m_thumbnails;
    QUrl m_thumbnailSource;
    quint64 m_thumbnailSerial = 0;
    quint64 m_thumbnailGeneration = 0; // bumped when the hover ends or the video changes
    cv::CancelToken m_thumbnailCancel;
    bool m_thumbnailBusy = false;
    qint64 m_thumbnailWanted = -1; // the step under the mouse, or -1
    QCache<qint64, QImage> m_thumbnailCache{64 << 20}; // cost in bytes
    SubtitleController m_subtitles;
    cv::PlayerClickGesture m_clickGesture;
    QElapsedTimer m_clock;
};
