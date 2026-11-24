#pragma once

#include "scale/common/error.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace scale::shader::cli {

// Launch the installed tool directly and inherit its diagnostics. The input
// path is absolute. An empty result means no spirv-val was found.
[[nodiscard]] Result<std::optional<std::uint32_t>> run_validator(const std::filesystem::path& input);

}  // namespace scale::shader::cli
