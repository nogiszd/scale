#include "scale/common/text.h"
#include "scale/shader/decode.h"

#include <gtest/gtest.h>

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

namespace {

using namespace scale::shader;

// Fixture words from the T-704 ABI contract. LLVM independently assembles
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
    case OperandKind::inline_constant: return std::to_string(operand.value);
    default: return "<unexpected operand>";
    }
}

// Format only the T-704 example's instruction forms. Compare resolved
// operands as well as bytes so a decoder that just copied the words fails.
std::string instruction_text(const DecodedInstruction& instruction) {
    switch (instruction.opcode) {
    case Opcode::buffer_load_dword:
    case Opcode::buffer_store_dword: {
        const auto& buffer = std::get<BufferOperands>(instruction.operands);
        EXPECT_EQ(instruction.encoding, Encoding::mubuf);
        EXPECT_EQ(buffer.immediate_offset_bytes, 0);
        EXPECT_EQ(buffer.scalar_offset_bytes.kind, OperandKind::inline_constant);
        const auto* mnemonic = instruction.opcode == Opcode::buffer_load_dword ?
            "buffer_load_dword" : "buffer_store_dword";
        return std::format("{} v{}, v{}, s[{}:{}], {} offen", mnemonic, buffer.data_vgpr,
                           buffer.address_vgpr, buffer.resource_sgpr_base, buffer.resource_sgpr_base + 3,
                           operand_text(buffer.scalar_offset_bytes));
    }
    case Opcode::s_waitcnt: {
        const auto& counters = std::get<WaitCounters>(instruction.operands);
        EXPECT_EQ(instruction.encoding, Encoding::sopp);
        EXPECT_EQ(counters.expcnt, 7);
        EXPECT_EQ(counters.lgkmcnt, 63);
        return std::format("s_waitcnt vmcnt({})", counters.vmcnt);
    }
    case Opcode::v_add_nc_u32: {
        const auto& alu = std::get<AluOperands>(instruction.operands);
        EXPECT_EQ(instruction.encoding, Encoding::vop2);
        EXPECT_EQ(alu.source_count, 2);
        return std::format("v_add_nc_u32_e32 {}, {}, {}", operand_text(alu.destination),
                           operand_text(alu.sources[0]), operand_text(alu.sources[1]));
    }
    case Opcode::s_endpgm:
        EXPECT_EQ(instruction.encoding, Encoding::sopp);
        EXPECT_TRUE(std::holds_alternative<std::monostate>(instruction.operands));
        return "s_endpgm";
    default: return "<unexpected opcode>";
    }
}

TEST(ShaderLlvm, T704ExampleAgreesWithDisassembly) {
    constexpr std::string_view reference_path = SCALE_SHADER_LLVM_REFERENCE;
    if (reference_path.empty()) {
        GTEST_SKIP() << "llvm-mc and llvm-objdump with AMDGPU were unavailable at configure time";
    }

    const auto path = scale::to_wide(reference_path);
    ASSERT_TRUE(path.has_value()) << path.error().to_string();
    std::ifstream file{std::filesystem::path(*path)};
    ASSERT_TRUE(file.is_open()) << reference_path;
    std::ostringstream output;
    output << file.rdbuf();
    const auto disassembly = output.str();

    const auto decoded = decode(scale::ByteView(std::as_bytes(std::span(kExampleWords))));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();
    ASSERT_EQ(decoded->size(), 5u);

    std::istringstream listing(disassembly);
    std::size_t instruction_index = 0;
    std::size_t word_index = 0;
    for (std::string line; std::getline(listing, line);) {
        const auto comment = line.find("//");
        if (comment == std::string::npos) continue;
        SCOPED_TRACE(line);
        ASSERT_LT(instruction_index, decoded->size());
        const auto& instruction = (*decoded)[instruction_index++];
        EXPECT_EQ(without_whitespace(std::string_view(line).substr(0, comment)),
                  without_whitespace(instruction_text(instruction)));

        std::istringstream raw(line.substr(comment + 2));
        std::uint64_t address = 0;
        char separator = '\0';
        ASSERT_TRUE(static_cast<bool>(raw >> std::hex >> address >> separator));
        ASSERT_EQ(separator, ':');
        EXPECT_EQ(address, instruction.dword_offset * sizeof(std::uint32_t));
        std::size_t words_in_instruction = 0;
        for (std::uint32_t word; raw >> word;) {
            ASSERT_LT(words_in_instruction, instruction.word_count);
            ASSERT_LT(word_index, std::size(kExampleWords));
            EXPECT_EQ(word, instruction.raw_words[words_in_instruction++]);
            EXPECT_EQ(word, kExampleWords[word_index++]);
        }
        EXPECT_TRUE(raw.eof());
        EXPECT_EQ(words_in_instruction, instruction.word_count);
    }
    EXPECT_EQ(instruction_index, decoded->size()) << disassembly;
    EXPECT_EQ(word_index, std::size(kExampleWords)) << disassembly;
}

}  // namespace
