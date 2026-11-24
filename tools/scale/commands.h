#pragma once

// CLI command registry.
//
// Exit codes (stable, documented in README.md):
//   0  success
//   1  operation failed (diagnostics printed / written)
//   2  usage error (unknown command, bad arguments)
//   3  command not implemented yet

#include <span>
#include <string>
#include <string_view>

namespace scale::cli {

inline constexpr int kExitSuccess = 0;
inline constexpr int kExitFailure = 1;
inline constexpr int kExitUsage = 2;
inline constexpr int kExitNotImplemented = 3;

// Arguments after the command name, UTF-8 encoded.
using Args = std::span<const std::string>;
using Handler = int (*)(Args args);

struct Command {
    std::string_view name;
    std::string_view synopsis;  // e.g. "inspect <file> [--json]"
    std::string_view summary;
    std::string_view task;      // owning TASKS.md id while unimplemented
    Handler handler;            // nullptr = not implemented yet
};

[[nodiscard]] std::span<const Command> commands();

// Prints text to stdout / stderr without locale translation.
void out(std::string_view text);
void err(std::string_view text);

}  // namespace scale::cli
