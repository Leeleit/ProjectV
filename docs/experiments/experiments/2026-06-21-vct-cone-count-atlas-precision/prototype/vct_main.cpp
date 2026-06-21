// SPDX-License-Identifier: MIT
// Standalone Vulkan 1.4 compute harness for VCT cone count × atlas precision sweet spot.
// 3 cone counts (6/12/24) × 3 atlas precisions (R8/R16F/R32F) × 1024 reference × 1 scene.
//
// Hypothesis: 6 cones × R16G16B16A16_SFLOAT = sweet spot (per Crassin 2011 GIVoxels §5
// 5-6 cones diffuse baseline + OGRE 2019 R8 banding risk + Panteleev 2014 R16F baseline).
//
// Build: see CMakeLists.txt. Requires Vulkan 1.4 headers, glslc, NVIDIA driver 555+ on
// RTX 3060 Ti (Ampere GA104). Single GPU vendor validated.
//
// Output: results.csv (12 measurements per config: 9 measured + 3 references).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace {

constexpr uint32_t kAtlasSize = 128u;            // 128^3 atlas (production-ish)
constexpr uint32_t kAtlasVoxels = kAtlasSize * kAtlasSize * kAtlasSize;
constexpr uint32_t kSampleGrid = 1024u;           // 1024x1024 PSNR sample grid (1M work items)
constexpr uint32_t kIterations = 100u;
constexpr uint32_t kWarmup = 10u;
constexpr uint32_t kMaxMipLevels = 8u;            // log2(128) + 1

#define VK_CHECK(call)                                                          \
    do { VkResult r_ = (call);                                                  \
         if (r_ != VK_SUCCESS) { std::cerr << "VK err " << r_ << " @ "           \
                       << __FILE__ << ":" << __LINE__ << "\n"; std::exit(1); }  \
    } while (0)

struct AtlasFormat { std::string name; VkFormat fmt; uint32_t bpp; };
const std::vector<AtlasFormat> kFormats = {
    {"R8G8B8A8_UNORM",     VK_FORMAT_R8G8B8A8_UNORM,       4u},
    {"R16G16B16A16_SFLOAT", VK_FORMAT_R16G16B16A16_SFLOAT, 8u},
    {"R32G32B32A32_SFLOAT", VK_FORMAT_R32G32B32A32_SFLOAT,16u},
};

struct ConeVariant { std::string name; std::string spv; uint32_t n; };
const std::vector<ConeVariant> kCones = {
    {"CONE_6",     "cone_march_CONE_6.spv",      6u},
    {"CONE_12",    "cone_march_CONE_12.spv",    12u},
    {"CONE_24",    "cone_march_CONE_24.spv",    24u},
    {"CONE_1024",  "cone_march_CONE_1024.spv",1024u},
};

struct AtlasRes {
    VkImage image; VkDeviceMemory mem; VkImageView view; VkSampler samp;
    VkFormat fmt; uint32_t mips; uint64_t vram;
};

struct VState {
    VkInstance inst; VkPhysicalDevice phys; VkDevice dev;
    VkQueue queue; uint32_t qf;
    VkCommandPool pool;
    VkQueryPool tsPool;
    uint32_t tsPeriod;
    std::vector<VkShaderModule> coneShaders;
    std::vector<VkPipeline> conePipes;
    std::vector<VkPipelineLayout> coneLayouts;
    VkDescriptorSetLayout coneDsl;
    VkDescriptorPool descPool;
    std::vector<VkDescriptorSet> coneSets;  // 4 cones × 3 formats = 12
    std::vector<AtlasRes> atlases;          // 3
    VkImage outImg; VkDeviceMemory outMem; VkImageView outView;
    VkBuffer hostBuf; VkDeviceMemory hostMem;
    VkPipelineLayout coneLayout;  // single shared layout
};

// Forward decls
uint16_t f32_to_f16(float v);

uint32_t FindMem(VkPhysicalDevice p, uint32_t bits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(p, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((bits & (1u<<i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    std::cerr << "No memory type\n"; std::exit(1);
}

VkShaderModule LoadShader(VkDevice dev, const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { std::cerr << "Shader not found: " << path << "\n"; std::exit(1); }
    size_t sz = f.tellg(); f.seekg(0);
    std::vector<char> code(sz); f.read(code.data(), sz);
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = sz; ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule m;
    VK_CHECK(vkCreateShaderModule(dev, &ci, nullptr, &m));
    return m;
}

void InitVk(VState& s) {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "vct_bench"; app.apiVersion = VK_API_VERSION_1_4;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VK_CHECK(vkCreateInstance(&ici, nullptr, &s.inst));
    uint32_t dc = 0; vkEnumeratePhysicalDevices(s.inst, &dc, nullptr);
    std::vector<VkPhysicalDevice> pds(dc);
    vkEnumeratePhysicalDevices(s.inst, &dc, pds.data());
    s.phys = pds[0];
    VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(s.phys, &pp);
    std::cout << "GPU: " << pp.deviceName << " (Vulkan "
              << VK_VERSION_MAJOR(pp.apiVersion) << "." << VK_VERSION_MINOR(pp.apiVersion) << ")\n";
    s.tsPeriod = pp.limits.timestampPeriod;
    uint32_t qfc = 0; vkGetPhysicalDeviceQueueFamilyProperties(s.phys, &qfc, nullptr);
    std::vector<VkQueueFamilyProperties> qfs(qfc);
    vkGetPhysicalDeviceQueueFamilyProperties(s.phys, &qfc, qfs.data());
    s.qf = UINT32_MAX;
    for (uint32_t i = 0; i < qfc; ++i)
        if (qfs[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { s.qf = i; break; }
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = s.qf; qci.queueCount = 1; qci.pQueuePriorities = &prio;
    VkPhysicalDeviceFeatures feats{};
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci; dci.pEnabledFeatures = &feats;
    VK_CHECK(vkCreateDevice(s.phys, &dci, nullptr, &s.dev));
    vkGetDeviceQueue(s.dev, s.qf, 0, &s.queue);
    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = s.qf;
    VK_CHECK(vkCreateCommandPool(s.dev, &cpci, nullptr, &s.pool));
    VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qpci.queryType = VK_QUERY_TYPE_TIMESTAMP; qpci.queryCount = 2u;
    VK_CHECK(vkCreateQueryPool(s.dev, &qpci, nullptr, &s.tsPool));
}

void CreateAtlases(VState& s) {
    s.atlases.resize(kFormats.size());
    for (size_t i = 0; i < kFormats.size(); ++i) {
        AtlasRes& a = s.atlases[i];
        a.fmt = kFormats[i].fmt;
        a.mips = kMaxMipLevels;
        VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ici.imageType = VK_IMAGE_TYPE_3D;
        ici.format = a.fmt;
        ici.extent = {kAtlasSize, kAtlasSize, kAtlasSize};
        ici.mipLevels = a.mips;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT |
                    VK_IMAGE_USAGE_STORAGE_BIT |
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // for vkCmdBlitImage mip gen
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK(vkCreateImage(s.dev, &ici, nullptr, &a.image));
        VkMemoryRequirements req; vkGetImageMemoryRequirements(s.dev, a.image, &req);
        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = FindMem(s.phys, req.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(s.dev, &mai, nullptr, &a.mem));
        VK_CHECK(vkBindImageMemory(s.dev, a.image, a.mem, 0));
        a.vram = req.size;
        // View (all mips)
        VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivci.image = a.image; ivci.viewType = VK_IMAGE_VIEW_TYPE_3D;
        ivci.format = a.fmt;
        ivci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, a.mips, 0, 1};
        VK_CHECK(vkCreateImageView(s.dev, &ivci, nullptr, &a.view));
        // Sampler (linear filtering + mipmapping)
        VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_LINEAR; sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_CHECK(vkCreateSampler(s.dev, &sci, nullptr, &a.samp));
        // Synthetic CPU content: ground plane at y=64 (gray), sky above (white).
        // Then upload + mip-gen via vkCmdBlitImage.
        std::vector<uint8_t> data(kAtlasVoxels * kFormats[i].bpp);
        for (uint32_t z = 0; z < kAtlasSize; ++z)
        for (uint32_t y = 0; y < kAtlasSize; ++y)
        for (uint32_t x = 0; x < kAtlasSize; ++x) {
            float dist_to_ground = float(y) - 64.0f;
            float sky = (dist_to_ground > 0.0f) ? 1.0f : 0.0f;
            float ground = (dist_to_ground <= 0.0f) ? 1.0f : 0.0f;
            // Add radial obstacle in center (a "boulder")
            float dx = float(x) - float(kAtlasSize)/2.0f;
            float dz = float(z) - float(kAtlasSize)/2.0f;
            float r2 = dx*dx + dz*dz;
            float boulder = (r2 < 30.0f*30.0f && dist_to_ground > -5.0f && dist_to_ground < 5.0f) ? 1.0f : 0.0f;
            float R = ground * 0.3f + sky * 0.9f;
            float G = ground * 0.3f + sky * 0.95f;
            float B = ground * 0.3f + sky * 1.0f;
            float A = ground + sky;  // alpha=1 if solid
            if (boulder > 0.5f) { R = 0.4f; G = 0.2f; B = 0.1f; A = 1.0f; }
            size_t off = (size_t(z)*kAtlasSize*kAtlasSize + size_t(y)*kAtlasSize + x) * kFormats[i].bpp;
            if (a.fmt == VK_FORMAT_R8G8B8A8_UNORM) {
                auto* p = reinterpret_cast<uint8_t*>(&data[off]);
                p[0] = (uint8_t)(R*255); p[1] = (uint8_t)(G*255);
                p[2] = (uint8_t)(B*255); p[3] = (uint8_t)(A*255);
            } else if (a.fmt == VK_FORMAT_R16G16B16A16_SFLOAT) {
                auto* p = reinterpret_cast<uint16_t*>(&data[off]);
                p[0] = uint16_t(f32_to_f16(R)); p[1] = uint16_t(f32_to_f16(G));
                p[2] = uint16_t(f32_to_f16(B)); p[3] = uint16_t(f32_to_f16(A));
            } else {  // R32_SFLOAT
                auto* p = reinterpret_cast<float*>(&data[off]);
                p[0] = R; p[1] = G; p[2] = B; p[3] = A;
            }
        }
        // Upload to mip 0
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = data.size(); bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkBuffer stagingBuf; VK_CHECK(vkCreateBuffer(s.dev, &bci, nullptr, &stagingBuf));
        VkMemoryRequirements sreq; vkGetBufferMemoryRequirements(s.dev, stagingBuf, &sreq);
        VkMemoryAllocateInfo smai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        smai.allocationSize = sreq.size;
        smai.memoryTypeIndex = FindMem(s.phys, sreq.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkDeviceMemory stagingMem; VK_CHECK(vkAllocateMemory(s.dev, &smai, nullptr, &stagingMem));
        VK_CHECK(vkBindBufferMemory(s.dev, stagingBuf, stagingMem, 0));
        void* mapped; VK_CHECK(vkMapMemory(s.dev, stagingMem, 0, data.size(), 0, &mapped));
        std::memcpy(mapped, data.data(), data.size());
        vkUnmapMemory(s.dev, stagingMem);
        // Record upload + mip chain blit
        VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbai.commandPool = s.pool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
        VkCommandBuffer cmd; VK_CHECK(vkAllocateCommandBuffers(s.dev, &cbai, &cmd));
        VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &cbbi));
        // Mip 0: UNDEFINED → TRANSFER_DST
        VkImageMemoryBarrier b0{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b0.image = a.image;
        b0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b0.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        b0.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b0.srcAccessMask = 0; b0.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &b0);
        VkBufferImageCopy bic{};
        bic.bufferOffset = 0; bic.bufferRowLength = 0; bic.bufferImageHeight = 0;
        bic.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        bic.imageOffset = {0, 0, 0};
        bic.imageExtent = {kAtlasSize, kAtlasSize, kAtlasSize};
        vkCmdCopyBufferToImage(cmd, stagingBuf, a.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);
        // Mip chain blit
        for (uint32_t m = 1; m < a.mips; ++m) {
            uint32_t srcSize = kAtlasSize >> (m - 1);
            uint32_t dstSize = kAtlasSize >> m;
            VkImageBlit blit{};
            blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m-1, 0, 1};
            blit.srcOffsets[0] = {0, 0, 0}; blit.srcOffsets[1] = {int32_t(srcSize), int32_t(srcSize), 1};
            blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m, 0, 1};
            blit.dstOffsets[0] = {0, 0, 0}; blit.dstOffsets[1] = {int32_t(dstSize), int32_t(dstSize), 1};
            // Transition src → TRANSFER_SRC, dst → TRANSFER_DST
            VkImageMemoryBarrier bS{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            bS.image = a.image; bS.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, m-1, 1, 0, 1};
            bS.oldLayout = (m == 1) ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            bS.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            bS.srcAccessMask = (m == 1) ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_TRANSFER_READ_BIT;
            bS.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &bS);
            VkImageMemoryBarrier bD{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            bD.image = a.image; bD.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, m, 1, 0, 1};
            bD.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; bD.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            bD.srcAccessMask = 0; bD.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &bD);
            vkCmdBlitImage(cmd, a.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           a.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
        }
        // Final: all mips → SHADER_READ_ONLY_OPTIMAL
        VkImageMemoryBarrier bf{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        bf.image = a.image; bf.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, a.mips, 0, 1};
        bf.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        bf.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bf.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        bf.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &bf);
        VK_CHECK(vkEndCommandBuffer(cmd));
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(s.queue, 1, &si, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(s.queue));
        vkFreeCommandBuffers(s.dev, s.pool, 1, &cmd);
        vkDestroyBuffer(s.dev, stagingBuf, nullptr);
        vkFreeMemory(s.dev, stagingMem, nullptr);
    }
}

// Helper: float32 to float16 (IEEE 754 binary16)
uint16_t f32_to_f16(float v) {
    uint32_t x; std::memcpy(&x, &v, 4);
    uint32_t sign = (x >> 16) & 0x8000u;
    int32_t exp = int32_t((x >> 23) & 0xffu) - 127 + 15;
    uint32_t mant = x & 0x7fffffu;
    if (exp <= 0) return uint16_t(sign);
    if (exp >= 31) return uint16_t(sign | 0x7c00u);
    return uint16_t(sign | (uint32_t(exp) << 10) | (mant >> 13));
}

void SetupDescriptorsAndPipes(VState& s) {
    // Descriptor set layout: atlas sampler + storage output image
    VkDescriptorSetLayoutBinding b[2]{};
    b[0].binding = 0; b[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b[0].descriptorCount = 1; b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    b[1].binding = 1; b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    b[1].descriptorCount = 1; b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 2; dslci.pBindings = b;
    VK_CHECK(vkCreateDescriptorSetLayout(s.dev, &dslci, nullptr, &s.coneDsl));
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &s.coneDsl;
    VK_CHECK(vkCreatePipelineLayout(s.dev, &plci, nullptr, &s.coneLayout));
    // Output image (initial layout UNDEFINED, will transition to GENERAL in first dispatch)
    VkImageCreateInfo oci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    oci.imageType = VK_IMAGE_TYPE_2D;
    oci.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    oci.extent = {kSampleGrid, kSampleGrid, 1};
    oci.mipLevels = 1; oci.arrayLayers = 1; oci.samples = VK_SAMPLE_COUNT_1_BIT;
    oci.tiling = VK_IMAGE_TILING_OPTIMAL;
    oci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    oci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    oci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(s.dev, &oci, nullptr, &s.outImg));
    VkMemoryRequirements oreq; vkGetImageMemoryRequirements(s.dev, s.outImg, &oreq);
    VkMemoryAllocateInfo omai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    omai.allocationSize = oreq.size;
    omai.memoryTypeIndex = FindMem(s.phys, oreq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(s.dev, &omai, nullptr, &s.outMem));
    VK_CHECK(vkBindImageMemory(s.dev, s.outImg, s.outMem, 0));
    VkImageViewCreateInfo ovci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ovci.image = s.outImg; ovci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ovci.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ovci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(s.dev, &ovci, nullptr, &s.outView));
    // Host readback buffer
    VkBufferCreateInfo hbci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    hbci.size = uint64_t(kSampleGrid) * kSampleGrid * 16;
    hbci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    hbci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(s.dev, &hbci, nullptr, &s.hostBuf));
    VkMemoryRequirements hreq; vkGetBufferMemoryRequirements(s.dev, s.hostBuf, &hreq);
    VkMemoryAllocateInfo hmai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    hmai.allocationSize = hreq.size;
    hmai.memoryTypeIndex = FindMem(s.phys, hreq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK(vkAllocateMemory(s.dev, &hmai, nullptr, &s.hostMem));
    VK_CHECK(vkBindBufferMemory(s.dev, s.hostBuf, s.hostMem, 0));
    // Descriptor pool
    VkDescriptorPoolSize ps[2]{};
    ps[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; ps[0].descriptorCount = 12;
    ps[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; ps[1].descriptorCount = 12;
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 12; dpci.poolSizeCount = 2; dpci.pPoolSizes = ps;
    VK_CHECK(vkCreateDescriptorPool(s.dev, &dpci, nullptr, &s.descPool));
    // Create 4 cone shaders + 4 pipelines
    s.coneShaders.resize(4); s.conePipes.resize(4);
    for (uint32_t i = 0; i < kCones.size(); ++i) {
        s.coneShaders[i] = LoadShader(s.dev, kCones[i].spv);
        VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        cpci.stage.module = s.coneShaders[i];
        cpci.stage.pName = "main";
        cpci.layout = s.coneLayout;
        VK_CHECK(vkCreateComputePipelines(s.dev, VK_NULL_HANDLE, 1, &cpci, nullptr, &s.conePipes[i]));
    }
    // Descriptor sets: 4 cone × 3 format = 12
    s.coneSets.resize(12);
    std::vector<VkDescriptorSetLayout> layouts(12, s.coneDsl);
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = s.descPool; dsai.descriptorSetCount = 12; dsai.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(s.dev, &dsai, s.coneSets.data()));
    for (uint32_t fi = 0; fi < 3; ++fi)
    for (uint32_t ci = 0; ci < 4; ++ci) {
        uint32_t idx = fi * 4 + ci;
        VkDescriptorImageInfo ii[2]{};
        ii[0].sampler = s.atlases[fi].samp; ii[0].imageView = s.atlases[fi].view;
        ii[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ii[1].sampler = VK_NULL_HANDLE; ii[1].imageView = s.outView;
        ii[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet w[2]{};
        w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[0].dstSet = s.coneSets[idx]; w[0].dstBinding = 0; w[0].descriptorCount = 1;
        w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[0].pImageInfo = &ii[0];
        w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[1].dstSet = s.coneSets[idx]; w[1].dstBinding = 1; w[1].descriptorCount = 1;
        w[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w[1].pImageInfo = &ii[1];
        vkUpdateDescriptorSets(s.dev, 2, w, 0, nullptr);
    }
}

double MeasureOnce(VState& s, uint32_t coneIdx, uint32_t fmtIdx) {
    uint32_t dsIdx = fmtIdx * 4 + coneIdx;
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = s.pool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(s.dev, &cbai, &cmd));
    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(cmd, &cbbi));
    // Transition output image UNDEFINED → GENERAL on first call
    static bool outInit = false;
    if (!outInit) {
        VkImageMemoryBarrier bo{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        bo.image = s.outImg; bo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        bo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bo.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        bo.srcAccessMask = 0; bo.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &bo);
        outInit = true;
    }
    vkCmdResetQueryPool(cmd, s.tsPool, 0, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, s.tsPool, 0);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s.conePipes[coneIdx]);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s.coneLayout, 0, 1, &s.coneSets[dsIdx], 0, nullptr);
    vkCmdDispatch(cmd, kSampleGrid/8, kSampleGrid/8, 1);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, s.tsPool, 1);
    VK_CHECK(vkEndCommandBuffer(cmd));
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(s.queue, 1, &si, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(s.queue));
    uint64_t ts[2];
    vkGetQueryPoolResults(s.dev, s.tsPool, 0, 2, sizeof(ts), ts, sizeof(uint64_t),
                          VK_QUERY_RESULT_WAIT_BIT);
    vkFreeCommandBuffers(s.dev, s.pool, 1, &cmd);
    return double(ts[1] - ts[0]) * double(s.tsPeriod) / 1e6;  // ms
}

void ReadbackOutput(VState& s, std::vector<float>& data) {
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = s.pool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
    VkCommandBuffer cmd; VK_CHECK(vkAllocateCommandBuffers(s.dev, &cbai, &cmd));
    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(cmd, &cbbi));
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.image = s.outImg; b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    b.oldLayout = VK_IMAGE_LAYOUT_GENERAL; b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b);
    VkBufferImageCopy bic{};
    bic.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    bic.imageExtent = {kSampleGrid, kSampleGrid, 1};
    vkCmdCopyImageToBuffer(cmd, s.outImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s.hostBuf, 1, &bic);
    // Transition back to GENERAL for next dispatch
    VkImageMemoryBarrier b2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b2.image = s.outImg; b2.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; b2.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b2.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT; b2.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b2);
    VK_CHECK(vkEndCommandBuffer(cmd));
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(s.queue, 1, &si, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(s.queue));
    vkFreeCommandBuffers(s.dev, s.pool, 1, &cmd);
    data.resize(size_t(kSampleGrid) * kSampleGrid * 4);
    void* mapped; VK_CHECK(vkMapMemory(s.dev, s.hostMem, 0, data.size()*4, 0, &mapped));
    std::memcpy(data.data(), mapped, data.size()*4);
    vkUnmapMemory(s.dev, s.hostMem);
}

double PSNR(const std::vector<float>& a, const std::vector<float>& b) {
    double mse = 0; size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) mse += (a[i] - b[i]) * (a[i] - b[i]);
    mse /= n;
    if (mse < 1e-10) return 99.9;
    return 10.0 * std::log10(1.0 / mse);
}

}  // namespace

int main() {
    VState s{};
    InitVk(s);
    CreateAtlases(s);
    SetupDescriptorsAndPipes(s);
    std::cout << "Atlases created (3 formats). VRAM per atlas:\n";
    for (auto& a : s.atlases)
        std::cout << "  " << a.vram / 1024 / 1024 << " MiB (mips=" << a.mips << ")\n";

    // Run measurements: for each (format, cone), warmup then N iters
    std::ofstream csv("results.csv");
    csv << "format,cones,ms_mean,ms_p95,psnr_vs_1024\n";
    std::vector<std::vector<float>> refs(3);
    for (uint32_t fmt = 0; fmt < 3; ++fmt) {
        // Run 1024-cone reference first (compute it once for PSNR baseline)
        for (uint32_t w = 0; w < kWarmup; ++w) MeasureOnce(s, 3, fmt);
        MeasureOnce(s, 3, fmt);  // one extra to ensure output written
        ReadbackOutput(s, refs[fmt]);
        // Compute PSNR reference stats
        double refSum = 0, refMin = refs[fmt][0], refMax = refs[fmt][0];
        for (float v : refs[fmt]) { refSum += v; if (v < refMin) refMin = v; if (v > refMax) refMax = v; }
        std::cout << "Reference (R" << (fmt == 0 ? "8" : fmt == 1 ? "16F" : "32F")
                  << " × 1024): mean=" << (refSum/refs[fmt].size())
                  << ", min=" << refMin << ", max=" << refMax << "\n";
        for (uint32_t cone = 0; cone < 4; ++cone) {
            for (uint32_t w = 0; w < kWarmup; ++w) MeasureOnce(s, cone, fmt);
            std::vector<double> samples;
            samples.reserve(kIterations);
            for (uint32_t i = 0; i < kIterations; ++i)
                samples.push_back(MeasureOnce(s, cone, fmt));
            std::sort(samples.begin(), samples.end());
            double mean = 0; for (double v : samples) mean += v;
            mean /= samples.size();
            double p95 = samples[size_t(samples.size()*0.95)];
            double psnr = 0;
            if (cone == 3) psnr = 99.9;
            else {
                std::vector<float> cur;
                MeasureOnce(s, cone, fmt);
                ReadbackOutput(s, cur);
                psnr = PSNR(refs[fmt], cur);
            }
            std::cout << kFormats[fmt].name << " × " << kCones[cone].name
                      << ": mean=" << mean << " ms, p95=" << p95
                      << " ms, PSNR=" << psnr << " dB\n";
            csv << kFormats[fmt].name << "," << kCones[cone].n << ","
                << mean << "," << p95 << "," << psnr << "\n";
        }
    }
    csv.close();
    std::cout << "Wrote results.csv\n";
    // Cleanup (minimal)
    for (auto& p : s.conePipes) vkDestroyPipeline(s.dev, p, nullptr);
    for (auto& m : s.coneShaders) vkDestroyShaderModule(s.dev, m, nullptr);
    for (auto& a : s.atlases) {
        vkDestroySampler(s.dev, a.samp, nullptr);
        vkDestroyImageView(s.dev, a.view, nullptr);
        vkDestroyImage(s.dev, a.image, nullptr);
        vkFreeMemory(s.dev, a.mem, nullptr);
    }
    vkDestroyPipelineLayout(s.dev, s.coneLayout, nullptr);
    vkDestroyDescriptorSetLayout(s.dev, s.coneDsl, nullptr);
    vkDestroyDescriptorPool(s.dev, s.descPool, nullptr);
    vkDestroyImageView(s.dev, s.outView, nullptr);
    vkDestroyImage(s.dev, s.outImg, nullptr);
    vkFreeMemory(s.dev, s.outMem, nullptr);
    vkDestroyBuffer(s.dev, s.hostBuf, nullptr);
    vkFreeMemory(s.dev, s.hostMem, nullptr);
    vkDestroyQueryPool(s.dev, s.tsPool, nullptr);
    vkDestroyCommandPool(s.dev, s.pool, nullptr);
    vkDestroyDevice(s.dev, nullptr);
    vkDestroyInstance(s.inst, nullptr);
    return 0;
}
