#pragma once

#include "scenes.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace hzb_smart_mip {

// Ground truth visibility: synthetic depth buffer = solid chunks at varying depths.
// A chunk is "occluded" if there exists a closer solid chunk whose screen-AABB
// overlaps the query chunk's screen-AABB at any pixel.
//
// For prototype: bucket solids into screen-grid cells (spatial hash), then query each
// chunk's relevant buckets. O(N + K) per frame where K = avg chunks per cell bucket.
//
// Output: vector<bool> same size as chunks; true = actually visible (NOT safe to cull),
// false = occluded (safe to cull).

struct SolidChunk {
    float centerX;
    float centerY;
    float centerZ;
    float halfExtent;
    float depth;
    float screenMinX;
    float screenMinY;
    float screenMaxX;
    float screenMaxY;
};

inline std::vector<SolidChunk> BuildSolidChunks(
    const std::vector<ChunkAabb> &chunks,
    const Camera &cam,
    int screenW,
    int screenH)
{
    std::vector<SolidChunk> solids;
    solids.reserve(chunks.size());
    for (const auto &c : chunks) {
        SolidChunk s{};
        s.centerX = c.centerX;
        s.centerY = c.centerY;
        s.centerZ = c.centerZ;
        s.halfExtent = c.halfExtent;
        const float dx = c.centerX - cam.posX;
        const float dy = c.centerY - cam.posY;
        const float dz = c.centerZ - cam.posZ;
        s.depth = std::sqrt(dx * dx + dy * dy + dz * dz);

        ScreenAabb screen{};
        if (ProjectChunkToScreen(c, cam, screenW, screenH, screen)) {
            s.screenMinX = screen.minX;
            s.screenMinY = screen.minY;
            s.screenMaxX = screen.maxX;
            s.screenMaxY = screen.maxY;
        } else {
            s.screenMinX = s.screenMinY = s.screenMaxX = s.screenMaxY = -1.0f;
        }
        solids.push_back(s);
    }
    std::sort(solids.begin(), solids.end(), [](const SolidChunk &a, const SolidChunk &b) {
        return a.depth < b.depth;
    });
    return solids;
}

inline bool ScreenAabbOverlap(
    float minX1, float minY1, float maxX1, float maxY1,
    float minX2, float minY2, float maxX2, float maxY2)
{
    return !(maxX1 < minX2 || maxX2 < minX1 || maxY1 < minY2 || maxY2 < minY1);
}

// Compute ground-truth visibility using bucket sort by screen-grid cell.
// Bucket cell size = 64x64 pixels. O(N + N*K) where K = avg chunks per cell.
inline std::vector<bool> ComputeGroundTruthVisibility(
    const std::vector<ChunkAabb> &chunks,
    const Camera &cam,
    int screenW,
    int screenH)
{
    constexpr int kBucketSize = 64;
    const int gridW = (screenW + kBucketSize - 1) / kBucketSize;
    const int gridH = (screenH + kBucketSize - 1) / kBucketSize;

    std::vector<SolidChunk> solids = BuildSolidChunks(chunks, cam, screenW, screenH);

    // Bucket solids by screen-grid cell
    std::unordered_multimap<uint32_t, size_t> buckets;
    for (size_t i = 0; i < solids.size(); ++i) {
        const SolidChunk &s = solids[i];
        if (s.screenMinX < 0.0f) continue;
        const int minBX = std::max(0, static_cast<int>(s.screenMinX) / kBucketSize);
        const int maxBX = std::min(gridW - 1, static_cast<int>(s.screenMaxX) / kBucketSize);
        const int minBY = std::max(0, static_cast<int>(s.screenMinY) / kBucketSize);
        const int maxBY = std::min(gridH - 1, static_cast<int>(s.screenMaxY) / kBucketSize);
        for (int by = minBY; by <= maxBY; ++by) {
            for (int bx = minBX; bx <= maxBX; ++bx) {
                const uint32_t key = static_cast<uint32_t>(by) * static_cast<uint32_t>(gridW) + static_cast<uint32_t>(bx);
                buckets.emplace(key, i);
            }
        }
    }

    std::vector<bool> visible(chunks.size(), true);

    for (size_t qi = 0; qi < chunks.size(); ++qi) {
        const ChunkAabb &query = chunks[qi];
        ScreenAabb qScreen{};
        if (!ProjectChunkToScreen(query, cam, screenW, screenH, qScreen)) {
            visible[qi] = true;
            continue;
        }

        const float dx = query.centerX - cam.posX;
        const float dy = query.centerY - cam.posY;
        const float dz = query.centerZ - cam.posZ;
        const float queryDepth = std::sqrt(dx * dx + dy * dy + dz * dz);

        // Look up buckets overlapping query screen AABB
        const int minBX = std::max(0, static_cast<int>(qScreen.minX) / kBucketSize);
        const int maxBX = std::min(gridW - 1, static_cast<int>(qScreen.maxX) / kBucketSize);
        const int minBY = std::max(0, static_cast<int>(qScreen.minY) / kBucketSize);
        const int maxBY = std::min(gridH - 1, static_cast<int>(qScreen.maxY) / kBucketSize);

        bool occluded = false;
        for (int by = minBY; by <= maxBY && !occluded; ++by) {
            for (int bx = minBX; bx <= maxBX && !occluded; ++bx) {
                const uint32_t key = static_cast<uint32_t>(by) * static_cast<uint32_t>(gridW) + static_cast<uint32_t>(bx);
                auto range = buckets.equal_range(key);
                for (auto it = range.first; it != range.second; ++it) {
                    const size_t si = it->second;
                    const SolidChunk &s = solids[si];
                    if (s.depth >= queryDepth) continue;
                    if (s.screenMinX < 0.0f) continue;
                    if (ScreenAabbOverlap(
                            qScreen.minX, qScreen.minY, qScreen.maxX, qScreen.maxY,
                            s.screenMinX, s.screenMinY, s.screenMaxX, s.screenMaxY)) {
                        occluded = true;
                        break;
                    }
                }
            }
        }
        visible[qi] = !occluded;
    }

    return visible;
}

}  // namespace hzb_smart_mip
