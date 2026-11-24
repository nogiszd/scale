#include "scale/host/supervisor.h"

#include "scale/common/text.h"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <format>
#include <memory>
#include <thread>
#include <utility>

namespace scale::host {
namespace {

std::unexpected<Error> host_error(std::string_view code, std::string message) {
    return fail("host", code, std::move(message));
}

std::string win32_message(std::string_view what) {
    return std::format("{} failed (Win32 error {})", what, ::GetLastError());
}

class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE h) : h_(h == INVALID_HANDLE_VALUE ? nullptr : h) {}
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& o) noexcept : h_(std::exchange(o.h_, nullptr)) {}
    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        if (this != &o) {
            reset();
            h_ = std::exchange(o.h_, nullptr);
        }
        return *this;
    }
    ~UniqueHandle() { reset(); }

    void reset() {
        if (h_ != nullptr) {
            ::CloseHandle(h_);
            h_ = nullptr;
        }
    }
    [[nodiscard]] HANDLE get() const { return h_; }
    [[nodiscard]] explicit operator bool() const { return h_ != nullptr; }

private:
    HANDLE h_ = nullptr;
};

struct Pipe {
    UniqueHandle read;   // kept by the supervisor
    UniqueHandle write;  // inherited by the child
};

Result<Pipe> make_pipe(std::string_view name) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE r = nullptr;
    HANDLE w = nullptr;
    if (!::CreatePipe(&r, &w, &sa, 0)) {
        return host_error("launch_failed", win32_message(std::format("CreatePipe ({})", name)));
    }
    Pipe p{UniqueHandle(r), UniqueHandle(w)};
    // Only the write end is inherited.
    if (!::SetHandleInformation(p.read.get(), HANDLE_FLAG_INHERIT, 0)) {
        return host_error("launch_failed", win32_message("SetHandleInformation"));
    }
    return p;
}

// Reads until EOF, keeping at most `cap` bytes. Always drains the pipe.
void drain(HANDLE h, CapturedStream& sink, std::size_t cap) {
    char buffer[4096];
    DWORD n = 0;
    while (::ReadFile(h, buffer, sizeof(buffer), &n, nullptr) && n > 0) {
        const std::size_t room = cap - std::min(cap, sink.data.size());
        const std::size_t take = std::min<std::size_t>(room, n);
        sink.data.append(buffer, take);
        if (take < n) {
            sink.truncated = true;
        }
    }
}

void split_report(const std::string& raw, bool raw_truncated, ProcessResult& result) {
    std::size_t start = 0;
    while (start < raw.size()) {
        std::size_t end = raw.find('\n', start);
        const bool complete = end != std::string::npos;
        if (!complete) {
            end = raw.size();
        }
        std::string line = raw.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (result.report_lines.size() >= kMaxReportLines || line.size() > kMaxReportLineBytes) {
            result.report_truncated = true;
            return;
        }
        result.report_lines.push_back(std::move(line));
        start = end + 1;
    }
    result.report_truncated = result.report_truncated || raw_truncated;
}

class AttributeList {
public:
    Result<void> init(DWORD count) {
        SIZE_T size = 0;
        ::InitializeProcThreadAttributeList(nullptr, count, 0, &size);
        storage_ = std::make_unique<std::byte[]>(size);
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.get());
        if (!::InitializeProcThreadAttributeList(list_, count, 0, &size)) {
            list_ = nullptr;
            return host_error("launch_failed", win32_message("InitializeProcThreadAttributeList"));
        }
        return {};
    }
    Result<void> add(DWORD_PTR attribute, void* value, SIZE_T size) {
        if (!::UpdateProcThreadAttribute(list_, 0, attribute, value, size, nullptr, nullptr)) {
            return host_error("launch_failed", win32_message("UpdateProcThreadAttribute"));
        }
        return {};
    }
    [[nodiscard]] LPPROC_THREAD_ATTRIBUTE_LIST get() const { return list_; }
    ~AttributeList() {
        if (list_ != nullptr) {
            ::DeleteProcThreadAttributeList(list_);
        }
    }

private:
    std::unique_ptr<std::byte[]> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
};

Result<UniqueHandle> make_job(const ProcessLimits& limits) {
    UniqueHandle job(::CreateJobObjectW(nullptr, nullptr));
    if (!job) {
        return host_error("launch_failed", win32_message("CreateJobObjectW"));
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                            JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION |
                                            JOB_OBJECT_LIMIT_JOB_MEMORY;
    info.JobMemoryLimit = static_cast<SIZE_T>(limits.job_memory_bytes);
    if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        return host_error("launch_failed", win32_message("SetInformationJobObject"));
    }
    return job;
}

}  // namespace

std::string_view to_string(Termination termination) noexcept {
    switch (termination) {
        case Termination::exited: return "exited";
        case Termination::exception: return "exception";
        case Termination::timeout: return "timeout";
    }
    return "unknown";
}

std::wstring build_command_line(const std::vector<std::wstring>& argv) {
    std::wstring line;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i != 0) {
            line += L' ';
        }
        const std::wstring& arg = argv[i];
        if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
            line += arg;
            continue;
        }
        line += L'"';
        std::size_t backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') {
                ++backslashes;
                continue;
            }
            if (c == L'"') {
                line.append(backslashes * 2 + 1, L'\\');
            } else {
                line.append(backslashes, L'\\');
            }
            backslashes = 0;
            line += c;
        }
        line.append(backslashes * 2, L'\\');
        line += L'"';
    }
    return line;
}

std::optional<ExceptionReport> parse_exception_report(std::string_view line) {
    constexpr std::string_view kPrefix = "exception code=0x";
    constexpr std::string_view kAddress = " address=0x";
    if (!line.starts_with(kPrefix)) {
        return std::nullopt;
    }
    line.remove_prefix(kPrefix.size());
    const auto sep = line.find(kAddress);
    if (sep == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view code_text = line.substr(0, sep);
    const std::string_view address_text = line.substr(sep + kAddress.size());
    ExceptionReport r;
    const auto c = std::from_chars(code_text.data(), code_text.data() + code_text.size(), r.code, 16);
    const auto a = std::from_chars(address_text.data(), address_text.data() + address_text.size(), r.address, 16);
    if (code_text.empty() || address_text.empty() || c.ec != std::errc() ||
        c.ptr != code_text.data() + code_text.size() || a.ec != std::errc() ||
        a.ptr != address_text.data() + address_text.size()) {
        return std::nullopt;
    }
    return r;
}

Result<ProcessResult> run_supervised(const LaunchRequest& request) {
    if (!request.executable.is_absolute()) {
        return host_error("bad_request", "executable path must be absolute");
    }

    // ---- Pipes and stdin --------------------------------------------------------
    auto out_pipe = make_pipe("stdout");
    if (!out_pipe) return std::unexpected(out_pipe.error());
    auto err_pipe = make_pipe("stderr");
    if (!err_pipe) return std::unexpected(err_pipe.error());
    auto report_pipe = make_pipe("report");
    if (!report_pipe) return std::unexpected(report_pipe.error());

    SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
    UniqueHandle null_in(::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                                       OPEN_EXISTING, 0, nullptr));
    if (!null_in) {
        return host_error("launch_failed", win32_message("CreateFileW(NUL)"));
    }

    // ---- Command line -------------------------------------------------------------
    std::vector<std::wstring> argv;
    argv.push_back(request.executable.wstring());
    argv.push_back(*to_wide(std::format("{}{:x}", kReportHandleOption,
                                        reinterpret_cast<std::uintptr_t>(report_pipe->write.get()))));
    for (const std::string& a : request.arguments) {
        auto wide = to_wide(a);
        if (!wide) {
            return host_error("bad_request", "argument is not valid UTF-8: " + wide.error().message);
        }
        argv.push_back(std::move(*wide));
    }
    std::wstring command_line = build_command_line(argv);
    if (command_line.size() >= 32767) {
        return host_error("bad_request", "command line exceeds 32767 characters");
    }

    // ---- Job and attributes -----------------------------------------------------------
    auto job = make_job(request.limits);
    if (!job) return std::unexpected(job.error());

    HANDLE inherited[] = {null_in.get(), out_pipe->write.get(), err_pipe->write.get(), report_pipe->write.get()};
    HANDLE job_handle = job->get();
    AttributeList attributes;
    if (auto r = attributes.init(2); !r) return std::unexpected(r.error());
    if (auto r = attributes.add(PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited)); !r) {
        return std::unexpected(r.error());
    }
    if (auto r = attributes.add(PROC_THREAD_ATTRIBUTE_JOB_LIST, &job_handle, sizeof(job_handle)); !r) {
        return std::unexpected(r.error());
    }

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = null_in.get();
    si.StartupInfo.hStdOutput = out_pipe->write.get();
    si.StartupInfo.hStdError = err_pipe->write.get();
    si.lpAttributeList = attributes.get();

    // ---- Launch -------------------------------------------------------------------------
    PROCESS_INFORMATION pi{};
    const auto start = std::chrono::steady_clock::now();
    if (!::CreateProcessW(request.executable.c_str(), command_line.data(), nullptr, nullptr, TRUE,
                          EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW, nullptr,
                          nullptr, &si.StartupInfo, &pi)) {
        return host_error("launch_failed", win32_message("CreateProcessW"));
    }
    UniqueHandle process(pi.hProcess);
    UniqueHandle thread(pi.hThread);

    // Close our copies of the child's ends so the readers see EOF.
    out_pipe->write.reset();
    err_pipe->write.reset();
    report_pipe->write.reset();
    null_in.reset();

    ProcessResult result;
    CapturedStream report_raw;
    const std::size_t cap = request.limits.max_capture_bytes;
    {
        std::jthread out_reader([&] { drain(out_pipe->read.get(), result.out, cap); });
        std::jthread err_reader([&] { drain(err_pipe->read.get(), result.err, cap); });
        std::jthread report_reader(
            [&] { drain(report_pipe->read.get(), report_raw, kMaxReportLines * (kMaxReportLineBytes + 1)); });

        const auto timeout_ms = static_cast<DWORD>(
            std::clamp<std::int64_t>(request.limits.timeout.count(), 0, static_cast<std::int64_t>(INFINITE - 1)));
        const DWORD wait = ::WaitForSingleObject(process.get(), timeout_ms);
        if (wait == WAIT_TIMEOUT) {
            result.termination = Termination::timeout;
        } else if (wait != WAIT_OBJECT_0) {
            ::TerminateJobObject(job->get(), kKilledExitCode);
            return host_error("supervision_failed", win32_message("WaitForSingleObject"));
        }
        // Kill the job: on timeout this stops the worker, otherwise it stops any
        // descendants that would keep the pipes open.
        ::TerminateJobObject(job->get(), kKilledExitCode);
        if (::WaitForSingleObject(process.get(), 10'000) != WAIT_OBJECT_0) {
            return host_error("supervision_failed", "process did not terminate after job termination");
        }
        // Readers finish when the last writer closes; jthreads join here.
    }
    result.elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    DWORD exit_code = 0;
    if (!::GetExitCodeProcess(process.get(), &exit_code)) {
        return host_error("supervision_failed", win32_message("GetExitCodeProcess"));
    }
    result.exit_code = exit_code;
    result.abnormal_exit_status = (exit_code & 0xC0000000u) == 0xC0000000u && exit_code != kKilledExitCode;

    split_report(report_raw.data, report_raw.truncated, result);
    for (const std::string& line : result.report_lines) {
        if (auto e = parse_exception_report(line)) {
            result.exception = *e;
            if (result.termination != Termination::timeout) {
                result.termination = Termination::exception;
            }
            break;
        }
    }
    return result;
}

}  // namespace scale::host
