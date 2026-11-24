#include "run_tool.h"

#include "scale/common/text.h"

#include <cstdio>

namespace scale::test {

std::optional<ToolOutput> run_tool(const std::filesystem::path& tool, const std::vector<std::string>& arguments) {
    // cmd.exe /c strips one pair of outer quotes: ""tool" "arg" ... 2>&1"
    std::wstring command = L"\"\"" + tool.wstring() + L"\"";
    for (const std::string& a : arguments) {
        if (a.find('"') != std::string::npos) return std::nullopt;
        const auto wide = scale::to_wide(a);
        if (!wide) return std::nullopt;
        command += L" \"" + *wide + L"\"";
    }
    command += L" 2>&1\"";
    FILE* pipe = ::_wpopen(command.c_str(), L"rb");
    if (pipe == nullptr) return std::nullopt;
    ToolOutput result;
    char buffer[4096];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof(buffer), pipe)) > 0) result.output.append(buffer, n);
    result.exit_code = ::_pclose(pipe);
    return result;
}

}  // namespace scale::test
