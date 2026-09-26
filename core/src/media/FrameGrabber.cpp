#include "FrameGrabber.h"

#include "ClipExporter.h"

namespace cv {

QImage FrameGrabber::grab(const QString &path, double seconds, int maxWidth, const CancelToken &cancel) const
{
    return run(path, seconds, maxWidth, false, cancel);
}

QImage FrameGrabber::grabKeyframe(const QString &path, double seconds, int maxWidth, const CancelToken &cancel) const
{
    const QImage frame = run(path, seconds, maxWidth, true, cancel);
    // A stream without flagged keyframes (e.g. intra refresh) gives nothing that way
    return frame.isNull() ? run(path, seconds, maxWidth, false, cancel) : frame;
}

QImage FrameGrabber::run(const QString &path, double seconds, int maxWidth, bool keyframe,
                         const CancelToken &cancel) const
{
    QStringList args{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-loglevel"),
                     QStringLiteral("error")};
    // Only the keyframe the seek lands on is decoded, and kept even though it's before `seconds`
    // (passthrough: otherwise its negative timestamp gets it dropped).
    if (keyframe)
        args << QStringLiteral("-noaccurate_seek") << QStringLiteral("-skip_frame") << QStringLiteral("nokey");
    args << QStringLiteral("-ss") << ClipExporter::formatSeconds(seconds) << QStringLiteral("-i") << path
         << QStringLiteral("-frames:v") << QStringLiteral("1") << QStringLiteral("-an") << QStringLiteral("-sn");
    // -2 keeps the height even, so full size skips the filter rather than change an odd height.
    if (maxWidth > 0)
        args << QStringLiteral("-vf") << QStringLiteral("scale='min(%1,iw)':-2").arg(maxWidth);
    if (keyframe)
        args << QStringLiteral("-fps_mode") << QStringLiteral("passthrough");
    args << QStringLiteral("-f") << QStringLiteral("image2pipe") << QStringLiteral("-c:v") << QStringLiteral("bmp")
         << QStringLiteral("-");
    const ProcessResult result = runProcess(m_paths.ffmpeg, args, cancel);

    if (result.exitCode != 0 || result.standardOutput.isEmpty())
        return {};
    return QImage::fromData(result.standardOutput, "BMP");
}

} // namespace cv
