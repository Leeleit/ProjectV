#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "depth_quant_bench.hpp"
#include "voxel_scene.hpp"

using namespace depth_quant;

void printUsage() {
    std::cerr << "Usage: depth_quant_bench [--output FILE] [--warmup N] [--iterations N]\n"
              << "Default: --warmup 100 --iterations 1000 --output results.csv\n";
}

int main(int argc, char** argv) {
    int warmup = 100;
    int iterations = 1000;
    std::string outputPath = "results.csv";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--warmup" && i + 1 < argc) warmup = std::stoi(argv[++i]);
        else if (arg == "--iterations" && i + 1 < argc) iterations = std::stoi(argv[++i]);
        else if (arg == "--output" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
    }

    std::vector<std::pair<int, int>> resolutions = {
        {1280, 720}, {1920, 1080}
    };
    std::vector<int> viewDistances = {64, 128, 256};
    std::vector<DepthFormat> formats = {
        DepthFormat::D32_SFLOAT,
        DepthFormat::D16_UNORM,
        DepthFormat::D16_UNORM_REVERSE_Z
    };

    std::vector<BenchConfig> configs;
    for (int scene = 0; scene < sceneCount(); ++scene) {
        for (const auto& res : resolutions) {
            for (int vd : viewDistances) {
                for (DepthFormat fmt : formats) {
                    BenchConfig cfg{};
                    cfg.sceneIdx = scene;
                    cfg.width = res.first;
                    cfg.height = res.second;
                    cfg.viewDistanceM = vd;
                    cfg.depthFormat = fmt;
                    cfg.cullPattern = CullPattern::HZB_MIPCHAIN;
                    configs.push_back(cfg);
                }
            }
        }
    }

    std::cout << "Total configs: " << configs.size()
              << " (4 scenes × 3 res × 3 dist × 3 formats)" << std::endl;

    auto t0 = std::chrono::high_resolution_clock::now();
    auto results = runAnalyticalBenchmark(configs, warmup, iterations);
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "Done in " << elapsed << " s\n\n";

    std::ofstream csv(outputPath);
    csv << "scene,format,reverse_z,width,height,view_distance_m,"
        << "vram_depth_mib,vram_hzb_mib,vram_total_mib,"
        << "psnr_db,mean_cull_error,false_culled,total_boxes\n";
    for (const auto& r : results) {
        bool reverseZ = (r.config.depthFormat == DepthFormat::D16_UNORM_REVERSE_Z);
        auto vram = computeVram(r.config.width, r.config.height, r.config.depthFormat);
        csv << sceneName(r.config.sceneIdx) << ","
            << depthFormatName(r.config.depthFormat) << ","
            << (reverseZ ? "1" : "0") << ","
            << r.config.width << ","
            << r.config.height << ","
            << r.config.viewDistanceM << ","
            << vram.depthAttachmentMiB << ","
            << vram.hzbMipchainMiB << ","
            << vram.totalMiB << ","
            << r.psnrDB << ","
            << r.meanCullError << ","
            << r.falseCulled << ","
            << r.totalBoxes << "\n";
    }
    csv.close();

    std::cout << "Summary (mean across iters):\n";
    std::cout << "scene     format                res       vd_m  vram_mib  psnr_db  cull_err  false/total\n";
    for (const auto& r : results) {
        std::printf("%-9s %-20s %4dx%-4d %4d  %7.2f  %7.2f  %9.6f  %d/%d\n",
            sceneName(r.config.sceneIdx),
            depthFormatName(r.config.depthFormat),
            r.config.width, r.config.height,
            r.config.viewDistanceM,
            r.vramMiB,
            r.psnrDB,
            r.meanCullError,
            r.falseCulled, r.totalBoxes);
    }
    std::cout << "\nCSV written to: " << outputPath << std::endl;
    return 0;
}
