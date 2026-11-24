#include "scale/diagnostics/diagnostic_log.h"

#include "scale_test.h"

#include <string>

using scale::diagnostics::DiagnosticEvent;
using scale::diagnostics::DiagnosticLog;
using scale::diagnostics::Severity;

namespace {

DiagnosticEvent unresolved(std::string symbol) {
    return DiagnosticEvent{
        .component = "runtime",
        .stage = "import_resolution",
        .code = "unresolved_import",
        .severity = Severity::error,
        .fatal = true,
        .message = "no adapter or approved module exports this symbol",
        .attributes = {{"module", "fixture_platform.dll"}, {"symbol", std::move(symbol)}},
    };
}

}  // namespace

SCALE_TEST(log_deduplicates_identical_events) {
    DiagnosticLog log;
    log.report(unresolved("FixtureAllocate"));
    log.report(unresolved("FixtureFree"));
    log.report(unresolved("FixtureAllocate"));

    SCALE_REQUIRE(log.entries().size() == 2);
    SCALE_CHECK_EQ(log.total_reported(), 3u);
    const auto& first = log.entries()[0];
    SCALE_CHECK_EQ(first.count, 2u);
    SCALE_CHECK_EQ(first.first_sequence, 1u);
    SCALE_CHECK_EQ(first.last_sequence, 3u);
    SCALE_CHECK_EQ(log.entries()[1].count, 1u);
    SCALE_CHECK(log.has_fatal());
    SCALE_CHECK(log.has_errors());
}

SCALE_TEST(log_is_bounded) {
    DiagnosticLog log(2);
    log.report(unresolved("A"));
    log.report(unresolved("B"));
    log.report(unresolved("C"));
    log.report(unresolved("A"));
    SCALE_CHECK_EQ(log.entries().size(), 2u);
    SCALE_CHECK_EQ(log.dropped(), 1u);
    SCALE_CHECK_EQ(log.entries()[0].count, 2u);
    SCALE_CHECK_EQ(log.total_reported(), 4u);
}

SCALE_TEST(log_summary_orders_by_severity) {
    DiagnosticLog log;
    log.report(DiagnosticEvent{.component = "loader",
                               .stage = "pe_parse",
                               .code = "unknown_section_flag",
                               .severity = Severity::warning,
                               .fatal = false,
                               .message = "",
                               .attributes = {}});
    log.report(unresolved("FixtureAllocate"));
    const std::string summary = log.summary();
    const auto fatal_pos = summary.find("[error, fatal] runtime/import_resolution/unresolved_import");
    const auto warn_pos = summary.find("[warning] loader/pe_parse/unknown_section_flag");
    SCALE_REQUIRE(fatal_pos != std::string::npos);
    SCALE_REQUIRE(warn_pos != std::string::npos);
    SCALE_CHECK(fatal_pos < warn_pos);
    SCALE_CHECK(summary.find("symbol=FixtureAllocate") != std::string::npos);
}

SCALE_TEST(log_empty_summary) {
    const DiagnosticLog log;
    SCALE_CHECK_EQ(log.summary(), std::string("no diagnostics\n"));
    SCALE_CHECK(!log.has_fatal());
}
