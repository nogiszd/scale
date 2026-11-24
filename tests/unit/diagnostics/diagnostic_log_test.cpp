#include "scale/diagnostics/diagnostic_log.h"

#include <gtest/gtest.h>

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

TEST(DiagnosticLog, DeduplicatesIdenticalEvents) {
    DiagnosticLog log;
    log.report(unresolved("FixtureAllocate"));
    log.report(unresolved("FixtureFree"));
    log.report(unresolved("FixtureAllocate"));

    ASSERT_EQ(log.entries().size(), 2u);
    EXPECT_EQ(log.total_reported(), 3u);
    const auto& first = log.entries()[0];
    EXPECT_EQ(first.count, 2u);
    EXPECT_EQ(first.first_sequence, 1u);
    EXPECT_EQ(first.last_sequence, 3u);
    EXPECT_EQ(log.entries()[1].count, 1u);
    EXPECT_TRUE(log.has_fatal());
    EXPECT_TRUE(log.has_errors());
}

TEST(DiagnosticLog, IsBounded) {
    DiagnosticLog log(2);
    log.report(unresolved("A"));
    log.report(unresolved("B"));
    log.report(unresolved("C"));
    log.report(unresolved("A"));
    EXPECT_EQ(log.entries().size(), 2u);
    EXPECT_EQ(log.dropped(), 1u);
    EXPECT_EQ(log.entries()[0].count, 2u);
    EXPECT_EQ(log.total_reported(), 4u);
}

TEST(DiagnosticLog, SummaryOrdersBySeverity) {
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
    ASSERT_NE(fatal_pos, std::string::npos);
    ASSERT_NE(warn_pos, std::string::npos);
    EXPECT_LT(fatal_pos, warn_pos);
    EXPECT_NE(summary.find("symbol=FixtureAllocate"), std::string::npos);
}

TEST(DiagnosticLog, EmptySummary) {
    const DiagnosticLog log;
    EXPECT_EQ(log.summary(), "no diagnostics\n");
    EXPECT_FALSE(log.has_fatal());
}
