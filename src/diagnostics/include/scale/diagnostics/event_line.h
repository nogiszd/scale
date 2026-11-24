#pragma once

// One-line text encoding of a DiagnosticEvent for process boundaries (the
// worker report channel, see docs/contracts/run.md):
//
//   event component=<c> stage=<s> code=<k> severity=<info|warning|error>
//         fatal=<0|1> a.<key>=<value> ... message=<text>
//
// All values are percent-encoded: bytes outside [A-Za-z0-9._:/\-] become
// %XX (uppercase hex), so values never contain spaces or '='. Attribute
// order is preserved. `message` is always last.

#include "scale/diagnostics/diagnostic_log.h"

#include <optional>
#include <string>
#include <string_view>

namespace scale::diagnostics {

[[nodiscard]] std::string to_event_line(const DiagnosticEvent& event);

// Parses a line produced by to_event_line. Returns nullopt for anything
// malformed (unknown keys, bad escapes, missing fields).
[[nodiscard]] std::optional<DiagnosticEvent> parse_event_line(std::string_view line);

[[nodiscard]] std::string percent_encode(std::string_view value);
[[nodiscard]] std::optional<std::string> percent_decode(std::string_view value);

}  // namespace scale::diagnostics
