#pragma once

#include "scale/common/error.h"

#include <cstdint>
#include <filesystem>
#include <span>

namespace scale::shader::cli {

// Publish a complete new module and preserve the original output on failure.
[[nodiscard]] Result<void> write_module(const std::filesystem::path& input,
                                        const std::filesystem::path& output,
                                        std::span<const std::uint32_t> words);

}  // namespace scale::shader::cli
