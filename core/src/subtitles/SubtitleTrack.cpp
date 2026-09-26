#include "SubtitleTrack.h"

#include <algorithm>

namespace cv {

int SubtitleTrack::cueAt(double seconds) const
{
    // The first cue starting after `seconds`; the one showing, if any, is before it.
    const auto after = std::upper_bound(cues.cbegin(), cues.cend(), seconds,
                                        [](double t, const SubtitleCue &cue) { return t < cue.start; });
    // Overlapping cues are rare and few, so looking back a handful is enough.
    int looked = 0;
    for (auto it = after; it != cues.cbegin() && looked < 8; ++looked) {
        --it;
        if (seconds < it->end)
            return static_cast<int>(it - cues.cbegin());
    }
    return -1;
}

void SubtitleTrack::normalize()
{
    cues.removeIf([](const SubtitleCue &cue) { return !(cue.end > cue.start); });
    std::stable_sort(cues.begin(), cues.end(),
                     [](const SubtitleCue &a, const SubtitleCue &b) { return a.start < b.start; });
}

} // namespace cv
