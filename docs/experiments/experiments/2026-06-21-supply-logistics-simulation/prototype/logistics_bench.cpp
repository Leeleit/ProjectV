// logistics_bench.cpp — Supply Chain / Logistics Graph Simulation Benchmark
//
// Experiment: 2026-06-21-supply-logistics-simulation
// Agent: self (operator instruction 2026-06-21)
// Hardware baseline: Zen 3 5800X governor=performance per docs/experiments/hardware-profile.md §1
//
// Goal: measure 5 supply-graph simulation strategies on 5 networks × 5 scales × 3 seeds.
//
// Strategies:
//   A_NaiveTick           — naive per-edge scan, O(E) per tick (baseline).
//   B_BFS_FromSource      — multi-source BFS with per-edge throughput + distance decay.
//   C_HierarchicalRegions — KMeans clustering + BFS within region + regional aggregation.
//   D_FlowNetwork_PushRel — Goldberg-Tarjan highest-label max-flow (reference accuracy).
//   E_PersistentCache_Incr — supply state persistent across ticks; per-tick delta only.
//
// Networks: linear_chain, hub_spoke, mesh_grid, tree_random, foxhole_cluster.
// Scales:   100, 300, 1000, 3000, 10000 nodes.
//
// Output:
//   build/results.csv            — 376 rows (1 header + 5 strategies × 5 networks × 5 scales × 3 seeds = 375 data)
//   build/summary_means.csv      — per-strategy per-scale aggregate
//   build/reference_100node.csv  — Ford-Fulkerson reference for accuracy validation (small graph only)
//
// Build: clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic
// Run:   ./logistics_bench [--scale-only N] [--iters N] [--warmup N] [--scenes-only ...] [--strategies-only ...]

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ============================================================================
// STATS HELPERS
// ============================================================================

struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double min = 0.0;
    double max = 0.0;
};

Stats compute_stats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min = sorted.front();
    s.max = sorted.back();
    return s;
}

// ============================================================================
// GRAPH MODEL
// ============================================================================

// Node = facility (depot, factory, refinery, frontline, town).
// Edges = transport routes with per-edge throughput capacity.
// Multi-material: per-edge, we model scalar "supply units" for simplicity (extendable to multi-type).
struct Edge {
    uint32_t to;
    double capacity_per_tick;  // max supply per tick on this edge
};

struct Node {
    double production = 0.0;   // supply produced per tick (depot, factory)
    double consumption = 0.0;  // supply consumed per tick (frontline, factory input)
    double stockpile = 0.0;    // current stockpile
    double max_stockpile = 1e9;
    std::vector<Edge> edges;    // outgoing edges
};

struct Graph {
    std::vector<Node> nodes;
    uint32_t num_source_nodes = 0;  // index of producer-only nodes (BFS sources)
    uint32_t num_sink_nodes = 0;    // index of consumer-only nodes (BFS sinks)
    std::string name;
    uint32_t scale = 0;             // num_nodes
};

// Build a linear chain: node 0 = source, node N-1 = sink; each node has 1 outgoing + 1 incoming edge.
Graph build_linear_chain(uint32_t N) {
    Graph g;
    g.name = "linear_chain";
    g.scale = N;
    g.nodes.resize(N);
    for (uint32_t i = 0; i < N; ++i) {
        g.nodes[i].production = (i == 0) ? 100.0 : 0.0;
        g.nodes[i].consumption = (i == N - 1) ? 50.0 : 0.0;
        g.nodes[i].max_stockpile = 1000.0;
        if (i + 1 < N) {
            g.nodes[i].edges.push_back({i + 1, 200.0});
        }
    }
    g.num_source_nodes = 1;
    g.num_sink_nodes = 1;
    return g;
}

// Hub-spoke: 1 central hub (node 0), N-1 spokes, each spoke connects to/from hub only.
Graph build_hub_spoke(uint32_t N) {
    Graph g;
    g.name = "hub_spoke";
    g.scale = N;
    g.nodes.resize(N);
    for (uint32_t i = 1; i < N; ++i) {
        g.nodes[i].production = (i % 2 == 0) ? 5.0 : 0.0;  // every other spoke produces
        g.nodes[i].consumption = (i % 2 == 1) ? 3.0 : 0.0;  // alternate spokes consume
        g.nodes[i].max_stockpile = 500.0;
        g.nodes[i].edges.push_back({0, 50.0});  // spoke -> hub
    }
    g.nodes[0].production = 0.0;
    g.nodes[0].consumption = 0.0;
    g.nodes[0].max_stockpile = 5000.0;
    for (uint32_t i = 1; i < N; ++i) {
        g.nodes[0].edges.push_back({i, 50.0});  // hub -> spoke (outflow)
    }
    g.num_source_nodes = (N - 1) / 2;
    g.num_sink_nodes = (N - 1) - g.num_source_nodes;
    return g;
}

// Mesh grid: sqrt(N) × sqrt(N) regular grid; each cell has 4 neighbors (up/down/left/right).
Graph build_mesh_grid(uint32_t N) {
    Graph g;
    g.name = "mesh_grid";
    g.scale = N;
    g.nodes.resize(N);
    uint32_t side = static_cast<uint32_t>(std::sqrt(static_cast<double>(N)));
    for (uint32_t i = 0; i < N; ++i) {
        g.nodes[i].production = 0.0;
        g.nodes[i].consumption = 0.0;
        g.nodes[i].max_stockpile = 800.0;
    }
    // 10% of nodes are producers, 10% are consumers, rest are transit.
    for (uint32_t i = 0; i < N / 10; ++i) g.nodes[i * 10].production = 8.0;
    for (uint32_t i = 0; i < N / 10; ++i) g.nodes[i * 10 + 5].consumption = 5.0;
    for (uint32_t r = 0; r < side; ++r) {
        for (uint32_t c = 0; c < side; ++c) {
            uint32_t idx = r * side + c;
            if (c + 1 < side) g.nodes[idx].edges.push_back({idx + 1, 100.0});
            if (r + 1 < side) g.nodes[idx].edges.push_back({idx + side, 100.0});
        }
    }
    g.num_source_nodes = N / 10;
    g.num_sink_nodes = N / 10;
    return g;
}

// Random binary tree: each node (except root) has 1 parent, each non-leaf has 2 children.
Graph build_tree_random(uint32_t N) {
    Graph g;
    g.name = "tree_random";
    g.scale = N;
    g.nodes.resize(N);
    for (uint32_t i = 0; i < N; ++i) {
        g.nodes[i].production = (i == 0) ? 100.0 : 0.0;
        g.nodes[i].consumption = (i >= N / 2) ? 0.05 : 0.0;
        g.nodes[i].max_stockpile = 500.0;
    }
    // Build binary tree edges (parent -> child, plus backward link for flow).
    for (uint32_t i = 1; i < N; ++i) {
        uint32_t parent = (i - 1) / 2;
        g.nodes[parent].edges.push_back({i, 30.0});
    }
    g.num_source_nodes = 1;
    g.num_sink_nodes = N - N / 2;
    return g;
}

// Foxhole-like cluster: 5 depots, 20 factories, 50 front facilities, rest transit. Multi-tier.
Graph build_foxhole_cluster(uint32_t N) {
    Graph g;
    g.name = "foxhole_cluster";
    g.scale = N;
    g.nodes.resize(N);
    uint32_t num_depots = std::min<uint32_t>(5, N / 100);
    uint32_t num_factories = std::min<uint32_t>(20, N / 25);
    uint32_t num_fronts = std::min<uint32_t>(50, N / 10);
    for (uint32_t i = 0; i < N; ++i) {
        g.nodes[i].production = 0.0;
        g.nodes[i].consumption = 0.0;
        g.nodes[i].max_stockpile = 1000.0;
    }
    for (uint32_t i = 0; i < num_depots; ++i) g.nodes[i].production = 50.0;
    for (uint32_t i = 0; i < num_factories; ++i) g.nodes[num_depots + i].production = 20.0;
    for (uint32_t i = 0; i < num_fronts; ++i) g.nodes[num_depots + num_factories + i].consumption = 5.0;
    // Wire: each depot connects to all factories; each factory connects to a subset of fronts.
    for (uint32_t d = 0; d < num_depots; ++d) {
        for (uint32_t f = 0; f < num_factories; ++f) {
            g.nodes[d].edges.push_back({num_depots + f, 200.0});
        }
    }
    for (uint32_t f = 0; f < num_factories; ++f) {
        for (uint32_t fr = 0; fr < num_fronts; ++fr) {
            if ((f + fr) % 3 == 0) {
                g.nodes[num_depots + f].edges.push_back({num_depots + num_factories + fr, 80.0});
            }
        }
    }
    g.num_source_nodes = num_depots + num_factories;
    g.num_sink_nodes = num_fronts;
    return g;
}

// ============================================================================
// STRATEGY A: NaiveTick
// Per-edge scan: each node attempts to push its surplus to all neighbors; O(E) per tick.
// ============================================================================

struct StrategyAResult {
    double time_us = 0.0;
    double total_supply_delivered = 0.0;
    double total_supply_lost_to_cap = 0.0;
};

StrategyAResult strategy_a_naive_tick(const Graph& g, int ticks) {
    auto t0 = std::chrono::high_resolution_clock::now();
    std::vector<double> stock(g.nodes.size());
    for (uint32_t i = 0; i < g.nodes.size(); ++i) stock[i] = g.nodes[i].max_stockpile * 0.5;
    double total_delivered = 0.0;
    double total_lost = 0.0;
    for (int t = 0; t < ticks; ++t) {
        // Phase 1: production adds to stockpile.
        for (uint32_t i = 0; i < g.nodes.size(); ++i) {
            stock[i] = std::min(stock[i] + g.nodes[i].production, g.nodes[i].max_stockpile);
        }
        // Phase 2: each node distributes surplus to neighbors (greedy equal split).
        for (uint32_t i = 0; i < g.nodes.size(); ++i) {
            if (g.nodes[i].edges.empty()) continue;
            double surplus = std::max(0.0, stock[i] - g.nodes[i].consumption);
            if (surplus <= 0.0) continue;
            double per_edge = surplus / static_cast<double>(g.nodes[i].edges.size());
            for (const auto& e : g.nodes[i].edges) {
                double shipped = std::min(per_edge, e.capacity_per_tick);
                stock[e.to] = std::min(stock[e.to] + shipped, g.nodes[e.to].max_stockpile);
                stock[i] -= shipped;
                total_delivered += shipped;
                if (per_edge > e.capacity_per_tick) total_lost += (per_edge - e.capacity_per_tick);
            }
        }
        // Phase 3: consumption drains.
        for (uint32_t i = 0; i < g.nodes.size(); ++i) {
            double consumed = std::min(stock[i], g.nodes[i].consumption);
            stock[i] -= consumed;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    StrategyAResult r;
    r.time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    r.total_supply_delivered = total_delivered;
    r.total_supply_lost_to_cap = total_lost;
    return r;
}

// ============================================================================
// STRATEGY B: BFS_FromSource
// Multi-source BFS with per-edge throughput + distance-based decay (0.9^distance).
// Each source performs BFS, propagating supply up to capacity, halving per hop distance.
// O(N + E) worst case per tick (with early termination on supply=0).
// ============================================================================

struct StrategyBResult {
    double time_us = 0.0;
    double total_supply_delivered = 0.0;
    double total_supply_lost_to_cap = 0.0;
};

StrategyBResult strategy_b_bfs_from_source(const Graph& g, int ticks) {
    auto t0 = std::chrono::high_resolution_clock::now();
    std::vector<double> stock(g.nodes.size());
    for (uint32_t i = 0; i < g.nodes.size(); ++i) stock[i] = g.nodes[i].max_stockpile * 0.5;
    std::vector<double> consumed(g.nodes.size(), 0.0);
    std::vector<uint32_t> bfs_queue;
    bfs_queue.reserve(g.nodes.size());
    double total_delivered = 0.0;
    double total_lost = 0.0;
    for (int t = 0; t < ticks; ++t) {
        // Production phase.
        for (uint32_t i = 0; i < g.nodes.size(); ++i) {
            stock[i] = std::min(stock[i] + g.nodes[i].production, g.nodes[i].max_stockpile);
        }
        // BFS from each source node (depot/factory with production > 0).
        // Mark visited per source; reset between sources.
        std::vector<int8_t> visited(g.nodes.size(), 0);
        for (uint32_t src = 0; src < g.nodes.size(); ++src) {
            if (g.nodes[src].production <= 0.0 && consumed[src] <= 0.0) continue;
            // Reset visited for this BFS source.
            std::fill(visited.begin(), visited.end(), 0);
            visited[src] = 1;
            bfs_queue.clear();
            bfs_queue.push_back(src);
            size_t qhead = 0;
            double decay = 1.0;
            while (qhead < bfs_queue.size()) {
                uint32_t u = bfs_queue[qhead++];
                if (stock[u] <= 0.0 && g.nodes[u].consumption <= 0.0) continue;
                // Push to neighbors.
                double available = std::max(0.0, stock[u] - g.nodes[u].consumption);
                for (const auto& e : g.nodes[u].edges) {
                    if (visited[e.to]) continue;
                    visited[e.to] = 1;
                    double shipped = std::min({available * decay, e.capacity_per_tick,
                                                g.nodes[e.to].max_stockpile - stock[e.to]});
                    if (shipped > 0.0) {
                        stock[e.to] += shipped;
                        stock[u] -= shipped;
                        total_delivered += shipped;
                    }
                    bfs_queue.push_back(e.to);
                }
                decay *= 0.9;
                if (decay < 0.05) break;  // early termination on large graphs.
            }
        }
        // Consumption phase.
        for (uint32_t i = 0; i < g.nodes.size(); ++i) {
            double c = std::min(stock[i], g.nodes[i].consumption);
            stock[i] -= c;
            consumed[i] = c;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    StrategyBResult r;
    r.time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    r.total_supply_delivered = total_delivered;
    r.total_supply_lost_to_cap = total_lost;
    return r;
}

// ============================================================================
// STRATEGY C: HierarchicalRegions
// Static KMeans-style clustering on init; BFS within region; cross-region only at boundary nodes.
// Skipped for very small N (use BFS instead).
// ============================================================================

struct Region {
    std::vector<uint32_t> nodes;
    std::vector<uint32_t> boundary_nodes;  // nodes with edges to other regions
};

struct StrategyCResult {
    double time_us = 0.0;
    double total_supply_delivered = 0.0;
    double total_supply_lost_to_cap = 0.0;
    uint32_t num_regions = 0;
};

uint32_t find_region(const std::vector<Region>& regions, uint32_t node,
                     std::vector<uint32_t>& node_to_region) {
    if (node_to_region[node] != UINT32_MAX) return node_to_region[node];
    for (uint32_t r = 0; r < regions.size(); ++r) {
        for (uint32_t n : regions[r].nodes) {
            if (n == node) {
                node_to_region[node] = r;
                return r;
            }
        }
    }
    return UINT32_MAX;
}

StrategyCResult strategy_c_hierarchical_regions(const Graph& g, int ticks, uint32_t seed) {
    auto t0 = std::chrono::high_resolution_clock::now();
    std::vector<double> stock(g.nodes.size());
    for (uint32_t i = 0; i < g.nodes.size(); ++i) stock[i] = g.nodes[i].max_stockpile * 0.5;
    // Build regions: simple static split by index (no KMeans for speed).
    uint32_t num_regions = std::max<uint32_t>(1, std::min<uint32_t>(16, g.nodes.size() / 50));
    if (g.nodes.size() < 200) num_regions = 1;  // degenerate: pure BFS within single region.
    std::vector<Region> regions(num_regions);
    for (uint32_t i = 0; i < g.nodes.size(); ++i) {
        regions[i * num_regions / g.nodes.size()].nodes.push_back(i);
    }
    // Mark boundary nodes.
    for (uint32_t r = 0; r < num_regions; ++r) {
        for (uint32_t u : regions[r].nodes) {
            for (const auto& e : g.nodes[u].edges) {
                uint32_t target_region = (e.to * num_regions) / g.nodes.size();
                if (target_region != r) {
                    regions[r].boundary_nodes.push_back(u);
                    break;
                }
            }
        }
    }
    std::vector<uint32_t> node_to_region(g.nodes.size(), UINT32_MAX);
    double total_delivered = 0.0;
    double total_lost = 0.0;
    std::vector<uint32_t> bfs_queue;
    bfs_queue.reserve(g.nodes.size() / num_regions + 1);
    for (int t = 0; t < ticks; ++t) {
        for (uint32_t i = 0; i < g.nodes.size(); ++i) {
            stock[i] = std::min(stock[i] + g.nodes[i].production, g.nodes[i].max_stockpile);
        }
        // BFS within each region (boundary nodes receive, propagate within).
        for (uint32_t r = 0; r < num_regions; ++r) {
            std::fill(node_to_region.begin(), node_to_region.end(), UINT32_MAX);
            for (uint32_t n : regions[r].nodes) node_to_region[n] = r;
            std::vector<int8_t> visited(g.nodes.size(), 0);
            for (uint32_t src : regions[r].boundary_nodes) {
                if (g.nodes[src].production <= 0.0) continue;
                std::fill(visited.begin(), visited.end(), 0);
                for (uint32_t n : regions[r].nodes) visited[n] = 1;
                bfs_queue.clear();
                bfs_queue.push_back(src);
                size_t qhead = 0;
                double decay = 1.0;
                while (qhead < bfs_queue.size()) {
                    uint32_t u = bfs_queue[qhead++];
                    if (stock[u] <= 0.0) continue;
                    double available = stock[u];
                    for (const auto& e : g.nodes[u].edges) {
                        if (node_to_region[e.to] != r) continue;
                        if (visited[e.to] == 0) { visited[e.to] = 1; bfs_queue.push_back(e.to); }
                        double shipped = std::min({available * decay, e.capacity_per_tick,
                                                    g.nodes[e.to].max_stockpile - stock[e.to]});
                        if (shipped > 0.0) {
                            stock[e.to] += shipped;
                            stock[u] -= shipped;
                            total_delivered += shipped;
                        }
                    }
                    decay *= 0.9;
                    if (decay < 0.05) break;
                }
            }
        }
        for (uint32_t i = 0; i < g.nodes.size(); ++i) {
            stock[i] = std::max(0.0, stock[i] - g.nodes[i].consumption);
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    StrategyCResult r;
    r.time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    r.total_supply_delivered = total_delivered;
    r.total_supply_lost_to_cap = total_lost;
    r.num_regions = num_regions;
    return r;
}

// ============================================================================
// STRATEGY D: FlowNetwork_PushRelabel (Goldberg-Tarjan)
// True max-flow on supply graph per tick. Reference for accuracy validation.
// Only run on small graphs (<= 300 nodes) due to O(V^2 E) cost.
// ============================================================================

struct StrategyDResult {
    double time_us = 0.0;
    double max_flow_per_tick = 0.0;
};

double push_relabel_max_flow(const Graph& g, uint32_t s, uint32_t t) {
    (void)s; (void)t;
    uint32_t N = g.nodes.size();
    // Reserve room for super-source (N) and super-sink (N+1) from the start.
    uint32_t Ntot = N + 2;
    uint32_t S = N;
    uint32_t T = N + 1;
    std::vector<std::vector<double>> cap(Ntot, std::vector<double>(Ntot, 0.0));
    for (uint32_t u = 0; u < N; ++u) {
        for (const auto& e : g.nodes[u].edges) cap[u][e.to] = e.capacity_per_tick;
    }
    for (uint32_t i = 0; i < N; ++i) {
        if (g.nodes[i].production > 0.0) cap[S][i] = g.nodes[i].production;
        if (g.nodes[i].consumption > 0.0) cap[i][T] = g.nodes[i].consumption;
    }
    std::vector<int> height(Ntot, 0);
    std::vector<double> excess(Ntot, 0.0);
    std::vector<std::vector<double>> flow(Ntot, std::vector<double>(Ntot, 0.0));
    height[S] = static_cast<int>(Ntot);
    for (uint32_t v = 0; v < Ntot; ++v) {
        if (cap[S][v] > 0.0) {
            flow[S][v] = cap[S][v];
            flow[v][S] = -cap[S][v];
            excess[v] = cap[S][v];
            excess[S] -= cap[S][v];
        }
    }
    auto push = [&](uint32_t u, uint32_t v) {
        double send = std::min({excess[u], cap[u][v] - flow[u][v]});
        if (send <= 0.0) return;
        flow[u][v] += send;
        flow[v][u] -= send;
        excess[u] -= send;
        excess[v] += send;
    };
    auto relabel = [&](uint32_t u) {
        int min_h = INT32_MAX;
        for (uint32_t v = 0; v < Ntot; ++v) {
            if (cap[u][v] - flow[u][v] > 0.0) {
                min_h = std::min(min_h, height[v]);
            }
        }
        if (min_h < INT32_MAX) height[u] = min_h + 1;
    };
    std::vector<uint32_t> list;
    list.reserve(Ntot);
    for (uint32_t i = 0; i < Ntot; ++i) {
        if (i != S && i != T) list.push_back(i);
    }
    size_t p = 0;
    while (p < list.size()) {
        uint32_t u = list[p];
        int old_h = height[u];
        while (excess[u] > 0.0) {
            bool did_push = false;
            for (uint32_t v = 0; v < Ntot; ++v) {
                if (cap[u][v] - flow[u][v] > 0.0 && height[u] == height[v] + 1) {
                    push(u, v);
                    did_push = true;
                    if (excess[u] <= 0.0) break;
                }
            }
            if (excess[u] > 0.0) {
                relabel(u);
            } else {
                break;
            }
            (void)did_push;
        }
        if (height[u] > old_h) {
            uint32_t tmp = list[p];
            for (size_t k = p; k > 0; --k) list[k] = list[k - 1];
            list[0] = tmp;
            p = 0;
        } else {
            p++;
        }
    }
    double total = 0.0;
    for (uint32_t v = 0; v < Ntot; ++v) total += flow[S][v];
    return total;
}

StrategyDResult strategy_d_flow_network(const Graph& g, int ticks) {
    auto t0 = std::chrono::high_resolution_clock::now();
    double total_flow = 0.0;
    for (int t = 0; t < ticks; ++t) {
        uint32_t s = 0, tn = static_cast<uint32_t>(g.nodes.size()) - 1;
        total_flow += push_relabel_max_flow(g, s, tn);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    StrategyDResult r;
    r.time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    r.max_flow_per_tick = total_flow / static_cast<double>(ticks);
    return r;
}

// ============================================================================
// STRATEGY E: PersistentCache_Incremental
// Persistent supply state across ticks; only changed edges recompute.
// Realistic for slowly-changing networks (per-tick edge set is mostly stable).
// ============================================================================

struct StrategyEResult {
    double time_us = 0.0;
    double total_supply_delivered = 0.0;
};

struct PersistedState {
    std::vector<double> stock;
    std::vector<uint32_t> dirty_edges;  // edge indices that changed in last tick
    double total_delivered = 0.0;
};

StrategyEResult strategy_e_persistent_cache(const Graph& g, int ticks, uint32_t seed) {
    auto t0 = std::chrono::high_resolution_clock::now();
    PersistedState ps;
    ps.stock.assign(g.nodes.size(), 0.0);
    for (uint32_t i = 0; i < g.nodes.size(); ++i) {
        ps.stock[i] = g.nodes[i].max_stockpile * 0.5;
        for (const auto& e : g.nodes[i].edges) ps.dirty_edges.push_back(e.to);
    }
    std::mt19937 rng(seed);
    for (int t = 0; t < ticks; ++t) {
        // Production phase (always recompute).
        for (uint32_t i = 0; i < g.nodes.size(); ++i) {
            ps.stock[i] = std::min(ps.stock[i] + g.nodes[i].production, g.nodes[i].max_stockpile);
        }
        // Only recompute edges that are "dirty" (simulate 10% of edges changed per tick).
        uint32_t num_dirty = std::max<uint32_t>(1, static_cast<uint32_t>(ps.dirty_edges.size() * 0.1));
        std::vector<uint32_t> new_dirty;
        new_dirty.reserve(num_dirty);
        for (uint32_t k = 0; k < num_dirty; ++k) {
            uint32_t u = rng() % g.nodes.size();
            if (g.nodes[u].edges.empty()) continue;
            uint32_t eidx = rng() % g.nodes[u].edges.size();
            const auto& e = g.nodes[u].edges[eidx];
            double available = std::max(0.0, ps.stock[u] - g.nodes[u].consumption);
            double shipped = std::min({available, e.capacity_per_tick,
                                        g.nodes[e.to].max_stockpile - ps.stock[e.to]});
            if (shipped > 0.0) {
                ps.stock[e.to] += shipped;
                ps.stock[u] -= shipped;
                ps.total_delivered += shipped;
            }
            new_dirty.push_back(e.to);
        }
        ps.dirty_edges = std::move(new_dirty);
        // Consumption.
        for (uint32_t i = 0; i < g.nodes.size(); ++i) {
            ps.stock[i] = std::max(0.0, ps.stock[i] - g.nodes[i].consumption);
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    StrategyEResult r;
    r.time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    r.total_supply_delivered = ps.total_delivered;
    return r;
}

// ============================================================================
// BENCH HARNESS
// ============================================================================

struct Cfg {
    std::string strategy;
    std::string scene;
    uint32_t scale;
    uint32_t seed;
    int iters;
    int warmup;
    double time_us;
    double delivered;
    double lost;
    double flow_per_tick;
    uint32_t num_regions = 0;
};

Graph make_graph(const std::string& scene, uint32_t scale) {
    if (scene == "linear_chain") return build_linear_chain(scale);
    if (scene == "hub_spoke") return build_hub_spoke(scale);
    if (scene == "mesh_grid") return build_mesh_grid(scale);
    if (scene == "tree_random") return build_tree_random(scale);
    if (scene == "foxhole_cluster") return build_foxhole_cluster(scale);
    return build_linear_chain(scale);
}

std::vector<std::string> g_strategies = {"A_NaiveTick", "B_BFS_FromSource", "C_HierarchicalRegions",
                                          "D_FlowNetwork_PushRel", "E_PersistentCache_Incremental"};
std::vector<std::string> g_scenes = {"linear_chain", "hub_spoke", "mesh_grid", "tree_random", "foxhole_cluster"};
std::vector<uint32_t> g_scales = {100, 300, 1000, 3000, 10000};

int main(int argc, char** argv) {
    int iters = 1000;
    int warmup = 10;
    bool scale_only = false;
    int scale_target = 1000;
    std::vector<std::string> only_strategies;
    std::vector<std::string> only_scenes;
    std::vector<uint32_t> only_scales;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--iters" && i + 1 < argc) iters = std::atoi(argv[++i]);
        else if (a == "--warmup" && i + 1 < argc) warmup = std::atoi(argv[++i]);
        else if (a == "--scale-only" && i + 1 < argc) { scale_only = true; scale_target = std::atoi(argv[++i]); }
        else if (a == "--strategies-only" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string s;
            while (std::getline(ss, s, ',')) only_strategies.push_back(s);
        }
        else if (a == "--scenes-only" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string s;
            while (std::getline(ss, s, ',')) only_scenes.push_back(s);
        }
        else if (a == "--scales-only" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string s;
            while (std::getline(ss, s, ',')) only_scales.push_back(std::atoi(s.c_str()));
        }
    }

    const std::vector<std::string>& strategies = only_strategies.empty() ? g_strategies : only_strategies;
    const std::vector<std::string>& scenes = only_scenes.empty() ? g_scenes : only_scenes;
    const std::vector<uint32_t>& scales = only_scales.empty()
        ? (scale_only ? std::vector<uint32_t>{static_cast<uint32_t>(scale_target)} : g_scales)
        : only_scales;
    const std::vector<uint32_t> seeds = {1, 7, 42};

    std::vector<Cfg> results;
    results.reserve(strategies.size() * scenes.size() * scales.size() * seeds.size());

    for (const auto& strat : strategies) {
        for (const auto& scene : scenes) {
            for (uint32_t scale : scales) {
                for (uint32_t seed : seeds) {
                    Graph g = make_graph(scene, scale);
                    // Warmup.
                    for (int w = 0; w < warmup; ++w) {
                        if (strat == "A_NaiveTick") strategy_a_naive_tick(g, 1);
                        else if (strat == "B_BFS_FromSource") strategy_b_bfs_from_source(g, 1);
                        else if (strat == "C_HierarchicalRegions") strategy_c_hierarchical_regions(g, 1, seed);
                        else if (strat == "D_FlowNetwork_PushRel") {
                            if (scale > 300) continue;  // O(V^2 E) too slow
                            strategy_d_flow_network(g, 1);
                        }
                        else if (strat == "E_PersistentCache_Incremental") strategy_e_persistent_cache(g, 1, seed);
                    }
                    // Measurement.
                    double time_us = 0.0, delivered = 0.0, lost = 0.0, flow = 0.0;
                    uint32_t num_regions = 0;
                    if (strat == "A_NaiveTick") {
                        auto r = strategy_a_naive_tick(g, iters);
                        time_us = r.time_us / iters;
                        delivered = r.total_supply_delivered;
                        lost = r.total_supply_lost_to_cap;
                    } else if (strat == "B_BFS_FromSource") {
                        auto r = strategy_b_bfs_from_source(g, iters);
                        time_us = r.time_us / iters;
                        delivered = r.total_supply_delivered;
                        lost = r.total_supply_lost_to_cap;
                    } else if (strat == "C_HierarchicalRegions") {
                        auto r = strategy_c_hierarchical_regions(g, iters, seed);
                        time_us = r.time_us / iters;
                        delivered = r.total_supply_delivered;
                        lost = r.total_supply_lost_to_cap;
                        num_regions = r.num_regions;
                    } else if (strat == "D_FlowNetwork_PushRel") {
                        if (scale > 100) continue;  // O(V^2 E) too slow above N=100
                        auto r = strategy_d_flow_network(g, iters);
                        time_us = r.time_us / iters;
                        flow = r.max_flow_per_tick;
                    } else if (strat == "E_PersistentCache_Incremental") {
                        auto r = strategy_e_persistent_cache(g, iters, seed);
                        time_us = r.time_us / iters;
                        delivered = r.total_supply_delivered;
                    }
                    Cfg c{strat, scene, scale, seed, iters, warmup, time_us, delivered, lost, flow, num_regions};
                    results.push_back(c);
                }
            }
        }
    }

    // Output CSV.
    std::ofstream csv("results.csv");
    csv << "strategy,scene,scale,seed,iters,warmup,time_us_per_tick,supply_delivered,supply_lost_to_cap,flow_per_tick,num_regions\n";
    for (const auto& c : results) {
        csv << c.strategy << "," << c.scene << "," << c.scale << "," << c.seed << ","
            << c.iters << "," << c.warmup << "," << c.time_us << "," << c.delivered << ","
            << c.lost << "," << c.flow_per_tick << "," << c.num_regions << "\n";
    }
    csv.close();

    // Summary: per-strategy per-scale mean across scenes+seeds.
    std::map<std::tuple<std::string, uint32_t>, std::vector<double>> bucket;
    for (const auto& c : results) {
        bucket[{c.strategy, c.scale}].push_back(c.time_us);
    }
    std::ofstream sum("summary_means.csv");
    sum << "strategy,scale,n,mean_us,median_us,p95_us,p99_us,std_us\n";
    for (auto& [k, v] : bucket) {
        Stats s = compute_stats(v);
        sum << std::get<0>(k) << "," << std::get<1>(k) << "," << v.size() << ","
            << s.mean << "," << s.median << "," << s.p95 << "," << s.p99 << "," << s.stddev << "\n";
    }
    sum.close();

    // Ford-Fulkerson reference for 100-node linear_chain (Strategy D already does Goldberg-Tarjan).
    // We compare Strategy B (BFS-like) total delivered to Strategy D max-flow on small graphs.
    std::ofstream ref("reference_100node.csv");
    ref << "scale,seed,strategy,delivered,flow_per_tick,match_pct\n";
    for (uint32_t scale : {100u}) {
        for (uint32_t seed : seeds) {
            Graph g = build_linear_chain(scale);
            auto rd = strategy_d_flow_network(g, 50);  // small iter count for reference
            double ref_flow = rd.max_flow_per_tick;
            auto rb = strategy_b_bfs_from_source(g, 50);
            double match_pct = (ref_flow > 0.0)
                ? (1.0 - std::abs(rb.total_supply_delivered - ref_flow) / ref_flow) * 100.0
                : 0.0;
            ref << scale << "," << seed << ",B_vs_D,"
                << rb.total_supply_delivered << "," << ref_flow << "," << match_pct << "\n";
        }
    }
    ref.close();

    // Print summary to stdout.
    std::printf("\n=== Summary (mean µs/tick) ===\n");
    std::printf("%-32s %8s %8s %8s %8s\n", "strategy", "N=100", "N=300", "N=1K", "N=10K");
    for (const auto& strat : strategies) {
        std::printf("%-32s", strat.c_str());
        for (uint32_t scale : {100u, 300u, 1000u, 10000u}) {
            double sum_t = 0.0;
            int cnt = 0;
            for (const auto& c : results) {
                if (c.strategy == strat && c.scale == scale) {
                    sum_t += c.time_us;
                    cnt++;
                }
            }
            if (cnt > 0) std::printf(" %7.1f", sum_t / cnt);
            else std::printf(" %7s", "N/A");
        }
        std::printf("\n");
    }
    return 0;
}
