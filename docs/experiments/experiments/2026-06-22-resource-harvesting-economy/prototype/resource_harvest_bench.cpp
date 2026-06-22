#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint32_t CHUNK_SIZE = 16;        // 16x16 cells per chunk
static constexpr uint32_t CHUNK_CELLS = CHUNK_SIZE * CHUNK_SIZE;
static constexpr uint32_t WORLD_CHUNKS = 64;       // 64x64 chunks
static constexpr uint32_t WORLD_CELLS = WORLD_CHUNKS * WORLD_CHUNKS * CHUNK_CELLS; // 1,048,576
static constexpr float    EXTRACTION_RATE = 0.1f;  // base extraction per tick
static constexpr uint32_t N_ITER = 1000;
static constexpr uint32_t N_WARMUP = 10;

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------
enum class Strategy : uint8_t {
    A_NoHarvesting = 0,
    B_StaticNode_Depletion,
    C_ProceduralNode_DynamicRichness,
    D_ExtractorBuilding_Tiered,
    E_FullEconomyChain,
    COUNT
};

static constexpr const char* STRATEGY_NAMES[] = {
    "A_NoHarvesting",
    "B_StaticNode_Depletion",
    "C_ProceduralNode_DynamicRichness",
    "D_ExtractorBuilding_Tiered",
    "E_FullEconomyChain"
};

static_assert(std::size(STRATEGY_NAMES) == static_cast<size_t>(Strategy::COUNT));

enum class Scene : uint8_t {
    s1_uniform = 0,
    s2_clustered,
    s3_deep_gradient,
    s4_oil_field,
    s5_multi_tier,
    COUNT
};

static constexpr const char* SCENE_NAMES[] = {
    "s1_uniform",
    "s2_clustered",
    "s3_deep_gradient",
    "s4_oil_field",
    "s5_multi_tier"
};

static_assert(std::size(SCENE_NAMES) == static_cast<size_t>(Scene::COUNT));

// Resource types
enum class Resource : uint8_t {
    None = 0,
    Iron,
    Copper,
    Coal,
    Oil,
    Rare,
    COUNT
};

// ---------------------------------------------------------------------------
// Cell — single voxel tile
// ---------------------------------------------------------------------------
struct Cell {
    Resource type  : 4;     // 0..5
    uint16_t richness : 12; // 0..4095 (fixed-point, /4095 = 0..1)
    uint16_t depth;         // 0 = surface
    uint32_t node_id;       // which node cluster this belongs to (0 = none)
};

static_assert(sizeof(Cell) <= 8);

// ---------------------------------------------------------------------------
// World state
// ---------------------------------------------------------------------------
struct World {
    std::vector<Cell> cells;        // flat: [chunk_idx * CHUNK_CELLS + local_idx]
    std::vector<uint64_t> chunk_seeds; // per-chunk RNG seed
    uint32_t num_chunks;
    uint32_t num_cells;

    World() : num_chunks(WORLD_CHUNKS * WORLD_CHUNKS), num_cells(num_chunks * CHUNK_CELLS) {
        cells.resize(num_cells);
        chunk_seeds.resize(num_chunks);
    }

    uint32_t chunk_index(uint32_t cx, uint32_t cz) const {
        return cz * WORLD_CHUNKS + cx;
    }

    uint32_t cell_index(uint32_t cx, uint32_t cz, uint32_t lx, uint32_t lz) const {
        return chunk_index(cx, cz) * CHUNK_CELLS + lz * CHUNK_SIZE + lx;
    }

    Cell& at(uint32_t cx, uint32_t cz, uint32_t lx, uint32_t lz) {
        return cells[cell_index(cx, cz, lx, lz)];
    }
};

// ---------------------------------------------------------------------------
// Scene generators
// ---------------------------------------------------------------------------
static void gen_uniform(World& w, std::mt19937_64& rng) {
    // 10% resource cells, evenly distributed, richness ~U(0.3, 1.0)
    std::uniform_real_distribution<float> rich_dist(0.3f, 1.0f);
    std::bernoulli_distribution has_resource(0.1f);
    for (uint32_t i = 0; i < w.num_cells; ++i) {
        auto& c = w.cells[i];
        if (has_resource(rng)) {
            c.type = static_cast<Resource>(1 + (rng() % 5));
            c.richness = static_cast<uint16_t>(rich_dist(rng) * 4095.0f);
            c.depth = static_cast<uint16_t>(rng() % 64);
            c.node_id = 1 + (rng() % 1000);
        }
    }
}

static void gen_clustered(World& w, std::mt19937_64& rng) {
    // 0.5% of chunks have rich clusters (80% cells in that chunk have resources)
    // rest are barren
    std::uniform_real_distribution<float> rich_dist(0.7f, 1.0f);
    std::bernoulli_distribution cluster_chance(0.005f);
    uint32_t node_counter = 1;
    for (uint32_t cz = 0; cz < WORLD_CHUNKS; ++cz) {
        for (uint32_t cx = 0; cx < WORLD_CHUNKS; ++cx) {
            if (!cluster_chance(rng)) continue;
            uint32_t nid = node_counter++;
            for (uint32_t lz = 0; lz < CHUNK_SIZE; ++lz) {
                for (uint32_t lx = 0; lx < CHUNK_SIZE; ++lx) {
                    auto& c = w.at(cx, cz, lx, lz);
                    c.type = static_cast<Resource>(1 + (rng() % 5));
                    c.richness = static_cast<uint16_t>(rich_dist(rng) * 4095.0f);
                    c.depth = 0;
                    c.node_id = nid;
                }
            }
        }
    }
}

static void gen_deep_gradient(World& w, std::mt19937_64& rng) {
    // Resources found only below depth 20, richness increases with depth
    std::uniform_real_distribution<float> rich_dist(0.0f, 1.0f);
    std::bernoulli_distribution has_resource(0.08f);
    for (uint32_t i = 0; i < w.num_cells; ++i) {
        auto& c = w.cells[i];
        c.depth = static_cast<uint16_t>(rng() % 64);
        if (c.depth < 20) continue;
        if (has_resource(rng)) {
            c.type = static_cast<Resource>(1 + (rng() % 5));
            float depth_factor = static_cast<float>(c.depth) / 64.0f;
            c.richness = static_cast<uint16_t>((0.3f + 0.7f * depth_factor) * 4095.0f);
            c.node_id = 1 + (rng() % 1000);
        }
    }
}

static void gen_oil_field(World& w, std::mt19937_64& rng) {
    // Single massive oil field in center, all other cells barren
    // Only resources in a central 4x4 chunk area
    std::uniform_real_distribution<float> rich_dist(0.5f, 1.0f);
    uint32_t center = WORLD_CHUNKS / 2;
    for (uint32_t cz = center - 2; cz < center + 2; ++cz) {
        for (uint32_t cx = center - 2; cx < center + 2; ++cx) {
            for (uint32_t lz = 0; lz < CHUNK_SIZE; ++lz) {
                for (uint32_t lx = 0; lx < CHUNK_SIZE; ++lx) {
                    auto& c = w.at(cx, cz, lx, lz);
                    c.type = Resource::Oil;
                    c.richness = static_cast<uint16_t>(rich_dist(rng) * 4095.0f);
                    c.depth = 50 + static_cast<uint16_t>(rng() % 14);
                    c.node_id = 1;
                }
            }
        }
    }
}

static void gen_multi_tier(World& w, std::mt19937_64& rng) {
    // Iron (60%), Copper (25%), Coal (10%), Rare (5%) distributed uniformly
    std::uniform_real_distribution<float> rich_dist(0.3f, 1.0f);
    std::uniform_int_distribution<int> tier_dist(0, 99);
    for (uint32_t i = 0; i < w.num_cells; ++i) {
        auto& c = w.cells[i];
        int roll = tier_dist(rng);
        if (roll < 60) {
            c.type = Resource::Iron;
        } else if (roll < 85) {
            c.type = Resource::Copper;
        } else if (roll < 95) {
            c.type = Resource::Coal;
        } else if (roll < 100) {
            c.type = Resource::Rare;
        } else {
            continue;
        }
        c.richness = static_cast<uint16_t>(rich_dist(rng) * 4095.0f);
        c.depth = static_cast<uint16_t>(rng() % 64);
        c.node_id = 1 + (rng() % 5000);
    }
}

using SceneGen = void(*)(World&, std::mt19937_64&);
static constexpr SceneGen SCENE_GENS[] = {
    gen_uniform,
    gen_clustered,
    gen_deep_gradient,
    gen_oil_field,
    gen_multi_tier
};

// ---------------------------------------------------------------------------
// Strategy tick implementations
// ---------------------------------------------------------------------------
static void tick_no_harvest(World& w) {
    // Loop only — no operation. Measures iteration overhead.
    for (uint32_t i = 0; i < w.num_cells; ++i) {
        auto& c = w.cells[i];
        (void)c;
    }
}

static void tick_static_depletion(World& w) {
    // Foxhole-style: reduce richness, if depleted start respawn timer
    // Respawn timer stored as richness overflow (negative = counting down)
    for (uint32_t i = 0; i < w.num_cells; ++i) {
        auto& c = w.cells[i];
        if (c.type == Resource::None) continue;
        if (c.richness > 0) {
            // Normal extraction
            float r = static_cast<float>(c.richness) / 4096.0f;
            r = std::max(0.0f, r - EXTRACTION_RATE);
            c.richness = static_cast<uint16_t>(r * 4095.0f);
            if (c.richness == 0) {
                // Start respawn timer (encoded as negative: -1..-100 ticks)
                c.depth = 50 + (c.node_id % 51); // random respawn delay
            }
        } else {
            // Respawn countdown
            if (c.depth > 0) {
                c.depth--;
                if (c.depth == 0) {
                    c.richness = 4095; // full restore
                }
            }
        }
    }
}

static void tick_dynamic_richness(World& w) {
    // Depth-based extraction: deeper = higher yield, slow replenish
    for (uint32_t i = 0; i < w.num_cells; ++i) {
        auto& c = w.cells[i];
        if (c.type == Resource::None) continue;
        float r = static_cast<float>(c.richness) / 4096.0f;
        if (r > 0.0f) {
            float depth_bonus = 1.0f + static_cast<float>(c.depth) * 0.01f;
            float extraction = EXTRACTION_RATE * depth_bonus;
            r = std::max(0.0f, r - extraction);
            c.richness = static_cast<uint16_t>(r * 4095.0f);
        }
        // Slow replenish toward 4095
        float replenish = 0.005f * (1.0f + static_cast<float>(c.depth) * 0.005f);
        r = std::min(1.0f, r + replenish);
        c.richness = static_cast<uint16_t>(r * 4095.0f);
    }
}

static void tick_extractor_tiered(World& w) {
    // Extractor model: each cell is an "extractor" with tier (from depth)
    // output = base_rate * tier * proximity_bonus
    // Adjacency: check 4 neighbors for same node_id
    for (uint32_t cz = 0; cz < WORLD_CHUNKS; ++cz) {
        for (uint32_t cx = 0; cx < WORLD_CHUNKS; ++cx) {
            for (uint32_t lz = 0; lz < CHUNK_SIZE; ++lz) {
                for (uint32_t lx = 0; lx < CHUNK_SIZE; ++lx) {
                    auto& c = w.at(cx, cz, lx, lz);
                    if (c.type == Resource::None) continue;
                    uint32_t tier = 1 + (c.depth / 16);
                    float r = static_cast<float>(c.richness) / 4096.0f;
                    // Adjacency: count same-node neighbors
                    uint32_t adj = 0;
                    if (lx > 0 && w.at(cx, cz, lx-1, lz).node_id == c.node_id) adj++;
                    if (lx+1 < CHUNK_SIZE && w.at(cx, cz, lx+1, lz).node_id == c.node_id) adj++;
                    if (lz > 0 && w.at(cx, cz, lx, lz-1).node_id == c.node_id) adj++;
                    if (lz+1 < CHUNK_SIZE && w.at(cx, cz, lx, lz+1).node_id == c.node_id) adj++;
                    float adj_bonus = 1.0f + static_cast<float>(adj) * 0.125f;
                    float extraction = EXTRACTION_RATE * static_cast<float>(tier) * adj_bonus;
                    r = std::max(0.0f, r - extraction);
                    c.richness = static_cast<uint16_t>(r * 4095.0f);
                }
            }
        }
    }
}

static void tick_full_chain(World& w) {
    // Full economy chain: harvest → refine → produce
    // Harvest rate based on richness, refine converts 2 ore → 1 bar, produce consumes bars
    struct Factory {
        uint32_t cell_idx;
        Resource input_type;
        float input_stock;
        float output_stock;
    };
    // Allocate factories once per tick (simplified: use every 10th cell as factory)
    thread_local std::vector<Factory> factories;
    factories.clear();

    for (uint32_t i = 0; i < w.num_cells; ++i) {
        auto& c = w.cells[i];
        if (c.type == Resource::None) continue;
        // Harvest
        float r = static_cast<float>(c.richness) / 4096.0f;
        float harvest = EXTRACTION_RATE * (0.5f + r * 0.5f);
        r = std::max(0.0f, r - harvest);
        c.richness = static_cast<uint16_t>(r * 4095.0f);

        // Every 10th cell is a factory
        if ((i % 10) == 0) {
            factories.push_back({i, c.type, harvest, 0.0f});
        }
    }

    // Refine: 2 input → 1 output
    for (auto& f : factories) {
        f.output_stock += f.input_stock * 0.5f;
    }
}

// ---------------------------------------------------------------------------
// Tick function table
// ---------------------------------------------------------------------------
using TickFn = void(*)(World&);
static constexpr TickFn TICK_FNS[] = {
    tick_no_harvest,
    tick_static_depletion,
    tick_dynamic_richness,
    tick_extractor_tiered,
    tick_full_chain
};

static_assert(std::size(TICK_FNS) == static_cast<size_t>(Strategy::COUNT));

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------
struct Timer {
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    TimePoint start;

    void begin() { start = Clock::now(); }

    double elapsed_ns() const {
        auto end = Clock::now();
        return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
};

// ---------------------------------------------------------------------------
// Benchmark runner
// ---------------------------------------------------------------------------
struct Result {
    Strategy strategy;
    Scene scene;
    uint64_t seed;
    double mean_ns;
    double median_ns;
    double p95_ns;
    double p99_ns;
    double std_ns;
};

static Result run_bench(Strategy strat, Scene sc, uint64_t seed) {
    World w;
    std::mt19937_64 rng{seed};

    // Generate scene
    SCENE_GENS[static_cast<size_t>(sc)](w, rng);

    // Warmup
    for (uint32_t i = 0; i < N_WARMUP; ++i) {
        TICK_FNS[static_cast<size_t>(strat)](w);
    }

    // Bench
    std::vector<double> samples;
    samples.reserve(N_ITER);
    Timer timer;

    for (uint32_t i = 0; i < N_ITER; ++i) {
        timer.begin();
        TICK_FNS[static_cast<size_t>(strat)](w);
        double ns = timer.elapsed_ns();
        samples.push_back(ns);
    }

    // Stats
    std::sort(samples.begin(), samples.end());
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    double mean = sum / static_cast<double>(samples.size());

    double sq_sum = 0.0;
    for (auto v : samples) {
        double diff = v - mean;
        sq_sum += diff * diff;
    }
    double variance = sq_sum / static_cast<double>(samples.size());
    double stddev = std::sqrt(variance);

    size_t n = samples.size();
    double median = samples[n / 2];
    double p95 = samples[static_cast<size_t>(n * 0.95)];
    double p99 = samples[static_cast<size_t>(n * 0.99)];

    return {strat, sc, seed, mean, median, p95, p99, stddev};
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    std::printf("strategy,scene,seed,mean_ns,median_ns,p95_ns,p99_ns,std_ns,ns_per_chunk\n");

    for (uint32_t s = 0; s < static_cast<uint32_t>(Strategy::COUNT); ++s) {
        auto strat = static_cast<Strategy>(s);
        for (uint32_t sc = 0; sc < static_cast<uint32_t>(Scene::COUNT); ++sc) {
            auto scene = static_cast<Scene>(sc);
            for (uint64_t seed = 1; seed <= 5; ++seed) {
                auto r = run_bench(strat, scene, seed);
                double ns_per_cell = r.mean_ns / static_cast<double>(WORLD_CELLS);
                double ns_per_chunk = ns_per_cell * static_cast<double>(CHUNK_CELLS);
                std::printf("%s,%s,%llu,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f\n",
                    STRATEGY_NAMES[s], SCENE_NAMES[sc],
                    static_cast<unsigned long long>(seed),
                    r.mean_ns, r.median_ns, r.p95_ns, r.p99_ns, r.std_ns,
                    ns_per_chunk);
            }
        }
    }

    return 0;
}
