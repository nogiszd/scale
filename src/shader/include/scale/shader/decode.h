#pragma once

#include "scale/common/byte_reader.h"
#include "scale/common/error.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace scale::shader {

enum class Encoding : std::uint8_t {
    sop1,
    sop2,
    sopk,
    sopp,
    vop1,
    vop2,
    vop3,
    mubuf,
    vopc,
};

// Equivalent e32 and e64 forms share an opcode. Encoding distinguishes them.
// The ALU and comparison subset uses unmodified forms.
enum class Opcode : std::uint8_t {
    s_mov_b32,
    s_add_u32,
    s_movk_i32,
    s_endpgm,
    s_waitcnt,
    v_mov_b32,
    v_add_nc_u32,
    v_mul_lo_u32,
    v_add_f32,
    v_mul_f32,
    buffer_load_dword,
    buffer_store_dword,
    v_cmp_eq_u32,
    v_cmp_lt_u32,
    s_and_saveexec_b32,
    s_cbranch_scc0,
    s_cbranch_scc1,
    s_cbranch_execz,
    s_branch,
};

enum class OperandKind : std::uint8_t {
    sgpr,
    vgpr,
    inline_constant,
    literal_constant,
    immediate,
    vcc_lo,  // ISA selector 106.
    exec_lo,  // ISA selector 126.
};

struct Operand {
    OperandKind kind{};
    // Unsigned bits of the raw source or destination field, such as a 9-bit
    // VOP source, an 8-bit SSRC, or the 16-bit s_movk_i32 immediate.
    // For a literal, this is the selector in the source field.
    std::uint16_t encoded{};
    // Zero-based SGPR or VGPR index, ISA selector for vcc_lo or exec_lo,
    // or the full 32-bit constant bit pattern. The immediate in s_movk_i32
    // is already sign-extended.
    std::uint32_t value{};
};

struct AluOperands {
    Operand destination;  // SGPR, VGPR, or exec_lo for s_mov_b32.
    std::array<Operand, 3> sources{};  // In ISA source order.
    std::uint8_t source_count{};  // Number of valid entries at the start of sources.
};

// The supported VOPC forms implicitly write vcc_lo and have no destination field.
struct CompareOperands {
    // In ISA source order: the 9-bit SRC0 selector, then the 8-bit VSRC1
    // VGPR index. Both raw fields are preserved in Operand::encoded.
    std::array<Operand, 2> sources{};
};

struct BranchOperands {
    std::uint16_t simm16{};  // Raw bits of the signed displacement.
    // Offset of the following instruction plus sign-extended simm16, in
    // dwords from the start of the stream. May be outside the stream.
    std::int64_t target_dword{};
};

// Thresholds from the gfx1030 s_waitcnt instruction.
struct WaitCounters {
    std::uint8_t vmcnt{};
    std::uint8_t expcnt{};
    std::uint8_t lgkmcnt{};
};

// Untyped dword MUBUF with OFFEN set, IDXEN clear and optional flags clear.
struct BufferOperands {
    std::uint8_t data_vgpr{};  // Load destination or store source.
    std::uint8_t address_vgpr{};
    std::uint8_t resource_sgpr_base{};  // First index of the four-SGPR tuple.
    Operand scalar_offset_bytes;  // SGPR or inline constant.
    std::uint16_t immediate_offset_bytes{};
};

struct DecodedInstruction {
    Opcode opcode{};
    Encoding encoding{};
    std::size_t dword_offset{};  // Zero-based offset from the start of the stream.
    std::array<std::uint32_t, 3> raw_words{};  // Includes the literal when present.
    std::uint8_t word_count{};  // Number of valid raw_words, from 1 to 3.
    // monostate for s_endpgm (immediate zero), WaitCounters for s_waitcnt,
    // BufferOperands for MUBUF, AluOperands for arithmetic, moves and
    // s_and_saveexec_b32, CompareOperands for VOPC, and BranchOperands for SOPP
    // branches.
    // Implicit operands and effects, such as SCC for s_add_u32, follow Opcode.
    std::variant<std::monostate, AluOperands, WaitCounters, BufferOperands,
                 CompareOperands, BranchOperands> operands;
};

// Decode the entire little-endian gfx1030 instruction stream in order,
// including any instructions after s_endpgm. Empty input yields an empty vector.
// The caller validates program termination, branch targets, dispatch rules and
// register usage under the synthetic ABI. Successful results own their contents.
// On failure, return the first error (component "shader") with its dword
// offset and available raw words in the message. Error codes are
// truncated_encoding, unsupported_encoding, unsupported_operand and
// unsupported_modifier.
[[nodiscard]] Result<std::vector<DecodedInstruction>> decode(ByteView code);

}  // namespace scale::shader
