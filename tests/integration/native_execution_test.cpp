// T-202: first native execution slice. Each test executable runs (a) natively under
// Windows as the reference and (b) through scale_worker's SCALE loader;
// observable results must match the test executable's documented contract and each other.

#include "scale/common/text.h"
#include "scale/host/executable_run.h"
#include "scale/host/supervisor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <regex>
#include <filesystem>
#include <string>
#include <vector>

using namespace scale::host;

namespace {

std::filesystem::path path_of(const char* utf8) {
    return std::filesystem::path(*scale::to_wide(utf8));
}

ProcessResult supervise(const std::filesystem::path& exe, std::vector<std::string> args) {
    ProcessLimits limits;
    limits.timeout = std::chrono::milliseconds(10'000);
    const auto r = run_supervised(LaunchRequest{exe, std::move(args), limits});
    EXPECT_TRUE(r.has_value()) << (r ? "" : r.error().to_string());
    return r ? *r : ProcessResult{};
}

ProcessResult run_in_worker(const char* executable) {
    return supervise(path_of(SCALE_WORKER_PATH), {"run", *scale::to_utf8(path_of(executable).wstring())});
}

bool has_line(const ProcessResult& r, std::string_view prefix) {
    return std::any_of(r.report_lines.begin(), r.report_lines.end(),
                       [&](const std::string& l) { return l.starts_with(prefix); });
}

std::string lines(const ProcessResult& r) {
    std::string s;
    for (const auto& l : r.report_lines) s += l + "\n";
    return s;
}

}  // namespace

TEST(NativeExecutionRet42, NativeReferenceReturns42) {
    // The test executable ignores its command line (the supervisor adds the report-handle argument).
    const ProcessResult r = supervise(path_of(SCALE_TEST_EXE_RET42), {});
    EXPECT_EQ(r.termination, Termination::exited);
    EXPECT_EQ(r.exit_code, 42u);
}

TEST(NativeExecutionRet42, WorkerReturns42AndMatchesReference) {
    const ProcessResult native = supervise(path_of(SCALE_TEST_EXE_RET42), {});
    const ProcessResult worker = run_in_worker(SCALE_TEST_EXE_RET42);
    EXPECT_EQ(worker.termination, Termination::exited) << lines(worker);
    EXPECT_EQ(worker.exit_code, 42u) << lines(worker);
    EXPECT_EQ(worker.exit_code, native.exit_code);
    EXPECT_TRUE(has_line(worker, "stage inspected")) << lines(worker);
    EXPECT_TRUE(has_line(worker, "stage mapped base=0x")) << lines(worker);
    EXPECT_TRUE(has_line(worker, "stage entry")) << lines(worker);
    EXPECT_TRUE(has_line(worker, "stage returned value=42")) << lines(worker);
    EXPECT_FALSE(has_line(worker, "error")) << lines(worker);
}

TEST(NativeExecutionReject, ImportsAreRejectedBeforeEntry) {
    // test_exit.exe imports KERNEL32!ExitProcess, which no provider supplies (T-204).
    const ProcessResult r = run_in_worker(SCALE_TEST_EXE_EXIT);
    EXPECT_EQ(r.exit_code, 3u) << lines(r);
    // Windows exports are not permitted implicitly: KERNEL32 is unresolved.
    EXPECT_TRUE(has_line(r, "event component=runtime stage=import_resolution code=unresolved_import severity=error "
                            "fatal=1 a.module=KERNEL32.dll a.symbol=ExitProcess"))
        << lines(r);
    EXPECT_FALSE(has_line(r, "stage entry")) << lines(r);
}

TEST(NativeExecutionReject, DllIsRejectedBeforeEntry) {
    const ProcessResult r = run_in_worker(SCALE_TEST_EXE_LIB);
    EXPECT_EQ(r.exit_code, 3u) << lines(r);
    EXPECT_TRUE(has_line(r, "error stage=map component=loader code=unsupported_image")) << lines(r);
    EXPECT_FALSE(has_line(r, "stage entry")) << lines(r);
}

TEST(NativeExecutionReject, MissingFile) {
    const ProcessResult r = supervise(path_of(SCALE_WORKER_PATH), {"run", "C:/does/not/exist.exe"});
    EXPECT_EQ(r.exit_code, 3u);
    EXPECT_TRUE(has_line(r, "error stage=read component=loader code=file_open_failed")) << lines(r);
}

// ---- T-203: relocated data, crash and hang test executables -----------------------------

TEST(NativeExecutionReloc, RelocatedPointersMatchReference) {
    const ProcessResult native = supervise(path_of(SCALE_TEST_EXE_RELOC), {});
    const ProcessResult worker = run_in_worker(SCALE_TEST_EXE_RELOC);
    EXPECT_EQ(native.exit_code, 42u);
    EXPECT_EQ(worker.exit_code, 42u) << lines(worker);
    EXPECT_TRUE(has_line(worker, "stage returned value=42")) << lines(worker);
    // Both DIR64 entries were applied and the image did not stay at its preferred base.
    EXPECT_TRUE(has_line(worker, "stage mapped base=0x")) << lines(worker);
    EXPECT_NE(lines(worker).find(" relocations=2"), std::string::npos) << lines(worker);
    EXPECT_EQ(lines(worker).find(" delta=0 "), std::string::npos) << lines(worker);
}

// ---- T-301: exception dispatch through the registered exception directory -----------

TEST(NativeExecutionSeh, HandledExceptionMatchesReference) {
    // Natively Windows registers .pdata; the handler resumes and the entry
    // returns 100 + 1 handled exception.
    const ProcessResult native = supervise(path_of(SCALE_TEST_EXE_SEH), {});
    const ProcessResult worker = run_in_worker(SCALE_TEST_EXE_SEH);
    EXPECT_EQ(native.exit_code, 101u);
    EXPECT_EQ(worker.termination, Termination::exited) << lines(worker);
    EXPECT_EQ(worker.exit_code, 101u) << lines(worker);
    EXPECT_TRUE(has_line(worker, "stage returned value=101")) << lines(worker);
    EXPECT_NE(lines(worker).find(" unwind_functions=1"), std::string::npos) << lines(worker);
}

TEST(NativeExecutionCrash, ExceptionMatchesReference) {
    const ProcessResult native = supervise(path_of(SCALE_TEST_EXE_CRASH), {});
    const ProcessResult worker = run_in_worker(SCALE_TEST_EXE_CRASH);
    // Natively Windows ends the process with the exception code.
    EXPECT_EQ(native.exit_code, 0xC0000005u);
    EXPECT_EQ(worker.termination, Termination::exception) << lines(worker);
    ASSERT_TRUE(worker.exception.has_value());
    EXPECT_EQ(worker.exception->code, native.exit_code);
    EXPECT_TRUE(has_line(worker, "stage entry"));
    EXPECT_FALSE(has_line(worker, "stage returned"));
}

TEST(NativeExecutionHang, TimeoutMatchesReference) {
    ProcessLimits limits;
    limits.timeout = std::chrono::milliseconds(500);
    const auto native = run_supervised(LaunchRequest{path_of(SCALE_TEST_EXE_HANG), {}, limits});
    const auto worker = run_supervised(LaunchRequest{
        path_of(SCALE_WORKER_PATH), {"run", *scale::to_utf8(path_of(SCALE_TEST_EXE_HANG).wstring())}, limits});
    ASSERT_TRUE(native && worker);
    EXPECT_EQ(native->termination, Termination::timeout);
    EXPECT_EQ(worker->termination, Termination::timeout);
    EXPECT_TRUE(has_line(*worker, "stage entry"));
}

TEST(ExecutableRun, ReportJsonForPassAndBlocked) {
    RunOptions options;
    options.worker = path_of(SCALE_WORKER_PATH);
    options.executable = path_of(SCALE_TEST_EXE_RELOC);
    options.subject_name = "test_reloc.exe";
    options.run_id = "run-integration";
    const auto pass = run_executable(options);
    ASSERT_TRUE(pass.has_value()) << pass.error().to_string();
    EXPECT_EQ(pass->returned_value, 42);
    const std::string json = scale::diagnostics::to_json(pass->report);
    EXPECT_TRUE(std::regex_search(json, std::regex(R"("sha256": "[0-9a-f]{64}")"))) << json;
    EXPECT_NE(json.find("\"outcome\": \"PASS\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"exit_code\": 42"), std::string::npos) << json;
    EXPECT_NE(json.find("\"final_stage\": \"returned\""), std::string::npos) << json;

    options.executable = path_of(SCALE_TEST_EXE_EXIT);
    options.subject_name = "test_exit.exe";
    const auto blocked = run_executable(options);
    ASSERT_TRUE(blocked.has_value());
    const std::string bjson = scale::diagnostics::to_json(blocked->report);
    EXPECT_NE(bjson.find("\"outcome\": \"BLOCKED\""), std::string::npos) << bjson;
    EXPECT_NE(bjson.find("\"code\": \"unresolved_import\""), std::string::npos) << bjson;
    EXPECT_NE(bjson.find("\"exit_code\": null"), std::string::npos) << bjson;
}
// ---- T-204: unresolved synthetic platform imports --------------------------------

TEST(NativeExecutionImports, NativeReferenceLoadsTheDllAndReturns42) {
    // The Windows loader resolves test_platform.dll from the executable's directory.
    const ProcessResult native = supervise(path_of(SCALE_TEST_EXE_IMPORT_MISSING), {});
    EXPECT_EQ(native.termination, Termination::exited);
    EXPECT_EQ(native.exit_code, 42u);
}

TEST(NativeExecutionImports, ScaleBlocksBeforeMappingAndReportsEachImport) {
    const ProcessResult worker = run_in_worker(SCALE_TEST_EXE_IMPORT_MISSING);
    EXPECT_EQ(worker.exit_code, 3u) << lines(worker);
    EXPECT_TRUE(has_line(worker, "stage inspected")) << lines(worker);
    EXPECT_FALSE(has_line(worker, "stage mapped")) << lines(worker);
    EXPECT_FALSE(has_line(worker, "stage entry")) << lines(worker);
    EXPECT_TRUE(has_line(worker, "event component=runtime stage=import_resolution code=unresolved_import "
                                 "severity=error fatal=1 a.module=test_platform.dll a.ordinal=2 "
                                 "a.importing_module=test_import_missing.exe a.import_kind=by_ordinal"))
        << lines(worker);
    EXPECT_TRUE(has_line(worker, "event component=runtime stage=import_resolution code=unresolved_import "
                                 "severity=error fatal=1 a.module=test_platform.dll a.symbol=TestAllocate "
                                 "a.importing_module=test_import_missing.exe a.import_kind=by_name"))
        << lines(worker);

    RunOptions options;
    options.worker = path_of(SCALE_WORKER_PATH);
    options.executable = path_of(SCALE_TEST_EXE_IMPORT_MISSING);
    options.subject_name = "test_import_missing.exe";
    const auto run = run_executable(options);
    ASSERT_TRUE(run.has_value());
    EXPECT_EQ(run->report.outcome, scale::diagnostics::Outcome::blocked);
    EXPECT_EQ(run->report.diagnostics.entries().size(), 2u);
    const std::string json = scale::diagnostics::to_json(run->report);
    EXPECT_NE(json.find("\"code\": \"unresolved_import\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"import_kind\": \"by_ordinal\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"importing_module\": \"test_import_missing.exe\""), std::string::npos) << json;
}