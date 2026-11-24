#pragma once

// Diagnostic events and a bounded, deduplicating collector.
// Contract: docs/contracts/diagnostics.md (schema_version 1).

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace scale::diagnostics {

class JsonWriter;

enum class Severity { info, warning, error };

[[nodiscard]] std::string_view to_string(Severity severity) noexcept;

// A component-specific key/value detail, e.g. {"module", "test_platform.dll"}.
// Keys are lowercase snake_case. Values are bounded, non-secret text.
struct Attribute {
    std::string key;
    std::string value;

    friend bool operator==(const Attribute&, const Attribute&) = default;
};

struct DiagnosticEvent {
    std::string component;  // "loader", "runtime", "graphics", "shader", ...
    std::string stage;      // "pe_parse", "import_resolution", "spirv_emit", ...
    std::string code;       // stable machine identifier, e.g. "unresolved_import"
    Severity severity = Severity::error;
    bool fatal = false;
    std::string message;    // human-readable English text
    std::vector<Attribute> attributes;

    friend bool operator==(const DiagnosticEvent&, const DiagnosticEvent&) = default;
};

// One deduplicated event with occurrence statistics. Sequence numbers are
// 1-based positions in the stream of all reported events.
struct DiagnosticEntry {
    DiagnosticEvent event;
    std::uint64_t count = 0;
    std::uint64_t first_sequence = 0;
    std::uint64_t last_sequence = 0;
};

class DiagnosticLog {
public:
    // Default bound on distinct entries; further distinct events are counted in
    // dropped() but not stored. Duplicates of stored entries are always counted.
    static constexpr std::size_t kDefaultMaxEntries = 4096;

    explicit DiagnosticLog(std::size_t max_entries = kDefaultMaxEntries);

    void report(DiagnosticEvent event);

    [[nodiscard]] std::span<const DiagnosticEntry> entries() const noexcept { return entries_; }
    [[nodiscard]] std::uint64_t total_reported() const noexcept { return sequence_; }
    [[nodiscard]] std::uint64_t dropped() const noexcept { return dropped_; }
    [[nodiscard]] bool has_fatal() const noexcept { return fatal_seen_; }
    [[nodiscard]] bool has_errors() const noexcept { return error_seen_; }

    // Writes the entries as a JSON array value.
    void write_json(JsonWriter& writer) const;

    // Concise human-readable blocker summary, one line per entry, most severe
    // first. Returns "no diagnostics\n" when empty.
    [[nodiscard]] std::string summary() const;

private:
    std::vector<DiagnosticEntry> entries_;
    std::size_t max_entries_;
    std::uint64_t sequence_ = 0;
    std::uint64_t dropped_ = 0;
    bool fatal_seen_ = false;
    bool error_seen_ = false;
};

}  // namespace scale::diagnostics
