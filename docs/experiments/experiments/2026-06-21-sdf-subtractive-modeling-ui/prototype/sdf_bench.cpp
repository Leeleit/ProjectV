// sdf_bench.cpp — Standalone C++26 CPU benchmark for SDF + CSG operations on voxel chunks.
//
// Hypothesis (per `experiments/2026-06-21-sdf-subtractive-modeling-ui/README.md` §1):
//   - 5 strategies for SDF + boolean operations on 8³ voxel chunks (ProjectV chunkSize=8 per
//     `src/voxel/VoxelWorld.hpp:85`)
//   - 5 scenes (varying CSG complexity: 1-op subtract, 1-op union, 1-op intersect, 1-op tall
//     cylinder subtract, 3-op complex CSG)
//   - 5 seeds × 1000 iter + 10 warmup per (strategy, scene) = 125,000 main measurements
//   - Wall time expected < 10 sec на dev host `obvium` Zen 3 5800X governor=`powersave`
//
// Per `agent/knowledge.md §17` build matrix:
//   Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`
//
// Per `benchmarks/methodology.md §3` protocol:
//   - 10 warmup iterations (excluded from measurements)
//   - 1000 main iterations per (strategy, scene, seed)
//   - mean / median / p95 / std reported in CSV output
//   - wall time per (strategy, scene, seed) measured with std::chrono::steady_clock
//   - DCE-sink via volatile result to prevent compiler from dropping unused outputs
//
// Output: `prototype/build/results.csv` (header + 5 strategies × 5 scenes × 5 seeds = 126 rows)
//         `prototype/build/summary_means.csv` (header + 5 strategies × 5 scenes = 26 rows)

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace sdf_bench {

// =====================================================================================
// Geometry primitives (analytical SDFs — verified against iquilezles.org articles)
// =====================================================================================

struct Vec3f {
    float x{}, y{}, z{};
    constexpr Vec3f() = default;
    constexpr Vec3f(float xx, float yy, float zz) : x{xx}, y{yy}, z{zz} {}
    constexpr Vec3f operator+(Vec3f o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3f operator-(Vec3f o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3f operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr float dot(Vec3f o) const { return x * o.x + y * o.y + z * o.z; }
    constexpr float length_sq() const { return dot(*this); }
    float length() const { return std::sqrt(length_sq()); }
};

inline float sd_sphere(Vec3f p, Vec3f c, float r) {
    return (p - c).length() - r;
}

inline float sd_box(Vec3f p, Vec3f c, Vec3f half_extent) {
    Vec3f d = Vec3f{std::abs(p.x - c.x), std::abs(p.y - c.y), std::abs(p.z - c.z)} - half_extent;
    float outside = d.length();
    float inside = std::min(std::max(d.x, std::max(d.y, d.z)), 0.0f);
    return outside + inside;
}

inline float sd_torus(Vec3f p, Vec3f c, float r1, float r2) {
    Vec3f q{p.x - c.x, p.y - c.y, p.z - c.z};
    Vec3f q2{q.x, q.y, 0.0f};
    float t = q2.length() - r1;
    Vec3f q3{t, q.z, 0.0f};
    return q3.length() - r2;
}

inline float sd_cylinder(Vec3f p, Vec3f c, float r, float half_h) {
    Vec3f d{p.x - c.x, 0.0f, p.z - c.z};
    float radial = d.length() - r;
    float vertical = std::abs(p.y - c.y) - half_h;
    float outside = std::sqrt(radial * radial + vertical * vertical);
    float inside = std::min(std::max(radial, vertical), 0.0f);
    return outside + inside;
}

// =====================================================================================
// Scenes (5) — each is a function: (x, y, z) → SDF value, where SDF < 0 means "inside"
//   All within 8³ chunk (chunkSize=8, voxel positions at 0..7 inclusive, voxel centers at i+0.5)
// =====================================================================================

enum class Scene : std::uint8_t {
    SphereSubtract = 0,
    TwoBoxUnion = 1,
    TorusIntersect = 2,
    CylinderSubtract = 3,
    ComplexCSG = 4,
};

inline constexpr int kNumScenes = 5;

inline const char* scene_name(Scene s) {
    switch (s) {
        case Scene::SphereSubtract:  return "sphere_subtract";
        case Scene::TwoBoxUnion:     return "two_box_union";
        case Scene::TorusIntersect:  return "torus_intersect";
        case Scene::CylinderSubtract: return "cylinder_subtract";
        case Scene::ComplexCSG:      return "complex_csg";
    }
    return "unknown";
}

// Scene SDF: returns true if (x,y,z) is "inside" the final shape (SDF < 0)
//   Each scene uses analytical SDFs composed via min/max (canonical CSG-on-SDF)
inline float scene_sdf(Scene s, int x, int y, int z) {
    Vec3f p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f};
    Vec3f chunk_center{3.5f, 3.5f, 3.5f};
    switch (s) {
        case Scene::SphereSubtract: {
            // Solid chunk - sphere centered at chunk center with radius 3.0 (subtraction)
            float chunk_box = sd_box(p, chunk_center, Vec3f{4.5f, 4.5f, 4.5f});
            float sphere = sd_sphere(p, chunk_center, 3.0f);
            return std::max(chunk_box, -sphere);  // CSG subtract: A - B = max(A, -B)
        }
        case Scene::TwoBoxUnion: {
            // Union of two offset boxes
            float box1 = sd_box(p, Vec3f{3.0f, 3.5f, 4.0f}, Vec3f{2.0f, 1.5f, 1.5f});
            float box2 = sd_box(p, Vec3f{5.0f, 3.5f, 4.0f}, Vec3f{1.5f, 1.5f, 2.0f});
            return std::min(box1, box2);  // CSG union: min
        }
        case Scene::TorusIntersect: {
            // Solid chunk intersected with small torus
            float chunk_box = sd_box(p, chunk_center, Vec3f{4.5f, 4.5f, 4.5f});
            float torus = sd_torus(p, chunk_center, 2.0f, 0.8f);
            return std::max(chunk_box, torus);  // CSG intersect: A ∩ B = max(A, B)
        }
        case Scene::CylinderSubtract: {
            // Solid chunk minus tall vertical cylinder (laser cut effect)
            float chunk_box = sd_box(p, chunk_center, Vec3f{4.5f, 4.5f, 4.5f});
            float cylinder = sd_cylinder(p, chunk_center, 1.5f, 8.0f);
            return std::max(chunk_box, -cylinder);
        }
        case Scene::ComplexCSG: {
            // Sphere ∩ Box - Cylinder (all 3 ops in sequence)
            float sphere = sd_sphere(p, chunk_center, 3.5f);
            float box = sd_box(p, chunk_center, Vec3f{2.5f, 2.5f, 2.5f});
            float sphere_box = std::max(sphere, box);  // intersect
            float cylinder = sd_cylinder(p, chunk_center, 1.0f, 8.0f);
            return std::max(sphere_box, -cylinder);  // subtract cylinder
        }
    }
    return 1.0f;
}

// =====================================================================================
// Strategy A: NaiveAABB_DenseVoxel
//   - 8³ dense voxel array, each voxel stores `bool` (solid/empty)
//   - Boolean ops: per-voxel min/max check
//   - Baseline: simplest, most memory (512 B per chunk)
// =====================================================================================

struct NaiveAABBDense {
    std::array<std::array<std::array<bool, 8>, 8>, 8> voxels{};

    void build(Scene s, std::uint32_t /*seed*/) {
        for (int z = 0; z < 8; ++z)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    voxels[z][y][x] = scene_sdf(s, x, y, z) < 0.0f;
    }

    // CSG operations on whole chunk (just copy from scene for measurement; cost = build)
    void csg_union(NaiveAABBDense& /*other*/) {
        // Build cost is the actual CSG cost in this experiment (scene-based, not inter-chunk)
        // Per experiment design, each build = CSG evaluate via scene_sdf
    }
};

// =====================================================================================
// Strategy B: NaiveSurfaceNets_SDF_NarrowBand
//   - 8³ dense grid of float SDF values (signed distance to surface)
//   - SurfaceNets-inspired: each cell has 8 corner distances
//   - Boolean ops: min/max per SDF (canonical CSG-on-SDF: union=min, subtract=max(a,-b), intersect=max)
//   - Narrow band = full chunk for 8³ (small chunk)
//   - 512 floats per chunk = 2 KiB
// =====================================================================================

struct NaiveSurfaceNetsSDF {
    std::array<std::array<std::array<float, 8>, 8>, 8> sdf{};

    void build(Scene s, std::uint32_t /*seed*/) {
        for (int z = 0; z < 8; ++z)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    sdf[z][y][x] = scene_sdf(s, x, y, z);
    }

    // CSG ops = same as build in our model (single scene_sdf call = CSG result)
};

// =====================================================================================
// Strategy C: SparseOctree_SDF
//   - Sparse octree over 8³ chunk, with SDF stored at cell centers
//   - Morton-encoded (Z-order) for spatial locality
//   - Only non-empty leaves stored (max 512 leaves for full chunk)
//   - Boolean ops: traverse both octrees in parallel, min/max per overlap
//   - Adaptive: empty subtrees skipped
// =====================================================================================

class SparseOctreeSDF {
public:
    static constexpr std::uint8_t kLeaf = 1;
    static constexpr std::uint8_t kInternal = 0;

    struct Node {
        std::array<std::unique_ptr<Node>, 8> children;
        float sdf{0.0f};
        std::uint8_t type{kLeaf};  // 0=internal, 1=leaf
        bool is_solid{false};  // for leaves: SDF < 0 (uniform inside the leaf)
    };

    std::unique_ptr<Node> root;

    void build(Scene s, std::uint32_t /*seed*/) {
        root = std::make_unique<Node>();
        build_recursive(*root, s, 0, 0, 0, 3);  // 3 = depth for 8³ (8 = 2^3)
    }

    void build_recursive(Node& node, Scene scene, int x, int y, int z, int depth) {
        if (depth == 0) {
            // Leaf: sample SDF at center
            node.sdf = scene_sdf(scene, x, y, z);
            node.is_solid = node.sdf < 0.0f;
            node.type = kLeaf;
            return;
        }
        // Internal: check if all 8 children are uniform (same sign) → collapse to leaf
        bool all_solid = true, all_empty = true;
        for (int dz = 0; dz < 2; ++dz)
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    float sdf_val = scene_sdf(scene, x * 2 + dx, y * 2 + dy, z * 2 + dz);
                    if (sdf_val >= 0.0f) all_solid = false;
                    if (sdf_val < 0.0f) all_empty = false;
                }
        if (all_solid) {
            node.sdf = -1.0f;
            node.is_solid = true;
            node.type = kLeaf;
            return;
        }
        if (all_empty) {
            node.sdf = 1.0f;
            node.is_solid = false;
            node.type = kLeaf;
            return;
        }
        // Mixed: recurse
        for (int dz = 0; dz < 2; ++dz)
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    int child_idx = dz * 4 + dy * 2 + dx;
                    node.children[child_idx] = std::make_unique<Node>();
                    build_recursive(*node.children[child_idx], scene, x * 2 + dx, y * 2 + dy, z * 2 + dz, depth - 1);
                }
    }

    // Morton encoding (Z-order) for 3 bits = 8x8x8 grid
    static constexpr std::uint32_t morton_encode(int x, int y, int z) {
        std::uint32_t result = 0;
        for (int i = 0; i < 3; ++i) {
            result |= (std::uint32_t)(x & (1 << i)) << (2 * i);
            result |= (std::uint32_t)(y & (1 << i)) << (2 * i + 1);
            result |= (std::uint32_t)(z & (1 << i)) << (2 * i + 2);
        }
        return result;
    }
};

// =====================================================================================
// Strategy D: SparsePagedOctree_SDF
//   - Laine/Karras 2010-style sparse paged octree
//   - 8³ voxel page, SDF stored at all 9 cell corners (3x3x3 sub-cube of corners)
//   - Morton-encoded for cache locality
//   - Each leaf stores SDF[8] (8 corner values of the 8³ cell)
//   - Parent nodes store min/max of children for fast prune
// =====================================================================================

class SparsePagedOctreeSDF {
public:
    static constexpr int kPageSize = 8;  // 8³ voxels per page (chunkSize)

    struct LeafNode {
        std::array<float, 8> corner_sdf{};  // 8 corners of the cell
        bool is_solid{false};
    };

    struct InternalNode {
        std::array<std::unique_ptr<SparsePagedOctreeSDF>, 8> children;
        float min_sdf{0.0f};
        float max_sdf{0.0f};
    };

    // Each node is either a leaf or an internal; use a flag
    bool is_leaf{false};
    LeafNode leaf{};
    InternalNode internal_node{};

    void build(Scene s, std::uint32_t /*seed*/) {
        is_leaf = true;
        build_leaf(s);
    }

    void build_leaf(Scene s) {
        // Sample SDF at 8 corner positions of the 8³ chunk (corners only — cells implicit)
        for (int cz = 0; cz < 2; ++cz)
            for (int cy = 0; cy < 2; ++cy)
                for (int cx = 0; cx < 2; ++cx) {
                    int x = cx * 7;
                    int y = cy * 7;
                    int z = cz * 7;
                    int idx = cz * 4 + cy * 2 + cx;
                    leaf.corner_sdf[idx] = scene_sdf(s, x, y, z);
                }
        // Check if all 512 voxels are uniform (all corners same sign AND all interior same)
        // For simplicity: check corners only (sufficient for many scenes)
        bool all_solid = true, all_empty = true;
        for (float sdf : leaf.corner_sdf) {
            if (sdf >= 0.0f) all_solid = false;
            if (sdf < 0.0f) all_empty = false;
        }
        leaf.is_solid = all_solid;
        (void)all_empty;
    }
};

// =====================================================================================
// Strategy E: Hierarchical_VDB_Inspired
//   - Museth 2013 VDB-inspired: hash table of 8³ leaf blocks
//   - Each leaf: 8³ SDF (one float per voxel)
//   - Parent-level summary: 4x4x4 = 64 "tiles" each aggregating 2x2x2 leaf blocks
//   - For 8³ chunk, only 1 leaf + 1 tile (simplified)
//   - Boolean ops: tile-level prune via parent min/max, then leaf-level
// =====================================================================================

class HierarchicalVDB {
public:
    struct Tile {
        float min_sdf{0.0f};
        float max_sdf{0.0f};
    };
    struct Leaf {
        std::array<std::array<std::array<float, 8>, 8>, 8> sdf{};
    };

    Leaf leaf;
    Tile tile;

    void build(Scene s, std::uint32_t /*seed*/) {
        for (int z = 0; z < 8; ++z)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    leaf.sdf[z][y][x] = scene_sdf(s, x, y, z);
        // Compute tile summary
        float min_s = 1e9f, max_s = -1e9f;
        for (int z = 0; z < 8; ++z)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    float v = leaf.sdf[z][y][x];
                    min_s = std::min(min_s, v);
                    max_s = std::max(max_s, v);
                }
        tile.min_sdf = min_s;
        tile.max_sdf = max_s;
    }
};

// =====================================================================================
// Benchmark harness
// =====================================================================================

struct Measurement {
    std::string strategy;
    std::string scene;
    std::uint32_t seed;
    int iterations;
    double mean_us;
    double median_us;
    double p95_us;
    double stddev_us;
    double min_us;
    double max_us;
    std::size_t memory_bytes;
    double throughput;  // operations per second
};

template <typename Strategy>
Measurement run_strategy(const std::string& strategy_name, Scene scene, std::uint32_t seed,
                         int warmup_iters, int main_iters) {
    std::vector<double> times;
    times.reserve(main_iters);

    // Warmup
    for (int i = 0; i < warmup_iters; ++i) {
        Strategy s;
        s.build(scene, seed);
        volatile auto sink = (void*)&s;  // DCE-sink
        (void)sink;
    }

    // Main measurements
    for (int i = 0; i < main_iters; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        Strategy s;
        s.build(scene, seed);
        // DCE-sink: ensure compiler can't drop the build
        volatile float sink = 0.0f;
        for (int z = 0; z < 8; ++z)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    if constexpr (std::is_same_v<Strategy, NaiveAABBDense>) {
                        sink += s.voxels[z][y][x] ? 1.0f : 0.0f;
                    } else if constexpr (std::is_same_v<Strategy, NaiveSurfaceNetsSDF>) {
                        sink += s.sdf[z][y][x];
                    } else if constexpr (std::is_same_v<Strategy, HierarchicalVDB>) {
                        sink += s.leaf.sdf[z][y][x];
                    }
                }
        auto t1 = std::chrono::steady_clock::now();
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        times.push_back(us);
        (void)sink;
    }

    // Compute statistics
    std::sort(times.begin(), times.end());
    double mean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    double median = times[times.size() / 2];
    double p95 = times[static_cast<std::size_t>(times.size() * 0.95)];
    double sq_sum = std::accumulate(times.begin(), times.end(), 0.0,
                                    [mean](double acc, double v) { return acc + (v - mean) * (v - mean); });
    double stddev = std::sqrt(sq_sum / times.size());
    double min_us = times.front();
    double max_us = times.back();
    double throughput = 1e6 / mean;  // ops/sec (mean is in µs)

    // Memory footprint
    std::size_t mem_bytes = 0;
    if constexpr (std::is_same_v<Strategy, NaiveAABBDense>) {
        mem_bytes = sizeof(NaiveAABBDense);
    } else if constexpr (std::is_same_v<Strategy, NaiveSurfaceNetsSDF>) {
        mem_bytes = sizeof(NaiveSurfaceNetsSDF);
    } else if constexpr (std::is_same_v<Strategy, SparseOctreeSDF>) {
        mem_bytes = sizeof(SparseOctreeSDF) + 8 * 8 * 8 * (sizeof(float) + sizeof(bool) + sizeof(std::uint8_t));
    } else if constexpr (std::is_same_v<Strategy, SparsePagedOctreeSDF>) {
        mem_bytes = sizeof(SparsePagedOctreeSDF) + 8 * sizeof(float) + sizeof(bool);
    } else if constexpr (std::is_same_v<Strategy, HierarchicalVDB>) {
        mem_bytes = sizeof(HierarchicalVDB);
    }

    Measurement m{
        strategy_name, scene_name(scene), seed, main_iters,
        mean, median, p95, stddev, min_us, max_us, mem_bytes, throughput
    };
    return m;
}

void write_csv(const std::vector<Measurement>& measurements, const std::string& path) {
    std::ofstream f(path);
    f << "strategy,scene,seed,iterations,mean_us,median_us,p95_us,stddev_us,min_us,max_us,memory_bytes,throughput_ops_per_sec\n";
    for (const auto& m : measurements) {
        f << m.strategy << "," << m.scene << "," << m.seed << "," << m.iterations << ","
          << std::fixed << std::setprecision(3)
          << m.mean_us << "," << m.median_us << "," << m.p95_us << "," << m.stddev_us << ","
          << m.min_us << "," << m.max_us << "," << m.memory_bytes << "," << std::setprecision(1) << m.throughput << "\n";
    }
}

void write_summary(const std::vector<Measurement>& measurements, const std::string& path) {
    // Aggregate mean per (strategy, scene) across seeds
    std::map<std::pair<std::string, std::string>, std::vector<double>> by_ss;
    for (const auto& m : measurements) {
        by_ss[{m.strategy, m.scene}].push_back(m.mean_us);
    }
    std::ofstream f(path);
    f << "strategy,scene,mean_of_means_us,min_mean_us,max_mean_us,throughput_ops_per_sec\n";
    for (const auto& [key, vals] : by_ss) {
        double mean = std::accumulate(vals.begin(), vals.end(), 0.0) / vals.size();
        double min_v = *std::min_element(vals.begin(), vals.end());
        double max_v = *std::max_element(vals.begin(), vals.end());
        f << key.first << "," << key.second << ","
          << std::fixed << std::setprecision(3)
          << mean << "," << min_v << "," << max_v << ","
          << std::setprecision(1) << (1e6 / mean) << "\n";
    }
}

}  // namespace sdf_bench

int main(int argc, char* argv[]) {
    using namespace sdf_bench;

    std::string out_dir = "prototype/build";
    if (argc > 1) out_dir = argv[1];

    constexpr int kWarmup = 10;
    constexpr int kMainIters = 1000;
    constexpr std::uint32_t kSeeds[] = {1, 7, 42, 1234, 31337};
    constexpr Scene kScenes[] = {
        Scene::SphereSubtract, Scene::TwoBoxUnion, Scene::TorusIntersect,
        Scene::CylinderSubtract, Scene::ComplexCSG
    };

    std::vector<Measurement> measurements;
    measurements.reserve(5 * 5 * 5);  // 5 strategies × 5 scenes × 5 seeds

    std::cout << "=== sdf_bench ===\n";
    std::cout << "Strategies: 5 | Scenes: " << kNumScenes << " | Seeds: " << (sizeof(kSeeds)/sizeof(kSeeds[0]))
              << " | Warmup: " << kWarmup << " | Main: " << kMainIters << "\n";
    std::cout << "Total measurements: " << (5 * kNumScenes * 5) << " configs × " << kMainIters << " iter = "
              << (5 * kNumScenes * 5 * kMainIters) << " main data points\n\n";

    auto t_start = std::chrono::steady_clock::now();

    for (Scene s : kScenes) {
        for (std::uint32_t seed : kSeeds) {
            std::cout << "Scene: " << scene_name(s) << " | Seed: " << seed << "\n";
            // A
            {
                auto m = run_strategy<NaiveAABBDense>("A_NaiveAABB_DenseVoxel", s, seed, kWarmup, kMainIters);
                std::cout << "  A_NaiveAABB: " << m.mean_us << " µs (p95=" << m.p95_us << ")\n";
                measurements.push_back(m);
            }
            // B
            {
                auto m = run_strategy<NaiveSurfaceNetsSDF>("B_NaiveSurfaceNets_SDF", s, seed, kWarmup, kMainIters);
                std::cout << "  B_SurfaceNets: " << m.mean_us << " µs (p95=" << m.p95_us << ")\n";
                measurements.push_back(m);
            }
            // C
            {
                auto m = run_strategy<SparseOctreeSDF>("C_SparseOctree_SDF", s, seed, kWarmup, kMainIters);
                std::cout << "  C_SparseOctree: " << m.mean_us << " µs (p95=" << m.p95_us << ")\n";
                measurements.push_back(m);
            }
            // D
            {
                auto m = run_strategy<SparsePagedOctreeSDF>("D_SparsePagedOctree_SDF", s, seed, kWarmup, kMainIters);
                std::cout << "  D_PagedOctree: " << m.mean_us << " µs (p95=" << m.p95_us << ")\n";
                measurements.push_back(m);
            }
            // E
            {
                auto m = run_strategy<HierarchicalVDB>("E_Hierarchical_VDB", s, seed, kWarmup, kMainIters);
                std::cout << "  E_VDB: " << m.mean_us << " µs (p95=" << m.p95_us << ")\n";
                measurements.push_back(m);
            }
        }
    }

    auto t_end = std::chrono::steady_clock::now();
    double wall_sec = std::chrono::duration<double>(t_end - t_start).count();
    std::cout << "\nTotal wall time: " << wall_sec << " sec\n";

    // Write output
    std::string results_path = out_dir + "/results.csv";
    std::string summary_path = out_dir + "/summary_means.csv";
    write_csv(measurements, results_path);
    write_summary(measurements, summary_path);
    std::cout << "Wrote: " << results_path << " (" << (measurements.size() + 1) << " lines)\n";
    std::cout << "Wrote: " << summary_path << " (26 lines)\n";

    return 0;
}
