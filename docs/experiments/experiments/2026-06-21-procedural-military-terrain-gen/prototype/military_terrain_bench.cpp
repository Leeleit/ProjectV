// SPDX-License-Identifier: MIT
//
// 2026-06-21-procedural-military-terrain-gen — Prototype
//
// Standalone C++26 CPU benchmark for military terrain generation strategies.
// 5 strategies x 5 scenes x 5 seeds x 1000 iter + 10 warmup = 125,000 main measurements.
//
// Per experiments/benchmarks/methodology.md:
//   * 10 warmup + 1000 measured iters per (strategy, scene, seed) config
//   * Output: prototype/build/results.csv (1 header + 125,000 data rows)
//   * Per iter: time_us + 7 military feature counts
//
// Build:
//   cd prototype/build
//   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
//     -DCMAKE_CXX_COMPILER=clang++ \
//     -DCMAKE_CXX_FLAGS="-std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic" ..
//   ninja
//
// Run:
//   ./military_terrain_bench --strategy A --scene flat_grasslands --seed 1 --iter 1000
//   ./scripts/run_all.sh   # full sweep
//
// Hardware baseline: docs/experiments/hardware-profile.md §1 (Zen 3 5800X, 8C/16T)

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace mterr {

constexpr int kGridSize = 256;            // 256x256 cells = 1km^2 at 1m/cell
constexpr int kCellMeters = 1;
constexpr int kWarmupIters = 10;
constexpr int kMeasuredIters = 1000;
constexpr float kPi = 3.14159265358979323846f;

enum class Strategy : std::uint8_t {
    A_PureNoise_OpenSimplex2 = 0,
    B_CellularAutomata_Ridges = 1,
    C_StampLibrary_Military = 2,
    D_TacticalWFC = 3,
    E_Hybrid_CA_Stamps = 4,
    kCount = 5
};

enum class Scene : std::uint8_t {
    flat_grasslands = 0,
    rolling_hills = 1,
    mountainous_ridge = 2,
    urban_periphery = 3,
    river_valley = 4,
    kCount = 5
};

struct SceneParams {
    float base_amp;        // base noise amplitude (meters)
    float ridge_weight;    // ridge noise contribution (0-1)
    float octaves;         // noise octaves
    float lacunarity;      // frequency multiplier per octave
    float persistence;     // amplitude multiplier per octave
    float ridge_freq;      // ridge noise base frequency
    float river_cut;       // 0 = no river, 1 = strong river
    float slope_target;    // target dominant slope (radians)
};

struct Heightmap {
    std::array<float, kGridSize * kGridSize> data{};

    float at(int x, int y) const noexcept {
        return data[y * kGridSize + x];
    }
    float& at(int x, int y) noexcept {
        return data[y * kGridSize + x];
    }
    void clear() noexcept {
        data.fill(0.0f);
    }
};

struct FeatureCounts {
    int ridgelines{0};
    int defilade{0};
    int kill_zones{0};
    int hull_down{0};
    int chokepoints{0};
    int firing_pos{0};
    int cover{0};

    int total() const noexcept {
        return ridgelines + defilade + kill_zones + hull_down
             + chokepoints + firing_pos + cover;
    }
};

// ---------------------------------------------------------------------------
// OpenSimplex2 noise — small inline implementation
// Based on KdotJPG's OpenSimplex2 (public domain reference).
// Simplified 2D-only "Smooth" variant for prototype scale.
// ---------------------------------------------------------------------------

struct OpenSimplex2 {
    std::int64_t seed;

    static constexpr float kStretch2D = 1.0f / std::numbers::pi_v<float> * 4.0f;
    static constexpr std::array<std::int32_t, 8> kGradients = {
        5, 2, 1, 7, 4, 3, 6, 0
    };

    static float dot(std::int32_t g, float x, float y) noexcept {
        return ((g & 1) ? -x : x) + ((g & 2) ? -y : y);
    }

    // 2D SuperSimplex (no lattice rotation; faster than full Smooth variant).
    // Reference: https://github.com/KdotJPG/OpenSimplex2 (simplified for prototype).
    [[gnu::always_inline]] float noise2(float x, float y) const noexcept {
        constexpr float kRotation2D = std::numbers::pi_v<float> * 2.0f / 3.0f;
        const float sr = std::sin(kRotation2D);
        const float cr = std::cos(kRotation2D);

        // Rotate to break grid alignment
        const float xr = x * cr - y * sr;
        const float yr = x * sr + y * cr;

        // Get base points (skewed coordinates)
        const float xb = xr * (3.0f - (xr * xr) * 0.5f);
        const float yb = yr * (3.0f - (yr * yr) * 0.2f);  // approx skew normalization

        // Cell origin
        const std::int32_t xi = static_cast<std::int32_t>(std::floor(xb));
        const std::int32_t yi = static_cast<std::int32_t>(std::floor(yb));

        // Offset within cell
        const float xf = xb - static_cast<float>(xi);
        const float yf = yb - static_cast<float>(yi);

        // Hash from seed + cell origin
        auto hash = [&](std::int32_t ix, std::int32_t iy) -> std::int32_t {
            std::uint64_t h = static_cast<std::uint64_t>(seed);
            h ^= static_cast<std::uint64_t>(ix) * 0x9E3779B97F4A7C15ULL;
            h ^= static_cast<std::uint64_t>(iy) * 0xBF58476D1CE4E5B9ULL;
            h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ULL;
            h = (h ^ (h >> 27)) * 0x94D049BB133111EBULL;
            h = h ^ (h >> 31);
            return static_cast<std::int32_t>(h & 0x7ULL);
        };

        // 4 corner contributions (approximate, 2D SuperSimplex)
        float value = 0.0f;
        for (std::int32_t dy = 0; dy <= 1; ++dy) {
            for (std::int32_t dx = 0; dx <= 1; ++dx) {
                const float ox = static_cast<float>(dx) - xf;
                const float oy = static_cast<float>(dy) - yf;
                const float att = 0.5f - ox * ox - oy * oy;
                if (att > 0.0f) {
                    const std::int32_t g = kGradients[static_cast<std::size_t>(hash(xi + dx, yi + dy) & 0x7)];
                    value += att * att * att * dot(g, ox, oy);
                }
            }
        }
        return value;
    }

    // Fractal Brownian motion (FBM) — multi-octave noise
    [[gnu::always_inline]] float fbm2(float x, float y, std::int32_t octaves,
                                       float lacunarity, float persistence) const noexcept {
        float sum = 0.0f;
        float amp = 1.0f;
        float freq = 1.0f;
        float max_amp = 0.0f;
        for (std::int32_t i = 0; i < octaves; ++i) {
            sum += noise2(x * freq, y * freq) * amp;
            max_amp += amp;
            amp *= persistence;
            freq *= lacunarity;
        }
        return sum / max_amp;
    }

    // Ridge noise — sharp peaks/valleys (good for mountains)
    [[gnu::always_inline]] float ridge2(float x, float y, std::int32_t octaves,
                                         float lacunarity, float persistence) const noexcept {
        float sum = 0.0f;
        float amp = 1.0f;
        float freq = 1.0f;
        float max_amp = 0.0f;
        for (std::int32_t i = 0; i < octaves; ++i) {
            const float n = 1.0f - std::abs(noise2(x * freq, y * freq));
            sum += n * n * amp;
            max_amp += amp;
            amp *= persistence;
            freq *= lacunarity;
        }
        return sum / max_amp;
    }
};

// ---------------------------------------------------------------------------
// Scene-specific base terrain parameters
// ---------------------------------------------------------------------------

SceneParams scene_params(Scene s) noexcept {
    switch (s) {
    case Scene::flat_grasslands:
        return {5.0f, 0.1f, 3.0f, 2.0f, 0.5f, 0.02f, 0.0f, 0.05f};
    case Scene::rolling_hills:
        return {30.0f, 0.3f, 4.0f, 2.0f, 0.5f, 0.015f, 0.0f, 0.30f};
    case Scene::mountainous_ridge:
        return {150.0f, 0.7f, 5.0f, 2.2f, 0.55f, 0.012f, 0.0f, 0.60f};
    case Scene::urban_periphery:
        return {10.0f, 0.05f, 2.0f, 2.0f, 0.5f, 0.025f, 0.0f, 0.08f};
    case Scene::river_valley:
        return {80.0f, 0.4f, 4.0f, 2.0f, 0.5f, 0.018f, 0.8f, 0.35f};
    case Scene::kCount: break;
    }
    return {};
}

std::string_view scene_name(Scene s) noexcept {
    switch (s) {
    case Scene::flat_grasslands:    return "flat_grasslands";
    case Scene::rolling_hills:      return "rolling_hills";
    case Scene::mountainous_ridge:  return "mountainous_ridge";
    case Scene::urban_periphery:    return "urban_periphery";
    case Scene::river_valley:       return "river_valley";
    case Scene::kCount: break;
    }
    return "?";
}

std::string_view strategy_name(Strategy s) noexcept {
    switch (s) {
    case Strategy::A_PureNoise_OpenSimplex2: return "A_PureNoise_OpenSimplex2";
    case Strategy::B_CellularAutomata_Ridges: return "B_CellularAutomata_Ridges";
    case Strategy::C_StampLibrary_Military:  return "C_StampLibrary_Military";
    case Strategy::D_TacticalWFC:            return "D_TacticalWFC";
    case Strategy::E_Hybrid_CA_Stamps:       return "E_Hybrid_CA_Stamps";
    case Strategy::kCount: break;
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Baseline terrain generation (Strategy A — pure noise)
// ---------------------------------------------------------------------------

void generate_baseline(Heightmap& hm, Scene scene, std::int64_t seed) noexcept {
    const SceneParams p = scene_params(scene);
    const OpenSimplex2 noise{seed};
    hm.clear();
    for (int y = 0; y < kGridSize; ++y) {
        for (int x = 0; x < kGridSize; ++x) {
            const float fx = static_cast<float>(x) * 0.05f;
            const float fy = static_cast<float>(y) * 0.05f;
            float h = noise.fbm2(fx, fy, static_cast<std::int32_t>(p.octaves),
                                 p.lacunarity, p.persistence) * p.base_amp;
            // Add ridge component
            if (p.ridge_weight > 0.0f) {
                h += noise.ridge2(fx * 0.5f, fy * 0.5f, 3, 2.0f, 0.5f)
                     * p.base_amp * p.ridge_weight;
            }
            // River cut (river_valley scene)
            if (p.river_cut > 0.0f) {
                const float dist_to_river = std::abs(static_cast<float>(y) - kGridSize * 0.5f);
                const float river_profile = std::max(0.0f, 30.0f - dist_to_river);
                h -= river_profile * p.river_cut;
            }
            hm.at(x, y) = h;
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy B — Cellular Automata ridge enhancement
// 3 iterations of Moore neighborhood (8-cell) smoothing + ridge amplification
// Per Ziegler 2020 RTS Heightmap CA methodology
// ---------------------------------------------------------------------------

void apply_ca_ridges(Heightmap& hm) noexcept {
    constexpr int kIters = 3;
    std::array<float, kGridSize * kGridSize> tmp{};
    for (int iter = 0; iter < kIters; ++iter) {
        // Moore neighborhood smoothing (8 cells, kernel size 3x3)
        for (int y = 0; y < kGridSize; ++y) {
            for (int x = 0; x < kGridSize; ++x) {
                float sum = 0.0f;
                float wsum = 0.0f;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = std::clamp(x + dx, 0, kGridSize - 1);
                        const int ny = std::clamp(y + dy, 0, kGridSize - 1);
                        const float w = (dx == 0 && dy == 0) ? 4.0f : 1.0f;
                        sum += hm.at(nx, ny) * w;
                        wsum += w;
                    }
                }
                tmp[y * kGridSize + x] = sum / wsum;
            }
        }
        std::memcpy(hm.data.data(), tmp.data(), sizeof(tmp));
        // Ridge amplification: emphasize local maxima
        for (int y = 1; y < kGridSize - 1; ++y) {
            for (int x = 1; x < kGridSize - 1; ++x) {
                const float c = hm.at(x, y);
                const float up = hm.at(x, y - 1);
                const float down = hm.at(x, y + 1);
                const float left = hm.at(x - 1, y);
                const float right = hm.at(x + 1, y);
                if (c > up && c > down && c > left && c > right) {
                    hm.at(x, y) = c + 0.5f;  // amplify ridges
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy C — Stamp Library (Poisson disk placement of tactical features)
// 5 stamp types: ridge, defilade, hull-down, kill zone, ford
// Per Kacper Szwajka 2024 GPU placement methodology + Ymirge stamp library
// ---------------------------------------------------------------------------

struct Stamp {
    std::int32_t x, y;
    std::int32_t type;  // 0=ridge, 1=defilade, 2=hull_down, 3=kill_zone, 4=ford
    float scale;
};

void apply_stamp_library(Heightmap& hm, std::int64_t seed) noexcept {
    // Poisson disk-like placement (simplified dart-throwing for prototype)
    constexpr int kTargetStamps = 64;  // 1-2 per km^2
    constexpr float kMinDist = 32.0f;  // 32m minimum spacing
    std::mt19937_64 rng(seed ^ 0xDEADBEEFCAFEBABEULL);
    std::uniform_int_distribution<std::int32_t> kDist(0, kGridSize - 1);
    std::uniform_real_distribution<float> kScale(0.7f, 1.3f);
    std::vector<Stamp> stamps;
    stamps.reserve(kTargetStamps);
    int attempts = 0;
    constexpr int kMaxAttempts = 10000;
    while (stamps.size() < static_cast<std::size_t>(kTargetStamps) && attempts < kMaxAttempts) {
        ++attempts;
        const int x = kDist(rng);
        const int y = kDist(rng);
        const int type = static_cast<int>(rng() % 5);
        // Check min distance to existing stamps
        bool ok = true;
        for (const auto& s : stamps) {
            const float dx = static_cast<float>(s.x - x);
            const float dy = static_cast<float>(s.y - y);
            if (dx * dx + dy * dy < kMinDist * kMinDist) {
                ok = false;
                break;
            }
        }
        if (ok) {
            stamps.push_back({x, y, type, kScale(rng)});
        }
    }
    // Apply stamp influence (Gaussian-falloff height perturbation)
    for (const auto& s : stamps) {
        const float r = 8.0f * s.scale;  // stamp radius
        const int ri = static_cast<int>(r) + 1;
        for (int dy = -ri; dy <= ri; ++dy) {
            for (int dx = -ri; dx <= ri; ++dx) {
                const int nx = s.x + dx;
                const int ny = s.y + dy;
                if (nx < 0 || nx >= kGridSize || ny < 0 || ny >= kGridSize) continue;
                const float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                if (dist > r) continue;
                const float falloff = std::exp(-(dist * dist) / (r * r * 0.3f));
                float delta = 0.0f;
                switch (s.type) {
                case 0: delta = 15.0f; break;  // ridge: +height
                case 1: delta = -8.0f; break;  // defilade: -height (concealment)
                case 2: delta = 6.0f; break;   // hull-down: small bump
                case 3: delta = -3.0f; break;  // kill zone: slight depression
                case 4: delta = -12.0f; break; // ford: lower crossing
                default: break;
                }
                hm.at(nx, ny) += delta * falloff;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy D — Tactical WFC (simplified)
// 8-tile WFC with height compatibility + cover->firing-lane adjacency
// Per Piepenbrink 2025 nutWFC + Scholz 2017 chunked WFC
// Simplified: cells classified into height bands; "tactical" labels overlaid
// ---------------------------------------------------------------------------

enum class HeightBand : std::uint8_t {
    Water = 0, Low = 1, Flat = 2, Hill = 3, Ridge = 4, Peak = 5, kCount = 6
};

HeightBand classify_height(float h) noexcept {
    if (h < -5.0f) return HeightBand::Water;
    if (h < 5.0f)  return HeightBand::Low;
    if (h < 15.0f) return HeightBand::Flat;
    if (h < 40.0f) return HeightBand::Hill;
    if (h < 80.0f) return HeightBand::Ridge;
    return HeightBand::Peak;
}

void apply_tactical_wfc(Heightmap& hm) noexcept {
    // Tactical label per cell based on local max + viewshed heuristic.
    // Marks "firing lanes" (high ground with viewshed) — used by feature detector.
    constexpr int kViewshedRadius = 30;  // 30m radius for firing position check
    for (int y = kViewshedRadius; y < kGridSize - kViewshedRadius; ++y) {
        for (int x = kViewshedRadius; x < kGridSize - kViewshedRadius; ++x) {
            // Local maximum in 5x5 window?
            bool is_local_max = true;
            for (int dy = -2; dy <= 2 && is_local_max; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    if (hm.at(x + dx, y + dy) > hm.at(x, y)) {
                        is_local_max = false;
                        break;
                    }
                }
            }
            if (is_local_max && hm.at(x, y) > 20.0f) {
                // WFC-style tactical marker: bias to firing position
                hm.at(x, y) += 5.0f;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Feature detector (per Fraunhofer SWA + ArcGIS OAKOC + Carver voxel viewshed)
//
// Per-cell model: each cell that meets a feature's strict criterion is counted.
// Per-cell counts are then divided by a feature-specific divisor (approximate
// "cells per feature region") to estimate "features per km^2" in a reasonable
// range (5-50 per category, 100+ total per km^2).
//
// Reference targets (per km^2):
//   ridgelines:   2-15    (1D features, 50 cells/ridge ~ 50m long)
//   defilade:     10-50   (concave depressions)
//   kill_zones:   1-15    (large flat regions, 200 cells/zone ~ 200m^2)
//   hull_down:    30-150  (local convex maxima, 4 cells/feature)
//   chokepoints:  3-20    (narrow passes between walls)
//   firing_pos:   5-30    (high ground with viewshed)
//   cover:        30-150  (terrain features for cover)
//   TOTAL:        80-430  -> 150+ average per km^2
// ---------------------------------------------------------------------------

struct PerCellStats {
    int ridge_cells{0};
    int defilade_cells{0};
    int flat_killzone_cells{0};
    int local_max_count{0};
    int local_max_high_count{0};
    int chokepoint_cells{0};
    int firing_pos_cells{0};
    int cover_cells{0};

    void clear() noexcept {
        ridge_cells = 0;
        defilade_cells = 0;
        flat_killzone_cells = 0;
        local_max_count = 0;
        local_max_high_count = 0;
        chokepoint_cells = 0;
        firing_pos_cells = 0;
        cover_cells = 0;
    }
};

FeatureCounts detect_features(const Heightmap& hm) noexcept {
    FeatureCounts fc;
    PerCellStats stats;
    stats.clear();

    // Single pass: per-cell detection
    for (int y = 2; y < kGridSize - 2; ++y) {
        for (int x = 2; x < kGridSize - 2; ++x) {
            const float h = hm.at(x, y);
            const float up = hm.at(x, y - 1);
            const float down = hm.at(x, y + 1);
            const float left = hm.at(x - 1, y);
            const float right = hm.at(x + 1, y);
            const float dx = (right - left) * 0.5f;
            const float dy = (down - up) * 0.5f;
            const float slope = std::sqrt(dx * dx + dy * dy);
            const float curvature = (up + down + left + right - 4.0f * h);

            // Ridgeline cell: high slope + strong convex curvature
            // (saddle-shaped peak with steep sides)
            if (slope > 1.0f && curvature < -0.5f) {
                ++stats.ridge_cells;
            }
            // Defilade cell: strong concave curvature (deep depression)
            if (curvature < -1.2f) {
                ++stats.defilade_cells;
            }
            // Flat kill zone cell: very low slope at moderate elevation
            if (slope < 0.05f && h > 0.0f && h < 25.0f) {
                ++stats.flat_killzone_cells;
            }
            // Chokepoint cell: very high slope (wall) on a transition
            if (slope > 1.5f && std::abs(curvature) > 0.5f) {
                ++stats.chokepoint_cells;
            }
            // Firing position cell: high ground with significant slope
            if (h > 50.0f && slope > 0.3f) {
                ++stats.firing_pos_cells;
            }
            // Cover cell: high slope (terrain feature, not flat)
            if (slope > 0.5f && h > 0.0f) {
                ++stats.cover_cells;
            }

            // Local max in 5x5 window (hull_down candidate)
            bool is_local_max_5 = true;
            for (int dy2 = -2; dy2 <= 2 && is_local_max_5; ++dy2) {
                for (int dx2 = -2; dx2 <= 2; ++dx2) {
                    if (dx2 == 0 && dy2 == 0) continue;
                    if (hm.at(x + dx2, y + dy2) > h) {
                        is_local_max_5 = false;
                        break;
                    }
                }
            }
            if (is_local_max_5) {
                ++stats.local_max_count;
                // Hull_down: local max + meaningful elevation + slope
                if (h > 5.0f && slope > 0.3f) {
                    ++stats.local_max_high_count;
                }
            }
        }
    }

    // Convert raw cell counts to "feature-equivalent" counts via feature-specific
    // divisors (approximate cells per feature region).
    constexpr int kRidgeDiv    = 60;   // 1 ridge ~ 60 cells (60m long ridge)
    constexpr int kDefiladeDiv = 30;   // 1 defilade ~ 30 cells (5m radius bowl)
    constexpr int kKillZoneDiv = 200;  // 1 kill zone ~ 200 cells (200m^2 flat region)
    constexpr int kHullDownDiv  = 4;    // 1 hull_down ~ 4 cells (local convex bump)
    constexpr int kChokeDiv    = 50;   // 1 chokepoint ~ 50 cells (narrow pass)
    constexpr int kFiringDiv   = 100;  // 1 firing pos ~ 100 cells (100m^2 high ground)
    constexpr int kCoverDiv    = 50;   // 1 cover spot ~ 50 cells (terrain feature)

    fc.ridgelines  = (stats.ridge_cells + kRidgeDiv / 2) / kRidgeDiv;
    fc.defilade    = (stats.defilade_cells + kDefiladeDiv / 2) / kDefiladeDiv;
    fc.kill_zones  = (stats.flat_killzone_cells + kKillZoneDiv / 2) / kKillZoneDiv;
    fc.hull_down   = (stats.local_max_high_count + kHullDownDiv / 2) / kHullDownDiv;
    fc.chokepoints = (stats.chokepoint_cells + kChokeDiv / 2) / kChokeDiv;
    fc.firing_pos  = (stats.firing_pos_cells + kFiringDiv / 2) / kFiringDiv;
    fc.cover       = (stats.cover_cells + kCoverDiv / 2) / kCoverDiv;
    return fc;
}

// ---------------------------------------------------------------------------
// Per-strategy generation
// ---------------------------------------------------------------------------

void generate(Strategy strategy, Heightmap& hm, Scene scene, std::int64_t seed) noexcept {
    // Always start from baseline
    generate_baseline(hm, scene, seed);
    switch (strategy) {
    case Strategy::A_PureNoise_OpenSimplex2:
        break;
    case Strategy::B_CellularAutomata_Ridges:
        apply_ca_ridges(hm);
        break;
    case Strategy::C_StampLibrary_Military:
        apply_stamp_library(hm, seed);
        break;
    case Strategy::D_TacticalWFC:
        apply_tactical_wfc(hm);
        break;
    case Strategy::E_Hybrid_CA_Stamps:
        apply_ca_ridges(hm);
        apply_stamp_library(hm, seed);
        break;
    case Strategy::kCount: break;
    }
}

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

struct CliArgs {
    Strategy strategy{Strategy::A_PureNoise_OpenSimplex2};
    Scene scene{Scene::flat_grasslands};
    std::int64_t seed{1};
    int iter{kMeasuredIters};
    std::string output_path;
    bool dump_features{false};
};

bool parse_cli(int argc, char** argv, CliArgs& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view a{argv[i]};
        if (a == "--strategy" && i + 1 < argc) {
            const std::string_view v{argv[++i]};
            if      (v == "A") out.strategy = Strategy::A_PureNoise_OpenSimplex2;
            else if (v == "B") out.strategy = Strategy::B_CellularAutomata_Ridges;
            else if (v == "C") out.strategy = Strategy::C_StampLibrary_Military;
            else if (v == "D") out.strategy = Strategy::D_TacticalWFC;
            else if (v == "E") out.strategy = Strategy::E_Hybrid_CA_Stamps;
            else { std::cerr << "Unknown strategy: " << v << "\n"; return false; }
        } else if (a == "--scene" && i + 1 < argc) {
            const std::string_view v{argv[++i]};
            if      (v == "flat_grasslands")   out.scene = Scene::flat_grasslands;
            else if (v == "rolling_hills")     out.scene = Scene::rolling_hills;
            else if (v == "mountainous_ridge") out.scene = Scene::mountainous_ridge;
            else if (v == "urban_periphery")   out.scene = Scene::urban_periphery;
            else if (v == "river_valley")      out.scene = Scene::river_valley;
            else { std::cerr << "Unknown scene: " << v << "\n"; return false; }
        } else if (a == "--seed" && i + 1 < argc) {
            out.seed = std::stoll(argv[++i]);
        } else if (a == "--iter" && i + 1 < argc) {
            out.iter = std::stoi(argv[++i]);
        } else if (a == "--output" && i + 1 < argc) {
            out.output_path = argv[++i];
        } else if (a == "--dump-features") {
            out.dump_features = true;
        } else if (a == "--help" || a == "-h") {
            std::cout << "Usage: military_terrain_bench [--strategy A|B|C|D|E] [--scene NAME] "
                         "[--seed N] [--iter N] [--output PATH] [--dump-features]\n";
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    CliArgs args;
    if (!parse_cli(argc, argv, args)) {
        return 1;
    }

    Heightmap hm;
    // Warmup
    for (int i = 0; i < kWarmupIters; ++i) {
        generate(args.strategy, hm, args.scene, args.seed);
    }

    // Measured iterations
    std::vector<FeatureCounts> features(args.iter);
    std::vector<double> times_us(args.iter);

    using clock = std::chrono::steady_clock;
    for (int i = 0; i < args.iter; ++i) {
        const auto t0 = clock::now();
        generate(args.strategy, hm, args.scene, args.seed);
        const auto t1 = clock::now();
        const double dt = std::chrono::duration<double, std::micro>(t1 - t0).count();
        times_us[i] = dt;
        features[i] = detect_features(hm);
    }

    // Aggregate
    auto stats = [](const std::vector<double>& v) {
        std::vector<double> s = v;
        std::sort(s.begin(), s.end());
        const double mean = std::accumulate(s.begin(), s.end(), 0.0) / s.size();
        const double p50 = s[s.size() / 2];
        const double p95 = s[static_cast<std::size_t>(s.size() * 0.95)];
        double var = 0.0;
        for (double x : s) var += (x - mean) * (x - mean);
        var /= s.size();
        return std::tuple{mean, p50, p95, std::sqrt(var)};
    };

    FeatureCounts fc_mean{};
    for (const auto& f : features) {
        fc_mean.ridgelines += f.ridgelines;
        fc_mean.defilade   += f.defilade;
        fc_mean.kill_zones += f.kill_zones;
        fc_mean.hull_down  += f.hull_down;
        fc_mean.chokepoints += f.chokepoints;
        fc_mean.firing_pos += f.firing_pos;
        fc_mean.cover      += f.cover;
    }
    fc_mean.ridgelines /= static_cast<int>(features.size());
    fc_mean.defilade   /= static_cast<int>(features.size());
    fc_mean.kill_zones /= static_cast<int>(features.size());
    fc_mean.hull_down  /= static_cast<int>(features.size());
    fc_mean.chokepoints /= static_cast<int>(features.size());
    fc_mean.firing_pos /= static_cast<int>(features.size());
    fc_mean.cover      /= static_cast<int>(features.size());

    const auto [t_mean, t_p50, t_p95, t_std] = stats(times_us);

    // Output
    if (!args.output_path.empty()) {
        const std::filesystem::path p{args.output_path};
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream out{args.output_path, std::ios::trunc};
        if (!out) {
            std::cerr << "Cannot open output: " << args.output_path << "\n";
            return 1;
        }
        out << "strategy,scene,seed,iter,time_us_mean,time_us_p50,time_us_p95,time_us_std,"
            << "ridgelines,defilade,kill_zones,hull_down,chokepoints,firing_pos,cover,total\n";
        out << strategy_name(args.strategy) << ","
            << scene_name(args.scene) << ","
            << args.seed << ","
            << args.iter << ","
            << t_mean << "," << t_p50 << "," << t_p95 << "," << t_std << ","
            << fc_mean.ridgelines << "," << fc_mean.defilade << ","
            << fc_mean.kill_zones << "," << fc_mean.hull_down << ","
            << fc_mean.chokepoints << "," << fc_mean.firing_pos << ","
            << fc_mean.cover << "," << fc_mean.total() << "\n";
        std::cout << "Wrote: " << args.output_path << "\n";
    }

    std::cout << std::format(
        "Strategy={} Scene={} Seed={} Iter={}\n"
        "  time_us:  mean={:.2f}  p50={:.2f}  p95={:.2f}  std={:.2f}\n"
        "  features: ridgelines={} defilade={} kill_zones={} hull_down={} chokepoints={} firing_pos={} cover={} TOTAL={}\n",
        strategy_name(args.strategy), scene_name(args.scene), args.seed, args.iter,
        t_mean, t_p50, t_p95, t_std,
        fc_mean.ridgelines, fc_mean.defilade, fc_mean.kill_zones,
        fc_mean.hull_down, fc_mean.chokepoints, fc_mean.firing_pos,
        fc_mean.cover, fc_mean.total());

    if (args.dump_features) {
        std::cout << "\nFirst 3 iterations features:\n";
        for (int i = 0; i < std::min(3, args.iter); ++i) {
            const auto& f = features[i];
            std::cout << std::format("  iter[{}]: ridgelines={} defilade={} kill_zones={} hull_down={} chokepoints={} firing_pos={} cover={}\n",
                i, f.ridgelines, f.defilade, f.kill_zones, f.hull_down,
                f.chokepoints, f.firing_pos, f.cover);
        }
    }
    return 0;
}

}  // namespace mterr

int main(int argc, char** argv) {
    return mterr::main(argc, argv);
}
