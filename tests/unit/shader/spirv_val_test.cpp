#include "run_tool.h"
#include "spirv_test_programs.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

using namespace scale::shader::test;

void validate(std::span<const std::uint32_t> code, std::string_view name) {
    constexpr std::string_view tool_path = SCALE_SHADER_SPIRV_VAL;
    if (tool_path.empty()) GTEST_SKIP() << "spirv-val was unavailable at configure time";
    const std::filesystem::path tool(std::u8string(tool_path.begin(), tool_path.end()));
    if (!std::filesystem::exists(tool)) GTEST_SKIP() << "spirv-val is unavailable at " << tool_path;

    const auto module = compile_test_program(code);
    ASSERT_TRUE(module.has_value()) << module.error().to_string();
    constexpr std::string_view test_dir = SCALE_SHADER_SPIRV_TEST_DIR;
    const std::filesystem::path directory(std::u8string(test_dir.begin(), test_dir.end()));
    const auto path = directory / name;
    const auto bytes = std::as_bytes(std::span(module->words));
    std::ofstream output(path, std::ios::binary);
    ASSERT_TRUE(output.is_open()) << path;
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.close();
    ASSERT_TRUE(output.good()) << path;

    const auto utf8_path = path.u8string();
    const std::string module_arg(utf8_path.begin(), utf8_path.end());
    const auto result = scale::test::run_tool(tool, {"--target-env", "vulkan1.2", module_arg});
    ASSERT_TRUE(result.has_value()) << "Could not run spirv-val at " << tool;
    EXPECT_EQ(result->exit_code, 0) << path << '\n' << result->output;
}

TEST(ShaderSpirvValidation, T704IntegerProgram) {
    validate(kAddLaneProgram, "t703_add_lane.spv");
}

TEST(ShaderSpirvValidation, FloatArithmeticProgram) {
    validate(kFloatProgram, "t703_float.spv");
}

TEST(ShaderSpirvValidation, ScalarCarryAndVectorCopyProgram) {
    validate(kScalarProgram, "t703_scalar.spv");
}

}  // namespace
