# Test infrastructure: GoogleTest (pinned) run through CTest.
#
# Labels (use exactly one primary label per test):
#   quick        small deterministic CPU tests, no processes or GPU
#   integration  worker process, test executables, CLI end-to-end
#   gpu          requires real GPU hardware (software Vulkan must be labeled
#                additionally with `software_gpu`)
#
# GTEST_SKIP() is reported by CTest as SKIPPED, never as PASS.

include(FetchContent)
include(GoogleTest)

# Pinned GoogleTest release. Update version, URL and hash together; the hash
# is the `digest` published on the GitHub release page. To build offline, pass
# -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path to extracted googletest-1.18.0>.
set(SCALE_GOOGLETEST_VERSION 1.18.0)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/releases/download/v${SCALE_GOOGLETEST_VERSION}/googletest-${SCALE_GOOGLETEST_VERSION}.tar.gz
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL)
set(gtest_force_shared_crt ON CACHE BOOL "Use the same /MD runtime as SCALE" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
message(STATUS "GoogleTest ${SCALE_GOOGLETEST_VERSION} (pinned, SHA-256 verified)")

set(SCALE_TEST_DEFAULT_TIMEOUT 30)
set(SCALE_TEST_LABELS quick integration gpu)
# Matches GoogleTest's per-test skip marker. Shared with the self-test in
# tests/unit/CMakeLists.txt so the mapping stays verified.
set(SCALE_GTEST_SKIP_REGEX "\\[  SKIPPED \\]")

function(_scale_check_labels test_name)
    set(primary "")
    foreach(label IN LISTS ARGN)
        if(label IN_LIST SCALE_TEST_LABELS)
            list(APPEND primary ${label})
        endif()
    endforeach()
    list(LENGTH primary count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "Test ${test_name} must have exactly one of: ${SCALE_TEST_LABELS} (got: ${ARGN})")
    endif()
endfunction()

# scale_add_test(NAME <target> SOURCES ... [LIBS ...] LABELS ... [TIMEOUT <s>])
#
# Builds a GoogleTest executable (with gtest_main) and registers every test
# case as its own CTest test named `<target>.<Suite>.<Case>`.
function(scale_add_test)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "NAME;TIMEOUT" "SOURCES;LIBS;LABELS")
    if(NOT ARG_NAME OR NOT ARG_SOURCES)
        message(FATAL_ERROR "scale_add_test requires NAME and SOURCES")
    endif()
    _scale_check_labels(${ARG_NAME} ${ARG_LABELS})
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT ${SCALE_TEST_DEFAULT_TIMEOUT})
    endif()

    add_executable(${ARG_NAME} ${ARG_SOURCES})
    target_link_libraries(${ARG_NAME} PRIVATE GTest::gtest_main ${ARG_LIBS} scale::build_options)
    gtest_discover_tests(${ARG_NAME}
        TEST_PREFIX "${ARG_NAME}."
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 30
        PROPERTIES
            LABELS "${ARG_LABELS}"
            TIMEOUT ${ARG_TIMEOUT}
            SKIP_REGULAR_EXPRESSION "${SCALE_GTEST_SKIP_REGEX}")
endfunction()

# scale_add_command_test(NAME <name> COMMAND <exe> [args...] LABELS ...
#                        [EXPECT_EXIT <code>] [EXPECT_OUTPUT <regex>]
#                        [TIMEOUT <s>])
#
# Process-level test (CLI exit codes and output). Runs the command through
# tests/support/expect_command.cmake and checks its exact exit code (default
# 0) and, optionally, that combined stdout+stderr matches a regex.
function(scale_add_command_test)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "NAME;TIMEOUT;EXPECT_EXIT;EXPECT_OUTPUT" "COMMAND;LABELS")
    if(NOT ARG_NAME OR NOT ARG_COMMAND)
        message(FATAL_ERROR "scale_add_command_test requires NAME and COMMAND")
    endif()
    _scale_check_labels(${ARG_NAME} ${ARG_LABELS})
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT ${SCALE_TEST_DEFAULT_TIMEOUT})
    endif()
    if(NOT DEFINED ARG_EXPECT_EXIT)
        set(ARG_EXPECT_EXIT 0)
    endif()
    # Arguments are joined with a private separator and split again in the
    # script; generator expressions such as $<TARGET_FILE:scale> still work.
    list(JOIN ARG_COMMAND "@@" command_arg)
    add_test(NAME ${ARG_NAME}
        COMMAND ${CMAKE_COMMAND}
            "-DSCALE_COMMAND=${command_arg}"
            "-DSCALE_EXPECT_EXIT=${ARG_EXPECT_EXIT}"
            "-DSCALE_EXPECT_OUTPUT=${ARG_EXPECT_OUTPUT}"
            -P "${PROJECT_SOURCE_DIR}/tests/support/expect_command.cmake")
    set_tests_properties(${ARG_NAME} PROPERTIES
        LABELS "${ARG_LABELS}"
        TIMEOUT ${ARG_TIMEOUT})
endfunction()
