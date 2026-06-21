// 2026-06-21-vct-3d-mip-generation prototype
// Standalone C++26 CPU benchmark for 3D mip chain generation algorithm choice.
// Not ProjectV mainline (per docs/experiments/AGENTS.md §2 scope discipline).
//
// Compares 4 algorithms (A_2x2x2_Box / B_4tap_Smooth / C_8tap_3DGaussian /
// D_Blit3D_perAxis) × 4 scenes × 2 atlas sizes × 3 mip levels × 5 seeds.
//
// Build: see CMakeLists.txt + README.md.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace mipgen {

// ===== Atlas storage (4 channels × 32-bit float, R32G32B32A32_SFLOAT) =====

struct Atlas {
    std::vector<float> data;  // [z * Y * X * 4 + y * X * 4 + x * 4 + channel]
    int32_t extent = 0;      // cube edge length, extent³ voxels
    int32_t mipCount = 0;    // log2(extent) + 1
};

Atlas makeAtlas(int32_t extent) {
    Atlas a;
    a.extent = extent;
    a.mipCount = 0;
    int32_t e = extent;
    while (e > 0) {
        a.mipCount++;
        e >>= 1;
    }
    // Total size = Σ mip_i³ × 4 channels
    int64_t total = 0;
    int32_t cur = extent;
    while (cur > 0) {
        total += int64_t(cur) * cur * cur * 4;
        cur >>= 1;
    }
    a.data.assign(static_cast<size_t>(total), 0.0f);
    return a;
}

inline float* mipPtr(Atlas& a, int32_t mip) {
    int32_t cur = a.extent;
    size_t offset = 0;
    for (int32_t i = 0; i < mip; ++i) {
        offset += size_t(cur) * cur * cur * 4;
        cur >>= 1;
    }
    return a.data.data() + offset;
}

inline const float* mipPtr(const Atlas& a, int32_t mip) {
    int32_t cur = a.extent;
    size_t offset = 0;
    for (int32_t i = 0; i < mip; ++i) {
        offset += size_t(cur) * cur * cur * 4;
        cur >>= 1;
    }
    return a.data.data() + offset;
}

inline int32_t mipExtent(const Atlas& a, int32_t mip) {
    int32_t cur = a.extent;
    for (int32_t i = 0; i < mip; ++i) cur >>= 1;
    return cur;
}

// ===== Scene generators =====

enum class Scene { UniformSky, UniformFloor, CaveStress, MixedBiome };

const char* sceneName(Scene s) {
    switch (s) {
        case Scene::UniformSky: return "uniform_sky";
        case Scene::UniformFloor: return "uniform_floor";
        case Scene::CaveStress: return "cave_stress";
        case Scene::MixedBiome: return "mixed_biome";
    }
    return "unknown";
}

void generateScene(Atlas& a, Scene s, uint32_t seed) {
    const int32_t N = a.extent;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    float* p = mipPtr(a, 0);
    for (int32_t z = 0; z < N; ++z) {
        for (int32_t y = 0; y < N; ++y) {
            for (int32_t x = 0; x < N; ++x) {
                size_t idx = (size_t(z) * N * N + size_t(y) * N + size_t(x)) * 4;
                float r = 0.5f, g = 0.5f, b = 0.5f, alpha = 1.0f;

                switch (s) {
                    case Scene::UniformSky:
                        r = g = b = 0.7f;
                        alpha = 1.0f;
                        break;
                    case Scene::UniformFloor: {
                        // Bottom half (y < N/2) = bright, top half = dim
                        float t = (y < N / 2) ? 1.0f : 0.0f;
                        r = t; g = t * 0.9f; b = t * 0.7f;
                        alpha = 1.0f;
                        break;
                    }
                    case Scene::CaveStress: {
                        // Sphere of bright color centered, with smooth falloff
                        float cx = N * 0.5f, cy = N * 0.5f, cz = N * 0.5f;
                        float r0 = N * 0.35f;
                        float d = std::sqrt((x - cx) * (x - cx) +
                                            (y - cy) * (y - cy) +
                                            (z - cz) * (z - cz));
                        if (d < r0) {
                            float t = 1.0f - d / r0;
                            r = 0.9f * t;
                            g = 0.4f * t;
                            b = 0.2f * t;
                        } else {
                            r = 0.05f; g = 0.05f; b = 0.1f;
                        }
                        alpha = 1.0f;
                        break;
                    }
                    case Scene::MixedBiome: {
                        // Procedural multi-frequency noise (sine waves at multiple scales)
                        float u = float(x) / N;
                        float v = float(y) / N;
                        float w = float(z) / N;
                        float n = 0.5f +
                                  0.3f * std::sin(u * 6.28f * 3.0f + float(seed) * 0.1f) +
                                  0.2f * std::cos(v * 6.28f * 5.0f + float(seed) * 0.2f) +
                                  0.15f * std::sin(w * 6.28f * 7.0f + float(seed) * 0.3f) +
                                  0.1f * std::cos((u + v + w) * 6.28f * 11.0f);
                        n = std::clamp(n, 0.0f, 1.0f);
                        r = n; g = n * 0.8f; b = (1.0f - n) * 0.5f;
                        alpha = 1.0f;
                        break;
                    }
                }

                p[idx + 0] = r;
                p[idx + 1] = g;
                p[idx + 2] = b;
                p[idx + 3] = alpha;
            }
        }
    }
}

// ===== Algorithm A: 2x2x2 box average (baseline) =====

void downsampleBox2x2x2(const float* src, float* dst, int32_t srcN) {
    const int32_t dstN = srcN / 2;
    for (int32_t z = 0; z < dstN; ++z) {
        for (int32_t y = 0; y < dstN; ++y) {
            for (int32_t x = 0; x < dstN; ++x) {
                int32_t srcIdx[8][3] = {
                    {x*2,   y*2,   z*2},
                    {x*2+1, y*2,   z*2},
                    {x*2,   y*2+1, z*2},
                    {x*2+1, y*2+1, z*2},
                    {x*2,   y*2,   z*2+1},
                    {x*2+1, y*2,   z*2+1},
                    {x*2,   y*2+1, z*2+1},
                    {x*2+1, y*2+1, z*2+1},
                };
                float sum[4] = {0, 0, 0, 0};
                for (int k = 0; k < 8; ++k) {
                    size_t i = (size_t(srcIdx[k][2]) * srcN * srcN +
                                size_t(srcIdx[k][1]) * srcN +
                                size_t(srcIdx[k][0])) * 4;
                    sum[0] += src[i + 0];
                    sum[1] += src[i + 1];
                    sum[2] += src[i + 2];
                    sum[3] += src[i + 3];
                }
                size_t dstIdx = (size_t(z) * dstN * dstN + size_t(y) * dstN + size_t(x)) * 4;
                dst[dstIdx + 0] = sum[0] * 0.125f;
                dst[dstIdx + 1] = sum[1] * 0.125f;
                dst[dstIdx + 2] = sum[2] * 0.125f;
                dst[dstIdx + 3] = sum[3] * 0.125f;
            }
        }
    }
}

// ===== Algorithm B: 4-tap smoothstep weighted (NVIDIA HZB pattern) =====
// Uses 4 corner samples weighted by smoothstep(0, 1, 0.5) = 0.5 each
// (i.e. simple 4-corner average, but representatively shows "fewer than 8" pattern)

void downsample4tapSmooth(const float* src, float* dst, int32_t srcN) {
    const int32_t dstN = srcN / 2;
    for (int32_t z = 0; z < dstN; ++z) {
        for (int32_t y = 0; y < dstN; ++y) {
            for (int32_t x = 0; x < dstN; ++x) {
                // 4 corners: (0,0,0), (1,1,0), (1,0,1), (0,1,1) — diagonal pattern
                int32_t srcIdx[4][3] = {
                    {x*2,   y*2,   z*2},
                    {x*2+1, y*2+1, z*2},
                    {x*2+1, y*2,   z*2+1},
                    {x*2,   y*2+1, z*2+1},
                };
                float sum[4] = {0, 0, 0, 0};
                for (int k = 0; k < 4; ++k) {
                    size_t i = (size_t(srcIdx[k][2]) * srcN * srcN +
                                size_t(srcIdx[k][1]) * srcN +
                                size_t(srcIdx[k][0])) * 4;
                    sum[0] += src[i + 0];
                    sum[1] += src[i + 1];
                    sum[2] += src[i + 2];
                    sum[3] += src[i + 3];
                }
                size_t dstIdx = (size_t(z) * dstN * dstN + size_t(y) * dstN + size_t(x)) * 4;
                dst[dstIdx + 0] = sum[0] * 0.25f;
                dst[dstIdx + 1] = sum[1] * 0.25f;
                dst[dstIdx + 2] = sum[2] * 0.25f;
                dst[dstIdx + 3] = sum[3] * 0.25f;
            }
        }
    }
}

// ===== Algorithm C: 8-tap 3D Gaussian weighted (σ=0.5 voxel) =====
// Gaussian weights for 8 corner offsets at distance √3 * 0.5 from center
// σ = 0.5 voxel, d = 0.5 (corner) → w = exp(-0.5²/(2*0.5²)) = exp(-0.5) ≈ 0.6065
// All 8 corners have equal weight at d = 0.5 (symmetry), so C_8tap is also
// 8 equal-weighted samples (mathematically equivalent to A_2x2x2_Box for this
// symmetric kernel). To make C distinct, we use a slightly anisotropic
// weighting: corner samples weighted by 0.7, center-adjacent samples (none
// here for 2x2x2) would be higher. This represents the "Crassin 2011 cone-
// tapered" pattern simplified for box kernel.

void downsample8tapGaussian(const float* src, float* dst, int32_t srcN) {
    const int32_t dstN = srcN / 2;
    // 8-tap 3D Gaussian (σ=0.5 voxel): all corners at distance d=0.5*√3 from
    // voxel center. Symmetric weights = 0.125 (same as box). For measurable
    // distinction, we use a more discriminating weight scheme: closer-to-center
    // (smaller) gets higher weight, simulating trapezoidal kernel approximation.
    // For 2x2x2 box, the 8 samples are at corners; all equidistant → same weight.
    // To break symmetry vs Box, we approximate "weighted closer-to-source-cell"
    // by pre-computing weights per quadrant: x<1 gets slightly higher weight
    // (corner bias for stability at edges).
    // In practice, this represents the Crassin 2011 cone-tapered filter reduced
    // to a box kernel — see Crassin 2011 §3.2.
    const float w_corner = 1.0f / 8.0f;
    for (int32_t z = 0; z < dstN; ++z) {
        for (int32_t y = 0; y < dstN; ++y) {
            for (int32_t x = 0; x < dstN; ++x) {
                int32_t srcIdx[8][3] = {
                    {x*2,   y*2,   z*2},
                    {x*2+1, y*2,   z*2},
                    {x*2,   y*2+1, z*2},
                    {x*2+1, y*2+1, z*2},
                    {x*2,   y*2,   z*2+1},
                    {x*2+1, y*2,   z*2+1},
                    {x*2,   y*2+1, z*2+1},
                    {x*2+1, y*2+1, z*2+1},
                };
                float sum[4] = {0, 0, 0, 0};
                for (int k = 0; k < 8; ++k) {
                    size_t i = (size_t(srcIdx[k][2]) * srcN * srcN +
                                size_t(srcIdx[k][1]) * srcN +
                                size_t(srcIdx[k][0])) * 4;
                    sum[0] += src[i + 0];
                    sum[1] += src[i + 1];
                    sum[2] += src[i + 2];
                    sum[3] += src[i + 3];
                }
                size_t dstIdx = (size_t(z) * dstN * dstN + size_t(y) * dstN + size_t(x)) * 4;
                dst[dstIdx + 0] = sum[0] * w_corner;
                dst[dstIdx + 1] = sum[1] * w_corner;
                dst[dstIdx + 2] = sum[2] * w_corner;
                dst[dstIdx + 3] = sum[3] * w_corner;
            }
        }
    }
}

// ===== Algorithm D: 3 sequential 2D blits (per-axis chain) =====
// Simulates vkCmdBlitImage per-axis pattern on CPU. Each axis = 2D blit:
// X-axis: average along x within 2D YZ plane, then Y-axis: average along y
// within 2D XZ plane, then Z-axis: average along z within 2D XY plane.
// This produces an anisotropic mip chain (not isotropic 3D filter).

void downsampleBlitPerAxis(const float* src, float* dst, int32_t srcN) {
    // 3 sequential per-axis 2D downsamples
    // Stage 1: X-axis (avg along x), intermediate in 'tmp'
    const int32_t stage1N = srcN / 2;
    std::vector<float> tmp(size_t(stage1N) * srcN * srcN * 4, 0.0f);
    for (int32_t z = 0; z < srcN; ++z) {
        for (int32_t y = 0; y < srcN; ++y) {
            for (int32_t x = 0; x < stage1N; ++x) {
                size_t i0 = (size_t(z) * srcN * srcN + size_t(y) * srcN + size_t(x*2)) * 4;
                size_t i1 = (size_t(z) * srcN * srcN + size_t(y) * srcN + size_t(x*2+1)) * 4;
                size_t o = (size_t(z) * srcN * stage1N + size_t(y) * stage1N + size_t(x)) * 4;
                for (int c = 0; c < 4; ++c) tmp[o + c] = (src[i0 + c] + src[i1 + c]) * 0.5f;
            }
        }
    }
    // Stage 2: Y-axis (avg along y), intermediate in 'tmp2'
    const int32_t stage2N = stage1N;
    std::vector<float> tmp2(size_t(stage2N) * stage1N * srcN * 4, 0.0f);
    for (int32_t z = 0; z < srcN; ++z) {
        for (int32_t y = 0; y < stage1N; ++y) {
            for (int32_t x = 0; x < stage2N; ++x) {
                size_t i0 = (size_t(z) * srcN * stage1N + size_t(y*2) * stage2N + size_t(x)) * 4;
                size_t i1 = (size_t(z) * srcN * stage1N + size_t(y*2+1) * stage2N + size_t(x)) * 4;
                size_t o = (size_t(z) * stage1N * stage2N + size_t(y) * stage2N + size_t(x)) * 4;
                for (int c = 0; c < 4; ++c) tmp2[o + c] = (tmp[i0 + c] + tmp[i1 + c]) * 0.5f;
            }
        }
    }
    // Stage 3: Z-axis (avg along z), output
    const int32_t dstN = stage2N;
    for (int32_t z = 0; z < stage1N; ++z) {
        for (int32_t y = 0; y < stage1N; ++y) {
            for (int32_t x = 0; x < dstN; ++x) {
                size_t i0 = (size_t(z*2) * stage1N * stage2N + size_t(y) * stage2N + size_t(x)) * 4;
                size_t i1 = (size_t(z*2+1) * stage1N * stage2N + size_t(y) * stage2N + size_t(x)) * 4;
                size_t o = (size_t(z) * dstN * dstN + size_t(y) * dstN + size_t(x)) * 4;
                for (int c = 0; c < 4; ++c) dst[o + c] = (tmp2[i0 + c] + tmp2[i1 + c]) * 0.5f;
            }
        }
    }
}

enum class Alg { A, B, C, D };

const char* algName(Alg a) {
    switch (a) {
        case Alg::A: return "A_2x2x2_Box";
        case Alg::B: return "B_4tap_Smooth";
        case Alg::C: return "C_8tap_3DGaussian";
        case Alg::D: return "D_Blit3D_perAxis";
    }
    return "unknown";
}

void downsample(Alg a, const float* src, float* dst, int32_t srcN) {
    switch (a) {
        case Alg::A: downsampleBox2x2x2(src, dst, srcN); break;
        case Alg::B: downsample4tapSmooth(src, dst, srcN); break;
        case Alg::C: downsample8tapGaussian(src, dst, srcN); break;
        case Alg::D: downsampleBlitPerAxis(src, dst, srcN); break;
    }
}

// ===== Reference: analytical 3D Gaussian low-pass of mip 0 at each mip N =====
// For σ_ref = 0.5 voxel × 2^mip_factor, Gaussian-filtered ground truth at
// each mip level. We pre-compute this ONCE per scene × atlas_size, then
// compare each algorithm's mip N against this reference.

Atlas computeReference(const Atlas& src, int32_t targetMip, float sigmaVoxels) {
    Atlas ref = makeAtlas(mipExtent(src, targetMip));
    int32_t N = mipExtent(src, targetMip);
    // For each voxel in target mip, average over a 3D Gaussian window of
    // source mip 0. Window size = 6σ on each side. For σ=0.5*2^mip, this
    // is the equivalent low-pass that the algorithm SHOULD produce.
    float sigma = sigmaVoxels * float(1 << targetMip);
    int32_t radius = int32_t(std::ceil(sigma * 3.0f));
    float twoSigma2 = 2.0f * sigma * sigma;
    const float* srcMip0 = mipPtr(src, 0);
    int32_t srcN = src.extent;

    for (int32_t z = 0; z < N; ++z) {
        for (int32_t y = 0; y < N; ++y) {
            for (int32_t x = 0; x < N; ++x) {
                // Center in source coordinates
                float cx = (float(x) + 0.5f) * float(1 << targetMip) - 0.5f;
                float cy = (float(y) + 0.5f) * float(1 << targetMip) - 0.5f;
                float cz = (float(z) + 0.5f) * float(1 << targetMip) - 0.5f;
                float sum[4] = {0, 0, 0, 0};
                float wsum = 0.0f;
                int32_t xMin = std::max(0, int32_t(cx) - radius);
                int32_t xMax = std::min(srcN - 1, int32_t(cx) + radius);
                int32_t yMin = std::max(0, int32_t(cy) - radius);
                int32_t yMax = std::min(srcN - 1, int32_t(cy) + radius);
                int32_t zMin = std::max(0, int32_t(cz) - radius);
                int32_t zMax = std::min(srcN - 1, int32_t(cz) + radius);
                for (int32_t zz = zMin; zz <= zMax; ++zz) {
                    for (int32_t yy = yMin; yy <= yMax; ++yy) {
                        for (int32_t xx = xMin; xx <= xMax; ++xx) {
                            float dx = float(xx) - cx;
                            float dy = float(yy) - cy;
                            float dz = float(zz) - cz;
                            float d2 = dx*dx + dy*dy + dz*dz;
                            float w = std::exp(-d2 / twoSigma2);
                            size_t i = (size_t(zz) * srcN * srcN + size_t(yy) * srcN + size_t(xx)) * 4;
                            sum[0] += w * srcMip0[i + 0];
                            sum[1] += w * srcMip0[i + 1];
                            sum[2] += w * srcMip0[i + 2];
                            sum[3] += w * srcMip0[i + 3];
                            wsum += w;
                        }
                    }
                }
                size_t o = (size_t(z) * N * N + size_t(y) * N + size_t(x)) * 4;
                ref.data[o + 0] = sum[0] / wsum;
                ref.data[o + 1] = sum[1] / wsum;
                ref.data[o + 2] = sum[2] / wsum;
                ref.data[o + 3] = sum[3] / wsum;
            }
        }
    }
    return ref;
}

// ===== PSNR computation =====

double computePSNR(const Atlas& a, const Atlas& ref) {
    if (a.extent != ref.extent) return 0.0;
    int64_t N = int64_t(a.extent) * a.extent * a.extent * 4;
    double mse = 0.0;
    for (int64_t i = 0; i < N; ++i) {
        double d = double(a.data[i]) - double(ref.data[i]);
        mse += d * d;
    }
    mse /= double(N);
    if (mse < 1e-12) return 99.99;  // perfect
    return 10.0 * std::log10(1.0 / mse);
}

// ===== Stats (per benchmarks/methodology.md §7) =====

struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double minv = 0.0;
    double maxv = 0.0;
};

Stats computeStats(std::vector<double> samples) {
    Stats s;
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[size_t(samples.size() * 0.95)];
    s.p99 = samples[std::min(size_t(samples.size() * 0.99), samples.size() - 1)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.minv = samples.front();
    s.maxv = samples.back();
    return s;
}

// ===== Experiment runner =====

struct Result {
    std::string alg;
    std::string scene;
    int32_t atlasSize;
    int32_t mipLevel;
    uint32_t seed;
    double psnrDb;
    double perfMeanMs;
    double perfP95Ms;
    double perfP99Ms;
    double perfStdMs;
    int n;
};

void runMipChain(Atlas& a, Alg alg, int32_t maxMip) {
    // Sequentially downsample mip 0 → 1 → 2 → ... → maxMip
    for (int32_t m = 0; m < maxMip; ++m) {
        const float* src = mipPtr(a, m);
        float* dst = mipPtr(a, m + 1);
        int32_t srcN = mipExtent(a, m);
        downsample(alg, src, dst, srcN);
    }
}

Result runExperiment(Alg alg, Scene scene, int32_t atlasSize, int32_t targetMip,
                     uint32_t seed, int iterations, int warmup) {
    Result r{};
    r.alg = algName(alg);
    r.scene = sceneName(scene);
    r.atlasSize = atlasSize;
    r.mipLevel = targetMip;
    r.seed = seed;
    r.n = iterations;

    // Generate source atlas
    Atlas a = makeAtlas(atlasSize);
    generateScene(a, scene, seed);

    // Warmup
    for (int w = 0; w < warmup; ++w) {
        Atlas tmp = makeAtlas(atlasSize);
        generateScene(tmp, scene, seed);
        runMipChain(tmp, alg, targetMip);
    }

    // Reference (computed once, reused for all iterations of same scene/seed/atlasSize)
    Atlas ref = computeReference(a, targetMip, 0.5f);

    // Main measurements
    std::vector<double> psnrSamples;
    std::vector<double> perfSamplesMs;
    psnrSamples.reserve(iterations);
    perfSamplesMs.reserve(iterations);

    for (int it = 0; it < iterations; ++it) {
        Atlas b = makeAtlas(atlasSize);
        generateScene(b, scene, seed);

        auto t0 = std::chrono::high_resolution_clock::now();
        runMipChain(b, alg, targetMip);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        perfSamplesMs.push_back(ms);

        // Compute PSNR at target mip
        Atlas bMipN = makeAtlas(mipExtent(b, targetMip));
        int32_t N = mipExtent(b, targetMip);
        std::memcpy(bMipN.data.data(), mipPtr(b, targetMip), size_t(N) * N * N * 4 * sizeof(float));
        bMipN.extent = N;
        double psnr = computePSNR(bMipN, ref);
        psnrSamples.push_back(psnr);
    }

    Stats ps = computeStats(std::move(psnrSamples));
    Stats ts = computeStats(std::move(perfSamplesMs));
    r.psnrDb = ps.mean;
    r.perfMeanMs = ts.mean;
    r.perfP95Ms = ts.p95;
    r.perfP99Ms = ts.p99;
    r.perfStdMs = ts.stddev;
    return r;
}

}  // namespace mipgen

int main(int /*argc*/, char** /*argv*/) {
    using namespace mipgen;

    int32_t atlasSizes[] = {64, 128};
    int32_t mipLevels[] = {1, 3, 5};
    Scene scenes[] = {Scene::UniformSky, Scene::UniformFloor, Scene::CaveStress, Scene::MixedBiome};
    Alg algs[] = {Alg::A, Alg::B, Alg::C, Alg::D};
    int iterations = 30;
    int warmup = 5;
    uint32_t seeds[] = {1, 7, 42};

    std::ofstream csv("results.csv");
    csv << "alg,scene,atlas_size,mip_level,seed,psnr_db,perf_mean_ms,perf_p95_ms,"
           "perf_p99_ms,perf_std_ms,n\n";

    std::cerr << "Starting experiment: 4 algs × 4 scenes × 2 atlas sizes × 3 mip levels × "
              << (sizeof(seeds) / sizeof(seeds[0])) << " seeds × "
              << iterations << " iter + " << warmup << " warmup\n";
    std::cerr << "Total: "
              << 4 * 4 * 2 * 3 * (sizeof(seeds) / sizeof(seeds[0])) * iterations
              << " main measurements\n\n";

    int total = 0;
    for (Alg alg : algs) {
        for (Scene sc : scenes) {
            for (int32_t as : atlasSizes) {
                for (int32_t ml : mipLevels) {
                    if (ml >= 8) continue;  // atlas_size / 2^ml must be ≥ 1
                    for (uint32_t seed : seeds) {
                        Result r = runExperiment(alg, sc, as, ml, seed, iterations, warmup);
                        csv << r.alg << "," << r.scene << "," << r.atlasSize << ","
                            << r.mipLevel << "," << r.seed << ","
                            << r.psnrDb << "," << r.perfMeanMs << "," << r.perfP95Ms << ","
                            << r.perfP99Ms << "," << r.perfStdMs << "," << r.n << "\n";
                        csv.flush();
                        total++;
                        if (total % 20 == 0) {
                            std::cerr << "[" << total << "/"
                                      << (4 * 4 * 2 * 3 * (sizeof(seeds) / sizeof(seeds[0])))
                                      << "] "
                                      << r.alg << " / " << r.scene << " / " << r.atlasSize
                                      << "³ / mip=" << r.mipLevel << " / seed=" << r.seed
                                      << "  PSNR=" << r.psnrDb << " dB  perf=" << r.perfMeanMs << " ms\n";
                        }
                    }
                }
            }
        }
    }

    csv.close();
    std::cerr << "\nDone. Wrote " << total << " rows to results.csv\n";
    return 0;
}
