#pragma once

// Structured error type shared by all components.
//
// `component` and `code` are stable, lowercase snake_case identifiers that map
// directly onto the diagnostic contract (docs/contracts/diagnostics.md).
// `message` is human-readable English text and may change between versions.

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace scale {

struct Error {
    std::string component;  // e.g. "loader", "shader"
    std::string code;       // e.g. "truncated_header", "unsupported_opcode"
    std::string message;    // human-readable detail

    // "component/code: message"
    [[nodiscard]] std::string to_string() const;
};

template <class T>
using Result = std::expected<T, Error>;

// Convenience constructor: `return scale::fail("loader", "bad_magic", "...");`
[[nodiscard]] inline std::unexpected<Error> fail(std::string_view component,
                                                 std::string_view code,
                                                 std::string message) {
    return std::unexpected<Error>(
        Error{std::string(component), std::string(code), std::move(message)});
}

}  // namespace scale
