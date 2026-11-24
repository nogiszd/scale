#include "scale/shader/test_case.h"

#include <algorithm>
#include <bitset>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <utility>

namespace scale::shader {
namespace {

// Validate before parsing so every raw byte copied from a string stays UTF-8.
bool valid_utf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i++]);
        if (lead < 0x80) continue;

        unsigned remaining = 0;
        std::uint32_t codepoint = 0;
        std::uint32_t minimum = 0;
        if (lead >= 0xc2 && lead <= 0xdf) {
            remaining = 1;
            codepoint = lead & 0x1fu;
            minimum = 0x80;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            remaining = 2;
            codepoint = lead & 0x0fu;
            minimum = 0x800;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            remaining = 3;
            codepoint = lead & 0x07u;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (remaining > text.size() - i) return false;
        while (remaining != 0) {
            const auto byte = static_cast<unsigned char>(text[i++]);
            if ((byte & 0xc0u) != 0x80u) return false;
            codepoint = (codepoint << 6) | (byte & 0x3fu);
            --remaining;
        }
        if (codepoint < minimum || codepoint > 0x10ffffu || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            return false;
        }
    }
    return true;
}

void append_utf8(std::string& text, std::uint32_t codepoint) {
    if (codepoint < 0x80) {
        text.push_back(static_cast<char>(codepoint));
        return;
    }
    if (codepoint < 0x800) {
        text.push_back(static_cast<char>(0xc0u | (codepoint >> 6)));
    } else {
        if (codepoint < 0x10000) {
            text.push_back(static_cast<char>(0xe0u | (codepoint >> 12)));
        } else {
            text.push_back(static_cast<char>(0xf0u | (codepoint >> 18)));
            text.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3fu)));
        }
        text.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3fu)));
    }
    text.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
}

constexpr bool digit(char ch) { return ch >= '0' && ch <= '9'; }

enum class Field : std::size_t {
    schema_version, abi_version, name, isa_target, stage, wave_size,
    workgroup_size, workgroup_count, code_words, input_words, expected_output_words,
    output_type, seed, initial_exec, initial_output_words,
};

constexpr std::string_view kFields[]{
    "schema_version", "abi_version", "name", "isa_target", "stage", "wave_size",
    "workgroup_size", "workgroup_count", "code_words", "input_words", "expected_output_words",
    "output_type", "seed", "initial_exec", "initial_output_words",
};

constexpr std::size_t kFirstV1Field = static_cast<std::size_t>(Field::initial_exec);
static_assert(std::size(kFields) == static_cast<std::size_t>(Field::initial_output_words) + 1);

// The schema admits one object with scalar members and flat arrays. Parse
// those directly instead of allocating a general-purpose JSON tree.
class Parser {
public:
    explicit Parser(std::string_view json) : json_(json) {}

    Result<ShaderTestCase> parse() {
        if (!take('{')) return expected_value("expected a JSON object");
        if (!take('}')) {
            for (;;) {
                if (!take('"')) return syntax("expected an object member name");
                const auto key = read_string_body();
                if (!key) return std::unexpected(key.error());
                if (!take(':')) return syntax("expected ':' after object member name");
                const auto entry = std::ranges::find(kFields, *key);
                if (entry == std::end(kFields)) return error("testcase_unknown_field", "unknown test case field");
                const auto index = static_cast<std::size_t>(entry - std::begin(kFields));
                field_ = *entry;
                if (index >= kFirstV1Field && seen_.test(static_cast<std::size_t>(Field::abi_version)) &&
                        version_ == ShaderTestAbiVersion::v0) {
                    return error("testcase_unknown_field", "field is defined for ABI v1 only");
                }
                if (seen_.test(index)) return error("testcase_duplicate_field", "duplicate field");
                seen_.set(index);
                if (auto result = read_field(static_cast<Field>(index)); !result) {
                    return std::unexpected(std::move(result.error()));
                }
                field_ = {};
                if (take('}')) break;
                if (!take(',')) return syntax("expected ',' or '}' after object member");
            }
        }
        skip_space();
        if (position_ != json_.size()) return syntax("unexpected data after the test case object");

        const auto required = version_ == ShaderTestAbiVersion::v1 ? std::size(kFields) : kFirstV1Field;
        for (std::size_t i = 0; i < required; ++i) {
            if (!seen_.test(i)) {
                field_ = kFields[i];
                return error("testcase_missing_field", "required field is missing");
            }
        }
        if (version_ == ShaderTestAbiVersion::v1) {
            result_.v1 = v1_;
        } else {
            for (std::size_t i = kFirstV1Field; i < std::size(kFields); ++i) {
                if (seen_.test(i)) {
                    field_ = kFields[i];
                    return error("testcase_unknown_field", "field is defined for ABI v1 only");
                }
            }
        }
        return std::move(result_);
    }

private:
    char peek() const { return position_ == json_.size() ? '\0' : json_[position_]; }

    void skip_space() {
        while (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n') ++position_;
    }

    bool take(char ch) {
        skip_space();
        if (peek() != ch) return false;
        ++position_;
        return true;
    }

    std::unexpected<Error> error(std::string_view code, std::string_view detail) const {
        if (field_.empty()) return fail("shader", code, std::format("At byte {}: {}", position_, detail));
        return fail("shader", code, std::format("{} at byte {}: {}", field_, position_, detail));
    }

    std::unexpected<Error> syntax(std::string_view detail) const { return error("testcase_invalid_json", detail); }
    std::unexpected<Error> invalid(std::string_view detail) const { return error("testcase_invalid_value", detail); }

    std::unexpected<Error> expected_value(std::string_view detail) const {
        switch (peek()) {
        case '{': case '[': case '"': case '-': return invalid(detail);
        default: break;
        }
        const auto tail = json_.substr(position_);
        if (digit(peek()) || tail.starts_with("null") || tail.starts_with("true") || tail.starts_with("false")) {
            return invalid(detail);
        }
        return syntax(detail);
    }

    std::optional<std::uint32_t> hex_quad() {
        if (json_.size() - position_ < 4) return std::nullopt;
        const auto* first = json_.data() + position_;
        std::uint32_t value = 0;
        const auto [end, ec] = std::from_chars(first, first + 4, value, 16);
        if (ec != std::errc{} || end != first + 4) return std::nullopt;
        position_ += 4;
        return value;
    }

    Result<void> append_escape(std::string& text) {
        const char escaped = peek();
        if (position_ == json_.size()) return syntax("incomplete string escape");
        ++position_;
        switch (escaped) {
        case '"': case '\\': case '/': text.push_back(escaped); return {};
        case 'b': text.push_back('\b'); return {};
        case 'f': text.push_back('\f'); return {};
        case 'n': text.push_back('\n'); return {};
        case 'r': text.push_back('\r'); return {};
        case 't': text.push_back('\t'); return {};
        case 'u': break;
        default: return syntax("unknown string escape");
        }

        auto codepoint = hex_quad();
        if (!codepoint) return syntax("expected four hexadecimal digits in a Unicode escape");
        if (*codepoint >= 0xd800 && *codepoint <= 0xdbff) {
            if (!json_.substr(position_).starts_with("\\u")) return syntax("missing low surrogate");
            position_ += 2;
            const auto low = hex_quad();
            if (!low || *low < 0xdc00 || *low > 0xdfff) return syntax("invalid low surrogate");
            codepoint = 0x10000u + ((*codepoint - 0xd800u) << 10) + (*low - 0xdc00u);
        } else if (*codepoint >= 0xdc00 && *codepoint <= 0xdfff) {
            return syntax("unpaired low surrogate");
        }
        append_utf8(text, *codepoint);
        return {};
    }

    Result<std::string> read_string_body() {
        std::string text;
        while (position_ < json_.size()) {
            const char ch = json_[position_++];
            if (ch == '"') return text;
            if (static_cast<unsigned char>(ch) < 0x20) return syntax("unescaped control character");
            if (ch != '\\') {
                text.push_back(ch);
            } else if (auto result = append_escape(text); !result) {
                return std::unexpected(std::move(result.error()));
            }
        }
        return syntax("unterminated string");
    }

    Result<std::string> read_string() {
        if (!take('"')) return expected_value("expected a string");
        return read_string_body();
    }

    Result<std::string_view> read_integer() {
        skip_space();
        const auto start = position_;
        if (peek() == '-') ++position_;
        if (!digit(peek())) {
            if (position_ != start) return syntax("expected digit after '-'");
            return expected_value("expected an integer");
        }
        if (peek() == '0') {
            ++position_;
            if (digit(peek())) return syntax("leading zero in number");
        } else {
            while (digit(peek())) ++position_;
        }
        const auto integer_end = position_;
        if (peek() == '.') {
            ++position_;
            if (!digit(peek())) return syntax("expected a fraction digit");
            while (digit(peek())) ++position_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++position_;
            if (peek() == '+' || peek() == '-') ++position_;
            if (!digit(peek())) return syntax("expected an exponent digit");
            while (digit(peek())) ++position_;
        }
        if (position_ != integer_end) return invalid("expected an integer");
        return json_.substr(start, position_ - start);
    }

    Result<std::uint32_t> read_unsigned() {
        const auto text = read_integer();
        if (!text) return std::unexpected(text.error());
        if (*text == "-0") return 0;
        std::uint32_t value = 0;
        const auto [end, ec] = std::from_chars(text->data(), text->data() + text->size(), value);
        if (ec != std::errc{} || end != text->data() + text->size()) return invalid("expected an unsigned 32-bit integer");
        return value;
    }

    Result<std::uint32_t> read_word() {
        const auto text = read_string();
        if (!text) return std::unexpected(text.error());
        if (text->size() != 10 || !text->starts_with("0x") ||
                text->find_first_not_of("0123456789abcdef", 2) != std::string::npos) {
            return invalid("expected 0x followed by eight lowercase hexadecimal digits");
        }
        std::uint32_t word = 0;
        static_cast<void>(std::from_chars(text->data() + 2, text->data() + text->size(), word, 16));
        return word;
    }

    template <class ReadElement>
    Result<void> read_array(ReadElement read_element) {
        if (!take('[')) return expected_value("expected an array");
        if (take(']')) return {};
        for (;;) {
            if (auto read = read_element(); !read) return read;
            if (take(']')) return {};
            if (!take(',')) return syntax("expected ',' or ']' after array element");
        }
    }

    template <std::size_t N>
    Result<void> read_fixed(std::array<std::uint32_t, N>& words, Result<std::uint32_t> (Parser::*read)()) {
        std::size_t size = 0;
        auto result = read_array([&]() -> Result<void> {
            const auto value = (this->*read)();
            if (!value) return std::unexpected(value.error());
            if (size == N) return invalid(std::format("expected exactly {} array elements", N));
            words[size++] = *value;
            return {};
        });
        if (!result) return result;
        if (size != N) return invalid(std::format("expected exactly {} array elements", N));
        return {};
    }

    Result<void> read_version(Field field) {
        const auto text = read_integer();
        if (!text) return std::unexpected(text.error());
        if (field == Field::schema_version) {
            if (*text == "1") return {};
        } else {
            if (*text == "0" || *text == "-0") return {};
            if (*text == "1") {
                version_ = ShaderTestAbiVersion::v1;
                return {};
            }
        }
        return error("testcase_unsupported_version", "unsupported test case version");
    }

    Result<void> read_dispatch_field(Field field) {
        if (field == Field::isa_target || field == Field::stage) {
            const auto text = read_string();
            if (!text) return std::unexpected(text.error());
            if (*text == (field == Field::isa_target ? "gfx1030" : "compute")) return {};
        } else if (field == Field::wave_size) {
            const auto size = read_unsigned();
            if (!size) return std::unexpected(size.error());
            if (*size == 32) return {};
        } else {
            std::array<std::uint32_t, 3> dimensions{};
            if (auto result = read_fixed(dimensions, &Parser::read_unsigned); !result) return result;
            if (dimensions == std::array<std::uint32_t, 3>{field == Field::workgroup_size ? 32u : 1u, 1, 1}) return {};
        }
        return error("testcase_unsupported_dispatch", "dispatch field is outside the shader ABI");
    }

    Result<void> read_field(Field field) {
        switch (field) {
        case Field::schema_version: case Field::abi_version: return read_version(field);
        case Field::isa_target: case Field::stage: case Field::wave_size:
        case Field::workgroup_size: case Field::workgroup_count: return read_dispatch_field(field);
        case Field::name: {
            auto name = read_string();
            if (!name) return std::unexpected(std::move(name.error()));
            if (name->empty()) return invalid("test case name must be nonempty");
            result_.name = std::move(*name);
            return {};
        }
        case Field::code_words: {
            auto result = read_array([&]() -> Result<void> {
                const auto word = read_word();
                if (!word) return std::unexpected(word.error());
                result_.code_words.push_back(*word);
                return {};
            });
            if (!result) return result;
            if (result_.code_words.empty()) return invalid("code_words must be nonempty");
            return {};
        }
        case Field::input_words: return read_fixed(result_.input_words, &Parser::read_word);
        case Field::expected_output_words: return read_fixed(result_.expected_output_words, &Parser::read_word);
        case Field::initial_output_words: return read_fixed(v1_.initial_output_words, &Parser::read_word);
        case Field::output_type: {
            const auto type = read_string();
            if (!type) return std::unexpected(type.error());
            if (*type == "u32") return {};
            if (*type != "f32") return invalid("expected u32 or f32");
            result_.output_type = ShaderTestOutputType::f32;
            return {};
        }
        case Field::seed: {
            skip_space();
            if (json_.substr(position_).starts_with("null")) {
                position_ += 4;
                return {};
            }
            const auto seed = read_unsigned();
            if (!seed) return std::unexpected(seed.error());
            result_.seed = *seed;
            return {};
        }
        case Field::initial_exec: {
            const auto mask = read_word();
            if (!mask) return std::unexpected(mask.error());
            v1_.initial_exec = *mask;
            return {};
        }
        }
        return error("testcase_unknown_field", "unknown test case field");
    }

    std::string_view json_;
    std::string_view field_;
    std::size_t position_{};
    std::bitset<std::size(kFields)> seen_;
    ShaderTestAbiVersion version_{};
    ShaderTestCase result_;
    ShaderTestCaseV1 v1_;
};

}  // namespace

Result<ShaderTestCase> parse_test_case(std::string_view json) {
    if (!valid_utf8(json)) return fail("shader", "testcase_invalid_utf8", "test case must be valid UTF-8");
    return Parser(json).parse();
}

Result<ShaderTestCase> load_test_case(const std::filesystem::path& path, std::uint64_t max_bytes) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) return fail("shader", "testcase_open_failed", "could not open shader test case");

    std::string json;
    std::array<char, 4096> buffer{};
    // Detect oversized streams even when a file changes while being read.
    do {
        const auto remaining = std::min<std::uint64_t>(max_bytes, json.max_size()) - json.size();
        const auto request = remaining < buffer.size() ? remaining + 1 : buffer.size();
        stream.read(buffer.data(), static_cast<std::streamsize>(request));
        if (stream.bad() || (stream.fail() && !stream.eof())) {
            return fail("shader", "testcase_read_failed", "could not read shader test case");
        }
        const auto count = static_cast<std::size_t>(stream.gcount());
        if (count > remaining) {
            return fail("shader", "testcase_too_large", "shader test case exceeds the byte limit");
        }
        json.append(buffer.data(), count);
    } while (!stream.eof());
    return parse_test_case(json);
}

}  // namespace scale::shader
