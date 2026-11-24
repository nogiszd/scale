// T-601: Vulkan compute harness on every available device
// (docs/contracts/gpu_targets.md). A missing Vulkan loader, or no supported
// device, makes the test SKIPPED, never PASS.

#include "scale/common/text.h"
#include "scale/graphics/vulkan_compute.h"

#include <gtest/gtest.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace scale::graphics;

namespace {

std::vector<std::uint32_t> read_spirv(const char* utf8_path) {
    const auto wide = scale::to_wide(utf8_path);
    std::ifstream in(std::filesystem::path(wide ? *wide : std::wstring()), std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::uint32_t> words(bytes.size() / 4);
    std::memcpy(words.data(), bytes.data(), words.size() * 4);
    return words;
}

std::vector<DeviceInfo> supported_devices_or_skip(bool& skipped) {
    skipped = false;
    const auto devices = enumerate_devices();
    if (!devices) {
        skipped = true;
        std::cout << "Vulkan unavailable: " << devices.error().to_string() << "\n";
        return {};
    }
    std::vector<DeviceInfo> supported;
    for (const DeviceInfo& d : *devices) {
        std::cout << "[device " << d.index << "] " << d.fingerprint()
                  << (d.is_software() ? " [software_gpu]" : "")
                  << (d.supported ? "" : " [unsupported: " + d.unsupported_reason + "]") << "\n";
        if (d.supported) supported.push_back(d);
    }
    if (supported.empty()) skipped = true;
    return supported;
}

}  // namespace

TEST(VulkanCompute, EnumeratesDevicesWithFingerprints) {
    bool skipped = false;
    const auto devices = supported_devices_or_skip(skipped);
    if (skipped) GTEST_SKIP() << "no supported Vulkan device";
    for (const DeviceInfo& d : devices) {
        EXPECT_FALSE(d.name.empty());
        EXPECT_GE(d.api_version, VK_MAKE_API_VERSION(0, 1, 2, 0));
        EXPECT_GT(d.subgroup_size, 0u);
        EXPECT_NE(d.fingerprint().find(d.name), std::string::npos);
    }
}

TEST(VulkanCompute, KnownShaderOnEveryDevice) {
    bool skipped = false;
    const auto devices = supported_devices_or_skip(skipped);
    if (skipped) GTEST_SKIP() << "no supported Vulkan device";
    const auto spirv = read_spirv(SCALE_TEST_SPIRV_SCALE3);
    ASSERT_FALSE(spirv.empty()) << "compiled shader missing: " << SCALE_TEST_SPIRV_SCALE3;

    // 2 workgroups x 32 invocations; inputs near 2^32 exercise wraparound.
    constexpr std::uint32_t kCount = 64;
    std::vector<std::uint32_t> input(kCount);
    for (std::uint32_t i = 0; i < kCount; ++i) input[i] = 0xFFFFFFF0u + i * 7u;
    const std::vector<std::uint32_t> initial(kCount, 0xDEADBEEFu);

    for (const DeviceInfo& info : devices) {
        SCOPED_TRACE(info.fingerprint());
        auto device = ComputeDevice::open(info.index);
        ASSERT_TRUE(device.has_value()) << device.error().to_string();
        const auto out = device->dispatch({spirv, input, initial, 2});
        ASSERT_TRUE(out.has_value()) << out.error().to_string();
        ASSERT_EQ(out->size(), kCount);
        for (std::uint32_t i = 0; i < kCount; ++i) {
            const std::uint32_t expected = input[i] * 3u + i;  // u32 arithmetic wraps modulo 2^32
            EXPECT_EQ((*out)[i], expected) << "word " << i;
        }
        std::cout << "PASS on device " << info.index << ": " << info.name << "\n";
    }
}

TEST(VulkanCompute, RejectsBadRequests) {
    bool skipped = false;
    const auto devices = supported_devices_or_skip(skipped);
    if (skipped) GTEST_SKIP() << "no supported Vulkan device";
    auto device = ComputeDevice::open(devices.front().index);
    ASSERT_TRUE(device.has_value());
    const std::vector<std::uint32_t> words(4, 0);
    EXPECT_FALSE(device->dispatch({{}, words, words, 1}).has_value());
    EXPECT_FALSE(device->dispatch({words, words, words, 0}).has_value());

    const auto out_of_range = ComputeDevice::open(1000);
    ASSERT_FALSE(out_of_range.has_value());
    EXPECT_EQ(out_of_range.error().code, "unsupported_device");
}
