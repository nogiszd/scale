#include "scale/shader_ref/interpreter.h"

#include <bit>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace scale::shader_ref {
namespace {

using shader::AluOperands;
using shader::BranchOperands;
using shader::BufferOperands;
using shader::CompareOperands;
using shader::DecodedInstruction;
using shader::Encoding;
using shader::Opcode;
using shader::Operand;
using shader::OperandKind;
using shader::WaitCounters;

// ABI register roles (docs/contracts/shader_abi.md).
constexpr std::uint32_t kAddressVgpr = 1;
constexpr std::uint32_t kReservedSgprEnd = 8;  // s0..s3 input, s4..s7 output descriptors
constexpr std::uint8_t kInputResource = 0;
constexpr std::uint8_t kOutputResource = 4;
constexpr std::uint32_t kAllLanes = 0xFFFFFFFFu;

// ISA source/destination selectors (RDNA 2 ISA §3.6).
constexpr std::uint16_t kLiteralSelector = 255;
constexpr std::uint16_t kFirstVgprSelector = 256;
constexpr std::uint16_t kLastSgpr = 105;
constexpr std::uint16_t kVccLoSelector = 106;
constexpr std::uint16_t kExecLoSelector = 126;

std::unexpected<Error> ref_error(std::string_view code, const DecodedInstruction& inst, std::string_view detail) {
    return fail("shader_ref", code, std::format("dword 0x{:x}: {}", inst.dword_offset, detail));
}

bool is_v1_opcode(Opcode op) {
    switch (op) {
        case Opcode::v_cmp_eq_u32:
        case Opcode::v_cmp_lt_u32:
        case Opcode::s_and_saveexec_b32:
        case Opcode::s_cbranch_scc0:
        case Opcode::s_cbranch_scc1:
        case Opcode::s_cbranch_execz:
        case Opcode::s_branch:
            return true;
        default:
            return false;
    }
}

bool is_branch(Opcode op) {
    return op == Opcode::s_cbranch_scc0 || op == Opcode::s_cbranch_scc1 || op == Opcode::s_cbranch_execz ||
           op == Opcode::s_branch;
}

// ---- Independent operand encoding rules ------------------------------------------

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

// Verifies an operand against its raw encoding. Returns the constant bits for
// constants, nullopt for registers. `eight_bit_vgpr_field`: the operand comes
// from an 8-bit VGPR field (VOP2 VSRC1, VOPC VSRC1, vector destinations).
Result<std::optional<std::uint32_t>> check_source(const Operand& op, const DecodedInstruction& inst,
                                                  bool eight_bit_vgpr_field) {
    switch (op.kind) {
        case OperandKind::sgpr:
            if (op.encoded > kLastSgpr || op.value != op.encoded) {
                return ref_error("decoder_disagreement", inst,
                                 std::format("SGPR operand value {} vs encoding {}", op.value, op.encoded));
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
        case OperandKind::literal_constant:
            if (op.encoded != kLiteralSelector || inst.word_count < 2 ||
                inst.raw_words[inst.word_count - 1] != op.value) {
                return ref_error("decoder_disagreement", inst,
                                 std::format("literal 0x{:08x} does not match the trailing instruction dword",
                                             op.value));
            }
            return std::optional<std::uint32_t>{op.value};
        case OperandKind::immediate: {
            const auto sign_extended =
                static_cast<std::uint32_t>(static_cast<std::int32_t>(std::bit_cast<std::int16_t>(op.encoded)));
            if (inst.opcode != Opcode::s_movk_i32 || op.value != sign_extended) {
                return ref_error("decoder_disagreement", inst, "SOPK immediate is not the sign-extended field");
            }
            return std::optional<std::uint32_t>{sign_extended};
        }
        case OperandKind::vcc_lo:
            if (op.encoded != kVccLoSelector || op.value != kVccLoSelector) {
                return ref_error("decoder_disagreement", inst, "vcc_lo operand does not carry selector 106");
            }
            return std::optional<std::uint32_t>{};
        case OperandKind::exec_lo:
            if (op.encoded != kExecLoSelector || op.value != kExecLoSelector) {
                return ref_error("decoder_disagreement", inst, "exec_lo operand does not carry selector 126");
            }
            return std::optional<std::uint32_t>{};
    }
    return ref_error("unsupported_instruction", inst, "unknown operand kind");
}

bool is_finite_normal(std::uint32_t bits) {
    const std::uint32_t exponent = (bits >> 23) & 0xFFu;
    return exponent != 0 && exponent != 0xFFu;
}

// Branch target recomputed from the raw instruction word (RDNA 2 ISA §12.5:
// PC of the next instruction plus the sign-extended SIMM16, in dwords).
Result<std::int64_t> branch_target(const DecodedInstruction& inst) {
    const auto* b = std::get_if<BranchOperands>(&inst.operands);
    if (b == nullptr) return ref_error("unsupported_instruction", inst, "branch without branch operands");
    const auto raw = static_cast<std::uint16_t>(inst.raw_words[0] & 0xFFFFu);
    const std::int64_t target = static_cast<std::int64_t>(inst.dword_offset) + inst.word_count +
                                std::bit_cast<std::int16_t>(raw);
    if (inst.word_count != 1 || b->simm16 != raw || b->target_dword != target) {
        return ref_error("decoder_disagreement", inst,
                         std::format("branch target {} (simm16 0x{:04x}) vs raw-word target {}", b->target_dword,
                                     b->simm16, target));
    }
    return target;
}

// Static operand-form checks for one instruction (independent of state).
Result<void> check_operand_forms(const DecodedInstruction& inst, AbiVersion abi) {
    if (abi == AbiVersion::v0 && is_v1_opcode(inst.opcode)) {
        return ref_error("unsupported_abi", inst, "EXEC-mask and branch instructions require ABI v1");
    }
    if (const auto* a = std::get_if<AluOperands>(&inst.operands)) {
        const bool exec_dest_allowed = abi == AbiVersion::v1 && inst.opcode == Opcode::s_mov_b32;
        if (a->destination.kind == OperandKind::exec_lo && !exec_dest_allowed) {
            return ref_error("unsupported_abi", inst, "exec_lo can only be written by s_mov_b32 under ABI v1");
        }
        if (a->destination.kind == OperandKind::vcc_lo) {
            return ref_error("unsupported_abi", inst, "vcc_lo is not a writable destination here");
        }
        const bool vector = inst.encoding == Encoding::vop1 || inst.encoding == Encoding::vop2 ||
                            inst.encoding == Encoding::vop3;
        if (auto c = check_source(a->destination, inst, vector); !c) {
            return std::unexpected(c.error());
        }
        for (std::uint8_t i = 0; i < a->source_count && i < a->sources.size(); ++i) {
            const Operand& src = a->sources[i];
            const bool vcc_source_allowed =
                abi == AbiVersion::v1 && (inst.opcode == Opcode::s_and_saveexec_b32 ||
                                          (inst.opcode == Opcode::s_mov_b32 &&
                                           a->destination.kind == OperandKind::exec_lo));
            if (src.kind == OperandKind::exec_lo || (src.kind == OperandKind::vcc_lo && !vcc_source_allowed)) {
                return ref_error("unsupported_abi", inst, "special register source is outside the ABI");
            }
            if (auto c = check_source(src, inst, inst.encoding == Encoding::vop2 && i == 1); !c) {
                return std::unexpected(c.error());
            }
        }
    } else if (const auto* c = std::get_if<CompareOperands>(&inst.operands)) {
        if (c->sources[0].kind == OperandKind::vcc_lo || c->sources[0].kind == OperandKind::exec_lo ||
            c->sources[1].kind != OperandKind::vgpr) {
            return ref_error("unsupported_abi", inst, "VOPC sources must be a register/constant and a VGPR");
        }
        if (auto r = check_source(c->sources[0], inst, false); !r) return std::unexpected(r.error());
        if (auto r = check_source(c->sources[1], inst, true); !r) return std::unexpected(r.error());
    } else if (is_branch(inst.opcode)) {
        if (auto t = branch_target(inst); !t) return std::unexpected(t.error());
    }
    return {};
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
    if (auto forms = check_operand_forms(inst, abi_); !forms) return forms;
    WaveState& s = state_;
    const std::uint32_t exec = s.exec;  // mask at the start of this instruction

    // ---- operand helpers -------------------------------------------------------------
    const auto read_sgpr = [&](std::uint32_t index) -> Result<std::uint32_t> {
        if (index < kReservedSgprEnd) {
            return ref_error("reserved_register", inst, std::format("s{} is an opaque ABI descriptor", index));
        }
        if (!s.sgpr_defined[index]) {
            return ref_error("undefined_register", inst, std::format("read of undefined s{}", index));
        }
        return s.sgpr[index];
    };
    // Uniform (scalar) source value; vcc_lo where the forms check allowed it.
    const auto scalar_source = [&](const Operand& op) -> Result<std::uint32_t> {
        auto constant = check_source(op, inst, false);
        if (!constant) return std::unexpected(constant.error());
        if (*constant) return **constant;
        if (op.kind == OperandKind::vcc_lo) return s.vcc;
        if (op.kind != OperandKind::sgpr) {
            return ref_error("unsupported_instruction", inst, "scalar instruction reads a VGPR");
        }
        return read_sgpr(op.value);
    };
    // Per-lane source values for the active lanes. Never called with exec == 0.
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
        if (op.kind != OperandKind::vgpr) {
            return ref_error("unsupported_abi", inst, "special register is not a vector source");
        }
        if ((pending_lanes_[op.value] & exec) != 0) {
            return ref_error("load_not_ready", inst, std::format("v{} is read before s_waitcnt vmcnt(0)", op.value));
        }
        if ((s.vgpr_defined_lanes[op.value] & exec) != exec) {
            return ref_error("undefined_register", inst, std::format("v{} is undefined in an active lane", op.value));
        }
        for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
            if ((exec >> lane) & 1u) lanes[lane] = s.vgpr[op.value][lane];
        }
        return lanes;
    };
    const auto write_sgpr = [&](const Operand& dest, std::uint32_t value) -> Result<void> {
        if (dest.kind != OperandKind::sgpr || dest.value > kLastSgpr || dest.encoded != dest.value) {
            return ref_error("decoder_disagreement", inst, "scalar destination is not a general-purpose SGPR");
        }
        if (dest.value < kReservedSgprEnd) {
            return ref_error("reserved_register", inst, std::format("s{} is an opaque ABI descriptor", dest.value));
        }
        s.sgpr[dest.value] = value;
        s.sgpr_defined.set(dest.value);
        return {};
    };
    const auto check_vgpr_dest = [&](std::uint32_t index) -> Result<void> {
        if (index == kAddressVgpr) return ref_error("reserved_register", inst, "v1 is immutable under the shader ABI");
        if ((pending_lanes_[index] & exec) != 0) {
            return ref_error("load_not_ready", inst,
                             std::format("v{} is overwritten before s_waitcnt vmcnt(0)", index));
        }
        return {};
    };
    const auto write_vgpr = [&](std::uint32_t index, const LaneWords& values) {
        for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
            if ((exec >> lane) & 1u) s.vgpr[index][lane] = values[lane];
        }
        s.vgpr_defined_lanes[index] |= exec;
    };

    // ---- instruction semantics --------------------------------------------------------------
    switch (inst.opcode) {
        case Opcode::s_endpgm:
            ended_ = true;
            return {};

        case Opcode::s_waitcnt: {
            const auto* w = std::get_if<WaitCounters>(&inst.operands);
            if (w == nullptr) return ref_error("unsupported_instruction", inst, "s_waitcnt without counters");
            // Only vmcnt(0) with expcnt and lgkmcnt at their maximum (not waited on).
            if (w->vmcnt != 0 || w->expcnt != 7 || w->lgkmcnt != 63) {
                return ref_error("unsupported_abi", inst, "only s_waitcnt vmcnt(0) is allowed by the shader ABI");
            }
            pending_lanes_.fill(0);  // clears pending lanes even where EXEC is now clear
            return {};
        }

        case Opcode::s_mov_b32:
        case Opcode::s_movk_i32:
        case Opcode::s_add_u32:
        case Opcode::s_and_saveexec_b32: {
            const auto* a = std::get_if<AluOperands>(&inst.operands);
            const std::uint8_t arity = inst.opcode == Opcode::s_add_u32 ? 2 : 1;
            if (a == nullptr || a->source_count != arity) {
                return ref_error("unsupported_instruction", inst, "unexpected scalar operand count");
            }
            // Scalar instructions execute once per wave, even with EXEC == 0.
            // All sources are read before any destination is written.
            auto x = scalar_source(a->sources[0]);
            if (!x) return std::unexpected(x.error());
            if (inst.opcode == Opcode::s_add_u32) {
                auto y = scalar_source(a->sources[1]);
                if (!y) return std::unexpected(y.error());
                const std::uint64_t sum = std::uint64_t{*x} + *y;  // SCC = carry out
                if (auto w = write_sgpr(a->destination, static_cast<std::uint32_t>(sum)); !w) return w;
                s.scc = (sum >> 32) != 0;
                return {};
            }
            if (inst.opcode == Opcode::s_and_saveexec_b32) {
                // dst = old EXEC; EXEC = old EXEC & src; SCC = (new EXEC != 0); VCC preserved.
                const std::uint32_t old_exec = s.exec;
                if (auto w = write_sgpr(a->destination, old_exec); !w) return w;
                s.exec = old_exec & *x;
                s.scc = s.exec != 0;
                return {};
            }
            if (a->destination.kind == OperandKind::exec_lo) {
                s.exec = *x;  // VCC and SCC preserved
                return {};
            }
            return write_sgpr(a->destination, *x);
        }

        case Opcode::v_cmp_eq_u32:
        case Opcode::v_cmp_lt_u32: {
            const auto* c = std::get_if<CompareOperands>(&inst.operands);
            if (c == nullptr) return ref_error("unsupported_instruction", inst, "VOPC without compare operands");
            // VCC = active lanes whose unsigned compare (source order) is true; inactive bits clear.
            std::uint32_t mask = 0;
            if (exec != 0) {  // an empty mask reads no sources
                auto x = lane_source(c->sources[0], false);
                if (!x) return std::unexpected(x.error());
                auto y = lane_source(c->sources[1], true);
                if (!y) return std::unexpected(y.error());
                for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
                    if (((exec >> lane) & 1u) == 0) continue;
                    const bool r = inst.opcode == Opcode::v_cmp_eq_u32 ? (*x)[lane] == (*y)[lane]
                                                                        : (*x)[lane] < (*y)[lane];
                    if (r) mask |= 1u << lane;
                }
            }
            s.vcc = mask;  // EXEC and SCC preserved
            return {};
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
            if (exec == 0) return {};  // empty mask: no source reads, no writes
            auto x = lane_source(a->sources[0], false);
            if (!x) return std::unexpected(x.error());
            LaneWords result = *x;
            if (arity == 2) {
                auto y = lane_source(a->sources[1], inst.encoding == Encoding::vop2);
                if (!y) return std::unexpected(y.error());
                for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
                    if (((exec >> lane) & 1u) == 0) continue;
                    const std::uint32_t l = (*x)[lane];
                    const std::uint32_t r = (*y)[lane];
                    switch (inst.opcode) {
                        case Opcode::v_add_nc_u32: result[lane] = l + r; break;  // wraps
                        case Opcode::v_mul_lo_u32:
                            result[lane] = static_cast<std::uint32_t>(std::uint64_t{l} * r);
                            break;
                        default: {
                            // binary32 RNE, no contraction; finite-normal domain for active lanes.
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
                                 "MUBUF must use the ABI resource tuple, v1 addressing and zero offsets");
            }
            if (auto c = check_source(b->scalar_offset_bytes, inst, false); !c) return std::unexpected(c.error());
            const std::uint32_t data = b->data_vgpr;
            if (!load) {
                bool any_pending = false;
                for (std::uint32_t p : pending_lanes_) any_pending = any_pending || p != 0;
                if (any_pending) {
                    return ref_error("load_not_ready", inst, "output store issued before s_waitcnt vmcnt(0)");
                }
                if ((s.vgpr_defined_lanes[data] & exec) != exec) {
                    return ref_error("undefined_register", inst,
                                     std::format("store source v{} is undefined in an active lane", data));
                }
            } else if (auto c = check_vgpr_dest(data); !c) {
                return c;
            }
            LaneWords loaded{};
            for (std::uint32_t lane = 0; lane < kWaveSize; ++lane) {
                if (((exec >> lane) & 1u) == 0) continue;  // inactive lanes access no memory
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
                pending_lanes_[data] |= exec;  // only active destination lanes become pending
            } else {
                ++stores_;
            }
            return {};
        }

        case Opcode::s_cbranch_scc0:
        case Opcode::s_cbranch_scc1:
        case Opcode::s_cbranch_execz:
        case Opcode::s_branch:
            return ref_error("invalid_program", inst, "branches are executed by run_abi_v1, not step()");
    }
    return ref_error("unsupported_instruction", inst, "opcode is outside the modeled subset");
}

Result<void> check_program(std::span<const DecodedInstruction> program, AbiVersion abi) {
    if (program.empty()) return fail("shader_ref", "invalid_program", "empty program");
    std::size_t expected_offset = 0;
    std::size_t stores = 0;
    for (std::size_t i = 0; i < program.size(); ++i) {
        const DecodedInstruction& inst = program[i];
        if (inst.dword_offset != expected_offset || inst.word_count == 0 || inst.word_count > 3) {
            return ref_error("decoder_disagreement", inst,
                             std::format("instruction {} starts at dword {} but the stream position is {}", i,
                                         inst.dword_offset, expected_offset));
        }
        expected_offset += inst.word_count;
        if (inst.opcode == Opcode::s_endpgm && i + 1 != program.size()) {
            return ref_error("invalid_program", inst, "s_endpgm must be the last instruction");
        }
        if (inst.opcode == Opcode::buffer_store_dword) ++stores;
        if (auto forms = check_operand_forms(inst, abi); !forms) return forms;
    }
    const std::size_t end = program.size() - 1;
    if (program[end].opcode != Opcode::s_endpgm) {
        return fail("shader_ref", "invalid_program", "program does not end with s_endpgm");
    }
    if (stores != 1 || end == 0 || program[end - 1].opcode != Opcode::buffer_store_dword) {
        return ref_error("invalid_program", program[end],
                         "exactly one output store must directly precede s_endpgm");
    }
    const std::size_t store = end - 1;
    for (std::size_t i = 0; i < program.size(); ++i) {
        if (!is_branch(program[i].opcode)) continue;
        const auto target = branch_target(program[i]);
        if (!target) return std::unexpected(target.error());
        // Forward only, never past the store, and onto the first dword of an instruction.
        bool starts_instruction = false;
        std::size_t target_index = 0;
        for (std::size_t j = i + 1; j <= store; ++j) {
            if (static_cast<std::int64_t>(program[j].dword_offset) == *target) {
                starts_instruction = true;
                target_index = j;
                break;
            }
        }
        if (!starts_instruction || target_index <= i) {
            return ref_error("invalid_program", program[i],
                             std::format("branch target dword {} is not a later instruction at or before the store",
                                         *target));
        }
    }
    return {};
}

namespace {

Result<DispatchResult> run(std::span<const DecodedInstruction> program, Interpreter interp, AbiVersion abi) {
    if (auto c = check_program(program, abi); !c) return std::unexpected(c.error());
    std::size_t executed = 0;
    std::size_t pc = 0;
    while (pc < program.size()) {
        const DecodedInstruction& inst = program[pc];
        if (executed++ > program.size()) {
            return ref_error("invalid_program", inst, "instruction limit exceeded");
        }
        if (is_branch(inst.opcode)) {
            const WaveState& s = interp.state();
            const bool taken = inst.opcode == Opcode::s_branch || (inst.opcode == Opcode::s_cbranch_scc0 && !s.scc) ||
                               (inst.opcode == Opcode::s_cbranch_scc1 && s.scc) ||
                               (inst.opcode == Opcode::s_cbranch_execz && s.exec == 0);
            if (!taken) {
                ++pc;
                continue;
            }
            const std::int64_t target = *branch_target(inst);  // validated by check_program
            std::size_t next = pc + 1;
            while (static_cast<std::int64_t>(program[next].dword_offset) != target) ++next;
            pc = next;
            continue;
        }
        if (auto r = interp.step(inst); !r) return std::unexpected(r.error());
        if (interp.ended()) break;
        ++pc;
    }
    if (!interp.ended()) {
        return fail("shader_ref", "invalid_program", "execution did not reach s_endpgm");
    }
    if (interp.stores() != 1) {
        return fail("shader_ref", "invalid_program", "execution did not issue exactly one output store");
    }
    return DispatchResult{interp.buffers().output, interp.buffers().output_written, executed, interp.state()};
}

}  // namespace

Result<DispatchResult> run_abi_v0(std::span<const DecodedInstruction> program, const BufferWords& input) {
    Buffers buffers;
    buffers.input = input;
    return run(program, Interpreter(abi_v0_entry_state(), buffers, AbiVersion::v0), AbiVersion::v0);
}

Result<DispatchResult> run_abi_v1(std::span<const DecodedInstruction> program, const BufferWords& input,
                                  const AbiV1Dispatch& dispatch) {
    WaveState state = abi_v0_entry_state();  // v0/v1 defined for all lanes, VCC = SCC = 0
    state.exec = dispatch.initial_exec;
    Buffers buffers;
    buffers.input = input;
    buffers.output = dispatch.initial_output;
    return run(program, Interpreter(state, buffers, AbiVersion::v1), AbiVersion::v1);
}

}  // namespace scale::shader_ref
