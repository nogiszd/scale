#pragma once

#include "scale/common/error.h"
#include "scale/shader/ir.h"

#include <cstdint>
#include <vector>

namespace scale::shader {

struct SpirvModule {
    std::vector<std::uint32_t> words;
    bool requires_float32_rte{};  // The device must support shaderRoundingModeRTEFloat32.
};

// Emit SPIR-V 1.5 for Vulkan 1.2, with a compute entry point named "main"
// and the two storage buffers defined by T-704. The caller checks device
// capabilities and the test case's floating-point domain before dispatch.
// Errors use component "shader" and one of these codes: invalid_ir,
// unsupported_exec, unsupported_ir_opcode, spirv_limit_exceeded.
[[nodiscard]] Result<SpirvModule> emit_spirv(const IrProgram& program);

}  // namespace scale::shader
