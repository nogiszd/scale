#include "run_tool.h"

// POSIX implementation. UNVERIFIED until T-802 builds SCALE on Linux.

#include <cstdio>
#include <sys/wait.h>

namespace scale::test {
namespace {

std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    return out + "'";
}

}  // namespace

std::optional<ToolOutput> run_tool(const std::filesystem::path& tool, const std::vector<std::string>& arguments) {
    std::string command = shell_quote(tool.string());
    for (const std::string& a : arguments) {
        if (a.find('"') != std::string::npos) return std::nullopt;  // same contract as Windows
        command += " " + shell_quote(a);
    }
    command += " 2>&1";
    FILE* pipe = ::popen(command.c_str(), "r");
    if (pipe == nullptr) return std::nullopt;
    ToolOutput result;
    char buffer[4096];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof(buffer), pipe)) > 0) result.output.append(buffer, n);
    const int status = ::pclose(pipe);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}

}  // namespace scale::test
