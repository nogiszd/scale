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

scale_add_command_test(NAME cli_unimplemented_command_reports_task
    COMMAND $<TARGET_FILE:scale> run missing.exe
    EXPECT_EXIT 3
    EXPECT_OUTPUT "command 'run' is not implemented yet \\(task T-203\\)"
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

# --- scale inspect (T-103) ------------------------------------------------------
# Malformed inputs generated at configure time (never executed).
set(_scale_bad_pe "${CMAKE_CURRENT_BINARY_DIR}/not_a_pe.bin")
file(WRITE "${_scale_bad_pe}" "this is not a PE file")

scale_add_command_test(NAME cli_inspect_text
    COMMAND $<TARGET_FILE:scale> inspect $<TARGET_FILE:fixture_exit>
    EXPECT_OUTPUT "type: +PE32\\+ AMD64 executable\n.*\n  \\.text +va 0x1000 .*KERNEL32\\.dll\n +ExitProcess \\(hint [0-9]+\\)\n.*relocations: 1 blocks, 2 entries, absolute 1, dir64 1\n"
    LABELS integration)
scale_add_command_test(NAME cli_inspect_json_imports
    COMMAND $<TARGET_FILE:scale> inspect --json $<TARGET_FILE:fixture_ordinal>
    EXPECT_OUTPUT "\"schema_version\": 1,\n.*\"result\": \"ok\",\n.*\"module\": \"fixture_lib\\.dll\",.*\"kind\": \"name\",\n +\"name\": \"FixtureMul\",.*\"kind\": \"ordinal\",\n +\"ordinal\": 7\n"
    LABELS integration)
scale_add_command_test(NAME cli_inspect_json_dll
    COMMAND $<TARGET_FILE:scale> inspect $<TARGET_FILE:fixture_lib> --json
    EXPECT_OUTPUT "\"is_dll\": true,\n.*\"name\": \"export_table\",\n.*\"imports\": \\[\\],\n"
    LABELS integration)
scale_add_command_test(NAME cli_inspect_rejects_malformed
    COMMAND $<TARGET_FILE:scale> inspect ${_scale_bad_pe}
    EXPECT_EXIT 1
    EXPECT_OUTPUT "error: loader/truncated_dos_header: "
    LABELS integration)
scale_add_command_test(NAME cli_inspect_json_error_document
    COMMAND $<TARGET_FILE:scale> inspect --json ${_scale_bad_pe}
    EXPECT_EXIT 1
    EXPECT_OUTPUT "\"result\": \"error\",\n +\"error\": \\{\n +\"component\": \"loader\",\n +\"code\": \"truncated_dos_header\",\n"
    LABELS integration)
scale_add_command_test(NAME cli_inspect_missing_file
    COMMAND $<TARGET_FILE:scale> inspect ${CMAKE_CURRENT_BINARY_DIR}/does_not_exist.exe
    EXPECT_EXIT 1
    EXPECT_OUTPUT "error: loader/file_open_failed: "
    LABELS integration)
scale_add_command_test(NAME cli_inspect_requires_file
    COMMAND $<TARGET_FILE:scale> inspect
    EXPECT_EXIT 2
    EXPECT_OUTPUT "usage: scale inspect <file> \\[--json\\]"
    LABELS integration)
scale_add_command_test(NAME cli_inspect_rejects_two_files
    COMMAND $<TARGET_FILE:scale> inspect a.exe b.exe
    EXPECT_EXIT 2
    EXPECT_OUTPUT "takes exactly one file"
    LABELS integration)
scale_add_command_test(NAME cli_inspect_rejects_unknown_option
    COMMAND $<TARGET_FILE:scale> inspect --verbose a.exe
    EXPECT_EXIT 2
    EXPECT_OUTPUT "unknown option '--verbose'"
    LABELS integration)

# --- Worker supervision (T-201) -------------------------------------------------
scale_add_test(NAME integration_host_supervisor
    SOURCES host_supervisor_test.cpp
    LIBS scale::host scale::common
    LABELS integration
    TIMEOUT 60)
add_dependencies(integration_host_supervisor scale_worker)
target_compile_definitions(integration_host_supervisor PRIVATE
    "SCALE_WORKER_PATH=\"$<TARGET_FILE:scale_worker>\"")
