// scenes.hpp — synthetic voxel material atlas scenes
// Generated procedurally to represent ProjectV material types.
// Per `2026-06-21-sub-chunk-layers` precedent for direct comparability.
//
// Each scene produces 64 chunks × chunkSize=8³ voxels × 3 atlas types
// (diffuse RGBA8, normal XYZ8, ORM AO/Roughness/Metallic packed RGBA8).
// Total 64 × 512 × 4 bytes × 3 atlas = 384 KiB uncompressed per scene.

#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace texcomp {

using u8 = std::uint8_t;
using u32 = std::uint32_t;

constexpr int kChunkSize = 8;
constexpr int kChunksPerScene = 64;
constexpr int kAtlasTypes = 3;  // diffuse, normal, ORM

enum class AtlasType : int {
    Diffuse = 0,  // RGBA8 (color + alpha)
    Normal = 1,   // XYZ8 (normal packed: -1..1 → 0..255)
    ORM = 2,      // AO (R) + Roughness (G) + Metallic (B) + Padding (A)
};

struct RGBA8 {
    u8 r, g, b, a;
};

struct Voxel {
    u8 material;  // 0 = air, 1..N = material index
};

struct ChunkData {
    std::array<std::array<std::array<Voxel, kChunkSize>, kChunkSize>, kChunkSize> voxels{};
    std::vector<u8> material_ids;  // per-voxel material id
};

struct AtlasImage {
    int width = 0;
    int height = 0;
    std::vector<RGBA8> pixels;

    int size_bytes() const noexcept { return width * height * 4; }
};

inline const char* AtlasTypeName(AtlasType t) {
    switch (t) {
        case AtlasType::Diffuse: return "diffuse";
        case AtlasType::Normal:  return "normal";
        case AtlasType::ORM:     return "orm";
    }
    return "unknown";
}

struct Scene {
    std::string name;
    std::vector<ChunkData> chunks;  // 64 chunks
    int material_count = 0;

    // Per-atlas (diffuse/normal/ORM) reconstructed as 2D image (W=kChunkSize*sqrt(64)=64, H=kChunkSize*sqrt(64)=64).
    std::array<AtlasImage, kAtlasTypes> atlases{};
};

inline std::vector<Scene> GenerateScenes(u32 seed) {
    std::vector<Scene> scenes;
    std::mt19937 rng(seed);

    // ---- Scene 1: uniform_diffuse ----
    // 1 material × 64 chunks × uniform diffuse atlas (smooth color gradient test).
    {
        Scene s;
        s.name = "uniform_diffuse";
        s.material_count = 1;
        // Half air, half solid (material id 1).
        for (int c = 0; c < kChunksPerScene; ++c) {
            ChunkData cd;
            for (int z = 0; z < kChunkSize; ++z) {
                for (int y = 0; y < kChunkSize; ++y) {
                    for (int x = 0; x < kChunkSize; ++x) {
                        cd.voxels[z][y][x].material = (y < kChunkSize / 2) ? 1 : 0;
                    }
                }
            }
            s.chunks.push_back(std::move(cd));
        }
        scenes.push_back(std::move(s));
    }

    // ---- Scene 2: biome_pbr ----
    // 4 materials × 64 chunks × diffuse + normal + ORM (production atlas test).
    {
        Scene s;
        s.name = "biome_pbr";
        s.material_count = 4;
        for (int c = 0; c < kChunksPerScene; ++c) {
            ChunkData cd;
            std::uniform_int_distribution<int> mat_dist(0, 3);
            for (int z = 0; z < kChunkSize; ++z) {
                for (int y = 0; y < kChunkSize; ++y) {
                    for (int x = 0; x < kChunkSize; ++x) {
                        cd.voxels[z][y][x].material = (mat_dist(rng) == 0) ? 0 : (mat_dist(rng) + 1);
                    }
                }
            }
            s.chunks.push_back(std::move(cd));
        }
        scenes.push_back(std::move(s));
    }

    // ---- Scene 3: cave_roughness ----
    // 8 materials × 64 chunks × high-frequency roughness atlas (worst case).
    {
        Scene s;
        s.name = "cave_roughness";
        s.material_count = 8;
        for (int c = 0; c < kChunksPerScene; ++c) {
            ChunkData cd;
            std::uniform_int_distribution<int> mat_dist(0, 7);
            for (int z = 0; z < kChunkSize; ++z) {
                for (int y = 0; y < kChunkSize; ++y) {
                    for (int x = 0; x < kChunkSize; ++x) {
                        cd.voxels[z][y][x].material = mat_dist(rng);
                    }
                }
            }
            s.chunks.push_back(std::move(cd));
        }
        scenes.push_back(std::move(s));
    }

    // ---- Scene 4: metal_emissive ----
    // 16 materials × 64 chunks × emissive atlas (HDR BC6H path).
    {
        Scene s;
        s.name = "metal_emissive";
        s.material_count = 16;
        for (int c = 0; c < kChunksPerScene; ++c) {
            ChunkData cd;
            std::uniform_int_distribution<int> mat_dist(0, 15);
            for (int z = 0; z < kChunkSize; ++z) {
                for (int y = 0; y < kChunkSize; ++y) {
                    for (int x = 0; x < kChunkSize; ++x) {
                        cd.voxels[z][y][x].material = mat_dist(rng);
                    }
                }
            }
            s.chunks.push_back(std::move(cd));
        }
        scenes.push_back(std::move(s));
    }

    // ---- Scene 5: mixed_stress ----
    // 32 materials × 64 chunks × heterogeneous diffuse+normal+ORM
    // (representative of Stage 4.3 128m draw distance workload).
    {
        Scene s;
        s.name = "mixed_stress";
        s.material_count = 32;
        for (int c = 0; c < kChunksPerScene; ++c) {
            ChunkData cd;
            std::uniform_int_distribution<int> mat_dist(0, 31);
            for (int z = 0; z < kChunkSize; ++z) {
                for (int y = 0; y < kChunkSize; ++y) {
                    for (int x = 0; x < kChunkSize; ++x) {
                        cd.voxels[z][y][x].material = mat_dist(rng);
                    }
                }
            }
            s.chunks.push_back(std::move(cd));
        }
        scenes.push_back(std::move(s));
    }

    // ---- Build atlases for each scene ----
    constexpr int kAtlasSide = 64;  // 64×64 = 64 chunks of 8×8 face atlas per side
    for (auto& s : scenes) {
        for (int t = 0; t < kAtlasTypes; ++t) {
            s.atlases[t].width = kAtlasSide;
            s.atlases[t].height = kAtlasSide;
            s.atlases[t].pixels.assign(kAtlasSide * kAtlasSide, RGBA8{0, 0, 0, 255});

            // Generate per-voxel pattern based on material id + atlas type.
            for (int cy = 0; cy < kAtlasSide / kChunkSize; ++cy) {
                for (int cx = 0; cx < kAtlasSide / kChunkSize; ++cx) {
                    int chunk_idx = cy * (kAtlasSide / kChunkSize) + cx;
                    if (chunk_idx >= (int)s.chunks.size()) break;
                    const auto& chunk = s.chunks[chunk_idx];
                    for (int ly = 0; ly < kChunkSize; ++ly) {
                        for (int lx = 0; lx < kChunkSize; ++lx) {
                            // Sample middle voxel layer for atlas tile.
                            int mat_id = chunk.voxels[kChunkSize / 2][ly][lx].material;
                            int px = cx * kChunkSize + lx;
                            int py = cy * kChunkSize + ly;
                            RGBA8 c{};
                            switch (static_cast<AtlasType>(t)) {
                                case AtlasType::Diffuse: {
                                    // Per-material diffuse color (palette of 32 distinct colors).
                                    auto palette = [](int m) -> RGBA8 {
                                        constexpr u8 table[32][3] = {
                                            {128, 128, 128}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255},
                                            {255, 255, 0}, {255, 0, 255}, {0, 255, 255}, {255, 128, 0},
                                            {128, 255, 0}, {0, 255, 128}, {128, 0, 255}, {255, 0, 128},
                                            {64, 64, 64}, {192, 192, 192}, {128, 64, 0}, {64, 128, 0},
                                            {0, 128, 64}, {0, 64, 128}, {64, 0, 128}, {128, 0, 64},
                                            {200, 100, 50}, {100, 200, 50}, {50, 200, 100}, {50, 100, 200},
                                            {100, 50, 200}, {200, 50, 100}, {128, 200, 200}, {200, 128, 200},
                                            {200, 200, 128}, {50, 50, 50}, {80, 80, 80}, {180, 180, 180},
                                        };
                                        int idx = (m == 0) ? 0 : (m & 0x1F);
                                        return RGBA8{table[idx][0], table[idx][1], table[idx][2], 255};
                                    };
                                    c = palette(mat_id);
                                    break;
                                }
                                case AtlasType::Normal: {
                                    // Normal XYZ packed (-1..1 → 0..255). Per-material synthetic normal.
                                    if (mat_id == 0) {
                                        c = RGBA8{128, 128, 255, 255};  // up
                                    } else {
                                        u8 nx = (u8)((mat_id * 37) & 0xFF);
                                        u8 ny = (u8)((mat_id * 73) & 0xFF);
                                        u8 nz = 255;
                                        c = RGBA8{nx, ny, nz, 255};
                                    }
                                    break;
                                }
                                case AtlasType::ORM: {
                                    // AO (R) + Roughness (G) + Metallic (B) + padding.
                                    u8 ao = 255;
                                    u8 rough = (u8)((mat_id * 23) & 0xFF);
                                    u8 metal = (mat_id > 16) ? 255 : (mat_id > 4 ? 128 : 0);
                                    c = RGBA8{ao, rough, metal, 255};
                                    break;
                                }
                            }
                            s.atlases[t].pixels[py * kAtlasSide + px] = c;
                        }
                    }
                }
            }
        }
    }

    return scenes;
}

}  // namespace texcomp
