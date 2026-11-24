#include "run_command.h"

#include "scale/common/text.h"
#include "scale/host/fixture_run.h"

#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>

namespace scale::cli {
namespace {

constexpr std::string_view kUsage =
    "usage: scale run <trusted-fixture.exe> [--report <path>] [--timeout-ms <n>]\n";

int usage_error(std::string_view message) {
    err(std::format("error: {}\n{}", message, kUsage));
    return kExitUsage;
}

}  // namespace

int cmd_run(Args args) {
    std::optional<std::string> fixture;
    std::optional<std::string> report_path;
    std::uint64_t timeout_ms = 10'000;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--") {
            if (i + 1 < args.size()) {
                return usage_error("the fixture entry contract (int fixture_entry(void)) takes no arguments");
            }
            break;
        }
        if (a == "--report" || a == "--timeout-ms") {
            if (i + 1 >= args.size()) {
                return usage_error(std::format("{} needs a value", a));
            }
            const std::string& v = args[++i];
            if (a == "--report") {
                report_path = v;
            } else {
                const auto r = std::from_chars(v.data(), v.data() + v.size(), timeout_ms);
                if (r.ec != std::errc() || r.ptr != v.data() + v.size() || timeout_ms == 0 ||
                    timeout_ms > 3'600'000) {
                    return usage_error("--timeout-ms must be an integer between 1 and 3600000");
                }
            }
        } else if (a.starts_with("-")) {
            return usage_error(std::format("unknown option '{}'", a));
        } else if (fixture) {
            return usage_error("'run' takes exactly one fixture");
        } else {
            fixture = a;
        }
    }
    if (!fixture) {
        err(std::string(kUsage));
        return kExitUsage;
    }

    const auto fixture_wide = to_wide(*fixture);
    if (!fixture_wide) {
        return usage_error(fixture_wide.error().to_string());
    }
    const auto worker = host::default_worker_path();
    if (!worker) {
        err(std::format("error: {}\n", worker.error().to_string()));
        return kExitFailure;
    }

    host::FixtureRunOptions options;
    options.worker = *worker;
    options.fixture = std::filesystem::path(*fixture_wide);
    options.subject_name = *fixture;
    options.limits.timeout = std::chrono::milliseconds(timeout_ms);
    const auto run = host::run_fixture(options);
    if (!run) {
        err(std::format("error: {}\n", run.error().to_string()));
        return kExitFailure;
    }
    const diagnostics::RunReport& r = run->report;

    std::string text;
    text += std::format("run:      {}\n", r.run_id);
    text += std::format("fixture:  {}\n", r.subject.name);
    text += std::format("sha256:   {}\n", r.subject.sha256.value_or("(unavailable)"));
    text += std::format("stage:    {}\n", r.final_stage);
    text += std::format("outcome:  {}\n", diagnostics::to_string(r.outcome));
    if (run->returned_value) {
        text += std::format("returned: {}\n", *run->returned_value);
    }
    text += std::format("elapsed:  {} ms\n", r.elapsed_ms.value_or(0));
    text += "diagnostics:\n" + r.diagnostics.summary();

    int exit_code = r.outcome == diagnostics::Outcome::pass ? kExitSuccess : kExitFailure;
    if (report_path) {
        const auto path_wide = to_wide(*report_path);
        std::ofstream file;
        if (path_wide) {
            file.open(std::filesystem::path(*path_wide), std::ios::binary | std::ios::trunc);
        }
        const std::string json = diagnostics::to_json(r);
        if (!file || !file.write(json.data(), static_cast<std::streamsize>(json.size()))) {
            text += std::format("error: cannot write report to '{}'\n", *report_path);
            exit_code = kExitFailure;
        } else {
            text += std::format("report:   {}\n", *report_path);
        }
    }
    out(text);
    return exit_code;
}

}  // namespace scale::cli
