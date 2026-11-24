#include "scale/diagnostics/json_writer.h"

#include <array>
#include <stdexcept>

namespace scale::diagnostics {
namespace {

constexpr std::string_view kReplacement = "\xEF\xBF\xBD";  // U+FFFD

// Returns the length of the valid UTF-8 sequence starting at text[i], or 0 if
// the sequence is invalid (overlong, surrogate, out of range or truncated).
std::size_t valid_utf8_length(std::string_view text, std::size_t i) {
    const auto byte = [&](std::size_t k) { return static_cast<unsigned char>(text[k]); };
    const unsigned char b0 = byte(i);
    const std::size_t remaining = text.size() - i;
    const auto cont = [&](std::size_t k) { return (byte(i + k) & 0xC0u) == 0x80u; };

    if (b0 < 0x80u) {
        return 1;
    }
    if (b0 >= 0xC2u && b0 <= 0xDFu) {
        return (remaining >= 2 && cont(1)) ? 2 : 0;
    }
    if (b0 >= 0xE0u && b0 <= 0xEFu) {
        if (remaining < 3 || !cont(1) || !cont(2)) {
            return 0;
        }
        const unsigned char b1 = byte(i + 1);
        if (b0 == 0xE0u && b1 < 0xA0u) {
            return 0;  // overlong
        }
        if (b0 == 0xEDu && b1 >= 0xA0u) {
            return 0;  // UTF-16 surrogate
        }
        return 3;
    }
    if (b0 >= 0xF0u && b0 <= 0xF4u) {
        if (remaining < 4 || !cont(1) || !cont(2) || !cont(3)) {
            return 0;
        }
        const unsigned char b1 = byte(i + 1);
        if (b0 == 0xF0u && b1 < 0x90u) {
            return 0;  // overlong
        }
        if (b0 == 0xF4u && b1 >= 0x90u) {
            return 0;  // above U+10FFFF
        }
        return 4;
    }
    return 0;
}

void append_escaped(std::string& out, std::string_view text) {
    constexpr std::array<char, 16> hex = {'0', '1', '2', '3', '4', '5', '6', '7',
                                          '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    out += '"';
    std::size_t i = 0;
    while (i < text.size()) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c >= 0x80u) {
            const std::size_t len = valid_utf8_length(text, i);
            if (len == 0) {
                out += kReplacement;
                ++i;
            } else {
                out.append(text.substr(i, len));
                i += len;
            }
            continue;
        }
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20u || c == 0x7Fu) {
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 0x0Fu];
                } else {
                    out += static_cast<char>(c);
                }
        }
        ++i;
    }
    out += '"';
}

}  // namespace

std::string json_quote(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2);
    append_escaped(out, text);
    return out;
}

void JsonWriter::newline_indent() {
    out_ += '\n';
    out_.append(stack_.size() * 2, ' ');
}

void JsonWriter::before_value() {
    if (stack_.empty()) {
        if (root_written_) {
            throw std::logic_error("JsonWriter: multiple top-level values");
        }
        root_written_ = true;
        return;
    }
    Frame& top = stack_.back();
    if (top.scope == Scope::object) {
        if (!key_pending_) {
            throw std::logic_error("JsonWriter: value inside object without key");
        }
        key_pending_ = false;
        return;
    }
    if (top.has_items) {
        out_ += ',';
    }
    top.has_items = true;
    newline_indent();
}

void JsonWriter::key(std::string_view name) {
    if (stack_.empty() || stack_.back().scope != Scope::object || key_pending_) {
        throw std::logic_error("JsonWriter: key outside object or duplicate key call");
    }
    Frame& top = stack_.back();
    if (top.has_items) {
        out_ += ',';
    }
    top.has_items = true;
    newline_indent();
    append_escaped(out_, name);
    out_ += ": ";
    key_pending_ = true;
}

void JsonWriter::begin_object() {
    before_value();
    out_ += '{';
    stack_.push_back({Scope::object, false});
}

void JsonWriter::end_object() {
    if (stack_.empty() || stack_.back().scope != Scope::object || key_pending_) {
        throw std::logic_error("JsonWriter: mismatched end_object");
    }
    const bool had_items = stack_.back().has_items;
    stack_.pop_back();
    if (had_items) {
        newline_indent();
    }
    out_ += '}';
}

void JsonWriter::begin_array() {
    before_value();
    out_ += '[';
    stack_.push_back({Scope::array, false});
}

void JsonWriter::end_array() {
    if (stack_.empty() || stack_.back().scope != Scope::array) {
        throw std::logic_error("JsonWriter: mismatched end_array");
    }
    const bool had_items = stack_.back().has_items;
    stack_.pop_back();
    if (had_items) {
        newline_indent();
    }
    out_ += ']';
}

void JsonWriter::value(std::string_view text) {
    before_value();
    append_escaped(out_, text);
}

void JsonWriter::value(bool flag) {
    before_value();
    out_ += flag ? "true" : "false";
}

void JsonWriter::value(std::int64_t number) {
    before_value();
    out_ += std::to_string(number);
}

void JsonWriter::value(std::uint64_t number) {
    before_value();
    out_ += std::to_string(number);
}

void JsonWriter::null() {
    before_value();
    out_ += "null";
}

bool JsonWriter::complete() const noexcept {
    return root_written_ && stack_.empty() && !key_pending_;
}

const std::string& JsonWriter::str() const {
    if (!complete()) {
        throw std::logic_error("JsonWriter: document incomplete");
    }
    return out_;
}

}  // namespace scale::diagnostics
