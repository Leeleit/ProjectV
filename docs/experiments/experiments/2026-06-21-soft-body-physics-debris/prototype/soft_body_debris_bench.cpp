// SPDX-License-Identifier: MIT
// 2026-06-21-soft-body-physics-debris — standalone C++26 CPU benchmark.
// Measures per-tick cost of 5 soft-body strategies on cloth panels (32-128 vertices)
// across 5 scenes with 3 panel sizes and 5 seeds. Standalone prototype per
// `docs/experiments/AGENTS.md §1` (build dir inside prototype/, no mainline pollution).
//
// Build: clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic
// Run: ./prototype/build/soft_body_debris_bench

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sbd {

// --- 3D vector (SoA-friendly POD) ---------------------------------------
struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
    constexpr Vec3 operator+(Vec3 o) const noexcept { return {x+o.x, y+o.y, z+o.z}; }
    constexpr Vec3 operator-(Vec3 o) const noexcept { return {x-o.x, y-o.y, z-o.z}; }
    constexpr Vec3 operator*(double s) const noexcept { return {x*s, y*s, z*s}; }
    constexpr Vec3 operator/(double s) const noexcept { return {x/s, y/s, z/s}; }
    Vec3& operator+=(Vec3 o) noexcept { x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3& operator-=(Vec3 o) noexcept { x-=o.x; y-=o.y; z-=o.z; return *this; }
    Vec3& operator*=(double s) noexcept { x*=s; y*=s; z*=s; return *this; }
    Vec3& operator/=(double s) noexcept { x/=s; y/=s; z/=s; return *this; }
};

inline double dot(Vec3 a, Vec3 b) noexcept { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline double length(Vec3 v) noexcept { return std::sqrt(dot(v,v)); }
inline Vec3 normalize(Vec3 v) noexcept { double L = length(v); return L>1e-12 ? v*(1.0/L) : Vec3{0,0,0}; }

// --- Statistics (per `benchmarks/methodology.md §7`) ---------------------
struct Stats {
    double mean{0.0};
    double median{0.0};
    double p95{0.0};
    double p99{0.0};
    double stddev{0.0};
    double minv{0.0};
    double maxv{0.0};
};

Stats compute_stats(std::span<const double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / samples.size();
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.minv = sorted.front();
    s.maxv = sorted.back();
    return s;
}

// --- Distance constraint (SoA) ------------------------------------------
struct Constraint {
    uint32_t a{0};
    uint32_t b{0};
    double rest_length{0.0};
    double stiffness{0.0};      // 0..1 (1 = rigid)
    double compliance{0.0};     // XPBD: 1/(k*dt^2)
};

// --- Panel: vertices + constraints + anchors ----------------------------
struct Panel {
    std::vector<Vec3> pos;
    std::vector<Vec3> vel;
    std::vector<Vec3> pred_pos;   // for PBD/XPBD predicted position cache
    std::vector<double> inv_mass;
    std::vector<Constraint> constraints;
    std::vector<uint8_t> is_anchor;  // 1 = kinematic, 0 = dynamic
    Vec3 gravity{0.0, -9.81, 0.0};
    Vec3 wind{0.0, 0.0, 0.0};
    double dt{1.0/60.0};
    double stretch_ratio{0.0};   // max stretch / rest across constraints
    int iter_used{0};

    [[nodiscard]] uint32_t n_verts() const noexcept { return static_cast<uint32_t>(pos.size()); }
    [[nodiscard]] uint32_t n_constraints() const noexcept { return static_cast<uint32_t>(constraints.size()); }
};

// --- Panel generation: square cloth grid with structural + shear + bend constraints -----
// Topology: (N x N) grid, structural (4-neighbor horizontal/vertical), shear (4-neighbor diagonals),
// bend (skip-1 in each direction). Anchors = top row corners.
Panel make_panel(uint32_t n_grid, double size_m, std::mt19937& rng, double tear_prob) {
    Panel p;
    const uint32_t n = n_grid;
    const double dx = size_m / static_cast<double>(n - 1);
    p.pos.reserve(n*n);
    p.vel.assign(n*n, Vec3{0,0,0});
    p.pred_pos.assign(n*n, Vec3{0,0,0});
    p.inv_mass.assign(n*n, 1.0);  // unit mass, 1.0 kg -> invMass = 1
    p.is_anchor.assign(n*n, 0);
    for (uint32_t r = 0; r < n; ++r) {
        for (uint32_t c = 0; c < n; ++c) {
            const double x = static_cast<double>(c) * dx - size_m*0.5;
            const double y = 0.0;
            const double z = static_cast<double>(r) * dx - size_m*0.5;
            p.pos.push_back({x, y, z});
        }
    }
    // Anchor: top row (r = n-1) at corners c=0 and c=n-1
    p.is_anchor[(n-1)*n + 0] = 1;
    p.is_anchor[(n-1)*n + (n-1)] = 1;
    p.inv_mass[(n-1)*n + 0] = 0.0;
    p.inv_mass[(n-1)*n + (n-1)] = 0.0;

    auto idx = [n](uint32_t r, uint32_t c) { return r*n + c; };
    // Structural (4-neighbor)
    for (uint32_t r = 0; r < n; ++r) {
        for (uint32_t c = 0; c < n; ++c) {
            if (c+1 < n) p.constraints.push_back({idx(r,c), idx(r,c+1), dx, 0.9, 0.0});
            if (r+1 < n) p.constraints.push_back({idx(r,c), idx(r+1,c), dx, 0.9, 0.0});
        }
    }
    // Shear (4-neighbor diagonals)
    const double diag = dx * std::sqrt(2.0);
    for (uint32_t r = 0; r + 1 < n; ++r) {
        for (uint32_t c = 0; c + 1 < n; ++c) {
            p.constraints.push_back({idx(r,c), idx(r+1,c+1), diag, 0.7, 0.0});
            p.constraints.push_back({idx(r+1,c), idx(r,c+1), diag, 0.7, 0.0});
        }
    }
    // Bend (skip-1)
    const double two_dx = 2.0 * dx;
    for (uint32_t r = 0; r < n; ++r) {
        for (uint32_t c = 0; c + 2 < n; ++c) {
            p.constraints.push_back({idx(r,c), idx(r,c+2), two_dx, 0.4, 0.0});
        }
    }
    for (uint32_t r = 0; r + 2 < n; ++r) {
        for (uint32_t c = 0; c < n; ++c) {
            p.constraints.push_back({idx(r,c), idx(r+2,c), two_dx, 0.4, 0.0});
        }
    }
    // Tearing: random constraints get very low stiffness (or skip)
    if (tear_prob > 0.0) {
        std::uniform_real_distribution<double> u(0.0, 1.0);
        for (auto& c : p.constraints) {
            if (u(rng) < tear_prob) c.stiffness = 0.01;  // near-broken
        }
    }
    return p;
}

}  // namespace sbd

// --- Strategies -----------------------------------------------------------
namespace sbd::strat {

// A_RigidProxy: zero-cost baseline (no per-vert work). Trivially correct.
inline void A_RigidProxy(Panel& /*p*/) noexcept {
    // No-op: rigid body proxy simulated elsewhere (e.g. closed `tank-terrain-interaction-physics`)
    return;
}

// B_MassSpring: Hooke's law springs + explicit Euler integration (semi-implicit).
// Per closed `xpbd-vs-mass-spring` 2024-2026 production commentary: cheap, instability-prone.
inline void B_MassSpring(Panel& p) noexcept {
    const double dt = p.dt;
    const Vec3 g = p.gravity;
    const Vec3 w = p.wind;
    const double k_spring = 5000.0;     // spring stiffness N/m
    const double damping = 0.98;        // velocity damping
    // 1) Apply forces and integrate velocity
    std::vector<Vec3> force(p.n_verts(), Vec3{0,0,0});
    for (uint32_t i = 0; i < p.n_verts(); ++i) {
        if (p.is_anchor[i]) continue;
        // gravity + wind
        force[i] += g * (1.0 / p.inv_mass[i]);
        force[i] += w * (1.0 / p.inv_mass[i]);
    }
    // Spring forces
    for (const auto& c : p.constraints) {
        if (c.stiffness <= 0.0) continue;
        const Vec3 d = p.pos[c.b] - p.pos[c.a];
        const double L = length(d);
        if (L < 1e-12) continue;
        const double f = k_spring * c.stiffness * (L - c.rest_length);
        const Vec3 dir = d * (1.0 / L);
        if (!p.is_anchor[c.a]) force[c.a] += dir * f;
        if (!p.is_anchor[c.b]) force[c.b] -= dir * f;
    }
    // Integrate v and x
    for (uint32_t i = 0; i < p.n_verts(); ++i) {
        if (p.is_anchor[i]) continue;
        p.vel[i] = p.vel[i] * damping + force[i] * (dt * p.inv_mass[i]);
        p.pos[i] += p.vel[i] * dt;
    }
}

// C_PBD: position projection (Müller 2007) with N iterations.
inline void C_PBD(Panel& p) noexcept {
    const double dt = p.dt;
    const Vec3 g = p.gravity;
    const Vec3 w = p.wind;
    constexpr int kIter = 8;
    // 1) Predict positions
    for (uint32_t i = 0; i < p.n_verts(); ++i) {
        if (p.is_anchor[i]) { p.pred_pos[i] = p.pos[i]; continue; }
        p.vel[i] += (g + w) * dt;
        p.pred_pos[i] = p.pos[i] + p.vel[i] * dt;
    }
    // 2) Project constraints
    for (int it = 0; it < kIter; ++it) {
        for (const auto& c : p.constraints) {
            if (c.stiffness <= 0.0) continue;
            const Vec3 d = p.pred_pos[c.b] - p.pred_pos[c.a];
            const double L = length(d);
            if (L < 1e-12) continue;
            const double wsum = p.inv_mass[c.a] + p.inv_mass[c.b];
            if (wsum < 1e-12) continue;
            const double C = L - c.rest_length;
            const Vec3 dir = d * (1.0 / L);
            const double s = c.stiffness * C / wsum;
            const Vec3 corr = dir * s;
            p.pred_pos[c.a] += corr * p.inv_mass[c.a];
            p.pred_pos[c.b] -= corr * p.inv_mass[c.b];
        }
    }
    // 3) Update velocity and position
    double max_stretch = 0.0;
    for (uint32_t i = 0; i < p.n_verts(); ++i) {
        if (p.is_anchor[i]) continue;
        p.vel[i] = (p.pred_pos[i] - p.pos[i]) / dt;
        p.pos[i] = p.pred_pos[i];
    }
    for (const auto& c : p.constraints) {
        const double L = length(p.pos[c.b] - p.pos[c.a]);
        const double ratio = (c.rest_length > 0.0) ? (L / c.rest_length - 1.0) : 0.0;
        if (std::abs(ratio) > std::abs(max_stretch)) max_stretch = ratio;
    }
    p.stretch_ratio = max_stretch;
    p.iter_used = kIter;
}

// D_XPBD: extended PBD with compliance (Macklin 2016).
// Per Macklin 2016: Δp = -((C + α·λ_old) / (|∇C|² + α·k_eff)) · ∇C.
// For distance constraints: wsum_eff = wsum + α; λ is constraint Lagrange multiplier.
inline void D_XPBD(Panel& p) noexcept {
    const double dt = p.dt;
    const Vec3 g = p.gravity;
    const Vec3 w = p.wind;
    constexpr int kIter = 8;
    // 1) Predict positions
    for (uint32_t i = 0; i < p.n_verts(); ++i) {
        if (p.is_anchor[i]) { p.pred_pos[i] = p.pos[i]; continue; }
        p.vel[i] += (g + w) * dt;
        p.pred_pos[i] = p.pos[i] + p.vel[i] * dt;
    }
    // 2) Project constraints with compliance
    for (int it = 0; it < kIter; ++it) {
        for (const auto& c : p.constraints) {
            if (c.stiffness <= 0.0) continue;
            const Vec3 d = p.pred_pos[c.b] - p.pred_pos[c.a];
            const double L = length(d);
            if (L < 1e-12) continue;
            const double wsum = p.inv_mass[c.a] + p.inv_mass[c.b];
            if (wsum < 1e-12) continue;
            // compliance: α = 1 / (k · dt²).  Here k = stiffness*1e5, dt=1/60.
            const double alpha = 1.0 / (c.stiffness * 1.0e5 * dt * dt);
            const double C = L - c.rest_length;
            const Vec3 dir = d * (1.0 / L);
            // XPBD: divide by wsum + alpha/k (k_constraint = stiffness * 1e5)
            const double denom = wsum + alpha / (c.stiffness * 1.0e5);
            const double s = C / denom;
            const Vec3 corr = dir * s;
            p.pred_pos[c.a] += corr * p.inv_mass[c.a];
            p.pred_pos[c.b] -= corr * p.inv_mass[c.b];
        }
    }
    // 3) Update velocity and position
    double max_stretch = 0.0;
    for (uint32_t i = 0; i < p.n_verts(); ++i) {
        if (p.is_anchor[i]) continue;
        p.vel[i] = (p.pred_pos[i] - p.pos[i]) / dt;
        p.pos[i] = p.pred_pos[i];
    }
    for (const auto& c : p.constraints) {
        const double L = length(p.pos[c.b] - p.pos[c.a]);
        const double ratio = (c.rest_length > 0.0) ? (L / c.rest_length - 1.0) : 0.0;
        if (std::abs(ratio) > std::abs(max_stretch)) max_stretch = ratio;
    }
    p.stretch_ratio = max_stretch;
    p.iter_used = kIter;
}

// E_ProjectiveDynamics: simplified global/local solver (Bouaziz 2014).
// Analytical proxy: skip full Cholesky global step (cost O(n^3) per iter);
// instead approximate global step as identity (q_new = b) and do local projections only.
// This is intentionally cheap — measures the local-step cost of PD.
inline void E_ProjectiveDynamics(Panel& p) noexcept {
    const double dt = p.dt;
    const Vec3 g = p.gravity;
    const Vec3 w = p.wind;
    constexpr int kIter = 10;
    // 1) Predict positions (same as PBD/XPBD)
    for (uint32_t i = 0; i < p.n_verts(); ++i) {
        if (p.is_anchor[i]) { p.pred_pos[i] = p.pos[i]; continue; }
        p.vel[i] += (g + w) * dt;
        p.pred_pos[i] = p.pos[i] + p.vel[i] * dt;
    }
    // 2) Global step: analytical proxy = position from inertia (q_new = M^-1 · F).
    // Simplified: copy pred to working set.
    for (uint32_t i = 0; i < p.n_verts(); ++i) {
        // q is in pred_pos after global step (skip Cholesky for analytical speed)
    }
    // 3) Local step: constraint projection (same as PBD but 10 iters, lower stiffness)
    for (int it = 0; it < kIter; ++it) {
        for (const auto& c : p.constraints) {
            if (c.stiffness <= 0.0) continue;
            const Vec3 d = p.pred_pos[c.b] - p.pred_pos[c.a];
            const double L = length(d);
            if (L < 1e-12) continue;
            const double wsum = p.inv_mass[c.a] + p.inv_mass[c.b];
            if (wsum < 1e-12) continue;
            const double C = L - c.rest_length;
            const Vec3 dir = d * (1.0 / L);
            const double s = c.stiffness * C / wsum;
            const Vec3 corr = dir * s;
            p.pred_pos[c.a] += corr * p.inv_mass[c.a];
            p.pred_pos[c.b] -= corr * p.inv_mass[c.b];
        }
    }
    // 4) Update velocity and position
    double max_stretch = 0.0;
    for (uint32_t i = 0; i < p.n_verts(); ++i) {
        if (p.is_anchor[i]) continue;
        p.vel[i] = (p.pred_pos[i] - p.pos[i]) / dt;
        p.pos[i] = p.pred_pos[i];
    }
    for (const auto& c : p.constraints) {
        const double L = length(p.pos[c.b] - p.pos[c.a]);
        const double ratio = (c.rest_length > 0.0) ? (L / c.rest_length - 1.0) : 0.0;
        if (std::abs(ratio) > std::abs(max_stretch)) max_stretch = ratio;
    }
    p.stretch_ratio = max_stretch;
    p.iter_used = kIter;
}

}  // namespace sbd::strat

// --- Scenes ----------------------------------------------------------------
namespace sbd::scene {

enum class SceneId : int { CalmStatic = 0, Breeze3ms = 1, Wind15ms = 2, ImpactCollapse = 3, TearingLocalized = 4 };
constexpr std::array<std::string_view, 5> kSceneNames = {
    "calm_static", "breeze_3ms", "wind_15ms", "impact_collapse", "tearing_localized"
};
constexpr std::array<double, 5> kWindSpeeds = {0.0, 3.0, 15.0, 5.0, 8.0};

void setup_panel(Panel& p, SceneId scene, std::mt19937& rng) {
    // rng reserved for future per-scene jitter (e.g. wind turbulence)
    (void)rng;
    switch (scene) {
        case SceneId::CalmStatic:
            p.gravity = {0, -9.81, 0};
            p.wind = {0, 0, 0};
            break;
        case SceneId::Breeze3ms:
            p.gravity = {0, -9.81, 0};
            p.wind = {kWindSpeeds[1], 0, 0};
            break;
        case SceneId::Wind15ms:
            p.gravity = {0, -9.81, 0};
            p.wind = {kWindSpeeds[2], 0, 0};
            break;
        case SceneId::ImpactCollapse:
            p.gravity = {0, -9.81, 0};
            p.wind = {kWindSpeeds[3], 0, 0};
            break;
        case SceneId::TearingLocalized:
            p.gravity = {0, -9.81, 0};
            p.wind = {kWindSpeeds[4], 0, 0};
            break;
    }
}

}  // namespace sbd::scene

// --- Strategy dispatch -----------------------------------------------------
namespace sbd {

using StrategyFn = void(*)(Panel&);
constexpr std::array<StrategyFn, 5> kStrategies = {
    &strat::A_RigidProxy,
    &strat::B_MassSpring,
    &strat::C_PBD,
    &strat::D_XPBD,
    &strat::E_ProjectiveDynamics
};
constexpr std::array<std::string_view, 5> kStrategyNames = {
    "A_RigidProxy", "B_MassSpring", "C_PBD", "D_XPBD", "E_ProjectiveDynamics"
};

// --- Benchmark harness -----------------------------------------------------
struct BenchResult {
    std::string strategy;
    std::string scene;
    uint32_t n_verts{0};
    uint32_t seed{0};
    Stats us_per_tick;
    double max_stretch{0.0};
    int iter_used{0};
};

BenchResult run_bench(int strat_idx, int scene_idx, uint32_t n_grid, uint32_t seed) {
    using clock = std::chrono::high_resolution_clock;
    std::mt19937 rng_init(seed);
    const double tear_prob = (scene_idx == 4) ? 0.05 : 0.0;
    Panel p = make_panel(n_grid, 1.0, rng_init, tear_prob);
    std::mt19937 rng(scene_idx * 1000 + seed);
    scene::setup_panel(p, static_cast<scene::SceneId>(scene_idx), rng);
    // For impact_collapse: drop one anchor mid-sim
    bool anchor_dropped = (scene_idx == 3);

    constexpr int kWarmup = 10;
    constexpr int kTimed = 1000;

    // Warmup
    for (int i = 0; i < kWarmup; ++i) {
        if (anchor_dropped && i == kWarmup / 2 && p.is_anchor[p.n_verts() - 1]) {
            p.is_anchor[p.n_verts() - 1] = 0;
            p.inv_mass[p.n_verts() - 1] = 1.0;
        }
        kStrategies[strat_idx](p);
    }

    // Timed runs
    std::vector<double> samples;
    samples.reserve(kTimed);
    double max_stretch = 0.0;
    int iter_used = 0;
    for (int i = 0; i < kTimed; ++i) {
        const auto t0 = clock::now();
        kStrategies[strat_idx](p);
        const auto t1 = clock::now();
        const double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        samples.push_back(us);
        max_stretch = std::max(max_stretch, std::abs(p.stretch_ratio));
        iter_used = p.iter_used;
    }
    BenchResult r;
    r.strategy = std::string(kStrategyNames[strat_idx]);
    r.scene = std::string(scene::kSceneNames[scene_idx]);
    r.n_verts = p.n_verts();
    r.seed = seed;
    r.us_per_tick = compute_stats(samples);
    r.max_stretch = max_stretch;
    r.iter_used = iter_used;
    return r;
}

void write_csv_header(std::ofstream& out) {
    out << "strategy,scene,n_verts,seed,mean_us,median_us,p95_us,p99_us,std_us,min_us,max_us,max_stretch,iter_used\n";
}

void write_csv_row(std::ofstream& out, const BenchResult& r) {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "%s,%s,%u,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.6f,%d\n",
        r.strategy.c_str(), r.scene.c_str(), r.n_verts, r.seed,
        r.us_per_tick.mean, r.us_per_tick.median, r.us_per_tick.p95, r.us_per_tick.p99,
        r.us_per_tick.stddev, r.us_per_tick.minv, r.us_per_tick.maxv,
        r.max_stretch, r.iter_used);
    out << buf;
}

}  // namespace sbd

int main() {
    using namespace sbd;
    // 5 strategies × 5 scenes × 3 panel_sizes × 5 seeds = 375 configs × 1000 iter = 375,000 main measurements
    constexpr std::array<uint32_t, 3> kPanelGrids = {6, 8, 11};  // 36, 64, 121 verts
    constexpr std::array<uint32_t, 5> kSeeds = {1, 7, 42, 1234, 31337};

    const std::string out_path = "prototype/build/results.csv";
    std::ofstream out(out_path);
    if (!out) {
        std::fprintf(stderr, "ERROR: cannot open %s for write\n", out_path.c_str());
        return 1;
    }
    write_csv_header(out);

    const auto t_start = std::chrono::high_resolution_clock::now();
    int config_count = 0;
    for (int s = 0; s < 5; ++s) {
        for (int sc = 0; sc < 5; ++sc) {
            for (uint32_t ng : kPanelGrids) {
                for (uint32_t seed : kSeeds) {
                    BenchResult r = run_bench(s, sc, ng, seed);
                    write_csv_row(out, r);
                    out.flush();
                    ++config_count;
                    if (config_count % 25 == 0) {
                        std::fprintf(stderr, "[%d/375] %s/%s/%u/%u mean=%.4f us max_stretch=%.4f\n",
                            config_count, r.strategy.c_str(), r.scene.c_str(), r.n_verts, r.seed,
                            r.us_per_tick.mean, r.max_stretch);
                    }
                }
            }
        }
    }
    out.close();
    const auto t_end = std::chrono::high_resolution_clock::now();
    const double wall_sec = std::chrono::duration<double>(t_end - t_start).count();
    std::fprintf(stderr, "Done: %d configs, wall time %.2f sec, output %s\n",
        config_count, wall_sec, out_path.c_str());
    return 0;
}


