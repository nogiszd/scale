#pragma once

// UTF-8 <-> UTF-16 conversion for the Win32 boundary. Internally Scale uses
// UTF-8 std::string; wide strings appear only when calling Win32 APIs.

#include "scale/common/error.h"

#include <string>
#include <string_view>

namespace scale {

// Fails with common/invalid_utf16 on unpaired surrogates.
[[nodiscard]] Result<std::string> to_utf8(std::wstring_view wide);

// Fails with common/invalid_utf8 on malformed input.
[[nodiscard]] Result<std::wstring> to_wide(std::string_view utf8);

}  // namespace scale
