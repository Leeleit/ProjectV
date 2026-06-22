#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <random>
#include <fstream>
#include <string>
#include <algorithm>
#include <memory>
#include <iomanip>

// Simple 3D Vector structure
struct Vec3 {
    double x, y, z;

    constexpr Vec3() : x(0.0), y(0.0), z(0.0) {}
    constexpr Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

    constexpr Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    constexpr Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    constexpr Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }
    constexpr Vec3 operator/(double s) const { return Vec3(x / s, y / s, z / s); }

    constexpr double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    constexpr Vec3 cross(const Vec3& o) const {
        return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
    }

    double lengthSq() const { return x * x + y * y + z * z; }
    double length() const { return std::sqrt(lengthSq()); }

    Vec3 normalized() const {
        double len = length();
        if (len < 1e-9) return Vec3(0, 0, 0);
        return *this / len;
    }
};

enum class GuidanceLaw {
    CLOS,
    PurePursuit,
    ConstantPN,
    AdaptivePN,
    AugmentedPN
};

enum class ScenarioType {
    StaticTarget,
    LinearTarget,
    ManeuveringTarget,
    Countermeasures,
    MultipleMissiles
};

std::string toString(GuidanceLaw law) {
    switch (law) {
        case GuidanceLaw::CLOS: return "CLOS";
        case GuidanceLaw::PurePursuit: return "PurePursuit";
        case GuidanceLaw::ConstantPN: return "ConstantPN";
        case GuidanceLaw::AdaptivePN: return "AdaptivePN";
        case GuidanceLaw::AugmentedPN: return "AugmentedPN";
    }
    return "Unknown";
}

std::string toString(ScenarioType sc) {
    switch (sc) {
        case ScenarioType::StaticTarget: return "StaticTarget";
        case ScenarioType::LinearTarget: return "LinearTarget";
        case ScenarioType::ManeuveringTarget: return "ManeuveringTarget";
        case ScenarioType::Countermeasures: return "Countermeasures";
        case ScenarioType::MultipleMissiles: return "MultipleMissiles";
    }
    return "Unknown";
}

// Physical structures
struct TargetState {
    Vec3 pos;
    Vec3 vel;
    Vec3 acc;
};

struct DecoyState {
    bool active = false;
    Vec3 pos;
    Vec3 vel;
    double intensity = 0.0;
};

struct MissileState {
    Vec3 pos;
    Vec3 vel;
    double mass = 80.0;
    double time = 0.0;
    double last_lat_acc = 0.0;
};

// Simulation settings
struct SimParams {
    double dt = 0.01; // 100 Hz simulation tick
    double max_time = 15.0; // max 15 seconds flight
    double max_lat_g = 35.0; // max 35G lateral acceleration limit
    double missile_drag_k = 0.001; // aerodynamic drag scaling
    double missile_thrust = 20000.0; // motor thrust in Newtons
    double motor_burn_time = 3.0; // active burn duration
    double missile_empty_mass = 50.0;
    double fuel_mass = 30.0;
    double seeker_fov = 10.0 * M_PI / 180.0; // 10 degrees half-angle seeker FOV
    double seeker_noise = 0.002; // angular measurement noise standard deviation
};

// Global parameters instance
const SimParams g_params;

// Seeker measurement function
Vec3 getSeekerMeasurement(const MissileState& missile, const TargetState& target, const DecoyState& decoy,
                          bool has_eccm, std::mt19937& rng, std::normal_distribution<double>& dist) {
    Vec3 r_target = target.pos - missile.pos;
    double dist_target = r_target.length();
    Vec3 dir_target = r_target.normalized();

    // Check if decoy flare is deployed and inside FOV
    bool lock_on_decoy = false;
    if (decoy.active && decoy.intensity > 0.0) {
        Vec3 r_decoy = decoy.pos - missile.pos;
        double dist_decoy = r_decoy.length();
        Vec3 dir_decoy = r_decoy.normalized();

        Vec3 missile_dir = missile.vel.normalized();
        double cos_angle_target = missile_dir.dot(dir_target);
        double cos_angle_decoy = missile_dir.dot(dir_decoy);

        double limit_cos = std::cos(g_params.seeker_fov);

        if (cos_angle_decoy > limit_cos && cos_angle_target > limit_cos) {
            // Decoy flare is in seeker FOV
            if (has_eccm) {
                // ECCM evaluates relative kinematics
                // Decoy flare decelerates extremely fast due to drag (simulated by rapid velocity vector change)
                Vec3 rel_vel_decoy = decoy.vel - missile.vel;
                Vec3 los_rate_decoy = r_decoy.cross(rel_vel_decoy) / r_decoy.lengthSq();
                double los_rate_mag = los_rate_decoy.length();

                // If decoy angular rate is too high (rapid drop-away), seeker filters it
                if (los_rate_mag < 1.2) {
                    // Decide based on intensity relative to distance-squared signal strength
                    double sig_target = 1.0 / (dist_target * dist_target);
                    double sig_decoy = decoy.intensity / (dist_decoy * dist_decoy);
                    if (sig_decoy > sig_target * 1.5) {
                        lock_on_decoy = true;
                    }
                }
            } else {
                // No ECCM: simple signal-to-noise lock on strongest emitter
                double sig_target = 1.0 / (dist_target * dist_target);
                double sig_decoy = decoy.intensity / (dist_decoy * dist_decoy);
                if (sig_decoy > sig_target) {
                    lock_on_decoy = true;
                }
            }
        }
    }

    Vec3 true_dir = lock_on_decoy ? (decoy.pos - missile.pos).normalized() : dir_target;

    // Apply seeker noise (orthogonal to the line of sight)
    Vec3 noise_v1 = true_dir.cross(missile.vel.normalized()).normalized();
    if (noise_v1.lengthSq() < 0.5) {
        noise_v1 = true_dir.cross(Vec3(0, 1, 0)).normalized();
    }
    Vec3 noise_v2 = true_dir.cross(noise_v1).normalized();

    double n1 = dist(rng) * g_params.seeker_noise;
    double n2 = dist(rng) * g_params.seeker_noise;

    Vec3 noisy_dir = (true_dir + noise_v1 * n1 + noise_v2 * n2).normalized();
    return noisy_dir * (lock_on_decoy ? (decoy.pos - missile.pos).length() : dist_target);
}

// Implement Guidance laws (returns acceleration command vector)
Vec3 computeGuidance(GuidanceLaw law, const MissileState& missile, const Vec3& relative_pos_measured,
                     const Vec3& target_vel, const Vec3& target_acc, double dt) {
    Vec3 r = relative_pos_measured;
    double R = r.length();
    Vec3 u_r = r.normalized();

    // Estimate relative velocity from measured relative position change rate
    // In our simplified loop, we can estimate relative velocity
    static Vec3 last_r = r;
    [[maybe_unused]] Vec3 r_rate = (r - last_r) / dt;
    last_r = r;

    Vec3 v_m = missile.vel;
    Vec3 u_vm = v_m.normalized();

    switch (law) {
        case GuidanceLaw::CLOS: {
            // Target is at missile.pos + r
            Vec3 target_pos = missile.pos + r;
            Vec3 line_of_sight = target_pos; // Shooter is at (0,0,0)
            Vec3 u_los = line_of_sight.normalized();

            // Distance of missile from the Line of Sight
            Vec3 proj = u_los * missile.pos.dot(u_los);
            Vec3 deviation = missile.pos - proj;

            // Command lateral acceleration to pull back to the line of sight
            double Kp = 25.0;
            double Kv = 6.0;
            Vec3 lateral_vel = v_m - u_los * v_m.dot(u_los);
            Vec3 cmd = deviation * (-Kp) - lateral_vel * Kv;
            return cmd;
        }
        case GuidanceLaw::PurePursuit: {
            // Pull velocity vector towards target vector
            double K_pursuit = 2.5;
            Vec3 cmd = (u_r - u_vm) * (K_pursuit * v_m.length());
            return cmd;
        }
        case GuidanceLaw::ConstantPN: {
            // Ω = (R x Vr) / (R.R)
            Vec3 v_r = target_vel - v_m; // true relative velocity proxy for simplicity
            Vec3 omega = r.cross(v_r) / (R * R);
            double N = 3.5;
            Vec3 cmd = omega.cross(v_m) * N;
            return cmd;
        }
        case GuidanceLaw::AdaptivePN: {
            Vec3 v_r = target_vel - v_m;
            Vec3 omega = r.cross(v_r) / (R * R);
            // Adapt N: start lower to conserve energy and limit sensor noise amplification,
            // ramp up in terminal phase for maximum guidance precision
            double initial_range = 5000.0;
            double progress = std::clamp(1.0 - (R / initial_range), 0.0, 1.0);
            double N = 3.0 + 1.8 * progress; // N goes from 3.0 to 4.8
            Vec3 cmd = omega.cross(v_m) * N;
            return cmd;
        }
        case GuidanceLaw::AugmentedPN: {
            Vec3 v_r = target_vel - v_m;
            Vec3 omega = r.cross(v_r) / (R * R);
            double N = 3.5;
            Vec3 acceleration_compensation = target_acc * (N / 2.0);
            Vec3 cmd = omega.cross(v_m) * N + acceleration_compensation;
            return cmd;
        }
    }
    return Vec3(0, 0, 0);
}

// Kinematics integration step
void updateMissileState(MissileState& missile, const Vec3& guidance_cmd, double dt) {
    missile.time += dt;

    // Mass decay due to fuel burn
    double burn_t = std::min(missile.time, g_params.motor_burn_time);
    missile.mass = g_params.missile_empty_mass + g_params.fuel_mass * (1.0 - burn_t / g_params.motor_burn_time);

    Vec3 u_vm = missile.vel.normalized();
    double speed = missile.vel.length();

    // Forces: Thrust
    Vec3 thrust_acc(0, 0, 0);
    if (missile.time < g_params.motor_burn_time) {
        thrust_acc = u_vm * (g_params.missile_thrust / missile.mass);
    }

    // Forces: Drag
    Vec3 drag_acc = u_vm * (-g_params.missile_drag_k * speed * speed / missile.mass);

    // Forces: Lateral guidance command projection and limit
    Vec3 lat_cmd = guidance_cmd - u_vm * guidance_cmd.dot(u_vm); // force perpendicular acceleration
    double lat_g_limit_acc = g_params.max_lat_g * 9.81;
    double cmd_len = lat_cmd.length();
    if (cmd_len > lat_g_limit_acc) {
        lat_cmd = lat_cmd * (lat_g_limit_acc / cmd_len);
    }

    // Gravity
    Vec3 grav(0, -9.81, 0);

    Vec3 total_acc = thrust_acc + drag_acc + lat_cmd + grav;

    // Ground collision avoidance at low altitudes: restrict negative vertical acceleration
    if (missile.pos.y < 40.0 && total_acc.y < 0.0) {
        double factor = std::clamp(missile.pos.y / 40.0, 0.0, 1.0);
        total_acc.y *= factor;
        if (missile.pos.y < 15.0) {
            total_acc.y += (15.0 - missile.pos.y) * 3.0; // soft pull-up spring force
        }
    }

    missile.last_lat_acc = lat_cmd.length();

    // Integrate
    missile.pos = missile.pos + missile.vel * dt;
    missile.vel = missile.vel + total_acc * dt;
}

// Target flight path generator
void updateTargetState(TargetState& target, double time, ScenarioType scenario, double dt) {
    switch (scenario) {
        case ScenarioType::StaticTarget: {
            target.pos = Vec3(5000.0, 200.0, 0.0);
            target.vel = Vec3(0.0, 0.0, 0.0);
            target.acc = Vec3(0.0, 0.0, 0.0);
            break;
        }
        case ScenarioType::LinearTarget: {
            // Flies straight at 300 m/s with 1000m altitude offset
            target.vel = Vec3(-280.0, 0.0, 108.0);
            target.acc = Vec3(0, 0, 0);
            target.pos = target.pos + target.vel * dt;
            break;
        }
        case ScenarioType::ManeuveringTarget: {
            // Dynamic weave maneuver: 9G sinusoidal oscillation
            Vec3 base_vel = Vec3(-280.0, 0.0, 108.0);
            double freq = 0.25; // 0.25 Hz wave frequency
            double max_acc_mag = 9.0 * 9.81; // 9G peak acceleration

            Vec3 weave_dir = Vec3(0.0, 1.0, 0.0); // Weave vertically
            Vec3 weave_acc = weave_dir * (max_acc_mag * std::sin(2.0 * M_PI * freq * time));

            target.acc = weave_acc;
            target.vel = base_vel + target.acc * (1.0 / (2.0 * M_PI * freq)) * (-std::cos(2.0 * M_PI * freq * time) + 1.0);
            target.pos = target.pos + target.vel * dt;
            break;
        }
        case ScenarioType::Countermeasures: {
            // Flies straight
            target.vel = Vec3(-280.0, 0.0, 108.0);
            target.acc = Vec3(0, 0, 0);
            target.pos = target.pos + target.vel * dt;
            break;
        }
        case ScenarioType::MultipleMissiles: {
            // Sharp weave
            Vec3 base_vel = Vec3(-260.0, 0.0, 150.0);
            double freq = 0.35;
            double max_acc_mag = 8.0 * 9.81;
            Vec3 weave_dir = Vec3(0.0, 1.0, 0.0);
            target.acc = weave_dir * (max_acc_mag * std::sin(2.0 * M_PI * freq * time));
            target.vel = base_vel + target.acc * (1.0 / (2.0 * M_PI * freq)) * (-std::cos(2.0 * M_PI * freq * time) + 1.0);
            target.pos = target.pos + target.vel * dt;
            break;
        }
    }
}

// Decoy flare dynamics
void updateDecoyState(DecoyState& decoy, [[maybe_unused]] double time, [[maybe_unused]] const TargetState& target, double dt) {
    if (!decoy.active) return;

    // Decay intensity
    decoy.intensity *= std::exp(-dt / 1.0); // Half life of ~0.7 seconds

    // Heavy drag slows flare down rapidly relative to air
    double drag_k = 0.04;
    Vec3 air_speed = decoy.vel;
    Vec3 drag = air_speed * (-drag_k * air_speed.length());
    Vec3 gravity(0, -9.81, 0);

    decoy.vel = decoy.vel + (drag + gravity) * dt;
    decoy.pos = decoy.pos + decoy.vel * dt;
}

// Simulation runner
struct SimResult {
    bool success;
    double miss_distance;
    double flight_time;
    double peak_g;
    double guidance_cpu_time_us;
};

// Analytical closest approach interpolation between steps
double interpolateClosestApproach(const Vec3& m_pos_prev, const Vec3& m_vel_prev,
                                  const Vec3& t_pos_prev, const Vec3& t_vel_prev, double dt) {
    Vec3 r0 = t_pos_prev - m_pos_prev;
    Vec3 v_m = m_vel_prev;
    Vec3 v_t = t_vel_prev;
    Vec3 v_rel = v_t - v_m;
    double v_sq = v_rel.lengthSq();
    if (v_sq < 1e-9) {
        return r0.length();
    }
    double t_min = -r0.dot(v_rel) / v_sq;
    t_min = std::clamp(t_min, 0.0, dt);
    return (r0 + v_rel * t_min).length();
}

SimResult runSimulation(GuidanceLaw law, ScenarioType scenario, int seed, double start_delay = 0.0) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> dist(0.0, 1.0);

    MissileState missile;
    missile.pos = Vec3(0.0, 10.0, 0.0); // launch platform at (0, 10, 0)
    missile.vel = Vec3(300.0, 0.0, 0.0); // pointing forward, initial eject speed

    TargetState target;
    // Initial target positions
    if (scenario == ScenarioType::StaticTarget) {
        target.pos = Vec3(5000.0, 200.0, 0.0);
        target.vel = Vec3(0.0, 0.0, 0.0);
        target.acc = Vec3(0.0, 0.0, 0.0);
    } else {
        target.pos = Vec3(5000.0, 1000.0, -1000.0);
        target.vel = Vec3(-280.0, 0.0, 108.0);
        target.acc = Vec3(0.0, 0.0, 0.0);
    }

    DecoyState decoy;

    double time = 0.0;
    double min_distance = 1e9;
    double peak_acc_mag = 0.0;

    // Guidance timing accumulation
    double total_cpu_ns = 0.0;
    uint64_t cpu_calls = 0;

    // Apply start delay (for multiple missiles)
    // Run target simulation until missile launches
    while (time < start_delay) {
        updateTargetState(target, time, scenario, g_params.dt);
        time += g_params.dt;
    }

    // Reset missile time offset
    missile.time = 0.0;

    bool decoy_deployed = false;
    double distance_prev = 1e9;

    bool has_eccm = (law == GuidanceLaw::ConstantPN || law == GuidanceLaw::AdaptivePN || law == GuidanceLaw::AugmentedPN);

    while (time < g_params.max_time + start_delay) {
        // Deploy flare in countermeasures scenario at t = 3.0s
        if (scenario == ScenarioType::Countermeasures && !decoy_deployed && (time - start_delay) >= 3.0) {
            decoy.active = true;
            decoy.pos = target.pos;
            // Flare ejected downwards and backwards
            decoy.vel = target.vel + Vec3(40.0, -60.0, 20.0);
            decoy.intensity = 3.5; // Starts 3.5x brighter than typical target
            decoy_deployed = true;
        }

        // Save states before integration step for interpolation
        Vec3 m_pos_prev = missile.pos;
        Vec3 m_vel_prev = missile.vel;
        Vec3 t_pos_prev = target.pos;
        Vec3 t_vel_prev = target.vel;

        updateTargetState(target, time, scenario, g_params.dt);
        updateDecoyState(decoy, time, target, g_params.dt);

        // Seeker measurement (noisy)
        Vec3 rel_measured = getSeekerMeasurement(missile, target, decoy, has_eccm, rng, dist);

        // Compute guidance command (with high-precision timer)
        auto start_t = std::chrono::high_resolution_clock::now();
        Vec3 cmd = computeGuidance(law, missile, rel_measured, target.vel, target.acc, g_params.dt);
        auto end_t = std::chrono::high_resolution_clock::now();

        total_cpu_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
        cpu_calls++;

        // Missile dynamics integration
        updateMissileState(missile, cmd, g_params.dt);

        double current_dist = (target.pos - missile.pos).length();

        // Compute analytical closest approach distance during the step
        double interp_dist = interpolateClosestApproach(m_pos_prev, m_vel_prev, t_pos_prev, t_vel_prev, g_params.dt);
        if (interp_dist < min_distance) {
            min_distance = interp_dist;
        }

        // Peak G tracker (based on applied limited lateral acceleration)
        if (missile.last_lat_acc > peak_acc_mag) {
            peak_acc_mag = missile.last_lat_acc;
        }

        // Divergence criteria: if distance starts increasing rapidly after close proximity
        if (current_dist < 100.0 && current_dist > distance_prev * 1.05) {
            // Missile passed the target, exit loop
            break;
        }
        distance_prev = current_dist;

        // Altitude failsafe
        if (missile.pos.y < -5.0) {
            break;
        }

        // Success terminal condition
        if (current_dist < 0.5) {
            break;
        }

        time += g_params.dt;
    }

    SimResult res;
    res.miss_distance = min_distance;
    res.flight_time = missile.time;
    res.peak_g = peak_acc_mag / 9.81;
    res.success = (min_distance < 1.0);
    res.guidance_cpu_time_us = (cpu_calls > 0) ? (total_cpu_ns / (cpu_calls * 1000.0)) : 0.0;

    return res;
}

int main() {
    std::cout << "Starting Missile Guidance Simulation and Benchmark..." << std::endl;

    std::vector<GuidanceLaw> laws = {
        GuidanceLaw::CLOS,
        GuidanceLaw::PurePursuit,
        GuidanceLaw::ConstantPN,
        GuidanceLaw::AdaptivePN,
        GuidanceLaw::AugmentedPN
    };

    std::vector<ScenarioType> scenarios = {
        ScenarioType::StaticTarget,
        ScenarioType::LinearTarget,
        ScenarioType::ManeuveringTarget,
        ScenarioType::Countermeasures,
        ScenarioType::MultipleMissiles
    };

    std::vector<int> seeds = {1, 7, 42, 1234, 31337};

    const int iterations = 200; // Monte Carlo iterations per config for statistical significance

    std::ofstream out("results.csv");
    out << "Law,Scenario,Seed,SuccessRate,MeanMissDistance,MeanFlightTime,MeanPeakG,GuidanceCPUTimeUs\n";

    std::cout << std::left << std::setw(15) << "Law" 
              << std::setw(20) << "Scenario" 
              << std::setw(15) << "Success Rate" 
              << std::setw(15) << "Mean Miss (m)" 
              << std::setw(15) << "Mean Time (s)"
              << std::setw(15) << "Mean Peak G" 
              << std::setw(18) << "CPU Time (us)" << std::endl;
    std::cout << std::string(113, '-') << std::endl;

    for (auto law : laws) {
        for (auto sc : scenarios) {
            double total_miss = 0.0;
            double total_time = 0.0;
            double total_g = 0.0;
            double total_cpu = 0.0;
            int successful_intercepts = 0;
            int total_runs = 0;

            for (auto seed : seeds) {
                double seed_miss = 0.0;
                double seed_time = 0.0;
                double seed_g = 0.0;
                double seed_cpu = 0.0;
                int seed_success = 0;

                for (int i = 0; i < iterations; ++i) {
                    // For multiple missiles, we sweep the start delays: 0.0, 1.0, 2.0s
                    double delay = 0.0;
                    if (sc == ScenarioType::MultipleMissiles) {
                        delay = (i % 3) * 1.0;
                    }

                    SimResult res = runSimulation(law, sc, seed + i, delay);
                    seed_miss += res.miss_distance;
                    seed_time += res.flight_time;
                    seed_g += res.peak_g;
                    seed_cpu += res.guidance_cpu_time_us;
                    if (res.success) {
                        seed_success++;
                    }
                }

                double success_rate = static_cast<double>(seed_success) / iterations;
                out << toString(law) << ","
                    << toString(sc) << ","
                    << seed << ","
                    << success_rate << ","
                    << (seed_miss / iterations) << ","
                    << (seed_time / iterations) << ","
                    << (seed_g / iterations) << ","
                    << (seed_cpu / iterations) << "\n";

                total_miss += seed_miss;
                total_time += seed_time;
                total_g += seed_g;
                total_cpu += seed_cpu;
                successful_intercepts += seed_success;
                total_runs += iterations;
            }

            double overall_success = static_cast<double>(successful_intercepts) / total_runs;
            double overall_miss = total_miss / total_runs;
            double overall_time = total_time / total_runs;
            double overall_g = total_g / total_runs;
            double overall_cpu = total_cpu / total_runs;

            std::cout << std::left << std::setw(15) << toString(law)
                      << std::setw(20) << toString(sc)
                      << std::setw(15) << std::fixed << std::setprecision(2) << overall_success
                      << std::setw(15) << std::fixed << std::setprecision(3) << overall_miss
                      << std::setw(15) << std::fixed << std::setprecision(2) << overall_time
                      << std::setw(15) << std::fixed << std::setprecision(1) << overall_g
                      << std::setw(18) << std::fixed << std::setprecision(4) << overall_cpu << std::endl;
        }
        std::cout << std::string(98, '-') << std::endl;
    }

    out.close();
    std::cout << "Simulation benchmark completed. Results written to results.csv" << std::endl;
    return 0;
}
