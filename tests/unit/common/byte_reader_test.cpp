#include "scale/common/byte_reader.h"

#include "scale_test.h"

#include <array>
#include <cstddef>
#include <limits>

namespace {

constexpr std::array<std::byte, 8> kBytes = {std::byte{0x4D}, std::byte{0x5A}, std::byte{0x90}, std::byte{0x00},
                                             std::byte{0x78}, std::byte{0x56}, std::byte{0x34}, std::byte{0x12}};

}  // namespace

SCALE_TEST(byte_view_reads_little_endian) {
    const scale::ByteView view(kBytes);
    SCALE_CHECK_EQ(view.u16(0).value(), 0x5A4Du);
    SCALE_CHECK_EQ(view.u32(4).value(), 0x12345678u);
    SCALE_CHECK_EQ(view.u64(0).value(), 0x1234567800905A4Dull);
}

SCALE_TEST(byte_view_rejects_out_of_bounds_reads) {
    const scale::ByteView view(kBytes);
    SCALE_CHECK(!view.u32(5).has_value());
    SCALE_CHECK(!view.u64(1).has_value());
    SCALE_CHECK(!view.u8(8).has_value());
    SCALE_CHECK(!view.u8(std::numeric_limits<std::size_t>::max()).has_value());
}

SCALE_TEST(byte_view_subview_bounds) {
    const scale::ByteView view(kBytes);
    const auto sub = view.subview(4, 4);
    SCALE_REQUIRE(sub.has_value());
    SCALE_CHECK_EQ(sub->size(), 4u);
    SCALE_CHECK_EQ(sub->u32(0).value(), 0x12345678u);
    SCALE_CHECK(!sub->u8(4).has_value());
    SCALE_CHECK(view.subview(8, 0).has_value());
    SCALE_CHECK(!view.subview(4, 5).has_value());
    SCALE_CHECK(!view.subview(1, std::numeric_limits<std::size_t>::max()).has_value());
}

SCALE_TEST(byte_view_empty) {
    const scale::ByteView view;
    SCALE_CHECK(view.empty());
    SCALE_CHECK(!view.u8(0).has_value());
}
