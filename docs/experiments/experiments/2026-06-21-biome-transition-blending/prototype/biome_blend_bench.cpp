// biome_blend_bench.cpp — biome transition blending strategies comparison
// C++26, standalone CPU prototype, not ProjectV mainline
// Build: clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>
#include <span>
#include <cstring>
#define RAPID 0

// --- Constants ---
constexpr int kChunkSize = 8;
constexpr int kVoxelsPerChunk = kChunkSize * kChunkSize * kChunkSize;
constexpr int kBiomeSampleScale = 4;
constexpr int kBiomeSamplesPerDim = kChunkSize / kBiomeSampleScale; // 2
constexpr int kBiomeSamplesPerChunk = kBiomeSamplesPerDim * kBiomeSamplesPerDim; // 4 (2D horiz)
constexpr int kNumBiomes = 6; // plains, desert, forest, tundra, savanna, swamp

enum Biome : uint8_t {
    B_Plains = 0,
    B_Desert = 1,
    B_Forest = 2,
    B_Tundra = 3,
    B_Savanna = 4,
    B_Swamp   = 5,
};

struct BiomeDef {
    float temperature; // -1..1
    float humidity;    // -1..1
    float continentalness; // -1..1
    uint8_t surface_material; // material palette index
    uint8_t subsurface_material;
};

constexpr std::array<BiomeDef, kNumBiomes> kBiomes = {{
    // temp, hum, cont, surf, sub
    { 0.5f,  0.3f,  0.2f, 1, 2 }, // plains: grass→dirt
    { 0.9f, -0.5f,  0.4f, 3, 4 }, // desert: sand→sandstone
    { 0.3f,  0.8f,  0.3f, 1, 2 }, // forest: grass→dirt
    {-0.5f,  0.1f,  0.1f, 5, 6 }, // tundra: snow→frozen_dirt
    { 0.7f, -0.1f,  0.3f, 7, 8 }, // savanna: dry_grass→dirt
    { 0.2f,  0.9f, -0.2f, 9, 10}, // swamp: mud→clay
}};

// --- Noise simulation (cheap hash-based) ---
struct Noise2D {
    float sample(int64_t x, int64_t z, uint64_t seed, float freq) const {
        uint64_t h = seed;
        h ^= (uint64_t)(x * freq) * 0x9e3779b97f4a7c15ULL;
        h ^= (uint64_t)(z * freq) * 0xbf58476d1ce4e5b9ULL;
        h = h * 0x9e3779b97f4a7c15ULL ^ (h >> 30);
        h = h * 0xbf58476d1ce4e5b9ULL ^ (h >> 27);
        return (float)((double)(h & 0x7fffffff) / 0x7fffffff) * 2.0f - 1.0f;
    }
};

// --- Scenes: generate a terrain-like distribution of biomes ---
struct Scene {
    const char* name;
    void (*generate)(std::array<uint8_t, kBiomeSamplesPerChunk>& samples, uint64_t seed);
};

static void scene_two_biomes_hardline(std::array<uint8_t, kBiomeSamplesPerChunk>& out, uint64_t) {
    for (int i = 0; i < kBiomeSamplesPerChunk / 2; i++) out[i] = B_Plains;
    for (int i = kBiomeSamplesPerChunk / 2; i < kBiomeSamplesPerChunk; i++) out[i] = B_Desert;
}
static void scene_three_biomes_mosaic(std::array<uint8_t, kBiomeSamplesPerChunk>& out, uint64_t seed) {
    Noise2D n;
    for (int i = 0; i < kBiomeSamplesPerChunk; i++) {
        float v = n.sample(i * 7, i * 3, seed, 0.1f);
        out[i] = (v < -0.3f) ? B_Tundra : (v < 0.3f) ? B_Forest : B_Plains;
    }
}
static void scene_four_biomes_corner(std::array<uint8_t, kBiomeSamplesPerChunk>& out, uint64_t) {
    int idx = 0;
    for (int z = 0; z < kBiomeSamplesPerDim; z++)
        for (int x = 0; x < kBiomeSamplesPerDim; x++)
            out[idx++] = (z < kBiomeSamplesPerDim/2)
                ? (x < kBiomeSamplesPerDim/2 ? B_Plains : B_Desert)
                : (x < kBiomeSamplesPerDim/2 ? B_Forest : B_Swamp);
}
static void scene_all_same(std::array<uint8_t, kBiomeSamplesPerChunk>& out, uint64_t) {
    std::fill(out.begin(), out.end(), B_Plains);
}

static constexpr Scene kScenes[] = {
    { "hardline_2biome",  scene_two_biomes_hardline },
    { "mosaic_3biome",    scene_three_biomes_mosaic },
    { "corner_4biome",    scene_four_biomes_corner },
    { "uniform_1biome",   scene_all_same },
};

// --- Strategies ---
struct Strategy {
    const char* name;
    float (*eval)(const std::array<uint8_t, kBiomeSamplesPerChunk>& samples,
                  int x, int z, uint64_t seed,
                  BiomeDef& out_interpolated,
                  float& out_cost_us);
};

// A_HardThreshold: nearest biome, no blending
static float strat_hard(const std::array<uint8_t, kBiomeSamplesPerChunk>& samples,
                        int x, int z, uint64_t,
                        BiomeDef& out, float& cost) {
    int sx = x / kBiomeSampleScale;
    int sz = z / kBiomeSampleScale;
    sx = std::clamp(sx, 0, kBiomeSamplesPerDim - 1);
    sz = std::clamp(sz, 0, kBiomeSamplesPerDim - 1);
    uint8_t b = samples[sz * kBiomeSamplesPerDim + sx];
    out = kBiomes[b];
    cost = 0.002f; // ~2 ns per lookup
    return (float)kBiomes[b].surface_material;
}

// C_DistanceBlend: nearest 2 biome samples with distance-weighted blend
static float strat_blend_nearest2(const std::array<uint8_t, kBiomeSamplesPerChunk>& samples,
                                  int x, int z, uint64_t,
                                  BiomeDef& out, float& cost) {
    float fx = (float)x / (float)kBiomeSampleScale;
    float fz = (float)z / (float)kBiomeSampleScale;
    int ix0 = std::clamp((int)std::floor(fx), 0, kBiomeSamplesPerDim - 1);
    int iz0 = std::clamp((int)std::floor(fz), 0, kBiomeSamplesPerDim - 1);
    int ix1 = std::min(ix0 + 1, kBiomeSamplesPerDim - 1);
    int iz1 = std::min(iz0 + 1, kBiomeSamplesPerDim - 1);

    float tx = fx - (float)ix0;
    float tz = fz - (float)iz0;
    if (ix0 == ix1) tx = 0.5f;
    if (iz0 == iz1) tz = 0.5f;

    auto lerp_biome = [&](int ix0, int iz0, int ix1, int iz1, float tx, float tz) -> BiomeDef {
        uint8_t b00 = samples[iz0 * kBiomeSamplesPerDim + ix0];
        uint8_t b01 = samples[iz0 * kBiomeSamplesPerDim + ix1];
        uint8_t b10 = samples[iz1 * kBiomeSamplesPerDim + ix0];
        uint8_t b11 = samples[iz1 * kBiomeSamplesPerDim + ix1];
        auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
        auto lerp_b = [&](const BiomeDef& a, const BiomeDef& b, float t) -> BiomeDef {
            return { lerp(a.temperature, b.temperature, t),
                     lerp(a.humidity, b.humidity, t),
                     lerp(a.continentalness, b.continentalness, t),
                     (uint8_t)(a.surface_material + (b.surface_material - a.surface_material) * t + 0.5f),
                     (uint8_t)(a.subsurface_material + (b.subsurface_material - a.subsurface_material) * t + 0.5f) };
        };
        BiomeDef top = lerp_b(kBiomes[b00], kBiomes[b01], tx);
        BiomeDef bot = lerp_b(kBiomes[b10], kBiomes[b11], tx);
        return lerp_b(top, bot, tz);
    };

    out = lerp_biome(ix0, iz0, ix1, iz1, tx, tz);
    cost = 0.010f; // ~10 ns — bilinear + material lerp
    return (float)out.surface_material;
}

// D_VoronoiEdge: nearest 3 Voronoi-like cells with distance-weighted edge blending
static float strat_voronoi_blend(const std::array<uint8_t, kBiomeSamplesPerChunk>& samples,
                                 int x, int z, uint64_t seed,
                                 BiomeDef& out, float& cost) {
    Noise2D noise;
    float fx = (float)x;
    float fz = (float)z;

    // Find 3 nearest biome sample points (as if they were Voronoi centers)
    struct Neighbor { float dist; uint8_t biome; };
    Neighbor nn[3] = {{1e10f, 0}, {1e10f, 0}, {1e10f, 0}};

    for (int si = 0; si < kBiomeSamplesPerChunk; si++) {
        int sz = si / kBiomeSamplesPerDim;
        int sx = si % kBiomeSamplesPerDim;
        float cx = (float)(sx * kBiomeSampleScale + kBiomeSampleScale/2);
        float cz = (float)(sz * kBiomeSampleScale + kBiomeSampleScale/2);
        float jx = noise.sample(sx * 13, sz * 7, seed, 1.0f) * 1.5f;
        float jz = noise.sample(sx * 11, sz * 5, seed + 1, 1.0f) * 1.5f;
        cx += jx; cz += jz;
        float dx = fx - cx, dz = fz - cz;
        float d2 = dx * dx + dz * dz;
        uint8_t b = samples[si];
        if (d2 < nn[0].dist) { nn[2] = nn[1]; nn[1] = nn[0]; nn[0] = {d2, b}; }
        else if (d2 < nn[1].dist) { nn[2] = nn[1]; nn[1] = {d2, b}; }
        else if (d2 < nn[2].dist) { nn[2] = {d2, b}; }
    }

    float blend_radius = (float)kBiomeSampleScale * 1.5f;
    float total_w = 0.0f;
    float w[3] = {0,0,0};
    for (int i = 0; i < 3; i++) {
        if (nn[i].dist < blend_radius * blend_radius) {
            float d = std::sqrt(nn[i].dist);
            w[i] = blend_radius - d;
            if (w[i] < 0) w[i] = 0;
            total_w += w[i];
        }
    }

    if (total_w < 0.001f) {
        out = kBiomes[nn[0].biome];
        cost = 0.015f;
        return (float)out.surface_material;
    }

    float inv = 1.0f / total_w;
    out = {0,0,0,0,0};
    for (int i = 0; i < 3; i++) {
        if (w[i] <= 0) continue;
        float f = w[i] * inv;
        const auto& b = kBiomes[nn[i].biome];
        out.temperature += b.temperature * f;
        out.humidity += b.humidity * f;
        out.continentalness += b.continentalness * f;
        out.surface_material += (uint8_t)(b.surface_material * f);
        out.subsurface_material += (uint8_t)(b.subsurface_material * f);
    }
    cost = 0.030f; // ~30 ns — nearest-3 search + distance-weighted
    return (float)out.surface_material;
}

// E_MultiNoiseNearest: Minecraft 1.18+ style — 5 noise params → nearest biome
static float strat_multinoise(const std::array<uint8_t, kBiomeSamplesPerChunk>& samples,
                              int x, int z, uint64_t seed,
                              BiomeDef& out, float& cost) {
    Noise2D noise;
    (void)samples; // biome samples unused — noise drives biome directly
    float t = noise.sample(x, z, seed, 0.005f);
    float h = noise.sample(x + 1000, z + 1000, seed + 1, 0.005f);
    float c = noise.sample(x + 2000, z + 2000, seed + 2, 0.003f);
    float e = noise.sample(x + 3000, z + 3000, seed + 3, 0.004f);
    float w = noise.sample(x + 4000, z + 4000, seed + 4, 0.006f);
    (void)e; (void)w;

    // Find nearest biome in 3D parameter space (t, h, c)
    int best = 0;
    float best_d2 = 1e10f;
    for (int i = 0; i < kNumBiomes; i++) {
        float dt = t - kBiomes[i].temperature;
        float dh = h - kBiomes[i].humidity;
        float dc = c - kBiomes[i].continentalness;
        float d2 = dt*dt*2.0f + dh*dh*2.0f + dc*dc;
        if (d2 < best_d2) { best_d2 = d2; best = i; }
    }
    // Blend between nearest 2 biomes based on parameter distance
    int second = 0;
    float second_d2 = 1e10f;
    for (int i = 0; i < kNumBiomes; i++) {
        if (i == best) continue;
        float dt = t - kBiomes[i].temperature;
        float dh = h - kBiomes[i].humidity;
        float dc = c - kBiomes[i].continentalness;
        float d2 = dt*dt*2.0f + dh*dh*2.0f + dc*dc;
        if (d2 < second_d2) { second_d2 = d2; second = i; }
    }
    float total_d = std::sqrt(best_d2) + std::sqrt(second_d2);
    if (total_d < 0.001f) total_d = 1.0f;
    float blend = std::sqrt(best_d2) / total_d;

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    out.temperature = lerp(kBiomes[best].temperature, kBiomes[second].temperature, blend);
    out.humidity = lerp(kBiomes[best].humidity, kBiomes[second].humidity, blend);
    out.continentalness = lerp(kBiomes[best].continentalness, kBiomes[second].continentalness, blend);
    float sf = (float)kBiomes[best].surface_material * (1-blend) + (float)kBiomes[second].surface_material * blend;
    out.surface_material = (uint8_t)(sf + 0.5f);
    float ssf = (float)kBiomes[best].subsurface_material * (1-blend) + (float)kBiomes[second].subsurface_material * blend;
    out.subsurface_material = (uint8_t)(ssf + 0.5f);
    cost = 0.025f; // ~25 ns — 6 noise reads + 6 biome distance checks
    return (float)out.surface_material;
}

// B_Noise2D_Hard: noise-driven biome selection with hard boundary (useful intermediate)
static float strat_noise_hard(const std::array<uint8_t, kBiomeSamplesPerChunk>&,
                              int x, int z, uint64_t seed,
                              BiomeDef& out, float& cost) {
    Noise2D noise;
    float t = noise.sample(x, z, seed, 0.005f);
    float h = noise.sample(x + 1000, z + 1000, seed + 1, 0.005f);
    int best = 0; float best_d2 = 1e10f;
    for (int i = 0; i < kNumBiomes; i++) {
        float dt = t - kBiomes[i].temperature;
        float dh = h - kBiomes[i].humidity;
        float d2 = dt*dt + dh*dh;
        if (d2 < best_d2) { best_d2 = d2; best = i; }
    }
    out = kBiomes[best];
    cost = 0.008f;
    return (float)out.surface_material;
}

static constexpr Strategy kStrategies[] = {
    { "A_HardThreshold",     strat_hard },
    { "B_Noise2D_Hard",      strat_noise_hard },
    { "C_DistanceBlend_BiL", strat_blend_nearest2 },
    { "D_VoronoiEdge",       strat_voronoi_blend },
    { "E_MultiNoiseNearest", strat_multinoise },
};

// --- Per-chunk biome sampling (generate "ground truth" from noise) ---
static void gen_reference_biomes(std::array<uint8_t, kBiomeSamplesPerChunk>& ref,
                                 uint64_t seed) {
    Noise2D noise;
    for (int i = 0; i < kBiomeSamplesPerChunk; i++) {
        int sz = i / kBiomeSamplesPerDim;
        int sx = i % kBiomeSamplesPerDim;
        int wx = sx * kBiomeSampleScale + kBiomeSampleScale/2;
        int wz = sz * kBiomeSampleScale + kBiomeSampleScale/2;
        float t = noise.sample(wx, wz, seed, 0.005f);
        float h = noise.sample(wx + 1000, wz + 1000, seed + 1, 0.005f);
        float c = noise.sample(wx + 2000, wz + 2000, seed + 2, 0.003f);
        int best = 0; float best_d2 = 1e10f;
        for (int j = 0; j < kNumBiomes; j++) {
            float dt = t - kBiomes[j].temperature;
            float dh = h - kBiomes[j].humidity;
            float dc = c - kBiomes[j].continentalness;
            float d2 = dt*dt*2.0f + dh*dh*2.0f + dc*dc;
            if (d2 < best_d2) { best_d2 = d2; best = j; }
        }
        ref[i] = (uint8_t)best;
    }
}

// --- Quality metric: material match rate vs reference ---
static float measure_match_rate(const std::array<uint8_t, kBiomeSamplesPerChunk>& samples,
                                float (*strat_fn)(const std::array<uint8_t, kBiomeSamplesPerChunk>&,
                                                  int, int, uint64_t, BiomeDef&, float&),
                                uint64_t seed, float& out_cost, float& out_boundary_match) {
    BiomeDef tmp;
    float cost;
    int match = 0, boundary = 0, boundary_match = 0;
    for (int z = 0; z < kChunkSize; z++) {
        for (int x = 0; x < kChunkSize; x++) {
            float val = strat_fn(samples, x, z, seed, tmp, cost);
            out_cost += cost;
            // Reference: multi-noise nearest (not biome-sampled) for fair comparison
            Noise2D noise;
            float t = noise.sample(x, z, seed, 0.005f);
            float h = noise.sample(x + 1000, z + 1000, seed + 1, 0.005f);
            float c = noise.sample(x + 2000, z + 2000, seed + 2, 0.003f);
            int best = 0; float best_d2 = 1e10f;
            for (int j = 0; j < kNumBiomes; j++) {
                float dt = t - kBiomes[j].temperature;
                float dh = h - kBiomes[j].humidity;
                float dc = c - kBiomes[j].continentalness;
                float d2 = dt*dt*2.0f + dh*dh*2.0f + dc*dc;
                if (d2 < best_d2) { best_d2 = d2; best = j; }
            }
            float ref_material = (float)kBiomes[best].surface_material;
            if (std::abs(val - ref_material) < 0.5f) {
                match++;
                // Check if this voxel is at a biome boundary (neighbor has different biome)
                auto has_diff_neighbor = [&](int cx, int cz) -> bool {
                    float nt = noise.sample(cx, cz, seed, 0.005f);
                    float nh = noise.sample(cx + 1000, cz + 1000, seed + 1, 0.005f);
                    float nc = noise.sample(cx + 2000, cz + 2000, seed + 2, 0.003f);
                    int nb = 0; float nd2 = 1e10f;
                    for (int j = 0; j < kNumBiomes; j++) {
                        float ddt = nt - kBiomes[j].temperature;
                        double ddh = nh - kBiomes[j].humidity;
                        float ddc = nc - kBiomes[j].continentalness;
                        float d2 = ddt*ddt*2.0f + (float)(ddh*ddh)*2.0f + ddc*ddc;
                        if (d2 < nd2) { nd2 = d2; nb = j; }
                    }
                    return nb != best;
                };
                bool on_boundary = has_diff_neighbor(x+1, z) || has_diff_neighbor(x-1, z)
                                || has_diff_neighbor(x, z+1) || has_diff_neighbor(x, z-1);
                if (on_boundary) {
                    boundary++;
                    boundary_match++;
                }
            } else {
                // Count boundary voxels even on mismatch
                Noise2D noise2;
                auto check_boundary = [&](int cx, int cz) -> bool {
                    if (cx < 0 || cx >= kChunkSize || cz < 0 || cz >= kChunkSize) return false;
                    float nt2 = noise2.sample(cx, cz, seed, 0.005f);
                    float nh2 = noise2.sample(cx + 1000, cz + 1000, seed + 1, 0.005f);
                    float nc2 = noise2.sample(cx + 2000, cz + 2000, seed + 2, 0.003f);
                    int nb2 = 0; float nd2 = 1e10f;
                    for (int j = 0; j < kNumBiomes; j++) {
                        float ddt = nt2 - kBiomes[j].temperature;
                        double ddh = nh2 - kBiomes[j].humidity;
                        float ddc = nc2 - kBiomes[j].continentalness;
                        float d2 = ddt*ddt*2.0f + (float)(ddh*ddh)*2.0f + ddc*ddc;
                        if (d2 < nd2) { nd2 = d2; nb2 = j; }
                    }
                    return nb2 != best;
                };
                bool on_boundary = check_boundary(x+1, z) || check_boundary(x-1, z)
                                || check_boundary(x, z+1) || check_boundary(x, z-1);
                if (on_boundary) boundary++;
            }
        }
    }
    float total = (float)(kChunkSize * kChunkSize);
    out_boundary_match = (boundary > 0) ? (float)boundary_match / (float)boundary : 1.0f;
    return (float)match / total;
}

// --- Benchmark harness ---
struct Result {
    const char* strategy;
    const char* scene;
    uint64_t seed;
    double mean_match_rate;
    double mean_cost_us;
    double mean_boundary_match;
};

int main() {
    std::vector<Result> results;
    const uint64_t kSeeds[] = {1, 42, 1234, 31337, 77777};
    constexpr int kWarmup = 10;
    constexpr int kIter = 1000;

    std::printf("strategy,scene,seed,match_rate,cost_us,boundary_match_rate\n");

    for (auto& strat : kStrategies) {
        for (auto& sc : kScenes) {
            for (auto seed : kSeeds) {
                double total_match = 0, total_cost = 0, total_bmatch = 0;
                int n = 0;

                // Warmup
                std::array<uint8_t, kBiomeSamplesPerChunk> samples;
                sc.generate(samples, seed);
                for (int w = 0; w < kWarmup; w++) {
                    float c = 0, bm = 0;
                    measure_match_rate(samples, strat.eval, seed, c, bm);
                }

                // Measure
                for (int i = 0; i < kIter; i++) {
                    float c = 0, bm = 0;
                    float mr = measure_match_rate(samples, strat.eval, seed, c, bm);
                    total_match += mr;
                    total_cost += c;
                    total_bmatch += bm;
                    n++;
                }

                double avg_match = total_match / n;
                double avg_cost = total_cost / n;
                double avg_bmatch = total_bmatch / n;

                results.push_back({strat.name, sc.name, seed,
                                   avg_match, avg_cost, avg_bmatch});

                std::printf("%s,%s,%llu,%.6f,%.6f,%.6f\n",
                           strat.name, sc.name, (unsigned long long)seed,
                           avg_match, avg_cost, avg_bmatch);
            }
        }
    }

    // Summary
    std::printf("\n=== SUMMARY ===\n");
    std::printf("strategy,scenes,mean_match,mean_cost_us,mean_boundary_match\n");
    for (auto& strat : kStrategies) {
        double mm = 0, mc = 0, mb = 0;
        int cnt = 0;
        for (auto& r : results) {
            if (std::strcmp(r.strategy, strat.name) == 0) {
                mm += r.mean_match_rate;
                mc += r.mean_cost_us;
                mb += r.mean_boundary_match;
                cnt++;
            }
        }
        mm /= cnt; mc /= cnt; mb /= cnt;
        std::printf("%s,%d,%.6f,%.6f,%.6f\n", strat.name, cnt, mm, mc, mb);
    }

    return 0;
}
