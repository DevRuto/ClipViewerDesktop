#pragma once

#include "FfmpegPaths.h"
#include "Process.h"
#include "SmartCutPlan.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace cv {

enum class ExportMode {
    // Frame-accurate and near-instant: re-encodes only the partial GOPs at each end of the range
    // and stream-copies the rest, so quality and size match the source. Needs 8-bit 4:2:0 H.264;
    // anything else falls back to Reencode.
    SmartCut,
    // Re-encode everything to H.264/AAC: slower, but usually a much smaller file.
    Reencode,
};

// A trimmed range of inputPath to write to outputPath. Times in seconds.
struct ExportRequest
{
    QString inputPath;
    QString outputPath;
    double start = 0;
    double end = 0;
    ExportMode mode = ExportMode::SmartCut;
    // x264 CRF for Reencode; lower is higher quality.
    int crf = 18;
    // x264 preset for Reencode. On a 5-minute 720p60 clip, veryfast at CRF 18 matched the quality
    // of fast at CRF 20 in about half the time.
    QString preset = QStringLiteral("veryfast");

    double length() const { return end - start; }
};

struct ExportResult
{
    // Why a smart cut re-encoded everything instead; empty if it didn't.
    QString fallbackReason;
};

// Cuts a time range out of a video with ffmpeg. See "Smart cut gotchas" in CLAUDE.md before
// changing any of the arguments.
class ClipExporter
{
public:
    using Progress = std::function<void(double)>;

    // Both modes write H.264 in MP4.
    static constexpr auto OutputExtension = ".mp4";

    explicit ClipExporter(FfmpegPaths paths) : m_paths(std::move(paths)) {}

    // Runs the export, reporting progress from 0 to 1 (from worker threads). Blocks; call from a
    // worker thread. On failure or cancellation the partial output is deleted and the exception
    // (FfmpegError, MediaError, OperationCancelled, std::invalid_argument) is rethrown.
    ExportResult exportClip(const ExportRequest &request, const Progress &progress = {},
                            const CancelToken &cancel = {}) const;

    // Throws std::invalid_argument for a bad range or an output that would overwrite the source.
    static void validate(const ExportRequest &request);

    // The ffmpeg arguments for a Reencode export.
    static QStringList buildArguments(const ExportRequest &request);

    // Stream-copies a keyframe-to-keyframe segment of the video to MPEG-TS (output path appended
    // by the caller).
    static QStringList copySegmentArguments(const QString &input, const SmartCutSegment &segment, double tolerance);

    // Re-encodes a segment of the video to H.264 in MPEG-TS (output path appended by the caller).
    static QStringList encodeSegmentArguments(const QString &input, const QString &pixelFormat,
                                              const SmartCutSegment &segment, double tolerance);

    // Parses ffprobe packet lines ("pts_time,flags") into sorted, distinct keyframe times relative
    // to fileStart.
    static QList<double> parseKeyframes(const QString &csv, double fileStart);

    // Reads the encoded position from an ffmpeg -progress line ("out_time_us=..."), or -1.
    static double parseProgressSeconds(const QString &line);

    // Seconds with at least 3 and at most 6 decimals, invariant culture: 1.5 -> "1.500".
    static QString formatSeconds(double seconds);

private:
    struct SourceInfo
    {
        QString videoCodec;
        QString pixelFormat;
        double frameRate = 0;
        double startTime = 0;
        QString audioCodec;
    };

    static QString smartCutUnsupportedReason(const SourceInfo &source);
    SourceInfo probeSource(const QString &input, const CancelToken &cancel) const;
    QList<double> probeKeyframes(const QString &input, double fileStart, double start, double end,
                                 const CancelToken &cancel) const;
    double probeAudioStartTime(const QString &input, const CancelToken &cancel) const;
    void reencode(const ExportRequest &request, const Progress &progress, const CancelToken &cancel) const;
    void smartCut(const ExportRequest &request, const SourceInfo &source, const Progress &progress,
                  const CancelToken &cancel) const;

    FfmpegPaths m_paths;
};

} // namespace cv
