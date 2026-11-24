#pragma once

// Worker process supervision (T-201).
//
// Runs one executable in a fresh process inside a Windows job object:
//   - the process is created directly in the job (PROC_THREAD_ATTRIBUTE_JOB_LIST);
//   - the job kills all its processes when the supervisor closes it, caps
//     committed memory and suppresses the Windows Error Reporting UI
//     (JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION);
//   - only an explicit handle list is inherited: stdin (NUL), the stdout and
//     stderr pipes and the report pipe;
//   - stdout/stderr are captured up to a byte limit (the rest is drained and
//     discarded, so the child never blocks on a full pipe);
//   - a wall-clock timeout terminates the whole job.
//
// The worker runs with the caller's (unelevated) token. This isolates crashes
// from the supervisor. It is NOT a security boundary (AGENTS.md).
//
// Report channel: the first command-line argument passed to the child is
// `--scale-report-handle=<hex>`, the value of an inherited pipe handle. The
// child may write newline-terminated UTF-8 lines to it. The scale worker uses
// it to report unhandled exceptions as
//   exception code=0x<8 hex> address=0x<16 hex>
// so that guest output on stdout/stderr cannot fake a crash report.

#include "scale/common/error.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scale::host {

inline constexpr std::string_view kReportHandleOption = "--scale-report-handle=";
inline constexpr std::size_t kMaxReportLineBytes = 1024;
inline constexpr std::size_t kMaxReportLines = 256;
// Exit code the supervisor gives the job's processes when it kills them.
inline constexpr std::uint32_t kKilledExitCode = 0xFFFFFFFFu;

struct ProcessLimits {
    std::chrono::milliseconds timeout{10'000};
    std::uint64_t job_memory_bytes = 1ull << 30;  // committed memory of all job processes
    std::size_t max_capture_bytes = 1u << 20;     // per stream
};

struct LaunchRequest {
    std::filesystem::path executable;    // absolute path; no search path is used
    std::vector<std::string> arguments;  // UTF-8, after the report-handle argument
    ProcessLimits limits;
};

enum class Termination {
    exited,     // the process exited (exit_code is its exit code)
    exception,  // the worker reported an unhandled exception on the report channel
    timeout,    // the supervisor killed the job after limits.timeout
};

[[nodiscard]] std::string_view to_string(Termination termination) noexcept;

struct ExceptionReport {
    std::uint32_t code = 0;     // NTSTATUS exception code, e.g. 0xC0000005
    std::uint64_t address = 0;  // exception address as reported by the worker
};

struct CapturedStream {
    std::string data;
    bool truncated = false;  // output beyond max_capture_bytes was discarded
};

struct ProcessResult {
    Termination termination = Termination::exited;
    std::uint32_t exit_code = 0;
    // True when the exit code has NTSTATUS error severity (0xC0000000 bits),
    // e.g. a crash that bypassed the worker's exception filter (fail-fast,
    // stack overflow). It cannot be told apart from a process that exits
    // with such a value on purpose.
    bool abnormal_exit_status = false;
    std::optional<ExceptionReport> exception;
    CapturedStream out;
    CapturedStream err;
    std::vector<std::string> report_lines;  // every line on the report channel
    bool report_truncated = false;
    std::chrono::milliseconds elapsed{0};
};

// Launches and supervises the process. Errors (component "host"):
//   bad_request          relative executable path, invalid argument, command line too long
//   launch_failed        pipe, job or process creation failed (Win32 error in message)
//   supervision_failed   waiting for or querying the process failed
[[nodiscard]] Result<ProcessResult> run_supervised(const LaunchRequest& request);

// Builds a Windows command line whose CommandLineToArgvW parsing yields
// exactly `argv` (Microsoft C/C++ argument quoting rules).
[[nodiscard]] std::wstring build_command_line(const std::vector<std::wstring>& argv);

// Parses one report-channel line of the form
//   exception code=0x<hex> address=0x<hex>
[[nodiscard]] std::optional<ExceptionReport> parse_exception_report(std::string_view line);

}  // namespace scale::host
