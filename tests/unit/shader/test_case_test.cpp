#include "scale/shader/test_case.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <format>
#include <fstream>
#include <utility>

namespace {

using namespace scale::shader;
using Fields = std::vector<std::pair<std::string_view, std::string>>;

std::string dwords(const ShaderTestLaneWords& words) {
    std::string json = "[";
    for (std::size_t lane = 0; lane < words.size(); ++lane) {
        if (lane != 0) json += ',';
        json += std::format("\"0x{:08x}\"", words[lane]);
    }
    return json + ']';
}

ShaderTestLaneWords input_words() {
    ShaderTestLaneWords words{};
    for (std::uint32_t lane = 0; lane < words.size(); ++lane) words[lane] = lane;
    return words;
}

Fields v0_fields() {
    // The T-704 example, including the wraparound at lane 1.
    auto input = input_words();
    input[1] = 0xffffffffu;
    ShaderTestLaneWords output{};
    for (std::uint32_t lane = 0; lane < output.size(); ++lane) output[lane] = (lane == 1 ? 0u : lane * 2);
    return {
        {"schema_version", "1"}, {"abi_version", "0"}, {"name", "\"add_lane_to_input\""},
        {"isa_target", "\"gfx1030\""}, {"stage", "\"compute\""}, {"wave_size", "32"},
        {"workgroup_size", "[32,1,1]"}, {"workgroup_count", "[1,1,1]"},
        {"code_words", R"(["0xe0301000","0x80000201","0xbf8c3f70","0x4a040500","0xe0701000","0x80010201","0xbf810000"])"},
        {"input_words", dwords(input)}, {"expected_output_words", dwords(output)},
        {"output_type", "\"u32\""}, {"seed", "null"},
    };
}

std::string& field(Fields& fields, std::string_view name) {
    return std::ranges::find(fields, name, &Fields::value_type::first)->second;
}

Fields v1_fields() {
    auto fields = v0_fields();
    auto initial = input_words();
    initial.fill(0xdeadbeefu);
    auto expected = initial;
    for (std::uint32_t lane = 4; lane <= 7; ++lane) expected[lane] = lane + 1;
    field(fields, "name") = "\"masked_add_lane\"";
    field(fields, "abi_version") = "1";
    field(fields, "code_words") = R"(["0x7d820083","0xbe883c6a","0xbf880004","0xe0301000","0x80000201",
        "0xbf8c3f70","0x4a040481","0xe0701000","0x80010201","0xbf810000"])";
    field(fields, "input_words") = dwords(input_words());
    field(fields, "expected_output_words") = dwords(expected);
    fields.emplace_back("initial_exec", "\"0x000000ff\"");
    fields.emplace_back("initial_output_words", dwords(initial));
    return fields;
}

std::string object(const Fields& fields) {
    std::string text = "{";
    for (const auto& [name, value] : fields) {
        if (text.size() != 1) text += ',';
        text += std::format("\n\"{}\": {}", name, value);
    }
    return text + "\n}\n";
}

void expect_error(const scale::Result<ShaderTestCase>& result, std::string_view code) {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().component, "shader");
    EXPECT_EQ(result.error().code, code) << result.error().to_string();
}

void reject_field(std::string_view name, std::string_view value, std::string_view code, Fields fields = v0_fields()) {
    SCOPED_TRACE(name);
    SCOPED_TRACE(value);
    field(fields, name) = value;
    expect_error(parse_test_case(object(fields)), code);
}

std::filesystem::path test_dir() {
    constexpr std::string_view path = SCALE_SHADER_TESTCASE_TEST_DIR;
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
}

TEST(ShaderTestCase, LoadsT704Example) {
    const auto path = test_dir() / "t705_add_lane.json";
    const auto json = object(v0_fields());
    std::ofstream output(path, std::ios::binary);
    output << json;
    output.close();
    ASSERT_TRUE(output.good());

    const auto test = load_test_case(path, json.size());
    ASSERT_TRUE(test.has_value()) << test.error().to_string();
    EXPECT_EQ(test->abi_version(), ShaderTestAbiVersion::v0);
    EXPECT_FALSE(test->v1);
    EXPECT_EQ(test->name, "add_lane_to_input");
    EXPECT_EQ(test->output_type, ShaderTestOutputType::u32);
    EXPECT_FALSE(test->seed);
    EXPECT_EQ(test->code_words, (std::vector<std::uint32_t>{
        0xe0301000, 0x80000201, 0xbf8c3f70, 0x4a040500, 0xe0701000, 0x80010201, 0xbf810000,
    }));
    for (std::uint32_t lane = 0; lane < test->input_words.size(); ++lane) {
        SCOPED_TRACE(lane);
        EXPECT_EQ(test->input_words[lane], (lane == 1 ? 0xffffffffu : lane));
        EXPECT_EQ(test->expected_output_words[lane], (lane == 1 ? 0u : lane * 2));
    }
}

TEST(ShaderTestCase, ParsesV1MaskedStoreCaseInAnyFieldOrder) {
    auto fields = v1_fields();
    field(fields, "seed") = "4294967295";
    std::ranges::reverse(fields);  // Includes the v1 fields before abi_version.
    const auto test = parse_test_case(object(fields));
    ASSERT_TRUE(test.has_value()) << test.error().to_string();
    EXPECT_EQ(test->abi_version(), ShaderTestAbiVersion::v1);
    ASSERT_TRUE(test->v1);
    EXPECT_EQ(test->v1->initial_exec, 0xffu);
    EXPECT_EQ(test->seed, 0xffffffffu);
    EXPECT_EQ(test->code_words.front(), 0x7d820083u);
    for (std::uint32_t lane = 0; lane < test->input_words.size(); ++lane) {
        SCOPED_TRACE(lane);
        EXPECT_EQ(test->input_words[lane], lane);
        EXPECT_EQ(test->v1->initial_output_words[lane], 0xdeadbeefu);
        EXPECT_EQ(test->expected_output_words[lane], (lane >= 4 && lane <= 7 ? lane + 1 : 0xdeadbeefu));
    }
}

TEST(ShaderTestCase, PreservesZeroExecAndFloatOutputType) {
    auto fields = v1_fields();
    field(fields, "output_type") = "\"f32\"";
    field(fields, "initial_exec") = "\"0x00000000\"";
    field(fields, "expected_output_words") = field(fields, "initial_output_words");
    const auto test = parse_test_case(object(fields));
    ASSERT_TRUE(test.has_value()) << test.error().to_string();
    ASSERT_TRUE(test->v1);
    EXPECT_EQ(test->v1->initial_exec, 0u);
    EXPECT_EQ(test->output_type, ShaderTestOutputType::f32);
    EXPECT_EQ(test->expected_output_words, test->v1->initial_output_words);
}

TEST(ShaderTestCase, DecodesEscapedKeysAndUtf8Names) {
    auto fields = v0_fields();
    std::ranges::find(fields, "name", &Fields::value_type::first)->first = R"(n\u0061me)";
    field(fields, R"(n\u0061me)") = R"("SCALE \"case\" \/ \\ \b\f\n\r\t café \u03bb \uD834\uDD1E")";
    const auto test = parse_test_case(object(fields));
    ASSERT_TRUE(test.has_value()) << test.error().to_string();
    EXPECT_EQ(test->name, "SCALE \"case\" / \\ \b\f\n\r\t café λ \xf0\x9d\x84\x9e");
}

TEST(ShaderTestCaseReject, InvalidUtf8) {
    for (const auto bytes : {"\x80", "\xc0\x80", "\xe0\x80\x80", "\xe2\x28\xa1", "\xed\xa0\x80",
                             "\xf4\x90\x80\x80", "\xf0\x9f\x92"}) {
        reject_field("name", std::format("\"{}\"", bytes), "testcase_invalid_utf8");
    }
}

TEST(ShaderTestCaseReject, InvalidJson) {
    for (const auto text : {"", "{", "{\"name\"}", "{\"name\":}", "{\"name\":\"x\",}",
                            "{\"name\":\"x\" \"seed\":null}", "{\"name\":\"x\"}{}"}) {
        SCOPED_TRACE(text);
        expect_error(parse_test_case(text), "testcase_invalid_json");
    }
    for (const auto value : {R"("\q")", R"("\uZZZZ")", R"("\uD800")", R"("\uDC00")", R"("\uD800\u0041")", "\"a\nb\""}) {
        reject_field("name", value, "testcase_invalid_json");
    }
    for (const auto value : {"01", "-", "1.", "1e+", "NaN"}) {
        reject_field("seed", value, "testcase_invalid_json");
    }
    reject_field("code_words", R"(["0xbf810000",])", "testcase_invalid_json");
    reject_field("code_words", R"(["0xbf810000" "0xbf810000"])", "testcase_invalid_json");
}

TEST(ShaderTestCaseReject, DuplicateAndUnknownFields) {
    auto fields = v0_fields();
    fields.emplace_back(R"(n\u0061me)", "\"duplicate\"");
    expect_error(parse_test_case(object(fields)), "testcase_duplicate_field");
    fields.back() = {"extra", "0"};
    expect_error(parse_test_case(object(fields)), "testcase_unknown_field");

    for (const auto name : {"initial_exec", "initial_output_words"}) {
        fields = v1_fields();
        field(fields, "abi_version") = "0";
        std::erase_if(fields, [=](const auto& entry) {
            return (entry.first == "initial_exec" || entry.first == "initial_output_words") && entry.first != name;
        });
        expect_error(parse_test_case(object(fields)), "testcase_unknown_field");
        std::ranges::reverse(fields);
        expect_error(parse_test_case(object(fields)), "testcase_unknown_field");
    }
}

TEST(ShaderTestCaseReject, EveryRequiredFieldInBothVersions) {
    for (const auto& original : {v0_fields(), v1_fields()}) {
        for (const auto& entry : original) {
            SCOPED_TRACE(entry.first);
            auto fields = original;
            std::erase_if(fields, [&](const auto& candidate) { return candidate.first == entry.first; });
            expect_error(parse_test_case(object(fields)), "testcase_missing_field");
        }
    }
}

TEST(ShaderTestCaseReject, UnsupportedVersionAndDispatchValues) {
    for (const auto name : {"schema_version", "abi_version"}) {
        reject_field(name, "2", "testcase_unsupported_version");
        reject_field(name, "-1", "testcase_unsupported_version");
        reject_field(name, "1.0", "testcase_invalid_value");
    }
    reject_field("isa_target", "\"gfx1100\"", "testcase_unsupported_dispatch");
    reject_field("stage", "\"vertex\"", "testcase_unsupported_dispatch");
    reject_field("wave_size", "64", "testcase_unsupported_dispatch");
    reject_field("workgroup_size", "[8,4,1]", "testcase_unsupported_dispatch");
    reject_field("workgroup_count", "[2,1,1]", "testcase_unsupported_dispatch");
    reject_field("workgroup_size", "[32,1]", "testcase_invalid_value");
    reject_field("workgroup_count", "[1,1,1,1]", "testcase_invalid_value");
}

TEST(ShaderTestCaseReject, InvalidValues) {
    expect_error(parse_test_case("[]"), "testcase_invalid_value");
    reject_field("name", "\"\"", "testcase_invalid_value");
    reject_field("name", "false", "testcase_invalid_value");
    reject_field("output_type", "\"i32\"", "testcase_invalid_value");
    reject_field("code_words", "[]", "testcase_invalid_value");
    reject_field("code_words", "{}", "testcase_invalid_value");
    for (const auto invalid_word : {"[42]", R"(["0Xbf810000"])", R"(["0xBF810000"])", R"(["0x810000"])",
                                    R"(["0x0bf810000"])", R"(["0xbf81000g"])"}) {
        reject_field("code_words", invalid_word, "testcase_invalid_value");
    }
    for (const auto name : {"input_words", "expected_output_words", "initial_output_words"}) {
        reject_field(name, "[]", "testcase_invalid_value", v1_fields());
        const auto oversized = dwords(ShaderTestLaneWords{});
        reject_field(name, oversized.substr(0, oversized.size() - 1) + R"(,"0x00000000"])",
                     "testcase_invalid_value", v1_fields());
    }
    for (const auto seed : {"-1", "4294967296", "1.0", "1e0", "true", "\"12\""}) {
        reject_field("seed", seed, "testcase_invalid_value");
    }
    reject_field("initial_exec", "0", "testcase_invalid_value", v1_fields());
}

TEST(ShaderTestCaseReject, FileFailuresAndByteLimit) {
    const auto path = test_dir() / "t705_byte_limit.json";
    auto json = object(v0_fields());
    json.resize(4096, ' ');
    std::ofstream output(path, std::ios::binary);
    output << json;
    output.close();
    ASSERT_TRUE(output.good());
    EXPECT_TRUE(load_test_case(path, json.size()).has_value());
    expect_error(load_test_case(path, json.size() - 1), "testcase_too_large");
    expect_error(load_test_case(path, 0), "testcase_too_large");
    expect_error(load_test_case(test_dir() / "t705_nonexistent" / "case.json", 4096), "testcase_open_failed");

    const auto directory = load_test_case(test_dir(), 4096);
    ASSERT_FALSE(directory.has_value());
    EXPECT_TRUE(directory.error().code == "testcase_open_failed" || directory.error().code == "testcase_read_failed");
}

}  // namespace
