#include "scale/common/checked.h"

#include "scale_test.h"

#include <cstdint>
#include <limits>

using scale::checked_add;
using scale::checked_align_up;
using scale::checked_cast;
using scale::checked_mul;
using scale::checked_sub;
using scale::range_within;

SCALE_TEST(checked_add_detects_overflow) {
    constexpr auto max32 = std::numeric_limits<std::uint32_t>::max();
    SCALE_CHECK_EQ(checked_add<std::uint32_t>(1, 2).value(), 3u);
    SCALE_CHECK_EQ(checked_add<std::uint32_t>(max32 - 1, 1).value(), max32);
    SCALE_CHECK(!checked_add<std::uint32_t>(max32, 1).has_value());
}

SCALE_TEST(checked_sub_detects_underflow) {
    SCALE_CHECK_EQ(checked_sub<std::uint64_t>(5, 5).value(), 0u);
    SCALE_CHECK(!checked_sub<std::uint64_t>(4, 5).has_value());
}

SCALE_TEST(checked_mul_detects_overflow) {
    constexpr auto max64 = std::numeric_limits<std::uint64_t>::max();
    SCALE_CHECK_EQ(checked_mul<std::uint64_t>(0, max64).value(), 0u);
    SCALE_CHECK_EQ(checked_mul<std::uint64_t>(max64, 1).value(), max64);
    SCALE_CHECK(!checked_mul<std::uint64_t>(max64 / 2 + 1, 2).has_value());
}

SCALE_TEST(checked_cast_rejects_narrowing) {
    SCALE_CHECK_EQ(checked_cast<std::uint16_t>(std::uint64_t{65535}).value(), 65535u);
    SCALE_CHECK(!checked_cast<std::uint16_t>(std::uint64_t{65536}).has_value());
}

SCALE_TEST(range_within_boundaries) {
    SCALE_CHECK(range_within<std::size_t>(0, 10, 10));
    SCALE_CHECK(range_within<std::size_t>(10, 0, 10));
    SCALE_CHECK(!range_within<std::size_t>(10, 1, 10));
    SCALE_CHECK(!range_within<std::size_t>(11, 0, 10));
    // offset + size would wrap around
    SCALE_CHECK(!range_within<std::size_t>(5, std::numeric_limits<std::size_t>::max(), 10));
}

SCALE_TEST(checked_align_up_power_of_two_only) {
    SCALE_CHECK_EQ(checked_align_up<std::uint32_t>(0x1001, 0x1000).value(), 0x2000u);
    SCALE_CHECK_EQ(checked_align_up<std::uint32_t>(0x1000, 0x1000).value(), 0x1000u);
    SCALE_CHECK(!checked_align_up<std::uint32_t>(10, 3).has_value());
    SCALE_CHECK(!checked_align_up<std::uint32_t>(10, 0).has_value());
    SCALE_CHECK(!checked_align_up<std::uint32_t>(0xFFFFFFFFu, 0x1000).has_value());
}
