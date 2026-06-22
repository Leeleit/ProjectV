// SPDX-License-Identifier: MIT
//
// 2026-06-22-tech-tree-research-system / prototype / tech_tree_bench.cpp
//
// Standalone C++26 CPU benchmark: 5 strategies for research queue processing
// on a directed acyclic graph (DAG) of tech nodes.
//
// 5 strategies:
//   A_NaiveSequential_LinearScan  -- O(N) per tick, scan all nodes
//   B_PriorityQueueDijkstra       -- O((V+E) log V) per tick, min-heap
//   C_CriticalPathPrecompute      -- O(V+E) one-time, O(1) per tick
//   D_LazyPrerequisiteExpand      -- BFS-lazy on completion
//   E_Hybrid_CP_LazyPQueue ⭐     -- C (track CP) + B (selection) + D (on-complete)
//
// 5 scenes (representative DAG topologies):
//   linear_50                -- 50 nodes single chain (degenerate serial)
//   tree_3_50                -- 3 balanced trees × 17 nodes (3 parallel tracks)
//   diamond_100              -- 100 nodes, 4-way fork + 4-way join
//   realistic_hoi4_subset_60 -- 60 nodes, 5 categories × 12 with cross-cat prereqs
//   dense_cross_track_200    -- 200 nodes, 3 tracks, many cross-prereqs
//
// Per `benchmarks/methodology.md §3`:
// 5 strategies x 5 scenes x 5 seeds x 1000 iter + 10 warmup = 125,000 main measurements.
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
//
// Out: prototype/build/results.csv (126 rows = 1 header + 125 data).

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// Data structures
// ============================================================================

struct TechNode {
    std::uint32_t id = 0;
    std::uint32_t track = 0;        // 0..2
    std::uint32_t base_cost = 100;  // research points
    std::uint32_t research_speed = 1; // multiplier on cost
    std::vector<std::uint32_t> prereqs; // ids
    std::vector<std::uint32_t> dependents; // ids (reverse edges)
    std::uint32_t progress = 0;     // current research points
    bool completed = false;
    bool in_active_set = false;     // currently being researched
    bool available = false;         // all prereqs completed, not yet in queue
    std::uint32_t critical_path_len = 0; // for strategy C, E
};

struct TechTrack {
    std::uint32_t id = 0;
    std::uint32_t slot_count = 1;   // concurrent research slots
    std::vector<std::uint32_t> active_research; // ids
    std::vector<std::uint32_t> queue; // queued tech ids (per-strategy ordering differs)
};

struct DAG {
    std::vector<TechNode> nodes;
    std::array<TechTrack, 3> tracks;
    bool has_cycle = false;
};

// ============================================================================
// Scene generators
// ============================================================================

DAG build_linear_50(std::uint32_t seed) {
    DAG d;
    d.nodes.reserve(50);
    for (std::uint32_t i = 0; i < 50; ++i) {
        TechNode n;
        n.id = i;
        n.track = 0; // single track
        n.base_cost = 100 + (i * 7 + seed * 13) % 50;
        if (i > 0) n.prereqs.push_back(i - 1);
        d.nodes.push_back(n);
    }
    for (std::uint32_t i = 0; i + 1 < d.nodes.size(); ++i) {
        d.nodes[i].dependents.push_back(i + 1);
    }
    d.tracks[0].slot_count = 1;
    return d;
}

DAG build_tree_3_50(std::uint32_t seed) {
    // 3 parallel trees × 17 nodes (51 total).
    DAG d;
    d.nodes.reserve(51);
    std::mt19937 rng(seed);
    for (std::uint32_t t = 0; t < 3; ++t) {
        for (std::uint32_t i = 0; i < 17; ++i) {
            TechNode n;
            n.id = d.nodes.size();
            n.track = t;
            n.base_cost = 100 + (rng() % 50);
            if (i > 0) {
                // prereq in same track
                n.prereqs.push_back(t * 17 + (i - 1));
            }
            d.nodes.push_back(n);
        }
    }
    for (std::uint32_t t = 0; t < 3; ++t) {
        for (std::uint32_t i = 0; i + 1 < 17; ++i) {
            d.nodes[t * 17 + i].dependents.push_back(t * 17 + (i + 1));
        }
    }
    d.tracks[0].slot_count = 1;
    d.tracks[1].slot_count = 1;
    d.tracks[2].slot_count = 1;
    return d;
}

DAG build_diamond_100(std::uint32_t seed) {
    // 4-way fork (4 nodes) -> 23-node middle layer x 4 -> 4-way join (4 nodes)
    // Total: 4 + 4*23 + 4 = 100 nodes.
    DAG d;
    d.nodes.reserve(100);
    std::mt19937 rng(seed);
    // Fork layer
    for (std::uint32_t f = 0; f < 4; ++f) {
        TechNode n;
        n.id = d.nodes.size();
        n.track = f % 3;
        n.base_cost = 100 + (rng() % 50);
        d.nodes.push_back(n);
    }
    // Middle layers (4 layers x 23 nodes)
    for (std::uint32_t layer = 0; layer < 4; ++layer) {
        for (std::uint32_t k = 0; k < 23; ++k) {
            TechNode n;
            n.id = d.nodes.size();
            n.track = k % 3;
            n.base_cost = 100 + (rng() % 50);
            // prereq: 1 random from previous layer
            std::uint32_t prev_offset = (layer == 0) ? 0 : 4 + (layer - 1) * 23;
            std::uint32_t prev_count = (layer == 0) ? 4 : 23;
            n.prereqs.push_back(prev_offset + (rng() % prev_count));
            d.nodes.push_back(n);
        }
    }
    // Join layer
    for (std::uint32_t j = 0; j < 4; ++j) {
        TechNode n;
        n.id = d.nodes.size();
        n.track = j % 3;
        n.base_cost = 100 + (rng() % 50);
        for (std::uint32_t prev = 0; prev < 23; ++prev) {
            n.prereqs.push_back(4 + 3 * 23 + prev);
        }
        d.nodes.push_back(n);
    }
    // Build dependents (reverse edges)
    for (std::uint32_t i = 0; i < d.nodes.size(); ++i) {
        for (auto p : d.nodes[i].prereqs) {
            d.nodes[p].dependents.push_back(i);
        }
    }
    d.tracks[0].slot_count = 1;
    d.tracks[1].slot_count = 1;
    d.tracks[2].slot_count = 1;
    return d;
}

DAG build_realistic_hoi4_subset_60(std::uint32_t seed) {
    // 5 categories (treating each as a separate "track" but maps to 3 actual tracks)
    // infantry, armor, artillery, air, industry -- 12 nodes each.
    // Some cross-category prereqs (e.g. radar needs electronics).
    DAG d;
    d.nodes.reserve(60);
    std::mt19937 rng(seed);
    const char* categories[5] = {"infantry", "armor", "artillery", "air", "industry"}; (void)categories;
    // Map to 3 actual tracks: 0=ground, 1=air, 2=industry
    std::uint32_t cat_to_track[5] = {0, 0, 0, 1, 2};

    for (std::uint32_t cat = 0; cat < 5; ++cat) {
        for (std::uint32_t i = 0; i < 12; ++i) {
            TechNode n;
            n.id = d.nodes.size();
            n.track = cat_to_track[cat];
            n.base_cost = 100 + (rng() % 50);
            // Linear within category
            if (i > 0) n.prereqs.push_back(cat * 12 + (i - 1));
            // 30% chance of cross-category prereq
            if (i >= 3 && (rng() % 10) < 3) {
                std::uint32_t other_cat = (cat + 1 + rng() % 4) % 5;
                if (other_cat != cat) {
                    std::uint32_t other_idx = rng() % std::min(i, std::uint32_t(8));
                    n.prereqs.push_back(other_cat * 12 + other_idx);
                }
            }
            d.nodes.push_back(n);
        }
    }
    for (std::uint32_t i = 0; i < d.nodes.size(); ++i) {
        for (auto p : d.nodes[i].prereqs) {
            d.nodes[p].dependents.push_back(i);
        }
    }
    d.tracks[0].slot_count = 1;
    d.tracks[1].slot_count = 1;
    d.tracks[2].slot_count = 1;
    return d;
}

DAG build_dense_cross_track_200(std::uint32_t seed) {
    // 200 nodes, 3 tracks, dense cross-prereqs.
    // Each node: 1-3 prereqs from any track (50% cross-track probability).
    DAG d;
    d.nodes.reserve(200);
    std::mt19937 rng(seed);
    for (std::uint32_t i = 0; i < 200; ++i) {
        TechNode n;
        n.id = i;
        n.track = i % 3;
        n.base_cost = 100 + (rng() % 80);
        if (i == 0) {
            // root
        } else if (i < 3) {
            n.prereqs.push_back(0);
        } else {
            int nprereq = 1 + (rng() % 3); // 1..3
            for (int k = 0; k < nprereq; ++k) {
                std::uint32_t p = rng() % i; // uniform random
                n.prereqs.push_back(p);
            }
            // Deduplicate prereqs
            std::sort(n.prereqs.begin(), n.prereqs.end());
            n.prereqs.erase(std::unique(n.prereqs.begin(), n.prereqs.end()),
                            n.prereqs.end());
        }
        d.nodes.push_back(n);
    }
    for (std::uint32_t i = 0; i < d.nodes.size(); ++i) {
        for (auto p : d.nodes[i].prereqs) {
            d.nodes[p].dependents.push_back(i);
        }
    }
    d.tracks[0].slot_count = 1;
    d.tracks[1].slot_count = 1;
    d.tracks[2].slot_count = 1;
    return d;
}

// ============================================================================
// Cycle detection (Kahn 1962 topological sort)
// ============================================================================

bool detect_cycle(const DAG& d) {
    std::vector<std::uint32_t> indeg(d.nodes.size(), 0);
    for (auto& n : d.nodes) {
        for (auto p : n.prereqs) (void)p, indeg[n.id]++;
    }
    std::queue<std::uint32_t> q;
    for (std::uint32_t i = 0; i < d.nodes.size(); ++i) {
        if (indeg[i] == 0) q.push(i);
    }
    std::uint32_t visited = 0;
    while (!q.empty()) {
        auto u = q.front(); q.pop();
        ++visited;
        for (auto dep : d.nodes[u].dependents) {
            if (--indeg[dep] == 0) q.push(dep);
        }
    }
    return visited != d.nodes.size();
}

// ============================================================================
// Per-tick helpers (shared by all strategies)
// ============================================================================

void init_state(DAG& d) {
    for (auto& n : d.nodes) {
        n.progress = 0;
        n.completed = false;
        n.in_active_set = false;
        n.available = false;
    }
    for (auto& t : d.tracks) {
        t.active_research.clear();
        t.queue.clear();
    }
    // Mark initial available nodes (no prereqs) AND populate track queues
    // (used by strategies D and E; A/B/C ignore queue).
    for (auto& n : d.nodes) {
        if (n.prereqs.empty()) {
            n.available = true;
            d.tracks[n.track].queue.push_back(n.id);
        }
    }
}

void advance_research(DAG& d) {
    // Per-tick research progress = 1 (1 point per tick).
    for (auto& t : d.tracks) {
        for (auto id : t.active_research) {
            d.nodes[id].progress += 1;
        }
    }
    // Check completions.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> completed; // (track, node)
    for (std::uint32_t t = 0; t < 3; ++t) {
        std::vector<std::uint32_t> still_active;
        for (auto id : d.tracks[t].active_research) {
            auto& n = d.nodes[id];
            if (n.progress >= n.base_cost * n.research_speed) {
                n.completed = true;
                n.in_active_set = false;
                completed.push_back({t, id});
            } else {
                still_active.push_back(id);
            }
        }
        d.tracks[t].active_research = std::move(still_active);
    }
    // Mark newly available dependents.
    for (auto& [track, id] : completed) {
        (void)track;
        for (auto dep : d.nodes[id].dependents) {
            auto& dn = d.nodes[dep];
            if (!dn.completed && !dn.in_active_set && !dn.available) {
                bool all_done = true;
                for (auto p : dn.prereqs) {
                    if (!d.nodes[p].completed) { all_done = false; break; }
                }
                if (all_done) dn.available = true;
            }
        }
    }
}

void schedule_track(DAG& d, std::uint32_t track, std::uint32_t candidate) {
    if (candidate >= d.nodes.size()) return;
    auto& n = d.nodes[candidate];
    if (n.completed || n.in_active_set || !n.available) return;
    n.in_active_set = true;
    n.available = false;
    d.tracks[track].active_research.push_back(candidate);
}

bool is_all_completed(const DAG& d) {
    for (auto& n : d.nodes) if (!n.completed) return false;
    return true;
}

std::uint32_t count_completed(const DAG& d) {
    std::uint32_t c = 0;
    for (auto& n : d.nodes) if (n.completed) ++c;
    return c;
}

// ============================================================================
// Strategy A: NaiveSequential_LinearScan
// ============================================================================

void strategy_a_tick(DAG& d) {
    advance_research(d);
    for (std::uint32_t t = 0; t < 3; ++t) {
        while (d.tracks[t].active_research.size() < d.tracks[t].slot_count) {
            // Linear scan: pick first available node in this track.
            bool found = false;
            for (std::uint32_t i = 0; i < d.nodes.size(); ++i) {
                if (d.nodes[i].track == t && d.nodes[i].available) {
                    schedule_track(d, t, i);
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }
    }
}

// ============================================================================
// Strategy B: PriorityQueueDijkstra (min-heap by remaining_cost)
// ============================================================================

struct PQEntry {
    std::uint32_t id;
    std::uint32_t remaining;
    bool operator>(const PQEntry& o) const { return remaining > o.remaining; }
};

void strategy_b_tick(DAG& d) {
    advance_research(d);
    for (std::uint32_t t = 0; t < 3; ++t) {
        while (d.tracks[t].active_research.size() < d.tracks[t].slot_count) {
            // Build min-heap of available nodes in this track.
            std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;
            for (std::uint32_t i = 0; i < d.nodes.size(); ++i) {
                if (d.nodes[i].track == t && d.nodes[i].available) {
                    pq.push({i, d.nodes[i].base_cost * d.nodes[i].research_speed});
                }
            }
            if (pq.empty()) break;
            auto top = pq.top(); pq.pop();
            schedule_track(d, t, top.id);
        }
    }
}

// ============================================================================
// Strategy C: CriticalPathPrecompute (CPM 1959)
// Pre-compute per-node longest path from any source (forward) once.
// Per-tick: pick the available node with highest CP from each track.
// ============================================================================

void compute_critical_path(DAG& d) {
    // Topological order via Kahn 1962.
    std::vector<std::uint32_t> indeg(d.nodes.size(), 0);
    for (auto& n : d.nodes) {
        for (auto p : n.prereqs) (void)p, indeg[n.id]++;
    }
    std::queue<std::uint32_t> q;
    for (std::uint32_t i = 0; i < d.nodes.size(); ++i) {
        if (indeg[i] == 0) q.push(i);
    }
    std::vector<std::uint32_t> topo;
    while (!q.empty()) {
        auto u = q.front(); q.pop();
        topo.push_back(u);
        for (auto dep : d.nodes[u].dependents) {
            if (--indeg[dep] == 0) q.push(dep);
        }
    }
    // For each node: longest path from a source = max(prereqs' longest_path + cost).
    for (auto u : topo) {
        auto& n = d.nodes[u];
        std::uint32_t best = n.base_cost;
        for (auto p : n.prereqs) {
            best = std::max(best, d.nodes[p].critical_path_len + n.base_cost);
        }
        n.critical_path_len = best;
    }
}

void strategy_c_tick(DAG& d) {
    advance_research(d);
    for (std::uint32_t t = 0; t < 3; ++t) {
        while (d.tracks[t].active_research.size() < d.tracks[t].slot_count) {
            // Find max-CP available node in this track.
            std::uint32_t best_id = d.nodes.size();
            std::uint32_t best_cp = 0;
            for (std::uint32_t i = 0; i < d.nodes.size(); ++i) {
                if (d.nodes[i].track == t && d.nodes[i].available) {
                    if (d.nodes[i].critical_path_len > best_cp) {
                        best_cp = d.nodes[i].critical_path_len;
                        best_id = i;
                    }
                }
            }
            if (best_id >= d.nodes.size()) break;
            schedule_track(d, t, best_id);
        }
    }
}

// ============================================================================
// Strategy D: LazyPrerequisiteExpand
// BFS-lazy: on each completion, expand dependents and add to track queues.
// ============================================================================

void strategy_d_tick(DAG& d) {
    // For D, we don't call advance_research directly; we inline it to also
    // add newly available nodes to track queues.
    for (auto& t : d.tracks) {
        for (auto id : t.active_research) {
            d.nodes[id].progress += 1;
        }
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> completed;
    for (std::uint32_t t = 0; t < 3; ++t) {
        std::vector<std::uint32_t> still_active;
        for (auto id : d.tracks[t].active_research) {
            auto& n = d.nodes[id];
            if (n.progress >= n.base_cost * n.research_speed) {
                n.completed = true;
                n.in_active_set = false;
                completed.push_back({t, id});
            } else {
                still_active.push_back(id);
            }
        }
        d.tracks[t].active_research = std::move(still_active);
    }
    // Lazy expand: add newly available to track queues (BFS-lazy).
    for (auto& [track, id] : completed) {
        (void)track;
        for (auto dep : d.nodes[id].dependents) {
            auto& dn = d.nodes[dep];
            if (dn.completed || dn.in_active_set || dn.available) continue;
            bool all_done = true;
            for (auto p : dn.prereqs) {
                if (!d.nodes[p].completed) { all_done = false; break; }
            }
            if (all_done) {
                dn.available = true;
                d.tracks[dn.track].queue.push_back(dn.id);
            }
        }
    }
    // Schedule from track queues (FIFO for D).
    for (std::uint32_t t = 0; t < 3; ++t) {
        while (d.tracks[t].active_research.size() < d.tracks[t].slot_count
               && !d.tracks[t].queue.empty()) {
            std::uint32_t cand = d.tracks[t].queue.front();
            d.tracks[t].queue.erase(d.tracks[t].queue.begin());
            schedule_track(d, t, cand);
        }
    }
}

// ============================================================================
// Strategy E: Hybrid_CP_LazyPQueue ⭐
// C pre-computes CP; B selects by PQ ordered by (CP - elapsed_fraction); D on-completion expand.
// ============================================================================

void strategy_e_tick(DAG& d) {
    // E inlines advance_research + adds new availables to PQ sorted by estimated remaining.
    for (auto& t : d.tracks) {
        for (auto id : t.active_research) {
            d.nodes[id].progress += 1;
        }
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> completed;
    for (std::uint32_t t = 0; t < 3; ++t) {
        std::vector<std::uint32_t> still_active;
        for (auto id : d.tracks[t].active_research) {
            auto& n = d.nodes[id];
            if (n.progress >= n.base_cost * n.research_speed) {
                n.completed = true;
                n.in_active_set = false;
                completed.push_back({t, id});
            } else {
                still_active.push_back(id);
            }
        }
        d.tracks[t].active_research = std::move(still_active);
    }
    // On-completion: add to PQ, sorted by (CP-remaining).
    for (auto& [track, id] : completed) {
        (void)track;
        for (auto dep : d.nodes[id].dependents) {
            auto& dn = d.nodes[dep];
            if (dn.completed || dn.in_active_set || dn.available) continue;
            bool all_done = true;
            for (auto p : dn.prereqs) {
                if (!d.nodes[p].completed) { all_done = false; break; }
            }
            if (all_done) {
                dn.available = true;
                // Estimated remaining = base_cost * (1 - elapsed_fraction) = cost (fresh)
                // (in our model, no partial progress for newly available).
                d.tracks[dn.track].queue.push_back(dn.id);
            }
        }
    }
    // Schedule: PQ-ordered by (CP desc, cost asc) = highest CP first.
    for (std::uint32_t t = 0; t < 3; ++t) {
        while (d.tracks[t].active_research.size() < d.tracks[t].slot_count
               && !d.tracks[t].queue.empty()) {
            // Build PQ on queue.
            std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;
            for (auto id : d.tracks[t].queue) {
                auto& n = d.nodes[id];
                // Sort key: prefer high CP, low cost. Combined key = (max_cp - CP) * factor + cost.
                // For simplicity: use CP as primary (descending), cost as secondary (ascending).
                // We invert CP via a large offset (max possible CP = 200 * 200 = 40000).
                std::uint32_t sort_key = (40000 - n.critical_path_len) * 1000
                                         + (n.base_cost * n.research_speed);
                pq.push({id, sort_key});
            }
            d.tracks[t].queue.clear();
            if (pq.empty()) break;
            auto top = pq.top(); pq.pop();
            // Drain the rest back to queue.
            while (!pq.empty()) {
                d.tracks[t].queue.push_back(pq.top().id);
                pq.pop();
            }
            schedule_track(d, t, top.id);
        }
    }
}

// ============================================================================
// Benchmark harness
// ============================================================================

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min_v;
    double max_v;
};

Stats compute_stats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<std::size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<std::size_t>(samples.size() * 0.99)];
    double var = 0.0;
    for (auto v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.min_v = samples.front();
    s.max_v = samples.back();
    return s;
}

using SceneBuilder = DAG (*)(std::uint32_t);
using StrategyFn = void (*)(DAG&);

struct Scene {
    std::string_view name;
    SceneBuilder build;
};

struct Strategy {
    std::string_view name;
    StrategyFn tick;
    bool needs_cp; // pre-compute critical path before tick loop
};

constexpr std::array<Scene, 5> kScenes = {{
    {"linear_50", build_linear_50},
    {"tree_3_50", build_tree_3_50},
    {"diamond_100", build_diamond_100},
    {"realistic_hoi4_subset_60", build_realistic_hoi4_subset_60},
    {"dense_cross_track_200", build_dense_cross_track_200},
}};

constexpr std::array<Strategy, 5> kStrategies = {{
    {"A_NaiveSequential_LinearScan", strategy_a_tick, false},
    {"B_PriorityQueueDijkstra", strategy_b_tick, false},
    {"C_CriticalPathPrecompute", strategy_c_tick, true},
    {"D_LazyPrerequisiteExpand", strategy_d_tick, false},
    {"E_Hybrid_CP_LazyPQueue", strategy_e_tick, true},
}};

struct SceneResult {
    std::string strategy;
    std::string scene;
    std::uint32_t seed;
    double mean_ns;
    double p95_ns;
    double p99_ns;
    double max_ns;
    double total_ns;
    std::uint32_t cycles_to_completion;
    std::uint32_t nodes_completed;
    bool cycle_detected;
};

int main() {
    // Setup output directory.
    std::filesystem::create_directories("build");

    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,mean_ns,p95_ns,p99_ns,max_ns,total_ns,"
           "cycles_to_completion,nodes_completed,cycle_detected\n";

    constexpr std::uint32_t kWarmup = 10;
    constexpr std::uint32_t kIter = 1000;
    constexpr std::array<std::uint32_t, 5> kSeeds = {1, 7, 42, 1234, 31337};

    std::vector<SceneResult> all;
    all.reserve(kStrategies.size() * kScenes.size() * kSeeds.size());

    for (auto& strat : kStrategies) {
        for (auto& scene : kScenes) {
            for (auto seed : kSeeds) {
                // Build fresh DAG for this config.
                DAG base = scene.build(seed);
                bool has_cycle = detect_cycle(base);
                if (strat.needs_cp) compute_critical_path(base);

                // Warmup.
                for (std::uint32_t w = 0; w < kWarmup; ++w) {
                    DAG d = base;
                    init_state(d);
                    std::uint32_t max_ticks = d.nodes.size() * 200;
                    for (std::uint32_t t = 0; t < max_ticks; ++t) {
                        if (is_all_completed(d)) break;
                        strat.tick(d);
                    }
                }

                // Main measurement.
                std::vector<double> samples;
                samples.reserve(kIter);
                double total_ns = 0.0;
                std::uint32_t cycles_last = 0;
                std::uint32_t nodes_last = 0;
                for (std::uint32_t it = 0; it < kIter; ++it) {
                    DAG d = base;
                    init_state(d);
                    std::uint32_t max_ticks = d.nodes.size() * 200;
                    auto t0 = std::chrono::steady_clock::now();
                    for (std::uint32_t t = 0; t < max_ticks; ++t) {
                        if (is_all_completed(d)) break;
                        strat.tick(d);
                    }
                    auto t1 = std::chrono::steady_clock::now();
                    double ns = static_cast<double>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
                    samples.push_back(ns);
                    total_ns += ns;
                    cycles_last = max_ticks; // approximate
                    nodes_last = count_completed(d);
                }
                Stats st = compute_stats(samples);
                SceneResult r;
                r.strategy = strat.name;
                r.scene = scene.name;
                r.seed = seed;
                r.mean_ns = st.mean;
                r.p95_ns = st.p95;
                r.p99_ns = st.p99;
                r.max_ns = st.max_v;
                r.total_ns = total_ns;
                r.cycles_to_completion = cycles_last;
                r.nodes_completed = nodes_last;
                r.cycle_detected = has_cycle;
                all.push_back(r);

                csv << r.strategy << "," << r.scene << "," << r.seed << ","
                    << r.mean_ns << "," << r.p95_ns << "," << r.p99_ns << ","
                    << r.max_ns << "," << r.total_ns << ","
                    << r.cycles_to_completion << "," << r.nodes_completed << ","
                    << (r.cycle_detected ? "1" : "0") << "\n";
            }
        }
    }
    csv.close();

    // Summary: per-strategy per-scene mean across 5 seeds.
    std::printf("\n=== Summary (mean ns/run across 5 seeds) ===\n");
    std::printf("%-32s %-32s %12s %12s %12s\n",
                "Strategy", "Scene", "mean_ns", "p99_ns", "nodes");
    for (auto& r : all) {
        std::printf("%-32s %-32s %12.1f %12.1f %12u\n",
                    r.strategy.c_str(), r.scene.c_str(),
                    r.mean_ns, r.p99_ns, r.nodes_completed);
    }

    // Cycle detection summary.
    std::printf("\n=== Cycle detection ===\n");
    for (auto& scene : kScenes) {
        bool any = false;
        for (auto seed : kSeeds) {
            DAG d = scene.build(seed);
            if (detect_cycle(d)) { any = true; break; }
        }
        std::printf("%-32s cycle_detected=%s\n", scene.name.data(),
                    any ? "YES" : "no");
    }

    return 0;
}
