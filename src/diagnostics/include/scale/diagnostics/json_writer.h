#pragma once

// Minimal streaming JSON writer for diagnostic reports.
//
// Produces pretty-printed (2-space) RFC 8259 JSON. Strings are emitted as
// UTF-8; invalid UTF-8 sequences are replaced with U+FFFD so that bytes taken
// from untrusted binaries can never produce an invalid document. Misuse (e.g. a
// value without a key inside an object) is a programming error and throws
// std::logic_error.

#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace scale::diagnostics {

class JsonWriter {
public:
    void begin_object();
    void end_object();
    void begin_array();
    void end_array();

    void key(std::string_view name);

    void value(std::string_view text);
    void value(const char* text) { value(std::string_view(text)); }
    void value(bool flag);
    void value(std::int64_t number);
    void value(std::uint64_t number);
    void null();

    template <std::integral T>
        requires(!std::same_as<T, bool> && !std::same_as<T, std::int64_t> &&
                 !std::same_as<T, std::uint64_t>)
    void value(T number) {
        if constexpr (std::signed_integral<T>) {
            value(static_cast<std::int64_t>(number));
        } else {
            value(static_cast<std::uint64_t>(number));
        }
    }

    // Shorthand for key(name) followed by value(v).
    template <class V>
    void field(std::string_view name, const V& v) {
        key(name);
        value(v);
    }

    // True once the single top-level value has been closed.
    [[nodiscard]] bool complete() const noexcept;

    // Returns the document. Throws std::logic_error if it is incomplete.
    [[nodiscard]] const std::string& str() const;

private:
    enum class Scope { object, array };
    struct Frame {
        Scope scope;
        bool has_items;
    };

    void before_value();
    void newline_indent();
    void write_string(std::string_view text);

    std::string out_;
    std::vector<Frame> stack_;
    bool key_pending_ = false;
    bool root_written_ = false;
};

// Escapes `text` as a JSON string literal including the surrounding quotes.
[[nodiscard]] std::string json_quote(std::string_view text);

}  // namespace scale::diagnostics
