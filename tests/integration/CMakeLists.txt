# Integration tests: CLI end-to-end, worker process and fixture images.
# Component owners add independent blocks below.

# --- CLI smoke tests (T-000) -------------------------------------------------
scale_add_command_test(NAME cli_version
    COMMAND $<TARGET_FILE:scale> --version
    EXPECT_OUTPUT "^scale ${PROJECT_VERSION}\n"
    LABELS integration)

scale_add_command_test(NAME cli_help_lists_commands
    COMMAND $<TARGET_FILE:scale> help
    EXPECT_OUTPUT "usage: scale <command>.*inspect <file>.*run <trusted-fixture.exe>"
    LABELS integration)

scale_add_command_test(NAME cli_no_arguments_is_usage_error
    COMMAND $<TARGET_FILE:scale>
    EXPECT_EXIT 2
    EXPECT_OUTPUT "usage: scale <command>"
    LABELS integration)

scale_add_command_test(NAME cli_unknown_command_is_usage_error
    COMMAND $<TARGET_FILE:scale> frobnicate
    EXPECT_EXIT 2
    EXPECT_OUTPUT "unknown command 'frobnicate'"
    LABELS integration)

# Replace with real inspect tests when T-103 lands.
scale_add_command_test(NAME cli_unimplemented_command_reports_task
    COMMAND $<TARGET_FILE:scale> inspect missing.exe
    EXPECT_EXIT 3
    EXPECT_OUTPUT "command 'inspect' is not implemented yet \\(task T-103\\)"
    LABELS integration)

# --- PE fixtures vs dumpbin (T-102) --------------------------------------------
get_filename_component(_scale_linker_dir "${CMAKE_LINKER}" DIRECTORY)
find_program(SCALE_DUMPBIN dumpbin HINTS "${_scale_linker_dir}" NO_CACHE)
if(SCALE_DUMPBIN)
    message(STATUS "dumpbin for PE comparison: ${SCALE_DUMPBIN}")
else()
    message(STATUS "dumpbin not found: PE comparison tests will be SKIPPED")
    set(SCALE_DUMPBIN "")
endif()

scale_add_test(NAME integration_pe_dumpbin
    SOURCES pe_dumpbin_test.cpp
    LIBS scale::loader scale::common
    LABELS integration)
add_dependencies(integration_pe_dumpbin fixture_exit fixture_lib fixture_ordinal)
target_compile_definitions(integration_pe_dumpbin PRIVATE
    "SCALE_DUMPBIN_PATH=\"${SCALE_DUMPBIN}\""
    "SCALE_FIXTURE_EXIT=\"$<TARGET_FILE:fixture_exit>\""
    "SCALE_FIXTURE_LIB=\"$<TARGET_FILE:fixture_lib>\""
    "SCALE_FIXTURE_ORDINAL=\"$<TARGET_FILE:fixture_ordinal>\"")
