// HZB Smart Mip Selection — main benchmark harness
// Standalone C++26 CPU cull simulator.
// Per `agent/knowledge.md` + benchmarks/methodology.md §3 protocol:
//   warmup 10 iters, measure 1000 iters, mean/median/p95/p99/std.
// CSV output: results.csv
// Build: see README.md

#include "cull_simulator.hpp"
#include "ground_truth_raycaster.hpp"
#include "scenes.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace hzb_smart_mip {

struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double minv = 0.0;
    double maxv = 0.0;
};

Stats ComputeStats(std::vector<double> samples)
{
    Stats s{};
    if (samples.empty()) return s;
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    std::sort(samples.begin(), samples.end());
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
    s.minv = samples.front();
    s.maxv = samples.back();
    double var = 0.0;
    for (double v : samples) {
        const double d = v - s.mean;
        var += d * d;
    }
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    return s;
}

// Parse comma-separated list into vector<T>
std::vector<std::string> SplitCsv(const std::string &s)
{
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

SceneType ParseScene(const std::string &name)
{
    if (name == "uniform_floor") return SceneType::UniformFloor;
    if (name == "forest_floor") return SceneType::ForestFloor;
    if (name == "cave_stress") return SceneType::CaveStress;
    if (name == "mixed_biome") return SceneType::MixedBiome;
    if (name == "view_dolly_stress") return SceneType::ViewDollyStress;
    return SceneType::UniformFloor;
}

CullStrategy ParseStrategy(const std::string &name)
{
    if (name == "A_UniformMip0") return CullStrategy::A_UniformMip0;
    if (name == "B_UniformMipGlobal") return CullStrategy::B_UniformMipGlobal;
    if (name == "C_PerChunkStaticMip") return CullStrategy::C_PerChunkStaticMip;
    if (name == "D_PerChunkDynamicDispatch") return CullStrategy::D_PerChunkDynamicDispatch;
    return CullStrategy::A_UniformMip0;
}

struct Args {
    std::vector<SceneType> scenes{SceneType::UniformFloor};
    std::vector<uint32_t> seeds{1, 7, 42, 1234, 31337};
    std::vector<CullStrategy> strategies{CullStrategy::A_UniformMip0};
    int iter = 1000;
    int warmup = 10;
    std::string output = "../results.csv";
    int screenWidth = 1920;
    int screenHeight = 1080;
    int conservativePixels = 8;
};

Args ParseArgs(int argc, char **argv)
{
    Args a{};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 < argc) return argv[++i];
            return "";
        };
        if (arg == "--scenes") {
            a.scenes.clear();
            for (const auto &s : SplitCsv(next())) a.scenes.push_back(ParseScene(s));
        } else if (arg == "--seeds") {
            a.seeds.clear();
            for (const auto &s : SplitCsv(next())) {
                a.seeds.push_back(static_cast<uint32_t>(std::stoul(s)));
            }
        } else if (arg == "--strategies") {
            a.strategies.clear();
            for (const auto &s : SplitCsv(next())) a.strategies.push_back(ParseStrategy(s));
        } else if (arg == "--iter") {
            a.iter = std::stoi(next());
        } else if (arg == "--warmup") {
            a.warmup = std::stoi(next());
        } else if (arg == "--output") {
            a.output = next();
        } else if (arg == "--screen-width") {
            a.screenWidth = std::stoi(next());
        } else if (arg == "--screen-height") {
            a.screenHeight = std::stoi(next());
        } else if (arg == "--conservative-pixels") {
            a.conservativePixels = std::stoi(next());
        } else if (arg == "--help" || arg == "-h") {
            std::printf("Usage: %s [--scenes s1,s2] [--seeds 1,7,42] [--strategies A,B,C,D] "
                        "[--iter N] [--warmup N] [--output PATH] [--screen-width W] [--screen-height H] "
                        "[--conservative-pixels K]\n", argv[0]);
            std::exit(0);
        }
    }
    return a;
}

}  // namespace hzb_smart_mip

int main(int argc, char **argv)
{
    using namespace hzb_smart_mip;

    Args args = ParseArgs(argc, argv);

    FILE *csv = std::fopen(args.output.c_str(), "w");
    if (!csv) {
        std::fprintf(stderr, "ERROR: cannot open output file %s\n", args.output.c_str());
        return 1;
    }
    std::fprintf(csv, "scene,seed,strategy,chunk_count,culled_count,false_negative_count,"
                       "cull_rate,total_texels_touched,mean_compute_us,p99_compute_us,psnr_db,"
                       "frame_time_us_mean,frame_time_us_p99\n");

    CullConfig config{};
    config.screenWidth = args.screenWidth;
    config.screenHeight = args.screenHeight;
    config.conservativePixels = args.conservativePixels;

    // For each scene, generate chunks once (deterministic by seed)
    // Then for each seed (for camera trajectory + chunk variations), run strategies × iter.
    int totalConfigs = static_cast<int>(args.scenes.size() * args.seeds.size() * args.strategies.size());
    int configIdx = 0;

    for (SceneType scene : args.scenes) {
        for (uint32_t seed : args.seeds) {
            // Generate chunks for this scene+seed
            const std::vector<ChunkAabb> chunks = GenerateScene(scene, seed);
            // Pick a representative camera (mid-dolly position for view_dolly, fixed otherwise)
            Camera cam = GenerateCamera(scene, 100);  // frame 100 for view_dolly
            // Compute ground truth once
            const std::vector<bool> gt = ComputeGroundTruthVisibility(chunks, cam, args.screenWidth, args.screenHeight);
            // B_UniformMipGlobal: pick global mip per frame
            const int globalMip = PickGlobalMip(chunks, cam, args.screenWidth, args.screenHeight, args.conservativePixels);

            // Build HIZ depth pyramid once for this scene+seed+camera
            const HizDepthPyramid hiz = BuildHizDepthPyramid(chunks, cam, args.screenWidth, args.screenHeight, config.maxMipLevel);

            for (CullStrategy strategy : args.strategies) {
                ++configIdx;
                std::printf("[%d/%d] %s seed=%u %s ... ", configIdx, totalConfigs,
                            SceneName(scene), seed, StrategyName(strategy));
                std::fflush(stdout);

                // Warmup
                for (int w = 0; w < args.warmup; ++w) {
                    (void)SimulateBatch(chunks, cam, strategy, config, globalMip, gt, hiz);
                }

                // Measure (collect frame times)
                std::vector<double> frameTimesUs;
                frameTimesUs.reserve(args.iter);
                AggregateMetrics lastAgg{};
                for (int it = 0; it < args.iter; ++it) {
                    const auto t0 = std::chrono::high_resolution_clock::now();
                    lastAgg = SimulateBatch(chunks, cam, strategy, config, globalMip, gt, hiz);
                    const auto t1 = std::chrono::high_resolution_clock::now();
                    const double elapsedUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
                    frameTimesUs.push_back(elapsedUs);
                }

                const Stats frameStats = ComputeStats(frameTimesUs);
                const double cullRate = lastAgg.chunkCount > 0
                    ? static_cast<double>(lastAgg.culledCount) / static_cast<double>(lastAgg.chunkCount)
                    : 0.0;

                std::printf("cull_rate=%.3f FN=%d texels=%lld PSNR=%.2f dB frame_us(mean)=%.2f\n",
                            cullRate, lastAgg.falseNegativeCount, lastAgg.totalTexelsTouched,
                            lastAgg.psnrDb, frameStats.mean);

                std::fprintf(csv, "%s,%u,%s,%d,%d,%d,%.6f,%lld,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                             SceneName(scene), seed, StrategyName(strategy),
                             lastAgg.chunkCount, lastAgg.culledCount, lastAgg.falseNegativeCount,
                             cullRate, lastAgg.totalTexelsTouched,
                             lastAgg.meanComputeUs, lastAgg.p99ComputeUs,
                             lastAgg.psnrDb,
                             frameStats.mean, frameStats.p99);
                std::fflush(csv);
            }
        }
    }

    std::fclose(csv);
    std::printf("\nDone. Results: %s\n", args.output.c_str());
    return 0;
}
