#include "scale/diagnostics/json_writer.h"

#include "scale_test.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using scale::diagnostics::json_quote;
using scale::diagnostics::JsonWriter;

SCALE_TEST(json_quote_escapes_specials) {
    SCALE_CHECK_EQ(json_quote("a\"b\\c"), std::string(R"("a\"b\\c")"));
    SCALE_CHECK_EQ(json_quote("\n\t\r\b\f"), std::string(R"("\n\t\r\b\f")"));
    SCALE_CHECK_EQ(json_quote(std::string("\x01\x1f\x7f", 3)), std::string(R"("\u0001\u001f\u007f")"));
    SCALE_CHECK_EQ(json_quote(std::string("nul\0x", 5)), std::string(R"("nul\u0000x")"));
}

SCALE_TEST(json_quote_preserves_valid_utf8) {
    const std::string text = "\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80";
    SCALE_CHECK_EQ(json_quote(text), "\"" + text + "\"");
}

SCALE_TEST(json_quote_replaces_invalid_utf8) {
    const std::string fffd = "\xEF\xBF\xBD";
    SCALE_CHECK_EQ(json_quote("\xFF"), "\"" + fffd + "\"");
    SCALE_CHECK_EQ(json_quote("\xC0\xAF"), "\"" + fffd + fffd + "\"");      // overlong
    SCALE_CHECK_EQ(json_quote("\xED\xA0\x80"), "\"" + fffd + fffd + fffd + "\"");  // surrogate
    SCALE_CHECK_EQ(json_quote("a\xE4\xB8"), "\"a" + fffd + fffd + "\"");    // truncated
    SCALE_CHECK_EQ(json_quote("\xF4\x90\x80\x80"), "\"" + fffd + fffd + fffd + fffd + "\"");  // > U+10FFFF
}

SCALE_TEST(json_writer_nested_document) {
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
    SCALE_CHECK(w.complete());
    SCALE_CHECK_EQ(w.str(), expected);
}

SCALE_TEST(json_writer_rejects_misuse) {
    {
        JsonWriter w;
        w.begin_object();
        bool threw = false;
        try {
            w.value(1);  // value without key
        } catch (const std::logic_error&) {
            threw = true;
        }
        SCALE_CHECK(threw);
    }
    {
        JsonWriter w;
        w.begin_array();
        SCALE_CHECK(!w.complete());
        bool threw = false;
        try {
            (void)w.str();
        } catch (const std::logic_error&) {
            threw = true;
        }
        SCALE_CHECK(threw);
    }
    {
        JsonWriter w;
        w.value(1);
        bool threw = false;
        try {
            w.value(2);  // second root
        } catch (const std::logic_error&) {
            threw = true;
        }
        SCALE_CHECK(threw);
    }
}
