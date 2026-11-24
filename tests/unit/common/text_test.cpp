#include "scale/common/error.h"
#include "scale/common/text.h"

#include <gtest/gtest.h>

#include <string>

TEST(Text, Utf8RoundTrip) {
    const std::wstring wide = L"title\x00E9\x4E2D\xD83D\xDE00";  // e-acute, CJK, emoji (surrogate pair)
    const auto utf8 = scale::to_utf8(wide);
    ASSERT_TRUE(utf8.has_value());
    EXPECT_EQ(*utf8, std::string("title\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80"));
    const auto back = scale::to_wide(*utf8);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(*back == wide);
}

TEST(Text, Utf8RejectsMalformedInput) {
    const auto r = scale::to_wide(std::string("\xC3\x28", 2));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().component, "common");
    EXPECT_EQ(r.error().code, "invalid_utf8");
}

TEST(Text, Utf16RejectsUnpairedSurrogate) {
    const wchar_t lone[] = {L'a', static_cast<wchar_t>(0xD800), L'b'};
    const auto r = scale::to_utf8(std::wstring_view(lone, 3));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, "invalid_utf16");
}

TEST(Error, ToStringFormat) {
    const scale::Error e{"loader", "bad_magic", "expected MZ"};
    EXPECT_EQ(e.to_string(), "loader/bad_magic: expected MZ");
    const scale::Error bare{"loader", "bad_magic", ""};
    EXPECT_EQ(bare.to_string(), "loader/bad_magic");
}

// ---- Portable implementation strictness (T-801, RFC 3629 / Unicode 3.9) ----------

TEST(Text, Utf8RejectsEveryIllFormedClass) {
    const std::string bad[] = {
        std::string("\xC0\x80", 2),          // overlong NUL
        std::string("\xE0\x80\xAF", 3),      // overlong '/'
        std::string("\xED\xA0\x80", 3),      // encoded surrogate U+D800
        std::string("\xF4\x90\x80\x80", 4),  // > U+10FFFF
        std::string("\xE4\xB8", 2),          // truncated
        std::string("\x80", 1),              // stray continuation byte
        std::string("\xFF", 1),              // invalid lead byte
        std::string("a\xC3", 2),             // truncated at end
    };
    for (const std::string& s : bad) {
        const auto r = scale::to_wide(s);
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().code, "invalid_utf8");
    }
}

TEST(Text, Utf16RejectsLoneSurrogates) {
    if constexpr (sizeof(wchar_t) == 2) {
        const wchar_t low_first[] = {static_cast<wchar_t>(0xDC00), L'a'};
        const wchar_t high_at_end[] = {L'a', static_cast<wchar_t>(0xD83D)};
        const wchar_t high_then_char[] = {static_cast<wchar_t>(0xD83D), L'b'};
        for (std::wstring_view s : {std::wstring_view(low_first, 2), std::wstring_view(high_at_end, 2),
                                    std::wstring_view(high_then_char, 2)}) {
            const auto r = scale::to_utf8(s);
            ASSERT_FALSE(r.has_value());
            EXPECT_EQ(r.error().code, "invalid_utf16");
        }
    } else {
        GTEST_SKIP() << "wchar_t is UTF-32 on this platform";
    }
}

TEST(Text, BoundaryCodePointsRoundTrip) {
    // Last code point of each UTF-8 length class, and the first after the surrogate gap.
    const std::string boundaries =
        "\x7F" "\xC2\x80" "\xDF\xBF" "\xE0\xA0\x80" "\xED\x9F\xBF" "\xEE\x80\x80" "\xEF\xBF\xBF"
        "\xF0\x90\x80\x80" "\xF4\x8F\xBF\xBF";
    const auto wide = scale::to_wide(boundaries);
    ASSERT_TRUE(wide.has_value()) << wide.error().to_string();
    const auto back = scale::to_utf8(*wide);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, boundaries);
    if constexpr (sizeof(wchar_t) == 2) {
        // U+10FFFF is the surrogate pair DBFF DFFF.
        EXPECT_EQ(static_cast<unsigned>(wide->back()), 0xDFFFu);
    }
}