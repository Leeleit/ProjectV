#pragma once
// Vulkan 1.4 base setup for the frame-flight-allocator-budget prototype.
// Single physical device (first discrete GPU), one graphics+compute queue,
// transient command pool. Validation layers OFF (perf-sensitive harness).

// VK_NO_PROTOTYPES makes vulkan.h declare types only (no inline function
// bodies); volk.h then provides function-pointer externs that get populated
// by volkLoadInstance / volkLoadDevice.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace prototype {

struct DeviceContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    uint32_t graphicsFamily = 0;
    VkPhysicalDeviceMemoryProperties memProps{};
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps{};
    bool memoryBudgetExt = false;
    VmaAllocator allocator = VK_NULL_HANDLE;
};

inline void checkVk(VkResult r, const char* tag) {
    if (r != VK_SUCCESS) {
        throw std::runtime_error(std::string("Vulkan call failed: ") + tag +
                                 " VkResult=" + std::to_string((int)r));
    }
}

inline void checkVma(VkResult r, const char* tag) {
    if (r != VK_SUCCESS) {
        throw std::runtime_error(std::string("VMA call failed: ") + tag +
                                 " VkResult=" + std::to_string((int)r));
    }
}

inline DeviceContext createDeviceContext(bool enableMemoryBudgetExt) {
    DeviceContext ctx{};

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "frame-flight-prototype";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "prototype";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    std::vector<const char*> instanceExts = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = (uint32_t)instanceExts.size();
    instanceInfo.ppEnabledExtensionNames = instanceExts.data();

    checkVk(vkCreateInstance(&instanceInfo, nullptr, &ctx.instance), "vkCreateInstance");
    volkLoadInstance(ctx.instance);

    uint32_t gpuCount = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &gpuCount, nullptr);
    if (gpuCount == 0) throw std::runtime_error("No Vulkan physical devices");
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(ctx.instance, &gpuCount, gpus.data());

    // Pick first discrete GPU.
    for (auto gpu : gpus) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(gpu, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            ctx.physical = gpu;
            break;
        }
    }
    if (ctx.physical == VK_NULL_HANDLE) ctx.physical = gpus[0];

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(ctx.physical, &props);
    fprintf(stderr, "[harness] GPU: %s (Vulkan %u.%u.%u)\n",
            props.deviceName,
            VK_VERSION_MAJOR(props.apiVersion),
            VK_VERSION_MINOR(props.apiVersion),
            VK_VERSION_PATCH(props.apiVersion));

    // Memory props + optional budget extension.
    VkPhysicalDeviceMemoryProperties2 memProps2{};
    memProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    if (enableMemoryBudgetExt) {
        memProps2.pNext = &ctx.budgetProps;
        ctx.budgetProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
        ctx.memoryBudgetExt = true;
    }
    vkGetPhysicalDeviceMemoryProperties2(ctx.physical, &memProps2);
    ctx.memProps = memProps2.memoryProperties;

    fprintf(stderr, "[harness] Memory heaps: %u, MemoryBudgetExt=%d\n",
            ctx.memProps.memoryHeapCount, ctx.memoryBudgetExt);
    for (uint32_t i = 0; i < ctx.memProps.memoryHeapCount; ++i) {
        fprintf(stderr, "  heap[%u] size=%lu MiB flags=0x%x",
                i, ctx.memProps.memoryHeaps[i].size / (1024 * 1024),
                ctx.memProps.memoryHeaps[i].flags);
        if (ctx.memoryBudgetExt) {
            fprintf(stderr, " budget=%lu MiB usage=%lu MiB",
                    ctx.budgetProps.heapBudget[i] / (1024 * 1024),
                    ctx.budgetProps.heapUsage[i] / (1024 * 1024));
        }
        fprintf(stderr, "\n");
    }

    // Queue family (graphics + compute).
    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfs(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical, &qfCount, qfs.data());
    for (uint32_t i = 0; i < qfCount; ++i) {
        if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (qfs[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            ctx.graphicsFamily = i;
            break;
        }
    }
    if (ctx.graphicsFamily == UINT32_MAX) throw std::runtime_error("No gfx+compute queue");

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qInfo{};
    qInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qInfo.queueFamilyIndex = ctx.graphicsFamily;
    qInfo.queueCount = 1;
    qInfo.pQueuePriorities = &priority;

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo dInfo{};
    dInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dInfo.queueCreateInfoCount = 1;
    dInfo.pQueueCreateInfos = &qInfo;
    dInfo.pEnabledFeatures = &features;

    checkVk(vkCreateDevice(ctx.physical, &dInfo, nullptr, &ctx.device), "vkCreateDevice");
    volkLoadDevice(ctx.device);
    vkGetDeviceQueue(ctx.device, ctx.graphicsFamily, 0, &ctx.graphicsQueue);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = ctx.graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    checkVk(vkCreateCommandPool(ctx.device, &poolInfo, nullptr, &ctx.commandPool),
            "vkCreateCommandPool");

    // VMA allocator (optional budget ext).
    VmaVulkanFunctions vks{};
    vks.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vks.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vks.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vks.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vks.vkAllocateMemory = vkAllocateMemory;
    vks.vkFreeMemory = vkFreeMemory;
    vks.vkMapMemory = vkMapMemory;
    vks.vkUnmapMemory = vkUnmapMemory;
    vks.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vks.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vks.vkBindBufferMemory = vkBindBufferMemory;
    vks.vkBindImageMemory = vkBindImageMemory;
    vks.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vks.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vks.vkCreateBuffer = vkCreateBuffer;
    vks.vkDestroyBuffer = vkDestroyBuffer;
    vks.vkCreateImage = vkCreateImage;
    vks.vkDestroyImage = vkDestroyImage;
    vks.vkCmdCopyBuffer = vkCmdCopyBuffer;

    VmaAllocatorCreateInfo ai{};
    ai.physicalDevice = ctx.physical;
    ai.device = ctx.device;
    ai.instance = ctx.instance;
    ai.vulkanApiVersion = VK_API_VERSION_1_4;
    ai.pVulkanFunctions = &vks;
    ai.flags = enableMemoryBudgetExt ? VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT : 0;

    checkVma(vmaCreateAllocator(&ai, &ctx.allocator), "vmaCreateAllocator");
    return ctx;
}

inline void destroyDeviceContext(DeviceContext& ctx) {
    if (ctx.allocator) vmaDestroyAllocator(ctx.allocator);
    if (ctx.commandPool) vkDestroyCommandPool(ctx.device, ctx.commandPool, nullptr);
    if (ctx.device) vkDestroyDevice(ctx.device, nullptr);
    if (ctx.instance) vkDestroyInstance(ctx.instance, nullptr);
    ctx = {};
}

}  // namespace prototype
