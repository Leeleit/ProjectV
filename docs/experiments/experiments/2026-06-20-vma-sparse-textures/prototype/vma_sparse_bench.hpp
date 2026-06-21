// SPDX-License-Identifier: MIT
//
// vma_sparse_bench.cpp — Standalone Vulkan 1.4 + VMA 3.4.0 prototype for
// `2026-06-20-vma-sparse-textures`.
//
// Compares three virtual-texturing strategies for ProjectV Stage 2.3:
//   1. dense_atlas  — single dense 16 MiB atlas (current mainline pattern, no VT)
//   2. sparse_atlas — hardware `VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT` atlas
//                     (bind pages incrementally via `vkQueueBindSparse`)
//   3. software_vt  — texture atlas (16 MiB) + R32Uint page table texture + CPU
//                     page manager (LRU eviction) — shlom.dev pattern
//
// Measures: peak VRAM (via `VK_EXT_memory_budget`), bind latency, simulated
// page-miss cost in shader (CPU emulation).
//
// Build (standalone, NOT mainline):
//   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
//     -DCMAKE_CXX_COMPILER=clang++ \
//     -DCMAKE_CXX_FLAGS="-O3 -march=native -std=c++26"
//   cmake --build build -j
//
// Run (dev host, RTX 3060 Ti Ampere, headless):
//   ./build/vma_sparse_bench --variant=dense --iters=1000
//   ./build/vma_sparse_bench --variant=sparse --iters=1000
//   ./build/vma_sparse_bench --variant=software-vt --iters=1000
//
// Output: stdout summary + results.csv (mean/median/p95/p99/std per metric).

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vma_bench {

constexpr std::string_view kAppName = "vma_sparse_bench";
constexpr std::string_view kEngineName = "ProjectV-Experiments";
constexpr uint32_t kVulkanApiVersion = VK_API_VERSION_1_4;

constexpr uint32_t kAtlasSize = 1024u;          // 1024×1024 RGBA8 = 4 MiB
constexpr uint32_t kAtlasSizeBytes = kAtlasSize * kAtlasSize * 4u;
constexpr uint32_t kPageSize = 64u;            // 64×64 page
constexpr uint32_t kPageSizeBytes = kPageSize * kPageSize * 4u;
constexpr uint32_t kPageGridDim = kAtlasSize / kPageSize;  // 16×16 = 256 pages
constexpr uint32_t kPageCount = kPageGridDim * kPageGridDim;

struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double min_v = 0.0;
    double max_v = 0.0;
};

[[nodiscard]] Stats ComputeStats(std::span<const double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min_v = sorted.front();
    s.max_v = sorted.back();
    return s;
}

struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue sparseBindingQueue = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    uint32_t graphicsFamily = 0u;
    uint32_t sparseBindingFamily = 0u;
    bool memoryBudgetSupported = false;
    bool sparseResidencySupported = false;
};

[[nodiscard]] bool CreateContext(Context& ctx) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = kAppName.data();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = kEngineName.data();
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = kVulkanApiVersion;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

    if (vkCreateInstance(&instanceInfo, nullptr, &ctx.instance) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vkCreateInstance\n");
        return false;
    }

    uint32_t deviceCount = 0u;
    vkEnumeratePhysicalDevices(ctx.instance, &deviceCount, nullptr);
    if (deviceCount == 0u) {
        std::fprintf(stderr, "FAIL: no physical devices\n");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(ctx.instance, &deviceCount, devices.data());
    ctx.physicalDevice = devices[0];

    auto sparseProps = VkPhysicalDeviceSparseProperties{};
    auto props = VkPhysicalDeviceProperties2{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2(ctx.physicalDevice, &props);

    auto features = VkPhysicalDeviceFeatures2{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vkGetPhysicalDeviceFeatures2(ctx.physicalDevice, &features);
    ctx.sparseResidencySupported = features.features.sparseResidencyImage2D != VK_FALSE;

    uint32_t queueFamilyCount = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        ctx.physicalDevice, &queueFamilyCount, families.data());

    ctx.graphicsFamily = UINT32_MAX;
    ctx.sparseBindingFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u &&
            ctx.graphicsFamily == UINT32_MAX) {
            ctx.graphicsFamily = i;
        }
        if ((families[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0u) {
            ctx.sparseBindingFamily = i;
        }
    }
    if (ctx.graphicsFamily == UINT32_MAX) {
        std::fprintf(stderr, "FAIL: no graphics queue\n");
        return false;
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfos[2]{};
    uint32_t queueInfoCount = 1u;
    queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfos[0].queueFamilyIndex = ctx.graphicsFamily;
    queueInfos[0].queueCount = 1u;
    queueInfos[0].pQueuePriorities = &queuePriority;
    if (ctx.sparseBindingFamily != UINT32_MAX &&
        ctx.sparseBindingFamily != ctx.graphicsFamily) {
        queueInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[1].queueFamilyIndex = ctx.sparseBindingFamily;
        queueInfos[1].queueCount = 1u;
        queueInfos[1].pQueuePriorities = &queuePriority;
        queueInfoCount = 2u;
    }

    auto bindMemory2 = VkPhysicalDeviceBindMemoryFeatures{};
    bindMemory2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BIND_MEMORY_FEATURES;

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.sparseBinding = VK_TRUE;
    deviceFeatures.sparseResidencyImage2D = ctx.sparseResidencySupported ? VK_TRUE : VK_FALSE;
    deviceFeatures.shaderResourceResidency = ctx.sparseResidencySupported ? VK_TRUE : VK_FALSE;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &bindMemory2;
    deviceInfo.queueCreateInfoCount = queueInfoCount;
    deviceInfo.pQueueCreateInfos = queueInfos;
    deviceInfo.pEnabledFeatures = &deviceFeatures;

    if (vkCreateDevice(ctx.physicalDevice, &deviceInfo, nullptr, &ctx.device) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vkCreateDevice\n");
        return false;
    }

    vkGetDeviceQueue(ctx.device, ctx.graphicsFamily, 0u, &ctx.graphicsQueue);
    if (ctx.sparseBindingFamily != UINT32_MAX) {
        vkGetDeviceQueue(ctx.device, ctx.sparseBindingFamily, 0u, &ctx.sparseBindingQueue);
    } else {
        ctx.sparseBindingQueue = ctx.graphicsQueue;
    }

    VmaVulkanFunctions vulkanFunctions{};
    const VkResult importResult = vmaImportVulkanFunctionsFromVolk(
        ctx.instance, ctx.device, &vulkanFunctions);
    if (importResult != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vmaImportVulkanFunctionsFromVolk (rc=%d)\n",
                     static_cast<int>(importResult));
        return false;
    }

    VmaAllocatorCreateInfo allocInfo{};
    allocInfo.physicalDevice = ctx.physicalDevice;
    allocInfo.device = ctx.device;
    allocInfo.instance = ctx.instance;
    allocInfo.vulkanApiVersion = kVulkanApiVersion;
    allocInfo.pVulkanFunctions = &vulkanFunctions;

    if (vmaCreateAllocator(&allocInfo, &ctx.allocator) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vmaCreateAllocator\n");
        return false;
    }
    return true;
}

void DestroyContext(Context& ctx) {
    if (ctx.allocator != VK_NULL_HANDLE) vmaDestroyAllocator(ctx.allocator);
    if (ctx.device != VK_NULL_HANDLE) vkDestroyDevice(ctx.device, nullptr);
    if (ctx.instance != VK_NULL_HANDLE) vkDestroyInstance(ctx.instance, nullptr);
}

}  // namespace vma_bench

// Variant implementations + harness split into separate translation units
// for readability — see main.cpp for measurement loop.

int main(int argc, char** argv);
