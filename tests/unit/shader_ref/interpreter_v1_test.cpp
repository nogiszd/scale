// T-712: reference interpreter under shader ABI v1 (docs/contracts/shader_abi.md,
// T-707). Instruction words come from llvm-mc/llvm-objdump 23.1.6 (gfx1030,
// branch targets resolved in an object file). ABI v1 instructions are built as
// decoded structs from those words, because the v3 decoder implementation
// (T-708) is not merged yet. v0 forms go through scale::shader::decode.
// All expected values are computed by hand from the contract.

#include "scale/shader_ref/interpreter.h"

#include <gtest/gtest.h>

#include <bit>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

using namespace scale::shader_ref;
using scale::shader::AluOperands;
using scale::shader::BranchOperands;
using scale::shader::CompareOperands;
using scale::shader::DecodedInstruction;
using scale::shader::Encoding;
using scale::shader::Opcode;
using scale::shader::Operand;
using scale::shader::OperandKind;

namespace {

constexpr std::uint32_t kSentinel = 0xDEADBEEFu;

// ---- operand helpers (raw selector in `encoded`, as decode.h v3 specifies) --------
Operand sgpr(std::uint16_t n) { return {OperandKind::sgpr, n, n}; }
Operand vgpr8(std::uint16_t n) { return {OperandKind::vgpr, n, n}; }  // 8-bit VGPR field
Operand vcc() { return {OperandKind::vcc_lo, 106, 106}; }
Operand exec_dst() { return {OperandKind::exec_lo, 126, 126}; }
Operand inline_int(std::uint16_t v) { return {OperandKind::inline_constant, static_cast<std::uint16_t>(128 + v), v}; }
Operand literal(std::uint32_t v) { return {OperandKind::literal_constant, 255, v}; }

DecodedInstruction v0_form(std::initializer_list<std::uint32_t> words) {
    const std::vector<std::uint32_t> w(words);
    auto r = scale::shader::decode(scale::ByteView(std::as_bytes(std::span(w))));
    EXPECT_TRUE(r.has_value() && r->size() == 1) << (r ? "" : r.error().to_string());
    return r && !r->empty() ? (*r)[0] : DecodedInstruction{};
}

DecodedInstruction raw(Opcode op, Encoding enc, std::initializer_list<std::uint32_t> words) {
    DecodedInstruction d;
    d.opcode = op;
    d.encoding = enc;
    for (std::uint32_t w : words) d.raw_words[d.word_count++] = w;
    return d;
}

DecodedInstruction vopc(Opcode op, Operand src0, std::uint16_t vsrc1, std::uint32_t word) {
    DecodedInstruction d = raw(op, Encoding::vopc, {word});
    d.operands = CompareOperands{{src0, vgpr8(vsrc1)}};
    return d;
}

DecodedInstruction saveexec(std::uint16_t dst, Operand src, std::uint32_t word) {
    DecodedInstruction d = raw(Opcode::s_and_saveexec_b32, Encoding::sop1, {word});
    AluOperands a{sgpr(dst)};
    a.sources[0] = src;
    a.source_count = 1;
    d.operands = a;
    return d;
}

DecodedInstruction mov_exec(Operand src, std::initializer_list<std::uint32_t> words) {
    DecodedInstruction d = raw(Opcode::s_mov_b32, Encoding::sop1, words);
    AluOperands a{exec_dst()};
    a.sources[0] = src;
    a.source_count = 1;
    d.operands = a;
    return d;
}

DecodedInstruction branch(Opcode op, std::uint32_t word) {
    DecodedInstruction d = raw(op, Encoding::sopp, {word});
    d.operands = BranchOperands{static_cast<std::uint16_t>(word & 0xFFFF), 0};  // target set by Program
    return d;
}

// Assigns stream offsets and, like a decoder, branch targets from the raw SIMM16.
struct Program {
    std::vector<DecodedInstruction> insts;
    std::size_t next = 0;
    Program& add(DecodedInstruction d) {
        d.dword_offset = next;
        next += d.word_count;
        if (auto* b = std::get_if<BranchOperands>(&d.operands)) {
            b->target_dword = static_cast<std::int64_t>(next) + std::bit_cast<std::int16_t>(b->simm16);
        }
        insts.push_back(d);
        return *this;
    }
};

// llvm-mc encodings
constexpr std::uint32_t kCmpLt3V0 = 0x7D820083;       // v_cmp_lt_u32_e32 vcc_lo, 3, v0
constexpr std::uint32_t kCmpEq5V0 = 0x7D840085;       // v_cmp_eq_u32_e32 vcc_lo, 5, v0
constexpr std::uint32_t kCmpEqS9V3 = 0x7D840609;      // v_cmp_eq_u32_e32 vcc_lo, s9, v3
constexpr std::uint32_t kSaveexecS8Vcc = 0xBE883C6A;  // s_and_saveexec_b32 s8, vcc_lo
constexpr std::uint32_t kSaveexecS4Vcc = 0xBE843C6A;  // s_and_saveexec_b32 s4, vcc_lo
constexpr std::uint32_t kMovExecS8 = 0xBEFE0308;      // s_mov_b32 exec_lo, s8
constexpr std::uint32_t kMovExecLit = 0xBEFE03FF;     // s_mov_b32 exec_lo, <literal>
constexpr std::uint32_t kLoadV2[] = {0xE0301000, 0x80000201};
constexpr std::uint32_t kStoreV2[] = {0xE0701000, 0x80010201};
constexpr std::uint32_t kWait = 0xBF8C3F70;
constexpr std::uint32_t kAddV2One = 0x4A040481;   // v_add_nc_u32_e32 v2, 1, v2
constexpr std::uint32_t kMovV2V0 = 0x7E040300;    // v_mov_b32_e32 v2, v0
constexpr std::uint32_t kEndpgm = 0xBF810000;

DecodedInstruction load() { return v0_form({kLoadV2[0], kLoadV2[1]}); }
DecodedInstruction store() { return v0_form({kStoreV2[0], kStoreV2[1]}); }
DecodedInstruction endpgm() { return v0_form({kEndpgm}); }

BufferWords identity_input() {
    BufferWords in{};
    for (std::uint32_t i = 0; i < kBufferWords; ++i) in[i] = i;
    return in;
}

BufferWords sentinel_output() {
    BufferWords out{};
    out.fill(kSentinel);
    return out;
}

// The T-707 contract example (s_cbranch_execz store = 0xBF880004).
Program t707_example() {
    Program p;
    p.add(vopc(Opcode::v_cmp_lt_u32, inline_int(3), 0, kCmpLt3V0))
        .add(saveexec(8, vcc(), kSaveexecS8Vcc))
        .add(branch(Opcode::s_cbranch_execz, 0xBF880004))
        .add(load())
        .add(v0_form({kWait}))
        .add(v0_form({kAddV2One}))
        .add(store())
        .add(endpgm());
    return p;
}

std::string run_error(const Program& p, std::uint32_t exec = 0xFFFFFFFFu) {
    const auto r = run_abi_v1(p.insts, identity_input(), {exec, sentinel_output()});
    return r ? "ok" : r.error().code;
}

// Static structure check only (no execution), so each static rule is tested
// on its own rather than through a later dynamic failure.
std::string static_error(const Program& p) {
    const auto r = check_program(p.insts, AbiVersion::v1);
    return r ? "ok" : r.error().code;
}

}  // namespace

// ---- The two worked examples from the T-707 contract ----------------------------

TEST(ShaderRefV1, ContractExampleActiveLanes4To7) {
    const Program p = t707_example();
    const auto r = run_abi_v1(p.insts, identity_input(), {0x000000FFu, sentinel_output()});
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    BufferWords expected = sentinel_output();
    expected[4] = 5;
    expected[5] = 6;
    expected[6] = 7;
    expected[7] = 8;
    EXPECT_EQ(r->output, expected);
    EXPECT_EQ(r->output_written, 0x000000F0u);
    EXPECT_EQ(r->final_state.vcc, 0x000000F0u);
    EXPECT_EQ(r->final_state.sgpr[8], 0x000000FFu);
    EXPECT_EQ(r->final_state.exec, 0x000000F0u);
    EXPECT_TRUE(r->final_state.scc);
    EXPECT_EQ(r->instructions_executed, 8u);
}

TEST(ShaderRefV1, ContractExampleEmptyMaskTakesBranch) {
    const Program p = t707_example();
    const auto r = run_abi_v1(p.insts, identity_input(), {0x0000000Fu, sentinel_output()});
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    EXPECT_EQ(r->output, sentinel_output());  // no lane wrote; v2 was never read
    EXPECT_EQ(r->output_written, 0u);
    EXPECT_EQ(r->final_state.vcc, 0u);
    EXPECT_EQ(r->final_state.exec, 0u);
    EXPECT_EQ(r->final_state.sgpr[8], 0x0000000Fu);
    EXPECT_FALSE(r->final_state.scc);
    EXPECT_EQ(r->instructions_executed, 5u);  // cmp, saveexec, branch, store, endpgm
}

// ---- Scalar branches -----------------------------------------------------------------

TEST(ShaderRefV1, SccAndUnconditionalBranches) {
    // s_add_u32 s9, 1, 2 (SCC = 0); scc1 not taken -> v2 = 11; scc0 taken (skips 22);
    // s_branch taken (skips 33). Every lane stores 11.
    Program p;
    p.add(v0_form({0x80098281}))  // s_add_u32 s9, 1, 2
        .add(branch(Opcode::s_cbranch_scc1, 0xBF850001))
        .add(v0_form({0x7E04028B}))  // v_mov_b32 v2, 11
        .add(branch(Opcode::s_cbranch_scc0, 0xBF840001))
        .add(v0_form({0x7E040296}))  // v_mov_b32 v2, 22
        .add(branch(Opcode::s_branch, 0xBF820001))
        .add(v0_form({0x7E0402A1}))  // v_mov_b32 v2, 33
        .add(store())
        .add(endpgm());
    const auto r = run_abi_v1(p.insts, identity_input(), {0xFFFFFFFFu, sentinel_output()});
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    BufferWords expected{};
    expected.fill(11);
    EXPECT_EQ(r->output, expected);
    EXPECT_EQ(r->instructions_executed, 7u);
}

// ---- EXEC save/restore and reactivation ---------------------------------------------

TEST(ShaderRefV1, ReactivatedLanesKeepEarlierValues) {
    // v_cmp_eq vcc, 5, v0 -> VCC = bit 5; saveexec s8; v_mov v2, 0x1234 (lane 5 only);
    // s_mov exec_lo, s8 restores all lanes; store. v2 was defined (0x77) beforehand.
    WaveState s = abi_v0_entry_state();
    s.vgpr[2].fill(0x77);
    s.vgpr_defined_lanes[2] = 0xFFFFFFFFu;
    Buffers b;
    b.output = sentinel_output();
    Interpreter interp(s, b, AbiVersion::v1);
    DecodedInstruction mov_lit = v0_form({0x7E0402FF, 0x00001234});  // v_mov_b32 v2, 0x1234
    for (const auto& inst : {vopc(Opcode::v_cmp_eq_u32, inline_int(5), 0, kCmpEq5V0), saveexec(8, vcc(), kSaveexecS8Vcc),
                             mov_lit, mov_exec(sgpr(8), {kMovExecS8}), store()}) {
        const auto r = interp.step(inst);
        ASSERT_TRUE(r.has_value()) << r.error().to_string();
    }
    EXPECT_EQ(interp.state().vcc, 1u << 5);
    EXPECT_EQ(interp.state().exec, 0xFFFFFFFFu);
    EXPECT_TRUE(interp.state().scc);  // from saveexec, preserved by the exec_lo move
    for (std::uint32_t i = 0; i < kBufferWords; ++i) {
        EXPECT_EQ(interp.buffers().output[i], i == 5 ? 0x1234u : 0x77u) << i;
    }
}

TEST(ShaderRefV1, ReactivatedLanesKeepTheirUndefinedState) {
    // Same program from the ABI entry state: lanes other than 5 never wrote v2.
    Program p;
    p.add(vopc(Opcode::v_cmp_eq_u32, inline_int(5), 0, kCmpEq5V0))
        .add(saveexec(8, vcc(), kSaveexecS8Vcc))
        .add(v0_form({0x7E0402FF, 0x00001234}))
        .add(mov_exec(sgpr(8), {kMovExecS8}))
        .add(store())
        .add(endpgm());
    EXPECT_EQ(run_error(p), "undefined_register");
}

TEST(ShaderRefV1, EmptyMaskReadsNoSources) {
    // s9, v3 and v2 are undefined, but with EXEC == 0 nothing is read or written.
    Program p;
    p.add(vopc(Opcode::v_cmp_eq_u32, sgpr(9), 3, kCmpEqS9V3)).add(v0_form({kAddV2One})).add(store()).add(endpgm());
    const auto r = run_abi_v1(p.insts, identity_input(), {0u, sentinel_output()});
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    EXPECT_EQ(r->final_state.vcc, 0u);
    EXPECT_EQ(r->output, sentinel_output());
    // The same compare with active lanes must reject the undefined s9.
    EXPECT_EQ(run_error(p, 0x1u), "undefined_register");
}

TEST(ShaderRefV1, CompareClearsInactiveBits) {
    WaveState s = abi_v0_entry_state();
    s.exec = 0x0000FF00u;
    s.vcc = 0xFFFFFFFFu;  // must be fully replaced
    Interpreter interp(s, {}, AbiVersion::v1);
    ASSERT_TRUE(interp.step(vopc(Opcode::v_cmp_lt_u32, inline_int(3), 0, kCmpLt3V0)).has_value());
    EXPECT_EQ(interp.state().vcc, 0x0000FF00u);  // 3 < lane for lanes 8..15; all others clear
    EXPECT_EQ(interp.state().exec, 0x0000FF00u);
}

// ---- Per-lane pending loads -----------------------------------------------------------

TEST(ShaderRefV1, PendingLoadsArePerLaneAndSurviveMaskChanges) {
    WaveState s = abi_v0_entry_state();
    s.exec = 0x0000000Fu;
    Buffers b;
    b.input = identity_input();
    Interpreter interp(s, b, AbiVersion::v1);
    ASSERT_TRUE(interp.step(load()).has_value());
    EXPECT_EQ(interp.pending_lanes(2), 0x0000000Fu);
    ASSERT_TRUE(interp.step(mov_exec(literal(0xF0), {kMovExecLit, 0xF0})).has_value());
    // Lanes 4..7 are not pending: overwriting them is allowed.
    ASSERT_TRUE(interp.step(v0_form({kMovV2V0})).has_value());
    ASSERT_TRUE(interp.step(mov_exec(literal(0xFF), {kMovExecLit, 0xFF})).has_value());
    const auto read = interp.step(v0_form({kAddV2One}));  // lanes 0..3 still pending
    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().code, "load_not_ready");
}

TEST(ShaderRefV1, WaitClearsPendingLanesEvenWhileInactive) {
    WaveState s = abi_v0_entry_state();
    s.exec = 0x0000000Fu;
    Buffers b;
    b.input = identity_input();
    Interpreter interp(s, b, AbiVersion::v1);
    ASSERT_TRUE(interp.step(load()).has_value());
    ASSERT_TRUE(interp.step(mov_exec(literal(0xF0), {kMovExecLit, 0xF0})).has_value());
    ASSERT_TRUE(interp.step(v0_form({kWait})).has_value());
    EXPECT_EQ(interp.pending_lanes(2), 0u);
    ASSERT_TRUE(interp.step(mov_exec(literal(0x0F), {kMovExecLit, 0x0F})).has_value());
    const auto r = interp.step(v0_form({kAddV2One}));
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    EXPECT_EQ(interp.state().vgpr[2][3], 4u);  // input[3] + 1
}

// ---- Static structure rules -------------------------------------------------------------

TEST(ShaderRefV1Structure, BranchTargetRules) {
    EXPECT_EQ(static_error(t707_example()), "ok");
    {  // branch to itself (simm16 = -1)
        Program p;
        p.add(branch(Opcode::s_branch, 0xBF82FFFF)).add(v0_form({kMovV2V0})).add(store()).add(endpgm());
        EXPECT_EQ(static_error(p), "invalid_program");
    }
    {  // backward branch (simm16 = -2)
        Program p;
        p.add(v0_form({kMovV2V0})).add(branch(Opcode::s_branch, 0xBF82FFFE)).add(store()).add(endpgm());
        EXPECT_EQ(static_error(p), "invalid_program");
    }
    {  // bypasses the store: targets s_endpgm
        Program p;
        p.add(v0_form({kMovV2V0})).add(branch(Opcode::s_branch, 0xBF820002)).add(store()).add(endpgm());
        EXPECT_EQ(static_error(p), "invalid_program");
    }
    {  // lands inside a literal (second dword of v_mov v2, 0x1234)
        Program p;
        p.add(branch(Opcode::s_branch, 0xBF820001))
            .add(v0_form({0x7E0402FF, 0x00001234}))
            .add(store())
            .add(endpgm());
        EXPECT_EQ(static_error(p), "invalid_program");
    }
    {  // outside the stream
        Program p;
        p.add(v0_form({kMovV2V0})).add(branch(Opcode::s_branch, 0xBF820100)).add(store()).add(endpgm());
        EXPECT_EQ(static_error(p), "invalid_program");
    }
    {  // an untaken branch is still checked
        Program p;
        p.add(v0_form({0x80098281}))  // SCC = 0
            .add(branch(Opcode::s_cbranch_scc1, 0xBF82FFFF))
            .add(v0_form({kMovV2V0}))
            .add(store())
            .add(endpgm());
        EXPECT_EQ(static_error(p), "invalid_program");
    }
}

TEST(ShaderRefV1Structure, DecoderDisagreementsAreReported) {
    {  // decoded target differs from the raw SIMM16
        Program p = t707_example();
        std::get<BranchOperands>(p.insts[2].operands).target_dword += 1;
        EXPECT_EQ(run_error(p, 0xFF), "decoder_disagreement");
    }
    {  // vcc_lo operand with a wrong selector
        Program p = t707_example();
        std::get<AluOperands>(p.insts[1].operands).sources[0].encoded = 107;
        EXPECT_EQ(run_error(p, 0xFF), "decoder_disagreement");
    }
    {  // stream offsets out of step
        Program p = t707_example();
        p.insts[4].dword_offset += 1;
        EXPECT_EQ(run_error(p, 0xFF), "decoder_disagreement");
    }
}

TEST(ShaderRefV1Structure, AbiRules) {
    {  // v1 instructions are not ABI v0
        const Program p = t707_example();
        BufferWords in = identity_input();
        const auto r = run_abi_v0(p.insts, in);
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().code, "unsupported_abi");
    }
    {  // saveexec into a descriptor SGPR
        Program p;
        p.add(vopc(Opcode::v_cmp_lt_u32, inline_int(3), 0, kCmpLt3V0))
            .add(saveexec(4, vcc(), kSaveexecS4Vcc))
            .add(v0_form({kMovV2V0}))
            .add(store())
            .add(endpgm());
        EXPECT_EQ(run_error(p), "reserved_register");
    }
    {  // vcc_lo is not a vector ALU source
        Program p;
        DecodedInstruction add = v0_form({kAddV2One});
        std::get<AluOperands>(add.operands).sources[0] = vcc();
        add.raw_words[0] = 0x4A04046A;  // v_add_nc_u32_e32 v2, vcc_lo, v2
        p.add(v0_form({kMovV2V0})).add(add).add(store()).add(endpgm());
        EXPECT_EQ(run_error(p), "unsupported_abi");
    }
    {  // store must directly precede s_endpgm
        Program p;
        p.add(v0_form({kMovV2V0})).add(store()).add(v0_form({kMovV2V0})).add(endpgm());
        EXPECT_EQ(run_error(p), "invalid_program");
    }
}

// ---- Real v3 decoder output through the independent interpreter (after T-708) ----
// The interpreter re-derives vcc_lo/exec_lo selectors, VOPC fields and branch
// targets from the raw words. Any decoder disagreement fails these cases.

namespace {
std::vector<DecodedInstruction> decode_stream(std::initializer_list<std::uint32_t> words) {
    const std::vector<std::uint32_t> w(words);
    auto r = scale::shader::decode(scale::ByteView(std::as_bytes(std::span(w))));
    EXPECT_TRUE(r.has_value()) << (r ? "" : r.error().to_string());
    return r ? *r : std::vector<DecodedInstruction>{};
}
}  // namespace

TEST(ShaderRefV1Decoded, ContractExampleFromRealDecoder) {
    const auto program = decode_stream({0x7D820083, 0xBE883C6A, 0xBF880004, 0xE0301000, 0x80000201, 0xBF8C3F70,
                                        0x4A040481, 0xE0701000, 0x80010201, 0xBF810000});
    ASSERT_EQ(program.size(), 8u);
    ASSERT_TRUE(check_program(program, AbiVersion::v1).has_value());
    // The decoded structs match the hand-built ones the other tests use.
    const Program hand = t707_example();
    for (std::size_t i = 0; i < program.size(); ++i) {
        EXPECT_EQ(program[i].opcode, hand.insts[i].opcode) << i;
        EXPECT_EQ(program[i].dword_offset, hand.insts[i].dword_offset) << i;
    }
    EXPECT_EQ(std::get<BranchOperands>(program[2].operands).target_dword, 7);

    auto r = run_abi_v1(program, identity_input(), {0x000000FFu, sentinel_output()});
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    BufferWords expected = sentinel_output();
    for (std::uint32_t lane = 4; lane <= 7; ++lane) expected[lane] = lane + 1;
    EXPECT_EQ(r->output, expected);

    r = run_abi_v1(program, identity_input(), {0x0000000Fu, sentinel_output()});
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    EXPECT_EQ(r->output, sentinel_output());
}

TEST(ShaderRefV1Decoded, BranchesAndExecRestoreFromRealDecoder) {
    // scc program: every lane stores 11.
    const auto scc = decode_stream({0x80098281, 0xBF850001, 0x7E04028B, 0xBF840001, 0x7E040296, 0xBF820001,
                                    0x7E0402A1, 0xE0701000, 0x80010201, 0xBF810000});
    const auto r = run_abi_v1(scc, identity_input(), {0xFFFFFFFFu, sentinel_output()});
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    BufferWords eleven{};
    eleven.fill(11);
    EXPECT_EQ(r->output, eleven);

    // restore program: reactivated lanes never wrote v2 -> undefined_register.
    const auto restore = decode_stream({0x7D840085, 0xBE883C6A, 0x7E0402FF, 0x00001234, 0xBEFE0308, 0xE0701000,
                                        0x80010201, 0xBF810000});
    ASSERT_EQ(restore.size(), 6u);
    EXPECT_EQ(std::get<AluOperands>(restore[3].operands).destination.kind, OperandKind::exec_lo);
    const auto e = run_abi_v1(restore, identity_input(), {0xFFFFFFFFu, sentinel_output()});
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code, "undefined_register");
}