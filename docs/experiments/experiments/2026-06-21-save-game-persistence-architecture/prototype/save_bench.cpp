// 2026-06-21-save-game-persistence-architecture/prototype/save_bench.cpp
// Main harness for save game persistence architecture benchmark.
// 5 strategies x 5 scenes x 5 seeds x 100 iterations + 10 warmup.
// Output: results.csv (per-measurement) + summary_means.csv (per-strategy x scene).

#include "compression.hpp"
#include "strategies.hpp"
#include "world_model.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;
using namespace save_bench;

struct SceneConfig {
    std::string_view name;
    int chunk_dim;
    float fill_density;
    float entity_density;
};

constexpr std::array<SceneConfig, 5> kScenes = {{
    {"small_world",      6,  0.30f, 0.50f},
    {"medium_world",    10,  0.30f, 0.50f},
    {"large_world",     14,  0.30f, 0.50f},
    {"adaptive_scaling", 10,  0.30f, 0.50f},
    {"realistic_combat", 10,  0.60f, 1.50f},
}};

constexpr std::array<float, 5> kMutationPcts = {0.0f, 1.0f, 10.0f, 50.0f, 100.0f};

constexpr std::array<std::uint64_t, 5> kSeeds = {1, 7, 42, 1234, 31337};

struct Measurement {
    std::string strategy;
    std::string scene;
    std::uint64_t seed{};
    int iter{};
    std::string op;
    double us{};
    std::size_t bytes{};
    bool ok{};
};

inline auto now_us() -> double {
    return std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

template <typename Strategy>
auto run_full(const SceneConfig& scene,
              std::uint64_t seed,
              int iter,
              const fs::path& save_path,
              std::vector<Measurement>& out) -> void {
    auto world = synthesize_world(seed, scene.chunk_dim, scene.fill_density, scene.entity_density);

    const auto t_save0 = now_us();
    const auto save_res = Strategy::save(world, save_path);
    const auto t_save1 = now_us();
    out.push_back({std::string(Strategy::kName), std::string(scene.name), seed, iter, "save",
                   t_save1 - t_save0, save_res.bytes_written, true});

    const auto t_load0 = now_us();
    auto loaded = Strategy::load(save_path);
    const auto t_load1 = now_us();
    const bool ok = worlds_equal(world, loaded);
    out.push_back({std::string(Strategy::kName), std::string(scene.name), seed, iter, "load",
                   t_load1 - t_load0, save_res.bytes_written, ok});

    const auto t_v0 = now_us();
    const bool verify_ok = worlds_equal(world, loaded);
    const auto t_v1 = now_us();
    out.push_back({std::string(Strategy::kName), std::string(scene.name), seed, iter, "verify",
                   t_v1 - t_v0, 0, verify_ok});

    const float mutation_pct = kMutationPcts[iter % kMutationPcts.size()];
    auto mutated = mutate_world(world, seed ^ 0xCAFEu, mutation_pct);
    const auto t_ms0 = now_us();
    const auto ms_res = Strategy::mutate_save(world, mutated, save_path);
    const auto t_ms1 = now_us();
    out.push_back({std::string(Strategy::kName), std::string(scene.name), seed, iter,
                   "mutate_save_" + std::to_string(static_cast<int>(mutation_pct)),
                   t_ms1 - t_ms0, ms_res.bytes_written, true});

    const auto t_dl0 = now_us();
    auto delta_loaded = Strategy::load(save_path);
    const auto t_dl1 = now_us();
    const bool delta_ok = worlds_equal(mutated, delta_loaded);
    out.push_back({std::string(Strategy::kName), std::string(scene.name), seed, iter, "delta_load",
                   t_dl1 - t_dl0, ms_res.bytes_written, delta_ok});
}

auto main(int argc, char** argv) -> int {
    int iterations = 30;
    int warmup = 10;
    std::string out_dir = "build";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--iter" && i + 1 < argc) iterations = std::atoi(argv[++i]);
        else if (a == "--warmup" && i + 1 < argc) warmup = std::atoi(argv[++i]);
        else if (a == "--out" && i + 1 < argc) out_dir = argv[++i];
    }

    fs::create_directories(out_dir);
    const fs::path save_dir = out_dir + "/saves";
    fs::create_directories(save_dir);

    std::vector<Measurement> measurements;
    measurements.reserve(5 * 5 * 5 * iterations * 5);

    // Warmup.
    for (int w = 0; w < warmup; ++w) {
        fs::path p = save_dir / ("warmup_" + std::to_string(w) + ".bin");
        auto w_test = synthesize_world(0, 4, 0.3f, 0.5f);
        strategies::B_ChunkedBinaryRaw::save(w_test, p);
        std::filesystem::remove(p);
    }

    // Main run: 5 strategies * 5 scenes * 5 seeds * 100 iter = 12500 calls per strategy.
    int total = 5 * 5 * 5 * iterations;
    int done = 0;
    auto t_start = std::chrono::steady_clock::now();

    for (int si = 0; si < 5; ++si) {
        const auto& scene = kScenes[si];
        for (auto seed : kSeeds) {
            for (int it = 0; it < iterations; ++it) {
                // Strategy A.
                {
                    fs::path p = save_dir / (std::string("A_") + std::string(scene.name) + "_" +
                                             std::to_string(seed) + "_" + std::to_string(it) + ".json");
                    run_full<strategies::A_FullJSON>(scene, seed, it, p, measurements);
                    std::filesystem::remove(p);
                    ++done;
                    if (done % 100 == 0) {
                        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();
                        std::fprintf(stderr, "[%d/%d] elapsed=%.1fs eta=%.1fs\n",
                                     done, total, elapsed, elapsed * (total - done) / done);
                    }
                }
                // Strategy B.
                {
                    fs::path p = save_dir / (std::string("B_") + std::string(scene.name) + "_" +
                                             std::to_string(seed) + "_" + std::to_string(it) + ".bin");
                    run_full<strategies::B_ChunkedBinaryRaw>(scene, seed, it, p, measurements);
                    std::filesystem::remove(p);
                    ++done;
                }
                // Strategy C.
                {
                    fs::path p = save_dir / (std::string("C_") + std::string(scene.name) + "_" +
                                             std::to_string(seed) + "_" + std::to_string(it) + ".zst");
                    run_full<strategies::C_ChunkedBinaryZstd>(scene, seed, it, p, measurements);
                    std::filesystem::remove(p);
                    ++done;
                }
                // Strategy D.
                {
                    fs::path p = save_dir / (std::string("D_") + std::string(scene.name) + "_" +
                                             std::to_string(seed) + "_" + std::to_string(it) + ".dvpd");
                    run_full<strategies::D_VersionedChunkedDeltaLZ4>(scene, seed, it, p, measurements);
                    std::filesystem::remove(p);
                    ++done;
                }
                // Strategy E.
                {
                    fs::path p = save_dir / (std::string("E_") + std::string(scene.name) + "_" +
                                             std::to_string(seed) + "_" + std::to_string(it) + ".cas");
                    run_full<strategies::E_ContentAddressedDedupe>(scene, seed, it, p, measurements);
                    std::filesystem::remove(p);
                    fs::remove_all(p.string() + ".cas");
                    ++done;
                }
            }
        }
    }

    auto elapsed_total = std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();

    // Write results.csv.
    {
        std::ofstream out(out_dir + "/results.csv");
        out << "strategy,scene,seed,iter,op,us,bytes,ok\n";
        for (const auto& m : measurements) {
            out << m.strategy << ',' << m.scene << ',' << m.seed << ',' << m.iter << ','
                << m.op << ',' << std::fixed << std::setprecision(2) << m.us << ','
                << m.bytes << ',' << (m.ok ? "1" : "0") << '\n';
        }
    }

    // Write summary_means.csv.
    {
        std::map<std::tuple<std::string, std::string, std::string>, std::vector<double>> by_group;
        std::map<std::tuple<std::string, std::string, std::string>, std::size_t> by_group_bytes;
        for (const auto& m : measurements) {
            const auto key = std::make_tuple(m.strategy, m.scene, m.op);
            by_group[key].push_back(m.us);
            by_group_bytes[key] = m.bytes;
        }
        std::ofstream out(out_dir + "/summary_means.csv");
        out << "strategy,scene,op,mean_us,median_us,p95_us,n,bytes\n";
        for (auto& [key, us_vec] : by_group) {
            std::sort(us_vec.begin(), us_vec.end());
            const double mean = std::accumulate(us_vec.begin(), us_vec.end(), 0.0) / us_vec.size();
            const double median = us_vec[us_vec.size() / 2];
            const double p95 = us_vec[static_cast<std::size_t>(us_vec.size() * 0.95)];
            const auto& [strategy, scene, op] = key;
            out << strategy << ',' << scene << ',' << op << ','
                << std::fixed << std::setprecision(2) << mean << ',' << median << ',' << p95 << ','
                << us_vec.size() << ',' << by_group_bytes[key] << '\n';
        }
    }

    std::fprintf(stderr, "Done. %zu measurements in %.2fs wall.\n", measurements.size(), elapsed_total);
    std::fprintf(stderr, "Output: %s/results.csv, %s/summary_means.csv\n", out_dir.c_str(), out_dir.c_str());
    return 0;
}