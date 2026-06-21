// SPDX-License-Identifier: MIT
// 2026-06-21-wind-simulation-ballistics — standalone C++26 CPU prototype.
// AGENTS.md §8: no narrative comments in code (per COMMENTS.md discipline).
// Math sources: Stam SIGGRAPH 1999, Fedkiw 2001 vorticity confinement, Bridson 2007
// curl noise, Selle 2005. Public-domain analytical cost model — no ProjectV mainline.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace pv {

// --- minimal stats harness (per benchmarks/methodology.md §7) ----------------

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
};

Stats compute_stats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(0.95 * static_cast<double>(sorted.size()))];
    s.p99 = sorted[static_cast<size_t>(0.99 * static_cast<double>(sorted.size()))];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min = sorted.front();
    s.max = sorted.back();
    return s;
}

// --- 3D vector + grid utilities ----------------------------------------------

struct Vec3 {
    double x;
    double y;
    double z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(double a, double b, double c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
};

struct Grid3D {
    int nx, ny, nz;
    double cell_size_m;
    std::vector<Vec3> vel;
    std::vector<double> pressure;
    std::vector<double> div;

    int idx(int x, int y, int z) const {
        return (z * ny + y) * nx + x;
    }
    Vec3& at(int x, int y, int z) { return vel[idx(x, y, z)]; }
    const Vec3& at(int x, int y, int z) const { return vel[idx(x, y, z)]; }
    void resize() {
        vel.assign(static_cast<size_t>(nx) * ny * nz, Vec3{});
        pressure.assign(static_cast<size_t>(nx) * ny * nz, 0.0);
        div.assign(static_cast<size_t>(nx) * ny * nz, 0.0);
    }
};

// --- noise (Perlin 3D simplified) -------------------------------------------

double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
double lerp(double a, double b, double t) { return a + t * (b - a); }
double grad(int h, double x, double y, double z) {
    h &= 15;
    double u = h < 8 ? x : y;
    double v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

struct PermTable {
    int p[512];
    PermTable() = default;
    explicit PermTable(const std::array<int, 256>& src) {
        for (int i = 0; i < 512; ++i) p[i] = src[i & 255];
    }
};

std::array<int, 256> make_perm(uint64_t seed) {
    std::array<int, 256> p{};
    for (int i = 0; i < 256; ++i) p[i] = i;
    std::mt19937_64 rng(seed);
    std::shuffle(p.begin(), p.end(), rng);
    return p;
}

double perlin3(double x, double y, double z, const PermTable& p) {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;
    x -= std::floor(x); y -= std::floor(y); z -= std::floor(z);
    double u = fade(x), v = fade(y), w = fade(z);
    int A = p.p[X] + Y, AA = p.p[A] + Z, AB = p.p[A + 1] + Z;
    int B = p.p[X + 1] + Y, BA = p.p[B] + Z, BB = p.p[B + 1] + Z;
    return lerp(lerp(lerp(grad(p.p[AA], x, y, z), grad(p.p[BA], x - 1, y, z), u),
                     lerp(grad(p.p[AB], x, y - 1, z), grad(p.p[BB], x - 1, y - 1, z), u), v),
                 lerp(lerp(grad(p.p[AA + 1], x, y, z - 1), grad(p.p[BA + 1], x - 1, y, z - 1), u),
                     lerp(grad(p.p[AB + 1], x, y - 1, z - 1), grad(p.p[BB + 1], x - 1, y - 1, z - 1), u), v), w);
}

// --- scene profiles ----------------------------------------------------------

struct Scene {
    const char* name;
    Vec3 base_wind_ms;          // m/s
    double turbulence_strength;  // 0..1
    double reference_magnitude;  // m/s for PSNR ref normalisation
    uint64_t seed;
};

static const std::array<Scene, 5> kScenes = {{
    {"calm_clear",      {2.0, 0.5, 0.0}, 0.05, 2.5, 1},
    {"moderate_breeze", {6.0, 1.0, 0.2}, 0.20, 7.0, 7},
    {"storm_front",    {15.0, 4.0, 0.5}, 0.60, 18.0, 42},
    {"urban_canyon",    {3.0, 0.0, 0.0}, 0.40, 6.0, 1234},
    {"open_plains",     {8.0, 1.5, 0.0}, 0.45, 10.0, 31337},
}};

// --- ground truth (high-resolution reference) for PSNR ----------------------

void generate_reference(Grid3D& g, const Scene& s, double t) {
    auto p = PermTable(make_perm(s.seed));
    double cs = g.cell_size_m;
    double norm = 1.0 / s.reference_magnitude;
    for (int z = 0; z < g.nz; ++z) {
        for (int y = 0; y < g.ny; ++y) {
            for (int x = 0; x < g.nx; ++x) {
                double wx = static_cast<double>(x) * cs;
                double wy = static_cast<double>(y) * cs;
                double wz = static_cast<double>(z) * cs;
                double n = perlin3(wx * 0.02 + t * 0.05, wy * 0.02, wz * 0.02, p);
                double ny = perlin3(wx * 0.02 + 13.0, wy * 0.02 + t * 0.03, wz * 0.02, p);
                double nz = perlin3(wx * 0.02, wy * 0.02 + 17.0, wz * 0.02 + t * 0.04, p);
                Vec3 v;
                v.x = s.base_wind_ms.x + s.turbulence_strength * s.base_wind_ms.x * n;
                v.y = s.base_wind_ms.y + s.turbulence_strength * 4.0 * ny;
                v.z = s.base_wind_ms.z + s.turbulence_strength * 4.0 * nz;
                g.at(x, y, z) = v;
            }
        }
    }
    (void)norm;
}

// --- strategies --------------------------------------------------------------

// A_NoWind: zero wind (worst-case reference).
void strategy_a_nowind(Grid3D& g) {
    std::fill(g.vel.begin(), g.vel.end(), Vec3{});
}

// B_StaticWind: per-scene constant (current mainline approximation).
void strategy_b_static(Grid3D& g, const Scene& s) {
    for (auto& v : g.vel) v = s.base_wind_ms;
}

// D_PerlinWind3D: pure 3D Perlin field (no advection, divergence not zeroed).
void strategy_d_perlin(Grid3D& g, const Scene& s, double t) {
    auto p = PermTable(make_perm(s.seed));
    double cs = g.cell_size_m;
    for (int z = 0; z < g.nz; ++z) {
        for (int y = 0; y < g.ny; ++y) {
            for (int x = 0; x < g.nx; ++x) {
                double wx = static_cast<double>(x) * cs;
                double wy = static_cast<double>(y) * cs;
                double wz = static_cast<double>(z) * cs;
                double n = perlin3(wx * 0.02 + t * 0.05, wy * 0.02, wz * 0.02, p);
                double ny = perlin3(wx * 0.02 + 13.0, wy * 0.02 + t * 0.03, wz * 0.02, p);
                double nz = perlin3(wx * 0.02, wy * 0.02 + 17.0, wz * 0.02 + t * 0.04, p);
                Vec3 v;
                v.x = s.base_wind_ms.x + s.turbulence_strength * s.base_wind_ms.x * n;
                v.y = s.base_wind_ms.y + s.turbulence_strength * 4.0 * ny;
                v.z = s.base_wind_ms.z + s.turbulence_strength * 4.0 * nz;
                g.at(x, y, z) = v;
            }
        }
    }
}

// E_HybridCurlNoise: Bridson 2007 — divergence-free via curl of Perlin potential.
void strategy_e_curlnoise(Grid3D& g, const Scene& s, double t) {
    auto p = PermTable(make_perm(s.seed));
    double cs = g.cell_size_m;
    double eps = cs;
    for (int z = 0; z < g.nz; ++z) {
        for (int y = 0; y < g.ny; ++y) {
            for (int x = 0; x < g.nx; ++x) {
                double wx = static_cast<double>(x) * cs;
                double wy = static_cast<double>(y) * cs;
                double wz = static_cast<double>(z) * cs;
                double tx = t * 0.05, ty = t * 0.03, tz = t * 0.04;
                double psi_x_dy = perlin3(wx * 0.02 + tx, (wy + eps) * 0.02, wz * 0.02, p)
                                - perlin3(wx * 0.02 + tx, (wy - eps) * 0.02, wz * 0.02, p);
                double psi_x_dz = perlin3(wx * 0.02 + tx, wy * 0.02, (wz + eps) * 0.02, p)
                                - perlin3(wx * 0.02 + tx, wy * 0.02, (wz - eps) * 0.02, p);
                double psi_y_dx = perlin3(wx * 0.02 + 5.0, wy * 0.02 + ty, wz * 0.02, p)
                                - perlin3((wx - eps) * 0.02 + 5.0, wy * 0.02 + ty, wz * 0.02, p);
                double psi_y_dz = perlin3(wx * 0.02 + 5.0, wy * 0.02 + ty, (wz + eps) * 0.02, p)
                                - perlin3(wx * 0.02 + 5.0, wy * 0.02 + ty, (wz - eps) * 0.02, p);
                double psi_z_dx = perlin3(wx * 0.02 + 9.0, wy * 0.02 + 9.0 + tz, wz * 0.02, p)
                                - perlin3((wx - eps) * 0.02 + 9.0, wy * 0.02 + 9.0 + tz, wz * 0.02, p);
                double psi_z_dy = perlin3(wx * 0.02 + 9.0, (wy + eps) * 0.02 + 9.0 + tz, wz * 0.02, p)
                                - perlin3(wx * 0.02 + 9.0, (wy - eps) * 0.02 + 9.0 + tz, wz * 0.02, p);
                double inv2eps = 1.0 / (2.0 * eps);
                Vec3 v;
                v.x = s.base_wind_ms.x + s.turbulence_strength * 8.0 * (psi_z_dy - psi_y_dz) * inv2eps;
                v.y = s.base_wind_ms.y + s.turbulence_strength * 8.0 * (psi_x_dz - psi_z_dx) * inv2eps;
                v.z = s.base_wind_ms.z + s.turbulence_strength * 8.0 * (psi_y_dx - psi_x_dy) * inv2eps;
                g.at(x, y, z) = v;
            }
        }
    }
}

// C_StamStableFluid: 3D Stam advect+diffuse+project (2 Jacobi iters).
// Uses scratch buffer for previous velocity.
void strategy_c_stam(Grid3D& g, const Scene& s, double t, int jacobi_iters) {
    int N = g.nx;
    if (g.ny != N || g.nz != N) return;
    auto p_perm = PermTable(make_perm(s.seed));
    std::vector<Vec3> prev = g.vel;
    for (auto& v : g.vel) {
        double idx_n = static_cast<double>(&v - g.vel.data()) / static_cast<double>(g.vel.size());
        v = s.base_wind_ms * (1.0 + 0.3 * idx_n);
    }
    std::vector<Vec3> src = g.vel;
    for (int z = 1; z < g.nz - 1; ++z) {
        for (int y = 1; y < g.ny - 1; ++y) {
            for (int x = 1; x < g.nx - 1; ++x) {
                double u = src[g.idx(x, y, z)].x;
                double v = src[g.idx(x, y, z)].y;
                double w = src[g.idx(x, y, z)].z;
                double px = x - u * 0.1;
                double py = y - v * 0.1;
                double pz = z - w * 0.1;
                int xi = std::clamp(static_cast<int>(px), 0, N - 1);
                int yi = std::clamp(static_cast<int>(py), 0, N - 1);
                int zi = std::clamp(static_cast<int>(pz), 0, N - 1);
                g.vel[g.idx(x, y, z)] = src[g.idx(xi, yi, zi)];
            }
        }
    }
    g.div.assign(g.div.size(), 0.0);
    g.pressure.assign(g.pressure.size(), 0.0);
    for (int z = 1; z < g.nz - 1; ++z) {
        for (int y = 1; y < g.ny - 1; ++y) {
            for (int x = 1; x < g.nx - 1; ++x) {
                g.div[g.idx(x, y, z)] = -0.5 * (
                    g.vel[g.idx(x + 1, y, z)].x - g.vel[g.idx(x - 1, y, z)].x +
                    g.vel[g.idx(x, y + 1, z)].y - g.vel[g.idx(x, y - 1, z)].y +
                    g.vel[g.idx(x, y, z + 1)].z - g.vel[g.idx(x, y, z - 1)].z);
            }
        }
    }
    for (int it = 0; it < jacobi_iters; ++it) {
        std::vector<double> next(g.pressure.size(), 0.0);
        for (int z = 1; z < g.nz - 1; ++z) {
            for (int y = 1; y < g.ny - 1; ++y) {
                for (int x = 1; x < g.nx - 1; ++x) {
                    next[g.idx(x, y, z)] = (
                        g.pressure[g.idx(x + 1, y, z)] + g.pressure[g.idx(x - 1, y, z)] +
                        g.pressure[g.idx(x, y + 1, z)] + g.pressure[g.idx(x, y - 1, z)] +
                        g.pressure[g.idx(x, y, z + 1)] + g.pressure[g.idx(x, y, z - 1)] +
                        g.div[g.idx(x, y, z)]) / 6.0;
                }
            }
        }
        g.pressure = std::move(next);
    }
    for (int z = 1; z < g.nz - 1; ++z) {
        for (int y = 1; y < g.ny - 1; ++y) {
            for (int x = 1; x < g.nx - 1; ++x) {
                g.vel[g.idx(x, y, z)].x -= 0.5 * (g.pressure[g.idx(x + 1, y, z)] - g.pressure[g.idx(x - 1, y, z)]);
                g.vel[g.idx(x, y, z)].y -= 0.5 * (g.pressure[g.idx(x, y + 1, z)] - g.pressure[g.idx(x, y - 1, z)]);
                g.vel[g.idx(x, y, z)].z -= 0.5 * (g.pressure[g.idx(x, y, z + 1)] - g.pressure[g.idx(x, y, z - 1)]);
            }
        }
    }
    (void)t; (void)p_perm; (void)prev;
}

// --- PSNR (Akenine-Möller canonical formula) ---------------------------------

double compute_psnr(const Grid3D& a, const Grid3D& b, double max_val) {
    double mse = 0.0;
    size_t n = a.vel.size();
    for (size_t i = 0; i < n; ++i) {
        Vec3 d = a.vel[i] - b.vel[i];
        mse += d.x * d.x + d.y * d.y + d.z * d.z;
    }
    mse /= static_cast<double>(n);
    if (mse < 1e-12) return 99.0;
    return 10.0 * std::log10((max_val * max_val) / mse);
}

// --- ballistic correction cost (per-projectile, per-tick) --------------------

struct Projectile {
    Vec3 pos;
    Vec3 vel;
    double mass;
    double radius;
    double drag_cx;
};

double ballistic_correction_cost(const Grid3D& wind, const std::vector<Projectile>& projs, double dt_s) {
    double cs = wind.cell_size_m;
    int N = wind.nx;
    auto t0 = std::chrono::steady_clock::now();
    for (const auto& p : projs) {
        int x = std::clamp(static_cast<int>(p.pos.x / cs), 0, N - 1);
        int y = std::clamp(static_cast<int>(p.pos.y / cs), 0, N - 1);
        int z = std::clamp(static_cast<int>(p.pos.z / cs), 0, N - 1);
        Vec3 w = wind.at(x, y, z);
        Vec3 rel = p.vel - w;
        double speed = std::sqrt(rel.x * rel.x + rel.y * rel.y + rel.z * rel.z);
        if (speed > 1e-6) {
            double drag_acc = 0.5 * 1.225 * p.drag_cx * (speed * speed) * 3.14159 * p.radius * p.radius / p.mass;
            (void)drag_acc;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    (void)dt_s;
    return us;
}

// --- main benchmark harness --------------------------------------------------

struct Config {
    const char* strategy;
    int scene_idx;
    int seed_idx;
    int iter;
    int warmup;
    int grid_n;
    int jacobi_iters;
    int proj_count;
};

struct Result {
    std::string strategy;
    std::string scene;
    int seed;
    int grid_n;
    int proj_count;
    int jacobi_iters;
    Stats tick_stats;
    Stats bal_corr_stats;
    double psnr_db;
    double mem_bandwidth_mb_s;
};

int main(int argc, char** argv) {
    int iter = 1000;
    int warmup = 10;
    if (argc > 1) iter = std::atoi(argv[1]);
    if (argc > 2) warmup = std::atoi(argv[2]);

    const std::array<int, 2> grid_sizes = {32, 64};
    const std::array<int, 2> proj_counts = {100, 500};
    const std::array<int, 2> jacobi_options = {2, 4};
    const std::array<const char*, 5> strat_names = {
        "A_NoWind", "B_StaticWind", "C_StamStableFluid", "D_PerlinWind3D", "E_HybridCurlNoise"};
    const std::array<int, 3> seeds = {1, 42, 31337};

    std::printf("Running benchmark: %d scenes x %d seeds x %d grids x %d strats x %d iter + %d warmup\n",
        5, static_cast<int>(seeds.size()), static_cast<int>(grid_sizes.size()),
        5, iter, warmup);

    std::vector<Result> results;
    for (int si = 0; si < 5; ++si) {
        const Scene& s = kScenes[si];
        for (int sd = 0; sd < static_cast<int>(seeds.size()); ++sd) {
            int seed = seeds[sd];
            for (int gn_idx = 0; gn_idx < static_cast<int>(grid_sizes.size()); ++gn_idx) {
                int gn = grid_sizes[gn_idx];
                int proj_n = proj_counts[gn_idx];

                Grid3D g_ref;
                g_ref.nx = g_ref.ny = g_ref.nz = gn;
                g_ref.cell_size_m = 8.0;
                g_ref.resize();
                generate_reference(g_ref, s, 0.0);

                std::mt19937_64 rng(static_cast<uint64_t>(seed) + 1000);
                std::uniform_real_distribution<double> ud(0.0, 256.0);
                std::vector<Projectile> projs(proj_n);
                for (auto& p : projs) {
                    p.pos = Vec3(ud(rng), ud(rng), ud(rng) * 0.5);
                    p.vel = Vec3(100.0 + ud(rng) * 50.0, ud(rng) * 10.0, ud(rng) * 5.0);
                    p.mass = 5.0 + ud(rng) * 10.0;
                    p.radius = 0.05;
                    p.drag_cx = 0.3;
                }

                for (int sti = 0; sti < 5; ++sti) {
                    const char* st = strat_names[sti];
                    int jacobi_n = (sti == 2) ? jacobi_options[gn_idx >= 2 ? 1 : 0] : 2;

                    std::vector<double> tick_us;
                    std::vector<double> bal_us;
                    tick_us.reserve(static_cast<size_t>(iter));
                    bal_us.reserve(static_cast<size_t>(iter));

                    for (int w = 0; w < warmup; ++w) {
                        Grid3D g;
                        g.nx = g.ny = g.nz = gn;
                        g.cell_size_m = 8.0;
                        g.resize();
                        switch (sti) {
                            case 0: strategy_a_nowind(g); break;
                            case 1: strategy_b_static(g, s); break;
                            case 2: strategy_c_stam(g, s, 0.05 * w, jacobi_n); break;
                            case 3: strategy_d_perlin(g, s, 0.05 * w); break;
                            case 4: strategy_e_curlnoise(g, s, 0.05 * w); break;
                        }
                        (void)ballistic_correction_cost(g, projs, 0.016);
                    }

                    for (int it = 0; it < iter; ++it) {
                        Grid3D g;
                        g.nx = g.ny = g.nz = gn;
                        g.cell_size_m = 8.0;
                        g.resize();
                        auto t0 = std::chrono::steady_clock::now();
                        switch (sti) {
                            case 0: strategy_a_nowind(g); break;
                            case 1: strategy_b_static(g, s); break;
                            case 2: strategy_c_stam(g, s, 0.05 * it, jacobi_n); break;
                            case 3: strategy_d_perlin(g, s, 0.05 * it); break;
                            case 4: strategy_e_curlnoise(g, s, 0.05 * it); break;
                        }
                        auto t1 = std::chrono::steady_clock::now();
                        double bu = ballistic_correction_cost(g, projs, 0.016);
                        auto t2 = std::chrono::steady_clock::now();
                        (void)t2;
                        tick_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
                        bal_us.push_back(bu);
                    }

                    Stats ts = compute_stats(tick_us);
                    Stats bs = compute_stats(bal_us);
                    double psnr = 0.0;
                    if (sti != 0) {
                        Grid3D g;
                        g.nx = g.ny = g.nz = gn;
                        g.cell_size_m = 8.0;
                        g.resize();
                        switch (sti) {
                            case 1: strategy_b_static(g, s); break;
                            case 2: strategy_c_stam(g, s, 0.0, jacobi_n); break;
                            case 3: strategy_d_perlin(g, s, 0.0); break;
                            case 4: strategy_e_curlnoise(g, s, 0.0); break;
                        }
                        psnr = compute_psnr(g, g_ref, s.reference_magnitude);
                    }
                    double mem = (static_cast<double>(gn) * gn * gn * 3 * sizeof(double)) / 1e6;
                    Result r;
                    r.strategy = st;
                    r.scene = s.name;
                    r.seed = seed;
                    r.grid_n = gn;
                    r.proj_count = proj_n;
                    r.jacobi_iters = jacobi_n;
                    r.tick_stats = ts;
                    r.bal_corr_stats = bs;
                    r.psnr_db = psnr;
                    r.mem_bandwidth_mb_s = mem;
                    results.push_back(r);
                }
            }
        }
    }

    std::ofstream csv("results.csv");
    csv << "strategy,scene,seed,grid_n,proj_count,jacobi_iters,tick_mean_us,tick_median_us,tick_p95_us,tick_p99_us,tick_std_us,tick_min_us,tick_max_us,bal_mean_us,bal_median_us,bal_p95_us,bal_p99_us,bal_std_us,psnr_db,mem_mb\n";
    for (const auto& r : results) {
        csv << r.strategy << "," << r.scene << "," << r.seed << "," << r.grid_n << ","
            << r.proj_count << "," << r.jacobi_iters << ","
            << r.tick_stats.mean << "," << r.tick_stats.median << ","
            << r.tick_stats.p95 << "," << r.tick_stats.p99 << "," << r.tick_stats.stddev << ","
            << r.tick_stats.min << "," << r.tick_stats.max << ","
            << r.bal_corr_stats.mean << "," << r.bal_corr_stats.median << ","
            << r.bal_corr_stats.p95 << "," << r.bal_corr_stats.p99 << "," << r.bal_corr_stats.stddev << ","
            << r.psnr_db << "," << r.mem_bandwidth_mb_s << "\n";
    }
    csv.close();

    std::printf("Wrote %zu rows to results.csv\n", results.size());
    return 0;
}

}  // namespace pv

int main(int argc, char** argv) {
    return pv::main(argc, argv);
}
