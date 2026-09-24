#include "ClipExporter.h"

#include "MediaProbe.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>
#include <exception>
#include <future>
#include <mutex>

namespace cv {

namespace {

// Smart-cut segments sit next to untouched source frames, so they get a high quality setting; they
// are only a GOP long, so the cost is small.
const QString SegmentPreset = QStringLiteral("fast");
constexpr int SegmentCrf = 15;

// How far past each cut point to look for keyframes. Longer GOPs still cut correctly, they just
// re-encode more.
constexpr double KeyframeSearchSeconds = 20;

const QStringList SmartCutPixelFormats{QStringLiteral("yuv420p"), QStringLiteral("yuvj420p")};

// Audio codecs that can be stream-copied into MP4; others are re-encoded to AAC.
const QStringList Mp4AudioCodecs{QStringLiteral("aac"),  QStringLiteral("mp3"),  QStringLiteral("ac3"),
                                 QStringLiteral("eac3"), QStringLiteral("opus"), QStringLiteral("flac"),
                                 QStringLiteral("alac")};

QStringList quietArgs()
{
    return {QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-loglevel"),
            QStringLiteral("error"), QStringLiteral("-y")};
}

void tryDelete(const QString &path)
{
    // ffmpeg may not have released the file yet; a leftover partial file is not fatal.
    QFile::remove(path);
}

} // namespace

const QStringList ClipExporter::UploadableExtensions{QStringLiteral(".mp4"), QStringLiteral(".webm"),
                                                     QStringLiteral(".mov"), QStringLiteral(".avi"),
                                                     QStringLiteral(".mkv")};

ExportResult ClipExporter::exportClip(const ExportRequest &request, const Progress &progress,
                                      const CancelToken &cancel) const
{
    validate(request);

    ExportResult result;
    try {
        bool fallback = false;
        if (request.mode == ExportMode::SmartCut) {
            const SourceInfo source = probeSource(request.inputPath, cancel);
            result.fallbackReason = smartCutUnsupportedReason(source);
            fallback = !result.fallbackReason.isEmpty();
            if (!fallback)
                smartCut(request, source, progress, cancel);
        }
        if (request.mode == ExportMode::Reencode || fallback)
            reencode(request, progress, cancel);
    } catch (...) {
        tryDelete(request.outputPath);
        throw;
    }

    if (progress)
        progress(1);
    return result;
}

void ClipExporter::validate(const ExportRequest &request)
{
    if (request.start < 0)
        throw std::invalid_argument("Start time cannot be negative");
    if (request.end <= request.start)
        throw std::invalid_argument("End time must be after the start time");
    if (!QFileInfo(request.inputPath).isFile())
        throw std::invalid_argument(QStringLiteral("Video file not found: %1").arg(request.inputPath).toStdString());
    if (QFileInfo(request.inputPath).absoluteFilePath().compare(QFileInfo(request.outputPath).absoluteFilePath(),
                                                                Qt::CaseInsensitive) == 0)
        throw std::invalid_argument("Output file must be different from the source file");
}

// ---- Re-encode ----

void ClipExporter::reencode(const ExportRequest &request, const Progress &progress, const CancelToken &cancel) const
{
    const double total = request.length();
    runTool(m_paths.ffmpeg, buildArguments(request), cancel, [&](const QString &line) {
        const double seconds = parseProgressSeconds(line);
        if (seconds >= 0 && progress)
            progress(std::clamp(seconds / total, 0.0, 1.0));
    });
}

QStringList ClipExporter::buildArguments(const ExportRequest &request)
{
    QStringList args = quietArgs();
    args.insert(2, QStringLiteral("-nostats"));
    args << QStringLiteral("-ss") << formatSeconds(request.start) // input seek: fast, and exact when re-encoding
         << QStringLiteral("-i") << request.inputPath
         << QStringLiteral("-t") << formatSeconds(request.length())
         // First video stream and first audio stream if there is one; drop subtitles/data.
         << QStringLiteral("-map") << QStringLiteral("0:v:0") << QStringLiteral("-map") << QStringLiteral("0:a:0?")
         << QStringLiteral("-c:v") << QStringLiteral("libx264") << QStringLiteral("-preset") << request.preset
         << QStringLiteral("-crf") << QString::number(request.crf)
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
         << QStringLiteral("-c:a") << QStringLiteral("aac") << QStringLiteral("-b:a") << QStringLiteral("192k")
         << QStringLiteral("-movflags") << QStringLiteral("+faststart")
         << QStringLiteral("-progress") << QStringLiteral("pipe:1") << request.outputPath;
    return args;
}

double ClipExporter::parseProgressSeconds(const QString &line)
{
    static const QString key = QStringLiteral("out_time_us=");
    if (!line.startsWith(key))
        return -1;
    bool ok = false;
    const qint64 us = line.mid(key.size()).toLongLong(&ok);
    return ok ? us / 1'000'000.0 : -1; // "N/A" before the first frame is written
}

// ---- Smart cut ----

QString ClipExporter::smartCutUnsupportedReason(const SourceInfo &source)
{
    if (source.videoCodec != QLatin1String("h264"))
        return QStringLiteral("smart cut needs H.264 video, this is %1").arg(source.videoCodec);
    if (!SmartCutPixelFormats.contains(source.pixelFormat))
        return QStringLiteral("smart cut needs 8-bit 4:2:0 video, this is %1").arg(source.pixelFormat);
    return {};
}

void ClipExporter::smartCut(const ExportRequest &request, const SourceInfo &source, const Progress &progress,
                            const CancelToken &cancel) const
{
    const double start = request.start;
    const double end = request.end;
    const double tolerance = 0.5 / (source.frameRate > 0 ? source.frameRate : 60);
    const QList<double> keyframes = probeKeyframes(request.inputPath, source.startTime, start, end, cancel);
    const QList<SmartCutSegment> segments = SmartCutPlan::create(keyframes, start, end, tolerance);

    // Work next to the output: same drive, so there's room for the copied part and no cross-drive copy.
    const QFileInfo output(request.outputPath);
    QDir work(output.absolutePath());
    const QString workName = QStringLiteral(".%1.parts-%2")
                                 .arg(output.completeBaseName(), QUuid::createUuid().toString(QUuid::Id128));
    if (!work.mkdir(workName) || !work.cd(workName))
        throw MediaError(QStringLiteral("Couldn't create a work folder in %1").arg(output.absolutePath()));

    struct Cleanup
    {
        QDir dir;
        ~Cleanup() { dir.removeRecursively(); } // a killed ffmpeg may leave a file behind; not fatal
    } cleanup{work};

    // Rough relative cost per second, so progress moves in proportion to the time taken.
    auto cost = [](const SmartCutSegment &s) { return s.length() * (s.copy ? 1 : 30); };
    double total = end - start;
    for (const auto &segment : segments)
        total += cost(segment);
    double done = 0;
    std::mutex gate;
    auto completed = [&](double amount) {
        double fraction;
        {
            std::lock_guard lock(gate);
            fraction = (done += amount) / total;
        }
        if (progress)
            progress(fraction * 0.95); // the final mux is the rest
    };

    // One linked token for all jobs: the first failure cancels the others, not the caller.
    const CancelToken jobs = cancel.linked();
    std::vector<std::future<void>> running;
    auto startJob = [&](QStringList args, std::function<void()> onDone) {
        running.push_back(std::async(std::launch::async, [this, args = std::move(args), onDone, jobs] {
            try {
                runTool(m_paths.ffmpeg, args, jobs);
                onDone();
            } catch (...) {
                jobs.cancel();
                throw;
            }
        }));
    };

    QStringList names;
    for (qsizetype i = 0; i < segments.size(); ++i) {
        const SmartCutSegment segment = segments[i];
        const QString name = QStringLiteral("%1.ts").arg(i);
        names << name;
        QStringList args = segment.copy ? copySegmentArguments(request.inputPath, segment, tolerance)
                                        : encodeSegmentArguments(request.inputPath, source.pixelFormat, segment, tolerance);
        args << work.filePath(name);
        startJob(args, [&completed, segment, cost] { completed(cost(segment)); });
    }

    const bool copyAudio = Mp4AudioCodecs.contains(source.audioCodec);
    const QString audioPath = work.filePath(QStringLiteral("audio.mka"));
    if (copyAudio) {
        // An input seek snaps to the video keyframe before the start, so seek a bit early and let an
        // output seek drop the audio packets before the trim point.
        const double preroll = std::min(5.0, start);
        startJob(quietArgs() << QStringLiteral("-ss") << formatSeconds(start - preroll) << QStringLiteral("-i")
                             << request.inputPath << QStringLiteral("-ss") << formatSeconds(preroll)
                             << QStringLiteral("-t") << formatSeconds(end - start) << QStringLiteral("-map")
                             << QStringLiteral("0:a:0") << QStringLiteral("-vn") << QStringLiteral("-sn")
                             << QStringLiteral("-dn") << QStringLiteral("-c:a") << QStringLiteral("copy") << audioPath,
                 [] {});
    }

    // Wait for every job, then report the one that failed rather than the ones cancelled because of it.
    std::exception_ptr failure, cancelled;
    for (auto &job : running) {
        try {
            job.get();
        } catch (const OperationCancelled &) {
            if (!cancelled)
                cancelled = std::current_exception();
        } catch (...) {
            if (!failure)
                failure = std::current_exception();
        }
    }
    if (failure)
        std::rethrow_exception(failure);
    if (cancelled)
        std::rethrow_exception(cancelled);

    QFile list(work.filePath(QStringLiteral("list.txt")));
    if (!list.open(QIODevice::WriteOnly | QIODevice::Text))
        throw MediaError(QStringLiteral("Couldn't write the part list"));
    for (const QString &name : std::as_const(names))
        list.write(QStringLiteral("file '%1'\n").arg(name).toUtf8());
    list.close();

    QStringList mux = quietArgs();
    mux << QStringLiteral("-f") << QStringLiteral("concat") << QStringLiteral("-safe") << QStringLiteral("0")
        << QStringLiteral("-i") << list.fileName();
    if (copyAudio) {
        // The copied audio starts at the first whole packet after the trim point, plus codec
        // priming. The muxer would shift that to zero, so put the offset back.
        const double offset = probeAudioStartTime(audioPath, cancel);
        mux << QStringLiteral("-itsoffset") << formatSeconds(offset) << QStringLiteral("-i") << audioPath
            << QStringLiteral("-map") << QStringLiteral("0:v") << QStringLiteral("-map") << QStringLiteral("1:a")
            << QStringLiteral("-c") << QStringLiteral("copy");
    } else if (!source.audioCodec.isEmpty()) {
        mux << QStringLiteral("-ss") << formatSeconds(start) << QStringLiteral("-i") << request.inputPath
            << QStringLiteral("-t") << formatSeconds(end - start) << QStringLiteral("-map") << QStringLiteral("0:v")
            << QStringLiteral("-map") << QStringLiteral("1:a:0") << QStringLiteral("-c:v") << QStringLiteral("copy")
            << QStringLiteral("-c:a") << QStringLiteral("aac") << QStringLiteral("-b:a") << QStringLiteral("192k");
    } else {
        mux << QStringLiteral("-map") << QStringLiteral("0:v") << QStringLiteral("-c") << QStringLiteral("copy");
    }
    mux << QStringLiteral("-movflags") << QStringLiteral("+faststart") << request.outputPath;
    runTool(m_paths.ffmpeg, mux, cancel);
}

QStringList ClipExporter::copySegmentArguments(const QString &input, const SmartCutSegment &segment, double tolerance)
{
    // Timestamps after the -ss input seek are relative to it. The seek itself can land a GOP early
    // (ffmpeg backs off a little on B-frame streams) and -t cuts by decode time, so both ends are
    // cut by presentation time instead: the noise filter drops everything outside [0, length).
    const QString from = formatSeconds(-tolerance);
    const QString to = formatSeconds(segment.length() - tolerance);
    return quietArgs() << QStringLiteral("-ss") << formatSeconds(segment.start) << QStringLiteral("-i") << input
                       << QStringLiteral("-t") << formatSeconds(segment.length() + 1) // stop reading soon after the end
                       << QStringLiteral("-map") << QStringLiteral("0:v:0") << QStringLiteral("-an")
                       << QStringLiteral("-sn") << QStringLiteral("-dn") << QStringLiteral("-c:v")
                       << QStringLiteral("copy") << QStringLiteral("-bsf:v")
                       << QStringLiteral("noise=drop=lt(pts*tb\\,%1)+gte(pts*tb\\,%2)").arg(from, to)
                       << QStringLiteral("-f") << QStringLiteral("mpegts");
}

QStringList ClipExporter::encodeSegmentArguments(const QString &input, const QString &pixelFormat,
                                                 const SmartCutSegment &segment, double tolerance)
{
    // The start is either the trim point (the same -ss as the preview still) or a keyframe's exact
    // probed time. Keep it exact: an off-grid seek skews the timestamps x264 sees. The end frame
    // (the copied part's first frame, or the one at the trim point) is left out, so stop half a
    // frame early rather than rely on an exact float comparison.
    return quietArgs() << QStringLiteral("-ss") << formatSeconds(segment.start) << QStringLiteral("-i") << input
                       << QStringLiteral("-t") << formatSeconds(segment.length() - tolerance)
                       << QStringLiteral("-map") << QStringLiteral("0:v:0") << QStringLiteral("-an")
                       << QStringLiteral("-sn") << QStringLiteral("-dn") << QStringLiteral("-c:v")
                       << QStringLiteral("libx264") << QStringLiteral("-preset") << SegmentPreset
                       << QStringLiteral("-crf") << QString::number(SegmentCrf) << QStringLiteral("-pix_fmt")
                       << pixelFormat << QStringLiteral("-fps_mode") << QStringLiteral("passthrough")
                       << QStringLiteral("-f") << QStringLiteral("mpegts");
}

ClipExporter::SourceInfo ClipExporter::probeSource(const QString &input, const CancelToken &cancel) const
{
    const QByteArray json = runTool(
        m_paths.ffprobe,
        {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-of"), QStringLiteral("json"),
         QStringLiteral("-show_entries"),
         QStringLiteral("stream=codec_type,codec_name,pix_fmt,avg_frame_rate,r_frame_rate:format=start_time"), input},
        cancel);

    const QJsonObject root = QJsonDocument::fromJson(json).object();
    QJsonObject video, audio;
    for (const QJsonValue &value : root.value(QLatin1String("streams")).toArray()) {
        const QJsonObject stream = value.toObject();
        const QString type = stream.value(QLatin1String("codec_type")).toString();
        if (type == QLatin1String("video") && video.isEmpty())
            video = stream;
        else if (type == QLatin1String("audio") && audio.isEmpty())
            audio = stream;
    }
    if (video.isEmpty())
        throw MediaError(QStringLiteral("File has no video stream"));

    SourceInfo info;
    info.videoCodec = MediaProbe::field(video, QStringLiteral("codec_name"));
    info.pixelFormat = MediaProbe::field(video, QStringLiteral("pix_fmt"));
    info.frameRate = MediaProbe::parseRate(MediaProbe::field(video, QStringLiteral("avg_frame_rate")))
                         .value_or(MediaProbe::parseRate(MediaProbe::field(video, QStringLiteral("r_frame_rate")))
                                       .value_or(0));
    info.startTime =
        MediaProbe::field(root.value(QLatin1String("format")).toObject(), QStringLiteral("start_time")).toDouble();
    info.audioCodec = MediaProbe::field(audio, QStringLiteral("codec_name"));
    return info;
}

QList<double> ClipExporter::probeKeyframes(const QString &input, double fileStart, double start, double end,
                                           const CancelToken &cancel) const
{
    // Reads packet flags (no decoding) in a window at each end. read_intervals takes absolute
    // timestamps; the result is relative to the file's start, like -ss.
    const QString intervals = formatSeconds(fileStart + start) + QChar('%')
                              + formatSeconds(fileStart + std::min(end, start + KeyframeSearchSeconds)) + QChar(',')
                              + formatSeconds(fileStart + std::max(start, end - KeyframeSearchSeconds)) + QChar('%')
                              + formatSeconds(fileStart + end + 0.1);
    const QByteArray csv = runTool(
        m_paths.ffprobe,
        {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-select_streams"), QStringLiteral("v:0"),
         QStringLiteral("-show_entries"), QStringLiteral("packet=pts_time,flags"), QStringLiteral("-read_intervals"),
         intervals, QStringLiteral("-of"), QStringLiteral("csv=p=0"), input},
        cancel);
    return parseKeyframes(QString::fromUtf8(csv), fileStart);
}

QList<double> ClipExporter::parseKeyframes(const QString &csv, double fileStart)
{
    QList<double> keyframes;
    for (const QString &rawLine : csv.split(QChar('\n'), Qt::SkipEmptyParts)) {
        const QStringList fields = rawLine.trimmed().split(QChar(','));
        if (fields.size() < 2 || !fields[1].startsWith(QChar('K')))
            continue;
        bool ok = false;
        const double time = fields[0].toDouble(&ok);
        if (ok)
            keyframes << time - fileStart;
    }
    std::sort(keyframes.begin(), keyframes.end());
    keyframes.erase(std::unique(keyframes.begin(), keyframes.end()), keyframes.end());
    return keyframes;
}

double ClipExporter::probeAudioStartTime(const QString &input, const CancelToken &cancel) const
{
    const QByteArray out = runTool(m_paths.ffprobe,
                                   {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-select_streams"),
                                    QStringLiteral("a:0"), QStringLiteral("-show_entries"),
                                    QStringLiteral("stream=start_time"), QStringLiteral("-of"),
                                    QStringLiteral("csv=p=0"), input},
                                   cancel);
    return QString::fromUtf8(out).trimmed().toDouble(); // 0 when missing
}

QString ClipExporter::formatSeconds(double seconds)
{
    QString text = QString::number(seconds, 'f', 6);
    // Trim trailing zeros, keeping at least three decimals (like .NET's "0.000###").
    const qsizetype minLength = text.indexOf(QChar('.')) + 4;
    while (text.size() > minLength && text.endsWith(QChar('0')))
        text.chop(1);
    return text;
}

} // namespace cv
