#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <span>
#include <string>
#include <chrono>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace harness {

// Stats (matches benchmarks/methodology.md §3)
struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
    uint32_t n;
};

inline Stats ComputeStats(std::span<const double> samples) {
    Stats s{};
    s.n = static_cast<uint32_t>(samples.size());
    if (samples.empty()) return s;
    std::vector<double> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min = sorted.front();
    s.max = sorted.back();
    return s;
}

// RAII wrappers for Vulkan resources
class Buffer {
public:
    Buffer() = default;
    Buffer(VmaAllocator alloc, VkDeviceSize size, VkBufferUsageFlags usage,
           VmaMemoryUsage mem_usage, VmaAllocationCreateFlags flags = 0)
        : allocator_(alloc), size_(size) {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = mem_usage;
        alloc_info.flags = flags;
        if (vmaCreateBuffer(allocator_, &buffer_info, &alloc_info, &buffer_, &allocation_, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("vmaCreateBuffer failed");
        }
    }
    ~Buffer() {
        if (buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, buffer_, allocation_);
        }
    }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept
        : allocator_(other.allocator_), buffer_(other.buffer_),
          allocation_(other.allocation_), size_(other.size_) {
        other.buffer_ = VK_NULL_HANDLE;
        other.allocation_ = VK_NULL_HANDLE;
    }
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            if (buffer_ != VK_NULL_HANDLE) {
                vmaDestroyBuffer(allocator_, buffer_, allocation_);
            }
            allocator_ = other.allocator_;
            buffer_ = other.buffer_;
            allocation_ = other.allocation_;
            size_ = other.size_;
            other.buffer_ = VK_NULL_HANDLE;
            other.allocation_ = VK_NULL_HANDLE;
        }
        return *this;
    }
    VkBuffer handle() const { return buffer_; }
    VkDeviceSize size() const { return size_; }
    void* Map() {
        void* p = nullptr;
        vmaMapMemory(allocator_, allocation_, &p);
        return p;
    }
    void Unmap() {
        vmaUnmapMemory(allocator_, allocation_);
    }
private:
    VmaAllocator allocator_{VK_NULL_HANDLE};
    VkBuffer buffer_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    VkDeviceSize size_{0};
};

// RAII wrapper for query pool (GPU timestamp)
class QueryPool {
public:
    QueryPool() = default;
    QueryPool(VkDevice device, uint32_t query_count) : device_(device), count_(query_count) {
        VkQueryPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
        pool_info.queryCount = query_count;
        if (vkCreateQueryPool(device_, &pool_info, nullptr, &pool_) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateQueryPool failed");
        }
    }
    ~QueryPool() {
        if (pool_ != VK_NULL_HANDLE) {
            vkDestroyQueryPool(device_, pool_, nullptr);
        }
    }
    QueryPool(const QueryPool&) = delete;
    QueryPool& operator=(const QueryPool&) = delete;
    VkQueryPool handle() const { return pool_; }
    uint32_t count() const { return count_; }
private:
    VkDevice device_{VK_NULL_HANDLE};
    VkQueryPool pool_{VK_NULL_HANDLE};
    uint32_t count_{0};
};

// Vulkan 1.4 harness
class VulkanContext {
public:
    VkInstance instance_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue compute_queue_{VK_NULL_HANDLE};
    uint32_t compute_queue_family_{UINT32_MAX};
    VmaAllocator allocator_{VK_NULL_HANDLE};
    VkCommandPool cmd_pool_{VK_NULL_HANDLE};
    VkPhysicalDeviceProperties device_properties_{};
    VkPhysicalDeviceSubgroupProperties subgroup_properties_{};

    void Init() {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "gpu-fluid-ca-atomic-strategy";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "prototype";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_4;

        VkInstanceCreateInfo instance_info{};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;
        if (vkCreateInstance(&instance_info, nullptr, &instance_) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateInstance failed");
        }

        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
        if (device_count == 0) throw std::runtime_error("no Vulkan devices");
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());
        physical_device_ = devices[0];

        vkGetPhysicalDeviceProperties(physical_device_, &device_properties_);

        // Find compute-capable queue family
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families.data());
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                compute_queue_family_ = i;
                break;
            }
        }
        if (compute_queue_family_ == UINT32_MAX) {
            throw std::runtime_error("no compute-capable queue family");
        }

        float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = compute_queue_family_;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;

        VkPhysicalDeviceSubgroupProperties subgroup_props{};
        subgroup_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &subgroup_props;
        vkGetPhysicalDeviceProperties2(physical_device_, &props2);
        subgroup_properties_ = subgroup_props;

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledLayerCount = 0;
        device_info.enabledExtensionCount = 0;

        if (vkCreateDevice(physical_device_, &device_info, nullptr, &device_) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateDevice failed");
        }
        vkGetDeviceQueue(device_, compute_queue_family_, 0, &compute_queue_);

        // VMA
        // VMA_STATIC_VULKAN_FUNCTIONS=1: VMA uses vulkan.h prototypes directly from libvulkan.so.
        VmaAllocatorCreateInfo allocator_info{};
        allocator_info.physicalDevice = physical_device_;
        allocator_info.device = device_;
        allocator_info.instance = instance_;
        allocator_info.vulkanApiVersion = VK_API_VERSION_1_4;
        if (vmaCreateAllocator(&allocator_info, &allocator_) != VK_SUCCESS) {
            throw std::runtime_error("vmaCreateAllocator failed");
        }

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = compute_queue_family_;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device_, &pool_info, nullptr, &cmd_pool_) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateCommandPool failed");
        }
    }

    void Shutdown() {
        vkDestroyCommandPool(device_, cmd_pool_, nullptr);
        vmaDestroyAllocator(allocator_);
        vkDestroyDevice(device_, nullptr);
        vkDestroyInstance(instance_, nullptr);
    }

    void PrintDeviceInfo() const {
        std::printf("Device: %s (Vulkan %u.%u.%u)\n",
                    device_properties_.deviceName,
                    VK_VERSION_MAJOR(device_properties_.apiVersion),
                    VK_VERSION_MINOR(device_properties_.apiVersion),
                    VK_VERSION_PATCH(device_properties_.apiVersion));
        std::printf("  Driver: %u.%u.%u\n",
                    VK_VERSION_MAJOR(device_properties_.driverVersion),
                    VK_VERSION_MINOR(device_properties_.driverVersion),
                    VK_VERSION_PATCH(device_properties_.driverVersion));
        std::printf("  subgroupSize: %u\n", subgroup_properties_.subgroupSize);
        std::printf("  subgroupOperations supported stages: 0x%x\n",
                    subgroup_properties_.supportedStages);
        std::printf("  maxComputeSharedMemorySize: %u KB\n",
                    device_properties_.limits.maxComputeSharedMemorySize / 1024);
        std::printf("  maxComputeWorkGroupInvocations: %u\n",
                    device_properties_.limits.maxComputeWorkGroupInvocations);
        std::printf("  timestampPeriod: %f ns/tick\n",
                    device_properties_.limits.timestampPeriod);
    }

    // Allocate GPU-only storage buffer (with transfer_dst for vkCmdCopyBuffer reset)
    Buffer CreateStorageBuffer(VkDeviceSize size) {
        return Buffer(allocator_, size,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);
    }

    // Allocate staging buffer (host → device upload)
    Buffer CreateStagingBuffer(VkDeviceSize size) {
        return Buffer(allocator_, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }

    // Allocate readback buffer (device → host)
    Buffer CreateReadbackBuffer(VkDeviceSize size) {
        return Buffer(allocator_, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
    }

    // Allocate device-local buffer with optional initial upload
    Buffer CreateAndUpload(VkDeviceSize size, const void* src) {
        Buffer staging = CreateStagingBuffer(size);
        void* mapped = staging.Map();
        std::memcpy(mapped, src, size);
        staging.Unmap();

        Buffer dst = CreateStorageBuffer(size);

        CopyBuffer(staging.handle(), dst.handle(), size);
        return dst;
    }

    // Copy buffer → buffer (used internally for staging and readback)
    void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
        VkCommandBufferAllocateInfo cmd_alloc_info{};
        cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_alloc_info.commandPool = cmd_pool_;
        cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_alloc_info.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(device_, &cmd_alloc_info, &cmd);
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin_info);
        VkBufferCopy copy_region{};
        copy_region.size = size;
        vkCmdCopyBuffer(cmd, src, dst, 1, &copy_region);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;
        vkQueueSubmit(compute_queue_, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(compute_queue_);
        vkFreeCommandBuffers(device_, cmd_pool_, 1, &cmd);
    }

    // Allocate single-use command buffer
    VkCommandBuffer AllocateOneShotCommandBuffer() {
        VkCommandBufferAllocateInfo cmd_alloc_info{};
        cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_alloc_info.commandPool = cmd_pool_;
        cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_alloc_info.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(device_, &cmd_alloc_info, &cmd);
        return cmd;
    }

    void FreeCommandBuffer(VkCommandBuffer cmd) {
        vkFreeCommandBuffers(device_, cmd_pool_, 1, &cmd);
    }

    // Readback into host buffer (caller must keep returned Buffer alive)
    Buffer ReadbackBuffer(VkBuffer src, VkDeviceSize size) {
        Buffer readback = CreateReadbackBuffer(size);
        CopyBuffer(src, readback.handle(), size);
        return readback;
    }

    // Compute GPU timestamp delta (in nanoseconds)
    double ComputeTimestampDelta(const QueryPool& pool, uint32_t start_idx, uint32_t end_idx) {
        uint64_t timestamps[2] = {0, 0};
        vkGetQueryPoolResults(device_, pool.handle(), start_idx, 2,
                              sizeof(timestamps), timestamps, sizeof(uint64_t),
                              VK_QUERY_RESULT_WAIT_BIT);
        const double period = device_properties_.limits.timestampPeriod;
        return static_cast<double>(timestamps[1] - timestamps[0]) * period;
    }
};

} // namespace harness
