#include "scale/diagnostics/event_line.h"

#include <gtest/gtest.h>

#include <string>

using namespace scale::diagnostics;

TEST(EventLine, RoundTripsAllFields) {
    const DiagnosticEvent e{"runtime",
                            "import_resolution",
                            "unresolved_import",
                            Severity::error,
                            true,
                            "no provider = 100% missing\nsecond line",
                            {{"module", "test_platform.dll"},
                             {"symbol", "TestAllocate"},
                             {"importing_module", "my game.exe"},
                             {"note", "a=b %41 \xC3\xA9"}}};
    const std::string line = to_event_line(e);
    EXPECT_EQ(line.find('\n'), std::string::npos);
    EXPECT_TRUE(line.starts_with("event component=runtime stage=import_resolution code=unresolved_import "
                                 "severity=error fatal=1 a.module=test_platform.dll a.symbol=TestAllocate "
                                 "a.importing_module=my%20game.exe "))
        << line;
    const auto parsed = parse_event_line(line);
    ASSERT_TRUE(parsed.has_value()) << line;
    EXPECT_EQ(*parsed, e);
}

TEST(EventLine, NoAttributesAndEmptyMessage) {
    const DiagnosticEvent e{"host", "worker", "report_truncated", Severity::warning, false, "", {}};
    const std::string line = to_event_line(e);
    EXPECT_EQ(line, "event component=host stage=worker code=report_truncated severity=warning fatal=0 message=");
    EXPECT_EQ(parse_event_line(line), e);
}

TEST(EventLine, RejectsMalformedLines) {
    const std::string good = "event component=c stage=s code=k severity=info fatal=0 message=m";
    ASSERT_TRUE(parse_event_line(good).has_value());
    EXPECT_FALSE(parse_event_line("stage entry").has_value());
    EXPECT_FALSE(parse_event_line("event component=c stage=s code=k severity=info fatal=0").has_value());
    EXPECT_FALSE(parse_event_line("event stage=s component=c code=k severity=info fatal=0 message=m").has_value());
    EXPECT_FALSE(parse_event_line("event component=c stage=s code=k severity=fatal fatal=0 message=m").has_value());
    EXPECT_FALSE(parse_event_line("event component=c stage=s code=k severity=info fatal=2 message=m").has_value());
    EXPECT_FALSE(parse_event_line("event component= stage=s code=k severity=info fatal=0 message=m").has_value());
    EXPECT_FALSE(parse_event_line("event component=c stage=s code=k severity=info fatal=0 b.x=1 message=m").has_value());
    EXPECT_FALSE(parse_event_line("event component=c stage=s code=k severity=info fatal=0 a.=1 message=m").has_value());
    EXPECT_FALSE(parse_event_line("event component=c stage=s code=k severity=info fatal=0 message=bad%2").has_value());
    EXPECT_FALSE(parse_event_line("event component=c stage=s code=k severity=info fatal=0 message=bad%zz").has_value());
    EXPECT_FALSE(parse_event_line("event component=c stage=s code=k severity=info fatal=0 message=raw=equals").has_value());
}

TEST(PercentCoding, EncodesUnsafeBytes) {
    EXPECT_EQ(percent_encode("a b=c%d"), "a%20b%3Dc%25d");
    EXPECT_EQ(percent_encode("C:/x-y_z.1"), "C:/x-y_z.1");
    EXPECT_EQ(percent_decode("a%20b%3dc"), std::optional<std::string>("a b=c"));
    EXPECT_FALSE(percent_decode("%").has_value());
    EXPECT_FALSE(percent_decode("%4").has_value());
}
