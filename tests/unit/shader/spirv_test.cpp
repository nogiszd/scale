#include "case_program.h"
#include "spirv_test_programs.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>

namespace {

using namespace scale::shader;
using namespace scale::shader::test;
using Word = std::uint32_t;
using Instructions = std::vector<std::span<const Word>>;

Instructions instructions(const SpirvModule& module) {
    Instructions decoded;
    const std::span words(module.words);
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> 16;
        if (count == 0 || count > words.size() - offset) {
            ADD_FAILURE() << "Invalid SPIR-V instruction length at word " << offset;
            return {};
        }
        decoded.push_back(words.subspan(offset, count));
        offset += count;
    }
    return decoded;
}

auto find_opcode(const Instructions& code, Word opcode) {
    return std::ranges::find_if(code, [=](auto inst) { return (inst.front() & 0xffffu) == opcode; });
}

bool contains(const Instructions& code, Word opcode, std::initializer_list<Word> operands) {
    return std::ranges::any_of(code, [=](auto inst) {
        return (inst.front() & 0xffffu) == opcode && std::ranges::equal(inst.subspan(1), operands);
    });
}

TEST(ShaderSpirv, EmitsT704ComputeInterfaceAndIntegerBody) {
    const auto module = compile_test_program(kAddLaneProgram);
    ASSERT_TRUE(module.has_value()) << module.error().to_string();
    EXPECT_FALSE(module->requires_float32_rte);
    ASSERT_GE(module->words.size(), 5u);
    EXPECT_EQ(module->words[0], 0x07230203u);
    EXPECT_EQ(module->words[1], 0x00010500u);
    const auto code = instructions(*module);

    const auto entry = find_opcode(code, 15);  // OpEntryPoint
    ASSERT_NE(entry, code.end());
    ASSERT_EQ(entry->size(), 8u);
    EXPECT_EQ((*entry)[1], 5u);  // GLCompute
    EXPECT_EQ((*entry)[3], 0x6e69616du);  // main
    EXPECT_EQ((*entry)[4], 0u);
    const auto function = (*entry)[2];
    const auto lane = (*entry)[5];
    const auto input = (*entry)[6];
    const auto output = (*entry)[7];
    EXPECT_TRUE(contains(code, 16, {function, 17, 32, 1, 1}));  // LocalSize
    EXPECT_TRUE(contains(code, 71, {lane, 11, 29}));  // BuiltIn LocalInvocationIndex
    EXPECT_TRUE(contains(code, 71, {input, 34, 0}));  // DescriptorSet
    EXPECT_TRUE(contains(code, 71, {input, 33, 0}));  // Binding
    EXPECT_TRUE(contains(code, 71, {output, 34, 0}));
    EXPECT_TRUE(contains(code, 71, {output, 33, 1}));

    const auto array = find_opcode(code, 28);  // OpTypeArray
    ASSERT_NE(array, code.end());
    ASSERT_EQ(array->size(), 4u);
    EXPECT_TRUE(contains(code, 71, {(*array)[1], 6, 4}));  // ArrayStride
    EXPECT_TRUE(contains(code, 43, {(*array)[2], (*array)[3], 32}));  // Array length
    const auto block = find_opcode(code, 30);  // OpTypeStruct
    ASSERT_NE(block, code.end());
    ASSERT_EQ(block->size(), 3u);
    EXPECT_TRUE(contains(code, 71, {(*block)[1], 2}));  // Block
    EXPECT_TRUE(contains(code, 72, {(*block)[1], 0, 35, 0}));  // Offset
    EXPECT_TRUE(contains(code, 72, {(*block)[1], 0, 24}));  // NonWritable
    EXPECT_TRUE(std::ranges::any_of(code, [](auto inst) {
        return inst.size() == 4 && (inst[0] & 0xffffu) == 72 && inst[2] == 0 && inst[3] == 25;  // NonReadable
    }));

    EXPECT_NE(find_opcode(code, 128), code.end());  // OpIAdd
    EXPECT_NE(find_opcode(code, 134), code.end());  // OpUDiv byte-to-element index
    EXPECT_NE(find_opcode(code, 61), code.end());  // OpLoad
    EXPECT_NE(find_opcode(code, 62), code.end());  // OpStore
    EXPECT_FALSE(contains(code, 17, {4467}));  // RoundingModeRTE capability
    EXPECT_FALSE(contains(code, 16, {function, 4462, 32}));  // RoundingModeRTE 32
}

TEST(ShaderSpirv, FloatOperationsRequestRteAndPreventContraction) {
    const auto module = compile_test_program(kFloatProgram);
    ASSERT_TRUE(module.has_value()) << module.error().to_string();
    EXPECT_TRUE(module->requires_float32_rte);
    const auto code = instructions(*module);
    const auto entry = find_opcode(code, 15);
    ASSERT_NE(entry, code.end());
    ASSERT_GE(entry->size(), 3u);
    EXPECT_TRUE(contains(code, 17, {4467}));
    EXPECT_TRUE(contains(code, 16, {(*entry)[2], 4462, 32}));
    for (const auto opcode : {129u, 133u}) {  // OpFAdd, OpFMul
        const auto op = find_opcode(code, opcode);
        ASSERT_NE(op, code.end());
        ASSERT_EQ(op->size(), 5u);
        EXPECT_TRUE(contains(code, 71, {(*op)[2], 42}));  // NoContraction
    }
    EXPECT_NE(find_opcode(code, 124), code.end());  // OpBitcast
}

TEST(ShaderSpirv, EmitsScalarCarryAndPreservesWordCopies) {
    const auto module = compile_test_program(kScalarProgram);
    ASSERT_TRUE(module.has_value()) << module.error().to_string();
    EXPECT_FALSE(module->requires_float32_rte);
    const auto code = instructions(*module);
    EXPECT_NE(find_opcode(code, 176), code.end());  // OpULessThan
    EXPECT_NE(find_opcode(code, 83), code.end());  // OpCopyObject
    EXPECT_NE(find_opcode(code, 132), code.end());  // OpIMul
}

TEST(ShaderSpirvV1, EmbedsInitialExecAndUsesWorkgroupBarrierScopes) {
    const auto shader_case = read_shader_case("t707_exec_ff_lanes_4_to_7");
    ASSERT_TRUE(shader_case.has_value()) << shader_case.error().to_string();
    ASSERT_TRUE(shader_case->v1);
    const auto decoded = decode(scale::ByteView(std::as_bytes(std::span(shader_case->code_words))));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();
    const auto ir = lower_ir_v1(*decoded, shader_case->v1->initial_exec);
    ASSERT_TRUE(ir.has_value()) << ir.error().to_string();
    const auto module = emit_spirv(*ir);
    ASSERT_TRUE(module.has_value()) << module.error().to_string();
    EXPECT_FALSE(module->requires_float32_rte);
    const auto code = instructions(*module);

    const auto entry = find_opcode(code, 15);
    ASSERT_NE(entry, code.end());
    ASSERT_EQ(entry->size(), 9u);  // Lane index, two buffers, and the shared scratch array.
    const auto scratch = (*entry)[8];
    EXPECT_TRUE(std::ranges::any_of(code, [=](auto inst) {
        return inst.size() == 4 && (inst[0] & 0xffffu) == 59 && inst[2] == scratch && inst[3] == 4;
    }));
    EXPECT_EQ(std::ranges::count_if(code, [](auto inst) {
        return inst.size() == 4 && (inst[0] & 0xffffu) == 71 && inst[2] == 33;  // Binding
    }), 2);
    EXPECT_NE(find_opcode(code, 246), code.end());  // OpLoopMerge
    EXPECT_NE(find_opcode(code, 247), code.end());  // OpSelectionMerge
    EXPECT_NE(find_opcode(code, 251), code.end());  // OpSwitch

    const auto integer = find_opcode(code, 21);  // OpTypeInt
    ASSERT_NE(integer, code.end());
    ASSERT_GE(integer->size(), 2u);
    const auto u32 = (*integer)[1];
    std::size_t barriers = 0;
    for (const auto inst : code) {
        if ((inst.front() & 0xffffu) != 224) continue;  // OpControlBarrier
        ASSERT_EQ(inst.size(), 4u);
        EXPECT_TRUE(contains(code, 43, {u32, inst[1], 2}));  // Workgroup execution scope
        EXPECT_TRUE(contains(code, 43, {u32, inst[2], 2}));  // Workgroup memory scope
        EXPECT_TRUE(contains(code, 43, {u32, inst[3], 0x108}));  // AcquireRelease | WorkgroupMemory
        ++barriers;
    }
    EXPECT_EQ(barriers, 2u);

    const auto zero_exec = lower_ir_v1(*decoded, 0);
    ASSERT_TRUE(zero_exec.has_value()) << zero_exec.error().to_string();
    const auto zero_module = emit_spirv(*zero_exec);
    ASSERT_TRUE(zero_module.has_value()) << zero_module.error().to_string();
    EXPECT_NE(zero_module->words, module->words);
}

TEST(ShaderSpirvV1Reject, RejectsTooManyDispatcherCases) {
    std::vector<Word> words(16'383, 0xbf820000);  // Each branch targets the following instruction.
    words.insert(words.end(), {0xe0701000, 0x80010001, 0xbf810000});  // Store v0 and end.
    const auto decoded = decode(scale::ByteView(std::as_bytes(std::span(words))));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();
    const auto ir = lower_ir_v1(*decoded, 0xffffffffu);
    ASSERT_TRUE(ir.has_value()) << ir.error().to_string();
    ASSERT_EQ(ir->blocks().size(), 16'384u);
    const auto module = emit_spirv(*ir);
    ASSERT_FALSE(module.has_value());
    EXPECT_EQ(module.error().code, "spirv_limit_exceeded");
}

void expect_failure(const IrProgram& program, std::string_view code) {
    const auto result = emit_spirv(program);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().component, "shader");
    EXPECT_EQ(result.error().code, code);
}

TEST(ShaderSpirvReject, RejectsPartialExecAndUnexpectedMaskReferences) {
    auto ir = lower_test_program(kAddLaneProgram);
    ASSERT_TRUE(ir.has_value()) << ir.error().to_string();
    auto& exec = ir->instructions.at(ir->exec);
    exec.constant_bits = 0xffffu;
    expect_failure(*ir, "unsupported_exec");

    exec.constant_bits = 0xffffffffu;
    ir->instructions[ir->instructions.size() - 2].exec = ir->vcc;
    expect_failure(*ir, "unsupported_exec");
}

TEST(ShaderSpirvReject, RejectsInvalidReferencesAndNonAbiBufferOffsets) {
    auto ir = lower_test_program(kAddLaneProgram);
    ASSERT_TRUE(ir.has_value()) << ir.error().to_string();
    auto& store = ir->instructions[ir->instructions.size() - 2];
    store.inputs[0] = ir->instructions.size();
    expect_failure(*ir, "invalid_ir");
    store.inputs[0] = store.inputs[1];
    expect_failure(*ir, "invalid_ir");
}

TEST(ShaderSpirvReject, RejectsUseOfUnwaitedLoads) {
    auto ir = lower_test_program(kAddLaneProgram);
    ASSERT_TRUE(ir.has_value()) << ir.error().to_string();
    const auto wait = std::ranges::find_if(ir->instructions, [](const auto& inst) {
        return inst.opcode == IrOpcode::wait_vmem;
    });
    ASSERT_NE(wait, ir->instructions.end());
    wait->opcode = IrOpcode::constant;
    wait->result = IrValueType{IrType::u32, IrLanes::uniform};
    expect_failure(*ir, "invalid_ir");
}

TEST(ShaderSpirvReject, RejectsUnknownIrOpcode) {
    auto ir = lower_test_program(kAddLaneProgram);
    ASSERT_TRUE(ir.has_value()) << ir.error().to_string();
    const auto add = std::ranges::find_if(ir->instructions, [](const auto& inst) { return inst.opcode == IrOpcode::uadd; });
    ASSERT_NE(add, ir->instructions.end());
    add->opcode = static_cast<IrOpcode>(0xffu);
    expect_failure(*ir, "unsupported_ir_opcode");
}

}  // namespace
