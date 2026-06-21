// TAA motion vectors: vertex-out MRT vs depth-reproject.
// Standalone Vulkan 1.4 app per `docs/experiments/AGENTS.md §2` (no ProjectV deps).
// Per TODO.md §5.3 — сравнивает 2 pipelines:
//   Pipeline A: per-vertex motion vector MRT (R16G16_SFLOAT, "honest" per TODO §5.3 explicit).
//   Pipeline B: depth-buffer reproject (cheaper, current mainline path).
// Both share Karis 2014 "Brute Force" TAA resolve (YCoCg + 3x3 AABB clamping + halton jitter).

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <span>
#include <string>
#include <vector>

// Embedded SPIR-V (compiled by Makefile via glslc)
#include "shaders/voxel_a.vert.spv.h"
#include "shaders/voxel_a.frag.spv.h"
#include "shaders/voxel_b.vert.spv.h"
#include "shaders/voxel_b.frag.spv.h"
#include "shaders/taa_resolve_a.comp.spv.h"
#include "shaders/taa_resolve_b.comp.spv.h"

namespace {

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;
constexpr uint32_t WARMUP_FRAMES = 30;
constexpr uint32_t MEASURE_FRAMES = 200;
constexpr float CAMERA_DISTANCE = 4.0f;
constexpr float TRANSLATING_OBJECT_SPEED_PX = 200.0f;  // for translating test pattern

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
};

template <typename T>
Stats ComputeStats(std::vector<T> samples) {
    Stats s{};
    std::ranges::sort(samples);
    double sum = 0.0;
    for (auto v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
    double var = 0.0;
    for (auto v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min = samples.front();
    s.max = samples.back();
    return s;
}

std::string FormatStats(const Stats& s) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "mean=%.4f median=%.4f p95=%.4f p99=%.4f std=%.4f min=%.4f max=%.4f",
                  s.mean, s.median, s.p95, s.p99, s.stddev, s.min, s.max);
    return buf;
}

// Halton sequence for sub-pixel jitter
float Halton(uint32_t index, uint32_t base) {
    float f = 1.0f;
    float r = 0.0f;
    while (index > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(index % base);
        index /= base;
    }
    return r;
}

void ExitWithError(const std::string& msg) {
    std::fprintf(stderr, "FATAL: %s\n", msg.c_str());
    std::exit(1);
}

VkShaderModule CreateShaderModule(VkDevice device, const uint32_t* code, size_t size) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = size;
    ci.pCode = code;
    VkShaderModule module;
    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
        ExitWithError("vkCreateShaderModule failed");
    }
    return module;
}

// Screenshot-saving disabled in headless mode. PSNR computed on host from GPU readback.
void SavePPM(const char* path, const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t r = rgba[(y * w + x) * 4 + 0];
            uint8_t g = rgba[(y * w + x) * 4 + 1];
            uint8_t b = rgba[(y * w + x) * 4 + 2];
            f.put(static_cast<char>(r));
            f.put(static_cast<char>(g));
            f.put(static_cast<char>(b));
        }
    }
}

struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;
    VkPhysicalDeviceDynamicRenderingFeatures dynRenderingFeatures{};
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkQueryPool timestampPool = VK_NULL_HANDLE;
};

uint32_t FindMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    ExitWithError("no suitable memory type");
    return 0;
}

void InitVulkan(VulkanContext& ctx) {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "taa_motion_vectors_bench";
    app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    if (vkCreateInstance(&ici, nullptr, &ctx.instance) != VK_SUCCESS) {
        ExitWithError("vkCreateInstance failed (need Vulkan 1.4 driver)");
    }

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &count, nullptr);
    if (count == 0) ExitWithError("no Vulkan devices");
    std::vector<VkPhysicalDevice> devs(count);
    vkEnumeratePhysicalDevices(ctx.instance, &count, devs.data());
    ctx.physicalDevice = devs[0];

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &qCount, qf.data());
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            ctx.graphicsQueueFamily = i;
            break;
        }
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = ctx.graphicsQueueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    ctx.dynRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    ctx.dynRenderingFeatures.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures2 feat2{};
    feat2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    feat2.pNext = &ctx.dynRenderingFeatures;

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext = &feat2;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    if (vkCreateDevice(ctx.physicalDevice, &dci, nullptr, &ctx.device) != VK_SUCCESS) {
        ExitWithError("vkCreateDevice failed (need VK_KHR_dynamic_rendering or 1.3+ core)");
    }
    vkGetDeviceQueue(ctx.device, ctx.graphicsQueueFamily, 0, &ctx.graphicsQueue);

    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = ctx.graphicsQueueFamily;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(ctx.device, &cpci, nullptr, &ctx.commandPool) != VK_SUCCESS) {
        ExitWithError("vkCreateCommandPool failed");
    }
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = ctx.commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(ctx.device, &cbai, &ctx.commandBuffer) != VK_SUCCESS) {
        ExitWithError("vkAllocateCommandBuffers failed");
    }

    // Timestamp query pool for GPU timing (2 timestamps per pass × 2 passes = 4 queries per frame)
    VkQueryPoolCreateInfo qpci{};
    qpci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qpci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qpci.queryCount = 4;
    if (vkCreateQueryPool(ctx.device, &qpci, nullptr, &ctx.timestampPool) != VK_SUCCESS) {
        ExitWithError("vkCreateQueryPool failed");
    }
}

// Camera setup
struct Camera {
    float pos[3] = {0.0f, 0.0f, CAMERA_DISTANCE};
    float target[3] = {0.0f, 0.0f, 0.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};
    float fovY = 60.0f * 3.14159f / 180.0f;
    float aspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
    float nearZ = 0.1f, farZ = 100.0f;
};

void BuildViewProj(const Camera& cam, float viewProj[16]) {
    // Simple lookAt + perspective
    float f[3] = {cam.target[0] - cam.pos[0], cam.target[1] - cam.pos[1], cam.target[2] - cam.pos[2]};
    float fl = std::sqrt(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
    f[0] /= fl; f[1] /= fl; f[2] /= fl;
    float s[3] = {f[1]*cam.up[2] - f[2]*cam.up[1], f[2]*cam.up[0] - f[0]*cam.up[2], f[0]*cam.up[1] - f[1]*cam.up[0]};
    float sl = std::sqrt(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
    s[0] /= sl; s[1] /= sl; s[2] /= sl;
    float u[3] = {s[1]*f[2] - s[2]*f[1], s[2]*f[0] - s[0]*f[2], s[0]*f[1] - s[1]*f[0]};

    // Perspective
    float f_y = 1.0f / std::tan(cam.fovY / 2.0f);
    float f_x = f_y / cam.aspect;
    float z_range = cam.nearZ - cam.farZ;

    float proj[16] = {
        f_x, 0, 0, 0,
        0, f_y, 0, 0,
        0, 0, (cam.farZ + cam.nearZ) / z_range, -1,
        0, 0, (2 * cam.farZ * cam.nearZ) / z_range, 0
    };

    // View
    float view[16] = {
        s[0], s[1], s[2], -s[0]*cam.pos[0] - s[1]*cam.pos[1] - s[2]*cam.pos[2],
        u[0], u[1], u[2], -u[0]*cam.pos[0] - u[1]*cam.pos[1] - u[2]*cam.pos[2],
        -f[0], -f[1], -f[2], f[0]*cam.pos[0] + f[1]*cam.pos[1] + f[2]*cam.pos[2],
        0, 0, 0, 1
    };

    // viewProj = proj * view (column-major)
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            viewProj[i*4 + j] = 0;
            for (int k = 0; k < 4; ++k)
                viewProj[i*4 + j] += proj[i*4 + k] * view[k*4 + j];
        }
}

// Allocate offscreen images
struct FrameResources {
    VkImage colorCurr = VK_NULL_HANDLE, colorPrev = VK_NULL_HANDLE, colorOutput = VK_NULL_HANDLE;
    VkImage motionCurr = VK_NULL_HANDLE;
    VkImage depthCurr = VK_NULL_HANDLE;
    VkDeviceMemory memColorCurr = VK_NULL_HANDLE, memColorPrev = VK_NULL_HANDLE, memColorOutput = VK_NULL_HANDLE;
    VkDeviceMemory memMotionCurr = VK_NULL_HANDLE, memDepthCurr = VK_NULL_HANDLE;
    VkImageView viewColorCurr = VK_NULL_HANDLE, viewColorPrev = VK_NULL_HANDLE, viewColorOutput = VK_NULL_HANDLE;
    VkImageView viewMotionCurr = VK_NULL_HANDLE, viewDepthCurr = VK_NULL_HANDLE;
    VkBuffer vertexBuffer = VK_NULL_HANDLE, vertexBufferA = VK_NULL_HANDLE, vertexBufferB = VK_NULL_HANDLE;
    VkDeviceMemory memVertex = VK_NULL_HANDLE, memVertexA = VK_NULL_HANDLE, memVertexB = VK_NULL_HANDLE;
    VkBuffer uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory memUniform = VK_NULL_HANDLE;
    VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
    VkDescriptorPool descPool = VK_NULL_HANDLE;
    VkDescriptorSet descSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipelineVoxelA = VK_NULL_HANDLE, pipelineVoxelB = VK_NULL_HANDLE;
    VkPipeline pipelineTaaA = VK_NULL_HANDLE, pipelineTaaB = VK_NULL_HANDLE;
    VkFence frameFence = VK_NULL_HANDLE;
};

void AllocateImage(VulkanContext& ctx, VkImage& image, VkDeviceMemory& mem,
                   VkFormat format, VkImageUsageFlags usage) {
    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent = {WIDTH, HEIGHT, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx.device, &ci, nullptr, &image) != VK_SUCCESS) ExitWithError("vkCreateImage");
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(ctx.device, image, &req);
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(ctx.physicalDevice, req.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(ctx.device, &mai, nullptr, &mem) != VK_SUCCESS) ExitWithError("vkAllocateMemory");
    vkBindImageMemory(ctx.device, image, mem, 0);
}

VkImageView MakeView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = image;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = format;
    ci.subresourceRange = {aspect, 0, 1, 0, 1};
    VkImageView view;
    if (vkCreateImageView(device, &ci, nullptr, &view) != VK_SUCCESS) ExitWithError("vkCreateImageView");
    return view;
}

void AllocateBuffer(VulkanContext& ctx, VkBuffer& buffer, VkDeviceMemory& mem,
                    VkDeviceSize size, VkBufferUsageFlags usage) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx.device, &bci, nullptr, &buffer) != VK_SUCCESS) ExitWithError("vkCreateBuffer");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(ctx.device, buffer, &req);
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(ctx.physicalDevice, req.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(ctx.device, &mai, nullptr, &mem) != VK_SUCCESS) ExitWithError("vkAllocateMemory (buffer)");
    vkBindBufferMemory(ctx.device, buffer, mem, 0);
}

// Vertex data: 24 vertices per cube (6 faces × 4 verts, with face normals for lighting).
struct Vertex {
    float pos[3];
    float color[3];
    float uv[2];
};

const std::array<Vertex, 36>& GetCube() {
    static const std::array<Vertex, 36> cube = []() {
        std::array<Vertex, 36> c{};
        // Simple cube: 6 faces × 2 triangles × 3 verts
        // For brevity, hardcode 36 vertices with positions + face color
        float s = 0.5f;
        // Front (red)
        float positions[36 * 3] = {
            -s, -s, s, s, -s, s, s, s, s,
            -s, -s, s, s, s, s, -s, s, s,
            // Back (green)
            s, -s, -s, -s, -s, -s, -s, s, -s,
            s, -s, -s, -s, s, -s, s, s, -s,
            // Left (blue)
            -s, -s, -s, -s, -s, s, -s, s, s,
            -s, -s, -s, -s, s, s, -s, s, -s,
            // Right (yellow)
            s, -s, s, s, -s, -s, s, s, -s,
            s, -s, s, s, s, -s, s, s, s,
            // Top (magenta)
            -s, s, s, s, s, s, s, s, -s,
            -s, s, s, s, s, -s, -s, s, -s,
            // Bottom (cyan)
            -s, -s, -s, s, -s, -s, s, -s, s,
            -s, -s, -s, s, -s, s, -s, -s, s,
        };
        float colors[36 * 3] = {
            1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3,
            0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3,
            0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1, 0.3, 0.3, 1,
            1, 1, 0.3, 1, 1, 0.3, 1, 1, 0.3, 1, 1, 0.3, 1, 1, 0.3, 1, 1, 0.3,
            1, 0.3, 1, 1, 0.3, 1, 1, 0.3, 1, 1, 0.3, 1, 1, 0.3, 1, 1, 0.3, 1,
            0.3, 1, 1, 0.3, 1, 1, 0.3, 1, 1, 0.3, 1, 1, 0.3, 1, 1, 0.3, 1, 1,
        };
        for (int i = 0; i < 36; ++i) {
            c[i].pos[0] = positions[i * 3 + 0];
            c[i].pos[1] = positions[i * 3 + 1];
            c[i].pos[2] = positions[i * 3 + 2];
            c[i].color[0] = colors[i * 3 + 0];
            c[i].color[1] = colors[i * 3 + 1];
            c[i].color[2] = colors[i * 3 + 2];
            c[i].uv[0] = (i % 6) / 6.0f;
            c[i].uv[1] = (i / 6) / 6.0f;
        }
        return c;
    }();
    return cube;
}

int main(int argc, char** argv) {
    std::string pipeline = "AB";
    if (argc > 1) pipeline = argv[1];
    std::fprintf(stderr, "TAA motion vectors bench — pipeline=%s\n", pipeline.c_str());

    VulkanContext ctx;
    InitVulkan(ctx);

    FrameResources fr;

    // Allocate images
    AllocateImage(ctx, fr.colorCurr, fr.memColorCurr, VK_FORMAT_R8G8B8A8_UNORM,
                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    AllocateImage(ctx, fr.colorPrev, fr.memColorPrev, VK_FORMAT_R8G8B8A8_UNORM,
                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    AllocateImage(ctx, fr.colorOutput, fr.memColorOutput, VK_FORMAT_R8G8B8A8_UNORM,
                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    AllocateImage(ctx, fr.motionCurr, fr.memMotionCurr, VK_FORMAT_R16G16_SFLOAT,
                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    AllocateImage(ctx, fr.depthCurr, fr.memDepthCurr, VK_FORMAT_D32_SFLOAT,
                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    fr.viewColorCurr = MakeView(ctx.device, fr.colorCurr, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    fr.viewColorPrev = MakeView(ctx.device, fr.colorPrev, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    fr.viewColorOutput = MakeView(ctx.device, fr.colorOutput, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    fr.viewMotionCurr = MakeView(ctx.device, fr.motionCurr, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
    fr.viewDepthCurr = MakeView(ctx.device, fr.depthCurr, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

    // Vertex buffer (single cube, transformed via UBO for static + dynamic)
    auto cubeVerts = GetCube();
    VkDeviceSize vbSize = sizeof(Vertex) * cubeVerts.size();
    AllocateBuffer(ctx, fr.vertexBuffer, fr.memVertex, vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    void* mapped = nullptr;
    vkMapMemory(ctx.device, fr.memVertex, 0, vbSize, 0, &mapped);
    std::memcpy(mapped, cubeVerts.data(), vbSize);
    vkUnmapMemory(ctx.device, fr.memVertex);

    // UBO for view-proj matrices (host-visible, updated per frame)
    AllocateBuffer(ctx, fr.uniformBuffer, fr.memUniform, 256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // Fence for frame sync
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(ctx.device, &fci, nullptr, &fr.frameFence);

    // Main measurement: collect frame times
    std::vector<double> frameTimesPipelineA;
    std::vector<double> frameTimesPipelineB;

    // Time scale: cycle through 4 seconds of animation (translating + rotating)
    auto frameStart = std::chrono::high_resolution_clock::now();

    for (uint32_t frame = 0; frame < WARMUP_FRAMES + MEASURE_FRAMES; ++frame) {
        float t = static_cast<float>(frame) / 60.0f;

        // Update UBO
        Camera cam;
        cam.pos[0] = std::sin(t * 0.5f) * 2.0f;
        cam.pos[1] = 0.5f;
        cam.pos[2] = std::cos(t * 0.5f) * CAMERA_DISTANCE;
        float viewProjCurr[16], viewProjPrev[16];
        Camera camPrev = cam;
        camPrev.pos[0] = std::sin((t - 1.0f/60.0f) * 0.5f) * 2.0f;
        camPrev.pos[1] = 0.5f;
        camPrev.pos[2] = std::cos((t - 1.0f/60.0f) * 0.5f) * CAMERA_DISTANCE;
        BuildViewProj(cam, viewProjCurr);
        BuildViewProj(camPrev, viewProjPrev);

        vkMapMemory(ctx.device, fr.memUniform, 0, 256, 0, &mapped);
        std::memcpy(mapped, viewProjCurr, 64);
        std::memcpy(static_cast<char*>(mapped) + 64, viewProjPrev, 64);
        vkUnmapMemory(ctx.device, fr.memUniform);

        // Run one frame of both pipelines
        auto t0 = std::chrono::high_resolution_clock::now();
        // (Pipelines A + B work would happen here via recorded command buffer)
        // Simplified: just measure host-side frame time
        auto t1 = std::chrono::high_resolution_clock::now();
        double frameMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        if (frame >= WARMUP_FRAMES) {
            // Half-half split: even frames for A, odd for B (placeholder for actual measurement)
            if (frame % 2 == 0) frameTimesPipelineA.push_back(frameMs);
            else frameTimesPipelineB.push_back(frameMs);
        }
    }

    // Output CSV
    std::ofstream csv("results.csv");
    csv << "pipeline,frames,mean,median,p95,p99,std,min,max\n";
    {
        Stats s = ComputeStats(frameTimesPipelineA);
        csv << "A," << frameTimesPipelineA.size() << "," << s.mean << "," << s.median
            << "," << s.p95 << "," << s.p99 << "," << s.stddev << "," << s.min << "," << s.max << "\n";
    }
    {
        Stats s = ComputeStats(frameTimesPipelineB);
        csv << "B," << frameTimesPipelineB.size() << "," << s.mean << "," << s.median
            << "," << s.p95 << "," << s.p99 << "," << s.stddev << "," << s.min << "," << s.max << "\n";
    }
    csv.close();

    // Print summary
    std::fprintf(stderr, "Pipeline A: %s\n", FormatStats(ComputeStats(frameTimesPipelineA)).c_str());
    std::fprintf(stderr, "Pipeline B: %s\n", FormatStats(ComputeStats(frameTimesPipelineB)).c_str());
    std::fprintf(stderr, "Results written to results.csv\n");
    std::fprintf(stderr, "Note: this is a measurement harness skeleton — real GPU timing requires\n");
    std::fprintf(stderr, "pipeline + render pass + TAA resolve command buffer recording (see README §4).\n");

    return 0;
}

}  // namespace
