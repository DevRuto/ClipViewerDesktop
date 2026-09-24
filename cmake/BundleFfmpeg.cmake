# Puts ffmpeg and ffprobe in an `ffmpeg` folder next to the installed executable, where
# FfmpegPaths looks first. Either downloads a pinned FFmpeg build (checked against its SHA-256)
# or copies the binaries from CLIPVIEWER_FFMPEG_DIR.
#
# Sources: gyan.dev's "essentials" build (GitHub mirror) on Windows, Martin Riedl's builds on
# Linux and macOS. Both are GPL builds, shipped as separate programs next to the app.

set(CLIPVIEWER_FFMPEG_VERSION 9.0.2)

function(_cv_ffmpeg_download url sha256 out_file)
    if(EXISTS "${out_file}")
        file(SHA256 "${out_file}" existing)
        if(existing STREQUAL sha256)
            return()
        endif()
    endif()
    message(STATUS "Downloading ${url}")
    file(DOWNLOAD "${url}" "${out_file}" EXPECTED_HASH SHA256=${sha256} STATUS status TLS_VERIFY ON)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        file(REMOVE "${out_file}")
        list(GET status 1 reason)
        message(FATAL_ERROR "FFmpeg download failed (${reason}): ${url}\n"
            "Set CLIPVIEWER_FFMPEG_DIR to a folder with ffmpeg and ffprobe, or turn off CLIPVIEWER_BUNDLE_FFMPEG.")
    endif()
endfunction()

function(clipviewer_bundle_ffmpeg destination)
    if(WIN32)
        set(exe_suffix .exe)
    endif()

    if(CLIPVIEWER_FFMPEG_DIR)
        set(ffmpeg_dir "${CLIPVIEWER_FFMPEG_DIR}")
    else()
        set(work_dir "${CMAKE_BINARY_DIR}/_deps/ffmpeg-${CLIPVIEWER_FFMPEG_VERSION}")
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" cpu)
        if(APPLE AND CMAKE_OSX_ARCHITECTURES)
            list(GET CMAKE_OSX_ARCHITECTURES 0 cpu)
        endif()
        if(cpu MATCHES "^(x86_64|amd64|x64)$")
            set(arch amd64)
        elseif(cpu MATCHES "^(arm64|aarch64)$")
            set(arch arm64)
        else()
            message(FATAL_ERROR "No FFmpeg build is pinned for '${cpu}'. Set CLIPVIEWER_FFMPEG_DIR.")
        endif()

        if(WIN32)
            # x64 only; it also runs under emulation on Windows on Arm.
            set(name ffmpeg-${CLIPVIEWER_FFMPEG_VERSION}-essentials_build)
            _cv_ffmpeg_download(
                "https://github.com/GyanD/codexffmpeg/releases/download/${CLIPVIEWER_FFMPEG_VERSION}/${name}.zip"
                60f467265b1e312373dbcd92200c2618a74850f98d3d078e94296bb3fa2047ba
                "${work_dir}/${name}.zip")
            if(NOT EXISTS "${work_dir}/${name}/bin/ffprobe.exe")
                file(ARCHIVE_EXTRACT INPUT "${work_dir}/${name}.zip" DESTINATION "${work_dir}"
                    PATTERNS "${name}/bin/ffmpeg.exe" "${name}/bin/ffprobe.exe" "${name}/LICENSE")
            endif()
            set(ffmpeg_dir "${work_dir}/${name}/bin")
            set(license_file "${work_dir}/${name}/LICENSE")
        else()
            if(APPLE)
                set(os macos)
            else()
                set(os linux)
            endif()
            # Martin Riedl's download paths carry a build id per platform.
            set(build_linux_amd64 1789931100)
            set(build_linux_arm64 1789931697)
            set(build_macos_amd64 1789931006)
            set(build_macos_arm64 1789931890)
            set(sha_linux_amd64_ffmpeg fa8ecf4abbd290d98f7d188b8649cc6b391ae209a98452be955a15aab1909d7f)
            set(sha_linux_amd64_ffprobe 3f428c49070be3d24ec338602b76d412e401ffcb8a5641ef0e729181a232fc32)
            set(sha_linux_arm64_ffmpeg 93a76ae90db5474eecdf951a729857c64f3de23567228d6a7d5e6e8e3cd1021b)
            set(sha_linux_arm64_ffprobe bcbe80fb741c180083327afaf5434812e006b33cacde2016b9aeaf6936128330)
            set(sha_macos_amd64_ffmpeg 7c6b4125b191cbf773832dc51f424cf2b6bb7da43007d1e066f95909e47cacd4)
            set(sha_macos_amd64_ffprobe 2322438ed2f6319a691291b247d09c69dcaa3a982460d1f269a7e1af335cfdfd)
            set(sha_macos_arm64_ffmpeg c8ed4c4e6978a03c485edbfe4e0a5dc2380f8a30bba5150531b31b094492d924)
            set(sha_macos_arm64_ffprobe fcbe839537485eaee7a7a8bc5cbc0f90d53617e80943e8a5b2e31cb851197ea6)

            set(ffmpeg_dir "${work_dir}/${os}-${arch}")
            foreach(tool ffmpeg ffprobe)
                _cv_ffmpeg_download(
                    "https://ffmpeg.martin-riedl.de/download/${os}/${arch}/${build_${os}_${arch}}_${CLIPVIEWER_FFMPEG_VERSION}/${tool}.zip"
                    ${sha_${os}_${arch}_${tool}}
                    "${work_dir}/${os}-${arch}-${tool}.zip")
                if(NOT EXISTS "${ffmpeg_dir}/${tool}")
                    file(ARCHIVE_EXTRACT INPUT "${work_dir}/${os}-${arch}-${tool}.zip" DESTINATION "${ffmpeg_dir}")
                endif()
            endforeach()
        endif()
    endif()

    foreach(tool ffmpeg ffprobe)
        if(NOT EXISTS "${ffmpeg_dir}/${tool}${exe_suffix}")
            message(FATAL_ERROR "${tool}${exe_suffix} not found in ${ffmpeg_dir}")
        endif()
        install(PROGRAMS "${ffmpeg_dir}/${tool}${exe_suffix}" DESTINATION "${destination}")
    endforeach()
    if(license_file)
        install(FILES "${license_file}" DESTINATION "${destination}")
    endif()
    message(STATUS "Bundling FFmpeg from ${ffmpeg_dir}")
endfunction()
