#include "commands.h"

#include "inspect_command.h"
#include "run_command.h"

#include "scale/common/version.h"

#include <array>
#include <cstdio>
#include <format>

namespace scale::cli {

void out(std::string_view text) {
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
}

void err(std::string_view text) {
    std::fwrite(text.data(), 1, text.size(), stderr);
    std::fflush(stderr);
}

namespace {

int cmd_version(Args args) {
    if (!args.empty()) {
        err("error: 'version' takes no arguments\n");
        return kExitUsage;
    }
    out(std::format("scale {}\n", kVersionString));
    return kExitSuccess;
}

int cmd_help(Args args);

// One line per command. Owners of a not-yet-implemented command replace the
// nullptr handler with their implementation (declared in their own source
// file) and clear the task id. Keep entries sorted as listed in README.md.
constexpr std::array kCommands = {
    Command{"help", "help", "Show available commands", "", &cmd_help},
    Command{"version", "version", "Print the Scale version", "", &cmd_version},
    Command{"inspect", "inspect <file> [--json]", "Inspect a PE32+ image (read-only)", "", &cmd_inspect},
    Command{"run", "run <executable> [--report <path>]", "Execute a trusted test executable in a worker process",
            "", &cmd_run},
};

int cmd_help(Args) {
    std::string text = std::format("scale {} - Scarlett Compatibility and Layered Execution\n\n"
                                   "usage: scale <command> [arguments]\n\ncommands:\n",
                                   kVersionString);
    for (const Command& c : kCommands) {
        text += std::format("  {:<40} {}{}\n", c.synopsis, c.summary,
                            c.handler ? "" : " (not implemented)");
    }
    out(text);
    return kExitSuccess;
}

}  // namespace

std::span<const Command> commands() {
    return kCommands;
}

}  // namespace scale::cli
