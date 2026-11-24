#include "case_program.h"
#include "scale/shader/ir.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>

namespace {

using namespace scale::shader;

constexpr std::uint32_t kLoadV2 = 0xe0301000;
constexpr std::uint32_t kLoadV2Operands = 0x80000201;
constexpr std::uint32_t kWait = 0xbf8c3f70;
constexpr std::uint32_t kStore = 0xe0701000;
constexpr std::uint32_t kStoreV0Operands = 0x80010001;
constexpr std::uint32_t kEnd = 0xbf810000;

scale::Result<IrProgram> lower_words(std::span<const std::uint32_t> words) {
    const auto decoded = decode(scale::ByteView(std::as_bytes(words)));
    if (!decoded) return std::unexpected(decoded.error());
    return lower_ir(*decoded);
}

const IrInstruction* find(const IrProgram& program, std::size_t offset, IrOpcode opcode) {
    const auto found = std::ranges::find_if(program.instructions, [=](const IrInstruction& instruction) {
        return instruction.dword_offset == offset && instruction.opcode == opcode;
    });
    return found != program.instructions.end() ? &*found : nullptr;
}

void expect_type(const IrInstruction& instruction, IrType type, IrLanes lanes) {
    EXPECT_EQ(instruction.result, (IrValueType{type, lanes}));
}

void expect_constant(const IrInstruction& instruction, IrType type, std::uint32_t bits) {
    EXPECT_EQ(instruction.opcode, IrOpcode::constant);
    expect_type(instruction, type, IrLanes::uniform);
    EXPECT_EQ(instruction.constant_bits, bits);
    EXPECT_FALSE(instruction.exec);
}

TEST(ShaderIr, LowersT704ExampleWithExplicitExecAndBufferAddress) {
    // The exact instruction stream from docs/contracts/shader_abi.md.
    constexpr std::uint32_t words[]{
        kLoadV2, kLoadV2Operands, kWait, 0x4a040500, kStore, 0x80010201, kEnd,
    };
    const auto program = lower_words(words);
    ASSERT_TRUE(program.has_value()) << program.error().to_string();
    const auto& instructions = program->instructions;
    expect_constant(instructions.at(program->exec), IrType::mask32, 0xffffffffu);
    expect_constant(instructions.at(program->vcc), IrType::mask32, 0);
    expect_constant(instructions.at(program->entry_scc), IrType::flag, 0);
    EXPECT_EQ(program->exit_scc, program->entry_scc);

    const auto* load = find(*program, 0, IrOpcode::load_input);
    const auto* wait = find(*program, 2, IrOpcode::wait_vmem);
    const auto* add = find(*program, 3, IrOpcode::uadd);
    const auto* store = find(*program, 4, IrOpcode::store_output);
    const auto* end = find(*program, 6, IrOpcode::end);
    ASSERT_NE(load, nullptr);
    ASSERT_NE(wait, nullptr);
    ASSERT_NE(add, nullptr);
    ASSERT_NE(store, nullptr);
    ASSERT_NE(end, nullptr);
    EXPECT_EQ(end, &instructions.back());

    const auto& byte_offset = instructions.at(load->inputs[0]);
    EXPECT_EQ(byte_offset.opcode, IrOpcode::umul);
    EXPECT_FALSE(byte_offset.dword_offset);
    expect_type(byte_offset, IrType::u32, IrLanes::per_lane);
    const auto& lane = instructions.at(byte_offset.inputs[0]);
    EXPECT_EQ(lane.opcode, IrOpcode::lane_index);
    expect_type(lane, IrType::u32, IrLanes::per_lane);
    expect_constant(instructions.at(byte_offset.inputs[1]), IrType::u32, 4);

    expect_type(*load, IrType::u32, IrLanes::per_lane);
    expect_type(*add, IrType::u32, IrLanes::per_lane);
    EXPECT_EQ(load->input_count, 1);
    EXPECT_EQ(add->input_count, 2);
    EXPECT_EQ(&instructions.at(add->inputs[0]), &lane);
    EXPECT_EQ(&instructions.at(add->inputs[1]), load);
    EXPECT_EQ(store->input_count, 2);
    EXPECT_EQ(&instructions.at(store->inputs[0]), &byte_offset);
    EXPECT_EQ(&instructions.at(store->inputs[1]), add);
    EXPECT_EQ(load->exec, program->exec);
    EXPECT_EQ(add->exec, program->exec);
    EXPECT_EQ(store->exec, program->exec);
    EXPECT_FALSE(wait->result);
    EXPECT_FALSE(wait->exec);
    EXPECT_FALSE(store->result);
    EXPECT_FALSE(end->result);
}

TEST(ShaderIr, TracksScalarCarryAndBroadcastsUniformOperandsForVectorWrites) {
    constexpr std::uint32_t words[]{
        0xb008ffff,              // s_movk_i32 s8, -1
        0xbe890381,              // s_mov_b32 s9, 1
        0x800a0908,              // s_add_u32 s10, s8, s9
        0x7e04020a,              // v_mov_b32_e32 v2, s10
        0x4a060409,              // v_add_nc_u32_e32 v3, s9, v2
        0xd5690004, 0x02001409,  // v_mul_lo_u32 v4, s9, s10
        kStore, 0x80010401, kEnd,
    };
    const auto program = lower_words(words);
    ASSERT_TRUE(program.has_value()) << program.error().to_string();
    const auto& instructions = program->instructions;
    const auto* movk = find(*program, 0, IrOpcode::copy);
    const auto* smov = find(*program, 1, IrOpcode::copy);
    const auto* sum = find(*program, 2, IrOpcode::uadd);
    const auto* vmov = find(*program, 3, IrOpcode::copy);
    const auto* vadd = find(*program, 4, IrOpcode::uadd);
    const auto* vmul = find(*program, 5, IrOpcode::umul);
    const auto* store = find(*program, 7, IrOpcode::store_output);
    ASSERT_NE(movk, nullptr);
    ASSERT_NE(smov, nullptr);
    ASSERT_NE(sum, nullptr);
    ASSERT_NE(vmov, nullptr);
    ASSERT_NE(vadd, nullptr);
    ASSERT_NE(vmul, nullptr);
    ASSERT_NE(store, nullptr);
    expect_constant(instructions.at(movk->inputs[0]), IrType::u32, 0xffffffffu);
    expect_constant(instructions.at(smov->inputs[0]), IrType::u32, 1);
    expect_type(*movk, IrType::u32, IrLanes::uniform);
    expect_type(*smov, IrType::u32, IrLanes::uniform);
    expect_type(*sum, IrType::u32, IrLanes::uniform);
    EXPECT_FALSE(sum->exec);
    EXPECT_EQ(&instructions.at(sum->inputs[0]), movk);
    EXPECT_EQ(&instructions.at(sum->inputs[1]), smov);

    const auto& carry = instructions.at(program->exit_scc);
    EXPECT_EQ(carry.opcode, IrOpcode::uadd_carry);
    EXPECT_EQ(carry.dword_offset, 2u);
    expect_type(carry, IrType::flag, IrLanes::uniform);
    EXPECT_FALSE(carry.exec);
    EXPECT_EQ(&instructions.at(carry.inputs[0]), sum);
    EXPECT_EQ(&instructions.at(carry.inputs[1]), movk);
    expect_constant(instructions.at(program->vcc), IrType::mask32, 0);

    for (const auto* vector : {vmov, vadd, vmul}) {
        expect_type(*vector, IrType::u32, IrLanes::per_lane);
        EXPECT_EQ(vector->exec, program->exec);
    }
    EXPECT_EQ(&instructions.at(vmov->inputs[0]), sum);
    EXPECT_EQ(&instructions.at(vadd->inputs[1]), vmov);
    EXPECT_EQ(&instructions.at(vmul->inputs[0]), smov);
    EXPECT_EQ(&instructions.at(vmul->inputs[1]), sum);
    EXPECT_EQ(&instructions.at(store->inputs[1]), vmul);
}

TEST(ShaderIr, BitcastsRawWordsAroundTypedFloatOperations) {
    constexpr std::uint32_t words[]{
        kLoadV2, kLoadV2Operands, kWait,
        0x060604f0,              // v_add_f32_e32 v3, 0.5, v2
        0xd5080004, 0x0201e903,  // v_mul_f32_e64 v4, v3, 2.0
        kStore, 0x80010401, kEnd,
    };
    const auto program = lower_words(words);
    ASSERT_TRUE(program.has_value()) << program.error().to_string();
    const auto& instructions = program->instructions;
    const auto* load = find(*program, 0, IrOpcode::load_input);
    const auto* add = find(*program, 3, IrOpcode::fadd);
    const auto* mul = find(*program, 4, IrOpcode::fmul);
    const auto* store = find(*program, 6, IrOpcode::store_output);
    ASSERT_NE(load, nullptr);
    ASSERT_NE(add, nullptr);
    ASSERT_NE(mul, nullptr);
    ASSERT_NE(store, nullptr);

    const auto& half = instructions.at(add->inputs[0]);
    EXPECT_EQ(half.opcode, IrOpcode::bitcast);
    expect_type(half, IrType::f32, IrLanes::uniform);
    expect_constant(instructions.at(half.inputs[0]), IrType::u32, 0x3f000000);
    const auto& loaded_float = instructions.at(add->inputs[1]);
    EXPECT_EQ(loaded_float.opcode, IrOpcode::bitcast);
    expect_type(loaded_float, IrType::f32, IrLanes::per_lane);
    EXPECT_EQ(&instructions.at(loaded_float.inputs[0]), load);

    expect_type(*add, IrType::f32, IrLanes::per_lane);
    expect_type(*mul, IrType::f32, IrLanes::per_lane);
    EXPECT_EQ(add->exec, program->exec);
    EXPECT_EQ(mul->exec, program->exec);
    EXPECT_EQ(&instructions.at(mul->inputs[0]), add);
    const auto& stored_bits = instructions.at(store->inputs[1]);
    EXPECT_EQ(stored_bits.opcode, IrOpcode::bitcast);
    expect_type(stored_bits, IrType::u32, IrLanes::per_lane);
    EXPECT_EQ(&instructions.at(stored_bits.inputs[0]), mul);
}

TEST(ShaderIr, OneWaitMakesAllEarlierLoadsAvailable) {
    constexpr std::uint32_t words[]{
        kLoadV2, kLoadV2Operands, kLoadV2, 0x80000301, kWait,
        0x4a080702,  // v_add_nc_u32_e32 v4, v2, v3
        kStore, 0x80010401, kEnd,
    };
    const auto program = lower_words(words);
    ASSERT_TRUE(program.has_value()) << program.error().to_string();
    const auto* sum = find(*program, 5, IrOpcode::uadd);
    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(&program->instructions.at(sum->inputs[0]), find(*program, 0, IrOpcode::load_input));
    EXPECT_EQ(&program->instructions.at(sum->inputs[1]), find(*program, 2, IrOpcode::load_input));
}

void expect_lowering_error(std::initializer_list<std::uint32_t> words, std::string_view code) {
    const auto decoded = decode(scale::ByteView(std::as_bytes(std::span(words))));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();
    const auto result = lower_ir(*decoded);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().component, "shader");
    EXPECT_EQ(result.error().code, code);
}

TEST(ShaderIrReject, StoreAndEndMustBeTheLastTwoInstructions) {
    expect_lowering_error({}, "invalid_program");
    expect_lowering_error({kEnd}, "invalid_program");
    expect_lowering_error({kStore, kStoreV0Operands}, "invalid_program");
    expect_lowering_error({kStore, kStoreV0Operands, kEnd, kWait}, "invalid_program");
    expect_lowering_error({kStore, kStoreV0Operands, kWait, kEnd}, "invalid_program");
    expect_lowering_error({kStore, kStoreV0Operands, kStore, kStoreV0Operands, kEnd}, "invalid_program");
}

TEST(ShaderIrReject, RequiresAbiBufferAddressingAndWaitForm) {
    expect_lowering_error({kLoadV2, 0x80010201}, "unsupported_abi");  // Load from output tuple.
    expect_lowering_error({kStore, 0x80000001}, "unsupported_abi");  // Store to input tuple.
    expect_lowering_error({kLoadV2, 0x80000200}, "unsupported_abi");  // Address in v0.
    expect_lowering_error({0xe0301004, kLoadV2Operands}, "unsupported_abi");  // Immediate offset 4.
    expect_lowering_error({kLoadV2, 0x81000201}, "unsupported_abi");  // Inline scalar offset 1.
    expect_lowering_error({kLoadV2, 0x08000201}, "unsupported_abi");  // Scalar offset from s8.
    expect_lowering_error({0xbf8c3f71}, "unsupported_abi");  // vmcnt(1).
    expect_lowering_error({0xbf8c0000}, "unsupported_abi");  // Wait on the other counters too.
}

TEST(ShaderIrReject, ProtectsTheDescriptorTuplesAndAddressRegister) {
    expect_lowering_error({0xbe880300}, "reserved_register");  // s_mov_b32 s8, s0
    expect_lowering_error({0xbe800380}, "reserved_register");  // s_mov_b32 s0, 0
    expect_lowering_error({0x7e020300}, "reserved_register");  // v_mov_b32 v1, v0
    expect_lowering_error({kLoadV2, 0x80000101}, "reserved_register");  // Load into v1.
}

TEST(ShaderIrReject, ReportsUndefinedScalarVectorAndStoreSources) {
    expect_lowering_error({0xbe880309}, "undefined_register");  // s_mov_b32 s8, s9
    expect_lowering_error({0x7e040303}, "undefined_register");  // v_mov_b32 v2, v3
    expect_lowering_error({kStore, 0x80010201, kEnd}, "undefined_register");  // Store from v2.
}

TEST(ShaderIrReject, RequiresWaitBeforeReadOverwriteAndFinalStore) {
    expect_lowering_error({kLoadV2, kLoadV2Operands, 0x4a040500}, "load_not_ready");  // Read v2.
    expect_lowering_error({kLoadV2, kLoadV2Operands, 0x7e040300}, "load_not_ready");  // ALU write to v2.
    expect_lowering_error({kLoadV2, kLoadV2Operands, kLoadV2, kLoadV2Operands}, "load_not_ready");
    expect_lowering_error({kLoadV2, kLoadV2Operands, kStore, kStoreV0Operands, kEnd}, "load_not_ready");
    expect_lowering_error({kLoadV2, kLoadV2Operands, kWait, kLoadV2, 0x80000301,
                           kStore, kStoreV0Operands, kEnd}, "load_not_ready");
}

TEST(ShaderIrReject, ReportsMalformedDecodedOperands) {
    constexpr std::uint32_t words[]{0x7e040300, kStore, kStoreV0Operands, kEnd};
    auto decoded = decode(scale::ByteView(std::as_bytes(std::span(words))));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();
    std::get<AluOperands>(decoded->front().operands).source_count = 3;
    const auto bad_arity = lower_ir(*decoded);
    ASSERT_FALSE(bad_arity.has_value());
    EXPECT_EQ(bad_arity.error().code, "invalid_decoded_instruction");
    decoded->front().operands = std::monostate{};
    const auto bad_variant = lower_ir(*decoded);
    ASSERT_FALSE(bad_variant.has_value());
    EXPECT_EQ(bad_variant.error().code, "invalid_decoded_instruction");
}

TEST(ShaderIrV1, LowersT707ToGuardedOperationsAndUniformJumps) {
    const auto shader_case = test::read_shader_case("t707_exec_ff_lanes_4_to_7");
    ASSERT_TRUE(shader_case.has_value()) << shader_case.error().to_string();
    ASSERT_TRUE(shader_case->v1);
    const auto decoded = decode(scale::ByteView(std::as_bytes(std::span(shader_case->code_words))));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();
    const auto ir = lower_ir_v1(*decoded, shader_case->v1->initial_exec);
    ASSERT_TRUE(ir.has_value()) << ir.error().to_string();
    EXPECT_EQ(ir->initial_exec(), 0xffu);
    const auto blocks = ir->blocks();
    ASSERT_EQ(blocks.size(), 3u);
    EXPECT_EQ(blocks[0].dword_offset, 0u);
    ASSERT_EQ(blocks[0].operations.size(), 2u);
    const auto& compare = blocks[0].operations[0];
    EXPECT_EQ(compare.opcode, IrV1Opcode::compare_lt);
    EXPECT_EQ(compare.lanes, IrLanes::per_lane);
    ASSERT_TRUE(compare.destination);
    EXPECT_EQ(compare.destination->kind, IrV1RegisterKind::vcc);
    const auto& save = blocks[0].operations[1];
    EXPECT_EQ(save.opcode, IrV1Opcode::save_exec);
    EXPECT_EQ(save.lanes, IrLanes::uniform);
    ASSERT_TRUE(blocks[0].jump);
    EXPECT_EQ(blocks[0].jump->condition, IrV1Condition::exec_zero);
    EXPECT_EQ(blocks[0].jump->taken, 2u);
    EXPECT_EQ(blocks[0].jump->fallthrough, 1u);
    EXPECT_EQ(blocks[1].dword_offset, 3u);
    ASSERT_TRUE(blocks[1].jump);
    EXPECT_EQ(blocks[1].jump->condition, IrV1Condition::always);
    EXPECT_EQ(blocks[1].jump->taken, 2u);
    EXPECT_EQ(blocks[2].dword_offset, 7u);
    ASSERT_EQ(blocks[2].operations.size(), 1u);
    EXPECT_EQ(blocks[2].operations[0].opcode, IrV1Opcode::store_output);
    EXPECT_EQ(blocks[2].operations[0].lanes, IrLanes::per_lane);
    EXPECT_FALSE(blocks[2].jump);
}

void expect_v1_error(std::initializer_list<std::uint32_t> words, std::string_view code) {
    const auto bytes = std::as_bytes(std::span(words.begin(), words.size()));
    const auto decoded = decode(scale::ByteView(bytes));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();
    const auto ir = lower_ir_v1(*decoded, 0);
    ASSERT_FALSE(ir.has_value());
    EXPECT_EQ(ir.error().component, "shader");
    EXPECT_EQ(ir.error().code, code);
}

TEST(ShaderIrV1Reject, RejectsInvalidBranchTargets) {
    for (const auto branch : {0xbf82ffffu, 0xbf82fffeu, 0xbf820001u, 0xbf820002u, 0xbf820010u}) {
        // Respectively self, negative, second word of store, end, and beyond the stream.
        SCOPED_TRACE(branch);
        expect_v1_error({branch, kStore, kStoreV0Operands, kEnd}, "invalid_program");
    }
}

TEST(ShaderIrV1Reject, RequiresOneStoreAndOneTerminalEnd) {
    expect_v1_error({kEnd}, "invalid_program");
    expect_v1_error({kStore, kStoreV0Operands}, "invalid_program");
    expect_v1_error({kStore, kStoreV0Operands, kStore, kStoreV0Operands, kEnd}, "invalid_program");
    expect_v1_error({kEnd, kStore, kStoreV0Operands, kEnd}, "invalid_program");
}

TEST(ShaderIrV1Reject, ChecksOperandsInUnreachableBlocks) {
    constexpr std::uint32_t jump_over_one = 0xbf820001;
    expect_v1_error({jump_over_one, 0xbf8c3f71, kStore, kStoreV0Operands, kEnd}, "unsupported_abi");
    expect_v1_error({jump_over_one, 0x7e020280, kStore, kStoreV0Operands, kEnd}, "reserved_register");
    expect_v1_error({jump_over_one, 0xbe800380, kStore, kStoreV0Operands, kEnd}, "reserved_register");
    constexpr std::uint32_t jump_over_two = 0xbf820002;
    expect_v1_error({jump_over_two, 0xe0301001, 0x80000201, kStore, kStoreV0Operands, kEnd}, "unsupported_abi");
}

TEST(ShaderIrV1Reject, RejectsInconsistentDecodedBranch) {
    constexpr std::uint32_t words[]{0xbf820000, kStore, kStoreV0Operands, kEnd};
    auto decoded = decode(scale::ByteView(std::as_bytes(std::span(words))));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();
    std::get<BranchOperands>(decoded->front().operands).simm16 = 1;
    const auto ir = lower_ir_v1(*decoded, 0);
    ASSERT_FALSE(ir.has_value());
    EXPECT_EQ(ir.error().code, "invalid_decoded_instruction");
    EXPECT_NE(ir.error().message.find("s_branch"), std::string::npos);
}

}  // namespace
