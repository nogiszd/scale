#include "case_program.h"
#include "run_tool.h"
#include "spirv_test_programs.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

using namespace scale::shader::test;
using namespace scale::shader;

void validate(const SpirvModule& module, std::string_view name) {
    constexpr std::string_view tool_path = SCALE_SHADER_SPIRV_VAL;
    if (tool_path.empty()) GTEST_SKIP() << "spirv-val was unavailable at configure time";
    const auto tool = utf8_path(tool_path);
    if (!std::filesystem::exists(tool)) GTEST_SKIP() << "spirv-val is unavailable at " << tool_path;

    const auto path = utf8_path(SCALE_SHADER_SPIRV_TEST_DIR) / name;
    const auto bytes = std::as_bytes(std::span(module.words));
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

void validate(std::span<const std::uint32_t> code, std::string_view name) {
    const auto module = compile_test_program(code);
    ASSERT_TRUE(module.has_value()) << module.error().to_string();
    validate(*module, name);
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

class ShaderSpirvV1Validation : public testing::TestWithParam<std::string_view> {};

TEST_P(ShaderSpirvV1Validation, ValidatesAbiV1TestCase) {
    const auto shader_case = read_shader_case(GetParam());
    ASSERT_TRUE(shader_case.has_value()) << shader_case.error().to_string();
    ASSERT_TRUE(shader_case->v1);
    const auto decoded = decode(scale::ByteView(std::as_bytes(std::span(shader_case->code_words))));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();
    const auto ir = lower_ir_v1(*decoded, shader_case->v1->initial_exec);
    ASSERT_TRUE(ir.has_value()) << ir.error().to_string();
    const auto module = emit_spirv(*ir);
    ASSERT_TRUE(module.has_value()) << module.error().to_string();
    EXPECT_EQ(module->requires_float32_rte, shader_case->output_type == ShaderTestOutputType::f32);
    validate(*module, shader_case->name + ".spv");
}

INSTANTIATE_TEST_SUITE_P(AbiV1, ShaderSpirvV1Validation,
    testing::Values(std::string_view{"t707_exec_ff_lanes_4_to_7"}, std::string_view{"t707_exec_0f_branch_taken"},
                    std::string_view{"v1_scc_and_unconditional_branches"}, std::string_view{"v1_exec_restore_compare"},
                    std::string_view{"v1_zero_exec_f32"}),
    [](const auto& info) { return std::string(info.param); });

}  // namespace
