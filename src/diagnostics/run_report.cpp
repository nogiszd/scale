#include "scale/diagnostics/run_report.h"

#include "scale/common/version.h"
#include "scale/diagnostics/json_writer.h"

namespace scale::diagnostics {

std::string_view to_string(Outcome outcome) noexcept {
    switch (outcome) {
        case Outcome::pass: return "PASS";
        case Outcome::failed: return "FAILED";
        case Outcome::blocked: return "BLOCKED";
        case Outcome::skipped: return "SKIPPED";
        case Outcome::not_tested: return "NOT_TESTED";
    }
    return "UNKNOWN";
}

std::string to_json(const RunReport& r) {
    JsonWriter w;
    w.begin_object();
    w.field("schema_version", kReportSchemaVersion);
    w.field("run_id", r.run_id);

    w.key("tool");
    w.begin_object();
    w.field("name", "scale");
    w.field("version", kVersionString);
    w.end_object();

    w.field("command", r.command);

    w.key("subject");
    w.begin_object();
    w.field("kind", r.subject.kind);
    w.field("name", r.subject.name);
    w.key("sha256");
    if (r.subject.sha256) {
        w.value(*r.subject.sha256);
    } else {
        w.null();
    }
    w.end_object();

    w.field("final_stage", r.final_stage);
    w.field("outcome", to_string(r.outcome));
    w.key("exit_code");
    if (r.exit_code) {
        w.value(*r.exit_code);
    } else {
        w.null();
    }
    w.key("elapsed_ms");
    if (r.elapsed_ms) {
        w.value(*r.elapsed_ms);
    } else {
        w.null();
    }

    w.field("diagnostics_total", r.diagnostics.total_reported());
    w.field("diagnostics_dropped", r.diagnostics.dropped());
    w.key("diagnostics");
    r.diagnostics.write_json(w);
    w.end_object();

    std::string out = w.str();
    out += '\n';
    return out;
}

}  // namespace scale::diagnostics
