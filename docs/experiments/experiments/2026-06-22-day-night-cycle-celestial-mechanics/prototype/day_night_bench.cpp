// day_night_bench.cpp — Day/Night cycle celestial mechanics benchmark
// 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 measurements
// clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic day_night_bench.cpp -o build/day_night_bench

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <numbers>
#include <random>
#include <span>
#include <vector>

// --- Constants ---
constexpr double TAU = 2.0 * std::numbers::pi;
constexpr double DAY_PERIOD = 24000.0;       // Minecraft-style: 24000 ticks per day
constexpr double SUN_ORBITAL_PERIOD = DAY_PERIOD;
constexpr double MOON_ORBITAL_PERIOD = DAY_PERIOD * 8.0; // 8× slower = moon phase cycle

// Keplerian elements for our voxel world
constexpr double AXIAL_TILT = 23.44 * std::numbers::pi / 180.0; // Earth-like

// Star count for D strategy
constexpr int STAR_COUNT = 4000;

// Scenes
enum class Scene : int {
    UNIFORM_FLOOR = 0,
    FOREST_FLOOR,
    CAVE_STRESS,
    MIXED_BIOME,
    OPEN_OCEAN,
    COUNT
};
constexpr int SCENE_COUNT = static_cast<int>(Scene::COUNT);

const char* scene_name(Scene s) {
    switch (s) {
        case Scene::UNIFORM_FLOOR: return "uniform_floor";
        case Scene::FOREST_FLOOR:  return "forest_floor";
        case Scene::CAVE_STRESS:   return "cave_stress";
        case Scene::MIXED_BIOME:   return "mixed_biome";
        case Scene::OPEN_OCEAN:    return "open_ocean";
        default: return "unknown";
    }
}

// --- Per-scene parameters ---
struct SceneParams {
    double terrain_height;    // 0-1 normalized (0=void, 1=sky)
    double canopy_factor;     // 0=open sky, 1=full canopy (ambient attenuation)
    double horizon_visible;   // 0=no horizon (cave), 1=full horizon (ocean)
    double turbidity;         // atmospheric haze (1=clear, 10=very hazy)
};

constexpr std::array<SceneParams, SCENE_COUNT> SCENE_PARAMS = {{
    {/*UNIFORM_FLOOR*/  .terrain_height = 0.3, .canopy_factor = 0.0,  .horizon_visible = 0.8, .turbidity = 1.0},
    {/*FOREST_FLOOR*/   .terrain_height = 0.3, .canopy_factor = 0.7,  .horizon_visible = 0.3, .turbidity = 1.5},
    {/*CAVE_STRESS*/    .terrain_height = 0.0, .canopy_factor = 1.0,  .horizon_visible = 0.0, .turbidity = 3.0},
    {/*MIXED_BIOME*/    .terrain_height = 0.5, .canopy_factor = 0.3,  .horizon_visible = 0.6, .turbidity = 2.0},
    {/*OPEN_OCEAN*/     .terrain_height = 0.0, .canopy_factor = 0.0,  .horizon_visible = 1.0, .turbidity = 1.2},
}};

// --- Output state (what strategies produce) ---
struct CelestialState {
    double sun_zenith;        // radians, 0 = overhead
    double sun_azimuth;       // radians
    double moon_zenith;
    double moon_azimuth;
    double moon_phase;        // 0..1 (0=new, 0.5=full)
    double ambient_intensity; // 0..1
    double ambient_r;         // twilight color
    double ambient_g;
    double ambient_b;
    double star_brightness;   // 0..1
    int star_count_visible;   // for D strategy
};

// ============================================================
// Strategy A: NoCycle (fixed ambient, no computation)
// ============================================================
struct NoCycle {
    static constexpr const char* name = "A_NoCycle";

    CelestialState update([[maybe_unused]] double tick, [[maybe_unused]] const SceneParams& sp) {
        return CelestialState{
            .sun_zenith = 0.0,
            .sun_azimuth = 0.0,
            .moon_zenith = std::numbers::pi,
            .moon_azimuth = 0.0,
            .moon_phase = 0.5,
            .ambient_intensity = 1.0,
            .ambient_r = 1.0f, .ambient_g = 1.0f, .ambient_b = 1.0f,
            .star_brightness = 0.0,
            .star_count_visible = 0
        };
    }
};

// ============================================================
// Strategy B: SimpleSunAngle (Minecraft-style)
// ============================================================
struct SimpleSunAngle {
    static constexpr const char* name = "B_SimpleSunAngle";

    CelestialState update(double tick, [[maybe_unused]] const SceneParams& sp) {
        double celestial_angle = std::fmod(tick / DAY_PERIOD, 1.0); // 0..1
        double cos_angle = std::cos(celestial_angle * TAU);
        // Sun zenith from angle (0.25 = noon = overhead)
        double sun_angle_rad = (celestial_angle - 0.25) * TAU;

        // Ambient: Minecraft formula: cos(angle * 2PI) * 2 + 0.5, clamped
        double raw = cos_angle * 2.0 + 0.5;
        double ambient = std::clamp(raw, 0.0, 1.0);
        ambient = 1.0 - ambient;
        ambient = ambient * 0.8 + 0.2;

        // Moon is opposite sun
        double moon_angle_rad = sun_angle_rad + std::numbers::pi;
        double moon_phase = 0.5; // simplified: no phase tracking

        return CelestialState{
            .sun_zenith = std::abs(std::acos(std::clamp(std::sin(sun_angle_rad), -1.0, 1.0))),
            .sun_azimuth = 0.0,
            .moon_zenith = std::abs(std::acos(std::clamp(std::sin(moon_angle_rad), -1.0, 1.0))),
            .moon_azimuth = 0.0,
            .moon_phase = moon_phase,
            .ambient_intensity = ambient,
            .ambient_r = 1.0f, .ambient_g = 1.0f, .ambient_b = 1.0f,
            .star_brightness = 1.0f - ambient,
            .star_count_visible = 0
        };
    }
};

// ============================================================
// Strategy C: FullCelestial (Keplerian sun + moon orbits)
// ============================================================
struct FullCelestial {
    static constexpr const char* name = "C_FullCelestial";

    // Keplerian orbital elements for sun (Earth-centric simplified)
    struct Orbit {
        double semi_major;      // a
        double eccentricity;    // e
        double inclination;     // i (rad)
        double raan;            // Ω — right ascension of ascending node
        double arg_periapsis;   // ω — argument of periapsis
        double mean_anomaly;    // M at epoch
        double period_ticks;    // orbital period in game ticks
    };

    // Solve Kepler's equation M = E - e*sin(E) for eccentric anomaly E
    static double solve_kepler(double M, double e, int iter = 8) {
        double E = M;
        for (int i = 0; i < iter; ++i)
            E = M + e * std::sin(E); // fixed-point iteration
        return E;
    }

    // Get position in orbital plane from mean anomaly
    static void orbital_pos(double M, double e, double a, double& x, double& y) {
        double E = solve_kepler(M, e);
        x = a * (std::cos(E) - e);
        y = a * std::sqrt(1.0 - e * e) * std::sin(E);
    }

    // Convert orbital position to geocentric direction (unit vector)
    static void orbit_to_dir(double x, double y, const Orbit& orb,
                             double& dx, double& dy, double& dz) {
        // Rotate by argument of periapsis (in orbital plane)
        double cos_w = std::cos(orb.arg_periapsis);
        double sin_w = std::sin(orb.arg_periapsis);
        double xp = x * cos_w - y * sin_w;
        double yp = x * sin_w + y * cos_w;

        // Rotate by inclination
        double cos_i = std::cos(orb.inclination);
        double sin_i = std::sin(orb.inclination);

        // Rotate by RAAN
        double cos_raan = std::cos(orb.raan);
        double sin_raan = std::sin(orb.raan);

        // Stacked rotation: RAAN(z) * inclination(x) * arg_periapsis(z)
        double x_rot = xp * cos_raan - (yp * cos_i) * sin_raan;
        double y_rot = xp * sin_raan + (yp * cos_i) * cos_raan;
        double z_rot = yp * sin_i;

        double len = std::sqrt(x_rot * x_rot + y_rot * y_rot + z_rot * z_rot);
        dx = x_rot / len;
        dy = y_rot / len;
        dz = z_rot / len;
    }

    CelestialState update(double tick, [[maybe_unused]] const SceneParams& sp) {
        // Sun orbit (Earth-centric apparent orbit)
        Orbit sun_orbit = {
            .semi_major = 1.0,
            .eccentricity = 0.0167,  // Earth eccentricity
            .inclination = AXIAL_TILT,
            .raan = 0.0,
            .arg_periapsis = std::numbers::pi / 2.0,
            .mean_anomaly = std::fmod(tick / SUN_ORBITAL_PERIOD, 1.0) * TAU,
            .period_ticks = SUN_ORBITAL_PERIOD
        };

        // Moon orbit (simplified: circular, inclined 5° to ecliptic)
        Orbit moon_orbit = {
            .semi_major = 1.0,
            .eccentricity = 0.0549,  // Moon eccentricity
            .inclination = 5.0 * std::numbers::pi / 180.0,
            .raan = 0.0,
            .arg_periapsis = 0.0,
            .mean_anomaly = std::fmod(tick / MOON_ORBITAL_PERIOD, 1.0) * TAU,
            .period_ticks = MOON_ORBITAL_PERIOD
        };

        double sx, sy;
        orbital_pos(sun_orbit.mean_anomaly, sun_orbit.eccentricity, sun_orbit.semi_major, sx, sy);
        double sun_dx, sun_dy, sun_dz;
        orbit_to_dir(sx, sy, sun_orbit, sun_dx, sun_dy, sun_dz);

        double mx, my;
        orbital_pos(moon_orbit.mean_anomaly, moon_orbit.eccentricity, moon_orbit.semi_major, mx, my);
        double moon_dx, moon_dy, moon_dz;
        orbit_to_dir(mx, my, moon_orbit, moon_dx, moon_dy, moon_dz);

        // Zenit/azimuth from direction
        double sun_zenith = std::acos(std::clamp(sun_dy, -1.0, 1.0));
        double sun_azimuth = std::atan2(sun_dx, sun_dz);
        double moon_zenith = std::acos(std::clamp(moon_dy, -1.0, 1.0));
        double moon_azimuth = std::atan2(moon_dx, moon_dz);

        // Ambient from sun elevation
        double sun_elev = std::sin(sun_zenith - std::numbers::pi / 2.0);
        double ambient = std::clamp(sun_elev * 0.8 + 0.5, 0.05, 1.0);

        // Moon phase: angle between sun and moon vectors
        double dot = sun_dx * moon_dx + sun_dy * moon_dy + sun_dz * moon_dz;
        double moon_phase = (1.0 - std::clamp(dot, -1.0, 1.0)) * 0.5;

        return CelestialState{
            .sun_zenith = sun_zenith,
            .sun_azimuth = sun_azimuth,
            .moon_zenith = moon_zenith,
            .moon_azimuth = moon_azimuth,
            .moon_phase = moon_phase,
            .ambient_intensity = ambient,
            .ambient_r = 1.0, .ambient_g = 1.0, .ambient_b = 1.0,
            .star_brightness = 1.0 - ambient,
            .star_count_visible = 0
        };
    }
};

// ============================================================
// Strategy D: CelestialPlusStars (C + GPU-friendly star field)
// ============================================================
struct CelestialPlusStars {
    static constexpr const char* name = "D_CelestialPlusStars";

    FullCelestial celestial;
    std::array<float, STAR_COUNT * 4> stars; // x, y, z, magnitude (pre-generated)
    bool stars_initialized = false;

    void init_stars(unsigned seed) {
        if (stars_initialized) return;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist_angle(0.0f, std::numbers::pi_v<float>);
        std::uniform_real_distribution<float> dist_azimuth(0.0f, TAU);
        std::uniform_real_distribution<float> dist_mag(0.0f, 1.0f);

        // Distribute stars on celestial sphere following magnitude distribution
        for (int i = 0; i < STAR_COUNT; ++i) {
            float theta = dist_angle(rng);       // 0..π
            float phi = dist_azimuth(rng);       // 0..2π
            float mag = dist_mag(rng);           // 0..1 (brightness factor)

            // Cartesian on unit sphere (celestial coords)
            float x = std::sin(theta) * std::cos(phi);
            float y = std::cos(theta);
            float z = std::sin(theta) * std::sin(phi);

            stars[i * 4 + 0] = x;
            stars[i * 4 + 1] = y;
            stars[i * 4 + 2] = z;
            // Magnitude: dim stars more common (power law)
            stars[i * 4 + 3] = std::pow(mag, 0.6f) * 0.5f + 0.5f;
        }
        stars_initialized = true;
    }

    CelestialState update(double tick, const SceneParams& sp) {
        CelestialState state = celestial.update(tick, sp);

        // Count visible stars (above horizon, not outshone by sun/moon)
        double sun_elev = std::cos(state.sun_zenith);
        double threshold = std::clamp(sun_elev * 0.5 + 0.3, 0.0, 1.0);

        int visible = 0;
        // Sample every 8th star for benchmark (full count would be GPU-rendered)
        for (int i = 0; i < STAR_COUNT; i += 8) {
            float sy = stars[i * 4 + 1];
            float mag = stars[i * 4 + 3];
            // Star visible if above horizon AND bright enough relative to sun
            if (sy > 0.0f && mag > threshold * 0.5f)
                ++visible;
        }
        state.star_count_visible = visible;
        return state;
    }
};

// ============================================================
// Strategy E: PhysicalAttenuation (C + Rayleigh/Mie twilight)
// ============================================================
struct PhysicalAttenuation {
    static constexpr const char* name = "E_PhysicalAttenuation";

    FullCelestial celestial;

    // Rayleigh scattering phase function: P(θ) = 3/4 * (1 + cos²(θ))
    static double rayleigh_phase(double cos_theta) {
        return 0.75 * (1.0 + cos_theta * cos_theta);
    }

    // Mie scattering phase function (HG approximation)
    static double mie_phase(double cos_theta, double g) {
        double denom = 1.0 + g * g - 2.0 * g * cos_theta;
        return (1.0 - g * g) / (std::pow(denom, 1.5)) * 0.25 / std::numbers::pi;
    }

    CelestialState update(double tick, const SceneParams& sp) {
        CelestialState state = celestial.update(tick, sp);

        double sun_elev = std::cos(state.sun_zenith);
        double zenith_angle = state.sun_zenith;

        // Optical airmass approximation (Kasten & Young 1989)
        double airmass = 1.0 / (std::cos(zenith_angle) + 0.50572 * std::pow(zenith_angle * 180.0 / std::numbers::pi + 6.07995, -1.6364));
        airmass = std::clamp(airmass, 1.0, 40.0);

        // Rayleigh optical depth at sea level
        double tau_rayleigh = 0.0088 * std::pow(airmass, 0.84);
        // Mie optical depth (aerosol)
        double tau_mie = 0.0040 * sp.turbidity * std::pow(airmass, 0.75);

        // Wavelength-dependent Rayleigh scattering (λ⁻⁴ approximation)
        // Use 3 bands: R=650nm, G=510nm, B=475nm
        constexpr double lambda_r = 0.650; // µm
        constexpr double lambda_g = 0.510;
        constexpr double lambda_b = 0.475;

        // Normalized λ⁻⁴ coefficients
        double lr4 = 1.0 / std::pow(lambda_r, 4.0);
        double lg4 = 1.0 / std::pow(lambda_g, 4.0);
        double lb4 = 1.0 / std::pow(lambda_b, 4.0);

        // Twilight color: multiply Rayleigh scattering by exp(-τ/µ)
        // where µ = cos(zenith) — shorter path = more blue
        double view_angle = std::numbers::pi / 2.0; // looking at horizon
        double mu = std::cos(view_angle);

        double extinction_r = std::exp(-(lr4 * tau_rayleigh + tau_mie) / mu);
        double extinction_g = std::exp(-(lg4 * tau_rayleigh + tau_mie) / mu);
        double extinction_b = std::exp(-(lb4 * tau_rayleigh + tau_mie) / mu);

        // Scattered light (sky color at horizon during twilight)
        double cos_theta = std::cos(state.sun_zenith);
        double rphase = rayleigh_phase(cos_theta);
        double mphase = mie_phase(cos_theta, 0.76);

        // Combine: sun below horizon → sky gets its color from scattered light
        // above horizon → direct sunlight dominates
        double sun_below = std::clamp(-sun_elev * 5.0, 0.0, 1.0); // 0=above horizon, 1=well below

        double scatter_r = rphase * extinction_r * lr4 + mphase * extinction_r;
        double scatter_g = rphase * extinction_g * lg4 + mphase * extinction_g;
        double scatter_b = rphase * extinction_b * lb4 + mphase * extinction_b;

        // Normalize scattered light
        double scatter_max = std::max({scatter_r, scatter_g, scatter_b});
        if (scatter_max > 0.0) {
            // During twilight, normalize so brightest channel = 1
            double norm = 1.0 / scatter_max;
            state.ambient_r = scatter_r * norm * sun_below + (1.0 - sun_below) * 1.0;
            state.ambient_g = scatter_g * norm * sun_below + (1.0 - sun_below) * 1.0;
            state.ambient_b = scatter_b * norm * sun_below + (1.0 - sun_below) * 1.0;
        }

        // Canopy attenuation
        double canopy = 1.0 - sp.canopy_factor * 0.6;
        state.ambient_intensity *= canopy;

        return state;
    }
};

// ============================================================
// Benchmark harness
// ============================================================
struct Measurement {
    const char* strategy;
    const char* scene;
    unsigned seed;
    double mean_ns;
    double median_ns;
    double p95_ns;
    double p99_ns;
    double std_ns;
    int samples;
    double ambient_mean;
    double twilight_r_mean;
};

template <typename Strategy>
Measurement bench_strategy(unsigned seed, Scene scene, int warmup_iter, int main_iter) {
    Strategy strat;
    SceneParams sp = SCENE_PARAMS[static_cast<int>(scene)];

    // Initialize star field if applicable
    if constexpr (requires { strat.init_stars(seed); })
        strat.init_stars(seed);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> tick_dist(0.0, DAY_PERIOD);

    // Warmup
    double ambient_acc = 0.0;
    double twilight_r_acc = 0.0;
    for (int i = 0; i < warmup_iter; ++i) {
        double tick = tick_dist(rng);
        auto state = strat.update(tick, sp);
        ambient_acc += state.ambient_intensity;
        twilight_r_acc += state.ambient_r;
    }

    std::vector<double> samples;
    samples.reserve(main_iter);

    ambient_acc = 0.0;
    twilight_r_acc = 0.0;

    for (int i = 0; i < main_iter; ++i) {
        double tick = tick_dist(rng);

        auto t0 = std::chrono::high_resolution_clock::now();
        auto state = strat.update(tick, sp);
        auto t1 = std::chrono::high_resolution_clock::now();

        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        samples.push_back(ns);

        ambient_acc += state.ambient_intensity;
        twilight_r_acc += state.ambient_r;
    }

    std::sort(samples.begin(), samples.end());

    double mean = 0.0;
    for (double s : samples) mean += s;
    mean /= main_iter;

    double var = 0.0;
    for (double s : samples) var += (s - mean) * (s - mean);
    var /= main_iter;

    double median = samples[main_iter / 2];
    double p95 = samples[static_cast<int>(main_iter * 0.95)];
    double p99 = samples[static_cast<int>(main_iter * 0.99)];

    return Measurement{
        .strategy = Strategy::name,
        .scene = scene_name(scene),
        .seed = seed,
        .mean_ns = mean,
        .median_ns = median,
        .p95_ns = p95,
        .p99_ns = p99,
        .std_ns = std::sqrt(var),
        .samples = main_iter,
        .ambient_mean = ambient_acc / main_iter,
        .twilight_r_mean = twilight_r_acc / main_iter
    };
}

int main() {
    constexpr int WARMUP = 10;
    constexpr int MAIN = 1000;
    constexpr unsigned SEEDS[] = {1, 7, 42, 1234, 31337};

    // CSV header
    std::printf("strategy,scene,seed,mean_ns,median_ns,p95_ns,p99_ns,std_ns,samples,ambient_mean,twilight_r_mean\n");

    for (int si = 0; si < SCENE_COUNT; ++si) {
        Scene scene = static_cast<Scene>(si);
        for (unsigned seed : SEEDS) {
            auto m = bench_strategy<NoCycle>(seed, scene, WARMUP, MAIN);
            std::printf("%s,%s,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.4f,%.4f\n",
                m.strategy, m.scene, m.seed, m.mean_ns, m.median_ns, m.p95_ns, m.p99_ns, m.std_ns, m.samples, m.ambient_mean, m.twilight_r_mean);

            m = bench_strategy<SimpleSunAngle>(seed, scene, WARMUP, MAIN);
            std::printf("%s,%s,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.4f,%.4f\n",
                m.strategy, m.scene, m.seed, m.mean_ns, m.median_ns, m.p95_ns, m.p99_ns, m.std_ns, m.samples, m.ambient_mean, m.twilight_r_mean);

            m = bench_strategy<FullCelestial>(seed, scene, WARMUP, MAIN);
            std::printf("%s,%s,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.4f,%.4f\n",
                m.strategy, m.scene, m.seed, m.mean_ns, m.median_ns, m.p95_ns, m.p99_ns, m.std_ns, m.samples, m.ambient_mean, m.twilight_r_mean);

            m = bench_strategy<CelestialPlusStars>(seed, scene, WARMUP, MAIN);
            std::printf("%s,%s,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.4f,%.4f\n",
                m.strategy, m.scene, m.seed, m.mean_ns, m.median_ns, m.p95_ns, m.p99_ns, m.std_ns, m.samples, m.ambient_mean, m.twilight_r_mean);

            m = bench_strategy<PhysicalAttenuation>(seed, scene, WARMUP, MAIN);
            std::printf("%s,%s,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.4f,%.4f\n",
                m.strategy, m.scene, m.seed, m.mean_ns, m.median_ns, m.p95_ns, m.p99_ns, m.std_ns, m.samples, m.ambient_mean, m.twilight_r_mean);
        }
    }

    return 0;
}
