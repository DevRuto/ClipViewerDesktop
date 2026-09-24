#include "FrameGrabber.h"

#include "ClipExporter.h"

namespace cv {

QImage FrameGrabber::grab(const QString &path, double seconds, int maxWidth, const CancelToken &cancel) const
{
    const ProcessResult result = runProcess(
        m_paths.ffmpeg,
        {QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-loglevel"), QStringLiteral("error"),
         QStringLiteral("-ss"), ClipExporter::formatSeconds(seconds), QStringLiteral("-i"), path,
         QStringLiteral("-frames:v"), QStringLiteral("1"), QStringLiteral("-an"), QStringLiteral("-sn"),
         QStringLiteral("-vf"), QStringLiteral("scale='min(%1,iw)':-2").arg(maxWidth),
         QStringLiteral("-f"), QStringLiteral("image2pipe"), QStringLiteral("-c:v"), QStringLiteral("bmp"),
         QStringLiteral("-")},
        cancel);

    if (result.exitCode != 0 || result.standardOutput.isEmpty())
        return {};
    return QImage::fromData(result.standardOutput, "BMP");
}

} // namespace cv
