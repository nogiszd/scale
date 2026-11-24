#pragma once

#include "scale/common/error.h"
#include "scale/shader/decode.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace scale::shader {

enum class IrType : std::uint8_t {
    u32,
    f32,
    flag,
    mask32,
};

enum class IrLanes : std::uint8_t {
    uniform,
    per_lane,
};

struct IrValueType {
    IrType type;
    IrLanes lanes;

    bool operator==(const IrValueType&) const = default;
};

// A value is named by the index of its defining instruction in IrProgram.
using IrValueId = std::size_t;

enum class IrOpcode : std::uint8_t {
    constant,     // constant_bits holds raw u32, flag (0 or 1), or mask32 bits.
    lane_index,   // T-704 local invocation index as a per-lane u32.
    copy,         // Copy u32 bits, broadcasting a uniform input if needed.
    bitcast,      // Reinterpret u32 and f32 bits, preserving lane mode.
    uadd,         // Wrapping u32 addition.
    uadd_carry,   // Unsigned comparison: input 0 (sum) < input 1 (left operand).
    umul,         // Low 32 bits of the unsigned product.
    fadd,         // Separate binary32 operation under the T-704 float contract.
    fmul,
    load_input,   // T-704 input buffer, input 0 is the per-lane u32 byte offset.
    wait_vmem,    // Validated vmcnt(0), completes all preceding input loads.
    store_output, // T-704 output buffer, inputs are byte offset and u32 data.
    end,
};

struct IrInstruction {
    IrOpcode opcode;
    std::optional<IrValueType> result;  // Absent for wait, store and end.
    std::array<IrValueId, 2> inputs{};  // First input_count entries name earlier values.
    std::uint8_t input_count{};
    // Every guest vector operation names the active EXEC mask. Uniform inputs
    // to per-lane operations are broadcast. ABI v0 enables all lanes throughout.
    std::optional<IrValueId> exec;
    std::uint32_t constant_bits{};
    std::optional<std::size_t> dword_offset;  // Absent for ABI entry values.
};

struct IrProgram {
    std::vector<IrInstruction> instructions;
    IrValueId exec;       // Uniform mask32, fixed for the program.
    IrValueId vcc;        // Uniform mask32, fixed at zero in the decoded subset.
    IrValueId entry_scc;  // Uniform flag, initially false.
    IrValueId exit_scc;   // Uniform flag from the last scalar add, or entry_scc.
};

// Lower a complete decoded gfx1030 stream under the synthetic T-704 ABI v0.
// The result owns its data. Fixture metadata and the data-dependent floating
// point domain are checked by the caller. The first lowering failure uses
// component "shader" and one of these codes:
// invalid_decoded_instruction, invalid_program, unsupported_abi,
// reserved_register, undefined_register, load_not_ready.
[[nodiscard]] Result<IrProgram> lower_ir(std::span<const DecodedInstruction> instructions);

}  // namespace scale::shader
