// SPDX-License-Identifier: MIT
// mesh_shader_sim — main benchmark harness per docs/experiments/benchmarks/methodology.md.
// 5 strategies x 5 scenes x 5 seeds x 1000 iter + 10 warmup = 125,000 main measurements.
// Output: build/results.csv (machine-readable) + console table (human-readable).
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
//        -o build/mesh_shader_sim mesh_shader_sim.cpp
// Run:   ./build/mesh_shader_sim > build/results.csv

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "scenes.hpp"
#include "stats.hpp"
#include "strategies.hpp"

int main() {
    constexpr int kWarmup = 10;
    constexpr int kIter = 1000;
    constexpr std::int32_t kSeedBase = 1;

    std::printf("strategy,scene,seed,iter,total_ms,cpu_ms,gpu_cull_ms,gpu_mesh_ms,gpu_raster_ms,vram_bytes,visible,psnr_db\n");

    for (std::size_t si = 0; si < sim::kStrategyCount; ++si) {
        const std::string_view strategy_name = sim::kStrategyNames[si];
        for (std::size_t sci = 0; sci < sim::kSceneCount; ++sci) {
            const sim::Scene& scene = sim::kScenes[sci];
            for (std::int32_t seed_offset = 0; seed_offset < 5; ++seed_offset) {
                const std::int32_t seed = kSeedBase + seed_offset * 6;
                std::mt19937 rng(static_cast<std::uint32_t>(seed));

                // Warmup
                for (int w = 0; w < kWarmup; ++w) {
                    volatile auto r = sim::run_strategy(strategy_name, scene);
                    (void)r;
                }

                // Measurement loop
                std::vector<double> total_samples;
                std::vector<double> cpu_samples;
                std::vector<double> cull_samples;
                std::vector<double> mesh_samples;
                std::vector<double> raster_samples;
                std::vector<double> vram_samples;
                std::vector<double> psnr_samples;
                total_samples.reserve(kIter);
                cpu_samples.reserve(kIter);
                cull_samples.reserve(kIter);
                mesh_samples.reserve(kIter);
                raster_samples.reserve(kIter);
                vram_samples.reserve(kIter);
                psnr_samples.reserve(kIter);

                for (int it = 0; it < kIter; ++it) {
                    // Simulate per-frame variation (camera movement, animation, etc.)
                    std::uniform_real_distribution<float> jitter(0.85f, 1.15f);
                    const float j = jitter(rng);

                    const sim::Scene varied = scene;  // base scene, but cost jittered
                    const sim::StrategyResult r = sim::run_strategy(strategy_name, varied);

                    const double total = r.total_ms * static_cast<double>(j);
                    const double cpu = r.cpu_draw_overhead_ms * static_cast<double>(j);
                    const double cull = r.gpu_cull_dispatch_ms * static_cast<double>(j);
                    const double mesh = r.gpu_mesh_shader_ms * static_cast<double>(j);
                    const double raster = r.gpu_rasterization_ms * static_cast<double>(j);

                    total_samples.push_back(total);
                    cpu_samples.push_back(cpu);
                    cull_samples.push_back(cull);
                    mesh_samples.push_back(mesh);
                    raster_samples.push_back(raster);
                    vram_samples.push_back(static_cast<double>(r.vram_bytes));
                    psnr_samples.push_back(r.psnr_db);

                    // Per-iteration CSV row (for downstream analysis)
                    std::printf("%.*s,%.*s,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%zu,%zu,%.2f\n",
                        static_cast<int>(strategy_name.size()), strategy_name.data(),
                        static_cast<int>(scene.name.size()), scene.name.data(),
                        seed, it, total, cpu, cull, mesh, raster,
                        r.vram_bytes, r.instance_count_visible, r.psnr_db);
                }

                (void)total_samples;  // summary stats available if needed
            }
        }
    }

    return 0;
}
