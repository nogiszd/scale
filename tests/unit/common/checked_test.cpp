#include "scale/common/checked.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

using scale::checked_add;
using scale::checked_align_up;
using scale::checked_cast;
using scale::checked_mul;
using scale::checked_sub;
using scale::range_within;

TEST(Checked, AddDetectsOverflow) {
    constexpr auto max32 = std::numeric_limits<std::uint32_t>::max();
    EXPECT_EQ(checked_add<std::uint32_t>(1, 2), 3u);
    EXPECT_EQ(checked_add<std::uint32_t>(max32 - 1, 1), max32);
    EXPECT_FALSE(checked_add<std::uint32_t>(max32, 1).has_value());
}

TEST(Checked, SubDetectsUnderflow) {
    EXPECT_EQ(checked_sub<std::uint64_t>(5, 5), 0u);
    EXPECT_FALSE(checked_sub<std::uint64_t>(4, 5).has_value());
}

TEST(Checked, MulDetectsOverflow) {
    constexpr auto max64 = std::numeric_limits<std::uint64_t>::max();
    EXPECT_EQ(checked_mul<std::uint64_t>(0, max64), 0u);
    EXPECT_EQ(checked_mul<std::uint64_t>(max64, 1), max64);
    EXPECT_FALSE(checked_mul<std::uint64_t>(max64 / 2 + 1, 2).has_value());
}

TEST(Checked, CastRejectsNarrowing) {
    EXPECT_EQ(checked_cast<std::uint16_t>(std::uint64_t{65535}), std::uint16_t{65535});
    EXPECT_FALSE(checked_cast<std::uint16_t>(std::uint64_t{65536}).has_value());
}

TEST(Checked, RangeWithinBoundaries) {
    EXPECT_TRUE(range_within<std::size_t>(0, 10, 10));
    EXPECT_TRUE(range_within<std::size_t>(10, 0, 10));
    EXPECT_FALSE(range_within<std::size_t>(10, 1, 10));
    EXPECT_FALSE(range_within<std::size_t>(11, 0, 10));
    // offset + size would wrap around
    EXPECT_FALSE(range_within<std::size_t>(5, std::numeric_limits<std::size_t>::max(), 10));
}

TEST(Checked, AlignUpPowerOfTwoOnly) {
    EXPECT_EQ(checked_align_up<std::uint32_t>(0x1001, 0x1000), 0x2000u);
    EXPECT_EQ(checked_align_up<std::uint32_t>(0x1000, 0x1000), 0x1000u);
    EXPECT_FALSE(checked_align_up<std::uint32_t>(10, 3).has_value());
    EXPECT_FALSE(checked_align_up<std::uint32_t>(10, 0).has_value());
    EXPECT_FALSE(checked_align_up<std::uint32_t>(0xFFFFFFFFu, 0x1000).has_value());
}
