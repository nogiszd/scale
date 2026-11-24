#include "scale/host/supervisor.h"

#include <gtest/gtest.h>

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

using scale::host::build_command_line;
using scale::host::parse_exception_report;

namespace {

// Parses with the documented Windows rules and compares with the input.
std::vector<std::wstring> round_trip(const std::vector<std::wstring>& argv) {
    const std::wstring line = build_command_line(argv);
    int count = 0;
    LPWSTR* parsed = ::CommandLineToArgvW(line.c_str(), &count);
    std::vector<std::wstring> out;
    if (parsed != nullptr) {
        for (int i = 0; i < count; ++i) out.emplace_back(parsed[i]);
        ::LocalFree(parsed);
    }
    return out;
}

}  // namespace

TEST(CommandLine, RoundTripsAwkwardArguments) {
    const std::vector<std::wstring> argv = {
        L"C:\\Program Files\\scale\\scale_worker.exe",
        L"plain",
        L"with space",
        L"",
        L"quote\"inside",
        L"trailing\\",
        L"trailing space\\",
        L"back\\\\\"slash",
        L"\\\\server\\share\\",
        L"tab\there",
        L"\x00E9\x4E2D",
    };
    EXPECT_EQ(round_trip(argv), argv);
}

TEST(CommandLine, PlainArgumentsAreNotQuoted) {
    EXPECT_EQ(build_command_line({L"a.exe", L"b", L"c"}), L"a.exe b c");
    EXPECT_EQ(build_command_line({L"a b"}), L"\"a b\"");
}

TEST(ExceptionReport, ParsesWorkerLine) {
    const auto r = parse_exception_report("exception code=0xc0000005 address=0x00007ff6a1b21234");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->code, 0xC0000005u);
    EXPECT_EQ(r->address, 0x00007FF6A1B21234ull);
}

TEST(ExceptionReport, RejectsMalformedLines) {
    EXPECT_FALSE(parse_exception_report("exception code=0xc0000005").has_value());
    EXPECT_FALSE(parse_exception_report("exception code=0x address=0x1").has_value());
    EXPECT_FALSE(parse_exception_report("exception code=0xzz address=0x1").has_value());
    EXPECT_FALSE(parse_exception_report("exception code=0x1 address=0x1 trailing").has_value());
    EXPECT_FALSE(parse_exception_report("exception code=0x1ffffffff address=0x1").has_value());
    EXPECT_FALSE(parse_exception_report("note something").has_value());
}
