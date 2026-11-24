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

// ABI v1 uses the same bindings and launch shape, with masked storage writes
// and a workgroup-memory exchange for VCC. The caller supplies the initial
// output buffer and validates the dispatch inputs as described in lower_ir_v1.
[[nodiscard]] Result<SpirvModule> emit_spirv(const IrProgramV1& program);

}  // namespace scale::shader
