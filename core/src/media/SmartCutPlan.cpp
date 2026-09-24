#include "SmartCutPlan.h"

#include <algorithm>

namespace cv::SmartCutPlan {

QList<SmartCutSegment> create(QList<double> keyframes, double start, double end, double tolerance)
{
    keyframes.removeIf([&](double k) { return k < start - tolerance || k > end + tolerance; });
    std::sort(keyframes.begin(), keyframes.end());
    if (keyframes.isEmpty())
        return {{start, end, false}};

    const double copyStart = keyframes.first() - start <= tolerance ? start : keyframes.first();
    const double copyEnd = end - keyframes.last() <= tolerance ? end : keyframes.last();
    if (copyEnd - copyStart <= tolerance)
        return {{start, end, false}};

    QList<SmartCutSegment> segments;
    if (copyStart > start)
        segments.append({start, copyStart, false});
    segments.append({copyStart, copyEnd, true});
    if (end > copyEnd)
        segments.append({copyEnd, end, false});
    return segments;
}

} // namespace cv::SmartCutPlan
