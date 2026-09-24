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

    // The frame at `seconds`, scaled down to at most maxWidth pixels wide, or a null image if
    // there is no frame there (e.g. past the end). Blocks; call from a worker thread.
    QImage grab(const QString &path, double seconds, int maxWidth = 1920, const CancelToken &cancel = {}) const;

private:
    FfmpegPaths m_paths;
};

} // namespace cv
