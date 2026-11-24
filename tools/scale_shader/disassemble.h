#pragma once

#include "scale/shader/decode.h"

#include <string>

namespace scale::shader::cli {

[[nodiscard]] std::string disassemble(const DecodedInstruction& instruction);

}  // namespace scale::shader::cli
