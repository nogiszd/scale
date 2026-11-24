#include "disassemble.h"

#include <bit>
#include <format>
#include <string_view>

namespace scale::shader::cli {
namespace {

std::string_view mnemonic(Opcode opcode) {
    switch (opcode) {
    case Opcode::s_mov_b32: return "s_mov_b32";
    case Opcode::s_add_u32: return "s_add_u32";
    case Opcode::s_movk_i32: return "s_movk_i32";
    case Opcode::s_endpgm: return "s_endpgm";
    case Opcode::s_waitcnt: return "s_waitcnt";
    case Opcode::v_mov_b32: return "v_mov_b32";
    case Opcode::v_add_nc_u32: return "v_add_nc_u32";
    case Opcode::v_mul_lo_u32: return "v_mul_lo_u32";
    case Opcode::v_add_f32: return "v_add_f32";
    case Opcode::v_mul_f32: return "v_mul_f32";
    case Opcode::buffer_load_dword: return "buffer_load_dword";
    case Opcode::buffer_store_dword: return "buffer_store_dword";
    case Opcode::v_cmp_eq_u32: return "v_cmp_eq_u32";
    case Opcode::v_cmp_lt_u32: return "v_cmp_lt_u32";
    case Opcode::s_and_saveexec_b32: return "s_and_saveexec_b32";
    case Opcode::s_cbranch_scc0: return "s_cbranch_scc0";
    case Opcode::s_cbranch_scc1: return "s_cbranch_scc1";
    case Opcode::s_cbranch_execz: return "s_cbranch_execz";
    case Opcode::s_branch: return "s_branch";
    }
    return "<unknown opcode>";
}

std::string operand_text(const Operand& operand) {
    switch (operand.kind) {
    case OperandKind::sgpr: return std::format("s{}", operand.value);
    case OperandKind::vgpr: return std::format("v{}", operand.value);
    case OperandKind::vcc_lo: return "vcc_lo";
    case OperandKind::exec_lo: return "exec_lo";
    case OperandKind::literal_constant: return std::format("0x{:08x}", operand.value);
    case OperandKind::inline_constant:
        if (operand.encoded >= 240) {
            auto text = std::format("{}", std::bit_cast<float>(operand.value));
            if (text.find_first_of(".eE") == std::string::npos) text += ".0";
            return text;
        }
        [[fallthrough]];
    case OperandKind::immediate: return std::format("{}", std::bit_cast<std::int32_t>(operand.value));
    }
    return "<unknown operand>";
}

}  // namespace

std::string disassemble(const DecodedInstruction& instruction) {
    std::string text(mnemonic(instruction.opcode));
    if (instruction.encoding == Encoding::vop1 || instruction.encoding == Encoding::vop2 ||
            instruction.encoding == Encoding::vopc) {
        text += "_e32";
    } else if (instruction.encoding == Encoding::vop3 && instruction.opcode != Opcode::v_mul_lo_u32) {
        text += "_e64";
    }
    if (const auto* alu = std::get_if<AluOperands>(&instruction.operands)) {
        text += " " + operand_text(alu->destination);
        for (const auto& source : std::span(alu->sources).first(alu->source_count)) {
            text += ", " + operand_text(source);
        }
    } else if (const auto* compare = std::get_if<CompareOperands>(&instruction.operands)) {
        text += std::format(" vcc_lo, {}, {}", operand_text(compare->sources[0]), operand_text(compare->sources[1]));
    } else if (const auto* branch = std::get_if<BranchOperands>(&instruction.operands)) {
        text += std::format(" {:#x} (simm16 {:+})", branch->target_dword, std::bit_cast<std::int16_t>(branch->simm16));
    } else if (const auto* wait = std::get_if<WaitCounters>(&instruction.operands)) {
        text += std::format(" vmcnt({}) expcnt({}) lgkmcnt({})", wait->vmcnt, wait->expcnt, wait->lgkmcnt);
    } else if (const auto* buffer = std::get_if<BufferOperands>(&instruction.operands)) {
        text += std::format(" v{}, v{}, s[{}:{}], {} offen", buffer->data_vgpr, buffer->address_vgpr,
                            buffer->resource_sgpr_base, buffer->resource_sgpr_base + 3,
                            operand_text(buffer->scalar_offset_bytes));
        if (buffer->immediate_offset_bytes != 0) text += std::format(" offset:{}", buffer->immediate_offset_bytes);
    }

    std::string raw;
    for (const auto word : std::span(instruction.raw_words).first(instruction.word_count)) {
        if (!raw.empty()) raw += ' ';
        raw += std::format("{:08x}", word);
    }
    return std::format("{:04x}:  {:<26}  {}", instruction.dword_offset, raw, text);
}

}  // namespace scale::shader::cli
