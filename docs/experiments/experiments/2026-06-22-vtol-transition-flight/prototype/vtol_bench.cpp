// Standalone C++26 CPU prototype for VTOL/STOVL transition flight dynamics.
//
// Compares 5 strategies for modeling nacelle-angle interpolation between
// hover and forward-flight aerodynamic regimes (Harrier / V-22 / F-35B).
//
// Per `docs/experiments/AGENTS.md` and `_TEMPLATE/README.md`:
// - Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic
// - 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements
// - Output: prototype/build/results.csv (one row per config)
// - Mean / median / p95 / p99 / std
//
// Mapping to ProjectV hot-path (VTOL/STOVL craft in military sandbox):
// - 1 tick ≈ 33.3 ms (30 Hz fixed step) for VTOL vehicle physics
// - target: <0.03 ms per craft per tick (per backlog hypothesis)
// - 100 simultaneous VTOL craft × 0.03 ms = 3 ms = 9% of 30 Hz budget
// - 1000 craft × 0.03 ms = 30 ms = 90% of 30 Hz (would need LOD)
//
// References (see sources.md):
// - V-22 Osprey: 12 sec full conversion, 100 knots conversion corridor, nacelle 0-97.5°
// - AV-8B Harrier: Pegasus 11-105 23,500 lbf, VIFF 98° max, MTOW 31,000 lb
// - Bell XV-15: 25 ft rotor, 75° nacelle = shortest STO, 13,000 lb VTO
// - F-35B: SDLF + 3BSM + roll posts (nacelle = ~0° always, lift fan on/off)
// - EaglePubs VTOL Chapter 70: TWR > 1, 5-10% margin, disk loading
// - NASA YAV-8B: full-envelope model (parameter ID)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace vtol {

// ============================================================================
// Physical constants & units
// ============================================================================

constexpr double kG           = 9.80665;        // m/s^2
constexpr double kAirDensity  = 1.225;          // kg/m^3 (sea level ISA)
[[maybe_unused]] constexpr double kRhoConvCorridor = 100.0;       // knots (V-22 design point)
[[maybe_unused]] constexpr double kV22FullConversionSec = 12.0;   // V-22 min full conversion time
[[maybe_unused]] constexpr double kHarrierViffMaxDeg = 98.0;      // AV-8B VIFF max (forward of vertical)
constexpr double kV22RearwardDeg = 97.5;         // V-22 max nacelle (rearward flight)
[[maybe_unused]] constexpr double kXV15ShortestStoDeg = 75.0;     // XV-15 shortest STO nacelle
constexpr double kDefaultTickHz = 30.0;          // ProjectV fixed step

// ============================================================================
// State: 6-DOF rigid body + nacelle as 7th DOF
// ============================================================================

struct AircraftState {
    // 6-DOF rigid body (NED frame, body origin at CG)
    double x{0.0}, y{0.0}, z{0.0};          // m   position
    double u{0.0}, v{0.0}, w{0.0};          // m/s body linear velocity
    double phi{0.0}, theta{0.0}, psi{0.0};  // rad Euler angles
    double p{0.0}, q{0.0}, r{0.0};          // rad/s body angular rates
    // 7th DOF: nacelle angle (0 = forward, 90 = vertical hover, >90 = rearward)
    double nacelle_deg{90.0};               // deg
    // Mass properties (constant for prototype)
    double mass{6800.0};                    // kg (~15,000 lb MTOW)
    double inertia_xx{15000.0};             // kg*m^2
    double inertia_yy{30000.0};
    double inertia_zz{22000.0};
    // Geometry
    double wing_area{30.0};                 // m^2
    double wing_span{12.0};                 // m
    double disk_area{91.0};                 // m^2 (2x rotor, XV-15 class)
    // Propulsion
    double t_max_hover{100000.0};           // N total (2 nacelles, ~50 kN each)
    double t_max_forward{50000.0};          // N
    // Aerodynamic coefficients (simplified, ISA sea level)
    double cl_max{1.6};                     // 2D clean wing
    double cl_alpha{5.5};                   // /rad
    double cd0{0.025};                      // parasite
    double k_ind{0.05};                     // induced factor
    // Disk-loading model (XV-15-like)
    double hover_disk_loading{74.0};        // kg/m^2 (XV-15 measured)
    // Conversion state
    double conversion_target_deg{0.0};      // commanded nacelle angle
    double conversion_rate_dps{7.5};        // deg/s (V-22 ~90°/12s = 7.5°/s)
    // External conditions
    double airspeed_kt{0.0};
    double altitude_m{0.0};
};

struct AeroForces {
    double lift{0.0};           // N, +Z body
    double drag{0.0};           // N, -X body
    double side{0.0};           // N, +Y body
    double thrust_n{0.0};       // N along nacelle axis
    double thrust_t{0.0};       // N perpendicular (vertical component when tilted)
    double moment_l{0.0};       // N*m, roll
    double moment_m{0.0};       // N*m, pitch
    double moment_n{0.0};       // N*m, yaw
};

// ============================================================================
// Strategy enum
// ============================================================================

enum class Strategy : int {
    A_PureHover = 0,
    B_PureForward = 1,
    C_BlendedTransition = 2,
    D_BlendWithCrossover = 3,
    E_PhysicsCoupledTiltRotor = 4,
};

inline const char* strategy_name(Strategy s) {
    switch (s) {
        case Strategy::A_PureHover:           return "A_PureHover";
        case Strategy::B_PureForward:         return "B_PureForward";
        case Strategy::C_BlendedTransition:   return "C_BlendedTransition";
        case Strategy::D_BlendWithCrossover:  return "D_BlendWithCrossover";
        case Strategy::E_PhysicsCoupledTiltRotor: return "E_PhysicsCoupledTiltRotor";
    }
    return "UNKNOWN";
}

// ============================================================================
// Scene profiles (transition patterns)
// ============================================================================

struct SceneProfile {
    const char* name;
    double initial_airspeed_kt;
    double final_airspeed_kt;
    double initial_nacelle_deg;
    double final_nacelle_deg;
    double initial_altitude_m;
    double wind_speed_kt;       // crosswind during transition
    double weight_factor;        // gross weight multiplier
    double center_of_gravity_offset;  // CG offset (% of MAC, sign affects moment)
    double max_pitch_rate_dps;   // pitch rate limit (PIO sensitivity)
    bool engine_out;             // single-engine failure mid-transition
    bool carrier_landing;        // shipborne rolling vertical landing (SRVL) mode
};

inline SceneProfile make_scene_harrier_short_takeoff() {
    return SceneProfile{
        "harrier_short_takeoff",
        0.0, 120.0,    // 0 -> 120 kt (STO profile)
        90.0, 0.0,     // nacelle from vertical to forward
        0.0,
        0.0,           // no crosswind
        1.0,
        0.0,           // CG centered
        30.0,          // pitch rate limit
        false, false
    };
}

inline SceneProfile make_scene_osprey_full_tilt() {
    return SceneProfile{
        "osprey_full_tilt",
        0.0, 250.0,    // 0 -> 250 kt (full conversion)
        90.0, 0.0,
        0.0,
        10.0,          // 10 kt crosswind
        1.0,
        0.0,
        25.0,
        false, false
    };
}

inline SceneProfile make_scene_f35b_stovl_brake() {
    return SceneProfile{
        "f35b_stovl_brake",
        130.0, 0.0,   // mid-air to hover (rapid STOVL brake)
        0.0, 90.0,    // nacelle swings UP (F-35B = nacelle fixed ~0, lift fan on)
        100.0,         // 100 m altitude (low-level brake)
        5.0,
        0.95,
        0.05,          // CG offset (forward 5% MAC) — common in F-35B
        35.0,
        false, false
    };
}

inline SceneProfile make_scene_tiltrotor_wingborne() {
    return SceneProfile{
        "tiltrotor_wingborne",
        60.0, 280.0,   // mid-transition to wingborne (typical V-22 cruise)
        45.0, 0.0,     // 45° nacelle to 0°
        3000.0,        // 3000 m altitude
        15.0,
        1.0,
        0.0,
        20.0,
        false, false
    };
}

inline SceneProfile make_scene_emergency_single_engine() {
    return SceneProfile{
        "emergency_single_engine",
        80.0, 100.0,  // hover to slow forward, single-engine
        90.0, 60.0,   // nacelle tilts 30° (transition OR emergency)
        50.0,
        20.0,         // 20 kt crosswind
        0.85,          // lighter (fuel burned)
        -0.03,         // CG slightly aft
        15.0,          // reduced pitch rate (failure mode)
        true, false
    };
}

inline std::vector<SceneProfile> all_scenes() {
    return {
        make_scene_harrier_short_takeoff(),
        make_scene_osprey_full_tilt(),
        make_scene_f35b_stovl_brake(),
        make_scene_tiltrotor_wingborne(),
        make_scene_emergency_single_engine()
    };
}

// ============================================================================
// Per-strategy aero model
// ============================================================================

// A_PureHover: assume nacelle at 90° always, model as helicopter.
// No wing lift, all lift from disk.
inline AeroForces aero_pure_hover(const AircraftState& s, const SceneProfile& scene) {
    AeroForces f{};
    // Disk loading model: T = 2 * rho * A * v_i^2, P_ideal = T * v_i
    // For hover OGE: v_i = sqrt(T / (2*rho*A))
    // Total thrust from 2 disks must equal weight × TWR (typically 1.05-1.10 for VTOL).
    const double twr = 1.07;  // Harrier / V-22 hover TWR
    const double thrust = s.mass * kG * scene.weight_factor * twr;
    f.thrust_n = 0.0;          // no horizontal component
    f.thrust_t = thrust;       // vertical
    f.lift     = thrust;
    f.drag     = 0.0;
    f.side     = 0.0;
    // Reaction control: pure moments
    const double wind_kt = scene.wind_speed_kt;
    const double wind_ms = wind_kt * 0.514444;
    // Crosswind induces sideslip, need roll+pitch correction
    f.moment_l = 0.02 * s.inertia_xx * wind_ms;  // roll correction
    f.moment_m = 0.01 * s.inertia_yy * wind_ms;  // pitch correction
    f.moment_n = 0.005 * s.inertia_zz * wind_ms; // yaw correction
    return f;
}

// B_PureForward: assume nacelle at 0° always, model as fixed-wing.
// No hover capability, all lift from wing.
inline AeroForces aero_pure_forward(const AircraftState& s, const SceneProfile& scene) {
    AeroForces f{};
    const double v_ms = std::max(s.airspeed_kt * 0.514444, 1.0);
    const double qbar = 0.5 * kAirDensity * v_ms * v_ms;
    const double cl = s.cl_max * 0.7;  // cruise CL (clean config)
    const double cd = s.cd0 + s.k_ind * cl * cl;
    f.lift = qbar * s.wing_area * cl;
    f.drag = qbar * s.wing_area * cd;
    // Thrust along body X (forward)
    f.thrust_n = f.drag * 1.3;  // 30% thrust margin
    f.thrust_t = 0.0;
    // Pitching moment from CG offset (static stability)
    const double mac = 2.0;  // mean aerodynamic chord (m)
    const double cg_offset = scene.center_of_gravity_offset * mac;
    f.moment_m = -cl * qbar * s.wing_area * cg_offset;
    f.moment_l = 0.0;
    f.moment_n = 0.0;
    return f;
}

// C_BlendedTransition: linear blend of hover and forward aero per nacelle angle.
// nacelle_deg in [0, 90]: forward_lerp = 1 - nacelle_deg/90, hover_lerp = nacelle_deg/90
// nacelle_deg > 90: extrapolate as rearward
inline AeroForces aero_blended_transition(const AircraftState& s, const SceneProfile& scene) {
    AeroForces fh = aero_pure_hover(s, scene);
    AeroForces ff = aero_pure_forward(s, scene);
    AeroForces f{};
    // Normalize nacelle angle to [0, 1] for blend
    // 0° = full forward, 90° = full hover, 180° = full rearward
    // We use abs(nacelle - 90)/90 for forward blend, 1 - that for hover blend
    const double n = s.nacelle_deg;
    const double n_clamped = std::clamp(n, 0.0, 180.0);
    // For nacelle in [0, 90]: forward = 1 - n/90, hover = n/90
    // For nacelle in [90, 180]: forward = -(n-90)/90 (extrapolate to rearward),
    //   hover = 1 + (n-90)/90 (but capped at 1.0)
    double w_hover, w_forward;
    if (n_clamped <= 90.0) {
        w_hover = n_clamped / 90.0;
        w_forward = 1.0 - w_hover;
    } else {
        w_hover = 1.0;  // full hover behavior in rearward (nacelle past vertical)
        w_forward = 0.0;
    }
    f.lift     = fh.lift * w_hover + ff.lift * w_forward;
    f.drag     = fh.drag * w_hover + ff.drag * w_forward;
    f.thrust_n = fh.thrust_n * w_hover + ff.thrust_n * w_forward;
    f.thrust_t = fh.thrust_t * w_hover + ff.thrust_t * w_forward;
    f.moment_l = fh.moment_l * w_hover + ff.moment_l * w_forward;
    f.moment_m = fh.moment_m * w_hover + ff.moment_m * w_forward;
    f.moment_n = fh.moment_n * w_hover + ff.moment_n * w_forward;
    return f;
}

// D_BlendWithCrossover: same as C but smoothly blends thrust vector and matches
// moment crossover to prevent PIO (pilot-induced oscillation) at midpoint.
// Uses cosine smoothing (C¹) for blend, plus moment-correction term.
inline AeroForces aero_blend_with_crossover(const AircraftState& s, const SceneProfile& scene) {
    AeroForces fh = aero_pure_hover(s, scene);
    AeroForces ff = aero_pure_forward(s, scene);
    AeroForces f{};
    const double n = std::clamp(s.nacelle_deg, 0.0, 180.0);
    // Cosine smoothing: w_hover = (1 - cos(n_rad))/2 where n_rad in [0, pi]
    // This gives C¹-continuous blend with zero first derivative at 0 and pi.
    const double n_rad = (n / 180.0) * M_PI;
    const double w_hover = 0.5 * (1.0 - std::cos(n_rad));
    const double w_forward = 1.0 - w_hover;
    f.lift     = fh.lift * w_hover + ff.lift * w_forward;
    f.drag     = fh.drag * w_hover + ff.drag * w_forward;
    f.thrust_n = fh.thrust_n * w_hover + ff.thrust_n * w_forward;
    f.thrust_t = fh.thrust_t * w_hover + ff.thrust_t * w_forward;
    f.moment_l = fh.moment_l * w_hover + ff.moment_l * w_forward;
    f.moment_m = fh.moment_m * w_hover + ff.moment_m * w_forward;
    f.moment_n = fh.moment_n * w_hover + ff.moment_n * w_forward;
    // Crossover correction: ensure roll/yaw moments don't discontinuity-jump
    // at n=90 (where both models = 0). Add small matching term scaled by sin(2n).
    const double cross_term = std::sin(2.0 * n_rad);
    f.moment_l += 0.01 * s.inertia_xx * cross_term * (scene.wind_speed_kt * 0.514444);
    f.moment_n += 0.01 * s.inertia_zz * cross_term * (scene.wind_speed_kt * 0.514444);
    return f;
}

// E_PhysicsCoupledTiltRotor: full 7-DOF with nacelle as state.
// Includes: conversion corridor (limits per nacelle), tilt-pitch coupling
// (pitching moment as function of nacelle rate), engine-out asymmetric thrust.
inline AeroForces aero_physics_coupled_tilt_rotor(const AircraftState& s,
                                                 const SceneProfile& scene,
                                                 double nacelle_rate_dps) {
    AeroForces f = aero_blend_with_crossover(s, scene);
    // Conversion corridor enforcement: V-22 has 100-kt corridor per nacelle angle.
    // Outside corridor, force linear interpolation toward safer (higher nacelle) mode.
    // Corridor model: max speed at full forward = 250 kt, at 45° = 150 kt, at 90° = 0 kt.
    const double n = std::clamp(s.nacelle_deg, 0.0, 180.0);
    const double corridor_speed_kt = 250.0 * std::cos(n / 90.0 * M_PI / 2.0);
    if (s.airspeed_kt > corridor_speed_kt + 10.0) {
        // Outside corridor high side: reduce lift (prevent wing stall)
        f.lift *= corridor_speed_kt / std::max(s.airspeed_kt, 1.0);
    }
    // Tilt-pitch coupling: as nacelle rotates forward, CG shifts moment arm changes.
    // dM/d(nacelle_rate) ∝ nacelle_rate × (cg offset)
    const double cg_offset = scene.center_of_gravity_offset * 2.0;  // MAC = 2m
    f.moment_m += 0.02 * nacelle_rate_dps * cg_offset * s.inertia_yy;
    // Engine-out asymmetric thrust: 1 engine at 100%, 1 at 0%.
    // Produces large yawing moment.
    if (scene.engine_out) {
        const double thrust_per_engine = (f.thrust_n + f.thrust_t) * 0.5;
        f.moment_n += thrust_per_engine * (s.wing_span * 0.5);  // wing-tip moment arm
        // Also reduce total thrust by ~40% (V-22 cannot hover on 1 engine)
        f.thrust_n *= 0.4;
        f.thrust_t *= 0.4;
        f.lift *= 0.4;
    }
    return f;
}

// ============================================================================
// Strategy tick: advance nacelle + compute aero + update state
// ============================================================================

// Returns: time taken to compute one tick (nanoseconds), plus whether physics
// is plausible (no NaN, no out-of-bounds).
struct TickResult {
    double elapsed_ns{0.0};
    bool plausible{true};
    double new_nacelle_deg{0.0};
    double pitch_overshoot_deg{0.0};
};

inline TickResult tick_strategy(Strategy strat,
                                 const SceneProfile& scene,
                                 uint64_t seed,
                                 double tick_dt_s) {
    // Deterministic per-seed initialization
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> jitter(-0.05, 0.05);

    AircraftState s;
    s.airspeed_kt = scene.initial_airspeed_kt * (1.0 + jitter(rng));
    s.nacelle_deg = scene.initial_nacelle_deg * (1.0 + jitter(rng) * 0.02);
    s.mass = 6800.0 * scene.weight_factor;
    s.altitude_m = scene.initial_altitude_m;

    // Conversion command: linearly interpolate nacelle from initial to final over
    // scene duration (proportional to airspeed change rate).
    [[maybe_unused]] const double duration_s = 30.0;  // 30 sec full transition (typical V-22 STO/SRVL)
    const double dt = tick_dt_s;

    auto t0 = std::chrono::steady_clock::now();
    [[maybe_unused]] auto _t0_unused = t0;
    bool plausible = true;
    double max_pitch_overshoot = 0.0;

    // Simulate N substeps to advance nacelle + airspeed
    const int N_substeps = 10;
    const double substep_dt = dt / N_substeps;
    for (int i = 0; i < N_substeps; ++i) {
        const double t_norm = (i + 0.5) / N_substeps;  // midpoint of substep
        // Commanded nacelle angle
        const double target_nacelle = scene.initial_nacelle_deg
            + (scene.final_nacelle_deg - scene.initial_nacelle_deg) * t_norm;
        // Rate of change (deg/s) - limited by physical conversion rate
        const double desired_rate = (target_nacelle - s.nacelle_deg) / substep_dt;
        const double max_rate = s.conversion_rate_dps;
        const double applied_rate = std::clamp(desired_rate, -max_rate, max_rate);
        s.nacelle_deg += applied_rate * substep_dt;
        // Clamp to physical limits
        if (s.nacelle_deg < 0.0) s.nacelle_deg = 0.0;
        if (s.nacelle_deg > kV22RearwardDeg) s.nacelle_deg = kV22RearwardDeg;
        // Airspeed evolves
        const double target_airspeed = scene.initial_airspeed_kt
            + (scene.final_airspeed_kt - scene.initial_airspeed_kt) * t_norm;
        s.airspeed_kt = target_airspeed * (1.0 + jitter(rng) * 0.01);
        // Compute aero
        AeroForces f{};
        switch (strat) {
            case Strategy::A_PureHover:
                f = aero_pure_hover(s, scene);
                // Pure hover can't accelerate beyond ~30 kt; cap
                s.airspeed_kt = std::min(s.airspeed_kt, 30.0);
                break;
            case Strategy::B_PureForward:
                f = aero_pure_forward(s, scene);
                // Pure forward can't decelerate below stall (assume stall = 60 kt)
                s.airspeed_kt = std::max(s.airspeed_kt, 60.0);
                break;
            case Strategy::C_BlendedTransition:
                f = aero_blended_transition(s, scene);
                break;
            case Strategy::D_BlendWithCrossover:
                f = aero_blend_with_crossover(s, scene);
                break;
            case Strategy::E_PhysicsCoupledTiltRotor:
                f = aero_physics_coupled_tilt_rotor(s, scene, applied_rate);
                break;
        }
        // Update state (simplified 6-DOF: roll + pitch only)
        // Vertical force balance: f.thrust_t + f.lift - mass*g = mass*az
        [[maybe_unused]] const double vert_acc = (f.thrust_t + f.lift - s.mass * kG) / s.mass;
        // Pitch moment -> angular acc
        const double pitch_acc = f.moment_m / s.inertia_yy;
        const double pitch_deg = s.theta * 180.0 / M_PI;
        const double pitch_overshoot = std::abs(pitch_deg - 0.0);
        if (pitch_overshoot > max_pitch_overshoot) {
            max_pitch_overshoot = pitch_overshoot;
        }
        // Sanity check
        if (std::isnan(f.lift) || std::isnan(f.drag) || std::isnan(f.moment_m)) {
            plausible = false;
        }
        // Update theta (simplified - actual integration would use quaternion)
        s.theta += pitch_acc * substep_dt;
        s.theta = std::clamp(s.theta, -0.5, 0.5);
        // Limit pitch rate
        if (std::abs(pitch_acc) > scene.max_pitch_rate_dps * M_PI / 180.0) {
            plausible = false;  // PIO / structural limit exceeded
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    TickResult res;
    res.elapsed_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    res.plausible = plausible;
    res.new_nacelle_deg = s.nacelle_deg;
    res.pitch_overshoot_deg = max_pitch_overshoot;
    return res;
}

// ============================================================================
// Stats
// ============================================================================

struct Stats {
    double mean{0.0};
    double median{0.0};
    double p95{0.0};
    double p99{0.0};
    double stddev{0.0};
    double minv{0.0};
    double maxv{0.0};
    double n{0};
};

inline Stats compute_stats(const std::vector<double>& samples) {
    Stats s{};
    if (samples.empty()) return s;
    s.n = static_cast<double>(samples.size());
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    s.minv = sorted.front();
    s.maxv = sorted.back();
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    return s;
}

}  // namespace vtol

// ============================================================================
// Main
// ============================================================================

int main(int /*argc*/, char** /*argv*/) {
    using namespace vtol;
    const int N_iter = 1000;
    const int N_warmup = 10;
    const std::vector<uint64_t> seeds = {1, 7, 42, 1234, 31337};
    const double tick_dt_s = 1.0 / kDefaultTickHz;  // 33.3 ms tick

    // Open CSV
    const std::string csv_path = "build/results.csv";
    std::ofstream csv(csv_path);
    if (!csv) {
        std::fprintf(stderr, "ERROR: cannot open %s\n", csv_path.c_str());
        return 1;
    }
    csv << "strategy,scene,seed,n_iter,n_warmup,mean_ns,median_ns,p95_ns,p99_ns,"
           "stddev_ns,min_ns,max_ns,plausible_frac,pitch_overshoot_max_deg,"
           "theoretical_max_crafts_at_30hz\n";

    // Strategies and scenes
    const std::vector<Strategy> strategies = {
        Strategy::A_PureHover,
        Strategy::B_PureForward,
        Strategy::C_BlendedTransition,
        Strategy::D_BlendWithCrossover,
        Strategy::E_PhysicsCoupledTiltRotor,
    };
    const auto scenes = all_scenes();

    // Total configurations
    const size_t total_configs = strategies.size() * scenes.size() * seeds.size();
    size_t config_idx = 0;

    for (Strategy strat : strategies) {
        for (const auto& scene : scenes) {
            for (uint64_t seed : seeds) {
                ++config_idx;
                // Warmup
                std::vector<double> samples;
                samples.reserve(N_iter);
                int plausible_count = 0;
                double max_overshoot = 0.0;
                for (int i = 0; i < N_warmup; ++i) {
                    (void)tick_strategy(strat, scene, seed, tick_dt_s);
                }
                // Main measurements
                for (int i = 0; i < N_iter; ++i) {
                    TickResult r = tick_strategy(strat, scene, seed, tick_dt_s);
                    samples.push_back(r.elapsed_ns);
                    if (r.plausible) plausible_count++;
                    if (r.pitch_overshoot_deg > max_overshoot) {
                        max_overshoot = r.pitch_overshoot_deg;
                    }
                }
                Stats s = compute_stats(samples);
                const double plausible_frac = static_cast<double>(plausible_count) / N_iter;
                // Theoretical max craft count at 30 Hz = (tick_dt / mean_ns) * 1e9
                const double mean_ns = std::max(s.mean, 1.0);
                const double theoretical_max = (tick_dt_s * 1e9) / mean_ns;
                csv << strategy_name(strat) << ","
                    << scene.name << ","
                    << seed << ","
                    << N_iter << ","
                    << N_warmup << ","
                    << s.mean << ","
                    << s.median << ","
                    << s.p95 << ","
                    << s.p99 << ","
                    << s.stddev << ","
                    << s.minv << ","
                    << s.maxv << ","
                    << plausible_frac << ","
                    << max_overshoot << ","
                    << theoretical_max << "\n";
                csv.flush();
                // Progress
                std::printf("[%3zu/%3zu] %-25s | %-30s | seed=%5lu | "
                            "mean=%8.1f ns | p95=%8.1f ns | p99=%8.1f ns | "
                            "plausible=%.0f%% | max craft @30Hz=%.1f\n",
                            config_idx, total_configs,
                            strategy_name(strat), scene.name,
                            static_cast<unsigned long>(seed),
                            s.mean, s.p95, s.p99,
                            plausible_frac * 100.0, theoretical_max);
            }
        }
    }
    std::printf("\nDone. Results: %s\n", csv_path.c_str());
    csv.close();
    return 0;
}
