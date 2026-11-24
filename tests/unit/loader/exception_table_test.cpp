// T-301: validation of the x64 exception directory and unwind information
// (portable; nothing is mapped or registered here).
//
// Layout from exception_image(): .text [0x1000, 0x1030) executable,
// .rdata at 0x2000 with 0x200 bytes of file data. Unwind blobs are placed at
// .rdata offsets, so offset 0x40 is RVA 0x2040.

#include "scale/loader/exception_table.h"

#include "pe_builder.h"

#include <gtest/gtest.h>

using namespace scale::loader;
using scale::test::Bytes;
using scale::test::exception_image;
using scale::test::PeBuilder;
using scale::test::put_u32;
using scale::test::RuntimeFunctionSpec;
using scale::test::unwind_header;

namespace {

constexpr std::uint8_t kEHandler = 0x1;
constexpr std::uint8_t kUHandler = 0x2;
constexpr std::uint8_t kChain = 0x4;

Bytes with_u32s(Bytes b, std::initializer_list<std::uint32_t> words) {
    std::size_t offset = b.size();
    for (std::uint32_t w : words) {
        put_u32(b, offset, w);
        offset += 4;
    }
    return b;
}

scale::Result<std::optional<ExceptionTable>> validate(const PeBuilder& pe) {
    const Bytes bytes = pe.build();
    const auto image = inspect_pe(scale::ByteView(bytes));
    EXPECT_TRUE(image.has_value()) << image.error().to_string();
    if (!image) return std::unexpected(image.error());
    return validate_exception_table(scale::ByteView(bytes), *image);
}

std::string code_of(const PeBuilder& pe) {
    const auto r = validate(pe);
    if (r) return "ok";
    EXPECT_FALSE(r.error().message.empty());
    return r.error().code;
}

// One plain function [0x1000, 0x1010) with the given unwind blob at 0x2000.
PeBuilder single(Bytes unwind, RuntimeFunctionSpec f = {0x1000, 0x1010, 0x2000}) {
    return exception_image({{0x00, std::move(unwind)}}, {f});
}

}  // namespace

TEST(ExceptionTable, AbsentDirectoryIsNotAnError) {
    PeBuilder pe;
    pe.entry_rva = pe.add_section(".text", Bytes(0x10, std::byte{0xC3}), PeBuilder::kText);
    const auto r = validate(pe);
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    EXPECT_FALSE(r->has_value());
}

TEST(ExceptionTable, ValidTableCountsHandlersAndChains) {
    const PeBuilder pe = exception_image(
        {{0x00, unwind_header(1, 0)},                                     // plain
         {0x10, with_u32s(unwind_header(1, kEHandler | kUHandler), {0x1000, 0xAABBCCDD})},  // handler + data
         {0x20, with_u32s(unwind_header(2, kChain, 0, 2), {0, 0x1000, 0x1010, 0x2000})}},  // 2 code slots, chain
        {{0x1000, 0x1010, 0x2000}, {0x1010, 0x1020, 0x2010}, {0x1020, 0x1030, 0x2020}});
    const auto r = validate(pe);
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    ASSERT_TRUE(r->has_value());
    EXPECT_EQ((*r)->rva, 0x2100u);
    EXPECT_EQ((*r)->function_count, 3u);
    EXPECT_EQ((*r)->handler_count, 1u);
    EXPECT_EQ((*r)->chained_count, 1u);
}

TEST(ExceptionTable, OddCodeCountUsesAnEvenNumberOfSlots) {
    // 1 code -> 2 slots, so the handler RVA is at +8 (0x1000, executable).
    // Reading it at +6 (no rounding) would give 0x1000FFFF, outside every section.
    const PeBuilder pe = single(with_u32s(unwind_header(1, kEHandler, 0, 1), {0xFFFF0000u, 0x1000}));
    EXPECT_EQ(code_of(pe), "ok");
}
TEST(ExceptionTableReject, FunctionRanges) {
    EXPECT_EQ(code_of(single(unwind_header(1, 0), {0x1010, 0x1010, 0x2000})), "runtime_function_invalid");  // empty
    EXPECT_EQ(code_of(single(unwind_header(1, 0), {0x1010, 0x1000, 0x2000})), "runtime_function_invalid");  // reversed
    EXPECT_EQ(code_of(single(unwind_header(1, 0), {0x2000, 0x2010, 0x2000})), "runtime_function_invalid");  // .rdata
    EXPECT_EQ(code_of(single(unwind_header(1, 0), {0x1020, 0x1040, 0x2000})), "runtime_function_invalid");  // past .text
    EXPECT_EQ(code_of(single(unwind_header(1, 0), {0x9000, 0x9010, 0x2000})), "runtime_function_invalid");  // no section
}

TEST(ExceptionTableReject, UnsortedAndOverlappingEntries) {
    const auto unwind = std::vector<std::pair<std::uint32_t, Bytes>>{{0x00, unwind_header(1, 0)}};
    EXPECT_EQ(code_of(exception_image(unwind, {{0x1010, 0x1020, 0x2000}, {0x1000, 0x1010, 0x2000}})),
              "runtime_function_unsorted");
    EXPECT_EQ(code_of(exception_image(unwind, {{0x1000, 0x1018, 0x2000}, {0x1010, 0x1020, 0x2000}})),
              "runtime_function_unsorted");
    EXPECT_EQ(code_of(exception_image(unwind, {{0x1000, 0x1010, 0x2000}, {0x1010, 0x1020, 0x2000}})), "ok");
}

TEST(ExceptionTableReject, UnwindInfoLocation) {
    EXPECT_EQ(code_of(single(unwind_header(1, 0), {0x1000, 0x1010, 0x2002})), "unwind_info_invalid");  // misaligned
    EXPECT_EQ(code_of(single(unwind_header(1, 0), {0x1000, 0x1010, 0x9000})), "unwind_info_invalid");  // no section
    // Zero-fill: inside .rdata's virtual size but past its file data.
    const PeBuilder zero_fill =
        exception_image({{0x00, unwind_header(1, 0)}}, {{0x1000, 0x1010, 0x2800}}, 0x100, 0x1000);
    EXPECT_EQ(code_of(zero_fill), "unwind_info_invalid");
}

TEST(ExceptionTableReject, UnwindInfoHeader) {
    EXPECT_EQ(code_of(single(unwind_header(0, 0))), "unwind_info_invalid");                       // version 0
    EXPECT_EQ(code_of(single(unwind_header(3, 0))), "unwind_info_invalid");                       // version 3
    EXPECT_EQ(code_of(single(unwind_header(1, 0x8))), "unwind_info_invalid");                     // unknown flag
    EXPECT_EQ(code_of(single(with_u32s(unwind_header(1, kChain | kEHandler), {0x1000, 0x1010, 0x2000}))),
              "unwind_info_invalid");                                                              // chain + handler
    EXPECT_EQ(code_of(single(unwind_header(1, 0, 0x11))), "unwind_info_invalid");                 // prolog > 0x10
    EXPECT_EQ(code_of(single(unwind_header(1, 0, 0x10))), "ok");                                  // prolog == length
}

TEST(ExceptionTableReject, UnwindCodesAndTrailerStayInFileData) {
    // 254 codes: 4 + 508 bytes = exactly the 0x200 bytes of .rdata data.
    EXPECT_EQ(code_of(single(unwind_header(1, 0, 0, 254))), "ok");
    // 255 codes round up to 256 slots: 4 + 512 bytes, past the file data.
    EXPECT_EQ(code_of(single(unwind_header(1, 0, 0, 255))), "unwind_info_invalid");
    // Unwind info in the last 4 bytes; the handler RVA would be past the data.
    const PeBuilder trailer =
        exception_image({{0x1FC, unwind_header(1, kEHandler)}}, {{0x1000, 0x1010, 0x21FC}});
    EXPECT_EQ(code_of(trailer), "unwind_info_invalid");
}

TEST(ExceptionTableReject, HandlerOutsideExecutableSection) {
    EXPECT_EQ(code_of(single(with_u32s(unwind_header(1, kUHandler), {0x2000}))), "exception_handler_invalid");
    EXPECT_EQ(code_of(single(with_u32s(unwind_header(1, kEHandler), {0x9000}))), "exception_handler_invalid");
}

TEST(ExceptionTableReject, ChainedEntries) {
    // Chained function range not executable.
    EXPECT_EQ(code_of(single(with_u32s(unwind_header(1, kChain), {0x2000, 0x2010, 0x2040}))),
              "unwind_chain_invalid");
    // Chained unwind info itself invalid (version 3 at 0x2040).
    const PeBuilder bad_target = exception_image(
        {{0x00, with_u32s(unwind_header(1, kChain), {0x1000, 0x1010, 0x2040})}, {0x40, unwind_header(3, 0)}},
        {{0x1000, 0x1010, 0x2000}});
    EXPECT_EQ(code_of(bad_target), "unwind_info_invalid");
    // A chain that refers back to itself never ends: bounded by kMaxUnwindChainDepth.
    EXPECT_EQ(code_of(single(with_u32s(unwind_header(1, kChain), {0x1000, 0x1010, 0x2000}))),
              "unwind_chain_invalid");
}

TEST(ExceptionTableReject, DirectoryAlignment) {
    const PeBuilder pe = exception_image({{0x00, unwind_header(1, 0)}}, {{0x1000, 0x1010, 0x2000}}, 0x102);
    EXPECT_EQ(code_of(pe), "runtime_function_invalid");
}
