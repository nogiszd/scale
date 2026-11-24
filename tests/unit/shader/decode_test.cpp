#include "scale/shader/decode.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <ostream>
#include <span>
#include <string_view>
#include <variant>

namespace {

using scale::ByteView;
using namespace scale::shader;

ByteView code_bytes(std::span<const std::uint32_t> words) {
    return ByteView(std::as_bytes(words));
}

constexpr Operand sgpr(std::uint16_t index) {
    return {OperandKind::sgpr, index, index};
}

constexpr Operand vgpr(std::uint16_t encoded, std::uint32_t index) {
    return {OperandKind::vgpr, encoded, index};
}

constexpr Operand inline_constant(std::uint16_t encoded, std::uint32_t value) {
    return {OperandKind::inline_constant, encoded, value};
}

constexpr Operand literal(std::uint32_t value) {
    return {OperandKind::literal_constant, 255, value};
}

void expect_operand(const Operand& actual, const Operand& expected) {
    EXPECT_EQ(actual.kind, expected.kind);
    EXPECT_EQ(actual.encoded, expected.encoded);
    EXPECT_EQ(actual.value, expected.value);
}

struct AluVector {
    const char* name;
    Encoding encoding;
    Opcode opcode;
    std::array<std::uint32_t, 3> words;
    std::uint8_t word_count;
    AluOperands operands;
};

void PrintTo(const AluVector& vector, std::ostream* output) {
    *output << vector.name;
}

// Hand-encoded using AMD RDNA 2 ISA reference 70648, tables 10, 66–69,
// 76–79 and 82–83, and sections 12.7.1 and 12.8.1 for e64 opcode bases.
constexpr AluVector kAluVectors[]{
    {"Sop1MovLiteral", Encoding::sop1, Opcode::s_mov_b32, {0xbee903ff, 0x12345678}, 2,
     {sgpr(105), {literal(0x12345678)}, 1}},  // s_mov_b32 s105, 0x12345678
    {"Sop2AddInline", Encoding::sop2, Opcode::s_add_u32, {0x8013d069}, 1,
     {sgpr(19), {sgpr(105), inline_constant(208, 0xfffffff0)}, 2}},  // s_add_u32 s19, s105, -16
    {"SopkSignedImmediate", Encoding::sopk, Opcode::s_movk_i32, {0xb0118000}, 1,
     {sgpr(17), {Operand{OperandKind::immediate, 0x8000, 0xffff8000}}, 1}},  // s_movk_i32 s17, -32768
    {"Vop1Mov", Encoding::vop1, Opcode::v_mov_b32, {0x7e3e03ff}, 1,
     {vgpr(31, 31), {vgpr(511, 255)}, 1}},  // v_mov_b32_e32 v31, v255
    {"Vop3Mov", Encoding::vop3, Opcode::v_mov_b32, {0xd5810023, 0x020100f8}, 2,
     {vgpr(35, 35), {inline_constant(248, 0x3e22f983)}, 1}},  // v_mov_b32_e64 v35, 1/(2π)
    {"Vop2IntAdd", Encoding::vop2, Opcode::v_add_nc_u32, {0x4a27fec0}, 1,
     {vgpr(19, 19), {inline_constant(192, 64), vgpr(255, 255)}, 2}},  // v_add_nc_u32_e32 v19, 64, v255
    {"Vop2LiteralAndV255", Encoding::vop2, Opcode::v_add_nc_u32, {0x4a27feff, 0x87654321}, 2,
     {vgpr(19, 19), {literal(0x87654321), vgpr(255, 255)}, 2}},  // Same field value, different operand types.
    {"Vop3IntAdd", Encoding::vop3, Opcode::v_add_nc_u32, {0xd5250019, 0x02029208}, 2,
     {vgpr(25, 25), {sgpr(8), vgpr(329, 73)}, 2}},  // v_add_nc_u32_e64 v25, s8, v73
    {"Vop3IntMul", Encoding::vop3, Opcode::v_mul_lo_u32, {0xd5690008, 0x0200130a}, 2,
     {vgpr(8, 8), {vgpr(266, 10), sgpr(9)}, 2}},  // v_mul_lo_u32 v8, v10, s9
    {"Vop2FloatAdd", Encoding::vop2, Opcode::v_add_f32, {0x061608f1}, 1,
     {vgpr(11, 11), {inline_constant(241, 0xbf000000), vgpr(4, 4)}, 2}},  // v_add_f32_e32 v11, -0.5, v4
    {"Vop3FloatAddLiteral", Encoding::vop3, Opcode::v_add_f32, {0xd503000c, 0x02020eff, 0x3fa00000}, 3,
     {vgpr(12, 12), {literal(0x3fa00000), vgpr(263, 7)}, 2}},  // v_add_f32_e64 v12, 1.25, v7
    {"Vop2FloatMul", Encoding::vop2, Opcode::v_mul_f32, {0x101206f6}, 1,
     {vgpr(9, 9), {inline_constant(246, 0x40800000), vgpr(3, 3)}, 2}},  // v_mul_f32_e32 v9, 4.0, v3
    {"Vop3FloatMul", Encoding::vop3, Opcode::v_mul_f32, {0xd5080018, 0x0201ef80}, 2,
     {vgpr(24, 24), {vgpr(384, 128), inline_constant(247, 0xc0800000)}, 2}},  // v_mul_f32_e64 v24, v128, -4.0
};

class AluDecode : public testing::TestWithParam<AluVector> {};

TEST_P(AluDecode, DecodesHandEncodedOperands) {
    const auto& vector = GetParam();
    const auto result = decode(code_bytes(std::span(vector.words).first(vector.word_count)));
    ASSERT_TRUE(result.has_value()) << result.error().to_string();
    ASSERT_EQ(result->size(), 1u);
    const auto& instruction = result->front();
    EXPECT_EQ(instruction.encoding, vector.encoding);
    EXPECT_EQ(instruction.opcode, vector.opcode);
    EXPECT_EQ(instruction.dword_offset, 0u);
    EXPECT_EQ(instruction.raw_words, vector.words);
    EXPECT_EQ(instruction.word_count, vector.word_count);
    const auto* alu = std::get_if<AluOperands>(&instruction.operands);
    ASSERT_NE(alu, nullptr);
    expect_operand(alu->destination, vector.operands.destination);
    ASSERT_EQ(alu->source_count, vector.operands.source_count);
    for (std::size_t i = 0; i < alu->source_count; ++i) {
        expect_operand(alu->sources[i], vector.operands.sources[i]);
    }
}

INSTANTIATE_TEST_SUITE_P(Gfx1030, AluDecode, testing::ValuesIn(kAluVectors),
                        [](const testing::TestParamInfo<AluVector>& info) { return info.param.name; });

TEST(Decoder, ReadsOneLiteralForBothSourcesAndContinuesPastEndpgm) {
    constexpr std::uint32_t words[]{
        0x8014ffff, 0x12345678,  // s_add_u32 s20, 0x12345678, 0x12345678
        0xd5250005, 0x0201feff, 0x87654321,  // v_add_nc_u32_e64 v5, 0x87654321, 0x87654321
        0xbf810000,  // s_endpgm
        0xbf8ca23c,  // s_waitcnt vmcnt(44) expcnt(3) lgkmcnt(34), section 12.5
    };
    const auto result = decode(code_bytes(words));
    ASSERT_TRUE(result.has_value()) << result.error().to_string();
    ASSERT_EQ(result->size(), 4u);
    const auto& scalar = (*result)[0];
    EXPECT_EQ(scalar.word_count, 2);
    const auto* scalar_alu = std::get_if<AluOperands>(&scalar.operands);
    ASSERT_NE(scalar_alu, nullptr);
    ASSERT_EQ(scalar_alu->source_count, 2);
    expect_operand(scalar_alu->sources[0], literal(0x12345678));
    expect_operand(scalar_alu->sources[1], literal(0x12345678));
    const auto& vector = (*result)[1];
    EXPECT_EQ(vector.word_count, 3);
    EXPECT_EQ(vector.dword_offset, 2u);
    const auto* vector_alu = std::get_if<AluOperands>(&vector.operands);
    ASSERT_NE(vector_alu, nullptr);
    ASSERT_EQ(vector_alu->source_count, 2);
    expect_operand(vector_alu->sources[0], literal(0x87654321));
    expect_operand(vector_alu->sources[1], literal(0x87654321));
    const auto& end = (*result)[2];
    EXPECT_EQ(end.opcode, Opcode::s_endpgm);
    EXPECT_EQ(end.encoding, Encoding::sopp);
    EXPECT_EQ(end.dword_offset, 5u);
    EXPECT_EQ(end.word_count, 1);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(end.operands));
    const auto& wait = (*result)[3];
    EXPECT_EQ(wait.opcode, Opcode::s_waitcnt);
    EXPECT_EQ(wait.encoding, Encoding::sopp);
    EXPECT_EQ(wait.dword_offset, 6u);
    EXPECT_EQ(wait.word_count, 1);
    const auto* counters = std::get_if<WaitCounters>(&wait.operands);
    ASSERT_NE(counters, nullptr);
    EXPECT_EQ(counters->vmcnt, 44);
    EXPECT_EQ(counters->expcnt, 3);
    EXPECT_EQ(counters->lgkmcnt, 34);
}

TEST(Decoder, OnlyActiveVop3SourcesCanSelectALiteral) {
    constexpr std::uint32_t words[]{0xd5030002, 0x03fe04f2, 0xbf810000};
    const auto result = decode(code_bytes(words));
    ASSERT_TRUE(result.has_value()) << result.error().to_string();
    ASSERT_EQ(result->size(), 2u);
    EXPECT_EQ((*result)[0].word_count, 2);
    EXPECT_EQ((*result)[1].opcode, Opcode::s_endpgm);
    EXPECT_EQ((*result)[1].dword_offset, 2u);
}

TEST(Decoder, ReadsMubufAddressingFields) {
    // Tables 98–99. The decoder preserves fields before ABI validation.
    constexpr std::uint32_t words[]{
        0xe03011a4, 0xc0011314,  // buffer_load_dword v19, v20, s[4:7], 64 offen offset:420
        0xe0701fff, 0x6919fffd,  // buffer_store_dword v255, v253, s[100:103], s105 offen offset:4095
    };
    const auto result = decode(code_bytes(words));
    ASSERT_TRUE(result.has_value()) << result.error().to_string();
    ASSERT_EQ(result->size(), 2u);
    EXPECT_EQ((*result)[0].opcode, Opcode::buffer_load_dword);
    EXPECT_EQ((*result)[1].opcode, Opcode::buffer_store_dword);
    for (std::size_t i = 0; i < result->size(); ++i) {
        const auto& instruction = (*result)[i];
        EXPECT_EQ(instruction.encoding, Encoding::mubuf);
        EXPECT_EQ(instruction.word_count, 2);
        EXPECT_EQ(instruction.dword_offset, i * 2);
        EXPECT_EQ(instruction.raw_words[0], words[i * 2]);
        EXPECT_EQ(instruction.raw_words[1], words[i * 2 + 1]);
    }
    const auto* load = std::get_if<BufferOperands>(&(*result)[0].operands);
    const auto* store = std::get_if<BufferOperands>(&(*result)[1].operands);
    ASSERT_NE(load, nullptr);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(load->data_vgpr, 19);
    EXPECT_EQ(load->address_vgpr, 20);
    EXPECT_EQ(load->resource_sgpr_base, 4);
    EXPECT_EQ(load->immediate_offset_bytes, 420);
    expect_operand(load->scalar_offset_bytes, inline_constant(192, 64));
    EXPECT_EQ(store->data_vgpr, 255);
    EXPECT_EQ(store->address_vgpr, 253);
    EXPECT_EQ(store->resource_sgpr_base, 100);
    EXPECT_EQ(store->immediate_offset_bytes, 4095);
    expect_operand(store->scalar_offset_bytes, sgpr(105));
}

TEST(Decoder, AcceptsEmptyStream) {
    const auto result = decode(ByteView{});
    ASSERT_TRUE(result.has_value()) << result.error().to_string();
    EXPECT_TRUE(result->empty());
}

void expect_error(ByteView bytes, std::string_view code) {
    const auto result = decode(bytes);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().component, "shader");
    EXPECT_EQ(result.error().code, code);
}

void expect_error(std::initializer_list<std::uint32_t> words, std::string_view code) {
    expect_error(code_bytes(words), code);
}

TEST(DecoderReject, TruncatedEncoding) {
    constexpr std::uint32_t words[]{0xbf810000, 0xd503000c, 0x02020eff, 0x3fa00000};
    const auto bytes = code_bytes(words).bytes();
    for (const auto size : {1u, 2u, 3u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u}) {
        SCOPED_TRACE(size);
        expect_error(ByteView(bytes.first(size)), "truncated_encoding");
    }
    expect_error({0xe0301000}, "truncated_encoding");  // Missing MUBUF second word.
    expect_error({0xbee903ff}, "truncated_encoding");  // Missing scalar literal.
    expect_error({0x7e3e02fa}, "truncated_encoding");  // Missing DPP16 second word.
}

TEST(DecoderReject, UnsupportedEncoding) {
    expect_error({0xbf810000, 0xffffffff}, "unsupported_encoding");  // Failure after endpgm.
    expect_error({0xbf820000}, "unsupported_encoding");  // s_branch
    expect_error({0xe0341000, 0x80000201}, "unsupported_encoding");  // buffer_load_dwordx2
}

TEST(DecoderReject, UnsupportedOperand) {
    expect_error({0xbee9037d}, "unsupported_operand");  // null source
    expect_error({0xb06a8000}, "unsupported_operand");  // vcc_lo destination
    expect_error({0x7e3e02d1}, "unsupported_operand");  // reserved source selector
    expect_error({0xe0301000, 0x801a0201}, "unsupported_operand");  // Resource s[104:107].
    expect_error({0xe0301000, 0xff000201}, "unsupported_operand");  // SOFFSET cannot select a literal.
}

TEST(DecoderReject, UnsupportedModifier) {
    expect_error({0xbf810001}, "unsupported_modifier");  // Nonzero s_endpgm immediate.
    expect_error({0xbf8c0080}, "unsupported_modifier");  // Reserved bit in s_waitcnt.
    expect_error({0x7e3e02fa, 0xff00e401}, "unsupported_modifier");  // DPP16.
    expect_error({0xd503010c, 0x02020ef2}, "unsupported_modifier");  // VOP3 ABS.
    expect_error({0xd503000c, 0x22020ef2}, "unsupported_modifier");  // VOP3 NEG.
    expect_error({0xe0300000, 0x80000201}, "unsupported_modifier");  // OFFEN absent.
    expect_error({0xe0303000, 0x80000201}, "unsupported_modifier");  // IDXEN set.
    expect_error({0xe0311000, 0x80000201}, "unsupported_modifier");  // LDS set.
    expect_error({0xe0301000, 0x80800201}, "unsupported_modifier");  // TFE set.
}

}  // namespace
