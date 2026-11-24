#pragma once

// Strict UTF-8 <-> wide string conversion, portable (decision 0004). Internally
// SCALE uses UTF-8 std::string. std::wstring is UTF-16 where wchar_t is 16 bits
// (Windows) and UTF-32 otherwise; wide strings appear only at OS APIs.

#include "scale/common/error.h"

#include <string>
#include <string_view>

namespace scale {

// Fails with common/invalid_utf16 on unpaired surrogates.
[[nodiscard]] Result<std::string> to_utf8(std::wstring_view wide);

// Fails with common/invalid_utf8 on malformed input.
[[nodiscard]] Result<std::wstring> to_wide(std::string_view utf8);

}  // namespace scale
