#pragma once

// Running a trusted fixture through scale_worker and building its report
// (T-203). Contract: docs/contracts/run.md.

#include "scale/common/error.h"
#include "scale/diagnostics/run_report.h"
#include "scale/host/supervisor.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace scale::host {

struct FixtureRunOptions {
    std::filesystem::path worker;   // absolute path to scale_worker.exe
    std::filesystem::path fixture;  // fixture image (read-only)
    std::string subject_name;       // name shown in reports (usually the path as given)
    ProcessLimits limits;
    std::string run_id;             // empty = generate one
};

struct FixtureRun {
    ProcessResult process;
    diagnostics::RunReport report;
    std::optional<int> returned_value;  // set when the entry returned
};

// Launches the worker in run-fixture mode and builds the report. Errors are
// returned only when the worker could not be supervised at all.
[[nodiscard]] Result<FixtureRun> run_fixture(const FixtureRunOptions& options);

// Builds the report from a finished worker process (pure; exposed for tests).
//   PASS     the entry returned (exit_code = returned value)
//   BLOCKED  the loader rejected an unsupported feature or unresolved import
//   FAILED   malformed/unreadable input, unhandled exception, timeout or
//            an unexpected worker exit
[[nodiscard]] diagnostics::RunReport make_fixture_report(const ProcessResult& process,
                                                         diagnostics::Subject subject, std::string run_id,
                                                         std::chrono::milliseconds timeout);

// The returned value parsed from a `stage returned value=N` line, if any.
[[nodiscard]] std::optional<int> returned_value(const ProcessResult& process);

// "run-<UTC yyyymmddThhmmssZ>-<pid>-<sequence>"
[[nodiscard]] std::string make_run_id();

// scale_worker.exe in the directory of the current executable.
[[nodiscard]] Result<std::filesystem::path> default_worker_path();

}  // namespace scale::host
