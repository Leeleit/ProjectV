// world_model.hpp — World state + stats helper + scene constructors для factory production
// Per `2026-06-21-factory-production-system` experiment.

#pragma once

#include <array>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace pvf {

// Item types
constexpr int kMaxItemTypes = 16;

// Per-item definition
struct ItemDef {
    const char* name;
    int build_ticks;          // ticks to build 1 unit
    int cost_mass;            // mass cost per unit
    int cost_energy;          // energy cost per unit
    std::array<int, kMaxItemTypes> dep_count; // units of item j needed per unit of item i
};

// World state
struct World {
    std::array<ItemDef, kMaxItemTypes> item_defs;
    
    // Per-factory state (SoA layout)
    int n_factories = 0;
    std::vector<uint8_t> factory_item;          // current item being built (0 = idle)
    std::vector<int> factory_progress;          // current build progress (0..build_ticks)
    std::vector<int> factory_target_count;     // how many of this item to build
    std::vector<int> factory_completed;         // count of completed items
    
    // Item stockpile (shared, simulates supply)
    std::array<int, kMaxItemTypes> stockpile;
    
    // Pending queues per factory
    std::vector<std::vector<uint8_t>> factory_queues;  // per-factory queue of item_ids
    
    // Metrics
    int total_completed = 0;
    int total_expected = 0;
    int cycles_detected = 0;
    int items_completed_out_of_order = 0;
    
    // Wartime surge state
    int surge_tick = -1;        // tick at which surge happens (-1 = no surge)
    int surge_multiplier = 1;   // 1 → 10x at surge_tick
    
    void Init(int n) {
        n_factories = n;
        factory_item.assign(n, 0);
        factory_progress.assign(n, 0);
        factory_target_count.assign(n, 0);
        factory_completed.assign(n, 0);
        factory_queues.assign(n, {});
        stockpile.fill(0);
        total_completed = 0;
        total_expected = 0;
        cycles_detected = 0;
        items_completed_out_of_order = 0;
    }
};

// Stats helper
struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
    int n;
};

inline Stats ComputeStats(std::vector<double> samples) {
    Stats s{};
    s.n = (int)samples.size();
    if (s.n == 0) {
        s.mean = s.median = s.p95 = s.p99 = s.stddev = s.min = s.max = 0.0;
        return s;
    }
    std::sort(samples.begin(), samples.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / s.n;
    s.median = samples[s.n / 2];
    s.p95 = samples[(int)(s.n * 0.95)];
    if (s.p95 < s.median) s.p95 = samples.back(); // edge case for small n
    s.p99 = samples[(int)(s.n * 0.99)];
    if (s.p99 < s.p95) s.p99 = samples.back();
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / s.n);
    s.min = samples.front();
    s.max = samples.back();
    return s;
}

// ItemDef helpers
inline void InitItemDefs(std::array<ItemDef, kMaxItemTypes>& defs) {
    // Item 0 = idle (placeholder)
    for (int i = 0; i < kMaxItemTypes; ++i) {
        defs[i] = ItemDef{
            (i == 0 ? "idle" : "item"),
            10 + i * 2,  // build_ticks: 10, 12, 14, 16, ..., 40
            100 * (i + 1),
            50 * (i + 1),
            {} // zero-init dep_count
        };
    }
    
    // Names
    defs[1].name = "tank";
    defs[2].name = "apc";
    defs[3].name = "artillery";
    defs[4].name = "fighter";
    defs[5].name = "bomber";
    defs[6].name = "helicopter";
    defs[7].name = "destroyer";
    defs[8].name = "cruiser";
    defs[9].name = "submarine";
    defs[10].name = "truck";
    defs[11].name = "railcar";
    defs[12].name = "rifles";
    defs[13].name = "machine_guns";
    defs[14].name = "artillery_shells";
    defs[15].name = "fuel";
    
    // Dependency chains (raw → parts → weapon):
    // tank needs 2 rifles + 1 fuel (deps: 12=2, 15=1)
    defs[1].dep_count[12] = 2; defs[1].dep_count[15] = 1;
    // apc needs 1 rifle + 1 fuel
    defs[2].dep_count[12] = 1; defs[2].dep_count[15] = 1;
    // artillery needs 1 artillery_shells + 1 fuel
    defs[3].dep_count[14] = 1; defs[3].dep_count[15] = 1;
    // fighter needs 1 machine_guns + 2 fuel
    defs[4].dep_count[13] = 1; defs[4].dep_count[15] = 2;
    // bomber needs 2 machine_guns + 4 fuel
    defs[5].dep_count[13] = 2; defs[5].dep_count[15] = 4;
    // helicopter needs 1 machine_guns + 2 fuel
    defs[6].dep_count[13] = 1; defs[6].dep_count[15] = 2;
    // destroyer needs 1 artillery_shells + 2 fuel
    defs[7].dep_count[14] = 1; defs[7].dep_count[15] = 2;
    // cruiser needs 3 artillery_shells + 4 fuel
    defs[8].dep_count[14] = 3; defs[8].dep_count[15] = 4;
    // submarine needs 5 fuel
    defs[9].dep_count[15] = 5;
    // truck needs 1 fuel
    defs[10].dep_count[15] = 1;
    // railcar needs 1 fuel
    defs[11].dep_count[15] = 1;
    // rifles, machine_guns, artillery_shells, fuel = raw (no deps)
}

// Scene constructors

// Scene 1: single_item_uniform — 1000 factories × same item
inline World MakeSingleItemScene(int n, int seed) {
    World w;
    InitItemDefs(w.item_defs);
    w.Init(n);
    std::mt19937 rng(seed);
    
    // All factories build item 1 (tank), with 1 in queue
    for (int f = 0; f < n; ++f) {
        w.factory_item[f] = 1;
        w.factory_queues[f].push_back(1);
        w.factory_queues[f].push_back(1);
        w.factory_queues[f].push_back(1);
        w.total_expected += 3;
    }
    
    // Pre-stockpile deps
    w.stockpile[12] = n * 2;  // rifles
    w.stockpile[15] = n * 1;  // fuel
    
    return w;
}

// Scene 2: mixed_product_uniform — 1000 factories × 10 different items (round-robin)
inline World MakeMixedProductScene(int n, int seed) {
    World w;
    InitItemDefs(w.item_defs);
    w.Init(n);
    std::mt19937 rng(seed);
    
    // 10 different items per factory
    const int item_ids[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    for (int f = 0; f < n; ++f) {
        int item = item_ids[f % 10];
        w.factory_item[f] = item;
        for (int q = 0; q < 5; ++q) {
            w.factory_queues[f].push_back(item);
        }
        w.total_expected += 5;
    }
    
    // Pre-stockpile common deps
    w.stockpile[12] = n * 1;  // rifles
    w.stockpile[13] = n * 1;  // machine_guns
    w.stockpile[14] = n * 1;  // artillery_shells
    w.stockpile[15] = n * 10;  // fuel
    
    return w;
}

// Scene 3: multi_tier_dependencies — raw → parts → weapon (DAG chain)
inline World MakeMultiTierDependenciesScene(int n, int seed) {
    World w;
    InitItemDefs(w.item_defs);
    w.Init(n);
    std::mt19937 rng(seed);
    
    // 30% raw, 30% parts, 40% weapon
    for (int f = 0; f < n; ++f) {
        int bucket = f % 10;
        int item = 0;
        if (bucket < 3) item = 12;       // rifles (raw)
        else if (bucket < 6) item = 13;  // machine_guns (raw)
        else if (bucket < 8) item = 14;  // artillery_shells (raw)
        else if (bucket < 9) item = 15;  // fuel (raw)
        else item = 1;                   // tank (weapon)
        
        w.factory_item[f] = item;
        for (int q = 0; q < 5; ++q) {
            w.factory_queues[f].push_back(item);
        }
        w.total_expected += 5;
    }
    
    // No pre-stockpile (deps must be produced first)
    return w;
}

// Scene 4: wartime_surge — production bursts 1× → 10× at tick 500
inline World MakeWartimeSurgeScene(int n, int seed) {
    World w = MakeSingleItemScene(n, seed);
    w.surge_tick = 500;
    w.surge_multiplier = 10;
    
    // Pre-populate more queue items
    for (int f = 0; f < n; ++f) {
        for (int q = 0; q < 5; ++q) {
            w.factory_queues[f].push_back(1);
            w.total_expected++;
        }
    }
    
    return w;
}

// Scene 5: economic_complex — 100 sectors × 10 factories per sector + cross-sector deps
inline World MakeEconomicComplexScene(int n, int seed) {
    World w;
    InitItemDefs(w.item_defs);
    w.Init(n);
    std::mt19937 rng(seed);
    
    // Each factory specializes in one item (mod 10), all 5-item queue
    const int item_ids[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (int f = 0; f < n; ++f) {
        int item = item_ids[f % 10];
        w.factory_item[f] = item;
        for (int q = 0; q < 5; ++q) {
            w.factory_queues[f].push_back(item);
        }
        w.total_expected += 5;
    }
    
    // Sector-specific stockpile (sector = f / 10, total 100 sectors)
    for (int s = 0; s < 100; ++s) {
        w.stockpile[12] = n / 10 * 2;  // rifles
        w.stockpile[15] = n / 10 * 1;  // fuel
    }
    // Each sector has its own stockpile; for prototype we share globally (approximation)
    
    return w;
}

} // namespace pvf
