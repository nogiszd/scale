// T-711: independent reference interpreter. Instruction words come from
// llvm-mc 23.1.6 (-triple=amdgcn -mcpu=gfx1030 -show-encoding). Every
// expected value is computed by hand from the RDNA 2 ISA and ABI v0 rules,
// not from either implementation.

#include "scale/shader_ref/interpreter.h"

#include <gtest/gtest.h>

#include <bit>
#include <initializer_list>
#include <span>
#include <vector>

using namespace scale::shader_ref;
using scale::shader::DecodedInstruction;

namespace {

// --- llvm-mc encodings (little-endian dwords) ----------------------------------
constexpr std::uint32_t kSMovS8Neg1 = 0xBE8803C1;    // s_mov_b32 s8, -1
constexpr std::uint32_t kSAddS9S8One = 0x80098108;   // s_add_u32 s9, s8, 1
constexpr std::uint32_t kSAddS11OneTwo = 0x800B8281; // s_add_u32 s11, 1, 2
constexpr std::uint32_t kSMovkS10 = 0xB00A8000;      // s_movk_i32 s10, 0x8000
constexpr std::uint32_t kSMovS12Neg16 = 0xBE8C03D0;  // s_mov_b32 s12, -16
constexpr std::uint32_t kSMovS13Half = 0xBE8D03F0;   // s_mov_b32 s13, 0.5
constexpr std::uint32_t kSMovS14InvTwoPi = 0xBE8E03F8;  // s_mov_b32 s14, 1/(2*pi)
constexpr std::uint32_t kSMovS8S0 = 0xBE880300;      // s_mov_b32 s8, s0
constexpr std::uint32_t kVMovV2S9 = 0x7E040209;      // v_mov_b32_e32 v2, s9
constexpr std::uint32_t kVMovV3V0 = 0x7E060300;      // v_mov_b32_e32 v3, v0
constexpr std::uint32_t kVMovV1V0 = 0x7E020300;      // v_mov_b32_e32 v1, v0
constexpr std::uint32_t kVMovV3V9 = 0x7E060309;      // v_mov_b32_e32 v3, v9
constexpr std::uint32_t kVMovV2V0 = 0x7E040300;      // v_mov_b32_e32 v2, v0
constexpr std::uint32_t kVAddV4Neg1V0 = 0x4A0800C1;  // v_add_nc_u32_e32 v4, -1, v0
constexpr std::uint32_t kVAddV2V0V2 = 0x4A040500;    // v_add_nc_u32_e32 v2, v0, v2
constexpr std::uint32_t kVMulLoV5[] = {0xD5690005, 0x0201FF00, 0x00010000};  // v_mul_lo_u32 v5, v0, 0x10000
constexpr std::uint32_t kVMulLoV6[] = {0xD5690006, 0x0201FF05, 0x00010000};  // v_mul_lo_u32 v6, v5, 0x10000
constexpr std::uint32_t kVAddF32V7 = 0x060E10F2;     // v_add_f32_e32 v7, 1.0, v8
constexpr std::uint32_t kVMulF32V9 = 0x101210F7;     // v_mul_f32_e32 v9, -4.0, v8
constexpr std::uint32_t kVAddF32V10[] = {0xD503000A, 0x0202100F};  // v_add_f32_e64 v10, s15, v8
constexpr std::uint32_t kVMovV11[] = {0xD581000B, 0x020100C0};    // v_mov_b32_e64 v11, 64
constexpr std::uint32_t kLoadV2[] = {0xE0301000, 0x80000201};     // buffer_load_dword v2, v1, s[0:3], 0 offen
constexpr std::uint32_t kStoreV2[] = {0xE0701000, 0x80010201};    // buffer_store_dword v2, v1, s[4:7], 0 offen
constexpr std::uint32_t kWaitVm0 = 0xBF8C3F70;       // s_waitcnt vmcnt(0)
constexpr std::uint32_t kWaitLgkm0 = 0xBF8CC07F;     // s_waitcnt lgkmcnt(0)
constexpr std::uint32_t kEndpgm = 0xBF810000;        // s_endpgm

std::vector<DecodedInstruction> decode_words(std::initializer_list<std::span<const std::uint32_t>> parts) {
    std::vector<std::uint32_t> words;
    for (auto p : parts) words.insert(words.end(), p.begin(), p.end());
    auto r = scale::shader::decode(scale::ByteView(std::as_bytes(std::span(words))));
    EXPECT_TRUE(r.has_value()) << r.error().to_string();
    return r ? *r : std::vector<DecodedInstruction>{};
}

std::span<const std::uint32_t> one(const std::uint32_t& w) {
    return {&w, 1};
}

DecodedInstruction decode_one(std::initializer_list<std::span<const std::uint32_t>> parts) {
    auto v = decode_words(parts);
    EXPECT_EQ(v.size(), 1u);
    return v.empty() ? DecodedInstruction{} : v[0];
}

// Steps all instructions; asserts success.
Interpreter run_steps(WaveState state, const std::vector<DecodedInstruction>& program, Buffers buffers = {}) {
    Interpreter interp(state, buffers);
    for (const auto& inst : program) {
        const auto r = interp.step(inst);
        EXPECT_TRUE(r.has_value()) << r.error().to_string();
    }
    return interp;
}

std::string step_error(WaveState state, const std::vector<DecodedInstruction>& program, Buffers buffers = {}) {
    Interpreter interp(state, buffers);
    for (const auto& inst : program) {
        const auto r = interp.step(inst);
        if (!r) {
            EXPECT_EQ(r.error().component, "shader_ref");
            return r.error().code;
        }
    }
    return "no error";
}

constexpr std::uint32_t f32(float f) {
    return std::bit_cast<std::uint32_t>(f);
}

}  // namespace

// ---- Entry state ----------------------------------------------------------------------

TEST(ShaderRefEntry, AbiV0EntryState) {
    const WaveState s = abi_v0_entry_state();
    EXPECT_EQ(s.exec, 0xFFFFFFFFu);
    EXPECT_EQ(s.vcc, 0u);
    EXPECT_FALSE(s.scc);
    EXPECT_EQ(s.vgpr[0][31], 31u);
    EXPECT_EQ(s.vgpr[1][31], 124u);
    EXPECT_EQ(s.vgpr_defined_lanes[2], 0u);
    EXPECT_FALSE(s.sgpr_defined.any());
}

// ---- Scalar ALU -------------------------------------------------------------------------

TEST(ShaderRefScalar, MovAddCarryMovkAndInlineConstants) {
    const auto i = run_steps(abi_v0_entry_state(),
                             decode_words({one(kSMovS8Neg1), one(kSAddS9S8One), one(kSMovkS10), one(kSMovS12Neg16),
                                           one(kSMovS13Half), one(kSMovS14InvTwoPi)}));
    const auto& s = i.state();
    EXPECT_EQ(s.sgpr[8], 0xFFFFFFFFu);
    EXPECT_EQ(s.sgpr[9], 0u);          // 0xFFFFFFFF + 1 wraps ...
    EXPECT_TRUE(s.scc);                // ... and sets SCC (carry out)
    EXPECT_EQ(s.sgpr[10], 0xFFFF8000u);  // sign-extended 16-bit immediate
    EXPECT_EQ(s.sgpr[12], 0xFFFFFFF0u);  // inline -16
    EXPECT_EQ(s.sgpr[13], 0x3F000000u);  // inline 0.5
    EXPECT_EQ(s.sgpr[14], 0x3E22F983u);  // inline 1/(2*pi)
}

TEST(ShaderRefScalar, AddWithoutCarryClearsScc) {
    WaveState s = abi_v0_entry_state();
    s.scc = true;
    const auto i = run_steps(s, decode_words({one(kSAddS11OneTwo)}));
    EXPECT_EQ(i.state().sgpr[11], 3u);
    EXPECT_FALSE(i.state().scc);
}

// ---- Vector integer ALU ---------------------------------------------------------------

TEST(ShaderRefVector, MovAddWrapAndMulLo) {
    const auto i = run_steps(abi_v0_entry_state(),
                             decode_words({one(kSMovS8Neg1), one(kSAddS9S8One), one(kVMovV2S9), one(kVMovV3V0),
                                           one(kVAddV4Neg1V0), kVMulLoV5, kVMulLoV6, kVMovV11}));
    const auto& s = i.state();
    for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
        EXPECT_EQ(s.vgpr[2][lane], 0u) << lane;             // broadcast of s9 = 0
        EXPECT_EQ(s.vgpr[3][lane], lane) << lane;           // copy of v0
        EXPECT_EQ(s.vgpr[4][lane], lane - 1u) << lane;      // -1 + lane, lane 0 wraps to 0xFFFFFFFF
        EXPECT_EQ(s.vgpr[5][lane], lane << 16) << lane;     // low 32 bits of lane * 0x10000
        EXPECT_EQ(s.vgpr[6][lane], 0u) << lane;             // (lane << 16) * 0x10000 = lane << 32 -> low bits 0
        EXPECT_EQ(s.vgpr[11][lane], 64u) << lane;           // inline 64 (VOP3 form)
    }
    EXPECT_EQ(s.vgpr[4][0], 0xFFFFFFFFu);
}

TEST(ShaderRefVector, InactiveLanesAreUntouchedAndUnchecked) {
    WaveState s = abi_v0_entry_state();
    s.exec = 0x0000FFFFu;  // lanes 0..15 active
    s.vgpr[4].fill(0xAAAAAAAAu);
    s.vgpr_defined_lanes[4] = 0xFFFFFFFFu;
    // v9 is defined only in the active lanes: reading it must succeed.
    for (std::uint32_t lane = 0; lane < 16; ++lane) s.vgpr[9][lane] = 100 + lane;
    s.vgpr_defined_lanes[9] = 0x0000FFFFu;
    const auto i = run_steps(s, decode_words({one(kVAddV4Neg1V0), one(kVMovV3V9)}));
    for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
        if (lane < 16) {
            EXPECT_EQ(i.state().vgpr[4][lane], lane - 1u) << lane;
            EXPECT_EQ(i.state().vgpr[3][lane], 100 + lane) << lane;
        } else {
            EXPECT_EQ(i.state().vgpr[4][lane], 0xAAAAAAAAu) << lane;
        }
    }
    EXPECT_EQ(i.state().vgpr_defined_lanes[3], 0x0000FFFFu);
}

// ---- Vector f32 ALU ---------------------------------------------------------------------

TEST(ShaderRefFloat, AddMulAndRoundToNearestEven) {
    WaveState s = abi_v0_entry_state();
    s.vgpr[8].fill(f32(1.0f));
    s.vgpr[8][0] = f32(1.5f);
    s.vgpr[8][1] = 0x33800000u;  // 2^-24: 1 + 2^-24 is a tie between 1 and 1+2^-23 -> even (1.0)
    s.vgpr[8][2] = 0x34400000u;  // 1.5 * 2^-23: tie between 1+1ulp and 1+2ulp -> even (1+2ulp)
    s.vgpr_defined_lanes[8] = 0xFFFFFFFFu;
    s.sgpr[15] = f32(0.25f);
    s.sgpr_defined.set(15);
    const auto i = run_steps(s, decode_words({one(kVAddF32V7), one(kVMulF32V9), kVAddF32V10}));
    const auto& r = i.state();
    EXPECT_EQ(r.vgpr[7][0], 0x40200000u);  // 1.0 + 1.5 = 2.5
    EXPECT_EQ(r.vgpr[7][1], 0x3F800000u);  // RNE tie -> 1.0
    EXPECT_EQ(r.vgpr[7][2], 0x3F800002u);  // RNE tie -> 1 + 2^-22
    EXPECT_EQ(r.vgpr[7][3], 0x40000000u);  // 1.0 + 1.0 = 2.0
    EXPECT_EQ(r.vgpr[9][0], 0xC0C00000u);  // -4.0 * 1.5 = -6.0
    EXPECT_EQ(r.vgpr[9][3], 0xC0800000u);  // -4.0 * 1.0
    EXPECT_EQ(r.vgpr[10][0], 0x3FE00000u);  // 0.25 + 1.5 = 1.75
}

TEST(ShaderRefFloat, OperandsAndResultsMustBeFiniteNormal) {
    const auto with_v8 = [](std::uint32_t lane0) {
        WaveState s = abi_v0_entry_state();
        s.vgpr[8].fill(f32(1.0f));
        s.vgpr[8][0] = lane0;
        s.vgpr_defined_lanes[8] = 0xFFFFFFFFu;
        return s;
    };
    EXPECT_EQ(step_error(with_v8(0x00000001u), decode_words({one(kVAddF32V7)})), "float_domain");  // subnormal
    EXPECT_EQ(step_error(with_v8(0x7FC00000u), decode_words({one(kVAddF32V7)})), "float_domain");  // NaN
    EXPECT_EQ(step_error(with_v8(f32(-1.0f)), decode_words({one(kVAddF32V7)})), "float_domain");   // result 0
    EXPECT_EQ(step_error(with_v8(0x7F7FFFFFu), decode_words({one(kVMulF32V9)})), "float_domain");  // overflow
}

// ---- Full ABI v0 programs -----------------------------------------------------------

TEST(ShaderRefAbiV0, T704ExampleMatchesManualExpectedValues) {
    // input/expected from docs/contracts/shader_abi.md (lane 1 wraps: 0xFFFFFFFF + 1 = 0).
    BufferWords input{};
    for (std::uint32_t i = 0; i < kBufferWords; ++i) input[i] = i;
    input[1] = 0xFFFFFFFFu;
    const BufferWords expected = {0x00, 0x00, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10, 0x12, 0x14,
                                  0x16, 0x18, 0x1a, 0x1c, 0x1e, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2a,
                                  0x2c, 0x2e, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3a, 0x3c, 0x3e};
    const auto program = decode_words({kLoadV2, one(kWaitVm0), one(kVAddV2V0V2), kStoreV2, one(kEndpgm)});
    const auto r = run_abi_v0(program, input);
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    EXPECT_EQ(r->output, expected);
    EXPECT_EQ(r->output_written, 0xFFFFFFFFu);
    EXPECT_EQ(r->instructions_executed, 5u);
}

TEST(ShaderRefAbiV0, LoadOrderingRules) {
    const BufferWords input{};
    // Read of a pending load destination.
    auto r = run_abi_v0(decode_words({kLoadV2, one(kVAddV2V0V2), one(kWaitVm0), kStoreV2, one(kEndpgm)}), input);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, "load_not_ready");
    // Overwrite of a pending load destination.
    r = run_abi_v0(decode_words({kLoadV2, one(kVMovV2V0), one(kWaitVm0), kStoreV2, one(kEndpgm)}), input);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, "load_not_ready");
    // Store before the wait.
    r = run_abi_v0(decode_words({kLoadV2, kStoreV2, one(kEndpgm)}), input);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, "load_not_ready");
    // Only vmcnt(0) is an ABI v0 wait.
    r = run_abi_v0(decode_words({kLoadV2, one(kWaitLgkm0), kStoreV2, one(kEndpgm)}), input);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, "unsupported_abi");
}

TEST(ShaderRefAbiV0, ProgramShapeRules) {
    const BufferWords input{};
    const auto code = [&](std::vector<DecodedInstruction> p) {
        const auto r = run_abi_v0(p, input);
        return r ? std::string("ok") : r.error().code;
    };
    EXPECT_EQ(code(decode_words({one(kVMovV2V0), kStoreV2})), "invalid_program");  // no s_endpgm
    EXPECT_EQ(code(decode_words({one(kVMovV2V0), kStoreV2, one(kEndpgm), one(kEndpgm)})), "invalid_program");
    EXPECT_EQ(code(decode_words({one(kVMovV2V0), one(kEndpgm)})), "invalid_program");  // no store
    EXPECT_EQ(code(decode_words({one(kVMovV2V0), kStoreV2, one(kVMovV3V0), one(kEndpgm)})), "invalid_program");
    EXPECT_EQ(code(decode_words({one(kVMovV2V0), kStoreV2, one(kEndpgm)})), "ok");
}

TEST(ShaderRefAbiV0, RegisterRules) {
    EXPECT_EQ(step_error(abi_v0_entry_state(), decode_words({one(kSMovS8S0)})), "reserved_register");
    EXPECT_EQ(step_error(abi_v0_entry_state(), decode_words({one(kVMovV1V0)})), "reserved_register");
    EXPECT_EQ(step_error(abi_v0_entry_state(), decode_words({one(kVMovV3V9)})), "undefined_register");
    EXPECT_EQ(step_error(abi_v0_entry_state(), decode_words({one(kVMovV2S9)})), "undefined_register");
}

TEST(ShaderRefAbiV0, BufferBoundsAreChecked) {
    WaveState s = abi_v0_entry_state();
    s.vgpr[1][0] = 128;  // one past the 128-byte buffer
    EXPECT_EQ(step_error(s, decode_words({kLoadV2})), "out_of_bounds");
    s.vgpr[1][0] = 2;    // unaligned
    EXPECT_EQ(step_error(s, decode_words({kLoadV2})), "out_of_bounds");
}

// ---- Independence from the decoder ----------------------------------------------------

TEST(ShaderRefDecoderCheck, InlineConstantMustMatchItsSelector) {
    DecodedInstruction inst = decode_one({one(kVAddV4Neg1V0)});
    auto& alu = std::get<scale::shader::AluOperands>(inst.operands);
    ASSERT_EQ(alu.sources[0].encoded, 193u);  // selector for -1
    alu.sources[0].value = 5;                 // a decoder bug would look like this
    EXPECT_EQ(step_error(abi_v0_entry_state(), {inst}), "decoder_disagreement");
}

TEST(ShaderRefDecoderCheck, LiteralMustMatchTheTrailingDword) {
    DecodedInstruction inst = decode_one({kVMulLoV5});
    auto& alu = std::get<scale::shader::AluOperands>(inst.operands);
    ASSERT_EQ(alu.sources[1].kind, scale::shader::OperandKind::literal_constant);
    alu.sources[1].value = 0x20000;
    EXPECT_EQ(step_error(abi_v0_entry_state(), {inst}), "decoder_disagreement");
}

TEST(ShaderRefDecoderCheck, RegisterIndexMustMatchItsEncoding) {
    DecodedInstruction inst = decode_one({one(kVMovV3V0)});
    auto& alu = std::get<scale::shader::AluOperands>(inst.operands);
    alu.sources[0].value = 7;  // encoded is still 256 (v0)
    EXPECT_EQ(step_error(abi_v0_entry_state(), {inst}), "decoder_disagreement");
}

TEST(ShaderRefDecoderCheck, EveryInlineConstantSelector) {
    // v_mov_b32_e32 v2, <selector>: 0x7E040200 | selector (cf. llvm-mc: v_mov_b32 v2, 64 -> 0x7E0402C0).
    // Expected bit patterns written by hand from RDNA 2 ISA table "inline constants".
    struct Case {
        std::uint32_t selector;
        std::uint32_t bits;
    };
    std::vector<Case> cases;
    for (std::uint32_t i = 0; i <= 64; ++i) cases.push_back({128 + i, i});
    for (std::uint32_t i = 1; i <= 16; ++i) cases.push_back({192 + i, 0u - i});
    const Case floats[] = {{240, 0x3F000000u}, {241, 0xBF000000u}, {242, 0x3F800000u},
                           {243, 0xBF800000u}, {244, 0x40000000u}, {245, 0xC0000000u},
                           {246, 0x40800000u}, {247, 0xC0800000u}, {248, 0x3E22F983u}};
    cases.insert(cases.end(), std::begin(floats), std::end(floats));
    for (const Case& c : cases) {
        const std::uint32_t word = 0x7E040200u | c.selector;
        const auto program = decode_words({one(word)});
        ASSERT_EQ(program.size(), 1u) << c.selector;
        Interpreter interp(abi_v0_entry_state(), {});
        const auto r = interp.step(program[0]);
        ASSERT_TRUE(r.has_value()) << "selector " << c.selector << ": " << r.error().to_string();
        EXPECT_EQ(interp.state().vgpr[2][17], c.bits) << "selector " << c.selector;
    }
}