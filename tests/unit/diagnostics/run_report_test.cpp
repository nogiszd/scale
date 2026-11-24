#include "scale/common/version.h"
#include "scale/diagnostics/run_report.h"

#include <gtest/gtest.h>

#include <string>

using namespace scale::diagnostics;

// Golden test for report schema_version 1 (docs/contracts/diagnostics.md).
// Changing this output is a contract change: update the contract document and
// bump kReportSchemaVersion for any removal, rename or type change.
TEST(RunReport, SchemaV1Golden) {
    RunReport r;
    r.run_id = "test-run-0001";
    r.command = "run";
    r.subject = Subject{"fixture", "missing_import.exe", std::nullopt};
    r.final_stage = "import_resolution";
    r.outcome = Outcome::blocked;
    r.elapsed_ms = 12;
    r.diagnostics.report(DiagnosticEvent{
        .component = "runtime",
        .stage = "import_resolution",
        .code = "unresolved_import",
        .severity = Severity::error,
        .fatal = true,
        .message = "no resolver accepted this import",
        .attributes = {{"module", "fixture_platform.dll"}, {"symbol", "FixtureAllocate"}},
    });

    const std::string expected = std::string("{\n"
                                             "  \"schema_version\": 1,\n"
                                             "  \"run_id\": \"test-run-0001\",\n"
                                             "  \"tool\": {\n"
                                             "    \"name\": \"scale\",\n"
                                             "    \"version\": \"") +
                                 scale::kVersionString +
                                 "\"\n"
                                 "  },\n"
                                 "  \"command\": \"run\",\n"
                                 "  \"subject\": {\n"
                                 "    \"kind\": \"fixture\",\n"
                                 "    \"name\": \"missing_import.exe\",\n"
                                 "    \"sha256\": null\n"
                                 "  },\n"
                                 "  \"final_stage\": \"import_resolution\",\n"
                                 "  \"outcome\": \"BLOCKED\",\n"
                                 "  \"exit_code\": null,\n"
                                 "  \"elapsed_ms\": 12,\n"
                                 "  \"diagnostics_total\": 1,\n"
                                 "  \"diagnostics_dropped\": 0,\n"
                                 "  \"diagnostics\": [\n"
                                 "    {\n"
                                 "      \"component\": \"runtime\",\n"
                                 "      \"stage\": \"import_resolution\",\n"
                                 "      \"code\": \"unresolved_import\",\n"
                                 "      \"severity\": \"error\",\n"
                                 "      \"fatal\": true,\n"
                                 "      \"message\": \"no resolver accepted this import\",\n"
                                 "      \"attributes\": {\n"
                                 "        \"module\": \"fixture_platform.dll\",\n"
                                 "        \"symbol\": \"FixtureAllocate\"\n"
                                 "      },\n"
                                 "      \"count\": 1,\n"
                                 "      \"first_sequence\": 1,\n"
                                 "      \"last_sequence\": 1\n"
                                 "    }\n"
                                 "  ]\n"
                                 "}\n";
    EXPECT_EQ(to_json(r), expected);
}

TEST(RunReport, OutcomeNamesMatchReportingVocabulary) {
    EXPECT_EQ(to_string(Outcome::pass), std::string_view("PASS"));
    EXPECT_EQ(to_string(Outcome::failed), std::string_view("FAILED"));
    EXPECT_EQ(to_string(Outcome::blocked), std::string_view("BLOCKED"));
    EXPECT_EQ(to_string(Outcome::skipped), std::string_view("SKIPPED"));
    EXPECT_EQ(to_string(Outcome::not_tested), std::string_view("NOT_TESTED"));
}
