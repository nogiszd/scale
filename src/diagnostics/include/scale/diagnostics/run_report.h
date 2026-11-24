#pragma once

// Versioned machine-readable run report (report.json).
// Contract: docs/contracts/diagnostics.md.

#include "scale/diagnostics/diagnostic_log.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace scale::diagnostics {

inline constexpr int kReportSchemaVersion = 1;

// Result classification for a run or a stage. Matches the compatibility
// reporting vocabulary in AGENTS.md.
enum class Outcome { pass, failed, blocked, skipped, not_tested };

[[nodiscard]] std::string_view to_string(Outcome outcome) noexcept;

struct Subject {
    std::string kind;  // "fixture", "shader_test", ...
    std::string name;  // display name or path relative to a documented root
    std::optional<std::string> sha256;
};

struct RunReport {
    std::string run_id;
    std::string command;      // CLI command, e.g. "inspect", "run"
    Subject subject;
    std::string final_stage;  // last stage reached, e.g. "import_resolution"
    Outcome outcome = Outcome::not_tested;
    std::optional<std::int64_t> exit_code;
    std::optional<std::uint64_t> elapsed_ms;
    DiagnosticLog diagnostics;
};

// Serializes `report` as a complete JSON document (trailing newline included).
[[nodiscard]] std::string to_json(const RunReport& report);

}  // namespace scale::diagnostics
