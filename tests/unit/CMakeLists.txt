# Unit tests: one executable per component. Component owners add their own
# `scale_add_test` block; keep blocks independent to avoid merge conflicts.

scale_add_test(NAME unit_common
    SOURCES common/checked_test.cpp common/byte_reader_test.cpp common/text_test.cpp
    LIBS scale::common
    LABELS quick)

scale_add_test(NAME unit_diagnostics
    SOURCES diagnostics/json_writer_test.cpp diagnostics/diagnostic_log_test.cpp
            diagnostics/run_report_test.cpp
    LIBS scale::diagnostics
    LABELS quick)

# Harness self-test: a case that calls SCALE_SKIP must make the executable exit
# 77 (CTest SKIPPED), never 0. The probe is not registered directly.
add_executable(harness_skip_probe support/skip_probe.cpp)
target_link_libraries(harness_skip_probe PRIVATE scale::test_support scale::build_options)
scale_add_command_test(NAME unit_harness_skip_exit_code
    COMMAND $<TARGET_FILE:harness_skip_probe> passing_case skipped_case
    EXPECT_EXIT 77
    EXPECT_OUTPUT "\\[SKIP \\(probe skip\\)\\] skipped_case.*1 passed, 0 failed, 1 skipped"
    LABELS quick)
scale_add_command_test(NAME unit_harness_failure_exit_code
    COMMAND $<TARGET_FILE:harness_skip_probe> failing_case
    EXPECT_EXIT 1
    EXPECT_OUTPUT "\\[FAIL\\] failing_case"
    LABELS quick)
