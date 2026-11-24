#include "scale/common/byte_reader.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

namespace {

constexpr std::array<std::byte, 8> kBytes = {std::byte{0x4D}, std::byte{0x5A}, std::byte{0x90}, std::byte{0x00},
                                             std::byte{0x78}, std::byte{0x56}, std::byte{0x34}, std::byte{0x12}};

}  // namespace

TEST(ByteView, ReadsLittleEndian) {
    const scale::ByteView view(kBytes);
    EXPECT_EQ(view.u16(0), std::uint16_t{0x5A4D});
    EXPECT_EQ(view.u32(4), std::uint32_t{0x12345678});
    EXPECT_EQ(view.u64(0), std::uint64_t{0x1234567800905A4D});
}

TEST(ByteView, RejectsOutOfBoundsReads) {
    const scale::ByteView view(kBytes);
    EXPECT_FALSE(view.u32(5).has_value());
    EXPECT_FALSE(view.u64(1).has_value());
    EXPECT_FALSE(view.u8(8).has_value());
    EXPECT_FALSE(view.u8(std::numeric_limits<std::size_t>::max()).has_value());
}

TEST(ByteView, SubviewBounds) {
    const scale::ByteView view(kBytes);
    const auto sub = view.subview(4, 4);
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->size(), 4u);
    EXPECT_EQ(sub->u32(0), std::uint32_t{0x12345678});
    EXPECT_FALSE(sub->u8(4).has_value());
    EXPECT_TRUE(view.subview(8, 0).has_value());
    EXPECT_FALSE(view.subview(4, 5).has_value());
    EXPECT_FALSE(view.subview(1, std::numeric_limits<std::size_t>::max()).has_value());
}

TEST(ByteView, Empty) {
    const scale::ByteView view;
    EXPECT_TRUE(view.empty());
    EXPECT_FALSE(view.u8(0).has_value());
}
