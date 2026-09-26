#include "EditorController.h"

#include "Background.h"
#include "StillFrameProvider.h"
#include "TimeFormat.h"
#include "media/FrameGrabber.h"
#include "media/MediaDetails.h"
#include "media/MediaProbe.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QLocale>
#include <QPointer>
#include <QSaveFile>
#include <QThreadPool>

#include <cmath>

namespace {

using cv::MediaDetails::formatBytes;

} // namespace

EditorController::EditorController(StillFrameProvider *stills, StillFrameProvider *thumbnails,
                                   StillFrameProvider *subtitlePictures, QObject *parent)
    : QObject(parent), m_stills(stills), m_paths(cv::FfmpegPaths::locate()),
      m_settings(cv::AppSettings::load(cv::AppSettings::defaultPath())), m_thumbnails(thumbnails),
      m_subtitles(subtitlePictures)
{
    connect(&m_subtitles, &SubtitleController::message, this, &EditorController::setStatus);
    m_clock.start();
    m_settingsSave.setSingleShot(true);
    m_settingsSave.setInterval(500);
    connect(&m_settingsSave, &QTimer::timeout, this, &EditorController::writeSettings);
    if (!m_paths)
        m_status = QStringLiteral("FFmpeg wasn't found. Install it (e.g. winget install Gyan.FFmpeg) or set %1.")
                       .arg(QLatin1String(cv::FfmpegPaths::DirectoryEnvVar));
}

EditorController::~EditorController()
{
    // Worker threads delete the partial export and stop decoding once they notice.
    m_exportCancel.cancel();
    m_stillCancel.cancel();
    m_saveFrameCancel.cancel();
    m_thumbnailCancel.cancel();
    if (m_settingsSave.isActive())
        writeSettings();
}

QUrl EditorController::source() const
{
    return m_info ? QUrl::fromLocalFile(m_info->path) : QUrl();
}

QString EditorController::fileName() const
{
    return m_info ? QFileInfo(m_info->path).fileName() : QString();
}

QString EditorController::infoText() const
{
    if (!m_info)
        return {};
    const cv::MediaInfo &i = *m_info;
    QStringList parts{QStringLiteral("%1×%2").arg(i.width).arg(i.height),
                      QStringLiteral("%1 fps").arg(QString::number(i.frameRate, 'g', 4)),
                      i.hasAudio() ? QStringLiteral("%1 + %2").arg(i.videoCodec, i.audioCodec) : i.videoCodec,
                      formatBytes(static_cast<double>(i.sizeBytes))};
    return parts.join(QStringLiteral("  ·  "));
}

QVariantList EditorController::mediaDetails() const
{
    QVariantList rows;
    if (m_info) {
        for (const auto &row : cv::MediaDetails::describe(*m_info))
            rows << QVariantMap{{QStringLiteral("label"), row.label}, {QStringLiteral("value"), row.value}};
    }
    return rows;
}

void EditorController::copyMediaDetails() const
{
    if (!m_info)
        return;
    QStringList lines{m_info->path};
    for (const auto &row : cv::MediaDetails::describe(*m_info))
        lines << row.label + QStringLiteral(": ") + row.value;
    QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

QString EditorController::clipSummary() const
{
    if (!m_info)
        return {};
    const double length = m_trimEnd - m_trimStart;
    const auto frames = std::llround(length / frameDuration());
    QString summary = QStringLiteral("%1  ·  %2 frames").arg(cv::TimeFormat::format(length)).arg(frames);
    if (m_info->duration > 0)
        summary += QStringLiteral("  ·  ≈ %1").arg(formatBytes(m_info->sizeBytes * length / m_info->duration));
    return summary;
}

void EditorController::setSmartCut(bool value)
{
    if (m_settings.smartCut == value)
        return;
    m_settings.smartCut = value;
    saveSettings();
    emit smartCutChanged();
}

void EditorController::setReencodeOption(const QString &key, const QVariant &value)
{
    QJsonObject json = m_settings.reencode.toJson();
    json.insert(key, QJsonValue::fromVariant(value));
    setReencode(cv::ReencodeOptions::fromJson(json));
}

void EditorController::resetReencode()
{
    setReencode({});
}

void EditorController::setReencode(const cv::ReencodeOptions &options)
{
    if (m_settings.reencode == options)
        return;
    m_settings.reencode = options;
    saveSettings();
    emit reencodeChanged();
}

void EditorController::setVolume(double value)
{
    value = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_settings.volume + 1, value + 1))
        return;
    m_settings.volume = value;
    saveSettings();
    emit volumeChanged();
}

void EditorController::setMuted(bool value)
{
    if (m_settings.muted == value)
        return;
    m_settings.muted = value;
    saveSettings();
    emit mutedChanged();
}

void EditorController::setTheme(const QString &name)
{
    if (m_settings.theme == name)
        return;
    m_settings.theme = name;
    saveSettings();
    emit themeChanged();
}

void EditorController::setAlwaysOnTop(bool value)
{
    if (m_settings.alwaysOnTop == value)
        return;
    m_settings.alwaysOnTop = value;
    saveSettings();
    emit alwaysOnTopChanged();
}

void EditorController::saveSettings()
{
    m_settingsSave.start(); // a volume drag changes it many times a second
}

void EditorController::writeSettings()
{
    // Best effort: an unwritable settings file must never get in the way of playing or editing.
    if (!m_settings.save(cv::AppSettings::defaultPath()))
        qWarning("Couldn't save settings to %s", qPrintable(cv::AppSettings::defaultPath()));
}

void EditorController::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

// ---- Opening ----

void EditorController::openFile(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    if (!m_paths) {
        setStatus(QStringLiteral("Can't open videos without FFmpeg."));
        return;
    }
    if (m_exporting) {
        setStatus(QStringLiteral("Wait for the export to finish before opening another video."));
        return;
    }

    const quint64 generation = ++m_openGeneration;
    m_loading = true;
    emit loadingChanged();
    setStatus(QStringLiteral("Opening %1…").arg(QFileInfo(path).fileName()));

    struct Result
    {
        std::optional<cv::MediaInfo> info;
        QString error;
    };
    runInBackground(
        this,
        [paths = *m_paths, path]() -> Result {
            try {
                return {cv::MediaProbe(paths).probe(path), {}};
            } catch (const std::exception &e) {
                return {std::nullopt, firstLine(e.what())};
            }
        },
        [this, generation, path](Result result) {
            if (generation != m_openGeneration)
                return; // a later open replaced this one
            m_loading = false;
            emit loadingChanged();
            if (!result.info) {
                setStatus(QStringLiteral("Can't open %1: %2").arg(QFileInfo(path).fileName(), result.error));
                return;
            }
            m_info = std::move(result.info);
            m_trimStart = 0;
            m_trimEnd = m_info->duration;
            clearStill();
            clearThumbnail();
            m_thumbnailCache.clear();
            m_subtitles.setMedia(m_paths, &*m_info);
            // Before mediaChanged, so a playback error for the new source isn't cleared
            setStatus({});
            emit mediaChanged();
            emit trimChanged();
        });
}

void EditorController::reportPlaybackError(const QString &message)
{
    qWarning("Playback error: %s", qPrintable(message));
    const QString name = fileName();
    QString reason = message.trimmed().section(QLatin1Char('\n'), 0, 0).trimmed();
    if (reason.isEmpty())
        reason = QStringLiteral("unknown error");
    setStatus(name.isEmpty() ? QStringLiteral("Playback error: %1").arg(reason)
                             : QStringLiteral("Can't play %1: %2").arg(name, reason));
}

QString EditorController::trackLabel(const QMediaMetaData &track, int index) const
{
    QStringList parts{QStringLiteral("Track %1").arg(index + 1)};
    const auto language = track.value(QMediaMetaData::Language).value<QLocale::Language>();
    if (language != QLocale::AnyLanguage && language != QLocale::C)
        parts << QLocale::languageToString(language);
    const QString title = track.stringValue(QMediaMetaData::Title).trimmed();
    if (!title.isEmpty() && !parts.contains(title))
        parts << title;
    return parts.join(QStringLiteral(" · "));
}

// ---- Trim range ----

double EditorController::snap(double seconds) const
{
    const double frame = frameDuration();
    return std::round(seconds / frame) * frame;
}

double EditorController::minClip() const
{
    return std::min(frameDuration(), duration());
}

void EditorController::setTrimRange(double start, double end)
{
    start = std::clamp(start, 0.0, duration());
    end = std::clamp(end, 0.0, duration());
    if (start == m_trimStart && end == m_trimEnd)
        return;
    m_trimStart = start;
    m_trimEnd = end;
    emit trimChanged();
}

void EditorController::setTrimStart(double seconds)
{
    if (hasMedia())
        setTrimRange(std::min(snap(seconds), m_trimEnd - minClip()), m_trimEnd);
}

void EditorController::setTrimEnd(double seconds)
{
    if (hasMedia())
        setTrimRange(m_trimStart, std::max(snap(seconds), m_trimStart + minClip()));
}

bool EditorController::setTrimStartText(const QString &text)
{
    const auto time = cv::TimeFormat::parse(text);
    if (time)
        setTrimStart(*time);
    return time.has_value();
}

bool EditorController::setTrimEndText(const QString &text)
{
    const auto time = cv::TimeFormat::parse(text);
    if (time)
        setTrimEnd(*time);
    return time.has_value();
}

void EditorController::setStartHere(double position)
{
    if (!hasMedia())
        return;
    position = snap(position);
    const double end = position > m_trimEnd - minClip() ? duration() : m_trimEnd;
    setTrimRange(std::min(position, duration() - minClip()), end);
}

void EditorController::setEndHere(double position)
{
    if (!hasMedia())
        return;
    position = snap(position);
    const double start = position < m_trimStart + minClip() ? 0 : m_trimStart;
    setTrimRange(start, std::max(position, minClip()));
}

QString EditorController::formatTime(double seconds) const
{
    return cv::TimeFormat::format(seconds);
}

QString EditorController::formatShortTime(double seconds) const
{
    return cv::TimeFormat::formatShort(seconds);
}

// ---- Still frame ----

void EditorController::requestStill(double seconds)
{
    if (!m_paths || !m_info)
        return;
    m_stillCancel.cancel(); // the latest request wins
    m_stillCancel = cv::CancelToken();
    const quint64 generation = ++m_stillGeneration;

    runInBackground(
        this,
        [paths = *m_paths, path = m_info->path, seconds, cancel = m_stillCancel]() -> QImage {
            try {
                return cv::FrameGrabber(paths).grab(path, seconds, 1920, cancel);
            } catch (const std::exception &) {
                return {}; // cancelled, or no frame there: keep showing the live video
            }
        },
        [this, generation](QImage frame) {
            if (generation != m_stillGeneration || frame.isNull())
                return;
            m_stills->setFrame(frame);
            m_stillSource = QUrl(QStringLiteral("image://still/%1").arg(generation));
            emit stillChanged();
        });
}

void EditorController::clearStill()
{
    m_stillCancel.cancel();
    ++m_stillGeneration;
    if (m_stillSource.isEmpty())
        return;
    m_stillSource.clear();
    emit stillChanged();
}

// ---- Saving a frame ----

QUrl EditorController::suggestedFrameUrl(double seconds) const
{
    if (!m_info)
        return {};
    const QFileInfo source(m_info->path);
    const QDir folder = !m_settings.lastFrameFolder.isEmpty() && QFileInfo(m_settings.lastFrameFolder).isDir()
                            ? QDir(m_settings.lastFrameFolder)
                            : source.dir();
    // "1:23.456" isn't a valid file name on Windows
    const QString time = cv::TimeFormat::format(snap(std::max(0.0, seconds))).replace(QLatin1Char(':'), QLatin1Char('-'));
    return QUrl::fromLocalFile(folder.filePath(source.completeBaseName() + QLatin1Char('_') + time + QStringLiteral(".png")));
}

void EditorController::saveFrame(double seconds, const QUrl &destination)
{
    if (!m_paths || !m_info)
        return;
    QString output = destination.isLocalFile() ? destination.toLocalFile() : destination.toString();
    if (QFileInfo(output).suffix().compare(QLatin1String("png"), Qt::CaseInsensitive) != 0)
        output += QStringLiteral(".png"); // so the source video can never be overwritten
    // The last frame starts a frame before the end.
    seconds = std::clamp(snap(seconds), 0.0, std::max(0.0, duration() - frameDuration()));
    setStatus(QStringLiteral("Saving frame…"));

    runInBackground(
        this,
        [paths = *m_paths, path = m_info->path, seconds, output, cancel = m_saveFrameCancel]() -> QString {
            try {
                const QImage frame = cv::FrameGrabber(paths).grab(path, seconds, 0, cancel);
                if (frame.isNull())
                    return QStringLiteral("ffmpeg couldn't decode a frame there");
                QSaveFile file(output);
                if (!file.open(QIODevice::WriteOnly) || !frame.save(&file, "PNG") || !file.commit())
                    return file.errorString().isEmpty() ? QStringLiteral("couldn't write the file") : file.errorString();
                return {};
            } catch (const std::exception &e) {
                return firstLine(e.what());
            }
        },
        [this, output](QString error) {
            const QString name = QFileInfo(output).fileName();
            if (!error.isEmpty()) {
                setStatus(QStringLiteral("Couldn't save %1: %2").arg(name, error));
                return;
            }
            m_settings.lastFrameFolder = QFileInfo(output).absolutePath();
            saveSettings();
            setStatus(QStringLiteral("Saved %1  ·  %2")
                          .arg(name, formatBytes(static_cast<double>(QFileInfo(output).size()))));
        });
}

// ---- Hover thumbnails ----

// Hover times are rounded to steps of about a pixel on a wide timeline, at least a frame and at
// most a second, so small mouse moves hit the cache.
double EditorController::thumbnailStep() const
{
    return std::clamp(duration() / 1000, frameDuration(), 1.0);
}

void EditorController::requestThumbnail(double seconds)
{
    if (!m_paths || !m_info || !std::isfinite(seconds))
        return;
    const qint64 key = std::llround(std::clamp(seconds, 0.0, duration()) / thumbnailStep());
    if (key == m_thumbnailWanted)
        return;
    m_thumbnailWanted = key;
    if (const QImage *cached = m_thumbnailCache.object(key))
        showThumbnail(*cached);
    else if (!m_thumbnailBusy)
        grabThumbnail(key); // otherwise it starts when the running grab finishes
}

void EditorController::grabThumbnail(qint64 key)
{
    m_thumbnailBusy = true;
    // The last step can land on the very end, where there's no frame to decode.
    const double seconds = std::max(0.0, std::min(key * thumbnailStep(), duration() - frameDuration()));
    const quint64 generation = m_thumbnailGeneration;

    runInBackground(
        this,
        [paths = *m_paths, path = m_info->path, seconds, cancel = m_thumbnailCancel]() -> QImage {
            try {
                return cv::FrameGrabber(paths).grab(path, seconds, 320, cancel);
            } catch (const std::exception &) {
                return {}; // cancelled, or no frame there
            }
        },
        [this, key, generation](QImage frame) {
            if (generation != m_thumbnailGeneration)
                return; // the hover ended or the video changed meanwhile
            m_thumbnailBusy = false;
            if (!frame.isNull()) {
                m_thumbnailCache.insert(key, new QImage(frame), frame.sizeInBytes());
                showThumbnail(frame); // even if the mouse has moved on: it's the closest frame so far
            }
            if (m_thumbnailWanted < 0 || m_thumbnailWanted == key)
                return;
            if (const QImage *cached = m_thumbnailCache.object(m_thumbnailWanted))
                showThumbnail(*cached);
            else
                grabThumbnail(m_thumbnailWanted);
        });
}

void EditorController::showThumbnail(const QImage &frame)
{
    m_thumbnails->setFrame(frame);
    m_thumbnailSource = QUrl(QStringLiteral("image://thumb/%1").arg(++m_thumbnailSerial));
    emit thumbnailChanged();
}

void EditorController::clearThumbnail()
{
    m_thumbnailCancel.cancel();
    m_thumbnailCancel = cv::CancelToken();
    ++m_thumbnailGeneration;
    m_thumbnailBusy = false;
    m_thumbnailWanted = -1;
    if (m_thumbnailSource.isEmpty())
        return;
    m_thumbnailSource.clear();
    emit thumbnailChanged();
}

// ---- Clicks ----

int EditorController::videoClick(double x)
{
    return static_cast<int>(m_clickGesture.click(x, m_clock.elapsed()));
}

// ---- Export ----

QUrl EditorController::suggestedExportUrl() const
{
    if (!m_info)
        return {};
    const QFileInfo source(m_info->path);
    const QDir folder = !m_settings.lastExportFolder.isEmpty() && QFileInfo(m_settings.lastExportFolder).isDir()
                            ? QDir(m_settings.lastExportFolder)
                            : source.dir();
    return QUrl::fromLocalFile(folder.filePath(
        source.completeBaseName() + QStringLiteral("_clip") + QLatin1String(cv::ClipExporter::OutputExtension)));
}

void EditorController::exportTo(const QUrl &destination)
{
    if (!m_paths || !m_info || m_exporting)
        return;

    QString output = destination.isLocalFile() ? destination.toLocalFile() : destination.toString();
    if (QFileInfo(output).suffix().isEmpty())
        output += QLatin1String(cv::ClipExporter::OutputExtension);

    cv::ExportRequest request{m_info->path, output, m_trimStart, m_trimEnd,
                              m_settings.smartCut ? cv::ExportMode::SmartCut : cv::ExportMode::Reencode,
                              m_settings.reencode};
    m_exportCancel = cv::CancelToken();
    m_exporting = true;
    m_exportProgress = 0;
    emit exportingChanged();
    emit exportProgressChanged();
    setStatus(QStringLiteral("Exporting…"));

    QPointer<EditorController> self(this);
    auto onProgress = [self](double fraction) {
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, fraction] {
                if (self && self->m_exporting && fraction > self->m_exportProgress) {
                    self->m_exportProgress = fraction;
                    emit self->exportProgressChanged();
                }
            },
            Qt::QueuedConnection);
    };

    struct Result
    {
        bool ok = false;
        bool cancelled = false;
        QString message; // fallback reason or error
        double seconds = 0;
    };
    runInBackground(
        this,
        [paths = *m_paths, request, onProgress, cancel = m_exportCancel]() -> Result {
            QElapsedTimer timer;
            timer.start();
            try {
                const cv::ExportResult result = cv::ClipExporter(paths).exportClip(request, onProgress, cancel);
                return {true, false, result.fallbackReason, timer.elapsed() / 1000.0};
            } catch (const cv::OperationCancelled &) {
                return {false, true, {}, 0};
            } catch (const std::exception &e) {
                return {false, false, firstLine(e.what()), 0};
            }
        },
        [this, output](Result result) {
            m_exporting = false;
            emit exportingChanged();
            const QString name = QFileInfo(output).fileName();
            if (result.cancelled) {
                setStatus(QStringLiteral("Export cancelled."));
            } else if (!result.ok) {
                setStatus(QStringLiteral("Export failed: %1").arg(result.message));
            } else {
                m_settings.lastExportFolder = QFileInfo(output).absolutePath();
                saveSettings();
                QString status = QStringLiteral("Saved %1  ·  %2  ·  %3 s")
                                     .arg(name, formatBytes(static_cast<double>(QFileInfo(output).size())))
                                     .arg(result.seconds, 0, 'f', 1);
                if (!result.message.isEmpty())
                    status += QStringLiteral("  ·  re-encoded (%1)").arg(result.message);
                setStatus(status);
            }
        });
}

void EditorController::cancelExport()
{
    if (m_exporting)
        m_exportCancel.cancel();
}
