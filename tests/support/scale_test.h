#pragma once

// Minimal assertion harness for Scale tests.
//
//   SCALE_TEST(name) { SCALE_CHECK(x == 1); SCALE_CHECK_EQ(a, b); }
//   SCALE_SKIP("no Vulkan device");   // marks the current case skipped
//
// The executable runs every registered case (or those named on the command
// line) and exits with:
//   0   all executed cases passed
//   1   at least one case failed
//   77  no failures but at least one case skipped (CTest reports SKIPPED)
//
// Failed checks are recorded and the case continues; SCALE_REQUIRE aborts the
// current case on failure.

#include <format>
#include <source_location>
#include <string>
#include <string_view>

namespace scale::test {

using TestFn = void (*)();

void register_test(std::string_view name, TestFn fn);
void record_failure(std::string_view expression, std::string_view detail, std::source_location where);

struct SkipCase {
    std::string reason;
};
struct AbortCase {};

struct Registrar {
    Registrar(std::string_view name, TestFn fn) { register_test(name, fn); }
};

template <class T>
std::string describe(const T& value) {
    if constexpr (requires { std::format("{}", value); }) {
        return std::format("{}", value);
    } else {
        return "<unprintable>";
    }
}

}  // namespace scale::test

#define SCALE_TEST(name)                                                        \
    static void scale_test_##name();                                            \
    static const ::scale::test::Registrar scale_test_registrar_##name(#name,    \
                                                                      &scale_test_##name); \
    static void scale_test_##name()

#define SCALE_CHECK(expr)                                                                  \
    do {                                                                                   \
        if (!(expr)) {                                                                     \
            ::scale::test::record_failure(#expr, "", std::source_location::current());    \
        }                                                                                  \
    } while (false)

#define SCALE_REQUIRE(expr)                                                                \
    do {                                                                                   \
        if (!(expr)) {                                                                     \
            ::scale::test::record_failure(#expr, "", std::source_location::current());    \
            throw ::scale::test::AbortCase{};                                              \
        }                                                                                  \
    } while (false)

#define SCALE_CHECK_EQ(actual, expected)                                                   \
    do {                                                                                   \
        const auto& scale_actual_ = (actual);                                              \
        const auto& scale_expected_ = (expected);                                          \
        if (!(scale_actual_ == scale_expected_)) {                                         \
            ::scale::test::record_failure(                                                 \
                #actual " == " #expected,                                                  \
                std::format("actual: {}, expected: {}",                                   \
                            ::scale::test::describe(scale_actual_),                        \
                            ::scale::test::describe(scale_expected_)),                     \
                std::source_location::current());                                          \
        }                                                                                  \
    } while (false)

#define SCALE_SKIP(reason) throw ::scale::test::SkipCase{std::string(reason)}
