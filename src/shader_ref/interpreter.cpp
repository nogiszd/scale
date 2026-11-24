#include "scale/shader_ref/interpreter.h"

#include <bit>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace scale::shader_ref {
namespace {

using shader::AluOperands;
using shader::BufferOperands;
using shader::DecodedInstruction;
using shader::Encoding;
using shader::Opcode;
using shader::Operand;
using shader::OperandKind;
using shader::WaitCounters;

// ABI v0 register roles (docs/contracts/shader_abi.md).
constexpr std::uint32_t kAddressVgpr = 1;
constexpr std::uint32_t kReservedSgprEnd = 8;  // s0..s3 input, s4..s7 output descriptors
constexpr std::uint8_t kInputResource = 0;
constexpr std::uint8_t kOutputResource = 4;
constexpr std::uint32_t kAllLanes = 0xFFFFFFFFu;

std::unexpected<Error> ref_error(std::string_view code, const DecodedInstruction& inst, std::string_view detail) {
    return fail("shader_ref", code, std::format("dword 0x{:x}: {}", inst.dword_offset, detail));
}

// ---- Independent operand encoding rules (RDNA 2 ISA §3.6, source operands) --------

// Inline constant bits for an encoded source selector, or nullopt.
std::optional<std::uint32_t> inline_constant_bits(std::uint16_t encoded) {
    if (encoded >= 128 && encoded <= 192) return std::uint32_t{encoded} - 128u;                 // 0..64
    if (encoded >= 193 && encoded <= 208) return static_cast<std::uint32_t>(192 - int{encoded});  // -1..-16
    switch (encoded) {
        case 240: return 0x3F000000u;  // 0.5
        case 241: return 0xBF000000u;  // -0.5
        case 242: return 0x3F800000u;  // 1.0
        case 243: return 0xBF800000u;  // -1.0
        case 244: return 0x40000000u;  // 2.0
        case 245: return 0xC0000000u;  // -2.0
        case 246: return 0x40800000u;  // 4.0
        case 247: return 0xC0800000u;  // -4.0
        case 248: return 0x3E22F983u;  // 1/(2*pi)
        default: return std::nullopt;
    }
}

constexpr std::uint16_t kLiteralSelector = 255;
constexpr std::uint16_t kFirstVgprSelector = 256;
constexpr std::uint16_t kLastSgpr = 105;

// Verifies a source operand against its raw encoding and returns the
// constant bits for constants (nullopt for registers).
Result<std::optional<std::uint32_t>> check_source(const Operand& op, const DecodedInstruction& inst,
                                                  bool eight_bit_vgpr_field) {
    switch (op.kind) {
        case OperandKind::sgpr:
            if (op.encoded > kLastSgpr || op.value != op.encoded) {
                return ref_error("decoder_disagreement", inst, std::format("SGPR operand value {} vs encoding {}",
                                                                           op.value, op.encoded));
            }
            return std::optional<std::uint32_t>{};
        case OperandKind::vgpr: {
            const bool ok = eight_bit_vgpr_field ? op.value == op.encoded && op.encoded < 256
                                                 : op.encoded >= kFirstVgprSelector &&
                                                       op.value == std::uint32_t{op.encoded} - kFirstVgprSelector;
            if (!ok) {
                return ref_error("decoder_disagreement", inst,
                                 std::format("VGPR operand value {} vs encoding {}", op.value, op.encoded));
            }
            return std::optional<std::uint32_t>{};
        }
        case OperandKind::inline_constant: {
            const auto bits = inline_constant_bits(op.encoded);
            if (!bits || *bits != op.value) {
                return ref_error("decoder_disagreement", inst,
                                 std::format("inline constant selector {} resolved to 0x{:08x}", op.encoded, op.value));
            }
            return std::optional<std::uint32_t>{*bits};
        }
        case OperandKind::literal_constant: {
            if (op.encoded != kLiteralSelector || inst.word_count < 2 ||
                inst.raw_words[inst.word_count - 1] != op.value) {
                return ref_error("decoder_disagreement", inst,
                                 std::format("literal 0x{:08x} does not match the trailing instruction dword",
                                             op.value));
            }
            return std::optional<std::uint32_t>{op.value};
        }
        case OperandKind::immediate: {
            const auto sign_extended =
                static_cast<std::uint32_t>(static_cast<std::int32_t>(std::bit_cast<std::int16_t>(op.encoded)));
            if (inst.opcode != Opcode::s_movk_i32 || op.value != sign_extended) {
                return ref_error("decoder_disagreement", inst, "SOPK immediate is not the sign-extended field");
            }
            return std::optional<std::uint32_t>{sign_extended};
        }
    }
    return ref_error("unsupported_instruction", inst, "unknown operand kind");
}

bool is_finite_normal(std::uint32_t bits) {
    const std::uint32_t exponent = (bits >> 23) & 0xFFu;
    return exponent != 0 && exponent != 0xFFu;
}

}  // namespace

WaveState abi_v0_entry_state() {
    WaveState s;
    s.exec = kAllLanes;
    for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
        s.vgpr[0][lane] = lane;
        s.vgpr[kAddressVgpr][lane] = lane * 4;
    }
    s.vgpr_defined_lanes[0] = kAllLanes;
    s.vgpr_defined_lanes[kAddressVgpr] = kAllLanes;
    return s;
}

Result<void> Interpreter::step(const DecodedInstruction& inst) {
    if (ended_) {
        return ref_error("invalid_program", inst, "instruction after s_endpgm");
    }
    WaveState& s = state_;

    // ---- operand helpers (closures over this step) ------------------------------
    const auto read_sgpr = [&](std::uint32_t index) -> Result<std::uint32_t> {
        if (index < kReservedSgprEnd) {
            return ref_error("reserved_register", inst, std::format("s{} is an opaque ABI v0 descriptor", index));
        }
        if (!s.sgpr_defined[index]) {
            return ref_error("undefined_register", inst, std::format("read of undefined s{}", index));
        }
        return s.sgpr[index];
    };
    // Uniform (scalar) source value.
    const auto scalar_source = [&](const Operand& op) -> Result<std::uint32_t> {
        auto constant = check_source(op, inst, false);
        if (!constant) return std::unexpected(constant.error());
        if (*constant) return **constant;
        if (op.kind != OperandKind::sgpr) {
            return ref_error("unsupported_instruction", inst, "scalar instruction reads a VGPR");
        }
        return read_sgpr(op.value);
    };
    // Per-lane source values for the active lanes (inactive lanes are 0).
    const auto lane_source = [&](const Operand& op, bool eight_bit_vgpr_field) -> Result<LaneWords> {
        auto constant = check_source(op, inst, eight_bit_vgpr_field);
        if (!constant) return std::unexpected(constant.error());
        LaneWords lanes{};
        if (*constant) {
            lanes.fill(**constant);
            return lanes;
        }
        if (op.kind == OperandKind::sgpr) {
            auto v = read_sgpr(op.value);
            if (!v) return std::unexpected(v.error());
            lanes.fill(*v);
            return lanes;
        }
        if (pending_load_[op.value]) {
            return ref_error("load_not_ready", inst, std::format("v{} is read before s_waitcnt vmcnt(0)", op.value));
        }
        if ((s.vgpr_defined_lanes[op.value] & s.exec) != s.exec) {
            return ref_error("undefined_register", inst,
                             std::format("v{} is undefined in an active lane", op.value));
        }
        for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
            if ((s.exec >> lane) & 1u) lanes[lane] = s.vgpr[op.value][lane];
        }
        return lanes;
    };
    const auto write_sgpr = [&](const Operand& dest, std::uint32_t value) -> Result<void> {
        if (dest.kind != OperandKind::sgpr || dest.value > kLastSgpr || dest.encoded != dest.value) {
            return ref_error("decoder_disagreement", inst, "scalar destination is not a general-purpose SGPR");
        }
        if (dest.value < kReservedSgprEnd) {
            return ref_error("reserved_register", inst, std::format("s{} is an opaque ABI v0 descriptor", dest.value));
        }
        s.sgpr[dest.value] = value;
        s.sgpr_defined.set(dest.value);
        return {};
    };
    const auto check_vgpr_dest = [&](std::uint32_t index) -> Result<void> {
        if (index == kAddressVgpr) return ref_error("reserved_register", inst, "v1 is immutable under ABI v0");
        if (pending_load_[index]) {
            return ref_error("load_not_ready", inst, std::format("v{} is overwritten before s_waitcnt vmcnt(0)", index));
        }
        return {};
    };
    const auto write_vgpr = [&](std::uint32_t index, const LaneWords& values) {
        for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
            if ((s.exec >> lane) & 1u) s.vgpr[index][lane] = values[lane];
        }
        s.vgpr_defined_lanes[index] |= s.exec;
    };

    // ---- instruction semantics -----------------------------------------------------
    switch (inst.opcode) {
        case Opcode::s_endpgm:
            ended_ = true;
            return {};

        case Opcode::s_waitcnt: {
            const auto* w = std::get_if<WaitCounters>(&inst.operands);
            if (w == nullptr) return ref_error("unsupported_instruction", inst, "s_waitcnt without counters");
            // ABI v0 allows only vmcnt(0) with expcnt and lgkmcnt at their maximum (not waited on).
            if (w->vmcnt != 0 || w->expcnt != 7 || w->lgkmcnt != 63) {
                return ref_error("unsupported_abi", inst, "only s_waitcnt vmcnt(0) is allowed under ABI v0");
            }
            pending_load_.reset();
            return {};
        }

        case Opcode::s_mov_b32:
        case Opcode::s_movk_i32:
        case Opcode::s_add_u32: {
            const auto* a = std::get_if<AluOperands>(&inst.operands);
            const std::uint8_t arity = inst.opcode == Opcode::s_add_u32 ? 2 : 1;
            if (a == nullptr || a->source_count != arity) {
                return ref_error("unsupported_instruction", inst, "unexpected scalar operand count");
            }
            auto x = scalar_source(a->sources[0]);
            if (!x) return std::unexpected(x.error());
            std::uint32_t result = *x;
            if (inst.opcode == Opcode::s_add_u32) {
                auto y = scalar_source(a->sources[1]);
                if (!y) return std::unexpected(y.error());
                const std::uint64_t sum = std::uint64_t{*x} + *y;  // ISA: SCC = carry out
                result = static_cast<std::uint32_t>(sum);
                if (auto w = write_sgpr(a->destination, result); !w) return w;
                s.scc = (sum >> 32) != 0;
                return {};
            }
            return write_sgpr(a->destination, result);
        }

        case Opcode::v_mov_b32:
        case Opcode::v_add_nc_u32:
        case Opcode::v_mul_lo_u32:
        case Opcode::v_add_f32:
        case Opcode::v_mul_f32: {
            const auto* a = std::get_if<AluOperands>(&inst.operands);
            const std::uint8_t arity = inst.opcode == Opcode::v_mov_b32 ? 1 : 2;
            if (a == nullptr || a->source_count != arity) {
                return ref_error("unsupported_instruction", inst, "unexpected vector operand count");
            }
            if (a->destination.kind != OperandKind::vgpr || a->destination.encoded != a->destination.value) {
                return ref_error("decoder_disagreement", inst, "vector destination is not a VGPR field");
            }
            const std::uint32_t dest = a->destination.value;
            if (auto c = check_vgpr_dest(dest); !c) return c;
            // VOP2's second source is an 8-bit VGPR field; all other sources are 9-bit selectors.
            auto x = lane_source(a->sources[0], false);
            if (!x) return std::unexpected(x.error());
            LaneWords result = *x;
            if (arity == 2) {
                auto y = lane_source(a->sources[1], inst.encoding == Encoding::vop2);
                if (!y) return std::unexpected(y.error());
                for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
                    if (((s.exec >> lane) & 1u) == 0) continue;
                    const std::uint32_t l = (*x)[lane];
                    const std::uint32_t r = (*y)[lane];
                    switch (inst.opcode) {
                        case Opcode::v_add_nc_u32: result[lane] = l + r; break;  // wraps, no carry
                        case Opcode::v_mul_lo_u32:
                            result[lane] = static_cast<std::uint32_t>(std::uint64_t{l} * r);
                            break;
                        default: {
                            // binary32, round to nearest even, no contraction (ABI v0 domain:
                            // finite normal operands and results).
                            if (!is_finite_normal(l) || !is_finite_normal(r)) {
                                return ref_error("float_domain", inst,
                                                 std::format("lane {}: operand 0x{:08x} or 0x{:08x} is not finite "
                                                             "normal binary32", lane, l, r));
                            }
                            const float fl = std::bit_cast<float>(l);
                            const float fr = std::bit_cast<float>(r);
                            const float f = inst.opcode == Opcode::v_add_f32 ? fl + fr : fl * fr;
                            const auto bits = std::bit_cast<std::uint32_t>(f);
                            if (!is_finite_normal(bits)) {
                                return ref_error("float_domain", inst,
                                                 std::format("lane {}: result 0x{:08x} is not finite normal binary32",
                                                             lane, bits));
                            }
                            result[lane] = bits;
                        }
                    }
                }
            }
            write_vgpr(dest, result);
            return {};
        }

        case Opcode::buffer_load_dword:
        case Opcode::buffer_store_dword: {
            const auto* b = std::get_if<BufferOperands>(&inst.operands);
            if (b == nullptr) return ref_error("unsupported_instruction", inst, "MUBUF without buffer operands");
            const bool load = inst.opcode == Opcode::buffer_load_dword;
            const std::uint8_t resource = load ? kInputResource : kOutputResource;
            if (b->resource_sgpr_base != resource || b->address_vgpr != kAddressVgpr ||
                b->immediate_offset_bytes != 0 || b->scalar_offset_bytes.kind != OperandKind::inline_constant ||
                b->scalar_offset_bytes.value != 0) {
                return ref_error("unsupported_abi", inst,
                                 "MUBUF must use the ABI v0 resource tuple, v1 addressing and zero offsets");
            }
            if (auto c = check_source(b->scalar_offset_bytes, inst, false); !c) return std::unexpected(c.error());
            if (!load && pending_load_.any()) {
                return ref_error("load_not_ready", inst, "output store issued before s_waitcnt vmcnt(0)");
            }
            const std::uint32_t data = b->data_vgpr;
            if (load) {
                if (auto c = check_vgpr_dest(data); !c) return c;
            } else if ((s.vgpr_defined_lanes[data] & s.exec) != s.exec) {
                return ref_error("undefined_register", inst, std::format("store source v{} is undefined", data));
            }
            LaneWords loaded{};
            for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
                if (((s.exec >> lane) & 1u) == 0) continue;
                const std::uint32_t offset = s.vgpr[kAddressVgpr][lane];
                if (offset % 4 != 0 || offset > (kBufferWords - 1) * 4) {
                    return ref_error("out_of_bounds", inst,
                                     std::format("lane {}: byte offset {} outside the 128-byte buffer", lane, offset));
                }
                if (load) {
                    loaded[lane] = buffers_.input[offset / 4];
                } else {
                    buffers_.output[offset / 4] = s.vgpr[data][lane];
                    buffers_.output_written |= 1u << (offset / 4);
                }
            }
            if (load) {
                write_vgpr(data, loaded);
                pending_load_.set(data);
            } else {
                ++stores_;
            }
            return {};
        }
    }
    return ref_error("unsupported_instruction", inst, "opcode is outside the modeled subset");
}

Result<DispatchResult> run_abi_v0(std::span<const DecodedInstruction> program, const BufferWords& input) {
    Buffers buffers;
    buffers.input = input;
    Interpreter interp(abi_v0_entry_state(), buffers);
    std::size_t executed = 0;
    for (std::size_t i = 0; i < program.size(); ++i) {
        const DecodedInstruction& inst = program[i];
        if (inst.opcode == Opcode::s_endpgm && i + 1 != program.size()) {
            return ref_error("invalid_program", inst, "s_endpgm must be the last instruction");
        }
        if (inst.opcode == Opcode::s_endpgm &&
            (interp.stores() != 1 || i == 0 || program[i - 1].opcode != Opcode::buffer_store_dword)) {
            return ref_error("invalid_program", inst, "ABI v0 requires exactly one store immediately before s_endpgm");
        }
        if (auto r = interp.step(inst); !r) return std::unexpected(r.error());
        ++executed;
    }
    if (!interp.ended()) {
        return fail("shader_ref", "invalid_program", "program does not end with s_endpgm");
    }
    return DispatchResult{interp.buffers().output, interp.buffers().output_written, executed, interp.state()};
}

}  // namespace scale::shader_ref
