#include "scenes.hpp"
#include "sdf_overlay.hpp"
#include "vct_cone_march.hpp"
#include "physics_normals.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace sdf_hybrid;

// Compute mean + std for timing samples.
struct Stats {
    double mean_us;
    double std_us;
    double min_us;
    double max_us;
};

Stats compute_stats(const std::vector<double>& samples) {
    Stats s{};
    if (samples.empty()) return s;
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean_us = sum / samples.size();
    s.min_us = *std::min_element(samples.begin(), samples.end());
    s.max_us = *std::max_element(samples.begin(), samples.end());
    double var = 0.0;
    for (double v : samples) var += (v - s.mean_us) * (v - s.mean_us);
    s.std_us = std::sqrt(var / samples.size());
    return s;
}

struct Config {
    scenes::SceneKind   scene;
    std::uint32_t       seed;
    sdf::SdfEncoding    encoding;
    sdf::SdfBuild       build;
    vct::TermStrategy   term;
    int                 iters;
    int                 cones;
};

void print_usage(const char* prog) {
    std::fprintf(stderr,
        "Usage: %s [options]\n"
        "  --scene <name>      one of: uniform_air, uniform_floor, forest_floor, cave_stress, mixed_biome\n"
        "  --seed  <N>         scene seed (default: 42)\n"
        "  --encoding <name>   one of: A_None, B_R8_1byte, C_R8_4quant, D_RLE_NoneSparse\n"
        "  --build <name>      one of: J_JFA_GPU, K_BruteForce_BFS, L_AdaptiveMultiRes\n"
        "  --term <name>       one of: T_VoxelDiscrete, T_SDFSmooth, T_Hybrid\n"
        "  --iters <N>         iterations per measurement (default: 1000)\n"
        "  --cones <N>         cone count for VCT (default: 6; use 1024 for reference)\n"
        "  --csv <file>        append result to CSV file\n"
        "  --full              run all combinations (slow)\n"
        "\n",
        prog);
}

bool parse_scene(const std::string& s, scenes::SceneKind& out) {
    if (s == "uniform_air")   { out = scenes::SceneKind::UniformAir;   return true; }
    if (s == "uniform_floor") { out = scenes::SceneKind::UniformFloor; return true; }
    if (s == "forest_floor")  { out = scenes::SceneKind::ForestFloor;  return true; }
    if (s == "cave_stress")   { out = scenes::SceneKind::CaveStress;   return true; }
    if (s == "mixed_biome")   { out = scenes::SceneKind::MixedBiome;   return true; }
    return false;
}

int main(int argc, char** argv) {
    Config cfg{};
    cfg.scene = scenes::SceneKind::CaveStress;
    cfg.seed = 42;
    cfg.encoding = sdf::SdfEncoding::B_R8_1byte;
    cfg.build = sdf::SdfBuild::J_JFA_GPU;
    cfg.term = vct::TermStrategy::T_Hybrid;
    cfg.iters = 1000;
    cfg.cones = 6;
    std::string csv_path;
    bool full_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--scene" && i + 1 < argc) {
            if (!parse_scene(argv[++i], cfg.scene)) {
                std::fprintf(stderr, "Unknown scene: %s\n", argv[i]);
                return 1;
            }
        } else if (a == "--seed" && i + 1 < argc) {
            cfg.seed = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (a == "--encoding" && i + 1 < argc) {
            std::string s = argv[++i];
            if (s == "A_None") cfg.encoding = sdf::SdfEncoding::A_None;
            else if (s == "B_R8_1byte") cfg.encoding = sdf::SdfEncoding::B_R8_1byte;
            else if (s == "C_R8_4quant") cfg.encoding = sdf::SdfEncoding::C_R8_4quant;
            else if (s == "D_RLE_NoneSparse") cfg.encoding = sdf::SdfEncoding::D_RLE_NoneSparse;
            else { std::fprintf(stderr, "Unknown encoding: %s\n", s.c_str()); return 1; }
        } else if (a == "--build" && i + 1 < argc) {
            std::string s = argv[++i];
            if (s == "J_JFA_GPU") cfg.build = sdf::SdfBuild::J_JFA_GPU;
            else if (s == "K_BruteForce_BFS") cfg.build = sdf::SdfBuild::K_BruteForce_BFS;
            else if (s == "L_AdaptiveMultiRes") cfg.build = sdf::SdfBuild::L_AdaptiveMultiRes;
            else { std::fprintf(stderr, "Unknown build: %s\n", s.c_str()); return 1; }
        } else if (a == "--term" && i + 1 < argc) {
            std::string s = argv[++i];
            if (s == "T_VoxelDiscrete") cfg.term = vct::TermStrategy::T_VoxelDiscrete;
            else if (s == "T_SDFSmooth") cfg.term = vct::TermStrategy::T_SDFSmooth;
            else if (s == "T_Hybrid") cfg.term = vct::TermStrategy::T_Hybrid;
            else { std::fprintf(stderr, "Unknown term: %s\n", s.c_str()); return 1; }
        } else if (a == "--iters" && i + 1 < argc) {
            cfg.iters = std::stoi(argv[++i]);
        } else if (a == "--cones" && i + 1 < argc) {
            cfg.cones = std::stoi(argv[++i]);
        } else if (a == "--csv" && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (a == "--full") {
            full_mode = true;
        } else if (a == "--help" || a == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", a.c_str());
            print_usage(argv[0]);
            return 1;
        }
    }

    // Build scene.
    auto voxels = scenes::generate(cfg.scene, cfg.seed);

    // Find first air voxel as fragment origin (avoid always-hit bug for solid-fragment origins).
    int origin_x = 4, origin_y = 4, origin_z = 4;
    bool found_air = false;
    for (int z = 0; z < static_cast<int>(scenes::CHUNK_SIZE) && !found_air; ++z) {
        for (int y = 0; y < static_cast<int>(scenes::CHUNK_SIZE) && !found_air; ++y) {
            for (int x = 0; x < static_cast<int>(scenes::CHUNK_SIZE) && !found_air; ++x) {
                if (voxels[scenes::idx3(x, y, z)] == 0) {
                    origin_x = x; origin_y = y; origin_z = z;
                    found_air = true;
                }
            }
        }
    }
    vct::Vec3 fragment_origin = {static_cast<float>(origin_x),
                                  static_cast<float>(origin_y),
                                  static_cast<float>(origin_z)};

    // Build SDF (skip for A_None).
    sdf::SdfR8 sdf_arr{};
    std::vector<double> sdf_build_samples;
    sdf_build_samples.reserve(cfg.iters);
    if (cfg.encoding != sdf::SdfEncoding::A_None) {
        for (int i = 0; i < cfg.iters; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            sdf::generate(cfg.build, voxels, sdf_arr);
            auto t1 = std::chrono::high_resolution_clock::now();
            sdf_build_samples.push_back(
                std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
    }
    Stats sdf_stats = compute_stats(sdf_build_samples);

    // Skip T_SDFSmooth / T_Hybrid when A_None: SDF not generated, would always produce
    // (incorrect) hits. Treat as N/A = measure with T_VoxelDiscrete fallback.
    vct::TermStrategy effective_term = cfg.term;
    if (cfg.encoding == sdf::SdfEncoding::A_None &&
        (cfg.term == vct::TermStrategy::T_SDFSmooth ||
         cfg.term == vct::TermStrategy::T_Hybrid)) {
        effective_term = vct::TermStrategy::T_VoxelDiscrete;
    }

    // VCT cone-march measurement.
    std::vector<double> march_samples;
    march_samples.reserve(cfg.iters);
    float total_hit_count = 0.0f;
    for (int i = 0; i < cfg.iters; ++i) {
        vct::Vec3 normal = {0, 1, 0};
        auto t0 = std::chrono::high_resolution_clock::now();
        vct::FragmentResult fr = vct::aggregate_cones(
            effective_term, voxels, sdf_arr, normal, cfg.cones, 16.0f, fragment_origin);
        auto t1 = std::chrono::high_resolution_clock::now();
        march_samples.push_back(
            std::chrono::duration<double, std::micro>(t1 - t0).count());
        total_hit_count += vct::irradiance_proxy(fr, cfg.cones);
    }
    Stats march_stats = compute_stats(march_samples);
    float mean_irradiance = total_hit_count / cfg.iters;

    // PSNR vs 1024-cone reference (run once per config).
    vct::FragmentResult ref = vct::aggregate_cones(
        effective_term, voxels, sdf_arr, {0, 1, 0}, 1024, 16.0f, fragment_origin);
    float ref_irradiance = vct::irradiance_proxy(ref, 1024);
    float psnr_val = vct::psnr(mean_irradiance, ref_irradiance);

    // VRAM.
    std::size_t vram = sdf::vram_bytes(cfg.encoding, voxels);

    // Physics normal: measure all standard contacts.
    double voxel_normal_err_sum = 0.0;
    double sdf_normal_err_sum = 0.0;
    if (cfg.encoding != sdf::SdfEncoding::A_None) {
        for (const auto& cp : physics::STANDARD_CONTACTS) {
            auto cr = physics::measure_contact(voxels, sdf_arr, cp);
            voxel_normal_err_sum += cr.voxel_angular_error;
            sdf_normal_err_sum += cr.sdf_angular_error;
        }
    }
    double mean_voxel_normal_err = voxel_normal_err_sum / physics::STANDARD_CONTACTS.size();
    double mean_sdf_normal_err = sdf_normal_err_sum / physics::STANDARD_CONTACTS.size();

    // Output CSV row.
    std::ostringstream row;
    row << scenes::scene_name(cfg.scene) << ","
        << cfg.seed << ","
        << sdf::encoding_name(cfg.encoding) << ","
        << sdf::build_name(cfg.build) << ","
        << vct::term_name(cfg.term) << ","
        << cfg.cones << ","
        << cfg.iters << ","
        << (sdf_build_samples.empty() ? 0.0 : sdf_stats.mean_us) << ","
        << (sdf_build_samples.empty() ? 0.0 : sdf_stats.std_us) << ","
        << march_stats.mean_us << ","
        << march_stats.std_us << ","
        << vram << ","
        << mean_irradiance << ","
        << ref_irradiance << ","
        << psnr_val << ","
        << mean_voxel_normal_err << ","
        << mean_sdf_normal_err << "\n";

    if (!csv_path.empty()) {
        std::ofstream f(csv_path, std::ios::app);
        if (f.is_open()) {
            f << row.str();
        }
    } else {
        std::printf("scene,seed,encoding,build,term,cones,iters,sdf_build_us,sdf_std_us,march_us,march_std_us,vram_bytes,irradiance,ref_irradiance,psnr,voxel_normal_err,sdf_normal_err\n");
        std::printf("%s", row.str().c_str());
    }

    if (full_mode) {
        // Print human-readable summary.
        std::printf("\n[Summary]\n");
        std::printf("  Scene:      %s (seed %u)\n", scenes::scene_name(cfg.scene), cfg.seed);
        std::printf("  Encoding:   %s\n", sdf::encoding_name(cfg.encoding));
        std::printf("  Build:      %s\n", sdf::build_name(cfg.build));
        std::printf("  Term:       %s\n", vct::term_name(cfg.term));
        std::printf("  Cones:      %d (reference: 1024)\n", cfg.cones);
        std::printf("  Iters:      %d\n", cfg.iters);
        if (!sdf_build_samples.empty()) {
            std::printf("  SDF build:  %.3f µs (std %.3f, min %.3f, max %.3f)\n",
                sdf_stats.mean_us, sdf_stats.std_us, sdf_stats.min_us, sdf_stats.max_us);
        } else {
            std::printf("  SDF build:  (skipped, A_None encoding)\n");
        }
        std::printf("  Cone march: %.3f µs (std %.3f, min %.3f, max %.3f)\n",
            march_stats.mean_us, march_stats.std_us, march_stats.min_us, march_stats.max_us);
        std::printf("  VRAM:       %zu bytes/chunk\n", vram);
        std::printf("  Irradiance: %.4f (ref %.4f) → PSNR %.2f dB\n",
            mean_irradiance, ref_irradiance, psnr_val);
        std::printf("  Normal err: voxel %.2f° / SDF %.2f° (lower = smoother)\n",
            mean_voxel_normal_err, mean_sdf_normal_err);
    }

    return 0;
}
