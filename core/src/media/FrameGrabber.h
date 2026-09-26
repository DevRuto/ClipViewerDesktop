#pragma once

#include "FfmpegPaths.h"
#include "Process.h"

#include <QImage>

namespace cv {

// Decodes a single frame with ffmpeg. Uses the same input seeking as an accurate export, so the
// frame at a trim point is exactly the frame the exported clip starts with.
class FrameGrabber
{
public:
    explicit FrameGrabber(FfmpegPaths paths) : m_paths(std::move(paths)) {}

    // The frame at `seconds`, scaled down to at most maxWidth pixels wide (0: full size), or a null
    // image if there is no frame there (e.g. past the end). Blocks; call from a worker thread.
    QImage grab(const QString &path, double seconds, int maxWidth = 1920, const CancelToken &cancel = {}) const;

    // The keyframe at or before `seconds`: not exact, but it decodes one frame instead of up to a
    // whole GOP, so it's for previews. Falls back to grab() if that finds nothing.
    QImage grabKeyframe(const QString &path, double seconds, int maxWidth, const CancelToken &cancel = {}) const;

private:
    QImage run(const QString &path, double seconds, int maxWidth, bool keyframe, const CancelToken &cancel) const;

    FfmpegPaths m_paths;
};

} // namespace cv
