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
