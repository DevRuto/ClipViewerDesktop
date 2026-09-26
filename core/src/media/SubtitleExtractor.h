#pragma once

#include "FfmpegPaths.h"
#include "MediaInfo.h"
#include "Process.h"
#include "subtitles/SubtitleTrack.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace cv {

// Reads subtitles for our own overlay: a video's embedded subtitle streams (text through ffmpeg's
// SRT output, DVD pictures from ffprobe's packet dump) and separate subtitle files.
class SubtitleExtractor
{
public:
    explicit SubtitleExtractor(FfmpegPaths paths) : m_paths(std::move(paths)) {}

    // Subtitle stream `index` of the video (0-based among its subtitle streams). Reads through the
    // file, so it can take seconds on a large mkv. Blocks; call from a worker thread. Throws
    // FfmpegError, MediaError or OperationCancelled.
    SubtitleTrack extract(const QString &videoPath, int index, SubtitleFormat format, const CancelToken &cancel) const;

    // A subtitle file: .srt is parsed directly, the other text formats ffmpeg reads (.ass, .ssa,
    // .vtt) are converted to SRT first. Throws like extract; an unreadable or oversized file is a
    // MediaError.
    SubtitleTrack loadFile(const QString &path, const CancelToken &cancel) const;

    // Whether a path has one of the subtitle file extensions we load.
    static bool isSubtitleFile(const QString &path);

    // Subtitle files next to a video that belong to it: "<name>.srt" first, then others like
    // "<name>.en.srt" or "<name>.ass", by name.
    static QStringList sidecarFiles(const QString &videoPath);

    // A DVD picture track from ffprobe's JSON (-show_entries stream=extradata,width,height:
    // packet=pts_time,duration_time,data -show_data). A cue without a stop time ends at the next
    // packet, or after its packet's duration.
    static SubtitleTrack parsePictureTrack(const QByteArray &ffprobeJson);

private:
    FfmpegPaths m_paths;
};

} // namespace cv
