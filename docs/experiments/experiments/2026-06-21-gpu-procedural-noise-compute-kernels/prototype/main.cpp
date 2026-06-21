// SPDX-License-Identifier: MIT
// Standalone Vulkan 1.4 compute harness for gpu-procedural-noise-compute-kernels.
// 6 noise variants × 4096 chunks × 1000 iters with GPU timestamp queries.
//
// Build: see CMakeLists.txt. Requires Vulkan 1.4 headers, glslc, NVIDIA driver
// 555+ on RTX 3060 Ti (Ampere GA104). Single GPU vendor validated.
// Output: results.csv + human-readable summary printed to stdout.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace {

constexpr uint32_t kChunkVoxels = 512u;        // 8 * 8 * 8
constexpr uint32_t kWorkgroupSize = 64u;       // sweet spot per Nsight Ampere guidance
constexpr uint32_t kChunksPerDispatch = 4096u; // ~12x12x32 - large scene generation
constexpr uint32_t kDispatchGroups = (kChunksPerDispatch * kChunkVoxels) / kWorkgroupSize;
constexpr uint32_t kIterations = 1000u;
constexpr uint32_t kWarmup = 10u;
constexpr uint32_t kTimestampCount = 2u;       // top + bottom of pipe
constexpr const char* kCsvPath = "results.csv";

// 6 variants: each compiled as a separate SPIR-V file from the same source.
struct Variant {
    std::string name;
    std::string spvPath;
    const char* description;
};

const std::vector<Variant> kVariants = {
    {"VALUE",         "noise_value.spv",         "3D Value noise (cheapest baseline)"},
    {"PERLIN",        "noise_perlin.spv",        "3D Perlin improved (pure ALU, ~77 inst)"},
    {"SIMPLEX",       "noise_simplex.spv",       "3D Simplex (Gustavson, ~71 inst)"},
    {"OPENSIMPLEX2",  "noise_opensimplex2.spv",  "3D OpenSimplex2 (KdotJPG, BCC lattice)"},
    {"WORLEY",        "noise_worley.spv",        "3D Worley F1 cellular (27 cells, expensive)"},
};

struct VulkanState {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkQueryPool timestampPool = VK_NULL_HANDLE;
    uint32_t timestampPeriod = 0;  // ns per tick
    std::vector<VkShaderModule> shaderModules;
    std::vector<VkPipeline> pipelines;
    std::vector<VkPipelineLayout> pipelineLayouts;
    VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descSets;     // per variant
    VkBuffer ssbo = VK_NULL_HANDLE;
    VkDeviceMemory ssboMemory = VK_NULL_HANDLE;
};

struct Stats {
    double mean = 0;
    double median = 0;
    double p95 = 0;
    double p99 = 0;
    double stddev = 0;
    double min = 0;
    double max = 0;
};

Stats ComputeStats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = 0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
    s.min = samples.front();
    s.max = samples.back();
    double var = 0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    return s;
}

#define VK_CHECK(call)                                                          \
    do {                                                                         \
        VkResult r = (call);                                                     \
        if (r != VK_SUCCESS) {                                                   \
            std::cerr << "Vulkan error at " << __FILE__ << ":" << __LINE__        \
                      << " : " << r << "\n";                                     \
            std::abort();                                                        \
        }                                                                        \
    } while (0)

bool LoadSpirv(const std::string& path, std::vector<uint32_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    out.resize(size / sizeof(uint32_t));
    f.read(reinterpret_cast<char*>(out.data()), size);
    return true;
}

uint32_t FindComputeFamily(VkPhysicalDevice dev) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());
    for (uint32_t i = 0; i < count; i++) {
        if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            return i;
        }
    }
    return UINT32_MAX;
}

void InitVulkan(VulkanState& s) {
    // Application info - Vulkan 1.4
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "gpu-noise-bench";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "experiment-harness";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo icInfo{};
    icInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    icInfo.pApplicationInfo = &appInfo;
    VK_CHECK(vkCreateInstance(&icInfo, nullptr, &s.instance));

    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(s.instance, &devCount, nullptr);
    std::vector<VkPhysicalDevice> devs(devCount);
    vkEnumeratePhysicalDevices(s.instance, &devCount, devs.data());
    // Pick first NVIDIA device (RTX 3060 Ti per hardware-profile.md)
    for (auto d : devs) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(d, &props);
        if (props.vendorID == 0x10DE) {  // NVIDIA
            s.physicalDevice = d;
            break;
        }
    }
    if (s.physicalDevice == VK_NULL_HANDLE) s.physicalDevice = devs[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(s.physicalDevice, &props);
    std::cout << "GPU: " << props.deviceName << " (Vulkan " << VK_VERSION_MAJOR(props.apiVersion)
              << "." << VK_VERSION_MINOR(props.apiVersion) << "." << VK_VERSION_PATCH(props.apiVersion) << ")\n";

    uint32_t computeFamily = FindComputeFamily(s.physicalDevice);
    if (computeFamily == UINT32_MAX) {
        std::cerr << "No compute queue family\n";
        std::abort();
    }
    s.timestampPeriod = props.limits.timestampPeriod;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qInfo{};
    qInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qInfo.queueFamilyIndex = computeFamily;
    qInfo.queueCount = 1;
    qInfo.pQueuePriorities = &prio;

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo devInfo{};
    devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos = &qInfo;
    devInfo.pEnabledFeatures = &features;

    // Vulkan 1.4 requires declaring Vulkan 1.3/1.4 features via pNext chain.
    VkPhysicalDeviceVulkan13Features v13{};
    v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    v13.synchronization2 = VK_TRUE;
    v13.dynamicRendering = VK_TRUE;
    v13.pNext = nullptr;
    VkPhysicalDeviceVulkan14Features v14{};
    v14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    v14.globalPriorityQuery = VK_TRUE;
    v14.pNext = &v13;

    devInfo.pNext = &v14;
    VK_CHECK(vkCreateDevice(s.physicalDevice, &devInfo, nullptr, &s.device));

    vkGetDeviceQueue(s.device, computeFamily, 0, &s.computeQueue);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = computeFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(s.device, &poolInfo, nullptr, &s.commandPool));

    // Timestamp query pool - 2 timestamps per iteration (top + bottom)
    VkQueryPoolCreateInfo qpInfo{};
    qpInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qpInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qpInfo.queryCount = kTimestampCount;
    VK_CHECK(vkCreateQueryPool(s.device, &qpInfo, nullptr, &s.timestampPool));
}

void CreateSSBO(VulkanState& s) {
    VkDeviceSize size = kChunksPerDispatch * kChunkVoxels * sizeof(float);
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(s.device, &bufInfo, nullptr, &s.ssbo));

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(s.device, s.ssbo, &memReq);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(s.physicalDevice, &memProps);
    uint32_t memType = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((memReq.memoryTypeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memType = i;
            break;
        }
    }
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memType;
    VK_CHECK(vkAllocateMemory(s.device, &allocInfo, nullptr, &s.ssboMemory));
    VK_CHECK(vkBindBufferMemory(s.device, s.ssbo, s.ssboMemory, 0));
}

void CreateDescriptorsAndPipelines(VulkanState& s) {
    // Descriptor set layout: 1 SSBO binding
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo dslInfo{};
    dslInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslInfo.bindingCount = 1;
    dslInfo.pBindings = &binding;
    VK_CHECK(vkCreateDescriptorSetLayout(s.device, &dslInfo, nullptr, &s.descSetLayout));

    // Push constant range: vec4 (chunkOrigin.xyz, seed)
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(float) * 4;

    s.shaderModules.resize(kVariants.size());
    s.pipelines.resize(kVariants.size());
    s.pipelineLayouts.resize(kVariants.size());
    s.descSets.resize(kVariants.size());

    // Pool size: 1 descriptor per variant
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(kVariants.size());

    VkDescriptorPoolCreateInfo dpInfo{};
    dpInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpInfo.maxSets = static_cast<uint32_t>(kVariants.size());
    dpInfo.poolSizeCount = 1;
    dpInfo.pPoolSizes = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(s.device, &dpInfo, nullptr, &s.descPool));

    for (size_t i = 0; i < kVariants.size(); i++) {
        std::vector<uint32_t> spirv;
        if (!LoadSpirv(kVariants[i].spvPath, spirv)) {
            std::cerr << "Failed to load SPIR-V: " << kVariants[i].spvPath << "\n";
            std::abort();
        }
        VkShaderModuleCreateInfo smInfo{};
        smInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smInfo.codeSize = spirv.size() * sizeof(uint32_t);
        smInfo.pCode = spirv.data();
        VK_CHECK(vkCreateShaderModule(s.device, &smInfo, nullptr, &s.shaderModules[i]));

        VkPipelineLayoutCreateInfo plInfo{};
        plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plInfo.setLayoutCount = 1;
        plInfo.pSetLayouts = &s.descSetLayout;
        plInfo.pushConstantRangeCount = 1;
        plInfo.pPushConstantRanges = &pcRange;
        VK_CHECK(vkCreatePipelineLayout(s.device, &plInfo, nullptr, &s.pipelineLayouts[i]));

        VkComputePipelineCreateInfo cpInfo{};
        cpInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cpInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        cpInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        cpInfo.stage.module = s.shaderModules[i];
        cpInfo.stage.pName = "main";
        cpInfo.layout = s.pipelineLayouts[i];
        VK_CHECK(vkCreateComputePipelines(s.device, VK_NULL_HANDLE, 1, &cpInfo, nullptr, &s.pipelines[i]));

        VkDescriptorSetAllocateInfo dsInfo{};
        dsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsInfo.descriptorPool = s.descPool;
        dsInfo.descriptorSetCount = 1;
        dsInfo.pSetLayouts = &s.descSetLayout;
        VK_CHECK(vkAllocateDescriptorSets(s.device, &dsInfo, &s.descSets[i]));

        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = s.ssbo;
        bufInfo.offset = 0;
        bufInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = s.descSets[i];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &bufInfo;
        vkUpdateDescriptorSets(s.device, 1, &write, 0, nullptr);
    }
}

double RunVariant(VulkanState& s, uint32_t variantIndex) {
    std::vector<double> samples;
    samples.reserve(kIterations);

    // Allocate one command buffer reused across iters.
    VkCommandBufferAllocateInfo cbInfo{};
    cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbInfo.commandPool = s.commandPool;
    cbInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(s.device, &cbInfo, &cmd));

    for (uint32_t iter = 0; iter < kWarmup + kIterations; iter++) {
        VK_CHECK(vkResetCommandPool(s.device, s.commandPool, 0));
        VK_CHECK(vkResetCommandBuffer(cmd, 0));

        VkCommandBufferBeginInfo cbBegin{};
        cbBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &cbBegin));

        vkCmdResetQueryPool(cmd, s.timestampPool, 0, kTimestampCount);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s.timestampPool, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s.pipelines[variantIndex]);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s.pipelineLayouts[variantIndex],
                                0, 1, &s.descSets[variantIndex], 0, nullptr);
        float pc[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        vkCmdPushConstants(cmd, s.pipelineLayouts[variantIndex], VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(pc), pc);
        vkCmdDispatch(cmd, kDispatchGroups, 1, 1);

        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s.timestampPool, 1);
        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(s.computeQueue, 1, &submit, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(s.computeQueue));

        uint64_t timestamps[kTimestampCount];
        VK_CHECK(vkGetQueryPoolResults(s.device, s.timestampPool, 0, kTimestampCount,
                                        sizeof(timestamps), timestamps, sizeof(uint64_t),
                                        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
        uint64_t diff = timestamps[1] - timestamps[0];
        double ms = (double)diff * (double)s.timestampPeriod / 1e6;
        if (iter >= kWarmup) {
            samples.push_back(ms);
        }
    }

    vkFreeCommandBuffers(s.device, s.commandPool, 1, &cmd);
    Stats st = ComputeStats(std::move(samples));
    std::printf("  %-12s mean=%7.4f ms  median=%7.4f  p95=%7.4f  p99=%7.4f  std=%6.4f  min=%7.4f  max=%7.4f\n",
                kVariants[variantIndex].name.c_str(), st.mean, st.median, st.p95, st.p99, st.stddev, st.min, st.max);
    return st.mean;
}

void Cleanup(VulkanState& s) {
    for (auto p : s.pipelines) vkDestroyPipeline(s.device, p, nullptr);
    for (auto pl : s.pipelineLayouts) vkDestroyPipelineLayout(s.device, pl, nullptr);
    for (auto sm : s.shaderModules) vkDestroyShaderModule(s.device, sm, nullptr);
    vkDestroyDescriptorPool(s.device, s.descPool, nullptr);
    vkDestroyDescriptorSetLayout(s.device, s.descSetLayout, nullptr);
    vkDestroyBuffer(s.device, s.ssbo, nullptr);
    vkFreeMemory(s.device, s.ssboMemory, nullptr);
    vkDestroyQueryPool(s.device, s.timestampPool, nullptr);
    vkDestroyCommandPool(s.device, s.commandPool, nullptr);
    vkDestroyDevice(s.device, nullptr);
    vkDestroyInstance(s.instance, nullptr);
}

void WriteCsv(const std::vector<std::pair<std::string, double>>& results) {
    std::ofstream f(kCsvPath);
    f << "variant,mean_ms\n";
    for (auto& [name, mean] : results) {
        f << name << "," << mean << "\n";
    }
}

}  // namespace

int main() {
    std::printf("=== gpu-procedural-noise-compute-kernels benchmark ===\n");
    std::printf("Dev host: NVIDIA RTX 3060 Ti (Ampere GA104, Vulkan 1.4)\n");
    std::printf("Dispatch: %u chunks × %u voxels = %u invocations per dispatch\n",
                kChunksPerDispatch, kChunkVoxels, kChunksPerDispatch * kChunkVoxels);
    std::printf("Workgroup size: %u threads (sweet spot for Ampere occupancy per Nsight)\n", kWorkgroupSize);
    std::printf("Iterations: %u (warmup %u)\n\n", kIterations, kWarmup);

    VulkanState s;
    InitVulkan(s);
    CreateSSBO(s);
    CreateDescriptorsAndPipelines(s);

    std::vector<std::pair<std::string, double>> results;
    for (size_t i = 0; i < kVariants.size(); i++) {
        std::printf("Variant %zu/%zu: %s - %s\n", i + 1, kVariants.size(),
                    kVariants[i].name.c_str(), kVariants[i].description);
        double mean = RunVariant(s, i);
        results.emplace_back(kVariants[i].name, mean);
    }

    WriteCsv(results);
    std::printf("\nResults written to %s\n", kCsvPath);

    Cleanup(s);
    return 0;
}
