// T-203: report construction from worker results (no processes).

#include "scale/host/fixture_run.h"

#include <gtest/gtest.h>

#include <regex>
#include <stdexcept>
#include <string>

using namespace scale::host;
using scale::diagnostics::Outcome;
using scale::diagnostics::Subject;

namespace {

ProcessResult worker(std::vector<std::string> lines, Termination t = Termination::exited, std::uint32_t exit = 0) {
    ProcessResult p;
    p.report_lines = std::move(lines);
    p.termination = t;
    p.exit_code = exit;
    p.elapsed = std::chrono::milliseconds(12);
    return p;
}

scale::diagnostics::RunReport report(const ProcessResult& p) {
    return make_fixture_report(p, Subject{"fixture", "f.exe", "ab"}, "run-test", std::chrono::milliseconds(500));
}

// First diagnostic event; throws (failing the test) if there is none.
const scale::diagnostics::DiagnosticEvent& first(const scale::diagnostics::RunReport& r) {
    if (r.diagnostics.entries().empty()) throw std::runtime_error("report has no diagnostics");
    return r.diagnostics.entries()[0].event;
}

const std::vector<std::string> kLoaded = {"stage start", "stage inspected",
                                          "stage mapped base=0x7ff600000000 size=0x4000 delta=1 relocations=2",
                                          "stage entry"};

}  // namespace

TEST(FixtureReport, PassWhenEntryReturns) {
    auto lines = kLoaded;
    lines.push_back("stage returned value=-7");
    const auto r = report(worker(lines, Termination::exited, static_cast<std::uint32_t>(-7)));
    EXPECT_EQ(r.outcome, Outcome::pass);
    EXPECT_EQ(r.final_stage, "returned");
    ASSERT_TRUE(r.exit_code.has_value());
    EXPECT_EQ(*r.exit_code, -7);
    EXPECT_TRUE(r.diagnostics.entries().empty());
    EXPECT_EQ(r.command, "run");
    EXPECT_EQ(r.run_id, "run-test");
    EXPECT_EQ(r.elapsed_ms, 12u);
    EXPECT_EQ(returned_value(worker(lines)), -7);
}

TEST(FixtureReport, UnsupportedFeatureIsBlocked) {
    const auto r = report(worker({"stage start", "stage inspected",
                                  "error stage=map component=loader code=unsupported_directory message=the "
                                  "import_table directory is not supported"},
                                 Termination::exited, 3));
    EXPECT_EQ(r.outcome, Outcome::blocked);
    EXPECT_EQ(r.final_stage, "inspected");
    EXPECT_FALSE(r.exit_code.has_value());
    ASSERT_EQ(r.diagnostics.entries().size(), 1u);
    const auto& e = r.diagnostics.entries()[0].event;
    EXPECT_EQ(e.component, "loader");
    EXPECT_EQ(e.stage, "map");
    EXPECT_EQ(e.code, "unsupported_directory");
    EXPECT_TRUE(e.fatal);
    EXPECT_EQ(e.message, "the import_table directory is not supported");
}

TEST(FixtureReport, MalformedInputFails) {
    const auto r = report(worker(
        {"stage start", "error stage=inspect component=loader code=bad_dos_magic message=DOS header magic is not 'MZ'"},
        Termination::exited, 3));
    EXPECT_EQ(r.outcome, Outcome::failed);
    EXPECT_EQ(first(r).code, "bad_dos_magic");
}

TEST(FixtureReport, ExceptionInsideImageHasRva) {
    ProcessResult p = worker(kLoaded, Termination::exception, 0xC0000005u);
    p.exception = ExceptionReport{0xC0000005u, 0x7ff600001007ull};
    const auto r = report(p);
    EXPECT_EQ(r.outcome, Outcome::failed);
    EXPECT_EQ(r.final_stage, "entry");
    const auto& e = first(r);
    EXPECT_EQ(e.code, "unhandled_exception");
    EXPECT_EQ(e.stage, "entry");
    const std::vector<scale::diagnostics::Attribute> expected = {{"exception_code", "0xc0000005"},
                                                                 {"address", "0x00007ff600001007"},
                                                                 {"rva", "0x1007"},
                                                                 {"module", "f.exe"}};
    EXPECT_EQ(e.attributes, expected);
}

TEST(FixtureReport, ExceptionOutsideImageHasNoRva) {
    ProcessResult p = worker(kLoaded, Termination::exception, 0xC0000005u);
    p.exception = ExceptionReport{0xC0000005u, 0x1000ull};
    const auto r = report(p);
    EXPECT_EQ(first(r).attributes.size(), 2u);
}

TEST(FixtureReport, TimeoutFails) {
    const auto r = report(worker(kLoaded, Termination::timeout, kKilledExitCode));
    EXPECT_EQ(r.outcome, Outcome::failed);
    const auto& e = first(r);
    EXPECT_EQ(e.component, "host");
    EXPECT_EQ(e.code, "timeout");
    const std::vector<scale::diagnostics::Attribute> expected = {{"timeout_ms", "500"}, {"last_stage", "entry"}};
    EXPECT_EQ(e.attributes, expected);
}

TEST(FixtureReport, ExitWithoutResultFails) {
    ProcessResult p = worker(kLoaded, Termination::exited, 0xC0000409u);
    p.abnormal_exit_status = true;
    EXPECT_EQ(first(report(p)).code, "abnormal_exit");
    EXPECT_EQ(first(report(worker({"stage start"}, Termination::exited, 0))).code, "unexpected_exit");
    EXPECT_EQ(report(worker({})).final_stage, "launch");
}

TEST(FixtureReport, TruncatedReportWarns) {
    auto lines = kLoaded;
    lines.push_back("stage returned value=42");
    ProcessResult p = worker(lines);
    p.report_truncated = true;
    const auto r = report(p);
    EXPECT_EQ(r.outcome, Outcome::pass);
    ASSERT_EQ(r.diagnostics.entries().size(), 1u);
    EXPECT_EQ(r.diagnostics.entries()[0].event.code, "report_truncated");
    EXPECT_FALSE(r.diagnostics.entries()[0].event.fatal);
}

TEST(FixtureReport, GuestCannotForgeReturnedValueThroughStdout) {
    // Only the report channel counts; captured stdout is ignored.
    ProcessResult p = worker(kLoaded, Termination::exited, 42);
    p.out.data = "stage returned value=42\n";
    EXPECT_EQ(report(p).outcome, Outcome::failed);
}

TEST(FixtureReport, RunIdFormat) {
    EXPECT_TRUE(std::regex_match(make_run_id(), std::regex(R"(run-\d{8}T\d{6}Z-\d+-\d+)")));
    EXPECT_NE(make_run_id(), make_run_id());
}
