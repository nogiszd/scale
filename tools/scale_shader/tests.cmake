# --- scale-shader (T-706) ------------------------------------------------------
set(case_dir "${PROJECT_SOURCE_DIR}/tests/shader/cases")
set(test_dir "${CMAKE_CURRENT_BINARY_DIR}")
set(validator_skip "SKIPPED: spirv-val is unavailable")

scale_add_command_test(NAME shader_cli_decode_t704
    COMMAND $<TARGET_FILE:scale-shader> decode "${case_dir}/t704_add_lane_to_input.json"
    EXPECT_OUTPUT "v_add_nc_u32_e32 v2, v0, v2"
    LABELS integration)
scale_add_command_test(NAME shader_cli_decode_t707
    COMMAND $<TARGET_FILE:scale-shader> decode "${case_dir}/t707_exec_ff_lanes_4_to_7.json"
    EXPECT_OUTPUT "0002: +bf880004 +s_cbranch_execz 0x7 \\(simm16 \\+4\\)"
    LABELS integration)
scale_add_command_test(NAME shader_cli_decode_hex_words
    COMMAND $<TARGET_FILE:scale-shader> decode "0xe0301000 80000201" bf810000
    EXPECT_OUTPUT "0002: +bf810000 +s_endpgm"
    LABELS integration)

foreach(case_name IN ITEMS t704_add_lane_to_input t707_exec_ff_lanes_4_to_7 t707_exec_0f_branch_taken)
    if(case_name MATCHES "^t704")
        set(abi 0)
        set(output "${test_dir}/${case_name} module 雪.spv")
        set(v0_module "${output}")
    else()
        set(abi 1)
        set(output "${test_dir}/${case_name} module.spv")
    endif()
    scale_add_command_test(NAME shader_cli_compile_${case_name}
        COMMAND $<TARGET_FILE:scale-shader> compile "${case_dir}/${case_name}.json" -o "${output}"
        EXPECT_OUTPUT "compiled \"[^\"]+\" \\(ABI v${abi}\\)"
        LABELS integration)
    set_tests_properties(shader_cli_compile_${case_name} PROPERTIES FIXTURES_SETUP shader_cli_${case_name}_module)

    add_test(NAME shader_cli_validate_${case_name}
        COMMAND scale-shader validate "${output}")
    set_tests_properties(shader_cli_validate_${case_name} PROPERTIES LABELS integration TIMEOUT 30
        FIXTURES_REQUIRED shader_cli_${case_name}_module SKIP_REGULAR_EXPRESSION "${validator_skip}")
endforeach()

scale_add_command_test(NAME shader_cli_initial_exec_changes_module
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${test_dir}/t707_exec_ff_lanes_4_to_7 module.spv"
        "${test_dir}/t707_exec_0f_branch_taken module.spv"
    EXPECT_EXIT 1
    LABELS integration)
set_tests_properties(shader_cli_initial_exec_changes_module PROPERTIES FIXTURES_REQUIRED
    "shader_cli_t707_exec_ff_lanes_4_to_7_module;shader_cli_t707_exec_0f_branch_taken_module")

scale_add_command_test(NAME shader_cli_validate_without_tool
    COMMAND $<TARGET_FILE:scale-shader> validate "${v0_module}"
    EXPECT_EXIT 1
    EXPECT_OUTPUT "${validator_skip}"
    LABELS integration)
set_tests_properties(shader_cli_validate_without_tool PROPERTIES FIXTURES_REQUIRED
    shader_cli_t704_add_lane_to_input_module ENVIRONMENT "PATH=${test_dir};VULKAN_SDK=")

file(WRITE "${test_dir}/malformed.json" "{\"schema_version\": 1,")
file(WRITE "${test_dir}/malformed module.spv" "bad!")
configure_file("${case_dir}/t704_add_lane_to_input.json" "${test_dir}/protected_case.json" COPYONLY)

scale_add_command_test(NAME shader_cli_rejects_malformed_json
    COMMAND $<TARGET_FILE:scale-shader> compile "${test_dir}/malformed.json" -o "${test_dir}/unused.spv"
    EXPECT_EXIT 1
    EXPECT_OUTPUT "shader/testcase_invalid_json"
    LABELS integration)
scale_add_command_test(NAME shader_cli_rejects_malformed_word
    COMMAND $<TARGET_FILE:scale-shader> decode 0x7DGG0000
    EXPECT_EXIT 2
    EXPECT_OUTPUT "shader/invalid_word"
    LABELS integration)
scale_add_command_test(NAME shader_cli_reports_truncated_encoding
    COMMAND $<TARGET_FILE:scale-shader> decode 0xe0301000
    EXPECT_EXIT 1
    EXPECT_OUTPUT "shader/truncated_encoding.*dword 0x0"
    LABELS integration)
scale_add_command_test(NAME shader_cli_rejects_bad_compile_arguments
    COMMAND $<TARGET_FILE:scale-shader> compile "${case_dir}/t704_add_lane_to_input.json"
    EXPECT_EXIT 2
    EXPECT_OUTPUT "usage: scale-shader"
    LABELS integration)
scale_add_command_test(NAME shader_cli_preserves_input_when_used_as_output
    COMMAND $<TARGET_FILE:scale-shader> compile "${test_dir}/protected_case.json" -o "${test_dir}/protected_case.json"
    EXPECT_EXIT 1
    EXPECT_OUTPUT "shader/spirv_output_is_input"
    LABELS integration)
scale_add_command_test(NAME shader_cli_validate_rejects_missing_module
    COMMAND $<TARGET_FILE:scale-shader> validate "${test_dir}/missing.spv"
    EXPECT_EXIT 1
    EXPECT_OUTPUT "shader/spirv_open_failed"
    LABELS integration)
scale_add_command_test(NAME shader_cli_validate_rejects_malformed_module
    COMMAND $<TARGET_FILE:scale-shader> validate "${test_dir}/malformed module.spv"
    EXPECT_EXIT 1
    EXPECT_OUTPUT "shader/validator_rejected"
    LABELS integration)
set_tests_properties(shader_cli_validate_rejects_malformed_module PROPERTIES
    SKIP_REGULAR_EXPRESSION "${validator_skip}")
