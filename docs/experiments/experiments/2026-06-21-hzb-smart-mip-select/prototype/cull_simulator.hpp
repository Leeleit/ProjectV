#pragma once

#include "scenes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace hzb_smart_mip {

enum class CullStrategy {
    A_UniformMip0,
    B_UniformMipGlobal,
    C_PerChunkStaticMip,
    D_PerChunkDynamicDispatch
};

inline const char *StrategyName(CullStrategy s)
{
    switch (s) {
        case CullStrategy::A_UniformMip0: return "A_UniformMip0";
        case CullStrategy::B_UniformMipGlobal: return "B_UniformMipGlobal";
        case CullStrategy::C_PerChunkStaticMip: return "C_PerChunkStaticMip";
        case CullStrategy::D_PerChunkDynamicDispatch: return "D_PerChunkDynamicDispatch";
    }
    return "unknown";
}

struct CullResult {
    bool culled;
    int mipLevelUsed;
    int texelsTouched;
    double computeCostUs;
};

struct CullConfig {
    int screenWidth = 1920;
    int screenHeight = 1080;
    int conservativePixels = 8;
    int globalMipLevel = 5;
    int maxMipLevel = 10;  // HIZ chain typically 10-11 levels for 1080p
};

inline float ComputeScreenExtentPixels(
    const ChunkAabb &chunk,
    const Camera &cam,
    int screenW,
    int screenH)
{
    ScreenAabb screen{};
    if (!ProjectChunkToScreen(chunk, cam, screenW, screenH, screen)) {
        return 0.0f;
    }
    const float w = std::max(0.0f, screen.maxX - screen.minX);
    const float h = std::max(0.0f, screen.maxY - screen.minY);
    return std::max(w, h);
}

inline int ComputeTexelsTouched(const float screenExtentPixels, const int mipLevel)
{
    const float extentAtMip = std::max(1.0f, screenExtentPixels / static_cast<float>(1 << mipLevel));
    const int texels = static_cast<int>(std::ceil(extentAtMip * extentAtMip));
    return std::max(1, texels);
}

inline int ComputePerChunkMip(const float screenExtentPixels, int conservativePixels)
{
    return SelectMipForScreenExtent(screenExtentPixels, conservativePixels);
}

// Synthetic HIZ depth pyramid: per mip level, for each screen-cell, max depth (in [0,1]) of
// chunks covering that cell. Built by bucketing chunks and per-cell computing max.
//
// For prototype: store as 2D array per mip level.
// texelDepth[mip][y][x] = max depth of chunks covering screen cell at (x,y) at mip level.
// Mip 0 = full resolution (1 pixel per cell). Mip K = cells of 2^K x 2^K pixels.
struct HizDepthPyramid {
    int width = 0;
    int height = 0;
    int maxMip = 0;
    std::vector<std::vector<std::vector<float>>> texelDepth;  // [mip][y][x]
    float bucketScreenSize = 16.0f;  // base cell = 16x16 pixels at mip 0
};

inline std::vector<std::pair<int, int>> ComputeOverlappingBuckets(
    const ScreenAabb &screen,
    int gridW,
    int gridH)
{
    std::vector<std::pair<int, int>> out;
    if (screen.minX >= screen.maxX || screen.minY >= screen.maxY) return out;
    const int minBX = std::max(0, static_cast<int>(screen.minX) / 16);
    const int maxBX = std::min(gridW - 1, static_cast<int>(screen.maxX) / 16);
    const int minBY = std::max(0, static_cast<int>(screen.minY) / 16);
    const int maxBY = std::min(gridH - 1, static_cast<int>(screen.maxY) / 16);
    for (int by = minBY; by <= maxBY; ++by) {
        for (int bx = minBX; bx <= maxBX; ++bx) {
            out.emplace_back(bx, by);
        }
    }
    return out;
}

// Build HIZ pyramid. For each chunk, project to screen, sample depths at each mip level.
inline HizDepthPyramid BuildHizDepthPyramid(
    const std::vector<ChunkAabb> &chunks,
    const Camera &cam,
    int screenW,
    int screenH,
    int maxMip)
{
    HizDepthPyramid pyr{};
    pyr.width = screenW;
    pyr.height = screenH;
    pyr.maxMip = maxMip;
    // Initialize to 1.0 = max depth (no occluder seen yet)
    pyr.texelDepth.resize(maxMip + 1);
    for (int m = 0; m <= maxMip; ++m) {
        const int mipW = std::max(1, screenW >> m);
        const int mipH = std::max(1, screenH >> m);
        pyr.texelDepth[m].assign(mipH, std::vector<float>(mipW, 1.0f));
    }

    // For each chunk, contribute minDepth01 (nearest face) at each mip
    for (const auto &chunk : chunks) {
        ScreenAabb screen{};
        if (!ProjectChunkToScreen(chunk, cam, screenW, screenH, screen)) continue;
        const float dx = chunk.centerX - cam.posX;
        const float dy = chunk.centerY - cam.posY;
        const float dz = chunk.centerZ - cam.posZ;
        const float depth = std::sqrt(dx * dx + dy * dy + dz * dz);
        // Nearest face depth (closest to camera)
        const float minDepth01 = std::min(1.0f, std::max(0.0f, (depth - chunk.halfExtent) / 256.0f));

        for (int m = 0; m <= maxMip; ++m) {
            const int mipW = std::max(1, screenW >> m);
            const int mipH = std::max(1, screenH >> m);
            const int minBX = std::max(0, static_cast<int>(screen.minX / static_cast<float>(1 << m)));
            const int maxBX = std::min(mipW - 1, static_cast<int>(screen.maxX / static_cast<float>(1 << m)));
            const int minBY = std::max(0, static_cast<int>(screen.minY / static_cast<float>(1 << m)));
            const int maxBY = std::min(mipH - 1, static_cast<int>(screen.maxY / static_cast<float>(1 << m)));
            for (int by = minBY; by <= maxBY; ++by) {
                for (int bx = minBX; bx <= maxBX; ++bx) {
                    // HIZ stores MIN (nearest) depth per Greene 1993
                    if (minDepth01 < pyr.texelDepth[m][by][bx]) {
                        pyr.texelDepth[m][by][bx] = minDepth01;
                    }
                }
            }
        }
    }

    return pyr;
}

// Sample HIZ pyramid at mip level for a screen-space AABB.
// For MIN pyramid, returns MAX depth value across the chunk's screen AABB
// (= furthest nearest-occluder = worst-case for occlusion cull check).
inline float SampleHizMaxDepth(
    const HizDepthPyramid &pyr,
    const ScreenAabb &screen,
    int mipLevel)
{
    if (mipLevel < 0 || mipLevel > pyr.maxMip) return 1.0f;
    const int mipW = std::max(1, pyr.width >> mipLevel);
    const int mipH = std::max(1, pyr.height >> mipLevel);
    const int minBX = std::max(0, static_cast<int>(screen.minX / static_cast<float>(1 << mipLevel)));
    const int maxBX = std::min(mipW - 1, static_cast<int>(screen.maxX / static_cast<float>(1 << mipLevel)));
    const int minBY = std::max(0, static_cast<int>(screen.minY / static_cast<float>(1 << mipLevel)));
    const int maxBY = std::min(mipH - 1, static_cast<int>(screen.maxY / static_cast<float>(1 << mipLevel)));
    float maxD = 0.0f;
    for (int by = minBY; by <= maxBY; ++by) {
        for (int bx = minBX; bx <= maxBX; ++bx) {
            if (pyr.texelDepth[mipLevel][by][bx] > maxD) {
                maxD = pyr.texelDepth[mipLevel][by][bx];
            }
        }
    }
    return maxD;
}

// Simulate cull for one chunk under a strategy, using HIZ depth pyramid.
// Conservative cull: chunk is culled if for all texels in chunk AABB at mip K,
// HIZ depth > chunk.minDepth (further occluders visible = chunk behind).
// Wait, that's wrong direction. Let me think.
//
// HIZ pyramid stores MAX depth in texel = furthest visible depth at that texel (Greene 1993 conservative).
// For occlusion cull:
//   - chunk is BEHIND occluder iff chunk.minDepth > HIZ_depth (occluder is FURTHER than chunk = no occlusion? No)
//   - Actually: occluder is FURTHER means we can see up to occluder distance. If chunk.minDepth < HIZ_depth
//     means chunk is CLOSER than furthest visible depth, so chunk might be visible.
//   - chunk is OCCLUDED iff chunk.maxDepth <= HIZ_depth (chunk entirely behind furthest visible depth)
//   - I.e., chunk is CULLED iff chunk.maxDepth <= HIZ_max_depth_in_chunk_AABB
//
// Wait, this is still ambiguous. Standard HIZ cull:
// - HIZ stores NEAREST occluder depth in each texel (or MAX, depends on convention)
// - Conservative cull: chunk is occluded iff HIZ_depth(chunk screen AABB) > chunk.minDepth
//   (i.e., the closest visible depth in that AABB is further than the chunk's closest face)
// - If HIZ stores NEAREST, then HIZ < chunk.minDepth means occluder is closer than chunk = chunk occluded
// - If HIZ stores MAX (furthest visible), then chunk occluded iff HIZ > chunk.minDepth? No...
//
// Let me think again. HIZ pyramid for occlusion culling (Greene 1993 / Siggraph 2008):
// - Stores NEAREST depth (closest occluder) per texel at each mip level
// - For a chunk AABB on screen, query HIZ at appropriate mip level
// - If HIZ_nearest_depth < chunk.minDepth (i.e., something is closer than chunk's near face) → chunk occluded → CULL
// - Else → chunk potentially visible → don't cull
//
// Our prototype uses MAX depth (which is WRONG convention for occlusion cull).
// Switch to MIN depth.
inline CullResult SimulateCull(
    const ChunkAabb &chunk,
    const Camera &cam,
    CullStrategy strategy,
    const CullConfig &config,
    int globalMipPerFrame,
    const HizDepthPyramid &hiz)
{
    CullResult result{};
    ScreenAabb screen{};
    if (!ProjectChunkToScreen(chunk, cam, config.screenWidth, config.screenHeight, screen)) {
        result.culled = false;
        result.mipLevelUsed = 0;
        result.texelsTouched = 1;
        result.computeCostUs = 0.001;
        return result;
    }

    const float screenExtent = std::max(screen.maxX - screen.minX, screen.maxY - screen.minY);
    if (screenExtent <= 0.0f) {
        result.culled = false;
        result.mipLevelUsed = 0;
        result.texelsTouched = 1;
        result.computeCostUs = 0.001;
        return result;
    }

    int mip = 0;
    switch (strategy) {
        case CullStrategy::A_UniformMip0: mip = 0; break;
        case CullStrategy::B_UniformMipGlobal: mip = globalMipPerFrame; break;
        case CullStrategy::C_PerChunkStaticMip:
            mip = ComputePerChunkMip(screenExtent, config.conservativePixels);
            break;
        case CullStrategy::D_PerChunkDynamicDispatch:
            mip = ComputePerChunkMip(screenExtent, config.conservativePixels);
            break;
    }
    mip = std::min(mip, hiz.maxMip);

    result.mipLevelUsed = mip;
    result.texelsTouched = ComputeTexelsTouched(screenExtent, mip);
    result.computeCostUs = static_cast<double>(result.texelsTouched) * 1.0 / 1000.0;

    // Cull logic with MIN pyramid:
    // For each texel in chunk screen AABB, HIZ stores nearest occluder depth.
    // Chunk occluded iff for ALL texels in chunk AABB, HIZ_min < chunk.minDepth01
    //   (every pixel where chunk would be drawn has a closer occluder)
    // Equivalently: chunk occluded iff MAX(HIZ_min in chunk AABB) < chunk.minDepth01
    //
    // For cull: chunk is culled iff MAX(HIZ_min in chunk AABB) < chunk.minDepth01
    const float hizMaxOfMins = SampleHizMaxDepth(hiz, screen, mip);
    const float dx = chunk.centerX - cam.posX;
    const float dy = chunk.centerY - cam.posY;
    const float dz = chunk.centerZ - cam.posZ;
    const float chunkDepth = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float chunkMinDepth01 = std::min(1.0f, std::max(0.0f, (chunkDepth - chunk.halfExtent) / 256.0f));

    // Cull iff all nearest-occluder depths in chunk AABB are < chunk.minDepth01
    result.culled = (hizMaxOfMins < chunkMinDepth01);

    return result;
}

struct AggregateMetrics {
    int chunkCount = 0;
    int culledCount = 0;
    int falseNegativeCount = 0;
    long long totalTexelsTouched = 0;
    double meanComputeUs = 0.0;
    double p99ComputeUs = 0.0;
    double psnrDb = std::numeric_limits<double>::infinity();
};

inline AggregateMetrics SimulateBatch(
    const std::vector<ChunkAabb> &chunks,
    const Camera &cam,
    CullStrategy strategy,
    const CullConfig &config,
    int globalMipPerFrame,
    const std::vector<bool> &groundTruthVisible,
    const HizDepthPyramid &hiz)
{
    AggregateMetrics agg{};
    agg.chunkCount = static_cast<int>(chunks.size());
    std::vector<double> computeCosts;
    computeCosts.reserve(chunks.size());

    for (size_t i = 0; i < chunks.size(); ++i) {
        const CullResult cr = SimulateCull(chunks[i], cam, strategy, config, globalMipPerFrame, hiz);
        if (cr.culled) ++agg.culledCount;
        if (cr.culled && i < groundTruthVisible.size() && groundTruthVisible[i]) {
            ++agg.falseNegativeCount;
        }
        agg.totalTexelsTouched += cr.texelsTouched;
        computeCosts.push_back(cr.computeCostUs);
    }

    if (!computeCosts.empty()) {
        double sum = 0.0;
        for (double c : computeCosts) sum += c;
        agg.meanComputeUs = sum / static_cast<double>(computeCosts.size());
        std::sort(computeCosts.begin(), computeCosts.end());
        agg.p99ComputeUs = computeCosts[static_cast<size_t>(computeCosts.size() * 0.99)];
    }

    if (agg.chunkCount > 0 && agg.falseNegativeCount > 0) {
        const double mse = static_cast<double>(agg.falseNegativeCount) / static_cast<double>(agg.chunkCount);
        if (mse > 0.0) {
            agg.psnrDb = 10.0 * std::log10(1.0 / mse);
        }
    }

    return agg;
}

inline int PickGlobalMip(
    const std::vector<ChunkAabb> &chunks,
    const Camera &cam,
    int screenW,
    int screenH,
    int conservativePixels)
{
    if (chunks.empty()) return 0;
    std::vector<float> extents;
    extents.reserve(chunks.size());
    for (const auto &c : chunks) {
        const float e = ComputeScreenExtentPixels(c, cam, screenW, screenH);
        if (e > 0.0f) extents.push_back(e);
    }
    if (extents.empty()) return 0;
    std::sort(extents.begin(), extents.end());
    const float median = extents[extents.size() / 2];
    return ComputePerChunkMip(median, conservativePixels);
}

}  // namespace hzb_smart_mip
