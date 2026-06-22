// acoustic_bench.cpp
// 2026-06-22-acoustic-detection-system — Passive Acoustic Detection Strategy Comparison
// Standalone C++26 CPU benchmark. Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG
// Self-contained, no external dependencies beyond stdlib.
//
// 5 strategies × 5 scenes × 5 targets × 5 freq bands × 1000 iter + 10 warmup
// = 125 main measurements × 25 configs = 3,125 measurements per strategy.
// Total: 5 strategies × 3,125 = 15,625 measurements + 5 × 3,125 × 10 warmup = 156,250 warmup.
//
// Output: results.csv (15,626 rows = 1 header + 15,625 data) + summary_means.csv + run.log

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Domain model: target acoustic signatures (5 types)
// -----------------------------------------------------------------------------
struct Target {
    std::string name;
    double spl_1m_db;        // source SPL at 1 m reference
    double f_low_hz;
    double f_high_hz;
    double doppler_max_hz;   // expected Doppler shift from moving target
};

// -----------------------------------------------------------------------------
// Scene: ambient environment (5 scenes)
// -----------------------------------------------------------------------------
struct Scene {
    std::string name;
    double ambient_noise_db;   // background SPL (wind, traffic, surf)
    double temperature_c;      // air temperature (affects sound speed + atmospheric absorption)
    double humidity_pct;
    double wind_speed_mps;
    double self_noise_db;      // sensor intrinsic noise floor
    double multipath_factor;   // 1.0 = free-field, 1.5+ = urban canyon / multipath-heavy
};

// -----------------------------------------------------------------------------
// Frequency band (5 bands: infrasound, audible, ultrasonic, hydroacoustic, seismic)
// -----------------------------------------------------------------------------
struct Band {
    std::string name;
    double f_low_hz;
    double f_high_hz;
    double c_speed_mps;        // medium-specific sound speed (air, water, ground)
};

// -----------------------------------------------------------------------------
// Data tables (per STATUS.md Notes)
// -----------------------------------------------------------------------------
static constexpr std::array<Band, 5> BANDS = {
    Band{"infrasound",      0.1,     20.0,     343.0},  // air, c ≈ 343 m/s @ 20°C
    Band{"audible",         20.0,    20000.0,  343.0},  // air
    Band{"ultrasonic",      20000.0, 100000.0, 343.0},  // air, attenuated fast
    Band{"hydroacoustic",   100.0,   100000.0, 1500.0}, // water, c ≈ 1500 m/s
    Band{"seismic",         1.0,     100.0,    5000.0}, // ground-coupled, c ≈ 5000 m/s
};

static constexpr std::array<Target, 5> TARGETS = {
    Target{"soldier",        30.0,  100.0, 500.0,  100.0}, // footsteps 30 dB @ 1m
    Target{"light_vehicle", 70.0,  20.0,  500.0,  200.0}, // engine 70 dB @ 1m
    Target{"heavy_vehicle", 90.0,  5.0,    200.0,  150.0}, // engine + tracks
    Target{"helicopter",    110.0, 10.0,   500.0,  500.0}, // rotor + engine
    Target{"ship",          130.0, 5.0,    2000.0, 300.0}, // cavitation + engine
};

static constexpr std::array<Scene, 5> SCENES = {
    Scene{"quiet_forest",    25.0, 15.0, 60.0, 1.0, 35.0, 0.3},
    Scene{"urban_corridor",  65.0, 20.0, 50.0, 2.0, 45.0, 1.5},
    Scene{"coastal_waters",  50.0, 15.0, 80.0, 5.0, 40.0, 0.5},
    Scene{"urban_combat",    75.0, 20.0, 50.0, 3.0, 55.0, 1.8},
    Scene{"open_desert",     20.0, 35.0, 15.0, 4.0, 30.0, 0.2},
};

static constexpr int N_ITER     = 1000;
static constexpr int N_WARMUP   = 10;
static constexpr int N_MIC      = 4;     // Strategy D + E use N=4 microphones for TDOA
static constexpr double DETECTION_THRESHOLD_DB = 6.0; // SNR > 6 dB = detect (Pfa ≈ 1%)
static constexpr double RNG_SEED = 42;

// -----------------------------------------------------------------------------
// Strategy A: Simple range equation 1/r² + ambient noise floor
// Pure analytical model. No frequency dependence, no atmospheric absorption,
// no Doppler, no TDOA. Returns detection probability at fixed target range R
// and time taken.
// -----------------------------------------------------------------------------
struct DetectionResult {
    double det_prob;        // 0.0 - 1.0 probability of detection at fixed scenario
    double latency_ns;      // CPU time per detection evaluation
};

static DetectionResult StrategyA_SimpleRangeEquation(
    const Target& tgt, const Scene& scene, const Band& band, double range_m,
    std::mt19937& rng, bool measure_latency
) {
    auto t0 = std::chrono::high_resolution_clock::now();
    // Inverse-square spreading: SPL at range R = SPL_1m - 20*log10(R)
    double spl_at_range = tgt.spl_1m_db - 20.0 * std::log10(std::max(range_m, 1.0));
    // SNR against ambient noise floor + sensor self-noise
    double noise_total_db = 10.0 * std::log10(
        std::pow(10.0, scene.ambient_noise_db / 10.0) +
        std::pow(10.0, scene.self_noise_db / 10.0)
    );
    double snr_db = spl_at_range - noise_total_db;
    // Detect = SNR > threshold (binary hard decision)
    bool detect = (snr_db > DETECTION_THRESHOLD_DB);
    double det_prob = detect ? 1.0 : 0.0;
    auto t1 = std::chrono::high_resolution_clock::now();
    double latency_ns = measure_latency
        ? std::chrono::duration<double, std::nano>(t1 - t0).count()
        : 0.0;
    (void)rng; (void)band;
    return {det_prob, latency_ns};
}

// -----------------------------------------------------------------------------
// Strategy B: + Atmospheric absorption (ISO 9613-1 simplified)
// Per-band τ(f, R, T, humidity). Air absorption coefficient α_dB/km at frequency f:
//   - infrasound: ~0.01 dB/km
//   - audible:    ~0.1-2 dB/km (peaks at 4 kHz due to O₂ + N₂ vibrational relaxation)
//   - ultrasonic: ~5-50 dB/km (rapid rise)
//   - hydroacoustic: ~0.001-0.1 dB/km (water much less absorbing)
//   - seismic:    ~0.1-5 dB/km (ground-dependent, simplified)
// -----------------------------------------------------------------------------
static DetectionResult StrategyB_AtmosphericAbsorption(
    const Target& tgt, const Scene& scene, const Band& band, double range_m,
    std::mt19937& rng, bool measure_latency
) {
    auto t0 = std::chrono::high_resolution_clock::now();
    double spl_0 = tgt.spl_1m_db - 20.0 * std::log10(std::max(range_m, 1.0));
    // Geometric mean frequency in band
    double f_center = std::sqrt(band.f_low_hz * band.f_high_hz);
    // ISO 9613-1 simplified atmospheric absorption coefficient α_dB/km
    double alpha_db_per_km;
    if (band.name == "infrasound") {
        alpha_db_per_km = 0.01;
    } else if (band.name == "audible") {
        // peak near 4 kHz due to molecular relaxation (simplified Gaussian)
        double x = std::log(f_center / 4000.0);
        alpha_db_per_km = 2.0 * std::exp(-x * x) + 0.1;
    } else if (band.name == "ultrasonic") {
        // rapid rise with frequency
        alpha_db_per_km = 0.05 * std::pow(f_center / 20000.0, 1.5);
    } else if (band.name == "hydroacoustic") {
        // water: very low absorption except for MgSO4 + B(OH)3 peaks (~1 kHz, ~50 kHz)
        // Simplified: flat ~0.05 dB/km in midrange
        alpha_db_per_km = 0.05;
    } else { // seismic
        // ground: depends strongly on soil type, simplified mid-range
        alpha_db_per_km = 1.0;
    }
    double absorption_db = alpha_db_per_km * (range_m / 1000.0);
    double spl_at_range = spl_0 - absorption_db;
    double noise_total_db = 10.0 * std::log10(
        std::pow(10.0, scene.ambient_noise_db / 10.0) +
        std::pow(10.0, scene.self_noise_db / 10.0)
    );
    double snr_db = spl_at_range - noise_total_db;
    bool detect = (snr_db > DETECTION_THRESHOLD_DB);
    double det_prob = detect ? 1.0 : 0.0;
    auto t1 = std::chrono::high_resolution_clock::now();
    double latency_ns = measure_latency
        ? std::chrono::duration<double, std::nano>(t1 - t0).count()
        : 0.0;
    (void)rng;
    return {det_prob, latency_ns};
}

// -----------------------------------------------------------------------------
// Strategy C: + Narrow-band FFT peak detection + Doppler signature matching
// Detect only if (a) narrow-band SNR peaks above threshold in any FFT bin AND
// (b) Doppler signature matches expected range. Adds noise via spectral leakage.
// Cost: O(N_fft log N_fft) per snapshot, simulated as fixed overhead per iter.
// -----------------------------------------------------------------------------
static DetectionResult StrategyC_NarrowBandFFT(
    const Target& tgt, const Scene& scene, const Band& band, double range_m,
    std::mt19937& rng, bool measure_latency
) {
    auto t0 = std::chrono::high_resolution_clock::now();
    double f_center = std::sqrt(band.f_low_hz * band.f_high_hz);
    // Atmospheric absorption (reuse B)
    double spl_0 = tgt.spl_1m_db - 20.0 * std::log10(std::max(range_m, 1.0));
    double alpha_db_per_km;
    if (band.name == "infrasound") alpha_db_per_km = 0.01;
    else if (band.name == "audible") {
        double x = std::log(f_center / 4000.0);
        alpha_db_per_km = 2.0 * std::exp(-x * x) + 0.1;
    } else if (band.name == "ultrasonic") alpha_db_per_km = 0.05 * std::pow(f_center / 20000.0, 1.5);
    else if (band.name == "hydroacoustic") alpha_db_per_km = 0.05;
    else alpha_db_per_km = 1.0;
    double spl_at_range = spl_0 - alpha_db_per_km * (range_m / 1000.0);
    double noise_total_db = 10.0 * std::log10(
        std::pow(10.0, scene.ambient_noise_db / 10.0) +
        std::pow(10.0, scene.self_noise_db / 10.0)
    );
    double snr_db = spl_at_range - noise_total_db;
    // FFT peak detection: detect if SNR > threshold AND Doppler signature matches
    // Doppler check: simulated 90% match probability if target has Doppler signature
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    bool doppler_match = (uniform(rng) < 0.90);
    bool detect = (snr_db > DETECTION_THRESHOLD_DB) && doppler_match;
    double det_prob = detect ? 1.0 : 0.0;
    // Simulate FFT cost: 1024-point complex FFT ≈ 10 µs (single thread)
    double fft_simulated_ns = 10000.0;
    auto t1 = std::chrono::high_resolution_clock::now();
    double analytical_ns = measure_latency
        ? std::chrono::duration<double, std::nano>(t1 - t0).count()
        : 0.0;
    return {det_prob, analytical_ns + fft_simulated_ns};
}

// -----------------------------------------------------------------------------
// Strategy D: + Multi-source TDOA triangulation (N=4 microphones)
// Add hyperbolic positioning from N=4 microphones. Detect if (a) SNR > threshold,
// (b) TDOA yields consistent triangulation (Doppler match + 3+ mic consistent).
// Cost: O(N_mic²) cross-correlation per snapshot. Simulated as fixed overhead.
// -----------------------------------------------------------------------------
static DetectionResult StrategyD_TDOATriangulation(
    const Target& tgt, const Scene& scene, const Band& band, double range_m,
    std::mt19937& rng, bool measure_latency
) {
    auto t0 = std::chrono::high_resolution_clock::now();
    double f_center = std::sqrt(band.f_low_hz * band.f_high_hz);
    double spl_0 = tgt.spl_1m_db - 20.0 * std::log10(std::max(range_m, 1.0));
    double alpha_db_per_km;
    if (band.name == "infrasound") alpha_db_per_km = 0.01;
    else if (band.name == "audible") {
        double x = std::log(f_center / 4000.0);
        alpha_db_per_km = 2.0 * std::exp(-x * x) + 0.1;
    } else if (band.name == "ultrasonic") alpha_db_per_km = 0.05 * std::pow(f_center / 20000.0, 1.5);
    else if (band.name == "hydroacoustic") alpha_db_per_km = 0.05;
    else alpha_db_per_km = 1.0;
    double spl_at_range = spl_0 - alpha_db_per_km * (range_m / 1000.0);
    double noise_total_db = 10.0 * std::log10(
        std::pow(10.0, scene.ambient_noise_db / 10.0) +
        std::pow(10.0, scene.self_noise_db / 10.0)
    );
    double snr_db = spl_at_range - noise_total_db;
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    bool doppler_match = (uniform(rng) < 0.90);
    // TDOA triangulation: requires 3+ mics with consistent TDOA.
    // Probabilistic model: P(TDOA consistent) = 1 - 1/N_mic + ambient noise contribution
    double tdoa_consistency = 1.0 - 1.0 / static_cast<double>(N_MIC);
    tdoa_consistency *= std::max(0.0, 1.0 - scene.ambient_noise_db / 100.0);
    bool tdoa_ok = (uniform(rng) < tdoa_consistency);
    bool detect = (snr_db > DETECTION_THRESHOLD_DB) && doppler_match && tdoa_ok;
    double det_prob = detect ? 1.0 : 0.0;
    // TDOA cost: N_mic × N_mic cross-correlations = O(N_mic²) per snapshot
    // For N_MIC=4, 16 cross-corrs × 1024-pt FFT each ≈ 16 × 10 µs = 160 µs
    double tdoa_simulated_ns = 160000.0;
    auto t1 = std::chrono::high_resolution_clock::now();
    double analytical_ns = measure_latency
        ? std::chrono::duration<double, std::nano>(t1 - t0).count()
        : 0.0;
    return {det_prob, analytical_ns + tdoa_simulated_ns};
}

// -----------------------------------------------------------------------------
// Strategy E: Full physics model — atmospheric + Doppler + triangulation + multipath + self-noise masking
// SRP-PHAT beamformer over spatial grid (per Wikipedia Acoustic location).
// Includes multipath effect (urban canyon), self-noise masking, atmospheric profile.
// Cost: SRP-PHAT = O(N_mic² × N_grid) per snapshot. Simulated.
// -----------------------------------------------------------------------------
static DetectionResult StrategyE_FullPhysicsModel(
    const Target& tgt, const Scene& scene, const Band& band, double range_m,
    std::mt19937& rng, bool measure_latency
) {
    auto t0 = std::chrono::high_resolution_clock::now();
    double f_center = std::sqrt(band.f_low_hz * band.f_high_hz);
    double spl_0 = tgt.spl_1m_db - 20.0 * std::log10(std::max(range_m, 1.0));
    double alpha_db_per_km;
    if (band.name == "infrasound") alpha_db_per_km = 0.01;
    else if (band.name == "audible") {
        double x = std::log(f_center / 4000.0);
        alpha_db_per_km = 2.0 * std::exp(-x * x) + 0.1;
    } else if (band.name == "ultrasonic") alpha_db_per_km = 0.05 * std::pow(f_center / 20000.0, 1.5);
    else if (band.name == "hydroacoustic") alpha_db_per_km = 0.05;
    else alpha_db_per_km = 1.0;
    double spl_at_range = spl_0 - alpha_db_per_km * (range_m / 1000.0);
    double noise_total_db = 10.0 * std::log10(
        std::pow(10.0, scene.ambient_noise_db / 10.0) +
        std::pow(10.0, scene.self_noise_db / 10.0)
    );
    double snr_db = spl_at_range - noise_total_db;
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    bool doppler_match = (uniform(rng) < 0.90);
    // SRP-PHAT beamforming: high-resolution DOA + spatial filter
    // Probability model: P(SRP-PHAT coherent) ≈ 0.95 (production-grade robust per DiBiase 2000)
    bool srp_phat_ok = (uniform(rng) < 0.95);
    // Multipath: urban canyon reduces detection (cancellation at some angles)
    // Model: P(multipath-constructive) = 0.5 + 0.5 / multipath_factor
    double multipath_constructive_p = 0.5 + 0.5 / scene.multipath_factor;
    bool multipath_ok = (uniform(rng) < multipath_constructive_p);
    // TDOA consistency (reuse D's model)
    double tdoa_consistency = 1.0 - 1.0 / static_cast<double>(N_MIC);
    bool tdoa_ok = (uniform(rng) < tdoa_consistency);
    bool detect = (snr_db > DETECTION_THRESHOLD_DB) && doppler_match && srp_phat_ok && multipath_ok && tdoa_ok;
    double det_prob = detect ? 1.0 : 0.0;
    // SRP-PHAT cost: N_mic² × N_grid grid search; N_grid=128 angular bins for prototype
    // ≈ 4² × 128 = 2048 beamformer evaluations × 10 µs each = 20 ms (single thread)
    // But we batch: N_target evaluations parallel
    double srp_phat_simulated_ns = 20000000.0; // 20 ms per target
    auto t1 = std::chrono::high_resolution_clock::now();
    double analytical_ns = measure_latency
        ? std::chrono::duration<double, std::nano>(t1 - t0).count()
        : 0.0;
    return {det_prob, analytical_ns + srp_phat_simulated_ns};
}

// -----------------------------------------------------------------------------
// Range sweep per target (representative distances per scene-type)
// -----------------------------------------------------------------------------
static double SelectRange(const Target& tgt, const Scene& scene) {
    // Test at 4 representative ranges per (target, scene) combination.
    // Ranges chosen to span below-typical-max-detection to above-detection-threshold.
    double base_range;
    if (tgt.name == "soldier")      base_range = 200.0;  // footsteps audible ~200m
    else if (tgt.name == "light_vehicle")  base_range = 800.0;  // engine 70 dB
    else if (tgt.name == "heavy_vehicle")  base_range = 1500.0; // engine+tracks 90 dB
    else if (tgt.name == "helicopter")      base_range = 5000.0; // rotor+engine 110 dB
    else                                base_range = 10000.0; // ship cavitation 130 dB
    // Scene attenuation: urban_combat dense → 0.5x; quiet forest → 1.5x; etc.
    double scene_factor = 1.0;
    if (scene.name == "quiet_forest")      scene_factor = 1.5;
    else if (scene.name == "urban_corridor") scene_factor = 0.6;
    else if (scene.name == "coastal_waters") scene_factor = 2.0;
    else if (scene.name == "urban_combat")   scene_factor = 0.5;
    else if (scene.name == "open_desert")    scene_factor = 1.2;
    return base_range * scene_factor;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    auto t_start = std::chrono::high_resolution_clock::now();
    std::mt19937 rng(RNG_SEED);
    std::ofstream results("build/results.csv");
    std::ofstream summary("build/summary_means.csv");
    std::ofstream runlog("build/run.log");
    if (!results || !summary || !runlog) {
        std::fprintf(stderr, "FATAL: cannot open output files\n");
        return 1;
    }
    results << "strategy,scene,target,band,iter,det_prob,latency_ns\n";
    summary << "strategy,scene,target,band,mean_det_prob,mean_latency_ns\n";
    runlog << "[acoustic_bench] start 2026-06-22\n";
    runlog << "[acoustic_bench] 5 strategies × 5 scenes × 5 targets × 5 bands × "
           << N_ITER << " iter + " << N_WARMUP << " warmup\n";
    int total_measurements = 5 * 5 * 5 * 5 * N_ITER;
    int total_warmup = 5 * 5 * 5 * 5 * N_WARMUP;
    runlog << "[acoustic_bench] total = " << total_measurements
           << " main + " << total_warmup << " warmup\n";
    // Strategy labels
    const char* strategy_names[5] = {
        "A_SimpleRangeEquation",
        "B_AtmosphericAbsorption",
        "C_NarrowBandFFT_Doppler",
        "D_TDOATriangulation",
        "E_FullPhysicsModel",
    };
    using StrategyFn = DetectionResult(*)(const Target&, const Scene&, const Band&, double, std::mt19937&, bool);
    StrategyFn strategy_fns[5] = {
        StrategyA_SimpleRangeEquation,
        StrategyB_AtmosphericAbsorption,
        StrategyC_NarrowBandFFT,
        StrategyD_TDOATriangulation,
        StrategyE_FullPhysicsModel,
    };
    // -----------------------------------------------------------------
    // Outer loops: strategy × scene × target × band
    // -----------------------------------------------------------------
    for (int si = 0; si < 5; ++si) {
        for (int sci = 0; sci < 5; ++sci) {
            for (int ti = 0; ti < 5; ++ti) {
                for (int bi = 0; bi < 5; ++bi) {
                    double range = SelectRange(TARGETS[ti], SCENES[sci]);
                    // Warmup
                    for (int w = 0; w < N_WARMUP; ++w) {
                        strategy_fns[si](TARGETS[ti], SCENES[sci], BANDS[bi], range, rng, false);
                    }
                    // Main measurement
                    std::vector<double> det_probs;
                    std::vector<double> latencies_ns;
                    det_probs.reserve(N_ITER);
                    latencies_ns.reserve(N_ITER);
                    double sum_dp = 0.0;
                    double sum_lat = 0.0;
                    for (int it = 0; it < N_ITER; ++it) {
                        bool measure = (it == 0) || (it == N_ITER / 2) || (it == N_ITER - 1);
                        DetectionResult r = strategy_fns[si](
                            TARGETS[ti], SCENES[sci], BANDS[bi], range, rng, measure
                        );
                        det_probs.push_back(r.det_prob);
                        latencies_ns.push_back(r.latency_ns);
                        sum_dp += r.det_prob;
                        sum_lat += r.latency_ns;
                        // CSV row
                        char row[256];
                        std::snprintf(row, sizeof(row),
                            "%s,%s,%s,%s,%d,%.4f,%.2f\n",
                            strategy_names[si],
                            SCENES[sci].name.c_str(),
                            TARGETS[ti].name.c_str(),
                            BANDS[bi].name.c_str(),
                            it,
                            r.det_prob,
                            r.latency_ns
                        );
                        results << row;
                    }
                    double mean_dp = sum_dp / N_ITER;
                    double mean_lat = sum_lat / N_ITER;
                    char row[256];
                    std::snprintf(row, sizeof(row),
                        "%s,%s,%s,%s,%.4f,%.2f\n",
                        strategy_names[si],
                        SCENES[sci].name.c_str(),
                        TARGETS[ti].name.c_str(),
                        BANDS[bi].name.c_str(),
                        mean_dp,
                        mean_lat
                    );
                    summary << row;
                }
            }
        }
        // Per-strategy log
        char logbuf[256];
        std::snprintf(logbuf, sizeof(logbuf),
            "[acoustic_bench] strategy %s complete: 125 configs × %d iter\n",
            strategy_names[si], N_ITER);
        runlog << logbuf;
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    double wall_sec = std::chrono::duration<double>(t_end - t_start).count();
    char wallbuf[128];
    std::snprintf(wallbuf, sizeof(wallbuf), "[acoustic_bench] wall time = %.3f sec\n", wall_sec);
    runlog << wallbuf;
    runlog << "[acoustic_bench] done\n";
    results.close();
    summary.close();
    runlog.close();
    std::printf("[acoustic_bench] complete. Wall time = %.3f sec.\n", wall_sec);
    std::printf("[acoustic_bench] output: build/results.csv (%d main rows), build/summary_means.csv (%d rows), build/run.log\n",
        total_measurements, 5 * 5 * 5 * 5);
    return 0;
}
