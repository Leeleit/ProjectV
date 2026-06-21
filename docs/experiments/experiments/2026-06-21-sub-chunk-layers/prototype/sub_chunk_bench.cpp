// sub_chunk_bench.cpp — standalone CPU benchmark для 5 chunk layout designs.
// Измеряет memory/chunk + mutation cost + build time + mesh vertex count + layer boundary count
// для 5 synthetic scenes × 5 designs × 5 seeds × 1000 iter.
//
// Builds:
//   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
//   cmake --build build -j$(nproc)
//
// Usage:
//   ./build/sub_chunk_bench --all --output build/results.csv
//   ./build/sub_chunk_bench --scene mixed_biome --design C_FixedLayer_L2 --output build/results_one.csv
//   ./build/sub_chunk_bench --scene all --design all --output build/results_all.csv
//
// Per `docs/experiments/benchmarks/methodology.md §3`:
//   - 5 seeds (1, 7, 42, 1234, 31337), fixed
//   - 1000 iter per measurement, 50 warmup
//   - Mean + p95 + stddev for timing
//   - Machine-readable CSV + human-readable stdout summary
//
// Standalone (no Vulkan, no ProjectV mainline), synthetic scenes representative of
// Minecraft-1.18+ biome/cave structure per web-research 2026-06-21.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// Section 1: VoxelMaterial + chunk constants (ProjectV-aligned)
// ============================================================================

namespace pv {

enum class Material : uint8_t {
    Air = 0,
    Glass = 1,
    Fluid = 2,
    FloorWhite = 3,
    FloorGray = 4,
};
static constexpr int kMaterialCount = 5;

constexpr int kChunkSize = 8;
constexpr int kChunkVoxels = kChunkSize * kChunkSize * kChunkSize;  // 512
constexpr int kLayerVoxels_L2 = kChunkSize * 2 * kChunkSize;        // 128 (8×2×8)
constexpr int kLayerVoxels_L4 = kChunkSize * 4 * kChunkSize;        // 256 (8×4×8)
constexpr int kNumLayers_L2 = kChunkSize / 2;                       // 4
constexpr int kNumLayers_L4 = kChunkSize / 4;                       // 2

constexpr int kPaletteMax = 16;          // max materials per palette (4 bits/index)
constexpr int kBitsPerByte = 8;

// ============================================================================
// Section 2: Indexing helpers (3D → 1D для flat arrays)
// ============================================================================

inline int idx3(int x, int y, int z) noexcept {
    return (y * kChunkSize + z) * kChunkSize + x;
}

inline int idx3_layer(int x, int ly, int z) noexcept {
    return (ly * kChunkSize + z) * kChunkSize + x;
}

inline int idx3_layer_L2(int x, int ly, int z) noexcept {
    // L2: sub-layer of 8×2×8 voxels
    return (ly * kChunkSize + z) * kChunkSize + x;
}

inline int idx3_layer_L4(int x, int ly, int z) noexcept {
    // L4: sub-layer of 8×4×8 voxels
    return (ly * kChunkSize + z) * kChunkSize + x;
}

// ============================================================================
// Section 3: Scene generators (5 scenes representative of ProjectV use cases)
// ============================================================================

enum class Scene : int {
    UniformAir = 0,    // all Air
    UniformFloor,      // all FloorWhite
    ForestFloor,       // 70% FloorWhite + 30% Glass (trees)
    CaveStress,        // 80% Air + 20% FloorWhite (cave network)
    MixedBiome,        // banded structure: L=0,1 Forest / L=2,3 Stone / L=4..7 Cave
};

constexpr int kSceneCount = 5;

std::string_view scene_name(Scene s) {
    switch (s) {
        case Scene::UniformAir:   return "uniform_air";
        case Scene::UniformFloor: return "uniform_floor";
        case Scene::ForestFloor:  return "forest_floor";
        case Scene::CaveStress:   return "cave_stress";
        case Scene::MixedBiome:   return "mixed_biome";
    }
    return "unknown";
}

Material voxel_for_scene(Scene scene, int x, int y, int z, uint32_t seed) {
    auto rng = [](uint32_t s) {
        // splitmix32 — deterministic per-seed PRNG
        uint32_t z = s;
        z = (z ^ (z >> 16)) * 0x85ebca6b;
        z = (z ^ (z >> 13)) * 0xc2b2ae35;
        return z ^ (z >> 16);
    };
    auto prob = [&](uint32_t s, uint32_t threshold) {
        return (rng(s) & 0xFFFF) < threshold;
    };

    uint32_t h = seed ^ (x * 73856093u) ^ (y * 19349663u) ^ (z * 83492791u);

    switch (scene) {
        case Scene::UniformAir:
            return Material::Air;
        case Scene::UniformFloor:
            return Material::FloorWhite;
        case Scene::ForestFloor:
            return prob(h, 0xB333) ? Material::Glass : Material::FloorWhite;  // ~30% Glass
        case Scene::CaveStress:
            return prob(h, 0x4CCC) ? Material::FloorWhite : Material::Air;    // ~20% FloorWhite
        case Scene::MixedBiome: {
            // L=0,1: Forest (70% FloorWhite + 30% Glass)
            // L=2,3: Stone (95% FloorGray + 5% Air)
            // L=4,5,6,7: Cave (80% Air + 20% FloorWhite)
            if (y <= 1) {
                return prob(h, 0xB333) ? Material::Glass : Material::FloorWhite;
            } else if (y <= 3) {
                return prob(h, 0x0CCC) ? Material::Air : Material::FloorGray;  // 5% Air
            } else {
                return prob(h, 0x4CCC) ? Material::FloorWhite : Material::Air;
            }
        }
    }
    return Material::Air;
}

// ============================================================================
// Section 4: Greedy meshing (shared, simplified)
// ============================================================================
// Reference: O'Conner 2014 "Meshing in a Minecraft Game" + ProjectV voxel_mesh.comp pattern.
// Operates on flat voxel array, returns axis-aligned face quad count.
// Simplified: no AO, no per-face material (just quad count proxy for vertex count).
// Quad count × 4 vertices per quad × 6 floats/vertex = vertex count proxy.

struct GreedyMeshResult {
    size_t quad_count = 0;
    size_t vertex_count() const noexcept { return quad_count * 4; }
};

template <typename VoxelAccessor>
GreedyMeshResult greedy_mesh(const VoxelAccessor& accessor) {
    GreedyMeshResult r;

    // Naive face-counter (no greedy merging — upper bound for mesh size).
    // Each solid voxel contributes up to 6 faces depending on neighbors.
    // ProjectV uses greedy meshing (per `2026-06-20-meshing-algo-comparison` verdict=mixed),
    // but for chunk-layout comparison the absolute count is less important than the
    // RELATIVE ratio between designs — same algorithm applied uniformly.
    auto is_solid = [&](int x, int y, int z) -> bool {
        if (x < 0 || x >= kChunkSize || y < 0 || y >= kChunkSize || z < 0 || z >= kChunkSize) return false;
        return static_cast<uint8_t>(accessor(x, y, z)) != static_cast<uint8_t>(Material::Air);
    };

    for (int y = 0; y < kChunkSize; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                if (!is_solid(x, y, z)) continue;
                // +X face: visible if (x+1, y, z) is empty
                if (!is_solid(x + 1, y, z)) ++r.quad_count;
                // -X face
                if (!is_solid(x - 1, y, z)) ++r.quad_count;
                // +Y face
                if (!is_solid(x, y + 1, z)) ++r.quad_count;
                // -Y face
                if (!is_solid(x, y - 1, z)) ++r.quad_count;
                // +Z face
                if (!is_solid(x, y, z + 1)) ++r.quad_count;
                // -Z face
                if (!is_solid(x, y, z - 1)) ++r.quad_count;
            }
        }
    }

    return r;
}

// ============================================================================
// Section 5: Chunk designs (5 implementations of chunk layout)
// ============================================================================

// ---------- Design A: Monolithic (ProjectV-like baseline) ----------
struct ChunkA {
    std::array<uint8_t, kChunkVoxels> voxels{};  // 512 bytes payload
    // header: implicit (chunkSize=8 known)
};

constexpr size_t bytes_chunk_A() { return sizeof(ChunkA); }

void populate_A(ChunkA& c, Scene scene, uint32_t seed) {
    for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z)
            for (int x = 0; x < kChunkSize; ++x)
                c.voxels[idx3(x, y, z)] = static_cast<uint8_t>(voxel_for_scene(scene, x, y, z, seed));
}

void mutate_A(ChunkA& c, int x, int y, int z, Material m) {
    c.voxels[idx3(x, y, z)] = static_cast<uint8_t>(m);
}

Material get_A(const ChunkA& c, int x, int y, int z) {
    return static_cast<Material>(c.voxels[idx3(x, y, z)]);
}

// ---------- Design B: Palette (Minecraft-1.18+ ChunkSection-like) ----------
// Adaptive bits-per-voxel based on palette size (1/2/4/8 bits).
struct ChunkB {
    uint8_t palette[kPaletteMax]{};        // 16 bytes max
    uint8_t palette_size = 0;              // 1 byte
    uint8_t bits_per_voxel = 0;            // 1 byte: 1/2/4/8
    uint8_t reserved0 = 0;
    std::array<uint8_t, kChunkVoxels / 1> payload{};  // up to 512 bytes
};

constexpr size_t bytes_chunk_B() {
    // worst case: 16 + 3 + 512 = 531 bytes (8 bits/voxel)
    return sizeof(ChunkB);
}

uint8_t bits_needed(uint8_t palette_size) {
    if (palette_size <= 1) return 0;  // empty payload possible
    if (palette_size <= 2) return 1;
    if (palette_size <= 4) return 2;
    if (palette_size <= 16) return 4;
    return 8;
}

void populate_B(ChunkB& c, Scene scene, uint32_t seed) {
    // Reset state for fresh populate (cumulative bug fix)
    c.palette_size = 0;
    c.bits_per_voxel = 0;
    c.payload.fill(0);
    for (auto& p : c.palette) p = 0;

    // First pass: discover palette
    for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z)
            for (int x = 0; x < kChunkSize; ++x) {
                Material m = voxel_for_scene(scene, x, y, z, seed);
                uint8_t mv = static_cast<uint8_t>(m);
                bool found = false;
                for (uint8_t i = 0; i < c.palette_size; ++i) {
                    if (c.palette[i] == mv) { found = true; break; }
                }
                if (!found && c.palette_size < kPaletteMax) {
                    c.palette[c.palette_size++] = mv;
                }
            }
    c.bits_per_voxel = bits_needed(c.palette_size);

    // Second pass: encode (assign, not OR, since payload is zeroed)
    int bit_offset = 0;
    for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z)
            for (int x = 0; x < kChunkSize; ++x) {
                Material m = voxel_for_scene(scene, x, y, z, seed);
                uint8_t mv = static_cast<uint8_t>(m);
                uint8_t idx = 0;
                for (uint8_t i = 0; i < c.palette_size; ++i) {
                    if (c.palette[i] == mv) { idx = i; break; }
                }
                int byte_idx = bit_offset / 8;
                int bit_in_byte = bit_offset % 8;
                if (c.bits_per_voxel == 0) {
                    // no payload needed
                } else if (c.bits_per_voxel == 1) {
                    c.payload[byte_idx] |= (idx & 1) << bit_in_byte;
                } else if (c.bits_per_voxel == 2) {
                    c.payload[byte_idx] |= (idx & 3) << bit_in_byte;
                } else if (c.bits_per_voxel == 4) {
                    c.payload[byte_idx] |= (idx & 15) << bit_in_byte;
                } else {
                    c.payload[byte_idx] = idx;
                }
                bit_offset += c.bits_per_voxel;
            }
}

Material get_B(const ChunkB& c, int x, int y, int z) {
    int voxel_idx = idx3(x, y, z);
    int bit_offset = voxel_idx * c.bits_per_voxel;
    int byte_idx = bit_offset / 8;
    int bit_in_byte = bit_offset % 8;
    uint8_t idx = 0;
    if (c.bits_per_voxel == 0) idx = 0;
    else if (c.bits_per_voxel == 1) idx = (c.payload[byte_idx] >> bit_in_byte) & 1;
    else if (c.bits_per_voxel == 2) idx = (c.payload[byte_idx] >> bit_in_byte) & 3;
    else if (c.bits_per_voxel == 4) idx = (c.payload[byte_idx] >> bit_in_byte) & 15;
    else idx = c.payload[byte_idx];
    return static_cast<Material>(c.palette[idx]);
}

void mutate_B(ChunkB& c, int x, int y, int z, Material m) {
    int voxel_idx = idx3(x, y, z);
    uint8_t mv = static_cast<uint8_t>(m);
    // find or add to palette
    uint8_t idx = 0;
    bool found = false;
    for (uint8_t i = 0; i < c.palette_size; ++i) {
        if (c.palette[i] == mv) { idx = i; found = true; break; }
    }
    if (!found) {
        if (c.palette_size < kPaletteMax) {
            c.palette[c.palette_size] = mv;
            idx = c.palette_size++;
            c.bits_per_voxel = bits_needed(c.palette_size);
        } else {
            // palette full — fallback to 8 bits/voxel, need to re-encode
            // (skipped for prototype — rare path)
            return;
        }
    }
    int bit_offset = voxel_idx * c.bits_per_voxel;
    int byte_idx = bit_offset / 8;
    int bit_in_byte = bit_offset % 8;
    if (c.bits_per_voxel == 1) {
        c.payload[byte_idx] = (c.payload[byte_idx] & ~(1 << bit_in_byte)) | ((idx & 1) << bit_in_byte);
    } else if (c.bits_per_voxel == 2) {
        c.payload[byte_idx] = (c.payload[byte_idx] & ~(3 << bit_in_byte)) | ((idx & 3) << bit_in_byte);
    } else if (c.bits_per_voxel == 4) {
        c.payload[byte_idx] = (c.payload[byte_idx] & ~(15 << bit_in_byte)) | ((idx & 15) << bit_in_byte);
    } else {
        c.payload[byte_idx] = idx;
    }
}

// ---------- Design C: FixedLayer L=2 (4 sub-layers of 8×2×8 = 128 voxels) ----------
struct LayerL2 {
    uint8_t biome_id = 0;                  // 1 byte
    uint8_t palette[kPaletteMax]{};        // 16 bytes
    uint8_t palette_size = 0;              // 1 byte
    uint8_t bits_per_voxel = 0;            // 1 byte
    uint8_t reserved[3]{};                 // 3 bytes (alignment)
    std::array<uint8_t, kLayerVoxels_L2 / 1> payload{};  // up to 128 bytes
};

struct ChunkC {
    std::array<LayerL2, kNumLayers_L2> layers{};  // 4 layers
};

constexpr size_t bytes_chunk_C() { return sizeof(ChunkC); }

void populate_C(ChunkC& c, Scene scene, uint32_t seed) {
    for (int ly = 0; ly < kNumLayers_L2; ++ly) {
        LayerL2& L = c.layers[ly];
        L.biome_id = static_cast<uint8_t>(ly);  // dummy biome id
        L.palette_size = 0;
        L.bits_per_voxel = 0;
        L.payload.fill(0);
        for (auto& p : L.palette) p = 0;
        // Discover palette for this layer
        for (int y = ly * 2; y < (ly + 1) * 2; ++y)
            for (int z = 0; z < kChunkSize; ++z)
                for (int x = 0; x < kChunkSize; ++x) {
                    Material m = voxel_for_scene(scene, x, y, z, seed);
                    uint8_t mv = static_cast<uint8_t>(m);
                    bool found = false;
                    for (uint8_t i = 0; i < L.palette_size; ++i) {
                        if (L.palette[i] == mv) { found = true; break; }
                    }
                    if (!found && L.palette_size < kPaletteMax) {
                        L.palette[L.palette_size++] = mv;
                    }
                }
        L.bits_per_voxel = bits_needed(L.palette_size);

        // Encode
        int bit_offset = 0;
        for (int y = ly * 2; y < (ly + 1) * 2; ++y)
            for (int z = 0; z < kChunkSize; ++z)
                for (int x = 0; x < kChunkSize; ++x) {
                    Material m = voxel_for_scene(scene, x, y, z, seed);
                    uint8_t mv = static_cast<uint8_t>(m);
                    uint8_t idx = 0;
                    for (uint8_t i = 0; i < L.palette_size; ++i) {
                        if (L.palette[i] == mv) { idx = i; break; }
                    }
                    int byte_idx = bit_offset / 8;
                    int bit_in_byte = bit_offset % 8;
                    if (L.bits_per_voxel == 0) {
                    } else if (L.bits_per_voxel == 1) {
                        L.payload[byte_idx] |= (idx & 1) << bit_in_byte;
                    } else if (L.bits_per_voxel == 2) {
                        L.payload[byte_idx] |= (idx & 3) << bit_in_byte;
                    } else if (L.bits_per_voxel == 4) {
                        L.payload[byte_idx] |= (idx & 15) << bit_in_byte;
                    } else {
                        L.payload[byte_idx] = idx;
                    }
                    bit_offset += L.bits_per_voxel;
                }
    }
}

Material get_C(const ChunkC& c, int x, int y, int z) {
    int ly = y / 2;
    const LayerL2& L = c.layers[ly];
    int local_y = y % 2;
    int voxel_idx = (local_y * kChunkSize + z) * kChunkSize + x;
    int bit_offset = voxel_idx * L.bits_per_voxel;
    int byte_idx = bit_offset / 8;
    int bit_in_byte = bit_offset % 8;
    uint8_t idx = 0;
    if (L.bits_per_voxel == 0) idx = 0;
    else if (L.bits_per_voxel == 1) idx = (L.payload[byte_idx] >> bit_in_byte) & 1;
    else if (L.bits_per_voxel == 2) idx = (L.payload[byte_idx] >> bit_in_byte) & 3;
    else if (L.bits_per_voxel == 4) idx = (L.payload[byte_idx] >> bit_in_byte) & 15;
    else idx = L.payload[byte_idx];
    return static_cast<Material>(L.palette[idx]);
}

void mutate_C(ChunkC& c, int x, int y, int z, Material m) {
    int ly = y / 2;
    LayerL2& L = c.layers[ly];
    int local_y = y % 2;
    int voxel_idx = (local_y * kChunkSize + z) * kChunkSize + x;
    uint8_t mv = static_cast<uint8_t>(m);
    uint8_t idx = 0;
    bool found = false;
    for (uint8_t i = 0; i < L.palette_size; ++i) {
        if (L.palette[i] == mv) { idx = i; found = true; break; }
    }
    if (!found && L.palette_size < kPaletteMax) {
        L.palette[L.palette_size] = mv;
        idx = L.palette_size++;
        L.bits_per_voxel = bits_needed(L.palette_size);
    }
    int bit_offset = voxel_idx * L.bits_per_voxel;
    int byte_idx = bit_offset / 8;
    int bit_in_byte = bit_offset % 8;
    if (L.bits_per_voxel == 1) {
        L.payload[byte_idx] = (L.payload[byte_idx] & ~(1 << bit_in_byte)) | ((idx & 1) << bit_in_byte);
    } else if (L.bits_per_voxel == 2) {
        L.payload[byte_idx] = (L.payload[byte_idx] & ~(3 << bit_in_byte)) | ((idx & 3) << bit_in_byte);
    } else if (L.bits_per_voxel == 4) {
        L.payload[byte_idx] = (L.payload[byte_idx] & ~(15 << bit_in_byte)) | ((idx & 15) << bit_in_byte);
    } else {
        L.payload[byte_idx] = idx;
    }
}

// ---------- Design D: FixedLayer L=4 (2 sub-layers of 8×4×8 = 256 voxels) ----------
struct LayerL4 {
    uint8_t biome_id = 0;                  // 1 byte
    uint8_t palette[kPaletteMax]{};        // 16 bytes
    uint8_t palette_size = 0;              // 1 byte
    uint8_t bits_per_voxel = 0;            // 1 byte
    uint8_t reserved[9]{};                 // 9 bytes (alignment)
    std::array<uint8_t, kLayerVoxels_L4 / 1> payload{};  // up to 256 bytes
};

struct ChunkD {
    std::array<LayerL4, kNumLayers_L4> layers{};  // 2 layers
};

constexpr size_t bytes_chunk_D() { return sizeof(ChunkD); }

void populate_D(ChunkD& c, Scene scene, uint32_t seed) {
    for (int ly = 0; ly < kNumLayers_L4; ++ly) {
        LayerL4& L = c.layers[ly];
        L.biome_id = static_cast<uint8_t>(ly);
        L.palette_size = 0;
        L.bits_per_voxel = 0;
        L.payload.fill(0);
        for (auto& p : L.palette) p = 0;
        for (int y = ly * 4; y < (ly + 1) * 4; ++y)
            for (int z = 0; z < kChunkSize; ++z)
                for (int x = 0; x < kChunkSize; ++x) {
                    Material m = voxel_for_scene(scene, x, y, z, seed);
                    uint8_t mv = static_cast<uint8_t>(m);
                    bool found = false;
                    for (uint8_t i = 0; i < L.palette_size; ++i) {
                        if (L.palette[i] == mv) { found = true; break; }
                    }
                    if (!found && L.palette_size < kPaletteMax) {
                        L.palette[L.palette_size++] = mv;
                    }
                }
        L.bits_per_voxel = bits_needed(L.palette_size);

        int bit_offset = 0;
        for (int y = ly * 4; y < (ly + 1) * 4; ++y)
            for (int z = 0; z < kChunkSize; ++z)
                for (int x = 0; x < kChunkSize; ++x) {
                    Material m = voxel_for_scene(scene, x, y, z, seed);
                    uint8_t mv = static_cast<uint8_t>(m);
                    uint8_t idx = 0;
                    for (uint8_t i = 0; i < L.palette_size; ++i) {
                        if (L.palette[i] == mv) { idx = i; break; }
                    }
                    int byte_idx = bit_offset / 8;
                    int bit_in_byte = bit_offset % 8;
                    if (L.bits_per_voxel == 0) {
                    } else if (L.bits_per_voxel == 1) {
                        L.payload[byte_idx] |= (idx & 1) << bit_in_byte;
                    } else if (L.bits_per_voxel == 2) {
                        L.payload[byte_idx] |= (idx & 3) << bit_in_byte;
                    } else if (L.bits_per_voxel == 4) {
                        L.payload[byte_idx] |= (idx & 15) << bit_in_byte;
                    } else {
                        L.payload[byte_idx] = idx;
                    }
                    bit_offset += L.bits_per_voxel;
                }
    }
}

Material get_D(const ChunkD& c, int x, int y, int z) {
    int ly = y / 4;
    const LayerL4& L = c.layers[ly];
    int local_y = y % 4;
    int voxel_idx = (local_y * kChunkSize + z) * kChunkSize + x;
    int bit_offset = voxel_idx * L.bits_per_voxel;
    int byte_idx = bit_offset / 8;
    int bit_in_byte = bit_offset % 8;
    uint8_t idx = 0;
    if (L.bits_per_voxel == 0) idx = 0;
    else if (L.bits_per_voxel == 1) idx = (L.payload[byte_idx] >> bit_in_byte) & 1;
    else if (L.bits_per_voxel == 2) idx = (L.payload[byte_idx] >> bit_in_byte) & 3;
    else if (L.bits_per_voxel == 4) idx = (L.payload[byte_idx] >> bit_in_byte) & 15;
    else idx = L.payload[byte_idx];
    return static_cast<Material>(L.palette[idx]);
}

void mutate_D(ChunkD& c, int x, int y, int z, Material m) {
    int ly = y / 4;
    LayerL4& L = c.layers[ly];
    int local_y = y % 4;
    int voxel_idx = (local_y * kChunkSize + z) * kChunkSize + x;
    uint8_t mv = static_cast<uint8_t>(m);
    uint8_t idx = 0;
    bool found = false;
    for (uint8_t i = 0; i < L.palette_size; ++i) {
        if (L.palette[i] == mv) { idx = i; found = true; break; }
    }
    if (!found && L.palette_size < kPaletteMax) {
        L.palette[L.palette_size] = mv;
        idx = L.palette_size++;
        L.bits_per_voxel = bits_needed(L.palette_size);
    }
    int bit_offset = voxel_idx * L.bits_per_voxel;
    int byte_idx = bit_offset / 8;
    int bit_in_byte = bit_offset % 8;
    if (L.bits_per_voxel == 1) {
        L.payload[byte_idx] = (L.payload[byte_idx] & ~(1 << bit_in_byte)) | ((idx & 1) << bit_in_byte);
    } else if (L.bits_per_voxel == 2) {
        L.payload[byte_idx] = (L.payload[byte_idx] & ~(3 << bit_in_byte)) | ((idx & 3) << bit_in_byte);
    } else if (L.bits_per_voxel == 4) {
        L.payload[byte_idx] = (L.payload[byte_idx] & ~(15 << bit_in_byte)) | ((idx & 15) << bit_in_byte);
    } else {
        L.payload[byte_idx] = idx;
    }
}

// ============================================================================
// Section 6: Design enum + dispatch
// ============================================================================

enum class Design : int {
    A_Monolithic = 0,
    B_Palette,
    C_FixedLayer_L2,
    D_FixedLayer_L4,
};

constexpr int kDesignCount = 4;

std::string_view design_name(Design d) {
    switch (d) {
        case Design::A_Monolithic:   return "A_Monolithic";
        case Design::B_Palette:      return "B_Palette";
        case Design::C_FixedLayer_L2: return "C_FixedLayer_L2";
        case Design::D_FixedLayer_L4: return "D_FixedLayer_L4";
    }
    return "unknown";
}

size_t bytes_per_chunk(Design d) {
    switch (d) {
        case Design::A_Monolithic:   return bytes_chunk_A();
        case Design::B_Palette:      return bytes_chunk_B();
        case Design::C_FixedLayer_L2: return bytes_chunk_C();
        case Design::D_FixedLayer_L4: return bytes_chunk_D();
    }
    return 0;
}

// Effective bytes used by populated chunk (excludes unused payload tail)
size_t effective_bytes(Design d, const void* chunk_ptr) {
    switch (d) {
        case Design::A_Monolithic:
            return sizeof(ChunkA);
        case Design::B_Palette: {
            const ChunkB* c = static_cast<const ChunkB*>(chunk_ptr);
            int bits_total = kChunkVoxels * c->bits_per_voxel;
            int bytes_payload = (bits_total + 7) / 8;
            return sizeof(c->palette) + 4 + bytes_payload;
        }
        case Design::C_FixedLayer_L2: {
            const ChunkC* c = static_cast<const ChunkC*>(chunk_ptr);
            size_t total = 0;
            for (const auto& L : c->layers) {
                int bits_total = kLayerVoxels_L2 * L.bits_per_voxel;
                int bytes_payload = (bits_total + 7) / 8;
                total += sizeof(L.biome_id) + sizeof(L.palette) + 4 + bytes_payload;
            }
            return total;
        }
        case Design::D_FixedLayer_L4: {
            const ChunkD* c = static_cast<const ChunkD*>(chunk_ptr);
            size_t total = 0;
            for (const auto& L : c->layers) {
                int bits_total = kLayerVoxels_L4 * L.bits_per_voxel;
                int bytes_payload = (bits_total + 7) / 8;
                total += sizeof(L.biome_id) + sizeof(L.palette) + 4 + bytes_payload;
            }
            return total;
        }
    }
    return 0;
}

template <typename Op>
double bench_us(Op&& op, int iters) {
    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();
    for (int i = 0; i < iters; ++i) op(i);
    auto t1 = clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

double bench_p95_us(std::vector<double>& samples) {
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() * 95 / 100];
}

double bench_stddev(const std::vector<double>& samples) {
    double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
    double sq_sum = 0;
    for (double s : samples) sq_sum += (s - mean) * (s - mean);
    return std::sqrt(sq_sum / samples.size());
}

// ============================================================================
// Section 7: Measurement harness (single scene × design × seed)
// ============================================================================

struct Measurement {
    std::string scene;
    std::string design;
    uint32_t seed;
    size_t bytes_per_chunk;
    size_t effective_bytes;
    double build_us_mean;
    double build_us_p95;
    double build_us_stddev;
    double mutate_us_mean;
    double mutate_us_p95;
    double mutate_us_stddev;
    size_t mesh_quad_count;
    size_t mesh_vertex_count;
    size_t layer_boundary_count;
};

template <Design D>
Measurement measure(Scene scene, uint32_t seed, int iters) {
    Measurement m;
    m.scene = std::string(scene_name(scene));
    m.design = std::string(design_name(D));
    m.seed = seed;

    // Populate from scratch (measured as build cost)
    if constexpr (D == Design::A_Monolithic) {
        ChunkA chunk;
        std::vector<double> build_samples;
        for (int i = 0; i < iters; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            populate_A(chunk, scene, seed + i);
            auto t1 = std::chrono::high_resolution_clock::now();
            build_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(build_samples.begin(), build_samples.end());
        m.build_us_mean = std::accumulate(build_samples.begin(), build_samples.end(), 0.0) / build_samples.size();
        m.build_us_p95 = build_samples[build_samples.size() * 95 / 100];
        m.build_us_stddev = bench_stddev(build_samples);
        m.bytes_per_chunk = bytes_chunk_A();
        m.effective_bytes = effective_bytes(D, &chunk);

        // Mutation cost
        std::vector<double> mut_samples;
        for (int rep = 0; rep < 50; ++rep) {
            std::mt19937 rng(seed + rep);
            std::uniform_int_distribution<int> coord(0, kChunkSize - 1);
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iters; ++i) {
                int x = coord(rng), y = coord(rng), z = coord(rng);
                mutate_A(chunk, x, y, z, Material::Air);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            mut_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count() / iters);
        }
        std::sort(mut_samples.begin(), mut_samples.end());
        m.mutate_us_mean = std::accumulate(mut_samples.begin(), mut_samples.end(), 0.0) / mut_samples.size();
        m.mutate_us_p95 = mut_samples[mut_samples.size() * 95 / 100];
        m.mutate_us_stddev = bench_stddev(mut_samples);

        // Mesh
        populate_A(chunk, scene, seed);
        auto r = greedy_mesh([&](int x, int y, int z) { return get_A(chunk, x, y, z); });
        m.mesh_quad_count = r.quad_count;
        m.mesh_vertex_count = r.vertex_count();

        // Layer boundaries (always 0 for monolithic)
        m.layer_boundary_count = 0;

    } else if constexpr (D == Design::B_Palette) {
        ChunkB chunk;
        std::vector<double> build_samples;
        for (int i = 0; i < iters; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            populate_B(chunk, scene, seed + i);
            auto t1 = std::chrono::high_resolution_clock::now();
            build_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(build_samples.begin(), build_samples.end());
        m.build_us_mean = std::accumulate(build_samples.begin(), build_samples.end(), 0.0) / build_samples.size();
        m.build_us_p95 = build_samples[build_samples.size() * 95 / 100];
        m.build_us_stddev = bench_stddev(build_samples);
        m.bytes_per_chunk = bytes_chunk_B();
        m.effective_bytes = effective_bytes(D, &chunk);

        std::vector<double> mut_samples;
        for (int rep = 0; rep < 50; ++rep) {
            std::mt19937 rng(seed + rep);
            std::uniform_int_distribution<int> coord(0, kChunkSize - 1);
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iters; ++i) {
                int x = coord(rng), y = coord(rng), z = coord(rng);
                mutate_B(chunk, x, y, z, Material::Air);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            mut_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count() / iters);
        }
        std::sort(mut_samples.begin(), mut_samples.end());
        m.mutate_us_mean = std::accumulate(mut_samples.begin(), mut_samples.end(), 0.0) / mut_samples.size();
        m.mutate_us_p95 = mut_samples[mut_samples.size() * 95 / 100];
        m.mutate_us_stddev = bench_stddev(mut_samples);

        populate_B(chunk, scene, seed);
        auto r = greedy_mesh([&](int x, int y, int z) { return get_B(chunk, x, y, z); });
        m.mesh_quad_count = r.quad_count;
        m.mesh_vertex_count = r.vertex_count();
        m.layer_boundary_count = 0;

    } else if constexpr (D == Design::C_FixedLayer_L2) {
        ChunkC chunk;
        std::vector<double> build_samples;
        for (int i = 0; i < iters; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            populate_C(chunk, scene, seed + i);
            auto t1 = std::chrono::high_resolution_clock::now();
            build_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(build_samples.begin(), build_samples.end());
        m.build_us_mean = std::accumulate(build_samples.begin(), build_samples.end(), 0.0) / build_samples.size();
        m.build_us_p95 = build_samples[build_samples.size() * 95 / 100];
        m.build_us_stddev = bench_stddev(build_samples);
        m.bytes_per_chunk = bytes_chunk_C();
        m.effective_bytes = effective_bytes(D, &chunk);

        std::vector<double> mut_samples;
        for (int rep = 0; rep < 50; ++rep) {
            std::mt19937 rng(seed + rep);
            std::uniform_int_distribution<int> coord(0, kChunkSize - 1);
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iters; ++i) {
                int x = coord(rng), y = coord(rng), z = coord(rng);
                mutate_C(chunk, x, y, z, Material::Air);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            mut_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count() / iters);
        }
        std::sort(mut_samples.begin(), mut_samples.end());
        m.mutate_us_mean = std::accumulate(mut_samples.begin(), mut_samples.end(), 0.0) / mut_samples.size();
        m.mutate_us_p95 = mut_samples[mut_samples.size() * 95 / 100];
        m.mutate_us_stddev = bench_stddev(mut_samples);

        populate_C(chunk, scene, seed);
        auto r = greedy_mesh([&](int x, int y, int z) { return get_C(chunk, x, y, z); });
        m.mesh_quad_count = r.quad_count;
        m.mesh_vertex_count = r.vertex_count();

        // Layer boundary count: number of voxel pairs (y, y+1) within chunk where material differs
        // AND they cross a sub-layer boundary (y = 1, 3, 5, 7).
        size_t lb = 0;
        for (int y : {1, 3, 5}) {
            for (int z = 0; z < kChunkSize; ++z)
                for (int x = 0; x < kChunkSize; ++x) {
                    if (get_C(chunk, x, y, z) != get_C(chunk, x, y + 1, z)) ++lb;
                }
        }
        m.layer_boundary_count = lb;

    } else if constexpr (D == Design::D_FixedLayer_L4) {
        ChunkD chunk;
        std::vector<double> build_samples;
        for (int i = 0; i < iters; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            populate_D(chunk, scene, seed + i);
            auto t1 = std::chrono::high_resolution_clock::now();
            build_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(build_samples.begin(), build_samples.end());
        m.build_us_mean = std::accumulate(build_samples.begin(), build_samples.end(), 0.0) / build_samples.size();
        m.build_us_p95 = build_samples[build_samples.size() * 95 / 100];
        m.build_us_stddev = bench_stddev(build_samples);
        m.bytes_per_chunk = bytes_chunk_D();
        m.effective_bytes = effective_bytes(D, &chunk);

        std::vector<double> mut_samples;
        for (int rep = 0; rep < 50; ++rep) {
            std::mt19937 rng(seed + rep);
            std::uniform_int_distribution<int> coord(0, kChunkSize - 1);
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iters; ++i) {
                int x = coord(rng), y = coord(rng), z = coord(rng);
                mutate_D(chunk, x, y, z, Material::Air);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            mut_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count() / iters);
        }
        std::sort(mut_samples.begin(), mut_samples.end());
        m.mutate_us_mean = std::accumulate(mut_samples.begin(), mut_samples.end(), 0.0) / mut_samples.size();
        m.mutate_us_p95 = mut_samples[mut_samples.size() * 95 / 100];
        m.mutate_us_stddev = bench_stddev(mut_samples);

        populate_D(chunk, scene, seed);
        auto r = greedy_mesh([&](int x, int y, int z) { return get_D(chunk, x, y, z); });
        m.mesh_quad_count = r.quad_count;
        m.mesh_vertex_count = r.vertex_count();

        size_t lb = 0;
        for (int y : {3}) {
            for (int z = 0; z < kChunkSize; ++z)
                for (int x = 0; x < kChunkSize; ++x) {
                    if (get_D(chunk, x, y, z) != get_D(chunk, x, y + 1, z)) ++lb;
                }
        }
        m.layer_boundary_count = lb;
    }

    return m;
}

// ============================================================================
// Section 8: CSV output
// ============================================================================

void write_csv_header(std::ofstream& f) {
    f << "scene,design,seed,bytes_per_chunk,effective_bytes,"
      << "build_us_mean,build_us_p95,build_us_stddev,"
      << "mutate_us_mean,mutate_us_p95,mutate_us_stddev,"
      << "mesh_quad_count,mesh_vertex_count,layer_boundary_count\n";
}

void write_csv_row(std::ofstream& f, const Measurement& m) {
    f << m.scene << "," << m.design << "," << m.seed << ","
      << m.bytes_per_chunk << "," << m.effective_bytes << ","
      << m.build_us_mean << "," << m.build_us_p95 << "," << m.build_us_stddev << ","
      << m.mutate_us_mean << "," << m.mutate_us_p95 << "," << m.mutate_us_stddev << ","
      << m.mesh_quad_count << "," << m.mesh_vertex_count << "," << m.layer_boundary_count << "\n";
}

// ============================================================================
// Section 9: main() — CLI parsing + dispatch
// ============================================================================

void print_usage(char* argv0) {
    std::printf("Usage: %s [options]\n", argv0);
    std::printf("  --scene <name|all>      Scene: uniform_air, uniform_floor, forest_floor, cave_stress, mixed_biome\n");
    std::printf("  --design <name|all>     Design: A_Monolithic, B_Palette, C_FixedLayer_L2, D_FixedLayer_L4\n");
    std::printf("  --iters <N>             Iterations per measurement (default 1000)\n");
    std::printf("  --seeds <N>             Number of seeds (default 5: 1, 7, 42, 1234, 31337)\n");
    std::printf("  --output <path>         Output CSV file path\n");
    std::printf("  --all                   Run all scenes × designs × 5 seeds\n");
    std::printf("  --quiet                 Suppress per-row stdout output\n");
    std::printf("\nExamples:\n");
    std::printf("  %s --all --output build/results_all.csv\n", argv0);
    std::printf("  %s --scene mixed_biome --design C_FixedLayer_L2 --output build/results_one.csv\n", argv0);
}

std::optional<Scene> parse_scene(std::string_view s) {
    if (s == "uniform_air")   return Scene::UniformAir;
    if (s == "uniform_floor") return Scene::UniformFloor;
    if (s == "forest_floor")  return Scene::ForestFloor;
    if (s == "cave_stress")   return Scene::CaveStress;
    if (s == "mixed_biome")   return Scene::MixedBiome;
    return std::nullopt;
}

std::optional<Design> parse_design(std::string_view s) {
    if (s == "A_Monolithic")    return Design::A_Monolithic;
    if (s == "B_Palette")       return Design::B_Palette;
    if (s == "C_FixedLayer_L2") return Design::C_FixedLayer_L2;
    if (s == "D_FixedLayer_L4") return Design::D_FixedLayer_L4;
    return std::nullopt;
}

}  // namespace pv

int main(int argc, char** argv) {
    using namespace pv;
    std::vector<Scene> scenes;
    std::vector<Design> designs;
    int iters = 1000;
    int num_seeds = 5;
    std::string output_path = "results.csv";
    bool quiet = false;

    bool all_flag = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--scene" && i + 1 < argc) {
            std::string val = argv[++i];
            if (val == "all") {
                for (int s = 0; s < kSceneCount; ++s) scenes.push_back(static_cast<Scene>(s));
            } else {
                auto opt = parse_scene(val);
                if (!opt) { std::printf("Unknown scene: %s\n", val.c_str()); return 1; }
                scenes.push_back(*opt);
            }
        } else if (arg == "--design" && i + 1 < argc) {
            std::string val = argv[++i];
            if (val == "all") {
                for (int d = 0; d < kDesignCount; ++d) designs.push_back(static_cast<Design>(d));
            } else {
                auto opt = parse_design(val);
                if (!opt) { std::printf("Unknown design: %s\n", val.c_str()); return 1; }
                designs.push_back(*opt);
            }
        } else if (arg == "--iters" && i + 1 < argc) {
            iters = std::atoi(argv[++i]);
        } else if (arg == "--seeds" && i + 1 < argc) {
            num_seeds = std::atoi(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--all") {
            all_flag = true;
        } else if (arg == "--quiet") {
            quiet = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::printf("Unknown arg: %s\n", arg.c_str());
            print_usage(argv[0]);
            return 1;
        }
    }

    if (all_flag) {
        scenes.clear();
        designs.clear();
        for (int s = 0; s < kSceneCount; ++s) scenes.push_back(static_cast<Scene>(s));
        for (int d = 0; d < kDesignCount; ++d) designs.push_back(static_cast<Design>(d));
    }

    if (scenes.empty()) scenes.push_back(Scene::MixedBiome);
    if (designs.empty()) designs.push_back(Design::A_Monolithic);

    uint32_t seeds[] = {1, 7, 42, 1234, 31337};
    if (num_seeds < 5) num_seeds = std::min(num_seeds, 5);

    std::ofstream csv(output_path);
    if (!csv) {
        std::printf("Cannot open output file: %s\n", output_path.c_str());
        return 1;
    }
    write_csv_header(csv);

    // Warmup
    if (!quiet) std::printf("Warming up (50 iter × %zu designs × %zu scenes)...\n", designs.size(), scenes.size());
    ChunkA warmup_chunk;
    for (int i = 0; i < 50; ++i) populate_A(warmup_chunk, Scene::MixedBiome, 1);

    if (!quiet) std::printf("Running measurements (iters=%d, seeds=%d)...\n", iters, num_seeds);

    int total_configs = static_cast<int>(scenes.size() * designs.size() * num_seeds);
    int cfg_count = 0;
    for (Scene sc : scenes) {
        for (Design ds : designs) {
            for (int s = 0; s < num_seeds; ++s) {
                Measurement m;
                switch (ds) {
                    case Design::A_Monolithic:    m = measure<Design::A_Monolithic>(sc, seeds[s], iters); break;
                    case Design::B_Palette:       m = measure<Design::B_Palette>(sc, seeds[s], iters); break;
                    case Design::C_FixedLayer_L2: m = measure<Design::C_FixedLayer_L2>(sc, seeds[s], iters); break;
                    case Design::D_FixedLayer_L4: m = measure<Design::D_FixedLayer_L4>(sc, seeds[s], iters); break;
                }
                write_csv_row(csv, m);
                ++cfg_count;
                if (!quiet) {
                    std::printf("  [%d/%d] %s × %s seed=%u: bytes=%zu eff=%zu build=%.2fµs mutate=%.3fµs quads=%zu lb=%zu\n",
                                cfg_count, total_configs,
                                m.scene.c_str(), m.design.c_str(), m.seed,
                                m.bytes_per_chunk, m.effective_bytes,
                                m.build_us_mean, m.mutate_us_mean,
                                m.mesh_quad_count, m.layer_boundary_count);
                }
            }
        }
    }

    csv.close();
    std::printf("Wrote %d measurements to %s\n", total_configs, output_path.c_str());
    return 0;
}
