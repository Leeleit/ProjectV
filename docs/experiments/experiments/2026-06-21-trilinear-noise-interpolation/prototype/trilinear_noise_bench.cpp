#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <ranges>
#include <string>
#include <vector>
#include <chrono>

//======================================================================
// Noise — hash-based gradient noise (Perlin-like), 3D
//======================================================================

static constexpr int   PERM_SIZE   = 256;
static constexpr int   PERM_MASK   = PERM_SIZE - 1;

static int perm[PERM_SIZE * 2];

[[gnu::constructor]] static void init_perm() {
    for (int i = 0; i < PERM_SIZE; ++i) perm[i] = i;
    std::mt19937 rng(42);
    for (int i = PERM_SIZE - 1; i > 0; --i)
        std::swap(perm[i], perm[rng() % (i + 1)]);
    for (int i = 0; i < PERM_SIZE; ++i)
        perm[PERM_SIZE + i] = perm[i];
}

static double grad(int hash, double x, double y, double z) {
    int h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

static double fade(double t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }

static double lerp(double a, double b, double t) { return a + t * (b - a); }

static double noise3d(double x, double y, double z) {
    int X = (int)std::floor(x) & PERM_MASK;
    int Y = (int)std::floor(y) & PERM_MASK;
    int Z = (int)std::floor(z) & PERM_MASK;
    double dx = x - std::floor(x);
    double dy = y - std::floor(y);
    double dz = z - std::floor(z);
    double u = fade(dx), v = fade(dy), w = fade(dz);
    int a  = perm[X] + Y;
    int aa = perm[a] + Z;
    int ab = perm[a + 1] + Z;
    int b  = perm[X + 1] + Y;
    int ba = perm[b] + Z;
    int bb = perm[b + 1] + Z;

    return lerp(
        lerp(lerp(grad(perm[aa], dx, dy, dz), grad(perm[ba], dx - 1, dy, dz), u),
             lerp(grad(perm[ab], dx, dy - 1, dz), grad(perm[bb], dx - 1, dy - 1, dz), u), v),
        lerp(lerp(grad(perm[aa + 1], dx, dy, dz - 1), grad(perm[ba + 1], dx - 1, dy, dz - 1), u),
             lerp(grad(perm[ab + 1], dx, dy - 1, dz - 1), grad(perm[bb + 1], dx - 1, dy - 1, dz - 1), u), v),
        w);
}

static double fbm(double x, double y, double z, int octaves = 4) {
    double val = 0.0, amp = 1.0, freq = 1.0, max_val = 0.0;
    for (int i = 0; i < octaves; ++i) {
        val += amp * noise3d(x * freq, y * freq, z * freq);
        max_val += amp;
        amp *= 0.5;
        freq *= 2.0;
    }
    return val / max_val;
}

//======================================================================
// Scene definitions — 5 terrain profiles
//======================================================================

struct Scene {
    std::string name;
    double      scale;       // noise frequency scaling
    double      height_amp;  // vertical amplitude multiplier
    double      bias;        // density bias (shift)
    double      detail;      // high-frequency detail mix
};

static constexpr int CHUNK_SIZE = 8;
static constexpr int VOXEL_COUNT = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

static const Scene scenes[] = {
    { "flat_plains",   0.020, 0.3,  0.0, 0.1 },
    { "rolling_hills", 0.025, 1.0,  0.0, 0.2 },
    { "mountains",     0.030, 2.5, -0.2, 0.3 },
    { "cave_system",   0.050, 0.8,  0.3, 0.5 },
    { "island",        0.020, 1.2, -0.1, 0.2 },
};

// static int scene_voxel_count(const Scene&) { return VOXEL_COUNT; }

//======================================================================
// Density function for a scene
//======================================================================

static double eval_density(const Scene& scene, int x, int y, int z, int chunk_seed = 0) {
    double wx = (double)(x + chunk_seed * 137) * scene.scale;
    double wy = (double)y * scene.scale * 0.8;
    double wz = (double)(z + chunk_seed * 281) * scene.scale;

    double base = fbm(wx, wy, wz, 4);
    double h = (double)y / (double)CHUNK_SIZE - 0.5;
    double terrain = scene.height_amp * (1.0 - 2.0 * std::abs(h));

    if (scene.detail > 0.01) {
        double fine = fbm(wx * 4.0, wy * 4.0, wz * 4.0, 2);
        return base * (1.0 - scene.detail) + fine * scene.detail + terrain + scene.bias;
    }
    return base + terrain + scene.bias;
}

//======================================================================
// Interpolation helpers
//======================================================================

static double trilerp(const double c[8], double u, double v, double w) {
    double accum = 0.0;
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            for (int k = 0; k < 2; ++k) {
                double wi = i ? u : (1.0 - u);
                double wj = j ? v : (1.0 - v);
                double wk = k ? w : (1.0 - w);
                accum += c[i * 4 + j * 2 + k] * wi * wj * wk;
            }
    return accum;
}

// Catmull-Rom cubic interpolation in 1D
static double cr_spline(double p[4], double t) {
    return 0.5 * ((2.0 * p[1]) +
        (-p[0] + p[2]) * t +
        (2.0 * p[0] - 5.0 * p[1] + 4.0 * p[2] - p[3]) * t * t +
        (-p[0] + 3.0 * p[1] - 3.0 * p[2] + p[3]) * t * t * t);
}

static double tricubic_spline(const double c[64], double u, double v, double w) {
    double u_slice[4];
    for (int i = 0; i < 4; ++i) {
        double v_slice[4];
        for (int j = 0; j < 4; ++j) {
            double w_vals[4];
            for (int k = 0; k < 4; ++k)
                w_vals[k] = c[i * 16 + j * 4 + k];
            v_slice[j] = cr_spline(w_vals, w);
        }
        u_slice[i] = cr_spline(v_slice, v);
    }
    return cr_spline(u_slice, u);
}

//======================================================================
// Strategy implementations
//======================================================================

// A_PerVoxel: evaluate noise at every voxel (baseline)
static void eval_A_per_voxel(const Scene& scene, int seed, double* out) {
    for (int z = 0; z < CHUNK_SIZE; ++z)
        for (int y = 0; y < CHUNK_SIZE; ++y)
            for (int x = 0; x < CHUNK_SIZE; ++x)
                out[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] =
                    eval_density(scene, x, y, z, seed);
}

// B_CoarseNxNxN: evaluate noise at coarse grid + trilinear interpolation
// stride = CHUNK_SIZE / (N-1)
static void eval_coarse_trilerp(const Scene& scene, int seed, int N, double* out) {
    int stride = (CHUNK_SIZE) / (N - 1);
    std::vector<double> coarse_grid;
    coarse_grid.reserve(N * N * N);
    for (int cz = 0; cz < N; ++cz)
        for (int cy = 0; cy < N; ++cy)
            for (int cx = 0; cx < N; ++cx) {
                int gx = std::min(cx * stride, CHUNK_SIZE);
                int gy = std::min(cy * stride, CHUNK_SIZE);
                int gz = std::min(cz * stride, CHUNK_SIZE);
                coarse_grid.push_back(eval_density(scene, gx, gy, gz, seed));
            }

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        double tz = (double)z / (double)stride;
        int cz0 = (int)tz; if (cz0 >= N - 1) cz0 = N - 2;
        int cz1 = cz0 + 1;
        double wz = tz - (double)cz0;

        for (int y = 0; y < CHUNK_SIZE; ++y) {
            double ty = (double)y / (double)stride;
            int cy0 = (int)ty; if (cy0 >= N - 1) cy0 = N - 2;
            int cy1 = cy0 + 1;
            double wy = ty - (double)cy0;

            for (int x = 0; x < CHUNK_SIZE; ++x) {
                double tx = (double)x / (double)stride;
                int cx0 = (int)tx; if (cx0 >= N - 1) cx0 = N - 2;
                int cx1 = cx0 + 1;
                double wx = tx - (double)cx0;

                double c[8];
                int idx = 0;
                for (int di = 0; di < 2; ++di)
                    for (int dj = 0; dj < 2; ++dj)
                        for (int dk = 0; dk < 2; ++dk) {
                            int gi = (di ? cx1 : cx0);
                            int gj = (dj ? cy1 : cy0);
                            int gk = (dk ? cz1 : cz0);
                            c[idx++] = coarse_grid[gk * N * N + gj * N + gi];
                        }
                out[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] = trilerp(c, wx, wy, wz);
            }
        }
    }
}

// B2: Coarse 2x2x2 (stride = 8)
static void eval_B_2x2x2(const Scene& scene, int seed, double* out) {
    eval_coarse_trilerp(scene, seed, 2, out);
}

// C: Coarse 3x3x3 (stride = 4)
static void eval_C_3x3x3(const Scene& scene, int seed, double* out) {
    eval_coarse_trilerp(scene, seed, 3, out);
}

// D: Coarse 4x4x4 (stride = 2)
static void eval_D_4x4x4(const Scene& scene, int seed, double* out) {
    eval_coarse_trilerp(scene, seed, 4, out);
}

// E: Coarse 2x2x2 + Catmull-Rom cubic interpolation
static void eval_E_spline_2x2x2(const Scene& scene, int seed, double* out) {
    // For Catmull-Rom we need 4x4x4 neighborhood: sample at stride 4 to get
    // positions -4, 0, 4, 8 for proper cubic interpolation
    static constexpr int N = 4;
    static constexpr int STRIDE = 4;
    std::array<double, N * N * N> coarse{};

    for (int cz = 0; cz < N; ++cz)
        for (int cy = 0; cy < N; ++cy)
            for (int cx = 0; cx < N; ++cx) {
                int gx = (cx * STRIDE);
                int gy = (cy * STRIDE);
                int gz = (cz * STRIDE);
                coarse[cz * N * N + cy * N + cx] = eval_density(scene, gx, gy, gz, seed);
            }

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        double tz = (double)z / (double)STRIDE + 1.0;
        int cz0 = (int)tz; if (cz0 > N - 4) cz0 = N - 4;
        double wz = tz - (double)cz0;

        for (int y = 0; y < CHUNK_SIZE; ++y) {
            double ty = (double)y / (double)STRIDE + 1.0;
            int cy0 = (int)ty; if (cy0 > N - 4) cy0 = N - 4;
            double wy = ty - (double)cy0;

            for (int x = 0; x < CHUNK_SIZE; ++x) {
                double tx = (double)x / (double)STRIDE + 1.0;
                int cx0 = (int)tx; if (cx0 > N - 4) cx0 = N - 4;
                double wx = tx - (double)cx0;

                double c[64];
                int idx = 0;
                for (int di = 0; di < 4; ++di)
                    for (int dj = 0; dj < 4; ++dj)
                        for (int dk = 0; dk < 4; ++dk) {
                            int gi = cx0 + di;
                            int gj = cy0 + dj;
                            int gk = cz0 + dk;
                            c[idx++] = coarse[gk * N * N + gj * N + gi];
                        }
                out[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] = tricubic_spline(c, wx, wy, wz);
            }
        }
    }
}

//======================================================================
// Metrics
//======================================================================

struct Metrics {
    double psnr_density_db; // PSNR of density field
    double match_rate;       // binary voxel match rate (solid/air)
    double time_us;          // wall time in microseconds
};

static double compute_psnr(const double* ref, const double* test, int n) {
    double mse = 0.0;
    for (int i = 0; i < n; ++i) {
        double diff = ref[i] - test[i];
        mse += diff * diff;
    }
    mse /= (double)n;
    if (mse < 1e-15) return 100.0;
    double max_val = 1.0;
    return 10.0 * std::log10((max_val * max_val) / mse);
}

static double compute_match_rate(const double* ref, const double* test, int n) {
    int matches = 0;
    for (int i = 0; i < n; ++i) {
        bool ref_solid = ref[i] > 0.0;
        bool test_solid = test[i] > 0.0;
        if (ref_solid == test_solid) ++matches;
    }
    return (double)matches / (double)n;
}

template<typename Fn>
static Metrics measure(const Scene& scene, int seed, Fn&& eval_fn) {
    std::vector<double> ref(VOXEL_COUNT);
    std::vector<double> test(VOXEL_COUNT);

    // Generate reference (A_PerVoxel)
    eval_A_per_voxel(scene, seed, ref.data());

    // Time the strategy
    auto t0 = std::chrono::high_resolution_clock::now();
    constexpr int REPEAT = 100;
    for (int r = 0; r < REPEAT; ++r) {
        eval_fn(test.data());
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double time_us = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0 / (double)REPEAT;

    double psnr = compute_psnr(ref.data(), test.data(), VOXEL_COUNT);
    double match = compute_match_rate(ref.data(), test.data(), VOXEL_COUNT);

    return { psnr, match, time_us };
}

//======================================================================
// Main
//======================================================================

int main() {
    std::cout << "=== Trilinear Noise Interpolation Benchmark ===\n";
    std::cout << "Chunk size: " << CHUNK_SIZE << "^3 = " << VOXEL_COUNT << " voxels\n\n";

    std::ofstream csv("results.csv");
    csv << "strategy,scene,seed,psnr_density_db,match_rate,time_us\n";

    printf("%-14s %-16s %4s %12s %12s %10s\n",
           "STRATEGY", "SCENE", "SEED", "PSNR(dB)", "MATCH_RATE", "TIME(us)");

    struct Strategy {
        const char* name;
        void (*eval)(const Scene&, int, double*);
    };

    Strategy strategies[] = {
        { "A_PerVoxel",  [](const Scene& s, int seed, double* out) {
            eval_A_per_voxel(s, seed, out); } },
        { "B_Trilerp_2", eval_B_2x2x2 },
        { "C_Trilerp_3", eval_C_3x3x3 },
        { "D_Trilerp_4", eval_D_4x4x4 },
        { "E_Spline_2",  eval_E_spline_2x2x2 },
    };

    int seeds[] = { 1, 7, 42, 1234, 31337 };

    for (auto& strat : strategies) {
        for (auto& scene : scenes) {
            for (int seed : seeds) {
                Metrics m;
                if (strat.name[0] == 'A') {
                    // A_PerVoxel: measure noise eval time, psnr = 100
                    auto t0 = std::chrono::high_resolution_clock::now();
                    constexpr int R = 100;
                    std::vector<double> tmp(VOXEL_COUNT);
                    for (int r = 0; r < R; ++r)
                        strat.eval(scene, seed, tmp.data());
                    auto t1 = std::chrono::high_resolution_clock::now();
                    double time_us = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0 / (double)R;

                    std::vector<double> ref(VOXEL_COUNT);
                    eval_A_per_voxel(scene, seed, ref.data());
                    m = { 100.0, 1.0, time_us };
                } else {
                    m = measure(scene, seed, [&](double* out) { strat.eval(scene, seed, out); });
                }

                csv << std::format("{},{},{},{:.6f},{:.6f},{:.6f}\n",
                    strat.name, scene.name, seed, m.psnr_density_db, m.match_rate, m.time_us);

                printf("%-14s %-16s %4d %11.2f %11.6f %9.3f\n",
                       strat.name, scene.name.c_str(), seed,
                       m.psnr_density_db, m.match_rate, m.time_us);
            }
        }
    }

    csv.close();
    std::cout << "\nResults written to results.csv\n";
    return 0;
}
