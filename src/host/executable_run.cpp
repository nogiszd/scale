#include "scale/host/executable_run.h"

#include "scale/common/sha256.h"
#include "scale/common/text.h"
#include "scale/diagnostics/event_line.h"
#include "scale/loader/image_map.h"

#include <windows.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <format>
#include <string_view>
#include <utility>

namespace scale::host {
namespace {

using diagnostics::Attribute;
using diagnostics::DiagnosticEvent;
using diagnostics::Outcome;
using diagnostics::Severity;

// Value of `key=` in a space-separated line (up to the next space).
std::optional<std::string_view> token(std::string_view line, std::string_view key) {
    const std::string needle = " " + std::string(key) + "=";
    const auto at = line.find(needle);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    std::string_view rest = line.substr(at + needle.size());
    return rest.substr(0, rest.find(' '));
}

std::optional<std::uint64_t> hex_value(std::string_view text) {
    if (!text.starts_with("0x")) return std::nullopt;
    text.remove_prefix(2);
    std::uint64_t v = 0;
    const auto r = std::from_chars(text.data(), text.data() + text.size(), v, 16);
    if (r.ec != std::errc() || r.ptr != text.data() + text.size()) return std::nullopt;
    return v;
}

bool is_blocking_code(std::string_view code) {
    return code.starts_with("unsupported_") || code == "relocation_required" ||
           code == "writable_executable_section" || code == "entry_not_executable" || code == "unresolved_import" ||
           code == "unresolved_imports_omitted";
}

struct WorkerError {
    std::string stage;
    std::string component;
    std::string code;
    std::string message;
};

// "error stage=S component=C code=K message=M" (message runs to end of line)
std::optional<WorkerError> parse_error_line(std::string_view line) {
    if (!line.starts_with("error ")) return std::nullopt;
    const auto stage = token(line, "stage");
    const auto component = token(line, "component");
    const auto code = token(line, "code");
    const auto message_at = line.find(" message=");
    if (!stage || !component || !code || message_at == std::string_view::npos) return std::nullopt;
    return WorkerError{std::string(*stage), std::string(*component), std::string(*code),
                       std::string(line.substr(message_at + 9))};
}

}  // namespace

std::optional<int> returned_value(const ProcessResult& process) {
    for (const std::string& line : process.report_lines) {
        if (!std::string_view(line).starts_with("stage returned ")) continue;
        const auto v = token(line, "value");
        if (!v) continue;
        int value = 0;
        const auto r = std::from_chars(v->data(), v->data() + v->size(), value);
        if (r.ec == std::errc() && r.ptr == v->data() + v->size()) return value;
    }
    return std::nullopt;
}

diagnostics::RunReport make_run_report(const ProcessResult& process, diagnostics::Subject subject,
                                           std::string run_id, std::chrono::milliseconds timeout) {
    diagnostics::RunReport report;
    report.run_id = std::move(run_id);
    report.command = "run";
    report.subject = std::move(subject);
    report.elapsed_ms = static_cast<std::uint64_t>(process.elapsed.count());
    report.final_stage = "launch";

    std::optional<std::uint64_t> base;
    std::optional<std::uint64_t> size;
    std::optional<WorkerError> worker_error;
    std::vector<DiagnosticEvent> worker_events;  // structured `event` lines
    for (const std::string& line : process.report_lines) {
        const std::string_view l(line);
        if (l.starts_with("event ")) {
            if (auto event = diagnostics::parse_event_line(l)) {
                worker_events.push_back(std::move(*event));
            } else {
                worker_events.push_back(DiagnosticEvent{"host", "worker", "malformed_report_line", Severity::warning,
                                                        false, "a worker event line could not be parsed", {}});
            }
        } else if (l.starts_with("stage ")) {
            const auto name = l.substr(6, l.find(' ', 6) == std::string_view::npos ? std::string_view::npos
                                                                                   : l.find(' ', 6) - 6);
            report.final_stage = std::string(name);
            if (name == "mapped") {
                base = token(l, "base").and_then(hex_value);
                size = token(l, "size").and_then(hex_value);
            }
        } else if (!worker_error) {
            worker_error = parse_error_line(l);
        }
    }

    // Events reported by the worker come first, in order; the first fatal one
    // decides the outcome when nothing more specific (timeout, exception,
    // loader error) happened.
    const DiagnosticEvent* first_fatal = nullptr;
    for (const DiagnosticEvent& e : worker_events) {
        report.diagnostics.report(e);
        if (e.fatal && first_fatal == nullptr) first_fatal = &e;
    }

    const auto value = returned_value(process);
    if (process.termination == Termination::timeout) {
        report.outcome = Outcome::failed;
        report.diagnostics.report(DiagnosticEvent{
            "host", "worker", "timeout", Severity::error, true,
            std::format("worker did not finish within {} ms", timeout.count()),
            {{"timeout_ms", std::to_string(timeout.count())}, {"last_stage", report.final_stage}}});
    } else if (process.exception) {
        report.outcome = Outcome::failed;
        std::vector<Attribute> attrs = {{"exception_code", std::format("0x{:08x}", process.exception->code)},
                                        {"address", std::format("0x{:016x}", process.exception->address)}};
        if (base && size && process.exception->address >= *base && process.exception->address - *base < *size) {
            attrs.push_back({"rva", std::format("0x{:x}", process.exception->address - *base)});
            attrs.push_back({"module", report.subject.name});
        }
        report.diagnostics.report(DiagnosticEvent{"host", report.final_stage, "unhandled_exception", Severity::error,
                                                  true, "unhandled exception while running the executable",
                                                  std::move(attrs)});
    } else if (worker_error) {
        report.outcome = is_blocking_code(worker_error->code) ? Outcome::blocked : Outcome::failed;
        report.diagnostics.report(DiagnosticEvent{worker_error->component, worker_error->stage, worker_error->code,
                                                  Severity::error, true, worker_error->message, {}});
    } else if (first_fatal != nullptr) {
        report.outcome = is_blocking_code(first_fatal->code) ? Outcome::blocked : Outcome::failed;
    } else if (value) {
        report.outcome = Outcome::pass;
        report.exit_code = *value;
    } else {
        report.outcome = Outcome::failed;
        report.diagnostics.report(DiagnosticEvent{
            "host", "worker", process.abnormal_exit_status ? "abnormal_exit" : "unexpected_exit", Severity::error,
            true, "worker exited without reporting a result",
            {{"exit_code", std::format("0x{:08x}", process.exit_code)}, {"last_stage", report.final_stage}}});
    }
    if (process.report_truncated) {
        report.diagnostics.report(DiagnosticEvent{"host", "worker", "report_truncated", Severity::warning, false,
                                                  "the worker report channel exceeded its limits", {}});
    }
    return report;
}

std::string make_run_id() {
    static std::atomic<unsigned> sequence{0};
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    return std::format("run-{:%Y%m%dT%H%M%SZ}-{}-{}", now, ::GetCurrentProcessId(), sequence++);
}

Result<std::filesystem::path> default_worker_path() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (n == 0) {
            return fail("host", "worker_not_found", std::format("GetModuleFileNameW failed ({})", ::GetLastError()));
        }
        if (n < buffer.size()) {
            buffer.resize(n);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    auto path = std::filesystem::path(buffer).parent_path() / L"scale_worker.exe";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        return fail("host", "worker_not_found",
                    std::format("scale_worker.exe not found next to the executable ({})",
                                to_utf8(path.wstring()).value_or("<path>")));
    }
    return path;
}

Result<ExecutableRun> run_executable(const RunOptions& options) {
    diagnostics::Subject subject{"executable", options.subject_name, std::nullopt};
    if (const auto bytes = loader::read_file(options.executable)) {
        if (auto digest = sha256_hex(ByteView(*bytes))) {
            subject.sha256 = std::move(*digest);
        }
    }
    auto executable_utf8 = to_utf8(std::filesystem::absolute(options.executable).wstring());
    if (!executable_utf8) {
        return std::unexpected(executable_utf8.error());
    }
    auto process = run_supervised(LaunchRequest{options.worker, {"run", *executable_utf8}, options.limits});
    if (!process) {
        return std::unexpected(process.error());
    }
    ExecutableRun run;
    run.returned_value = returned_value(*process);
    run.report = make_run_report(*process, std::move(subject),
                                     options.run_id.empty() ? make_run_id() : options.run_id, options.limits.timeout);
    run.process = std::move(*process);
    return run;
}

}  // namespace scale::host
