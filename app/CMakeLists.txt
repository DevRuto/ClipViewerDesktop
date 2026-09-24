qt_add_executable(ClipViewerDesktop WIN32 MACOSX_BUNDLE
    src/EditorController.cpp
    src/EditorController.h
    src/StillFrameProvider.h
    src/main.cpp
)

if(WIN32)
    target_sources(ClipViewerDesktop PRIVATE app.rc)
endif()

target_compile_definitions(ClipViewerDesktop PRIVATE CLIPVIEWER_VERSION="${CLIPVIEWER_VERSION}")

# The generated QML type registrations include the QML_ELEMENT headers by file name.
target_include_directories(ClipViewerDesktop PRIVATE src)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(ClipViewerDesktop
    URI ClipViewer
    QML_FILES
        qml/AppButton.qml
        qml/Icon.qml
        qml/Main.qml
        qml/Theme.qml
        qml/TimeField.qml
        qml/TrimTimeline.qml
    RESOURCES
        assets/app-icon.png
        assets/fonts/Geist-Bold.ttf
        assets/fonts/Geist-Medium.ttf
        assets/fonts/Geist-Regular.ttf
        assets/fonts/Geist-SemiBold.ttf
        assets/fonts/GeistMono-Medium.ttf
        assets/fonts/GeistMono-Regular.ttf
        assets/fonts/GeistMono-SemiBold.ttf
)

target_link_libraries(ClipViewerDesktop PRIVATE
    cvcore
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
)

# `cmake --install build/<preset> --prefix dist` gives a runnable folder (an app bundle on macOS)
# with Qt's libraries, QML modules and plugins (windeployqt / macdeployqt under the hood).
include(GNUInstallDirs)
install(TARGETS ClipViewerDesktop BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Leave out what the app never loads: it's English only, renders with D3D11 on Windows (so no
# software OpenGL), plays through the FFmpeg media backend and only reads PNG/BMP images.
set(deploy_tool_options)
if(WIN32)
    set(deploy_tool_options
        --no-opengl-sw
        --skip-plugin-types qmltooling,generic,networkinformation,tls
        --exclude-plugins windowsmediaplugin,qicns,qtga,qtiff,qwbmp,qwebp)
endif()
qt_generate_deploy_qml_app_script(
    TARGET ClipViewerDesktop
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS
    DEPLOY_TOOL_OPTIONS ${deploy_tool_options}
)
install(SCRIPT ${deploy_script})

# windeployqt copies every Qt Quick Controls style; main.cpp only ever uses Basic.
if(WIN32)
    install(CODE [[
        set(prefix "${CMAKE_INSTALL_PREFIX}")
        foreach(style FluentWinUI3 Fusion Imagine Material Universal Windows)
            file(REMOVE_RECURSE "${prefix}/qml/QtQuick/Controls/${style}")
            file(GLOB style_dlls "${prefix}/bin/Qt6QuickControls2${style}*.dll")
            if(style_dlls)
                file(REMOVE ${style_dlls})
            endif()
        endforeach()
        file(REMOVE_RECURSE "${prefix}/qml/QtQuick/NativeStyle")
    ]])
endif()

# After the Qt deployment, so the deploy tools leave the FFmpeg binaries alone. FfmpegPaths looks
# in an `ffmpeg` folder next to the executable.
if(CLIPVIEWER_BUNDLE_FFMPEG)
    include(BundleFfmpeg)
    if(APPLE)
        clipviewer_bundle_ffmpeg(ClipViewerDesktop.app/Contents/MacOS/ffmpeg)
    else()
        clipviewer_bundle_ffmpeg(${CMAKE_INSTALL_BINDIR}/ffmpeg)
    endif()
endif()
