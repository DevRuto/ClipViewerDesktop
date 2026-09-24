#include "PlayerClickGesture.h"

namespace cv {

PlayerClickAction PlayerClickGesture::click(double x, qint64 nowMs)
{
    const int zone = x < EdgeZoneRatio ? -1 : x > 1 - EdgeZoneRatio ? 1 : 0;
    if (zone != 0 && zone == m_pendingZone && nowMs - m_pendingAt <= DoubleClickWindowMs) {
        m_pendingZone = 0;
        return zone < 0 ? PlayerClickAction::UndoToggleAndSeekBack : PlayerClickAction::UndoToggleAndSeekForward;
    }

    // An edge click waits for a possible second one; a middle click has nothing to wait for.
    m_pendingZone = zone;
    m_pendingAt = nowMs;
    return PlayerClickAction::TogglePlay;
}

} // namespace cv
