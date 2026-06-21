#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace hzb_smart_mip {

// Chunk = axis-aligned bounding box in world space.
// Matches ProjectV Sparse64Node chunk semantics: chunkSize=8 voxels * voxelSize.
// For our prototype, voxelSize=1m (matches ProjectV kVoxelSize baseline).
struct ChunkAabb {
    float centerX;
    float centerY;
    float centerZ;
    float halfExtent;
};

struct Camera {
    float posX;
    float posY;
    float posZ;
    float yawRad;  // around Y axis
    float pitchRad;
    float fovYRadians;  // vertical FOV, e.g. 1.5708 = 90 degrees
    float aspectRatio;  // width/height, e.g. 1.7777 = 16:9
};

// Screen-space projection: returns approximate screen-space AABB extent in pixels.
// Uses perspective projection formula. Conservative approximation (axis-aligned bounding
// rectangle around the projected AABB corners).
struct ScreenAabb {
    float minX;
    float minY;
    float maxX;
    float maxY;
    float minDepth;
    float maxDepth;
};

// Compute screen-space AABB. Conservative: projects all 8 corners, takes axis-aligned min/max.
// Returns false if AABB entirely outside frustum.
inline bool ProjectChunkToScreen(
    const ChunkAabb &chunk,
    const Camera &cam,
    const int screenWidth,
    const int screenHeight,
    ScreenAabb &out)
{
    // View matrix (camera at origin looking down -Z)
    const float cosY = std::cos(cam.yawRad);
    const float sinY = std::sin(cam.yawRad);
    const float cosP = std::cos(cam.pitchRad);
    const float sinP = std::sin(cam.pitchRad);

    // World-relative camera position
    const float dx = chunk.centerX - cam.posX;
    const float dy = chunk.centerY - cam.posY;
    const float dz = chunk.centerZ - cam.posZ;

    // View matrix: rotate world into camera space. Convention: camera looks down +Z in view space
    // (so in-front points have viewZ > 0).
    const float viewX = cosY * dx + sinY * dz;
    const float viewZ = -sinY * dx + cosY * dz;
    const float viewY = cosP * dy - sinP * viewZ;
    const float viewZ2 = sinP * dy + cosP * viewZ;

    // viewZ2 > 0 = in front of camera
    if (viewZ2 <= 0.1f) {
        return false;
    }

    const float tanHalfFov = std::tan(cam.fovYRadians * 0.5f);

    // Linear depth in view space (positive)
    const float depth = viewZ2;
    out.minDepth = depth - chunk.halfExtent;
    out.maxDepth = depth + chunk.halfExtent;
    if (out.maxDepth < 0.1f) return false;

    // Half-extent in screen space at chunk depth (perspective)
    const float focalLengthY = static_cast<float>(screenHeight) / (2.0f * tanHalfFov);

    // Conservative AABB screen extent
    const float screenHalfSize = chunk.halfExtent * focalLengthY / depth;
    const float screenRadius = 2.0f * screenHalfSize;

    // Project center to NDC (camera looks down +Z, so divide by viewZ2 positive)
    const float ndcX = viewX / viewZ2 / (tanHalfFov * cam.aspectRatio);
    const float ndcY = viewY / viewZ2 / tanHalfFov;
    const float screenCenterX = (ndcX * 0.5f + 0.5f) * static_cast<float>(screenWidth);
    const float screenCenterY = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(screenHeight);

    out.minX = screenCenterX - screenRadius;
    out.maxX = screenCenterX + screenRadius;
    out.minY = screenCenterY - screenRadius;
    out.maxY = screenCenterY + screenRadius;

    // Clamp to valid screen (chunks partially off-screen still valid for our prototype)
    // For chunks entirely outside, return false
    const float screenW = static_cast<float>(screenWidth);
    const float screenH = static_cast<float>(screenHeight);
    if (out.maxX < 0.0f || out.minX > screenW ||
        out.maxY < 0.0f || out.minY > screenH) {
        return false;  // entirely off-screen
    }
    out.minX = std::max(0.0f, out.minX);
    out.maxX = std::min(screenW, out.maxX);
    out.minY = std::max(0.0f, out.minY);
    out.maxY = std::min(screenH, out.maxY);
    return true;
}

// Compute mip level for a given screen-space AABB extent.
// Conservative: use max(width, height) and ensure at least kConservativePixels coverage.
// mip = floor(log2(screenExtent / kConservativePixels))
inline int SelectMipForScreenExtent(const float screenExtentPixels, const int kConservativePixels)
{
    if (screenExtentPixels <= 0.0f) return 0;
    // mip 0 = full res. Each mip doubles texel size (halves resolution).
    // If chunk projects to S pixels and we want kConservativePixels per chunk, we need
    // a mip where chunk covers ~S/kConservativePixels texels.
    // mip = ceil(log2(S / kConservativePixels))
    const float ratio = screenExtentPixels / static_cast<float>(kConservativePixels);
    if (ratio <= 1.0f) return 0;
    return static_cast<int>(std::ceil(std::log2(ratio)));
}

// Scene types
enum class SceneType {
    UniformFloor,
    ForestFloor,
    CaveStress,
    MixedBiome,
    ViewDollyStress
};

inline const char *SceneName(SceneType s)
{
    switch (s) {
        case SceneType::UniformFloor: return "uniform_floor";
        case SceneType::ForestFloor: return "forest_floor";
        case SceneType::CaveStress: return "cave_stress";
        case SceneType::MixedBiome: return "mixed_biome";
        case SceneType::ViewDollyStress: return "view_dolly_stress";
    }
    return "unknown";
}

// Generate synthetic chunk AABBs for a scene + seed.
// All scenes: chunks on a regular grid (voxelSize=1, chunkSize=8 → 8m chunks).
// Variations:
//   - uniform_floor: all chunks present, flat terrain
//   - forest_floor: all chunks present, heights vary ±2m
//   - cave_stress: half chunks missing (carved out), creating hidden interior
//   - mixed_biome: 70% present, mixed near/far
//   - view_dolly_stress: 80% present, biased toward camera path
inline std::vector<ChunkAabb> GenerateScene(
    SceneType scene,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::vector<ChunkAabb> chunks;

    // Grid: 16x4x16 = 1024 chunks (typical view)
    constexpr int gridX = 16;
    constexpr int gridY = 4;
    constexpr int gridZ = 16;
    constexpr float chunkSize = 8.0f;  // chunkSize=8 voxels * 1m/voxel = 8m chunks
    constexpr float halfExtent = chunkSize * 0.5f;

    for (int gx = 0; gx < gridX; ++gx) {
        for (int gy = 0; gy < gridY; ++gy) {
            for (int gz = 0; gz < gridZ; ++gz) {
                bool include = true;
                float heightVariation = 0.0f;

                switch (scene) {
                    case SceneType::UniformFloor:
                        include = true;
                        break;
                    case SceneType::ForestFloor:
                        include = true;
                        heightVariation = (rng() % 5) - 2;  // -2..+2m
                        break;
                    case SceneType::CaveStress: {
                        // Half chunks missing (carved out)
                        const uint32_t r = rng();
                        include = (r & 0x1) != 0;
                        break;
                    }
                    case SceneType::MixedBiome: {
                        const uint32_t r = rng();
                        include = (r % 10) >= 3;  // 70% present
                        if (include) {
                            heightVariation = (rng() % 7) - 3;  // -3..+3m
                        }
                        break;
                    }
                    case SceneType::ViewDollyStress: {
                        const uint32_t r = rng();
                        include = (r % 5) != 0;  // 80% present
                        break;
                    }
                }

                if (!include) continue;

                ChunkAabb c{};
                c.centerX = (gx - gridX / 2) * chunkSize;
                c.centerY = (gy - gridY / 2) * chunkSize + heightVariation;
                c.centerZ = (gz - gridZ / 2) * chunkSize;
                c.halfExtent = halfExtent;
                chunks.push_back(c);
            }
        }
    }
    return chunks;
}

// Generate camera trajectory for view_dolly_stress scene.
// Camera dollies forward through scene at constant rate.
inline Camera GenerateCamera(SceneType scene, uint32_t frameIndex)
{
    Camera cam{};
    cam.posX = 0.0f;
    cam.posY = 32.0f;  // 4 chunks up
    cam.posZ = -64.0f + static_cast<float>(frameIndex) * 0.128f;  // dolly forward 128m
    cam.yawRad = 0.0f;
    cam.pitchRad = 0.0f;
    cam.fovYRadians = 1.5708f;  // 90 degrees
    cam.aspectRatio = 16.0f / 9.0f;

    if (scene == SceneType::ViewDollyStress) {
        // Dolly continues
        cam.posZ = -100.0f + static_cast<float>(frameIndex % 200) * 1.0f;
    }
    return cam;
}

}  // namespace hzb_smart_mip
