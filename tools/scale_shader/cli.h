#pragma once

#include <span>
#include <string_view>

namespace scale::shader::cli {

inline constexpr int kExitSuccess = 0;
inline constexpr int kExitFailure = 1;
inline constexpr int kExitUsage = 2;

// Arguments are UTF-8 and start at the command name.
int run(std::span<const std::string_view> args) noexcept;

}  // namespace scale::shader::cli
