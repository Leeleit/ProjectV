// SPDX-License-Identifier: MIT
// Standalone C++26 CPU benchmark for LOD transition strategies (ProjectV Stage 4.2)
// No external dependencies beyond stdlib.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using f32 = float;
using u32 = uint32_t;
using i32 = int32_t;
using u8  = uint8_t;

struct Vec3 { f32 x, y, z; };

struct Vertex {
    Vec3 pos;
    Vec3 normal;
};

struct Quad {
    std::array<u32, 4> v;
    Vec3 face_normal;
    f32 ao = 1.0f;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Quad> quads;

    void clear() { vertices.clear(); quads.clear(); }
    size_t quad_count() const { return quads.size(); }
    size_t vertex_count() const { return vertices.size(); }
};

struct VoxelChunk {
    static constexpr i32 CHUNK_SIZE = 8;
    static constexpr i32 VOXEL_COUNT = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    std::array<u8, VOXEL_COUNT> voxels{};

    u8 at(i32 x, i32 y, i32 z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return 0;
        return voxels[x + CHUNK_SIZE * (y + CHUNK_SIZE * z)];
    }

    void set(i32 x, i32 y, i32 z, u8 v) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return;
        voxels[x + CHUNK_SIZE * (y + CHUNK_SIZE * z)] = v;
    }
};

enum class SceneType : int {
    UNIFORM_FLOOR = 0,
    FOREST_FLOOR,
    CAVE_STRESS,
    MIXED_BIOME,
    BIOME_BOUNDARY,
    COUNT
};

const char* scene_name(SceneType s) {
    switch (s) {
        case SceneType::UNIFORM_FLOOR:   return "uniform_floor";
        case SceneType::FOREST_FLOOR:   return "forest_floor";
        case SceneType::CAVE_STRESS:     return "cave_stress";
        case SceneType::MIXED_BIOME:     return "mixed_biome";
        case SceneType::BIOME_BOUNDARY:  return "biome_boundary";
        default:                         return "unknown";
    }
}

VoxelChunk generate_scene(SceneType type, u32 seed) {
    VoxelChunk chunk{};
    std::mt19937 rng(seed);

    switch (type) {
        case SceneType::UNIFORM_FLOOR:
            for (i32 x = 0; x < VoxelChunk::CHUNK_SIZE; ++x)
                for (i32 z = 0; z < VoxelChunk::CHUNK_SIZE; ++z) {
                    chunk.set(x, 0, z, 1);
                    chunk.set(x, 1, z, 1);
                }
            break;

        case SceneType::FOREST_FLOOR:
            for (i32 x = 0; x < VoxelChunk::CHUNK_SIZE; ++x)
                for (i32 z = 0; z < VoxelChunk::CHUNK_SIZE; ++z) {
                    chunk.set(x, 0, z, 1);
                    if ((x + z) % 3 == 0) chunk.set(x, 1, z, 1);
                }
            for (i32 x = 2; x < 6; ++x)
                for (i32 z = 2; z < 6; ++z)
                    if (((x * z + seed) & 3u) == 0) chunk.set(x, 2, z, 1);
            break;

        case SceneType::CAVE_STRESS:
            for (i32 x = 0; x < VoxelChunk::CHUNK_SIZE; ++x)
                for (i32 y = 0; y < VoxelChunk::CHUNK_SIZE; ++y)
                    for (i32 z = 0; z < VoxelChunk::CHUNK_SIZE; ++z)
                        if (x == 0 || x == VoxelChunk::CHUNK_SIZE - 1 ||
                            y == 0 || y == VoxelChunk::CHUNK_SIZE - 1 ||
                            z == 0 || z == VoxelChunk::CHUNK_SIZE - 1)
                            chunk.set(x, y, z, 1);
            {
                std::uniform_int_distribution<i32> dist(1, 6);
                for (i32 i = 0; i < 4; ++i) {
                    chunk.set(dist(rng), dist(rng), dist(rng), 0);
                    chunk.set(dist(rng), dist(rng), 0, 0);
                }
            }
            break;

        case SceneType::MIXED_BIOME:
            for (i32 x = 0; x < VoxelChunk::CHUNK_SIZE; ++x)
                for (i32 y = 0; y < VoxelChunk::CHUNK_SIZE; ++y)
                    for (i32 z = 0; z < VoxelChunk::CHUNK_SIZE; ++z) {
                        i32 mat = ((x / 2) + (y / 2) * 2 + (z / 2) * 4) % 2;
                        chunk.set(x, y, z, (u8)mat);
                    }
            break;

        case SceneType::BIOME_BOUNDARY:
            for (i32 x = 0; x < VoxelChunk::CHUNK_SIZE; ++x)
                for (i32 y = 0; y < VoxelChunk::CHUNK_SIZE; ++y)
                    for (i32 z = 0; z < VoxelChunk::CHUNK_SIZE; ++z)
                        if ((x % 2 == 0) && (z % 2 == 0))
                            chunk.set(x, y, z, 1);
            break;

        default:
            break;
    }
    return chunk;
}

// Culled surface mesh: one face per solid voxel face, skip if neighbor solid
void generate_mesh_culled(const VoxelChunk& chunk, Mesh& out) {
    out.clear();
    for (i32 x = 0; x < VoxelChunk::CHUNK_SIZE; ++x) {
        for (i32 y = 0; y < VoxelChunk::CHUNK_SIZE; ++y) {
            for (i32 z = 0; z < VoxelChunk::CHUNK_SIZE; ++z) {
                if (chunk.at(x, y, z) == 0) continue;

                if (chunk.at(x + 1, y, z) == 0) {
                    Vec3 b{(f32)(x + 1), (f32)y, (f32)z};
                    u32 v0 = (u32)out.vertices.size();
                    out.vertices.push_back({b, {1, 0, 0}});
                    out.vertices.push_back({{b.x, b.y, b.z + 1}, {1, 0, 0}});
                    out.vertices.push_back({{b.x, b.y + 1, b.z + 1}, {1, 0, 0}});
                    out.vertices.push_back({{b.x, b.y + 1, b.z}, {1, 0, 0}});
                    out.quads.push_back({{v0, v0 + 1, v0 + 2, v0 + 3}, {1, 0, 0}});
                }
                if (chunk.at(x - 1, y, z) == 0) {
                    Vec3 b{(f32)x, (f32)y, (f32)z};
                    u32 v0 = (u32)out.vertices.size();
                    out.vertices.push_back({b, {-1, 0, 0}});
                    out.vertices.push_back({{b.x, b.y + 1, b.z}, {-1, 0, 0}});
                    out.vertices.push_back({{b.x, b.y + 1, b.z + 1}, {-1, 0, 0}});
                    out.vertices.push_back({{b.x, b.y, b.z + 1}, {-1, 0, 0}});
                    out.quads.push_back({{v0, v0 + 1, v0 + 2, v0 + 3}, {-1, 0, 0}});
                }
                if (chunk.at(x, y + 1, z) == 0) {
                    Vec3 b{(f32)x, (f32)(y + 1), (f32)z};
                    u32 v0 = (u32)out.vertices.size();
                    out.vertices.push_back({b, {0, 1, 0}});
                    out.vertices.push_back({{b.x, b.y, b.z + 1}, {0, 1, 0}});
                    out.vertices.push_back({{b.x + 1, b.y, b.z + 1}, {0, 1, 0}});
                    out.vertices.push_back({{b.x + 1, b.y, b.z}, {0, 1, 0}});
                    out.quads.push_back({{v0, v0 + 1, v0 + 2, v0 + 3}, {0, 1, 0}});
                }
                if (chunk.at(x, y - 1, z) == 0) {
                    Vec3 b{(f32)x, (f32)y, (f32)z};
                    u32 v0 = (u32)out.vertices.size();
                    out.vertices.push_back({b, {0, -1, 0}});
                    out.vertices.push_back({{b.x + 1, b.y, b.z}, {0, -1, 0}});
                    out.vertices.push_back({{b.x + 1, b.y, b.z + 1}, {0, -1, 0}});
                    out.vertices.push_back({{b.x, b.y, b.z + 1}, {0, -1, 0}});
                    out.quads.push_back({{v0, v0 + 1, v0 + 2, v0 + 3}, {0, -1, 0}});
                }
                if (chunk.at(x, y, z + 1) == 0) {
                    Vec3 b{(f32)x, (f32)y, (f32)(z + 1)};
                    u32 v0 = (u32)out.vertices.size();
                    out.vertices.push_back({b, {0, 0, 1}});
                    out.vertices.push_back({{b.x + 1, b.y, b.z}, {0, 0, 1}});
                    out.vertices.push_back({{b.x + 1, b.y + 1, b.z}, {0, 0, 1}});
                    out.vertices.push_back({{b.x, b.y + 1, b.z}, {0, 0, 1}});
                    out.quads.push_back({{v0, v0 + 1, v0 + 2, v0 + 3}, {0, 0, 1}});
                }
                if (chunk.at(x, y, z - 1) == 0) {
                    Vec3 b{(f32)x, (f32)y, (f32)z};
                    u32 v0 = (u32)out.vertices.size();
                    out.vertices.push_back({b, {0, 0, -1}});
                    out.vertices.push_back({{b.x, b.y + 1, b.z}, {0, 0, -1}});
                    out.vertices.push_back({{b.x + 1, b.y + 1, b.z}, {0, 0, -1}});
                    out.vertices.push_back({{b.x + 1, b.y, b.z}, {0, 0, -1}});
                    out.quads.push_back({{v0, v0 + 1, v0 + 2, v0 + 3}, {0, 0, -1}});
                }
            }
        }
    }
}

void generate_mesh_lod(const VoxelChunk& chunk, Mesh& out, i32 lod_level) {
    const i32 step = 1 << lod_level;
    const i32 n = VoxelChunk::CHUNK_SIZE / step;

    VoxelChunk reduced{};
    for (i32 x = 0; x < n; ++x)
        for (i32 y = 0; y < n; ++y)
            for (i32 z = 0; z < n; ++z) {
                bool surface = false;
                for (i32 dx = 0; dx < step && !surface; ++dx)
                    for (i32 dy = 0; dy < step && !surface; ++dy)
                        for (i32 dz = 0; dz < step && !surface; ++dz) {
                            i32 sx = x * step + dx;
                            i32 sy = y * step + dy;
                            i32 sz = z * step + dz;
                            if (!chunk.at(sx, sy, sz)) continue;
                            if (!chunk.at(sx - 1, sy, sz) || !chunk.at(sx + 1, sy, sz) ||
                                !chunk.at(sx, sy - 1, sz) || !chunk.at(sx, sy + 1, sz) ||
                                !chunk.at(sx, sy, sz - 1) || !chunk.at(sx, sy, sz + 1))
                                surface = true;
                        }
                if (surface) reduced.set(x, y, z, 1);
            }

    VoxelChunk scaled{};
    for (i32 x = 0; x < n; ++x)
        for (i32 y = 0; y < n; ++y)
            for (i32 z = 0; z < n; ++z)
                if (reduced.at(x, y, z))
                    for (i32 dx = 0; dx < step; ++dx)
                        for (i32 dy = 0; dy < step; ++dy)
                            for (i32 dz = 0; dz < step; ++dz)
                                scaled.set(x * step + dx, y * step + dy, z * step + dz, 1);
    generate_mesh_culled(scaled, out);
}

enum class Strategy : int {
    A_POP = 0,
    B_CROSSFADE,
    C_GEOMORPH,
    D_PRECOMPUTED_MORPH_TARGETS,
    E_HZB_STITCH,
    COUNT
};

const char* strategy_name(Strategy s) {
    switch (s) {
        case Strategy::A_POP:                       return "A_Pop";
        case Strategy::B_CROSSFADE:                 return "B_Crossfade";
        case Strategy::C_GEOMORPH:                  return "C_Geomorph";
        case Strategy::D_PRECOMPUTED_MORPH_TARGETS: return "D_PreComputedMorphTargets";
        case Strategy::E_HZB_STITCH:                return "E_HZB_Stitch";
        default:                                    return "unknown";
    }
}

struct MorphTarget {
    std::array<Vec3, 4> delta{};
};

struct TransitionBundle {
    Mesh lod0{};
    Mesh lod1{};
    std::vector<MorphTarget> morph_targets{};
};

void build_transition_bundle(const VoxelChunk& chunk, TransitionBundle& bundle, Strategy strategy) {
    generate_mesh_lod(chunk, bundle.lod0, 0);
    generate_mesh_lod(chunk, bundle.lod1, 1);
    bundle.morph_targets.clear();

    if (strategy == Strategy::D_PRECOMPUTED_MORPH_TARGETS) {
        bundle.morph_targets.reserve(bundle.lod0.vertices.size());
        for (u32 i = 0; i < bundle.lod0.vertices.size(); ++i) {
            MorphTarget mt;
            const Vec3& ref = (i < bundle.lod1.vertices.size())
                ? bundle.lod1.vertices[i].pos
                : bundle.lod1.vertices[bundle.lod1.vertices.size() / 2].pos;
            Vec3 d{ref.x - bundle.lod0.vertices[i].pos.x,
                   ref.y - bundle.lod0.vertices[i].pos.y,
                   ref.z - bundle.lod0.vertices[i].pos.z};
            mt.delta = {d, d, d, d};
            bundle.morph_targets.push_back(mt);
        }
    }
}

struct BuildStats {
    double build_us = 0.0;
    size_t memory_bytes = 0;
    size_t triangle_count = 0;
};

BuildStats measure_build(const VoxelChunk& chunk, Strategy strategy) {
    BuildStats stats{};
    auto start = std::chrono::steady_clock::now();

    if (strategy == Strategy::A_POP) {
        Mesh lod0;
        generate_mesh_lod(chunk, lod0, 0);
        stats.triangle_count = lod0.quad_count() * 2;
        stats.memory_bytes = lod0.vertices.size() * sizeof(Vertex) + lod0.quads.size() * sizeof(Quad);
    } else if (strategy == Strategy::B_CROSSFADE) {
        Mesh lod0, lod1;
        generate_mesh_lod(chunk, lod0, 0);
        generate_mesh_lod(chunk, lod1, 1);
        stats.triangle_count = (lod0.quad_count() + lod1.quad_count()) * 2;
        stats.memory_bytes = (lod0.vertices.size() + lod1.vertices.size()) * sizeof(Vertex) +
                             (lod0.quads.size() + lod1.quads.size()) * sizeof(Quad);
    } else if (strategy == Strategy::C_GEOMORPH) {
        TransitionBundle bundle;
        build_transition_bundle(chunk, bundle, strategy);
        stats.triangle_count = bundle.lod0.quad_count() * 2;
        stats.memory_bytes = bundle.lod0.vertices.size() * (sizeof(Vertex) + sizeof(Vec3)) +
                             bundle.lod0.quads.size() * sizeof(Quad) +
                             bundle.lod1.vertices.size() * sizeof(Vertex);
    } else if (strategy == Strategy::D_PRECOMPUTED_MORPH_TARGETS) {
        TransitionBundle bundle;
        build_transition_bundle(chunk, bundle, strategy);
        stats.triangle_count = bundle.lod0.quad_count() * 2;
        stats.memory_bytes = bundle.lod0.vertices.size() * (sizeof(Vertex) + sizeof(MorphTarget)) +
                             bundle.lod0.quads.size() * sizeof(Quad) +
                             bundle.lod1.vertices.size() * sizeof(Vertex);
    } else if (strategy == Strategy::E_HZB_STITCH) {
        Mesh lod0, lod1;
        generate_mesh_lod(chunk, lod0, 0);
        generate_mesh_lod(chunk, lod1, 1);
        stats.triangle_count = lod0.quad_count() * 2;
        stats.memory_bytes = (lod0.vertices.size() + lod1.vertices.size()) * sizeof(Vertex) +
                             (lod0.quads.size() + lod1.quads.size()) * (sizeof(Quad) + 1);
    }

    auto end = std::chrono::steady_clock::now();
    stats.build_us = std::chrono::duration<double, std::micro>(end - start).count();
    return stats;
}

Vec3 continuous_lod_position(const Vec3& base, f32 t) {
    f32 t_floor = std::floor(t);
    f32 t_ceil  = std::ceil(t);
    f32 frac    = t - t_floor;
    i32 lod_lo  = (i32)t_floor;
    i32 lod_hi  = (i32)t_ceil;
    f32 s_lo    = (f32)(1 << lod_lo);
    f32 s_hi    = (f32)(1 << lod_hi);
    f32 x_lo = std::floor(base.x / s_lo) * s_lo;
    f32 x_hi = std::floor(base.x / s_hi) * s_hi;
    f32 y_lo = std::floor(base.y / s_lo) * s_lo;
    f32 y_hi = std::floor(base.y / s_hi) * s_hi;
    f32 z_lo = std::floor(base.z / s_lo) * s_lo;
    f32 z_hi = std::floor(base.z / s_hi) * s_hi;
    return {
        (1.0f - frac) * x_lo + frac * x_hi,
        (1.0f - frac) * y_lo + frac * y_hi,
        (1.0f - frac) * z_lo + frac * z_hi,
    };
}

f32 sqdist(const Vec3& a, const Vec3& b) {
    f32 dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

struct TransitionResult {
    f32 psnr_db = 0.0f;
    f32 max_vertex_discontinuity = 0.0f;
};

TransitionResult evaluate_strategy(const VoxelChunk& chunk, Strategy strategy, f32 t) {
    TransitionResult result{};
    Mesh lod0, lod1;
    generate_mesh_lod(chunk, lod0, 0);
    generate_mesh_lod(chunk, lod1, 1);

    auto compute_psnr = [&](const Mesh& rendered, bool is_lod1_selected) -> TransitionResult {
        TransitionResult r{};
        f32 total_err = 0.0f, max_err = 0.0f;
        size_t n = rendered.vertices.size();
        for (size_t i = 0; i < n; ++i) {
            Vec3 ref = continuous_lod_position(rendered.vertices[i].pos, t);
            f32 err = sqdist(rendered.vertices[i].pos, ref);
            total_err += err;
            max_err = std::max(max_err, err);
        }
        if (n > 0) {
            f32 mse = total_err / (f32)n;
            if (mse > 1e-10f) {
                f32 max_val = std::sqrt(3.0f);
                r.psnr_db = 10.0f * std::log10((max_val * max_val) / mse);
            } else {
                r.psnr_db = 100.0f;
            }
            r.max_vertex_discontinuity = std::sqrt(max_err);
        }
        (void)is_lod1_selected;
        return r;
    };

    if (strategy == Strategy::A_POP) {
        bool use_lod1 = t >= 0.5f;
        result = compute_psnr(use_lod1 ? lod1 : lod0, use_lod1);
    } else if (strategy == Strategy::E_HZB_STITCH) {
        bool use_lod1 = t >= 0.5f;
        result = compute_psnr(use_lod1 ? lod1 : lod0, use_lod1);
    } else {
        // B_CROSSFADE, C_GEOMORPH, D_PRECOMPUTED_MORPH_TARGETS
        // all blend LOD 0 + LOD 1 vertices linearly by t
        f32 total_err = 0.0f, max_err = 0.0f;
        size_t n = std::min(lod0.vertices.size(), lod1.vertices.size());
        for (size_t i = 0; i < n; ++i) {
            Vec3 morphed{
                (1.0f - t) * lod0.vertices[i].pos.x + t * lod1.vertices[i].pos.x,
                (1.0f - t) * lod0.vertices[i].pos.y + t * lod1.vertices[i].pos.y,
                (1.0f - t) * lod0.vertices[i].pos.z + t * lod1.vertices[i].pos.z,
            };
            Vec3 ref = continuous_lod_position(lod0.vertices[i].pos, t);
            f32 err = sqdist(morphed, ref);
            total_err += err;
            max_err = std::max(max_err, err);
        }
        if (n > 0) {
            f32 mse = total_err / (f32)n;
            if (mse > 1e-10f) {
                f32 max_val = std::sqrt(3.0f);
                result.psnr_db = 10.0f * std::log10((max_val * max_val) / mse);
            } else {
                result.psnr_db = 100.0f;
            }
            result.max_vertex_discontinuity = std::sqrt(max_err);
        }
    }
    return result;
}

struct Stats {
    f32 mean{};
    f32 median{};
    f32 p95{};
    f32 p99{};
    f32 stddev{};
    f32 minv{};
    f32 maxv{};
};

Stats compute_stats(std::vector<f32> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    f32 sum = std::accumulate(samples.begin(), samples.end(), 0.0f);
    s.mean = sum / (f32)samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[(size_t)(samples.size() * 0.95f)];
    s.p99 = samples[(size_t)(samples.size() * 0.99f)];
    f32 var = 0.0f;
    for (f32 v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / (f32)samples.size());
    s.minv = samples.front();
    s.maxv = samples.back();
    return s;
}

int main() {
    constexpr u32 SEEDS[] = {1, 7, 42, 1234, 31337};
    constexpr u32 N_ITER = 1000;
    constexpr u32 N_WARMUP = 10;
    constexpr f32 T_VALUES[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    std::ofstream out("results.csv");
    out << "strategy,scene,seed,t_value,build_us_mean,memory_bytes,triangle_count,psnr_db_mean,vertex_disc_max\n";

    printf("LOD Transition Strategy Benchmark\n");
    printf("=================================\n");
    printf("Strategies: %d | Scenes: %d | Seeds: %d | Iter/meas: %d | Warmup: %d\n",
           (int)Strategy::COUNT, (int)SceneType::COUNT,
           (int)(sizeof(SEEDS) / sizeof(SEEDS[0])), N_ITER, N_WARMUP);

    for (int s_int = 0; s_int < (int)Strategy::COUNT; ++s_int) {
        Strategy strategy = (Strategy)s_int;
        printf("\n[%s]\n", strategy_name(strategy));
        for (int scene_int = 0; scene_int < (int)SceneType::COUNT; ++scene_int) {
            SceneType scene = (SceneType)scene_int;
            for (u32 seed : SEEDS) {
                VoxelChunk chunk = generate_scene(scene, seed);

                for (u32 w = 0; w < N_WARMUP; ++w) {
                    BuildStats bs = measure_build(chunk, strategy);
                    (void)bs;
                }

                std::vector<f32> build_samples;
                build_samples.reserve(N_ITER);
                size_t mem = 0, tris = 0;
                for (u32 i = 0; i < N_ITER; ++i) {
                    BuildStats bs = measure_build(chunk, strategy);
                    build_samples.push_back((f32)bs.build_us);
                    mem = bs.memory_bytes;
                    tris = bs.triangle_count;
                }
                Stats bs_stats = compute_stats(build_samples);

                std::vector<f32> psnr_samples, disc_samples;
                for (f32 t : T_VALUES) {
                    TransitionResult tr = evaluate_strategy(chunk, strategy, t);
                    psnr_samples.push_back(tr.psnr_db);
                    disc_samples.push_back(tr.max_vertex_discontinuity);
                }
                Stats psnr_stats = compute_stats(psnr_samples);
                Stats disc_stats = compute_stats(disc_samples);

                f32 mean_psnr = psnr_stats.mean;
                f32 mean_disc = disc_stats.mean;

                out << strategy_name(strategy) << ","
                    << scene_name(scene) << ","
                    << seed << ","
                    << "avg,"
                    << bs_stats.mean << ","
                    << mem << ","
                    << tris << ","
                    << mean_psnr << ","
                    << mean_disc << "\n";

                printf("  %-16s seed=%u: build=%.3fus mem=%zuby tris=%zu psnr=%.2fdB disc=%.3f\n",
                       scene_name(scene), seed,
                       bs_stats.mean, mem, tris, mean_psnr, mean_disc);
            }
        }
    }

    out.close();
    printf("\nResults written to results.csv\n");
    return 0;
}
