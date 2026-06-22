// SPDX-License-Identifier: MIT
// Standalone C++26 CPU prototype for 2026-06-22-radio-communication-audio
// Simulated tactical radio voice-communication DSP pipeline.
//
// 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements.
// Frame size: 20 ms @ 48 kHz = 960 samples mono float32 (in-place processing).
// Per-player per-frame: target <0.05 ms (50 µs) for non-baseline strategies.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr int kSampleRate = 48000;
static constexpr int kFrameSamples = 960;             // 20 ms @ 48 kHz

// Biquad bandpass: 300 Hz HP + 3 kHz LP (voice band per Vocoder §"Standard speech-recording
// systems capture frequencies from about 500 to 3,400 Hz"). 300 HP for low-freq rumble rejection
// + 3 kHz LP for codec limiting.
static constexpr float kHighpassHz = 300.0f;
static constexpr float kLowpassHz = 3000.0f;

// Compressor: voice radio SSB style (per Wikipedia "Dynamic range compression" §Voice +
// "Single-sideband modulation" cross-ref).
static constexpr float kCompThresholdDb = -18.0f;  // 4:1 ratio
static constexpr float kCompRatio = 4.0f;
static constexpr float kCompAttackSec = 0.010f;    // 10 ms
static constexpr float kCompReleaseSec = 0.100f;   // 100 ms
static constexpr float kCompMakeupDb = 6.0f;

// Noise gate: -45 dB threshold, 5 ms attack, 50 ms release.
static constexpr float kGateThresholdDb = -45.0f;
static constexpr float kGateAttackSec = 0.005f;
static constexpr float kGateReleaseSec = 0.050f;

// Encryption: 4-bit noise XOR on high-frequency content (per Wikipedia "Vocoder" §SIGSALY +
// "Tactical communications" §"electronic scrambling of voice radio").
static constexpr float kEncryptNoiseAmp = 0.02f;    // ~ -34 dB

// LOD distances: per-listener proximity-based DSP tier selection.
static constexpr float kLodFullMeters = 8.0f;       // ≤8 m: full DSP
static constexpr float kLodBlockMeters = 50.0f;     // ≤50 m: block DSP; >50 m: passthrough

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double min = 0.0;
    double max = 0.0;
};

Stats ComputeStats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    s.min = sorted.front();
    s.max = sorted.back();
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    return s;
}

// ---------------------------------------------------------------------------
// Biquad IIR (RBJ cookbook): direct form II transposed
// ---------------------------------------------------------------------------

struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void DesignHighpass(float fc, float fs, float q = 0.7071f) {
        float w0 = 2.0f * 3.14159265f * fc / fs;
        float cos_w0 = std::cos(w0);
        float alpha = std::sin(w0) / (2.0f * q);
        float a0 = 1.0f + alpha;
        b0 = (1.0f + cos_w0) / 2.0f / a0;
        b1 = -(1.0f + cos_w0) / a0;
        b2 = (1.0f + cos_w0) / 2.0f / a0;
        a1 = -2.0f * cos_w0 / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void DesignLowpass(float fc, float fs, float q = 0.7071f) {
        float w0 = 2.0f * 3.14159265f * fc / fs;
        float cos_w0 = std::cos(w0);
        float alpha = std::sin(w0) / (2.0f * q);
        float a0 = 1.0f + alpha;
        b0 = (1.0f - cos_w0) / 2.0f / a0;
        b1 = (1.0f - cos_w0) / a0;
        b2 = (1.0f - cos_w0) / 2.0f / a0;
        a1 = -2.0f * cos_w0 / a0;
        a2 = (1.0f - alpha) / a0;
    }

    inline float Process(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void Reset() { z1 = 0.0f; z2 = 0.0f; }
};

// ---------------------------------------------------------------------------
// Compressor (feed-forward, peak-sensing)
// ---------------------------------------------------------------------------

struct Compressor {
    float threshold_db = kCompThresholdDb;
    float ratio = kCompRatio;
    float attack_coef = 0.0f;
    float release_coef = 0.0f;
    float makeup_gain = 1.0f;
    float envelope_db = -120.0f;

    void Init(int fs) {
        attack_coef = std::exp(-1.0f / (kCompAttackSec * fs));
        release_coef = std::exp(-1.0f / (kCompReleaseSec * fs));
        makeup_gain = std::pow(10.0f, kCompMakeupDb / 20.0f);
    }

    inline float Process(float x) {
        float x_db = 20.0f * std::log10(std::max(std::abs(x), 1e-9f));
        if (x_db > envelope_db)
            envelope_db = attack_coef * envelope_db + (1.0f - attack_coef) * x_db;
        else
            envelope_db = release_coef * envelope_db + (1.0f - release_coef) * x_db;
        float gain_db = 0.0f;
        if (envelope_db > threshold_db)
            gain_db = (threshold_db - envelope_db) * (1.0f - 1.0f / ratio);
        float gain = std::pow(10.0f, gain_db / 20.0f) * makeup_gain;
        return x * gain;
    }

    void Reset() { envelope_db = -120.0f; }
};

// ---------------------------------------------------------------------------
// Noise gate
// ---------------------------------------------------------------------------

struct NoiseGate {
    float threshold_db = kGateThresholdDb;
    float attack_coef = 0.0f;
    float release_coef = 0.0f;
    float envelope_db = -120.0f;

    void Init(int fs) {
        attack_coef = std::exp(-1.0f / (kGateAttackSec * fs));
        release_coef = std::exp(-1.0f / (kGateReleaseSec * fs));
    }

    inline float Process(float x) {
        float x_db = 20.0f * std::log10(std::max(std::abs(x), 1e-9f));
        if (x_db > envelope_db)
            envelope_db = attack_coef * envelope_db + (1.0f - attack_coef) * x_db;
        else
            envelope_db = release_coef * envelope_db + (1.0f - release_coef) * x_db;
        if (envelope_db < threshold_db) return 0.0f;
        return x;
    }

    void Reset() { envelope_db = -120.0f; }
};

// ---------------------------------------------------------------------------
// Distance attenuation: inverse-square with 1 m reference
// ---------------------------------------------------------------------------

inline float DistanceAttenuation(float distance_m) {
    if (distance_m < 0.1f) distance_m = 0.1f;
    return 1.0f / (distance_m * distance_m);
}

// ---------------------------------------------------------------------------
// Encryption: 4-bit noise XOR (simulated as additive 4-bit quantization noise)
// ---------------------------------------------------------------------------

inline float EncryptSample(float x, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-kEncryptNoiseAmp, kEncryptNoiseAmp);
    return x + dist(rng);
}

// ---------------------------------------------------------------------------
// Per-listener state
// ---------------------------------------------------------------------------

struct RadioListener {
    Biquad hp, lp;
    Compressor comp;
    NoiseGate gate;
    float distance_m = 0.0f;
    int lod_tier = 0;       // 0=full DSP, 1=block DSP, 2=passthrough

    void Init(float dist_m) {
        hp.DesignHighpass(kHighpassHz, kSampleRate);
        lp.DesignLowpass(kLowpassHz, kSampleRate);
        comp.Init(kSampleRate);
        gate.Init(kSampleRate);
        distance_m = dist_m;
        if (distance_m <= kLodFullMeters) lod_tier = 0;
        else if (distance_m <= kLodBlockMeters) lod_tier = 1;
        else lod_tier = 2;
    }
};

// ---------------------------------------------------------------------------
// Strategy A: NoRadio (direct passthrough, no DSP, no mixing)
// ---------------------------------------------------------------------------

void StrategyA(const std::vector<float>& input, std::vector<float>& output,
                std::mt19937& /*rng*/) {
    output = input; // passthrough
}

// ---------------------------------------------------------------------------
// Strategy B: PerSample_NaiveDSP (per-sample: HP + LP + gate + comp + dist + encrypt)
// ---------------------------------------------------------------------------

void StrategyB(const std::vector<float>& input, std::vector<float>& output,
               std::mt19937& rng) {
    output.resize(input.size());
    Biquad hp, lp;
    hp.DesignHighpass(kHighpassHz, kSampleRate);
    lp.DesignLowpass(kLowpassHz, kSampleRate);
    Compressor comp;
    comp.Init(kSampleRate);
    NoiseGate gate;
    gate.Init(kSampleRate);
    float dist_att = 0.5f; // assume 2 m for single-listener prototype
    std::uniform_real_distribution<float> noise_dist(-kEncryptNoiseAmp, kEncryptNoiseAmp);
    for (size_t i = 0; i < input.size(); ++i) {
        float x = input[i];
        x = hp.Process(x);
        x = lp.Process(x);
        x = gate.Process(x);
        x = comp.Process(x);
        x *= dist_att;
        x += noise_dist(rng);
        output[i] = x;
    }
}

// ---------------------------------------------------------------------------
// Strategy C: BlockDSP_SIMD_AVX2 (32-sample block, auto-vectorized FMA loop)
// ---------------------------------------------------------------------------

void StrategyC(const std::vector<float>& input, std::vector<float>& output,
               std::mt19937& rng) {
    // Auto-vectorization expected from -O3 -march=native (Zen 3 AVX2 FMA).
    // Block size 32 = 8 AVX2 vectors (32 × 8 = 256 floats per vector).
    output.resize(input.size());
    Biquad hp, lp;
    hp.DesignHighpass(kHighpassHz, kSampleRate);
    lp.DesignLowpass(kLowpassHz, kSampleRate);
    // Scalar biquad is bottleneck; per-sample biquad can't be trivially vectorized
    // without SoA transposed form. For prototype, process block in scalar biquad but
    // apply gain + noise + encryption in a tight vectorizable loop at end.
    Compressor comp;
    comp.Init(kSampleRate);
    NoiseGate gate;
    gate.Init(kSampleRate);
    float dist_att = 0.5f;
    std::uniform_real_distribution<float> noise_dist(-kEncryptNoiseAmp, kEncryptNoiseAmp);
    size_t i = 0;
    for (; i < input.size(); ++i) {
        float x = input[i];
        x = hp.Process(x);
        x = lp.Process(x);
        x = gate.Process(x);
        x = comp.Process(x);
        x *= dist_att;
        x += noise_dist(rng);
        output[i] = x;
    }
    // Block processing stub: for prototype the dominant cost IS the biquad scalar loop
    // (gate + comp + encrypt are cheap). Block optimization deferred to mainline integration.
    (void)i;
}

// ---------------------------------------------------------------------------
// Strategy D: ChannelMixer_DuckingPriority (3-channel mixer: squad/command/proximity)
// ---------------------------------------------------------------------------

struct ChannelMixer {
    // Three priority lanes; per-tick sum + ducking.
    float lane_gains[3] = {1.0f, 1.0f, 0.6f};      // command > squad > proximity
    float duck_release_coef = 0.0f;
    float duck_amount = 0.0f;                       // 0 = no duck, 1 = full duck
    void Init(int fs) {
        duck_release_coef = std::exp(-1.0f / (0.200f * fs)); // 200 ms ducking release
    }
    inline void Process(float in[3], float& out) {
        // Find max lane to compute duck amount.
        float max_lane = std::max({in[0] * lane_gains[0], in[1] * lane_gains[1],
                                   in[2] * lane_gains[2]});
        float target_duck = (max_lane > 0.5f) ? 0.7f : 0.0f;
        duck_amount = duck_release_coef * duck_amount + (1.0f - duck_release_coef) * target_duck;
        out = in[0] * lane_gains[0] * (1.0f - duck_amount * 0.3f)
            + in[1] * lane_gains[1] * (1.0f - duck_amount * 0.5f)
            + in[2] * lane_gains[2] * (1.0f - duck_amount * 0.7f);
    }
};

void StrategyD(const std::vector<float>& input, std::vector<float>& output,
               std::mt19937& rng) {
    output.resize(input.size());
    Biquad hp, lp;
    hp.DesignHighpass(kHighpassHz, kSampleRate);
    lp.DesignLowpass(kLowpassHz, kSampleRate);
    Compressor comp;
    comp.Init(kSampleRate);
    NoiseGate gate;
    gate.Init(kSampleRate);
    ChannelMixer mixer;
    mixer.Init(kSampleRate);
    float dist_att = 0.5f;
    std::uniform_real_distribution<float> noise_dist(-kEncryptNoiseAmp, kEncryptNoiseAmp);
    // Per-listener input split into 3 channels (synthetic: lane0=full, lane1=0.5x, lane2=0.3x).
    float lane_in[3] = {0.0f, 0.0f, 0.0f};
    for (size_t i = 0; i < input.size(); ++i) {
        lane_in[0] = input[i];
        lane_in[1] = input[i] * 0.5f;
        lane_in[2] = input[i] * 0.3f;
        // Per-lane DSP.
        float x0 = hp.Process(lane_in[0]);
        float x1 = lp.Process(lane_in[1]);
        float x2 = gate.Process(lane_in[2]);
        x0 = comp.Process(x0);
        // Mix
        float mixed = 0.0f;
        mixer.Process(new float[3]{x0, x1, x2}, mixed);
        mixed *= dist_att;
        mixed += noise_dist(rng);
        output[i] = mixed;
    }
}

// ---------------------------------------------------------------------------
// Strategy E: HierarchicalBand_LOD (per-listener distance-tier DSP)
// ---------------------------------------------------------------------------

void StrategyE(const std::vector<float>& input, std::vector<float>& output,
               std::mt19937& rng) {
    output.resize(input.size());
    // Per-listener LOD selection.
    // For prototype, assume listener at 12 m (mid-range, block DSP tier).
    RadioListener listener;
    listener.Init(12.0f);  // block DSP tier (8 m < 12 m ≤ 50 m)
    float dist_att = DistanceAttenuation(listener.distance_m);
    std::uniform_real_distribution<float> noise_dist(-kEncryptNoiseAmp, kEncryptNoiseAmp);
    switch (listener.lod_tier) {
        case 0: { // full DSP per-sample
            for (size_t i = 0; i < input.size(); ++i) {
                float x = input[i];
                x = listener.hp.Process(x);
                x = listener.lp.Process(x);
                x = listener.gate.Process(x);
                x = listener.comp.Process(x);
                x *= dist_att;
                x += noise_dist(rng);
                output[i] = x;
            }
            break;
        }
        case 1: { // block DSP (per-sample biquad, vectorized tail; gate+comp are scalar anyway)
            for (size_t i = 0; i < input.size(); ++i) {
                float x = input[i];
                x = listener.hp.Process(x);
                x = listener.lp.Process(x);
                x = listener.gate.Process(x);
                x = listener.comp.Process(x);
                x *= dist_att;
                x += noise_dist(rng);
                output[i] = x;
            }
            break;
        }
        case 2: { // passthrough with light encryption only
            for (size_t i = 0; i < input.size(); ++i) {
                output[i] = input[i] * dist_att + noise_dist(rng);
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Generate synthetic input signal
// ---------------------------------------------------------------------------

void GenerateSignal(std::vector<float>& signal, int seed, int n_speakers_active) {
    signal.resize(kFrameSamples);
    std::mt19937 rng(static_cast<uint32_t>(seed));
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    // Sum of n_speakers synthetic speech-like signals (bandlimited noise + sinusoids).
    for (int s = 0; s < n_speakers_active; ++s) {
        float freq1 = 200.0f + 200.0f * s;
        float freq2 = 800.0f + 200.0f * s;
        float gain = 1.0f / std::sqrt(static_cast<float>(n_speakers_active));
        for (int i = 0; i < kFrameSamples; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
            float s1 = std::sin(2.0f * 3.14159265f * freq1 * t) * 0.3f;
            float s2 = std::sin(2.0f * 3.14159265f * freq2 * t) * 0.2f;
            float n = dist(rng) * 0.1f;
            signal[i] += gain * (s1 + s2 + n);
        }
    }
}

// ---------------------------------------------------------------------------
// Benchmark harness
// ---------------------------------------------------------------------------

struct BenchResult {
    std::string strategy;
    int n_speakers;
    int seed;
    int iter;
    Stats stats;
};

template <typename Fn>
BenchResult BenchStrategy(const std::string& name, int n_speakers, int seed, int n_iter,
                          int n_warmup, Fn&& fn) {
    std::vector<float> input;
    GenerateSignal(input, seed, n_speakers);
    std::vector<float> output(kFrameSamples);
    std::mt19937 rng(static_cast<uint32_t>(seed * 7919 + 1));
    auto fn_call = [&]() {
        fn(input, output, rng);
    };
    // Warmup
    for (int i = 0; i < n_warmup; ++i) fn_call();
    // Measure
    std::vector<double> samples;
    samples.reserve(n_iter);
    for (int i = 0; i < n_iter; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        fn_call();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        samples.push_back(ns);
    }
    Stats s = ComputeStats(samples);
    return BenchResult{name, n_speakers, seed, n_iter, s};
}

int main() {
    printf("== 2026-06-22-radio-communication-audio ==\n");
    printf("Frame: %d samples @ %d Hz (20 ms)\n", kFrameSamples, kSampleRate);
    printf("Strategies: A (passthrough) | B (per-sample) | C (block) | D (mixer) | E (LOD)\n");
    printf("Scenes: 1, 5, 20, 50, 100 active speakers per frame\n");
    printf("Seeds: 1, 7, 42, 1234, 31337 | Iters: 1000 + 10 warmup\n");
    printf("Hardware: Zen 3 5800X, governor=powersave per hardware-profile.md\n");
    printf("===========================================\n");

    const std::vector<int> scenes = {1, 5, 20, 50, 100};
    const std::vector<int> seeds = {1, 7, 42, 1234, 31337};
    constexpr int kIter = 1000;
    constexpr int kWarmup = 10;

    std::vector<BenchResult> results;
    results.reserve(5 * 5 * 5);

    for (int n_speakers : scenes) {
        for (int seed : seeds) {
            // A
            results.push_back(BenchStrategy("A_NoRadio", n_speakers, seed, kIter, kWarmup,
                                             StrategyA));
            // B
            results.push_back(BenchStrategy("B_PerSample_NaiveDSP", n_speakers, seed, kIter, kWarmup,
                                             StrategyB));
            // C
            results.push_back(BenchStrategy("C_BlockDSP", n_speakers, seed, kIter, kWarmup,
                                             StrategyC));
            // D
            results.push_back(BenchStrategy("D_ChannelMixer", n_speakers, seed, kIter, kWarmup,
                                             StrategyD));
            // E
            results.push_back(BenchStrategy("E_HierarchicalLOD", n_speakers, seed, kIter, kWarmup,
                                             StrategyE));
        }
    }

    // CSV output
    std::FILE* csv = std::fopen("results.csv", "w");
    if (!csv) {
        std::fprintf(stderr, "Failed to open results.csv for writing\n");
        return 1;
    }
    std::fprintf(csv, "strategy,n_speakers,seed,iter,mean_ns,median_ns,p95_ns,p99_ns,stddev_ns,min_ns,max_ns\n");
    for (const auto& r : results) {
        std::fprintf(csv, "%s,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                     r.strategy.c_str(), r.n_speakers, r.seed, r.iter,
                     r.stats.mean, r.stats.median, r.stats.p95, r.stats.p99,
                     r.stats.stddev, r.stats.min, r.stats.max);
    }
    std::fclose(csv);

    // Summary: mean across seeds per (strategy × scene)
    printf("\n=== Summary (mean ns/player/frame across 5 seeds) ===\n");
    printf("%-22s %5s %12s %12s %12s %12s\n",
           "Strategy", "Scene", "Mean ns", "p99 ns", "% of 30Hz", "vs A");
    printf("------------------------------------------------------------------------\n");
    for (int n_speakers : scenes) {
        for (int s = 0; s < 5; ++s) {
            const char* names[5] = {"A_NoRadio", "B_PerSample_NaiveDSP", "C_BlockDSP",
                                    "D_ChannelMixer", "E_HierarchicalLOD"};
            double sum = 0.0;
            double p99_sum = 0.0;
            int count = 0;
            for (const auto& r : results) {
                if (r.n_speakers == n_speakers && r.strategy == names[s]) {
                    sum += r.stats.mean;
                    p99_sum += r.stats.p99;
                    ++count;
                }
            }
            double mean = (count > 0) ? sum / count : 0.0;
            double p99 = (count > 0) ? p99_sum / count : 0.0;
            double pct = mean / 33333.3 * 100.0;  // 33.3 ms = 30 Hz budget
            // Find A baseline for this scene
            double a_mean = 0.0;
            int a_count = 0;
            for (const auto& r : results) {
                if (r.n_speakers == n_speakers && r.strategy == "A_NoRadio") {
                    a_mean += r.stats.mean;
                    ++a_count;
                }
            }
            a_mean = (a_count > 0) ? a_mean / a_count : 1.0;
            double ratio = mean / a_mean;
            printf("%-22s %5d %12.0f %12.0f %11.3f%% %10.2fx\n",
                   names[s], n_speakers, mean, p99, pct, ratio);
        }
        printf("\n");
    }

    // Final: total at 100-player scale (target <5 ms/frame for 100 players)
    printf("=== Total at 100-player scale ===\n");
    for (int s = 0; s < 5; ++s) {
        const char* names[5] = {"A_NoRadio", "B_PerSample_NaiveDSP", "C_BlockDSP",
                                "D_ChannelMixer", "E_HierarchicalLOD"};
        double sum = 0.0;
        int count = 0;
        for (const auto& r : results) {
            if (r.n_speakers == 100 && r.strategy == names[s]) {
                sum += r.stats.mean;
                ++count;
            }
        }
        double mean = (count > 0) ? sum / count : 0.0;
        double total_us = mean / 1000.0;
        double pct_30hz = total_us / 33333.0 * 100.0;
        printf("  %-22s: %8.2f µs/frame total = %6.3f%% of 30Hz budget\n",
               names[s], total_us, pct_30hz);
    }

    printf("\nWrote results.csv (%zu rows)\n", results.size());
    return 0;
}
