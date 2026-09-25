#pragma once

#include "AppSettings.h"
#include "PlayerClickGesture.h"
#include "media/ClipExporter.h"
#include "media/FfmpegPaths.h"
#include "media/MediaInfo.h"
#include "media/Process.h"

#include <QCache>
#include <QElapsedTimer>
#include <QImage>
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
    Q_PROPERTY(double duration READ duration NOTIFY mediaChanged)
    Q_PROPERTY(double frameDuration READ frameDuration NOTIFY mediaChanged)
    Q_PROPERTY(double trimStart READ trimStart WRITE setTrimStart NOTIFY trimChanged)
    Q_PROPERTY(double trimEnd READ trimEnd WRITE setTrimEnd NOTIFY trimChanged)
    Q_PROPERTY(QString clipSummary READ clipSummary NOTIFY trimChanged)
    Q_PROPERTY(bool smartCut READ smartCut WRITE setSmartCut NOTIFY smartCutChanged)
    // The re-encode settings as { crf, preset, maxHeight, maxFrameRate, audioBitrate }; see cv::ReencodeOptions.
    Q_PROPERTY(QVariantMap reencode READ reencode NOTIFY reencodeChanged)
    Q_PROPERTY(bool reencodeIsDefault READ reencodeIsDefault NOTIFY reencodeChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportProgressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QUrl stillSource READ stillSource NOTIFY stillChanged)
    Q_PROPERTY(QUrl thumbnailSource READ thumbnailSource NOTIFY thumbnailChanged)

public:
    EditorController(StillFrameProvider *stills, StillFrameProvider *thumbnails, QObject *parent = nullptr);
    ~EditorController() override;

    bool ffmpegFound() const { return m_paths.has_value(); }
    bool hasMedia() const { return m_info.has_value(); }
    bool loading() const { return m_loading; }
    QUrl source() const;
    QString fileName() const;
    QString infoText() const;
    bool hasAudio() const { return m_info && m_info->hasAudio(); }
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
    double volume() const { return m_settings.volume; }
    void setVolume(double value);
    bool muted() const { return m_settings.muted; }
    void setMuted(bool value);
    QString theme() const { return m_settings.theme; }
    void setTheme(const QString &name);
    bool exporting() const { return m_exporting; }
    double exportProgress() const { return m_exportProgress; }
    QString status() const { return m_status; }
    QUrl stillSource() const { return m_stillSource; }
    QUrl thumbnailSource() const { return m_thumbnailSource; }

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
    // 2 undo toggle and seek forward. See PlayerClickGesture.
    Q_INVOKABLE int videoClick(double x);
    // "<last export folder, or the source's>/<name>_clip.mp4".
    Q_INVOKABLE QUrl suggestedExportUrl() const;
    Q_INVOKABLE void exportTo(const QUrl &destination);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void clearStatus() { setStatus({}); }
    // Sets one re-encode setting; a value that isn't one of its choices falls back to the default.
    Q_INVOKABLE void setReencodeOption(const QString &key, const QVariant &value);
    Q_INVOKABLE void resetReencode();

signals:
    void mediaChanged();
    void loadingChanged();
    void trimChanged();
    void smartCutChanged();
    void reencodeChanged();
    void volumeChanged();
    void mutedChanged();
    void themeChanged();
    void exportingChanged();
    void exportProgressChanged();
    void statusChanged();
    void stillChanged();
    void thumbnailChanged();

private:
    void setStatus(const QString &status);
    void setReencode(const cv::ReencodeOptions &options);
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
    QUrl m_stillSource;
    quint64 m_stillGeneration = 0;
    cv::CancelToken m_stillCancel;
    StillFrameProvider *m_thumbnails;
    QUrl m_thumbnailSource;
    quint64 m_thumbnailSerial = 0;
    quint64 m_thumbnailGeneration = 0; // bumped when the hover ends or the video changes
    cv::CancelToken m_thumbnailCancel;
    bool m_thumbnailBusy = false;
    qint64 m_thumbnailWanted = -1; // the step under the mouse, or -1
    QCache<qint64, QImage> m_thumbnailCache{64 << 20}; // cost in bytes
    cv::PlayerClickGesture m_clickGesture;
    QElapsedTimer m_clock;
};
