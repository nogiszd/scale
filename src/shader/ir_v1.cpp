#include "scale/shader/ir.h"

#include "ir_abi.h"

#include <algorithm>
#include <bit>
#include <format>
#include <utility>

namespace scale::shader {
namespace {

using namespace detail;

std::unexpected<Error> lower_error(const DecodedInstruction& inst, std::string_view code, std::string_view detail) {
    return fail("shader", code, std::format("IR lowering at dword 0x{:x} ({}): {}",
                                             inst.dword_offset, opcode_name(inst.opcode), detail));
}

std::optional<IrV1Condition> branch_condition(Opcode opcode) {
    switch (opcode) {
    case Opcode::s_branch: return IrV1Condition::always;
    case Opcode::s_cbranch_scc0: return IrV1Condition::scc_clear;
    case Opcode::s_cbranch_scc1: return IrV1Condition::scc_set;
    case Opcode::s_cbranch_execz: return IrV1Condition::exec_zero;
    default: return std::nullopt;
    }
}

class OperationLowerer {
public:
    explicit OperationLowerer(const DecodedInstruction& inst) : inst_(inst) {}

    Result<IrV1Operation> lower() const {
        switch (inst_.opcode) {
        case Opcode::s_mov_b32:
        case Opcode::s_movk_i32: return alu(IrV1Opcode::copy, IrLanes::uniform, 1);
        case Opcode::s_add_u32: return alu(IrV1Opcode::uadd_scc, IrLanes::uniform, 2);
        case Opcode::s_and_saveexec_b32: return alu(IrV1Opcode::save_exec, IrLanes::uniform, 1);
        case Opcode::v_mov_b32: return alu(IrV1Opcode::copy, IrLanes::per_lane, 1);
        case Opcode::v_add_nc_u32: return alu(IrV1Opcode::uadd, IrLanes::per_lane, 2);
        case Opcode::v_mul_lo_u32: return alu(IrV1Opcode::umul, IrLanes::per_lane, 2);
        case Opcode::v_add_f32: return alu(IrV1Opcode::fadd, IrLanes::per_lane, 2);
        case Opcode::v_mul_f32: return alu(IrV1Opcode::fmul, IrLanes::per_lane, 2);
        case Opcode::v_cmp_eq_u32: return compare(IrV1Opcode::compare_eq);
        case Opcode::v_cmp_lt_u32: return compare(IrV1Opcode::compare_lt);
        case Opcode::buffer_load_dword: return buffer(true);
        case Opcode::buffer_store_dword: return buffer(false);
        case Opcode::s_waitcnt: {
            const auto* counters = std::get_if<WaitCounters>(&inst_.operands);
            if (counters == nullptr) return invalid("expected wait counters");
            if (!matches_wait_abi(*counters)) return unsupported("ABI v1 requires s_waitcnt vmcnt(0)");
            return IrV1Operation{.opcode = IrV1Opcode::wait_vmem, .dword_offset = inst_.dword_offset};
        }
        default: return invalid("opcode cannot be lowered");
        }
    }

private:
    std::unexpected<Error> invalid(std::string_view detail) const {
        return lower_error(inst_, "invalid_decoded_instruction", detail);
    }

    std::unexpected<Error> unsupported(std::string_view detail) const {
        return lower_error(inst_, "unsupported_abi", detail);
    }

    Result<IrV1Register> gpr(const Operand& operand, bool writable) const {
        if (operand.kind == OperandKind::sgpr && operand.value <= 105) {
            if (operand.value < kOutputSgprBase + kResourceSgprCount) {
                return lower_error(inst_, "reserved_register", "s0 through s7 are opaque resource descriptors");
            }
            return IrV1Register{IrV1RegisterKind::sgpr, static_cast<std::uint8_t>(operand.value)};
        }
        if (operand.kind == OperandKind::vgpr && operand.value <= 255) {
            if (writable && operand.value == kAddressVgpr) {
                return lower_error(inst_, "reserved_register", "v1 is immutable");
            }
            return IrV1Register{IrV1RegisterKind::vgpr, static_cast<std::uint8_t>(operand.value)};
        }
        return invalid("expected an in-range general-purpose register");
    }

    Result<IrV1Operand> read(const Operand& operand, IrLanes lanes, bool may_read_vcc = false) const {
        if ((inst_.opcode == Opcode::s_movk_i32) != (operand.kind == OperandKind::immediate)) {
            return invalid("expected a SIMM16 immediate only for s_movk_i32");
        }
        switch (operand.kind) {
        case OperandKind::inline_constant:
        case OperandKind::literal_constant:
        case OperandKind::immediate:
            return operand.value;
        case OperandKind::sgpr:
        case OperandKind::vgpr: {
            if (lanes == IrLanes::uniform && operand.kind == OperandKind::vgpr) {
                return invalid("scalar operation has a VGPR source");
            }
            const auto reg = gpr(operand, false);
            if (!reg) return std::unexpected(reg.error());
            return *reg;
        }
        case OperandKind::vcc_lo:
            if (may_read_vcc && operand.encoded == 106 && operand.value == 106) {
                return IrV1Register{IrV1RegisterKind::vcc};
            }
            return unsupported("vcc_lo is only a source for save and restore EXEC");
        case OperandKind::exec_lo:
            return unsupported("exec_lo is only an explicit destination");
        }
        return invalid("unknown source operand kind");
    }

    Result<IrV1Register> destination(const Operand& operand, IrLanes lanes) const {
        if (inst_.opcode == Opcode::s_mov_b32 && operand.kind == OperandKind::exec_lo) {
            if (operand.encoded != 126 || operand.value != 126) return invalid("invalid exec_lo selector");
            return IrV1Register{IrV1RegisterKind::exec};
        }
        const auto expected = lanes == IrLanes::uniform ? OperandKind::sgpr : OperandKind::vgpr;
        if (operand.kind != expected) return invalid("destination register class does not match the opcode");
        return gpr(operand, true);
    }

    Result<IrV1Operation> alu(IrV1Opcode opcode, IrLanes lanes, std::uint8_t arity) const {
        const auto* operands = std::get_if<AluOperands>(&inst_.operands);
        if (operands == nullptr || operands->source_count != arity) {
            return invalid("ALU source count does not match the opcode");
        }
        const auto dest = destination(operands->destination, lanes);
        if (!dest) return std::unexpected(dest.error());
        const bool may_read_vcc = opcode == IrV1Opcode::save_exec || dest->kind == IrV1RegisterKind::exec;
        IrV1Operation op{.opcode = opcode, .lanes = lanes, .destination = *dest, .dword_offset = inst_.dword_offset};
        for (std::size_t i = 0; i < arity; ++i) {
            const auto input = read(operands->sources[i], lanes, may_read_vcc);
            if (!input) return std::unexpected(input.error());
            op.inputs[i] = *input;
        }
        return op;
    }

    Result<IrV1Operation> compare(IrV1Opcode opcode) const {
        const auto* operands = std::get_if<CompareOperands>(&inst_.operands);
        if (inst_.encoding != Encoding::vopc || operands == nullptr || operands->sources[1].kind != OperandKind::vgpr) {
            return invalid("expected VOPC operands with a VGPR second source");
        }
        const auto first = read(operands->sources[0], IrLanes::per_lane);
        if (!first) return std::unexpected(first.error());
        const auto second = read(operands->sources[1], IrLanes::per_lane);
        if (!second) return std::unexpected(second.error());
        return IrV1Operation{.opcode = opcode, .lanes = IrLanes::per_lane,
                             .destination = IrV1Register{IrV1RegisterKind::vcc},
                             .inputs = {*first, *second}, .dword_offset = inst_.dword_offset};
    }

    Result<IrV1Operation> buffer(bool load) const {
        const auto* operands = std::get_if<BufferOperands>(&inst_.operands);
        if (operands == nullptr) return invalid("expected MUBUF operands");
        if (!matches_buffer_abi(*operands, load)) {
            return unsupported("MUBUF must use the ABI resource tuple, v1 and zero offsets");
        }
        const Operand data{OperandKind::vgpr, operands->data_vgpr, operands->data_vgpr};
        const auto reg = gpr(data, load);
        if (!reg) return std::unexpected(reg.error());
        if (load) return IrV1Operation{.opcode = IrV1Opcode::load_input, .lanes = IrLanes::per_lane,
                                       .destination = *reg, .dword_offset = inst_.dword_offset};
        return IrV1Operation{.opcode = IrV1Opcode::store_output, .lanes = IrLanes::per_lane,
                             .inputs = {*reg}, .dword_offset = inst_.dword_offset};
    }

    const DecodedInstruction& inst_;
};

Result<std::size_t> branch_target(std::span<const DecodedInstruction> code, std::size_t index) {
    const auto& inst = code[index];
    const auto* branch = std::get_if<BranchOperands>(&inst.operands);
    if (inst.encoding != Encoding::sopp || inst.word_count != 1 || branch == nullptr) {
        return lower_error(inst, "invalid_decoded_instruction", "expected a one-dword SOPP branch");
    }
    if (!std::cmp_greater(branch->target_dword, inst.dword_offset) ||
            !std::cmp_less_equal(branch->target_dword, code[code.size() - 2].dword_offset)) {
        return lower_error(inst, "invalid_program", "branch must target a later instruction at or before the store");
    }
    const auto offset = static_cast<std::size_t>(branch->target_dword);
    const auto target = std::ranges::lower_bound(code, offset, {}, &DecodedInstruction::dword_offset);
    if (target == code.end() || target->dword_offset != offset) {
        return lower_error(inst, "invalid_program", "branch target must start an instruction");
    }
    if (!std::cmp_equal(std::bit_cast<std::int16_t>(branch->simm16), offset - inst.dword_offset - 1)) {
        return lower_error(inst, "invalid_decoded_instruction", "SIMM16 and the resolved branch target disagree");
    }
    return static_cast<std::size_t>(target - code.begin());
}

}  // namespace

Result<IrProgramV1> lower_ir_v1(std::span<const DecodedInstruction> code, std::uint32_t initial_exec) {
    if (code.size() < 2 || code.back().opcode != Opcode::s_endpgm ||
            code[code.size() - 2].opcode != Opcode::buffer_store_dword) {
        return fail("shader", "invalid_program", "IR lowering requires a terminal output store and s_endpgm");
    }
    std::size_t next_dword = 0;
    for (std::size_t i = 0; i < code.size(); ++i) {
        const auto& inst = code[i];
        const auto next = checked_add(next_dword, static_cast<std::size_t>(inst.word_count));
        if (!next || inst.dword_offset != next_dword || inst.word_count == 0 || inst.word_count > 3) {
            return lower_error(inst, "invalid_decoded_instruction", "invalid decoded stream offsets or word counts");
        }
        next_dword = *next;
        if ((inst.opcode == Opcode::buffer_store_dword && i != code.size() - 2) ||
                (inst.opcode == Opcode::s_endpgm && i != code.size() - 1)) {
            return lower_error(inst, "invalid_program", "expected one output store directly before the only s_endpgm");
        }
    }
    if (!std::holds_alternative<std::monostate>(code.back().operands)) {
        return lower_error(code.back(), "invalid_decoded_instruction", "s_endpgm has unexpected operands");
    }

    // Split at every target and after every branch, including unreachable
    // blocks so that their operand forms receive the same static checks.
    std::vector<std::size_t> starts{0};
    std::vector<std::optional<IrV1Jump>> branches(code.size());
    for (std::size_t i = 0; i < code.size(); ++i) {
        const auto condition = branch_condition(code[i].opcode);
        if (!condition) continue;
        const auto target = branch_target(code, i);
        if (!target) return std::unexpected(target.error());
        starts.push_back(*target);
        starts.push_back(i + 1);
        branches[i] = IrV1Jump{*condition, *target, i + 1};
    }
    std::ranges::sort(starts);
    starts.erase(std::ranges::unique(starts).begin(), starts.end());
    const auto block_for = [&](std::size_t index) {
        return static_cast<std::size_t>(std::ranges::lower_bound(starts, index) - starts.begin());
    };

    IrProgramV1 program;
    program.initial_exec_ = initial_exec;
    program.blocks_.reserve(starts.size());
    for (std::size_t block = 0; block < starts.size(); ++block) {
        const auto begin = starts[block];
        const auto end = block + 1 < starts.size() ? starts[block + 1] : code.size();
        IrV1Block lowered{.dword_offset = code[begin].dword_offset, .operations = {}, .jump = {}};
        for (std::size_t i = begin; i < end; ++i) {
            if (const auto& branch = branches[i]) {
                lowered.jump = IrV1Jump{branch->condition, block_for(branch->taken), block_for(branch->fallthrough)};
            } else if (code[i].opcode != Opcode::s_endpgm) {
                const auto operation = OperationLowerer(code[i]).lower();
                if (!operation) return std::unexpected(operation.error());
                lowered.operations.push_back(*operation);
            }
        }
        if (!lowered.jump && end < code.size()) {
            lowered.jump = IrV1Jump{IrV1Condition::always, block + 1};
        }
        program.blocks_.push_back(std::move(lowered));
    }
    return program;
}

}  // namespace scale::shader
