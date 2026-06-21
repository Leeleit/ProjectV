// lod_bench.cpp — standalone CPU benchmark для Stage 4.2 LOD uniform downsampling + stitch strategy.
// 4 downsample kernels × 3 stitch strategies × 5 scenes × 4 LOD levels = 240 measurements + T-junction
// hole detection на 2-chunk cluster.
//
// Builds:
//   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
//   cmake --build build -j$(nproc)
//
// Usage:
//   ./build/lod_bench --all --iters 1000 --seeds 5 --output build/results.csv
//   ./build/lod_bench --scene mixed_biome --kernel B --stitch Z --output build/results_one.csv
//   ./build/lod_bench --tjunc-only --output build/tjunc.csv
//
// Per `docs/experiments/benchmarks/methodology.md §3`:
//   - 5 seeds (1, 7, 42, 1234, 31337), fixed
//   - 1000 iter per measurement, 10 warmup
//   - Mean + p95 + stddev for timing
//   - Machine-readable CSV + human-readable stdout summary
//
// Standalone (no Vulkan, no ProjectV mainline), synthetic scenes representative of
// Minecraft-1.18+ biome/cave structure per `2026-06-21-sub-chunk-layers` precedent (same scenes
// + same seeds + same Material enum → direct comparability).

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
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// Section 1: VoxelMaterial + chunk constants (ProjectV-aligned, sub-chunk-layers compatible)
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
constexpr int kLODMax = 3;  // LOD 0, 1, 2, 3

inline int idx3(int x, int y, int z) noexcept {
    return (y * kChunkSize + z) * kChunkSize + x;
}

inline int idx3_at(int x, int y, int z, int s) noexcept {
    return (y * s + z) * s + x;
}

inline int lod_size(int lod) noexcept {
    int s = kChunkSize >> lod;
    return s < 1 ? 1 : s;
}

inline int lod_voxels(int lod) noexcept {
    int s = lod_size(lod);
    return s * s * s;
}

// ============================================================================
// Section 2: splitmix32 RNG + scene generators (sub-chunk-layers compatible)
// ============================================================================

enum class Scene : int {
    UniformAir = 0,
    UniformFloor,
    ForestFloor,
    CaveStress,
    MixedBiome,
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

inline uint32_t splitmix32(uint32_t z) noexcept {
    z = (z ^ (z >> 16)) * 0x85ebca6b;
    z = (z ^ (z >> 13)) * 0xc2b2ae35;
    return z ^ (z >> 16);
}

Material voxel_for_scene(Scene scene, int x, int y, int z, uint32_t seed) {
    auto prob = [&](uint32_t s, uint32_t threshold) {
        return (splitmix32(s) & 0xFFFF) < threshold;
    };
    uint32_t h = seed ^ (static_cast<uint32_t>(x) * 73856093u) ^
                 (static_cast<uint32_t>(y) * 19349663u) ^
                 (static_cast<uint32_t>(z) * 83492791u);
    switch (scene) {
        case Scene::UniformAir:   return Material::Air;
        case Scene::UniformFloor: return Material::FloorWhite;
        case Scene::ForestFloor:  return prob(h, 0xB333) ? Material::Glass : Material::FloorWhite;
        case Scene::CaveStress:   return prob(h, 0x4CCC) ? Material::FloorWhite : Material::Air;
        case Scene::MixedBiome:
            if (y <= 1) return prob(h, 0xB333) ? Material::Glass : Material::FloorWhite;
            if (y <= 3) return prob(h, 0x0CCC) ? Material::Air : Material::FloorGray;
            return prob(h, 0x4CCC) ? Material::FloorWhite : Material::Air;
    }
    return Material::Air;
}

void populate_chunk(std::array<uint8_t, kChunkVoxels>& c, Scene scene, uint32_t seed) {
    for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z)
            for (int x = 0; x < kChunkSize; ++x)
                c[idx3(x, y, z)] = static_cast<uint8_t>(voxel_for_scene(scene, x, y, z, seed));
}

// ============================================================================
// Section 3: Naive Greedy meshing (face counter, sub-chunk-layers compatible)
// ============================================================================

struct MeshResult {
    size_t quad_count = 0;
    size_t vertex_count() const noexcept { return quad_count * 4; }
};

// Emit quads for a single (LOD-sized) voxel grid `voxels[s³]` without boundary check.
// Caller pre-fills boundary buffer (1-voxel padding) for inter-chunk face emission.
template <int S>
MeshResult naive_mesh(const uint8_t* voxels) {
    MeshResult r;
    auto is_solid = [&](int x, int y, int z) -> bool {
        if (x < 0 || x >= S || y < 0 || y >= S || z < 0 || z >= S) return false;
        return voxels[idx3_at(x, y, z, S)] != static_cast<uint8_t>(Material::Air);
    };
    for (int y = 0; y < S; ++y) {
        for (int z = 0; z < S; ++z) {
            for (int x = 0; x < S; ++x) {
                if (!is_solid(x, y, z)) continue;
                if (!is_solid(x + 1, y, z)) ++r.quad_count;
                if (!is_solid(x - 1, y, z)) ++r.quad_count;
                if (!is_solid(x, y + 1, z)) ++r.quad_count;
                if (!is_solid(x, y - 1, z)) ++r.quad_count;
                if (!is_solid(x, y, z + 1)) ++r.quad_count;
                if (!is_solid(x, y, z - 1)) ++r.quad_count;
            }
        }
    }
    return r;
}

// ============================================================================
// Section 4: 4 downsampler kernels
// ============================================================================

enum class Kernel : int { A_Majority3D = 0, B_SurfacePreserve, C_SolidOnly, D_MaxPool };
constexpr int kKernelCount = 4;

std::string_view kernel_name(Kernel k) {
    switch (k) {
        case Kernel::A_Majority3D:     return "A_Majority3D";
        case Kernel::B_SurfacePreserve: return "B_SurfacePreserve";
        case Kernel::C_SolidOnly:      return "C_SolidOnly";
        case Kernel::D_MaxPool:        return "D_MaxPool";
    }
    return "unknown";
}

// Helper: histogram of up to 512 source voxels (step³ for step in {1,2,4,8}).
// Returns the most common material; tie-break prefer non-Air.
Material majority_group(const uint8_t* group, int n) {
    int hist[kMaterialCount] = {0, 0, 0, 0, 0};
    for (int i = 0; i < n; ++i) hist[group[i]]++;
    int best = 0, best_count = -1;
    for (int m = 0; m < kMaterialCount; ++m) {
        if (hist[m] > best_count ||
            (hist[m] == best_count && m != 0 && best == 0)) {
            best_count = hist[m];
            best = m;
        }
    }
    return static_cast<Material>(best);
}

// Kernel A: 3D majority with non-Air preference. O(step³) per output voxel.
void downsample_A(const uint8_t* src, uint8_t* dst, int dst_size) {
    const int step = kChunkSize / dst_size;
    uint8_t g[512];  // max step³ = 8³ = 512
    for (int y = 0; y < dst_size; ++y) {
        for (int z = 0; z < dst_size; ++z) {
            for (int x = 0; x < dst_size; ++x) {
                int sx = x * step, sy = y * step, sz = z * step;
                int k = 0;
                for (int dy = 0; dy < step; ++dy)
                    for (int dz = 0; dz < step; ++dz)
                        for (int dx = 0; dx < step; ++dx)
                            g[k++] = src[idx3(sx + dx, sy + dy, sz + dz)];
                dst[idx3_at(x, y, z, dst_size)] = static_cast<uint8_t>(majority_group(g, k));
            }
        }
    }
}

// Kernel B: surface-preserving. If all source voxels are same material, output that.
// Otherwise count histogram of non-Air voxels only; pick best (or Air if all-Air).
void downsample_B(const uint8_t* src, uint8_t* dst, int dst_size) {
    const int step = kChunkSize / dst_size;
    uint8_t g[512];
    for (int y = 0; y < dst_size; ++y) {
        for (int z = 0; z < dst_size; ++z) {
            for (int x = 0; x < dst_size; ++x) {
                int sx = x * step, sy = y * step, sz = z * step;
                int k = 0;
                for (int dy = 0; dy < step; ++dy)
                    for (int dz = 0; dz < step; ++dz)
                        for (int dx = 0; dx < step; ++dx)
                            g[k++] = src[idx3(sx + dx, sy + dy, sz + dz)];

                bool all_same = true;
                for (int i = 1; i < k; ++i) if (g[i] != g[0]) { all_same = false; break; }
                if (all_same) {
                    dst[idx3_at(x, y, z, dst_size)] = g[0];
                    continue;
                }
                int hist[kMaterialCount] = {0, 0, 0, 0, 0};
                for (int i = 0; i < k; ++i) {
                    if (g[i] == static_cast<uint8_t>(Material::Air)) continue;
                    hist[g[i]]++;
                }
                int best = 0, best_count = -1;
                for (int m = 1; m < kMaterialCount; ++m) {
                    if (hist[m] > best_count) { best_count = hist[m]; best = m; }
                }
                if (best_count == 0) best = 0;
                dst[idx3_at(x, y, z, dst_size)] = static_cast<uint8_t>(best);
            }
        }
    }
}

// Kernel C: solid-only. Output Air unless ALL step³ source voxels are non-air; else majority.
void downsample_C(const uint8_t* src, uint8_t* dst, int dst_size) {
    const int step = kChunkSize / dst_size;
    uint8_t g[512];
    for (int y = 0; y < dst_size; ++y) {
        for (int z = 0; z < dst_size; ++z) {
            for (int x = 0; x < dst_size; ++x) {
                int sx = x * step, sy = y * step, sz = z * step;
                int k = 0;
                for (int dy = 0; dy < step; ++dy)
                    for (int dz = 0; dz < step; ++dz)
                        for (int dx = 0; dx < step; ++dx)
                            g[k++] = src[idx3(sx + dx, sy + dy, sz + dz)];
                bool all_solid = true;
                for (int i = 0; i < k; ++i) {
                    if (g[i] == static_cast<uint8_t>(Material::Air)) { all_solid = false; break; }
                }
                if (!all_solid) {
                    dst[idx3_at(x, y, z, dst_size)] = static_cast<uint8_t>(Material::Air);
                } else {
                    dst[idx3_at(x, y, z, dst_size)] = static_cast<uint8_t>(majority_group(g, k));
                }
            }
        }
    }
}

// Kernel D: max-pool. Output non-Air if ANY of step³ is non-Air, else Air; material = majority.
void downsample_D(const uint8_t* src, uint8_t* dst, int dst_size) {
    const int step = kChunkSize / dst_size;
    uint8_t g[512];
    for (int y = 0; y < dst_size; ++y) {
        for (int z = 0; z < dst_size; ++z) {
            for (int x = 0; x < dst_size; ++x) {
                int sx = x * step, sy = y * step, sz = z * step;
                int k = 0;
                for (int dy = 0; dy < step; ++dy)
                    for (int dz = 0; dz < step; ++dz)
                        for (int dx = 0; dx < step; ++dx)
                            g[k++] = src[idx3(sx + dx, sy + dy, sz + dz)];
                bool any_solid = false;
                for (int i = 0; i < k; ++i) {
                    if (g[i] != static_cast<uint8_t>(Material::Air)) { any_solid = true; break; }
                }
                if (!any_solid) {
                    dst[idx3_at(x, y, z, dst_size)] = static_cast<uint8_t>(Material::Air);
                } else {
                    dst[idx3_at(x, y, z, dst_size)] = static_cast<uint8_t>(majority_group(g, k));
                }
            }
        }
    }
}

void downsample(Kernel k, const uint8_t* src, uint8_t* dst, int dst_size) {
    switch (k) {
        case Kernel::A_Majority3D:      downsample_A(src, dst, dst_size); break;
        case Kernel::B_SurfacePreserve: downsample_B(src, dst, dst_size); break;
        case Kernel::C_SolidOnly:       downsample_C(src, dst, dst_size); break;
        case Kernel::D_MaxPool:         downsample_D(src, dst, dst_size); break;
    }
}

// ============================================================================
// Section 5: 3 stitch strategies + extended mesh result
// ============================================================================

enum class Stitcher : int { X_None = 0, Y_TJunctionPad, Z_NeighborLocked };
constexpr int kStitcherCount = 3;

std::string_view stitcher_name(Stitcher s) {
    switch (s) {
        case Stitcher::X_None:           return "X_None";
        case Stitcher::Y_TJunctionPad:   return "Y_TJunctionPad";
        case Stitcher::Z_NeighborLocked: return "Z_NeighborLocked";
    }
    return "unknown";
}

struct ExtendedMeshResult {
    size_t interior_quads = 0;  // quads not on the chunk boundary
    size_t boundary_quads = 0;  // quads on the chunk boundary (+X/-X/+Y/-Y/+Z/-Z faces)
    size_t total_quads() const noexcept { return interior_quads + boundary_quads; }
    size_t total_vertices() const noexcept { return total_quads() * 4; }
};

// Mesh with explicit boundary classification.
// `voxels` is S³. `neighbor_dir = -1/+1` for each face (-X, +X, -Y, +Y, -Z, +Z),
// `neighbor_lod` = LOD of the neighbor chunk, `neighbor_voxels` = neighbor's downsampled buffer
// (size = neighbor_lod_size³). For X_None and Y_TJunctionPad, `neighbor_voxels` is unused.
// neighbor_lod is in [0..kLODMax] but a self-neighbor (LOD same as this chunk) uses
// neighbor_lod = current LOD.
template <int S>
ExtendedMeshResult naive_mesh_ext(const uint8_t* voxels, int neighbor_lod,
                                  const uint8_t* neighbor_voxels, Stitcher stitch) {
    ExtendedMeshResult r;
    auto is_solid = [&](int x, int y, int z) -> bool {
        if (x < 0 || x >= S || y < 0 || y >= S || z < 0 || z >= S) return false;
        return voxels[idx3_at(x, y, z, S)] != static_cast<uint8_t>(Material::Air);
    };
    // Helper: check if neighbor at the matching sub-voxel position is solid.
    auto neighbor_solid = [&](int dx, int dy, int dz, int dir) -> bool {
        if (!neighbor_voxels) return false;
        int ns = lod_size(neighbor_lod);
        if (dir == 0) {  // -X face: neighbor at x=-1, map to neighbor's (ns-1, y, z)
            if (dx < 0 || dx >= S) return false;
            int ndx = (ns - 1) * S + dx;  // dx in [0, S) -> ndx in [0, ns*S)
            int nx = ndx / S;
            if (nx < 0 || nx >= ns) return false;
            int ny = dy;
            int nz = dz;
            if (ny < 0 || ny >= ns || nz < 0 || nz >= ns) return false;
            return neighbor_voxels[idx3_at(nx, ny, nz, ns)] != static_cast<uint8_t>(Material::Air);
        } else {  // +X face: neighbor at x=S, map to neighbor's (0, y, z) (across S voxels)
            if (dx < 0 || dx >= S) return false;
            int ndx = dx;  // dx in [0, S) -> ndx in [0, S), but neighbor has ns voxels across ns×S
            int nx = ndx / S;
            if (nx >= ns) return false;
            int ny = dy;
            int nz = dz;
            if (ny < 0 || ny >= ns || nz < 0 || nz >= ns) return false;
            return neighbor_voxels[idx3_at(nx, ny, nz, ns)] != static_cast<uint8_t>(Material::Air);
        }
    };

    for (int y = 0; y < S; ++y) {
        for (int z = 0; z < S; ++z) {
            for (int x = 0; x < S; ++x) {
                if (!is_solid(x, y, z)) continue;
                // +X face: on boundary if x == S-1
                bool bx_pos = (x == S - 1);
                if (!is_solid(x + 1, y, z)) {
                    if (bx_pos) {
                        // Apply stitch strategy
                        if (stitch == Stitcher::Z_NeighborLocked && neighbor_voxels) {
                            // Emit a quad only if neighbor is solid at the matching sub-voxel
                            if (neighbor_solid(x, y, z, 1)) {
                                ++r.boundary_quads;
                            } else {
                                ++r.interior_quads;
                            }
                        } else if (stitch == Stitcher::Y_TJunctionPad) {
                            // Pad strategy: still emit (Z-fight acceptable for prototype)
                            ++r.boundary_quads;
                        } else {
                            ++r.boundary_quads;
                        }
                    } else ++r.interior_quads;
                }
                // -X face: on boundary if x == 0
                bool bx_neg = (x == 0);
                if (!is_solid(x - 1, y, z)) {
                    if (bx_neg) {
                        if (stitch == Stitcher::Z_NeighborLocked && neighbor_voxels) {
                            if (neighbor_solid(x, y, z, 0)) {
                                ++r.boundary_quads;
                            } else {
                                ++r.interior_quads;
                            }
                        } else {
                            ++r.boundary_quads;
                        }
                    } else ++r.interior_quads;
                }
                // +Y face
                bool by_pos = (y == S - 1);
                if (!is_solid(x, y + 1, z)) {
                    if (by_pos) ++r.boundary_quads; else ++r.interior_quads;
                }
                // -Y face
                bool by_neg = (y == 0);
                if (!is_solid(x, y - 1, z)) {
                    if (by_neg) ++r.boundary_quads; else ++r.interior_quads;
                }
                // +Z face
                bool bz_pos = (z == S - 1);
                if (!is_solid(x, y, z + 1)) {
                    if (bz_pos) ++r.boundary_quads; else ++r.interior_quads;
                }
                // -Z face
                bool bz_neg = (z == 0);
                if (!is_solid(x, y, z - 1)) {
                    if (bz_neg) ++r.boundary_quads; else ++r.interior_quads;
                }
            }
        }
    }
    return r;
}

// ============================================================================
// Section 6: T-junction hole detector
// ============================================================================
//
// Compares faces on a chunk boundary between a LOD 0 chunk and a LOD 1 neighbor.
// A T-junction hole exists at a low-LOD boundary face when:
//   - low-LOD chunk does NOT emit a face at this voxel position (i.e. the air-solid transition
//     is "smoothed out" by the downsample kernel), BUT
//   - the high-LOD neighbor at the matching sub-voxel position has an air-solid transition.
//
// This means there's a sub-voxel position where light can leak through the gap.

struct TJunctionResult {
    size_t hole_count = 0;
    size_t boundary_face_count = 0;  // total high-LOD boundary faces (for ratio)
    double hole_ratio() const noexcept {
        return boundary_face_count ? double(hole_count) / double(boundary_face_count) : 0.0;
    }
};

TJunctionResult detect_tjunctions(
    const std::array<uint8_t, kChunkVoxels>& hi_chunk,  // LOD 0 (8³) — the higher-LOD side
    const uint8_t* lo_voxels,  // LOD 1/2/3 (size 4³/2³/1³) — the lower-LOD side, downsampled
    int lo_size,  // 4, 2, or 1
    int face_dir  // 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
) {
    TJunctionResult r;
    // For each face voxel position on the higher-LOD side at the boundary,
    // check if the matching sub-voxel in the lower-LOD chunk has the same material as
    // the high-LOD face plane.
    // Simpler proxy: count positions where hi is solid AND lo (at 2x lower res) is air
    // at the corresponding 2x2x2 sub-block, AND hi's adjacent voxel is air (creating a face).
    auto hi_solid = [&](int x, int y, int z) -> bool {
        if (x < 0 || x >= kChunkSize || y < 0 || y >= kChunkSize || z < 0 || z >= kChunkSize) return false;
        return hi_chunk[idx3(x, y, z)] != static_cast<uint8_t>(Material::Air);
    };
    // For +X face: lo's x is lo_size-1 (boundary)
    // For -X face: lo's x is 0
    // Step ratio: lo has `lo_size` voxels covering 8 hi voxels. 1 lo voxel = (8/lo_size) hi voxels per axis.
    const int hi_per_lo = kChunkSize / lo_size;
    for (int y = 0; y < kChunkSize; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                bool is_hi_boundary_face = false;
                bool hi_face_solid = false;
                if (face_dir == 0 && x == kChunkSize - 1) {  // +X
                    is_hi_boundary_face = true;
                    hi_face_solid = hi_solid(x, y, z) && !hi_solid(x + 1, y, z);
                } else if (face_dir == 1 && x == 0) {  // -X
                    is_hi_boundary_face = true;
                    hi_face_solid = hi_solid(x, y, z) && !hi_solid(x - 1, y, z);
                } else if (face_dir == 2 && y == kChunkSize - 1) {
                    is_hi_boundary_face = true;
                    hi_face_solid = hi_solid(x, y, z) && !hi_solid(x, y + 1, z);
                } else if (face_dir == 3 && y == 0) {
                    is_hi_boundary_face = true;
                    hi_face_solid = hi_solid(x, y, z) && !hi_solid(x, y - 1, z);
                } else if (face_dir == 4 && z == kChunkSize - 1) {
                    is_hi_boundary_face = true;
                    hi_face_solid = hi_solid(x, y, z) && !hi_solid(x, y, z + 1);
                } else if (face_dir == 5 && z == 0) {
                    is_hi_boundary_face = true;
                    hi_face_solid = hi_solid(x, y, z) && !hi_solid(x, y, z - 1);
                }
                if (!is_hi_boundary_face) continue;
                if (hi_face_solid) ++r.boundary_face_count;
                // Map (x, y, z) to lo's coordinate. For +X: lo_x = lo_size-1, with sub-offset based on y,z.
                int lo_x = (face_dir == 0) ? (lo_size - 1) :
                           (face_dir == 1) ? 0 : x / hi_per_lo;
                int lo_y = (face_dir == 2) ? (lo_size - 1) :
                           (face_dir == 3) ? 0 : y / hi_per_lo;
                int lo_z = (face_dir == 4) ? (lo_size - 1) :
                           (face_dir == 5) ? 0 : z / hi_per_lo;
                if (lo_x < 0) lo_x = 0; if (lo_x >= lo_size) lo_x = lo_size - 1;
                if (lo_y < 0) lo_y = 0; if (lo_y >= lo_size) lo_y = lo_size - 1;
                if (lo_z < 0) lo_z = 0; if (lo_z >= lo_size) lo_z = lo_size - 1;
                Material lo_m = static_cast<Material>(lo_voxels[idx3_at(lo_x, lo_y, lo_z, lo_size)]);
                if (hi_face_solid && lo_m == Material::Air) {
                    // hi emits a face here but lo has no solid voxel → T-junction hole
                    ++r.hole_count;
                }
            }
        }
    }
    return r;
}

}  // namespace pv

// ============================================================================
// Section 7: Stats + measurement harness (per `docs/experiments/benchmarks/methodology.md §3`)
// ============================================================================

struct Stats {
    double mean = 0;
    double median = 0;
    double p95 = 0;
    double p99 = 0;
    double stddev = 0;
    double min_v = 0;
    double max_v = 0;
};

Stats compute_stats(std::vector<double>& samples) {
    Stats s;
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
    double var = 0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.min_v = samples.front();
    s.max_v = samples.back();
    return s;
}

using clk = std::chrono::steady_clock;

int main(int argc, char** argv) {
    using namespace pv;

    int n_iter = 1000;
    int n_warmup = 10;
    int n_seeds = 5;
    std::string out_path = "build/results.csv";
    std::string mode = "all";  // all | single | tjunc
    Scene single_scene = Scene::MixedBiome;
    Kernel single_kernel = Kernel::B_SurfacePreserve;
    Stitcher single_stitch = Stitcher::Z_NeighborLocked;
    int single_lod = 1;
    bool quiet = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--all") mode = "all";
        else if (a == "--tjunc-only") mode = "tjunc";
        else if (a == "--scene" && i + 1 < argc) {
            std::string s = argv[++i];
            if (s == "uniform_air") single_scene = Scene::UniformAir;
            else if (s == "uniform_floor") single_scene = Scene::UniformFloor;
            else if (s == "forest_floor") single_scene = Scene::ForestFloor;
            else if (s == "cave_stress") single_scene = Scene::CaveStress;
            else if (s == "mixed_biome") single_scene = Scene::MixedBiome;
            else if (s == "all") mode = "all";
        } else if (a == "--kernel" && i + 1 < argc) {
            std::string k = argv[++i];
            if (k == "A") single_kernel = Kernel::A_Majority3D;
            else if (k == "B") single_kernel = Kernel::B_SurfacePreserve;
            else if (k == "C") single_kernel = Kernel::C_SolidOnly;
            else if (k == "D") single_kernel = Kernel::D_MaxPool;
        } else if (a == "--stitch" && i + 1 < argc) {
            std::string s = argv[++i];
            if (s == "X") single_stitch = Stitcher::X_None;
            else if (s == "Y") single_stitch = Stitcher::Y_TJunctionPad;
            else if (s == "Z") single_stitch = Stitcher::Z_NeighborLocked;
        } else if (a == "--lod" && i + 1 < argc) single_lod = std::atoi(argv[++i]);
        else if (a == "--iters" && i + 1 < argc) n_iter = std::atoi(argv[++i]);
        else if (a == "--warmup" && i + 1 < argc) n_warmup = std::atoi(argv[++i]);
        else if (a == "--seeds" && i + 1 < argc) n_seeds = std::atoi(argv[++i]);
        else if (a == "--output" && i + 1 < argc) out_path = argv[++i];
        else if (a == "--quiet") quiet = true;
    }

    const uint32_t kSeeds[5] = {1, 7, 42, 1234, 31337};

    std::ofstream csv(out_path);
    if (!csv) {
        std::fprintf(stderr, "Cannot open output: %s\n", out_path.c_str());
        return 1;
    }
    csv << "scene,kernel,stitch,lod,seed,downsample_us_mean,downsample_us_p95,downsample_us_stddev,"
        << "mesh_quad_count_total,mesh_quad_count_interior,mesh_quad_count_boundary,"
        << "mesh_vertex_count_total\n";

    auto bench_one = [&](Scene scene, Kernel kernel, Stitcher stitch, int lod, uint32_t seed,
                         size_t& quad_total, size_t& quad_int, size_t& quad_bnd,
                         Stats& downsample_stats, Stats& stitch_stats) {
        std::array<uint8_t, kChunkVoxels> chunk{};
        populate_chunk(chunk, scene, seed);
        int dst_size = lod_size(lod);
        std::vector<uint8_t> dst(lod_voxels(lod));

        // Build neighbor buffer for Z_NeighborLocked: also LOD 0 (highest detail neighbor)
        std::array<uint8_t, kChunkVoxels> neighbor{};
        populate_chunk(neighbor, scene, seed ^ 0xDEADBEEF);  // different seed = realistic neighbor

        // Warmup
        for (int w = 0; w < n_warmup; ++w) {
            downsample(kernel, chunk.data(), dst.data(), dst_size);
        }
        // Downsample timing
        std::vector<double> downsample_samples;
        downsample_samples.reserve(n_iter);
        for (int it = 0; it < n_iter; ++it) {
            auto t0 = clk::now();
            downsample(kernel, chunk.data(), dst.data(), dst_size);
            auto t1 = clk::now();
            downsample_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        downsample_stats = compute_stats(downsample_samples);

        // Mesh timing + count
        std::vector<double> mesh_samples;
        mesh_samples.reserve(n_iter);
        ExtendedMeshResult last_mesh{};
        for (int it = 0; it < n_iter; ++it) {
            auto t0 = clk::now();
            // Re-mesh based on stitch strategy
            if (dst_size == 8) {
                ExtendedMeshResult m{};
                if (stitch == Stitcher::Z_NeighborLocked) {
                    m = naive_mesh_ext<8>(dst.data(), 0, neighbor.data(), stitch);
                } else {
                    m = naive_mesh_ext<8>(dst.data(), lod, nullptr, stitch);
                }
                last_mesh = m;
            } else if (dst_size == 4) {
                if (stitch == Stitcher::Z_NeighborLocked) {
                    last_mesh = naive_mesh_ext<4>(dst.data(), 0, neighbor.data(), stitch);
                } else {
                    last_mesh = naive_mesh_ext<4>(dst.data(), lod, nullptr, stitch);
                }
            } else if (dst_size == 2) {
                if (stitch == Stitcher::Z_NeighborLocked) {
                    last_mesh = naive_mesh_ext<2>(dst.data(), 0, neighbor.data(), stitch);
                } else {
                    last_mesh = naive_mesh_ext<2>(dst.data(), lod, nullptr, stitch);
                }
            } else {  // 1
                if (stitch == Stitcher::Z_NeighborLocked) {
                    last_mesh = naive_mesh_ext<1>(dst.data(), 0, neighbor.data(), stitch);
                } else {
                    last_mesh = naive_mesh_ext<1>(dst.data(), lod, nullptr, stitch);
                }
            }
            auto t1 = clk::now();
            mesh_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        stitch_stats = compute_stats(mesh_samples);
        quad_total = last_mesh.total_quads();
        quad_int = last_mesh.interior_quads;
        quad_bnd = last_mesh.boundary_quads;
    };

    // T-junction detection
    auto tjunc_one = [&](Scene scene, Kernel kernel, int lod, uint32_t seed) -> TJunctionResult {
        std::array<uint8_t, kChunkVoxels> hi_chunk{};
        populate_chunk(hi_chunk, scene, seed);
        int lo_size = lod_size(lod);
        std::vector<uint8_t> lo(lo_size * lo_size * lo_size);
        downsample(kernel, hi_chunk.data(), lo.data(), lo_size);
        TJunctionResult total{};
        for (int dir = 0; dir < 6; ++dir) {
            auto r = detect_tjunctions(hi_chunk, lo.data(), lo_size, dir);
            total.hole_count += r.hole_count;
            total.boundary_face_count += r.boundary_face_count;
        }
        return total;
    };

    if (mode == "all") {
        // 4 kernels × 3 stitchers × 5 scenes × 4 LOD levels × 5 seeds = 1200 main measurements
        for (int ki = 0; ki < kKernelCount; ++ki) {
            Kernel k = static_cast<Kernel>(ki);
            for (int si = 0; si < kStitcherCount; ++si) {
                Stitcher st = static_cast<Stitcher>(si);
                for (int sc = 0; sc < kSceneCount; ++sc) {
                    Scene s = static_cast<Scene>(sc);
                    for (int lod = 0; lod <= kLODMax; ++lod) {
                        for (int sd = 0; sd < n_seeds && sd < 5; ++sd) {
                            size_t qt, qi, qb;
                            Stats ds, ms;
                            bench_one(s, k, st, lod, kSeeds[sd], qt, qi, qb, ds, ms);
                            csv << scene_name(s) << "," << kernel_name(k) << ","
                                << stitcher_name(st) << "," << lod << "," << kSeeds[sd] << ","
                                << ds.mean << "," << ds.p95 << "," << ds.stddev << ","
                                << qt << "," << qi << "," << qb << "," << qt * 4 << "\n";
                            if (!quiet) {
                                std::printf("%-13s %-18s %-18s LOD=%d seed=%u  ds=%.3fus  quads=%zu (int=%zu bnd=%zu)\n",
                                            std::string(scene_name(s)).c_str(),
                                            std::string(kernel_name(k)).c_str(),
                                            std::string(stitcher_name(st)).c_str(),
                                            lod, kSeeds[sd], ds.mean, qt, qi, qb);
                            }
                        }
                    }
                }
            }
        }
        // T-junction detection sweep
        std::string tjunc_path = out_path;
        auto dot = tjunc_path.rfind('.');
        if (dot != std::string::npos) tjunc_path = tjunc_path.substr(0, dot) + "_tjunc.csv";
        else tjunc_path += "_tjunc.csv";
        std::ofstream tjunc_csv(tjunc_path);
        tjunc_csv << "scene,kernel,lod,seed,hole_count,boundary_face_count,hole_ratio\n";
        for (int ki = 0; ki < kKernelCount; ++ki) {
            Kernel k = static_cast<Kernel>(ki);
            for (int sc = 0; sc < kSceneCount; ++sc) {
                Scene s = static_cast<Scene>(sc);
                for (int lod = 1; lod <= kLODMax; ++lod) {  // LOD 0 = no neighbor mismatch
                    for (int sd = 0; sd < n_seeds && sd < 5; ++sd) {
                        auto r = tjunc_one(s, k, lod, kSeeds[sd]);
                        tjunc_csv << scene_name(s) << "," << kernel_name(k) << "," << lod << ","
                                  << kSeeds[sd] << "," << r.hole_count << ","
                                  << r.boundary_face_count << "," << r.hole_ratio() << "\n";
                    }
                }
            }
        }
        tjunc_csv.close();
        std::printf("\nWrote: %s + %s\n", out_path.c_str(), tjunc_path.c_str());
    } else if (mode == "tjunc") {
        // T-junction only
        std::string tjunc_path = out_path;
        auto dot = tjunc_path.rfind('.');
        if (dot != std::string::npos) tjunc_path = tjunc_path.substr(0, dot) + "_tjunc.csv";
        else tjunc_path += "_tjunc.csv";
        std::ofstream tjunc_csv(tjunc_path);
        tjunc_csv << "scene,kernel,lod,seed,hole_count,boundary_face_count,hole_ratio\n";
        for (int ki = 0; ki < kKernelCount; ++ki) {
            Kernel k = static_cast<Kernel>(ki);
            for (int sc = 0; sc < kSceneCount; ++sc) {
                Scene s = static_cast<Scene>(sc);
                for (int lod = 1; lod <= kLODMax; ++lod) {
                    for (int sd = 0; sd < n_seeds && sd < 5; ++sd) {
                        auto r = tjunc_one(s, k, lod, kSeeds[sd]);
                        tjunc_csv << scene_name(s) << "," << kernel_name(k) << "," << lod << ","
                                  << kSeeds[sd] << "," << r.hole_count << ","
                                  << r.boundary_face_count << "," << r.hole_ratio() << "\n";
                    }
                }
            }
        }
        tjunc_csv.close();
    } else {  // single
        size_t qt, qi, qb;
        Stats ds, ms;
        bench_one(single_scene, single_kernel, single_stitch, single_lod, kSeeds[0], qt, qi, qb, ds, ms);
        csv << scene_name(single_scene) << "," << kernel_name(single_kernel) << ","
            << stitcher_name(single_stitch) << "," << single_lod << "," << kSeeds[0] << ","
            << ds.mean << "," << ds.p95 << "," << ds.stddev << ","
            << qt << "," << qi << "," << qb << "," << qt * 4 << "\n";
        std::printf("%-13s %-18s %-18s LOD=%d seed=%u  ds=%.3fus p95=%.3fus std=%.3fus  quads=%zu\n",
                    std::string(scene_name(single_scene)).c_str(),
                    std::string(kernel_name(single_kernel)).c_str(),
                    std::string(stitcher_name(single_stitch)).c_str(),
                    single_lod, kSeeds[0], ds.mean, ds.p95, ds.stddev, qt);
    }
    csv.close();
    return 0;
}
