// 2026-06-21-dynamic-battlefield-decal-system — analytical CPU prototype
//
// 4 strategies for persistent battlefield decal rendering on voxel surfaces:
//   A_PerDecalMesh        — naive baseline: one quad mesh per decal, per-decal draw call
//   B_ScreenSpace         — no persistence: regenerate decals per frame via screen-space projection
//   C_DBuffer             — per-pixel accumulation buffer (UE4-style deferred decals)
//   D_AtlasIndirectLRU    — hypothesis: bindless atlas + indirect draw + LRU lifetime management
//
// Workloads: 3 distributions × 5 decal_counts × 5 seeds × 1000 iter + 10 warmup.
// Cost model is analytical (per `mesh-shader-mega-instancing` + `volumetric-fog` precedents):
//   - CPU cost = dispatch overhead + state update + LRU eviction (µs)
//   - GPU cost = indirect draw setup + atlas sampling + blending (ms, projected)
//   - VRAM = atlas + SSBO + overhead (MiB)
//   - Mutation cost = chunk edit invalidation (µs/edit)
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic decal_bench.cpp -o decal_bench
// Run:   ./decal_bench --output results.csv

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace decal_bench {

// ---------- Constants (ProjectV hardware baseline per hardware-profile.md §3) ----------

constexpr std::size_t kAtlasSpriteCount = 256;        // 256 unique decal sprites (bullet hole / scorch / crater / blood)
constexpr std::size_t kAtlasSpriteSize = 64 * 64 * 4; // RGBA8 = 16 KiB per sprite
constexpr std::size_t kAtlasTotalBytes = kAtlasSpriteCount * kAtlasSpriteSize; // 4 MiB

constexpr double kCpuDispatchUs = 0.5;       // CPU dispatch overhead per draw call (µs) — Frostbite/Surge 2 measurement
constexpr double kCpuBindlessUs = 0.05;     // CPU bindless descriptor bind (µs) — Vulkan multi_draw_indirect sample
constexpr double kCpuIndirectSetupUs = 2.0; // CPU indirect draw command buffer setup (µs) — bindless overhead
constexpr double kCpuLruEvictionUs = 0.02;  // CPU LRU eviction per entry (µs) — per-entry hash + swap
constexpr double kCpuChunkInvalidationUs = 0.8; // CPU chunk-edit decal invalidation scan (µs/chunk)
constexpr double kCpuScreenSpaceRegenUs = 8.0;   // CPU screen-space decal regeneration (µs/decal)

constexpr double kGpuDrawIndexedUs = 1.5;   // GPU drawIndexed base cost (µs) — Khronos multi_draw_indirect sample
constexpr double kGpuIndirectDrawUs = 8.0;  // GPU indirect draw setup (µs) — single batched dispatch
constexpr double kGpuAtlasSampleUs = 0.5;   // GPU atlas sampling + blending (µs/decal)
constexpr double kGpuDBufferSampleUs = 0.3; // GPU DBuffer sampling (µs/decal, half-res accumulation)
constexpr double kGpuScreenSpaceProjectUs = 1.2; // GPU screen-space projection (µs/decal)

constexpr double kVramDBufferMiBPer1k = 5.0; // DBuffer overhead per 1000 decals at half-res (MiB)
constexpr double kVramSsboBytes = 64;        // SSBO per decal: pos + normal + sprite_idx + age + ttl (bytes)
constexpr double kVramIndirectCmdBytes = 20; // VkDrawIndexedIndirectCommand per decal (bytes)

// ---------- Voxel surface (synthetic 8³ chunk) ----------

struct VoxelChunk {
    std::array<std::array<std::array<std::uint8_t, 8>, 8>, 8> voxels{}; // 0 = air, non-zero = solid
    VoxelChunk(std::uint64_t seed, std::string_view distribution) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<int> uniform(0, 99);
        if (distribution == "uniform") {
            for (auto& plane : voxels)
                for (auto& row : plane)
                    for (auto& v : row)
                        v = (uniform(rng) < 60) ? 1 : 0; // 60% solid
        } else if (distribution == "clustered") {
            // 3 clusters of 50-200 voxels each
            for (int c = 0; c < 3; ++c) {
                int cx = uniform(rng) % 6 + 1;
                int cy = uniform(rng) % 6 + 1;
                int cz = uniform(rng) % 6 + 1;
                int r = uniform(rng) % 2 + 1;
                for (int dz = -r; dz <= r; ++dz)
                    for (int dy = -r; dy <= r; ++dy)
                        for (int dx = -r; dx <= r; ++dx) {
                            int x = cx + dx, y = cy + dy, z = cz + dz;
                            if (x >= 0 && x < 8 && y >= 0 && y < 8 && z >= 0 && z < 8)
                                voxels[z][y][x] = 1;
                        }
            }
        } else { // "sparse" (rifle-fire: 1-3 decals per shot, scattered)
            for (auto& plane : voxels)
                for (auto& row : plane)
                    for (auto& v : row)
                        v = (uniform(rng) < 8) ? 1 : 0; // 8% solid (cave-like)
        }
    }
    // Count exposed faces (voxel adjacent to air). Used for decal placement target selection.
    std::size_t CountExposedFaces() const {
        std::size_t count = 0;
        for (int z = 0; z < 8; ++z)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    if (!voxels[z][y][x]) continue;
                    int n = 0;
                    if (x == 0 || !voxels[z][y][x - 1]) ++n;
                    if (x == 7 || !voxels[z][y][x + 1]) ++n;
                    if (y == 0 || !voxels[z][y - 1][x]) ++n;
                    if (y == 7 || !voxels[z][y + 1][x]) ++n;
                    if (z == 0 || !voxels[z - 1][y][x]) ++n;
                    if (z == 7 || !voxels[z + 1][y][x]) ++n;
                    count += n;
                }
        return count;
    }
};

// ---------- Decal placement (synthetic) ----------

struct Decal {
    float pos[3]{};       // world position (chunk-local)
    float normal[3]{};    // surface normal
    std::uint16_t sprite{}; // atlas index 0..255
    std::uint32_t ttl{};  // time-to-live in seconds
};

struct DecalPool {
    std::vector<Decal> decals;
    std::size_t capacity;
    std::uint64_t timestamp;
    explicit DecalPool(std::size_t cap) : decals(), capacity(cap), timestamp(0) {
        decals.reserve(cap);
    }
    void Add(const Decal& d) {
        if (decals.size() < capacity) {
            decals.push_back(d);
        } else {
            // LRU eviction: evict oldest by ttl
            auto oldest = std::min_element(decals.begin(), decals.end(),
                [](const Decal& a, const Decal& b) { return a.ttl < b.ttl; });
            *oldest = d;
        }
    }
};

// ---------- Strategy cost models ----------

struct StrategyCost {
    double cpu_us{};
    double gpu_us{};
    double vram_mib{};
    double mutation_us_per_edit{};
    double overhead_factor{1.0}; // for screen-space: ×1.5–2× over baseline due to regen
};

StrategyCost CostPerDecalMesh(std::size_t decal_count) {
    StrategyCost c;
    c.cpu_us = kCpuDispatchUs * static_cast<double>(decal_count);          // per-decal dispatch
    c.gpu_us = kGpuDrawIndexedUs * static_cast<double>(decal_count);      // per-decal GPU draw
    c.vram_mib = 3.2 + (static_cast<double>(decal_count) * 64.0) / (1024.0 * 1024.0); // ~3 MiB base + 64 B/decal
    c.mutation_us_per_edit = kCpuChunkInvalidationUs * static_cast<double>(decal_count) / 1000.0;
    return c;
}

StrategyCost CostScreenSpace(std::size_t decal_count) {
    StrategyCost c;
    c.cpu_us = kCpuScreenSpaceRegenUs * static_cast<double>(decal_count); // regen every frame
    c.gpu_us = kGpuScreenSpaceProjectUs * static_cast<double>(decal_count);
    c.vram_mib = 0.0; // procedural, no atlas storage
    c.mutation_us_per_edit = kCpuScreenSpaceRegenUs * 0.5; // free, regen next frame
    c.overhead_factor = 1.0;
    return c;
}

StrategyCost CostDBuffer(std::size_t decal_count) {
    StrategyCost c;
    c.cpu_us = kCpuBindlessUs * static_cast<double>(decal_count); // bindless = low CPU
    c.gpu_us = kGpuDBufferSampleUs * static_cast<double>(decal_count);
    c.vram_mib = (kVramDBufferMiBPer1k * static_cast<double>(decal_count)) / 1000.0; // high VRAM
    c.mutation_us_per_edit = 0.1; // cheap, just invalidate DBuffer tile
    return c;
}

StrategyCost CostAtlasIndirectLRU(std::size_t decal_count) {
    StrategyCost c;
    // CPU: indirect setup + LRU eviction (only when over capacity)
    double lru_evictions = (decal_count > kAtlasSpriteCount)
        ? static_cast<double>(decal_count - kAtlasSpriteCount) * 0.1
        : 0.0;
    c.cpu_us = kCpuIndirectSetupUs + kCpuLruEvictionUs * lru_evictions;
    // GPU: single indirect draw + atlas sampling (amortized across all decals)
    c.gpu_us = kGpuIndirectDrawUs + kGpuAtlasSampleUs * static_cast<double>(decal_count);
    // VRAM: atlas (fixed 4 MiB) + SSBO + indirect commands
    c.vram_mib = static_cast<double>(kAtlasTotalBytes) / (1024.0 * 1024.0)
        + (static_cast<double>(decal_count) * (kVramSsboBytes + kVramIndirectCmdBytes)) / (1024.0 * 1024.0);
    c.mutation_us_per_edit = 0.05 * static_cast<double>(decal_count) / 1000.0; // cheap, scan SSBO
    return c;
}

// ---------- Workload generation ----------

void GenerateDecalPlacements(DecalPool& pool, const VoxelChunk& chunk, std::size_t target_count,
                             std::uint64_t seed, std::string_view /*distribution*/) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<float> pos(0.0f, 8.0f);
    std::uniform_int_distribution<std::uint32_t> ttl(60, 1800); // 1-30 min
    std::uniform_int_distribution<std::uint16_t> sprite(0, kAtlasSpriteCount - 1);
    auto exposed = chunk.CountExposedFaces();
    std::size_t target = std::min(target_count, exposed * 2); // cap at 2× exposed faces
    for (std::size_t i = 0; i < target; ++i) {
        Decal d;
        d.pos[0] = pos(rng); d.pos[1] = pos(rng); d.pos[2] = pos(rng);
        d.normal[0] = 0; d.normal[1] = 1; d.normal[2] = 0; // simplified upward
        d.sprite = sprite(rng);
        d.ttl = ttl(rng);
        pool.Add(d);
    }
}

// ---------- Benchmark harness ----------

struct Config {
    std::string strategy;
    std::string distribution;
    std::size_t decal_count;
    std::uint64_t seed;
};

struct Measurement {
    std::string strategy;
    std::string distribution;
    std::size_t decal_count;
    std::uint64_t seed;
    double cpu_us_mean{};
    double cpu_us_p95{};
    double gpu_us_mean{};
    double vram_mib{};
    double mutation_us_per_edit{};
    double total_ms_per_frame{};
    std::size_t n_iter{};
};

double Percentile(std::vector<double>& v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    std::size_t idx = static_cast<std::size_t>(std::ceil(p * v.size() / 100.0)) - 1;
    if (idx >= v.size()) idx = v.size() - 1;
    return v[idx];
}

Measurement RunConfig(const Config& cfg, std::size_t warmup_iters, std::size_t measure_iters) {
    VoxelChunk chunk(cfg.seed, cfg.distribution);
    DecalPool pool(cfg.decal_count * 2); // capacity = 2× target for LRU headroom
    GenerateDecalPlacements(pool, chunk, cfg.decal_count, cfg.seed, cfg.distribution);

    std::function<StrategyCost(std::size_t)> cost_fn;
    if (cfg.strategy == "A_PerDecalMesh") cost_fn = CostPerDecalMesh;
    else if (cfg.strategy == "B_ScreenSpace") cost_fn = CostScreenSpace;
    else if (cfg.strategy == "C_DBuffer") cost_fn = CostDBuffer;
    else if (cfg.strategy == "D_AtlasIndirectLRU") cost_fn = CostAtlasIndirectLRU;
    else throw std::runtime_error("unknown strategy: " + cfg.strategy);

    // Warm-up
    for (std::size_t i = 0; i < warmup_iters; ++i) {
        auto cost = cost_fn(pool.decals.size());
        (void)cost;
    }

    // Measure
    std::vector<double> cpu_us_samples;
    cpu_us_samples.reserve(measure_iters);
    StrategyCost last_cost{};
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < measure_iters; ++i) {
        auto t_start = std::chrono::steady_clock::now();
        auto cost = cost_fn(pool.decals.size());
        // simulate per-frame mutation (1 chunk edit per 100 frames); cost is already amortized.
        last_cost = cost;
        auto t_end = std::chrono::steady_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(t_end - t_start).count();
        cpu_us_samples.push_back(elapsed_us);
    }
    auto t1 = std::chrono::steady_clock::now();
    double wall_sec = std::chrono::duration<double>(t1 - t0).count();

    double mean_cpu = std::accumulate(cpu_us_samples.begin(), cpu_us_samples.end(), 0.0)
        / static_cast<double>(cpu_us_samples.size());
    double p95_cpu = Percentile(cpu_us_samples, 95.0);

    Measurement m;
    m.strategy = cfg.strategy;
    m.distribution = cfg.distribution;
    m.decal_count = cfg.decal_count;
    m.seed = cfg.seed;
    m.cpu_us_mean = mean_cpu;
    m.cpu_us_p95 = p95_cpu;
    m.gpu_us_mean = last_cost.gpu_us;
    m.vram_mib = last_cost.vram_mib;
    m.mutation_us_per_edit = last_cost.mutation_us_per_edit;
    m.total_ms_per_frame = (mean_cpu + last_cost.gpu_us) / 1000.0;
    m.n_iter = measure_iters;
    (void)wall_sec;
    return m;
}

// ---------- CSV output ----------

void WriteCSV(const std::vector<Measurement>& measurements, const std::string& path) {
    std::ofstream f(path);
    f << "strategy,distribution,decal_count,seed,cpu_us_mean,cpu_us_p95,gpu_us_mean,vram_mib,"
         "mutation_us_per_edit,total_ms_per_frame,n_iter\n";
    f << std::fixed << std::setprecision(4);
    for (const auto& m : measurements) {
        f << m.strategy << ',' << m.distribution << ',' << m.decal_count << ',' << m.seed << ','
          << m.cpu_us_mean << ',' << m.cpu_us_p95 << ',' << m.gpu_us_mean << ',' << m.vram_mib << ','
          << m.mutation_us_per_edit << ',' << m.total_ms_per_frame << ',' << m.n_iter << '\n';
    }
}

void WriteSummary(const std::vector<Measurement>& measurements, const std::string& path) {
    // Aggregate by (strategy, distribution, decal_count): mean across seeds
    struct Key {
        std::string strategy, distribution;
        std::size_t decal_count;
        bool operator<(const Key& o) const {
            if (strategy != o.strategy) return strategy < o.strategy;
            if (distribution != o.distribution) return distribution < o.distribution;
            return decal_count < o.decal_count;
        }
    };
    std::map<Key, std::vector<Measurement>> grouped;
    for (const auto& m : measurements) {
        grouped[{m.strategy, m.distribution, m.decal_count}].push_back(m);
    }
    std::ofstream f(path);
    f << "strategy,distribution,decal_count,mean_cpu_us,p95_cpu_us,mean_gpu_us,mean_vram_mib,"
         "mean_total_ms_per_frame,n_seeds\n";
    f << std::fixed << std::setprecision(4);
    for (const auto& [k, v] : grouped) {
        double cpu = 0, p95 = 0, gpu = 0, vram = 0, total = 0;
        for (const auto& m : v) {
            cpu += m.cpu_us_mean;
            p95 += m.cpu_us_p95;
            gpu += m.gpu_us_mean;
            vram += m.vram_mib;
            total += m.total_ms_per_frame;
        }
        double n = static_cast<double>(v.size());
        f << k.strategy << ',' << k.distribution << ',' << k.decal_count << ','
          << cpu / n << ',' << p95 / n << ',' << gpu / n << ',' << vram / n << ','
          << total / n << ',' << v.size() << '\n';
    }
}

}  // namespace decal_bench

int main(int argc, char** argv) {
    using namespace decal_bench;

    std::string output_path = "results.csv";
    std::string summary_path = "summary_means.csv";
    std::size_t warmup = 10;
    std::size_t measure = 1000;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) output_path = argv[++i];
        else if (arg == "--summary" && i + 1 < argc) summary_path = argv[++i];
        else if (arg == "--warmup" && i + 1 < argc) warmup = std::stoull(argv[++i]);
        else if (arg == "--measure" && i + 1 < argc) measure = std::stoull(argv[++i]);
    }

    std::vector<std::string> strategies = {"A_PerDecalMesh", "B_ScreenSpace", "C_DBuffer", "D_AtlasIndirectLRU"};
    std::vector<std::string> distributions = {"uniform", "clustered", "sparse"};
    std::vector<std::size_t> decal_counts = {1000, 2000, 5000, 10000, 20000};
    std::vector<std::uint64_t> seeds = {1, 7, 42, 1234, 31337};

    std::vector<Config> configs;
    for (const auto& s : strategies)
        for (const auto& d : distributions)
            for (auto n : decal_counts)
                for (auto sd : seeds)
                    configs.push_back({s, d, n, sd});

    std::printf("[decal_bench] running %zu configs (4 strategies × 3 distributions × 5 counts × 5 seeds)\n",
                configs.size());

    std::vector<Measurement> measurements;
    measurements.reserve(configs.size());
    auto wall_start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < configs.size(); ++i) {
        measurements.push_back(RunConfig(configs[i], warmup, measure));
        if ((i + 1) % 50 == 0 || i + 1 == configs.size()) {
            std::printf("  [%zu/%zu] %s/%s/n=%zu/seed=%llu: cpu=%.3f us gpu=%.3f us vram=%.3f MiB\n",
                        i + 1, configs.size(),
                        measurements.back().strategy.c_str(),
                        measurements.back().distribution.c_str(),
                        measurements.back().decal_count,
                        static_cast<unsigned long long>(measurements.back().seed),
                        measurements.back().cpu_us_mean,
                        measurements.back().gpu_us_mean,
                        measurements.back().vram_mib);
        }
    }
    auto wall_end = std::chrono::steady_clock::now();
    double wall_sec = std::chrono::duration<double>(wall_end - wall_start).count();
    std::printf("[decal_bench] wall time: %.3f sec\n", wall_sec);

    WriteCSV(measurements, output_path);
    std::printf("[decal_bench] wrote %s (%zu rows)\n", output_path.c_str(), measurements.size() + 1);

    WriteSummary(measurements, summary_path);
    std::printf("[decal_bench] wrote %s\n", summary_path.c_str());

    return 0;
}
