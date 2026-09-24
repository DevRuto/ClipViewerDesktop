# Script mode (cmake -P), run on every build: writes OUTPUT, a header defining CLIPVIEWER_VERSION.
# VERSION wins when set (the release workflow passes the tag). Otherwise it comes from
# `git describe`: 1.2.0 on the v1.2.0 tag, 1.2.0-3-gabc1234 three commits later, -dirty with
# uncommitted changes. The header is only rewritten when the version changes, so builds stay
# incremental.
if(NOT VERSION AND GIT_EXECUTABLE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --match "v[0-9]*" --dirty
                    WORKING_DIRECTORY "${SOURCE_DIR}"
                    OUTPUT_VARIABLE describe RESULT_VARIABLE result
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(result EQUAL 0)
        string(REGEX REPLACE "^v" "" VERSION "${describe}")
    else()
        # No release tag reachable (none yet, or a shallow CI clone): the commit alone.
        execute_process(COMMAND "${GIT_EXECUTABLE}" describe --always --dirty
                        WORKING_DIRECTORY "${SOURCE_DIR}"
                        OUTPUT_VARIABLE describe RESULT_VARIABLE result
                        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(result EQUAL 0)
            set(VERSION "0.0.0-g${describe}")
        endif()
    endif()
endif()
if(NOT VERSION)
    set(VERSION "0.0.0-unknown")
endif()

file(WRITE "${OUTPUT}.tmp" "#pragma once\n#define CLIPVIEWER_VERSION \"${VERSION}\"\n")
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
