cmake_minimum_required(VERSION 3.21)

project(ClipViewerDesktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Multimedia Concurrent Test)
qt_standard_project_setup(REQUIRES 6.8)

option(CLIPVIEWER_BUILD_TESTS "Build the unit and integration tests" ON)
option(CLIPVIEWER_BUNDLE_FFMPEG "Install ffmpeg and ffprobe next to the app (downloaded unless CLIPVIEWER_FFMPEG_DIR is set)" OFF)
set(CLIPVIEWER_FFMPEG_DIR "" CACHE PATH "Folder with ffmpeg and ffprobe to bundle instead of downloading them")
# Shown in the title bar. The release workflow sets it from the tag (v1.2.3 -> 1.2.3).
set(CLIPVIEWER_VERSION "${PROJECT_VERSION}-dev" CACHE STRING "App version shown in the title bar")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(core)
add_subdirectory(app)

if(CLIPVIEWER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
