#include "scale/diagnostics/diagnostic_log.h"

#include "scale/diagnostics/json_writer.h"

#include <algorithm>
#include <format>

namespace scale::diagnostics {

std::string_view to_string(Severity severity) noexcept {
    switch (severity) {
        case Severity::info: return "info";
        case Severity::warning: return "warning";
        case Severity::error: return "error";
    }
    return "unknown";
}

DiagnosticLog::DiagnosticLog(std::size_t max_entries) : max_entries_(max_entries) {}

void DiagnosticLog::report(DiagnosticEvent event) {
    ++sequence_;
    fatal_seen_ = fatal_seen_ || event.fatal;
    error_seen_ = error_seen_ || event.severity == Severity::error;

    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const DiagnosticEntry& e) { return e.event == event; });
    if (it != entries_.end()) {
        ++it->count;
        it->last_sequence = sequence_;
        return;
    }
    if (entries_.size() >= max_entries_) {
        ++dropped_;
        return;
    }
    entries_.push_back(DiagnosticEntry{std::move(event), 1, sequence_, sequence_});
}

void DiagnosticLog::write_json(JsonWriter& w) const {
    w.begin_array();
    for (const DiagnosticEntry& entry : entries_) {
        const DiagnosticEvent& e = entry.event;
        w.begin_object();
        w.field("component", e.component);
        w.field("stage", e.stage);
        w.field("code", e.code);
        w.field("severity", to_string(e.severity));
        w.field("fatal", e.fatal);
        w.field("message", e.message);
        w.key("attributes");
        w.begin_object();
        for (const Attribute& a : e.attributes) {
            w.field(a.key, a.value);
        }
        w.end_object();
        w.field("count", entry.count);
        w.field("first_sequence", entry.first_sequence);
        w.field("last_sequence", entry.last_sequence);
        w.end_object();
    }
    w.end_array();
}

std::string DiagnosticLog::summary() const {
    if (entries_.empty() && dropped_ == 0) {
        return "no diagnostics\n";
    }
    const auto rank = [](const DiagnosticEntry& e) {
        return (e.event.fatal ? 4 : 0) + static_cast<int>(e.event.severity);
    };
    std::vector<const DiagnosticEntry*> ordered;
    ordered.reserve(entries_.size());
    for (const DiagnosticEntry& e : entries_) {
        ordered.push_back(&e);
    }
    std::stable_sort(ordered.begin(), ordered.end(),
                     [&](const DiagnosticEntry* a, const DiagnosticEntry* b) { return rank(*a) > rank(*b); });

    std::string out;
    for (const DiagnosticEntry* e : ordered) {
        const DiagnosticEvent& ev = e->event;
        out += std::format("[{}{}] {}/{}/{}", to_string(ev.severity), ev.fatal ? ", fatal" : "",
                           ev.component, ev.stage, ev.code);
        for (const Attribute& a : ev.attributes) {
            out += std::format(" {}={}", a.key, a.value);
        }
        if (!ev.message.empty()) {
            out += std::format(": {}", ev.message);
        }
        if (e->count > 1) {
            out += std::format(" (x{})", e->count);
        }
        out += '\n';
    }
    if (dropped_ != 0) {
        out += std::format("[warning] diagnostics/collector/entries_dropped: {} distinct events not stored\n",
                           dropped_);
    }
    return out;
}

}  // namespace scale::diagnostics
