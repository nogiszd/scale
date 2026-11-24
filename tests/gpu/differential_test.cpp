// T-720: differential runner. For every ABI v0 test case:
//   decode -> (a) independent reference interpreter (scale::shader_ref)
//          -> (b) IR -> SPIR-V -> Vulkan dispatch on every device
// Both are compared with hand-computed expected values and with each other.
// Instruction words come from llvm-mc 23.1.6 (gfx1030). The output buffer
// starts as 0xDEADBEEF so that unwritten words are detected.
//
// Cases are inline until the shared JSON test-case library (T-705) lands.

#include "scale/graphics/vulkan_compute.h"
#include "scale/shader/decode.h"
#include "scale/shader/ir.h"
#include "scale/shader/spirv.h"
#include "scale/shader_ref/interpreter.h"

#include <gtest/gtest.h>

#include <bit>
#include <format>
#include <iostream>
#include <span>
#include <string>
#include <vector>

using scale::shader_ref::BufferWords;
using scale::shader_ref::kBufferWords;

namespace {

constexpr std::uint32_t kSentinel = 0xDEADBEEFu;

struct DiffCase {
    std::string name;
    std::vector<std::uint32_t> code;
    BufferWords input{};
    BufferWords expected{};
};

std::uint32_t f32(float f) {
    return std::bit_cast<std::uint32_t>(f);
}

std::vector<DiffCase> cases() {
    std::vector<DiffCase> all;

    {  // T-704 contract example: out[i] = i + in[i] (mod 2^32); lane 1 wraps.
        DiffCase c{"t704_add_lane_to_input",
                   {0xE0301000, 0x80000201, 0xBF8C3F70, 0x4A040500, 0xE0701000, 0x80010201, 0xBF810000}};
        for (std::uint32_t i = 0; i < kBufferWords; ++i) c.input[i] = i;
        c.input[1] = 0xFFFFFFFFu;
        c.expected = {0x00, 0x00, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e,
                      0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3a, 0x3c, 0x3e};
        all.push_back(c);
    }
    {  // f32: out = -4.0 * (1.0 + in). in[i] = i + 1.5 (exact) -> -(4i + 10) exactly.
       // Lane 31: in = 2^-24, 1.0 + 2^-24 is an RNE tie -> 1.0, then -4.0.
        DiffCase c{"f32_add_mul_rne",
                   {0xE0301000, 0x80000201, 0xBF8C3F70, 0x060604F2, 0x100806F7, 0xE0701000, 0x80010401, 0xBF810000}};
        for (std::uint32_t i = 0; i < 31; ++i) {
            c.input[i] = f32(static_cast<float>(i) + 1.5f);
            c.expected[i] = f32(-static_cast<float>(4 * i + 10));  // small integers: exact
        }
        c.input[31] = 0x33800000u;     // 2^-24
        c.expected[31] = 0xC0800000u;  // -4.0
        all.push_back(c);
    }
    {  // Scalar literal + s_add, SGPR broadcast, v_mul_lo, v_add:
       // out[i] = (0x12345678 + 0x80000000) + i*i = 0x92345678 + i*i.
        DiffCase c{"scalar_broadcast_mul_lo",
                   {0xBE8803FF, 0x12345678, 0x8009FF08, 0x80000000, 0x7E040209, 0xD5690003, 0x02020100, 0x4A080702,
                    0xE0701000, 0x80010401, 0xBF810000}};
        for (std::uint32_t i = 0; i < kBufferWords; ++i) c.expected[i] = 0x92345678u + i * i;
        all.push_back(c);
    }
    {  // Double wrap: in[i] = 0xFFFFFFFF - i; (i + in[i]) = 0xFFFFFFFF; + 1 -> 0.
        DiffCase c{"u32_double_wrap",
                   {0xE0301000, 0x80000201, 0xBF8C3F70, 0x4A040500, 0x4A040481, 0xE0701000, 0x80010201, 0xBF810000}};
        for (std::uint32_t i = 0; i < kBufferWords; ++i) {
            c.input[i] = 0xFFFFFFFFu - i;
            c.expected[i] = 0;
        }
        all.push_back(c);
    }
    return all;
}

std::string diff(const BufferWords& expected, std::span<const std::uint32_t> actual) {
    std::string out;
    for (std::size_t i = 0; i < expected.size() && i < actual.size(); ++i) {
        if (expected[i] != actual[i]) out += std::format(" [{}] expected 0x{:08x} got 0x{:08x}", i, expected[i], actual[i]);
    }
    return out;
}

}  // namespace

TEST(Differential, InterpreterAndGpuAgreeWithExpectedValues) {
    const auto devices = scale::graphics::enumerate_devices();
    std::vector<scale::graphics::DeviceInfo> usable;
    if (devices) {
        for (const auto& d : *devices) {
            std::cout << "[device " << d.index << "] " << d.fingerprint() << (d.is_software() ? " [software_gpu]" : "")
                      << "\n";
            if (d.supported) usable.push_back(d);
        }
    } else {
        std::cout << "Vulkan unavailable: " << devices.error().to_string() << "\n";
    }

    std::size_t gpu_runs = 0;
    const BufferWords initial = [] {
        BufferWords w{};
        w.fill(kSentinel);
        return w;
    }();

    for (const DiffCase& c : cases()) {
        SCOPED_TRACE(c.name);
        const auto decoded = scale::shader::decode(scale::ByteView(std::as_bytes(std::span(c.code))));
        ASSERT_TRUE(decoded.has_value()) << decoded.error().to_string();

        // (a) independent reference interpreter
        const auto ref = scale::shader_ref::run_abi_v0(*decoded, c.input);
        ASSERT_TRUE(ref.has_value()) << ref.error().to_string();
        EXPECT_EQ(ref->output, c.expected) << "interpreter:" << diff(c.expected, ref->output);
        EXPECT_EQ(ref->output_written, 0xFFFFFFFFu);

        // (b) compiler: IR -> SPIR-V
        const auto ir = scale::shader::lower_ir(*decoded);
        ASSERT_TRUE(ir.has_value()) << ir.error().to_string();
        const auto module = scale::shader::emit_spirv(*ir);
        ASSERT_TRUE(module.has_value()) << module.error().to_string();

        for (const auto& info : usable) {
            if (module->requires_float32_rte && !info.rounding_mode_rte_f32) {
                std::cout << "SKIPPED " << c.name << " on device " << info.index
                          << ": shaderRoundingModeRTEFloat32 not supported\n";
                continue;
            }
            auto device = scale::graphics::ComputeDevice::open(info.index);
            ASSERT_TRUE(device.has_value()) << device.error().to_string();
            const auto gpu = device->dispatch({module->words, c.input, initial, 1});
            ASSERT_TRUE(gpu.has_value()) << info.fingerprint() << ": " << gpu.error().to_string();
            ASSERT_EQ(gpu->size(), kBufferWords);
            const std::string vs_expected = diff(c.expected, *gpu);
            const std::string vs_interpreter = diff(ref->output, *gpu);
            EXPECT_EQ(vs_expected, "") << "GPU vs expected on " << info.fingerprint();
            EXPECT_EQ(vs_interpreter, "") << "GPU vs interpreter on " << info.fingerprint();
            ++gpu_runs;
            std::cout << (vs_expected.empty() && vs_interpreter.empty() ? "PASS " : "FAIL ") << c.name
                      << " on device " << info.index << " (" << info.name << ")\n";
        }
    }
    if (gpu_runs == 0) {
        GTEST_SKIP() << "interpreter checks ran, but no usable Vulkan device executed the translated shaders";
    }
}
