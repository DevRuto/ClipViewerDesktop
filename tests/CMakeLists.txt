function(add_cv_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE cvcore Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_cv_test(tst_timeformat)
add_cv_test(tst_startup)
add_cv_test(tst_clipexporter)
add_cv_test(tst_subtitles)
add_cv_test(tst_log)

# Generates sample clips with ffmpeg's lavfi sources; skips itself when ffmpeg isn't found.
add_cv_test(tst_ffmpegintegration)
set_tests_properties(tst_ffmpegintegration PROPERTIES TIMEOUT 300)

# Plays ffmpeg-generated clips in each supported format through Qt Multimedia; skips without ffmpeg.
add_cv_test(tst_playbackformats)
target_link_libraries(tst_playbackformats PRIVATE Qt6::Multimedia)
set_tests_properties(tst_playbackformats PROPERTIES TIMEOUT 300 ENVIRONMENT QT_QPA_PLATFORM=offscreen)
