#include <cmath>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <numbers>
#include <cstdint>

constexpr double SAMPLE_RATE     = 48000.0;
constexpr double DURATION        = 1.0;
constexpr int    N_SAMPLES       = static_cast<int>(SAMPLE_RATE * DURATION);
constexpr int    WARMUP_ITER     = 10;
constexpr int    MEASURED_ITER   = 1000;
constexpr int    N_STRATEGIES    = 5;
constexpr int    N_TYPES         = 5;
constexpr int    N_DISTANCES     = 5;
constexpr int    N_SEEDS         = 5;
constexpr int    N_GRAINS_C      = 32;
constexpr int    N_GRAINS_REF    = 128;

enum class ExplosionType : int { HE = 0, Incendiary, Thermobaric, Nuclear, ArtilleryShell };
enum class Distance : int { Near = 0, Mid, Far, VeryFar, Extreme };
enum class Strategy  : int { NoAudio = 0, SingleShot, MultiLayer, PhysicallyModeled, AdaptiveHybrid };

static constexpr const char* STR_NAMES[] = {
    "A_NoAudio", "B_SingleShotSample", "C_MultiLayerSynthesis",
    "D_PhysicallyModeledPressureWave", "E_AdaptiveHybrid"
};
static constexpr const char* TYPE_NAMES[] = {
    "HE", "Incendiary", "Thermobaric", "Nuclear", "ArtilleryShell"
};
static constexpr const char* DIST_NAMES[] = {
    "near", "mid", "far", "veryfar", "extreme"
};

struct TypeBundle {
    double peak_pressure;
    double positive_duration;
    double decay_alpha;
    double freq_peak_hz;
    double freq_spread;
    double rumble_intensity;
    double debris_intensity;
    double second_shock_delay;
    double second_shock_ratio;
};

static constexpr TypeBundle TYPE_BUNDLES[N_TYPES] = {
    { 1.00, 0.008, 0.003, 8500.0, 4000.0, 0.4, 0.5, 0.0, 0.0 },
    { 0.60, 0.015, 0.005, 3500.0, 2000.0, 0.7, 0.3, 0.0, 0.0 },
    { 0.80, 0.060, 0.020, 150.0,  400.0,  0.9, 0.2, 0.0, 0.0 },
    { 8.00, 0.500, 0.100, 200.0,  8000.0, 0.8, 0.9, 0.15, 0.4 },
    { 0.90, 0.004, 0.002, 10000.0,3000.0, 0.2, 0.6, 0.0, 0.0 },
};

struct DistanceBundle {
    double attenuation;
    double lowpass_hz;
    double delay_seconds;
};

static constexpr DistanceBundle DIST_BUNDLES[N_DISTANCES] = {
    { 1.00, 20000.0, 0.005 },
    { 0.50, 12000.0, 0.050 },
    { 0.15,  6000.0, 0.200 },
    { 0.05,  3000.0, 0.500 },
    { 0.01,  1000.0, 1.000 },
};

struct Grain {
    float start_time;
    float duration;
    float amplitude;
    float frequency;
    int   env_type;
};

struct FriedlanderParams {
    double peak_pressure;
    double positive_duration;
    double decay_alpha;
    double delay;
    double second_shock_delay;
    double second_shock_ratio;
};

struct MultiLayerResult {
    std::vector<Grain> grains;
};

struct PhysModelResult {
    FriedlanderParams friedlander;
    double combustion_intensity;
    double combustion_duration;
    double combustion_lowpass;
};

struct HybridResult {
    std::vector<Grain> grains;
    FriedlanderParams transient;
    double shaping_lowpass_hz;
    double shaping_highpass_hz;
};

// ====================================================================
// Physics helpers
// ====================================================================

inline double friedlander(double t, const FriedlanderParams& p) {
    if (t < p.delay) return 0.0;
    double tp = t - p.delay;
    if (tp > p.positive_duration) return 0.0;
    double val = p.peak_pressure * std::exp(-tp / p.decay_alpha) * (1.0 - tp / p.positive_duration);
    if (p.second_shock_delay > 0.0) {
        double t2 = tp - p.second_shock_delay;
        if (t2 > 0.0 && t2 < p.positive_duration * 0.3)
            val += p.second_shock_ratio * p.peak_pressure
                 * std::exp(-t2 / (p.decay_alpha * 0.3))
                 * (1.0 - t2 / (p.positive_duration * 0.3));
    }
    return val;
}

inline double grain_env(double t_local, double dur, int type) {
    double nt = t_local / dur;
    if (nt < 0.0 || nt > 1.0) return 0.0;
    switch (type) {
        case 0: return std::exp(-4.0 * nt);
        case 1: return 1.0 - nt;
        case 2: return 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * (1.0 - nt)));
        default: return 1.0 - nt;
    }
}

// ====================================================================
// Strategy generators (timed)
// ====================================================================

uint64_t gen_A() { return 0; }

uint64_t gen_B(ExplosionType type, int seed) {
    return static_cast<uint64_t>(static_cast<int>(type) * 100 + seed);
}

MultiLayerResult gen_C(ExplosionType type, Distance dist, int seed) {
    const auto& tb = TYPE_BUNDLES[static_cast<int>(type)];
    std::mt19937_64 rng(static_cast<uint64_t>(seed) * 1000
                        + static_cast<int>(type) * 10
                        + static_cast<int>(dist));
    std::uniform_real_distribution<float> U;
    std::vector<Grain> grains; grains.reserve(N_GRAINS_C);
    for (int i = 0; i < 4; ++i) {
        Grain g;
        g.start_time = 0.0f + 0.05f * U(rng);
        g.duration   = 0.003f + 0.015f * U(rng);
        g.amplitude  = 0.8f + 0.2f * U(rng);
        g.frequency  = tb.freq_peak_hz + tb.freq_spread * (U(rng) - 0.5f);
        if (g.frequency < 20.0f) g.frequency = 20.0f;
        g.env_type = 0; grains.push_back(g);
    }
    for (int i = 0; i < 14; ++i) {
        Grain g;
        g.start_time = 0.05f + 0.6f * U(rng);
        g.duration   = 0.05f + 0.20f * U(rng);
        g.amplitude  = (0.2f + 0.4f * U(rng)) * tb.rumble_intensity;
        g.frequency  = 50.0f + 500.0f * U(rng);
        g.env_type = 1; grains.push_back(g);
    }
    for (int i = 0; i < 10; ++i) {
        Grain g;
        g.start_time = 0.1f + 0.4f * U(rng);
        g.duration   = 0.01f + 0.05f * U(rng);
        g.amplitude  = (0.1f + 0.3f * U(rng)) * tb.debris_intensity;
        g.frequency  = 2000.0f + 8000.0f * U(rng);
        g.env_type = 0; grains.push_back(g);
    }
    for (int i = 0; i < 4; ++i) {
        Grain g;
        g.start_time = 0.5f + 0.5f * U(rng);
        g.duration   = 0.2f + 0.3f * U(rng);
        g.amplitude  = 0.05f + 0.15f * U(rng);
        g.frequency  = 100.0f + 2000.0f * U(rng);
        g.env_type = 2; grains.push_back(g);
    }
    return {std::move(grains)};
}

PhysModelResult gen_D(ExplosionType type, Distance dist, int seed) {
    const auto& tb = TYPE_BUNDLES[static_cast<int>(type)];
    const auto& db = DIST_BUNDLES[static_cast<int>(dist)];
    std::mt19937_64 rng(static_cast<uint64_t>(seed) * 2000
                        + static_cast<int>(type) * 20
                        + static_cast<int>(dist));
    std::uniform_real_distribution<double> P(0.9, 1.1);
    FriedlanderParams fp;
    fp.peak_pressure       = tb.peak_pressure * db.attenuation * P(rng);
    fp.positive_duration   = tb.positive_duration * (1.0 + 0.5 * db.attenuation) * P(rng);
    fp.decay_alpha         = tb.decay_alpha * (1.0 + db.attenuation) * P(rng);
    fp.delay               = db.delay_seconds;
    fp.second_shock_delay  = tb.second_shock_delay;
    fp.second_shock_ratio  = tb.second_shock_ratio;
    return {fp, tb.rumble_intensity * db.attenuation,
            tb.positive_duration * 3.0, db.lowpass_hz};
}

HybridResult gen_E(ExplosionType type, Distance dist, int seed) {
    const auto& tb = TYPE_BUNDLES[static_cast<int>(type)];
    const auto& db = DIST_BUNDLES[static_cast<int>(dist)];
    auto base = gen_C(type, dist, seed + 100);
    std::mt19937_64 rng(static_cast<uint64_t>(seed) * 3000
                        + static_cast<int>(type) * 30
                        + static_cast<int>(dist));
    std::uniform_real_distribution<float> U;
    int extra = 0;
    switch (type) {
        case ExplosionType::HE:              extra = 16; break;
        case ExplosionType::Incendiary:      extra = 12; break;
        case ExplosionType::Thermobaric:     extra =  8; break;
        case ExplosionType::Nuclear:         extra = 20; break;
        case ExplosionType::ArtilleryShell:  extra = 10; break;
    }
    for (int i = 0; i < extra; ++i) {
        Grain g;
        g.start_time = 0.7f * U(rng);
        g.duration   = 0.01f + 0.15f * U(rng);
        g.amplitude  = 0.1f + 0.3f * U(rng);
        g.frequency  = tb.freq_peak_hz * (0.5f + U(rng));
        if (g.frequency < 20.0f) g.frequency = 20.0f;
        g.env_type   = static_cast<int>(g.start_time * 3.0f) % 3;
        base.grains.push_back(g);
    }
    FriedlanderParams fp;
    fp.peak_pressure      = tb.peak_pressure * db.attenuation * 0.3;
    fp.positive_duration  = tb.positive_duration * 0.5;
    fp.decay_alpha        = tb.decay_alpha * 0.3;
    fp.delay              = db.delay_seconds * 0.5;
    fp.second_shock_delay = 0.0;
    fp.second_shock_ratio = 0.0;
    double lp = 0, hp = 0;
    switch (type) {
        case ExplosionType::HE:              lp = 12000.0; hp =  80.0; break;
        case ExplosionType::Incendiary:      lp =  5000.0; hp = 150.0; break;
        case ExplosionType::Thermobaric:     lp =   400.0; hp =  20.0; break;
        case ExplosionType::Nuclear:         lp = 18000.0; hp =  20.0; break;
        case ExplosionType::ArtilleryShell:  lp = 14000.0; hp = 200.0; break;
    }
    return {std::move(base.grains), fp, lp, hp};
}

// ====================================================================
// Audio renderers (for quality metrics)
// ====================================================================

void render_ref(ExplosionType type, Distance dist, int seed, float* buf) {
    const auto& tb = TYPE_BUNDLES[static_cast<int>(type)];
    const auto& db = DIST_BUNDLES[static_cast<int>(dist)];
    std::mt19937_64 rng(static_cast<uint64_t>(seed) * 4000
                        + static_cast<int>(type) * 40
                        + static_cast<int>(dist));
    std::uniform_real_distribution<float> U;
    std::vector<Grain> grains; grains.reserve(N_GRAINS_REF);
    for (int i = 0; i < N_GRAINS_REF; ++i) {
        float layer = U(rng);
        Grain g;
        if (layer < 0.15f) {
            g.start_time = 0.05f * U(rng);
            g.duration   = 0.003f + 0.02f * U(rng);
            g.amplitude  = 0.6f + 0.4f * U(rng);
            g.frequency  = tb.freq_peak_hz + tb.freq_spread * (U(rng) - 0.5f);
            if (g.frequency < 20.0f) g.frequency = 20.0f;
            g.env_type = 0;
        } else if (layer < 0.55f) {
            g.start_time = 0.05f + 0.6f * U(rng);
            g.duration   = 0.05f + 0.25f * U(rng);
            g.amplitude  = (0.2f + 0.5f * U(rng)) * tb.rumble_intensity;
            g.frequency  = 40.0f + 600.0f * U(rng);
            g.env_type = 1;
        } else if (layer < 0.85f) {
            g.start_time = 0.08f + 0.5f * U(rng);
            g.duration   = 0.005f + 0.06f * U(rng);
            g.amplitude  = (0.1f + 0.35f * U(rng)) * tb.debris_intensity;
            g.frequency  = 1500.0f + 9000.0f * U(rng);
            g.env_type = 0;
        } else {
            g.start_time = 0.4f + 0.6f * U(rng);
            g.duration   = 0.15f + 0.35f * U(rng);
            g.amplitude  = 0.05f + 0.15f * U(rng);
            g.frequency  = 80.0f + 3000.0f * U(rng);
            g.env_type = 2;
        }
        grains.push_back(g);
    }
    std::mt19937_64 rrng(static_cast<uint64_t>(seed) * 7777);
    std::uniform_real_distribution<float> ND(-1.0f, 1.0f);
    for (int gi = 0; gi < N_GRAINS_REF; ++gi) {
        const auto& g = grains[gi];
        int sidx = static_cast<int>(g.start_time * N_SAMPLES);
        int durs = static_cast<int>(g.duration * SAMPLE_RATE);
        for (int j = 0; j < durs; ++j) {
            int idx = sidx + j;
            if (idx >= N_SAMPLES) break;
            double env = grain_env(static_cast<double>(j) / SAMPLE_RATE, g.duration, g.env_type);
            float n = ND(rrng);
            double tone = std::sin(2.0 * std::numbers::pi * g.frequency * j / SAMPLE_RATE);
            buf[idx] += static_cast<float>(db.attenuation * g.amplitude * env * (0.7 * n + 0.3 * tone));
        }
    }
}

void render_B(ExplosionType type, Distance dist, int seed, float* buf) {
    const auto& tb = TYPE_BUNDLES[static_cast<int>(type)];
    const auto& db = DIST_BUNDLES[static_cast<int>(dist)];
    std::mt19937_64 rng(static_cast<uint64_t>(seed) * 5000
                        + static_cast<int>(type) * 50
                        + static_cast<int>(dist));
    std::uniform_real_distribution<float> ND(-1.0f, 1.0f);
    for (int i = 0; i < N_SAMPLES; ++i) {
        double t = static_cast<double>(i) / SAMPLE_RATE;
        double env = std::exp(-5.0 * t);
        double tone = std::sin(2.0 * std::numbers::pi * tb.freq_peak_hz * t);
        buf[i] += static_cast<float>(db.attenuation * env * (0.5 * tone + 0.5 * ND(rng)));
    }
}

void render_C(const MultiLayerResult& r, ExplosionType type, Distance dist, int seed, float* buf) {
    const auto& db = DIST_BUNDLES[static_cast<int>(dist)];
    std::mt19937_64 rng(static_cast<uint64_t>(seed) * 6000
                        + static_cast<int>(type) * 60
                        + static_cast<int>(dist));
    std::uniform_real_distribution<float> ND(-1.0f, 1.0f);
    double gain = db.attenuation * 0.8;
    for (size_t gi = 0; gi < r.grains.size(); ++gi) {
        const auto& g = r.grains[gi];
        int sidx = static_cast<int>(g.start_time * N_SAMPLES);
        int durs = static_cast<int>(g.duration * SAMPLE_RATE);
        for (int j = 0; j < durs; ++j) {
            int idx = sidx + j;
            if (idx >= N_SAMPLES) break;
            double env = grain_env(static_cast<double>(j) / SAMPLE_RATE, g.duration, g.env_type);
            float n = ND(rng);
            double tone = std::sin(2.0 * std::numbers::pi * g.frequency * j / SAMPLE_RATE);
            buf[idx] += static_cast<float>(gain * g.amplitude * env * (0.7 * n + 0.3 * tone));
        }
    }
}

void render_D(const PhysModelResult& r, ExplosionType type, Distance dist, int seed, float* buf) {
    (void)type; (void)seed;
    const auto& fp = r.friedlander;
    uint64_t d_seed = 7000 + static_cast<int>(type) * 70 + static_cast<int>(dist);
    std::mt19937_64 rng(d_seed);
    std::uniform_real_distribution<float> ND(-1.0f, 1.0f);
    for (int i = 0; i < N_SAMPLES; ++i) {
        double t = static_cast<double>(i) / SAMPLE_RATE;
        buf[i] += static_cast<float>(friedlander(t, fp));
    }
    int delay_s   = static_cast<int>(fp.delay * SAMPLE_RATE);
    int dur_s     = static_cast<int>(r.combustion_duration * SAMPLE_RATE);
    double dt     = 1.0 / SAMPLE_RATE;
    double rc     = 1.0 / (2.0 * std::numbers::pi * r.combustion_lowpass);
    double alpha  = dt / (rc + dt);
    double prev   = 0.0;
    for (int i = 0; i < dur_s; ++i) {
        int idx = delay_s + i;
        if (idx >= N_SAMPLES) break;
        double nt   = static_cast<double>(i) / dur_s;
        double env  = std::exp(-3.0 * nt) * (1.0 - std::exp(-10.0 * (1.0 - nt)));
        double w    = ND(rng);
        prev       += alpha * (w - prev);
        buf[idx]   += static_cast<float>(r.combustion_intensity * env * prev);
    }
}

void render_E(const HybridResult& r, ExplosionType type, Distance dist, int seed, float* buf) {
    const auto& db = DIST_BUNDLES[static_cast<int>(dist)];
    uint64_t r_seed = 8000 + static_cast<int>(type) * 80 + static_cast<int>(dist);
    std::mt19937_64 rng(r_seed);
    std::uniform_real_distribution<float> ND(-1.0f, 1.0f);
    double gain = db.attenuation * 0.7;
    for (size_t gi = 0; gi < r.grains.size(); ++gi) {
        const auto& g = r.grains[gi];
        int sidx = static_cast<int>(g.start_time * N_SAMPLES);
        int durs = static_cast<int>(g.duration * SAMPLE_RATE);
        for (int j = 0; j < durs; ++j) {
            int idx = sidx + j;
            if (idx >= N_SAMPLES) break;
            double env = grain_env(static_cast<double>(j) / SAMPLE_RATE, g.duration, g.env_type);
            float n = ND(rng);
            double tone = std::sin(2.0 * std::numbers::pi * g.frequency * j / SAMPLE_RATE);
            buf[idx] += static_cast<float>(gain * g.amplitude * env * (0.7 * n + 0.3 * tone));
        }
    }
    for (int i = 0; i < N_SAMPLES; ++i) {
        double t = static_cast<double>(i) / SAMPLE_RATE;
        buf[i] += static_cast<float>(0.3 * friedlander(t, r.transient));
    }
    double dt    = 1.0 / SAMPLE_RATE;
    double rc_lp = 1.0 / (2.0 * std::numbers::pi * r.shaping_lowpass_hz);
    double al    = dt / (rc_lp + dt);
    double rc_hp = 1.0 / (2.0 * std::numbers::pi * r.shaping_highpass_hz);
    double ah    = rc_hp / (rc_hp + dt);
    double lp = 0.0, hp = 0.0;
    for (int i = 0; i < N_SAMPLES; ++i) {
        double x = static_cast<double>(buf[i]);
        lp += al * (x - lp);
        double h = x - lp;
        hp += ah * (h - hp);
        buf[i] = static_cast<float>(hp * 1.5);
    }
}

// ====================================================================
// Quality metrics
// ====================================================================

double psnr(const float* ref, const float* sig, int n) {
    double mse = 0.0, ref_energy = 0.0;
    for (int i = 0; i < n; ++i) {
        double d = static_cast<double>(ref[i]) - static_cast<double>(sig[i]);
        mse += d * d;
        ref_energy += static_cast<double>(ref[i]) * static_cast<double>(ref[i]);
    }
    mse /= n;
    if (mse < 1e-30) return 100.0;
    double rms = std::sqrt(ref_energy / n);
    if (rms < 1e-30) return 0.0;
    return 20.0 * std::log10(rms / std::sqrt(mse));
}

double centroid_analytic(Strategy strat, ExplosionType type, const void* params) {
    const auto& tb = TYPE_BUNDLES[static_cast<int>(type)];
    switch (strat) {
        case Strategy::NoAudio:
            return 0.0;
        case Strategy::SingleShot:
            return tb.freq_peak_hz;
        case Strategy::MultiLayer: {
            const auto* p = static_cast<const MultiLayerResult*>(params);
            double saf = 0, sa = 0;
            for (const auto& g : p->grains) { saf += g.amplitude * g.frequency; sa += g.amplitude; }
            return (sa > 0) ? saf / sa : 0.0;
        }
        case Strategy::PhysicallyModeled: {
            const auto* p = static_cast<const PhysModelResult*>(params);
            double fc = 1.0 / (2.0 * std::numbers::pi * p->friedlander.decay_alpha);
            return std::min(fc, tb.freq_peak_hz);
        }
        case Strategy::AdaptiveHybrid: {
            const auto* p = static_cast<const HybridResult*>(params);
            double saf = 0, sa = 0;
            for (const auto& g : p->grains) { saf += g.amplitude * g.frequency; sa += g.amplitude; }
            double c = (sa > 0) ? saf / sa : 0.0;
            if (c > p->shaping_lowpass_hz * 0.8) c = p->shaping_lowpass_hz * 0.8;
            if (c < p->shaping_highpass_hz * 1.2) c = p->shaping_highpass_hz * 1.2;
            return c;
        }
    }
    return 0.0;
}

// ====================================================================
// Benchmark harness
// ====================================================================

struct ResultRow {
    const char* strat_name;
    const char* type_name;
    const char* dist_name;
    int seed;
    double mean_us, median_us, p95_us, std_us;
    double psnr_db;
    double centroid_hz;
};

int main() {
    std::cout << "=== Explosion Acoustic Variety Benchmark ===\n"
              << "Configs: " << (N_STRATEGIES * N_TYPES * N_DISTANCES * N_SEEDS) << "\n"
              << "Iter/config: " << MEASURED_ITER << " + " << WARMUP_ITER << " warmup\n"
              << "Samples/config (quality): " << N_SAMPLES << "\n"
              << "Strategies: " << STR_NAMES[0] << ", " << STR_NAMES[1] << ", " << STR_NAMES[2]
              << ", " << STR_NAMES[3] << ", " << STR_NAMES[4] << "\n\n";

    std::vector<ResultRow> results;
    results.reserve(N_STRATEGIES * N_TYPES * N_DISTANCES * N_SEEDS);

    for (int t = 0; t < N_TYPES; ++t) {
        ExplosionType type = static_cast<ExplosionType>(t);
        for (int d = 0; d < N_DISTANCES; ++d) {
            Distance dist = static_cast<Distance>(d);
            for (int seed = 0; seed < N_SEEDS; ++seed) {
                std::vector<float> ref(N_SAMPLES, 0.0f);
                render_ref(type, dist, seed, ref.data());

                for (int s = 0; s < N_STRATEGIES; ++s) {
                    Strategy strat = static_cast<Strategy>(s);
                    volatile uint64_t sink = 0;
                    double times[MEASURED_ITER];

                    for (int w = 0; w < WARMUP_ITER; ++w) {
                        switch (strat) {
                            case Strategy::NoAudio:            sink += gen_A(); break;
                            case Strategy::SingleShot:         sink += gen_B(type, seed); break;
                            case Strategy::MultiLayer:         { auto r = gen_C(type, dist, seed); sink += r.grains.size(); } break;
                            case Strategy::PhysicallyModeled:  { auto r = gen_D(type, dist, seed); sink += static_cast<uint64_t>(r.friedlander.peak_pressure * 1e3); } break;
                            case Strategy::AdaptiveHybrid:     { auto r = gen_E(type, dist, seed); sink += r.grains.size(); } break;
                        }
                    }

                    for (int i = 0; i < MEASURED_ITER; ++i) {
                        auto t0 = std::chrono::high_resolution_clock::now();
                        switch (strat) {
                            case Strategy::NoAudio:            sink += gen_A(); break;
                            case Strategy::SingleShot:         sink += gen_B(type, seed); break;
                            case Strategy::MultiLayer:         { auto r = gen_C(type, dist, seed); sink += r.grains.size(); } break;
                            case Strategy::PhysicallyModeled:  { auto r = gen_D(type, dist, seed); sink += static_cast<uint64_t>(r.friedlander.peak_pressure * 1e3); } break;
                            case Strategy::AdaptiveHybrid:     { auto r = gen_E(type, dist, seed); sink += r.grains.size(); } break;
                        }
                        auto t1 = std::chrono::high_resolution_clock::now();
                        times[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
                    }

                    std::sort(times, times + MEASURED_ITER);
                    double sum   = std::accumulate(times, times + MEASURED_ITER, 0.0);
                    double mean  = sum / MEASURED_ITER;
                    double med   = times[MEASURED_ITER / 2];
                    double p95   = times[static_cast<int>(MEASURED_ITER * 0.95)];
                    double var   = 0.0;
                    for (int i = 0; i < MEASURED_ITER; ++i) { double d = times[i] - mean; var += d * d; }
                    double stdd  = std::sqrt(var / MEASURED_ITER);

                    std::vector<float> sig(N_SAMPLES, 0.0f);
                    double psnr_val = 0.0, cent_val = 0.0;
                    if (strat != Strategy::NoAudio) {
                        switch (strat) {
                            case Strategy::SingleShot:
                                render_B(type, dist, seed, sig.data());
                                psnr_val = psnr(ref.data(), sig.data(), N_SAMPLES);
                                cent_val = centroid_analytic(strat, type, nullptr);
                                break;
                            case Strategy::MultiLayer: {
                                auto p = gen_C(type, dist, seed);
                                render_C(p, type, dist, seed, sig.data());
                                psnr_val = psnr(ref.data(), sig.data(), N_SAMPLES);
                                cent_val = centroid_analytic(strat, type, &p);
                                break;
                            }
                            case Strategy::PhysicallyModeled: {
                                auto p = gen_D(type, dist, seed);
                                render_D(p, type, dist, seed, sig.data());
                                psnr_val = psnr(ref.data(), sig.data(), N_SAMPLES);
                                cent_val = centroid_analytic(strat, type, &p);
                                break;
                            }
                            case Strategy::AdaptiveHybrid: {
                                auto p = gen_E(type, dist, seed);
                                render_E(p, type, dist, seed, sig.data());
                                psnr_val = psnr(ref.data(), sig.data(), N_SAMPLES);
                                cent_val = centroid_analytic(strat, type, &p);
                                break;
                            }
                            default: break;
                        }
                    }

                    results.push_back({STR_NAMES[s], TYPE_NAMES[t], DIST_NAMES[d],
                                       seed, mean, med, p95, stdd, psnr_val, cent_val});
                }
            }
            std::cout << "  " << TYPE_NAMES[t] << " / " << DIST_NAMES[d] << " done\n";
        }
    }

    std::cout << "\n=== Writing results.csv ===\n";
    std::ofstream csv("results.csv");
    csv << "strategy,type,distance,seed,mean_us,median_us,p95_us,std_us,psnr_db,spectral_centroid_hz\n";
    for (const auto& r : results)
        csv << r.strat_name << "," << r.type_name << "," << r.dist_name << ","
            << r.seed << "," << r.mean_us << "," << r.median_us << "," << r.p95_us << ","
            << r.std_us << "," << r.psnr_db << "," << r.centroid_hz << "\n";

    std::cout << "=== Writing summary_means.csv ===\n";
    std::ofstream sm("summary_means.csv");
    sm << "strategy,type,distance,mean_us,psnr_db,spectral_centroid_hz\n";
    for (int s = 0; s < N_STRATEGIES; ++s) {
        for (int t = 0; t < N_TYPES; ++t) {
            for (int d = 0; d < N_DISTANCES; ++d) {
                double su = 0, sp = 0, sc = 0; int cnt = 0;
                for (const auto& r : results) {
                    if (r.strat_name == STR_NAMES[s] && r.type_name == TYPE_NAMES[t]
                        && r.dist_name == DIST_NAMES[d]) {
                        su += r.mean_us; sp += r.psnr_db; sc += r.centroid_hz; cnt++;
                    }
                }
                if (cnt > 0)
                    sm << STR_NAMES[s] << "," << TYPE_NAMES[t] << "," << DIST_NAMES[d] << ","
                       << (su/cnt) << "," << (sp/cnt) << "," << (sc/cnt) << "\n";
            }
        }
    }

    std::cout << "Done.\n";
    return 0;
}
