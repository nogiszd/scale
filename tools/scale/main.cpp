#include "commands.h"

#include "scale/common/text.h"

#include <exception>
#include <format>
#include <string>
#include <vector>

namespace {

int dispatch(const std::vector<std::string>& argv) {
    using namespace scale::cli;

    if (argv.size() < 2) {
        err("usage: scale <command> [arguments]\nRun 'scale help' for a list of commands.\n");
        return kExitUsage;
    }
    std::string_view name = argv[1];
    if (name == "--help" || name == "-h") {
        name = "help";
    } else if (name == "--version") {
        name = "version";
    }

    for (const Command& c : commands()) {
        if (c.name != name) {
            continue;
        }
        if (c.handler == nullptr) {
            err(std::format("error: command '{}' is not implemented yet (task {})\n", c.name, c.task));
            return kExitNotImplemented;
        }
        const Args args(argv.data() + 2, argv.size() - 2);
        return c.handler(args);
    }
    err(std::format("error: unknown command '{}'\nRun 'scale help' for a list of commands.\n", name));
    return kExitUsage;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc));
        for (int i = 0; i < argc; ++i) {
            auto converted = scale::to_utf8(argv[i]);
            if (!converted) {
                scale::cli::err(std::format("error: argument {} is not valid UTF-16: {}\n", i,
                                            converted.error().to_string()));
                return scale::cli::kExitUsage;
            }
            args.push_back(std::move(*converted));
        }
        return dispatch(args);
    } catch (const std::exception& e) {
        scale::cli::err(std::format("internal error: {}\n", e.what()));
        return scale::cli::kExitFailure;
    }
}
