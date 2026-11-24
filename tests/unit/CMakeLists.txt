# Unit tests: one GoogleTest executable per component. Component owners add
# their own `scale_add_test` block; keep blocks independent to avoid merge
# conflicts.

scale_add_test(NAME unit_common
    SOURCES common/checked_test.cpp common/byte_reader_test.cpp common/text_test.cpp common/sha256_test.cpp
    LIBS scale::common
    LABELS quick)

scale_add_test(NAME unit_diagnostics
    SOURCES diagnostics/json_writer_test.cpp diagnostics/diagnostic_log_test.cpp
            diagnostics/run_report_test.cpp diagnostics/event_line_test.cpp
    LIBS scale::diagnostics
    LABELS quick)

# --- PE inspector (T-101) -----------------------------------------------------
set(_scale_unit_loader_sources loader/pe_image_test.cpp loader/exception_table_test.cpp)
if(SCALE_PLATFORM_WINDOWS)
    list(APPEND _scale_unit_loader_sources loader/pe_image_win32_test.cpp)
endif()
scale_add_test(NAME unit_loader
    SOURCES ${_scale_unit_loader_sources}
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
if(SCALE_PLATFORM_WINDOWS)
    scale_add_test(NAME unit_host
        SOURCES host/supervisor_unit_test.cpp host/executable_report_test.cpp
        LIBS scale::host
        LABELS quick)
endif()

# --- Image mapping (T-202) -------------------------------------------------------
if(SCALE_PLATFORM_WINDOWS)
    scale_add_test(NAME unit_image_map
        SOURCES loader/image_map_test.cpp
        LIBS scale::loader
        LABELS quick)
endif()

# --- gfx1030 decoder (T-701) ---------------------------------------------------
if(SCALE_ENABLE_SHADER)
    add_subdirectory(shader)
endif()

# --- Import resolution policy (T-204) ---------------------------------------------
scale_add_test(NAME unit_runtime
    SOURCES runtime/import_resolver_test.cpp
    LIBS scale::runtime
    LABELS quick)
# --- Reference interpreter (T-711) ------------------------------------------------
if(SCALE_ENABLE_SHADER)
    scale_add_test(NAME unit_shader_ref
        SOURCES shader_ref/interpreter_test.cpp shader_ref/interpreter_v1_test.cpp
        LIBS scale::shader_ref
        LABELS quick)
endif()