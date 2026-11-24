# Test registration helpers shared by every component.
#
# Labels (use exactly one primary label per test):
#   quick        small deterministic CPU tests, no processes or GPU
#   integration  worker process, fixture images, CLI end-to-end
#   gpu          requires real GPU hardware (software Vulkan must be labeled
#                additionally with `software_gpu`)
#
# Exit code 77 means SKIPPED (e.g. no GPU, missing optional tool). A skipped
# test is never reported as PASS.

set(SCALE_TEST_SKIP_RETURN_CODE 77)
set(SCALE_TEST_DEFAULT_TIMEOUT 30)
set(SCALE_TEST_LABELS quick integration gpu)

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

# scale_add_test(NAME <name> SOURCES ... [LIBS ...] LABELS ... [TIMEOUT <s>])
#
# Builds a test executable using the in-tree assertion harness
# (tests/support/scale_test.h) and registers it with CTest.
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
    target_link_libraries(${ARG_NAME} PRIVATE scale::test_support ${ARG_LIBS} scale::build_options)
    add_test(NAME ${ARG_NAME} COMMAND ${ARG_NAME})
    set_tests_properties(${ARG_NAME} PROPERTIES
        LABELS "${ARG_LABELS}"
        TIMEOUT ${ARG_TIMEOUT}
        SKIP_RETURN_CODE ${SCALE_TEST_SKIP_RETURN_CODE})
endfunction()

# scale_add_command_test(NAME <name> COMMAND <exe> [args...] LABELS ...
#                        [EXPECT_EXIT <code>] [EXPECT_OUTPUT <regex>]
#                        [TIMEOUT <s>])
#
# Runs a command (e.g. the CLI) through tests/support/expect_command.cmake and
# checks its exact exit code (default 0) and, optionally, that the combined
# stdout+stderr matches a regular expression. Both conditions must hold.
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
