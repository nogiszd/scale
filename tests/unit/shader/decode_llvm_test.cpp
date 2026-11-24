#include "scale/common/text.h"
#include "scale/shader/decode.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace scale::shader;

// Test program words from the T-704 ABI contract. LLVM independently assembles
// and disassembles tests/shader/add_lane_to_input.s.
constexpr std::uint32_t kExampleWords[]{
    0xe0301000, 0x80000201, 0xbf8c3f70, 0x4a040500,
    0xe0701000, 0x80010201, 0xbf810000,
};

std::string without_whitespace(std::string_view text) {
    std::string result;
    for (char ch : text) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            result += ch;
        }
    }
    return result;
}

std::string operand_text(const Operand& operand) {
    switch (operand.kind) {
    case OperandKind::sgpr: return std::format("s{}", operand.value);
    case OperandKind::vgpr: return std::format("v{}", operand.value);
    case OperandKind::inline_constant: {
        if (operand.encoded < 240) {
            return std::to_string(std::bit_cast<std::int32_t>(operand.value));
        }
        auto text = std::format("{}", std::bit_cast<float>(operand.value));
        if (text.find_first_of(".eE") == std::string::npos) text += ".0";
        return text;
    }
    case OperandKind::literal_constant: return std::format("0x{:x}", operand.value);
    case OperandKind::immediate: {
        // LLVM prints s_movk_i32 as a 16-bit hexadecimal immediate.
        const auto low = static_cast<std::uint16_t>(operand.value);
        EXPECT_EQ(low, operand.encoded);
        EXPECT_EQ(operand.value, static_cast<std::uint32_t>(std::bit_cast<std::int16_t>(low)));
        return std::format("0x{:x}", low);
    }
    }
    return "<unexpected operand>";
}

std::string alu_text(const DecodedInstruction& instruction, std::string_view mnemonic, Encoding base_encoding) {
    std::string_view suffix;
    if (base_encoding == Encoding::vop1 || base_encoding == Encoding::vop2) {
        EXPECT_TRUE(instruction.encoding == base_encoding || instruction.encoding == Encoding::vop3);
        suffix = instruction.encoding == Encoding::vop3 ? "_e64" : "_e32";
    } else {
        EXPECT_EQ(instruction.encoding, base_encoding);
    }
    const auto& alu = std::get<AluOperands>(instruction.operands);
    if (alu.source_count > alu.sources.size()) return "<invalid source count>";
    auto text = std::format("{}{} {}", mnemonic, suffix, operand_text(alu.destination));
    for (const auto& source : std::span(alu.sources).first(alu.source_count)) {
        text += ", " + operand_text(source);
    }
    return text;
}

std::string instruction_text(const DecodedInstruction& instruction) {
    switch (instruction.opcode) {
    case Opcode::s_mov_b32: return alu_text(instruction, "s_mov_b32", Encoding::sop1);
    case Opcode::s_add_u32: return alu_text(instruction, "s_add_u32", Encoding::sop2);
    case Opcode::s_movk_i32: return alu_text(instruction, "s_movk_i32", Encoding::sopk);
    case Opcode::v_mov_b32: return alu_text(instruction, "v_mov_b32", Encoding::vop1);
    case Opcode::v_add_f32: return alu_text(instruction, "v_add_f32", Encoding::vop2);
    case Opcode::v_mul_f32: return alu_text(instruction, "v_mul_f32", Encoding::vop2);
    case Opcode::v_add_nc_u32: return alu_text(instruction, "v_add_nc_u32", Encoding::vop2);
    case Opcode::v_mul_lo_u32: return alu_text(instruction, "v_mul_lo_u32", Encoding::vop3);
    case Opcode::buffer_load_dword:
    case Opcode::buffer_store_dword: {
        const auto& buffer = std::get<BufferOperands>(instruction.operands);
        EXPECT_EQ(instruction.encoding, Encoding::mubuf);
        const auto* mnemonic = instruction.opcode == Opcode::buffer_load_dword ?
            "buffer_load_dword" : "buffer_store_dword";
        auto text = std::format("{} v{}, v{}, s[{}:{}], {} offen", mnemonic, buffer.data_vgpr,
                                buffer.address_vgpr, buffer.resource_sgpr_base, buffer.resource_sgpr_base + 3,
                                operand_text(buffer.scalar_offset_bytes));
        if (buffer.immediate_offset_bytes != 0) {
            text += std::format(" offset:{}", buffer.immediate_offset_bytes);
        }
        return text;
    }
    case Opcode::s_waitcnt: {
        const auto& counters = std::get<WaitCounters>(instruction.operands);
        EXPECT_EQ(instruction.encoding, Encoding::sopp);
        std::string text = "s_waitcnt";
        if (counters.vmcnt != 63) text += std::format(" vmcnt({})", counters.vmcnt);
        if (counters.expcnt != 7) text += std::format(" expcnt({})", counters.expcnt);
        if (counters.lgkmcnt != 63) text += std::format(" lgkmcnt({})", counters.lgkmcnt);
        return text;
    }
    case Opcode::s_endpgm:
        EXPECT_EQ(instruction.encoding, Encoding::sopp);
        EXPECT_TRUE(std::holds_alternative<std::monostate>(instruction.operands));
        return "s_endpgm";
    }
    return "<unexpected opcode>";
}

struct ReferenceInstruction {
    std::string text;
    std::uint64_t address{};
    std::vector<std::uint32_t> words;
};

void expect_disassembly(std::string_view test_program, std::size_t instruction_count,
                        std::span<const std::uint32_t> contract_words = {}) {
    constexpr std::string_view reference_dir = SCALE_SHADER_LLVM_REFERENCE_DIR;
    if (reference_dir.empty()) {
        GTEST_SKIP() << "llvm-mc and llvm-objdump with AMDGPU were unavailable at configure time";
    }

    const auto reference_path = std::format("{}/{}.disasm", reference_dir, test_program);
    const auto path = scale::to_wide(reference_path);
    ASSERT_TRUE(path.has_value()) << path.error().to_string();
    std::ifstream file{std::filesystem::path(*path)};
    ASSERT_TRUE(file.is_open()) << reference_path;
    std::ostringstream output;
    output << file.rdbuf();
    const auto disassembly = output.str();

    std::istringstream listing(disassembly);
    std::vector<ReferenceInstruction> reference;
    std::vector<std::uint32_t> words;
    for (std::string line; std::getline(listing, line);) {
        const auto comment = line.find("//");
        if (comment == std::string::npos) continue;
        SCOPED_TRACE(line);

        ReferenceInstruction expected{without_whitespace(std::string_view(line).substr(0, comment))};
        std::istringstream raw(line.substr(comment + 2));
        char separator = '\0';
        ASSERT_TRUE(static_cast<bool>(raw >> std::hex >> expected.address >> separator));
        ASSERT_EQ(separator, ':');
        for (std::uint32_t word; raw >> word;) {
            expected.words.push_back(word);
            words.push_back(word);
        }
        ASSERT_TRUE(raw.eof());
        ASSERT_FALSE(expected.words.empty());
        ASSERT_LE(expected.words.size(), 3u);
        reference.push_back(std::move(expected));
    }
    ASSERT_EQ(reference.size(), instruction_count) << disassembly;
    if (!contract_words.empty()) {
        ASSERT_TRUE(std::ranges::equal(words, contract_words)) << disassembly;
    }

    const auto decoded = decode(scale::ByteView(std::as_bytes(std::span(words))));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();
    ASSERT_EQ(decoded->size(), reference.size());
    for (std::size_t i = 0; i < reference.size(); ++i) {
        const auto& expected = reference[i];
        const auto& instruction = (*decoded)[i];
        SCOPED_TRACE(std::format("{} at 0x{:x}", test_program, expected.address));
        EXPECT_EQ(without_whitespace(instruction_text(instruction)), expected.text);
        EXPECT_EQ(instruction.dword_offset * sizeof(std::uint32_t), expected.address);
        ASSERT_EQ(instruction.word_count, expected.words.size());
        const auto decoded_words = std::span(instruction.raw_words).first(instruction.word_count);
        EXPECT_TRUE(std::ranges::equal(decoded_words, expected.words));
    }
}

TEST(ShaderLlvm, T704ExampleAgreesWithDisassembly) {
    expect_disassembly("add_lane_to_input", 5, kExampleWords);
}

TEST(ShaderLlvm, FullSubsetAgreesWithDisassembly) {
    expect_disassembly("decoder_subset", 33);
}

}  // namespace
