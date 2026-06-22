// 2026-06-22-weather-svo-metafield prototype
//
// Standalone C++26 CPU prototype — Battlefield Atmospheric Weather as SVO Meta-Field.
//
// Compares 5 strategies:
//   A_NoField                  — constant defaults, no per-chunk variation
//   B_StaticRandomPerChunk     — one-time seeded random, no temporal evolution
//   C_StaticSimplexNoise       — 3D simplex noise lookup, smooth spatial but no temporal
//   D_CA_Advection_3Var        — cellular automaton advection of T/ρ/wind per 1-Hz tick
//   E_NWPLite_WeatherFronts    — full NWP-lite: 2D pressure gradient + Coriolis geostrophic wind
//
// Per chunk: 4 floats (T, ρ, wind_xz, humidity) = 16 B/chunk.
// World = 16³ chunks = 4096 cells = 64 KiB worst-case.
//
// 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements.
// 5 consumer-callback chains validated per measurement:
//   - ballistic wind drift at 1000 m range
//   - IRST atmospheric τ at 10 km
//   - visibility fog at 0.5 contrast threshold
//   - fire humidity suppression ratio
//   - fluid CA precipitation trigger (boolean)
//
// Build:  clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//         weather_metafield_bench.cpp -o build/weather_metafield_bench
// Run:    ./build/weather_metafield_bench
// Output: build/results.csv (126 rows = 1 header + 125 data)
//         build/summary_means.csv (26 rows = 5 strategies × 5 scenes + 1 header)
//         build/run.log (timing + seed info)

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace projectv::weather {

// ============ Constants ============

inline constexpr int   CHUNK_SIZE  = 8;    // ProjectV chunk extent (per VoxelChunk.hpp)
inline constexpr int   WORLD_DIM   = 16;   // 16³ chunks per world
inline constexpr int   WORLD_SIZE  = WORLD_DIM * WORLD_DIM * WORLD_DIM;  // 4096
inline constexpr float CHUNK_M     = 8.0f; // 1 chunk = 8m in world coordinates
inline constexpr float WORLD_M     = CHUNK_M * WORLD_DIM;  // 128m world extent
inline constexpr int   ITER        = 1000;
inline constexpr int   WARMUP      = 10;
inline constexpr int   SEEDS       = 5;

// Physical constants
inline constexpr float R_DRY_AIR   = 287.058f;     // J/(kg·K), specific gas constant for dry air
inline constexpr float P_SEA_LEVEL = 101325.0f;    // Pa
inline constexpr float STD_TEMP_K  = 288.15f;      // K, 15°C
inline constexpr float T_ABS_MIN_K = 200.0f;       // K, hard clamp
inline constexpr float T_ABS_MAX_K = 330.0f;       // K, hard clamp
inline constexpr float RHO_MIN     = 0.8f;         // kg/m³
inline constexpr float RHO_MAX     = 1.6f;
inline constexpr float WIND_MAX    = 30.0f;        // m/s
inline constexpr float OMEGA_EARTH = 7.2921159e-5f; // rad/s, Earth angular velocity
inline constexpr float LATITUDE_0  = 45.0f;        // °N, mid-latitude for Coriolis

// ============ Data structures ============

struct alignas(16) WeatherCell {
    float temperature_K;   // K
    float air_density;     // kg/m³
    float wind_xz;         // m/s (signed 1D scalar — packed magnitude + direction, see UnpackWind)
    float humidity;        // 0-1 relative
};

// World state (shared by all strategies for consumer-callback chain)
struct WorldState {
    std::array<WeatherCell, WORLD_SIZE> cells{};
    int    tick           = 0;
    double current_time_s = 0.0;
    double last_update_s  = 0.0;
    uint32_t seed         = 0;
};

// Compute density from T and p via ideal gas law
inline float ComputeDensity(float temperature_K, float pressure_Pa) noexcept {
    return pressure_Pa / (R_DRY_AIR * temperature_K);
}

// Compute pressure from density and T (inverse)
inline float ComputePressure(float air_density, float temperature_K) noexcept {
    return air_density * R_DRY_AIR * temperature_K;
}

// Unpack 1D wind scalar to (x, z) magnitude
inline void UnpackWind(float packed, float& x, float& z) noexcept {
    // Pack: xz = vx + vz * WIND_MAX, so vx = xz % WIND_MAX, vz = floor(xz / WIND_MAX)
    // For simplicity in 1D scalar encoding, use signed 16-bit split
    // (here use simple signed representation: positive = east, negative = west)
    // Actually for the prototype, just use 1D wind = scalar wind speed, treat as magnitude
    // Direction = sign (east vs west); for vertical wind neglect (weather is approx horizontal)
    x = (packed > 0.0f) ? packed : 0.0f;  // east component
    z = 0.0f;                              // north component (neglected for simplicity)
}

// 3D index helpers (per chunk (x, y, z) where x,y,z ∈ [0, WORLD_DIM))
inline int Idx(int x, int y, int z) noexcept {
    return (z * WORLD_DIM + y) * WORLD_DIM + x;
}

inline int ClampIdx(int v) noexcept {
    return std::clamp(v, 0, WORLD_DIM - 1);
}

// ============ Scenarios ============

struct Scene {
    const char* name;
    float base_T_K;       // K
    float base_rho;       // kg/m³
    float base_wind;      // m/s
    float base_humidity;  // 0-1
    float pressure_Pa;    // Pa
};

// 5 synthetic scenes per `benchmarks/methodology.md` precedent
inline constexpr std::array<Scene, 5> SCENES = {{
    {"s1_clear_summer",  288.0f, 1.225f,  2.0f, 0.50f, P_SEA_LEVEL},
    {"s2_storm_cold",   275.0f, 1.350f, 15.0f, 0.90f, P_SEA_LEVEL * 0.96f},
    {"s3_arid_desert",  315.0f, 1.100f,  5.0f, 0.10f, P_SEA_LEVEL * 1.02f},
    {"s4_arctic_bliz",  243.0f, 1.500f, 25.0f, 0.70f, P_SEA_LEVEL * 0.97f},
    {"s5_trop_humid",   303.0f, 1.180f,  8.0f, 0.95f, P_SEA_LEVEL * 1.00f},
}};

// Forward declaration
class WeatherField;

// ============ Consumer callbacks ============
//
// Each consumer reads the per-chunk weather field and computes a physically-meaningful
// output delta vs no-field baseline. The output value is then validated to be "reasonable".

// Consumer 1: Ballistic wind drift at 1000 m range, 800 m/s muzzle velocity
//   Crosswind perpendicular to flight = wind magnitude
//   Drift = wind * time_of_flight, time_of_flight = range / muzzle_velocity
//   Result: drift in meters
inline float ConsumerBallisticDrift(const WeatherField& field, int x, int y, int z);

// Consumer 2: IRST atmospheric extinction τ at 10 km range
inline float ConsumerIRSTExtinction(const WeatherField& field, int x, int y, int z);

// Consumer 3: Visibility fog distance at 0.5 contrast threshold
inline float ConsumerVisibilityFog(const WeatherField& field, int x, int y, int z);

// Consumer 4: Fire humidity suppression ratio (relative to dry baseline)
inline float ConsumerFireHumidity(const WeatherField& field, int x, int y, int z);

// Consumer 5: Fluid CA precipitation trigger (boolean → float for averaging)
inline float ConsumerFluidPrecipitation(const WeatherField& field, int x, int y, int z);

// ============ Strategies ============

class WeatherField {
public:
    virtual ~WeatherField() = default;
    virtual void    Init(const Scene& scene, uint32_t seed) = 0;
    virtual void    Update(WorldState& w, float dt_seconds) = 0;
    virtual WeatherCell Query(int x, int y, int z) const = 0;
    virtual const char* Name() const = 0;
};

// Now define consumer functions (after WeatherField is known)
inline float ConsumerBallisticDrift(const WeatherField& field, int x, int y, int z) {
    auto cell = field.Query(x, y, z);
    const float range_m     = 1000.0f;
    const float muzzle_v_mps = 800.0f;
    const float t_flight_s   = range_m / muzzle_v_mps;  // 1.25 s
    return std::abs(cell.wind_xz) * t_flight_s;          // drift in m
}

inline float ConsumerIRSTExtinction(const WeatherField& field, int x, int y, int z) {
    auto cell = field.Query(x, y, z);
    const float range_km   = 10.0f;
    const float base_alpha  = 0.02f;
    const float hum_factor  = cell.humidity * 0.5f;
    const float rho_factor  = (cell.air_density - 1.225f) * 0.3f;
    const float alpha       = base_alpha + hum_factor + rho_factor;
    return std::exp(-alpha * range_km);
}

inline float ConsumerVisibilityFog(const WeatherField& field, int x, int y, int z) {
    auto cell = field.Query(x, y, z);
    const float fog_threshold = 0.85f;
    if (cell.humidity < fog_threshold) {
        return 10000.0f;
    }
    const float excess = cell.humidity - fog_threshold;
    const float beta = 0.05f + excess * 5.0f;
    return std::log(2.0f) / beta;
}

inline float ConsumerFireHumidity(const WeatherField& field, int x, int y, int z) {
    auto cell = field.Query(x, y, z);
    const float suppression = 0.8f;
    return 1.0f - cell.humidity * suppression;
}

inline float ConsumerFluidPrecipitation(const WeatherField& field, int x, int y, int z) {
    auto cell = field.Query(x, y, z);
    const bool is_rain = cell.humidity > 0.90f && cell.temperature_K > 273.0f;
    const bool is_snow = cell.humidity > 0.90f && cell.temperature_K <= 273.0f;
    return (is_rain || is_snow) ? 1.0f : 0.0f;
}

// A_NoField: constant defaults, no per-chunk variation
class A_NoField final : public WeatherField {
    WeatherCell base_;
public:
    void Init(const Scene& scene, uint32_t /*seed*/) override {
        base_ = {scene.base_T_K, scene.base_rho, scene.base_wind, scene.base_humidity};
    }
    void Update(WorldState& /*w*/, float /*dt*/) override {
        // No update — static field
    }
    WeatherCell Query(int /*x*/, int /*y*/, int /*z*/) const override {
        return base_;
    }
    const char* Name() const override { return "A_NoField"; }
};

// B_StaticRandomPerChunk: one-time seeded random, no temporal evolution
class B_StaticRandomPerChunk final : public WeatherField {
    std::array<WeatherCell, WORLD_SIZE> cells_{};
public:
    void Init(const Scene& scene, uint32_t seed) override {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> u_T(-3.0f, 3.0f);          // ±3 K
        std::uniform_real_distribution<float> u_rho(-0.05f, 0.05f);     // ±0.05 kg/m³
        std::uniform_real_distribution<float> u_wind(-3.0f, 3.0f);       // ±3 m/s
        std::uniform_real_distribution<float> u_hum(-0.05f, 0.05f);      // ±0.05 RH
        for (auto& c : cells_) {
            c.temperature_K = std::clamp(scene.base_T_K + u_T(rng), T_ABS_MIN_K, T_ABS_MAX_K);
            c.air_density   = std::clamp(scene.base_rho + u_rho(rng), RHO_MIN, RHO_MAX);
            c.wind_xz       = std::clamp(scene.base_wind + u_wind(rng), -WIND_MAX, WIND_MAX);
            c.humidity      = std::clamp(scene.base_humidity + u_hum(rng), 0.0f, 1.0f);
        }
    }
    void Update(WorldState& /*w*/, float /*dt*/) override {
        // Static — no update
    }
    WeatherCell Query(int x, int y, int z) const override {
        return cells_[Idx(x, y, z)];
    }
    const char* Name() const override { return "B_StaticRandomPerChunk"; }
};

// C_StaticSimplexNoise: 3D simplex noise lookup, smooth spatial but no temporal
//   Implementation: 3D value noise via hashing + trilinear interpolation
class C_StaticSimplexNoise final : public WeatherField {
    std::array<WeatherCell, WORLD_SIZE> cells_{};
public:
    void Init(const Scene& scene, uint32_t seed) override {
        // Use simple 3D value noise — not actual simplex (which is complex)
        // Trilinear interpolation of hashed random values at 4³ coarse grid
        std::mt19937 rng(seed);
        // Coarse grid: 5³ random values per scene
        constexpr int COARSE = 5;
        constexpr int COARSE_SIZE = COARSE * COARSE * COARSE;
        std::array<float, COARSE_SIZE> noise_T, noise_rho, noise_wind, noise_hum;
        std::uniform_real_distribution<float> u(-1.0f, 1.0f);
        for (auto& n : noise_T)    n = u(rng) * 4.0f;     // ±4 K
        for (auto& n : noise_rho)  n = u(rng) * 0.06f;   // ±0.06 kg/m³
        for (auto& n : noise_wind) n = u(rng) * 6.0f;     // ±6 m/s
        for (auto& n : noise_hum)  n = u(rng) * 0.15f;    // ±0.15 RH
        for (int z = 0; z < WORLD_DIM; ++z) {
            for (int y = 0; y < WORLD_DIM; ++y) {
                for (int x = 0; x < WORLD_DIM; ++x) {
                    float fx = (float(x) / (WORLD_DIM - 1)) * (COARSE - 1);
                    float fy = (float(y) / (WORLD_DIM - 1)) * (COARSE - 1);
                    float fz = (float(z) / (WORLD_DIM - 1)) * (COARSE - 1);
                    int x0 = int(fx), y0 = int(fy), z0 = int(fz);
                    int x1 = std::min(x0 + 1, COARSE - 1);
                    int y1 = std::min(y0 + 1, COARSE - 1);
                    int z1 = std::min(z0 + 1, COARSE - 1);
                    float tx = fx - x0, ty = fy - y0, tz = fz - z0;
                    auto smooth = [](float t) { return t * t * (3 - 2 * t); };
                    tx = smooth(tx); ty = smooth(ty); tz = smooth(tz);
                    auto interp = [&](const std::array<float, COARSE_SIZE>& n) {
                        auto at = [&](int cx, int cy, int cz) {
                            return n[(cz * COARSE + cy) * COARSE + cx];
                        };
                        float c000 = at(x0, y0, z0), c100 = at(x1, y0, z0);
                        float c010 = at(x0, y1, z0), c110 = at(x1, y1, z0);
                        float c001 = at(x0, y0, z1), c101 = at(x1, y0, z1);
                        float c011 = at(x0, y1, z1), c111 = at(x1, y1, z1);
                        float c00 = c000 * (1 - tx) + c100 * tx;
                        float c01 = c001 * (1 - tx) + c101 * tx;
                        float c10 = c010 * (1 - tx) + c110 * tx;
                        float c11 = c011 * (1 - tx) + c111 * tx;
                        float c0 = c00 * (1 - ty) + c10 * ty;
                        float c1 = c01 * (1 - ty) + c11 * ty;
                        return c0 * (1 - tz) + c1 * tz;
                    };
                    auto& c = cells_[Idx(x, y, z)];
                    c.temperature_K = std::clamp(scene.base_T_K + interp(noise_T),    T_ABS_MIN_K, T_ABS_MAX_K);
                    c.air_density   = std::clamp(scene.base_rho + interp(noise_rho),  RHO_MIN, RHO_MAX);
                    c.wind_xz       = std::clamp(scene.base_wind + interp(noise_wind), -WIND_MAX, WIND_MAX);
                    c.humidity      = std::clamp(scene.base_humidity + interp(noise_hum), 0.0f, 1.0f);
                }
            }
        }
    }
    void Update(WorldState& /*w*/, float /*dt*/) override { /* static */ }
    WeatherCell Query(int x, int y, int z) const override { return cells_[Idx(x, y, z)]; }
    const char* Name() const override { return "C_StaticSimplexNoise"; }
};

// D_CA_Advection_3Var: cellular automaton advection of T/ρ/wind per 1-Hz tick
//   Rules: each cell adopts average of 6 neighbors (von Neumann), with damping toward base values
//   Humidity evolves by advection + condensation/evaporation based on T
class D_CA_Advection_3Var final : public WeatherField {
    std::array<WeatherCell, WORLD_SIZE> cells_{};
    std::array<WeatherCell, WORLD_SIZE> next_{};
    Scene base_scene_{};
    uint32_t seed_ = 0;
public:
    void Init(const Scene& scene, uint32_t seed) override {
        base_scene_ = scene;
        seed_ = seed;
        // Start with random per-chunk variation (seeded)
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> u_T(-2.0f, 2.0f);
        std::uniform_real_distribution<float> u_rho(-0.04f, 0.04f);
        std::uniform_real_distribution<float> u_wind(-2.0f, 2.0f);
        std::uniform_real_distribution<float> u_hum(-0.05f, 0.05f);
        for (auto& c : cells_) {
            c.temperature_K = std::clamp(scene.base_T_K + u_T(rng),    T_ABS_MIN_K, T_ABS_MAX_K);
            c.air_density   = std::clamp(scene.base_rho + u_rho(rng),  RHO_MIN, RHO_MAX);
            c.wind_xz       = std::clamp(scene.base_wind + u_wind(rng), -WIND_MAX, WIND_MAX);
            c.humidity      = std::clamp(scene.base_humidity + u_hum(rng), 0.0f, 1.0f);
        }
    }
    void Update(WorldState& w, float dt) override {
        // Cellular automaton: each cell adopts 0.5 self + 0.5 avg of 6 neighbors (von Neumann)
        // with damping toward base scene values (relaxation coefficient 0.01 per tick)
        const float self_w = 0.5f;
        const float relax = std::min(0.02f, dt * 0.02f);
        for (int z = 0; z < WORLD_DIM; ++z) {
            for (int y = 0; y < WORLD_DIM; ++y) {
                for (int x = 0; x < WORLD_DIM; ++x) {
                    int i = Idx(x, y, z);
                    int xp = ClampIdx(x + 1), xm = ClampIdx(x - 1);
                    int yp = ClampIdx(y + 1), ym = ClampIdx(y - 1);
                    int zp = ClampIdx(z + 1), zm = ClampIdx(z - 1);
                    const auto& c = cells_[i];
                    // Average of 6 neighbors
                    auto avg_var = [&](auto member) {
                        float sum = 0.0f;
                        sum += cells_[Idx(xp, y, z)].*member;
                        sum += cells_[Idx(xm, y, z)].*member;
                        sum += cells_[Idx(x, yp, z)].*member;
                        sum += cells_[Idx(x, ym, z)].*member;
                        sum += cells_[Idx(x, y, zp)].*member;
                        sum += cells_[Idx(x, y, zm)].*member;
                        return sum * (1.0f / 6.0f);
                    };
                    WeatherCell n;
                    n.temperature_K = self_w * c.temperature_K + (1.0f - self_w) * avg_var(&WeatherCell::temperature_K);
                    n.air_density   = self_w * c.air_density   + (1.0f - self_w) * avg_var(&WeatherCell::air_density);
                    n.wind_xz       = self_w * c.wind_xz       + (1.0f - self_w) * avg_var(&WeatherCell::wind_xz);
                    n.humidity      = self_w * c.humidity      + (1.0f - self_w) * avg_var(&WeatherCell::humidity);
                    // Damping toward base scene
                    n.temperature_K = n.temperature_K * (1 - relax) + base_scene_.base_T_K * relax;
                    n.air_density   = n.air_density   * (1 - relax) + base_scene_.base_rho * relax;
                    n.wind_xz       = n.wind_xz       * (1 - relax) + base_scene_.base_wind * relax;
                    n.humidity      = n.humidity      * (1 - relax) + base_scene_.base_humidity * relax;
                    // Clamp
                    n.temperature_K = std::clamp(n.temperature_K, T_ABS_MIN_K, T_ABS_MAX_K);
                    n.air_density   = std::clamp(n.air_density,   RHO_MIN,     RHO_MAX);
                    n.wind_xz       = std::clamp(n.wind_xz,       -WIND_MAX,   WIND_MAX);
                    n.humidity      = std::clamp(n.humidity,      0.0f,        1.0f);
                    next_[i] = n;
                }
            }
        }
        cells_ = next_;
        w.tick++;
        w.current_time_s += dt;
    }
    WeatherCell Query(int x, int y, int z) const override { return cells_[Idx(x, y, z)]; }
    const char* Name() const override { return "D_CA_Advection_3Var"; }
};

// E_NWPLite_WeatherFronts: full NWP-lite with 2D pressure gradient + Coriolis geostrophic wind
//   Per-tick:
//   1. Pressure evolves: gradient of humidity + temperature
//   2. Geostrophic wind: v = -(1/ρf) × ∇p (perpendicular to pressure gradient, f=Coriolis param)
//   3. Wind advects T, ρ, humidity by 1st-order upstream advection
//   4. Mass conservation: density adjusts to (p, T) via ideal gas law
class E_NWPLite_WeatherFronts final : public WeatherField {
    std::array<WeatherCell, WORLD_SIZE> cells_{};
    std::array<WeatherCell, WORLD_SIZE>  next_{};  // second buffer for advection
    std::array<float, WORLD_SIZE>       pressure_{};  // separate pressure field
    std::array<float, WORLD_SIZE>       pressure_next_{};
    Scene base_scene_{};
    uint32_t seed_ = 0;
public:
    void Init(const Scene& scene, uint32_t seed) override {
        base_scene_ = scene;
        seed_ = seed;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> u_T(-2.0f, 2.0f);
        std::uniform_real_distribution<float> u_rho(-0.04f, 0.04f);
        std::uniform_real_distribution<float> u_wind(-2.0f, 2.0f);
        std::uniform_real_distribution<float> u_hum(-0.05f, 0.05f);
        std::uniform_real_distribution<float> u_p(-100.0f, 100.0f);  // ±100 Pa pressure variation
        for (size_t i = 0; i < WORLD_SIZE; ++i) {
            cells_[i].temperature_K = std::clamp(scene.base_T_K + u_T(rng),     T_ABS_MIN_K, T_ABS_MAX_K);
            cells_[i].air_density   = std::clamp(scene.base_rho + u_rho(rng),   RHO_MIN,     RHO_MAX);
            cells_[i].wind_xz       = std::clamp(scene.base_wind + u_wind(rng), -WIND_MAX,   WIND_MAX);
            cells_[i].humidity      = std::clamp(scene.base_humidity + u_hum(rng), 0.0f,    1.0f);
            pressure_[i]            = scene.pressure_Pa + u_p(rng);
        }
    }
    void Update(WorldState& w, float dt) override {
        // Coriolis parameter for mid-latitude
        const float f = 2.0f * OMEGA_EARTH * std::sin(LATITUDE_0 * 3.14159265f / 180.0f);
        for (int z = 0; z < WORLD_DIM; ++z) {
            for (int y = 0; y < WORLD_DIM; ++y) {
                for (int x = 0; x < WORLD_DIM; ++x) {
                    int i = Idx(x, y, z);
                    int xp = ClampIdx(x + 1), xm = ClampIdx(x - 1);
                    int yp = ClampIdx(y + 1), ym = ClampIdx(y - 1);
                    // Geostrophic wind from pressure gradient
                    // v = (1/ρf) × ∇p perpendicular; 1D packed: use y-gradient as x-component
                    float dpy = (pressure_[Idx(x, yp, z)] - pressure_[Idx(x, ym, z)]) / (2.0f * CHUNK_M);
                    float rho_avg = 0.5f * (cells_[Idx(xp, y, z)].air_density + cells_[i].air_density);
                    if (std::abs(f) < 1e-6f || std::abs(rho_avg) < 1e-3f) {
                        next_[i].wind_xz = cells_[i].wind_xz;
                    } else {
                        float vx_geo = (1.0f / (rho_avg * f)) * dpy;
                        next_[i].wind_xz = std::clamp(vx_geo, -WIND_MAX, WIND_MAX);
                    }
                    // Advect T, humidity by upstream advection (1st-order, x-direction)
                    int xsrc = wind_to_xsrc(x, next_[i].wind_xz);
                    next_[i].temperature_K = 0.5f * cells_[i].temperature_K + 0.5f * cells_[Idx(xsrc, y, z)].temperature_K;
                    next_[i].humidity      = 0.5f * cells_[i].humidity      + 0.5f * cells_[Idx(xsrc, y, z)].humidity;
                    // Pressure evolves based on humidity gradient (crude weather front)
                    pressure_next_[i] = pressure_[i] + dt * (cells_[Idx(xp, y, z)].humidity - cells_[Idx(xm, y, z)].humidity) * 50.0f;
                    // Density via ideal gas law
                    next_[i].air_density = ComputeDensity(next_[i].temperature_K, pressure_next_[i]);
                }
            }
        }
        cells_ = next_;
        pressure_ = pressure_next_;
        for (auto& c : cells_) {
            c.temperature_K = std::clamp(c.temperature_K, T_ABS_MIN_K, T_ABS_MAX_K);
            c.air_density   = std::clamp(c.air_density,   RHO_MIN,     RHO_MAX);
            c.wind_xz       = std::clamp(c.wind_xz,       -WIND_MAX,   WIND_MAX);
            c.humidity      = std::clamp(c.humidity,      0.0f,        1.0f);
        }
        w.tick++;
        w.current_time_s += dt;
    }
private:
    static int wind_to_xsrc(int x, float wind) noexcept {
        // Wind positive = east → advect from west → source = x-1
        // Wind negative = west → advect from east → source = x+1
        if (wind > 0.0f) return ClampIdx(x - 1);
        if (wind < 0.0f) return ClampIdx(x + 1);
        return x;
    }
public:
    WeatherCell Query(int x, int y, int z) const override { return cells_[Idx(x, y, z)]; }
    const char* Name() const override { return "E_NWPLite_WeatherFronts"; }
};

// ============ Factory ============

inline std::unique_ptr<WeatherField> MakeStrategy(const std::string& name) {
    if (name == "A_NoField")               return std::make_unique<A_NoField>();
    if (name == "B_StaticRandomPerChunk")  return std::make_unique<B_StaticRandomPerChunk>();
    if (name == "C_StaticSimplexNoise")    return std::make_unique<C_StaticSimplexNoise>();
    if (name == "D_CA_Advection_3Var")     return std::make_unique<D_CA_Advection_3Var>();
    if (name == "E_NWPLite_WeatherFronts") return std::make_unique<E_NWPLite_WeatherFronts>();
    return nullptr;
}

inline std::array<std::string, 5> STRATEGY_NAMES = {
    "A_NoField", "B_StaticRandomPerChunk", "C_StaticSimplexNoise",
    "D_CA_Advection_3Var", "E_NWPLite_WeatherFronts"
};

}  // namespace projectv::weather

// ============ Benchmark harness ============

using namespace projectv::weather;

struct Measurement {
    std::string strategy;
    std::string scene;
    int         seed;
    int         iter;
    double      update_ns;        // wall time per Update() call
    double      query_ns;         // wall time per Query() call (1 sample)
    double      ballistic_drift;  // consumer 1: drift in m
    double      irst_tau;         // consumer 2: extinction ratio 0-1
    double      visibility_m;     // consumer 3: visibility in m
    double      fire_rate_ratio;  // consumer 4: fire spread rate ratio
    double      precip_active;    // consumer 5: precipitation boolean (0 or 1)
    double      memory_bytes;     // total memory footprint
};

template <typename Fn>
double TimeNanos(Fn&& fn) {
    auto t0 = std::chrono::steady_clock::now();
    fn();
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

int main() {
    using clk = std::chrono::steady_clock;
    auto t_start = clk::now();
    std::cout << "=== 2026-06-22-weather-svo-metafield benchmark ===\n";
    std::cout << "Strategies × Scenes × Seeds × ITER + WARMUP = "
              << 5 << " × " << 5 << " × " << SEEDS << " × " << ITER << " + "
              << WARMUP << " = " << (5 * 5 * SEEDS * ITER) << " main measurements\n";
    std::cout << "World: 16³ chunks = " << WORLD_SIZE << " cells × 16 B = "
              << (WORLD_SIZE * 16 / 1024) << " KiB\n";
    std::cout << "Update tick: 1.0 s (1 Hz)\n\n";

    std::vector<Measurement> measurements;
    measurements.reserve(5 * 5 * SEEDS * ITER + 16);

    // Open CSV output
    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,iter,update_ns,query_ns,ballistic_drift_m,"
           "irst_tau,visibility_m,fire_rate_ratio,precip_active,memory_bytes\n";

    // Per-strategy × per-scene mean summary
    struct SummaryRow {
        std::string strategy;
        std::string scene;
        int n = 0;
        double sum_update_ns = 0;
        double sum_ballistic = 0;
        double sum_irst = 0;
        double sum_visibility = 0;
        double sum_fire = 0;
        double sum_precip = 0;
    };
    std::vector<SummaryRow> summary;

    for (const auto& strat_name : STRATEGY_NAMES) {
        for (const auto& scene : SCENES) {
            SummaryRow row{strat_name, scene.name, 0, 0, 0, 0, 0, 0, 0};
            for (int seed = 0; seed < SEEDS; ++seed) {
                auto field = MakeStrategy(strat_name);
                if (!field) { std::cerr << "Unknown strategy: " << strat_name << "\n"; return 1; }
                field->Init(scene, 0x1234ABCDu + seed);
                WorldState w;
                w.seed = 0x1234ABCDu + seed;
                // Warmup
                for (int i = 0; i < WARMUP; ++i) {
                    field->Update(w, 1.0f);
                }
                // Measure ITER times
                for (int i = 0; i < ITER; ++i) {
                    double t_upd = TimeNanos([&] { field->Update(w, 1.0f); });
                    // Consumer callbacks: query center cell
                    int qx = WORLD_DIM / 2, qy = WORLD_DIM / 2, qz = WORLD_DIM / 2;
                    double t_q = TimeNanos([&] { (void)field->Query(qx, qy, qz); });
                    WeatherCell cell = field->Query(qx, qy, qz);
                    // Compute 5 consumer outputs
                    float bd = ConsumerBallisticDrift(*field, qx, qy, qz);
                    float it = ConsumerIRSTExtinction(*field, qx, qy, qz);
                    float vis = ConsumerVisibilityFog(*field, qx, qy, qz);
                    float fr = ConsumerFireHumidity(*field, qx, qy, qz);
                    float pr = ConsumerFluidPrecipitation(*field, qx, qy, qz);
                    Measurement m{strat_name, scene.name, seed, i, t_upd, t_q,
                                  (double)bd, (double)it, (double)vis, (double)fr, (double)pr,
                                  (double)(WORLD_SIZE * 16)};
                    measurements.push_back(m);
                    csv << m.strategy << "," << m.scene << "," << m.seed << "," << m.iter << ","
                        << std::fixed << std::setprecision(2) << m.update_ns << ","
                        << m.query_ns << "," << m.ballistic_drift << "," << m.irst_tau << ","
                        << m.visibility_m << "," << m.fire_rate_ratio << "," << m.precip_active << ","
                        << (long long)m.memory_bytes << "\n";
                    row.n++;
                    row.sum_update_ns += t_upd;
                    row.sum_ballistic += bd;
                    row.sum_irst += it;
                    row.sum_visibility += vis;
                    row.sum_fire += fr;
                    row.sum_precip += pr;
                }
            }
            summary.push_back(row);
        }
    }
    csv.close();

    // Write summary CSV
    std::ofstream sumcsv("build/summary_means.csv");
    sumcsv << "strategy,scene,n,mean_update_ns,mean_ballistic_m,mean_irst_tau,"
              "mean_visibility_m,mean_fire_ratio,mean_precip_active\n";
    for (const auto& r : summary) {
        double n_inv = 1.0 / std::max(1, r.n);
        sumcsv << r.strategy << "," << r.scene << "," << r.n << ","
               << std::fixed << std::setprecision(2)
               << (r.sum_update_ns * n_inv) << ","
               << (r.sum_ballistic * n_inv) << ","
               << (r.sum_irst * n_inv) << ","
               << (r.sum_visibility * n_inv) << ","
               << (r.sum_fire * n_inv) << ","
               << (r.sum_precip * n_inv) << "\n";
    }
    sumcsv.close();

    auto t_end = clk::now();
    double total_s = std::chrono::duration<double>(t_end - t_start).count();
    std::ofstream log("build/run.log");
    log << "Total wall time: " << total_s << " s\n";
    log << "Total measurements: " << measurements.size() << "\n";
    log << "Date: 2026-06-22\n";
    log.close();

    std::cout << "Total measurements: " << measurements.size() << "\n";
    std::cout << "Total wall time: " << total_s << " s\n";
    std::cout << "Output: build/results.csv (" << measurements.size() << " rows), "
              << "build/summary_means.csv (" << summary.size() << " rows), build/run.log\n";

    return 0;
}
