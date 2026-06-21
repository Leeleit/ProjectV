// SPDX-License-Identifier: MIT
// 2026-06-21-eye-tracked-foveated — standalone C++26 CPU foveation density map simulator.
//
// CPU-only synthetic voxel scene foveation cost model.
// NOT ProjectV mainline; standalone dev-host harness.
//
// Measures effective fragment count per viewport under different foveation strategies
// and compares against uniform 1x1 fragment shading baseline.
//
// Build: clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic foveation_sim.cpp -o foveation_sim
// Run:   ./foveation_sim 2>&1 | tee run.log
// Output: build/results.csv (75 rows × 12 cols)

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace foveation {

// ============================================================================
// Configuration
// ============================================================================

enum class Strategy : std::uint8_t {
    A_None          = 0,  // uniform 1x1 fragment density, baseline
    B_FixedFoveation2x,    // center 30% viewport 1x1, periphery 2x2 (no gaze input)
    C_GazeFoveation2x,     // gaze-driven foveal 1x1, mid 2x2, peripheral 4x4 (eye-tracked)
    D_GazeFoveation4x,     // gaze-driven aggressive — same as C but 4x4 max in periphery
};

constexpr std::string_view strategy_name(Strategy s) {
    switch (s) {
        case Strategy::A_None:           return "A_None";
        case Strategy::B_FixedFoveation2x: return "B_Fixed2x";
        case Strategy::C_GazeFoveation2x:  return "C_Gaze2x";
        case Strategy::D_GazeFoveation4x:  return "D_Gaze4x";
    }
    return "unknown";
}

enum class Scene : std::uint8_t {
    uniform_floor   = 0,
    forest_floor,
    cave_stress,
    mixed_biome,
    uniform_air,
};

constexpr std::string_view scene_name(Scene s) {
    switch (s) {
        case Scene::uniform_floor: return "uniform_floor";
        case Scene::forest_floor:  return "forest_floor";
        case Scene::cave_stress:   return "cave_stress";
        case Scene::mixed_biome:   return "mixed_biome";
        case Scene::uniform_air:   return "uniform_air";
    }
    return "unknown";
}

struct Extent {
    std::uint32_t width;
    std::uint32_t height;
};

constexpr std::array<Extent, 3> extents = {{
    {1920, 1080},  // 1080p
    {2560, 1440},  // 1440p
    {3840, 2160},  // 4K
}};

constexpr std::string_view extent_name(Extent e) {
    if (e.width == 1920) return "1080p";
    if (e.width == 2560) return "1440p";
    if (e.width == 3840) return "4K";
    return "unknown";
}

// ============================================================================
// Stats
// ============================================================================

struct Stats {
    double mean   = 0.0;
    double median = 0.0;
    double p95    = 0.0;
    double p99    = 0.0;
    double stddev = 0.0;
    double min    = 0.0;
    double max    = 0.0;
};

Stats compute_stats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean   = sum / static_cast<double>(samples.size());
    s.median = samples[samples.size() / 2];
    s.p95    = samples[static_cast<std::size_t>(samples.size() * 0.95)];
    s.p99    = samples[static_cast<std::size_t>(samples.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min = samples.front();
    s.max = samples.back();
    return s;
}

// ============================================================================
// Foveation density map
// ============================================================================
//
// Density values represent fragment shading rate:
//   1.0 = full density (1x1 shading, 1 fragment per pixel)
//   0.5 = 2x2 shading, 1 fragment per 4 pixels
//   0.25 = 4x4 shading, 1 fragment per 16 pixels
//
// Per `VK_KHR_fragment_shading_rate` Tier 2 attachment method:
//   - 1x1 density: every pixel gets a fragment invocation
//   - 2x2 density: 2x2 pixel blocks share one fragment (cost = 1/4 per pixel)
//   - 4x4 density: 4x4 pixel blocks share one fragment (cost = 1/16 per pixel)
//
// We discretize the viewport into "tiles" of fixed size (16x16 in our model;
// matches Vulkan min tile size on most hardware). Each tile gets a single density value.

constexpr std::uint32_t kTileSize = 16;

struct GazePoint {
    float x;  // [0,1] viewport normalized
    float y;
};

// Returns density map for the entire viewport (1 entry per tile).
std::vector<float> generate_density_map(Strategy strategy, GazePoint gaze,
                                         std::uint32_t tiles_x, std::uint32_t tiles_y) {
    std::vector<float> density(static_cast<std::size_t>(tiles_x) * tiles_y, 1.0f);

    switch (strategy) {
        case Strategy::A_None:
            // All 1.0 — uniform 1x1 (already initialized).
            break;

        case Strategy::B_FixedFoveation2x: {
            // Fixed: center 30% viewport 1x1, periphery 2x2 (no gaze).
            const float center_frac = 0.30f;
            const std::uint32_t cx = tiles_x / 2;
            const std::uint32_t cy = tiles_y / 2;
            const std::uint32_t half_w = static_cast<std::uint32_t>(
                static_cast<float>(tiles_x) * center_frac * 0.5f);
            const std::uint32_t half_h = static_cast<std::uint32_t>(
                static_cast<float>(tiles_y) * center_frac * 0.5f);
            for (std::uint32_t y = 0; y < tiles_y; ++y) {
                for (std::uint32_t x = 0; x < tiles_x; ++x) {
                    const std::size_t idx = static_cast<std::size_t>(y) * tiles_x + x;
                    if (x >= cx - half_w && x < cx + half_w &&
                        y >= cy - half_h && y < cy + half_h) {
                        density[idx] = 1.0f;   // foveal: full 1x1
                    } else {
                        density[idx] = 0.25f;  // peripheral: 2x2 (1/4 fragments)
                    }
                }
            }
            break;
        }

        case Strategy::C_GazeFoveation2x: {
            // Gaze-driven: foveal (5° radius) 1x1, mid (5-20°) 2x2, peripheral (>20°) 4x4.
            // For 90° FOV typical VR, 5° = ~11% viewport radius, 20° = ~44%.
            // Map tile coordinates to angular distance from gaze.
            const float foveal_frac = 0.10f;   // ~10% viewport radius = ~9° at 90° FOV
            const float mid_frac    = 0.35f;   // ~35% viewport radius = ~32° at 90° FOV
            const float gaze_x = gaze.x * static_cast<float>(tiles_x);
            const float gaze_y = gaze.y * static_cast<float>(tiles_y);
            for (std::uint32_t y = 0; y < tiles_y; ++y) {
                for (std::uint32_t x = 0; x < tiles_x; ++x) {
                    const std::size_t idx = static_cast<std::size_t>(y) * tiles_x + x;
                    const float dx = (static_cast<float>(x) + 0.5f - gaze_x) /
                                     static_cast<float>(tiles_x);
                    const float dy = (static_cast<float>(y) + 0.5f - gaze_y) /
                                     static_cast<float>(tiles_y);
                    const float r = std::sqrt(dx * dx + dy * dy);
                    if (r < foveal_frac) {
                        density[idx] = 1.0f;   // foveal: 1x1
                    } else if (r < mid_frac) {
                        density[idx] = 0.25f;  // mid: 2x2
                    } else {
                        density[idx] = 0.0625f; // peripheral: 4x4
                    }
                }
            }
            break;
        }

        case Strategy::D_GazeFoveation4x: {
            // Gaze-driven aggressive — foveal 1x1, mid 2x2, peripheral 4x4 (same as C above).
            // The "4x" in name denotes the maximum 4x4 reduction in periphery.
            // Algorithmically identical to C — kept as a separate enum for CSV clarity.
            const float foveal_frac = 0.10f;
            const float mid_frac    = 0.35f;
            const float gaze_x = gaze.x * static_cast<float>(tiles_x);
            const float gaze_y = gaze.y * static_cast<float>(tiles_y);
            for (std::uint32_t y = 0; y < tiles_y; ++y) {
                for (std::uint32_t x = 0; x < tiles_x; ++x) {
                    const std::size_t idx = static_cast<std::size_t>(y) * tiles_x + x;
                    const float dx = (static_cast<float>(x) + 0.5f - gaze_x) /
                                     static_cast<float>(tiles_x);
                    const float dy = (static_cast<float>(y) + 0.5f - gaze_y) /
                                     static_cast<float>(tiles_y);
                    const float r = std::sqrt(dx * dx + dy * dy);
                    if (r < foveal_frac) {
                        density[idx] = 1.0f;
                    } else if (r < mid_frac) {
                        density[idx] = 0.25f;
                    } else {
                        density[idx] = 0.0625f;
                    }
                }
            }
            break;
        }
    }

    return density;
}

// ============================================================================
// Effective fragment count
// ============================================================================
//
// For a density map d (1.0 = full 1x1, 0.25 = 2x2, 0.0625 = 4x4):
//   effective_fragments_per_tile = d * (tile_size_x * tile_size_y)
//   total_effective_fragments = sum over all tiles of effective_fragments_per_tile
//
// Baseline (A_None, d=1.0 everywhere):
//   baseline = tiles_x * tiles_y * (tile_size^2) = total_pixels
//
// Savings vs baseline = 1.0 - (effective / baseline)

double compute_effective_fragment_count(const std::vector<float>& density,
                                        std::uint32_t /*tiles_x*/, std::uint32_t /*tiles_y*/) {
    const double kTilePixels = static_cast<double>(kTileSize) * static_cast<double>(kTileSize);
    double total = 0.0;
    for (float d : density) {
        total += static_cast<double>(d) * kTilePixels;
    }
    return total;
}

// ============================================================================
// Gaze point generation (synthetic, scene-seeded)
// ============================================================================
//
// Real gaze input would come from `XR_EXT_eye_gaze_interaction` per
// OpenXR §12.31. For this prototype we synthesize a "natural" gaze pattern:
// gaze tends to center-of-screen for calm scenes, off-center for stress scenes.

GazePoint generate_synthetic_gaze(Scene scene, std::uint32_t seed) {
    std::mt19937 rng(seed);
    // Standard normal distribution for gaze offset from screen center.
    std::normal_distribution<double> nd(0.0, 0.20);

    GazePoint gaze{};
    switch (scene) {
        case Scene::uniform_floor:
        case Scene::uniform_air:
            // Uniform scenes: gaze centered, small drift.
            gaze.x = static_cast<float>(0.5 + nd(rng) * 0.05);
            gaze.y = static_cast<float>(0.5 + nd(rng) * 0.05);
            break;
        case Scene::forest_floor:
        case Scene::mixed_biome:
            // Biome scenes: gaze explores, medium offset.
            gaze.x = static_cast<float>(0.5 + nd(rng) * 0.10);
            gaze.y = static_cast<float>(0.5 + nd(rng) * 0.08);
            break;
        case Scene::cave_stress:
            // Cave: gaze often on walls/details, larger offset.
            gaze.x = static_cast<float>(0.5 + nd(rng) * 0.15);
            gaze.y = static_cast<float>(0.5 + nd(rng) * 0.12);
            break;
    }
    // Clamp to [0.05, 0.95] — never gaze exactly at edge.
    gaze.x = std::clamp(gaze.x, 0.05f, 0.95f);
    gaze.y = std::clamp(gaze.y, 0.05f, 0.95f);
    return gaze;
}

// ============================================================================
// Bandwidth / cost model (per-frame fragment shading cost)
// ============================================================================
//
// Analytical projection based on `vk-fragment-shading-rate-voxel` baseline
// measurements (closed mixed): Stage 5.1 VCT cone-march + Stage 5.2 RTX shadow
// + TAA resolve are fragment-bound on RTX 3060 Ti, per-fragment cost ~14% of
// dispatch time + memory-bound 65.6% of 448 GB/s peak bandwidth.
//
// Model: total cost = effective_fragments * per_fragment_cost
// Per-fragment cost normalized so that baseline (A_None, full density) = 1.0.
// Savings vs baseline = 1 - effective_fragments/total_pixels.

struct FrameCostEstimate {
    double effective_fragments;  // per viewport
    double total_pixels;
    double cost_ratio;          // vs baseline (A_None = 1.0)
    double savings_pct;         // (1 - cost_ratio) * 100
};

FrameCostEstimate estimate_frame_cost(Strategy strategy, GazePoint gaze, Extent extent) {
    const std::uint32_t tiles_x = (extent.width  + kTileSize - 1) / kTileSize;
    const std::uint32_t tiles_y = (extent.height + kTileSize - 1) / kTileSize;
    auto density = generate_density_map(strategy, gaze, tiles_x, tiles_y);
    const double effective = compute_effective_fragment_count(density, tiles_x, tiles_y);
    const double total = static_cast<double>(extent.width) * static_cast<double>(extent.height);
    FrameCostEstimate r{};
    r.effective_fragments = effective;
    r.total_pixels        = total;
    r.cost_ratio          = effective / total;
    r.savings_pct         = (1.0 - r.cost_ratio) * 100.0;
    return r;
}

// ============================================================================
// Per-config measurement
// ============================================================================

struct ConfigResult {
    Strategy strategy;
    Scene scene;
    std::uint32_t seed;
    Extent extent;
    Stats cost_ratio;     // measured across N iterations with different synthesized gaze
    Stats savings_pct;
    double effective_fragments_mean;
    double total_pixels;
    std::uint32_t iterations;
};

ConfigResult measure_config(Strategy strategy, Scene scene, std::uint32_t seed,
                             Extent extent, std::uint32_t iterations, std::uint32_t warmup) {
    std::vector<double> cost_ratios;
    std::vector<double> savings_pcts;
    cost_ratios.reserve(iterations);
    savings_pcts.reserve(iterations);

    std::uint32_t tiles_x = (extent.width  + kTileSize - 1) / kTileSize;
    std::uint32_t tiles_y = (extent.height + kTileSize - 1) / kTileSize;
    double total = static_cast<double>(extent.width) * static_cast<double>(extent.height);

    for (std::uint32_t i = 0; i < iterations + warmup; ++i) {
        // Per-iteration gaze (synthetic, sub-seeded for reproducibility).
        GazePoint gaze = generate_synthetic_gaze(scene, seed * 1000u + i);
        auto density = generate_density_map(strategy, gaze, tiles_x, tiles_y);
        double eff = compute_effective_fragment_count(density, tiles_x, tiles_y);
        double ratio = eff / total;
        double savings = (1.0 - ratio) * 100.0;
        if (i >= warmup) {
            cost_ratios.push_back(ratio);
            savings_pcts.push_back(savings);
        }
    }

    ConfigResult r{};
    r.strategy     = strategy;
    r.scene        = scene;
    r.seed         = seed;
    r.extent       = extent;
    r.cost_ratio   = compute_stats(cost_ratios);
    r.savings_pct  = compute_stats(savings_pcts);
    r.effective_fragments_mean = r.cost_ratio.mean * total;
    r.total_pixels  = total;
    r.iterations    = iterations;
    return r;
}

// ============================================================================
// CSV output
// ============================================================================

void write_csv_header(std::ofstream& f) {
    f << "strategy,scene,seed,extent,tiles_x,tiles_y,total_pixels,"
         "iterations,warmup,"
         "cost_ratio_mean,cost_ratio_median,cost_ratio_p95,cost_ratio_p99,cost_ratio_stddev,"
         "savings_pct_mean,savings_pct_median,savings_pct_p95,savings_pct_p99,savings_pct_stddev,"
         "effective_fragments_mean,wall_time_us\n";
}

void write_csv_row(std::ofstream& f, const ConfigResult& r, std::uint32_t tiles_x,
                    std::uint32_t tiles_y, long long wall_time_us) {
    f << strategy_name(r.strategy) << ','
      << scene_name(r.scene) << ','
      << r.seed << ','
      << extent_name(r.extent) << ','
      << tiles_x << ',' << tiles_y << ','
      << static_cast<std::uint64_t>(r.total_pixels) << ','
      << r.iterations << ",10,"
      << r.cost_ratio.mean  << ',' << r.cost_ratio.median << ','
      << r.cost_ratio.p95   << ',' << r.cost_ratio.p99    << ','
      << r.cost_ratio.stddev << ','
      << r.savings_pct.mean << ',' << r.savings_pct.median << ','
      << r.savings_pct.p95  << ',' << r.savings_pct.p99    << ','
      << r.savings_pct.stddev << ','
      << r.effective_fragments_mean << ','
      << wall_time_us << '\n';
}

}  // namespace foveation

// ============================================================================
// Main
// ============================================================================

int main() {
    using namespace foveation;

    constexpr std::uint32_t kIterations = 1000;  // per `benchmarks/methodology.md §3`
    constexpr std::uint32_t kWarmup     = 10;

    const std::array<Strategy, 4> strategies = {
        Strategy::A_None,
        Strategy::B_FixedFoveation2x,
        Strategy::C_GazeFoveation2x,
        Strategy::D_GazeFoveation4x,
    };

    const std::array<Scene, 5> scenes = {
        Scene::uniform_floor,
        Scene::forest_floor,
        Scene::cave_stress,
        Scene::mixed_biome,
        Scene::uniform_air,
    };

    const std::array<std::uint32_t, 5> seeds = {1, 7, 42, 1234, 31337};

    std::filesystem::create_directory("build");

    std::ofstream csv("build/results.csv");
    write_csv_header(csv);

    std::printf("2026-06-21-eye-tracked-foveated — standalone CPU foveation simulator\n");
    std::printf("Strategies=%zu Scenes=%zu Seeds=%zu Extents=%zu Iterations=%u Warmup=%u\n",
                strategies.size(), scenes.size(), seeds.size(),
                extents.size(), kIterations, kWarmup);
    std::printf("Total configs: %zu (= 4 strategies × 5 scenes × 5 seeds × 3 extents)\n",
                strategies.size() * scenes.size() * seeds.size() * extents.size());
    std::printf("Total measurements: %zu (= %zu configs × %u iters)\n",
                strategies.size() * scenes.size() * seeds.size() * extents.size(),
                strategies.size() * scenes.size() * seeds.size() * extents.size(),
                kIterations);
    std::printf("Tile size: %u×%u (matches Vulkan min tile size on most HW)\n\n",
                kTileSize, kTileSize);

    std::size_t total_configs = 0;
    double total_wall_time_us = 0.0;

    auto overall_start = std::chrono::steady_clock::now();

    for (auto strategy : strategies) {
        for (auto scene : scenes) {
            for (auto seed : seeds) {
                for (auto extent : extents) {
                    auto t0 = std::chrono::steady_clock::now();
                    ConfigResult r = measure_config(strategy, scene, seed, extent,
                                                     kIterations, kWarmup);
                    auto t1 = std::chrono::steady_clock::now();
                    auto wall_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
                    total_wall_time_us += static_cast<double>(wall_us);

                    const std::uint32_t tiles_x = (extent.width  + kTileSize - 1) / kTileSize;
                    const std::uint32_t tiles_y = (extent.height + kTileSize - 1) / kTileSize;
                    write_csv_row(csv, r, tiles_x, tiles_y, wall_us);

                    total_configs++;

                    // Compact progress line per config.
                    std::printf("[%3zu] %-15s %-15s seed=%-6u %-6s cost_ratio=%.4f savings=%6.2f%% (%ld us)\n",
                                total_configs, std::string(strategy_name(strategy)).c_str(),
                                std::string(scene_name(scene)).c_str(), seed,
                                std::string(extent_name(extent)).c_str(),
                                r.cost_ratio.mean, r.savings_pct.mean, static_cast<long>(wall_us));
                }
            }
        }
    }

    auto overall_end = std::chrono::steady_clock::now();
    double total_wall_time_s = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 overall_end - overall_start).count() / 1000.0;

    csv.close();

    std::printf("\n========================================\n");
    std::printf("Done: %zu configs x %u iters = %zu measurements\n",
                total_configs, kIterations, total_configs * kIterations);
    std::printf("Wall time: %.3f s (%.1f us avg/config)\n",
                total_wall_time_s, total_wall_time_us / static_cast<double>(total_configs));
    std::printf("Output: build/results.csv (%zu rows × 23 cols)\n",
                total_configs + 1);
    std::printf("========================================\n");

    return 0;
}
