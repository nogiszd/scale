#include "validator.h"

#include <windows.h>

#include <format>
#include <memory>
#include <string>

namespace scale::shader::cli {
namespace {

std::wstring environment(const wchar_t* name) {
    const auto required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0) return {};
    std::wstring value(required, L'\0');
    const auto length = GetEnvironmentVariableW(name, value.data(), required);
    if (length >= required) return {};
    value.resize(length);
    return value;
}

std::filesystem::path find_validator() {
    constexpr wchar_t executable[] = L"spirv-val.exe";
    const auto search_path = environment(L"PATH");
    if (!search_path.empty()) {
        // Supply PATH explicitly to avoid the default current-directory search.
        const auto required = SearchPathW(search_path.c_str(), executable, nullptr, 0, nullptr, nullptr);
        if (required != 0) {
            std::wstring file(required, L'\0');
            const auto length = SearchPathW(search_path.c_str(), executable, nullptr, required, file.data(), nullptr);
            if (length != 0 && length < required) {
                file.resize(length);
                return file;
            }
        }
    }
    const auto sdk = environment(L"VULKAN_SDK");
    if (!sdk.empty()) {
        const auto candidate = std::filesystem::path(sdk) / L"Bin" / executable;
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) return candidate;
    }
    return {};
}

struct CloseHandleDeleter {
    void operator()(HANDLE handle) const noexcept { CloseHandle(handle); }
};

}  // namespace

Result<std::optional<std::uint32_t>> run_validator(const std::filesystem::path& input) {
    const auto tool = find_validator();
    if (tool.empty()) return std::nullopt;
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    const auto file = CreateFileW(input.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, &attributes,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return fail("shader", "spirv_read_failed", std::format("could not open module ({})", GetLastError()));
    }
    const std::unique_ptr<void, CloseHandleDeleter> module(file);

    // Open the Unicode path ourselves and give the validator binary stdin.
    // Some Windows builds of spirv-val use an ANSI command-line entry point.
    std::wstring command = L"\"" + tool.native() + L"\" --target-env vulkan1.2 -";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = module.get();
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(tool.c_str(), command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &info)) {
        return fail("shader", "validator_start_failed", std::format("CreateProcessW failed ({})", GetLastError()));
    }
    const std::unique_ptr<void, CloseHandleDeleter> process(info.hProcess);
    CloseHandle(info.hThread);

    if (WaitForSingleObject(process.get(), INFINITE) != WAIT_OBJECT_0) {
        return fail("shader", "validator_wait_failed", std::format("could not wait for spirv-val ({})", GetLastError()));
    }
    DWORD exit_code{};
    if (!GetExitCodeProcess(process.get(), &exit_code)) {
        return fail("shader", "validator_wait_failed", std::format("could not read spirv-val status ({})", GetLastError()));
    }
    return static_cast<std::uint32_t>(exit_code);
}

}  // namespace scale::shader::cli
