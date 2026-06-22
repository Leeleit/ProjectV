#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <random>
#include <memory>

// ============================================================================
// MATERIAL ENUMS AND PROPERTIES
// ============================================================================

enum class SurfaceType : uint8_t {
    Asphalt = 0,
    Grass,
    Sand,
    Mud,
    Ice,
    Count
};

inline std::string GetSurfaceName(SurfaceType type) {
    switch (type) {
        case SurfaceType::Asphalt: return "Asphalt";
        case SurfaceType::Grass: return "Grass";
        case SurfaceType::Sand: return "Sand";
        case SurfaceType::Mud: return "Mud";
        case SurfaceType::Ice: return "Ice";
        default: return "Unknown";
    }
}

struct PacejkaParams {
    double B; // Stiffness
    double C; // Shape
    double D; // Peak friction coefficient
    double E; // Curvature
};

// Standard Pacejka longitudinal parameters
inline PacejkaParams GetPacejkaParams(SurfaceType type) {
    switch (type) {
        case SurfaceType::Asphalt: return { 10.0, 1.9, 1.0, 0.97 };
        case SurfaceType::Grass:   return { 8.0, 1.6, 0.6, 0.8 };
        case SurfaceType::Sand:    return { 5.0, 1.3, 0.4, 0.5 };
        case SurfaceType::Mud:     return { 4.0, 1.2, 0.25, 0.3 };
        case SurfaceType::Ice:     return { 12.0, 1.8, 0.1, 1.0 };
        default:                   return { 10.0, 1.9, 1.0, 0.97 };
    }
}

// Simple linear coefficients for Strategy B and C
inline double GetTractionCoefficient(SurfaceType type) {
    switch (type) {
        case SurfaceType::Asphalt: return 1.0;
        case SurfaceType::Grass:   return 0.6;
        case SurfaceType::Sand:    return 0.4;
        case SurfaceType::Mud:     return 0.25;
        case SurfaceType::Ice:     return 0.1;
        default:                   return 1.0;
    }
}

// ============================================================================
// WHEEL STRUCTURE (AoS Layout)
// ============================================================================

struct Wheel {
    SurfaceType surface = SurfaceType::Asphalt;
    double omega = 0.0;          // angular velocity (rad/s)
    double radius = 0.4;         // wheel radius (m)
    double mass = 40.0;          // wheel mass (kg)
    double inertia = 0.5 * 40.0 * 0.4 * 0.4; // rotational inertia (kg*m^2)
    double normal_load = 5000.0; // normal force Fz (N)
    double slip_ratio = 0.0;     // slip ratio kappa
    double traction_force = 0.0; // longitudinal force Fx (N)
    double linear_velocity = 0.0; // vehicle speed at wheel contact patch (m/s)
    double drive_torque = 0.0;   // applied torque (Nm)
    double brake_torque = 0.0;   // braking torque (Nm)
};

// ============================================================================
// WHEEL STRUCTURE (SoA Layout for Strategy E)
// ============================================================================

struct WheelSoA {
    std::vector<uint8_t> surface;
    std::vector<float> omega;
    std::vector<float> radius;
    std::vector<float> inertia;
    std::vector<float> normal_load;
    std::vector<float> slip_ratio;
    std::vector<float> traction_force;
    std::vector<float> linear_velocity;
    std::vector<float> drive_torque;
    std::vector<float> brake_torque;

    // Pacejka pre-unrolled arrays to avoid branch inside SIMD loop
    std::vector<float> pacejka_B;
    std::vector<float> pacejka_C;
    std::vector<float> pacejka_D;
    std::vector<float> pacejka_E;

    void resize(size_t n) {
        surface.resize(n, static_cast<uint8_t>(SurfaceType::Asphalt));
        omega.resize(n, 0.0f);
        radius.resize(n, 0.4f);
        inertia.resize(n, 0.5f * 40.0f * 0.4f * 0.4f);
        normal_load.resize(n, 5000.0f);
        slip_ratio.resize(n, 0.0f);
        traction_force.resize(n, 0.0f);
        linear_velocity.resize(n, 0.0f);
        drive_torque.resize(n, 0.0f);
        brake_torque.resize(n, 0.0f);

        pacejka_B.resize(n, 10.0f);
        pacejka_C.resize(n, 1.9f);
        pacejka_D.resize(n, 1.0f);
        pacejka_E.resize(n, 0.97f);
    }
};

// ============================================================================
// INITIALIZATION HELPERS
// ============================================================================

Wheel GetInitialWheel(int seed) {
    Wheel w;
    std::mt19937 gen(200 + seed);
    std::uniform_int_distribution<int> dist_surface(0, static_cast<int>(SurfaceType::Count) - 1);
    std::uniform_real_distribution<double> dist_load(4000.0, 6000.0);
    std::uniform_real_distribution<double> dist_speed(0.0, 30.0); // 0 to 108 km/h

    w.surface = static_cast<SurfaceType>(dist_surface(gen));
    w.normal_load = dist_load(gen);
    w.linear_velocity = dist_speed(gen);
    w.omega = w.linear_velocity / w.radius; // Rolling start baseline
    return w;
}

// ============================================================================
// STRATEGY UPDATES
// ============================================================================

// Strategy A: Constant traction coefficient baseline (no slip, no torque dynamics)
void UpdateStrategyA(Wheel& w, double dt) {
    // Fx = Fz * mu (always 1.0)
    w.traction_force = w.normal_load * 1.0;
    // Simulate linear roll update without slip dynamics
    w.omega = w.linear_velocity / w.radius;
}

// Strategy B: Surface material lookup (no slip dynamics)
void UpdateStrategyB(Wheel& w, double dt) {
    double mu = GetTractionCoefficient(w.surface);
    w.traction_force = w.normal_load * mu;
    w.omega = w.linear_velocity / w.radius;
}

// Strategy C: Linear wheel slip model & torque dynamics
void UpdateStrategyC(Wheel& w, double dt) {
    // 1. Calculate slip ratio kappa
    double v_abs = std::abs(w.linear_velocity);
    double slip = 0.0;
    if (v_abs > 0.1) {
        slip = (w.omega * w.radius - w.linear_velocity) / v_abs;
    } else {
        slip = (w.omega * w.radius - w.linear_velocity) / 0.1;
    }
    w.slip_ratio = std::clamp(slip, -1.0, 1.0);

    // 2. Linear slip model: traction scales with slip up to peak, then flat
    double mu_peak = GetTractionCoefficient(w.surface);
    double slope = 10.0; // Dynamic friction builds up quickly
    double mu_dyn = mu_peak * std::min(1.0, std::abs(w.slip_ratio) * slope);

    // Sign of force opposes direction of relative motion
    double force_direction = (w.omega * w.radius >= w.linear_velocity) ? 1.0 : -1.0;
    w.traction_force = w.normal_load * mu_dyn * force_direction;

    // 3. Torque balance to update angular velocity
    double brake_torque_dir = (w.omega > 0.0) ? -1.0 : (w.omega < 0.0 ? 1.0 : 0.0);
    double applied_brake = w.brake_torque * brake_torque_dir;

    double net_torque = w.drive_torque + applied_brake - (w.traction_force * w.radius);
    w.omega += (net_torque / w.inertia) * dt;
}

// Strategy D: Non-linear Pacejka Magic Formula & torque dynamics
void UpdateStrategyD(Wheel& w, double dt) {
    // 1. Calculate slip ratio kappa
    double v_abs = std::abs(w.linear_velocity);
    double slip = 0.0;
    if (v_abs > 0.1) {
        slip = (w.omega * w.radius - w.linear_velocity) / v_abs;
    } else {
        slip = (w.omega * w.radius - w.linear_velocity) / 0.1;
    }
    w.slip_ratio = std::clamp(slip, -1.0, 1.0);

    // 2. Pacejka magic formula calculation
    PacejkaParams p = GetPacejkaParams(w.surface);
    double B_slip = p.B * w.slip_ratio;
    double mu_dyn = p.D * std::sin(p.C * std::atan(B_slip - p.E * (B_slip - std::atan(B_slip))));
    w.traction_force = w.normal_load * mu_dyn;

    // 3. Torque balance to update angular velocity
    double brake_torque_dir = (w.omega > 0.0) ? -1.0 : (w.omega < 0.0 ? 1.0 : 0.0);
    double applied_brake = w.brake_torque * brake_torque_dir;

    double net_torque = w.drive_torque + applied_brake - (w.traction_force * w.radius);
    w.omega += (net_torque / w.inertia) * dt;
}

// Strategy E: Optimized and vectorized SoA implementation
void UpdateStrategyE(WheelSoA& soa, int num_wheels, float dt) {
    const float eps = 0.1f;

    #pragma omp simd
    for (int i = 0; i < num_wheels; ++i) {
        float omega_val = soa.omega[i];
        float radius_val = soa.radius[i];
        float lin_vel = soa.linear_velocity[i];
        float norm_load = soa.normal_load[i];
        float drive_trq = soa.drive_torque[i];
        float brake_trq = soa.brake_torque[i];
        float inertia_val = soa.inertia[i];

        // 1. Slip Ratio
        float v_abs = std::abs(lin_vel);
        float divisor = (v_abs > eps) ? v_abs : eps;
        float slip = (omega_val * radius_val - lin_vel) / divisor;

        // Clamp to [-1.0, 1.0] without branch
        slip = std::max(-1.0f, std::min(1.0f, slip));
        soa.slip_ratio[i] = slip;

        // 2. Pacejka Magic Formula
        float B = soa.pacejka_B[i];
        float C = soa.pacejka_C[i];
        float D = soa.pacejka_D[i];
        float E = soa.pacejka_E[i];

        float B_slip = B * slip;
        // Fast float arctan approximations are avoided here to keep accuracy,
        // std::atan and std::sin are vectorized by modern compilers on Zen 3.
        float slip_atan = std::atan(B_slip);
        float inner = B_slip - E * (B_slip - slip_atan);
        float mu_dyn = D * std::sin(C * std::atan(inner));
        float force = norm_load * mu_dyn;
        soa.traction_force[i] = force;

        // 3. Torque balance
        float brake_dir = (omega_val > 0.0f) ? -1.0f : ((omega_val < 0.0f) ? 1.0f : 0.0f);
        float applied_brake = brake_trq * brake_dir;

        float net_torque = drive_trq + applied_brake - (force * radius_val);
        soa.omega[i] = omega_val + (net_torque / inertia_val) * dt;
    }
}

// ============================================================================
// BENCHMARK EXECUTIVE
// ============================================================================

struct BenchResult {
    std::string strategy;
    std::string scale_name;
    int num_wheels = 0;
    int seed = 0;
    double avg_step_time_ns = 0.0;
    double avg_slip_ratio = 0.0;
    double avg_traction_force = 0.0;
};

int main() {
    std::cout << "=====================================================================" << std::endl;
    std::cout << "ProjectV Terrain Traction Variation Benchmark" << std::endl;
    std::cout << "Target C++26 standard, Zen 3 hardware profile optimized" << std::endl;
    std::cout << "=====================================================================" << std::endl;

    std::vector<std::string> strategy_names = {
        "A_Constant_Traction",
        "B_Surface_Lookup",
        "C_Slip_Linear",
        "D_Slip_Pacejka",
        "E_Vectorized_SoA"
    };

    std::vector<std::string> scale_names = {
        "jeep_64",
        "truck_256",
        "convoy_1024",
        "regiment_4096",
        "division_16384"
    };

    std::vector<int> scale_sizes = {64, 256, 1024, 4096, 16384};

    const int num_seeds = 5;
    const double sim_duration = 10.0; // 10 seconds of simulated time
    const double dt = 0.016;          // 16 ms physics step (625 steps total)

    // Warm-up cache
    for (int i = 0; i < 1000; ++i) {
        Wheel w = GetInitialWheel(i);
        UpdateStrategyD(w, dt);
    }

    std::vector<BenchResult> all_results;

    for (int scale_idx = 0; scale_idx < 5; ++scale_idx) {
        int num_wheels = scale_sizes[scale_idx];
        std::string scale_name = scale_names[scale_idx];

        std::cout << "Running scale: " << scale_name << " (" << num_wheels << " wheels)" << std::endl;

        for (int strat_idx = 0; strat_idx < 5; ++strat_idx) {
            std::string strat_name = strategy_names[strat_idx];

            for (int seed = 0; seed < num_seeds; ++seed) {
                std::mt19937 gen(2400 + seed);
                std::vector<double> step_times_ns;
                int steps = static_cast<int>(sim_duration / dt);
                step_times_ns.reserve(steps);

                if (strat_idx == 4) { // Strategy E: Vectorized SoA
                    WheelSoA soa;
                    soa.resize(num_wheels);

                    std::uniform_int_distribution<int> dist_surface(0, 4);
                    std::uniform_real_distribution<float> dist_load(4000.0f, 6000.0f);
                    std::uniform_real_distribution<float> dist_speed(0.0f, 30.0f);

                    for (int i = 0; i < num_wheels; ++i) {
                        SurfaceType st = static_cast<SurfaceType>(dist_surface(gen));
                        soa.surface[i] = static_cast<uint8_t>(st);
                        soa.normal_load[i] = dist_load(gen);
                        soa.linear_velocity[i] = dist_speed(gen);
                        soa.omega[i] = soa.linear_velocity[i] / soa.radius[i];

                        PacejkaParams p = GetPacejkaParams(st);
                        soa.pacejka_B[i] = static_cast<float>(p.B);
                        soa.pacejka_C[i] = static_cast<float>(p.C);
                        soa.pacejka_D[i] = static_cast<float>(p.D);
                        soa.pacejka_E[i] = static_cast<float>(p.E);
                    }

                    for (int step = 0; step < steps; ++step) {
                        // Apply engine torque perturbation at step 100
                        if (step == 100) {
                            for (int i = 0; i < num_wheels; ++i) {
                                soa.drive_torque[i] = 1500.0f; // Spike drive torque (accelerating)
                            }
                        }
                        // Apply heavy brake at step 400
                        if (step == 400) {
                            for (int i = 0; i < num_wheels; ++i) {
                                soa.drive_torque[i] = 0.0f;
                                soa.brake_torque[i] = 3000.0f; // Heavy braking
                            }
                        }

                        auto start_t = std::chrono::high_resolution_clock::now();

                        UpdateStrategyE(soa, num_wheels, static_cast<float>(dt));

                        auto end_t = std::chrono::high_resolution_clock::now();
                        double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                        step_times_ns.push_back(ns);
                    }

                    // Collect metrics
                    double slip_sum = 0.0;
                    double force_sum = 0.0;
                    for (int i = 0; i < num_wheels; ++i) {
                        slip_sum += soa.slip_ratio[i];
                        force_sum += soa.traction_force[i];
                    }

                    double sum_time = 0.0;
                    for (double ns : step_times_ns) sum_time += ns;

                    all_results.push_back({
                        strat_name,
                        scale_name,
                        num_wheels,
                        seed,
                        (sum_time / steps) / num_wheels, // ns per wheel-step
                        slip_sum / num_wheels,
                        force_sum / num_wheels
                    });

                } else { // Strategy A, B, C, D (AoS layout)
                    std::vector<Wheel> wheels(num_wheels);
                    for (int i = 0; i < num_wheels; ++i) {
                        wheels[i] = GetInitialWheel(seed + i);
                    }

                    for (int step = 0; step < steps; ++step) {
                        // Apply engine torque perturbation
                        if (step == 100) {
                            for (int i = 0; i < num_wheels; ++i) {
                                wheels[i].drive_torque = 1500.0;
                            }
                        }
                        // Apply brake
                        if (step == 400) {
                            for (int i = 0; i < num_wheels; ++i) {
                                wheels[i].drive_torque = 0.0;
                                wheels[i].brake_torque = 3000.0;
                            }
                        }

                        auto start_t = std::chrono::high_resolution_clock::now();

                        for (int i = 0; i < num_wheels; ++i) {
                            if (strat_idx == 0) {
                                UpdateStrategyA(wheels[i], dt);
                            } else if (strat_idx == 1) {
                                UpdateStrategyB(wheels[i], dt);
                            } else if (strat_idx == 2) {
                                UpdateStrategyC(wheels[i], dt);
                            } else { // Strategy D
                                UpdateStrategyD(wheels[i], dt);
                            }
                        }

                        auto end_t = std::chrono::high_resolution_clock::now();
                        double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                        step_times_ns.push_back(ns);
                    }

                    // Collect metrics
                    double slip_sum = 0.0;
                    double force_sum = 0.0;
                    for (int i = 0; i < num_wheels; ++i) {
                        slip_sum += wheels[i].slip_ratio;
                        force_sum += wheels[i].traction_force;
                    }

                    double sum_time = 0.0;
                    for (double ns : step_times_ns) sum_time += ns;

                    all_results.push_back({
                        strat_name,
                        scale_name,
                        num_wheels,
                        seed,
                        (sum_time / steps) / num_wheels, // ns per wheel-step
                        slip_sum / num_wheels,
                        force_sum / num_wheels
                    });
                }
            }
        }
    }

    // Write results to CSV
    std::string csv_path = "results.csv";
    std::cout << "Writing " << all_results.size() << " entries to " << csv_path << "..." << std::endl;
    std::ofstream csv(csv_path);
    csv << "Strategy,Scale,NumWheels,Seed,StepTimeNsPerWheel,AvgSlipRatio,AvgTractionForce\n";
    for (const auto& r : all_results) {
        csv << r.strategy << ","
            << r.scale_name << ","
            << r.num_wheels << ","
            << r.seed << ","
            << std::fixed << std::setprecision(2) << r.avg_step_time_ns << ","
            << r.avg_slip_ratio << ","
            << r.avg_traction_force << "\n";
    }
    csv.close();

    // Print summary table
    std::cout << "\n=====================================================================" << std::endl;
    std::cout << "SUMMARY STATISTICS (Averages over all Scales and Seeds)" << std::endl;
    std::cout << "=====================================================================" << std::endl;
    std::cout << std::left << std::setw(25) << "Strategy" 
              << std::setw(15) << "Scale Size" 
              << std::setw(30) << "Mean Time/Wheel-Step (ns)" 
              << std::setw(20) << "Avg Traction Force (N)" << std::endl;
    std::cout << "---------------------------------------------------------------------" << std::endl;

    for (const auto& strat : strategy_names) {
        for (int size : scale_sizes) {
            double time_sum = 0.0;
            double force_sum = 0.0;
            int total_runs = 0;

            for (const auto& r : all_results) {
                if (r.strategy == strat && r.num_wheels == size) {
                    time_sum += r.avg_step_time_ns;
                    force_sum += r.avg_traction_force;
                    total_runs++;
                }
            }

            double mean_time = time_sum / total_runs;
            double mean_force = force_sum / total_runs;

            std::cout << std::left << std::setw(25) << strat 
                       << std::setw(15) << size 
                       << std::fixed << std::setprecision(1)
                       << std::setw(30) << mean_time 
                       << std::setw(20) << mean_force << std::endl;
        }
        std::cout << "---------------------------------------------------------------------" << std::endl;
    }
    std::cout << "=====================================================================" << std::endl;

    return 0;
}
