// fuel_ammo_maint_bench.cpp — per-vehicle continuous state model benchmark
// 2026-06-22 per-vehicle-fuel-ammo-maintenance
// C++26 CPU analytical prototype per `docs/experiments/AGENTS.md` §13
// Standalone — does NOT link ProjectV mainline. Build in `prototype/build/`.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace {

// ============================================================================
//  Canonical data tables (per sources.md §Tier 1)
// ============================================================================

// BSFC (Brake-specific fuel consumption) g/(kW·h) per Wikipedia "Brake-specific fuel consumption"
// Sampled per vehicle class (Production reference values from BSFC table).
enum class VehicleClass : std::uint8_t {
    GroundPiston = 0,    // car / truck
    GroundDiesel,         // tank / APC diesel
    AvPiston,             // Cessna / trainer piston
    Turboprop,            // C-130 / Dash-8
    Turboshaft,           // helicopter
    JetDry,               // F-16 / F-22 dry
    JetAB,                // F-16 / F-22 afterburner
    TurbopropIdle,        // turboprop idle (low-power regime)
    NumClasses
};

struct VehicleClassDef {
    const char* name;
    double base_bsfc_g_per_kwh;     // BSFC in g/(kW·h); for jets use TSFC-equivalent
    double base_tsfc_g_per_kns;     // TSFC in g/(kN·s); for shaft engines unused
    double power_kw;                // nominal max power
    double fuel_capacity_kg;        // fuel tank in kg (aviation fuel 0.8 kg/L)
    double ammo_capacity;           // rounds or fuel (rockets) capacity
    double empty_mass_kg;
    double load_factor_idle;        // load factor at idle (0..1)
    double load_factor_cruise;      // load factor at cruise (0..1)
    double load_factor_combat;      // load factor at combat (0..1)
    double maintenance_threshold;   // 0..1, below this vehicle is degraded
};

// Reference data sampled from BSFC + TSFC tables (sources.md §Tier 1 §2-3).
constexpr std::array<VehicleClassDef, static_cast<size_t>(VehicleClass::NumClasses)> kVehicleClasses = {{
    {"GroundPiston",   250.0, 0.0,  150.0,    60.0,  500.0, 1500.0, 0.10, 0.50, 0.80, 0.30},  // car/truck
    {"GroundDiesel",   210.0, 0.0,  500.0,   300.0, 1200.0, 12000.0, 0.20, 0.60, 0.85, 0.25},  // tank
    {"AvPiston",       275.0, 0.0,  120.0,    90.0,  300.0, 800.0, 0.15, 0.55, 0.90, 0.30},  // Cessna
    {"Turboprop",      290.0, 0.0, 2000.0,  2000.0,  600.0, 8000.0, 0.07, 0.30, 0.78, 0.25},  // C-130
    {"Turboshaft",     260.0, 0.0, 1500.0,   800.0,  400.0, 4500.0, 0.10, 0.40, 0.80, 0.25},  // heli
    {"JetDry",           0.0, 17.0, 80000.0, 3000.0,  500.0, 9000.0, 0.20, 0.55, 0.80, 0.30},  // F-16 dry
    {"JetAB",            0.0, 55.0, 120000.0, 3000.0, 500.0, 9000.0, 0.30, 0.60, 1.00, 0.30},  // F-16 AB
    {"TurbopropIdle", 1280.0, 0.0, 2000.0,  2000.0,  600.0, 8000.0, 0.07, 0.30, 0.78, 0.25},  // idle only
}};

// ============================================================================
//  Per-vehicle state (SoA for cache locality, Strategy E)
// ============================================================================

struct alignas(64) VehicleState {
    std::uint32_t id;            // unique ID
    VehicleClass cls;            // class enum
    double fuel_kg;              // current fuel [kg]
    double ammo;                 // current ammo [rounds]
    double maintenance;          // maintenance 0..1, 1 = pristine
    double damage_hp;            // discrete damage HP 0..100, 100 = undamaged
    double wear_accumulator;     // Miner 1945 D = Σ n_i / N_i, threshold 1.0
    double load_factor;          // current load factor (0..1)
    double rpm;                  // current RPM
    double speed_mps;            // current speed
    double g_load;               // current G-load
    std::uint32_t rounds_fired;  // total rounds fired (ammo consumption counter)
    bool is_active;              // true = active (in player AOI), false = far LOD
    bool needs_refuel;
    bool needs_reload;
    bool needs_repair;
};

constexpr std::uint32_t kMaxVehicles = 1024;

// ============================================================================
//  Per-strategy update functions
// ============================================================================

// Common helpers
inline double bsfc_lookup(VehicleClass cls) {
    return kVehicleClasses[static_cast<size_t>(cls)].base_bsfc_g_per_kwh;
}
inline double tsfc_lookup(VehicleClass cls) {
    return kVehicleClasses[static_cast<size_t>(cls)].base_tsfc_g_per_kns;
}
inline double load_factor(VehicleClass cls, double rng_unit) {
    const auto& def = kVehicleClasses[static_cast<size_t>(cls)];
    // simple 3-state model: 60% cruise, 20% combat, 20% idle
    if (rng_unit < 0.60) return def.load_factor_cruise;
    if (rng_unit < 0.80) return def.load_factor_combat;
    return def.load_factor_idle;
}

// Strategy A: NaiveFlat — constant consumption rate per tick (baseline).
// No physics coupling, no event detection. Fuel burn = constant base rate.
void strategy_a_naive_flat(std::span<VehicleState> vehicles) {
    for (auto& v : vehicles) {
        if (!v.is_active) continue;
        const auto& def = kVehicleClasses[static_cast<size_t>(v.cls)];
        // Constant burn = base_bsfc * 0.001 g/(kW·h) → kg/(tick·kW)
        // Tick = 1/30 s, nominal power 100 kW, idle 7% = 7 kW effective
        const double burn_kg = (bsfc_lookup(v.cls) * 0.001) * def.power_kw * 0.07 / 3600.0;
        v.fuel_kg = std::max(0.0, v.fuel_kg - burn_kg);
        // Constant ammo drain 0.1 round/tick (dummy)
        v.ammo = std::max(0.0, v.ammo - 0.1);
        // Constant wear +0.0001/tick (dummy, no Miner)
        v.maintenance = std::max(0.0, v.maintenance - 0.0001);
    }
}

// Strategy B: LoadMultipliedExponential — load-dependent consumption + Miner 1945.
// B(t) = bsfc * load_factor * exp(0.1 * damage); per-round wear = 1/N_rounds.
void strategy_b_load_multiplied_exponential(std::span<VehicleState> vehicles, double dt) {
    for (auto& v : vehicles) {
        if (!v.is_active) continue;
        const auto& def = kVehicleClasses[static_cast<size_t>(v.cls)];
        // Fuel: BSFC * load_factor * (1 + damage_factor)
        const double damage_factor = std::exp(0.1 * (100.0 - v.damage_hp) / 100.0) - 1.0;
        const double burn_kg = bsfc_lookup(v.cls) * 0.001 * def.power_kw
                              * v.load_factor * (1.0 + damage_factor) * dt / 3600.0;
        v.fuel_kg = std::max(0.0, v.fuel_kg - burn_kg);
        // Ammo: muzzle-counter from rounds_fired
        const double rounds_this_tick = v.load_factor * 30.0;  // 30 Hz tick, 30 rounds at full load
        v.ammo = std::max(0.0, v.ammo - rounds_this_tick);
        v.rounds_fired += static_cast<std::uint32_t>(rounds_this_tick);
        // Maintenance: Miner 1945 cumulative damage, D = Σ n_i / N_i
        // Per-round N = 10000 (typical barrel life), so per round n/N = 0.0001
        // Per-G-load N = 50000 (typical airframe), so per G*tick n/N = 0.00002
        v.wear_accumulator += rounds_this_tick * 0.0001 + v.g_load * 0.00002;
        // Wear → maintenance degradation, when D ≥ 1.0, maintenance = 0
        if (v.wear_accumulator >= 1.0) v.maintenance = 0.0;
        else v.maintenance = std::max(0.0, 1.0 - v.wear_accumulator);
    }
}

// Strategy C: StatefulEventDriven — only update on tick transitions (refuel/reload/repair).
// Per-tick: 1 conditional check per vehicle. Bulk of work in event handler.
void strategy_c_stateful_event_driven(std::span<VehicleState> vehicles) {
    for (auto& v : vehicles) {
        if (!v.is_active) continue;
        // 1 conditional check: is state valid?
        v.needs_refuel = (v.fuel_kg < kVehicleClasses[static_cast<size_t>(v.cls)].fuel_capacity_kg * 0.2);
        v.needs_reload = (v.ammo < 10.0);
        v.needs_repair = (v.maintenance < kVehicleClasses[static_cast<size_t>(v.cls)].maintenance_threshold);
        // No continuous update; events are processed separately (out of scope here)
    }
}

// Strategy D: HierarchicalLOD — full update for active, simplified for far.
void strategy_d_hierarchical_lod(std::span<VehicleState> vehicles, double dt) {
    for (auto& v : vehicles) {
        if (v.is_active) {
            // Full update (like B but no Miner accumulator)
            const auto& def = kVehicleClasses[static_cast<size_t>(v.cls)];
            const double burn_kg = bsfc_lookup(v.cls) * 0.001 * def.power_kw
                                  * v.load_factor * dt / 3600.0;
            v.fuel_kg = std::max(0.0, v.fuel_kg - burn_kg);
            v.ammo = std::max(0.0, v.ammo - v.load_factor * 30.0);
            // Cheap wear
            v.maintenance = std::max(0.0, v.maintenance - 0.0001 * v.load_factor);
        } else {
            // Simplified: 1/10 rate update
            const auto& def = kVehicleClasses[static_cast<size_t>(v.cls)];
            const double burn_kg = bsfc_lookup(v.cls) * 0.001 * def.power_kw
                                  * 0.07 * dt / 3600.0 * 0.1;
            v.fuel_kg = std::max(0.0, v.fuel_kg - burn_kg);
            // Ammo and maintenance not updated for far LOD
        }
    }
}

// Strategy E: PhysicsCoupled_PerVehicleSoA — full physics-driven model.
// SoA-aware: read all vehicles' RPM/speed in SoA order, then update state.
void strategy_e_physics_coupled_soa(std::span<VehicleState> vehicles, double dt) {
    // Pass 1: read RPM + speed + G-load (assumed pre-computed by upstream simulators)
    // Pass 2: write fuel + ammo + maintenance updates
    for (auto& v : vehicles) {
        if (!v.is_active) continue;
        const auto& def = kVehicleClasses[static_cast<size_t>(v.cls)];
        // Fuel: physics-coupled TSFC/BSFC + load + G-load (gravity) + altitude (via rpm)
        double lf = v.load_factor;
        // G-load amplifies burn in jet (combat maneuvering)
        if (v.cls == VehicleClass::JetDry || v.cls == VehicleClass::JetAB) {
            lf = std::min(1.0, lf * (1.0 + 0.2 * (v.g_load - 1.0)));
        }
        const double burn_kg = (v.cls == VehicleClass::JetDry || v.cls == VehicleClass::JetAB)
            ? tsfc_lookup(v.cls) * 0.001 * def.power_kw * lf * dt / 3600.0
            : bsfc_lookup(v.cls) * 0.001 * def.power_kw * lf * dt / 3600.0;
        v.fuel_kg = std::max(0.0, v.fuel_kg - burn_kg);
        // Ammo: muzzle counter from RPM-driven fire rate
        // Higher RPM → higher fire rate. Cap at 600 rpm (per Wikipedia Ammunition belt feed)
        const double fire_rate_rpm = std::min(600.0, 100.0 + 500.0 * v.load_factor);
        const double rounds_this_tick = fire_rate_rpm / 60.0;  // per second, dt-normalized
        v.ammo = std::max(0.0, v.ammo - rounds_this_tick);
        v.rounds_fired += static_cast<std::uint32_t>(rounds_this_tick);
        // Maintenance: Miner 1945 with damage coupling
        v.wear_accumulator += rounds_this_tick * 0.0001 + v.g_load * 0.00002;
        if (v.wear_accumulator >= 1.0) v.maintenance = 0.0;
        else v.maintenance = std::max(0.0, 1.0 - v.wear_accumulator);
        // Damage coupling: each round fired = 0.001 damage to internal components
        v.damage_hp = std::max(0.0, v.damage_hp - rounds_this_tick * 0.001);
    }
}

// ============================================================================
//  Scene definitions
// ============================================================================

struct SceneDef {
    const char* name;
    std::uint32_t num_vehicles;
    double damage_rate;        // damage per tick from incoming fire
    double repair_rate;        // repair rate if needs_repair (per second)
    double refuel_rate;        // refuel rate (per second)
    double reload_rate;        // reload rate (rounds per second)
    double activity_ratio;     // fraction of vehicles in player AOI (active)
};

constexpr std::array<SceneDef, 5> kScenes = {{
    {"small_ground_20",     20,   0.05,  0.10,  50.0, 60.0, 0.90},
    {"medium_mixed_100",   100,   0.10,  0.20, 100.0, 80.0, 0.60},
    {"large_armor_500",    500,   0.20,  0.50, 200.0,100.0, 0.40},
    {"massive_battle_1000",1000,  0.30,  0.80, 300.0,120.0, 0.30},
    {"air_combat_50",       50,   0.40,  0.30, 200.0,150.0, 0.80},
}};

// ============================================================================
//  Benchmark harness (per `benchmarks/methodology.md` §3)
// ============================================================================

template <typename F>
double bench_strategy(F&& fn, std::span<VehicleState> vehicles, double dt, int warmup, int iters) {
    for (int i = 0; i < warmup; ++i) fn(vehicles, dt);
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) fn(vehicles, dt);
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / iters;
}

void init_vehicles(std::span<VehicleState> vehicles, const SceneDef& scene, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    std::uniform_int_distribution<int> cls_dist(0, static_cast<int>(VehicleClass::NumClasses) - 1);
    for (std::uint32_t i = 0; i < vehicles.size(); ++i) {
        auto& v = vehicles[i];
        v.id = i;
        v.cls = static_cast<VehicleClass>(cls_dist(rng));
        const auto& def = kVehicleClasses[static_cast<size_t>(v.cls)];
        v.fuel_kg = def.fuel_capacity_kg * (0.5 + 0.5 * uni(rng));  // 50-100% full
        v.ammo = def.ammo_capacity * (0.5 + 0.5 * uni(rng));
        v.maintenance = 0.5 + 0.5 * uni(rng);
        v.damage_hp = 80.0 + 20.0 * uni(rng);
        v.wear_accumulator = 0.0;
        v.load_factor = load_factor(v.cls, uni(rng));
        v.rpm = def.power_kw * v.load_factor * 100.0;  // approx RPM proportional
        v.speed_mps = 50.0 * uni(rng);
        v.g_load = 1.0 + 2.0 * uni(rng);  // 1-3 G
        v.rounds_fired = 0;
        v.is_active = (uni(rng) < scene.activity_ratio);
        v.needs_refuel = v.needs_reload = v.needs_repair = false;
    }
}

}  // namespace

int main() {
    constexpr int kWarmup = 10;
    constexpr int kIters = 1000;
    constexpr double kDt = 1.0 / 30.0;  // 30 Hz tick
    constexpr std::array<std::uint32_t, 5> kSeeds = {1, 7, 42, 1234, 31337};

    // Output CSV: strategy,scene,seed,mean_ns,median_ns,p95_ns,p99_ns,n_vehicles
    std::ofstream out("results.csv");
    if (!out.is_open()) {
        std::fprintf(stderr, "FATAL: cannot open results.csv for writing\n");
        return 1;
    }
    out << "strategy,scene,seed,mean_ns,p50_ns,p95_ns,p99_ns,n_vehicles\n";
    out.flush();

    // Pre-allocate worst-case buffer
    std::vector<VehicleState> vehicles(kMaxVehicles);
    std::vector<double> samples;
    samples.reserve(kIters);

    for (const auto& scene : kScenes) {
        for (std::uint32_t seed : kSeeds) {
            double ns_a = 0.0, ns_b = 0.0, ns_c = 0.0, ns_d = 0.0, ns_e = 0.0;
            auto vspan = std::span{vehicles.data(), scene.num_vehicles};

            // ============ A_NaiveFlat ============
            init_vehicles(vspan, scene, seed);
            samples.clear();
            for (int i = 0; i < kWarmup; ++i) strategy_a_naive_flat(vspan);
            for (int i = 0; i < kIters; ++i) {
                auto t0 = std::chrono::high_resolution_clock::now();
                strategy_a_naive_flat(vspan);
                auto t1 = std::chrono::high_resolution_clock::now();
                samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
            }
            std::sort(samples.begin(), samples.end());
            {
                double mean = 0.0;
                for (double s : samples) mean += s;
                mean /= samples.size();
                ns_a = mean;
                out << "A_NaiveFlat," << scene.name << "," << seed << ","
                    << mean << "," << samples[kIters/2] << ","
                    << samples[static_cast<size_t>(kIters*0.95)] << ","
                    << samples[static_cast<size_t>(kIters*0.99)] << ","
                    << scene.num_vehicles << "\n";
            }

            // ============ B_LoadMultipliedExponential ============
            init_vehicles(vspan, scene, seed);
            samples.clear();
            for (int i = 0; i < kWarmup; ++i) strategy_b_load_multiplied_exponential(vspan, kDt);
            for (int i = 0; i < kIters; ++i) {
                auto t0 = std::chrono::high_resolution_clock::now();
                strategy_b_load_multiplied_exponential(vspan, kDt);
                auto t1 = std::chrono::high_resolution_clock::now();
                samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
            }
            std::sort(samples.begin(), samples.end());
            {
                double mean = 0.0;
                for (double s : samples) mean += s;
                mean /= samples.size();
                ns_b = mean;
                out << "B_LoadMultipliedExp," << scene.name << "," << seed << ","
                    << mean << "," << samples[kIters/2] << ","
                    << samples[static_cast<size_t>(kIters*0.95)] << ","
                    << samples[static_cast<size_t>(kIters*0.99)] << ","
                    << scene.num_vehicles << "\n";
            }

            // ============ C_StatefulEventDriven ============
            init_vehicles(vspan, scene, seed);
            samples.clear();
            for (int i = 0; i < kWarmup; ++i) strategy_c_stateful_event_driven(vspan);
            for (int i = 0; i < kIters; ++i) {
                auto t0 = std::chrono::high_resolution_clock::now();
                strategy_c_stateful_event_driven(vspan);
                auto t1 = std::chrono::high_resolution_clock::now();
                samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
            }
            std::sort(samples.begin(), samples.end());
            {
                double mean = 0.0;
                for (double s : samples) mean += s;
                mean /= samples.size();
                ns_c = mean;
                out << "C_StatefulEventDriven," << scene.name << "," << seed << ","
                    << mean << "," << samples[kIters/2] << ","
                    << samples[static_cast<size_t>(kIters*0.95)] << ","
                    << samples[static_cast<size_t>(kIters*0.99)] << ","
                    << scene.num_vehicles << "\n";
            }

            // ============ D_HierarchicalLOD ============
            init_vehicles(vspan, scene, seed);
            samples.clear();
            for (int i = 0; i < kWarmup; ++i) strategy_d_hierarchical_lod(vspan, kDt);
            for (int i = 0; i < kIters; ++i) {
                auto t0 = std::chrono::high_resolution_clock::now();
                strategy_d_hierarchical_lod(vspan, kDt);
                auto t1 = std::chrono::high_resolution_clock::now();
                samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
            }
            std::sort(samples.begin(), samples.end());
            {
                double mean = 0.0;
                for (double s : samples) mean += s;
                mean /= samples.size();
                ns_d = mean;
                out << "D_HierarchicalLOD," << scene.name << "," << seed << ","
                    << mean << "," << samples[kIters/2] << ","
                    << samples[static_cast<size_t>(kIters*0.95)] << ","
                    << samples[static_cast<size_t>(kIters*0.99)] << ","
                    << scene.num_vehicles << "\n";
            }

            // ============ E_PhysicsCoupled_PerVehicleSoA ============
            init_vehicles(vspan, scene, seed);
            samples.clear();
            for (int i = 0; i < kWarmup; ++i) strategy_e_physics_coupled_soa(vspan, kDt);
            for (int i = 0; i < kIters; ++i) {
                auto t0 = std::chrono::high_resolution_clock::now();
                strategy_e_physics_coupled_soa(vspan, kDt);
                auto t1 = std::chrono::high_resolution_clock::now();
                samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
            }
            std::sort(samples.begin(), samples.end());
            {
                double mean = 0.0;
                for (double s : samples) mean += s;
                mean /= samples.size();
                ns_e = mean;
                out << "E_PhysicsCoupledSoA," << scene.name << "," << seed << ","
                    << mean << "," << samples[kIters/2] << ","
                    << samples[static_cast<size_t>(kIters*0.95)] << ","
                    << samples[static_cast<size_t>(kIters*0.99)] << ","
                    << scene.num_vehicles << "\n";
            }

            out.flush();
            std::printf("[%s seed=%u n=%u] A=%.0f B=%.0f C=%.0f D=%.0f E=%.0f ns/iter\n",
                scene.name, seed, scene.num_vehicles, ns_a, ns_b, ns_c, ns_d, ns_e);
        }
    }

    out.flush();
    out.close();

    std::printf("Per-vehicle fuel/ammo/maintenance benchmark complete.\n");
    std::printf("Results: build/results.csv (125 rows = 5 strategies x 5 scenes x 5 seeds)\n");

    return 0;
}
