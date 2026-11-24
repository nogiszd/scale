#include "scale/common/error.h"
#include "scale/common/text.h"

#include "scale_test.h"

#include <string>

SCALE_TEST(utf8_round_trip) {
    const std::wstring wide = L"title\x00E9\x4E2D\xD83D\xDE00";  // e-acute, CJK, emoji (surrogate pair)
    const auto utf8 = scale::to_utf8(wide);
    SCALE_REQUIRE(utf8.has_value());
    SCALE_CHECK_EQ(*utf8, std::string("title\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80"));
    const auto back = scale::to_wide(*utf8);
    SCALE_REQUIRE(back.has_value());
    SCALE_CHECK(*back == wide);
}

SCALE_TEST(utf8_rejects_malformed_input) {
    const auto r = scale::to_wide(std::string("\xC3\x28", 2));
    SCALE_REQUIRE(!r.has_value());
    SCALE_CHECK_EQ(r.error().component, std::string("common"));
    SCALE_CHECK_EQ(r.error().code, std::string("invalid_utf8"));
}

SCALE_TEST(utf16_rejects_unpaired_surrogate) {
    const wchar_t lone[] = {L'a', static_cast<wchar_t>(0xD800), L'b'};
    const auto r = scale::to_utf8(std::wstring_view(lone, 3));
    SCALE_REQUIRE(!r.has_value());
    SCALE_CHECK_EQ(r.error().code, std::string("invalid_utf16"));
}

SCALE_TEST(error_to_string_format) {
    const scale::Error e{"loader", "bad_magic", "expected MZ"};
    SCALE_CHECK_EQ(e.to_string(), std::string("loader/bad_magic: expected MZ"));
    const scale::Error bare{"loader", "bad_magic", ""};
    SCALE_CHECK_EQ(bare.to_string(), std::string("loader/bad_magic"));
}
