#include "cli.h"

#include <cstdio>
#include <exception>
#include <vector>

int main(int argc, char** argv) {
    try {
        const std::vector<std::string_view> args(argv + 1, argv + argc);
        return scale::shader::cli::run(args);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "internal error: %s\n", error.what());
        return scale::shader::cli::kExitFailure;
    }
}
