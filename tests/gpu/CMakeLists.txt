# GPU tests (label gpu). Built with -DSCALE_ENABLE_GRAPHICS=ON.
# See docs/contracts/gpu_targets.md: every test runs on every available device.

# --- Vulkan compute harness (T-601) -------------------------------------------
find_program(SCALE_GLSLANG_VALIDATOR glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin" REQUIRED)
set(_scale3_spv "${CMAKE_CURRENT_BINARY_DIR}/scale3_plus_index.spv")
add_custom_command(
    OUTPUT "${_scale3_spv}"
    COMMAND "${SCALE_GLSLANG_VALIDATOR}" -V --target-env vulkan1.2
            -o "${_scale3_spv}" "${CMAKE_CURRENT_SOURCE_DIR}/scale3_plus_index.comp"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scale3_plus_index.comp"
    COMMENT "glslangValidator scale3_plus_index.comp")
add_custom_target(scale_gpu_test_shaders DEPENDS "${_scale3_spv}")

scale_add_test(NAME gpu_vulkan_compute
    SOURCES vulkan_compute_test.cpp
    LIBS scale::graphics scale::common
    LABELS gpu
    TIMEOUT 120)
add_dependencies(gpu_vulkan_compute scale_gpu_test_shaders)
target_compile_definitions(gpu_vulkan_compute PRIVATE "SCALE_TEST_SPIRV_SCALE3=\"${_scale3_spv}\"")

# --- Differential runner: interpreter vs GPU on JSON test cases (T-720, T-721) --
if(TARGET scale_shader_spirv AND TARGET scale_shader_ref AND TARGET scale_shader_testcase)
    scale_add_test(NAME gpu_shader_differential
        SOURCES differential_test.cpp
        LIBS scale::graphics scale::shader_spirv scale::shader_ir scale::shader_decode scale::shader_ref
             scale::shader_testcase
        LABELS gpu
        TIMEOUT 120)
    target_compile_definitions(gpu_shader_differential PRIVATE
        "SCALE_TEST_SHADER_CASES=\"${PROJECT_SOURCE_DIR}/tests/shader/cases\"")
endif()
