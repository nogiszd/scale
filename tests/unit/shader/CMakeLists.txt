scale_add_test(NAME unit_shader_decode
    SOURCES decode_test.cpp
    LIBS scale::shader_decode
    LABELS quick)

# --- Shader IR v0 (T-702) ---------------------------------------------------
scale_add_test(NAME unit_shader_ir
    SOURCES ir_test.cpp
    LIBS scale::shader_ir
    LABELS quick)

scale_add_test(NAME integration_shader_llvm
    SOURCES decode_llvm_test.cpp
    LIBS scale::shader_decode
    LABELS integration)

find_program(SCALE_SHADER_LLVM_MC llvm-mc NO_CACHE)
find_program(SCALE_SHADER_LLVM_OBJDUMP llvm-objdump NO_CACHE)

# Obtain independent reference output at configure time using direct process
# invocations. The GoogleTest cases compare the listings against the decoder.
set(llvm_reference_dir "")
if(EXISTS "${SCALE_SHADER_LLVM_MC}" AND EXISTS "${SCALE_SHADER_LLVM_OBJDUMP}")
    execute_process(COMMAND "${SCALE_SHADER_LLVM_MC}" --version
        OUTPUT_VARIABLE llvm_mc_version RESULT_VARIABLE llvm_mc_status TIMEOUT 10)
    execute_process(COMMAND "${SCALE_SHADER_LLVM_OBJDUMP}" --version
        OUTPUT_VARIABLE llvm_objdump_version RESULT_VARIABLE llvm_objdump_status TIMEOUT 10)
    set(amdgpu_target "(^|\n)[ \t]+(amdgpu|amdgcn)[ \t]+-")
    if(llvm_mc_status STREQUAL "0" AND llvm_objdump_status STREQUAL "0" AND
            llvm_mc_version MATCHES "${amdgpu_target}" AND llvm_objdump_version MATCHES "${amdgpu_target}")
        set(llvm_reference_dir "${CMAKE_CURRENT_BINARY_DIR}")
        foreach(test_program IN ITEMS add_lane_to_input decoder_subset)
            set(assembly "${PROJECT_SOURCE_DIR}/tests/shader/${test_program}.s")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${assembly}")
            set(object "${llvm_reference_dir}/${test_program}.o")
            execute_process(COMMAND "${SCALE_SHADER_LLVM_MC}" -triple=amdgcn -mcpu=gfx1030
                -filetype=obj -o "${object}" "${assembly}"
                RESULT_VARIABLE assemble_status ERROR_VARIABLE assemble_error TIMEOUT 10)
            if(assemble_status STREQUAL "0")
                execute_process(COMMAND "${SCALE_SHADER_LLVM_OBJDUMP}" --disassemble --mcpu=gfx1030 "${object}"
                    RESULT_VARIABLE disassemble_status OUTPUT_VARIABLE disassembly
                    ERROR_VARIABLE disassemble_error TIMEOUT 10)
                if(NOT disassemble_status STREQUAL "0")
                    set(disassembly "llvm-objdump failed (${disassemble_status}): ${disassemble_error}")
                endif()
            else()
                set(disassembly "llvm-mc failed (${assemble_status}): ${assemble_error}")
            endif()
            file(WRITE "${llvm_reference_dir}/${test_program}.disasm" "${disassembly}")
        endforeach()
        string(REGEX MATCH "LLVM version [^\r\n]+" mc_version_line "${llvm_mc_version}")
        string(REGEX MATCH "LLVM version [^\r\n]+" objdump_version_line "${llvm_objdump_version}")
        message(STATUS "Shader reference: llvm-mc ${mc_version_line}, llvm-objdump ${objdump_version_line}")
    endif()
endif()
if(NOT llvm_reference_dir)
    message(STATUS "AMDGPU llvm-mc and llvm-objdump are unavailable: shader comparisons will be SKIPPED")
endif()
target_compile_definitions(integration_shader_llvm PRIVATE
    "SCALE_SHADER_LLVM_REFERENCE_DIR=\"${llvm_reference_dir}\"")

# --- Direct SPIR-V emission and validation (T-703) ----------------------------
scale_add_test(NAME unit_shader_spirv
    SOURCES spirv_test.cpp
    LIBS scale::shader_spirv
    LABELS quick)

scale_add_test(NAME integration_shader_spirv_val
    SOURCES spirv_val_test.cpp
    LIBS scale::shader_spirv
    LABELS integration)

find_program(SCALE_SHADER_SPIRV_VAL spirv-val HINTS "$ENV{VULKAN_SDK}/Bin" NO_CACHE)
if(EXISTS "${SCALE_SHADER_SPIRV_VAL}")
    message(STATUS "spirv-val for shader tests: ${SCALE_SHADER_SPIRV_VAL}")
else()
    set(SCALE_SHADER_SPIRV_VAL "")
    message(STATUS "spirv-val unavailable: shader validation tests will be SKIPPED")
endif()
target_compile_definitions(integration_shader_spirv_val PRIVATE
    "SCALE_SHADER_SPIRV_VAL=\"${SCALE_SHADER_SPIRV_VAL}\""
    "SCALE_SHADER_SPIRV_TEST_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\"")
