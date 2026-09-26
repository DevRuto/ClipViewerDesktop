# UI-free logic: time formatting, command line, settings and everything that drives ffmpeg.
# No widgets or QML, so it can be tested headless.
add_library(cvcore STATIC
    src/AppSettings.cpp
    src/AppSettings.h
    src/PlayerClickGesture.cpp
    src/PlayerClickGesture.h
    src/StartupArgs.cpp
    src/StartupArgs.h
    src/TimeFormat.cpp
    src/TimeFormat.h
    src/media/ClipExporter.cpp
    src/media/ClipExporter.h
    src/media/FfmpegPaths.cpp
    src/media/FfmpegPaths.h
    src/media/FrameGrabber.cpp
    src/media/FrameGrabber.h
    src/media/MediaDetails.cpp
    src/media/MediaDetails.h
    src/media/MediaInfo.h
    src/media/MediaProbe.cpp
    src/media/MediaProbe.h
    src/media/Process.cpp
    src/media/Process.h
    src/media/ReencodeOptions.cpp
    src/media/ReencodeOptions.h
    src/media/SmartCutPlan.cpp
    src/media/SmartCutPlan.h
)

target_include_directories(cvcore PUBLIC src)
# Gui only for QImage (FrameGrabber decodes the still frame).
target_link_libraries(cvcore PUBLIC Qt6::Core Qt6::Gui)
