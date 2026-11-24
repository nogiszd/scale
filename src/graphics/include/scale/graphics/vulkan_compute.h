#pragma once

// Minimal Vulkan compute harness (T-601). Host graphics plumbing only: it
// runs a SPIR-V compute shader with two storage buffers and reads the result
// back. It is not D3D12.X support and knows nothing about console objects.
//
// Contract for the shader (matches shader ABI v0/v1 bindings):
//   descriptor set 0, binding 0: input storage buffer
//   descriptor set 0, binding 1: output storage buffer
//   entry point "main", dispatched as (groups_x, 1, 1)
//
// Portable: uses only the Vulkan API (no OS headers, decision 0004).
// Errors use component "graphics" and these codes:
//   vulkan_unavailable   no Vulkan loader or instance creation failed
//   device_lost / vulkan_error   a Vulkan call failed (VkResult in message)
//   unsupported_device   device below Vulkan 1.2 or without a compute queue
//   no_memory_type       no host-visible, host-coherent memory type
//   timeout              the dispatch did not finish within the limit

#include "scale/common/error.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace scale::graphics {

// Capability fingerprint recorded with every GPU result (docs/contracts/gpu_targets.md).
struct DeviceInfo {
    std::uint32_t index = 0;  // position in vkEnumeratePhysicalDevices
    std::string name;
    std::uint32_t vendor_id = 0;
    std::uint32_t device_id = 0;
    std::string device_type;  // "discrete", "integrated", "virtual", "cpu", "other"
    std::uint32_t api_version = 0;
    std::uint32_t driver_version = 0;
    std::uint32_t driver_id = 0;
    std::string driver_name;
    std::string driver_info;
    std::uint32_t subgroup_size = 0;
    std::uint32_t min_subgroup_size = 0;  // 0 when Vulkan 1.3 properties are unavailable
    std::uint32_t max_subgroup_size = 0;
    bool rounding_mode_rte_f32 = false;
    bool denorm_preserve_f32 = false;
    bool denorm_flush_to_zero_f32 = false;
    std::uint32_t max_compute_shared_memory = 0;
    bool supported = false;  // Vulkan >= 1.2 and a compute queue
    std::string unsupported_reason;

    [[nodiscard]] bool is_software() const noexcept { return device_type == "cpu"; }
    // One line: "<name> | vendor 0x1002 device 0x7550 | discrete | Vulkan 1.4.359 | driver ..."
    [[nodiscard]] std::string fingerprint() const;
};

// Lists every Vulkan physical device with its fingerprint.
[[nodiscard]] Result<std::vector<DeviceInfo>> enumerate_devices();

struct DispatchRequest {
    std::span<const std::uint32_t> spirv;
    std::span<const std::uint32_t> input_words;
    std::span<const std::uint32_t> initial_output_words;  // also defines the output size
    std::uint32_t groups_x = 1;
    std::chrono::milliseconds timeout{10'000};
};

class ComputeDevice {
public:
    // Opens device `index` from enumerate_devices(); it must be supported.
    [[nodiscard]] static Result<ComputeDevice> open(std::uint32_t index);

    ComputeDevice(ComputeDevice&&) noexcept;
    ComputeDevice& operator=(ComputeDevice&&) noexcept;
    ~ComputeDevice();

    [[nodiscard]] const DeviceInfo& info() const noexcept;

    // Creates the pipeline, uploads the buffers, dispatches, waits and returns
    // the output buffer contents.
    [[nodiscard]] Result<std::vector<std::uint32_t>> dispatch(const DispatchRequest& request);

private:
    struct Impl;
    explicit ComputeDevice(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}  // namespace scale::graphics
