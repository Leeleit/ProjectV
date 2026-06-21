#include <algorithm>
#include <array>
#include <bit>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <numbers>
#include <random>
#include <span>
#include <vector>

// === Constants ===
constexpr double kGravity = 9.80665;     // m/s^2
constexpr double kAirDensity = 1.225;     // kg/m^3 at sea level
constexpr double kTickSeconds = 1.0 / 60.0; // 60 Hz physics tick

// === Projectile types ===
enum class ProjectileType : uint8_t { AP, APCBC, HEAT, HE, APCR };

struct ProjectileDef {
    double caliber_mm;     // mm
    double mass_kg;        // kg
    double muzzle_velocity; // m/s
    double drag_coeff;     // Cd (0 = vacuum)
    double explosive_kg;   // HE filler mass, kg
    ProjectileType type;
    const char* name;
};

// === Reference projectiles (historical data for DeMarre) ===
constexpr std::array kProjectiles = {
    ProjectileDef{ .caliber_mm = 75,  .mass_kg = 6.3,   .muzzle_velocity = 780.0, .drag_coeff = 0.35, .explosive_kg = 0.0, .type = ProjectileType::AP,    .name = "75mm_AP" },
    ProjectileDef{ .caliber_mm = 88,  .mass_kg = 10.2,  .muzzle_velocity = 810.0, .drag_coeff = 0.32, .explosive_kg = 0.1, .type = ProjectileType::APCBC, .name = "88mm_APCBC" },
    ProjectileDef{ .caliber_mm = 105, .mass_kg = 15.1,  .muzzle_velocity = 730.0, .drag_coeff = 0.30, .explosive_kg = 0.2, .type = ProjectileType::AP,    .name = "105mm_AP" },
    ProjectileDef{ .caliber_mm = 120, .mass_kg = 22.0,  .muzzle_velocity = 800.0, .drag_coeff = 0.28, .explosive_kg = 0.3, .type = ProjectileType::APCBC, .name = "120mm_APCBC" },
    ProjectileDef{ .caliber_mm = 152, .mass_kg = 43.5,  .muzzle_velocity = 655.0, .drag_coeff = 0.27, .explosive_kg = 5.3, .type = ProjectileType::HE,    .name = "152mm_HE" },
    ProjectileDef{ .caliber_mm = 90,  .mass_kg = 3.9,   .muzzle_velocity = 1020.0,.drag_coeff = 0.25, .explosive_kg = 0.0, .type = ProjectileType::APCR,  .name = "90mm_APCR" },
    ProjectileDef{ .caliber_mm = 100, .mass_kg = 15.8,  .muzzle_velocity = 900.0, .drag_coeff = 0.29, .explosive_kg = 0.0, .type = ProjectileType::AP,    .name = "100mm_AP" },
    ProjectileDef{ .caliber_mm = 122, .mass_kg = 25.0,  .muzzle_velocity = 780.0, .drag_coeff = 0.28, .explosive_kg = 3.5, .type = ProjectileType::HE,    .name = "122mm_HE" },
};

// === Active projectile state ===
struct Projectile {
    double px, py, pz;      // position (m)
    double vx, vy, vz;      // velocity (m/s)
    ProjectileDef def;
    double flight_time;     // seconds since fired
    bool active;
};

struct Impact {
    double x, y, z;         // impact position
    double velocity;        // impact velocity (m/s)
    double angle;           // impact angle from normal (degrees)
    double penetration_mm;  // computed penetration
    ProjectileType type;
};

// === Penetration models ===

// DeMarre formula for AP/APC/APCBC/APCR
// P = P_ref * (V/V_ref)^1.4283 * (C/C_ref)^1.0714 * (M/C³)^0.7143 / (M_ref/C_ref³)^0.7143
double demarre_penetration(double velocity, double caliber_mm, double mass_kg,
                           double ref_velocity, double ref_caliber, double ref_mass, double ref_pen_mm) {
    double v_ratio = velocity / ref_velocity;
    double c_ratio = caliber_mm / ref_caliber;
    double m_norm = mass_kg / (caliber_mm * caliber_mm * caliber_mm);
    double ref_norm = ref_mass / (ref_caliber * ref_caliber * ref_caliber);
    double p = ref_pen_mm *
        std::pow(v_ratio, 1.4283) *
        std::pow(c_ratio, 1.0714) *
        std::pow(m_norm / ref_norm, 0.7143);
    return p;
}

// Krupp formula (simpler, for ballpark): P = 100 * V * sqrt(M) / (2400 * sqrt(C/100))
double krupp_penetration(double velocity, double mass_kg, double caliber_mm) {
    return 100.0 * velocity * std::sqrt(mass_kg) / (2400.0 * std::sqrt(caliber_mm / 100.0));
}

// Angle modifier: effective thickness = t / cos(angle_normal)
// DeMarre oblique: multiply by cos(angle)^1.2 for AP
double angle_modifier_deMarre(double angle_deg) {
    double rad = angle_deg * std::numbers::pi / 180.0;
    return std::pow(std::cos(rad), 1.2);
}

// === Ballistic integration (RK4) ===
struct BallisticState {
    double px, py, pz;
    double vx, vy, vz;
};

BallisticState ballistic_deriv(const BallisticState& s, const ProjectileDef& def) {
    double speed = std::sqrt(s.vx * s.vx + s.vy * s.vy + s.vz * s.vz);
    // Drag force: F_drag = 0.5 * rho * v^2 * Cd * A
    // A = pi * (d/2)^2, d in meters
    double d_m = def.caliber_mm / 1000.0;
    double area = std::numbers::pi * d_m * d_m * 0.25;
    double drag_mag = 0.5 * kAirDensity * speed * speed * def.drag_coeff * area / def.mass_kg;

    BallisticState ds{};
    ds.px = s.vx;
    ds.py = s.vy;
    ds.pz = s.vz;
    if (speed > 1e-6) {
        ds.vx = -drag_mag * s.vx / speed;
        ds.vy = -drag_mag * s.vy / speed;
        ds.vz = -drag_mag * s.vz / speed - kGravity;
    } else {
        ds.vz = -kGravity;
    }
    return ds;
}

void rk4_step(BallisticState& s, const ProjectileDef& def, double dt) {
    auto k1 = ballistic_deriv(s, def);
    BallisticState s2{s.px + 0.5 * dt * k1.px, s.py + 0.5 * dt * k1.py, s.pz + 0.5 * dt * k1.pz,
                      s.vx + 0.5 * dt * k1.vx, s.vy + 0.5 * dt * k1.vy, s.vz + 0.5 * dt * k1.vz};
    auto k2 = ballistic_deriv(s2, def);
    BallisticState s3{s.px + 0.5 * dt * k2.px, s.py + 0.5 * dt * k2.py, s.pz + 0.5 * dt * k2.pz,
                      s.vx + 0.5 * dt * k2.vx, s.vy + 0.5 * dt * k2.vy, s.vz + 0.5 * dt * k2.vz};
    auto k3 = ballistic_deriv(s3, def);
    BallisticState s4{s.px + dt * k3.px, s.py + dt * k3.py, s.pz + dt * k3.pz,
                      s.vx + dt * k3.vx, s.vy + dt * k3.vy, s.vz + dt * k3.vz};
    auto k4 = ballistic_deriv(s4, def);

    s.px += (dt / 6.0) * (k1.px + 2.0 * k2.px + 2.0 * k3.px + k4.px);
    s.py += (dt / 6.0) * (k1.py + 2.0 * k2.py + 2.0 * k3.py + k4.py);
    s.pz += (dt / 6.0) * (k1.pz + 2.0 * k2.pz + 2.0 * k3.pz + k4.pz);
    s.vx += (dt / 6.0) * (k1.vx + 2.0 * k2.vx + 2.0 * k3.vx + k4.vx);
    s.vy += (dt / 6.0) * (k1.vy + 2.0 * k2.vy + 2.0 * k3.vy + k4.vy);
    s.vz += (dt / 6.0) * (k1.vz + 2.0 * k2.vz + 2.0 * k3.vz + k4.vz);
}

// === Ballistic Table (5D precomputed) ===
// Dimensions: caliber_idx x velocity_bucket x angle_bucket x range_bucket x mass_bucket
// For prototype: 2^3 per dim = 243 entries per projectile type, linear interp

struct BallisticTable {
    static constexpr int kCaliberSteps = 4;
    static constexpr int kVelocitySteps = 8;
    static constexpr int kAngleSteps = 6;
    static constexpr int kRangeSteps = 4;
    static constexpr int kTotalEntries = kCaliberSteps * kVelocitySteps * kAngleSteps * kRangeSteps;

    std::array<double, kTotalEntries> penetration_mm{};

    static int idx(int ci, int vi, int ai, int ri) {
        return ((ci * kVelocitySteps + vi) * kAngleSteps + ai) * kRangeSteps + ri;
    }

    void build(const std::vector<ProjectileDef>& defs) {
        // Sample: for each projectile type, precompute penetration at grid points
        for (size_t ci = 0; ci < defs.size() && ci < (size_t)kCaliberSteps; ++ci) {
            auto& def = defs[ci];
            for (int vi = 0; vi < kVelocitySteps; ++vi) {
                double vel = def.muzzle_velocity * (0.3 + 0.7 * vi / (kVelocitySteps - 1));
                for (int ai = 0; ai < kAngleSteps; ++ai) {
                    double angle = 30.0 * ai / (kAngleSteps - 1);
                    for (int ri = 0; ri < kRangeSteps; ++ri) {
                        double range_factor = 1.0 - 0.5 * ri / (kRangeSteps - 1); // velocity loss proxy
                        double hit_vel = vel * range_factor;
                        double p = demarre_penetration(
                            hit_vel, def.caliber_mm, def.mass_kg,
                            800.0, 88.0, 10.2, 180.0); // ref: 88mm APCBC
                        p *= angle_modifier_deMarre(angle);
                        penetration_mm[idx((int)ci, vi, ai, ri)] = p;
                    }
                }
            }
        }
    }

    double lookup(int ci, double velocity, double angle_deg, double range_m) const {
        // Clamp + linear interpolation (simplified: nearest neighbor for speed)
        int vi = std::clamp((int)(kVelocitySteps * (velocity / 1000.0)), 0, kVelocitySteps - 1);
        int ai = std::clamp((int)(kAngleSteps * angle_deg / 30.0), 0, kAngleSteps - 1);
        int ri = std::clamp((int)(kRangeSteps * (1.0 - range_m / 2000.0)), 0, kRangeSteps - 1);
        ci = std::clamp(ci, 0, kCaliberSteps - 1);
        return penetration_mm[idx(ci, vi, ai, ri)];
    }
};

// === Scene definitions ===
struct Scene {
    const char* name;
    int num_projectiles;       // active projectiles per tick
    double duration_seconds;   // simulation duration
    double spread_degrees;     // aiming spread
    bool moving_targets;       // target movement
};

constexpr std::array kScenes = {
    Scene{ "duel",          10,   10.0, 0.5,  false },
    Scene{ "squad",         50,   15.0, 1.0,  false },
    Scene{ "platoon",       200,  20.0, 1.5,  true  },
    Scene{ "company",       500,  30.0, 2.0,  true  },
    Scene{ "bombardment",   1000, 60.0, 5.0,  false },
};

// === Strategy interface ===
struct StrategyResult {
    const char* name;
    double total_time_us;
    double pSNR;              // vs reference (RK4 numerical integration)
    double penetration_accuracy; // mean penetration error vs reference
};

// === Strategy implementations ===

// A: Baseline hit-scan (no ballistics)
double strategy_A_baseline(std::span<const Projectile> projs, std::vector<Impact>& impacts, int tick) {
    auto start = __builtin_readcyclecounter();
    impacts.clear();
    for (auto& p : projs) {
        if (!p.active) continue;
        // Hit-scan: instant hit at aim point, distance 500-1500m
        Impact imp;
        imp.x = p.px + 1000.0;
        imp.y = p.py;
        imp.z = p.pz - 1.0;
        imp.velocity = p.def.muzzle_velocity;
        imp.angle = 10.0;
        imp.penetration_mm = demarre_penetration(p.def.muzzle_velocity, p.def.caliber_mm,
            p.def.mass_kg, 800.0, 88.0, 10.2, 180.0);
        imp.type = p.def.type;
        impacts.push_back(imp);
    }
    auto end = __builtin_readcyclecounter();
    return (double)(end - start) / 2.5; // approximate ns from cycles (2.5 GHz Zen 3)
}

// B: Precomputed ballistic table lookup
double strategy_B_table_lookup(std::span<const Projectile> projs, std::vector<Impact>& impacts,
                                const BallisticTable& table, int tick) {
    auto start = __builtin_readcyclecounter();
    impacts.clear();
    for (auto& p : projs) {
        if (!p.active) continue;
        // Compute time of flight estimate from range
        double range = 500.0 + 1000.0 * ((double)(p.px * 123 + tick) / 100000.0);
        range = std::fmod(range, 2000.0);
        double vel_at_impact = p.def.muzzle_velocity * (0.6 + 0.4 * (1.0 - range / 2000.0));

        Impact imp;
        imp.x = p.px + (float)range;
        imp.y = p.py;
        imp.z = p.pz - 0.5;
        imp.velocity = vel_at_impact;
        imp.angle = 10.0 + 20.0 * (range / 2000.0); // angle increases with range
        imp.penetration_mm = table.lookup(0, vel_at_impact, imp.angle, range);
        imp.type = p.def.type;
        impacts.push_back(imp);
    }
    auto end = __builtin_readcyclecounter();
    return (double)(end - start) / 2.5;
}

// C: RK4 numerical integration per projectile per tick
double strategy_C_numint_rk4(std::span<Projectile> projs, std::vector<Impact>& impacts,
                              double dt, int max_ticks, int tick) {
    auto start = __builtin_readcyclecounter();
    impacts.clear();
    for (auto& p : projs) {
        if (!p.active) continue;

        // Fire projectile if not already in flight (first tick)
        if (p.flight_time < 1e-6) {
            // Random direction within spread
            double theta = 15.0 * std::numbers::pi / 180.0; // 15 degree elevation
            double phi = 0.0;
            p.vx = p.def.muzzle_velocity * std::cos(theta) * std::cos(phi);
            p.vy = p.def.muzzle_velocity * std::cos(theta) * std::sin(phi);
            p.vz = p.def.muzzle_velocity * std::sin(theta);
        }

        // RK4 step
        BallisticState bs{p.px, p.py, p.pz, p.vx, p.vy, p.vz};
        rk4_step(bs, p.def, dt);
        p.px = bs.px; p.py = bs.py; p.pz = bs.pz;
        p.vx = bs.vx; p.vy = bs.vy; p.vz = bs.vz;
        p.flight_time += dt;

        // Ground impact check
        if (p.pz <= 0.0 || p.flight_time > 10.0) {
            double speed = std::sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz);
            double angle = std::atan2(std::abs(p.vz), std::sqrt(p.vx * p.vx + p.vy * p.vy)) * 180.0 / std::numbers::pi;
            Impact imp;
            imp.x = p.px; imp.y = p.py; imp.z = 0.0;
            imp.velocity = speed;
            imp.angle = 90.0 - angle; // angle from vertical
            imp.penetration_mm = demarre_penetration(speed, p.def.caliber_mm, p.def.mass_kg,
                800.0, 88.0, 10.2, 180.0);
            imp.penetration_mm *= angle_modifier_deMarre(imp.angle);
            imp.type = p.def.type;
            impacts.push_back(imp);
            p.active = false;
        }
    }
    auto end = __builtin_readcyclecounter();
    return (double)(end - start) / 2.5;
}

// D: Table lookup + GPU particle proxy cost
double strategy_D_table_particle(std::span<const Projectile> projs, std::vector<Impact>& impacts,
                                  const BallisticTable& table, int tick, int num_visible) {
    auto start = __builtin_readcyclecounter();
    // Same as B but add GPU particle cost model
    double cpu_cost = strategy_B_table_lookup(projs, impacts, table, tick);

    // GPU particle proxy cost: 
    // Indirect draw dispatch overhead ~0.01 ms + 0.0001 ms per particle
    double gpu_cost_ns = 10000.0 + 0.1 * num_visible;

    return cpu_cost + gpu_cost_ns;
}

// E: Hybrid — table flight path + analytic aim prediction
double strategy_E_hybrid_aim(std::span<const Projectile> projs, std::vector<Impact>& impacts,
                              std::vector<double>& aim_predictions, const BallisticTable& table, int tick) {
    auto start = __builtin_readcyclecounter();
    impacts.clear();
    aim_predictions.clear();

    for (auto& p : projs) {
        if (!p.active) continue;

        // Compute time of flight via ballistic table
        double range = 500.0 + 1000.0 * ((double)(p.px * 456 + tick * 789) / 100000.0);
        range = std::fmod(range, 2000.0);
        double vel_at_impact = p.def.muzzle_velocity * (0.6 + 0.4 * (1.0 - range / 2000.0));
        double tof_estimate = range / (0.5 * (p.def.muzzle_velocity + vel_at_impact));

        // Analytic aim prediction: lead = target_velocity * tof
        // For static targets: aim = direct
        double target_speed = 0.0; // static in most scenes
        double lead_x = target_speed * tof_estimate;
        double aim_x = p.px + range + lead_x;

        Impact imp;
        imp.x = aim_x;
        imp.y = p.py;
        imp.z = p.pz - 0.5;
        imp.velocity = vel_at_impact;
        imp.angle = 10.0 + 20.0 * (range / 2000.0);
        imp.penetration_mm = table.lookup(0, vel_at_impact, imp.angle, range);
        imp.type = p.def.type;
        impacts.push_back(imp);
        aim_predictions.push_back(lead_x);
    }
    auto end = __builtin_readcyclecounter();
    return (double)(end - start) / 2.5;
}

// === PSNR computation ===
double compute_psnr(const std::vector<Impact>& ref, const std::vector<Impact>& test) {
    if (ref.empty() || test.empty()) return 0.0;
    double mse = 0.0;
    size_t n = std::min(ref.size(), test.size());
    for (size_t i = 0; i < n; ++i) {
        double err = ref[i].penetration_mm - test[i].penetration_mm;
        mse += err * err;
    }
    mse /= n;
    if (mse < 1e-12) return 100.0;
    double max_val = 300.0; // max penetration mm
    return 10.0 * std::log10((max_val * max_val) / mse);
}

// === Main benchmark harness ===
int main(int argc, char** argv) {
    // Parse args
    int num_iter = 1000;
    int num_warmup = 10;
    int seed = 42;
    if (argc > 1) num_iter = std::atoi(argv[1]);
    if (argc > 2) num_warmup = std::atoi(argv[2]);
    if (argc > 3) seed = std::atoi(argv[3]);

    // Build ballistic table
    BallisticTable table;
    table.build(std::vector<ProjectileDef>(kProjectiles.begin(), kProjectiles.end()));

    // CSV header
    std::printf("strategy,scene,seed,iter,time_us,psnr\n");

    // Pre-allocate
    std::vector<Projectile> projs;
    std::vector<Impact> impacts, ref_impacts;
    std::vector<double> aim_predictions;

    for (auto& scene : kScenes) {
        for (int s = 0; s < 5; ++s) {
            int cur_seed = seed + s * 1000;
            std::mt19937 rng(cur_seed);

            // === Warmup ===
            for (int w = 0; w < num_warmup; ++w) {
                // Generate projectiles for this tick
                projs.clear();
                int num_new = scene.num_projectiles;
                for (int i = 0; i < num_new; ++i) {
                    auto& def = kProjectiles[rng() % kProjectiles.size()];
                    projs.push_back(Projectile{
                        .px = (double)(rng() % 100) * 10.0,
                        .py = (double)(rng() % 100) * 10.0,
                        .pz = 10.0,
                        .vx = 0, .vy = 0, .vz = 0,
                        .def = def,
                        .flight_time = 0.0,
                        .active = true,
                    });
                }
                int tick = w;
                strategy_C_numint_rk4(projs, ref_impacts, kTickSeconds, 600, tick);
            }

            // === Main measurements ===
            for (int iter = 0; iter < num_iter; ++iter) {
                int tick = iter;

                // Generate projectiles for this tick
                projs.clear();
                int num_new = scene.num_projectiles;
                for (int i = 0; i < num_new; ++i) {
                    auto& def = kProjectiles[rng() % kProjectiles.size()];
                    projs.push_back(Projectile{
                        .px = (double)(rng() % 100) * 10.0,
                        .py = (double)(rng() % 100) * 10.0,
                        .pz = 10.0,
                        .vx = 0, .vy = 0, .vz = 0,
                        .def = def,
                        .flight_time = 0.0,
                        .active = true,
                    });
                }

                // Run reference (RK4)
                auto projs_copy = projs;
                ref_impacts.clear();
                strategy_C_numint_rk4(projs_copy, ref_impacts, kTickSeconds, 600, tick);

                // Strategy A: baseline hit-scan
                {
                    auto projs_a = projs;
                    std::vector<Impact> imp_a;
                    double t = strategy_A_baseline(projs_a, imp_a, tick);
                    double psnr = compute_psnr(ref_impacts, imp_a);
                    std::printf("A_Baseline,%s,%d,%d,%.1f,%.3f\n", scene.name, cur_seed, tick, t, psnr);
                }

                // Strategy B: table lookup
                {
                    auto projs_b = projs;
                    std::vector<Impact> imp_b;
                    double t = strategy_B_table_lookup(projs_b, imp_b, table, tick);
                    double psnr = compute_psnr(ref_impacts, imp_b);
                    std::printf("B_TableLookup,%s,%d,%d,%.1f,%.3f\n", scene.name, cur_seed, tick, t, psnr);
                }

                // Strategy C: RK4 (already computed as reference)
                {
                    double t = strategy_C_numint_rk4(projs, ref_impacts, kTickSeconds, 600, tick);
                    std::printf("C_NumIntRK4,%s,%d,%d,%.1f,100.000\n", scene.name, cur_seed, tick, t);
                }

                // Strategy D: Table + GPU particle
                {
                    auto projs_d = projs;
                    std::vector<Impact> imp_d;
                    double t = strategy_D_table_particle(projs_d, imp_d, table, tick, scene.num_projectiles);
                    double psnr = compute_psnr(ref_impacts, imp_d);
                    std::printf("D_TableParticle,%s,%d,%d,%.1f,%.3f\n", scene.name, cur_seed, tick, t, psnr);
                }

                // Strategy E: Hybrid aim prediction
                {
                    auto projs_e = projs;
                    std::vector<Impact> imp_e;
                    std::vector<double> aims;
                    double t = strategy_E_hybrid_aim(projs_e, imp_e, aims, table, tick);
                    double psnr = compute_psnr(ref_impacts, imp_e);
                    std::printf("E_HybridAimPred,%s,%d,%d,%.1f,%.3f\n", scene.name, cur_seed, tick, t, psnr);
                }
            }
        }
    }

    return 0;
}
