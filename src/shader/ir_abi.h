#pragma once

#include "scale/shader/decode.h"

#include <string_view>

namespace scale::shader::detail {

inline constexpr std::uint8_t kWordBytes = 4;
inline constexpr std::uint8_t kAddressVgpr = 1;
inline constexpr std::uint8_t kInputSgprBase = 0;
inline constexpr std::uint8_t kOutputSgprBase = 4;
inline constexpr std::uint8_t kResourceSgprCount = 4;

inline bool matches_buffer_abi(const BufferOperands& operands, bool load) {
    return operands.resource_sgpr_base == (load ? kInputSgprBase : kOutputSgprBase) &&
           operands.address_vgpr == kAddressVgpr && operands.immediate_offset_bytes == 0 &&
           operands.scalar_offset_bytes.kind == OperandKind::inline_constant &&
           operands.scalar_offset_bytes.value == 0;
}

inline bool matches_wait_abi(const WaitCounters& counters) {
    return counters.vmcnt == 0 && counters.expcnt == 7 && counters.lgkmcnt == 63;
}

inline std::string_view opcode_name(Opcode opcode) {
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
    case Opcode::s_buffer_load_dword: return "s_buffer_load_dword";
    }
    return "unknown opcode";
}

}  // namespace scale::shader::detail
