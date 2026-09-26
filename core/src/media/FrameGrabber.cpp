#include "FrameGrabber.h"

#include "ClipExporter.h"

namespace cv {

QImage FrameGrabber::grab(const QString &path, double seconds, int maxWidth, const CancelToken &cancel) const
{
    QStringList args{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-loglevel"),
                     QStringLiteral("error"), QStringLiteral("-ss"), ClipExporter::formatSeconds(seconds),
                     QStringLiteral("-i"), path, QStringLiteral("-frames:v"), QStringLiteral("1"), QStringLiteral("-an"),
                     QStringLiteral("-sn")};
    // -2 keeps the height even, so full size skips the filter rather than change an odd height.
    if (maxWidth > 0)
        args << QStringLiteral("-vf") << QStringLiteral("scale='min(%1,iw)':-2").arg(maxWidth);
    args << QStringLiteral("-f") << QStringLiteral("image2pipe") << QStringLiteral("-c:v") << QStringLiteral("bmp")
         << QStringLiteral("-");
    const ProcessResult result = runProcess(m_paths.ffmpeg, args, cancel);

    if (result.exitCode != 0 || result.standardOutput.isEmpty())
        return {};
    return QImage::fromData(result.standardOutput, "BMP");
}

} // namespace cv
