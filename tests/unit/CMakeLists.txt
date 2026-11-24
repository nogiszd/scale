# Unit tests: one GoogleTest executable per component. Component owners add
# their own `scale_add_test` block; keep blocks independent to avoid merge
# conflicts.

scale_add_test(NAME unit_common
    SOURCES common/checked_test.cpp common/byte_reader_test.cpp common/text_test.cpp
    LIBS scale::common
    LABELS quick)

scale_add_test(NAME unit_diagnostics
    SOURCES diagnostics/json_writer_test.cpp diagnostics/diagnostic_log_test.cpp
            diagnostics/run_report_test.cpp
    LIBS scale::diagnostics
    LABELS quick)

# --- PE inspector (T-101) -----------------------------------------------------
scale_add_test(NAME unit_loader
    SOURCES loader/pe_image_test.cpp
    LIBS scale::loader
    LABELS quick)

# --- Test infrastructure self-test (T-001) -----------------------------------
# The probe is built but its cases are not registered (they skip and fail on
# purpose). These tests check that GoogleTest's skip output matches the
# SKIP_REGULAR_EXPRESSION that scale_add_test gives every test, so a skipped
# test can never be reported as PASS, and that failures make the run fail.
add_executable(gtest_probe support/gtest_probe.cpp)
target_link_libraries(gtest_probe PRIVATE GTest::gtest_main scale::build_options)

scale_add_command_test(NAME selftest_gtest_skip_matches_ctest_skip_regex
    COMMAND $<TARGET_FILE:gtest_probe> --gtest_filter=Probe.Skipped
    EXPECT_EXIT 0
    EXPECT_OUTPUT "${SCALE_GTEST_SKIP_REGEX} Probe\\.Skipped"
    LABELS quick)
scale_add_command_test(NAME selftest_gtest_pass_reports_ok
    COMMAND $<TARGET_FILE:gtest_probe> --gtest_filter=Probe.Passing
    EXPECT_EXIT 0
    EXPECT_OUTPUT "\\[       OK \\] Probe\\.Passing.*\\[  PASSED  \\] 1 test\\."
    LABELS quick)
scale_add_command_test(NAME selftest_gtest_failure_fails_run
    COMMAND $<TARGET_FILE:gtest_probe> --gtest_filter=Probe.Failing
    EXPECT_EXIT 1
    EXPECT_OUTPUT "\\[  FAILED  \\] Probe\\.Failing"
    LABELS quick)

# --- Host supervisor helpers (T-201) -------------------------------------------
scale_add_test(NAME unit_host
    SOURCES host/supervisor_unit_test.cpp
    LIBS scale::host
    LABELS quick)

# --- Fixture loader mapping (T-202) ---------------------------------------------
scale_add_test(NAME unit_image_map
    SOURCES loader/image_map_test.cpp
    LIBS scale::loader
    LABELS quick)
