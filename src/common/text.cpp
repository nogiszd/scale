#include "scale/common/text.h"

#include <windows.h>

#include <climits>
#include <format>

namespace scale {

Result<std::string> to_utf8(std::wstring_view wide) {
    if (wide.empty()) {
        return std::string();
    }
    if (wide.size() > static_cast<std::size_t>(INT_MAX)) {
        return fail("common", "input_too_large", "UTF-16 input exceeds INT_MAX code units");
    }
    const int in_len = static_cast<int>(wide.size());
    const int out_len = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), in_len,
                                              nullptr, 0, nullptr, nullptr);
    if (out_len <= 0) {
        return fail("common", "invalid_utf16",
                    std::format("WideCharToMultiByte failed (error {})", ::GetLastError()));
    }
    std::string out(static_cast<std::size_t>(out_len), '\0');
    const int written = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), in_len,
                                              out.data(), out_len, nullptr, nullptr);
    if (written != out_len) {
        return fail("common", "invalid_utf16", "WideCharToMultiByte produced inconsistent length");
    }
    return out;
}

Result<std::wstring> to_wide(std::string_view utf8) {
    if (utf8.empty()) {
        return std::wstring();
    }
    if (utf8.size() > static_cast<std::size_t>(INT_MAX)) {
        return fail("common", "input_too_large", "UTF-8 input exceeds INT_MAX bytes");
    }
    const int in_len = static_cast<int>(utf8.size());
    const int out_len =
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), in_len, nullptr, 0);
    if (out_len <= 0) {
        return fail("common", "invalid_utf8",
                    std::format("MultiByteToWideChar failed (error {})", ::GetLastError()));
    }
    std::wstring out(static_cast<std::size_t>(out_len), L'\0');
    const int written =
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), in_len, out.data(), out_len);
    if (written != out_len) {
        return fail("common", "invalid_utf8", "MultiByteToWideChar produced inconsistent length");
    }
    return out;
}

}  // namespace scale
