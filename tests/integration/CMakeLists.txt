# Integration tests: CLI end-to-end, worker process and test executables.
# Component owners add independent blocks below.

# --- CLI smoke tests (T-000) -------------------------------------------------
scale_add_command_test(NAME cli_version
    COMMAND $<TARGET_FILE:scale> --version
    EXPECT_OUTPUT "^scale ${PROJECT_VERSION}\n"
    LABELS integration)

scale_add_command_test(NAME cli_help_lists_commands
    COMMAND $<TARGET_FILE:scale> help
    EXPECT_OUTPUT "usage: scale <command>.*inspect <file>.*run <executable>"
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


# --- PE test executables vs dumpbin (T-102) --------------------------------------------
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
add_dependencies(integration_pe_dumpbin test_exit test_lib test_ordinal)
target_compile_definitions(integration_pe_dumpbin PRIVATE
    "SCALE_DUMPBIN_PATH=\"${SCALE_DUMPBIN}\""
    "SCALE_TEST_EXE_EXIT=\"$<TARGET_FILE:test_exit>\""
    "SCALE_TEST_EXE_LIB=\"$<TARGET_FILE:test_lib>\""
    "SCALE_TEST_EXE_ORDINAL=\"$<TARGET_FILE:test_ordinal>\"")

# --- scale inspect (T-103) ------------------------------------------------------
# Malformed inputs generated at configure time (never executed).
set(_scale_bad_pe "${CMAKE_CURRENT_BINARY_DIR}/not_a_pe.bin")
file(WRITE "${_scale_bad_pe}" "this is not a PE file")

scale_add_command_test(NAME cli_inspect_text
    COMMAND $<TARGET_FILE:scale> inspect $<TARGET_FILE:test_exit>
    EXPECT_OUTPUT "type: +PE32\\+ AMD64 executable\n.*\n  \\.text +va 0x1000 .*KERNEL32\\.dll\n +ExitProcess \\(hint [0-9]+\\)\n.*relocations: 1 blocks, 2 entries, absolute 1, dir64 1\n"
    LABELS integration)
scale_add_command_test(NAME cli_inspect_json_imports
    COMMAND $<TARGET_FILE:scale> inspect --json $<TARGET_FILE:test_ordinal>
    EXPECT_OUTPUT "\"schema_version\": 1,\n.*\"result\": \"ok\",\n.*\"module\": \"test_lib\\.dll\",.*\"kind\": \"name\",\n +\"name\": \"TestMul\",.*\"kind\": \"ordinal\",\n +\"ordinal\": 7\n"
    LABELS integration)
scale_add_command_test(NAME cli_inspect_json_dll
    COMMAND $<TARGET_FILE:scale> inspect $<TARGET_FILE:test_lib> --json
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

# --- First native execution slice (T-202) ----------------------------------------
scale_add_test(NAME integration_native_execution
    SOURCES native_execution_test.cpp
    LIBS scale::host scale::common
    LABELS integration
    TIMEOUT 60)
add_dependencies(integration_native_execution scale_worker test_ret42 test_reloc test_crash test_hang
    test_exit test_lib)
target_compile_definitions(integration_native_execution PRIVATE
    "SCALE_WORKER_PATH=\"$<TARGET_FILE:scale_worker>\""
    "SCALE_TEST_EXE_RET42=\"$<TARGET_FILE:test_ret42>\""
    "SCALE_TEST_EXE_RELOC=\"$<TARGET_FILE:test_reloc>\""
    "SCALE_TEST_EXE_CRASH=\"$<TARGET_FILE:test_crash>\""
    "SCALE_TEST_EXE_HANG=\"$<TARGET_FILE:test_hang>\""
    "SCALE_TEST_EXE_EXIT=\"$<TARGET_FILE:test_exit>\""
    "SCALE_TEST_EXE_LIB=\"$<TARGET_FILE:test_lib>\"")

# --- scale run (T-203) ------------------------------------------------------------
scale_add_command_test(NAME cli_run_pass
    COMMAND $<TARGET_FILE:scale> run $<TARGET_FILE:test_ret42>
    EXPECT_OUTPUT "sha256: +[0-9a-f]+\nstage: +returned\noutcome: +PASS\nreturned: +42\n.*no diagnostics"
    LABELS integration)
scale_add_command_test(NAME cli_run_relocated
    COMMAND $<TARGET_FILE:scale> run $<TARGET_FILE:test_reloc>
    EXPECT_OUTPUT "outcome: +PASS\nreturned: +42\n"
    LABELS integration)
scale_add_command_test(NAME cli_run_blocked_on_imports
    COMMAND $<TARGET_FILE:scale> run $<TARGET_FILE:test_exit>
    EXPECT_EXIT 1
    EXPECT_OUTPUT "outcome: +BLOCKED\n.*\\[error, fatal\\] loader/map/unsupported_directory: the import_table directory"
    LABELS integration)
scale_add_command_test(NAME cli_run_crash_fails
    COMMAND $<TARGET_FILE:scale> run $<TARGET_FILE:test_crash>
    EXPECT_EXIT 1
    EXPECT_OUTPUT "outcome: +FAILED\n.*host/entry/unhandled_exception exception_code=0xc0000005 address=0x[0-9a-f]+ rva=0x[0-9a-f]+"
    LABELS integration)
scale_add_command_test(NAME cli_run_timeout_fails
    COMMAND $<TARGET_FILE:scale> run $<TARGET_FILE:test_hang> --timeout-ms 500
    EXPECT_EXIT 1
    EXPECT_OUTPUT "outcome: +FAILED\n.*host/worker/timeout timeout_ms=500 last_stage=entry"
    LABELS integration)
scale_add_command_test(NAME cli_run_malformed_fails
    COMMAND $<TARGET_FILE:scale> run ${_scale_bad_pe}
    EXPECT_EXIT 1
    EXPECT_OUTPUT "outcome: +FAILED\n.*loader/inspect/truncated_dos_header"
    LABELS integration)
scale_add_command_test(NAME cli_run_rejects_guest_arguments
    COMMAND $<TARGET_FILE:scale> run $<TARGET_FILE:test_ret42> -- extra
    EXPECT_EXIT 2
    EXPECT_OUTPUT "takes no arguments"
    LABELS integration)
scale_add_command_test(NAME cli_run_rejects_bad_timeout
    COMMAND $<TARGET_FILE:scale> run $<TARGET_FILE:test_ret42> --timeout-ms 0
    EXPECT_EXIT 2
    EXPECT_OUTPUT "--timeout-ms must be an integer"
    LABELS integration)
scale_add_command_test(NAME cli_run_requires_executable
    COMMAND $<TARGET_FILE:scale> run
    EXPECT_EXIT 2
    EXPECT_OUTPUT "usage: scale run <executable>"
    LABELS integration)

# report.json written by the CLI, then checked by a dependent test.
set(_scale_run_report "${CMAKE_CURRENT_BINARY_DIR}/reports/cli_run_report.json")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/reports")
scale_add_command_test(NAME cli_run_writes_report
    COMMAND $<TARGET_FILE:scale> run $<TARGET_FILE:test_reloc> --report ${_scale_run_report}
    EXPECT_OUTPUT "report: +.*cli_run_report\\.json"
    LABELS integration)
set_tests_properties(cli_run_writes_report PROPERTIES FIXTURES_SETUP cli_run_report)
add_test(NAME cli_run_report_content
    COMMAND ${CMAKE_COMMAND}
        "-DSCALE_FILE=${_scale_run_report}"
        "-DSCALE_EXPECT_CONTENT=\"schema_version\": 1,\n  \"run_id\": \"run-[0-9]+T[0-9]+Z-[0-9]+-[0-9]+\",.*\"command\": \"run\",.*\"kind\": \"executable\",.*\"sha256\": \"[0-9a-f]+\".*\"final_stage\": \"returned\",\n  \"outcome\": \"PASS\",\n  \"exit_code\": 42,\n  \"elapsed_ms\": [0-9]+,.*\"diagnostics\": \\[\\]"
        -P "${PROJECT_SOURCE_DIR}/tests/support/expect_file.cmake")
set_tests_properties(cli_run_report_content PROPERTIES
    LABELS integration FIXTURES_REQUIRED cli_run_report TIMEOUT 30)