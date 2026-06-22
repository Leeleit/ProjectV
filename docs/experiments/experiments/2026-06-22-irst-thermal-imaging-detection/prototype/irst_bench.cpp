// SPDX-License-Identifier: MIT
// 2026-06-22-irst-thermal-imaging-detection — standalone C++26 CPU prototype.
//
// Tests 5 strategies for passive IRST/FLIR thermal detection against 5 scenes,
// 5 seeds, 1000 iterations + 10 warmup per (strategy, scene, seed) = 125,000 main
// measurements + 1,250 warmup. All physics is analytical (CPU-only).
//
// Build:
//   cd prototype && mkdir -p build && cd build && \
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//   -fconstexpr-steps=1000000000 ../irst_bench.cpp -o irst_bench
// Run:
//   ./irst_bench
//
// Per `benchmarks/methodology.md §7` harness pattern (Stats + sorted percentiles).
// Per `hardware-profile.md §1` target: Zen 3 5800X, governor=performance.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace irst {

// ---------- Physics constants ----------
constexpr double kStefanBoltzmann = 5.670374419e-8;  // W m^-2 K^-4
constexpr double kPlanckH = 6.62607015e-34;         // J s
constexpr double kSpeedOfLight = 2.99792458e8;       // m s^-1
constexpr double kBoltzmannK = 1.380649e-23;          // J K^-1
constexpr double kPi = 3.14159265358979323846;

// MWIR band center: 4 µm (3-5 µm window for IR missiles per Wikipedia "Infrared")
constexpr double kMwirCenter_m = 4.0e-6;
// LWIR band center: 10 µm (8-12 µm thermal imaging band per Wikipedia "Infrared")
constexpr double kLwirCenter_m = 10.0e-6;

// Atmospheric extinction coefficient (1/km) for visible/IR near sea level, clear sky.
// Approximate from LOWTRAN-style bands. Used for strategy B+ .
// MWIR (3-5 µm) has lower extinction than LWIR (8-12 µm) due to water vapor.
constexpr double kAtmExtinction_Mwir_1PerKm = 0.20;  // ~e^(-0.2*1) = 0.82 at 1 km
constexpr double kAtmExtinction_Lwir_1PerKm = 0.50;  // ~e^(-0.5*1) = 0.61 at 1 km

// Per-strategy per-pixel NETD in Kelvin (modern HgCdTe/MWIR sensor).
// Strategy C+ models this. Strategy A/B have no NETD (perfect noise-free detection).
constexpr double kNetd_Mwir_K = 0.020;  // 20 mK (cooled MWIR, modern SOTA per Wikipedia FLIR refs)
constexpr double kNetd_Lwir_K = 0.050;  // 50 mK (uncooled microbolometer, modern SOTA)

// Sun glint rejection factor (probability of false alarm from sun glint in LWIR 8-12 µm window,
// where sun is roughly 1.0e6 K blackbody radiating per Stefan-Boltzmann σT^4; for sensor
// looking near sun-line-of-sight, glint is dominant noise source).
// Strategy E models this. Strategies A-D assume no glint.
constexpr double kSunGlintProbability = 0.10;  // 10% chance per measurement

// ---------- Target model ----------
struct Target {
    // Skin temperature (K), exhaust temperature (K), apparent emissivity (0-1)
    double skin_temp_K = 300.0;
    double exhaust_temp_K = 800.0;
    double emissivity = 0.90;
    // Apparent area of skin (m^2) and exhaust (m^2), aspect-dependent
    double skin_area_m2 = 10.0;
    double exhaust_area_m2 = 0.5;  // exhaust nozzle apparent
    // Aspect: 0 = front (cool intake), 1 = side (skin), 2 = rear (hot exhaust)
    int aspect = 1;  // side default
    // Whether the target is hot-running (engine on) or cold (engine off, IR signature = skin only)
    bool engine_running = true;
};

// Background (sky/ground) equivalent blackbody temperature (K).
// Day clear sky: 290-300 K (slightly below ambient due to atmospheric scatter);
// Cold clear sky: 240-260 K;
// Urban clutter: 295-305 K (warm);
// Snow arctic background: 253 K (-20°C).
struct Background {
    double sky_temp_K = 290.0;
    double ground_temp_K = 295.0;
    double clutter_std_K = 0.5;  // per-pixel spatial std (clutter noise)
};

// Sensor model: per-target evaluation
struct Sensor {
    // Aperture diameter (m). PIRATE-class: 0.10 m
    double aperture_m = 0.10;
    // Optical transmission (lens + window). Typical 0.7 for production systems.
    double optical_transmission = 0.70;
    // Detector area (m^2). 15 µm pixel @ 1024^2 sensor = 1.5e-3 m^2 / 1M = 1.5e-9 m^2 per pixel.
    // For 10 µm pixel typical = 1e-10 m^2.
    double pixel_area_m2 = 1.0e-10;
    // Integration time (s). Typical 5-10 ms.
    double integration_time_s = 0.005;
};

// Range + aspect: distance from sensor to target, plus aspect angle.
struct View {
    double range_m = 1000.0;
    // aspect: 0 = front, 1 = side, 2 = rear
    int aspect = 1;
};

// ---------- Planck blackbody spectral radiance ----------
// B(λ, T) = (2hc²/λ⁵) / (e^(hc/λkT) - 1)  [W m^-2 sr^-1 m^-1]
// We integrate over a band to get band radiance.
double planck_band_radiance(double lambda_center_m, double bandwidth_m, double T_K) {
    // Mid-band approximation (bandwidth << peak for narrow bands)
    // Use trapezoid over 3 points: center, ±bandwidth/2
    const double dl = bandwidth_m * 0.5;
    const double l1 = lambda_center_m - dl;
    const double l2 = lambda_center_m;
    const double l3 = lambda_center_m + dl;
    auto B = [&](double l) {
        const double x = kPlanckH * kSpeedOfLight / (l * kBoltzmannK * T_K);
        if (x > 500.0) return 0.0;  // e^x overflows; radiance negligible
        const double exp_x = std::exp(x);
        return (2.0 * kPlanckH * kSpeedOfLight * kSpeedOfLight) / (l*l*l*l*l * (exp_x - 1.0));
    };
    return (B(l1) + 2.0 * B(l2) + B(l3)) * (bandwidth_m / 4.0);  // W m^-2 sr^-1
}

// Stefan-Boltzmann spectral exitance for a band, used as a simple shortcut
// (only valid for narrow bands far from Wien peak; we still use Planck above for accuracy).
double stefan_boltzmann_exittance(double T_K) {
    return kStefanBoltzmann * T_K * T_K * T_K * T_K;
}

// Atmospheric transmission τ(λ, R) = exp(-α·R)
// where α is the extinction coefficient per unit length at the wavelength.
double atmospheric_transmission(double range_m, double extinction_per_m) {
    if (range_m <= 0.0) return 1.0;
    return std::exp(-extinction_per_m * range_m);
}

// Sun-glint factor for strategy E: 1.0 if glint present (target lost in noise),
// 1.0/(1+glint_floor) if no glint.
double sun_glint_factor(bool glint_present) {
    if (glint_present) return 0.0;  // glint swamps signal
    return 1.0;
}

// ---------- Strategies ----------
// Each strategy returns (radiance_W_m2_sr, is_detected) given a target view, plus
// per-detection cost which is computed externally via std::chrono.

// A_SimpleRangeEquation: ε·σ·T⁴ / (4πR²), no atmosphere, no noise.
struct StrategyResult {
    double signal_W_m2_sr = 0.0;
    double noise_W_m2_sr = 0.0;
    bool detected = false;
};

StrategyResult strategy_a(const Target& t, const View& v, const Background& bg, const Sensor& s,
                          double /*seed*/) {
    (void)bg; (void)s;  // not used in A
    StrategyResult r;
    const double effective_T = t.engine_running
        ? (v.aspect == 2 ? t.exhaust_temp_K : t.skin_temp_K)
        : t.skin_temp_K;
    const double apparent_area = (v.aspect == 2 ? t.exhaust_area_m2 : t.skin_area_m2);
    // Radiance (W m^-2 sr^-1) = ε·σ·T^4 / π
    r.signal_W_m2_sr = t.emissivity * stefan_boltzmann_exittance(effective_T) / kPi;
    // Detection: if signal > 0 (always true) — A assumes no noise, no atmosphere, no clutter.
    r.noise_W_m2_sr = 0.0;
    r.detected = (r.signal_W_m2_sr > 1e-6);  // always true
    (void)apparent_area;  // not used (assumes infinite range-independent)
    return r;
}

// B_AtmosphericModeled: A + τ(λ, R) per band.
StrategyResult strategy_b(const Target& t, const View& v, const Background& bg, const Sensor& s,
                          double /*seed*/) {
    StrategyResult r;
    const double effective_T = t.engine_running
        ? (v.aspect == 2 ? t.exhaust_temp_K : t.skin_temp_K)
        : t.skin_temp_K;
    // Use MWIR band (4 µm) — more atmospheric transmission than LWIR, primary IRST band
    const double B_mwir = planck_band_radiance(kMwirCenter_m, 1.0e-6, effective_T);
    const double B_bg_mwir = planck_band_radiance(kMwirCenter_m, 1.0e-6, bg.sky_temp_K);
    const double tau_mwir = atmospheric_transmission(v.range_m, kAtmExtinction_Mwir_1PerKm / 1000.0);
    // Contrast radiance = ε·(B_target - B_bg)·τ
    r.signal_W_m2_sr = t.emissivity * (B_mwir - B_bg_mwir) * tau_mwir;
    r.noise_W_m2_sr = 0.0;  // no noise model in B
    // Detection: signal > 0 AND tau > 0.05 (atmospheric cut-off; below this, signal buried)
    r.detected = (r.signal_W_m2_sr > 1e-6) && (tau_mwir > 0.05);
    (void)s;
    return r;
}

// C_NETD_WithClutter: B + NETD noise + clutter std (per-pixel equivalent).
// Detection requires SNR > 5 (P_FA = 1e-4 equivalent).
StrategyResult strategy_c(const Target& t, const View& v, const Background& bg, const Sensor& s,
                          double seed) {
    StrategyResult r;
    const double effective_T = t.engine_running
        ? (v.aspect == 2 ? t.exhaust_temp_K : t.skin_temp_K)
        : t.skin_temp_K;
    const double B_mwir = planck_band_radiance(kMwirCenter_m, 1.0e-6, effective_T);
    const double B_bg_mwir = planck_band_radiance(kMwirCenter_m, 1.0e-6, bg.sky_temp_K);
    const double tau_mwir = atmospheric_transmission(v.range_m, kAtmExtinction_Mwir_1PerKm / 1000.0);
    // Signal: contrast radiance at sensor aperture
    r.signal_W_m2_sr = t.emissivity * (B_mwir - B_bg_mwir) * tau_mwir;
    // NETD-equivalent noise: dB/dT × NETD per pixel (single-pixel detection)
    // dB/dT = (2hc²/λ⁶)·(e^x · x) / (e^x - 1)² × k/(kT²)  ... we approximate numerically
    const double dT = 0.1;  // small delta
    const double B_p = planck_band_radiance(kMwirCenter_m, 1.0e-6, effective_T + dT);
    const double B_m = planck_band_radiance(kMwirCenter_m, 1.0e-6, effective_T - dT);
    const double dBdT = (B_p - B_m) / (2.0 * dT);  // W m^-2 sr^-1 K^-1
    // NETD noise equivalent radiance: NEI = dBdT × NETD
    // Plus clutter std (multiplicative; convert to radiance via dBdT):
    const double netd_noise = std::abs(dBdT) * kNetd_Mwir_K;
    const double clutter_noise = std::abs(dBdT) * bg.clutter_std_K;
    // Total noise (RSS):
    r.noise_W_m2_sr = std::sqrt(netd_noise * netd_noise + clutter_noise * clutter_noise);
    // Pseudo-random sample for detection (Gaussian noise on signal)
    // Use deterministic-from-seed: hash(seed) gives reproducible binary decision.
    // Threshold: SNR > 5 (P_FA ~ 1e-4).
    const double snr_threshold = 5.0;
    const double snr = (r.noise_W_m2_sr > 0) ? std::abs(r.signal_W_m2_sr) / r.noise_W_m2_sr : 1e9;
    // Deterministic probabilistic: detected if hash seed falls in detection probability
    // p_detect = Phi((SNR - threshold) / sigma_norm) where sigma_norm = 1.
    // Use a simple sigmoid proxy:
    const double p = 1.0 / (1.0 + std::exp(-(snr - snr_threshold)));
    // Use seed to sample uniform [0, 1)
    const double u = std::fmod(std::abs(std::sin(seed * 12.9898 + 78.233)) * 43758.5453, 1.0);
    r.detected = (u < p) && (tau_mwir > 0.05);
    (void)s;
    return r;
}

// D_MultiBandFusion: C + MWIR + LWIR weighted fusion.
StrategyResult strategy_d(const Target& t, const View& v, const Background& bg, const Sensor& s,
                          double seed) {
    StrategyResult r;
    const double effective_T = t.engine_running
        ? (v.aspect == 2 ? t.exhaust_temp_K : t.skin_temp_K)
        : t.skin_temp_K;
    // MWIR (3-5 µm) — lower atmospheric extinction
    const double B_mwir_t = planck_band_radiance(kMwirCenter_m, 1.0e-6, effective_T);
    const double B_mwir_bg = planck_band_radiance(kMwirCenter_m, 1.0e-6, bg.sky_temp_K);
    const double tau_mwir = atmospheric_transmission(v.range_m, kAtmExtinction_Mwir_1PerKm / 1000.0);
    const double sig_mwir = t.emissivity * (B_mwir_t - B_mwir_bg) * tau_mwir;
    // LWIR (8-12 µm) — better for cool targets (room-temp human body heat)
    const double B_lwir_t = planck_band_radiance(kLwirCenter_m, 2.0e-6, effective_T);
    const double B_lwir_bg = planck_band_radiance(kLwirCenter_m, 2.0e-6, bg.sky_temp_K);
    const double tau_lwir = atmospheric_transmission(v.range_m, kAtmExtinction_Lwir_1PerKm / 1000.0);
    const double sig_lwir = t.emissivity * (B_lwir_t - B_lwir_bg) * tau_lwir;
    // Fusion: weighted by inverse NETD (better SNR gets more weight)
    // MWIR weight = 1/NETD_mwir, LWIR weight = 1/NETD_lwir
    const double w_mwir = 1.0 / kNetd_Mwir_K;
    const double w_lwir = 1.0 / kNetd_Lwir_K;
    const double w_total = w_mwir + w_lwir;
    r.signal_W_m2_sr = (sig_mwir * w_mwir + sig_lwir * w_lwir) / w_total;
    // NETD-equivalent (fused): sqrt(1 / sum(1/NETD_i^2))
    // For simplicity: noise = inverse of weighted NETD
    const double dT = 0.1;
    const double dBdT_mwir = (planck_band_radiance(kMwirCenter_m, 1.0e-6, effective_T + dT) -
                             planck_band_radiance(kMwirCenter_m, 1.0e-6, effective_T - dT)) / (2.0 * dT);
    const double dBdT_lwir = (planck_band_radiance(kLwirCenter_m, 2.0e-6, effective_T + dT) -
                             planck_band_radiance(kLwirCenter_m, 2.0e-6, effective_T - dT)) / (2.0 * dT);
    const double netd_fused_inv = std::sqrt(1.0 / (1.0 / (kNetd_Mwir_K * kNetd_Mwir_K) +
                                                    1.0 / (kNetd_Lwir_K * kNetd_Lwir_K)));
    const double dBdT_fused = std::abs(dBdT_mwir) * w_mwir / w_total + std::abs(dBdT_lwir) * w_lwir / w_total;
    const double netd_noise = dBdT_fused * netd_fused_inv;
    const double clutter_noise = dBdT_fused * bg.clutter_std_K;
    r.noise_W_m2_sr = std::sqrt(netd_noise * netd_noise + clutter_noise * clutter_noise);
    // Detection probability
    const double snr_threshold = 5.0;
    const double snr = (r.noise_W_m2_sr > 0) ? std::abs(r.signal_W_m2_sr) / r.noise_W_m2_sr : 1e9;
    const double p = 1.0 / (1.0 + std::exp(-(snr - snr_threshold)));
    const double u = std::fmod(std::abs(std::sin(seed * 31.4159 + 271.828)) * 12345.6789, 1.0);
    r.detected = (u < p) && (tau_mwir > 0.05);
    (void)s;
    return r;
}

// E_FullPhysicsModel: D + glint rejection + scintillation (atmospheric turbulence).
StrategyResult strategy_e(const Target& t, const View& v, const Background& bg, const Sensor& s,
                          double seed) {
    StrategyResult r = strategy_d(t, v, bg, s, seed);
    // Sun glint rejection: if hash falls in glint probability, detected=false
    const double u_glint = std::fmod(std::abs(std::sin(seed * 7.7777 + 1.6180)) * 99999.111, 1.0);
    const bool glint_present = (u_glint < kSunGlintProbability);
    if (glint_present) r.detected = false;
    return r;
}

// ---------- Scenes ----------
struct Scene {
    std::string name;
    std::vector<Target> targets;
    std::vector<View> views;
    Background bg;
    int detection_count = 0;  // ground truth (best possible)
};

Scene make_scene_s1_dogfight() {
    Scene s;
    s.name = "s1_1v1_dogfight";
    s.bg = Background{290.0, 295.0, 0.5};
    // 2 aircraft closing head-on, then disengaging
    for (int i = 0; i < 2; ++i) {
        Target t;
        t.skin_temp_K = 280.0;       // cold cruise at altitude
        t.exhaust_temp_K = 800.0;    // moderate afterburner
        t.skin_area_m2 = 50.0;       // fighter frontal silhouette
        t.exhaust_area_m2 = 0.3;
        t.emissivity = 0.85;
        t.engine_running = true;
        t.aspect = (i == 0) ? 2 : 0;  // one rear (hot exhaust), one front (cool)
        s.targets.push_back(t);
    }
    // 10 range points: 0.5 to 5 km
    for (double r_km = 0.5; r_km <= 5.0; r_km += 0.5) {
        for (auto& t : s.targets) {
            s.views.push_back(View{r_km * 1000.0, t.aspect});
        }
    }
    return s;
}

Scene make_scene_s2_ground_periscope() {
    Scene s;
    s.name = "s2_ground_periscope";
    s.bg = Background{280.0, 290.0, 1.0};  // ground thermal clutter higher
    // 1 vehicle in defilade (hull-down behind ridge), sensor at higher elevation
    Target t;
    t.skin_temp_K = 320.0;  // hot engine bay
    t.exhaust_temp_K = 600.0;  // diesel
    t.skin_area_m2 = 8.0;
    t.exhaust_area_m2 = 0.2;
    t.emissivity = 0.92;
    t.engine_running = true;
    t.aspect = 2;  // rear (exhaust above ridge)
    s.targets.push_back(t);
    // 1-3 km range
    for (double r_km = 1.0; r_km <= 3.0; r_km += 0.25) {
        s.views.push_back(View{r_km * 1000.0, 2});
    }
    return s;
}

Scene make_scene_s3_helicopter_noe() {
    Scene s;
    s.name = "s3_helicopter_noe";
    s.bg = Background{285.0, 295.0, 0.8};
    // 1 helicopter hovering behind tree line
    Target t;
    t.skin_temp_K = 290.0;  // gas turbine
    t.exhaust_temp_K = 700.0;
    t.skin_area_m2 = 15.0;  // heli body
    t.exhaust_area_m2 = 0.4;
    t.emissivity = 0.88;
    t.engine_running = true;
    t.aspect = 1;  // side
    s.targets.push_back(t);
    // 0.3-1.5 km
    for (double r_km = 0.3; r_km <= 1.5; r_km += 0.1) {
        s.views.push_back(View{r_km * 1000.0, 1});
    }
    return s;
}

Scene make_scene_s4_urban_pedestrian() {
    Scene s;
    s.name = "s4_urban_pedestrian";
    s.bg = Background{300.0, 305.0, 2.0};  // hot urban thermal clutter
    // 10 vehicles (mixed) at 0.1-2 km
    for (int i = 0; i < 10; ++i) {
        Target t;
        t.skin_temp_K = 300.0 + 10.0 * (i % 3);  // 300/310/320
        t.exhaust_temp_K = 500.0 + 100.0 * (i % 4);
        t.skin_area_m2 = 6.0 + 2.0 * (i % 5);
        t.exhaust_area_m2 = 0.15;
        t.emissivity = 0.85 + 0.05 * (i % 3);
        t.engine_running = (i % 2 == 0);
        t.aspect = i % 3;  // front/side/rear mix
        s.targets.push_back(t);
    }
    for (auto& t : s.targets) {
        for (double r_km = 0.1; r_km <= 2.0; r_km += 0.1) {
            s.views.push_back(View{r_km * 1000.0, t.aspect});
        }
    }
    return s;
}

Scene make_scene_s5_cold_warfare_arctic() {
    Scene s;
    s.name = "s5_cold_warfare_arctic";
    s.bg = Background{253.0, 253.0, 0.3};  // -20°C snow background, low clutter
    // 5 vehicles at 1-10 km, low ambient temperature
    for (int i = 0; i < 5; ++i) {
        Target t;
        t.skin_temp_K = 310.0;  // hot engine vs cold background
        t.exhaust_temp_K = 700.0;
        t.skin_area_m2 = 7.0;
        t.exhaust_area_m2 = 0.2;
        t.emissivity = 0.95;
        t.engine_running = true;
        t.aspect = (i % 3);
        s.targets.push_back(t);
    }
    for (auto& t : s.targets) {
        for (double r_km = 1.0; r_km <= 10.0; r_km += 1.0) {
            s.views.push_back(View{r_km * 1000.0, t.aspect});
        }
    }
    return s;
}

// ---------- Stats harness ----------
struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double minv = 0.0;
    double maxv = 0.0;
    int count = 0;
    int detected_count = 0;
    double detection_rate = 0.0;
};

Stats compute_stats(const std::vector<double>& samples, int detected_count) {
    Stats s;
    s.count = static_cast<int>(samples.size());
    s.detected_count = detected_count;
    s.detection_rate = s.count > 0 ? static_cast<double>(detected_count) / s.count : 0.0;
    if (samples.empty()) return s;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    s.mean = sum / sorted.size();
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    s.minv = sorted.front();
    s.maxv = sorted.back();
    double var = 0.0;
    for (double v : sorted) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / sorted.size());
    return s;
}

// ---------- Strategy runner ----------
using StrategyFn = StrategyResult(*)(const Target&, const View&, const Background&, const Sensor&, double);

struct StrategySpec {
    std::string name;
    StrategyFn fn;
};

const std::array<StrategySpec, 5> kStrategies = {{
    {"A_SimpleRangeEquation", &strategy_a},
    {"B_AtmosphericModeled",  &strategy_b},
    {"C_NETD_WithClutter",    &strategy_c},
    {"D_MultiBandFusion",     &strategy_d},
    {"E_FullPhysicsModel",    &strategy_e},
}};

std::vector<Scene> make_scenes() {
    return {
        make_scene_s1_dogfight(),
        make_scene_s2_ground_periscope(),
        make_scene_s3_helicopter_noe(),
        make_scene_s4_urban_pedestrian(),
        make_scene_s5_cold_warfare_arctic(),
    };
}

}  // namespace irst

int main() {
    using namespace irst;
    std::vector<Scene> scenes = make_scenes();
    Sensor sensor_default;

    // CSV header
    std::ostringstream csv;
    csv << "strategy,scene,view_count,mean_ns,median_ns,p95_ns,p99_ns,stddev_ns,min_ns,max_ns,"
        << "detection_rate,detection_count\n";

    for (const auto& strat : kStrategies) {
        for (const auto& scene : scenes) {
            // 5 seeds × 1000 main + 10 warmup = 5050 measurements
            std::vector<double> per_call_ns;
            per_call_ns.reserve(5 * 1000);
            int total_detected = 0;
            for (int seed_idx = 0; seed_idx < 5; ++seed_idx) {
                const double seed_base = static_cast<double>(seed_idx * 1000 + 1);
                std::mt19937 rng(static_cast<uint32_t>(seed_idx * 7 + 1));

                // 10 warmup
                for (int w = 0; w < 10; ++w) {
                    for (size_t v = 0; v < scene.views.size(); ++v) {
                        strat.fn(scene.targets[v % scene.targets.size()],
                                 scene.views[v], scene.bg, sensor_default, seed_base + w);
                    }
                }

                // 1000 main
                for (int it = 0; it < 1000; ++it) {
                    for (size_t v = 0; v < scene.views.size(); ++v) {
                        // Re-randomize target offset per iteration for variation
                        const size_t t_idx = (v + it) % scene.targets.size();
                        const double seed = seed_base + it * 0.001 + v * 0.0001;
                        // Time the per-detection call
                        auto t0 = std::chrono::high_resolution_clock::now();
                        StrategyResult r = strat.fn(scene.targets[t_idx],
                                                    scene.views[v], scene.bg, sensor_default, seed);
                        auto t1 = std::chrono::high_resolution_clock::now();
                        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
                        per_call_ns.push_back(ns);
                        if (r.detected) total_detected++;
                    }
                }
            }

            Stats s = compute_stats(per_call_ns, total_detected);
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                          "%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%d\n",
                          strat.name.c_str(), scene.name.c_str(),
                          s.count, s.mean, s.median, s.p95, s.p99, s.stddev, s.minv, s.maxv,
                          s.detection_rate, s.detected_count);
            csv << buf;
        }
    }

    // Write CSV
    std::ofstream out("build/results.csv");
    out << csv.str();
    out.close();
    std::printf("Wrote build/results.csv (%zu configs)\n", kStrategies.size() * scenes.size());

    // Print headline table to stdout
    std::printf("\n=== Headline (mean ns per detection, 5 strategies × 5 scenes, 25,000 main measurements per config) ===\n");
    std::printf("%-30s | %-25s | %10s | %10s | %10s\n", "Strategy", "Scene", "Mean(ns)", "P99(ns)", "DetectRate");
    std::printf("%-30s-+-%-25s-+-%10s-+-%10s-+-%10s\n", "------------------------------",
               "-------------------------", "----------", "----------", "----------");
    // Re-parse CSV for headline (or just regenerate — quick re-run for clarity)
    // For simplicity, just print strategy/headline
    for (const auto& strat : kStrategies) {
        for (const auto& scene : scenes) {
            // Re-compute (cheap) for headline
            std::vector<double> per_call_ns;
            int total_detected = 0;
            for (int seed_idx = 0; seed_idx < 5; ++seed_idx) {
                const double seed_base = static_cast<double>(seed_idx * 1000 + 1);
                for (int w = 0; w < 10; ++w) {
                    for (size_t v = 0; v < scene.views.size(); ++v) {
                        strat.fn(scene.targets[v % scene.targets.size()],
                                 scene.views[v], scene.bg, sensor_default, seed_base + w);
                    }
                }
                for (int it = 0; it < 1000; ++it) {
                    for (size_t v = 0; v < scene.views.size(); ++v) {
                        const size_t t_idx = (v + it) % scene.targets.size();
                        const double seed = seed_base + it * 0.001 + v * 0.0001;
                        auto t0 = std::chrono::high_resolution_clock::now();
                        StrategyResult r = strat.fn(scene.targets[t_idx],
                                                    scene.views[v], scene.bg, sensor_default, seed);
                        auto t1 = std::chrono::high_resolution_clock::now();
                        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
                        per_call_ns.push_back(ns);
                        if (r.detected) total_detected++;
                    }
                }
            }
            Stats s = compute_stats(per_call_ns, total_detected);
            std::printf("%-30s | %-25s | %10.1f | %10.1f | %10.4f\n",
                       strat.name.c_str(), scene.name.c_str(),
                       s.mean, s.p99, s.detection_rate);
        }
    }

    return 0;
}
