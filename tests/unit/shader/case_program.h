#pragma once

#include "scale/shader/test_case.h"

namespace scale::shader::test {

inline std::filesystem::path utf8_path(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

inline Result<ShaderTestCase> read_shader_case(std::string_view name) {
    return load_test_case(utf8_path(SCALE_SHADER_CASE_DIR) / (std::string(name) + ".json"), 1024 * 1024);
}

}  // namespace scale::shader::test
