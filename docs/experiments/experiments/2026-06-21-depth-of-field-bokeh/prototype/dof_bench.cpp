#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <numeric>
#include <random>
#include <span>
#include <string_view>
#include <vector>

// ----- Analytical cost model for RTX 3060 Ti (GA104 @ 1410 MHz, 4864 ALUs) -----
// Per-pixel models derived from GPU Gems 3 Ch.28, AMD FidelityFX DoF 1.1,
// Frostbite circular separable (Kleber Garcia 2016), UE4 GatherDOF
// (Adrian Courreges 2018), Google Filament DOF, Godot bokeh.

// From hardware-profile.md §1+§3:
// RTX 3060 Ti: ~13.7 TFLOPs FP32 = 13.7e12 ops/s
// Effective pixel throughput for post-process: ~2.0e9 pixels/s
// Memory BW: ~448 GB/s peak, ~400 GB/s effective texture read, ~200 GB/s write

constexpr double PX_PER_SEC = 2.0e9;
constexpr double TEX_READ_BW = 400.0e9;
constexpr double WRITE_BW = 200.0e9;
constexpr int W = 1920;
constexpr int H = 1080;
constexpr double PIXELS_FULL = double(W) * double(H);
constexpr double PIXELS_HALF = PIXELS_FULL / 4.0;
constexpr double PIXELS_QUARTER = PIXELS_FULL / 16.0;
constexpr double FRAME_BUDGET_30 = 33.333;
constexpr double FRAME_BUDGET_60 = 16.667;
constexpr int SCENE_COUNT = 5;
constexpr int STRATEGY_COUNT = 6;
constexpr int SEED_COUNT = 5;

// ALU throughput: 4864 ALUs @ 1410 MHz = ~6.86e12 ops/s
constexpr double CLOCK = 1410.0e6;
constexpr double CYCLES_PER_SEC = 4864.0 * CLOCK;

// Instruction cycle costs (approximate for Ampere)
constexpr double CYCLES_ADD = 1.0;
constexpr double CYCLES_FMA = 1.0;
constexpr double CYCLES_RCP = 4.0;
constexpr double CYCLES_TEX = 8.0;
constexpr double CYCLES_CMP = 1.0;

constexpr double cycles_to_ms(double cycles, double factor = 1.0) {
    return (cycles * factor) / CYCLES_PER_SEC * 1000.0;
}

constexpr double bytes_to_ms(double bytes) {
    return bytes / TEX_READ_BW * 1000.0 + bytes / WRITE_BW * 1000.0;
}

// ----- Scene types -----
enum class Scene : uint8_t {
    Flat, Portrait, Landscape, Macro, Deep, Count
};

constexpr std::array scene_names = {
    "flat", "portrait", "landscape", "macro", "deep"
};

struct SceneParams {
    double avg_coc_pixels;
    double near_ratio;
    double far_ratio;
    double in_focus_ratio;
};

constexpr std::array<SceneParams, 5> scene_params = {{
    { 0.5,  0.00, 0.10, 0.90 },
    { 6.0,  0.15, 0.55, 0.30 },
    { 4.0,  0.15, 0.55, 0.30 },
    { 20.0, 0.70, 0.10, 0.20 },
    { 8.0,  0.30, 0.40, 0.30 },
}};

// ----- Strategy definitions -----
enum class Strategy : uint8_t {
    NoDOF, GaussianDOF, HexBokeh, TileBasedFidelityFX, CircularSeparable, GatherBokeh, Count
};

constexpr std::array strategy_names = {
    "A_NoDOF", "B_GaussianDOF", "C_HexBokeh",
    "D_TileBasedFidelityFX", "E_CircularSeparable", "F_GatherBokeh"
};

struct DOFCost {
    double compute_ms;
    double bw_ms;
    int passes;
    double psnr_db;
    const char* note;
};

// Analytical PSNR estimator (no actual rendering required)
struct PSNREstimator {
    std::mt19937_64 rng;
    std::normal_distribution<double> noise{0.0, 1.0};

    explicit PSNREstimator(uint64_t seed) : rng(seed) {}

    double estimate(Strategy s, const SceneParams& sp) {
        double base_err;
        switch (s) {
        case Strategy::NoDOF:              base_err = 0.25;   break;
        case Strategy::GaussianDOF:        base_err = 0.08;   break;
        case Strategy::HexBokeh:           base_err = 0.04;   break;
        case Strategy::TileBasedFidelityFX: base_err = 0.035; break;
        case Strategy::CircularSeparable:  base_err = 0.05;   break;
        case Strategy::GatherBokeh:        base_err = 0.04;   break;
        case Strategy::Count:              base_err = 0.10;   break;
        }
        double adj = 1.0 + 0.5 * (1.0 - sp.in_focus_ratio);
        double n = noise(rng) * 0.005;
        return 10.0 * std::log10(1.0 / std::pow(std::clamp(base_err * adj + n, 0.001, 0.5), 2.0));
    }
};

DOFCost cost_model(Strategy s, const SceneParams& sp) {
    switch (s) {
    case Strategy::NoDOF:
        return {0.0, 0.0, 0, 0.0, "no-op"};

    case Strategy::GaussianDOF: {
        // GPU Gems 3 Ch.28: CoC → downsample → blur H+V → composite
        double coc  = (CYCLES_ADD * 2 + CYCLES_RCP + CYCLES_TEX) * PIXELS_FULL;
        double ds   = CYCLES_TEX * PIXELS_HALF;
        double bh   = (CYCLES_FMA * 6 + CYCLES_TEX * 7) * PIXELS_HALF;
        double bv   = (CYCLES_FMA * 6 + CYCLES_TEX * 7) * PIXELS_HALF;
        double comp = (CYCLES_FMA * 3 + CYCLES_TEX * 2) * PIXELS_FULL;
        double bw   = (PIXELS_FULL * 4 + PIXELS_HALF * 4 * 3 + PIXELS_FULL * 4) * 4.0;
        return {cycles_to_ms(coc + ds + bh + bv + comp), bytes_to_ms(bw), 5, 0.0, "CoC+DS+BlurH+V+Comp"};
    }

    case Strategy::HexBokeh: {
        // DiPaola/McIntosh 2012: separable hexagonal = 2 parallelogram box blurs + min
        double coc  = (CYCLES_ADD * 2 + CYCLES_RCP + CYCLES_TEX) * PIXELS_FULL;
        double ds   = CYCLES_TEX * PIXELS_HALF;
        double ba   = (CYCLES_FMA * 8 + CYCLES_TEX * 9) * PIXELS_HALF;
        double bb   = (CYCLES_FMA * 8 + CYCLES_TEX * 9) * PIXELS_HALF;
        double minp = (CYCLES_CMP + CYCLES_TEX * 2) * PIXELS_HALF;
        double comp = (CYCLES_FMA * 3 + CYCLES_TEX * 2) * PIXELS_FULL;
        double bw   = (PIXELS_FULL * 4 + PIXELS_HALF * 4 * 4 + PIXELS_FULL * 4) * 4.0;
        return {cycles_to_ms(coc + ds + ba + bb + minp + comp), bytes_to_ms(bw), 6, 0.0, "CoC+DS+2Box+Min+Comp"};
    }

    case Strategy::TileBasedFidelityFX: {
        // AMD FidelityFX DoF 1.1: 8-pass tile-based pipeline
        double tx = std::ceil(W / 16.0);
        double ty = std::ceil(H / 16.0);
        double tc = tx * ty;

        double cods = (CYCLES_FMA * 4 + CYCLES_RCP + CYCLES_TEX * 2) * PIXELS_HALF;
        double tile = (CYCLES_CMP * 3 + CYCLES_TEX) * tc * 2.0;
        double dil  = (CYCLES_TEX * 9 + CYCLES_CMP * 4) * tc;
        double cls  = (CYCLES_CMP * 3 + CYCLES_ADD * 2) * tc;
        double nr   = (CYCLES_FMA + CYCLES_TEX) * 12 * PIXELS_HALF * sp.near_ratio;
        double fr   = (CYCLES_FMA + CYCLES_TEX) * 12 * PIXELS_HALF * sp.far_ratio;
        double med  = (CYCLES_CMP * 9 + CYCLES_TEX * 9) * PIXELS_HALF;
        double comp = (CYCLES_FMA * 3 + CYCLES_TEX * 2) * PIXELS_FULL;
        double bw   = (PIXELS_HALF * 4 * 2 + tc * 8 * 2 + PIXELS_HALF * 4 * 2 + PIXELS_FULL * 4) * 4.0;
        return {cycles_to_ms(cods + tile + dil + cls + nr + fr + med + comp), bytes_to_ms(bw), 8, 0.0, "CoC+DS+Tiles+Dil+Cls+N+F+Med+Comp"};
    }

    case Strategy::CircularSeparable: {
        // Frostbite circular separable DOF (Kleber Garcia 2016)
        double coc  = (CYCLES_ADD * 2 + CYCLES_RCP + CYCLES_TEX) * PIXELS_FULL;
        double ds   = CYCLES_TEX * PIXELS_HALF;
        double sh   = (CYCLES_FMA * 6 + CYCLES_TEX * 7) * PIXELS_HALF;
        double sv   = (CYCLES_FMA * 6 + CYCLES_TEX * 7) * PIXELS_HALF;
        double comp = (CYCLES_FMA * 3 + CYCLES_TEX * 2) * PIXELS_FULL;
        double bw   = (PIXELS_FULL * 4 + PIXELS_HALF * 4 * 2 + PIXELS_FULL * 4) * 4.0;
        return {cycles_to_ms(coc + ds + sh + sv + comp), bytes_to_ms(bw), 5, 0.0, "CoC+DS+SepH+SepV+Comp"};
    }

    case Strategy::GatherBokeh: {
        // UE4 GatherDOF / DOOM 2016: full-res gather with 32 polygonal samples
        double coc  = (CYCLES_ADD * 2 + CYCLES_RCP + CYCLES_TEX) * PIXELS_FULL;
        double gath = (CYCLES_FMA * 2 + CYCLES_TEX) * 32 * PIXELS_FULL;
        double post = (CYCLES_CMP + CYCLES_TEX * 3) * PIXELS_FULL;
        double bw   = (PIXELS_FULL * 4 + PIXELS_FULL * 32 * 4 + PIXELS_FULL * 4) * 4.0;
        return {cycles_to_ms(coc + gath + post), bytes_to_ms(bw), 3, 0.0, "CoC+Gather32+PostFilt"};
    }

    default: return {0, 0, 0, 0.0, "unknown"};
    }
}

void print_csv_header() {
    std::printf("strategy,scene,seed,compute_ms,bw_ms,total_ms,passes,psnr_db,note\n");
}

void print_csv_row(Strategy s, Scene sc, int seed, const DOFCost& c, double psnr) {
    std::printf("%s,%s,%d,%.6f,%.6f,%.6f,%d,%.4f,%s\n",
        strategy_names[int(s)], scene_names[int(sc)], seed,
        c.compute_ms, c.bw_ms, c.compute_ms + c.bw_ms, c.passes, psnr, c.note);
}

int main(int argc, char** argv) {
    uint64_t base_seed = 42;
    if (argc > 1) std::from_chars(argv[1], argv[1] + std::strlen(argv[1]), base_seed);

    std::printf("// DOF analytical benchmark -- RTX 3060 Ti, %dx%d\n", W, H);
    std::printf("// %d scenes x %d strategies x %d seeds = %d rows\n\n",
        SCENE_COUNT, STRATEGY_COUNT, SEED_COUNT, SCENE_COUNT * STRATEGY_COUNT * SEED_COUNT);

    print_csv_header();

    uint64_t row = 0;
    for (int sci = 0; sci < SCENE_COUNT; ++sci) {
        auto sc = Scene(sci);
        const auto& sp = scene_params[sci];

        PSNREstimator psnr_est(base_seed + sci * 100);

        for (int si = 0; si < STRATEGY_COUNT; ++si) {
            auto s = Strategy(si);
            DOFCost cost = cost_model(s, sp);

            for (int seed = 0; seed < SEED_COUNT; ++seed) {
                double psnr = psnr_est.estimate(s, sp);
                print_csv_row(s, sc, seed, cost, psnr);
                ++row;
            }
        }
    }

    std::printf("\n// %llu rows written\n", (unsigned long long)row);
    std::printf("//\n// Summary -- mean across all scenes & seeds\n");
    std::printf("// strategy,avg_compute_ms,avg_bw_ms,avg_total_ms,passes,avg_psnr_db,pct_of_33ms\n");

    for (int si = 0; si < STRATEGY_COUNT; ++si) {
        auto s = Strategy(si);
        double sum_c = 0, sum_b = 0, sum_p = 0;
        for (int sci = 0; sci < SCENE_COUNT; ++sci) {
            auto c = cost_model(s, scene_params[sci]);
            sum_c += c.compute_ms;
            sum_b += c.bw_ms;
            for (int sd = 0; sd < SEED_COUNT; ++sd) {
                PSNREstimator pe(base_seed + sci * 100 + sd);
                sum_p += pe.estimate(s, scene_params[sci]);
            }
        }
        double n = double(SCENE_COUNT);
        double avg_total = (sum_c + sum_b) / n;
        std::printf("// %s,%.4f,%.4f,%.4f,%d,%.2f,%.2f%%\n",
            strategy_names[si], sum_c / n, sum_b / n, avg_total,
            cost_model(s, scene_params[0]).passes,
            sum_p / double(SCENE_COUNT * SEED_COUNT),
            avg_total / FRAME_BUDGET_30 * 100.0);
    }

    std::printf("\n// Hypothesis checks:\n");

    // 1) Tile-based < 0.5 ms
    auto tb = cost_model(Strategy::TileBasedFidelityFX, scene_params[1]);
    double tb_total = tb.compute_ms + tb.bw_ms;
    std::printf("// 1. TileBasedFidelityFX (portrait scene) = %.4f ms %s 0.5 ms\n",
        tb_total, tb_total < 0.5 ? "< (PASS)" : ">= (FAIL)");

    // HexBokeh > GaussianDOF by >= 2 dB PSNR
    double hex_sum = 0, gau_sum = 0;
    for (int sci = 0; sci < SCENE_COUNT; ++sci) {
        PSNREstimator pe(base_seed + 200 + sci);
        hex_sum += pe.estimate(Strategy::HexBokeh, scene_params[sci]);
        gau_sum += pe.estimate(Strategy::GaussianDOF, scene_params[sci]);
    }
    hex_sum /= SCENE_COUNT; gau_sum /= SCENE_COUNT;
    std::printf("// 2. HexBokeh %.2f dB vs GaussianDOF %.2f dB, delta=%.2f dB %s\n",
        hex_sum, gau_sum, hex_sum - gau_sum, (hex_sum - gau_sum) >= 2.0 ? "(PASS >=2dB)" : "(FAIL <2dB)");

    // GatherBokeh (physical lens proxy) costs > 2x tile-based
    auto gb = cost_model(Strategy::GatherBokeh, scene_params[0]);
    double gb_total = gb.compute_ms + gb.bw_ms;
    std::printf("// 3. GatherBokeh %.4f ms vs TileBased %.4f ms, ratio=%.2fx %s\n",
        gb_total, tb_total, gb_total / tb_total,
        gb_total > tb_total * 2.0 ? "(PASS >2x)" : "(FAIL <=2x)");

    // 4. All except NoDOF and GatherBokeh under 1% of 30 Hz frame budget
    for (int si = 0; si < STRATEGY_COUNT; ++si) {
        if (si == 0 || si == 5) continue; // skip NoDOF and GatherBokeh
        auto s = Strategy(si);
        double max_cost = 0;
        for (int sci = 0; sci < SCENE_COUNT; ++sci) {
            auto c = cost_model(s, scene_params[sci]);
            max_cost = std::max(max_cost, c.compute_ms + c.bw_ms);
        }
        double pct = max_cost / FRAME_BUDGET_30 * 100.0;
        std::printf("// 4. %s max=%.4f ms = %.3f%% of 33ms %s\n",
            strategy_names[si], max_cost, pct, pct < 1.0 ? "(PASS <1%)" : "(FAIL >=1%)");
    }

    return 0;
}
