#pragma once
// Four allocation strategies simulating ProjectV's growing transient SSBO
// workload. Each strategy implements `runFrame(...)` returning per-frame stats.
//
// Workload (per frame) — mirrors ProjectV's planned Stage 2.x/3.x/5.x transient
// SSBO additions (per `experiments/.../README.md` §1 hypothesis):
//   1) persistent mapped:  material visual SSBO   256 KiB (host-visible)
//   2) persistent mapped:  cluster grid SSBO       27 KiB (host-visible)
//   3) N small SSBOs:      PackedFace-like          256 B each, N chunks
//   4) medium SSBO:        NanoVDB transient        1 MiB
//   5) large SSBO:         BLAS pool entry          4 MiB
//   6) image:              HZB mip or VCT layer     4 MiB (R32G32_UINT)
//
// Every K=200 frames: world edit spike -> re-flatten NanoVDB (extra 8 MiB).
//
// All four strategies share the SAME workload; only the allocator strategy
// differs. This isolates the allocator design choice as the single independent
// variable, per `benchmarks/methodology.md` §3 (control vs hypothesis).

#include "harness.hpp"
#include "benchmark.hpp"

#include <vector>

namespace prototype {

struct WorkloadParams {
    uint32_t smallSsboCount = 64;
    uint32_t worldEditEveryKFrames = 200;
    size_t worldEditExtraBytes = 8 * 1024 * 1024;
};

struct GpuResource {
    VkBuffer buffer = VK_NULL_HANDLE;  // OR VkImage; union via `kind`.
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    bool isImage = false;
};

// Persistent resources shared across all strategies (created once, lifetime
// = whole experiment). Kept outside the strategies because they're not part
// of the per-frame allocation question.
inline void createPersistent(DeviceContext& ctx, VkBuffer& matBuf, VmaAllocation& matAlloc,
                             VkBuffer& cluBuf, VmaAllocation& cluAlloc) {
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    bi.size = 256 * 1024;
    vmaCreateBuffer(ctx.allocator, &bi, &ai, &matBuf, &matAlloc, nullptr);

    bi.size = 27 * 1024;
    vmaCreateBuffer(ctx.allocator, &bi, &ai, &cluBuf, &cluAlloc, nullptr);
}

// Helper: free per-frame resources properly.
inline void freeFrameResources(DeviceContext& ctx, std::vector<GpuResource>& r) {
    for (auto& g : r) {
        if (g.isImage) vmaDestroyImage(ctx.allocator, g.image, g.allocation);
        else vmaDestroyBuffer(ctx.allocator, g.buffer, g.allocation);
    }
    r.clear();
}

// Helper: query budget for the device-local heap (heap 0 typically).
inline void fillBudgetSample(DeviceContext& ctx, FrameSample& s) {
    if (!ctx.memoryBudgetExt) return;
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(ctx.allocator, budgets);
    for (uint32_t i = 0; i < ctx.memProps.memoryHeapCount; ++i) {
        if (ctx.memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            s.heapBudgetBytes = budgets[i].budget;
            s.heapUsageBytes = budgets[i].usage;
            break;
        }
    }
}

// ============================================================================
// Strategy A: VMA default allocator (CURRENT ProjectV mainline behavior).
// No pool, no budget tracking, no hard cap. Just `vmaCreateBuffer/Image`.
// ============================================================================
class StrategyA_Default {
public:
    StrategyA_Default(DeviceContext& ctx) : ctx_(ctx) {}
    ~StrategyA_Default() = default;

    FrameSample runFrame(uint32_t frameIdx, const WorkloadParams& p) {
        FrameSample s{};
        Stopwatch sw;
        sw.start();
        std::vector<GpuResource> fr;

        for (uint32_t i = 0; i < p.smallSsboCount; ++i) {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = 256;
            bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            GpuResource g;
            if (vmaCreateBuffer(ctx_.allocator, &bi, &ai, &g.buffer, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        }
        {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = 1024 * 1024;
            bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            GpuResource g;
            if (vmaCreateBuffer(ctx_.allocator, &bi, &ai, &g.buffer, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        }
        {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = 4 * 1024 * 1024;
            bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            GpuResource g;
            if (vmaCreateBuffer(ctx_.allocator, &bi, &ai, &g.buffer, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        }
        {
            VkImageCreateInfo ii{};
            ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            ii.imageType = VK_IMAGE_TYPE_2D;
            ii.format = VK_FORMAT_R32G32_UINT;
            ii.extent = {1024, 1024, 1};
            ii.mipLevels = 1;
            ii.arrayLayers = 1;
            ii.samples = VK_SAMPLE_COUNT_1_BIT;
            ii.tiling = VK_IMAGE_TILING_OPTIMAL;
            ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            GpuResource g;
            g.isImage = true;
            if (vmaCreateImage(ctx_.allocator, &ii, &ai, &g.image, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        }
        if (p.worldEditEveryKFrames > 0 &&
            (frameIdx % p.worldEditEveryKFrames) == (p.worldEditEveryKFrames - 1)) {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = p.worldEditExtraBytes;
            bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            GpuResource g;
            if (vmaCreateBuffer(ctx_.allocator, &bi, &ai, &g.buffer, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        }

        s.allocLatencyUs = sw.stopUs();
        s.freeCount = fr.size();
        s.liveAllocations = fr.size();
        freeFrameResources(ctx_, fr);
        fillBudgetSample(ctx_, s);
        return s;
    }

    const char* name() const { return "A_Default"; }

private:
    DeviceContext& ctx_;
};

// ============================================================================
// Strategy B: VMA default allocator + budget tracking enabled.
// Adds `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` + `vmaSetCurrentFrameIndex`
// per frame. NO pool — same allocation path as A, but budget is observable.
// Hypothesis: catches budget exhaustion without preventing it.
// ============================================================================
class StrategyB_BudgetTrack {
public:
    StrategyB_BudgetTrack(DeviceContext& ctx) : ctx_(ctx) {}
    ~StrategyB_BudgetTrack() = default;
    // Same per-frame code as A but uses WITHIN_BUDGET flag.
    // We delegate the alloc body by aliasing the same code as A, then
    // marking each allocation WITHIN_BUDGET.
    FrameSample runFrame(uint32_t frameIdx, const WorkloadParams& p) {
        FrameSample s{};
        vmaSetCurrentFrameIndex(ctx_.allocator, frameIdx);
        Stopwatch sw;
        sw.start();
        std::vector<GpuResource> fr;

        auto tryBuffer = [&](VkDeviceSize sz, bool within) {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = sz;
            bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            if (within) ai.flags |= VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
            GpuResource g;
            if (vmaCreateBuffer(ctx_.allocator, &bi, &ai, &g.buffer, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        };

        for (uint32_t i = 0; i < p.smallSsboCount; ++i) tryBuffer(256, true);
        tryBuffer(1024 * 1024, true);
        tryBuffer(4 * 1024 * 1024, true);

        {
            VkImageCreateInfo ii{};
            ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            ii.imageType = VK_IMAGE_TYPE_2D;
            ii.format = VK_FORMAT_R32G32_UINT;
            ii.extent = {1024, 1024, 1};
            ii.mipLevels = 1;
            ii.arrayLayers = 1;
            ii.samples = VK_SAMPLE_COUNT_1_BIT;
            ii.tiling = VK_IMAGE_TILING_OPTIMAL;
            ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            ai.flags |= VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
            GpuResource g;
            g.isImage = true;
            if (vmaCreateImage(ctx_.allocator, &ii, &ai, &g.image, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        }

        if (p.worldEditEveryKFrames > 0 &&
            (frameIdx % p.worldEditEveryKFrames) == (p.worldEditEveryKFrames - 1)) {
            // World edit goes WITHOUT budget cap (must succeed or trigger
            // graceful degradation explicitly). Simulates critical allocation.
            tryBuffer(p.worldEditExtraBytes, false);
        }

        s.allocLatencyUs = sw.stopUs();
        s.freeCount = fr.size();
        s.liveAllocations = fr.size();
        freeFrameResources(ctx_, fr);
        fillBudgetSample(ctx_, s);
        return s;
    }
    const char* name() const { return "B_BudgetTrack"; }
private:
    DeviceContext& ctx_;
};

// ============================================================================
// Strategy C: VMA linear pool (ring buffer) per-frame.
// `VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT` + `maxBlockCount=1` -> ring buffer.
// Per frame: re-create pool (or wrap cursor). Allocations are linear bump;
// free-at-once at end of frame via `vmaResetPool`.
// Hypothesis: sub-microsecond per-alloc, predictable latency.
// ============================================================================
class StrategyC_LinearPool {
public:
    StrategyC_LinearPool(DeviceContext& ctx) : ctx_(ctx) {
        pickMemoryTypeForDeviceLocal();
    }
    ~StrategyC_LinearPool() = default;

    FrameSample runFrame(uint32_t frameIdx, const WorkloadParams& p) {
        FrameSample s{};
        vmaSetCurrentFrameIndex(ctx_.allocator, frameIdx);
        Stopwatch sw;
        sw.start();

        // Per-frame pool (created and destroyed each frame to simulate ring
        // buffer wrapping). In production this would be a single pre-allocated
        // pool with reset cursor; we recreate to model the realistic cost of
        // pool-bring-up cost in worst case. Set blockSize large enough for
        // typical frame workload + headroom.
        VmaPoolCreateInfo pi{};
        pi.memoryTypeIndex = devLocalType_;
        pi.flags = VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT;
        pi.blockSize = 64ULL * 1024 * 1024;  // 64 MiB ring block.
        pi.maxBlockCount = 1;  // required for ring-buffer per VMA docs.

        VmaPool pool;
        if (vmaCreatePool(ctx_.allocator, &pi, &pool) != VK_SUCCESS) {
            s.failedCount++;
            s.allocLatencyUs = sw.stopUs();
            fillBudgetSample(ctx_, s);
            return s;
        }

        std::vector<GpuResource> fr;
        auto tryBuf = [&](VkDeviceSize sz) {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = sz;
            bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            VmaAllocationCreateInfo ai{};
            ai.pool = pool;
            GpuResource g;
            if (vmaCreateBuffer(ctx_.allocator, &bi, &ai, &g.buffer, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        };

        for (uint32_t i = 0; i < p.smallSsboCount; ++i) tryBuf(256);
        tryBuf(1024 * 1024);
        tryBuf(4 * 1024 * 1024);

        {
            VkImageCreateInfo ii{};
            ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            ii.imageType = VK_IMAGE_TYPE_2D;
            ii.format = VK_FORMAT_R32G32_UINT;
            ii.extent = {1024, 1024, 1};
            ii.mipLevels = 1;
            ii.arrayLayers = 1;
            ii.samples = VK_SAMPLE_COUNT_1_BIT;
            ii.tiling = VK_IMAGE_TILING_OPTIMAL;
            ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VmaAllocationCreateInfo ai{};
            ai.pool = pool;
            GpuResource g;
            g.isImage = true;
            if (vmaCreateImage(ctx_.allocator, &ii, &ai, &g.image, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        }
        if (p.worldEditEveryKFrames > 0 &&
            (frameIdx % p.worldEditEveryKFrames) == (p.worldEditEveryKFrames - 1)) {
            tryBuf(p.worldEditExtraBytes);
        }

        s.allocLatencyUs = sw.stopUs();
        s.freeCount = fr.size();
        s.liveAllocations = fr.size();
        freeFrameResources(ctx_, fr);
        // Pool destruction = ring-buffer reset.
        vmaDestroyPool(ctx_.allocator, pool);
        fillBudgetSample(ctx_, s);
        return s;
    }
    const char* name() const { return "C_LinearPool"; }

private:
    DeviceContext& ctx_;
    uint32_t devLocalType_ = UINT32_MAX;
    void pickMemoryTypeForDeviceLocal() {
        for (uint32_t i = 0; i < ctx_.memProps.memoryTypeCount; ++i) {
            if (ctx_.memProps.memoryTypes[i].propertyFlags &
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                devLocalType_ = i;
                return;
            }
        }
        devLocalType_ = 0;
    }
};

// ============================================================================
// Strategy E: PRE-CREATED linear ring buffer pool, reused across frames.
// Production-realistic: pool is created once at startup (like Frostbite Frame
// Graph transient resources). Allocations are linear bumps; the ring buffer
// wraps when the cursor reaches end-of-block (single block per VMA docs).
// No pool destruction between frames — this measures the steady-state cost.
// ============================================================================
class StrategyE_PreCreatedRing {
public:
    StrategyE_PreCreatedRing(DeviceContext& ctx) : ctx_(ctx) {
        pickMemoryTypeForDeviceLocal();
        // Pre-create pool (single block, ring buffer).
        VmaPoolCreateInfo pi{};
        pi.memoryTypeIndex = devLocalType_;
        pi.flags = VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT;
        pi.blockSize = 64ULL * 1024 * 1024;
        pi.maxBlockCount = 1;
        if (vmaCreatePool(ctx_.allocator, &pi, &pool_) != VK_SUCCESS) {
            fprintf(stderr, "[E] pool create FAILED\n");
            pool_ = VK_NULL_HANDLE;
        } else {
            fprintf(stderr, "[E] pre-created 64 MiB ring pool\n");
        }
    }
    ~StrategyE_PreCreatedRing() {
        if (pool_) vmaDestroyPool(ctx_.allocator, pool_);
    }

    FrameSample runFrame(uint32_t frameIdx, const WorkloadParams& p) {
        FrameSample s{};
        vmaSetCurrentFrameIndex(ctx_.allocator, frameIdx);
        if (!pool_) {
            s.failedCount++;
            return s;
        }
        Stopwatch sw;
        sw.start();

        std::vector<GpuResource> fr;
        auto tryBuf = [&](VkDeviceSize sz) {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = sz;
            bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            VmaAllocationCreateInfo ai{};
            ai.pool = pool_;
            GpuResource g;
            if (vmaCreateBuffer(ctx_.allocator, &bi, &ai, &g.buffer, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        };
        for (uint32_t i = 0; i < p.smallSsboCount; ++i) tryBuf(256);
        tryBuf(1024 * 1024);
        tryBuf(4 * 1024 * 1024);
        {
            VkImageCreateInfo ii{};
            ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            ii.imageType = VK_IMAGE_TYPE_2D;
            ii.format = VK_FORMAT_R32G32_UINT;
            ii.extent = {1024, 1024, 1};
            ii.mipLevels = 1;
            ii.arrayLayers = 1;
            ii.samples = VK_SAMPLE_COUNT_1_BIT;
            ii.tiling = VK_IMAGE_TILING_OPTIMAL;
            ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VmaAllocationCreateInfo ai{};
            ai.pool = pool_;
            GpuResource g;
            g.isImage = true;
            if (vmaCreateImage(ctx_.allocator, &ii, &ai, &g.image, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        }
        if (p.worldEditEveryKFrames > 0 &&
            (frameIdx % p.worldEditEveryKFrames) == (p.worldEditEveryKFrames - 1)) {
            tryBuf(p.worldEditExtraBytes);
        }
        s.allocLatencyUs = sw.stopUs();
        s.freeCount = fr.size();
        s.liveAllocations = fr.size();
        // Capture peak BEFORE freeing.
        fillBudgetSample(ctx_, s);
        freeFrameResources(ctx_, fr);
        return s;
    }
    const char* name() const { return "E_PreCreatedRing"; }

private:
    DeviceContext& ctx_;
    VmaPool pool_ = VK_NULL_HANDLE;
    uint32_t devLocalType_ = UINT32_MAX;
    void pickMemoryTypeForDeviceLocal() {
        for (uint32_t i = 0; i < ctx_.memProps.memoryTypeCount; ++i) {
            if (ctx_.memProps.memoryTypes[i].propertyFlags &
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                devLocalType_ = i;
                return;
            }
        }
        devLocalType_ = 0;
    }
};

class StrategyD_DoubleBuffer {
public:
    StrategyD_DoubleBuffer(DeviceContext& ctx) : ctx_(ctx) {
        pickMemoryTypeForDeviceLocal();
    }
    ~StrategyD_DoubleBuffer() = default;
    FrameSample runFrame(uint32_t frameIdx, const WorkloadParams& p) {
        FrameSample s{};
        vmaSetCurrentFrameIndex(ctx_.allocator, frameIdx);
        Stopwatch sw;
        sw.start();

        VmaPoolCreateInfo pi{};
        pi.memoryTypeIndex = devLocalType_;
        pi.flags = VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT;
        pi.blockSize = 64ULL * 1024 * 1024;
        pi.maxBlockCount = 1;

        VmaPool pool;
        if (vmaCreatePool(ctx_.allocator, &pi, &pool) != VK_SUCCESS) {
            s.failedCount++;
            s.allocLatencyUs = sw.stopUs();
            fillBudgetSample(ctx_, s);
            return s;
        }

        std::vector<GpuResource> fr;
        auto tryBuf = [&](VkDeviceSize sz) {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = sz;
            bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            VmaAllocationCreateInfo ai{};
            ai.pool = pool;
            ai.flags |= VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;  // hard cap.
            GpuResource g;
            if (vmaCreateBuffer(ctx_.allocator, &bi, &ai, &g.buffer, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        };
        for (uint32_t i = 0; i < p.smallSsboCount; ++i) tryBuf(256);
        tryBuf(1024 * 1024);
        tryBuf(4 * 1024 * 1024);

        {
            VkImageCreateInfo ii{};
            ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            ii.imageType = VK_IMAGE_TYPE_2D;
            ii.format = VK_FORMAT_R32G32_UINT;
            ii.extent = {1024, 1024, 1};
            ii.mipLevels = 1;
            ii.arrayLayers = 1;
            ii.samples = VK_SAMPLE_COUNT_1_BIT;
            ii.tiling = VK_IMAGE_TILING_OPTIMAL;
            ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VmaAllocationCreateInfo ai{};
            ai.pool = pool;
            ai.flags |= VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
            GpuResource g;
            g.isImage = true;
            if (vmaCreateImage(ctx_.allocator, &ii, &ai, &g.image, &g.allocation, nullptr)
                != VK_SUCCESS) s.failedCount++;
            else fr.push_back(g);
        }
        if (p.worldEditEveryKFrames > 0 &&
            (frameIdx % p.worldEditEveryKFrames) == (p.worldEditEveryKFrames - 1)) {
            tryBuf(p.worldEditExtraBytes);  // world edit critical
        }
        s.allocLatencyUs = sw.stopUs();
        s.freeCount = fr.size();
        s.liveAllocations = fr.size();
        freeFrameResources(ctx_, fr);
        vmaDestroyPool(ctx_.allocator, pool);
        fillBudgetSample(ctx_, s);
        return s;
    }
    const char* name() const { return "D_DoubleBuffer"; }
private:
    DeviceContext& ctx_;
    uint32_t devLocalType_ = UINT32_MAX;
    void pickMemoryTypeForDeviceLocal() {
        for (uint32_t i = 0; i < ctx_.memProps.memoryTypeCount; ++i) {
            if (ctx_.memProps.memoryTypes[i].propertyFlags &
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                devLocalType_ = i;
                return;
            }
        }
        devLocalType_ = 0;
    }
};

}  // namespace prototype
