#include "scenes.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>

namespace sdf_hybrid::scenes {

namespace {

// Set a voxel at (x, y, z) to value v. Bounds-checked.
inline void set_voxel(Chunk& c, int x, int y, int z, Voxel v) noexcept {
    if (x < 0 || y < 0 || z < 0) return;
    if (x >= static_cast<int>(CHUNK_SIZE) ||
        y >= static_cast<int>(CHUNK_SIZE) ||
        z >= static_cast<int>(CHUNK_SIZE)) return;
    c[idx3(x, y, z)] = v;
}

// Get a voxel at (x, y, z). Returns 0 (air) if out of bounds (for SDF purposes).
inline Voxel get_voxel(const Chunk& c, int x, int y, int z) noexcept {
    if (x < 0 || y < 0 || z < 0) return 0;
    if (x >= static_cast<int>(CHUNK_SIZE) ||
        y >= static_cast<int>(CHUNK_SIZE) ||
        z >= static_cast<int>(CHUNK_SIZE)) return 0;
    return c[idx3(x, y, z)];
}

// Per sub-chunk-layers: 1 material, 95% empty.
Chunk make_uniform_air(std::mt19937& rng) noexcept {
    Chunk c{};
    // 5% solid voxels, randomly placed.
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (std::size_t i = 0; i < CHUNK_VOLUME; ++i) {
        if (dist(rng) < 0.05f) {
            c[i] = 1;  // material 1
        }
    }
    return c;
}

// 1 material, solid ground (bottom half).
Chunk make_uniform_floor(std::mt19937&) noexcept {
    Chunk c{};
    for (std::size_t z = 0; z < CHUNK_SIZE / 2; ++z) {
        for (std::size_t y = 0; y < CHUNK_SIZE; ++y) {
            for (std::size_t x = 0; x < CHUNK_SIZE; ++x) {
                c[idx3(x, y, z)] = 1;
            }
        }
    }
    return c;
}

// 2 materials: ground (bottom 3 layers = mat 1) + 1-2 trees (random positions = mat 2).
Chunk make_forest_floor(std::mt19937& rng) noexcept {
    Chunk c{};
    // Ground.
    for (std::size_t z = 0; z < 3; ++z) {
        for (std::size_t y = 0; y < CHUNK_SIZE; ++y) {
            for (std::size_t x = 0; x < CHUNK_SIZE; ++x) {
                c[idx3(x, y, z)] = 1;
            }
        }
    }
    // 1-2 trees: trunk (3-5 high) + small canopy.
    std::uniform_int_distribution<int> tree_x(1, static_cast<int>(CHUNK_SIZE) - 2);
    std::uniform_int_distribution<int> tree_z(3, static_cast<int>(CHUNK_SIZE) - 3);
    int num_trees = 1 + (rng() % 2);
    for (int t = 0; t < num_trees; ++t) {
        int tx = tree_x(rng);
        int tz = tree_z(rng);
        int trunk_h = 3 + (rng() % 3);
        for (int h = 0; h < trunk_h; ++h) {
            set_voxel(c, tx, h + 3, tz, 2);
        }
        // Canopy: 3x3x1 above trunk.
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                set_voxel(c, tx + dx, trunk_h + 3, tz + dy, 2);
            }
        }
    }
    return c;
}

// 3 materials: cave with biome walls (worst case VCT per sub-chunk-layers).
Chunk make_cave_stress(std::mt19937& rng) noexcept {
    Chunk c{};
    // Outer shell (mat 1 = stone).
    for (int z = 0; z < static_cast<int>(CHUNK_SIZE); ++z) {
        for (int y = 0; y < static_cast<int>(CHUNK_SIZE); ++y) {
            for (int x = 0; x < static_cast<int>(CHUNK_SIZE); ++x) {
                if (x == 0 || y == 0 || z == 0 ||
                    x == static_cast<int>(CHUNK_SIZE) - 1 ||
                    y == static_cast<int>(CHUNK_SIZE) - 1 ||
                    z == static_cast<int>(CHUNK_SIZE) - 1) {
                    c[idx3(x, y, z)] = 1;
                }
            }
        }
    }
    // Inner cave void (0 = air) — 2x2x2 at random position.
    std::uniform_int_distribution<int> cave_pos(2, 5);
    int cx = cave_pos(rng);
    int cy = cave_pos(rng);
    int cz = cave_pos(rng);
    for (int dz = 0; dz < 2; ++dz) {
        for (int dy = 0; dy < 2; ++dy) {
            for (int dx = 0; dx < 2; ++dx) {
                set_voxel(c, cx + dx, cy + dy, cz + dz, 0);
            }
        }
    }
    // Some biome wall material (mat 2) on inner cave surface.
    for (int z = 1; z < static_cast<int>(CHUNK_SIZE) - 1; ++z) {
        for (int y = 1; y < static_cast<int>(CHUNK_SIZE) - 1; ++y) {
            for (int x = 1; x < static_cast<int>(CHUNK_SIZE) - 1; ++x) {
                Voxel v = c[idx3(x, y, z)];
                if (v == 1 && get_voxel(c, x, y, z - 1) == 0) {
                    c[idx3(x, y, z)] = 2;  // biome wall facing cave
                }
            }
        }
    }
    // A few isolated mat 3 features (crystals) for visual variety.
    std::uniform_int_distribution<int> feat_pos(1, 6);
    int num_features = 2 + (rng() % 3);
    for (int f = 0; f < num_features; ++f) {
        int fx = feat_pos(rng);
        int fy = feat_pos(rng);
        int fz = feat_pos(rng);
        set_voxel(c, fx, fy, fz, 3);
    }
    return c;
}

// 4 materials: heterogeneous biome (bottom = mat 1 grass, mid = mixed 2/3, top = mat 4 stone).
Chunk make_mixed_biome(std::mt19937& rng) noexcept {
    Chunk c{};
    // Bottom 2 layers: grass (mat 1).
    for (std::size_t z = 0; z < 2; ++z) {
        for (std::size_t y = 0; y < CHUNK_SIZE; ++y) {
            for (std::size_t x = 0; x < CHUNK_SIZE; ++x) {
                c[idx3(x, y, z)] = 1;
            }
        }
    }
    // Middle 3 layers: mix of dirt (2) and sand (3), random.
    std::uniform_int_distribution<int> mat_23(0, 1);
    for (std::size_t z = 2; z < 5; ++z) {
        for (std::size_t y = 0; y < CHUNK_SIZE; ++y) {
            for (std::size_t x = 0; x < CHUNK_SIZE; ++x) {
                c[idx3(x, y, z)] = mat_23(rng) ? 2 : 3;
            }
        }
    }
    // Top 3 layers: stone (4), with some air pockets.
    std::uniform_real_distribution<float> air_pocket(0.0f, 1.0f);
    for (std::size_t z = 5; z < CHUNK_SIZE; ++z) {
        for (std::size_t y = 0; y < CHUNK_SIZE; ++y) {
            for (std::size_t x = 0; x < CHUNK_SIZE; ++x) {
                c[idx3(x, y, z)] = air_pocket(rng) < 0.85f ? 4 : 0;
            }
        }
    }
    return c;
}

}  // namespace

Chunk generate(SceneKind kind, std::uint32_t seed) noexcept {
    std::mt19937 rng(seed);
    switch (kind) {
        case SceneKind::UniformAir:    return make_uniform_air(rng);
        case SceneKind::UniformFloor:  return make_uniform_floor(rng);
        case SceneKind::ForestFloor:   return make_forest_floor(rng);
        case SceneKind::CaveStress:    return make_cave_stress(rng);
        case SceneKind::MixedBiome:    return make_mixed_biome(rng);
    }
    return Chunk{};
}

}  // namespace sdf_hybrid::scenes
