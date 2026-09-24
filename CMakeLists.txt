cmake_minimum_required(VERSION 3.21)

project(ClipViewerDesktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Multimedia Concurrent Test)
qt_standard_project_setup(REQUIRES 6.8)

option(CLIPVIEWER_BUILD_TESTS "Build the unit and integration tests" ON)

add_subdirectory(core)

if(CLIPVIEWER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
