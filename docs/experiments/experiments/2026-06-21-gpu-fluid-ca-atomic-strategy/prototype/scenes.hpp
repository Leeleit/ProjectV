#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>

namespace scenes {

// 4-byte per cell
struct Cell {
    uint32_t material;
    uint32_t age;
    uint32_t reserved0;
    uint32_t reserved1;
};

// Scene type enum
enum class SceneType : uint32_t {
    Empty           = 0,  // 64x64x64, 0 fluid cells (control)
    Sparse          = 1,  // 64x64x64, ~1% density random (~4096 cells)
    VerticalColumn  = 2,  // 64x1x1 column, 64 cells (worst case fall contention)
    WaterTower      = 3,  // 8x32x8 dense block, 2048 cells (vertical pressure)
    LavaPool        = 4,  // 32x4x32 horizontal slab, 4096 cells (horizontal pressure)
};

inline const char* SceneName(SceneType s) {
    switch (s) {
        case SceneType::Empty: return "empty";
        case SceneType::Sparse: return "sparse";
        case SceneType::VerticalColumn: return "vertical_column";
        case SceneType::WaterTower: return "water_tower";
        case SceneType::LavaPool: return "lava_pool";
    }
    return "unknown";
}

// Dimensions per scene
struct SceneDims {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};

inline SceneDims SceneDimensions(SceneType s) {
    switch (s) {
        case SceneType::Empty:          return {16, 16, 16};
        case SceneType::Sparse:         return {16, 16, 16};
        case SceneType::VerticalColumn: return {64, 64, 1};
        case SceneType::WaterTower:     return {8, 32, 8};
        case SceneType::LavaPool:       return {32, 4, 32};
    }
    return {0, 0, 0};
}

// Generate initial cell pattern
inline std::vector<Cell> GenerateScene(SceneType type, uint32_t seed) {
    const SceneDims dims = SceneDimensions(type);
    const uint32_t total = dims.width * dims.height * dims.depth;
    std::vector<Cell> cells(total);
    // Initialize to Air (material=0)
    for (auto& c : cells) {
        c.material = 0u;
        c.age = 0u;
        c.reserved0 = 0u;
        c.reserved1 = 0u;
    }
    constexpr uint32_t kFluid = 4u;
    std::mt19937 rng(seed);
    switch (type) {
        case SceneType::Empty:
            // 0 fluid cells (all Air)
            break;
        case SceneType::Sparse: {
            // ~1% density: 4096 fluid cells in 64^3=262144 total
            std::uniform_int_distribution<uint32_t> dist(0, total - 1);
            for (uint32_t i = 0; i < 4096; ++i) {
                cells[dist(rng)].material = kFluid;
            }
            break;
        }
        case SceneType::VerticalColumn: {
            // 64x1x1: column of 64 fluid cells at x=0, z=0, y=[0,63]
            for (uint32_t y = 0; y < 64; ++y) {
                const uint32_t idx = y; // x=0, z=0
                cells[idx].material = kFluid;
            }
            break;
        }
        case SceneType::WaterTower: {
            // 8x32x8: dense block of 2048 fluid cells, y=[0,31]
            for (uint32_t y = 0; y < 32; ++y) {
                for (uint32_t z = 0; z < 8; ++z) {
                    for (uint32_t x = 0; x < 8; ++x) {
                        const uint32_t idx = (y * 8 + z) * 8 + x;
                        cells[idx].material = kFluid;
                    }
                }
            }
            break;
        }
        case SceneType::LavaPool: {
            // 32x4x32: horizontal slab, y=[0,3]
            for (uint32_t y = 0; y < 4; ++y) {
                for (uint32_t z = 0; z < 32; ++z) {
                    for (uint32_t x = 0; x < 32; ++x) {
                        const uint32_t idx = (y * 32 + z) * 32 + x;
                        cells[idx].material = kFluid;
                    }
                }
            }
            break;
        }
    }
    return cells;
}

// Count fluid cells (for validation)
inline uint32_t CountFluid(const std::vector<Cell>& cells) {
    uint32_t n = 0;
    for (const auto& c : cells) {
        if (c.material == 4u) ++n;
    }
    return n;
}

} // namespace scenes
