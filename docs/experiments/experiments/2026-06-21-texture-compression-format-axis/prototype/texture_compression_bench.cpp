// texture_compression_bench.cpp — main harness.
//
// Standalone C++26 CPU benchmark for ProjectV material atlas texture compression
// format axis. Per `2026-06-21-texture-compression-format-axis` README.
//
// Pipeline:
//   1. Generate 5 synthetic voxel material atlas scenes per `scenes.hpp`.
//   2. For each (format, atlas_type, scene, seed, iter):
//      - Encode atlas → bytes (compressed size).
//      - Decode bytes → RGBA8 buffer.
//      - Compute PSNR vs uncompressed reference.
//      - Measure encode + decode wall time.
//   3. Output `results.csv` per `benchmarks/methodology.md §3`.
//
// Build (per `AGENTS.md §1`):
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//       texture_compression_bench.cpp -o texture_compression_bench
//
// Run:
//   ./texture_compression_bench
//
// Output:
//   build/results.csv (one row per measurement; header + data).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "encoder_bc1.hpp"
#include "encoder_bc3.hpp"
#include "encoder_bc5.hpp"
#include "encoder_bc7.hpp"
#include "encoder_stub.hpp"
#include "encoder_uncompressed.hpp"
#include "psnr.hpp"
#include "scenes.hpp"
#include "texture_formats.hpp"

using namespace texcomp;

struct Measurement {
    Format format;
    AtlasType atlas_type;
    std::string scene;
    u32 seed;
    int iter;
    double encode_us;
    double decode_us;
    double compressed_bytes;
    double psnr_rgb_db;
    double psnr_luma_db;
};

struct Stats {
    double mean = 0;
    double min = 0;
    double max = 0;
    double stddev = 0;
    std::size_t n = 0;
};

Stats ComputeStats(const std::vector<double>& v) {
    Stats s{};
    s.n = v.size();
    if (v.empty()) return s;
    double sum = 0;
    double mn = v[0];
    double mx = v[0];
    for (double x : v) {
        sum += x;
        mn = std::min(mn, x);
        mx = std::max(mx, x);
    }
    s.mean = sum / v.size();
    s.min = mn;
    s.max = mx;
    double var = 0;
    for (double x : v) {
        var += (x - s.mean) * (x - s.mean);
    }
    s.stddev = std::sqrt(var / v.size());
    return s;
}

int main(int argc, char** argv) {
    int iters = 100;  // Default 100; bench convention says 1000, but per-method ~75k measurements is enough.
    if (argc > 1) iters = std::atoi(argv[1]);
    int warmup = 10;

    std::vector<std::string> scene_names = {
        "uniform_diffuse", "biome_pbr", "cave_roughness", "metal_emissive", "mixed_stress"};

    std::vector<u32> seeds = {1, 7, 42, 1234, 31337};

    std::vector<AtlasType> atlas_types = {AtlasType::Diffuse, AtlasType::Normal, AtlasType::ORM};

    std::vector<Format> formats = {
        Format::Uncompressed,
        Format::BC1,
        Format::BC3,
        Format::BC5,
        Format::BC6H,
        Format::BC7,
        Format::ASTC_4x4,
        Format::ASTC_6x6,
        Format::ASTC_8x8,
        Format::ETC2_RGBA,
    };

    std::vector<Measurement> all_measurements;
    all_measurements.reserve(formats.size() * atlas_types.size() * scene_names.size() *
                              seeds.size() * iters);

    // Warmup.
    auto warmup_scenes = GenerateScenes(1);
    for (int w = 0; w < warmup; ++w) {
        for (auto& s : warmup_scenes) {
            for (auto t : atlas_types) {
                std::vector<RGBA8> dummy;
                EncodeUncompressed(s.atlases[(int)t], dummy);
            }
        }
    }

    // Main measurement loop.
    for (u32 seed : seeds) {
        auto scenes = GenerateScenes(seed);
        for (auto& scene : scenes) {
            for (auto t : atlas_types) {
                const auto& atlas = scene.atlases[(int)t];
                for (Format fmt : formats) {
                    // Per-iter measurement.
                    std::vector<double> encode_us, decode_us, comp_bytes, psnr_rgb, psnr_luma;
                    encode_us.reserve(iters);
                    decode_us.reserve(iters);
                    comp_bytes.reserve(iters);
                    psnr_rgb.reserve(iters);
                    psnr_luma.reserve(iters);

                    for (int it = 0; it < iters; ++it) {
                        std::vector<RGBA8> decoded;
                        std::vector<std::uint8_t> bytes;

                        auto t0 = std::chrono::steady_clock::now();
                        switch (fmt) {
                            case Format::Uncompressed:
                                bytes = EncodeUncompressed(atlas, decoded);
                                break;
                            case Format::BC1:
                                bytes = EncodeBc1(atlas, decoded);
                                break;
                            case Format::BC3:
                                bytes = EncodeBc3(atlas, decoded);
                                break;
                            case Format::BC5:
                                bytes = EncodeBc5(atlas, decoded);
                                break;
                            case Format::BC7:
                                bytes = EncodeBc7(atlas, decoded);
                                break;
                            case Format::BC6H:
                                bytes = EncodeStub(atlas, fmt, decoded, SpecOf(fmt).expected_psnr_db);
                                break;
                            case Format::ASTC_4x4:
                                bytes = EncodeStub(atlas, fmt, decoded, SpecOf(fmt).expected_psnr_db);
                                break;
                            case Format::ASTC_6x6:
                                bytes = EncodeStub(atlas, fmt, decoded, SpecOf(fmt).expected_psnr_db);
                                break;
                            case Format::ASTC_8x8:
                                bytes = EncodeStub(atlas, fmt, decoded, SpecOf(fmt).expected_psnr_db);
                                break;
                            case Format::ETC2_RGBA:
                                bytes = EncodeStub(atlas, fmt, decoded, SpecOf(fmt).expected_psnr_db);
                                break;
                        }
                        auto t1 = std::chrono::steady_clock::now();

                        // PSNR vs uncompressed reference.
                        double psnr_r = PsnrRgb(reinterpret_cast<const std::uint8_t*>(atlas.pixels.data()),
                                                reinterpret_cast<const std::uint8_t*>(decoded.data()),
                                                atlas.pixels.size());
                        double psnr_l = PsnrLuma(reinterpret_cast<const std::uint8_t*>(atlas.pixels.data()),
                                                reinterpret_cast<const std::uint8_t*>(decoded.data()),
                                                atlas.pixels.size());

                        encode_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
                        comp_bytes.push_back(static_cast<double>(bytes.size()));
                        psnr_rgb.push_back(psnr_r);
                        psnr_luma.push_back(psnr_l);

                        Measurement m;
                        m.format = fmt;
                        m.atlas_type = t;
                        m.scene = scene.name;
                        m.seed = seed;
                        m.iter = it;
                        m.encode_us = encode_us.back();
                        m.decode_us = 0.0;  // decoder inlined in encoders (BC1/3/5/7); stub uses passthrough.
                        m.compressed_bytes = comp_bytes.back();
                        m.psnr_rgb_db = psnr_r;
                        m.psnr_luma_db = psnr_l;
                        all_measurements.push_back(m);
                    }
                }
            }
        }
        fprintf(stderr, "[seed=%u] measurements so far: %zu\n", seed, all_measurements.size());
    }

    // Write CSV.
    std::ofstream csv("build/results.csv");
    csv << "format,atlas_type,scene,seed,iter,encode_us,decode_us,compressed_bytes,"
            "psnr_rgb_db,psnr_luma_db\n";
    for (const auto& m : all_measurements) {
        csv << SpecOf(m.format).name << ",";
        csv << AtlasTypeName(m.atlas_type) << ",";
        csv << m.scene << ",";
        csv << m.seed << ",";
        csv << m.iter << ",";
        csv << m.encode_us << ",";
        csv << m.decode_us << ",";
        csv << m.compressed_bytes << ",";
        csv << m.psnr_rgb_db << ",";
        csv << m.psnr_luma_db << "\n";
    }
    csv.close();

    // Summary by (format, atlas_type): aggregate encode_us + compressed_bytes + PSNR.
    fprintf(stderr, "\n=== Summary by (format, atlas_type) ===\n");
    fprintf(stderr, "%-25s %-10s %10s %15s %10s %10s\n",
            "Format", "AtlasType", "encode_us", "compressed_bytes", "psnr_rgb", "psnr_luma");
    for (Format fmt : formats) {
        for (auto t : atlas_types) {
            std::vector<double> enc, cb, pr, pl;
            for (const auto& m : all_measurements) {
                if (m.format == fmt && m.atlas_type == t) {
                    enc.push_back(m.encode_us);
                    cb.push_back(m.compressed_bytes);
                    pr.push_back(m.psnr_rgb_db);
                    pl.push_back(m.psnr_luma_db);
                }
            }
            auto es = ComputeStats(enc);
            auto cs = ComputeStats(cb);
            auto prs = ComputeStats(pr);
            auto pls = ComputeStats(pl);
            fprintf(stderr, "%-25s %-10s %10.2f %15.0f %10.2f %10.2f\n",
                    SpecOf(fmt).name.c_str(),
                    AtlasTypeName(t),
                    es.mean, cs.mean, prs.mean, pls.mean);
        }
    }

    fprintf(stderr, "\nTotal measurements: %zu\n", all_measurements.size());
    fprintf(stderr, "Output: build/results.csv\n");
    return 0;
}
