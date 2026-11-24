#include "cli.h"

#include "scale/common/text.h"

#include <cstdio>
#include <exception>
#include <string>
#include <vector>

int wmain(int argc, wchar_t** argv) {
    try {
        std::vector<std::string> converted;
        converted.reserve(static_cast<std::size_t>(argc));
        for (int i = 1; i < argc; ++i) {
            auto argument = scale::to_utf8(argv[i]);
            if (!argument) {
                std::fprintf(stderr, "error: argument %d: %s\n", i, argument.error().to_string().c_str());
                return scale::shader::cli::kExitUsage;
            }
            converted.push_back(std::move(*argument));
        }
        const std::vector<std::string_view> args(converted.begin(), converted.end());
        return scale::shader::cli::run(args);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "internal error: %s\n", error.what());
        return scale::shader::cli::kExitFailure;
    }
}
