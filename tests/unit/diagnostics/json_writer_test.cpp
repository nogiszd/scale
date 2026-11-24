#include "scale/diagnostics/json_writer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using scale::diagnostics::json_quote;
using scale::diagnostics::JsonWriter;

TEST(JsonQuote, EscapesSpecials) {
    EXPECT_EQ(json_quote("a\"b\\c"), R"("a\"b\\c")");
    EXPECT_EQ(json_quote("\n\t\r\b\f"), R"("\n\t\r\b\f")");
    EXPECT_EQ(json_quote(std::string("\x01\x1f\x7f", 3)), R"("\u0001\u001f\u007f")");
    EXPECT_EQ(json_quote(std::string("nul\0x", 5)), R"("nul\u0000x")");
}

TEST(JsonQuote, PreservesValidUtf8) {
    const std::string text = "\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80";
    EXPECT_EQ(json_quote(text), "\"" + text + "\"");
}

TEST(JsonQuote, ReplacesInvalidUtf8) {
    const std::string fffd = "\xEF\xBF\xBD";
    EXPECT_EQ(json_quote("\xFF"), "\"" + fffd + "\"");
    EXPECT_EQ(json_quote("\xC0\xAF"), "\"" + fffd + fffd + "\"");                    // overlong
    EXPECT_EQ(json_quote("\xED\xA0\x80"), "\"" + fffd + fffd + fffd + "\"");         // surrogate
    EXPECT_EQ(json_quote("a\xE4\xB8"), "\"a" + fffd + fffd + "\"");                  // truncated
    EXPECT_EQ(json_quote("\xF4\x90\x80\x80"), "\"" + fffd + fffd + fffd + fffd + "\"");  // > U+10FFFF
}

TEST(JsonWriter, NestedDocument) {
    JsonWriter w;
    w.begin_object();
    w.field("name", "fixture");
    w.field("count", 3);
    w.field("big", std::numeric_limits<std::uint64_t>::max());
    w.field("neg", std::int64_t{-5});
    w.field("ok", true);
    w.key("none");
    w.null();
    w.key("list");
    w.begin_array();
    w.value(1);
    w.begin_object();
    w.end_object();
    w.begin_array();
    w.end_array();
    w.end_array();
    w.end_object();

    const std::string expected = "{\n"
                                 "  \"name\": \"fixture\",\n"
                                 "  \"count\": 3,\n"
                                 "  \"big\": 18446744073709551615,\n"
                                 "  \"neg\": -5,\n"
                                 "  \"ok\": true,\n"
                                 "  \"none\": null,\n"
                                 "  \"list\": [\n"
                                 "    1,\n"
                                 "    {},\n"
                                 "    []\n"
                                 "  ]\n"
                                 "}";
    EXPECT_TRUE(w.complete());
    EXPECT_EQ(w.str(), expected);
}

TEST(JsonWriter, RejectsValueWithoutKey) {
    JsonWriter w;
    w.begin_object();
    EXPECT_THROW(w.value(1), std::logic_error);
}

TEST(JsonWriter, RejectsIncompleteDocument) {
    JsonWriter w;
    w.begin_array();
    EXPECT_FALSE(w.complete());
    EXPECT_THROW((void)w.str(), std::logic_error);
}

TEST(JsonWriter, RejectsSecondRoot) {
    JsonWriter w;
    w.value(1);
    EXPECT_THROW(w.value(2), std::logic_error);
}
