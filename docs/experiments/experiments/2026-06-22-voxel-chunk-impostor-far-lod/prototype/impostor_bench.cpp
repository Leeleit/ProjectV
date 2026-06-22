// Voxel Chunk Impostor Rendering — Analytical Benchmark
// 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup
// Build: clang++-22 -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
//        -o build/impostor_bench impostor_bench.cpp

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <vector>

// ---- Constants (ProjectV mainline-compatible) ----
static constexpr int kChunkSize = 8;          // 8³ voxels per chunk
static constexpr int kChunkVoxels = kChunkSize * kChunkSize * kChunkSize;
static constexpr int kImpostorTexSize = 64;    // 64×64 texels per face
static constexpr int kMaxMaterials = 16;       // max distinct materials in 8³
static constexpr int kOctreeMaxDepth = 3;      // octree subdivision depth
static constexpr int kDrawCallOverheadCycles = 200;  // ~0.1 µs at 2 GHz
static constexpr int kQuadVertCostCycles = 4;       // 4 vertices per quad
static constexpr int kFragCyclesPerPixel = 8;       // per-fragment shader cycles
static constexpr int kTexSampleCycles = 6;           // texture sample cycles

// RTX 3060 Ti metrics (Ampere GA104)
static constexpr double kGhz = 1.67;           // GA104 boost ~1.67 GHz
static constexpr double kCycleToUs = 1.0 / (kGhz * 1000.0);
static constexpr double kDrawSetupUs = 0.5;    // indirect draw + state change
static constexpr double kQuadTriCount = 2.0;    // 2 triangles per quad
static constexpr double kBytesPerTexel = 4.0;   // RGBA8 UNORM

// Scene descriptions
enum SceneId : int {
    S1_UniformStone = 0,
    S2_UniformAir,
    S3_MixedBiome,
    S4_ComplexOrganic,
    S5_StructuredBuilding,
    kSceneCount
};

static constexpr const char* kSceneNames[] = {
    "s1_uniform_stone",
    "s2_uniform_air",
    "s3_mixed_biome",
    "s4_complex_organic",
    "s5_structured_building"
};

// Per-scene generation params
struct SceneParams {
    int material_count;       // distinct materials in chunk
    double fill_ratio;        // fraction of non-air voxels
    double surface_complexity; // 0=flat faces, 1=max irregular
    char const* name;
};

static constexpr SceneParams kSceneParams[kSceneCount] = {
    {1,  1.00, 0.05, "Uniform stone — single material, fully solid"},
    {0,  0.00, 0.00, "Uniform air — empty chunk"},
    {5,  0.50, 0.40, "Mixed biome — 5 materials, 50% fill, moderate surface"},
    {5,  0.30, 0.85, "Complex organic — cave-like, intricate surfaces"},
    {8,  0.60, 0.30, "Structured building — rooms/corridors, multi-level"}
};

// Strategy IDs
enum StrategyId : int {
    A_NoImpostor = 0,
    B_SingleQuad,
    C_Static6Face,
    D_OctreeImpostor,
    E_GPUComputeDynamic,
    kStrategyCount
};

static constexpr const char* kStrategyNames[] = {
    "A_NoImpostor",
    "B_SingleQuad_WorldDir",
    "C_Static6Face_CubeMap",
    "D_OctreeImpostor",
    "E_GPUCompute_DynamicRebake"
};

// Per-strategy analytical params
struct StrategyParams {
    double vertex_cost_us;       // per-vertex GPU cost (µs)
    double frag_cost_us_per_px;  // per-fragment cost per covered pixel
    double cycle_base_quad;      // base quad rasterization cycles
    int    face_count;            // number of impostor faces
    double texel_count;           // total texels per chunk
    double quality_base;          // base quality score (0-1)
    double view_angle_decay;      // quality loss at 45° off-axis
    double mutation_cost_us;      // cost to re-bake on mutation (µs)
    double metadata_bytes;        // per-chunk metadata overhead
};

static constexpr StrategyParams kStrategyParams[kStrategyCount] = {
    // A_NoImpostor — no GPU cost, no VRAM, no update cost
    {0.0, 0.0, 0.0, 0, 0.0, 0.10, 0.0, 0.0, 0.0},
    // B_SingleQuad — 1 quad, 4 verts, dominant color
    {kDrawCallOverheadCycles * kCycleToUs + 4.0 * kQuadVertCostCycles * kCycleToUs,
     8.0 * kFragCyclesPerPixel * kCycleToUs,
     kQuadTriCount, 1, double(kImpostorTexSize * kImpostorTexSize),
     0.35, 0.30, 1.0, 64.0},
    // C_Static6Face — 6 cube faces, 2 nearest rendered (blended)
    {kDrawCallOverheadCycles * kCycleToUs + 8.0 * kQuadVertCostCycles * kCycleToUs,
     12.0 * kFragCyclesPerPixel * kCycleToUs,
     kQuadTriCount * 2.0, 6, double(kImpostorTexSize * kImpostorTexSize * 6),
     0.70, 0.05, 8.0, 256.0},
    // D_OctreeImpostor — adaptive: uniform nodes = 1 quad, complex = 6-face
    {kDrawCallOverheadCycles * kCycleToUs + 6.0 * kQuadVertCostCycles * kCycleToUs,
     10.0 * kFragCyclesPerPixel * kCycleToUs,
     kQuadTriCount * 1.5, 6, double(kImpostorTexSize * kImpostorTexSize * 3),
     0.85, 0.03, 12.0, 512.0},
    // E_GPUComputeDynamic — D + compute shader re-bake
    {kDrawCallOverheadCycles * kCycleToUs + 6.0 * kQuadVertCostCycles * kCycleToUs,
     10.0 * kFragCyclesPerPixel * kCycleToUs,
     kQuadTriCount * 1.5, 6, double(kImpostorTexSize * kImpostorTexSize * 3),
     0.90, 0.02, double(kImpostorTexSize * kImpostorTexSize * 6) * 0.002, 512.0},
};

// Per-chunk classification result
struct ChunkStats {
    int    material_count;
    double fill_ratio;
    double surface_complexity;  // fraction of faces that are non-planar
    int    octree_uniform_nodes; // for D/E: how many octree nodes are uniform
    int    octree_total_nodes;
};

// Generate deterministic chunk from seed + scene
static ChunkStats classify_chunk(SceneId scene, int seed) {
    std::mt19937 rng(unsigned(seed * 31337 + int(scene) * 7919));
    auto const& params = kSceneParams[scene];

    ChunkStats s;
    s.material_count = params.material_count;
    s.fill_ratio = params.fill_ratio;
    s.surface_complexity = params.surface_complexity;

    // Simulate octree subdivision for D/E
    // Count how many octree nodes (at max_depth) would be uniform
    int const leaves = 1 << (kOctreeMaxDepth * 3); // 8³ at depth 3
    s.octree_total_nodes = leaves;
    int uniform_count = 0;
    for (int i = 0; i < leaves; ++i) {
        // Uniform leaf depends on material_count and fill_ratio
        double uniform_prob = 1.0;
        if (params.fill_ratio > 0.99) {
            uniform_prob = 0.95; // almost full = mostly uniform
        } else if (params.fill_ratio < 0.01) {
            uniform_prob = 0.98; // almost empty = mostly uniform
        } else {
            // Mixed: probability that a 2×2×2 sub-block is single-material
            uniform_prob = (1.0 - params.surface_complexity * 0.5) *
                           std::max(0.0, 1.0 - double(params.material_count) / 16.0);
        }
        // Add per-leaf noise
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        if (dist(rng) < uniform_prob) ++uniform_count;
    }
    s.octree_uniform_nodes = uniform_count;

    return s;
}

// Analytical cost computation for one strategy × chunk × view profile
struct CostResult {
    double render_us;         // GPU rendering cost per frame (µs)
    double vram_bytes;        // VRAM consumption (bytes)
    double quality;           // quality score (0-1)
    double update_us;         // re-bake cost on chunk mutation (µs)
};

static CostResult evaluate_strategy(StrategyId strat, ChunkStats const& chunk,
                                    SceneId scene, int /*seed*/) {
    auto const& sp = kStrategyParams[strat];

    if (strat == A_NoImpostor) {
        return {0.0, 0.0, 0.10, 0.0};
    }

    double render_us = 0.0;
    double vram_bytes = 0.0;
    double quality = 0.0;
    double update_us = sp.mutation_cost_us;

    switch (strat) {
    case B_SingleQuad: {
        // 1 quad, 4 verts, ~4-16 covered pixels at far distance
        double covered_pixels = 8.0 * 8.0; // ~8×8 px on screen at far LOD
        render_us = kDrawSetupUs + 4.0 * sp.vertex_cost_us +
                    sp.frag_cost_us_per_px * covered_pixels;
        vram_bytes = sp.texel_count * kBytesPerTexel + sp.metadata_bytes;
        quality = sp.quality_base + 0.20 * chunk.fill_ratio;
        // View-angle decay for single quad
        quality -= sp.view_angle_decay * 0.3;
        quality = std::clamp(quality, 0.0, 1.0);
        break;
    }
    case C_Static6Face: {
        // 2 nearest faces blended, each ~16×16 px at far distance
        double covered_pixels = 16.0 * 16.0 * 2.0;
        render_us = kDrawSetupUs * 2.0 + 8.0 * sp.vertex_cost_us +
                    sp.frag_cost_us_per_px * covered_pixels;
        vram_bytes = sp.texel_count * kBytesPerTexel + sp.metadata_bytes;
        // Quality: good silhouette, best for uniform/simple chunks
        double material_penalty = 1.0 - 0.08 * std::max(0, chunk.material_count - 1);
        quality = sp.quality_base * material_penalty;
        quality -= sp.view_angle_decay * (1.0 - chunk.fill_ratio) * 0.2;
        quality = std::clamp(quality, 0.0, 1.0);
        break;
    }
    case D_OctreeImpostor: {
        // Adaptive: uniform nodes get 1 quad, non-uniform get 2 quads (blended)
        int uniform = chunk.octree_uniform_nodes;
        int total = chunk.octree_total_nodes;
        int non_uniform = total - uniform;

        double uniform_quads = double(uniform);       // 1 quad each
        double complex_quads = double(non_uniform) * 2.0; // 2 quads each (blended)
        double total_quads = uniform_quads + complex_quads;

        // Only visible nodes contribute (approx 50% of far chunks visible)
        double visible_quads = total_quads * 0.5;

        render_us = kDrawSetupUs * 2.0 + visible_quads * 4.0 * sp.vertex_cost_us +
                    sp.frag_cost_us_per_px * visible_quads * 4.0; // ~4px each at far distance

        // VRAM: uniform → 1 face, non-uniform → 6 faces
        double uniform_texels = double(uniform) * double(kImpostorTexSize * kImpostorTexSize);
        double complex_texels = double(non_uniform) * double(kImpostorTexSize * kImpostorTexSize) * 6.0;

        // But total stored is less — at far LOD, use lower resolution
        double lod_factor = 0.25; // quarter resolution for far impostors
        vram_bytes = (uniform_texels + complex_texels) * kBytesPerTexel * lod_factor
                     + sp.metadata_bytes * double(total) / 8.0;

        // Quality: best of all strategies for complex chunks
        quality = sp.quality_base;
        quality -= 0.10 * (1.0 - double(uniform) / double(total)); // penalty for non-uniform
        quality = std::clamp(quality, 0.0, 1.0);
        break;
    }
    case E_GPUComputeDynamic: {
        // Same rendering cost as D
        auto d_result = evaluate_strategy(D_OctreeImpostor, chunk, scene, 0);
        render_us = d_result.render_us;
        vram_bytes = d_result.vram_bytes;

        // Update cost includes compute shader dispatch
        update_us = sp.mutation_cost_us * chunk.surface_complexity;

        // Slightly better quality from more frequent updates
        quality = d_result.quality + 0.05;
        quality = std::clamp(quality, 0.0, 1.0);
        break;
    }
    default:
        break;
    }

    return {render_us, vram_bytes, quality, update_us};
}

// ---- Harness ----
struct Measurement {
    char const* strategy;
    char const* scene;
    int seed;
    double mean_render_us;
    double median_render_us;
    double p95_render_us;
    double p99_render_us;
    double std_render_us;
    double vram_kb;
    double quality;
    double update_us;
    double wall_ms;
};

static double compute_median(std::span<double const> samples) {
    std::vector<double> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    size_t n = sorted.size();
    if (n % 2 == 0) return (sorted[n/2 - 1] + sorted[n/2]) * 0.5;
    return sorted[n/2];
}

static double compute_p(std::span<double const> samples, double pct) {
    std::vector<double> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    size_t idx = size_t(double(sorted.size() - 1) * pct / 100.0);
    return sorted[idx];
}

int main() {
    std::vector<Measurement> results;

    auto const& hp = kStrategyParams;
    auto const& sp = kSceneParams;

    for (int strat = 0; strat < kStrategyCount; ++strat) {
        for (int scene = 0; scene < kSceneCount; ++scene) {
            for (int seed : {1, 7, 42, 1234, 31337}) {
                ChunkStats chunk = classify_chunk(SceneId(scene), seed);

                // Warm-up: 10 iterations
                for (int w = 0; w < 10; ++w) {
                    volatile auto r = evaluate_strategy(StrategyId(strat), chunk, SceneId(scene), seed);
                    (void)r;
                }

                // Main: 1000 iterations
                std::vector<double> samples;
                samples.reserve(1000);

                auto t0 = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < 1000; ++i) {
                    auto r = evaluate_strategy(StrategyId(strat), chunk, SceneId(scene), seed);
                    samples.push_back(r.render_us);
                }
                auto t1 = std::chrono::high_resolution_clock::now();
                double wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

                double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
                double mean = sum / double(samples.size());
                double median = compute_median(samples);
                double p95 = compute_p(samples, 95.0);
                double p99 = compute_p(samples, 99.0);

                double sq_sum = 0.0;
                for (auto v : samples) sq_sum += (v - mean) * (v - mean);
                double std_dev = std::sqrt(sq_sum / double(samples.size()));

                auto ref_result = evaluate_strategy(StrategyId(strat), chunk, SceneId(scene), seed);

                results.push_back({
                    kStrategyNames[strat],
                    kSceneNames[scene],
                    seed,
                    mean,
                    median,
                    p95,
                    p99,
                    std_dev,
                    ref_result.vram_bytes / 1024.0,
                    ref_result.quality,
                    ref_result.update_us,
                    wall_ms
                });
            }
        }
    }

    // CSV output
    std::printf("strategy,scene,seed,mean_render_us,median_render_us,p95_render_us,"
                "p99_render_us,std_render_us,vram_kb,quality,update_us,wall_ms\n");
    for (auto const& m : results) {
        std::printf("%s,%s,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.4f,%.4f,%.6f,%.4f\n",
                    m.strategy, m.scene, m.seed,
                    m.mean_render_us, m.median_render_us, m.p95_render_us,
                    m.p99_render_us, m.std_render_us,
                    m.vram_kb, m.quality, m.update_us, m.wall_ms);
    }

    // Summary per strategy (mean across all configs)
    std::printf("\n=== SUMMARY (mean across all configs) ===\n");
    std::printf("strategy,mean_render_us,mean_vram_kb,mean_quality,mean_update_us\n");
    for (int strat = 0; strat < kStrategyCount; ++strat) {
        double rsum = 0, vsum = 0, qsum = 0, usum = 0;
        int count = 0;
        for (auto const& m : results) {
            if (m.strategy == kStrategyNames[strat]) {
                rsum += m.mean_render_us;
                vsum += m.vram_kb;
                qsum += m.quality;
                usum += m.update_us;
                ++count;
            }
        }
        std::printf("%s,%.6f,%.4f,%.4f,%.6f\n",
                    kStrategyNames[strat],
                    rsum / count, vsum / count, qsum / count, usum / count);
    }

    // Per-strategy × per-scene means
    std::printf("\n=== PER-STRATEGY × PER-SCENE (mean_render_us) ===\n");
    std::printf("strategy");
    for (int s = 0; s < kSceneCount; ++s) std::printf(",%s", kSceneNames[s]);
    std::printf("\n");
    for (int strat = 0; strat < kStrategyCount; ++strat) {
        std::printf("%s", kStrategyNames[strat]);
        for (int scene = 0; scene < kSceneCount; ++scene) {
            double sum = 0; int cnt = 0;
            for (auto const& m : results) {
                if (m.strategy == kStrategyNames[strat] && m.scene == kSceneNames[scene]) {
                    sum += m.mean_render_us; ++cnt;
                }
            }
            std::printf(",%.6f", sum / cnt);
        }
        std::printf("\n");
    }

    return 0;
}
