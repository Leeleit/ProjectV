// SPDX-License-Identifier: MIT
// Standalone C++26 CPU prototype for 2026-06-22-procedural-engine-sound
// Procedural real-time engine sound synthesis for Stage 6+ military sandbox.
//
// 6 strategies x 5 vehicle profiles x 5 RPM profiles x 1000 iter + 10 warmup = 150,000 main measurements.
// Audio buffer: 1024 samples @ 44.1 kHz = 23.22 ms (typical miniaudio render block).
// Per-vehicle per-tick parameter update target: <0.01 ms.
// Per-buffer fill target: <0.05 ms @ 1024 samples.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <random>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr int kSampleRate = 44100;
static constexpr int kBufferSamples = 1024;
static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr int kMaxHarmonics = 8;

// ---------------------------------------------------------------------------
// Vehicle profile data (5 profiles per Wikipedia ICE classification)
// ---------------------------------------------------------------------------

enum class VehicleClass : int {
    kFourCylTractor = 0,   // 4-cyl, low RPM, no turbo (e.g., vintage tractor / jeep)
    kSixCylDieselTruck,    // 6-cyl diesel, mid RPM, turbo (e.g., Ural-4320, M35)
    kV8GasolineSport,      // V8 gasoline, high RPM, no turbo (e.g., M4 Sherman, GAZ-13)
    kV12Exotic,            // V12 exotic, very high RPM, no turbo (e.g., supercar)
    kWankelRotary,         // Wankel 2-rotor, high RPM, no turbo (e.g., Mazda RX-7)
    kCount
};

static constexpr const char* kVehicleNames[static_cast<int>(VehicleClass::kCount)] = {
    "4cyl_tractor",        // [800, 2400] RPM
    "6cyl_diesel_truck",   // [800, 3500] RPM, turbo
    "v8_gasoline_sport",   // [900, 7500] RPM
    "v12_exotic",          // [900, 9000] RPM
    "wankel_rotary"        // [2000, 9000] RPM
};

static constexpr int kCylinders[static_cast<int>(VehicleClass::kCount)] = {
    4, 6, 8, 12, 2  // Wankel 2-rotor = 2 effective working chambers
};

static constexpr int kIdleRpm[static_cast<int>(VehicleClass::kCount)] = {
    800, 800, 900, 900, 2000
};

static constexpr int kRedlineRpm[static_cast<int>(VehicleClass::kCount)] = {
    2400, 3500, 7500, 9000, 9000
};

static constexpr bool kTurboFlag[static_cast<int>(VehicleClass::kCount)] = {
    false, true, false, false, false
};

// Per-cylinder harmonic weights (sum-of-sines amplitudes for harmonics 1..8)
// Empirically derived: 4-cyl = strong fundamental + low harmonics (smooth);
// V8 = full spectrum up to 8th (rich rumble); V12 = extended to 12 (complex);
// Wankel = continuous spectrum with high content above 6th harmonic (wail).
static constexpr float kHarmonicWeights[static_cast<int>(VehicleClass::kCount)][kMaxHarmonics] = {
    // 4cyl tractor: fundamental dominant, weak 2nd/4th (smooth)
    {1.00f, 0.45f, 0.20f, 0.10f, 0.05f, 0.02f, 0.01f, 0.005f},
    // 6cyl diesel: staccato, mid-range harmonics emphasized
    {1.00f, 0.60f, 0.40f, 0.30f, 0.22f, 0.15f, 0.10f, 0.06f},
    // V8 gasoline: full spectrum (the classic muscle rumble)
    {1.00f, 0.75f, 0.55f, 0.42f, 0.32f, 0.24f, 0.18f, 0.12f},
    // V12 exotic: extended harmonics up to 12 (supercharger whine)
    {1.00f, 0.80f, 0.65f, 0.55f, 0.45f, 0.38f, 0.30f, 0.25f},
    // Wankel rotary: high-frequency emphasis (the wail)
    {1.00f, 0.65f, 0.50f, 0.45f, 0.40f, 0.35f, 0.30f, 0.25f}
};

// RPM profiles (5 deterministic states) — engine RPM trajectory during typical drive
// Format: idle 1/3 -> mid 1/3 -> high 1/3 -> high -> back to mid
static constexpr float kRpmProfile[5] = {
    0.0f,   // idle (just started)
    0.25f,  // low cruise
    0.50f,  // mid cruise
    0.75f,  // high cruise
    1.00f   // redline (WOT)
};

// ---------------------------------------------------------------------------
// Engine state
// ---------------------------------------------------------------------------

struct EngineState {
    float rpm = 0.0f;                  // current RPM
    float throttle = 0.0f;             // 0-1
    float load = 0.0f;                 // 0-1
    float phase[kMaxHarmonics] = {};   // per-harmonic phase accumulator (radians)
    float fm_phase_carrier = 0.0f;     // FM carrier phase
    float fm_phase_modulator = 0.0f;   // FM modulator phase
    // KS-style delay line (for strategy E)
    std::array<float, 4096> ks_delay_line{};
    int ks_delay_pos = 0;
    float ks_filter_state = 0.0f;
    float ks_lowpass_state = 0.0f;
    // Noise state (for strategy F)
    std::mt19937 rng{42};
};

// ---------------------------------------------------------------------------
// Per-vehicle profile memory footprint (for B_Phoneme_SamplePlayback)
// ---------------------------------------------------------------------------

struct VehicleProfile {
    VehicleClass vehicle_class;
    int cylinders;
    int idle_rpm;
    int redline_rpm;
    bool turbo;
    std::array<float, kMaxHarmonics> harmonic_weights;
    std::array<float, kBufferSamples> phoneme_sample;  // pre-rendered engine cycle
};

// ---------------------------------------------------------------------------
// Stats helper
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
// Strategy: A_NoEngineAudio
// Silent control - zero-fill output buffer.
// ---------------------------------------------------------------------------

void FillNoEngineAudio(VehicleProfile& prof, EngineState& state,
                       float* output, int n_samples) {
    (void)prof; (void)state;
    std::memset(output, 0, n_samples * sizeof(float));
}

void UpdateNoEngineAudio(VehicleProfile& prof, EngineState& state,
                         float rpm, float throttle, float load) {
    (void)prof; (void)state; (void)rpm; (void)throttle; (void)load;
}

// ---------------------------------------------------------------------------
// Strategy: B_Phoneme_SamplePlayback
// Pre-rendered engine cycle sample, looped with linear-interpolation pitch shift.
// Single multiplication per sample.
// ---------------------------------------------------------------------------

void InitPhonemeSample(VehicleProfile& prof) {
    // Pre-render a 1024-sample engine cycle: fundamental + 3 harmonics
    float base_freq = static_cast<float>(kIdleRpm[static_cast<int>(prof.vehicle_class)]) / 60.0f;
    float base_omega = kTwoPi * base_freq;
    for (int i = 0; i < kBufferSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float val = 0.0f;
        for (int h = 0; h < 4; ++h) {
            val += prof.harmonic_weights[h] * std::sin(base_omega * static_cast<float>(h + 1) * t)
                   / static_cast<float>(h + 1);
        }
        prof.phoneme_sample[i] = val * 0.5f;
    }
}

void UpdatePhonemeSample(VehicleProfile& prof, EngineState& state,
                         float rpm, float throttle, float load) {
    (void)prof; (void)load;
    state.rpm = rpm;
    state.throttle = throttle;
}

void FillPhonemeSample(VehicleProfile& prof, EngineState& state,
                       float* output, int n_samples) {
    // Pitch ratio = current rpm / idle rpm (so idle plays at original speed)
    float base_rpm = static_cast<float>(kIdleRpm[static_cast<int>(prof.vehicle_class)]);
    float pitch_ratio = state.rpm / base_rpm;
    if (pitch_ratio < 0.01f) pitch_ratio = 0.01f;
    if (pitch_ratio > 10.0f) pitch_ratio = 10.0f;
    float pos = static_cast<float>(state.ks_delay_pos);
    for (int i = 0; i < n_samples; ++i) {
        pos += pitch_ratio;
        while (pos >= kBufferSamples) pos -= static_cast<float>(kBufferSamples);
        int idx_lo = static_cast<int>(pos);
        int idx_hi = (idx_lo + 1) % kBufferSamples;
        float frac = pos - static_cast<float>(idx_lo);
        output[i] = prof.phoneme_sample[idx_lo] * (1.0f - frac)
                  + prof.phoneme_sample[idx_hi] * frac;
    }
    state.ks_delay_pos = static_cast<int>(pos);
}

// ---------------------------------------------------------------------------
// Strategy: C_AdditiveHarmonics_SumOfSines
// Fundamental + N=8 harmonics weighted by cylinder count.
// 8 sin() per sample.
// ---------------------------------------------------------------------------

void UpdateAdditiveHarmonics(VehicleProfile& prof, EngineState& state,
                             float rpm, float throttle, float load) {
    (void)prof; (void)load;
    state.rpm = rpm;
    state.throttle = throttle;
}

void FillAdditiveHarmonics(VehicleProfile& prof, EngineState& state,
                           float* output, int n_samples) {
    // Engine fundamental frequency: f_0 = RPM * cylinders / (60 * 2) Hz
    // (cylinders / 2 firing events per revolution for 4-stroke)
    float f0 = state.rpm * static_cast<float>(prof.cylinders) / 120.0f;
    if (f0 < 1.0f) f0 = 1.0f;
    float omega0 = kTwoPi * f0;
    float master_amp = 0.3f + 0.7f * state.throttle;  // louder at higher throttle
    for (int i = 0; i < n_samples; ++i) {
        float sum = 0.0f;
        for (int h = 0; h < kMaxHarmonics; ++h) {
            float phase_inc = omega0 * static_cast<float>(h + 1) / kSampleRate;
            state.phase[h] += phase_inc;
            if (state.phase[h] > kTwoPi) state.phase[h] -= kTwoPi;
            sum += prof.harmonic_weights[h] * std::sin(state.phase[h])
                   / static_cast<float>(h + 1);
        }
        output[i] = sum * master_amp * 0.3f;
    }
}

// ---------------------------------------------------------------------------
// Strategy: D_FM_2Operator
// 2-operator FM (carrier + modulator, Bessel function spectrum per Chowning 1973).
// Modulation index beta proportional to throttle load.
// 2 sin() per sample.
// ---------------------------------------------------------------------------

void UpdateFM2Operator(VehicleProfile& prof, EngineState& state,
                       float rpm, float throttle, float load) {
    (void)prof; (void)load;
    state.rpm = rpm;
    state.throttle = throttle;
}

void FillFM2Operator(VehicleProfile& prof, EngineState& state,
                     float* output, int n_samples) {
    // FM: FM(t) = A * sin(omega_c * t + beta * sin(omega_m * t))
    // Carrier = engine fundamental; Modulator = 2x carrier (produces 3rd-order Bessel sidebands).
    float f0 = state.rpm * static_cast<float>(prof.cylinders) / 120.0f;
    if (f0 < 1.0f) f0 = 1.0f;
    float omega_c = kTwoPi * f0;
    float omega_m = omega_c * 2.0f;
    // Modulation index beta = 0.5 (low) at idle, 5.0 (high) at redline -> rich spectrum
    float beta = 0.5f + 4.5f * state.throttle;
    float master_amp = 0.4f * (1.0f - 0.3f * (1.0f - state.throttle));
    float phase_inc_c = omega_c / kSampleRate;
    float phase_inc_m = omega_m / kSampleRate;
    for (int i = 0; i < n_samples; ++i) {
        state.fm_phase_modulator += phase_inc_m;
        if (state.fm_phase_modulator > kTwoPi) state.fm_phase_modulator -= kTwoPi;
        state.fm_phase_carrier += phase_inc_c;
        if (state.fm_phase_carrier > kTwoPi) state.fm_phase_carrier -= kTwoPi;
        float mod = beta * std::sin(state.fm_phase_modulator);
        output[i] = master_amp * std::sin(state.fm_phase_carrier + mod);
    }
}

// ---------------------------------------------------------------------------
// Strategy: E_PhysicalModeling_KarplusStrong_CombFilter
// Filtered delay-line feedback (delay length = Fs / (RPM * cylinders / 60 / 2) samples).
// Gain < 1 for stability. First-order LP on feedback.
// 1 read + 1 write + 1 filter op per sample.
// ---------------------------------------------------------------------------

void UpdateKarplusStrong(VehicleProfile& prof, EngineState& state,
                         float rpm, float throttle, float load) {
    (void)prof; (void)load;
    state.rpm = rpm;
    state.throttle = throttle;
}

// One-time delay-line initialization (called once per engine start, not per-tick).
// In production: called from EngineSoundComponent.OnAttach() or first RPM>0 event.
void InitKarplusStrongDelayLine(EngineState& state, unsigned seed) {
    state.rng.seed(seed);
    std::uniform_real_distribution<float> dist(-0.3f, 0.3f);
    for (auto& s : state.ks_delay_line) s = dist(state.rng);
    state.ks_filter_state = 1.0f;  // mark initialized
}

void FillKarplusStrong(VehicleProfile& prof, EngineState& state,
                       float* output, int n_samples) {
    // Delay length L = F_s / (f_0) = F_s / (RPM * cylinders / 120) samples
    float f0 = state.rpm * static_cast<float>(prof.cylinders) / 120.0f;
    if (f0 < 1.0f) f0 = 1.0f;
    int L = static_cast<int>(kSampleRate / f0);
    if (L < 4) L = 4;
    if (L >= static_cast<int>(state.ks_delay_line.size()))
        L = static_cast<int>(state.ks_delay_line.size()) - 1;
    float gain = 0.96f;  // <1 for stable feedback
    float lp_coef = 0.5f;  // first-order LP filter coefficient (averaging adjacent samples)
    float master_amp = 0.4f;
    int pos = state.ks_delay_pos;
    for (int i = 0; i < n_samples; ++i) {
        // Read from delay line
        float read_val = state.ks_delay_line[pos];
        // First-order LP filter: y[n] = (1-a)*y[n-1] + a*x[n]
        state.ks_lowpass_state = (1.0f - lp_coef) * state.ks_lowpass_state + lp_coef * read_val;
        // Write back (with gain)
        state.ks_delay_line[pos] = state.ks_lowpass_state * gain;
        output[i] = state.ks_lowpass_state * master_amp;
        pos++;
        if (pos >= L) pos = 0;
    }
    state.ks_delay_pos = pos;
}

// ---------------------------------------------------------------------------
// Strategy: F_Hybrid_AdditivePlusNoise
// C (8 harmonics) + filtered white noise for exhaust rumble.
// ~10 ops per sample.
// ---------------------------------------------------------------------------

void UpdateHybridAdditiveNoise(VehicleProfile& prof, EngineState& state,
                               float rpm, float throttle, float load) {
    (void)prof; (void)load;
    state.rpm = rpm;
    state.throttle = throttle;
}

void FillHybridAdditiveNoise(VehicleProfile& prof, EngineState& state,
                             float* output, int n_samples) {
    float f0 = state.rpm * static_cast<float>(prof.cylinders) / 120.0f;
    if (f0 < 1.0f) f0 = 1.0f;
    float omega0 = kTwoPi * f0;
    float master_amp = 0.3f + 0.5f * state.throttle;
    float noise_mix = 0.10f + 0.15f * state.throttle;  // more rumble at higher throttle
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    // Noise lowpass filter state (one-pole IIR)
    float& lp = state.ks_lowpass_state;
    float lp_coef = 0.05f;  // very narrow LP for rumble (low-freq)
    for (int i = 0; i < n_samples; ++i) {
        // C: additive harmonics
        float harm = 0.0f;
        for (int h = 0; h < kMaxHarmonics; ++h) {
            float phase_inc = omega0 * static_cast<float>(h + 1) / kSampleRate;
            state.phase[h] += phase_inc;
            if (state.phase[h] > kTwoPi) state.phase[h] -= kTwoPi;
            harm += prof.harmonic_weights[h] * std::sin(state.phase[h])
                    / static_cast<float>(h + 1);
        }
        // Filtered noise (exhaust rumble)
        float noise = dist(state.rng);
        lp = (1.0f - lp_coef) * lp + lp_coef * noise;
        output[i] = (harm * master_amp * 0.3f) + (lp * noise_mix);
    }
}

// ---------------------------------------------------------------------------
// Analytical reference (C with N=64 harmonics) for PSNR
// ---------------------------------------------------------------------------

void FillAnalyticalReference(VehicleProfile& prof, float rpm, float throttle,
                             float* output, int n_samples) {
    float f0 = rpm * static_cast<float>(prof.cylinders) / 120.0f;
    if (f0 < 1.0f) f0 = 1.0f;
    float omega0 = kTwoPi * f0;
    float master_amp = 0.3f + 0.7f * throttle;
    float phase[64] = {};
    int N = 64;
    for (int i = 0; i < n_samples; ++i) {
        float sum = 0.0f;
        for (int h = 0; h < N; ++h) {
            float phase_inc = omega0 * static_cast<float>(h + 1) / kSampleRate;
            phase[h] += phase_inc;
            if (phase[h] > kTwoPi) phase[h] -= kTwoPi;
            float w = (h < kMaxHarmonics) ? prof.harmonic_weights[h]
                   : prof.harmonic_weights[kMaxHarmonics - 1] * 0.5f;
            sum += w * std::sin(phase[h]) / static_cast<float>(h + 1);
        }
        output[i] = sum * master_amp * 0.3f;
    }
}

double ComputePSNR(const float* a, const float* b, int n) {
    double mse = 0.0;
    double max_val = 1.0;
    for (int i = 0; i < n; ++i) {
        double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        mse += d * d;
        if (std::abs(a[i]) > max_val) max_val = std::abs(a[i]);
        if (std::abs(b[i]) > max_val) max_val = std::abs(b[i]);
    }
    mse /= static_cast<double>(n);
    if (mse < 1e-12) return 99.0;  // essentially perfect
    double psnr = 10.0 * std::log10((max_val * max_val) / mse);
    return psnr;
}

// ---------------------------------------------------------------------------
// Build profile
// ---------------------------------------------------------------------------

VehicleProfile MakeProfile(VehicleClass vc) {
    VehicleProfile p{};
    p.vehicle_class = vc;
    p.cylinders = kCylinders[static_cast<int>(vc)];
    p.idle_rpm = kIdleRpm[static_cast<int>(vc)];
    p.redline_rpm = kRedlineRpm[static_cast<int>(vc)];
    p.turbo = kTurboFlag[static_cast<int>(vc)];
    for (int h = 0; h < kMaxHarmonics; ++h) {
        p.harmonic_weights[h] = kHarmonicWeights[static_cast<int>(vc)][h];
    }
    InitPhonemeSample(p);
    return p;
}

// ---------------------------------------------------------------------------
// Bench harness
// ---------------------------------------------------------------------------

struct BenchResult {
    double update_ns_mean = 0.0;
    double update_ns_p95 = 0.0;
    double fill_us_mean = 0.0;
    double fill_us_p95 = 0.0;
    double psnr_db = 0.0;
    size_t memory_bytes = 0;
};

template <typename UpdateFn, typename FillFn>
BenchResult RunStrategy(VehicleClass vc, int rpm_idx, unsigned seed,
                        UpdateFn update_fn, FillFn fill_fn,
                        const char* strategy_name) {
    (void)strategy_name;
    VehicleProfile prof = MakeProfile(vc);
    EngineState state;
    std::array<float, kBufferSamples> buf;
    std::array<float, kBufferSamples> ref_buf;
    float rpm = kIdleRpm[static_cast<int>(vc)]
              + (kRedlineRpm[static_cast<int>(vc)] - kIdleRpm[static_cast<int>(vc)])
                * kRpmProfile[rpm_idx];
    float throttle = kRpmProfile[rpm_idx];
    float load = kRpmProfile[rpm_idx] * 0.8f + 0.1f;

    // One-time engine start: initialize KS delay line (E strategy only).
    // In production: this is called once per engine start event, not per-tick.
    if (rpm > 100.0f) {
        InitKarplusStrongDelayLine(state, seed);
    }

    // Analytical reference for PSNR
    FillAnalyticalReference(prof, rpm, throttle, ref_buf.data(), kBufferSamples);

    std::vector<double> update_times_ns;
    std::vector<double> fill_times_us;
    update_times_ns.reserve(1000);
    fill_times_us.reserve(1000);

    const int kWarmup = 10;
    const int kIterations = 1000;

    // Warmup (steady-state; no state reset)
    for (int w = 0; w < kWarmup; ++w) {
        update_fn(prof, state, rpm, throttle, load);
        fill_fn(prof, state, buf.data(), kBufferSamples);
    }

    // Bench: steady-state (no state reset between iterations — measures real per-tick cost)
    auto t0 = std::chrono::high_resolution_clock::time_point{};
    auto t1 = std::chrono::high_resolution_clock::time_point{};
    for (int it = 0; it < kIterations; ++it) {
        // Parameter update
        t0 = std::chrono::high_resolution_clock::now();
        update_fn(prof, state, rpm, throttle, load);
        t1 = std::chrono::high_resolution_clock::now();
        double update_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        update_times_ns.push_back(update_ns);
        // Fill buffer
        t0 = std::chrono::high_resolution_clock::now();
        fill_fn(prof, state, buf.data(), kBufferSamples);
        t1 = std::chrono::high_resolution_clock::now();
        double fill_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        fill_times_us.push_back(fill_us);
    }

    // PSNR: fresh-state run (single iteration) for clean phase comparison vs analytical reference
    state = EngineState{};
    if (rpm > 100.0f) {
        InitKarplusStrongDelayLine(state, seed);
    }
    update_fn(prof, state, rpm, throttle, load);
    fill_fn(prof, state, buf.data(), kBufferSamples);
    double psnr = ComputePSNR(buf.data(), ref_buf.data(), kBufferSamples);

    BenchResult r{};
    Stats su = ComputeStats(update_times_ns);
    Stats sf = ComputeStats(fill_times_us);
    r.update_ns_mean = su.mean;
    r.update_ns_p95 = su.p95;
    r.fill_us_mean = sf.mean;
    r.fill_us_p95 = sf.p95;
    r.psnr_db = psnr;
    // Memory: sizeof EngineState (without delay_line) + profile
    r.memory_bytes = sizeof(EngineState) + sizeof(VehicleProfile);
    return r;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    const unsigned kSeeds[5] = {1, 7, 42, 1234, 31337};

    FILE* csv = std::fopen("build/results.csv", "w");
    if (!csv) {
        std::fprintf(stderr, "FATAL: cannot open build/results.csv\n");
        return 1;
    }
    std::fprintf(csv, "strategy,vehicle,rpm_profile,seed,update_ns_mean,update_ns_p95,"
                      "fill_us_mean,fill_us_p95,psnr_db,memory_bytes\n");

    // Measure: 6 strategies x 5 vehicles x 5 RPM profiles x 5 seeds = 750 configs
    struct StrategyDef {
        const char* name;
        void (*update)(VehicleProfile&, EngineState&, float, float, float);
        void (*fill)(VehicleProfile&, EngineState&, float*, int);
    };
    StrategyDef strategies[6] = {
        {"A_NoEngineAudio",          UpdateNoEngineAudio,        FillNoEngineAudio},
        {"B_Phoneme_SamplePlayback", UpdatePhonemeSample,        FillPhonemeSample},
        {"C_AdditiveHarmonics",      UpdateAdditiveHarmonics,    FillAdditiveHarmonics},
        {"D_FM_2Operator",           UpdateFM2Operator,          FillFM2Operator},
        {"E_KarplusStrong_Comb",     UpdateKarplusStrong,        FillKarplusStrong},
        {"F_Hybrid_AdditiveNoise",   UpdateHybridAdditiveNoise,  FillHybridAdditiveNoise}
    };

    // We need a single dispatch helper because RunStrategy is templated; use a lambda per strategy.
    int total_configs = 6 * 5 * 5 * 5;
    int config_count = 0;

    for (int s = 0; s < 6; ++s) {
        for (int v = 0; v < 5; ++v) {
            VehicleClass vc = static_cast<VehicleClass>(v);
            for (int r = 0; r < 5; ++r) {
                for (unsigned seed : kSeeds) {
                    BenchResult res{};
                    switch (s) {
                        case 0: res = RunStrategy(vc, r, seed, UpdateNoEngineAudio, FillNoEngineAudio, "A"); break;
                        case 1: res = RunStrategy(vc, r, seed, UpdatePhonemeSample, FillPhonemeSample, "B"); break;
                        case 2: res = RunStrategy(vc, r, seed, UpdateAdditiveHarmonics, FillAdditiveHarmonics, "C"); break;
                        case 3: res = RunStrategy(vc, r, seed, UpdateFM2Operator, FillFM2Operator, "D"); break;
                        case 4: res = RunStrategy(vc, r, seed, UpdateKarplusStrong, FillKarplusStrong, "E"); break;
                        case 5: res = RunStrategy(vc, r, seed, UpdateHybridAdditiveNoise, FillHybridAdditiveNoise, "F"); break;
                        default: break;
                    }
                    std::fprintf(csv, "%s,%s,%d,%u,%.1f,%.1f,%.4f,%.4f,%.2f,%zu\n",
                                 strategies[s].name,
                                 kVehicleNames[v],
                                 r, seed,
                                 res.update_ns_mean, res.update_ns_p95,
                                 res.fill_us_mean, res.fill_us_p95,
                                 res.psnr_db, res.memory_bytes);
                    config_count++;
                    if (config_count % 50 == 0) {
                        std::fprintf(stderr, "Progress: %d / %d configs\n",
                                     config_count, total_configs);
                    }
                }
            }
        }
    }
    std::fclose(csv);
    std::fprintf(stderr, "Done: %d configs written to build/results.csv\n", config_count);

    // Print summary table to stdout
    std::printf("\n=== Summary (per strategy, mean across all configs) ===\n");
    std::printf("%-32s %12s %12s %12s %10s\n",
                "Strategy", "Upd ns", "Fill us", "PSNR dB", "Mem KiB");
    std::printf("------------------------------------------------------------------------\n");

    FILE* csv_r = std::fopen("build/results.csv", "r");
    if (csv_r) {
        char line[256];
        std::fgets(line, sizeof(line), csv_r);  // skip header
        struct Acc { double update = 0; double fill = 0; double psnr = 0; size_t mem = 0; int n = 0; };
        Acc acc[6] = {};
        while (std::fgets(line, sizeof(line), csv_r)) {
            char strat[64] = {};
            char veh[32] = {};
            int rpm = 0;
            unsigned seed = 0;
            double un = 0, up95 = 0, fm = 0, fp95 = 0, psnr = 0;
            size_t mem = 0;
            int matched = std::sscanf(line, "%63[^,],%31[^,],%d,%u,%lf,%lf,%lf,%lf,%lf,%zu",
                                     strat, veh, &rpm, &seed, &un, &up95, &fm, &fp95, &psnr, &mem);
            if (matched < 10) continue;
            int idx = -1;
            if (std::strcmp(strat, "A_NoEngineAudio") == 0) idx = 0;
            else if (std::strcmp(strat, "B_Phoneme_SamplePlayback") == 0) idx = 1;
            else if (std::strcmp(strat, "C_AdditiveHarmonics") == 0) idx = 2;
            else if (std::strcmp(strat, "D_FM_2Operator") == 0) idx = 3;
            else if (std::strcmp(strat, "E_KarplusStrong_Comb") == 0) idx = 4;
            else if (std::strcmp(strat, "F_Hybrid_AdditiveNoise") == 0) idx = 5;
            if (idx < 0) continue;
            acc[idx].update += un;
            acc[idx].fill += fm;
            acc[idx].psnr += psnr;
            acc[idx].mem = mem;
            acc[idx].n++;
        }
        std::fclose(csv_r);
        for (int i = 0; i < 6; ++i) {
            if (acc[i].n > 0) {
                std::printf("%-32s %12.1f %12.3f %12.2f %10zu\n",
                            strategies[i].name,
                            acc[i].update / acc[i].n,
                            acc[i].fill / acc[i].n,
                            acc[i].psnr / acc[i].n,
                            acc[i].mem / 1024);
            }
        }
    }

    // Write run.log
    FILE* log = std::fopen("build/run.log", "w");
    if (log) {
        std::fprintf(log, "2026-06-22-procedural-engine-sound\n");
        std::fprintf(log, "Configs: %d (6 strategies x 5 vehicles x 5 RPM profiles x 5 seeds)\n", config_count);
        std::fprintf(log, "Wall time target: <60 sec @ Zen 3 5800X governor=performance\n");
        std::fclose(log);
    }

    return 0;
}
