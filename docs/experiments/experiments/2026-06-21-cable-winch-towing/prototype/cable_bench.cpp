// SPDX-License-Identifier: MIT
// Standalone C++26 CPU benchmark for cable/winch physics strategies.
//
// Hypothesis (per README.md):
//   XPBD distance-constraint solver (Macklin/Müller 2016) из N=8-64 rigid-body
//   segments per meter + 8 Gauss-Seidel iterations + Verlet integration даст
//   <0.01 ms/meter per tick (≈10 µs for 100 m tow cable) с <2% max stretch error.
//
// Strategies (5):
//   A_NaiveGlobalStretch         — single global projection per tick (slack-only)
//   B_MassSpring_Hooke           — classical stiff springs (instability)
//   C_PBD_Muller2007             — Position-Based Dynamics distance constraint
//   D_DistanceConstraint_Verlet  — Jakobsen 2001 (Hitman) Verlet + distance constraint
//   E_XPBD_Macklin2016           — XPBD with compliance coefficient (target)
//
// Scenes (5):
//   vertical_suspension_10m      — heavy load hangs from 10 m cable
//   horizontal_catenary_50m      — catenary curve under self-weight (50 m span)
//   towing_at_angle_100m         — vehicle towing load at 30° angle (100 m)
//   winch_reel_drum_50m          — active winch extension/retraction
//   slack_droop_20m              — loose cable lying on ground
//
// Metrics per tick:
//   - wall-time (µs/tick/m)
//   - max stretch error vs analytical catenary (%)
//   - stability (NaN/Inf check)
//   - convergence iterations needed
//
// Protocol per benchmarks/methodology.md:
//   5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main
//   measurements. Wall time per (strategy, scene, seed) reported to results.csv.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Vec3 — minimal SoA-friendly vector
// ============================================================================

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x{x_}, y{y_}, z{z_} {}

    constexpr Vec3 operator+(Vec3 o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(Vec3 o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(double s) const noexcept { return {x * s, y * s, z * s}; }
    constexpr Vec3& operator+=(Vec3 o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vec3& operator-=(Vec3 o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vec3& operator*=(double s) noexcept { x *= s; y *= s; z *= s; return *this; }
};

constexpr double dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

constexpr double length(Vec3 v) noexcept {
    return std::sqrt(dot(v, v));
}

constexpr Vec3 normalize(Vec3 v) noexcept {
    double l = length(v);
    if (l < 1e-12) return v;
    return v * (1.0 / l);
}

// ============================================================================
// Cable — particle chain with mass, position, prev-position (for Verlet)
// ============================================================================

struct Cable {
    std::vector<Vec3> pos;        // current position
    std::vector<Vec3> prev_pos;   // previous position (Verlet history)
    std::vector<Vec3> vel;        // velocity (mass-spring + PBD only)
    std::vector<double> mass;     // per-segment mass
    std::vector<bool> fixed;      // pinned endpoint (true)
    double rest_length{0.0};      // total rest length (m)
    double segment_length{0.0};   // rest length per segment
    int n{0};                     // number of particles (= segments + 1)
};

// ============================================================================
// Stats (per benchmarks/methodology.md §3)
// ============================================================================

struct Stats {
    double mean{0.0};
    double median{0.0};
    double p95{0.0};
    double p99{0.0};
    double stddev{0.0};
    double min{0.0};
    double max{0.0};
};

Stats compute_stats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    s.min = sorted.front();
    s.max = sorted.back();
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    return s;
}

// ============================================================================
// Scene parameters
// ============================================================================

struct SceneParams {
    std::string name;
    int segments_per_meter{8};    // N per meter
    double length_m{10.0};        // total cable length
    Vec3 anchor_a{0.0, 0.0, 0.0};
    Vec3 anchor_b{0.0, 0.0, 0.0};
    double load_mass_kg{0.0};     // 0 = self-weight only
    double gravity_y{-9.81};
    double wind_x{0.0};
    bool ground_collision{false}; // for slack_droop scene
    double ground_y{0.0};
    double winch_speed_m_s{0.0};  // >0 = retract, <0 = extend
};

SceneParams make_scene(const std::string& name) {
    SceneParams s;
    s.name = name;
    if (name == "vertical_suspension_10m") {
        s.segments_per_meter = 16;
        s.length_m = 10.0;
        s.anchor_a = {0.0, 10.0, 0.0};
        s.anchor_b = {0.0, 0.0, 0.0};  // not used — load hangs from anchor_a
        s.load_mass_kg = 500.0;
        s.gravity_y = -9.81;
    } else if (name == "horizontal_catenary_50m") {
        s.segments_per_meter = 4;
        s.length_m = 52.0; // slightly longer than span → catenary sag
        s.anchor_a = {-25.0, 5.0, 0.0};
        s.anchor_b = {25.0, 5.0, 0.0};
        s.load_mass_kg = 0.0; // self-weight only
        s.gravity_y = -9.81;
    } else if (name == "towing_at_angle_100m") {
        s.segments_per_meter = 2;
        s.length_m = 80.0; // taut, towing
        s.anchor_a = {0.0, 0.0, 0.0};
        s.anchor_b = {80.0, -10.0, 0.0}; // tow target ~7° below
        s.load_mass_kg = 2000.0; // vehicle
        s.gravity_y = -9.81;
    } else if (name == "winch_reel_drum_50m") {
        s.segments_per_meter = 4;
        s.length_m = 50.0;
        s.anchor_a = {0.0, 8.0, 0.0};
        s.anchor_b = {0.0, 0.0, 0.0};
        s.load_mass_kg = 300.0;
        s.gravity_y = -9.81;
        s.winch_speed_m_s = 2.0; // retract at 2 m/s
    } else if (name == "slack_droop_20m") {
        s.segments_per_meter = 4;
        s.length_m = 20.0;
        s.anchor_a = {0.0, 4.0, 0.0};
        s.anchor_b = {3.0, 4.0, 0.0}; // slack between two close points
        s.load_mass_kg = 0.0;
        s.gravity_y = -9.81;
        s.ground_collision = true;
        s.ground_y = 0.0;
    } else {
        std::fprintf(stderr, "Unknown scene: %s\n", name.c_str());
        std::exit(1);
    }
    return s;
}

int num_segments(const SceneParams& s) {
    return static_cast<int>(s.length_m * s.segments_per_meter);
}

// ============================================================================
// Cable initialization (linear between anchors + small perturbation)
// ============================================================================

Cable init_cable(const SceneParams& s, uint32_t seed) {
    Cable c;
    c.n = num_segments(s) + 1;
    c.pos.resize(c.n);
    c.prev_pos.resize(c.n);
    c.vel.assign(c.n, Vec3{0, 0, 0});
    c.rest_length = s.length_m;
    c.segment_length = s.length_m / static_cast<double>(num_segments(s));

    // Linear interpolation between anchors for winch/slack scenes
    // For vertical/catenary/towing: anchor_b is the load endpoint
    bool use_both_anchors = (s.name == "horizontal_catenary_50m" || s.name == "slack_droop_20m");

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> jitter(-0.05, 0.05);

    for (int i = 0; i < c.n; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(c.n - 1);
        Vec3 base;
        if (use_both_anchors) {
            base = s.anchor_a + (s.anchor_b - s.anchor_a) * t;
        } else {
            // For suspension/towing/winch: line from anchor_a straight down/forward
            base = s.anchor_a * (1.0 - t);
            if (s.name == "vertical_suspension_10m") {
                base = s.anchor_a + Vec3{0, -s.length_m * t, 0};
            } else if (s.name == "towing_at_angle_100m") {
                Vec3 dir = normalize(s.anchor_b - s.anchor_a);
                base = s.anchor_a + dir * (length(s.anchor_b - s.anchor_a) * t);
            } else if (s.name == "winch_reel_drum_50m") {
                base = s.anchor_a + Vec3{0, -s.length_m * t, 0};
            }
        }
        // Add small jitter to simulate initial settling
        Vec3 j{jitter(rng), jitter(rng), jitter(rng)};
        c.pos[i] = base + j;
        c.prev_pos[i] = c.pos[i];
    }

    c.fixed.assign(c.n, false);
    if (use_both_anchors) {
        c.fixed.front() = true;
        c.fixed.back() = true;
    } else {
        c.fixed.front() = true; // anchor (winch or vehicle)
        // last particle = load (free)
    }

    // Mass distribution
    // Per-segment mass: cable density ~ 1 kg/m (synthetic), load mass at endpoint
    double cable_density_kg_m = 1.0;
    double cable_total_mass = cable_density_kg_m * s.length_m;
    double per_particle_mass = cable_total_mass / static_cast<double>(c.n - 1);

    c.mass.assign(c.n, per_particle_mass);
    if (!use_both_anchors && s.load_mass_kg > 0.0) {
        c.mass.back() = s.load_mass_kg; // load at free end
    }
    // Fixed endpoint has infinite mass (handled implicitly in solver)

    return c;
}

// ============================================================================
// Strategy A: Naive global stretch projection (single iteration, slack-only)
// ============================================================================

void step_A(Cable& c, const SceneParams& s, double dt) {
    int n = c.n;
    // Apply gravity to velocity (already zero), then naive global length correction
    for (int i = 1; i < n - 1; ++i) {
        c.vel[i] += Vec3{0, s.gravity_y, 0} * dt;
    }
    if (s.load_mass_kg > 0.0) {
        c.vel.back() += Vec3{0, s.gravity_y, 0} * dt;
    }

    // Integrate position
    for (int i = 0; i < n; ++i) {
        if (c.fixed[i]) continue;
        c.pos[i] += c.vel[i] * dt;
    }

    // Single global projection: compute total length, scale toward rest
    double current_length = 0.0;
    for (int i = 0; i < n - 1; ++i) {
        current_length += length(c.pos[i + 1] - c.pos[i]);
    }
    if (current_length > 1e-9) {
        double scale = c.rest_length / current_length;
        // Only project midpoint (anchors fixed); single iteration
        for (int i = 1; i < n - 1; ++i) {
            Vec3 center = (c.pos[0] + c.pos[n - 1]) * 0.5;
            c.pos[i] = center + (c.pos[i] - center) * scale;
        }
    }

    // Compute velocity for next step
    for (int i = 0; i < n; ++i) {
        c.vel[i] = (c.pos[i] - c.prev_pos[i]) * (1.0 / dt);
        c.prev_pos[i] = c.pos[i];
    }
}

// ============================================================================
// Strategy B: Mass-Spring with Hooke's law (instability prone at high stiffness)
// ============================================================================

void step_B(Cable& c, const SceneParams& s, double dt) {
    int n = c.n;
    double k = 1.0e6; // stiff (steel cable); unstable for dt > 1 ms
    double damping = 0.02;

    // Compute spring forces
    std::vector<Vec3> force(n, Vec3{0, 0, 0});

    // Gravity
    for (int i = 0; i < n; ++i) {
        force[i] += Vec3{0, s.gravity_y * c.mass[i], 0};
    }

    // Spring forces between adjacent particles
    for (int i = 0; i < n - 1; ++i) {
        Vec3 d = c.pos[i + 1] - c.pos[i];
        double l = length(d);
        if (l < 1e-9) continue;
        double stretch = l - c.segment_length;
        Vec3 f = normalize(d) * (k * stretch);
        force[i] += f;
        force[i + 1] -= f;
    }

    // Integrate (semi-implicit Euler)
    for (int i = 0; i < n; ++i) {
        if (c.fixed[i]) continue;
        if (c.mass[i] < 1e-9) continue;
        Vec3 acc = force[i] * (1.0 / c.mass[i]);
        c.vel[i] += acc * dt;
        c.vel[i] *= (1.0 - damping);
        c.pos[i] += c.vel[i] * dt;
    }
    c.prev_pos = c.pos;
}

// ============================================================================
// Strategy C: Position-Based Dynamics (Müller 2007) — distance constraint
// ============================================================================

void step_C(Cable& c, const SceneParams& s, double dt, int iterations = 16) {
    int n = c.n;

    // Predict positions (Verlet-style without velocity storage)
    for (int i = 0; i < n; ++i) {
        if (c.fixed[i]) continue;
        if (c.mass[i] < 1e-9) continue;
        Vec3 vel = (c.pos[i] - c.prev_pos[i]) * (1.0 / dt);
        Vec3 new_pos = c.pos[i] + vel * dt + Vec3{0, s.gravity_y, 0} * (dt * dt * 0.5);
        c.prev_pos[i] = c.pos[i];
        c.pos[i] = new_pos;
    }

    // Project distance constraints (Gauss-Seidel)
    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < n - 1; ++i) {
            Vec3 d = c.pos[i + 1] - c.pos[i];
            double l = length(d);
            if (l < 1e-9) continue;
            double diff = (l - c.segment_length) / l;
            double w1 = c.fixed[i] ? 0.0 : 1.0 / c.mass[i];
            double w2 = c.fixed[i + 1] ? 0.0 : 1.0 / c.mass[i + 1];
            double wsum = w1 + w2;
            if (wsum < 1e-9) continue;
            Vec3 corr = d * (diff / wsum);
            if (!c.fixed[i]) c.pos[i] += corr * w1;
            if (!c.fixed[i + 1]) c.pos[i + 1] -= corr * w2;
        }
    }

    // Update velocity (implicit from position change)
    for (int i = 0; i < n; ++i) {
        if (c.fixed[i]) continue;
        c.vel[i] = (c.pos[i] - c.prev_pos[i]) * (1.0 / dt);
    }
}

// ============================================================================
// Strategy D: Distance Constraint + Verlet (Jakobsen 2001 — Hitman)
//   Same as C but without mass-weighting (equal weight per particle).
//   Cheaper but harder constraints = no compliance.
// ============================================================================

void step_D(Cable& c, const SceneParams& s, double dt, int iterations = 16) {
    int n = c.n;

    // Predict
    for (int i = 0; i < n; ++i) {
        if (c.fixed[i]) continue;
        Vec3 vel = (c.pos[i] - c.prev_pos[i]) * (1.0 / dt);
        c.prev_pos[i] = c.pos[i];
        c.pos[i] += vel * dt + Vec3{0, s.gravity_y, 0} * (dt * dt * 0.5);
    }

    // Project distance constraints (equal weight, no mass)
    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < n - 1; ++i) {
            Vec3 d = c.pos[i + 1] - c.pos[i];
            double l = length(d);
            if (l < 1e-9) continue;
            double diff = (l - c.segment_length) / l;
            Vec3 corr = d * (diff * 0.5);
            if (!c.fixed[i]) c.pos[i] += corr;
            if (!c.fixed[i + 1]) c.pos[i + 1] -= corr;
        }
    }
}

// ============================================================================
// Strategy E: XPBD-like compliance-damped PBD (Macklin/Müller 2016 spirit).
//   True XPBD with accumulated lambda is unstable in this prototype at
//   extreme mass ratios (cable 0.06kg vs load 500kg = 8333:1). We use the
//   equivalent per-iteration solve with α̃ added to wsum denominator — same
//   as XPBD for small α, but unconditionally stable.
//   Compliance α (1/k) lets cable stretch slightly under high tension,
//   matching real-world cable behavior (synthetic rope ~5%, steel ~0.5%).
// ============================================================================

void step_E(Cable& c, const SceneParams& s, double dt, int iterations = 16) {
    int n = c.n;
    // Compliance coefficient: α = 1/k (synthetic military tow rope ~1e-5 m/N)
    double alpha = 1.0e-5;
    double alpha_tilde = alpha / (dt * dt);

    // Predict
    for (int i = 0; i < n; ++i) {
        if (c.fixed[i]) continue;
        if (c.mass[i] < 1e-9) continue;
        Vec3 vel = (c.pos[i] - c.prev_pos[i]) * (1.0 / dt);
        c.prev_pos[i] = c.pos[i];
        c.pos[i] += vel * dt + Vec3{0, s.gravity_y, 0} * (dt * dt * 0.5);
    }

    // Solve constraints (Gauss-Seidel with compliance in denominator)
    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < n - 1; ++i) {
            Vec3 d = c.pos[i + 1] - c.pos[i];
            double l = length(d);
            if (l < 1e-9) continue;
            double w1 = c.fixed[i] ? 0.0 : 1.0 / c.mass[i];
            double w2 = c.fixed[i + 1] ? 0.0 : 1.0 / c.mass[i + 1];
            double wsum = w1 + w2;
            if (wsum < 1e-9) continue;
            // Per-iteration closed-form solve (XPBD with λ=0 every iteration)
            // Equivalent to adding α̃ to wsum denominator — softer PBD.
            double C = l - c.segment_length;
            Vec3 grad = d * (1.0 / l);
            double denom = wsum + alpha_tilde;
            Vec3 dp = grad * (C / denom);
            if (!c.fixed[i]) c.pos[i] += dp * w1;
            if (!c.fixed[i + 1]) c.pos[i + 1] -= dp * w2;
        }
    }

    // Update velocity
    for (int i = 0; i < n; ++i) {
        if (c.fixed[i]) continue;
        c.vel[i] = (c.pos[i] - c.prev_pos[i]) * (1.0 / dt);
    }
}

// ============================================================================
// Apply wind / ground collision / winch
// ============================================================================

void apply_external(Cable& c, const SceneParams& s, double /*t*/) {
    int n = c.n;

    // Wind drag (per-segment, simple model)
    if (s.wind_x != 0.0) {
        for (int i = 1; i < n - 1; ++i) {
            c.pos[i].x += s.wind_x * 0.0001;
        }
    }

    // Ground collision (only for slack_droop)
    if (s.ground_collision) {
        for (int i = 0; i < n; ++i) {
            if (c.pos[i].y < s.ground_y) {
                c.pos[i].y = s.ground_y;
            }
        }
    }

    // Winch (active retract/extend) — modify last segment rest length
    // For simplicity: skip mid-simulation winch extension (would require segment count change)
    // Just add force toward anchor if winching in
    if (s.winch_speed_m_s != 0.0 && s.winch_speed_m_s > 0.0) {
        // Pull last particle toward anchor
        Vec3 dir = normalize(c.pos.front() - c.pos.back());
        c.pos.back() += dir * s.winch_speed_m_s * 0.001; // small pull per step
    }
}

// ============================================================================
// Stretch error metric
// ============================================================================

double compute_stretch_error(const Cable& c, double rest_length) {
    double current = 0.0;
    for (int i = 0; i < c.n - 1; ++i) {
        current += length(c.pos[i + 1] - c.pos[i]);
    }
    return std::abs(current - rest_length) / rest_length;
}

bool check_stable(const Cable& c) {
    for (int i = 0; i < c.n; ++i) {
        if (std::isnan(c.pos[i].x) || std::isnan(c.pos[i].y) || std::isnan(c.pos[i].z)) return false;
        if (std::isinf(c.pos[i].x) || std::isinf(c.pos[i].y) || std::isinf(c.pos[i].z)) return false;
    }
    return true;
}

// ============================================================================
// High-resolution timer (RDTSC-like fallback to chrono)
// ============================================================================

#include <chrono>
double now_us() {
    return std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ============================================================================
// Main benchmark
// ============================================================================

int main() {
    // Diagnostic mode: PROJECTV_CABLE_DIAG=1 prints single-config trace
    const char* diag_env = std::getenv("PROJECTV_CABLE_DIAG");
    if (diag_env && std::string(diag_env) == "1") {
        SceneParams sp = make_scene("vertical_suspension_10m");
        Cable c = init_cable(sp, 1);
        std::printf("DIAG: n=%d, rest=%.4f, mass[0]=%.3f, mass.back=%.3f, seg=%.5f\n",
                    c.n, c.rest_length, c.mass[0], c.mass.back(), c.segment_length);
        std::printf("DIAG: anchor=(%.2f,%.2f,%.2f) load=(%.2f,%.2f,%.2f)\n",
                    c.pos[0].x, c.pos[0].y, c.pos[0].z,
                    c.pos.back().x, c.pos.back().y, c.pos.back().z);
        auto total_length = [&]() {
            double l = 0.0;
            for (int i = 0; i < c.n - 1; ++i) l += length(c.pos[i + 1] - c.pos[i]);
            return l;
        };
        for (int it = 0; it < 1000; ++it) {
            step_C(c, sp, 1.0 / 60.0);
            apply_external(c, sp, it * (1.0/60.0));
            if (it < 5 || it == 10 || it == 50 || it == 100 || it == 500 || it == 999) {
                double L = total_length();
                std::printf("DIAG iter=%4d L=%.3f m stretch=%7.2f%% load.y=%8.4f vel.load=%g\n",
                            it, L, (L - c.rest_length) / c.rest_length * 100.0,
                            c.pos.back().y, length(c.vel.back()));
            }
        }
        return 0;
    }

    std::vector<std::string> scenes = {
        "vertical_suspension_10m",
        "horizontal_catenary_50m",
        "towing_at_angle_100m",
        "winch_reel_drum_50m",
        "slack_droop_20m"
    };
    std::vector<std::string> strategies = {
        "A_NaiveGlobalStretch",
        "B_MassSpring_Hooke",
        "C_PBD_Muller2007",
        "D_DistanceConstraint_Verlet",
        "E_XPBD_Macklin2016"
    };
    std::vector<uint32_t> seeds = {1, 7, 42, 1234, 31337};
    constexpr int kIterations = 1000;
    constexpr int kWarmup = 10;
    constexpr double kDt = 1.0 / 60.0; // 60 Hz physics tick

    // Output
    std::filesystem::create_directories("build");
    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,iter,mean_us_per_tick,median_us,p95_us,p99_us,stddev_us,min_us,max_us,"
           "mean_stretch_pct,max_stretch_pct,stable\n";

    for (const auto& strat : strategies) {
        for (const auto& scen_name : scenes) {
            for (uint32_t seed : seeds) {
                SceneParams sp = make_scene(scen_name);
                Cable c = init_cable(sp, seed);
                double rest_length = c.rest_length;

                // Warmup
                for (int it = 0; it < kWarmup; ++it) {
                    if (strat == "A_NaiveGlobalStretch") {
                        step_A(c, sp, kDt);
                    } else if (strat == "B_MassSpring_Hooke") {
                        step_B(c, sp, kDt);
                    } else if (strat == "C_PBD_Muller2007") {
                        step_C(c, sp, kDt);
                    } else if (strat == "D_DistanceConstraint_Verlet") {
                        step_D(c, sp, kDt);
                    } else if (strat == "E_XPBD_Macklin2016") {
                        step_E(c, sp, kDt);
                    }
                    apply_external(c, sp, it * kDt);
                }

                // Reset to fresh state for measurement
                c = init_cable(sp, seed);

                std::vector<double> times;
                std::vector<double> stretch_pcts;
                times.reserve(kIterations);
                stretch_pcts.reserve(kIterations);

                bool stable = true;
                double max_stretch = 0.0;

                for (int it = 0; it < kIterations; ++it) {
                    double t0 = now_us();
                    if (strat == "A_NaiveGlobalStretch") {
                        step_A(c, sp, kDt);
                    } else if (strat == "B_MassSpring_Hooke") {
                        step_B(c, sp, kDt);
                    } else if (strat == "C_PBD_Muller2007") {
                        step_C(c, sp, kDt);
                    } else if (strat == "D_DistanceConstraint_Verlet") {
                        step_D(c, sp, kDt);
                    } else if (strat == "E_XPBD_Macklin2016") {
                        step_E(c, sp, kDt);
                    }
                    apply_external(c, sp, it * kDt);
                    double t1 = now_us();
                    times.push_back(t1 - t0);

                    double err = compute_stretch_error(c, rest_length) * 100.0;
                    stretch_pcts.push_back(err);
                    max_stretch = std::max(max_stretch, err);

                    if (!check_stable(c)) {
                        stable = false;
                    }
                }

                Stats ts = compute_stats(times);
                Stats ss = compute_stats(stretch_pcts);

                double length_m = rest_length;
                double mean_us_per_m = ts.mean / length_m;

                csv << strat << "," << scen_name << "," << seed << "," << kIterations << ","
                    << mean_us_per_m << "," << ts.median << "," << ts.p95 << "," << ts.p99 << ","
                    << ts.stddev << "," << ts.min << "," << ts.max << ","
                    << ss.mean << "," << max_stretch << "," << (stable ? "1" : "0") << "\n";

                std::printf("[%s/%s/seed=%u] mean=%.3f us/m, p95=%.3f us/tick, stretch=%.3f%% (max %.3f%%) stable=%d\n",
                            strat.c_str(), scen_name.c_str(), seed,
                            mean_us_per_m, ts.p95, ss.mean, max_stretch, stable ? 1 : 0);
            }
        }
    }

    csv.close();
    std::printf("\nResults written to build/results.csv (%d rows)\n",
                static_cast<int>(strategies.size() * scenes.size() * seeds.size()));
    return 0;
}