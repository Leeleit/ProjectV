// ProjectV Experiment: random-tick-section-skip
// Standalone C++26 CPU benchmark
// Simulates random tick loop with/without section-skip (tickRefCount) optimization
// Strategies:
//   A_NoSkip       — always iterate 3 random positions, check material
//   B_CounterCheck — check tickRefCount first; skip section if 0
//   C_PreCollect   — Paper-style: pre-collected tickable position array + shuffle pick 3
//   D_HybridAdaptive — tiered: if few tickables → A, if many → C
//   E_BaselineLoop — minimal loop overhead (3 iterations, no work)

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

// --- Configuration ---
static constexpr int CHUNK_SIZE = 8;
static constexpr int CHUNK_VOXELS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE; // 512
static constexpr int RANDOM_TICKS_PER_CHUNK = 3; // MC default 3 per section per tick

// --- Scenes ---
// Each scene defines the density of tickable blocks across N chunks.
// Scene 0 = uniform_air:     0% tickable (all air, most common in real world)
// Scene 1 = uniform_stone:   0% tickable (all non-tickable solid)
// Scene 2 = forest_floor:    ~15% tickable (grass + flowers on surface)
// Scene 3 = farm_biome:      ~40% tickable (crops + soil + water)
// Scene 4 = mixed_biome:     ~5% tickable (scattered tickables, realistic average)

struct SceneConfig {
    const char* name;
    double tickableDensity;     // probability a random voxel is tickable
    double chunkDensity;        // probability a chunk has any tickable voxel
};

static constexpr SceneConfig SCENES[] = {
    {"uniform_air",   0.000, 0.00},
    {"uniform_stone", 0.000, 0.00},
    {"forest_floor",  0.150, 0.85},
    {"farm_biome",    0.400, 1.00},
    {"mixed_biome",   0.050, 0.40},
};

// --- World model ---
struct Chunk {
    std::array<uint8_t, CHUNK_VOXELS> materials;
    uint16_t tickRefCount; // number of tickable blocks in this chunk
    std::vector<uint16_t> tickablePositions; // indices of tickable blocks (for Strategy C/D)
};

struct World {
    std::vector<Chunk> chunks;
    int nchunks;
};

static World generateWorld(int nchunks, int sceneIdx, uint64_t seed) {
    World w;
    w.nchunks = nchunks;
    w.chunks.resize(nchunks);
    const auto& cfg = SCENES[sceneIdx];

    std::mt19937_64 rng(seed);
    std::bernoulli_distribution tickableDist(cfg.tickableDensity);
    std::bernoulli_distribution chunkDist(cfg.chunkDensity);

    for (auto& chunk : w.chunks) {
        chunk.tickRefCount = 0;
        chunk.tickablePositions.clear();
        chunk.tickablePositions.reserve(CHUNK_VOXELS);

        bool hasAnyTickable = (cfg.tickableDensity == 0.0) ? false : chunkDist(rng);

        for (int i = 0; i < CHUNK_VOXELS; ++i) {
            bool tickable = hasAnyTickable && tickableDist(rng);
            chunk.materials[i] = tickable ? 1 : 0;
            if (tickable) {
                ++chunk.tickRefCount;
                chunk.tickablePositions.push_back(static_cast<uint16_t>(i));
            }
        }
    }
    return w;
}

// --- Strategy A: NoSkip ---
// Always iterate 3 random positions, check material, call tick if tickable
static uint64_t tickStrategyA(const World& w, std::mt19937_64& rng) {
    uint64_t totalTicks = 0;
    std::uniform_int_distribution<int> posDist(0, CHUNK_VOXELS - 1);

    for (const auto& chunk : w.chunks) {
        for (int i = 0; i < RANDOM_TICKS_PER_CHUNK; ++i) {
            int pos = posDist(rng);
            if (chunk.materials[pos] != 0) {
                // tick the block (no-op in benchmark, count it)
                ++totalTicks;
            }
        }
    }
    return totalTicks;
}

// --- Strategy B: CounterCheck ---
// Check tickRefCount first; if 0, skip entire chunk (save 3 random draws + 3 material reads)
static uint64_t tickStrategyB(const World& w, std::mt19937_64& rng) {
    uint64_t totalTicks = 0;
    std::uniform_int_distribution<int> posDist(0, CHUNK_VOXELS - 1);

    for (const auto& chunk : w.chunks) {
        if (chunk.tickRefCount == 0) {
            continue; // skip entire chunk
        }
        for (int i = 0; i < RANDOM_TICKS_PER_CHUNK; ++i) {
            int pos = posDist(rng);
            if (chunk.materials[pos] != 0) {
                ++totalTicks;
            }
        }
    }
    return totalTicks;
}

// --- Strategy C: PreCollect (Paper-style) ---
// Maintain pre-collected array of tickable positions; pick random indices from the list
// This is Paper's approach: once-built tickable-positions array, random picks each tick
// Cost per chunk: rng() + array read + tick call (no wasted draws on non-tickable blocks)
static uint64_t tickStrategyC(const World& w, std::mt19937_64& rng) {
    uint64_t totalTicks = 0;

    for (const auto& chunk : w.chunks) {
        int n = static_cast<int>(chunk.tickablePositions.size());
        if (n == 0) continue;

        int k = std::min(RANDOM_TICKS_PER_CHUNK, n);
        std::uniform_int_distribution<int> posDist(0, n - 1);
        for (int i = 0; i < k; ++i) {
            int idx = posDist(rng);
            // tick the block at tickablePositions[idx] (no-op, count it)
            ++totalTicks;
            (void)idx;
        }
    }
    return totalTicks;
}

// --- Strategy D: HybridAdaptive ---
// Tiered: if tickRefCount <= 3 → random-position (like A, but counter check still saves skip)
//          if tickRefCount > 3  → pre-collect list (like C)
static uint64_t tickStrategyD(const World& w, std::mt19937_64& rng) {
    uint64_t totalTicks = 0;
    std::uniform_int_distribution<int> posDist(0, CHUNK_VOXELS - 1);

    for (const auto& chunk : w.chunks) {
        if (chunk.tickRefCount == 0) {
            continue;
        }
        if (chunk.tickRefCount <= RANDOM_TICKS_PER_CHUNK) {
            // Few tickables: random-position sampling (same as B)
            for (int i = 0; i < RANDOM_TICKS_PER_CHUNK; ++i) {
                int pos = posDist(rng);
                if (chunk.materials[pos] != 0) {
                    ++totalTicks;
                }
            }
        } else {
            // Many tickables: random index into pre-collected list (same as C)
            int n = static_cast<int>(chunk.tickablePositions.size());
            int k = std::min(RANDOM_TICKS_PER_CHUNK, n);
            if (k == 0) continue;

            std::uniform_int_distribution<int> posDist(0, n - 1);
            for (int i = 0; i < k; ++i) {
                int idx = posDist(rng);
                ++totalTicks;
                (void)idx;
            }
        }
    }
    return totalTicks;
}

// --- Strategy E: BaselineLoop ---
// Minimal overhead: 3 iterations per chunk, no reads, no checks
static uint64_t tickStrategyE(const World& w, std::mt19937_64& rng) {
    uint64_t totalTicks = 0;
    (void)rng;

    for (const auto& chunk : w.chunks) {
        for (int i = 0; i < RANDOM_TICKS_PER_CHUNK; ++i) {
            // Just count iterations, no work
            ++totalTicks;
            (void)chunk;
        }
    }
    return totalTicks;
}

// --- Benchmark harness ---
struct Measurement {
    double meanUs;
    double medianUs;
    double p95Us;
    double p99Us;
    double stdUs;
    uint64_t totalTicks;
    uint64_t nchunks;
};

static constexpr int WARMUP = 10;

template<typename Func>
static Measurement measure(Func tickFn, const World& w, uint64_t seed, int niter) {
    std::vector<double> times(niter);
    uint64_t totalTicks = 0;

    for (int i = 0; i < WARMUP; ++i) {
        std::mt19937_64 rng(seed + i);
        tickFn(w, rng);
    }

    for (int i = 0; i < niter; ++i) {
        std::mt19937_64 rng(seed + i);
        auto start = std::chrono::steady_clock::now();
        totalTicks = tickFn(w, rng);
        auto end = std::chrono::steady_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start).count();
        times[i] = us;
    }

    std::sort(times.begin(), times.end());
    double sum = std::accumulate(times.begin(), times.end(), 0.0);
    double mean = sum / niter;
    double median = times[niter / 2];
    double p95 = times[static_cast<int>(niter * 0.95)];
    double p99 = times[static_cast<int>(niter * 0.99)];

    double sqSum = 0.0;
    for (double t : times) {
        double d = t - mean;
        sqSum += d * d;
    }
    double stdDev = std::sqrt(sqSum / niter);

    return {mean, median, p95, p99, stdDev, totalTicks, static_cast<uint64_t>(w.nchunks)};
}

int main(int argc, char** argv) {
    int nchunks = 10000;  // 10K chunks = 80K sections in MC terms = realistic loaded area
    int niter = 1000;

    if (argc > 1) nchunks = std::atoi(argv[1]);
    if (argc > 2) niter = std::atoi(argv[2]);

    std::printf("strategy,scene,nchunks,mean_us,median_us,p95_us,p99_us,std_us,total_ticks\n");

    constexpr int NSTRATS = 5;
    using StratFn = uint64_t(*)(const World&, std::mt19937_64&);
    constexpr StratFn STRATS[NSTRATS] = {
        tickStrategyA, tickStrategyB, tickStrategyC, tickStrategyD, tickStrategyE
    };
    constexpr const char* STRAT_NAMES[NSTRATS] = {
        "A_NoSkip", "B_CounterCheck", "C_PreCollect", "D_HybridAdaptive", "E_BaselineLoop"
    };
    constexpr int NSCENES = sizeof(SCENES) / sizeof(SCENES[0]);
    constexpr uint64_t SEEDS[5] = {1, 7, 42, 1234, 31337};

    for (int si = 0; si < NSCENES; ++si) {
        for (uint64_t seed : SEEDS) {
            // Pre-generate world (shared across strategies per scene+seed)
            World w = generateWorld(nchunks, si, seed);

            for (int st = 0; st < NSTRATS; ++st) {
                auto m = measure(
                    [st, &STRATS](const World& w, std::mt19937_64& rng) {
                        return STRATS[st](w, rng);
                    },
                    w, seed, niter
                );
                std::printf("%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%lu\n",
                    STRAT_NAMES[st], SCENES[si].name, nchunks,
                    m.meanUs, m.medianUs, m.p95Us, m.p99Us, m.stdUs,
                    (unsigned long)m.totalTicks);
            }
        }
    }

    return 0;
}
