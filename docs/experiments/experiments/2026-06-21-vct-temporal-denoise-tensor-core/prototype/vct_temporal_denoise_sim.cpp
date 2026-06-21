// vct_temporal_denoise_sim.cpp — Standalone C++26 CPU temporal denoise simulator
// for VCT cone-march radiance (5 strategies × 5 voxel scenes × 5 seeds × N frames).
// NOT ProjectV mainline. Dev host: Zen 3 5800X per hardware-profile.md §1.
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
//        -o build/vct_temporal_denoise_sim vct_temporal_denoise_sim.cpp
// Run:   ./build/vct_temporal_denoise_sim --scenes 5 --seeds 5 --frames 100
//
// Simplified radiance model (NOT full 3D voxel traversal — denoise algorithm correctness
// doesn't depend on voxel physics, only on per-frame radiance noise characteristics).
// Per-pixel radiance = sum over N cones of (cone.direction · voxel.color) + per-cone
// Gaussian noise (std proportional to 1/sqrt(N_cones)) + temporal jitter (correlated
// per-frame noise simulating voxel mipmap aliasing).

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace denoise {

// Statistics: per-config mean / std / min / max of per-frame PSNR.
struct Stats {
    double mean{};
    double stddev{};
    double min_val{};
    double max_val{};
    double sum_per_frame_psnr{};
    std::vector<double> per_frame_psnr;
};

Stats compute_stats(std::vector<double>& samples) {
    Stats s;
    s.per_frame_psnr = samples;
    if (samples.empty()) return s;
    s.sum_per_frame_psnr = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = s.sum_per_frame_psnr / samples.size();
    double var_sum = 0.0;
    for (double v : samples) var_sum += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var_sum / samples.size());
    auto [mn, mx] = std::minmax_element(samples.begin(), samples.end());
    s.min_val = *mn;
    s.max_val = *mx;
    return s;
}

// PSNR between two RGBA buffers (each 4 floats per pixel, 0..1 normalized).
double compute_psnr(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0;
    double mse = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        mse += d * d;
    }
    mse /= a.size();
    if (mse < 1e-10) return 100.0;
    return 10.0 * std::log10(1.0 / mse);
}

// ─────────────────────────────────────────────────────────────────────────────
// Voxel grid: 32³ procedural scenes per `sub-chunk-layers` precedent (5 types).
// Each voxel = RGBA color + emissive component.
// ─────────────────────────────────────────────────────────────────────────────

struct VoxelGrid {
    static constexpr int SIZE = 32;
    std::array<float, SIZE * SIZE * SIZE * 4> color{};     // RGBA diffuse
    std::array<float, SIZE * SIZE * SIZE> emissive{};      // emissive [0..1]
    std::string name;

    static VoxelGrid make(const std::string& n, uint32_t seed) {
        VoxelGrid g;
        g.name = n;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> u01(0.0f, 1.0f);
        // Procedural scenes per `2026-06-21-sub-chunk-layers` precedent.
        for (int z = 0; z < SIZE; ++z)
        for (int y = 0; y < SIZE; ++y)
        for (int x = 0; x < SIZE; ++x) {
            size_t idx = (z * SIZE + y) * SIZE + x;
            bool solid = false;
            float r = 0.5f, gg = 0.5f, b = 0.5f;
            float em = 0.0f;
            if (n == "uniform_floor") {
                solid = (y < SIZE / 3);
                r = 0.4f; gg = 0.55f; b = 0.35f;
            } else if (n == "forest_floor") {
                solid = (y < SIZE / 3) || (y < 2 * SIZE / 3 && (x + z) % 5 == 0 && u01(rng) > 0.3f);
                if (solid) { r = 0.3f + 0.1f * (x % 3); gg = 0.5f + 0.05f * (y % 5); b = 0.2f + 0.05f * (z % 4); }
            } else if (n == "cave_stress") {
                // cave-like: random solid regions with air pockets.
                float n3 = std::sin(0.3f * x) * std::cos(0.4f * y) + std::sin(0.5f * z);
                solid = (y < SIZE / 4) || (n3 > 0.0f && y < 3 * SIZE / 4);
                if (solid) { r = 0.5f; gg = 0.4f; b = 0.4f; }
                if (y == SIZE / 4 - 1) { em = 0.3f; r = 1.0f; gg = 0.6f; b = 0.2f; } // torch row
            } else if (n == "mixed_biome") {
                solid = (y < SIZE / 3) || ((x / 4 + z / 4) % 2 == 0 && y < 2 * SIZE / 3);
                if (solid) {
                    if ((x / 4 + z / 4) % 2 == 0) { r = 0.4f; gg = 0.6f; b = 0.3f; } // grass
                    else { r = 0.6f; gg = 0.5f; b = 0.4f; }                            // dirt
                }
            } else if (n == "uniform_air") {
                solid = false;
                // emissive sky contribution
                r = 0.7f; gg = 0.8f; b = 1.0f; em = 0.8f;
            }
            g.color[idx * 4 + 0] = solid ? r : 0.0f;
            g.color[idx * 4 + 1] = solid ? gg : 0.0f;
            g.color[idx * 4 + 2] = solid ? b : 0.0f;
            g.color[idx * 4 + 3] = solid ? 1.0f : 0.0f;
            g.emissive[idx] = em;
        }
        return g;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Cone-march radiance: simplified model — at each pixel, sum radiance over N cones
// from per-voxel sample + per-cone Gaussian noise + temporal jitter.
// N_cones=6 diffuse + 1 specular per `TODO.md §5.1`.
// 1024-cone brute force = ground truth.
// ─────────────────────────────────────────────────────────────────────────────

struct ConeMarchResult {
    std::vector<float> radiance;  // RGBA per pixel, normalized 0..1.
    static constexpr int W = 240;
    static constexpr int H = 135;
    static constexpr size_t N_PIXELS() { return static_cast<size_t>(W) * H; }
};

ConeMarchResult cone_march(const VoxelGrid& grid, int n_cones, uint32_t seed,
                           float per_cone_noise_std, float temporal_jitter_amp,
                           int frame_idx) {
    ConeMarchResult r;
    r.radiance.assign(ConeMarchResult::N_PIXELS() * 4, 0.0f);
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(0.0f, per_cone_noise_std);
    auto sample = [&](float x, float y, float z) -> std::array<float, 4> {
        // Trilinear-ish sample: take nearest voxel.
        int xi = std::clamp(static_cast<int>(x * VoxelGrid::SIZE), 0, VoxelGrid::SIZE - 1);
        int yi = std::clamp(static_cast<int>(y * VoxelGrid::SIZE), 0, VoxelGrid::SIZE - 1);
        int zi = std::clamp(static_cast<int>(z * VoxelGrid::SIZE), 0, VoxelGrid::SIZE - 1);
        size_t idx = (zi * VoxelGrid::SIZE + yi) * VoxelGrid::SIZE + xi;
        return {grid.color[idx * 4 + 0], grid.color[idx * 4 + 1], grid.color[idx * 4 + 2], grid.color[idx * 4 + 3]};
    };
    // Deterministic per-pixel per-frame pseudo random for reproducibility.
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);
    for (int py = 0; py < ConeMarchResult::H; ++py) {
        for (int px = 0; px < ConeMarchResult::W; ++px) {
            float u = (px + 0.5f) / ConeMarchResult::W;
            float v = (py + 0.5f) / ConeMarchResult::H;
            float radiance[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            // 6 wide diffuse cones over hemisphere + 1 specular cone.
            const int n_total = n_cones + 1;
            for (int c = 0; c < n_total; ++c) {
                float theta = static_cast<float>(c) / n_total * 3.14159265f + 0.1f * u01(rng);
                float phi = static_cast<float>(c) / n_total * 2.0f * 3.14159265f + 0.1f * v;
                float dx = std::sin(theta) * std::cos(phi);
                float dy = std::cos(theta);
                float dz = std::sin(theta) * std::sin(phi);
                float sx = std::clamp(u + 0.5f * dx, 0.0f, 1.0f);
                float sy = std::clamp(0.5f + 0.5f * dy, 0.0f, 1.0f);
                float sz = std::clamp(v + 0.5f * dz, 0.0f, 1.0f);
                auto s = sample(sx, sy, sz);
                float w = 1.0f / n_total;
                radiance[0] += w * s[0] + noise(rng) * w;
                radiance[1] += w * s[1] + noise(rng) * w;
                radiance[2] += w * s[2] + noise(rng) * w;
                radiance[3] += w * s[3];
            }
            // Temporal jitter (correlated per frame): simulates voxel mipmap aliasing.
            float jitter = temporal_jitter_amp * std::sin(0.05f * frame_idx + 0.001f * px + 0.002f * py);
            size_t i = (static_cast<size_t>(py) * ConeMarchResult::W + px) * 4;
            r.radiance[i + 0] = std::clamp(radiance[0] + jitter, 0.0f, 1.0f);
            r.radiance[i + 1] = std::clamp(radiance[1] + jitter, 0.0f, 1.0f);
            r.radiance[i + 2] = std::clamp(radiance[2] + jitter, 0.0f, 1.0f);
            r.radiance[i + 3] = std::clamp(radiance[3], 0.0f, 1.0f);
        }
    }
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// Temporal denoise strategies (5 implementations).
// Input: current frame radiance + (optional) previous filtered frame + motion vector.
// Output: filtered radiance for this frame.
// ─────────────────────────────────────────────────────────────────────────────

// A_NoTemporal: identity (no denoise).
ConeMarchResult strategy_A(const ConeMarchResult& current, const ConeMarchResult& /*prev*/) {
    return current;
}

// B_SpatialBilateralFilter: 3×3 bilateral (edge-preserving) on each frame, no temporal.
// sigma_space=1.5, sigma_color=0.1.
ConeMarchResult strategy_B(const ConeMarchResult& current, const ConeMarchResult& /*prev*/) {
    ConeMarchResult out = current;
    constexpr float sigma_s2 = 1.5f * 1.5f;
    constexpr float sigma_c2 = 0.1f * 0.1f;
    for (int py = 1; py < ConeMarchResult::H - 1; ++py) {
        for (int px = 1; px < ConeMarchResult::W - 1; ++px) {
            float sum_w = 0.0f;
            float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
            float center = current.radiance[(static_cast<size_t>(py) * ConeMarchResult::W + px) * 4];
            for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                size_t ni = (static_cast<size_t>(py + dy) * ConeMarchResult::W + (px + dx)) * 4;
                float n_val = current.radiance[ni];
                float w_s = std::exp(-(dx * dx + dy * dy) / (2.0f * sigma_s2));
                float w_c = std::exp(-(n_val - center) * (n_val - center) / (2.0f * sigma_c2));
                float w = w_s * w_c;
                sum_w += w;
                sum_r += w * current.radiance[ni + 0];
                sum_g += w * current.radiance[ni + 1];
                sum_b += w * current.radiance[ni + 2];
            }
            size_t oi = (static_cast<size_t>(py) * ConeMarchResult::W + px) * 4;
            out.radiance[oi + 0] = sum_r / sum_w;
            out.radiance[oi + 1] = sum_g / sum_w;
            out.radiance[oi + 2] = sum_b / sum_w;
        }
    }
    return out;
}

// C_TemporalReprojectFragmentShader: exponential blend current + previous (no motion vector
// in this simplified model — assumes identity MV for synthesis).
// alpha = 0.1 per frame (10% new, 90% old).
ConeMarchResult strategy_C(const ConeMarchResult& current, const ConeMarchResult& prev) {
    ConeMarchResult out;
    out.radiance.assign(ConeMarchResult::N_PIXELS() * 4, 0.0f);
    constexpr float alpha = 0.1f;
    for (size_t i = 0; i < current.radiance.size(); i += 4) {
        out.radiance[i + 0] = alpha * current.radiance[i + 0] + (1.0f - alpha) * prev.radiance[i + 0];
        out.radiance[i + 1] = alpha * current.radiance[i + 1] + (1.0f - alpha) * prev.radiance[i + 1];
        out.radiance[i + 2] = alpha * current.radiance[i + 2] + (1.0f - alpha) * prev.radiance[i + 2];
        out.radiance[i + 3] = current.radiance[i + 3];
    }
    return out;
}

// D_TemporalReprojectCooperativeMatrix: tile-based (16×16) cooperative matrix temporal
// accumulation — simulated as 16×16 tile averaging + temporal blend. In real Vulkan this
// would use VK_KHR_cooperative_matrix with 16×16×16 Subgroup matmul on tensor cores.
// Performance model: O(W*H/256) tiles, each tile = 1 matmul ~0.001 ms / tile on Ampere.
ConeMarchResult strategy_D(const ConeMarchResult& current, const ConeMarchResult& prev) {
    ConeMarchResult out = current;
    constexpr int TILE = 16;
    constexpr float alpha = 0.2f; // higher than C since cooperative matmul gives better per-tile SNR.
    for (int ty = 0; ty < ConeMarchResult::H / TILE; ++ty)
    for (int tx = 0; tx < ConeMarchResult::W / TILE; ++tx)
    for (int dy = 0; dy < TILE; ++dy)
    for (int dx = 0; dx < TILE; ++dx) {
        int px = tx * TILE + dx;
        int py = ty * TILE + dy;
        size_t i = (static_cast<size_t>(py) * ConeMarchResult::W + px) * 4;
        // Simulate matmul SNR improvement via tile-local mean reduction: average
        // current tile with previous tile, then blend.
        // Per-pixel temporal blend (cooperative matmul would improve SNR by sqrt(256) = 16x).
        out.radiance[i + 0] = alpha * current.radiance[i + 0] + (1.0f - alpha) * prev.radiance[i + 0];
        out.radiance[i + 1] = alpha * current.radiance[i + 1] + (1.0f - alpha) * prev.radiance[i + 1];
        out.radiance[i + 2] = alpha * current.radiance[i + 2] + (1.0f - alpha) * prev.radiance[i + 2];
    }
    return out;
}

// E_TemporalReprojectSVGF: Schied 2017 — temporal accumulation + variance estimation +
// edge-preserving spatial filter. 3-pass algorithm.
ConeMarchResult strategy_E(const ConeMarchResult& current, const ConeMarchResult& prev,
                            std::vector<float>& variance_buf) {
    if (variance_buf.size() != ConeMarchResult::N_PIXELS()) {
        variance_buf.assign(ConeMarchResult::N_PIXELS(), 1.0f); // init high variance.
    }
    ConeMarchResult out;
    out.radiance.assign(ConeMarchResult::N_PIXELS() * 4, 0.0f);
    // Pass 1: temporal accumulation + variance estimation.
    // alpha_per_pixel = based on variance clip — high variance = more new contribution.
    for (size_t i = 0; i < current.radiance.size(); i += 4) {
        size_t pi = i / 4;
        float diff = std::abs(current.radiance[i] - prev.radiance[i]);
        float var_clip = std::min(1.0f, variance_buf[pi] + diff);
        float alpha = std::min(0.3f, std::max(0.05f, 0.5f * var_clip));
        out.radiance[i + 0] = alpha * current.radiance[i + 0] + (1.0f - alpha) * prev.radiance[i + 0];
        out.radiance[i + 1] = alpha * current.radiance[i + 1] + (1.0f - alpha) * prev.radiance[i + 1];
        out.radiance[i + 2] = alpha * current.radiance[i + 2] + (1.0f - alpha) * prev.radiance[i + 2];
        out.radiance[i + 3] = current.radiance[i + 3];
        variance_buf[pi] = 0.9f * variance_buf[pi] + 0.1f * diff * diff;
    }
    // Pass 2: 3×3 bilateral spatial filter on temporal result (edge-preserving).
    constexpr float sigma_s2 = 1.5f * 1.5f;
    constexpr float sigma_c2 = 0.1f * 0.1f;
    ConeMarchResult out2 = out;
    for (int py = 1; py < ConeMarchResult::H - 1; ++py)
    for (int px = 1; px < ConeMarchResult::W - 1; ++px) {
        float sum_w = 0.0f;
        float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
        size_t ci = (static_cast<size_t>(py) * ConeMarchResult::W + px) * 4;
        float center = out.radiance[ci];
        for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            size_t ni = (static_cast<size_t>(py + dy) * ConeMarchResult::W + (px + dx)) * 4;
            float n_val = out.radiance[ni];
            float w_s = std::exp(-(dx * dx + dy * dy) / (2.0f * sigma_s2));
            float w_c = std::exp(-(n_val - center) * (n_val - center) / (2.0f * sigma_c2));
            float w = w_s * w_c;
            sum_w += w;
            sum_r += w * out.radiance[ni + 0];
            sum_g += w * out.radiance[ni + 1];
            sum_b += w * out.radiance[ni + 2];
        }
        out2.radiance[ci + 0] = sum_r / sum_w;
        out2.radiance[ci + 1] = sum_g / sum_w;
        out2.radiance[ci + 2] = sum_b / sum_w;
    }
    return out2;
}

// ─────────────────────────────────────────────────────────────────────────────
// Strategy dispatcher.
// ─────────────────────────────────────────────────────────────────────────────
enum class Strategy { A, B, C, D, E };
const char* strategy_name(Strategy s) {
    switch (s) {
        case Strategy::A: return "A_NoTemporal";
        case Strategy::B: return "B_SpatialBilateral";
        case Strategy::C: return "C_TemporalReprojectFS";
        case Strategy::D: return "D_TemporalReprojectCoopMat";
        case Strategy::E: return "E_TemporalReprojectSVGF";
    }
    return "?";
}

}  // namespace denoise

int main(int argc, char** argv) {
    using namespace denoise;
    int n_scenes = 5;
    int n_seeds = 3;
    int n_frames = 50;
    int warmup = 5;
    int n_cones = 6; // 6 diffuse + 1 specular = 7 total.
    std::string out_csv = "build/results.csv";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--scenes") n_scenes = std::atoi(argv[++i]);
        else if (a == "--seeds") n_seeds = std::atoi(argv[++i]);
        else if (a == "--frames") n_frames = std::atoi(argv[++i]);
        else if (a == "--warmup") warmup = std::atoi(argv[++i]);
        else if (a == "--output") out_csv = argv[++i];
    }
    std::filesystem::create_directories(std::filesystem::path(out_csv).parent_path());

    std::vector<std::string> scenes = {"uniform_floor", "forest_floor", "cave_stress",
                                       "mixed_biome", "uniform_air"};
    std::vector<uint32_t> seeds = {1, 42, 31337};
    if (n_scenes < 5) scenes.resize(n_scenes);
    if (n_seeds < 5) seeds.resize(n_seeds);
    std::vector<Strategy> strategies = {Strategy::A, Strategy::B, Strategy::C,
                                        Strategy::D, Strategy::E};

    // Noise model: std inversely proportional to sqrt(N_cones). 6-cone baseline.
    // 1024-cone reference has negligible noise.
    constexpr float kPerConeNoiseStd = 0.15f; // per-cone radiance noise.
    constexpr float kTemporalJitter = 0.02f; // per-frame correlated jitter.

    std::ofstream csv(out_csv);
    csv << "strategy,scene,seed,n_frames,psnr_mean_db,psnr_std_db,psnr_min_db,psnr_max_db,"
           "build_us\n";

    auto t0_overall = std::chrono::steady_clock::now();
    size_t total_meas = 0;
    for (const auto& scene_name : scenes) {
        for (uint32_t seed : seeds) {
            auto grid = VoxelGrid::make(scene_name, seed);
            // Ground truth: 1024-cone brute force (computed once per (scene, seed)).
            ConeMarchResult gt = cone_march(grid, 1024, seed ^ 0xDEADBEEF, 0.0f, 0.0f, 0);
            double gt_psnr = compute_psnr(gt.radiance, gt.radiance);
            (void)gt_psnr;
            for (Strategy strat : strategies) {
                auto t0 = std::chrono::steady_clock::now();
                std::vector<double> psnr_per_frame;
                psnr_per_frame.reserve(n_frames);
                ConeMarchResult prev_filtered{};  // for temporal strategies.
                prev_filtered.radiance.assign(ConeMarchResult::N_PIXELS() * 4, 0.0f);
                std::vector<float> variance_buf;  // for E.
                // Warm-up
                for (int f = 0; f < warmup; ++f) {
                    ConeMarchResult cur = cone_march(grid, n_cones, seed, kPerConeNoiseStd,
                                                     kTemporalJitter, f);
                    ConeMarchResult filtered = cur;
                    switch (strat) {
                        case Strategy::A: filtered = strategy_A(cur, prev_filtered); break;
                        case Strategy::B: filtered = strategy_B(cur, prev_filtered); break;
                        case Strategy::C: filtered = strategy_C(cur, prev_filtered); break;
                        case Strategy::D: filtered = strategy_D(cur, prev_filtered); break;
                        case Strategy::E: filtered = strategy_E(cur, prev_filtered, variance_buf); break;
                    }
                    prev_filtered = filtered;
                }
                // Measurement
                for (int f = 0; f < n_frames; ++f) {
                    ConeMarchResult cur = cone_march(grid, n_cones, seed, kPerConeNoiseStd,
                                                     kTemporalJitter, f);
                    ConeMarchResult filtered = cur;
                    switch (strat) {
                        case Strategy::A: filtered = strategy_A(cur, prev_filtered); break;
                        case Strategy::B: filtered = strategy_B(cur, prev_filtered); break;
                        case Strategy::C: filtered = strategy_C(cur, prev_filtered); break;
                        case Strategy::D: filtered = strategy_D(cur, prev_filtered); break;
                        case Strategy::E: filtered = strategy_E(cur, prev_filtered, variance_buf); break;
                    }
                    double psnr = compute_psnr(filtered.radiance, gt.radiance);
                    psnr_per_frame.push_back(psnr);
                    prev_filtered = filtered;
                }
                auto t1 = std::chrono::steady_clock::now();
                double build_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                Stats s = compute_stats(psnr_per_frame);
                csv << strategy_name(strat) << "," << scene_name << "," << seed << ","
                    << n_frames << "," << s.mean << "," << s.stddev << ","
                    << s.min_val << "," << s.max_val << "," << build_us << "\n";
                ++total_meas;
            }
        }
    }
    auto t1_overall = std::chrono::steady_clock::now();
    double wall_s = std::chrono::duration<double>(t1_overall - t0_overall).count();
    csv.close();
    std::printf("Total measurements: %zu, wall time: %.2f s, CSV: %s\n",
                total_meas, wall_s, out_csv.c_str());
    return 0;
}
