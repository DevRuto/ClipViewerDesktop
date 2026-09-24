function(add_cv_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE cvcore Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_cv_test(tst_timeformat)
add_cv_test(tst_startup)
add_cv_test(tst_clipexporter)

# Generates sample clips with ffmpeg's lavfi sources; skips itself when ffmpeg isn't found.
add_cv_test(tst_ffmpegintegration)
set_tests_properties(tst_ffmpegintegration PROPERTIES TIMEOUT 300)
