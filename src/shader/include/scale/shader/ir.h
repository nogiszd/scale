#pragma once

#include "scale/common/error.h"
#include "scale/shader/decode.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
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
    std::array<IrValueId, 3> inputs{};  // First input_count entries name earlier values.
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
// The result owns its data. The caller checks test case metadata and the
// data-dependent floating point domain. The first lowering failure uses
// component "shader" and one of these codes:
// invalid_decoded_instruction, invalid_program, unsupported_abi,
// reserved_register, undefined_register, load_not_ready.
[[nodiscard]] Result<IrProgram> lower_ir(std::span<const DecodedInstruction> instructions);

// ABI v1 uses register slots to carry values across forward control-flow
// joins. SGPR, EXEC and VCC slots are uniform. VGPR slots are per lane.
enum class IrV1RegisterKind : std::uint8_t { sgpr, vgpr, exec, vcc };

struct IrV1Register {
    IrV1RegisterKind kind{};
    std::uint8_t index{};  // Used for SGPR and VGPR slots.
};

using IrV1Operand = std::variant<std::uint32_t, IrV1Register>;

enum class IrV1Opcode : std::uint8_t {
    copy,
    uadd,
    uadd_scc,  // Also sets uniform SCC to the unsigned carry out.
    umul,
    fadd,      // Binary32 arithmetic on the bits of two dword operands.
    fmul,
    compare_eq,
    compare_lt,
    save_exec, // Saves EXEC, intersects it with the source and sets SCC on nonzero.
    load_input,
    wait_vmem,
    store_output,
};

struct IrV1Operation {
    IrV1Opcode opcode{};
    IrLanes lanes{};  // Per-lane work is guarded by the current EXEC mask.
    std::optional<IrV1Register> destination{};
    // Compare results combine into the uniform VCC destination. Loads and
    // stores use the ABI lane index. Only the store value is an explicit input.
    std::array<IrV1Operand, 2> inputs{};
    std::size_t dword_offset{};
};

enum class IrV1Condition : std::uint8_t { always, scc_clear, scc_set, exec_zero };

struct IrV1Jump {
    IrV1Condition condition{};
    std::size_t taken{};       // Index of the destination block.
    std::size_t fallthrough{}; // Used by a conditional jump.
};

struct IrV1Block {
    std::size_t dword_offset{};
    std::vector<IrV1Operation> operations;
    std::optional<IrV1Jump> jump;  // Absent at s_endpgm.
};

class IrProgramV1 {
public:
    [[nodiscard]] std::uint32_t initial_exec() const noexcept { return initial_exec_; }
    [[nodiscard]] std::span<const IrV1Block> blocks() const noexcept { return blocks_; }

private:
    friend Result<IrProgramV1> lower_ir_v1(std::span<const DecodedInstruction>, std::uint32_t);
    IrProgramV1() = default;

    std::uint32_t initial_exec_{};
    std::vector<IrV1Block> blocks_;
};

// Checks the ABI v1 instruction forms and the full forward control-flow
// graph, then lowers with the supplied initial EXEC mask. Errors use
// invalid_decoded_instruction, invalid_program, unsupported_abi or
// reserved_register. Before dispatch, the caller must validate executed
// reads, waits and the floating-point domain for the specific test inputs.
[[nodiscard]] Result<IrProgramV1> lower_ir_v1(std::span<const DecodedInstruction> instructions,
                                             std::uint32_t initial_exec);

}  // namespace scale::shader
