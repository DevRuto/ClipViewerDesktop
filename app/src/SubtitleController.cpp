#include "SubtitleController.h"

#include "Background.h"
#include "StillFrameProvider.h"
#include "media/SubtitleExtractor.h"
#include "subtitles/DvdSubtitle.h"

#include <QFileInfo>
#include <QLocale>

#include <algorithm>
#include <cmath>

namespace {

// "eng" -> "English"; an unknown code is shown as it is.
QString languageName(const QString &code)
{
    if (code.isEmpty())
        return {};
    const QLocale::Language language = QLocale::codeToLanguage(code);
    return language == QLocale::AnyLanguage || language == QLocale::C ? code : QLocale::languageToString(language);
}

QString unsupportedNote(const QString &codec)
{
    if (codec == QLatin1String("hdmv_pgs_subtitle"))
        return QStringLiteral("Blu-ray picture subtitles aren't supported yet");
    return QStringLiteral("%1 subtitles aren't supported").arg(codec.isEmpty() ? QStringLiteral("These") : codec);
}

} // namespace

SubtitleController::SubtitleController(StillFrameProvider *pictures, QObject *parent)
    : QObject(parent), m_pictures(pictures)
{
}

SubtitleController::~SubtitleController()
{
    m_loadCancel.cancel(); // the worker stops ffmpeg/ffprobe once it notices
}

QVariantList SubtitleController::tracks() const
{
    QVariantList list;
    for (const Track &track : m_tracks) {
        list << QVariantMap{{QStringLiteral("label"), track.label},
                            {QStringLiteral("available"), track.format != cv::SubtitleFormat::Unsupported},
                            {QStringLiteral("note"), track.note}};
    }
    return list;
}

void SubtitleController::setMedia(const std::optional<cv::FfmpegPaths> &paths, const cv::MediaInfo *info)
{
    m_loadCancel.cancel();
    m_loadCancel = cv::CancelToken();
    ++m_generation;
    m_paths = paths;
    m_videoPath = info ? info->path : QString();
    m_tracks.clear();
    const bool wasLoading = m_loadingTrack >= 0;
    m_loadingTrack = -1;
    m_active = -1;
    m_position = 0;
    setDelay(0);
    clearCue();

    int firstFile = -1;
    if (info) {
        const QList<cv::StreamInfo> streams = info->subtitleStreams();
        for (int i = 0; i < streams.size(); ++i) {
            const cv::StreamInfo &stream = streams[i];
            QStringList parts{QStringLiteral("Track %1").arg(i + 1)};
            if (!stream.language.isEmpty())
                parts << languageName(stream.language);
            if (!stream.title.isEmpty() && !parts.contains(stream.title))
                parts << stream.title;
            Track track;
            track.label = parts.join(QStringLiteral(" · "));
            track.format = cv::subtitleFormat(stream.codec);
            track.streamIndex = i;
            if (track.format == cv::SubtitleFormat::Unsupported)
                track.note = unsupportedNote(stream.codec);
            m_tracks << track;
        }
        for (const QString &file : cv::SubtitleExtractor::sidecarFiles(info->path)) {
            if (firstFile < 0)
                firstFile = static_cast<int>(m_tracks.size());
            Track track;
            track.label = QStringLiteral("File · %1").arg(QFileInfo(file).fileName());
            track.format = cv::SubtitleFormat::Text;
            track.filePath = file;
            m_tracks << track;
        }
    }
    emit tracksChanged();
    emit activeTrackChanged();
    if (wasLoading)
        emit loadingChanged();
    // A subtitle file named after the video is meant to go with it
    if (firstFile >= 0)
        setActiveTrack(firstFile);
}

void SubtitleController::setActiveTrack(int index)
{
    if (index < 0 || index >= m_tracks.size()) {
        if (m_loadingTrack >= 0) {
            m_loadCancel.cancel();
            m_loadingTrack = -1;
            emit loadingChanged();
        }
        activate(-1);
        return;
    }
    if (index == m_active || index == m_loadingTrack || m_tracks[index].format == cv::SubtitleFormat::Unsupported)
        return;
    if (m_tracks[index].data)
        activate(index);
    else
        load(index);
}

void SubtitleController::cycle()
{
    for (int next = m_active + 1; next < m_tracks.size(); ++next) {
        if (m_tracks[next].format != cv::SubtitleFormat::Unsupported) {
            setActiveTrack(next);
            return;
        }
    }
    setActiveTrack(-1);
}

void SubtitleController::loadFile(const QUrl &file)
{
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (m_videoPath.isEmpty()) {
        emit message(QStringLiteral("Open a video before adding subtitles."));
        return;
    }
    if (!cv::SubtitleExtractor::isSubtitleFile(path)) {
        emit message(QStringLiteral("%1 isn't a subtitle file (.srt, .ass, .ssa or .vtt).").arg(QFileInfo(path).fileName()));
        return;
    }
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (!m_tracks[i].filePath.isEmpty() && QFileInfo(m_tracks[i].filePath) == QFileInfo(path)) {
            setActiveTrack(i);
            return;
        }
    }
    Track track;
    track.label = QStringLiteral("File · %1").arg(QFileInfo(path).fileName());
    track.format = cv::SubtitleFormat::Text;
    track.filePath = path;
    m_tracks << track;
    emit tracksChanged();
    setActiveTrack(static_cast<int>(m_tracks.size() - 1));
}

bool SubtitleController::isSubtitleFile(const QUrl &file) const
{
    return cv::SubtitleExtractor::isSubtitleFile(file.isLocalFile() ? file.toLocalFile() : file.toString());
}

void SubtitleController::load(int index)
{
    if (m_loadingTrack >= 0)
        m_loadCancel.cancel(); // only the latest pick is read
    m_loadCancel = cv::CancelToken();
    m_loadingTrack = index;
    emit loadingChanged();

    const Track &track = m_tracks[index];
    struct Result
    {
        std::optional<cv::SubtitleTrack> track;
        QString error;
        bool cancelled = false;
    };
    const bool needsFfmpeg = track.filePath.isEmpty()
                             || QFileInfo(track.filePath).suffix().compare(QLatin1String("srt"), Qt::CaseInsensitive) != 0;
    if (needsFfmpeg && !m_paths) {
        m_loadingTrack = -1;
        emit loadingChanged();
        emit message(QStringLiteral("Can't read these subtitles without FFmpeg."));
        return;
    }
    runInBackground(
        this,
        [paths = m_paths.value_or(cv::FfmpegPaths{}), video = m_videoPath, stream = track.streamIndex,
         file = track.filePath, format = track.format, cancel = m_loadCancel]() -> Result {
            try {
                const cv::SubtitleExtractor extractor(paths);
                return {file.isEmpty() ? extractor.extract(video, stream, format, cancel)
                                       : extractor.loadFile(file, cancel),
                        {}, false};
            } catch (const cv::OperationCancelled &) {
                return {std::nullopt, {}, true};
            } catch (const std::exception &e) {
                return {std::nullopt, firstLine(e.what()), false};
            }
        },
        [this, index, generation = m_generation](Result result) {
            if (generation != m_generation || index != m_loadingTrack)
                return; // another video, or another track was picked meanwhile
            m_loadingTrack = -1;
            emit loadingChanged();
            if (result.cancelled)
                return;
            Track &track = m_tracks[index];
            if (!result.track) {
                emit message(QStringLiteral("Can't read subtitles %1: %2").arg(track.label, result.error));
                return;
            }
            track.data = std::move(result.track);
            if (track.data->isEmpty()) {
                emit message(QStringLiteral("No subtitles found in %1.").arg(track.label));
                return;
            }
            activate(index);
        });
}

void SubtitleController::activate(int index)
{
    if (m_active != index) {
        m_active = index;
        emit activeTrackChanged();
    }
    clearCue();
    setPosition(m_position);
}

void SubtitleController::setPosition(double seconds)
{
    m_position = seconds;
    const cv::SubtitleTrack *track = m_active >= 0 && m_tracks[m_active].data ? &*m_tracks[m_active].data : nullptr;
    const int cue = track ? track->cueAt(seconds - m_delay) : -1;
    if (cue == m_cue)
        return;
    if (cue < 0)
        clearCue();
    else
        showCue(cue);
}

void SubtitleController::setDelay(double seconds)
{
    if (!std::isfinite(seconds))
        return;
    seconds = std::clamp(std::round(seconds * 10) / 10, -60.0, 60.0);
    if (seconds == m_delay)
        return;
    m_delay = seconds;
    emit delayChanged();
    setPosition(m_position);
}

void SubtitleController::showCue(int index)
{
    const cv::SubtitleTrack &track = *m_tracks[m_active].data;
    const cv::SubtitleCue &cue = track.cues[index];
    m_cue = index;
    m_cueText.clear();
    m_cueImage.clear();
    m_cueRect = {};
    if (!cue.isPicture()) {
        m_cueText = cue.text;
    } else if (const auto picture = cv::DvdSubtitle::render(cue.picture, track.palette);
               picture && !track.canvas.isEmpty()) {
        m_pictures->setFrame(picture->image);
        m_cueImage = QUrl(QStringLiteral("image://subtitle/%1").arg(++m_pictureSerial));
        const QSizeF canvas(track.canvas);
        m_cueRect = QRectF(picture->position.x() / canvas.width(), picture->position.y() / canvas.height(),
                           picture->position.width() / canvas.width(), picture->position.height() / canvas.height());
    }
    emit cueChanged();
}

void SubtitleController::clearCue()
{
    const bool had = m_cue >= 0 || !m_cueText.isEmpty() || !m_cueImage.isEmpty();
    m_cue = -1;
    m_cueText.clear();
    m_cueImage.clear();
    m_cueRect = {};
    if (had)
        emit cueChanged();
}
