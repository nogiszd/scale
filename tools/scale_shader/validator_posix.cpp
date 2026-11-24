#include "validator.h"

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <format>
#include <string>

extern char** environ;

namespace scale::shader::cli {
namespace {

struct FileDescriptor {
    int value;
    ~FileDescriptor() { if (value != -1) close(value); }
};

Result<std::optional<std::uint32_t>> launch(std::string tool, int module) {
    char target_option[] = "--target-env";
    char target[] = "vulkan1.2";
    char stdin_path[] = "-";
    std::array<char*, 5> args{tool.data(), target_option, target, stdin_path, nullptr};
    posix_spawn_file_actions_t actions{};
    const auto init = posix_spawn_file_actions_init(&actions);
    if (init != 0) {
        return fail("shader", "validator_start_failed", std::format("could not prepare validator stdin ({})", init));
    }
    pid_t process{};
    const auto redirected = posix_spawn_file_actions_adddup2(&actions, module, STDIN_FILENO);
    const auto started = redirected == 0 ?
        posix_spawnp(&process, tool.c_str(), &actions, nullptr, args.data(), environ) : redirected;
    posix_spawn_file_actions_destroy(&actions);
    if (started == ENOENT || started == ENOTDIR) return std::nullopt;
    if (started != 0) {
        return fail("shader", "validator_start_failed", std::format("posix_spawnp failed ({})", started));
    }

    int status{};
    while (waitpid(process, &status, 0) == -1) {
        if (errno != EINTR) {
            return fail("shader", "validator_wait_failed", std::format("waitpid failed ({})", errno));
        }
    }
    if (!WIFEXITED(status)) {
        return fail("shader", "validator_wait_failed", "spirv-val did not exit normally");
    }
    return static_cast<std::uint32_t>(WEXITSTATUS(status));
}

}  // namespace

Result<std::optional<std::uint32_t>> run_validator(const std::filesystem::path& input) {
    const auto* path = std::getenv("PATH");
    const auto* sdk = std::getenv("VULKAN_SDK");
    const bool search_path = path != nullptr && *path != '\0';
    const bool search_sdk = sdk != nullptr && *sdk != '\0';
    if (!search_path && !search_sdk) return std::nullopt;

    const FileDescriptor module{open(input.c_str(), O_RDONLY | O_CLOEXEC)};
    if (module.value == -1) {
        return fail("shader", "spirv_read_failed", std::format("could not open module ({})", errno));
    }
    if (search_path) {
        auto result = launch("spirv-val", module.value);
        if (!result || *result) return result;
    }
    if (search_sdk) {
        return launch((std::filesystem::path(sdk) / "bin" / "spirv-val").native(), module.value);
    }
    return std::nullopt;
}

}  // namespace scale::shader::cli
