#pragma once

#include "scale/shader/spirv.h"

namespace scale::shader::test {

// Exact T-704 add_lane_to_input program.
inline constexpr std::uint32_t kAddLaneProgram[]{
    0xe0301000, 0x80000201, 0xbf8c3f70, 0x4a040500, 0xe0701000, 0x80010201, 0xbf810000,
};

inline constexpr std::uint32_t kFloatProgram[]{
    0xe0301000, 0x80000201, 0xbf8c3f70,  // Load input into v2 and wait.
    0x060604f0,                          // v_add_f32_e32 v3, 0.5, v2
    0xd5080004, 0x0201e903,              // v_mul_f32_e64 v4, v3, 2.0
    0xe0701000, 0x80010401, 0xbf810000,  // Store v4 and end.
};

inline constexpr std::uint32_t kScalarProgram[]{
    0xb008ffff,              // s_movk_i32 s8, -1
    0xbe890381,              // s_mov_b32 s9, 1
    0x800a0908,              // s_add_u32 s10, s8, s9
    0x7e04020a,              // v_mov_b32_e32 v2, s10
    0xd5690004, 0x02020409,  // v_mul_lo_u32 v4, s9, v2
    0xe0701000, 0x80010401, 0xbf810000,
};

inline Result<IrProgram> lower_test_program(std::span<const std::uint32_t> words) {
    const auto decoded = decode(ByteView(std::as_bytes(words)));
    if (!decoded) return std::unexpected(decoded.error());
    return lower_ir(*decoded);
}

inline Result<SpirvModule> compile_test_program(std::span<const std::uint32_t> words) {
    const auto ir = lower_test_program(words);
    if (!ir) return std::unexpected(ir.error());
    return emit_spirv(*ir);
}

}  // namespace scale::shader::test
