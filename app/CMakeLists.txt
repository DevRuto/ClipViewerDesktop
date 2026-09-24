qt_add_executable(ClipViewerDesktop WIN32
    src/EditorController.cpp
    src/EditorController.h
    src/StillFrameProvider.h
    src/main.cpp
)

if(WIN32)
    target_sources(ClipViewerDesktop PRIVATE app.rc)
endif()

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

# `cmake --install build/<preset> --prefix dist` gives a runnable folder with Qt's DLLs, QML
# modules and plugins (windeployqt under the hood).
include(GNUInstallDirs)
install(TARGETS ClipViewerDesktop BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
qt_generate_deploy_qml_app_script(
    TARGET ClipViewerDesktop
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
