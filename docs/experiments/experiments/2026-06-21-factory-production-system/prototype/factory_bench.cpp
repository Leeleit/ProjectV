// factory_bench.cpp — 5 production schedulers × 5 scenes × 5 seeds × 1000 iter
// Per `2026-06-21-factory-production-system` experiment.
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic -o factory_bench factory_bench.cpp
// Run:   ./factory_bench > results.csv
//
// Output CSV columns: strategy,scene,seed,n_factories,n_iter,mean_ns_per_tick,
//                     total_completed,total_expected,throughput_pct,cycles_detected,out_of_order

#include "world_model.hpp"
#include <chrono>
#include <fstream>
#include <queue>
#include <vector>
#include <numeric>
#include <iomanip>
#include <iostream>
#include <cstdio>

using namespace pvf;

// === 5 SCHEDULERS ===

// A_NaiveLinearScan: scan all factories, advance first item in queue
struct RunResult {
    double mean_ns_per_tick;
    int total_completed;
    int total_expected;
    int cycles_detected;
    int out_of_order;
};

static RunResult RunNaiveLinearScan(World& w, int num_ticks) {
    int n = w.n_factories;
    int completed_start = w.total_completed;
    int out_of_order_start = w.items_completed_out_of_order;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int tick = 0; tick < num_ticks; ++tick) {
        for (int f = 0; f < n; ++f) {
            uint8_t item = w.factory_item[f];
            if (item == 0) continue;
            w.factory_progress[f]++;
            if (w.factory_progress[f] >= w.item_defs[item].build_ticks) {
                w.factory_progress[f] = 0;
                w.factory_completed[f]++;
                w.stockpile[item]++;
                w.total_completed++;
                
                if (!w.factory_queues[f].empty()) {
                    uint8_t next = w.factory_queues[f].front();
                    w.factory_queues[f].erase(w.factory_queues[f].begin());
                    w.factory_item[f] = next;
                } else {
                    w.factory_item[f] = 0;
                }
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return { ns / num_ticks, w.total_completed - completed_start, w.total_expected,
             0, w.items_completed_out_of_order - out_of_order_start };
}

// B_PriorityBucketQueue: priority queue of factories, process top N per tick
static RunResult RunPriorityBucketQueue(World& w, int num_ticks) {
    int n = w.n_factories;
    int completed_start = w.total_completed;
    int out_of_order_start = w.items_completed_out_of_order;
    
    // For priority: lower build_ticks = higher priority (cheaper items first)
    std::vector<std::pair<int, int>> pq; // (priority, factory_id)
    pq.reserve(n);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int tick = 0; tick < num_ticks; ++tick) {
        pq.clear();
        for (int f = 0; f < n; ++f) {
            if (w.factory_item[f] != 0) {
                int prio = w.item_defs[w.factory_item[f]].build_ticks;
                pq.push_back({-prio, f});
            }
        }
        std::make_heap(pq.begin(), pq.end());
        
        // Process all (or top half if too many)
        int to_process = (int)pq.size();
        if (to_process > n / 2 && n > 100) to_process = n / 2;
        
        while (to_process > 0 && !pq.empty()) {
            std::pop_heap(pq.begin(), pq.end());
            int f = pq.back().second;
            pq.pop_back();
            uint8_t item = w.factory_item[f];
            if (item == 0) continue;
            
            w.factory_progress[f]++;
            if (w.factory_progress[f] >= w.item_defs[item].build_ticks) {
                w.factory_progress[f] = 0;
                w.factory_completed[f]++;
                w.stockpile[item]++;
                w.total_completed++;
                
                if (!w.factory_queues[f].empty()) {
                    uint8_t next = w.factory_queues[f].front();
                    w.factory_queues[f].erase(w.factory_queues[f].begin());
                    w.factory_item[f] = next;
                } else {
                    w.factory_item[f] = 0;
                }
            }
            to_process--;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return { ns / num_ticks, w.total_completed - completed_start, w.total_expected,
             0, w.items_completed_out_of_order - out_of_order_start };
}

// C_DependencyDAG_TopoSort: build DAG, complete in topo order, detect cycles
static RunResult RunDependencyDAG(World& w, int num_ticks) {
    int n = w.n_factories;
    int completed_start = w.total_completed;
    int out_of_order_start = w.items_completed_out_of_order;
    int cycles = 0;
    
    // Precompute indegree per item (number of distinct dep item types)
    std::array<int, kMaxItemTypes> indegree = {};
    for (int i = 0; i < kMaxItemTypes; ++i) {
        for (int j = 0; j < kMaxItemTypes; ++j) {
            if (w.item_defs[i].dep_count[j] > 0) indegree[i]++;
        }
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int tick = 0; tick < num_ticks; ++tick) {
        // Kahn's algorithm: process items in order of indegree (0 = no deps first)
        // For tick-level, we order factories by their current item's topological priority
        std::vector<int> factory_order(n);
        std::iota(factory_order.begin(), factory_order.end(), 0);
        std::sort(factory_order.begin(), factory_order.end(), [&](int a, int b) {
            uint8_t item_a = w.factory_item[a];
            uint8_t item_b = w.factory_item[b];
            if (item_a == 0) return false;
            if (item_b == 0) return true;
            return indegree[item_a] < indegree[item_b];
        });
        
        for (int f : factory_order) {
            uint8_t item = w.factory_item[f];
            if (item == 0) continue;
            
            // Check deps satisfied (stockpile check)
            bool can_complete = true;
            for (int j = 0; j < kMaxItemTypes; ++j) {
                if (w.item_defs[item].dep_count[j] > 0 &&
                    w.stockpile[j] < w.item_defs[item].dep_count[j]) {
                    can_complete = false;
                    break;
                }
            }
            if (!can_complete) {
                w.items_completed_out_of_order++;
                continue;
            }
            
            w.factory_progress[f]++;
            if (w.factory_progress[f] >= w.item_defs[item].build_ticks) {
                w.factory_progress[f] = 0;
                w.factory_completed[f]++;
                
                // Consume deps
                for (int j = 0; j < kMaxItemTypes; ++j) {
                    if (w.item_defs[item].dep_count[j] > 0) {
                        w.stockpile[j] -= w.item_defs[item].dep_count[j];
                    }
                }
                w.stockpile[item]++;
                w.total_completed++;
                
                if (!w.factory_queues[f].empty()) {
                    uint8_t next = w.factory_queues[f].front();
                    w.factory_queues[f].erase(w.factory_queues[f].begin());
                    w.factory_item[f] = next;
                } else {
                    w.factory_item[f] = 0;
                }
            }
        }
        
        // Cycle detection: if no factory made progress in 100 ticks, cycle exists
        if (tick > 0 && tick % 100 == 0) {
            int progress = 0;
            for (int f = 0; f < n; ++f) {
                if (w.factory_item[f] != 0) progress++;
            }
            if (progress > 0 && w.total_completed == completed_start) {
                cycles++;
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return { ns / num_ticks, w.total_completed - completed_start, w.total_expected,
             cycles, w.items_completed_out_of_order - out_of_order_start };
}

// D_CriticalPathBatch: CPM-style, batch all factories on critical path
static RunResult RunCriticalPathBatch(World& w, int num_ticks) {
    int n = w.n_factories;
    int completed_start = w.total_completed;
    int out_of_order_start = w.items_completed_out_of_order;
    
    // Precompute critical path length per item (longest path through deps)
    std::array<int, kMaxItemTypes> cp = {};
    for (int i = 0; i < kMaxItemTypes; ++i) cp[i] = w.item_defs[i].build_ticks;
    for (int iter = 0; iter < kMaxItemTypes; ++iter) {
        for (int i = 0; i < kMaxItemTypes; ++i) {
            for (int j = 0; j < kMaxItemTypes; ++j) {
                if (w.item_defs[i].dep_count[j] > 0) {
                    cp[i] = std::max(cp[i], cp[j] + w.item_defs[i].build_ticks);
                }
            }
        }
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int tick = 0; tick < num_ticks; ++tick) {
        // Sort factories by CP (longest = critical path first)
        std::vector<int> factory_order(n);
        std::iota(factory_order.begin(), factory_order.end(), 0);
        std::sort(factory_order.begin(), factory_order.end(), [&](int a, int b) {
            uint8_t item_a = w.factory_item[a];
            uint8_t item_b = w.factory_item[b];
            if (item_a == 0) return false;
            if (item_b == 0) return true;
            return cp[item_a] > cp[item_b];
        });
        
        // Process ALL active factories per tick (batch parallel)
        for (int f : factory_order) {
            uint8_t item = w.factory_item[f];
            if (item == 0) continue;
            
            w.factory_progress[f]++;
            if (w.factory_progress[f] >= w.item_defs[item].build_ticks) {
                w.factory_progress[f] = 0;
                w.factory_completed[f]++;
                w.stockpile[item]++;
                w.total_completed++;
                
                if (!w.factory_queues[f].empty()) {
                    uint8_t next = w.factory_queues[f].front();
                    w.factory_queues[f].erase(w.factory_queues[f].begin());
                    w.factory_item[f] = next;
                } else {
                    w.factory_item[f] = 0;
                }
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return { ns / num_ticks, w.total_completed - completed_start, w.total_expected,
             0, w.items_completed_out_of_order - out_of_order_start };
}

// E_ProductionLinePipeline: assembly line, 3-stage batch
static RunResult RunProductionLinePipeline(World& w, int num_ticks) {
    int n = w.n_factories;
    int completed_start = w.total_completed;
    int out_of_order_start = w.items_completed_out_of_order;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int tick = 0; tick < num_ticks; ++tick) {
        // Wartime surge handling
        if (w.surge_tick >= 0 && tick == w.surge_tick) {
            for (int f = 0; f < n; ++f) {
                int add = w.surge_multiplier - 1;
                for (int q = 0; q < add; ++q) {
                    w.factory_queues[f].push_back(1);
                    w.total_expected++;
                }
            }
        }
        
        // Pipeline: 3 stages per tick (advance by 3)
        for (int f = 0; f < n; ++f) {
            uint8_t item = w.factory_item[f];
            if (item == 0) continue;
            int build_t = w.item_defs[item].build_ticks;
            w.factory_progress[f] += 3;
            if (w.factory_progress[f] >= build_t) {
                w.factory_progress[f] = 0;
                w.factory_completed[f]++;
                w.stockpile[item]++;
                w.total_completed++;
                
                if (!w.factory_queues[f].empty()) {
                    uint8_t next = w.factory_queues[f].front();
                    w.factory_queues[f].erase(w.factory_queues[f].begin());
                    w.factory_item[f] = next;
                } else {
                    w.factory_item[f] = 0;
                }
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return { ns / num_ticks, w.total_completed - completed_start, w.total_expected,
             0, w.items_completed_out_of_order - out_of_order_start };
}

// === MAIN HARNESS ===

using SchedulerFn = RunResult (*)(World&, int);

struct Strategy {
    const char* name;
    SchedulerFn fn;
};

int main() {
    const int n_iter = 1000;
    const int warmup = 10;
    const int n_factories = 1000;
    const int seeds[] = {1, 7, 42, 1234, 31337};
    const int n_seeds = 5;
    
    Strategy strategies[] = {
        {"A_NaiveLinearScan",        &RunNaiveLinearScan},
        {"B_PriorityBucketQueue",    &RunPriorityBucketQueue},
        {"C_DependencyDAG_TopoSort", &RunDependencyDAG},
        {"D_CriticalPathBatch",      &RunCriticalPathBatch},
        {"E_ProductionLinePipeline", &RunProductionLinePipeline},
    };
    const int n_strategies = 5;
    
    // Scene constructors
    struct Scene {
        const char* name;
        World (*make)(int, int);
    };
    Scene scenes[] = {
        {"single_item_uniform",        &MakeSingleItemScene},
        {"mixed_product_uniform",      &MakeMixedProductScene},
        {"multi_tier_dependencies",    &MakeMultiTierDependenciesScene},
        {"wartime_surge",              &MakeWartimeSurgeScene},
        {"economic_complex",           &MakeEconomicComplexScene},
    };
    const int n_scenes = 5;
    
    // CSV header
    std::cout << "strategy,scene,seed,n_factories,n_iter,mean_ns_per_tick,"
              << "total_completed,total_expected,throughput_pct,cycles_detected,out_of_order\n";
    std::cout << std::fixed;
    
    for (int si = 0; si < n_strategies; ++si) {
        for (int sc = 0; sc < n_scenes; ++sc) {
            for (int sd = 0; sd < n_seeds; ++sd) {
                int seed = seeds[sd];
                
                // Build scene
                World w = scenes[sc].make(n_factories, seed);
                
                // Warmup
                for (int w_tick = 0; w_tick < warmup; ++w_tick) {
                    World w_warmup = scenes[sc].make(n_factories, seed);
                    strategies[si].fn(w_warmup, 1);
                }
                
                // Main run
                RunResult r = strategies[si].fn(w, n_iter);
                
                double throughput_pct = w.total_expected > 0
                    ? 100.0 * r.total_completed / w.total_expected : 0.0;
                
                std::cout << strategies[si].name << ","
                          << scenes[sc].name << ","
                          << seed << ","
                          << n_factories << ","
                          << n_iter << ","
                          << std::setprecision(2) << r.mean_ns_per_tick << ","
                          << r.total_completed << ","
                          << r.total_expected << ","
                          << std::setprecision(2) << throughput_pct << ","
                          << r.cycles_detected << ","
                          << r.out_of_order << "\n";
            }
        }
    }
    
    return 0;
}
