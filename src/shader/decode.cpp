#include "scale/shader/decode.h"

#include <bit>
#include <format>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace scale::shader {
namespace {

// AMD RDNA 2 ISA reference (70648), sections 5.2, 6.2 and chapters 12–13.
constexpr std::size_t kDwordSize = sizeof(std::uint32_t);
constexpr std::uint16_t kLastSgpr = 105;
constexpr std::uint16_t kVccLoSelector = 106;
constexpr std::uint16_t kExecLoSelector = 126;
constexpr std::uint16_t kLiteralSelector = 255;
constexpr std::uint16_t kFirstVgprSelector = 256;

constexpr std::uint16_t field(std::uint32_t word, unsigned shift, std::uint32_t mask) {
    return static_cast<std::uint16_t>((word >> shift) & mask);
}

constexpr Operand reg(OperandKind kind, std::uint16_t index) {
    return Operand{kind, index, index};
}

std::optional<Operand> register_or_inline(std::uint16_t encoded) {
    if (encoded >= kFirstVgprSelector) {
        return Operand{OperandKind::vgpr, encoded, static_cast<std::uint32_t>(encoded) - kFirstVgprSelector};
    }
    if (encoded <= kLastSgpr) {
        return reg(OperandKind::sgpr, encoded);
    }
    if (encoded >= 128 && encoded <= 192) {
        return Operand{OperandKind::inline_constant, encoded, static_cast<std::uint32_t>(encoded) - 128u};
    }
    if (encoded >= 193 && encoded <= 208) {
        return Operand{OperandKind::inline_constant, encoded, 192u - encoded};
    }
    constexpr std::array<float, 9> float_constants{
        0.5f, -0.5f, 1.0f, -1.0f, 2.0f, -2.0f, 4.0f, -4.0f, 0.15915494309189532f,
    };
    if (encoded >= 240 && encoded <= 248) {
        return Operand{OperandKind::inline_constant, encoded,
                       std::bit_cast<std::uint32_t>(float_constants[encoded - 240])};
    }
    return std::nullopt;
}

class InstructionDecoder {
public:
    InstructionDecoder(ByteView code, std::size_t byte_offset) : code_(code), byte_offset_(byte_offset) {
        instruction_.dword_offset = byte_offset / kDwordSize;
    }

    Result<DecodedInstruction> decode_one() {
        const auto first = read_word();
        if (!first) {
            return error("truncated_encoding", "missing instruction dword");
        }
        const auto word = *first;
        // More specific scalar/vector prefixes precede their enclosing classes.
        switch (word >> 23) {
        case 0x17d: return sop1(word);
        case 0x17e: return error("unsupported_encoding", "SOPC is outside the subset");
        case 0x17f: return sopp(word);
        default: break;
        }
        if (word >> 28 == 0xb) return sopk(word);
        if (word >> 30 == 0b10) return sop2(word);
        if (word >> 25 == 0x3f) return vop1(word);
        if (word >> 25 == 0x3e) return vopc(word);
        if (word >> 31 == 0) return vop2(word);
        if (word >> 26 == 0x35) return vop3(word);
        if (word >> 26 == 0x38) return mubuf(word);
        return error("unsupported_encoding", "unknown or unsupported instruction format");
    }

private:
    std::optional<std::uint32_t> read_word() {
        const auto word = code_.u32(byte_offset_ + instruction_.word_count * kDwordSize);
        if (word) {
            instruction_.raw_words[instruction_.word_count++] = *word;
        }
        return word;
    }

    std::unexpected<Error> error(std::string_view code, std::string_view detail) const {
        std::string message = std::format("dword 0x{:x}: {}; raw words:", instruction_.dword_offset, detail);
        for (std::uint8_t i = 0; i < instruction_.word_count; ++i) {
            message += std::format(" 0x{:08x}", instruction_.raw_words[i]);
        }
        if (instruction_.word_count == 0) {
            message += " (none)";
        }
        return fail("shader", code, std::move(message));
    }

    std::unexpected<Error> unknown_opcode(std::string_view encoding, std::uint16_t opcode) const {
        return error("unsupported_encoding", std::format("unsupported {} opcode 0x{:x}", encoding, opcode));
    }

    DecodedInstruction finish(Encoding encoding, Opcode opcode) {
        instruction_.encoding = encoding;
        instruction_.opcode = opcode;
        return instruction_;
    }

    Result<Operand> source_operand(std::uint16_t encoded, std::optional<std::uint32_t> literal) const {
        if (encoded == kLiteralSelector && literal) {
            return Operand{OperandKind::literal_constant, encoded, *literal};
        }
        const auto resolved = register_or_inline(encoded);
        if (resolved) return *resolved;
        return error("unsupported_operand", std::format("unsupported source selector 0x{:x}", encoded));
    }

    Result<DecodedInstruction> alu(Encoding encoding, Opcode opcode, Operand destination,
                                   std::initializer_list<std::uint16_t> source_fields) {
        std::optional<std::uint32_t> literal;
        std::size_t index = 0;
        for (const auto encoded : source_fields) {
            // VOP2's second input is a dedicated 8-bit VGPR field.
            if (encoded == kLiteralSelector && (encoding != Encoding::vop2 || index == 0)) {
                literal = read_word();
                if (!literal) return error("truncated_encoding", "missing literal dword");
                break;  // All literal selectors in an instruction share one dword.
            }
            ++index;
        }
        if (destination.kind == OperandKind::sgpr && destination.encoded > kLastSgpr) {
            return error("unsupported_operand", "scalar destination must be an SGPR");
        }

        const bool reads_vcc = opcode == Opcode::s_and_saveexec_b32 || destination.kind == OperandKind::exec_lo;
        AluOperands operands{destination};
        for (const auto encoded : source_fields) {
            auto& source = operands.sources[operands.source_count];
            if (encoding == Encoding::vop2 && operands.source_count == 1) {
                source = reg(OperandKind::vgpr, encoded);
            } else if (reads_vcc && encoded == kVccLoSelector) {
                source = reg(OperandKind::vcc_lo, encoded);
            } else {
                auto resolved = source_operand(encoded, literal);
                if (!resolved) return std::unexpected(std::move(resolved.error()));
                source = *resolved;
            }
            ++operands.source_count;
        }
        instruction_.operands = operands;
        return finish(encoding, opcode);
    }

    Result<DecodedInstruction> sop1(std::uint32_t word) {
        const auto opcode = field(word, 8, 0xff);
        const auto dest = field(word, 16, 0x7f);
        const auto src = field(word, 0, 0xff);
        switch (opcode) {
        case 3:
            return alu(Encoding::sop1, Opcode::s_mov_b32,
                       reg(dest == kExecLoSelector ? OperandKind::exec_lo : OperandKind::sgpr, dest), {src});
        case 60: return alu(Encoding::sop1, Opcode::s_and_saveexec_b32, reg(OperandKind::sgpr, dest), {src});
        default: return unknown_opcode("SOP1", opcode);
        }
    }

    Result<DecodedInstruction> sop2(std::uint32_t word) {
        const auto opcode = field(word, 23, 0x7f);
        if (opcode != 0) return unknown_opcode("SOP2", opcode);
        return alu(Encoding::sop2, Opcode::s_add_u32, reg(OperandKind::sgpr, field(word, 16, 0x7f)),
                   {field(word, 0, 0xff), field(word, 8, 0xff)});
    }

    Result<DecodedInstruction> sopk(std::uint32_t word) {
        const auto opcode = field(word, 23, 0x1f);
        if (opcode != 0) return unknown_opcode("SOPK", opcode);
        const auto dest = field(word, 16, 0x7f);
        if (dest > kLastSgpr) return error("unsupported_operand", "scalar destination must be an SGPR");
        const auto immediate = field(word, 0, 0xffff);
        const auto signed_value = std::bit_cast<std::int16_t>(immediate);
        AluOperands operands{reg(OperandKind::sgpr, dest)};
        operands.sources[0] = Operand{OperandKind::immediate, immediate, static_cast<std::uint32_t>(signed_value)};
        operands.source_count = 1;
        instruction_.operands = operands;
        return finish(Encoding::sopk, Opcode::s_movk_i32);
    }

    Result<DecodedInstruction> sopp(std::uint32_t word) {
        const auto opcode = field(word, 16, 0x7f);
        const auto immediate = field(word, 0, 0xffff);
        switch (opcode) {
        case 1:
            if (immediate != 0) return error("unsupported_modifier", "s_endpgm immediate must be zero");
            return finish(Encoding::sopp, Opcode::s_endpgm);
        case 2: return branch(Opcode::s_branch, immediate);
        case 4: return branch(Opcode::s_cbranch_scc0, immediate);
        case 5: return branch(Opcode::s_cbranch_scc1, immediate);
        case 8: return branch(Opcode::s_cbranch_execz, immediate);
        case 12: {
            if (immediate & 0x80) return error("unsupported_modifier", "s_waitcnt reserved bit 7 is set");
            const auto vmcnt = (field(immediate, 14, 3) << 4) | field(immediate, 0, 0xf);
            instruction_.operands = WaitCounters{static_cast<std::uint8_t>(vmcnt),
                                                 static_cast<std::uint8_t>(field(immediate, 4, 7)),
                                                 static_cast<std::uint8_t>(field(immediate, 8, 0x3f))};
            return finish(Encoding::sopp, Opcode::s_waitcnt);
        }
        default: return unknown_opcode("SOPP", opcode);
        }
    }

    DecodedInstruction branch(Opcode opcode, std::uint16_t immediate) {
        // The next dword offset is at most the byte view's size divided by four.
        // Confirm it fits a signed target with room for a positive SIMM16.
        static_assert(std::cmp_less_equal(std::numeric_limits<std::size_t>::max() / kDwordSize,
                                        std::numeric_limits<std::int64_t>::max() -
                                            std::numeric_limits<std::int16_t>::max()));
        const auto next = static_cast<std::int64_t>(instruction_.dword_offset) + instruction_.word_count;
        instruction_.operands = BranchOperands{immediate, next + std::bit_cast<std::int16_t>(immediate)};
        return finish(Encoding::sopp, opcode);
    }

    Result<DecodedInstruction> vop1(std::uint32_t word) {
        const auto opcode = field(word, 9, 0xff);
        if (opcode != 1) return unknown_opcode("VOP1", opcode);
        return vector_e32(Encoding::vop1, Opcode::v_mov_b32, word);
    }

    Result<DecodedInstruction> vop2(std::uint32_t word) {
        const auto opcode = field(word, 25, 0x3f);
        switch (opcode) {
        case 3: return vector_e32(Encoding::vop2, Opcode::v_add_f32, word);
        case 8: return vector_e32(Encoding::vop2, Opcode::v_mul_f32, word);
        case 37: return vector_e32(Encoding::vop2, Opcode::v_add_nc_u32, word);
        default: return unknown_opcode("VOP2", opcode);
        }
    }

    Result<void> check_vector_e32_modifiers(std::uint16_t src0) {
        // DPP8, DPP8FI, SDWA and DPP16 use a separate second dword.
        if (src0 == 233 || src0 == 234 || src0 == 249 || src0 == 250) {
            if (!read_word()) return error("truncated_encoding", "missing DPP or SDWA dword");
            return error("unsupported_modifier", "DPP and SDWA are outside the subset");
        }
        return {};
    }

    Result<DecodedInstruction> vector_e32(Encoding encoding, Opcode opcode, std::uint32_t word) {
        const auto src0 = field(word, 0, 0x1ff);
        auto modifiers = check_vector_e32_modifiers(src0);
        if (!modifiers) return std::unexpected(std::move(modifiers.error()));
        const auto dest = reg(OperandKind::vgpr, field(word, 17, 0xff));
        if (encoding == Encoding::vop1) return alu(encoding, opcode, dest, {src0});
        return alu(encoding, opcode, dest, {src0, field(word, 9, 0xff)});
    }

    Result<DecodedInstruction> vopc(std::uint32_t word) {
        const auto opcode = field(word, 17, 0xff);
        Opcode decoded{};
        switch (opcode) {
        case 193: decoded = Opcode::v_cmp_lt_u32; break;
        case 194: decoded = Opcode::v_cmp_eq_u32; break;
        default: return unknown_opcode("VOPC", opcode);
        }
        const auto src0 = field(word, 0, 0x1ff);
        auto modifiers = check_vector_e32_modifiers(src0);
        if (!modifiers) return std::unexpected(std::move(modifiers.error()));
        std::optional<std::uint32_t> literal;
        if (src0 == kLiteralSelector) {
            literal = read_word();
            if (!literal) return error("truncated_encoding", "missing literal dword");
        }
        auto source = source_operand(src0, literal);
        if (!source) return std::unexpected(std::move(source.error()));
        instruction_.operands = CompareOperands{{*source, reg(OperandKind::vgpr, field(word, 9, 0xff))}};
        return finish(Encoding::vopc, decoded);
    }

    Result<DecodedInstruction> vop3(std::uint32_t word) {
        const auto second = read_word();
        if (!second) return error("truncated_encoding", "missing VOP3 dword");
        const auto opcode = field(word, 16, 0x3ff);
        Opcode decoded{};
        switch (opcode) {
        case 0x103: decoded = Opcode::v_add_f32; break;
        case 0x108: decoded = Opcode::v_mul_f32; break;
        case 0x125: decoded = Opcode::v_add_nc_u32; break;
        case 0x169: decoded = Opcode::v_mul_lo_u32; break;
        case 0x181: decoded = Opcode::v_mov_b32; break;
        default: return unknown_opcode("VOP3", opcode);
        }
        if ((word & 0xff00u) || (*second & 0xf8000000u)) {
            return error("unsupported_modifier", "VOP3 ABS, OPSEL, CLAMP, OMOD and NEG must be clear");
        }
        const auto dest = reg(OperandKind::vgpr, field(word, 0, 0xff));
        const auto src0 = field(*second, 0, 0x1ff);
        // Only active source fields select operands or consume a literal dword.
        if (decoded == Opcode::v_mov_b32) return alu(Encoding::vop3, decoded, dest, {src0});
        return alu(Encoding::vop3, decoded, dest, {src0, field(*second, 9, 0x1ff)});
    }

    Result<DecodedInstruction> mubuf(std::uint32_t word) {
        const auto second = read_word();
        if (!second) return error("truncated_encoding", "missing MUBUF dword");
        const auto opcode = field(word, 18, 0xff);
        Opcode decoded{};
        switch (opcode) {
        case 12: decoded = Opcode::buffer_load_dword; break;
        case 28: decoded = Opcode::buffer_store_dword; break;
        default: return unknown_opcode("MUBUF", opcode);
        }
        // Bits 12..17 are OFFEN, IDXEN, GLC, DLC, LDS and a reserved bit.
        // Bits 21..23 of dword 1 are reserved, SLC and TFE respectively.
        if ((word & 0x0003f000u) != 0x1000u || (*second & 0x00e00000u)) {
            return error("unsupported_modifier", "MUBUF requires OFFEN alone with optional and reserved bits clear");
        }
        const auto resource = field(*second, 16, 0x1f) * 4;
        if (resource + 3 > kLastSgpr) {
            return error("unsupported_operand", "MUBUF resource must use four general-purpose SGPRs");
        }
        const auto offset = register_or_inline(field(*second, 24, 0xff));
        if (!offset) return error("unsupported_operand", "MUBUF SOFFSET must be an SGPR or inline constant");
        instruction_.operands = BufferOperands{static_cast<std::uint8_t>(field(*second, 8, 0xff)),
                                               static_cast<std::uint8_t>(field(*second, 0, 0xff)),
                                               static_cast<std::uint8_t>(resource), *offset, field(word, 0, 0xfff)};
        return finish(Encoding::mubuf, decoded);
    }

    ByteView code_;
    std::size_t byte_offset_;
    DecodedInstruction instruction_;
};

}  // namespace

Result<std::vector<DecodedInstruction>> decode(ByteView code) {
    std::vector<DecodedInstruction> instructions;
    for (std::size_t byte_offset = 0; byte_offset < code.size();) {
        auto instruction = InstructionDecoder(code, byte_offset).decode_one();
        if (!instruction) return std::unexpected(std::move(instruction.error()));
        byte_offset += instruction->word_count * kDwordSize;
        instructions.push_back(*instruction);
    }
    return instructions;
}

}  // namespace scale::shader
