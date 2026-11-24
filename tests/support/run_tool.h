#pragma once

// Runs an external tool (dumpbin, spirv-val, llvm-mc, ...) from a test and
// captures its combined stdout/stderr. Platform-neutral header (decision
// 0004); implementations in run_tool_win32.cpp and run_tool_posix.cpp.
// For trusted, repository-configured tools only. This is not a sandbox.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace scale::test {

struct ToolOutput {
    int exit_code = -1;
    std::string output;  // stdout and stderr, interleaved
};

// nullopt if the tool could not be started, or if an argument contains a
// character the platform's command-line quoting can't represent safely ('"').
[[nodiscard]] std::optional<ToolOutput> run_tool(const std::filesystem::path& tool,
                                                 const std::vector<std::string>& arguments);

}  // namespace scale::test
