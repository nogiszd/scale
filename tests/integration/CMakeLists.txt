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
