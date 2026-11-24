// T-721: data-driven differential runner. Every tests/shader/cases/*.json test
// case (docs/contracts/shader_abi.md, loaded by scale::shader_testcase) is one
// CTest test:
//   decode -> (a) independent reference interpreter (scale::shader_ref,
//                 run_abi_v0 or run_abi_v1)
//          -> (b) IR -> SPIR-V -> Vulkan dispatch on every usable device
// Both are compared with the case's manual expected words and with each other,
// bit for bit. Neither implementation supplies its own expected values.

#include "scale/graphics/vulkan_compute.h"
#include "scale/shader/decode.h"
#include "scale/shader/ir.h"
#include "scale/shader/spirv.h"
#include "scale/shader/test_case.h"
#include "scale/shader_ref/interpreter.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <iostream>
#include <ostream>
#include <span>
#include <string>
#include <vector>

#ifndef SCALE_TEST_SHADER_CASES
#error "SCALE_TEST_SHADER_CASES must name the tests/shader/cases directory"
#endif

using scale::shader::ShaderTestAbiVersion;
using scale::shader_ref::BufferWords;
using scale::shader_ref::kBufferWords;

namespace {

constexpr std::uint64_t kMaxCaseBytes = 64 * 1024;
// Guards against a missing or emptied case directory passing as "no tests".
constexpr std::size_t kMinimumCases = 10;

// The parameter prints as the case name, so CTest names stay short and
// machine-independent (gtest_discover_tests uses the printed value).
struct CaseFile {
    std::filesystem::path path;
    friend void PrintTo(const CaseFile& c, std::ostream* os) { *os << c.path.stem().string(); }
};

std::vector<CaseFile> case_files() {
    std::vector<CaseFile> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(SCALE_TEST_SHADER_CASES, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back({entry.path()});
    }
    std::ranges::sort(files, {}, &CaseFile::path);
    return files;
}

const std::vector<scale::graphics::DeviceInfo>& usable_devices() {
    static const std::vector<scale::graphics::DeviceInfo> usable = [] {
        std::vector<scale::graphics::DeviceInfo> out;
        const auto devices = scale::graphics::enumerate_devices();
        if (!devices) {
            std::cout << "Vulkan unavailable: " << devices.error().to_string() << "\n";
            return out;
        }
        for (const auto& d : *devices) {
            std::cout << "[device " << d.index << "] " << d.fingerprint() << (d.is_software() ? " [software_gpu]" : "")
                      << "\n";
            if (d.supported) out.push_back(d);
        }
        return out;
    }();
    return usable;
}

BufferWords to_buffer(const scale::shader::ShaderTestLaneWords& w) {
    BufferWords b{};
    std::ranges::copy(w, b.begin());
    return b;
}

std::string diff(const BufferWords& expected, std::span<const std::uint32_t> actual) {
    std::string out;
    for (std::size_t i = 0; i < expected.size() && i < actual.size(); ++i) {
        if (expected[i] != actual[i]) out += std::format(" [{}] expected 0x{:08x} got 0x{:08x}", i, expected[i], actual[i]);
    }
    return out;
}

class Differential : public testing::TestWithParam<CaseFile> {};

}  // namespace

TEST(DifferentialCases, DirectoryHasAllCases) {
    const auto files = case_files();
    EXPECT_GE(files.size(), kMinimumCases) << "case directory: " << SCALE_TEST_SHADER_CASES;
}

TEST_P(Differential, InterpreterAndGpuAgreeWithExpectedValues) {
    const auto loaded = scale::shader::load_test_case(GetParam().path, kMaxCaseBytes);
    ASSERT_TRUE(loaded.has_value()) << GetParam().path.string() << ": " << loaded.error().to_string();
    const scale::shader::ShaderTestCase& c = *loaded;
    EXPECT_EQ(c.name, GetParam().path.stem().string()) << "case name must match its file name";
    const bool v1 = c.abi_version() == ShaderTestAbiVersion::v1;
    std::cout << "case " << c.name << ": abi v" << (v1 ? 1 : 0) << ", seed "
              << (c.seed ? std::to_string(*c.seed) : std::string("null (manual)")) << "\n";

    const BufferWords input = to_buffer(c.input_words);
    const BufferWords expected = to_buffer(c.expected_output_words);
    const BufferWords initial_output = v1 ? to_buffer(c.v1->initial_output_words) : [] {
        BufferWords w{};
        w.fill(0xDEADBEEFu);  // v0: every word must be written; detects unwritten words
        return w;
    }();

    const auto decoded = scale::shader::decode(scale::ByteView(std::as_bytes(std::span(c.code_words))));
    ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();

    // (a) independent reference interpreter
    const auto ref = v1 ? scale::shader_ref::run_abi_v1(*decoded, input, {c.v1->initial_exec, initial_output})
                        : scale::shader_ref::run_abi_v0(*decoded, input);
    ASSERT_TRUE(ref.has_value()) << ref.error().to_string();
    EXPECT_EQ(diff(expected, ref->output), "") << "interpreter vs expected";
    if (!v1) {
        EXPECT_EQ(ref->output_written, 0xFFFFFFFFu);
    }

    // (b) compiler: IR -> SPIR-V. The interpreter run above is the contract's
    // pre-dispatch check of executed reads, waits and the float domain.
    // ABI v1 compiles initial_exec into the module; the initial output words
    // go through the output binding.
    const auto module = [&]() -> scale::Result<scale::shader::SpirvModule> {
        if (v1) {
            const auto ir = scale::shader::lower_ir_v1(*decoded, c.v1->initial_exec);
            if (!ir) return std::unexpected(ir.error());
            return scale::shader::emit_spirv(*ir);
        }
        const auto ir = scale::shader::lower_ir(*decoded);
        if (!ir) return std::unexpected(ir.error());
        return scale::shader::emit_spirv(*ir);
    }();
    ASSERT_TRUE(module.has_value()) << module.error().to_string();

    std::size_t gpu_runs = 0;
    for (const auto& info : usable_devices()) {
        if (module->requires_float32_rte && !info.rounding_mode_rte_f32) {
            std::cout << "SKIPPED " << c.name << " on device " << info.index
                      << ": shaderRoundingModeRTEFloat32 not supported\n";
            continue;
        }
        auto device = scale::graphics::ComputeDevice::open(info.index);
        ASSERT_TRUE(device.has_value()) << device.error().to_string();
        const auto gpu = device->dispatch({module->words, input, initial_output, 1});
        ASSERT_TRUE(gpu.has_value()) << info.fingerprint() << ": " << gpu.error().to_string();
        ASSERT_EQ(gpu->size(), kBufferWords);
        const std::string vs_expected = diff(expected, *gpu);
        const std::string vs_interpreter = diff(ref->output, *gpu);
        EXPECT_EQ(vs_expected, "") << "GPU vs expected on " << info.fingerprint();
        EXPECT_EQ(vs_interpreter, "") << "GPU vs interpreter on " << info.fingerprint();
        ++gpu_runs;
        std::cout << (vs_expected.empty() && vs_interpreter.empty() ? "PASS " : "FAIL ") << c.name << " on device "
                  << info.index << " (" << info.name << ")\n";
    }
    if (gpu_runs == 0) {
        GTEST_SKIP() << "interpreter checks passed, but no usable Vulkan device executed the translated shader";
    }
}

INSTANTIATE_TEST_SUITE_P(Cases, Differential, testing::ValuesIn(case_files()),
                         [](const testing::TestParamInfo<CaseFile>& info) {
                             std::string name = info.param.path.stem().string();
                             std::ranges::replace_if(name, [](char ch) { return !std::isalnum(static_cast<unsigned char>(ch)); }, '_');
                             return name;
                         });
