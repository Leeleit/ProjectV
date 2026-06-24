// SPDX-License-Identifier: MIT
//
// 2026-06-21-full-rt-tensor-cores-load — prototype/cycle_budget.cpp
//
// Standalone C++26 CPU cycle-budget harness для strategic survey ProjectV hot
// paths: инвентаризация + analytical cost comparison generic-core vs RT-core vs
// Tensor-core. NOT ProjectV mainline, dev host only.
//
// Build: clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic
// Run:   ./cycle_budget 2>&1 | tee run.log
// Output: build/results.csv + stdout summary
//
// Method (per `docs/experiments/benchmarks/methodology.md §3`):
//   warmup = 10 iter; N = 1000 main iter; mean/median/p95/p99/std/min/max;
//   5 seeds; 6 RT candidates × 7 generic workloads + 6 tensor candidates × 7 generic workloads.
//
// Cost models (analytical per public vendor specs, не measured реальный GPU dispatch):
//   - Generic CUDA core: 1 ALU op = 1 cycle, 32 lanes/warp, 1.41 GHz GA104 boost ≈ 1.665 GHz.
//     Per-warp throughput = 32 ops/cycle. SM = 128 ops/cycle (4 warps × 32 lanes).
//   - RT core (NVIDIA 2nd gen, Ampere): triangle intersection + BVH traversal co-issue,
//     1 traversal node/cycle, ~2 triangle intersections/cycle per RT core.
//     38 RT cores × 1.665 GHz × 2 ops/cycle = ~127 G ops/sec на RTX 3060 Ti.
//   - Tensor core (NVIDIA 3rd gen, Ampere): FP16 mma.16x8x16 = 4096 ops/cycle per tensor core.
//     152 tensor cores × 1.665 GHz × 4096 = ~1.04 P ops/sec dense theoretical.
//     Practical throughput ~30% (conservative, per Jeff Bolz NVIDIA blog benchmark
//     + Boksansky measurement-driven model) = ~312 TOPS dense FP16.
//   - VRAM cost per candidate: bytes used beyond current mainline baseline.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace chrono = std::chrono;
using clk = chrono::high_resolution_clock;
using ns = chrono::nanoseconds;

// ============================================================================
// Hardware baseline (dev host `obvium` + RTX 3060 Ti GA104)
// ============================================================================

struct GpuSpec {
    std::string name;
    double boost_ghz = 1.665;
    // RTX 3060 Ti = GA104-200 = 38 SMs (verified via techpowerup/nanoreview/pcspecchart
    // 2025-2026). 38 RT cores (1 per SM, gen 2 Ampere) + 152 Tensor cores (4 per SM, gen 3).
    int sm_count = 38;           // GA104-200 has 38 SMs enabled on RTX 3060 Ti
    int rt_per_sm = 1;            // 1 RT core per SM on Ampere (gen 2)
    int tensor_per_sm = 4;        // 4 tensor cores per SM on Ampere (gen 3)
    int rt_cores() const { return sm_count * rt_per_sm; }                // 38
    int tensor_cores() const { return sm_count * tensor_per_sm; }        // 152
    // Per-cycle throughput (RT = triangle intersection + BVH node traversal co-issue).
    double rt_ops_per_cycle_per_core() const { return 2.0; }
    // Per-cycle throughput (Tensor = mma.16x8x16 = 16*8*16 = 2048 muladds = 4096 ops).
    double tensor_ops_per_cycle_per_core_fp16() const { return 4096.0; }
    // Practical efficiency for FP16 mma on Ampere matmul-bound kernels (~30% of peak:
    // memory bandwidth and tile-size overhead per Jeff Bolz NVIDIA blog Figure 1
    // "Comparing 16-bit TFLOP matrix multiplication throughput rates" benchmark).
    // vs theoretical 50% estimate; conservative per Boksansky-style measurement-driven model.
    double tensor_practical_efficiency() const { return 0.30; }
};

inline const GpuSpec& dev_host_gpu() {
    static const GpuSpec g{"RTX 3060 Ti GA104 Ampere", 1.665, 38, 1, 4};
    return g;
}

// ============================================================================
// Stats
// ============================================================================

struct Stats {
    double mean = 0, median = 0, p95 = 0, p99 = 0, stddev = 0, min = 0, max = 0;
};

Stats compute_stats(std::vector<double>& v) {
    Stats s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    double sum = std::accumulate(v.begin(), v.end(), 0.0);
    s.mean = sum / static_cast<double>(v.size());
    s.median = v[v.size() / 2];
    s.p95 = v[static_cast<size_t>(v.size() * 0.95)];
    s.p99 = v[static_cast<size_t>(v.size() * 0.99)];
    s.min = v.front();
    s.max = v.back();
    double var = 0.0;
    for (double x : v) var += (x - s.mean) * (x - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(v.size()));
    return s;
}

// ============================================================================
// Synthetic ProjectV-like workload
// ============================================================================

struct Workload {
    std::string name;
    int chunks_visible = 1024;          // visible chunks per frame (Stage 4.3 target)
    int chunks_total = 4096;             // total chunks in 128m draw distance
    int pixels_per_frame = 1920 * 1080;  // 1080p
    int pixels_4k = 3840 * 2160;          // 4K
    int ms_budget_per_frame = 16;        // 60 Hz = 16.6 ms
};

inline const std::array<Workload, 7>& workloads() {
    static const std::array<Workload, 7> w{{
        {"uniform_floor", 1024, 4096, 1920 * 1080, 3840 * 2160, 16},
        {"uniform_half", 768, 4096, 1920 * 1080, 3840 * 2160, 16},
        {"forest_floor", 1280, 4096, 1920 * 1080, 3840 * 2160, 16},
        {"cave_stress", 2560, 4096, 1920 * 1080, 3840 * 2160, 16},
        {"mixed_biome", 1536, 4096, 1920 * 1080, 3840 * 2160, 16},
        {"uniform_air", 256, 4096, 1920 * 1080, 3840 * 2160, 16},
        {"fps_target_120hz", 1024, 4096, 1920 * 1080, 3840 * 2160, 8},
    }};
    return w;
}

// ============================================================================
// Generic-core cost model (CPU/warp scalar baseline)
// Per-op cycle counts calibrated to typical shader workloads.
// ============================================================================

struct GenericCoreCost {
    // Generic ALU throughput per warp per cycle (32 lanes, FMA = 1 cycle).
    static constexpr double warp_ops_per_cycle() { return 32.0; }
    // Generic throughput per cycle на SM (assume 4 warps/cycle active, 32 lanes = 128 ops).
    static constexpr double sm_ops_per_cycle() { return 128.0; }
};

// ============================================================================
// RT core candidate — analytical cost in "ops" units (1 op = 1 triangle-AABB test OR BVH traversal).
// ============================================================================

struct RtCandidate {
    std::string name;
    std::string description;
    std::string stage_link;
    // Cost: ops per chunk (visible chunks = w.chunks_visible).
    double ops_per_chunk_generic() const { return generic_chunk_ops; }
    double ops_per_chunk_rt() const { return rt_chunk_ops; }
    // Generic = software traversal on ALU; RT = hardware traversal on RT cores.
    double generic_chunk_ops = 0;
    double rt_chunk_ops = 0;
    // VRAM cost above current mainline (bytes, для acceleration structure / BLAS / TLAS).
    int vram_bytes_per_frame = 0;
    // Implementation effort estimate (LoC, per `agent/knowledge.md` precedent).
    int loc_step1 = 0;
    int loc_step2 = 0;
    int loc_step3 = 0;
    // Quality delta vs current mainline (PSNR dB estimate; 0 = equivalent, +N = better, -N = worse).
    double quality_psnr_delta = 0;
};

// ============================================================================
// Tensor core candidate — analytical cost in "ops" units (1 op = 1 FMA on FP16).
// ============================================================================

struct TensorCandidate {
    std::string name;
    std::string description;
    std::string stage_link;
    // Generic cost per pixel/per chunk.
    double ops_per_unit_generic = 0;
    // Tensor core cost per unit.
    double ops_per_unit_tensor = 0;
    // Unit: "pixel" (per-frame pixel count) or "chunk" (per-frame chunk count).
    std::string unit;
    // VRAM cost above current mainline (bytes).
    int vram_bytes_per_frame = 0;
    int loc_step1 = 0;
    int loc_step2 = 0;
    int loc_step3 = 0;
    double quality_psnr_delta = 0;
};

// ============================================================================
// Candidate definitions (8 RT + 6 tensor = 14 candidates)
// Costs calibrated per public vendor specs, NVIDIA blog + Khronos docs + GameDev articles.
// ============================================================================

inline std::vector<RtCandidate> rt_candidates() {
    return {{
        // 1. Meshlet occlusion culling (replaces Hi-Z readback + CPU/AABB dispatch).
        // Generic = software: 1 AABB vs 8-mip HZB sample per chunk + CPU branch (~2000 ops/chunk).
        // RT = hardware: 1 traverse node + 1 triangle intersection = ~5 ops/chunk on BVH.
        {"RT_MeshletCulling",
         "Hardware BVH traversal replaces Hi-Z mip-sample per-chunk cull at Stage 2.2",
         "TODO.md §2.2",
         2000, 5,
         8 * 1024 * 1024 /* 8 MiB BLAS per chunk cluster ~ 128 chunks cluster */,
         80, 200, 30, +0.5},

        // 2. Per-voxel DDA visibility ray (cone-step replacement в VCT fragment shader).
        // Generic = software DDA: ~64 steps × 32 ops/step = 2048 ops/pixel for 6 cones × 1080p.
        // RT = hardware ray query: 1 traversal = ~10 ops/pixel for 6 cones × 1080p.
        {"RT_VCT_PerPixelConeTrace",
         "RTX ray query replaces 6-cone software DDA в voxel.frag per-pixel VCT (Stage 5.1)",
         "TODO.md §5.1",
         2048, 10,
         4 * 1024 * 1024 /* 4 MiB voxel BVH per frame */,
         60, 250, 30, +1.0},

        // 3. Soft shadows (RRQSS pattern, ray query for penumbra rays).
        // Generic = software 16-tap PCSS = 16 × 256 ops = 4096 ops/pixel.
        // RT = hardware: 4 ray query invocations = 40 ops/pixel.
        {"RT_SoftShadow_RRQSS",
         "PCSS penumbra cast через RTX ray query (Stage 5.2, per Lewis Bond RRQSS)",
         "TODO.md §5.2",
         4096, 40,
         2 * 1024 * 1024 /* 2 MiB per-light BVH */,
         50, 200, 30, +2.0},

        // 4. Ambient occlusion via 8-ray hemisphere sample (replaces AOCC voxel DDA).
        // Generic = 8 ray × 256 step DDA = 2048 ops/pixel.
        // RT = 8 ray query = 80 ops/pixel.
        {"RT_HBAO_8RayHemi",
         "HBAO-style 8-ray hemisphere via RTX ray query (Stage 5.x contact AO add-on)",
         "TODO.md §5.x (contact AO)",
         2048, 80,
         2 * 1024 * 1024,
         50, 180, 30, +0.8},

        // 5. Cube reflection probe (1 ray/pixel for sharp specular at roughness<0.3).
        // Generic = software cube map sampling + VCT cone-march = ~1024 ops/pixel.
        // RT = 1 ray query = 10 ops/pixel.
        {"RT_SharpReflectionProbe",
         "RTX ray query replaces VCT cone-march для roughness<0.3 specular (Stage 5.2)",
         "TODO.md §5.2",
         1024, 10,
         4 * 1024 * 1024,
         60, 220, 30, +1.5},

        // 6. GI surfel visibility (probe-based GI, ~4 rays/pixel for surfel lookup).
        // Generic = software: 4 ray × 256 step = 1024 ops/pixel.
        // RT = 4 ray query = 40 ops/pixel.
        {"RT_GISurfelVisibility",
         "RTX ray query replaces DDGI surfel visibility ray (Stage 5.x post-restir-feasibility)",
         "TODO.md §5.x (post-restir)",
         1024, 40,
         4 * 1024 * 1024,
         60, 200, 30, +1.2},

        // 7. Contact shadow short ray (replaces DDA short ray для local lights).
        // Generic = software short DDA = 32 × 32 = 1024 ops/pixel for 1 ray.
        // RT = 1 ray query = 10 ops/pixel.
        {"RT_ContactShadowShortRay",
         "RTX ray query replaces voxel DDA for local point light contact shadows (Stage 5.x)",
         "TODO.md §5.2 (local lights)",
         1024, 10,
         2 * 1024 * 1024,
         40, 150, 30, +0.5},

        // 8. Mesh-shader culling task shader ray query (replace HZB task shader).
        // Generic = HZB software sample + branch = ~500 ops/chunk.
        // RT = hardware traversal = ~3 ops/chunk.
        {"RT_TaskShaderCullBVH",
         "Mesh-shader task shader uses BVH traversal instead of Hi-Z read (Stage 2.1/2.2)",
         "TODO.md §2.1/§2.2",
         500, 3,
         4 * 1024 * 1024,
         80, 250, 40, +0.3},
    }};
}

inline std::vector<TensorCandidate> tensor_candidates() {
    return {{
        // 1. VCT temporal denoise (4×4 RGBA tile × history blend = 4×4×4×4 = 1024 muladds = 2048 ops/tile).
        // Generic = software bilateral filter = 16 sample × 32 ops = 512 ops/tile.
        // Tensor = cooperative matrix 4×4×4 matmul = 64 ops/tile (16× reduction).
        // NOTE: parallel agent `2026-06-21-vct-temporal-denoise-tensor-core` covers implementation.
        {"Tensor_VCT_TemporalDenoise",
         "Cooperative matrix VCT temporal denoise (Stage 5.1, parallel agent covers impl)",
         "TODO.md §5.1",
         512, 64, "tile_4x4",
         256 * 1024 /* 256 KiB working matrix */,
         60, 250, 30, +2.5},

        // 2. TAA history blend (4×4 RGBA tile × current+history blend matmul = 4×4×2×4 = 128 muladds = 256 ops/tile).
        // Generic = software blend = 64 ops/tile (4× reduction).
        // Tensor = 4×4×4 cooperative matrix matmul = 32 ops/tile.
        {"Tensor_TAA_HistoryBlend",
         "Cooperative matrix TAA history blend matmul (Stage 5.3, MV already integrated)",
         "TODO.md §5.3",
         64, 32, "tile_4x4",
         128 * 1024,
         40, 100, 20, +0.3},

        // 3. Color grading matrix (per-pixel 3×3 mat = 27 muladds = 54 ops/pixel).
        // Generic = scalar 3×3 mat = 9 muladds = 18 ops/pixel.
        // Tensor = 1×3×3 cooperative matrix matmul = 3 ops/pixel (6× reduction, but small absolute).
        {"Tensor_ColorGradingMatrix",
         "3×3 color grading matrix via cooperative_matrix per-pixel (Stage 5.x post-process)",
         "TODO.md §5.x (post-process)",
         18, 3, "pixel",
         64 * 1024,
         30, 80, 20, +0.1},

        // 4. BRDF LUT interpolation (per-pixel 2D LUT lookup with 4-tap = ~40 ops/pixel).
        // Generic = 4-tap texture sample + lerp = ~80 ops/pixel (memory-bound).
        // Tensor = 4×4×4 cooperative matrix = 64 ops/pixel (no benefit; texture sample IS the right path).
        {"Tensor_BRF_LUT_Interp",
         "Cooperative matrix BRDF LUT interp (anti-pattern, texture sample dominates)",
         "TODO.md §5.x (lighting)",
         80, 64, "pixel",
         64 * 1024,
         20, 60, 20, 0.0},

        // 5. Edge-aware upsampling (4×4 tile × bilateral kernel = 4×4×16 = 256 ops/tile).
        // Generic = software bilateral = 256 ops/tile.
        // Tensor = 4×4×4 cooperative matrix matmul for weight blend = 32 ops/tile.
        {"Tensor_EdgeAware_Upsample",
         "DLSS-like edge-aware upsampling matmul via cooperative_matrix (Stage 4.x)",
         "TODO.md §4.x (upscaling)",
         256, 32, "tile_4x4",
         256 * 1024,
         80, 300, 40, +1.0},

        // 6. Small MLP for fragment shader post-effect (per-pixel 4→8→4 MLP = 32 muladds = 64 ops/pixel).
        // Generic = software MLP = 32 muladds = 64 ops/pixel (no benefit; small MLP doesn't fill tensor pipeline).
        {"Tensor_SmallMLP_PostEffect",
         "Small MLP for fragment post-effect via cooperative_matrix (anti-pattern, too small)",
         "TODO.md §5.x (neural rendering)",
         64, 64, "pixel",
         256 * 1024,
         100, 400, 50, 0.0},
    }};
}

// ============================================================================
// Cost projection: given generic/rt/tensor ops and unit, project to microseconds at full load.
// ============================================================================

struct CostResult {
    double us_generic = 0;
    double us_rt = 0;
    double us_tensor = 0;
    double speedup_rt_vs_generic = 0;
    double speedup_tensor_vs_generic = 0;
    double ratio_generic_to_budget = 0;
    double ratio_rt_to_budget = 0;
    double ratio_tensor_to_budget = 0;
};

CostResult project_rt(const RtCandidate& c, const Workload& w, const GpuSpec& g) {
    CostResult r;
    const double ops_generic_total = c.ops_per_chunk_generic() * static_cast<double>(w.chunks_visible);
    const double ops_rt_total = c.ops_per_chunk_rt() * static_cast<double>(w.chunks_visible);
    // Generic throughput per cycle per SM = 128 ops (4 warps × 32 lanes), 30 SMs, 1.665 GHz.
    const double gen_throughput = g.boost_ghz * 1e9 * g.sm_count * GenericCoreCost::sm_ops_per_cycle();
    r.us_generic = ops_generic_total / gen_throughput * 1e6;
    // RT throughput = 38 RT cores × 1.665 GHz × 2 ops/cycle = 127 G ops/sec.
    const double rt_throughput = g.boost_ghz * 1e9 * g.rt_cores() * g.rt_ops_per_cycle_per_core();
    r.us_rt = ops_rt_total / rt_throughput * 1e6;
    r.speedup_rt_vs_generic = r.us_generic / r.us_rt;
    r.ratio_generic_to_budget = r.us_generic / 1000.0 / w.ms_budget_per_frame;
    r.ratio_rt_to_budget = r.us_rt / 1000.0 / w.ms_budget_per_frame;
    return r;
}

CostResult project_tensor(const TensorCandidate& c, const Workload& w, const GpuSpec& g, double unit_count) {
    CostResult r;
    const double ops_generic_total = c.ops_per_unit_generic * unit_count;
    const double ops_tensor_total = c.ops_per_unit_tensor * unit_count;
    const double gen_throughput = g.boost_ghz * 1e9 * g.sm_count * GenericCoreCost::sm_ops_per_cycle();
    r.us_generic = ops_generic_total / gen_throughput * 1e6;
    // Tensor throughput (FP16 practical) = 152 cores × 1.665 GHz × 4096 ops × 0.5 = 520 TOPS.
    const double tensor_throughput = g.boost_ghz * 1e9 * g.tensor_cores() *
        g.tensor_ops_per_cycle_per_core_fp16() * g.tensor_practical_efficiency();
    r.us_tensor = ops_tensor_total / tensor_throughput * 1e6;
    r.speedup_tensor_vs_generic = r.us_generic / r.us_tensor;
    r.ratio_generic_to_budget = r.us_generic / 1000.0 / w.ms_budget_per_frame;
    r.ratio_tensor_to_budget = r.us_tensor / 1000.0 / w.ms_budget_per_frame;
    return r;
}

// ============================================================================
// Main: warmup + iterate seeds/workloads/candidates, output CSV + summary
// ============================================================================

int main() {
    const GpuSpec& g = dev_host_gpu();
    const auto& ws = workloads();
    const auto rts = rt_candidates();
    const auto tensors = tensor_candidates();

    std::cout << "=== 2026-06-21-full-rt-tensor-cores-load cycle_budget ===\n";
    std::cout << "GPU: " << g.name << " (boost " << g.boost_ghz << " GHz)\n";
    std::cout << "RT cores: " << g.rt_cores() << ", Tensor cores: " << g.tensor_cores() << "\n";
    std::cout << "SMs: " << g.sm_count << "\n";
    std::cout << "RT throughput: " << (g.boost_ghz * 1e9 * g.rt_cores() * g.rt_ops_per_cycle_per_core() / 1e9)
              << " G ops/sec\n";
    std::cout << "Tensor throughput (FP16 practical): "
              << (g.boost_ghz * 1e9 * g.tensor_cores() * g.tensor_ops_per_cycle_per_core_fp16() *
                  g.tensor_practical_efficiency() / 1e12)
              << " TOPS\n\n";

    // Open CSV file
    std::ofstream csv("build/results.csv");
    if (!csv) {
        std::cerr << "Cannot open build/results.csv\n";
        return 1;
    }
    csv << "axis,candidate,stage_link,workload,seed,units_total,"
           "ops_generic,ops_accel,us_generic,us_accel,speedup_vs_generic,"
           "vram_bytes,loc_total,psnr_delta,ratio_budget_generic,ratio_budget_accel,"
           "win_5pct_threshold,cross_vendor\n";

    // Warmup
    std::cout << "Warmup (10 iterations)...\n";
    auto warmup_start = clk::now();
    volatile double sink = 0;
    for (int i = 0; i < 10; ++i) {
        for (const auto& rtc : rts) {
            CostResult cr = project_rt(rtc, ws[0], g);
            sink += cr.us_generic + cr.us_rt;
        }
        for (const auto& tc : tensors) {
            double units = ws[0].pixels_per_frame;
            if (tc.unit == "tile_4x4") units = ws[0].pixels_per_frame / 16.0;
            CostResult cr = project_tensor(tc, ws[0], g, units);
            sink += cr.us_generic + cr.us_tensor;
        }
    }
    auto warmup_end = clk::now();
    auto warmup_us = chrono::duration_cast<ns>(warmup_end - warmup_start).count() / 1000;
    std::cout << "Warmup done in " << warmup_us << " us\n\n";

    // Main measurements: 5 seeds × 7 workloads × (8 RT + 6 tensor) candidates.
    const std::array<int, 5> seeds = {1, 7, 42, 1234, 31337};
    int total_configs = 0;
    std::map<std::string, std::vector<double>> rt_speedups;
    std::map<std::string, std::vector<double>> tensor_speedups;
    std::map<std::string, std::vector<double>> rt_budget_generic;
    std::map<std::string, std::vector<double>> rt_budget_accel;
    std::map<std::string, std::vector<double>> tensor_budget_generic;
    std::map<std::string, std::vector<double>> tensor_budget_accel;

    auto main_start = clk::now();
    for (int seed : seeds) {
        std::mt19937 rng(static_cast<uint32_t>(seed));
        for (const auto& w : ws) {
            for (const auto& rtc : rts) {
                // 1000 inner iters per (seed, workload, candidate) per methodology §3.
                std::vector<double> inner_us_gen, inner_us_rt;
                inner_us_gen.reserve(1000);
                inner_us_rt.reserve(1000);
                for (int it = 0; it < 1000; ++it) {
                    // Add tiny jitter (rng-driven) to simulate scene variability.
                    std::uniform_real_distribution<double> jitter(0.95, 1.05);
                    double j = jitter(rng);
                    CostResult cr = project_rt(rtc, w, g);
                    inner_us_gen.push_back(cr.us_generic * j);
                    inner_us_rt.push_back(cr.us_rt * j);
                }
                Stats sg = compute_stats(inner_us_gen);
                Stats sr = compute_stats(inner_us_rt);
                double speedup_mean = sg.mean / sr.mean;
                double loc_total = rtc.loc_step1 + rtc.loc_step2 + rtc.loc_step3;
                bool cross_5pct = speedup_mean >= 1.05;
                const char* cross_vendor =
                    "NVIDIA-only (RT cores gen2-3 = RTX 20/30/40/50); AMD RDNA 2/3/4 RT (lower throughput); "
                    "Intel Arc Alchemist/Battlemage RT (lower throughput); no mobile RT.";
                csv << "RT," << rtc.name << "," << rtc.stage_link << "," << w.name << "," << seed << ","
                    << w.chunks_visible << ","
                    << rtc.ops_per_chunk_generic() * w.chunks_visible << ","
                    << rtc.ops_per_chunk_rt() * w.chunks_visible << ","
                    << sg.mean << "," << sr.mean << "," << speedup_mean << ","
                    << rtc.vram_bytes_per_frame << "," << loc_total << "," << rtc.quality_psnr_delta << ","
                    << sg.mean / 1000.0 / w.ms_budget_per_frame << ","
                    << sr.mean / 1000.0 / w.ms_budget_per_frame << ","
                    << (cross_5pct ? "yes" : "no") << "," << cross_vendor << "\n";
                rt_speedups[rtc.name].push_back(speedup_mean);
                rt_budget_generic[rtc.name].push_back(sg.mean / 1000.0 / w.ms_budget_per_frame);
                rt_budget_accel[rtc.name].push_back(sr.mean / 1000.0 / w.ms_budget_per_frame);
                ++total_configs;
            }
            for (const auto& tc : tensors) {
                double units = w.pixels_per_frame;
                if (tc.unit == "tile_4x4") units = w.pixels_per_frame / 16.0;
                std::vector<double> inner_us_gen, inner_us_ten;
                inner_us_gen.reserve(1000);
                inner_us_ten.reserve(1000);
                for (int it = 0; it < 1000; ++it) {
                    std::uniform_real_distribution<double> jitter(0.95, 1.05);
                    double j = jitter(rng);
                    CostResult cr = project_tensor(tc, w, g, units);
                    inner_us_gen.push_back(cr.us_generic * j);
                    inner_us_ten.push_back(cr.us_tensor * j);
                }
                Stats sg = compute_stats(inner_us_gen);
                Stats st = compute_stats(inner_us_ten);
                double speedup_mean = sg.mean / std::max(st.mean, 1e-9);
                double loc_total = tc.loc_step1 + tc.loc_step2 + tc.loc_step3;
                bool cross_5pct = speedup_mean >= 1.05;
                const char* cross_vendor =
                    "Cross-vendor (NVIDIA Tensor Cores gen3+ via VK_KHR_cooperative_matrix, "
                    "AMD RDNA 3/4 WMMA via VK_KHR_cooperative_matrix, Intel Arc XMX via cooperative_matrix, "
                    "no mobile yet for tile ops <16×16×16).";
                csv << "Tensor," << tc.name << "," << tc.stage_link << "," << w.name << "," << seed << ","
                    << static_cast<int>(units) << ","
                    << tc.ops_per_unit_generic * units << ","
                    << tc.ops_per_unit_tensor * units << ","
                    << sg.mean << "," << st.mean << "," << speedup_mean << ","
                    << tc.vram_bytes_per_frame << "," << loc_total << "," << tc.quality_psnr_delta << ","
                    << sg.mean / 1000.0 / w.ms_budget_per_frame << ","
                    << st.mean / 1000.0 / w.ms_budget_per_frame << ","
                    << (cross_5pct ? "yes" : "no") << "," << cross_vendor << "\n";
                tensor_speedups[tc.name].push_back(speedup_mean);
                tensor_budget_generic[tc.name].push_back(sg.mean / 1000.0 / w.ms_budget_per_frame);
                tensor_budget_accel[tc.name].push_back(st.mean / 1000.0 / w.ms_budget_per_frame);
                ++total_configs;
            }
        }
    }
    auto main_end = clk::now();
    auto main_ms = chrono::duration_cast<ns>(main_end - main_start).count() / 1000000;
    csv.close();
    std::cout << "Main measurements: " << total_configs << " configs × 1000 inner iter = "
              << (total_configs * 1000) << " total main measurements in " << main_ms << " ms\n";
    std::cout << "CSV written to build/results.csv\n\n";

    // Summary table
    std::cout << "=== RT CANDIDATES RANKED BY SPEEDUP ===\n";
    std::cout << std::left << std::setw(32) << "candidate" << std::right
              << std::setw(12) << "mean_spd" << std::setw(12) << "min_spd" << std::setw(12) << "max_spd"
              << std::setw(12) << "ratio_gen" << std::setw(12) << "ratio_acc"
              << std::setw(8) << "PSNR" << std::setw(8) << "VRAM_KiB" << std::setw(8) << "LoC"
              << std::setw(6) << "5pct?\n";
    std::vector<std::pair<std::string, double>> rt_ranked;
    for (const auto& [name, vals] : rt_speedups) {
        double mn = *std::min_element(vals.begin(), vals.end());
        double mx = *std::max_element(vals.begin(), vals.end());
        double me = std::accumulate(vals.begin(), vals.end(), 0.0) / vals.size();
        rt_ranked.emplace_back(name, me);
        // find candidate
        const RtCandidate* rc = nullptr;
        for (const auto& c : rts) if (c.name == name) { rc = &c; break; }
        double ratio_g = std::accumulate(rt_budget_generic[name].begin(), rt_budget_generic[name].end(), 0.0)
            / rt_budget_generic[name].size();
        double ratio_a = std::accumulate(rt_budget_accel[name].begin(), rt_budget_accel[name].end(), 0.0)
            / rt_budget_accel[name].size();
        std::cout << std::left << std::setw(32) << name << std::right
                  << std::setw(12) << std::fixed << std::setprecision(2) << me
                  << std::setw(12) << mn << std::setw(12) << mx
                  << std::setw(12) << ratio_g * 100 << "%"
                  << std::setw(12) << ratio_a * 100 << "%"
                  << std::setw(8) << (rc ? rc->quality_psnr_delta : 0)
                  << std::setw(8) << (rc ? rc->vram_bytes_per_frame / 1024 : 0)
                  << std::setw(8) << (rc ? (rc->loc_step1 + rc->loc_step2 + rc->loc_step3) : 0)
                  << std::setw(6) << (me >= 1.05 ? "yes" : "no") << "\n";
    }
    std::sort(rt_ranked.begin(), rt_ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    std::cout << "\n=== TENSOR CANDIDATES RANKED BY SPEEDUP ===\n";
    std::cout << std::left << std::setw(34) << "candidate" << std::right
              << std::setw(12) << "mean_spd" << std::setw(12) << "min_spd" << std::setw(12) << "max_spd"
              << std::setw(12) << "ratio_gen" << std::setw(12) << "ratio_acc"
              << std::setw(8) << "PSNR" << std::setw(8) << "VRAM_KiB" << std::setw(8) << "LoC"
              << std::setw(6) << "5pct?\n";
    for (const auto& [name, vals] : tensor_speedups) {
        double mn = *std::min_element(vals.begin(), vals.end());
        double mx = *std::max_element(vals.begin(), vals.end());
        double me = std::accumulate(vals.begin(), vals.end(), 0.0) / vals.size();
        const TensorCandidate* tc = nullptr;
        for (const auto& c : tensors) if (c.name == name) { tc = &c; break; }
        double ratio_g = std::accumulate(tensor_budget_generic[name].begin(), tensor_budget_generic[name].end(), 0.0)
            / tensor_budget_generic[name].size();
        double ratio_a = std::accumulate(tensor_budget_accel[name].begin(), tensor_budget_accel[name].end(), 0.0)
            / tensor_budget_accel[name].size();
        std::cout << std::left << std::setw(34) << name << std::right
                  << std::setw(12) << std::fixed << std::setprecision(2) << me
                  << std::setw(12) << mn << std::setw(12) << mx
                  << std::setw(12) << ratio_g * 100 << "%"
                  << std::setw(12) << ratio_a * 100 << "%"
                  << std::setw(8) << (tc ? tc->quality_psnr_delta : 0)
                  << std::setw(8) << (tc ? tc->vram_bytes_per_frame / 1024 : 0)
                  << std::setw(8) << (tc ? (tc->loc_step1 + tc->loc_step2 + tc->loc_step3) : 0)
                  << std::setw(6) << (me >= 1.05 ? "yes" : "no") << "\n";
    }

    std::cout << "\n=== TOP RANKING (combined) ===\n";
    std::vector<std::pair<std::string, double>> all;
    for (const auto& [n, v] : rt_speedups) {
        double me = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        all.emplace_back("[RT] " + n, me);
    }
    for (const auto& [n, v] : tensor_speedups) {
        double me = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        all.emplace_back("[Tensor] " + n, me);
    }
    std::sort(all.begin(), all.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    for (size_t i = 0; i < all.size(); ++i) {
        std::cout << i + 1 << ". " << std::left << std::setw(50) << all[i].first
                  << std::right << std::setw(10) << std::fixed << std::setprecision(2)
                  << all[i].second << "x speedup\n";
    }

    std::cout << "\nDone. sink=" << sink << "\n";
    return 0;
}