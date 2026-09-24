#pragma once

#include <QtGlobal>

namespace cv {

// What a click on the video should do.
enum class PlayerClickAction {
    TogglePlay,
    // A double-click on an edge: undo the first click's play/pause toggle, then seek.
    UndoToggleAndSeekBack,
    UndoToggleAndSeekForward,
};

// Click gestures on the video, as on the ClipViewer web player. A click toggles play/pause
// straight away. A second click in the same edge zone (the outer 30% on either side) within
// 300 ms makes it a double-click that seeks instead, and the first click's toggle is undone. So
// a plain click never waits, and a double-click only costs a brief play/pause flicker. The middle
// has no double-click gesture.
class PlayerClickGesture
{
public:
    static constexpr double EdgeZoneRatio = 0.3;
    static constexpr qint64 DoubleClickWindowMs = 300;

    // Resolves a click at x (0-1 across the video) made at nowMs (any monotonic clock).
    PlayerClickAction click(double x, qint64 nowMs);

private:
    int m_pendingZone = 0;
    qint64 m_pendingAt = 0;
};

} // namespace cv
