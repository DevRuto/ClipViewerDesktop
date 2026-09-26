qt_add_executable(ClipViewerDesktop WIN32 MACOSX_BUNDLE
    src/EditorController.cpp
    src/EditorController.h
    src/Background.h
    src/StillFrameProvider.h
    src/SubtitleController.cpp
    src/SubtitleController.h
    src/main.cpp
)

if(WIN32)
    target_sources(ClipViewerDesktop PRIVATE app.rc)
endif()

# Version.h is regenerated on every build, so the version follows new commits and tags without
# reconfiguring. main.cpp only recompiles when it actually changes.
find_package(Git QUIET)
set(version_header "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h")
add_custom_target(ClipViewerVersion
    COMMAND "${CMAKE_COMMAND}"
        "-DVERSION=${CLIPVIEWER_VERSION}"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DOUTPUT=${version_header}"
        -P "${PROJECT_SOURCE_DIR}/cmake/GitVersion.cmake"
    BYPRODUCTS "${version_header}"
    VERBATIM)
add_dependencies(ClipViewerDesktop ClipViewerVersion)
target_include_directories(ClipViewerDesktop PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

# The generated QML type registrations include the QML_ELEMENT headers by file name.
target_include_directories(ClipViewerDesktop PRIVATE src)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(ClipViewerDesktop
    URI ClipViewer
    QML_FILES
        qml/AppButton.qml
        qml/AppComboBox.qml
        qml/HoverPreview.qml
        qml/Icon.qml
        qml/Main.qml
        qml/PopupBackground.qml
        qml/ReencodeSettings.qml
        qml/SegmentedControl.qml
        qml/SettingsPane.qml
        qml/SubtitleSettings.qml
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
