#include "scale/graphics/vulkan_compute.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <functional>
#include <optional>
#include <utility>

namespace scale::graphics {
namespace {

std::unexpected<Error> vk_error(std::string_view what, VkResult result) {
    const std::string_view code = result == VK_ERROR_DEVICE_LOST ? "device_lost" : "vulkan_error";
    return fail("graphics", code, std::format("{} failed (VkResult {})", what, static_cast<int>(result)));
}

// Runs cleanup actions in reverse order of registration.
class Cleanup {
public:
    Cleanup() = default;
    Cleanup(const Cleanup&) = delete;
    Cleanup& operator=(const Cleanup&) = delete;
    ~Cleanup() {
        for (auto it = actions_.rbegin(); it != actions_.rend(); ++it) (*it)();
    }
    void add(std::function<void()> action) { actions_.push_back(std::move(action)); }

private:
    std::vector<std::function<void()>> actions_;
};

std::string version_string(std::uint32_t v) {
    return std::format("{}.{}.{}", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));
}

std::string type_name(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
        default: return "other";
    }
}

struct Instance {
    VkInstance handle = VK_NULL_HANDLE;
    Instance() = default;
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&& o) noexcept : handle(std::exchange(o.handle, VK_NULL_HANDLE)) {}
    Instance& operator=(Instance&& o) noexcept {
        if (this != &o) {
            if (handle) vkDestroyInstance(handle, nullptr);
            handle = std::exchange(o.handle, VK_NULL_HANDLE);
        }
        return *this;
    }
    ~Instance() {
        if (handle) vkDestroyInstance(handle, nullptr);
    }
};

Result<Instance> create_instance() {
    std::uint32_t loader_version = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion(&loader_version) != VK_SUCCESS || loader_version < VK_API_VERSION_1_2) {
        return fail("graphics", "vulkan_unavailable",
                    std::format("Vulkan loader {} is older than 1.2", version_string(loader_version)));
    }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "SCALE";
    app.pEngineName = "SCALE";
    app.apiVersion = std::min<std::uint32_t>(loader_version, VK_API_VERSION_1_3);
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    Instance instance;
    const VkResult r = vkCreateInstance(&ci, nullptr, &instance.handle);
    if (r != VK_SUCCESS) {
        return fail("graphics", "vulkan_unavailable", std::format("vkCreateInstance failed (VkResult {})",
                                                                 static_cast<int>(r)));
    }
    return instance;
}

std::optional<std::uint32_t> compute_queue_family(VkPhysicalDevice device) {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    for (std::uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) return i;
    }
    return std::nullopt;
}

DeviceInfo describe(VkPhysicalDevice device, std::uint32_t index) {
    DeviceInfo info;
    info.index = index;
    VkPhysicalDeviceProperties base{};
    vkGetPhysicalDeviceProperties(device, &base);
    info.name = base.deviceName;
    info.vendor_id = base.vendorID;
    info.device_id = base.deviceID;
    info.device_type = type_name(base.deviceType);
    info.api_version = base.apiVersion;
    info.driver_version = base.driverVersion;
    info.max_compute_shared_memory = base.limits.maxComputeSharedMemorySize;

    if (base.apiVersion < VK_API_VERSION_1_2) {
        info.unsupported_reason = "device supports Vulkan " + version_string(base.apiVersion) + " (< 1.2)";
        return info;
    }
    VkPhysicalDeviceVulkan13Properties p13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES};
    VkPhysicalDeviceVulkan12Properties p12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
    VkPhysicalDeviceVulkan11Properties p11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES};
    p11.pNext = &p12;
    if (base.apiVersion >= VK_API_VERSION_1_3) p12.pNext = &p13;
    VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    props.pNext = &p11;
    vkGetPhysicalDeviceProperties2(device, &props);

    info.subgroup_size = p11.subgroupSize;
    info.driver_id = static_cast<std::uint32_t>(p12.driverID);
    info.driver_name = p12.driverName;
    info.driver_info = p12.driverInfo;
    info.rounding_mode_rte_f32 = p12.shaderRoundingModeRTEFloat32 == VK_TRUE;
    info.denorm_preserve_f32 = p12.shaderDenormPreserveFloat32 == VK_TRUE;
    info.denorm_flush_to_zero_f32 = p12.shaderDenormFlushToZeroFloat32 == VK_TRUE;
    if (base.apiVersion >= VK_API_VERSION_1_3) {
        info.min_subgroup_size = p13.minSubgroupSize;
        info.max_subgroup_size = p13.maxSubgroupSize;
    }
    if (!compute_queue_family(device)) {
        info.unsupported_reason = "no compute queue family";
        return info;
    }
    info.supported = true;
    return info;
}

Result<std::vector<VkPhysicalDevice>> physical_devices(VkInstance instance) {
    std::uint32_t count = 0;
    VkResult r = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (r != VK_SUCCESS) return vk_error("vkEnumeratePhysicalDevices", r);
    std::vector<VkPhysicalDevice> devices(count);
    r = vkEnumeratePhysicalDevices(instance, &count, devices.data());
    if (r != VK_SUCCESS && r != VK_INCOMPLETE) return vk_error("vkEnumeratePhysicalDevices", r);
    devices.resize(count);
    return devices;
}

}  // namespace

std::string DeviceInfo::fingerprint() const {
    std::string s = std::format("{} | vendor 0x{:04x} device 0x{:04x} | {} | Vulkan {} | driver {} ({}) id {} raw 0x{:08x}",
                                name, vendor_id, device_id, device_type, version_string(api_version), driver_name,
                                driver_info, driver_id, driver_version);
    s += std::format(" | subgroup {} [{}..{}] | rte_f32 {} denorm_preserve_f32 {} ftz_f32 {} | shared {} B",
                     subgroup_size, min_subgroup_size, max_subgroup_size, rounding_mode_rte_f32,
                     denorm_preserve_f32, denorm_flush_to_zero_f32, max_compute_shared_memory);
    return s;
}

Result<std::vector<DeviceInfo>> enumerate_devices() {
    auto instance = create_instance();
    if (!instance) return std::unexpected(instance.error());
    auto devices = physical_devices(instance->handle);
    if (!devices) return std::unexpected(devices.error());
    std::vector<DeviceInfo> infos;
    for (std::uint32_t i = 0; i < devices->size(); ++i) infos.push_back(describe((*devices)[i], i));
    return infos;
}

struct ComputeDevice::Impl {
    Instance instance;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t family = 0;
    VkPhysicalDeviceMemoryProperties memory{};
    DeviceInfo info;

    ~Impl() {
        if (device) vkDestroyDevice(device, nullptr);
    }
};

ComputeDevice::ComputeDevice(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ComputeDevice::ComputeDevice(ComputeDevice&&) noexcept = default;
ComputeDevice& ComputeDevice::operator=(ComputeDevice&&) noexcept = default;
ComputeDevice::~ComputeDevice() = default;

const DeviceInfo& ComputeDevice::info() const noexcept {
    return impl_->info;
}

Result<ComputeDevice> ComputeDevice::open(std::uint32_t index) {
    auto impl = std::make_unique<Impl>();
    auto instance = create_instance();
    if (!instance) return std::unexpected(instance.error());
    impl->instance = std::move(*instance);
    auto devices = physical_devices(impl->instance.handle);
    if (!devices) return std::unexpected(devices.error());
    if (index >= devices->size()) {
        return fail("graphics", "unsupported_device", std::format("device index {} out of range", index));
    }
    impl->physical = (*devices)[index];
    impl->info = describe(impl->physical, index);
    if (!impl->info.supported) {
        return fail("graphics", "unsupported_device", impl->info.unsupported_reason);
    }
    impl->family = *compute_queue_family(impl->physical);
    vkGetPhysicalDeviceMemoryProperties(impl->physical, &impl->memory);

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = impl->family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    const VkResult r = vkCreateDevice(impl->physical, &dci, nullptr, &impl->device);
    if (r != VK_SUCCESS) return vk_error("vkCreateDevice", r);
    vkGetDeviceQueue(impl->device, impl->family, 0, &impl->queue);
    return ComputeDevice(std::move(impl));
}

Result<std::vector<std::uint32_t>> ComputeDevice::dispatch(const DispatchRequest& req) {
    Impl& d = *impl_;
    VkDevice dev = d.device;
    Cleanup cleanup;
    VkResult r = VK_SUCCESS;

    if (req.spirv.empty() || req.initial_output_words.empty() || req.input_words.empty() || req.groups_x == 0) {
        return fail("graphics", "vulkan_error", "dispatch request needs SPIR-V, input, output and groups_x > 0");
    }

    // ---- Buffers (host-visible, host-coherent) -------------------------------------
    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceSize size = 0;
    };
    const auto make_buffer = [&](std::span<const std::uint32_t> words, Buffer& b) -> Result<void> {
        b.size = words.size_bytes();
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = b.size;
        bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if ((r = vkCreateBuffer(dev, &bci, nullptr, &b.buffer)) != VK_SUCCESS) return vk_error("vkCreateBuffer", r);
        cleanup.add([dev, buf = b.buffer] { vkDestroyBuffer(dev, buf, nullptr); });
        VkMemoryRequirements req_mem{};
        vkGetBufferMemoryRequirements(dev, b.buffer, &req_mem);
        constexpr VkMemoryPropertyFlags wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        std::optional<std::uint32_t> type;
        for (std::uint32_t i = 0; i < d.memory.memoryTypeCount; ++i) {
            if ((req_mem.memoryTypeBits & (1u << i)) && (d.memory.memoryTypes[i].propertyFlags & wanted) == wanted) {
                type = i;
                break;
            }
        }
        if (!type) return fail("graphics", "no_memory_type", "no host-visible, host-coherent memory type");
        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = req_mem.size;
        mai.memoryTypeIndex = *type;
        if ((r = vkAllocateMemory(dev, &mai, nullptr, &b.memory)) != VK_SUCCESS) return vk_error("vkAllocateMemory", r);
        cleanup.add([dev, mem = b.memory] { vkFreeMemory(dev, mem, nullptr); });
        if ((r = vkBindBufferMemory(dev, b.buffer, b.memory, 0)) != VK_SUCCESS) return vk_error("vkBindBufferMemory", r);
        if ((r = vkMapMemory(dev, b.memory, 0, b.size, 0, &b.mapped)) != VK_SUCCESS) return vk_error("vkMapMemory", r);
        std::memcpy(b.mapped, words.data(), b.size);
        return {};
    };
    Buffer input, output;
    if (auto e = make_buffer(req.input_words, input); !e) return std::unexpected(e.error());
    if (auto e = make_buffer(req.initial_output_words, output); !e) return std::unexpected(e.error());

    // ---- Pipeline -----------------------------------------------------------------------
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = req.spirv.size_bytes();
    smci.pCode = req.spirv.data();
    VkShaderModule module = VK_NULL_HANDLE;
    if ((r = vkCreateShaderModule(dev, &smci, nullptr, &module)) != VK_SUCCESS) return vk_error("vkCreateShaderModule", r);
    cleanup.add([dev, module] { vkDestroyShaderModule(dev, module, nullptr); });

    VkDescriptorSetLayoutBinding bindings[2]{};
    for (std::uint32_t i = 0; i < 2; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 2;
    dslci.pBindings = bindings;
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    if ((r = vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &set_layout)) != VK_SUCCESS) {
        return vk_error("vkCreateDescriptorSetLayout", r);
    }
    cleanup.add([dev, set_layout] { vkDestroyDescriptorSetLayout(dev, set_layout, nullptr); });

    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &set_layout;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    if ((r = vkCreatePipelineLayout(dev, &plci, nullptr, &pipeline_layout)) != VK_SUCCESS) {
        return vk_error("vkCreatePipelineLayout", r);
    }
    cleanup.add([dev, pipeline_layout] { vkDestroyPipelineLayout(dev, pipeline_layout, nullptr); });

    VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = module;
    cpci.stage.pName = "main";
    cpci.layout = pipeline_layout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    if ((r = vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline)) != VK_SUCCESS) {
        return vk_error("vkCreateComputePipelines", r);
    }
    cleanup.add([dev, pipeline] { vkDestroyPipeline(dev, pipeline, nullptr); });

    // ---- Descriptors --------------------------------------------------------------------
    VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &pool_size;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if ((r = vkCreateDescriptorPool(dev, &dpci, nullptr, &pool)) != VK_SUCCESS) return vk_error("vkCreateDescriptorPool", r);
    cleanup.add([dev, pool] { vkDestroyDescriptorPool(dev, pool, nullptr); });
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &set_layout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if ((r = vkAllocateDescriptorSets(dev, &dsai, &set)) != VK_SUCCESS) return vk_error("vkAllocateDescriptorSets", r);
    VkDescriptorBufferInfo buffer_infos[2] = {{input.buffer, 0, VK_WHOLE_SIZE}, {output.buffer, 0, VK_WHOLE_SIZE}};
    VkWriteDescriptorSet writes[2]{};
    for (std::uint32_t i = 0; i < 2; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &buffer_infos[i];
    }
    vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);

    // ---- Record, submit, wait ---------------------------------------------------------------
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.queueFamilyIndex = d.family;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    if ((r = vkCreateCommandPool(dev, &cpi, nullptr, &command_pool)) != VK_SUCCESS) return vk_error("vkCreateCommandPool", r);
    cleanup.add([dev, command_pool] { vkDestroyCommandPool(dev, command_pool, nullptr); });
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = command_pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if ((r = vkAllocateCommandBuffers(dev, &cbai, &cmd)) != VK_SUCCESS) return vk_error("vkAllocateCommandBuffers", r);

    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if ((r = vkBeginCommandBuffer(cmd, &begin)) != VK_SUCCESS) return vk_error("vkBeginCommandBuffer", r);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &set, 0, nullptr);
    vkCmdDispatch(cmd, req.groups_x, 1, 1);
    // Make shader writes visible to the host before readback.
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0,
                         nullptr, 0, nullptr);
    if ((r = vkEndCommandBuffer(cmd)) != VK_SUCCESS) return vk_error("vkEndCommandBuffer", r);

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    if ((r = vkCreateFence(dev, &fci, nullptr, &fence)) != VK_SUCCESS) return vk_error("vkCreateFence", r);
    cleanup.add([dev, fence] { vkDestroyFence(dev, fence, nullptr); });
    // If anything below fails, wait for the device before objects are destroyed.
    cleanup.add([dev] { vkDeviceWaitIdle(dev); });

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    if ((r = vkQueueSubmit(d.queue, 1, &submit, fence)) != VK_SUCCESS) return vk_error("vkQueueSubmit", r);
    const auto timeout_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(req.timeout).count());
    r = vkWaitForFences(dev, 1, &fence, VK_TRUE, timeout_ns);
    if (r == VK_TIMEOUT) {
        return fail("graphics", "timeout", std::format("dispatch did not finish within {} ms", req.timeout.count()));
    }
    if (r != VK_SUCCESS) return vk_error("vkWaitForFences", r);

    std::vector<std::uint32_t> result(req.initial_output_words.size());
    std::memcpy(result.data(), output.mapped, output.size);
    return result;
}

}  // namespace scale::graphics
