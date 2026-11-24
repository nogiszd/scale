// T-202: first native execution slice. Each fixture runs (a) natively under
// Windows as the reference and (b) through scale_worker's fixture loader;
// observable results must match the fixture's documented contract and each other.

#include "scale/common/text.h"
#include "scale/host/supervisor.h"

#include <gtest/gtest.h>

#include <algorithm>
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

ProcessResult run_in_worker(const char* fixture) {
    return supervise(path_of(SCALE_WORKER_PATH), {"run-fixture", *scale::to_utf8(path_of(fixture).wstring())});
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

TEST(NativeFixtureRet42, NativeReferenceReturns42) {
    // The fixture ignores its command line (the supervisor adds the report-handle argument).
    const ProcessResult r = supervise(path_of(SCALE_FIXTURE_RET42), {});
    EXPECT_EQ(r.termination, Termination::exited);
    EXPECT_EQ(r.exit_code, 42u);
}

TEST(NativeFixtureRet42, WorkerReturns42AndMatchesReference) {
    const ProcessResult native = supervise(path_of(SCALE_FIXTURE_RET42), {});
    const ProcessResult worker = run_in_worker(SCALE_FIXTURE_RET42);
    EXPECT_EQ(worker.termination, Termination::exited) << lines(worker);
    EXPECT_EQ(worker.exit_code, 42u) << lines(worker);
    EXPECT_EQ(worker.exit_code, native.exit_code);
    EXPECT_TRUE(has_line(worker, "stage inspected")) << lines(worker);
    EXPECT_TRUE(has_line(worker, "stage mapped base=0x")) << lines(worker);
    EXPECT_TRUE(has_line(worker, "stage entry")) << lines(worker);
    EXPECT_TRUE(has_line(worker, "stage returned value=42")) << lines(worker);
    EXPECT_FALSE(has_line(worker, "error")) << lines(worker);
}

TEST(NativeFixtureReject, ImportsAreRejectedBeforeEntry) {
    // fixture_exit.exe imports KERNEL32!ExitProcess: unsupported by the T-202 loader.
    const ProcessResult r = run_in_worker(SCALE_FIXTURE_EXIT);
    EXPECT_EQ(r.exit_code, 3u) << lines(r);
    EXPECT_TRUE(has_line(r, "error component=loader code=unsupported_directory message=the import_table"))
        << lines(r);
    EXPECT_FALSE(has_line(r, "stage entry")) << lines(r);
}

TEST(NativeFixtureReject, DllIsRejectedBeforeEntry) {
    const ProcessResult r = run_in_worker(SCALE_FIXTURE_LIB);
    EXPECT_EQ(r.exit_code, 3u) << lines(r);
    EXPECT_TRUE(has_line(r, "error component=loader code=unsupported_image")) << lines(r);
    EXPECT_FALSE(has_line(r, "stage entry")) << lines(r);
}

TEST(NativeFixtureReject, MissingFile) {
    const ProcessResult r = supervise(path_of(SCALE_WORKER_PATH), {"run-fixture", "C:/does/not/exist.exe"});
    EXPECT_EQ(r.exit_code, 3u);
    EXPECT_TRUE(has_line(r, "error component=loader code=file_open_failed")) << lines(r);
}
