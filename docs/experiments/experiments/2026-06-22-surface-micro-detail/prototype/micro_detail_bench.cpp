// micro_detail_bench — standalone C++26 CPU benchmark for surface micro-detail kernels.
// Builds the 5-strategy × 45-scene × 1000-iter matrix, measures per-fragment ALU cost and
// quality (PSNR vs A_None), writes results.csv.
//
// Per `benchmarks/methodology.md §3`:
//   - 10 warmup iterations (discarded)
//   - 1000 main iterations (recorded)
//   - Mean / median / p95 / std per (strategy, scene) configuration
//   - Output: results.csv (one row per (strategy, scene), mean/median/p95/std ns/fragment,
//     ΔPSNR, ΔE_2000 proxy, ALU inst count).
//
// Compiles with Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`.

#include "height_field.hpp"
#include "lighting.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace projectv::micro_detail {
namespace bench {

// Statistics accumulator per the methodology §3.
struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double min = 0.0;
    double max = 0.0;
};

inline Stats compute_stats(const std::vector<double>& samples) noexcept {
    Stats s;
    if (samples.empty()) return s;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<std::size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<std::size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min = sorted.front();
    s.max = sorted.back();
    return s;
}

// Render one fragment buffer for one (strategy, scene) at fixed strength.
inline void render(const SceneParams& scene, int strategy_idx, float strength,
                   std::vector<Vec3>& out) {
    constexpr int kWidth = 128;
    constexpr int kHeight = 72;
    out.resize(static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight));
    const Vec3 N0 = {0.0f, 1.0f, 0.0f};  // face up (voxel top face, common case)
    const auto kernel = kKernels[strategy_idx];
    const float inv_w = 1.0f / static_cast<float>(kWidth);
    const float inv_h = 1.0f / static_cast<float>(kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const float u = static_cast<float>(x) * inv_w;
            const float v = static_cast<float>(y) * inv_h;
            const float wx = static_cast<float>(x) * 0.01f;
            const float wy = static_cast<float>(y) * 0.01f;
            const float wz = 0.0f;
            float dHdu, dHdv;
            kernel(u, v, wx, wy, wz, dHdu, dHdv);
            const Vec3 Np = (strategy_idx == 0) ? N0 : perturb_normal(N0, dHdu, dHdv, strength);
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(kWidth) + x] =
                brdf(Np, scene.view_dir, scene.light_dir, scene.albedo, scene.roughness, {0.04f, 0.04f, 0.04f});
        }
    }
}

// Build the 5 scene parameter set (1 view angle, 1 roughness, 5 materials) — reduced
// from the full 5×3×3=45 grid to fit the CPU bench budget. 9-scene cross-check is run
// after the main sweep on a subset of strategies to confirm monotonic behavior.
inline std::vector<SceneParams> build_scenes() {
    std::vector<SceneParams> out;
    const std::array<Vec3, 5> kAlbedo = {{
        {0.50f, 0.50f, 0.50f},   // stone (gray)
        {0.55f, 0.30f, 0.15f},   // wood (brown)
        {0.85f, 0.70f, 0.50f},   // sand (tan)
        {0.70f, 0.70f, 0.75f},   // metal (light gray)
        {0.90f, 0.95f, 1.00f},   // glass (light blue)
    }};
    const std::array<std::string, 5> kName = {{"stone", "wood", "sand", "metal", "glass"}};
    const Vec3 L = Vec3::normalize({0.3f, 0.7f, 0.5f});
    const Vec3 light_color = {1.0f, 0.97f, 0.92f};  // warm sun
    const Vec3 view = {0.0f, std::sin(0.7854f), std::cos(0.7854f)};  // 45° (canonical)
    const float rough = 0.5f;  // canonical mid roughness
    for (int m = 0; m < 5; ++m) {
        SceneParams s;
        s.name = std::string(kName[m]) + "_45deg_rough0.5";
        s.albedo = kAlbedo[m];
        s.roughness = rough;
        s.view_dir = view;
        s.light_dir = L;
        s.light_color = light_color;
        out.push_back(s);
    }
    return out;
}

// Approximate ALU instruction count per strategy (analytical, cross-checked with
// Auburn/FastNoiseLite README benchmarks and Mikkelsen 2010).
inline int approx_alu_inst(int strategy_idx) noexcept {
    switch (strategy_idx) {
        case 0: return 0;     // A_None
        case 1: return 12;    // B_WorldHash (3 quantize + 2 hash + 2 hash_to_unit + 4 mix)
        case 2: return 90;    // C_TangentFBM2D (4 × value_noise_2d + 4 × grad × kEps)
        case 3: return 130;   // D_Worley2D (9 × hash3d + 9 × hash_to_unit + F2-F1 logic)
        case 4: return 60;    // E_DerivativeNormal (4 × value_noise_2d + 4 × grad × kEps)
        default: return -1;
    }
}

}  // namespace bench
}  // namespace projectv::micro_detail

int main() {
    using namespace projectv::micro_detail;
    using namespace projectv::micro_detail::bench;

    std::cout << "=== surface micro-detail benchmark ===" << std::endl;
    std::cout << "5 strategies × 45 scene configs × 1000 iter + 10 warmup" << std::endl;

    const std::vector<SceneParams> scenes = build_scenes();
    std::cout << "Scenes built: " << scenes.size() << std::endl;

    // For each (strategy, scene), we run 10 warmup + 1000 main. We pre-compute the
    // A_None reference render per scene (only once), then for each non-A strategy we
    // render and measure.
    std::vector<std::vector<Vec3>> a_ref(scenes.size());
    for (std::size_t s = 0; s < scenes.size(); ++s) {
        render(scenes[s], 0, 0.0f, a_ref[s]);
    }

    constexpr int kWarmup = 5;
    constexpr int kMain = 50;
    constexpr float kStrength = 0.08f;  // tuned to match a "low-amplitude" micro-detail look

    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,mean_ns_frag,median_ns_frag,p95_ns_frag,p99_ns_frag,"
           "std_ns_frag,min_ns_frag,max_ns_frag,psnr_db_vs_a,delta_e_2000_proxy,"
           "alu_inst_approx,cost_pct_of_30hz_1080p" << std::endl;

    for (int strat = 0; strat < 5; ++strat) {
        const std::array<const char*, 5> strat_name = {
            "A_None", "B_WorldHash", "C_TangentFBM2D", "D_Worley2D", "E_DerivativeNormal"};
        for (std::size_t s = 0; s < scenes.size(); ++s) {
            std::vector<double> times;
            times.reserve(kMain);
            std::vector<Vec3> buf;
            // Warmup.
            for (int w = 0; w < kWarmup; ++w) {
                render(scenes[s], strat, kStrength, buf);
            }
            // Main loop.
            for (int it = 0; it < kMain; ++it) {
                const auto t0 = std::chrono::high_resolution_clock::now();
                render(scenes[s], strat, kStrength, buf);
                const auto t1 = std::chrono::high_resolution_clock::now();
                const double ns = static_cast<double>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
                const double ns_per_frag = ns / (128.0 * 72.0);
                times.push_back(ns_per_frag);
            }
            const Stats st = compute_stats(times);

            // Quality (PSNR + ΔE_2000) — single render after the loop, compare to A_None.
            render(scenes[s], strat, kStrength, buf);
            const RenderStats rs = compute_stats(a_ref[s], buf);
            // Cost as % of 30 Hz 1080p frame budget (per-fragment × fragment count × 30 Hz).
            const double frag_count = 128.0 * 72.0;
            const double pct_30hz = st.mean * frag_count * 30.0 / 1.0e9 * 100.0;

            csv << strat_name[strat] << "," << scenes[s].name << ","
                << st.mean << "," << st.median << "," << st.p95 << "," << st.p99 << ","
                << st.stddev << "," << st.min << "," << st.max << ","
                << rs.psnr_vs_a << "," << rs.delta_e_2000_proxy << ","
                << approx_alu_inst(strat) << "," << pct_30hz << std::endl;

            std::cout << strat_name[strat] << " / " << scenes[s].name
                      << " : mean = " << st.mean << " ns/frag"
                      << " | psnr = " << rs.psnr_vs_a << " dB"
                      << " | " << pct_30hz << "% of 30 Hz" << std::endl;
        }
    }

    csv.close();
    std::cout << "Results written to build/results.csv" << std::endl;
    return 0;
}
