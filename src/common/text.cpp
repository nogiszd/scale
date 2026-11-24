#include "scale/common/text.h"

// Portable, strict UTF-8 <-> wide conversion (decision 0004). std::wstring is
// UTF-16 where wchar_t is 16 bits (Windows) and UTF-32 otherwise.

#include <format>
#include <optional>

namespace scale {
namespace {

constexpr char32_t kMaxCodePoint = 0x10FFFF;

bool is_surrogate(char32_t c) {
    return c >= 0xD800 && c <= 0xDFFF;
}

// Decodes one UTF-8 sequence at text[i]. Returns the code point and advances i,
// or nullopt for any ill-formed sequence (RFC 3629: overlong, surrogate, > U+10FFFF, truncated).
std::optional<char32_t> decode_utf8(std::string_view text, std::size_t& i) {
    const auto byte = [&](std::size_t k) { return static_cast<unsigned char>(text[k]); };
    const unsigned char b0 = byte(i);
    std::size_t length = 0;
    char32_t cp = 0;
    char32_t min = 0;
    if (b0 < 0x80) {
        ++i;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0) {
        length = 2;
        cp = b0 & 0x1F;
        min = 0x80;
    } else if ((b0 & 0xF0) == 0xE0) {
        length = 3;
        cp = b0 & 0x0F;
        min = 0x800;
    } else if ((b0 & 0xF8) == 0xF0) {
        length = 4;
        cp = b0 & 0x07;
        min = 0x10000;
    } else {
        return std::nullopt;
    }
    if (text.size() - i < length) return std::nullopt;
    for (std::size_t k = 1; k < length; ++k) {
        const unsigned char b = byte(i + k);
        if ((b & 0xC0) != 0x80) return std::nullopt;
        cp = (cp << 6) | (b & 0x3F);
    }
    if (cp < min || cp > kMaxCodePoint || is_surrogate(cp)) return std::nullopt;
    i += length;
    return cp;
}

void append_utf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

}  // namespace

Result<std::string> to_utf8(std::wstring_view wide) {
    std::string out;
    out.reserve(wide.size());
    for (std::size_t i = 0; i < wide.size(); ++i) {
        char32_t cp = static_cast<char32_t>(wide[i]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (cp >= 0xD800 && cp <= 0xDBFF) {  // high surrogate: needs a following low surrogate
                if (i + 1 >= wide.size()) {
                    return fail("common", "invalid_utf16", std::format("unpaired high surrogate at index {}", i));
                }
                const char32_t low = static_cast<char32_t>(wide[i + 1]);
                if (low < 0xDC00 || low > 0xDFFF) {
                    return fail("common", "invalid_utf16", std::format("unpaired high surrogate at index {}", i));
                }
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                return fail("common", "invalid_utf16", std::format("unpaired low surrogate at index {}", i));
            }
        } else if (cp > kMaxCodePoint || is_surrogate(cp)) {
            return fail("common", "invalid_utf16", std::format("invalid code point at index {}", i));
        }
        append_utf8(out, cp);
    }
    return out;
}

Result<std::wstring> to_wide(std::string_view utf8) {
    std::wstring out;
    out.reserve(utf8.size());
    for (std::size_t i = 0; i < utf8.size();) {
        const std::size_t at = i;
        const auto cp = decode_utf8(utf8, i);
        if (!cp) {
            return fail("common", "invalid_utf8", std::format("ill-formed UTF-8 at byte {}", at));
        }
        if constexpr (sizeof(wchar_t) == 2) {
            if (*cp >= 0x10000) {
                const char32_t v = *cp - 0x10000;
                out += static_cast<wchar_t>(0xD800 + (v >> 10));
                out += static_cast<wchar_t>(0xDC00 + (v & 0x3FF));
                continue;
            }
        }
        out += static_cast<wchar_t>(*cp);
    }
    return out;
}

}  // namespace scale
