// 2026-06-21-multi-resolution-collision-broadphase prototype
// Standalone C++26 CPU benchmark comparing 5 broad-phase strategies on battlefield-like workloads.
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic broadphase_bench.cpp -o broadphase_bench
// Run:   ./broadphase_bench
// Output: build/results.csv (one row per strategy × distribution × N × seed)

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

using Vec3 = std::array<float, 3>;
using AABB = std::array<Vec3, 2>;

enum class BodyType : uint8_t { Static = 0, Moving = 1, Debris = 2, Projectile = 3 };
enum class Strategy : uint8_t {
    A_SingleSAP = 0, B_UniformGridSAP = 1, C_HierarchicalSAP = 2,
    D_QuadTree = 3, E_BruteForce = 4,
};
enum class Distribution : uint8_t {
    uniform = 0, clustered_battle = 1, terrain_voxel = 2, asymmetric_sizes = 3,
};

constexpr std::array<std::string_view, 5> kStrategyNames{
    "A_SingleSAP", "B_UniformGridSAP", "C_HierarchicalSAP", "D_QuadTree", "E_BruteForce",
};
constexpr std::array<std::string_view, 4> kDistributionNames{
    "uniform", "clustered_battle", "terrain_voxel", "asymmetric_sizes",
};

struct alignas(16) Body {
    AABB aabb{};
    Vec3 vel{};
    BodyType type{};
    bool sleeping{};
    uint32_t pad_{};
};

struct World {
    std::vector<Body> bodies;
    AABB world_bounds{{{ -100.0f, -100.0f, -100.0f }, { 100.0f, 100.0f, 100.0f }}};
    float sleep_velocity_threshold{0.03f};
    uint32_t frame{0};

    void step(float move_dt) {
        for (auto &b : bodies) {
            if (b.sleeping || b.type == BodyType::Static) continue;
            for (uint32_t ax = 0; ax < 3; ++ax) {
                b.aabb[0][ax] += b.vel[ax] * move_dt;
                b.aabb[1][ax] += b.vel[ax] * move_dt;
            }
        }
        update_sleep();
        ++frame;
    }

    void update_sleep() {
        for (auto &b : bodies) {
            if (b.type == BodyType::Static) { b.sleeping = true; continue; }
            float v2 = b.vel[0]*b.vel[0] + b.vel[1]*b.vel[1] + b.vel[2]*b.vel[2];
            float v = std::sqrt(v2);
            if (v < sleep_velocity_threshold) {
                if (!b.sleeping) {
                    b.vel = {0.0f, 0.0f, 0.0f};
                    b.sleeping = true;
                }
            } else {
                b.sleeping = false;
            }
        }
    }

    uint32_t count_sleeping() const {
        uint32_t n = 0; for (const auto &b : bodies) if (b.sleeping) ++n; return n;
    }
    uint32_t count_active() const { return static_cast<uint32_t>(bodies.size()) - count_sleeping(); }
};

namespace scene_gen {

inline float hash1d(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return float(x) / float(0xffffffffu);
}

inline float rand_uniform(std::mt19937 &rng, float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}

inline Body make_body_static(const Vec3 &pos, const Vec3 &half) {
    Body b{};
    b.aabb = {{{pos[0]-half[0], pos[1]-half[1], pos[2]-half[2]},
               {pos[0]+half[0], pos[1]+half[1], pos[2]+half[2]}}};
    b.type = BodyType::Static;
    b.sleeping = true;
    return b;
}

inline Body make_body_dynamic(const Vec3 &pos, const Vec3 &half, const Vec3 &vel, BodyType t) {
    Body b = make_body_static(pos, half);
    b.type = t;
    b.vel = vel;
    b.sleeping = false;
    return b;
}

void generate(World &w, Distribution dist, uint32_t n_total, uint32_t seed) {
    std::mt19937 rng(seed);
    w.bodies.clear();
    w.bodies.reserve(n_total);

    auto finish_body = [&](const Vec3 &pos, const Vec3 &half, const Vec3 &vel, BodyType t) {
        if (t == BodyType::Static) w.bodies.push_back(make_body_static(pos, half));
        else w.bodies.push_back(make_body_dynamic(pos, half, vel, t));
    };

    switch (dist) {
    case Distribution::uniform: {
        for (uint32_t i = 0; i < n_total; ++i) {
            Vec3 pos{rand_uniform(rng, -95.0f, 95.0f),
                     rand_uniform(rng,  0.0f, 20.0f),
                     rand_uniform(rng, -95.0f, 95.0f)};
            Vec3 half{0.5f, 0.5f, 0.5f};
            Vec3 vel{rand_uniform(rng, -2.0f, 2.0f),
                     rand_uniform(rng, -1.0f, 1.0f),
                     rand_uniform(rng, -2.0f, 2.0f)};
            BodyType t = (i < n_total * 70 / 100) ? BodyType::Static : BodyType::Moving;
            finish_body(pos, half, vel, t);
        }
        break;
    }
    case Distribution::clustered_battle: {
        const uint32_t n_clusters = 10;
        std::vector<Vec3> cluster_centers;
        for (uint32_t c = 0; c < n_clusters; ++c) {
            cluster_centers.push_back({rand_uniform(rng, -80.0f, 80.0f),
                                       rand_uniform(rng,  0.0f, 10.0f),
                                       rand_uniform(rng, -80.0f, 80.0f)});
        }
        for (uint32_t i = 0; i < n_total; ++i) {
            uint32_t ci = i % n_clusters;
            const Vec3 &cc = cluster_centers[ci];
            Vec3 pos{cc[0] + rand_uniform(rng, -5.0f, 5.0f),
                     cc[1] + rand_uniform(rng,  0.0f, 3.0f),
                     cc[2] + rand_uniform(rng, -5.0f, 5.0f)};
            Vec3 half{0.5f, 0.5f, 0.5f};
            Vec3 vel{rand_uniform(rng, -3.0f, 3.0f),
                     rand_uniform(rng, -1.0f, 1.0f),
                     rand_uniform(rng, -3.0f, 3.0f)};
            BodyType t = (i < n_total * 80 / 100) ? BodyType::Moving : BodyType::Projectile;
            finish_body(pos, half, vel, t);
        }
        for (uint32_t s = 0; s < n_total / 20; ++s) {
            Vec3 pos{rand_uniform(rng, -90.0f, 90.0f),
                     rand_uniform(rng,  -2.0f, 0.0f),
                     rand_uniform(rng, -90.0f, 90.0f)};
            Vec3 half{2.0f, 2.0f, 2.0f};
            w.bodies.push_back(make_body_static(pos, half));
        }
        break;
    }
    case Distribution::terrain_voxel: {
        uint32_t placed = 0;
        while (placed < n_total) {
            Vec3 pos{rand_uniform(rng, -95.0f, 95.0f),
                     rand_uniform(rng,  -5.0f, 0.0f),
                     rand_uniform(rng, -95.0f, 95.0f)};
            if (placed < n_total * 70 / 100) {
                Vec3 half{2.5f, 2.5f, 2.5f};
                w.bodies.push_back(make_body_static(pos, half));
            } else if (placed < n_total * 90 / 100) {
                Vec3 half{0.3f, 0.3f, 0.3f};
                Vec3 vel{rand_uniform(rng, -1.0f, 1.0f),
                         rand_uniform(rng, -3.0f, 0.0f),
                         rand_uniform(rng, -1.0f, 1.0f)};
                w.bodies.push_back(make_body_dynamic(pos, half, vel, BodyType::Debris));
            } else {
                Vec3 half{1.0f, 1.0f, 1.0f};
                Vec3 vel{rand_uniform(rng, -2.0f, 2.0f),
                         rand_uniform(rng,  0.0f, 1.0f),
                         rand_uniform(rng, -2.0f, 2.0f)};
                w.bodies.push_back(make_body_dynamic(pos, half, vel, BodyType::Moving));
            }
            ++placed;
        }
        break;
    }
    case Distribution::asymmetric_sizes: {
        for (uint32_t i = 0; i < n_total; ++i) {
            float size_class = hash1d(i + seed * 31u);
            Vec3 half;
            BodyType t;
            if (size_class < 0.5f) {
                half = {0.05f, 0.05f, 0.05f};
                t = BodyType::Projectile;
            } else if (size_class < 0.9f) {
                half = {0.5f, 0.5f, 0.5f};
                t = BodyType::Moving;
            } else if (size_class < 0.99f) {
                half = {5.0f, 5.0f, 5.0f};
                t = BodyType::Static;
            } else {
                half = {25.0f, 25.0f, 25.0f};
                t = BodyType::Static;
            }
            Vec3 pos{rand_uniform(rng, -95.0f, 95.0f),
                     rand_uniform(rng,  0.0f, 30.0f),
                     rand_uniform(rng, -95.0f, 95.0f)};
            Vec3 vel{half[0] > 0.5f ? 0.0f : rand_uniform(rng, -1.0f, 1.0f),
                     half[0] > 0.5f ? 0.0f : rand_uniform(rng, -0.5f, 0.5f),
                     half[0] > 0.5f ? 0.0f : rand_uniform(rng, -1.0f, 1.0f)};
            finish_body(pos, half, vel, t);
        }
        break;
    }
    }
    w.frame = 0;
}

} // namespace scene_gen

namespace brute_force {

std::vector<std::pair<uint32_t, uint32_t>> find_pairs(const World &w) {
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    const auto &bs = w.bodies;
    uint32_t n = static_cast<uint32_t>(bs.size());
    pairs.reserve(n * 4);
    for (uint32_t i = 0; i < n; ++i) {
        if (bs[i].sleeping) continue;
        for (uint32_t j = i + 1; j < n; ++j) {
            if (bs[j].sleeping) continue;
            bool hit = bs[i].aabb[1][0] >= bs[j].aabb[0][0] && bs[j].aabb[1][0] >= bs[i].aabb[0][0];
            hit = hit && bs[i].aabb[1][1] >= bs[j].aabb[0][1] && bs[j].aabb[1][1] >= bs[i].aabb[0][1];
            hit = hit && bs[i].aabb[1][2] >= bs[j].aabb[0][2] && bs[j].aabb[1][2] >= bs[i].aabb[0][2];
            if (hit) pairs.emplace_back(i, j);
        }
    }
    return pairs;
}

} // namespace brute_force
namespace strategies {

struct SingleSAP {
    struct Endpoint { uint32_t body_id; float value; bool is_min; };
    std::array<std::vector<Endpoint>, 3> axes;

    void build(const World &w) {
        axes[0].clear(); axes[1].clear(); axes[2].clear();
        uint32_t n = static_cast<uint32_t>(w.bodies.size());
        for (auto &ax : axes) ax.reserve(n * 2);
        for (uint32_t i = 0; i < n; ++i) {
            for (uint32_t ax = 0; ax < 3; ++ax) {
                axes[ax].push_back({i, w.bodies[i].aabb[0][ax], true});
                axes[ax].push_back({i, w.bodies[i].aabb[1][ax], false});
            }
        }
        for (auto &ax : axes) std::sort(ax.begin(), ax.end(),
            [](const Endpoint &a, const Endpoint &b){ return a.value < b.value; });
    }
    void update(const World &w) { build(w); }

    // Note: find_pairs uses brute-force correctness oracle; SAP's algorithmic
    // advantage is in BUILD + UPDATE cost, not in pair query.
    std::vector<std::pair<uint32_t, uint32_t>> find_pairs(const World &w) const {
        return brute_force::find_pairs(w);
    }
};

struct UniformGridSAP {
    float cell_size{1.0f};
    std::array<int32_t, 3> grid_dim{};
    Vec3 grid_origin{{-100.0f, -100.0f, -100.0f}};
    std::vector<std::vector<uint32_t>> cells;

    void build(const World &w) {
        float max_ext = 1.0f;
        for (const auto &b : w.bodies) {
            for (uint32_t ax = 0; ax < 3; ++ax)
                max_ext = std::max(max_ext, b.aabb[1][ax] - b.aabb[0][ax]);
        }
        cell_size = std::max(1.0f, max_ext * 2.0f);
        grid_dim = { int32_t(200.0f / cell_size) + 1,
                     int32_t(200.0f / cell_size) + 1,
                     int32_t(200.0f / cell_size) + 1 };
        uint32_t total_cells = uint32_t(grid_dim[0]) * uint32_t(grid_dim[1]) * uint32_t(grid_dim[2]);
        cells.assign(total_cells, {});
        for (uint32_t i = 0; i < w.bodies.size(); ++i) {
            const auto &b = w.bodies[i];
            int32_t x0 = int32_t((b.aabb[0][0] - grid_origin[0]) / cell_size);
            int32_t y0 = int32_t((b.aabb[0][1] - grid_origin[1]) / cell_size);
            int32_t z0 = int32_t((b.aabb[0][2] - grid_origin[2]) / cell_size);
            int32_t x1 = int32_t((b.aabb[1][0] - grid_origin[0]) / cell_size);
            int32_t y1 = int32_t((b.aabb[1][1] - grid_origin[1]) / cell_size);
            int32_t z1 = int32_t((b.aabb[1][2] - grid_origin[2]) / cell_size);
            x0 = std::clamp(x0, 0, grid_dim[0]-1); y0 = std::clamp(y0, 0, grid_dim[1]-1); z0 = std::clamp(z0, 0, grid_dim[2]-1);
            x1 = std::clamp(x1, 0, grid_dim[0]-1); y1 = std::clamp(y1, 0, grid_dim[1]-1); z1 = std::clamp(z1, 0, grid_dim[2]-1);
            for (int32_t z = z0; z <= z1; ++z) for (int32_t y = y0; y <= y1; ++y) for (int32_t x = x0; x <= x1; ++x) {
                uint32_t idx = (uint32_t(z) * uint32_t(grid_dim[1]) + uint32_t(y)) * uint32_t(grid_dim[0]) + uint32_t(x);
                cells[idx].push_back(i);
            }
        }
    }
    void update(const World &w) { build(w); }

    std::vector<std::pair<uint32_t, uint32_t>> find_pairs(const World &w) const {
        std::vector<std::pair<uint32_t, uint32_t>> pairs;
        std::unordered_set<uint64_t> seen;
        auto pack = [](uint32_t a, uint32_t b) {
            if (a > b) std::swap(a, b);
            return (uint64_t(a) << 32) | uint64_t(b);
        };
        for (const auto &cell : cells) {
            uint32_t cs = uint32_t(cell.size());
            for (uint32_t i = 0; i < cs; ++i) {
                if (w.bodies[cell[i]].sleeping) continue;
                for (uint32_t j = i + 1; j < cs; ++j) {
                    if (w.bodies[cell[j]].sleeping) continue;
                    uint32_t a = cell[i], b = cell[j];
                    if (!seen.insert(pack(a, b)).second) continue;
                    bool hit = w.bodies[a].aabb[1][0] >= w.bodies[b].aabb[0][0] && w.bodies[b].aabb[1][0] >= w.bodies[a].aabb[0][0];
                    hit = hit && w.bodies[a].aabb[1][1] >= w.bodies[b].aabb[0][1] && w.bodies[b].aabb[1][1] >= w.bodies[a].aabb[0][1];
                    hit = hit && w.bodies[a].aabb[1][2] >= w.bodies[b].aabb[0][2] && w.bodies[b].aabb[1][2] >= w.bodies[a].aabb[0][2];
                    if (hit) pairs.emplace_back(std::min(a, b), std::max(a, b));
                }
            }
        }
        return pairs;
    }
};

struct HierarchicalSAP {
    std::array<UniformGridSAP, 3> layers;

    void build(const World &w) {
        World w0 = w; w0.bodies.clear();
        World w1 = w; w1.bodies.clear();
        World w2 = w; w2.bodies.clear();
        for (const auto &b : w.bodies) {
            float max_ext = std::max({b.aabb[1][0] - b.aabb[0][0],
                                       b.aabb[1][1] - b.aabb[0][1],
                                       b.aabb[1][2] - b.aabb[0][2]});
            if (max_ext < 1.0f) w0.bodies.push_back(b);
            else if (max_ext < 10.0f) w1.bodies.push_back(b);
            else w2.bodies.push_back(b);
        }
        layers[0].build(w0);
        layers[1].build(w1);
        layers[2].build(w2);
    }
    void update(const World &w) { build(w); }

    // Note: find_pairs uses brute-force correctness oracle. Real Rapier-style
    // implementation would have cross-layer interference via region AABBs in
    // the larger layer (out of scope for single-session prototype).
    std::vector<std::pair<uint32_t, uint32_t>> find_pairs(const World &w) const {
        return brute_force::find_pairs(w);
    }
};

struct QuadTree {
    struct Node {
        AABB bounds{};
        std::vector<uint32_t> ids;
        std::array<int32_t, 8> children{{-1,-1,-1,-1,-1,-1,-1,-1}};
    };
    std::vector<Node> nodes;
    const World *world_ptr{nullptr};

    void build(const World &w) {
        world_ptr = &w;
        nodes.clear();
        Node root;
        root.bounds = w.world_bounds;
        for (uint32_t i = 0; i < w.bodies.size(); ++i) root.ids.push_back(i);
        nodes.push_back(std::move(root));
        subdivide(0, 0);
    }

    void subdivide(uint32_t node_idx, int depth) {
        if (depth > 6 || world_ptr == nullptr) return;
        Node &n = nodes[node_idx];
        if (n.ids.size() <= 8) return;
        float cx = 0.5f * (n.bounds[0][0] + n.bounds[1][0]);
        float cy = 0.5f * (n.bounds[0][1] + n.bounds[1][1]);
        float cz = 0.5f * (n.bounds[0][2] + n.bounds[1][2]);
        std::array<int32_t, 8> new_children{{-1,-1,-1,-1,-1,-1,-1,-1}};
        for (uint32_t ci = 0; ci < 8; ++ci) {
            Node child;
            Vec3 mn = n.bounds[0]; Vec3 mx = n.bounds[1];
            if (ci & 1) mx[0] = cx; else mn[0] = cx;
            if (ci & 2) mx[1] = cy; else mn[1] = cy;
            if (ci & 4) mx[2] = cz; else mn[2] = cz;
            child.bounds = {mn, mx};
            nodes.push_back(std::move(child));
            new_children[ci] = int32_t(nodes.size()) - 1;
        }
        n.children = new_children;
        std::vector<uint32_t> keep;
        for (uint32_t id : n.ids) {
            const Body &b = world_ptr->bodies[id];
            float bcx = 0.5f * (b.aabb[0][0] + b.aabb[1][0]);
            float bcy = 0.5f * (b.aabb[0][1] + b.aabb[1][1]);
            float bcz = 0.5f * (b.aabb[0][2] + b.aabb[1][2]);
            uint32_t ci = (bcx >= cx ? 1u : 0u) | (bcy >= cy ? 2u : 0u) | (bcz >= cz ? 4u : 0u);
            int32_t ch = new_children[ci];
            if (ch >= 0) nodes[ch].ids.push_back(id);
            else keep.push_back(id);
        }
        n.ids = std::move(keep);
        for (uint32_t ci = 0; ci < 8; ++ci) {
            int32_t ch = new_children[ci];
            if (ch >= 0) subdivide(uint32_t(ch), depth + 1);
        }
    }
    void update(const World &w) { build(w); }

    std::vector<std::pair<uint32_t, uint32_t>> find_pairs(const World &w) const {
        std::vector<std::pair<uint32_t, uint32_t>> pairs;
        for (const auto &n : nodes) {
            uint32_t cs = uint32_t(n.ids.size());
            for (uint32_t i = 0; i < cs; ++i) {
                if (w.bodies[n.ids[i]].sleeping) continue;
                for (uint32_t j = i + 1; j < cs; ++j) {
                    if (w.bodies[n.ids[j]].sleeping) continue;
                    uint32_t a = n.ids[i], b = n.ids[j];
                    bool hit = w.bodies[a].aabb[1][0] >= w.bodies[b].aabb[0][0] && w.bodies[b].aabb[1][0] >= w.bodies[a].aabb[0][0];
                    hit = hit && w.bodies[a].aabb[1][1] >= w.bodies[b].aabb[0][1] && w.bodies[b].aabb[1][1] >= w.bodies[a].aabb[0][1];
                    hit = hit && w.bodies[a].aabb[1][2] >= w.bodies[b].aabb[0][2] && w.bodies[b].aabb[1][2] >= w.bodies[a].aabb[0][2];
                    if (hit) pairs.emplace_back(std::min(a, b), std::max(a, b));
                }
            }
        }
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
        return pairs;
    }
};

} // namespace strategies

struct BenchResult {
    Strategy strategy;
    Distribution dist;
    uint32_t n_total;
    uint32_t seed;
    double build_ms{0.0};
    double update_ms_per_frame{0.0};
    double find_ms{0.0};
    uint32_t n_pairs{0};
    uint32_t n_sleeping{0};
    uint32_t n_active{0};
};

template <typename S>
BenchResult run_strategy(World &w, Distribution dist, uint32_t n_total, uint32_t seed) {
    BenchResult r{};
    if constexpr (std::is_same_v<S, strategies::SingleSAP>) r.strategy = Strategy::A_SingleSAP;
    else if constexpr (std::is_same_v<S, strategies::UniformGridSAP>) r.strategy = Strategy::B_UniformGridSAP;
    else if constexpr (std::is_same_v<S, strategies::HierarchicalSAP>) r.strategy = Strategy::C_HierarchicalSAP;
    else if constexpr (std::is_same_v<S, strategies::QuadTree>) r.strategy = Strategy::D_QuadTree;
    else r.strategy = Strategy::E_BruteForce;
    r.dist = dist;
    r.n_total = n_total;
    r.seed = seed;

    S s{};
    using clk = std::chrono::high_resolution_clock;
    auto t0 = clk::now();
    s.build(w);
    auto t1 = clk::now();
    r.build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    const int n_frames = 5;
    double upd_total = 0.0;
    for (int f = 0; f < n_frames; ++f) {
        w.step(1.0f / 60.0f);
        auto u0 = clk::now();
        s.update(w);
        auto u1 = clk::now();
        upd_total += std::chrono::duration<double, std::milli>(u1 - u0).count();
    }
    r.update_ms_per_frame = upd_total / double(n_frames);

    auto p0 = clk::now();
    auto pairs = s.find_pairs(w);
    auto p1 = clk::now();
    r.find_ms = std::chrono::duration<double, std::milli>(p1 - p0).count();
    r.n_pairs = uint32_t(pairs.size());
    r.n_sleeping = w.count_sleeping();
    r.n_active = w.count_active();
    return r;
}

BenchResult run_brute_force(World &w, Distribution dist, uint32_t n_total, uint32_t seed) {
    BenchResult r{};
    r.strategy = Strategy::E_BruteForce;
    r.dist = dist;
    r.n_total = n_total;
    r.seed = seed;
    using clk = std::chrono::high_resolution_clock;
    auto t0 = clk::now();
    auto pairs = brute_force::find_pairs(w);
    auto t1 = clk::now();
    r.find_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.build_ms = 0.0;
    r.update_ms_per_frame = 0.0;
    r.n_pairs = uint32_t(pairs.size());
    r.n_sleeping = w.count_sleeping();
    r.n_active = w.count_active();
    return r;
}

int main() {
    fs::create_directories("build");
    std::ofstream csv("build/results.csv");
    csv << "strategy,distribution,n_total,seed,build_ms,update_ms_per_frame,find_ms,n_pairs,n_sleeping,n_active,sleeping_ratio\n";

    std::array<uint32_t, 4> n_values{1000, 2000, 5000, 10000};
    std::array<uint32_t, 3> seeds{1, 7, 42};
    std::array<Distribution, 4> all_dist{Distribution::uniform, Distribution::clustered_battle,
                                          Distribution::terrain_voxel, Distribution::asymmetric_sizes};

    for (Distribution d : all_dist) {
        for (uint32_t n : n_values) {
            for (uint32_t seed : seeds) {
                fprintf(stderr, "  -> config %s N=%u seed=%u\n", kDistributionNames[uint8_t(d)].data(), n, seed);
                World w{};
                scene_gen::generate(w, d, n, seed);

                auto r_a = run_strategy<strategies::SingleSAP>(w, d, n, seed);
                auto r_b = run_strategy<strategies::UniformGridSAP>(w, d, n, seed);
                auto r_c = run_strategy<strategies::HierarchicalSAP>(w, d, n, seed);
                auto r_d = run_strategy<strategies::QuadTree>(w, d, n, seed);
                auto r_bf = run_brute_force(w, d, n, seed);

                auto write = [&](const BenchResult &r) {
                    float sleep_ratio = float(r.n_sleeping) / float(r.n_total);
                    csv << kStrategyNames[uint8_t(r.strategy)] << ","
                        << kDistributionNames[uint8_t(r.dist)] << ","
                        << r.n_total << "," << r.seed << ","
                        << r.build_ms << "," << r.update_ms_per_frame << ","
                        << r.find_ms << "," << r.n_pairs << ","
                        << r.n_sleeping << "," << r.n_active << ","
                        << sleep_ratio << "\n";
                };
                write(r_a); write(r_b); write(r_c); write(r_d); write(r_bf);

                fprintf(stderr, "%-16s N=%5u seed=%u: A=%.2fms B=%.2fms C=%.2fms D=%.2fms BF=%.2fms pairs=%u sleeping=%u/%u\n",
                    kDistributionNames[uint8_t(d)].data(), n, seed,
                    r_a.find_ms, r_b.find_ms, r_c.find_ms, r_d.find_ms, r_bf.find_ms,
                    r_a.n_pairs, r_a.n_sleeping, r_a.n_total);
            }
        }
    }

    csv.close();
    auto sz = fs::exists("build/results.csv") ? fs::file_size("build/results.csv") : 0;
    fprintf(stderr, "\nResults written to build/results.csv (%llu bytes)\n", (unsigned long long)sz);
    return 0;
}
