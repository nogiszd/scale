#pragma once

#include "scale/common/error.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scale::shader {

enum class ShaderTestAbiVersion : std::uint8_t {
    v0 = 0,
    v1 = 1,
};

enum class ShaderTestOutputType : std::uint8_t {
    u32,
    f32,
};

using ShaderTestLaneWords = std::array<std::uint32_t, 32>;

struct ShaderTestCaseV1 {
    std::uint32_t initial_exec{};
    ShaderTestLaneWords initial_output_words{};
};

struct ShaderTestCase {
    std::string name;
    std::vector<std::uint32_t> code_words;  // Dwords in instruction stream order.
    ShaderTestLaneWords input_words{};
    ShaderTestLaneWords expected_output_words{};
    ShaderTestOutputType output_type{};
    std::optional<std::uint32_t> seed;  // JSON null for manual inputs.
    std::optional<ShaderTestCaseV1> v1;  // Both v1 fields are required together.

    [[nodiscard]] constexpr ShaderTestAbiVersion abi_version() const noexcept {
        return v1 ? ShaderTestAbiVersion::v1 : ShaderTestAbiVersion::v0;
    }
};

// Parse one UTF-8 JSON test case under docs/contracts/shader_abi.md. The result
// owns its data. Schema, ISA, stage and dispatch fields must match the contract.
// The test case carries the resolved ABI version and dwords for the caller.
// Errors use component "shader" and reporting stage "testcase". Codes are:
//   testcase_invalid_utf8, testcase_invalid_json
//   testcase_duplicate_field, testcase_missing_field, testcase_unknown_field
//   testcase_invalid_value, testcase_unsupported_version, testcase_unsupported_dispatch
// Unknown fields include version-specific keys outside their ABI version.
[[nodiscard]] Result<ShaderTestCase> parse_test_case(std::string_view json);

// Load and parse a file with a caller-supplied byte limit. File failures use
// testcase_open_failed, testcase_read_failed or testcase_too_large.
[[nodiscard]] Result<ShaderTestCase> load_test_case(const std::filesystem::path& path,
                                                   std::uint64_t max_bytes);

}  // namespace scale::shader
