// God Rays / Crepuscular Rays / Sun Shafts — analytical cost model
// Standalone C++26 CPU. Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
//                            god_rays_sim.cpp -o build/god_rays_sim
// Output: build/results.csv (151 rows = 1 header + 150 measurements).
//
// Per-strategy costs calibrated against literature (see sources.md):
//   A_NoGodRays                          — baseline (no work)
//   B_ScreenSpaceRadialBlur              — Mitchell 2007 + Crytek 2008 (3 passes × N samples)
//   C_AnalyticOccludedRayMarch           — Yusov 2014 epipolar + 1D min/max trees
//   D_VolumetricConeTraceRayQuery        — Lumen 2022 hybrid RTX ray query
//   E_HybridRadialBlurPlusVolumetric     — B + D combined (cascade handoff)
//   F_PrecomputedSkydomeBaked            — static-only baked skydome texture
//
// Per-scene properties:
//   uniform_floor          — low occluder (0.10), high sun vis (0.85) — rays subtle
//   forest_floor           — med  occluder (0.45), med  sun vis (0.40) — rays prominent
//   cave_stress            — low occluder (entrance glow), low sun vis (0.05) — rays invisible
//   mixed_biome            — med  occluder (0.30), med-high sun vis (0.60) — rays medium
//   dense_foliage_stress   — high occluder (0.75), low sun vis (0.15) — rays very prominent
//
// Measurement grid: 6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 150,000 main
// measurements, wall time ~1-2 sec on Zen 3 5800X.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// Constants
// =============================================================================

constexpr int kWarmupIters = 10;
constexpr int kMainIters = 1000;
constexpr int kSeedCount = 5;
constexpr std::array<int, kSeedCount> kSeeds = {1, 7, 42, 1234, 31337};
constexpr std::array<int, 5> kResolutionsW = {1920, 2560, 3840, 1920, 1920};
constexpr std::array<int, 5> kResolutionsH = {1080, 1440, 2160, 1200, 1080};
constexpr std::array<const char*, 6> kStrategies = {
    "A_NoGodRays",
    "B_ScreenSpaceRadialBlur",
    "C_AnalyticOccludedRayMarch",
    "D_VolumetricConeTraceRayQuery",
    "E_HybridRadialBlurPlusVolumetric",
    "F_PrecomputedSkydomeBaked",
};
constexpr std::array<const char*, 5> kScenes = {
    "uniform_floor",
    "forest_floor",
    "cave_stress",
    "mixed_biome",
    "dense_foliage_stress",
};

// =============================================================================
// Scene properties
// =============================================================================

struct SceneProperties {
    double occluder_density;     // 0.0 = none, 1.0 = full occlusion (foliage, terrain)
    double sun_visibility;       // 0.0 = fully occluded, 1.0 = fully visible
    double scene_complexity;     // 0.0 = simple, 1.0 = dense (affects RTX traversal cost)
    const char* perceptual_label;
};

constexpr std::array<SceneProperties, 5> kSceneProps = {{
    {0.10, 0.85, 0.20, "rays_subtle"},   // uniform_floor
    {0.45, 0.40, 0.60, "rays_prominent"}, // forest_floor
    {0.05, 0.05, 0.30, "rays_invisible"}, // cave_stress (entrance glow only)
    {0.30, 0.60, 0.50, "rays_medium"},    // mixed_biome
    {0.75, 0.15, 0.80, "rays_very_prominent"}, // dense_foliage_stress
}};

// =============================================================================
// Strategy cost functions (analytical models calibrated to literature)
// =============================================================================

struct StrategyCost {
    double base_ms_1080p;         // base cost at 1080p
    double ms_per_megapixel;      // scaling per megapixel
    double vram_mib;              // VRAM allocation
    double base_psnr_db;          // perceptual quality baseline (vs A)
    double scene_complexity_mul;  // cost multiplier on scene complexity
    double scene_density_bonus_db; // PSNR bonus when occluder_density is high (rays more visible)
    bool requires_hw_rt;          // flag: needs HW RT (D, E)
    bool supports_dynamic_sun;    // flag: works with moving sun (time-of-day)
};

constexpr std::array<StrategyCost, 6> kStrategyCosts = {{
    // A_NoGodRays
    {0.000, 0.000,  0.00, 8.00, 0.00, 0.0, false, true},
    // B_ScreenSpaceRadialBlur (Mitchell 2007 + Crytek 2008: 3 passes × ~64 samples = 512 samples @ 1080p)
    {0.180, 0.075,  0.25, 13.50, 0.05, 6.0, false, true},
    // C_AnalyticOccludedRayMarch (Yusov 2014 epipolar: ~0.8 ms balanced @ 1280×720, scale to 1080p)
    {0.700, 0.260,  0.50, 14.50, 0.15, 5.0, false, true},
    // D_VolumetricConeTraceRayQuery (Lumen 2022 hybrid RTX: 1 ray/pixel @ Ampere, scene-bound)
    {0.550, 0.180, 12.00, 16.50, 0.45, 8.0, true,  true},
    // E_HybridRadialBlurPlusVolumetric (B + D combined cascade per Lumen 2022)
    {0.800, 0.260, 16.00, 17.50, 0.50, 9.0, true,  true},
    // F_PrecomputedSkydomeBaked (texture lookup, static-only)
    {0.050, 0.018,  2.00, 11.50, 0.00, 2.0, false, false},
}};

// =============================================================================
// Measurement computation (one iteration)
// =============================================================================

struct Measurement {
    double ms;
    double vram_mib;
    double psnr_db;
    double sun_visibility;
    double occluder_density;
};

Measurement ComputeOne(int strategy_idx, int scene_idx, int resolution_idx, std::mt19937& rng) {
    const auto& strat = kStrategyCosts[strategy_idx];
    const auto& scene = kSceneProps[scene_idx];

    const int W = kResolutionsW[resolution_idx];
    const int H = kResolutionsH[resolution_idx];
    const double megapixels = (W * H) / 1'000'000.0;

    // Cost: base + scaling + scene complexity adjustment
    std::normal_distribution<double> noise_dist(0.0, 0.03);  // 3% per-iter noise
    double scene_cost_mul = 1.0 + strat.scene_complexity_mul * scene.scene_complexity;
    double ms = (strat.base_ms_1080p + strat.ms_per_megapixel * megapixels) * scene_cost_mul
                * (1.0 + noise_dist(rng));

    // A_NoGodRays: strictly 0 ms
    if (strategy_idx == 0) ms = 0.0;

    // VRAM: constant per strategy (no per-frame alloc)
    std::normal_distribution<double> vram_noise(0.0, 0.01);
    double vram_mib = strat.vram_mib * (1.0 + vram_noise(rng));
    if (strategy_idx == 0) vram_mib = 0.0;

    // PSNR: base + density bonus + scene visibility factor
    std::normal_distribution<double> psnr_noise(0.0, 0.30);
    double density_bonus = strat.scene_density_bonus_db * scene.occluder_density;
    // Visibility factor: shafts require both occluders AND sun to be visible
    double visibility_factor = std::min(scene.sun_visibility * 2.0, 1.0); // boost when sun visible
    double psnr_db = 8.00 + (strat.base_psnr_db - 8.00) * visibility_factor + density_bonus
                     + psnr_noise(rng);
    if (strategy_idx == 0) psnr_db = 8.00;

    return {std::max(0.0, ms), std::max(0.0, vram_mib), std::max(8.0, psnr_db),
            scene.sun_visibility, scene.occluder_density};
}

// =============================================================================
// Statistics
// =============================================================================

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
};

Stats ComputeStats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.min = samples.front();
    s.max = samples.back();
    return s;
}

// =============================================================================
// Main experiment loop
// =============================================================================

int main() {
    const auto t_start = std::chrono::steady_clock::now();

    // Ensure build dir exists
    std::filesystem::create_directories("build");

    std::ofstream csv("build/results.csv");
    if (!csv) {
        std::fprintf(stderr, "FATAL: cannot open build/results.csv for writing\n");
        return 1;
    }
    csv << "strategy,scene,seed,resolution,mean_ms,median_ms,p95_ms,p99_ms,std_ms,"
           "mean_vram_mib,p95_vram_mib,mean_psnr_db,p95_psnr_db,sun_visibility,occluder_density\n";

    std::printf("God Rays / Crepuscular Rays — analytical cost model\n");
    std::printf("6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup\n");
    std::printf("Total configs: %d, total measurements: %d\n\n",
                6 * 5 * 5, 6 * 5 * 5 * kMainIters);

    // Per-resolution index per scene (use resolution 0 = 1080p for primary sweep)
    const int res_idx = 0;

    int total_configs = 0;
    int total_iters = 0;
    for (int s_strat = 0; s_strat < 6; ++s_strat) {
        for (int s_scene = 0; s_scene < 5; ++s_scene) {
            for (int s_seed_idx = 0; s_seed_idx < kSeedCount; ++s_seed_idx) {
                const int seed = kSeeds[s_seed_idx];
                std::mt19937 rng(static_cast<uint32_t>(seed) + s_strat * 1000u + s_scene * 100u);

                // Warmup
                for (int i = 0; i < kWarmupIters; ++i) {
                    volatile auto m = ComputeOne(s_strat, s_scene, res_idx, rng);
                    (void)m;
                }

                // Main measurements
                std::vector<double> ms_samples;
                std::vector<double> vram_samples;
                std::vector<double> psnr_samples;
                ms_samples.reserve(kMainIters);
                vram_samples.reserve(kMainIters);
                psnr_samples.reserve(kMainIters);

                for (int i = 0; i < kMainIters; ++i) {
                    auto m = ComputeOne(s_strat, s_scene, res_idx, rng);
                    ms_samples.push_back(m.ms);
                    vram_samples.push_back(m.vram_mib);
                    psnr_samples.push_back(m.psnr_db);
                }
                total_iters += kMainIters;
                ++total_configs;

                auto ms_stats = ComputeStats(ms_samples);
                auto vram_stats = ComputeStats(vram_samples);
                auto psnr_stats = ComputeStats(psnr_samples);

                const auto& scene = kSceneProps[s_scene];
                csv << kStrategies[s_strat] << ","
                    << kScenes[s_scene] << ","
                    << seed << ","
                    << kResolutionsW[res_idx] << "x" << kResolutionsH[res_idx] << ","
                    << ms_stats.mean << "," << ms_stats.median << ","
                    << ms_stats.p95 << "," << ms_stats.p99 << "," << ms_stats.stddev << ","
                    << vram_stats.mean << "," << vram_stats.p95 << ","
                    << psnr_stats.mean << "," << psnr_stats.p95 << ","
                    << scene.sun_visibility << "," << scene.occluder_density << "\n";
            }
        }
        std::printf("  [%d/6] %s done.\n", s_strat + 1, kStrategies[s_strat]);
    }

    csv.close();
    const auto t_end = std::chrono::steady_clock::now();
    const double wall_sec = std::chrono::duration<double>(t_end - t_start).count();

    std::printf("\n=== Summary ===\n");
    std::printf("Total configs: %d\n", total_configs);
    std::printf("Total measurements: %d\n", total_iters);
    std::printf("Wall time: %.3f sec\n", wall_sec);
    std::printf("Output: build/results.csv (%d rows)\n", total_configs + 1);
    return 0;
}