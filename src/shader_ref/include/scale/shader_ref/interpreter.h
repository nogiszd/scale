#pragma once

// Independent RDNA2 (gfx1030) reference interpreter (T-711).
//
// Evaluates decoded instructions (scale/shader/decode.h, frozen interface)
// directly over a modeled wave32. It shares NO semantic code with the
// compiler (shader_ir / shader_spirv), so the two implementations can't
// contain the same mistake by construction (AGENTS.md M7). Semantics come
// from the AMD RDNA 2 ISA reference (70648) and the synthetic ABI v0 in
// docs/contracts/shader_abi.md.
//
// It also cross-checks the decoder. Inline constants and literals are
// re-derived from each operand's raw `encoded` field and the instruction's
// raw words, and any disagreement with the decoder's resolved value is an
// error (`decoder_disagreement`).
//
// Errors use component "shader_ref" and these codes:
//   unsupported_instruction  opcode or operand form outside the modeled subset
//   decoder_disagreement     decoded operand value contradicts its raw encoding
//   reserved_register        s0..s7 used as an ALU operand, or v1 written (ABI v0)
//   undefined_register       an active lane reads a register it never wrote
//   load_not_ready           pending load destination read/overwritten, or a
//                            store issued before s_waitcnt vmcnt(0)
//   unsupported_abi          MUBUF form, resource or wait form outside ABI v0
//   out_of_bounds            buffer byte offset unaligned or outside 128 bytes
//   float_domain             f32 operand or result outside finite normal binary32
//   invalid_program          missing/extra s_endpgm, missing or repeated store,
//                            or the instruction limit was exceeded

#include "scale/common/error.h"
#include "scale/shader/decode.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <span>

namespace scale::shader_ref {

inline constexpr std::size_t kWaveSize = 32;
inline constexpr std::size_t kSgprCount = 106;  // s0..s105
inline constexpr std::size_t kVgprCount = 256;
inline constexpr std::size_t kBufferWords = 32;  // ABI v0 buffers are 128 bytes

using LaneWords = std::array<std::uint32_t, kWaveSize>;
using BufferWords = std::array<std::uint32_t, kBufferWords>;

struct WaveState {
    std::array<std::uint32_t, kSgprCount> sgpr{};
    std::bitset<kSgprCount> sgpr_defined;
    std::array<LaneWords, kVgprCount> vgpr{};
    std::array<std::uint32_t, kVgprCount> vgpr_defined_lanes{};  // bit per lane
    std::uint32_t exec = 0;  // EXEC_LO; EXEC_HI is unused in wave32
    std::uint32_t vcc = 0;   // VCC_LO
    bool scc = false;
};

struct Buffers {
    BufferWords input{};
    BufferWords output{};
    std::uint32_t output_written = 0;  // bit per output word
};

// Wave state at dispatch under ABI v0: v0 = lane index, v1 = 4 * lane,
// EXEC = all 32 lanes, VCC = 0, SCC = 0, everything else undefined.
[[nodiscard]] WaveState abi_v0_entry_state();

// Executes one instruction. s_endpgm only marks the program as ended.
class Interpreter {
public:
    Interpreter(WaveState state, Buffers buffers) : state_(state), buffers_(buffers) {}

    [[nodiscard]] Result<void> step(const shader::DecodedInstruction& instruction);

    [[nodiscard]] const WaveState& state() const noexcept { return state_; }
    [[nodiscard]] const Buffers& buffers() const noexcept { return buffers_; }
    [[nodiscard]] bool ended() const noexcept { return ended_; }
    [[nodiscard]] std::size_t stores() const noexcept { return stores_; }

private:
    WaveState state_;
    Buffers buffers_;
    std::bitset<kVgprCount> pending_load_;
    bool ended_ = false;
    std::size_t stores_ = 0;
};

struct DispatchResult {
    BufferWords output{};
    std::uint32_t output_written = 0;
    std::size_t instructions_executed = 0;
    WaveState final_state;
};

// Runs a complete ABI v0 program: straight-line code, exactly one output store,
// terminated by s_endpgm as the last instruction.
[[nodiscard]] Result<DispatchResult> run_abi_v0(std::span<const shader::DecodedInstruction> program,
                                                const BufferWords& input);

}  // namespace scale::shader_ref
