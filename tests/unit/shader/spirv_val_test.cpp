#include "spirv_test_programs.h"

#include "scale/common/text.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <process.h>
#include <string_view>

namespace {

using namespace scale::shader::test;

void validate(std::span<const std::uint32_t> code, std::wstring_view name) {
    constexpr std::string_view tool_path = SCALE_SHADER_SPIRV_VAL;
    if (tool_path.empty()) GTEST_SKIP() << "spirv-val was unavailable at configure time";
    const auto tool = scale::to_wide(tool_path);
    ASSERT_TRUE(tool.has_value()) << tool.error().to_string();
    if (!std::filesystem::exists(*tool)) GTEST_SKIP() << "spirv-val is unavailable at " << tool_path;

    const auto module = compile_test_program(code);
    ASSERT_TRUE(module.has_value()) << module.error().to_string();
    const auto directory = scale::to_wide(SCALE_SHADER_SPIRV_TEST_DIR);
    ASSERT_TRUE(directory.has_value()) << directory.error().to_string();
    const auto path = std::filesystem::path(*directory) / name;
    const auto bytes = std::as_bytes(std::span(module->words));
    std::ofstream output(path, std::ios::binary);
    ASSERT_TRUE(output.is_open()) << path;
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.close();
    ASSERT_TRUE(output.good()) << path;

    // _wspawnv invokes the SDK tool directly. Quote Windows arguments with
    // spaces and let the enclosing CTest case enforce the process timeout.
    const auto argv0 = L"\"" + *tool + L"\"";
    const auto module_arg = L"\"" + path.native() + L"\"";
    const wchar_t* const args[]{argv0.c_str(), L"--target-env", L"vulkan1.2", module_arg.c_str(), nullptr};
    const auto status = _wspawnv(_P_WAIT, tool->c_str(), args);
    const auto spawn_error = errno;
    ASSERT_NE(status, -1) << "Could not start spirv-val (errno " << spawn_error << ')';
    EXPECT_EQ(status, 0) << path;
}

TEST(ShaderSpirvValidation, T704IntegerProgram) {
    validate(kAddLaneProgram, L"t703_add_lane.spv");
}

TEST(ShaderSpirvValidation, FloatArithmeticProgram) {
    validate(kFloatProgram, L"t703_float.spv");
}

TEST(ShaderSpirvValidation, ScalarCarryAndVectorCopyProgram) {
    validate(kScalarProgram, L"t703_scalar.spv");
}

}  // namespace
