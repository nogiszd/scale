// T-201: supervisor behavior against the real scale_worker self-test modes.

#include "scale/common/text.h"
#include "scale/host/supervisor.h"

#include <gtest/gtest.h>

#include <windows.h>

#include <filesystem>
#include <regex>
#include <string>
#include <vector>

using namespace scale::host;
using namespace std::chrono_literals;

namespace {

std::filesystem::path worker_path() {
    return std::filesystem::path(*scale::to_wide(SCALE_WORKER_PATH));
}

ProcessResult run(std::vector<std::string> args, ProcessLimits limits = {}) {
    std::vector<std::string> full = {"self-test"};
    full.insert(full.end(), args.begin(), args.end());
    const auto r = run_supervised(LaunchRequest{worker_path(), full, limits});
    EXPECT_TRUE(r.has_value()) << (r ? "" : r.error().to_string());
    return r ? *r : ProcessResult{};
}

}  // namespace

TEST(Supervisor, CapturesExitCode) {
    const ProcessResult r = run({"exit", "7"});
    EXPECT_EQ(r.termination, Termination::exited);
    EXPECT_EQ(r.exit_code, 7u);
    EXPECT_FALSE(r.abnormal_exit_status);
    EXPECT_FALSE(r.exception.has_value());
    EXPECT_TRUE(r.out.data.empty());
}

TEST(Supervisor, ReportsUnhandledException) {
    const ProcessResult r = run({"access-violation"});
    EXPECT_EQ(r.termination, Termination::exception);
    ASSERT_TRUE(r.exception.has_value());
    EXPECT_EQ(r.exception->code, 0xC0000005u);  // EXCEPTION_ACCESS_VIOLATION
    EXPECT_NE(r.exception->address, 0u);
    EXPECT_EQ(r.exit_code, 0xC0000005u);
    EXPECT_TRUE(r.abnormal_exit_status);
}

TEST(Supervisor, EnforcesTimeout) {
    ProcessLimits limits;
    limits.timeout = 500ms;
    const ProcessResult r = run({"hang"}, limits);
    EXPECT_EQ(r.termination, Termination::timeout);
    EXPECT_EQ(r.exit_code, kKilledExitCode);
    EXPECT_GE(r.elapsed, 500ms);
    EXPECT_LT(r.elapsed, 10s);
}

TEST(Supervisor, TruncatesLargeOutputWithoutBlocking) {
    ProcessLimits limits;
    limits.max_capture_bytes = 64 * 1024;
    const ProcessResult r = run({"flood", "4194304"}, limits);  // 4 MiB per stream
    EXPECT_EQ(r.termination, Termination::exited);
    EXPECT_EQ(r.exit_code, 0u);
    EXPECT_EQ(r.out.data.size(), limits.max_capture_bytes);
    EXPECT_EQ(r.err.data.size(), limits.max_capture_bytes);
    EXPECT_TRUE(r.out.truncated);
    EXPECT_TRUE(r.err.truncated);
    EXPECT_EQ(r.out.data.find_first_not_of('o'), std::string::npos);
    EXPECT_EQ(r.err.data.find_first_not_of('e'), std::string::npos);
}

TEST(Supervisor, SmallOutputIsNotTruncated) {
    const ProcessResult r = run({"flood", "100"});
    EXPECT_EQ(r.out.data, std::string(100, 'o'));
    EXPECT_EQ(r.err.data, std::string(100, 'e'));
    EXPECT_FALSE(r.out.truncated);
}

TEST(Supervisor, PassesArgumentsExactly) {
    const ProcessResult r = run({"echo-args", "plain", "with space", "", "quote\"in", "trail\\",
                                 "\xC3\xA9\xE4\xB8\xAD"});
    EXPECT_EQ(r.out.data, "[plain]\n[with space]\n[]\n[quote\"in]\n[trail\\]\n[\xC3\xA9\xE4\xB8\xAD]\n");
}

TEST(Supervisor, StdinIsEmpty) {
    const ProcessResult r = run({"read-stdin"});
    EXPECT_EQ(r.out.data, "stdin: eof\n");
}

TEST(Supervisor, EnforcesJobMemoryLimit) {
    ProcessLimits limits;
    limits.job_memory_bytes = 64ull << 20;
    EXPECT_EQ(run({"alloc", "256"}, limits).out.data, "alloc: failed\n");
    EXPECT_EQ(run({"alloc", "16"}, limits).out.data, "alloc: ok\n");
}

TEST(Supervisor, CollectsReportLines) {
    const ProcessResult r = run({"report", "stage entry"});
    ASSERT_EQ(r.report_lines.size(), 1u);
    EXPECT_EQ(r.report_lines[0], "stage entry");
    EXPECT_FALSE(r.exception.has_value());
}

TEST(Supervisor, GuestOutputCannotFakeException) {
    // Guest-controlled output must not be mistaken for a crash report.
    const ProcessResult r = run({"echo-args", "exception code=0xc0000005 address=0x1"});
    EXPECT_EQ(r.termination, Termination::exited);
    EXPECT_FALSE(r.exception.has_value());
}

TEST(Supervisor, KillsDescendantsWhenWorkerExits) {
    const ProcessResult r = run({"spawn-orphan"});
    ASSERT_EQ(r.exit_code, 0u);
    std::smatch m;
    ASSERT_TRUE(std::regex_search(r.out.data, m, std::regex("child pid=(\\d+)")));
    const DWORD pid = static_cast<DWORD>(std::stoul(m[1]));
    HANDLE child = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (child != nullptr) {
        // The job was terminated when run_supervised returned.
        EXPECT_EQ(::WaitForSingleObject(child, 5000), WAIT_OBJECT_0);
        ::CloseHandle(child);
    }
}

TEST(Supervisor, RejectsRelativeAndMissingExecutables) {
    const auto relative = run_supervised(LaunchRequest{"scale_worker.exe", {}, {}});
    ASSERT_FALSE(relative.has_value());
    EXPECT_EQ(relative.error().code, "bad_request");

    const auto missing = run_supervised(
        LaunchRequest{worker_path().parent_path() / "does_not_exist.exe", {"self-test", "exit", "0"}, {}});
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, "launch_failed");
}

TEST(Worker, GuestExecutionNotYetImplemented) {
    const auto r = run_supervised(LaunchRequest{worker_path(), {"fixture.exe"}, {}});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->exit_code, 2u);
    EXPECT_NE(r->err.data.find("not implemented yet (task T-202)"), std::string::npos);
}
