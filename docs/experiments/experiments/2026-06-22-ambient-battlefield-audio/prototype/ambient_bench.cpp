// ambient_bench.cpp — C++26 standalone benchmark
// 5 ambient mixing strategies × 5 battlefield scenes × 5 seeds
// clang++ -O3 -march=native -std=c++26 -DNDEBUG -lpthread -o ambient_bench ambient_bench.cpp
// ./ambient_bench

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

// ============================================================
// Constants
// ============================================================
static constexpr int   kFrameSamples  = 128;
static constexpr int   kWarmupFrames  = 100;
static constexpr int   kMeasFrames    = 500;
static constexpr int   kSeeds         = 5;
static constexpr float kNearDist      = 30.0f;
static constexpr float kMidDist       = 100.0f;
static constexpr int   kPriorityCap   = 64;
static constexpr int   kNoAmbientCap  = 32;

static constexpr std::array<int, 5> kSceneSizes = {10, 50, 100, 150, 200};

static constexpr const char* kStrategyNames[] = {
    "A_NoAmbient", "B_Full3D_All", "C_Hybrid_LOD", "D_PriorityCap64", "E_SoA_MaxTP"
};

static constexpr const char* kSceneNames[] = {
    "fireteam10", "squad50", "platoon100", "company150", "battlefield200"
};

// ============================================================
// Types
// ============================================================
struct Vec3 { float x, y, z; };
struct Source { Vec3 pos; float priority; uint8_t type; float phase; };
struct Listener { Vec3 pos; };

struct SoA {
    std::vector<float> px, py, pz, prio, phases;
    int size() const { return (int)px.size(); }
};

static SoA to_soa(const std::vector<Source>& srcs) {
    SoA r;
    int n = (int)srcs.size();
    r.px.resize(n); r.py.resize(n); r.pz.resize(n);
    r.prio.resize(n); r.phases.resize(n);
    for (int i = 0; i < n; ++i) {
        r.px[i] = srcs[i].pos.x;
        r.py[i] = srcs[i].pos.y;
        r.pz[i] = srcs[i].pos.z;
        r.prio[i] = srcs[i].priority;
        r.phases[i] = srcs[i].phase;
    }
    return r;
}

// ============================================================
// Math helpers
// ============================================================
static float dist2(Vec3 a, Vec3 b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx*dx + dy*dy + dz*dz;
}

static float dist(Vec3 a, Vec3 b) { return std::sqrt(dist2(a, b)); }

static float atten(float d) {
    float s = 20.0f;
    return 1.0f / (1.0f + d*d / (s*s));
}

static void pan(float dx, float dy, float& gL, float& gR) {
    float az = std::atan2(dy, dx);
    gL = std::cos(az * 0.5f + 0.78539816339f);
    gR = std::sin(az * 0.5f + 0.78539816339f);
    float n = std::sqrt(gL*gL + gR*gR);
    if (n > 1e-10f) { gL /= n; gR /= n; }
}

// ============================================================
// Scene generation
// ============================================================
static std::vector<Source> gen_scene(int count, uint64_t seed) {
    std::mt19937_64 rng{seed};
    std::vector<Source> s(count);
    for (auto& src : s) {
        float r = std::min(
            std::exponential_distribution<float>(1.0f / 80.0f)(rng), 500.0f);
        float ah = std::uniform_real_distribution<float>(-1.4f, 1.4f)(rng);
        float av = std::uniform_real_distribution<float>(-0.1f, 0.2f)(rng);
        src.pos = {r * std::cos(ah) * std::cos(av),
                   r * std::sin(ah),
                   r * std::sin(av) + 1.5f};
        float dp = 1.0f / (1.0f + r * r / 2500.0f);
        src.priority = std::clamp(
            dp * std::uniform_real_distribution<float>(0.5f, 1.2f)(rng),
            0.0f, 1.0f);
        src.type = (uint8_t)std::uniform_int_distribution<int>(0, 4)(rng);
        src.phase = std::uniform_real_distribution<float>(0.0f, 6.2831853f)(rng);
    }
    return s;
}

// ============================================================
// Strategy implementations
// ============================================================

using StrategyFn = int (*)(const std::vector<Source>&, const Listener&,
                           float*, float*, int n, int frame);

// --- A: nearest 32 only (no distant ambient) ---
static int strat_A(const std::vector<Source>& srcs, const Listener& lis,
                   float* bufL, float* bufR, int n, int frame) {
    int N = (int)srcs.size();
    std::vector<float> d2(N);
    std::vector<int> idx(N);
    for (int i = 0; i < N; ++i) {
        d2[i] = dist2(srcs[i].pos, lis.pos);
        idx[i] = i;
    }
    int take = std::min(N, kNoAmbientCap);
    std::ranges::partial_sort(idx.begin(), idx.begin() + take, idx.end(),
        [&](int a, int b) { return d2[a] < d2[b]; });

    float inc = 6.2831853f * 200.0f / 48000.0f;
    for (int i = 0; i < take; ++i) {
        int si = idx[i];
        float d = std::sqrt(d2[si]);
        float a = atten(d);
        float dx = srcs[si].pos.x - lis.pos.x;
        float dy = srcs[si].pos.y - lis.pos.y;
        float gL, gR;
        pan(dx, dy, gL, gR);
        float ph = srcs[si].phase + (float)frame;
        for (int j = 0; j < n; ++j) {
            float samp = std::sin(ph) * a;
            bufL[j] += samp * gL;
            bufR[j] += samp * gR;
            ph += inc;
        }
    }
    return take;
}

// --- B: full 3D for ALL sources (reference) ---
static int strat_B(const std::vector<Source>& srcs, const Listener& lis,
                   float* bufL, float* bufR, int n, int frame) {
    float inc = 6.2831853f * 200.0f / 48000.0f;
    for (auto& s : srcs) {
        float d = dist(s.pos, lis.pos);
        float a = atten(d);
        float dx = s.pos.x - lis.pos.x;
        float dy = s.pos.y - lis.pos.y;
        float gL, gR;
        pan(dx, dy, gL, gR);
        float ph = s.phase + (float)frame;
        for (int j = 0; j < n; ++j) {
            float samp = std::sin(ph) * a;
            bufL[j] += samp * gL;
            bufR[j] += samp * gR;
            ph += inc;
        }
    }
    return (int)srcs.size();
}

// --- C: hybrid LOD (near=3D, mid=ambient, far=mono) ---
static int strat_C(const std::vector<Source>& srcs, const Listener& lis,
                   float* bufL, float* bufR, int n, int frame) {
    float inc = 6.2831853f * 200.0f / 48000.0f;
    float mono_acc = 0.0f;
    for (auto& s : srcs) {
        float d = dist(s.pos, lis.pos);
        if (d < kNearDist) {
            float a = atten(d);
            float dx = s.pos.x - lis.pos.x;
            float dy = s.pos.y - lis.pos.y;
            float gL, gR;
            pan(dx, dy, gL, gR);
            float ph = s.phase + (float)frame;
            for (int j = 0; j < n; ++j) {
                float samp = std::sin(ph) * a;
                bufL[j] += samp * gL;
                bufR[j] += samp * gR;
                ph += inc;
            }
        } else if (d < kMidDist) {
            float a = atten(d) * 0.5f;
            float dx = s.pos.x - lis.pos.x;
            float dy = s.pos.y - lis.pos.y;
            float az = std::atan2(dy, dx);
            float gL = std::cos(az * 0.5f + 0.78539816339f) * 0.3f;
            float gR = std::sin(az * 0.5f + 0.78539816339f) * 0.3f;
            float ph = s.phase + (float)frame;
            for (int j = 0; j < n; ++j) {
                float samp = std::sin(ph) * a;
                bufL[j] += samp * gL;
                bufR[j] += samp * gR;
                ph += inc;
            }
        } else {
            mono_acc += atten(d) * 0.1f * std::sin(s.phase + (float)frame);
        }
    }
    if (mono_acc != 0.0f) {
        float val = mono_acc / n;
        for (int j = 0; j < n; ++j) { bufL[j] += val; bufR[j] += val; }
    }
    return (int)srcs.size();
}

// --- D: priority cap 64 + LOD ---
static int strat_D(const std::vector<Source>& srcs, const Listener& lis,
                   float* bufL, float* bufR, int n, int frame) {
    int N = (int)srcs.size();
    int take = std::min(N, kPriorityCap);
    std::vector<float> prio(N);
    std::vector<int> idx(N);
    for (int i = 0; i < N; ++i) {
        float d2v = dist2(srcs[i].pos, lis.pos);
        float dp = 1.0f / (1.0f + d2v / (50.0f * 50.0f));
        prio[i] = srcs[i].priority * dp;
        idx[i] = i;
    }
    std::ranges::partial_sort(idx.begin(), idx.begin() + take, idx.end(),
        [&](int a, int b) { return prio[a] > prio[b]; });

    float inc = 6.2831853f * 200.0f / 48000.0f;
    float mono_acc = 0.0f;
    for (int i = 0; i < take; ++i) {
        int si = idx[i];
        float d = std::sqrt(dist2(srcs[si].pos, lis.pos));
        if (d < kNearDist) {
            float a = atten(d);
            float dx = srcs[si].pos.x - lis.pos.x;
            float dy = srcs[si].pos.y - lis.pos.y;
            float gL, gR;
            pan(dx, dy, gL, gR);
            float ph = srcs[si].phase + (float)frame;
            for (int j = 0; j < n; ++j) {
                float samp = std::sin(ph) * a;
                bufL[j] += samp * gL;
                bufR[j] += samp * gR;
                ph += inc;
            }
        } else if (d < kMidDist) {
            float a = atten(d) * 0.5f;
            float dx = srcs[si].pos.x - lis.pos.x;
            float dy = srcs[si].pos.y - lis.pos.y;
            float az = std::atan2(dy, dx);
            float gL = std::cos(az * 0.5f + 0.78539816339f) * 0.3f;
            float gR = std::sin(az * 0.5f + 0.78539816339f) * 0.3f;
            float ph = srcs[si].phase + (float)frame;
            for (int j = 0; j < n; ++j) {
                float samp = std::sin(ph) * a;
                bufL[j] += samp * gL;
                bufR[j] += samp * gR;
                ph += inc;
            }
        } else {
            mono_acc += atten(d) * 0.1f * std::sin(srcs[si].phase + (float)frame);
        }
    }
    if (mono_acc != 0.0f) {
        float val = mono_acc / n;
        for (int j = 0; j < n; ++j) { bufL[j] += val; bufR[j] += val; }
    }
    return take;
}

// --- E: SoA + prefetch (max single-thread throughput) ---
static int strat_E(const SoA& soa, const Listener& lis,
                   float* bufL, float* bufR, int n, int frame) {
    int N = soa.size();
    float inc = 6.2831853f * 200.0f / 48000.0f;
    for (int i = 0; i < N; ++i) {
        if ((i & 7) == 0 && i + 8 < N) {
            __builtin_prefetch(&soa.px[i + 8], 0, 3);
            __builtin_prefetch(&soa.py[i + 8], 0, 3);
            __builtin_prefetch(&soa.pz[i + 8], 0, 3);
            __builtin_prefetch(&soa.phases[i + 8], 0, 3);
        }
        float d = std::sqrt(dist2(
            {soa.px[i], soa.py[i], soa.pz[i]}, lis.pos));
        float a = atten(d);
        float dx = soa.px[i] - lis.pos.x;
        float dy = soa.py[i] - lis.pos.y;
        float gL, gR;
        pan(dx, dy, gL, gR);
        float ph = soa.phases[i] + (float)frame;
        for (int j = 0; j < n; ++j) {
            float samp = std::sin(ph) * a;
            bufL[j] += samp * gL;
            bufR[j] += samp * gR;
            ph += inc;
        }
    }
    return N;
}

// ============================================================
// PSNR
// ============================================================
static double compute_psnr(const float* ref, const float* test,
                           int n, float peak) {
    double mse = 0.0;
    for (int i = 0; i < n; ++i) {
        double d = (double)ref[i] - (double)test[i];
        mse += d * d;
    }
    mse /= n;
    if (mse < 1e-20) return 200.0;
    return 10.0 * std::log10((double)peak * (double)peak / mse);
}

// ============================================================
// Main
// ============================================================
int main() {
    std::printf("strategy,scene,sources,seed,mean_time_us,mean_active,psnr_db\n");

    for (int seed = 0; seed < kSeeds; ++seed) {
        for (int si = 0; si < (int)kSceneSizes.size(); ++si) {
            int N = kSceneSizes[si];
            auto srcs = gen_scene(N, 12345 + seed * 31337);
            auto soa = to_soa(srcs);
            Listener lis{{0.0f, 1.7f, 0.0f}};

            std::vector<float> bufL(kFrameSamples);
            std::vector<float> bufR(kFrameSamples);
            std::vector<float> refL(kFrameSamples);
            std::vector<float> refR(kFrameSamples);

            // Capture reference (strategy B, same frame as test capture)
            int ref_frame = kMeasFrames / 2;
            std::memset(refL.data(), 0, kFrameSamples * sizeof(float));
            std::memset(refR.data(), 0, kFrameSamples * sizeof(float));
            strat_B(srcs, lis, refL.data(), refR.data(), kFrameSamples, ref_frame);

            float peak = 0.0f;
            for (int i = 0; i < kFrameSamples; ++i) {
                peak = std::max(peak, std::abs(refL[i]));
                peak = std::max(peak, std::abs(refR[i]));
            }
            if (peak < 1e-10f) peak = 1.0f;

            // Each strategy
            StrategyFn strats[] = {strat_A, strat_B, strat_C, strat_D};

            for (int strat = 0; strat < 5; ++strat) {
                using clock = std::chrono::steady_clock;
                double total_us = 0.0;
                int active_sum = 0;

                std::vector<float> testL(kFrameSamples);
                std::vector<float> testR(kFrameSamples);

                for (int frame = -kWarmupFrames; frame < kMeasFrames; ++frame) {
                    bool record = (frame >= 0);
                    std::memset(bufL.data(), 0, kFrameSamples * sizeof(float));
                    std::memset(bufR.data(), 0, kFrameSamples * sizeof(float));

                    auto t0 = clock::now();

                    int active = 0;
                    if (strat < 4) {
                        active = strats[strat](srcs, lis, bufL.data(),
                                                bufR.data(), kFrameSamples, frame);
                    } else {
                        active = strat_E(soa, lis, bufL.data(),
                                         bufR.data(), kFrameSamples, frame);
                    }

                    auto t1 = clock::now();
                    if (record) {
                        total_us += std::chrono::duration<double,
                            std::micro>(t1 - t0).count();
                        active_sum += active;
                    }
                    if (record && frame == kMeasFrames / 2) {
                        std::memcpy(testL.data(), bufL.data(),
                                    kFrameSamples * sizeof(float));
                        std::memcpy(testR.data(), bufR.data(),
                                    kFrameSamples * sizeof(float));
                    }
                }

                double mean_us = total_us / kMeasFrames;
                double mean_active = (double)active_sum / kMeasFrames;

                // PSNR against reference
                std::vector<float> both_ref(kFrameSamples * 2);
                std::vector<float> both_test(kFrameSamples * 2);
                std::memcpy(both_ref.data(), refL.data(),
                            kFrameSamples * sizeof(float));
                std::memcpy(both_ref.data() + kFrameSamples, refR.data(),
                            kFrameSamples * sizeof(float));
                std::memcpy(both_test.data(), testL.data(),
                            kFrameSamples * sizeof(float));
                std::memcpy(both_test.data() + kFrameSamples, testR.data(),
                            kFrameSamples * sizeof(float));
                double psnr = compute_psnr(both_ref.data(), both_test.data(),
                                            kFrameSamples * 2, peak);

                std::printf("%s,%s,%d,%d,%.3f,%.1f,%.1f\n",
                    kStrategyNames[strat], kSceneNames[si],
                    N, seed, mean_us, mean_active, psnr);
            }
        }
    }

    return 0;
}
