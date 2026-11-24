#include "cli.h"

#include "disassemble.h"
#include "validator.h"
#include "write_module.h"

#include "scale/diagnostics/json_writer.h"
#include "scale/shader/spirv.h"
#include "scale/shader/test_case.h"

#include <charconv>
#include <cstdio>
#include <exception>
#include <format>

namespace scale::shader::cli {
namespace {

using Args = std::span<const std::string_view>;
using scale::diagnostics::json_quote;

constexpr std::uint64_t kMaxCaseBytes = 16 * 1024 * 1024;
constexpr std::string_view kUsage =
    "usage: scale-shader decode <case.json|words...>\n"
    "       scale-shader compile <case.json> -o <out.spv>\n"
    "       scale-shader validate <out.spv>\n";

void print(FILE* stream, std::string_view text) {
    std::fwrite(text.data(), 1, text.size(), stream);
}

int usage(std::string_view message) {
    print(stderr, std::format("error: {}\n{}", message, kUsage));
    return kExitUsage;
}

int failure(std::string_view stage, std::string_view subject, const Error& error) {
    print(stderr, std::format("error: {}/{}: {} {}: {}\n",
                              error.component, error.code, stage, json_quote(subject), error.message));
    return kExitFailure;
}

std::filesystem::path path_from_utf8(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

Result<std::vector<std::uint32_t>> parse_words(Args args) {
    constexpr std::string_view whitespace = " \t\n\r";
    std::vector<std::uint32_t> words;
    for (auto arg : args) {
        while (true) {
            const auto start = arg.find_first_not_of(whitespace);
            if (start == std::string_view::npos) break;
            arg.remove_prefix(start);
            auto token = arg.substr(0, arg.find_first_of(whitespace));
            arg.remove_prefix(token.size());
            if (token.starts_with("0x") || token.starts_with("0X")) token.remove_prefix(2);
            std::uint32_t word{};
            const auto end = token.data() + token.size();
            const auto parsed = std::from_chars(token.data(), end, word, 16);
            if (token.size() != 8 || parsed.ec != std::errc{} || parsed.ptr != end) {
                return fail("shader", "invalid_word", std::format("word {} must contain eight hex digits", words.size()));
            }
            words.push_back(word);
        }
    }
    if (words.empty()) return fail("shader", "invalid_word", "expected at least one hexadecimal dword");
    return words;
}

Result<std::vector<DecodedInstruction>> decode_words(std::span<const std::uint32_t> words) {
    return scale::shader::decode(ByteView(std::as_bytes(words)));
}

bool has_json_extension(std::string_view argument) {
    if (argument.size() < 5) return false;
    const auto ext = argument.substr(argument.size() - 5);
    constexpr std::string_view lower = ".json";
    constexpr std::string_view upper = ".JSON";
    for (std::size_t i = 0; i < ext.size(); ++i) {
        if (ext[i] != lower[i] && ext[i] != upper[i]) return false;
    }
    return true;
}

int decode_command(Args args) {
    if (args.empty()) return usage("decode requires a JSON case or hexadecimal words");
    const bool is_case = args.size() == 1 && has_json_extension(args.front());
    if (is_case) {
        const auto shader_case = load_test_case(path_from_utf8(args.front()), kMaxCaseBytes);
        if (!shader_case) return failure("testcase", args.front(), shader_case.error());
        const auto decoded = decode_words(shader_case->code_words);
        if (!decoded) return failure("decode", shader_case->name, decoded.error());
        print(stdout, std::format("case {} (ABI v{})\n", json_quote(shader_case->name),
                                  static_cast<unsigned>(shader_case->abi_version())));
        for (const auto& inst : *decoded) print(stdout, disassemble(inst) + '\n');
        return kExitSuccess;
    }

    const auto words = parse_words(args);
    if (!words) return usage(words.error().to_string());
    const auto decoded = decode_words(*words);
    if (!decoded) return failure("decode", "words", decoded.error());
    for (const auto& inst : *decoded) print(stdout, disassemble(inst) + '\n');
    return kExitSuccess;
}

int compile_command(Args args) {
    if (args.size() != 3 || args[0].empty() || args[1] != "-o" || args[2].empty()) {
        return usage("compile requires a JSON case and -o <out.spv>");
    }
    const auto input = path_from_utf8(args[0]);
    const auto shader_case = load_test_case(input, kMaxCaseBytes);
    if (!shader_case) return failure("testcase", args[0], shader_case.error());
    const auto decoded = decode_words(shader_case->code_words);
    if (!decoded) return failure("decode", shader_case->name, decoded.error());
    const auto module = [&]() -> Result<SpirvModule> {
        if (shader_case->v1) {
            const auto ir = lower_ir_v1(*decoded, shader_case->v1->initial_exec);
            if (!ir) return std::unexpected(ir.error());
            return emit_spirv(*ir);
        }
        const auto ir = lower_ir(*decoded);
        if (!ir) return std::unexpected(ir.error());
        return emit_spirv(*ir);
    }();
    if (!module) return failure("compile", shader_case->name, module.error());

    const auto result = write_module(input, path_from_utf8(args[2]), module->words);
    if (!result) return failure("write", args[2], result.error());
    print(stdout, std::format("compiled {} (ABI v{}) to {}\n", json_quote(shader_case->name),
                              static_cast<unsigned>(shader_case->abi_version()), json_quote(args[2])));
    return kExitSuccess;
}

int validate_command(Args args) {
    if (args.size() != 1 || args.front().empty()) return usage("validate requires one SPIR-V file");
    std::error_code error;
    const auto input = std::filesystem::absolute(path_from_utf8(args.front()), error);
    if (error || !std::filesystem::is_regular_file(input, error)) {
        return failure("validate", args.front(), Error{"shader", "spirv_open_failed", "expected an existing module file"});
    }
    const auto validation = run_validator(input);
    if (!validation) return failure("validate", args.front(), validation.error());
    if (!*validation) {
        print(stderr, "SKIPPED: spirv-val is unavailable on PATH or in VULKAN_SDK\n");
        return kExitFailure;
    }
    if (**validation != 0) {
        return failure("validate", args.front(), Error{"shader", "validator_rejected",
                       std::format("spirv-val exited with code {}", **validation)});
    }
    print(stdout, std::format("validated {} for Vulkan 1.2\n", json_quote(args.front())));
    return kExitSuccess;
}

int dispatch(Args args) {
    if (args.empty()) return usage("missing command");
    const auto command = args.front();
    args = args.subspan(1);
    if (command == "decode") return decode_command(args);
    if (command == "compile") return compile_command(args);
    if (command == "validate") return validate_command(args);
    if ((command == "help" || command == "--help" || command == "-h") && args.empty()) {
        print(stdout, kUsage);
        return kExitSuccess;
    }
    return usage(std::format("invalid command {}", json_quote(command)));
}

}  // namespace

int run(Args args) noexcept {
    try {
        return dispatch(args);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "internal error: %s\n", error.what());
        return kExitFailure;
    }
}

}  // namespace scale::shader::cli
