#pragma once

// Checked unsigned arithmetic and range helpers for parsing untrusted data.
// All functions return std::nullopt (or false) on overflow instead of wrapping.

#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>

namespace scale {

template <std::unsigned_integral T>
[[nodiscard]] constexpr std::optional<T> checked_add(T a, T b) noexcept {
    if (a > std::numeric_limits<T>::max() - b) {
        return std::nullopt;
    }
    return static_cast<T>(a + b);
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr std::optional<T> checked_sub(T a, T b) noexcept {
    if (b > a) {
        return std::nullopt;
    }
    return static_cast<T>(a - b);
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr std::optional<T> checked_mul(T a, T b) noexcept {
    if (a != 0 && b > std::numeric_limits<T>::max() / a) {
        return std::nullopt;
    }
    return static_cast<T>(a * b);
}

// Converts between unsigned types, failing if the value does not fit.
template <std::unsigned_integral To, std::unsigned_integral From>
[[nodiscard]] constexpr std::optional<To> checked_cast(From value) noexcept {
    if (value > std::numeric_limits<To>::max()) {
        return std::nullopt;
    }
    return static_cast<To>(value);
}

// True if [offset, offset + size) lies entirely within [0, limit).
// A zero-sized range at offset == limit is in range.
template <std::unsigned_integral T>
[[nodiscard]] constexpr bool range_within(T offset, T size, T limit) noexcept {
    return offset <= limit && size <= limit - offset;
}

// Rounds `value` up to a multiple of `alignment`, which must be a power of two.
template <std::unsigned_integral T>
[[nodiscard]] constexpr std::optional<T> checked_align_up(T value, T alignment) noexcept {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return std::nullopt;
    }
    const auto bumped = checked_add<T>(value, static_cast<T>(alignment - 1));
    if (!bumped) {
        return std::nullopt;
    }
    return static_cast<T>(*bumped & ~static_cast<T>(alignment - 1));
}

}  // namespace scale
