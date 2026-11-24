// scale_worker — the process the supervisor launches (T-201).
//
// usage: scale_worker --scale-report-handle=<hex> self-test <mode> [args]
//
//        scale_worker --scale-report-handle=<hex> run <path>
//
// run (T-202) maps a trusted test executable with scale::loader::map_image
// and calls its entry; see run_executable() below.
//
// Self-test modes exercise every supervision path:
//   exit <n>            exit with code n
//   access-violation    write through a null pointer (unhandled exception)
//   hang                sleep forever (timeout)
//   flood <bytes>       write <bytes> to stdout and to stderr
//   echo-args [args]    print each remaining argument on its own line
//   read-stdin          report whether stdin is at EOF
//   alloc <mib>         commit <mib> MiB and report success or failure
//   report <text>       write one line to the report channel
//   spawn-orphan        start a hanging copy of itself, print its pid, exit
//
// Exit codes: the mode's own code, 2 for usage errors, 3 (kWorkerLoadFailed)
// when run rejects the image before its entry is called.

#include "scale/common/text.h"
#include "scale/loader/image_map.h"
#include "scale/loader/pe_image.h"
#include "scale/diagnostics/event_line.h"
#include "scale/runtime/import_resolver.h"

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <format>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kWorkerLoadFailed = 3;
// Bounds the report channel (see scale/host/supervisor.h limits).
constexpr std::size_t kMaxImportEvents = 64;
HANDLE g_report = nullptr;

void write_all(HANDLE h, const char* data, std::size_t size) {
    while (size > 0) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(size > 65536 ? 65536 : size);
        if (!::WriteFile(h, data, chunk, &written, nullptr) || written == 0) {
            return;
        }
        data += written;
        size -= written;
    }
}

void write_out(const std::string& s) {
    write_all(::GetStdHandle(STD_OUTPUT_HANDLE), s.data(), s.size());
}

void report_line(const std::string& s) {
    if (g_report != nullptr) {
        const std::string line = s + "\n";
        write_all(g_report, line.data(), line.size());
    }
}

// Top-level filter: report the exception on the dedicated channel, then end
// the process with the exception code. Avoids heap use.
LONG WINAPI unhandled_filter(EXCEPTION_POINTERS* info) {
    const auto code = static_cast<unsigned>(info->ExceptionRecord->ExceptionCode);
    const auto address = reinterpret_cast<unsigned long long>(info->ExceptionRecord->ExceptionAddress);
    if (g_report != nullptr) {
        char line[96];
        const int n = std::snprintf(line, sizeof(line), "exception code=0x%08x address=0x%016llx\n", code, address);
        if (n > 0) {
            write_all(g_report, line, static_cast<std::size_t>(n));
        }
    }
    ::TerminateProcess(::GetCurrentProcess(), code);
    return EXCEPTION_EXECUTE_HANDLER;
}

std::string to_utf8(const wchar_t* w) {
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) {
        return {};
    }
    std::string s(static_cast<std::size_t>(n - 1), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

int usage() {
    const char msg[] = "usage: scale_worker --scale-report-handle=<hex> self-test <mode> [args]\n";
    write_all(::GetStdHandle(STD_ERROR_HANDLE), msg, sizeof(msg) - 1);
    return 2;
}

bool parse_u64(const std::string& s, unsigned long long& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    v = std::strtoull(s.c_str(), &end, 10);
    return end == s.c_str() + s.size();
}

int spawn_orphan() {
    wchar_t self[MAX_PATH];
    const DWORD len = ::GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return 3;
    }
    std::wstring cmd = L"\"" + std::wstring(self) + L"\" --scale-report-handle=0 self-test hang";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!::CreateProcessW(self, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return 3;
    }
    write_out("child pid=" + std::to_string(pi.dwProcessId) + "\n");
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return 0;
}

int self_test(const std::vector<std::string>& a) {
    if (a.empty()) return usage();
    const std::string& mode = a[0];
    unsigned long long n = 0;

    if (mode == "exit" && a.size() == 2 && parse_u64(a[1], n)) {
        return static_cast<int>(n);
    }
    if (mode == "access-violation") {
        volatile int* p = nullptr;
        *p = 1;
        return 0;
    }
    if (mode == "hang") {
        for (;;) ::Sleep(1000);
    }
    if (mode == "flood" && a.size() == 2 && parse_u64(a[1], n)) {
        const std::string out(static_cast<std::size_t>(n), 'o');
        const std::string err(static_cast<std::size_t>(n), 'e');
        write_all(::GetStdHandle(STD_OUTPUT_HANDLE), out.data(), out.size());
        write_all(::GetStdHandle(STD_ERROR_HANDLE), err.data(), err.size());
        return 0;
    }
    if (mode == "echo-args") {
        for (std::size_t i = 1; i < a.size(); ++i) write_out("[" + a[i] + "]\n");
        return 0;
    }
    if (mode == "read-stdin") {
        char c;
        DWORD read = 0;
        const BOOL ok = ::ReadFile(::GetStdHandle(STD_INPUT_HANDLE), &c, 1, &read, nullptr);
        write_out(ok && read == 0 ? "stdin: eof\n" : "stdin: data\n");
        return 0;
    }
    if (mode == "alloc" && a.size() == 2 && parse_u64(a[1], n)) {
        void* p = ::VirtualAlloc(nullptr, static_cast<SIZE_T>(n) << 20, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        write_out(p != nullptr ? "alloc: ok\n" : "alloc: failed\n");
        return 0;
    }
    if (mode == "report" && a.size() == 2) {
        report_line(a[1]);
        return 0;
    }
    if (mode == "spawn-orphan") {
        return spawn_orphan();
    }
    return usage();
}

void report_error(std::string_view stage, const scale::Error& e) {
    report_line("error stage=" + std::string(stage) + " component=" + e.component + " code=" + e.code + " message=" + e.message);
}

// Maps a trusted test executable with the SCALE loader and calls its entry
// (contract: int test_entry(void)). Reports each stage; the process exit
// code is the entry's return value. Loader failures exit with
// kWorkerLoadFailed and an `error` report line; the entry is never reached.
int run_executable(const std::string& path_utf8) {
    report_line("stage start");
    const auto wide = scale::to_wide(path_utf8);
    if (!wide) {
        report_error("read", wide.error());
        return kWorkerLoadFailed;
    }
    const auto bytes = scale::loader::read_file(std::filesystem::path(*wide));
    if (!bytes) {
        report_error("read", bytes.error());
        return kWorkerLoadFailed;
    }
    const scale::ByteView view(*bytes);
    const auto image = scale::loader::inspect_pe(view);
    if (!image) {
        report_error("inspect", image.error());
        return kWorkerLoadFailed;
    }
    report_line("stage inspected");

    // Import resolution happens before anything is mapped: an image with an
    // unresolved required import is never executed (AGENTS.md M3).
    if (!image->imports.empty()) {
        const std::string importing_module =
            scale::to_utf8(std::filesystem::path(*wide).filename().wstring()).value_or("<image>");
        const auto resolution = scale::runtime::resolve_imports(*image, importing_module);
        if (!resolution.complete()) {
            std::size_t sent = 0;
            for (const auto& unresolved : resolution.unresolved) {
                if (sent == kMaxImportEvents) break;
                report_line(scale::diagnostics::to_event_line(scale::runtime::to_event(unresolved)));
                ++sent;
            }
            if (resolution.unresolved.size() > sent) {
                report_line(scale::diagnostics::to_event_line(scale::diagnostics::DiagnosticEvent{
                    "runtime", "import_resolution", "unresolved_imports_omitted", scale::diagnostics::Severity::error,
                    true, "further unresolved imports were not reported individually",
                    {{"count", std::to_string(resolution.unresolved.size() - sent)}}}));
            }
            return kWorkerLoadFailed;
        }
        report_line(std::format("stage resolved imports={}", resolution.total));
    }

    auto mapped = scale::loader::map_image(view, *image);
    if (!mapped) {
        report_error("map", mapped.error());
        return kWorkerLoadFailed;
    }
    report_line(std::format("stage mapped base=0x{:x} size=0x{:x} delta={} relocations={}", mapped->base(), mapped->size(),
                            mapped->relocation_delta(), mapped->relocations_applied()));

    using Entry = int (*)();
    const auto entry = reinterpret_cast<Entry>(mapped->entry_point());
    report_line("stage entry");
    const int value = entry();
    report_line(std::format("stage returned value={}", value));
    return value;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    ::SetUnhandledExceptionFilter(unhandled_filter);

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(to_utf8(argv[i]));

    constexpr char kOption[] = "--scale-report-handle=";
    if (args.empty() || args[0].rfind(kOption, 0) != 0) {
        return usage();
    }
    const unsigned long long value = std::strtoull(args[0].c_str() + sizeof(kOption) - 1, nullptr, 16);
    g_report = value != 0 ? reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(value)) : nullptr;

    if (args.size() >= 2 && args[1] == "self-test") {
        return self_test(std::vector<std::string>(args.begin() + 2, args.end()));
    }
    if (args.size() == 3 && args[1] == "run") {
        return run_executable(args[2]);
    }
    return usage();
}
