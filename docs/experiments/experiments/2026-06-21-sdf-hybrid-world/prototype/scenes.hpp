#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace sdf_hybrid::scenes {

// ProjectV chunkSize per src/voxel/VoxelWorld.hpp:78.
inline constexpr std::size_t CHUNK_SIZE = 8;
inline constexpr std::size_t CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
inline constexpr std::size_t CHUNK_BYTES = CHUNK_VOLUME;

using Voxel = std::uint8_t;
using Chunk = std::array<Voxel, CHUNK_VOLUME>;

// 5 scene types per sub-chunk-layers precedent for direct comparability.
// Per sub-chunk-layers/README.md §9 + STATUS.md: "uniform_air" / "uniform_floor" / "forest_floor" /
// "cave_stress" / "mixed_biome". Same seed set: 1, 7, 42, 1234, 31337.
//
// Note: simplified for SDF overlay focus — each scene is a single chunkSize=8 cube.
// In real ProjectV, these would be at the chunk level, but for prototype we use self-contained cubes.

enum class SceneKind : std::uint8_t {
    UniformAir,    // 1 material, 95% empty (sparse)
    UniformFloor,  // 1 material, solid ground (dense single material)
    ForestFloor,   // 2 materials, ground + tree
    CaveStress,    // 3 materials, cave with biome walls (worst case VCT)
    MixedBiome,    // 4 materials, heterogeneous
};

constexpr const char* scene_name(SceneKind k) noexcept {
    switch (k) {
        case SceneKind::UniformAir:    return "uniform_air";
        case SceneKind::UniformFloor:  return "uniform_floor";
        case SceneKind::ForestFloor:   return "forest_floor";
        case SceneKind::CaveStress:    return "cave_stress";
        case SceneKind::MixedBiome:    return "mixed_biome";
    }
    return "unknown";
}

constexpr std::size_t scene_material_count(SceneKind k) noexcept {
    switch (k) {
        case SceneKind::UniformAir:    return 1;
        case SceneKind::UniformFloor:  return 1;
        case SceneKind::ForestFloor:   return 2;
        case SceneKind::CaveStress:    return 3;
        case SceneKind::MixedBiome:    return 4;
    }
    return 1;
}

// Index helpers.
inline constexpr std::size_t idx3(std::size_t x, std::size_t y, std::size_t z) noexcept {
    return (z * CHUNK_SIZE + y) * CHUNK_SIZE + x;
}

inline constexpr std::size_t idx3(int x, int y, int z) noexcept {
    return idx3(static_cast<std::size_t>(x), static_cast<std::size_t>(y), static_cast<std::size_t>(z));
}

// 3D coordinate: -1 = outside, 0 = surface voxel, +1 = inside (used by SDF generation).
struct Coord3 {
    int x, y, z;
};

// Generate a scene with a given seed.
Chunk generate(SceneKind kind, std::uint32_t seed) noexcept;

}  // namespace sdf_hybrid::scenes
