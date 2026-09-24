#pragma once

#include <QList>

namespace cv {

// A piece of a smart cut: [start, end) in seconds, either stream-copied or re-encoded.
struct SmartCutSegment
{
    double start = 0;
    double end = 0;
    bool copy = false;

    double length() const { return end - start; }
    bool operator==(const SmartCutSegment &) const = default;
};

namespace SmartCutPlan {

// Stream-copies from the first keyframe at or after start up to the last keyframe at or before
// end, and re-encodes the partial GOPs on either side. The copied part must end on a keyframe:
// B-frames just before an arbitrary end point can reference frames after it. Returns a single
// encoded segment if no keyframe-to-keyframe span fits in the range.
//
// keyframes: times in seconds, in any order. tolerance: how close a keyframe must be to a cut
// point to count as on it (half a frame, since trim points are snapped to the frame grid).
QList<SmartCutSegment> create(QList<double> keyframes, double start, double end, double tolerance);

} // namespace SmartCutPlan

} // namespace cv
