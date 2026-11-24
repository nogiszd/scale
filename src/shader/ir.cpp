#include "scale/shader/ir.h"

#include <bitset>
#include <format>
#include <string_view>
#include <utility>

namespace scale::shader {
namespace {

constexpr std::uint8_t kWordBytes = 4;
constexpr std::uint8_t kAddressVgpr = 1;
constexpr std::uint8_t kInputSgprBase = 0;
constexpr std::uint8_t kOutputSgprBase = 4;
constexpr std::uint8_t kResourceSgprCount = 4;

class Lowerer {
public:
    Result<IrProgram> run(std::span<const DecodedInstruction> instructions) {
        if (instructions.empty()) return error("invalid_program", "expected a terminal s_endpgm");
        initialize_wave();

        for (const auto& instruction : instructions) {
            current_ = &instruction;
            if (instruction.opcode == Opcode::s_endpgm) {
                if (!std::holds_alternative<std::monostate>(instruction.operands)) {
                    return error("invalid_decoded_instruction", "s_endpgm has unexpected operands");
                }
                if (&instruction != &instructions.back() || !stored_) {
                    return error("invalid_program", "s_endpgm must end the stream immediately after the output store");
                }
                append({.opcode = IrOpcode::end});
                return std::move(program_);
            }
            if (stored_) return error("invalid_program", "expected s_endpgm immediately after the output store");
            if (auto result = lower_instruction(); !result) return std::unexpected(std::move(result.error()));
        }
        return error("invalid_program", "expected a terminal s_endpgm");
    }

private:
    using Register = std::optional<IrValueId>;

    void initialize_wave() {
        program_.exec = constant(IrType::mask32, 0xffffffffu);
        program_.vcc = constant(IrType::mask32, 0);
        program_.entry_scc = constant(IrType::flag, 0);
        program_.exit_scc = program_.entry_scc;
        const auto lane = append({.opcode = IrOpcode::lane_index,
                                  .result = IrValueType{IrType::u32, IrLanes::per_lane}});
        vgprs_[0] = lane;
        const auto word_bytes = constant(IrType::u32, kWordBytes);
        vgprs_[kAddressVgpr] = append({.opcode = IrOpcode::umul,
                                     .result = IrValueType{IrType::u32, IrLanes::per_lane},
                                     .inputs = {lane, word_bytes}, .input_count = 2});
    }

    IrValueId append(IrInstruction instruction) {
        if (current_) instruction.dword_offset = current_->dword_offset;
        const auto id = program_.instructions.size();
        program_.instructions.push_back(instruction);
        return id;
    }

    IrValueId constant(IrType type, std::uint32_t bits) {
        return append({.opcode = IrOpcode::constant,
                       .result = IrValueType{type, IrLanes::uniform}, .constant_bits = bits});
    }

    IrValueId as_type(IrValueId id, IrType type) {
        const auto old_type = *program_.instructions[id].result;
        if (old_type.type == type) return id;
        return append({.opcode = IrOpcode::bitcast, .result = IrValueType{type, old_type.lanes},
                       .inputs = {id}, .input_count = 1});
    }

    std::unexpected<Error> error(std::string_view code, std::string_view detail) const {
        if (!current_) return fail("shader", code, std::format("IR lowering: {}", detail));
        return fail("shader", code, std::format("IR lowering at dword 0x{:x} (opcode {}): {}",
                    current_->dword_offset, static_cast<unsigned>(current_->opcode), detail));
    }

    Result<Register*> location(const Operand& operand) {
        if (operand.kind == OperandKind::sgpr && operand.value < sgprs_.size()) {
            if (operand.value < kOutputSgprBase + kResourceSgprCount) {
                return error("reserved_register", "s0 through s7 are opaque resource descriptors");
            }
            return &sgprs_[operand.value];
        }
        if (operand.kind == OperandKind::vgpr && operand.value < vgprs_.size()) {
            return &vgprs_[operand.value];
        }
        return error("invalid_decoded_instruction", "expected an in-range general-purpose register");
    }

    Result<Register*> writable(const Operand& operand, OperandKind required) {
        if (operand.kind != required) {
            return error("invalid_decoded_instruction", "destination register class does not match the opcode");
        }
        const auto slot = location(operand);
        if (!slot) return slot;
        if (operand.kind == OperandKind::vgpr) {
            if (operand.value == kAddressVgpr) return error("reserved_register", "v1 is immutable");
            if (pending_loads_[operand.value]) {
                return error("load_not_ready", "cannot overwrite a pending load destination before s_waitcnt vmcnt(0)");
            }
        }
        return slot;
    }

    Result<IrValueId> read(const Operand& operand, IrLanes lanes) {
        if ((operand.kind == OperandKind::immediate) != (current_->opcode == Opcode::s_movk_i32)) {
            return error("invalid_decoded_instruction", "immediate operand must be used only by s_movk_i32");
        }
        switch (operand.kind) {
        case OperandKind::inline_constant:
        case OperandKind::literal_constant:
        case OperandKind::immediate:
            return constant(IrType::u32, operand.value);
        case OperandKind::sgpr:
        case OperandKind::vgpr: {
            if (lanes == IrLanes::uniform && operand.kind == OperandKind::vgpr) {
                return error("invalid_decoded_instruction", "scalar operation has a VGPR source");
            }
            const auto slot = location(operand);
            if (!slot) return std::unexpected(slot.error());
            if (operand.kind == OperandKind::vgpr && pending_loads_[operand.value]) {
                return error("load_not_ready", "cannot read a pending load destination before s_waitcnt vmcnt(0)");
            }
            const auto& value = **slot;
            if (!value) {
                return error("undefined_register", std::format("read of undefined {}{}",
                    operand.kind == OperandKind::sgpr ? 's' : 'v', operand.value));
            }
            return *value;
        }
        }
        return error("invalid_decoded_instruction", "unknown source operand kind");
    }

    Result<void> alu(IrOpcode opcode, IrLanes lanes, IrType type = IrType::u32) {
        const auto* operands = std::get_if<AluOperands>(&current_->operands);
        const std::uint8_t arity = opcode == IrOpcode::copy ? 1 : 2;
        if (!operands || operands->source_count != arity) {
            return error("invalid_decoded_instruction", "ALU source count does not match the opcode");
        }
        const auto destination = writable(operands->destination,
                                          lanes == IrLanes::uniform ? OperandKind::sgpr : OperandKind::vgpr);
        if (!destination) return std::unexpected(destination.error());

        IrInstruction operation{.opcode = opcode, .result = IrValueType{type, lanes}, .input_count = arity};
        if (lanes == IrLanes::per_lane) operation.exec = program_.exec;
        for (std::size_t i = 0; i < arity; ++i) {
            const auto source = read(operands->sources[i], lanes);
            if (!source) return std::unexpected(source.error());
            operation.inputs[i] = as_type(*source, type);
        }
        const auto result = append(operation);
        **destination = result;
        if (current_->opcode == Opcode::s_add_u32) {
            // AMD RDNA 2 ISA 70648 §12.1. SCC is the unsigned carry out.
            program_.exit_scc = append({.opcode = IrOpcode::uadd_carry,
                                       .result = IrValueType{IrType::flag, IrLanes::uniform},
                                       .inputs = {result, operation.inputs[0]}, .input_count = 2});
        }
        return {};
    }

    Result<void> buffer(bool load) {
        const auto* operands = std::get_if<BufferOperands>(&current_->operands);
        if (!operands) return error("invalid_decoded_instruction", "expected MUBUF operands");
        const auto required_resource = load ? kInputSgprBase : kOutputSgprBase;
        if (operands->resource_sgpr_base != required_resource || operands->address_vgpr != kAddressVgpr ||
                operands->immediate_offset_bytes != 0 || operands->scalar_offset_bytes.value != 0 ||
                operands->scalar_offset_bytes.kind != OperandKind::inline_constant) {
            return error("unsupported_abi", "MUBUF must use the ABI resource tuple, v1 and zero offsets");
        }
        const auto byte_offset = *vgprs_[kAddressVgpr];
        const Operand data{OperandKind::vgpr, operands->data_vgpr, operands->data_vgpr};
        if (load) {
            const auto destination = writable(data, OperandKind::vgpr);
            if (!destination) return std::unexpected(destination.error());
            **destination = append({.opcode = IrOpcode::load_input,
                                    .result = IrValueType{IrType::u32, IrLanes::per_lane},
                                    .inputs = {byte_offset}, .input_count = 1, .exec = program_.exec});
            pending_loads_.set(operands->data_vgpr);
        } else {
            if (pending_loads_.any()) {
                return error("load_not_ready", "all input loads require s_waitcnt vmcnt(0) before the output store");
            }
            const auto source = read(data, IrLanes::per_lane);
            if (!source) return std::unexpected(source.error());
            const auto bits = as_type(*source, IrType::u32);
            append({.opcode = IrOpcode::store_output, .inputs = {byte_offset, bits},
                    .input_count = 2, .exec = program_.exec});
            stored_ = true;
        }
        return {};
    }

    Result<void> wait() {
        const auto* counters = std::get_if<WaitCounters>(&current_->operands);
        if (!counters) return error("invalid_decoded_instruction", "expected wait counters");
        if (counters->vmcnt != 0 || counters->expcnt != 7 || counters->lgkmcnt != 63) {
            return error("unsupported_abi", "ABI v0 requires s_waitcnt vmcnt(0) with the other counters at their maximums");
        }
        pending_loads_.reset();
        append({.opcode = IrOpcode::wait_vmem});
        return {};
    }

    Result<void> lower_instruction() {
        switch (current_->opcode) {
        case Opcode::s_mov_b32:
        case Opcode::s_movk_i32: return alu(IrOpcode::copy, IrLanes::uniform);
        case Opcode::s_add_u32: return alu(IrOpcode::uadd, IrLanes::uniform);
        case Opcode::s_waitcnt: return wait();
        case Opcode::v_mov_b32: return alu(IrOpcode::copy, IrLanes::per_lane);
        case Opcode::v_add_nc_u32: return alu(IrOpcode::uadd, IrLanes::per_lane);
        case Opcode::v_mul_lo_u32: return alu(IrOpcode::umul, IrLanes::per_lane);
        case Opcode::v_add_f32: return alu(IrOpcode::fadd, IrLanes::per_lane, IrType::f32);
        case Opcode::v_mul_f32: return alu(IrOpcode::fmul, IrLanes::per_lane, IrType::f32);
        case Opcode::buffer_load_dword: return buffer(true);
        case Opcode::buffer_store_dword: return buffer(false);
        case Opcode::s_endpgm: break;  // Handled by run with the stream-level checks.
        }
        return error("invalid_decoded_instruction", "opcode cannot be lowered");
    }

    IrProgram program_{};
    const DecodedInstruction* current_{};
    std::array<Register, 106> sgprs_{};
    std::array<Register, 256> vgprs_{};
    std::bitset<256> pending_loads_;
    bool stored_{};
};

}  // namespace

Result<IrProgram> lower_ir(std::span<const DecodedInstruction> instructions) {
    return Lowerer{}.run(instructions);
}

}  // namespace scale::shader
