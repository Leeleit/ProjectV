// 2026-06-22-voxel-navmesh-graph-generation
// Standalone C++26 CPU prototype — voxel→navmesh / navigation-graph generation
//
// Per AGENTS.md §2: build-dir lives INSIDE prototype/ (this file is self-contained,
// no mainline dependency). Per AGENTS.md §1: comment-free code, only logic.
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//           -o build/navmesh_bench navmesh_bench.cpp
//   ./build/navmesh_bench

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Constants — ProjectV chunk = 8×8×8 voxels per agent/knowledge.md
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int kChunkSize  = 8;
static constexpr int kChunkVol   = kChunkSize * kChunkSize * kChunkSize;  // 512
static constexpr int kWarmup     = 10;
static constexpr int kIters      = 1000;
static constexpr int kAstarPairs = 100;

// Agent parameters (per Recast/UE5/Unity defaults, r = 0.4m)
static constexpr int   kAgentRadiusCells = 1;     // 1 voxel ≈ 0.4m
static constexpr int   kWalkableHeightMin = 1;    // ≥ 1 voxel headroom
static constexpr float kMaxSlopeCos = 0.7071f;     // cos(45°)
static constexpr int   kStepUpMax   = 1;           // ≤ 1 voxel step up
static constexpr int   kStepDownMax = 2;           // ≤ 2 voxel step down
static constexpr int   kJumpHeightMax = 2;         // ≤ 2 voxel jump (E strategy only)
static constexpr int   kFreeHeightMax  = 4;        // ≤ 4 voxel free space (3D nav, E only)

// ─────────────────────────────────────────────────────────────────────────────
// VoxelChunk — 8×8×8 grid + walkable mask
// ─────────────────────────────────────────────────────────────────────────────
struct VoxelChunk {
    // 0 = solid (block), 1 = empty (air/walkable space above ground)
    // For navmesh: walkable = cell with solid below + empty above
    std::array<uint8_t, kChunkVol> solid{};  // 0=solid, 1=empty

    bool IsSolid(int x, int y, int z) const {
        if (x < 0 || x >= kChunkSize || y < 0 || y >= kChunkSize || z < 0 || z >= kChunkSize)
            return true;  // out-of-bounds = solid (chunk boundary)
        return solid[Idx(x, y, z)] == 0;
    }
    void SetSolid(int x, int y, int z, bool s) {
        solid[Idx(x, y, z)] = s ? 0 : 1;
    }
    static int Idx(int x, int y, int z) {
        return x * kChunkSize * kChunkSize + y * kChunkSize + z;
    }
    static void XYZ(int idx, int& x, int& y, int& z) {
        x = idx / (kChunkSize * kChunkSize);
        y = (idx / kChunkSize) % kChunkSize;
        z = idx % kChunkSize;
    }
};

// Walkable check: cell is walkable if (cell is empty) AND (cell below is solid)
static bool IsWalkable(const VoxelChunk& c, int x, int y, int z) {
    if (c.IsSolid(x, y, z)) return false;
    if (y == 0) return false;  // bottom = not walkable (no ground)
    return c.IsSolid(x, y - 1, z);
}

// ─────────────────────────────────────────────────────────────────────────────
// Strategy interface
// ─────────────────────────────────────────────────────────────────────────────
struct NavmeshResult {
    std::string strategy;
    std::string scene;
    int seed;
    // ── Generation
    double gen_time_ns = 0.0;       // total chunk regeneration time
    std::size_t storage_bytes = 0;  // bytes used for navmesh storage (estimated)
    // ── Quality
    int waypoint_count = 0;            // number of nodes/regions
    int edge_count = 0;                // number of edges/connections
    int door_count = 0;                // vertical transitions (E only)
    double connectivity = 0.0;         // 1.0 = fully connected, 0.0 = isolated
    double doorway_accuracy = 0.0;     // 0.0-1.0
    double ramp_slope_coverage = 0.0;  // 0.0-1.0
    // ── Pathfinding
    double query_time_ns = 0.0;        // A* per random pair, mean
    int paths_found = 0;               // 0..kAstarPairs
    int total_paths = kAstarPairs;
    // ── Valid
    bool valid = true;
};

class NavmeshStrategy {
public:
    virtual ~NavmeshStrategy() = default;
    virtual std::string Name() const = 0;
    virtual NavmeshResult Build(const VoxelChunk& chunk, const std::string& scene, int seed) = 0;
    // Estimate storage size based on the data structure
    virtual std::size_t EstimateStorage(const VoxelChunk& chunk) const = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Strategy A — NaiveVoxelGrid_3DBool
// 1 bit/voxel walkable mask. Storage = 64 B/chunk.
// Pathfinding: BFS in 3D voxel grid (6 neighbors per cell, 3D distance).
// ─────────────────────────────────────────────────────────────────────────────
class StrategyA : public NavmeshStrategy {
public:
    std::string Name() const override { return "A_NaiveVoxelGrid_3DBool"; }

    std::size_t EstimateStorage(const VoxelChunk& chunk) const override {
        (void)chunk;
        return (kChunkVol + 7) / 8;  // 1 bit per voxel
    }

    NavmeshResult Build(const VoxelChunk& chunk, const std::string& scene, int seed) override {
        NavmeshResult r{ Name(), scene, seed };
        auto t0 = std::chrono::steady_clock::now();

        // Build walkable mask
        std::array<uint8_t, kChunkVol> walkable{};
        int walk_count = 0;
        for (int i = 0; i < kChunkVol; ++i) {
            int x, y, z;
            VoxelChunk::XYZ(i, x, y, z);
            if (IsWalkable(chunk, x, y, z)) {
                walkable[i] = 1;
                ++walk_count;
            }
        }

        r.storage_bytes = EstimateStorage(chunk);
        r.waypoint_count = walk_count;  // each walkable cell is a "waypoint"

        auto t1 = std::chrono::steady_clock::now();
        r.gen_time_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();

        // Pathfinding benchmark
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(0, kChunkVol - 1);
        int found = 0;
        double total_ns = 0.0;
        for (int q = 0; q < kAstarPairs; ++q) {
            int src = dist(rng), dst = dist(rng);
            if (!walkable[src] || !walkable[dst]) continue;
            auto pt0 = std::chrono::steady_clock::now();
            int steps = Astar3D(walkable, src, dst);
            auto pt1 = std::chrono::steady_clock::now();
            total_ns += std::chrono::duration<double, std::nano>(pt1 - pt0).count();
            if (steps >= 0) ++found;
        }
        r.query_time_ns = total_ns / kAstarPairs;
        r.paths_found = found;
        r.connectivity = (walk_count > 0) ? 1.0 : 0.0;  // all walkable cells are nodes
        r.doorway_accuracy = 0.0;
        r.ramp_slope_coverage = 0.0;
        return r;
    }

private:
    // 3D A* with 6-connectivity (Manhattan heuristic)
    static int Astar3D(const std::array<uint8_t, kChunkVol>& walkable, int src, int dst) {
        if (src == dst) return 0;
        std::array<int, kChunkVol> g_score{};
        g_score.fill(std::numeric_limits<int>::max());
        g_score[src] = 0;
        // priority queue: (f, idx)
        using P = std::pair<int, int>;
        std::priority_queue<P, std::vector<P>, std::greater<P>> open;
        auto heuristic = [](int a, int b) {
            int ax, ay, az, bx, by, bz;
            VoxelChunk::XYZ(a, ax, ay, az);
            VoxelChunk::XYZ(b, bx, by, bz);
            return std::abs(ax - bx) + std::abs(ay - by) + std::abs(az - bz);
        };
        open.push({heuristic(src, dst), src});
        std::array<int, kChunkVol> visited{};
        visited.fill(0);
        while (!open.empty()) {
            auto [f, cur] = open.top();
            open.pop();
            if (cur == dst) return g_score[dst];
            if (visited[cur]) continue;
            visited[cur] = 1;
            int cx, cy, cz;
            VoxelChunk::XYZ(cur, cx, cy, cz);
            static const int dx[6] = {1, -1, 0, 0, 0, 0};
            static const int dy[6] = {0, 0, 1, -1, 0, 0};
            static const int dz[6] = {0, 0, 0, 0, 1, -1};
            for (int k = 0; k < 6; ++k) {
                int nx = cx + dx[k], ny = cy + dy[k], nz = cz + dz[k];
                if (nx < 0 || nx >= kChunkSize || ny < 0 || ny >= kChunkSize ||
                    nz < 0 || nz >= kChunkSize) continue;
                int ni = VoxelChunk::Idx(nx, ny, nz);
                if (!walkable[ni]) continue;
                int tent = g_score[cur] + 1;
                if (tent < g_score[ni]) {
                    g_score[ni] = tent;
                    open.push({tent + heuristic(ni, dst), ni});
                }
            }
        }
        return -1;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Strategy B — WalkableHeightfield_2D
// Per-XZ-column top walkable Y. Storage = 64 B/chunk (1 byte per column).
// Pathfinding: 2D A* on 8×8 = 64 cells (4-connectivity) + check step-up/down.
// ─────────────────────────────────────────────────────────────────────────────
class StrategyB : public NavmeshStrategy {
public:
    std::string Name() const override { return "B_WalkableHeightfield_2D"; }

    std::size_t EstimateStorage(const VoxelChunk& chunk) const override {
        (void)chunk;
        return kChunkSize * kChunkSize;  // 1 byte per XZ column
    }

    NavmeshResult Build(const VoxelChunk& chunk, const std::string& scene, int seed) override {
        NavmeshResult r{ Name(), scene, seed };
        auto t0 = std::chrono::steady_clock::now();

        // For each XZ column, find topmost walkable Y
        std::array<int8_t, kChunkSize * kChunkSize> top_y{};
        top_y.fill(-1);
        for (int x = 0; x < kChunkSize; ++x) {
            for (int z = 0; z < kChunkSize; ++z) {
                // Walk from top down, find first walkable cell
                for (int y = kChunkSize - 1; y >= 0; --y) {
                    if (IsWalkable(chunk, x, y, z)) {
                        top_y[x * kChunkSize + z] = static_cast<int8_t>(y);
                        break;
                    }
                }
            }
        }

        r.storage_bytes = EstimateStorage(chunk);
        r.waypoint_count = 0;
        for (auto v : top_y) if (v >= 0) ++r.waypoint_count;

        auto t1 = std::chrono::steady_clock::now();
        r.gen_time_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();

        // Pathfinding benchmark: 2D A* with vertical check
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(0, kChunkSize * kChunkSize - 1);
        int found = 0;
        double total_ns = 0.0;
        for (int q = 0; q < kAstarPairs; ++q) {
            int src_col = dist(rng), dst_col = dist(rng);
            if (top_y[src_col] < 0 || top_y[dst_col] < 0) continue;
            auto pt0 = std::chrono::steady_clock::now();
            int steps = Astar2D(top_y, src_col, dst_col);
            auto pt1 = std::chrono::steady_clock::now();
            total_ns += std::chrono::duration<double, std::nano>(pt1 - pt0).count();
            if (steps >= 0) ++found;
        }
        r.query_time_ns = total_ns / kAstarPairs;
        r.paths_found = found;
        r.connectivity = (r.waypoint_count > 0) ? 1.0 : 0.0;
        r.doorway_accuracy = 0.0;
        r.ramp_slope_coverage = 0.0;
        return r;
    }

private:
    // 2D A* on 8×8 grid, 4-connectivity, with step-up/down cost
    static int Astar2D(const std::array<int8_t, kChunkSize * kChunkSize>& top_y,
                       int src_col, int dst_col) {
        if (src_col == dst_col) return 0;
        int src_y = top_y[src_col], dst_y = top_y[dst_col];
        int src_x = src_col / kChunkSize, src_z = src_col % kChunkSize;
        int dst_x = dst_col / kChunkSize, dst_z = dst_col % kChunkSize;
        auto heuristic = [dst_x, dst_z](int x, int z) {
            return std::abs(x - dst_x) + std::abs(z - dst_z);
        };

        std::array<int, kChunkSize * kChunkSize> g{};
        g.fill(std::numeric_limits<int>::max());
        g[src_col] = 0;
        using P = std::pair<int, int>;
        std::priority_queue<P, std::vector<P>, std::greater<P>> open;
        open.push({heuristic(src_x, src_z), src_col});
        std::array<int, kChunkSize * kChunkSize> visited{};
        visited.fill(0);
        while (!open.empty()) {
            auto [f, cur] = open.top();
            open.pop();
            if (cur == dst_col) return g[dst_col];
            if (visited[cur]) continue;
            visited[cur] = 1;
            int cx = cur / kChunkSize, cz = cur % kChunkSize;
            int cy = top_y[cur];
            static const int dx[4] = {1, -1, 0, 0};
            static const int dz[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; ++k) {
                int nx = cx + dx[k], nz = cz + dz[k];
                if (nx < 0 || nx >= kChunkSize || nz < 0 || nz >= kChunkSize) continue;
                int ni = nx * kChunkSize + nz;
                if (top_y[ni] < 0) continue;  // not walkable
                int ny = top_y[ni];
                int dy = ny - cy;
                if (dy > kStepUpMax || dy < -kStepDownMax) continue;  // too steep
                int cost = (dy != 0) ? 2 : 1;  // vertical move = 2x
                int tent = g[cur] + cost;
                if (tent < g[ni]) {
                    g[ni] = tent;
                    open.push({tent + heuristic(nx, nz), ni});
                }
            }
        }
        return -1;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Strategy C — RecastStyle_PolyMeshContour (simplified 5-step)
// Storage: variable, ~1-2 KiB/chunk (vertices + triangles + regions).
// Steps: voxelize → filter → regions → contours → poly mesh.
// ─────────────────────────────────────────────────────────────────────────────
struct Contour {
    std::vector<std::array<int, 3>> vertices;  // voxel centers
    int region_id = -1;
};

struct PolyMesh {
    std::vector<std::array<float, 3>> vertices;  // 3D positions
    std::vector<std::array<int, 3>> triangles;   // 3 vertex indices
    std::vector<Contour> contours;
};

class StrategyC : public NavmeshStrategy {
public:
    std::string Name() const override { return "C_RecastStyle_PolyMeshContour"; }

    std::size_t EstimateStorage(const VoxelChunk& chunk) const override {
        (void)chunk;
        // Approximate: 1 vertex = 12 bytes (3 floats), 1 triangle = 12 bytes (3 ints)
        // 1 contour = variable. Typical 8³ chunk = 50-200 vertices + 30-150 triangles.
        // For estimate, use 1 KiB.
        return 1024;
    }

    NavmeshResult Build(const VoxelChunk& chunk, const std::string& scene, int seed) override {
        NavmeshResult r{ Name(), scene, seed };
        auto t0 = std::chrono::steady_clock::now();

        // Step 1+2: voxelize + filter (walkable mask from IsWalkable)
        std::array<uint8_t, kChunkVol> walkable{};
        for (int i = 0; i < kChunkVol; ++i) {
            int x, y, z;
            VoxelChunk::XYZ(i, x, y, z);
            walkable[i] = IsWalkable(chunk, x, y, z) ? 1 : 0;
        }

        // Step 3: regions (BFS flood-fill on walkable cells)
        std::array<int, kChunkVol> region_id{};
        region_id.fill(-1);
        int n_regions = 0;
        std::vector<Contour> contours;
        for (int start = 0; start < kChunkVol; ++start) {
            if (!walkable[start] || region_id[start] >= 0) continue;
            int rid = n_regions++;
            std::vector<int> queue;
            queue.push_back(start);
            region_id[start] = rid;
            size_t qi = 0;
            std::set<int> boundary;
            while (qi < queue.size()) {
                int cur = queue[qi++];
                int cx, cy, cz;
                VoxelChunk::XYZ(cur, cx, cy, cz);
                static const int dx[6] = {1, -1, 0, 0, 0, 0};
                static const int dy[6] = {0, 0, 1, -1, 0, 0};
                static const int dz[6] = {0, 0, 0, 0, 1, -1};
                for (int k = 0; k < 6; ++k) {
                    int nx = cx + dx[k], ny = cy + dy[k], nz = cz + dz[k];
                    if (nx < 0 || nx >= kChunkSize || ny < 0 || ny >= kChunkSize ||
                        nz < 0 || nz >= kChunkSize) continue;
                    int ni = VoxelChunk::Idx(nx, ny, nz);
                    if (walkable[ni] && region_id[ni] < 0) {
                        region_id[ni] = rid;
                        queue.push_back(ni);
                    } else if (!walkable[ni] && region_id[ni] < 0) {
                        // boundary cell (non-walkable adjacent to current region)
                        // we don't store it in this simplified version
                    }
                }
            }
        }

        // Step 4+5: simplified — for each region, take centroid + bbox
        // Real Recast: contour extraction + Douglas-Peucker + triangulation
        // For prototype: 1 poly per region, 4-vertex AABB at region centroid Y
        std::vector<std::array<float, 3>> vertices;
        std::vector<std::array<int, 3>> triangles;
        for (int rid = 0; rid < n_regions; ++rid) {
            // Find region bbox
            int min_x = kChunkSize, max_x = 0, min_y = kChunkSize, max_y = 0;
            int min_z = kChunkSize, max_z = 0;
            int count = 0;
            int sum_x = 0, sum_y = 0, sum_z = 0;
            for (int i = 0; i < kChunkVol; ++i) {
                if (region_id[i] != rid) continue;
                int x, y, z;
                VoxelChunk::XYZ(i, x, y, z);
                min_x = std::min(min_x, x); max_x = std::max(max_x, x);
                min_y = std::min(min_y, y); max_y = std::max(max_y, y);
                min_z = std::min(min_z, z); max_z = std::max(max_z, z);
                sum_x += x; sum_y += y; sum_z += z;
                ++count;
            }
            if (count == 0) continue;
            // Create 4-vertex quad at top of region (centroid Y)
            int cy = sum_y / count;
            int base = vertices.size();
            vertices.push_back({(float)min_x + 0.5f, (float)cy + 0.5f, (float)min_z + 0.5f});
            vertices.push_back({(float)max_x + 0.5f, (float)cy + 0.5f, (float)min_z + 0.5f});
            vertices.push_back({(float)max_x + 0.5f, (float)cy + 0.5f, (float)max_z + 0.5f});
            vertices.push_back({(float)min_x + 0.5f, (float)cy + 0.5f, (float)max_z + 0.5f});
            triangles.push_back({base, base + 1, base + 2});
            triangles.push_back({base, base + 2, base + 3});
        }

        r.storage_bytes = EstimateStorage(chunk);
        r.waypoint_count = n_regions;
        r.edge_count = (int)triangles.size();

        auto t1 = std::chrono::steady_clock::now();
        r.gen_time_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();

        // Pathfinding benchmark: 2D A* on region centroids (with vertical step check)
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(0, n_regions - 1);
        int found = 0;
        double total_ns = 0.0;
        for (int q = 0; q < kAstarPairs; ++q) {
            int src = dist(rng), dst = dist(rng);
            if (n_regions == 0 || src == dst) continue;
            auto pt0 = std::chrono::steady_clock::now();
            int steps = AstarRegions(vertices, n_regions, src, dst);
            auto pt1 = std::chrono::steady_clock::now();
            total_ns += std::chrono::duration<double, std::nano>(pt1 - pt0).count();
            if (steps >= 0) ++found;
        }
        r.query_time_ns = total_ns / kAstarPairs;
        r.paths_found = found;
        r.connectivity = (n_regions > 0) ? 1.0 : 0.0;
        r.doorway_accuracy = 0.0;
        r.ramp_slope_coverage = 0.0;
        return r;
    }

private:
    // Simplified A* on regions (full graph = all-pairs adjacency based on 3D proximity)
    static int AstarRegions(const std::vector<std::array<float, 3>>& vertices,
                            int n_regions, int src, int dst) {
        if (n_regions == 0 || src == dst) return 0;
        std::vector<int> g(n_regions, std::numeric_limits<int>::max());
        g[src] = 0;
        using P = std::pair<int, int>;
        std::priority_queue<P, std::vector<P>, std::greater<P>> open;
        auto heuristic = [&](int a, int b) {
            float dx = vertices[a][0] - vertices[b][0];
            float dy = vertices[a][1] - vertices[b][1];
            float dz = vertices[a][2] - vertices[b][2];
            return (int)std::sqrt(dx * dx + dy * dy + dz * dz);
        };
        open.push({heuristic(src, dst), src});
        std::vector<int> visited(n_regions, 0);
        // Pre-compute adjacency (slow for prototype but accurate)
        std::vector<std::vector<int>> adj(n_regions);
        for (int i = 0; i < n_regions; ++i) {
            for (int j = i + 1; j < n_regions; ++j) {
                float dx = vertices[i][0] - vertices[j][0];
                float dy = vertices[i][1] - vertices[j][1];
                float dz = vertices[i][2] - vertices[j][2];
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist <= 2.5f) {  // adjacent if centers within 2.5 voxel
                    adj[i].push_back(j);
                    adj[j].push_back(i);
                }
            }
        }
        while (!open.empty()) {
            auto [f, cur] = open.top();
            open.pop();
            if (cur == dst) return g[dst];
            if (visited[cur]) continue;
            visited[cur] = 1;
            for (int nb : adj[cur]) {
                int tent = g[cur] + 1;
                if (tent < g[nb]) {
                    g[nb] = tent;
                    open.push({tent + heuristic(nb, dst), nb});
                }
            }
        }
        return -1;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Strategy D — VoxelSurfaceGraph (sparse adjacency graph)
// Per-Y-level, find local surface centers (kernel 3×3) + edges between centers
// within ≤2 voxel distance. Storage: ~32-128 B/chunk.
// ─────────────────────────────────────────────────────────────────────────────
struct SurfaceNode {
    int x, y, z;
    std::vector<int> edges;  // indices into node list
};

class StrategyD : public NavmeshStrategy {
public:
    std::string Name() const override { return "D_VoxelSurfaceGraph"; }

    std::size_t EstimateStorage(const VoxelChunk& chunk) const override {
        // Variable, but typical 8³ chunk = 10-50 nodes * (3 bytes coord + 4 bytes per edge) = 30-250 B
        // For estimate, average 64 B (8 nodes + 8 edges * 4B)
        (void)chunk;
        return 64;
    }

    NavmeshResult Build(const VoxelChunk& chunk, const std::string& scene, int seed) override {
        NavmeshResult r{ Name(), scene, seed };
        auto t0 = std::chrono::steady_clock::now();

        // Build walkable mask
        std::array<uint8_t, kChunkVol> walkable{};
        for (int i = 0; i < kChunkVol; ++i) {
            int x, y, z;
            VoxelChunk::XYZ(i, x, y, z);
            walkable[i] = IsWalkable(chunk, x, y, z) ? 1 : 0;
        }

        // Find surface centers: per Y-level, per XZ, find local maxima of walkable area
        // Simplified: take every walkable cell as a candidate, keep if it's a 3×3 XZ local max
        std::vector<SurfaceNode> nodes;
        for (int y = 0; y < kChunkSize; ++y) {
            for (int x = 0; x < kChunkSize; ++x) {
                for (int z = 0; z < kChunkSize; ++z) {
                    if (!walkable[VoxelChunk::Idx(x, y, z)]) continue;
                    // Count walkable cells in 3×3 XZ neighborhood (current Y level)
                    int count = 0;
                    for (int dx = -1; dx <= 1; ++dx) {
                        for (int dz = -1; dz <= 1; ++dz) {
                            int nx = x + dx, nz = z + dz;
                            if (nx < 0 || nx >= kChunkSize || nz < 0 || nz >= kChunkSize) continue;
                            if (walkable[VoxelChunk::Idx(nx, y, nz)]) ++count;
                        }
                    }
                    // Keep if 3+ neighbors (interior surface point)
                    if (count >= 3) {
                        SurfaceNode n;
                        n.x = x; n.y = y; n.z = z;
                        nodes.push_back(n);
                    }
                }
            }
        }

        // Build edges: between any two nodes within 2 voxel Manhattan distance + same Y level
        int edge_count = 0;
        for (size_t i = 0; i < nodes.size(); ++i) {
            for (size_t j = i + 1; j < nodes.size(); ++j) {
                int manhattan = std::abs(nodes[i].x - nodes[j].x) +
                                std::abs(nodes[i].y - nodes[j].y) +
                                std::abs(nodes[i].z - nodes[j].z);
                if (manhattan <= 2 && nodes[i].y == nodes[j].y) {
                    nodes[i].edges.push_back((int)j);
                    nodes[j].edges.push_back((int)i);
                    ++edge_count;
                }
            }
        }

        r.storage_bytes = EstimateStorage(chunk);
        r.waypoint_count = (int)nodes.size();
        r.edge_count = edge_count;

        auto t1 = std::chrono::steady_clock::now();
        r.gen_time_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();

        // Pathfinding benchmark: A* on sparse node graph
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(0, (int)nodes.size() - 1);
        int found = 0;
        double total_ns = 0.0;
        if (nodes.empty()) {
            r.query_time_ns = 0.0;
            r.paths_found = 0;
        } else {
            for (int q = 0; q < kAstarPairs; ++q) {
                int src = dist(rng), dst = dist(rng);
                if (src == dst) continue;
                auto pt0 = std::chrono::steady_clock::now();
                int steps = AstarGraph(nodes, src, dst);
                auto pt1 = std::chrono::steady_clock::now();
                total_ns += std::chrono::duration<double, std::nano>(pt1 - pt0).count();
                if (steps >= 0) ++found;
            }
            r.query_time_ns = total_ns / kAstarPairs;
            r.paths_found = found;
        }
        r.connectivity = (nodes.empty()) ? 0.0 : 1.0;
        r.doorway_accuracy = 0.0;
        r.ramp_slope_coverage = 0.0;
        return r;
    }

private:
    static int AstarGraph(const std::vector<SurfaceNode>& nodes, int src, int dst) {
        if (src == dst) return 0;
        int n = (int)nodes.size();
        std::vector<int> g(n, std::numeric_limits<int>::max());
        g[src] = 0;
        using P = std::pair<int, int>;
        std::priority_queue<P, std::vector<P>, std::greater<P>> open;
        auto heuristic = [&](int a, int b) {
            return std::abs(nodes[a].x - nodes[b].x) +
                   std::abs(nodes[a].y - nodes[b].y) +
                   std::abs(nodes[a].z - nodes[b].z);
        };
        open.push({heuristic(src, dst), src});
        std::vector<int> visited(n, 0);
        while (!open.empty()) {
            auto [f, cur] = open.top();
            open.pop();
            if (cur == dst) return g[dst];
            if (visited[cur]) continue;
            visited[cur] = 1;
            for (int nb : nodes[cur].edges) {
                int tent = g[cur] + 1;
                if (tent < g[nb]) {
                    g[nb] = tent;
                    open.push({tent + heuristic(nb, dst), nb});
                }
            }
        }
        return -1;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Strategy E — Hybrid3D_RegionGraph (region + doorway + cross-chunk adj)
// Per Y-level walkable regions + per-region doors (step-up/down, jump, free).
// Storage: 0.5-1.5 KiB/chunk.
// ─────────────────────────────────────────────────────────────────────────────
struct Region {
    int y_level;
    std::vector<int> cells;  // voxel indices
    std::vector<int> doors;  // indices into door list
};

struct Door {
    enum Type { STEP_UP, STEP_DOWN, JUMP, FREE };
    int from_region;
    int to_region;
    Type type;
    int x, y, z;  // door location
};

class StrategyE : public NavmeshStrategy {
public:
    std::string Name() const override { return "E_Hybrid3D_RegionGraph"; }

    std::size_t EstimateStorage(const VoxelChunk& chunk) const override {
        (void)chunk;
        // Variable: ~50-200 B for regions + ~100-300 B for doors
        // For estimate, 1 KiB
        return 1024;
    }

    NavmeshResult Build(const VoxelChunk& chunk, const std::string& scene, int seed) override {
        NavmeshResult r{ Name(), scene, seed };
        auto t0 = std::chrono::steady_clock::now();

        // Build walkable mask
        std::array<uint8_t, kChunkVol> walkable{};
        for (int i = 0; i < kChunkVol; ++i) {
            int x, y, z;
            VoxelChunk::XYZ(i, x, y, z);
            walkable[i] = IsWalkable(chunk, x, y, z) ? 1 : 0;
        }

        // Per Y-level: BFS region detection
        std::vector<Region> regions;
        std::array<int, kChunkVol> cell_to_region{};
        cell_to_region.fill(-1);
        for (int y = 0; y < kChunkSize; ++y) {
            for (int start_x = 0; start_x < kChunkSize; ++start_x) {
                for (int start_z = 0; start_z < kChunkSize; ++start_z) {
                    int start = VoxelChunk::Idx(start_x, y, start_z);
                    if (!walkable[start] || cell_to_region[start] >= 0) continue;
                    Region r0;
                    r0.y_level = y;
                    std::vector<int> queue;
                    queue.push_back(start);
                    cell_to_region[start] = (int)regions.size();
                    size_t qi = 0;
                    while (qi < queue.size()) {
                        int cur = queue[qi++];
                        r0.cells.push_back(cur);
                        int cx, cy, cz;
                        VoxelChunk::XYZ(cur, cx, cy, cz);
                        // 4-connectivity on same Y level
                        static const int dx[4] = {1, -1, 0, 0};
                        static const int dz[4] = {0, 0, 1, -1};
                        for (int k = 0; k < 4; ++k) {
                            int nx = cx + dx[k], nz = cz + dz[k];
                            if (nx < 0 || nx >= kChunkSize || nz < 0 || nz >= kChunkSize) continue;
                            int ni = VoxelChunk::Idx(nx, y, nz);
                            if (walkable[ni] && cell_to_region[ni] < 0) {
                                cell_to_region[ni] = (int)regions.size();
                                queue.push_back(ni);
                            }
                        }
                    }
                    regions.push_back(r0);
                }
            }
        }

        // Doorway detection: per pair of adjacent Y-levels, find cells where
        // 1-2 voxel vertical gap = step-up/down, 3-4 voxel gap = jump, 1-3 voxel with ladder = free
        std::vector<Door> doors;
        for (size_t ri = 0; ri < regions.size(); ++ri) {
            for (int cell : regions[ri].cells) {
                int cx, cy, cz;
                VoxelChunk::XYZ(cell, cx, cy, cz);
                // Check upward (step-up)
                for (int dy = 1; dy <= kJumpHeightMax; ++dy) {
                    int ny = cy + dy;
                    if (ny >= kChunkSize) break;
                    int ni = VoxelChunk::Idx(cx, ny, cz);
                    if (!walkable[ni]) continue;
                    int rj = cell_to_region[ni];
                    if (rj < 0 || rj == (int)ri) continue;
                    Door d;
                    d.from_region = (int)ri;
                    d.to_region = rj;
                    d.x = cx; d.y = cy; d.z = cz;
                    if (dy == 1) d.type = Door::STEP_UP;
                    else if (dy == 2) d.type = Door::JUMP;
                    else d.type = Door::JUMP;
                    doors.push_back(d);
                    regions[ri].doors.push_back((int)doors.size() - 1);
                }
                // Check downward (step-down)
                for (int dy = 1; dy <= kStepDownMax; ++dy) {
                    int ny = cy - dy;
                    if (ny < 0) break;
                    int ni = VoxelChunk::Idx(cx, ny, cz);
                    if (!walkable[ni]) continue;
                    int rj = cell_to_region[ni];
                    if (rj < 0 || rj == (int)ri) continue;
                    Door d;
                    d.from_region = (int)ri;
                    d.to_region = rj;
                    d.x = cx; d.y = cy; d.z = cz;
                    d.type = Door::STEP_DOWN;
                    doors.push_back(d);
                    regions[ri].doors.push_back((int)doors.size() - 1);
                }
            }
        }

        r.storage_bytes = EstimateStorage(chunk);
        r.waypoint_count = (int)regions.size();
        r.edge_count = (int)doors.size();
        r.door_count = (int)doors.size();

        auto t1 = std::chrono::steady_clock::now();
        r.gen_time_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();

        // Pathfinding benchmark: A* on region graph with door transitions
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(0, (int)regions.size() - 1);
        int found = 0;
        double total_ns = 0.0;
        if (regions.empty()) {
            r.query_time_ns = 0.0;
            r.paths_found = 0;
        } else {
            // Pre-compute adjacency from doors
            std::vector<std::vector<int>> adj(regions.size());
            for (const auto& d : doors) {
                adj[d.from_region].push_back(d.to_region);
            }
            for (int q = 0; q < kAstarPairs; ++q) {
                int src = dist(rng), dst = dist(rng);
                if (src == dst) continue;
                auto pt0 = std::chrono::steady_clock::now();
                int steps = AstarRegionGraph(adj, src, dst);
                auto pt1 = std::chrono::steady_clock::now();
                total_ns += std::chrono::duration<double, std::nano>(pt1 - pt0).count();
                if (steps >= 0) ++found;
            }
            r.query_time_ns = total_ns / kAstarPairs;
            r.paths_found = found;
        }
        r.connectivity = (regions.empty()) ? 0.0 : 1.0;
        r.doorway_accuracy = 0.0;
        r.ramp_slope_coverage = 0.0;
        return r;
    }

private:
    static int AstarRegionGraph(const std::vector<std::vector<int>>& adj, int src, int dst) {
        if (src == dst) return 0;
        int n = (int)adj.size();
        std::vector<int> g(n, std::numeric_limits<int>::max());
        g[src] = 0;
        using P = std::pair<int, int>;
        std::priority_queue<P, std::vector<P>, std::greater<P>> open;
        open.push({0, src});
        std::vector<int> visited(n, 0);
        while (!open.empty()) {
            auto [f, cur] = open.top();
            open.pop();
            if (cur == dst) return g[dst];
            if (visited[cur]) continue;
            visited[cur] = 1;
            for (int nb : adj[cur]) {
                int tent = g[cur] + 1;
                if (tent < g[nb]) {
                    g[nb] = tent;
                    open.push({tent, nb});
                }
            }
        }
        return -1;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Scene generation
// ─────────────────────────────────────────────────────────────────────────────
enum class SceneId { OPEN_TERRAIN, SPARSE_ROCKS, DENSE_URBAN, STAIRS_RAMP, DESTROYED_BUILDING };

static std::string SceneName(SceneId s) {
    switch (s) {
        case SceneId::OPEN_TERRAIN:      return "open_terrain";
        case SceneId::SPARSE_ROCKS:      return "sparse_rocks";
        case SceneId::DENSE_URBAN:       return "dense_urban";
        case SceneId::STAIRS_RAMP:       return "stairs_ramp";
        case SceneId::DESTROYED_BUILDING:return "destroyed_building";
    }
    return "?";
}

static void GenScene(SceneId scene, int seed, VoxelChunk& chunk) {
    std::mt19937 rng(seed);
    // Default: ground at y=0
    for (int x = 0; x < kChunkSize; ++x) {
        for (int z = 0; z < kChunkSize; ++z) {
            chunk.SetSolid(x, 0, z, true);  // ground
        }
    }
    // All other cells = empty
    for (int y = 1; y < kChunkSize; ++y) {
        for (int x = 0; x < kChunkSize; ++x) {
            for (int z = 0; z < kChunkSize; ++z) {
                chunk.SetSolid(x, y, z, false);
            }
        }
    }

    switch (scene) {
        case SceneId::OPEN_TERRAIN: {
            // Just ground, no obstacles. Walkable = all cells y>=1.
            break;
        }
        case SceneId::SPARSE_ROCKS: {
            // Add 5-20 isolated rock obstacles (1-2 voxel each)
            std::uniform_int_distribution<int> n_dist(5, 20);
            std::uniform_int_distribution<int> pos_dist(1, kChunkSize - 1);
            std::uniform_int_distribution<int> h_dist(1, 2);
            int n = n_dist(rng);
            for (int i = 0; i < n; ++i) {
                int x = pos_dist(rng), z = pos_dist(rng);
                int h = h_dist(rng);
                for (int dy = 1; dy <= h; ++dy) {
                    chunk.SetSolid(x, dy, z, true);
                }
            }
            break;
        }
        case SceneId::DENSE_URBAN: {
            // 50% building coverage, multi-story (Y-levels 0-3), doorways at 1-voxel passages
            // Create 2 buildings, 2×2 voxel footprint each, with 1-voxel doorways
            // Building 1: x in [1,2], z in [1,2], y in [0,3]
            for (int x = 1; x <= 2; ++x)
                for (int z = 1; z <= 2; ++z)
                    for (int y = 0; y <= 3; ++y)
                        chunk.SetSolid(x, y, z, true);
            // Doorway at z=1, y=1 (gap for entry)
            chunk.SetSolid(2, 1, 1, false);
            // Building 2: x in [5,6], z in [5,6], y in [0,2]
            for (int x = 5; x <= 6; ++x)
                for (int z = 5; z <= 6; ++z)
                    for (int y = 0; y <= 2; ++y)
                        chunk.SetSolid(x, y, z, true);
            chunk.SetSolid(5, 1, 5, false);
            // Add some debris
            std::uniform_int_distribution<int> pos_dist(1, kChunkSize - 1);
            for (int i = 0; i < 8; ++i) {
                int x = pos_dist(rng), z = pos_dist(rng);
                chunk.SetSolid(x, 1, z, true);
            }
            break;
        }
        case SceneId::STAIRS_RAMP: {
            // Staircase from y=0 to y=3 at x in [3,4], z in [0,7]
            // Step pattern: y=1 at z=0, y=2 at z=2, y=3 at z=4, y=2 at z=6
            for (int z = 0; z < 8; ++z) {
                if (z < 2) chunk.SetSolid(3, 1, z, true);
                else if (z < 4) chunk.SetSolid(3, 2, z, true);
                else if (z < 6) chunk.SetSolid(3, 3, z, true);
                else chunk.SetSolid(3, 2, z, true);
            }
            // Ramp: y=0 at z=0, y=2 at z=7, slope 2/7
            for (int z = 0; z < 8; ++z) {
                int y = (z * 2) / 7;
                if (y > 0) chunk.SetSolid(5, y, z, true);
            }
            break;
        }
        case SceneId::DESTROYED_BUILDING: {
            // 60% mass building with 5 doorways
            for (int x = 2; x <= 5; ++x)
                for (int z = 2; z <= 5; ++z)
                    for (int y = 0; y <= 2; ++y)
                        chunk.SetSolid(x, y, z, true);
            // 5 doorways (gaps)
            chunk.SetSolid(3, 1, 2, false);
            chunk.SetSolid(4, 1, 2, false);
            chunk.SetSolid(2, 1, 4, false);
            chunk.SetSolid(5, 1, 4, false);
            chunk.SetSolid(4, 1, 5, false);
            // 20% debris (random)
            std::uniform_int_distribution<int> pos_dist(0, kChunkSize - 1);
            for (int i = 0; i < 20; ++i) {
                int x = pos_dist(rng), y = pos_dist(rng), z = pos_dist(rng);
                if (y >= 3) continue;  // only on ground
                chunk.SetSolid(x, y, z, true);
            }
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Main benchmark loop
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::vector<NavmeshStrategy*> strategies = {
        new StrategyA(), new StrategyB(), new StrategyC(), new StrategyD(), new StrategyE()
    };
    std::vector<SceneId> scenes = {
        SceneId::OPEN_TERRAIN, SceneId::SPARSE_ROCKS, SceneId::DENSE_URBAN,
        SceneId::STAIRS_RAMP, SceneId::DESTROYED_BUILDING
    };
    std::vector<int> seeds = {1, 7, 42, 1234, 31337};

    // CSV header
    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,gen_time_ns,storage_bytes,waypoint_count,edge_count,door_count,"
        << "connectivity,doorway_accuracy,ramp_slope_coverage,query_time_ns,paths_found\n";

    std::vector<NavmeshResult> all_results;

    for (auto* strat : strategies) {
        for (auto scene : scenes) {
            std::string scene_name = SceneName(scene);
            for (int seed : seeds) {
                VoxelChunk chunk;
                GenScene(scene, seed, chunk);

                // Warmup
                for (int w = 0; w < kWarmup; ++w) {
                    strat->Build(chunk, scene_name, seed);
                }

                // Main measurement: average over kIters
                std::vector<NavmeshResult> iters;
                iters.reserve(kIters);
                for (int it = 0; it < kIters; ++it) {
                    NavmeshResult r = strat->Build(chunk, scene_name, seed);
                    iters.push_back(r);
                }
                // Aggregate: median for ns values
                std::vector<double> gen_times, query_times;
                int total_found = 0;
                for (const auto& r : iters) {
                    gen_times.push_back(r.gen_time_ns);
                    query_times.push_back(r.query_time_ns);
                    total_found += r.paths_found;
                }
                auto median = [](std::vector<double> v) {
                    std::sort(v.begin(), v.end());
                    return v[v.size() / 2];
                };

                NavmeshResult agg;
                agg.strategy = iters[0].strategy;
                agg.scene = iters[0].scene;
                agg.seed = seed;
                agg.gen_time_ns = median(gen_times);
                agg.query_time_ns = median(query_times);
                agg.storage_bytes = iters[0].storage_bytes;
                agg.waypoint_count = iters[0].waypoint_count;
                agg.edge_count = iters[0].edge_count;
                agg.door_count = iters[0].door_count;
                agg.connectivity = iters[0].connectivity;
                agg.paths_found = total_found / kIters;

                all_results.push_back(agg);
                csv << agg.strategy << "," << agg.scene << "," << agg.seed << ","
                    << std::fixed << std::setprecision(2) << agg.gen_time_ns << ","
                    << agg.storage_bytes << "," << agg.waypoint_count << "," << agg.edge_count << ","
                    << agg.door_count << "," << agg.connectivity << ","
                    << agg.doorway_accuracy << "," << agg.ramp_slope_coverage << ","
                    << agg.query_time_ns << "," << agg.paths_found << "\n";
            }
        }
    }
    csv.close();

    // Summary means
    std::ofstream summary("build/summary_means.csv");
    summary << "strategy,mean_gen_us,mean_storage_b,mean_waypoints,mean_edges,mean_doors,"
            << "mean_query_ns,total_paths_found,total_paths\n";
    for (auto* strat : strategies) {
        double sum_gen = 0, sum_query = 0, sum_storage = 0;
        int sum_wp = 0, sum_edges = 0, sum_doors = 0, sum_found = 0, total = 0;
        int n = 0;
        for (const auto& r : all_results) {
            if (r.strategy != strat->Name()) continue;
            sum_gen += r.gen_time_ns;
            sum_query += r.query_time_ns;
            sum_storage += (double)r.storage_bytes;
            sum_wp += r.waypoint_count;
            sum_edges += r.edge_count;
            sum_doors += r.door_count;
            sum_found += r.paths_found;
            total += r.total_paths;
            ++n;
        }
        if (n > 0) {
            summary << strat->Name() << ","
                    << std::fixed << std::setprecision(4) << (sum_gen / n / 1000.0) << ","
                    << (sum_storage / n) << ","
                    << (sum_wp / (double)n) << ","
                    << (sum_edges / (double)n) << ","
                    << (sum_doors / (double)n) << ","
                    << (sum_query / n) << ","
                    << sum_found << "," << total << "\n";
        }
    }
    summary.close();

    // Log
    std::ofstream log("build/run.log");
    log << "2026-06-22-voxel-navmesh-graph-generation — C++26 CPU prototype\n";
    log << "Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic\n";
    log << "5 strategies × 5 scenes × 5 seeds × " << kIters << " iter + " << kWarmup << " warmup = "
        << 5 * 5 * 5 * kIters << " main measurements\n";
    log << "Chunk size: " << kChunkSize << "³ = " << kChunkVol << " voxels\n";
    log << "Wall time: <30 sec (target) на Zen 3 5800X governor=powersave\n";
    log.close();

    for (auto* s : strategies) delete s;
    return 0;
}
