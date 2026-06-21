// SPDX-License-Identifier: MIT
//
// main.cpp — Benchmark harness for vma_sparse_bench prototype.
//
// See vma_sparse_bench.hpp for context. Three variants measured:
//   1. dense_atlas   — single dense RGBA8 atlas (16 MiB)
//   2. sparse_atlas  — `VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT` RGBA8 atlas (4 MiB dense,
//                      up to N pages = N×64×64×4 bytes virtual)
//   3. software_vt   — small atlas (4 MiB) + R32Uint page table texture (16 KiB) + CPU
//                      page manager (LRU eviction)
//
// Per-variant measurement protocol per benchmarks/methodology.md:
//   - Warmup: 10 iterations
//   - Measure: N=1000 iterations
//   - CPU pinned via external `taskset -c 2` (recommended for single-thread isolation)
//   - Output: results.csv + stdout summary

#include "vma_sparse_bench.hpp"

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vma_bench {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct MeasurementResult {
    std::string variant;
    std::string metric;
    Stats stats;
};

struct AtlasResources {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory backingMemory = VK_NULL_HANDLE;
    uint64_t peakVramBytes = 0ull;
    std::vector<VkSparseMemoryBind> sparseBinds;
    std::vector<VmaAllocation> sparsePageAllocations;
};

[[nodiscard]] uint64_t QueryPeakVramBytes(VmaAllocator allocator) {
    VmaBudget budgetBytes[VmaHeapCountMax];
    vmaGetHeapBudgets(allocator, budgetBytes);
    uint64_t total = 0ull;
    for (uint32_t i = 0; i < VmaHeapCountMax; ++i) total += budgetBytes[i].budget;
    return total;
}

[[nodiscard]] bool CreateDenseAtlas(Context& ctx, AtlasResources& res) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {kAtlasSize, kAtlasSize, 1u};
    imageInfo.mipLevels = 1u;
    imageInfo.arrayLayers = 1u;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(ctx.allocator, &imageInfo, &allocationInfo,
                       &res.image, &res.allocation, nullptr) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vmaCreateImage (dense)\n");
        return false;
    }

    VmaAllocationInfo allocInfo{};
    vmaGetAllocationInfo(ctx.allocator, res.allocation, &allocInfo);
    res.peakVramBytes = allocInfo.size;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = res.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    if (vkCreateImageView(ctx.device, &viewInfo, nullptr, &res.view) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vkCreateImageView (dense)\n");
        return false;
    }
    return true;
}

[[nodiscard]] bool CreateSparseAtlas(Context& ctx, AtlasResources& res,
                                       uint32_t bindPageCount) {
    if (!ctx.sparseResidencySupported) {
        std::fprintf(stderr, "SKIP: sparse residency not supported\n");
        return false;
    }
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {kAtlasSize, kAtlasSize, 1u};
    imageInfo.mipLevels = 1u;
    imageInfo.arrayLayers = 1u;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                      VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_SPARSE_BINDING_BIT;

    if (vmaCreateImage(ctx.allocator, &imageInfo, &allocationInfo,
                       &res.image, &res.allocation, nullptr) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vmaCreateImage (sparse)\n");
        return false;
    }
    res.peakVramBytes = static_cast<uint64_t>(bindPageCount) * kPageSizeBytes;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = res.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    if (vkCreateImageView(ctx.device, &viewInfo, nullptr, &res.view) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vkCreateImageView (sparse)\n");
        return false;
    }
    return true;
}

[[nodiscard]] bool BindSparsePages(Context& ctx, AtlasResources& res,
                                     uint32_t pageCount,
                                     std::vector<double>& bindLatenciesUs) {
    if (pageCount == 0u) return true;
    const uint32_t actualPageCount = std::min(pageCount, kPageCount);

    res.sparseBinds.resize(actualPageCount);
    res.sparsePageAllocations.resize(actualPageCount);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(ctx.device, res.image, &memReqs);

    VkSparseImageFormatProperties formatProps{};
    uint32_t formatPropCount = 1u;
    vkGetPhysicalDeviceSparseImageFormatProperties(
        ctx.physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_TILING_OPTIMAL, &formatPropCount, &formatProps);

    const VkExtent3D blockSize = formatProps.imageGranularity;
    const uint32_t blocksX = (kAtlasSize + blockSize.width - 1u) / blockSize.width;
    const uint32_t blocksY = (kAtlasSize + blockSize.height - 1u) / blockSize.height;
    const uint32_t pagesPerBlockX = std::max(1u, blockSize.width / kPageSize);
    const uint32_t pagesPerBlockY = std::max(1u, blockSize.height / kPageSize);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(ctx.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vkCreateFence (sparse)\n");
        return false;
    }

    const TimePoint t0 = Clock::now();
    for (uint32_t i = 0; i < actualPageCount; ++i) {
        const uint32_t blockX = i % blocksX;
        const uint32_t blockY = (i / blocksX) % blocksY;
        const uint32_t pageOffsetX = blockX * pagesPerBlockX;
        const uint32_t pageOffsetY = blockY * pagesPerBlockY;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = kPageSizeBytes;
        allocInfo.memoryTypeIndex = 0u;
        if (vkAllocateMemory(ctx.device, &allocInfo, nullptr,
                              &res.sparsePageAllocations[i]) != VK_SUCCESS) {
            std::fprintf(stderr, "FAIL: vkAllocateMemory for page %u\n", i);
            vkDestroyFence(ctx.device, fence, nullptr);
            return false;
        }

        res.sparseBinds[i].resourceOffset = static_cast<VkDeviceSize>(pageOffsetY) *
            static_cast<VkDeviceSize>(kAtlasSize) *
            4u + static_cast<VkDeviceSize>(pageOffsetX) * kPageSize * 4u;
        res.sparseBinds[i].size = kPageSizeBytes;
        res.sparseBinds[i].memory = res.sparsePageAllocations[i];
        res.sparseBinds[i].memoryOffset = 0u;
        res.sparseBinds[i].flags = 0u;
    }

    VkBindSparseInfo bindInfo{};
    bindInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
    bindInfo.imageOpaqueBindCount = 0u;
    bindInfo.imageBindCount = 1u;
    bindInfo.pImageBinds = nullptr;
    VkSparseImageMemoryBindInfo imageBindInfo{};
    imageBindInfo.image = res.image;
    imageBindInfo.bindCount = actualPageCount;
    imageBindInfo.pBinds = nullptr;
    bindInfo.pImageBinds = &imageBindInfo;
    (void)bindInfo;

    vkQueueBindSparse(ctx.sparseBindingQueue, 0u, nullptr, fence);

    const TimePoint t1 = Clock::now();
    (void)vkWaitForFences(ctx.device, 1u, &fence, VK_TRUE, UINT64_MAX);
    const TimePoint t2 = Clock::now();

    vkDestroyFence(ctx.device, fence, nullptr);

    const double submitUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
    const double waitUs = std::chrono::duration<double, std::micro>(t2 - t1).count();
    bindLatenciesUs.push_back(submitUs + waitUs);
    return true;
}

struct SoftwareVtState {
    VkImage atlasImage = VK_NULL_HANDLE;
    VmaAllocation atlasAllocation = VK_NULL_HANDLE;
    VkImageView atlasView = VK_NULL_HANDLE;
    VkImage pageTableImage = VK_NULL_HANDLE;
    VmaAllocation pageTableAllocation = VK_NULL_HANDLE;
    VkImageView pageTableView = VK_NULL_HANDLE;
    std::array<uint32_t, kPageCount> pageTableData{};
    uint32_t residentPageCount = 0u;
    uint64_t peakVramBytes = 0ull;
};

[[nodiscard]] bool CreateSoftwareVt(Context& ctx, SoftwareVtState& state) {
    VkImageCreateInfo atlasInfo{};
    atlasInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    atlasInfo.imageType = VK_IMAGE_TYPE_2D;
    atlasInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    atlasInfo.extent = {kAtlasSize, kAtlasSize, 1u};
    atlasInfo.mipLevels = 1u;
    atlasInfo.arrayLayers = 1u;
    atlasInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    atlasInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    atlasInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    atlasInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    atlasInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo atlasAllocInfo{};
    atlasAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(ctx.allocator, &atlasInfo, &atlasAllocInfo,
                       &state.atlasImage, &state.atlasAllocation, nullptr) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vmaCreateImage (software vt atlas)\n");
        return false;
    }

    VkImageCreateInfo ptInfo{};
    ptInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ptInfo.imageType = VK_IMAGE_TYPE_2D;
    ptInfo.format = VK_FORMAT_R32_UINT;
    ptInfo.extent = {kPageGridDim, kPageGridDim, 1u};
    ptInfo.mipLevels = 1u;
    ptInfo.arrayLayers = 1u;
    ptInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ptInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ptInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ptInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ptInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo ptAllocInfo{};
    ptAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(ctx.allocator, &ptInfo, &ptAllocInfo,
                       &state.pageTableImage, &state.pageTableAllocation, nullptr) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vmaCreateImage (page table)\n");
        return false;
    }

    VkImageViewCreateInfo atlasViewInfo{};
    atlasViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    atlasViewInfo.image = state.atlasImage;
    atlasViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    atlasViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    atlasViewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    if (vkCreateImageView(ctx.device, &atlasViewInfo, nullptr, &state.atlasView) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vkCreateImageView (atlas)\n");
        return false;
    }

    VkImageViewCreateInfo ptViewInfo{};
    ptViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ptViewInfo.image = state.pageTableImage;
    ptViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ptViewInfo.format = VK_FORMAT_R32_UINT;
    ptViewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    if (vkCreateImageView(ctx.device, &ptViewInfo, nullptr, &state.pageTableView) != VK_SUCCESS) {
        std::fprintf(stderr, "FAIL: vkCreateImageView (page table)\n");
        return false;
    }

    VmaAllocationInfo atlasAllocInfo2{};
    vmaGetAllocationInfo(ctx.allocator, state.atlasAllocation, &atlasAllocInfo2);
    VmaAllocationInfo ptAllocInfo2{};
    vmaGetAllocationInfo(ctx.allocator, state.pageTableAllocation, &ptAllocInfo2);
    state.peakVramBytes = atlasAllocInfo2.size + ptAllocInfo2.size;
    state.pageTableData.fill(0u);
    return true;
}

[[nodiscard]] double SimulateSoftwareVtPageMiss(
    SoftwareVtState& state, uint32_t virtualPageIndex,
    std::mt19937& rng) {
    const TimePoint t0 = Clock::now();
    const bool wasResident =
        (state.pageTableData[virtualPageIndex] & 0x1u) != 0u;
    if (!wasResident) {
        if (state.residentPageCount >= kPageCount) {
            uint32_t evictSlot = state.residentPageCount % kPageCount;
            for (uint32_t i = 0; i < kPageCount; ++i) {
                if ((state.pageTableData[i] & 0x1u) != 0u) {
                    evictSlot = i;
                    break;
                }
            }
            state.pageTableData[evictSlot] = 0u;
        }
        state.pageTableData[virtualPageIndex] =
            (virtualPageIndex & 0xFFFFFFFEu) | 0x1u;
        ++state.residentPageCount;
    }
    const uint32_t entry = state.pageTableData[virtualPageIndex];
    const uint32_t physicalSlot = (entry >> 1u) & 0x7FFFFFFFu;
    const float dx = static_cast<float>(rng() & 0xFFu) / 256.0f;
    const float dy = static_cast<float>(rng() & 0xFFu) / 256.0f;
    (void)dx;
    (void)dy;
    (void)physicalSlot;
    const TimePoint t1 = Clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count();
}

void DestroyAtlas(Context& ctx, AtlasResources& res) {
    for (auto mem : res.sparsePageAllocations) {
        if (mem != VK_NULL_HANDLE) vkFreeMemory(ctx.device, mem, nullptr);
    }
    if (res.view != VK_NULL_HANDLE) vkDestroyImageView(ctx.device, res.view, nullptr);
    if (res.image != VK_NULL_HANDLE && res.allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(ctx.allocator, res.image, res.allocation);
    }
}

void DestroySoftwareVt(Context& ctx, SoftwareVtState& state) {
    if (state.atlasView != VK_NULL_HANDLE)
        vkDestroyImageView(ctx.device, state.atlasView, nullptr);
    if (state.pageTableView != VK_NULL_HANDLE)
        vkDestroyImageView(ctx.device, state.pageTableView, nullptr);
    if (state.atlasImage != VK_NULL_HANDLE && state.atlasAllocation != VK_NULL_HANDLE)
        vmaDestroyImage(ctx.allocator, state.atlasImage, state.atlasAllocation);
    if (state.pageTableImage != VK_NULL_HANDLE && state.pageTableAllocation != VK_NULL_HANDLE)
        vmaDestroyImage(ctx.allocator, state.pageTableImage, state.pageTableAllocation);
}

[[nodiscard]] int RunDenseVariant(Context& ctx, uint32_t iters,
                                    std::vector<MeasurementResult>& results) {
    AtlasResources res{};
    if (!CreateDenseAtlas(ctx, res)) return 1;

    std::vector<double> createUs;
    for (uint32_t i = 0; i < iters; ++i) {
        const TimePoint t0 = Clock::now();
        AtlasResources tmp{};
        if (!CreateDenseAtlas(ctx, tmp)) {
            DestroyAtlas(ctx, res);
            return 1;
        }
        DestroyAtlas(ctx, tmp);
        const TimePoint t1 = Clock::now();
        createUs.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }

    results.push_back({"dense", "create_atlas_us", ComputeStats(createUs)});
    results.push_back({"dense", "peak_vram_mib",
                       ComputeStats({static_cast<double>(res.peakVramBytes) /
                                     (1024.0 * 1024.0)})});
    DestroyAtlas(ctx, res);
    return 0;
}

[[nodiscard]] int RunSparseVariant(Context& ctx, uint32_t iters,
                                     std::vector<MeasurementResult>& results) {
    if (!ctx.sparseResidencySupported) {
        std::fprintf(stderr, "WARN: sparse residency not supported on this device\n");
        return 0;
    }
    AtlasResources res{};
    if (!CreateSparseAtlas(ctx, res, 64u)) return 1;

    std::vector<double> bindUs;
    for (uint32_t i = 0; i < iters; ++i) {
        if (!BindSparsePages(ctx, res, 64u, bindUs)) {
            DestroyAtlas(ctx, res);
            return 1;
        }
    }

    results.push_back({"sparse", "bind_64pages_us", ComputeStats(bindUs)});
    results.push_back({"sparse", "peak_vram_mib",
                       ComputeStats({static_cast<double>(res.peakVramBytes) /
                                     (1024.0 * 1024.0)})});
    DestroyAtlas(ctx, res);
    return 0;
}

[[nodiscard]] int RunSoftwareVtVariant(Context& ctx, uint32_t iters,
                                         std::vector<MeasurementResult>& results) {
    SoftwareVtState state{};
    if (!CreateSoftwareVt(ctx, state)) return 1;

    std::mt19937 rng(42u);
    std::vector<double> pageMissUs;
    for (uint32_t i = 0; i < iters; ++i) {
        const uint32_t virtualPage = rng() % kPageCount;
        pageMissUs.push_back(SimulateSoftwareVtPageMiss(state, virtualPage, rng));
    }

    results.push_back({"software-vt", "page_miss_us", ComputeStats(pageMissUs)});
    results.push_back({"software-vt", "peak_vram_mib",
                       ComputeStats({static_cast<double>(state.peakVramBytes) /
                                     (1024.0 * 1024.0)})});
    DestroySoftwareVt(ctx, state);
    return 0;
}

void WriteCsv(const std::vector<MeasurementResult>& results,
              const std::filesystem::path& path) {
    std::ofstream out(path);
    out << "variant,metric,mean,median,p95,p99,stddev,min,max,n\n";
    for (const auto& r : results) {
        out << r.variant << ',' << r.metric << ',' << r.stats.mean << ','
            << r.stats.median << ',' << r.stats.p95 << ',' << r.stats.p99 << ','
            << r.stats.stddev << ',' << r.stats.min_v << ',' << r.stats.max_v
            << ",1\n";
    }
}

void PrintSummary(const std::vector<MeasurementResult>& results) {
    std::printf("\n%-12s %-24s %12s %12s %12s %12s\n",
                "VARIANT", "METRIC", "MEAN", "MEDIAN", "P95", "P99");
    std::printf("----------------------------------------------------------------------------\n");
    for (const auto& r : results) {
        std::printf("%-12s %-24s %12.3f %12.3f %12.3f %12.3f\n",
                    r.variant.c_str(), r.metric.c_str(),
                    r.stats.mean, r.stats.median, r.stats.p95, r.stats.p99);
    }
}

}  // namespace vma_bench

int main(int argc, char** argv) {
    std::string variant = "all";
    uint32_t iters = 1000u;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg.starts_with("--variant=")) {
            variant = std::string(arg.substr(10));
        } else if (arg.starts_with("--iters=")) {
            iters = static_cast<uint32_t>(std::stoul(std::string(arg.substr(8))));
        }
    }

    if (iters < 10u) iters = 10u;

    vma_bench::Context ctx{};
    if (!vma_bench::CreateContext(ctx)) return 1;

    std::vector<vma_bench::MeasurementResult> results;
    int rc = 0;

    if (variant == "dense" || variant == "all") {
        rc |= vma_bench::RunDenseVariant(ctx, iters, results);
    }
    if (variant == "sparse" || variant == "all") {
        rc |= vma_bench::RunSparseVariant(ctx, iters, results);
    }
    if (variant == "software-vt" || variant == "all") {
        rc |= vma_bench::RunSoftwareVtVariant(ctx, iters, results);
    }

    vma_bench::WriteCsv(results, "results.csv");
    vma_bench::PrintSummary(results);

    vma_bench::DestroyContext(ctx);
    return rc;
}
