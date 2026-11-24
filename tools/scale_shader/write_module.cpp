#include "write_module.h"

#include <chrono>
#include <fstream>
#include <string>
#include <utility>

namespace scale::shader::cli {
namespace {

struct TemporaryOutput {
    std::filesystem::path directory;
    std::filesystem::path file;
    bool created{};

    ~TemporaryOutput() {
        if (!created) return;
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
        std::filesystem::remove(directory, ignored);
    }
};

}  // namespace

Result<void> write_module(const std::filesystem::path& input, const std::filesystem::path& output,
                          std::span<const std::uint32_t> words) {
    std::error_code error;
    const auto destination = std::filesystem::absolute(output, error);
    if (error) return fail("shader", "spirv_write_failed", "could not resolve output path");
    if (std::filesystem::equivalent(input, destination, error)) {
        return fail("shader", "spirv_output_is_input", "output refers to the input test case");
    }

    const auto bytes = std::as_bytes(words);
    if (!std::in_range<std::streamsize>(bytes.size())) {
        return fail("shader", "spirv_write_failed", "module is too large to write");
    }

    // Claim a sibling staging directory so rename can publish the module on
    // the same filesystem. Concurrent writers never share a temporary file.
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        const auto directory = destination.parent_path() /
            (".scale-shader-" + std::to_string(stamp) + "-" + std::to_string(attempt));
        TemporaryOutput temp{directory, directory / "module.spv"};
        temp.created = std::filesystem::create_directory(temp.directory, error);
        if (!temp.created) {
            if (error && error != std::errc::file_exists) {
                return fail("shader", "spirv_write_failed", "could not create output staging directory");
            }
            continue;
        }
        std::ofstream stream(temp.file, std::ios::binary);
        if (!stream.is_open()) return fail("shader", "spirv_write_failed", "could not open staged module");
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        stream.close();
        if (!stream) return fail("shader", "spirv_write_failed", "could not write the complete module");

        std::filesystem::rename(temp.file, destination, error);
        if (error) return fail("shader", "spirv_write_failed", "could not publish the module");
        return {};
    }
    return fail("shader", "spirv_write_failed", "could not reserve an output staging directory");
}

}  // namespace scale::shader::cli
