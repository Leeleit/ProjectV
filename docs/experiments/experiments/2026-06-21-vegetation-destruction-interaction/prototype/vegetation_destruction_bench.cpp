#include <iostream>
#include <vector>
#include <queue>
#include <chrono>
#include <random>
#include <algorithm>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdint>
#include <unordered_map>
#include <cmath>

using namespace std;

// ---------------------------------------------------------------------------
// Constantes
// ---------------------------------------------------------------------------
const int WS = 32;
const int TOTAL_VOXELS = WS * WS * WS;

enum Voxel : uint8_t {
    V_AIR = 0,
    V_WOOD = 1,
    V_LEAF = 2,
    V_GROUND = 3
};

// ---------------------------------------------------------------------------
// Union-Find (DSU)
// ---------------------------------------------------------------------------
class DSU {
public:
    vector<int> parent, rank;
    DSU(int n) : parent(n), rank(n, 0) {
        for (int i = 0; i < n; ++i) parent[i] = i;
    }
    int find(int i) {
        if (parent[i] == i) return i;
        return parent[i] = find(parent[i]);
    }
    bool unite(int i, int j) {
        int ri = find(i), rj = find(j);
        if (ri == rj) return false;
        if (rank[ri] < rank[rj]) swap(ri, rj);
        parent[rj] = ri;
        if (rank[ri] == rank[rj]) ++rank[ri];
        return true;
    }
};

// ---------------------------------------------------------------------------
// Helpers 3D
// ---------------------------------------------------------------------------
inline int vidx(int x, int y, int z) {
    return (x * WS + y) * WS + z;
}

inline void vcoords(int idx, int &x, int &y, int &z) {
    x = idx / (WS * WS);
    int r = idx % (WS * WS);
    y = r / WS;
    z = r % WS;
}

inline bool in_bounds(int x, int y, int z) {
    return x >= 0 && x < WS && y >= 0 && y < WS && z >= 0 && z < WS;
}

inline bool is_solid(uint8_t v) {
    return v == V_WOOD || v == V_LEAF;
}

// ---------------------------------------------------------------------------
// Computes all connected components of solid voxels (6-conn) via BFS from ground.
// Returns a boolean mask: false = detached (not connected to ground).
// ground_touching: solid voxel with Y == 0 is ground-anchored.
// ---------------------------------------------------------------------------
vector<bool> compute_cc_from_ground(const uint8_t *grid) {
    vector<bool> visited(TOTAL_VOXELS, false);
    vector<bool> anchored(TOTAL_VOXELS, false);
    queue<int> q;

    for (int x = 0; x < WS; ++x) {
        for (int z = 0; z < WS; ++z) {
            int idx0 = vidx(x, 0, z);
            if (grid[idx0] == V_GROUND || is_solid(grid[idx0])) {
                visited[idx0] = true;
                anchored[idx0] = true;
                q.push(idx0);
            }
        }
    }

    static const int dx[6] = {1,-1,0,0,0,0};
    static const int dy[6] = {0,0,1,-1,0,0};
    static const int dz[6] = {0,0,0,0,1,-1};

    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cx, cy, cz;
        vcoords(cur, cx, cy, cz);
        for (int d = 0; d < 6; ++d) {
            int nx = cx + dx[d], ny = cy + dy[d], nz = cz + dz[d];
            if (!in_bounds(nx, ny, nz)) continue;
            int nidx = vidx(nx, ny, nz);
            if (!visited[nidx] && is_solid(grid[nidx])) {
                visited[nidx] = true;
                anchored[nidx] = true;
                q.push(nidx);
            }
        }
    }

    return anchored;
}

// ---------------------------------------------------------------------------
// Compute per-CC properties for stress model
// ---------------------------------------------------------------------------
struct CCInfo {
    vector<int> voxels;
    int min_y;
    int support_layer_count; // voxels with Y == min_y
    bool connected_to_ground;
};

vector<CCInfo> compute_ccs(const uint8_t *grid, const vector<bool> &ground_anchored) {
    vector<bool> visited(TOTAL_VOXELS, false);
    vector<CCInfo> ccs;

    // First, mark all ground-anchored as visited (they are in CC 0 = "healthy")
    for (int i = 0; i < TOTAL_VOXELS; ++i) {
        if (ground_anchored[i]) visited[i] = true;
    }

    // Collect the "healthy" CC (everything connected to ground)
    CCInfo healthy;
    healthy.min_y = 0;
    healthy.support_layer_count = 0;
    healthy.connected_to_ground = true;
    for (int i = 0; i < TOTAL_VOXELS; ++i) {
        if (ground_anchored[i]) {
            healthy.voxels.push_back(i);
        }
    }
    if (!healthy.voxels.empty()) {
        ccs.push_back(move(healthy));
    }

    // Now find all detached CCs (solid voxels not in ground_anchored)
    static const int dx[6] = {1,-1,0,0,0,0};
    static const int dy[6] = {0,0,1,-1,0,0};
    static const int dz[6] = {0,0,0,0,1,-1};

    for (int i = 0; i < TOTAL_VOXELS; ++i) {
        if (!visited[i] && is_solid(grid[i])) {
            CCInfo cc;
            cc.min_y = 99;
            cc.connected_to_ground = false;
            queue<int> q;
            q.push(i);
            visited[i] = true;
            while (!q.empty()) {
                int cur = q.front(); q.pop();
                int cx, cy, cz;
                vcoords(cur, cx, cy, cz);
                cc.voxels.push_back(cur);
                if (cy < cc.min_y) cc.min_y = cy;
                int nx, ny, nz, nidx;
                for (int d = 0; d < 6; ++d) {
                    nx = cx + dx[d]; ny = cy + dy[d]; nz = cz + dz[d];
                    if (!in_bounds(nx, ny, nz)) continue;
                    nidx = vidx(nx, ny, nz);
                    if (!visited[nidx] && is_solid(grid[nidx])) {
                        visited[nidx] = true;
                        q.push(nidx);
                    }
                }
            }
            cc.support_layer_count = 0;
            for (int v : cc.voxels) {
                int cx, cy, cz;
                vcoords(v, cx, cy, cz);
                if (cy == cc.min_y) cc.support_layer_count++;
            }
            ccs.push_back(move(cc));
        }
    }

    return ccs;
}

// ---------------------------------------------------------------------------
// Stress topple check: for the ground-connected CC, look at the tree's
// TRUNK BASE (voxels at Y=1) vs CANOPY (voxels with Y > 1). If the trunk
// base layer is reduced below 5% of the canopy mass AND canopy > 50 voxels,
// topple the entire canopy (mark as detached). This captures the realistic
// felling failure mode where the trunk is splintered but still has 1-2 voxels
// connecting canopy to ground.
// ---------------------------------------------------------------------------
vector<bool> stress_topple_detached(const uint8_t *grid, const vector<bool> &ground_anchored,
                                     const vector<CCInfo> &ccs, int k_max_cantilever) {
    vector<bool> result = ground_anchored;
    (void)k_max_cantilever; // currently unused — cantilever is implicit via base ratio

    // Find the ground-connected CC that contains voxels at Y=1 (the tree base).
    int tree_base_voxels = 0;  // count at Y=1
    int tree_canopy_voxels = 0; // count at Y >= 2

    for (int i = 0; i < TOTAL_VOXELS; ++i) {
        if (!ground_anchored[i]) continue; // only look at ground-connected
        int vx, vy, vz;
        vcoords(i, vx, vy, vz);
        if (vy == 1 && is_solid(grid[i])) {
            tree_base_voxels++;
        } else if (vy >= 2 && is_solid(grid[i])) {
            tree_canopy_voxels++;
        }
    }

    // If canopy exists and base is < 5% of canopy mass, topple the canopy
    if (tree_canopy_voxels > 50 &&
        tree_base_voxels > 0 &&
        tree_base_voxels * 20 < tree_canopy_voxels) {
        // Topple all canopy voxels (Y >= 2) that were ground-anchored
        for (int i = 0; i < TOTAL_VOXELS; ++i) {
            if (!ground_anchored[i]) continue;
            int vx, vy, vz;
            vcoords(i, vx, vy, vz);
            if (vy >= 2 && is_solid(grid[i])) {
                result[i] = false; // detach
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Strategy A: naive global BFS from ground (ground truth)
// ---------------------------------------------------------------------------
vector<bool> strategy_A_naive_global_bfs(const uint8_t *grid) {
    return compute_cc_from_ground(grid);
}

// ---------------------------------------------------------------------------
// Strategy B: hierarchical DSU — local CCL on a single 8³ bounding box
// around the tree, + DSU merge with ground.
// Since trees fit in one 8³, we CCL that one chunk, then check each
// component for ground connection.
// ---------------------------------------------------------------------------
vector<bool> strategy_B_hierarchical_dsu(const uint8_t *grid) {
    // Find the bounding box of solid voxels
    int min_x = WS, max_x = 0, min_y = WS, max_y = 0, min_z = WS, max_z = 0;
    for (int i = 0; i < TOTAL_VOXELS; ++i) {
        if (!is_solid(grid[i])) continue;
        int cx, cy, cz;
        vcoords(i, cx, cy, cz);
        if (cx < min_x) min_x = cx;
        if (cx > max_x) max_x = cx;
        if (cy < min_y) min_y = cy;
        if (cy > max_y) max_y = cy;
        if (cz < min_z) min_z = cz;
        if (cz > max_z) max_z = cz;
    }

    // Expand a bit to cover boundary
    min_x = max(0, min_x - 1); max_x = min(WS-1, max_x + 1);
    min_y = max(0, min_y - 1); max_y = min(WS-1, max_y + 1);
    min_z = max(0, min_z - 1); max_z = min(WS-1, max_z + 1);

    int sx = max_x - min_x + 1;
    int sy = max_y - min_y + 1;
    int sz = max_z - min_z + 1;

    // Direct AABB indexing: lidx = (lx*sy + ly)*sz + lz, where (lx,ly,lz) are local coords
    // n_local_max = sx * sy * sz (max possible = ~16*16*16 = 4096 for trees)
    int n_local_max = sx * sy * sz;
    auto local_idx = [&](int lx, int ly, int lz) {
        return (lx * sy + ly) * sz + lz;
    };

    // First pass: assign local indices to solid voxels
    vector<int> local_of_global(TOTAL_VOXELS, -1);
    int n_local = 0;
    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                int gidx = vidx(x, y, z);
                if (!is_solid(grid[gidx])) continue;
                local_of_global[gidx] = n_local++;
            }
        }
    }

    if (n_local == 0) {
        vector<bool> anchored(TOTAL_VOXELS, false);
        for (int x = 0; x < WS; ++x)
            for (int z = 0; z < WS; ++z)
                anchored[vidx(x, 0, z)] = true;
        return anchored;
    }

    // DSU over local indices
    DSU dsu(n_local + 1); // +1 for virtual ground
    int ground_node = n_local;

    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                int gidx = vidx(x, y, z);
                int lidx = local_of_global[gidx];
                if (lidx < 0) continue;
                // 6-connectivity (only +X, +Y, +Z to avoid duplicates)
                if (x < max_x) {
                    int ngidx = vidx(x+1, y, z);
                    int nlidx = local_of_global[ngidx];
                    if (nlidx >= 0) dsu.unite(lidx, nlidx);
                }
                if (y < max_y) {
                    int ngidx = vidx(x, y+1, z);
                    int nlidx = local_of_global[ngidx];
                    if (nlidx >= 0) dsu.unite(lidx, nlidx);
                }
                if (z < max_z) {
                    int ngidx = vidx(x, y, z+1);
                    int nlidx = local_of_global[ngidx];
                    if (nlidx >= 0) dsu.unite(lidx, nlidx);
                }
                // If this voxel is at Y=1, link to ground
                if (y == 1) {
                    dsu.unite(lidx, ground_node);
                }
            }
        }
    }

    // Results: a solid voxel is anchored if its local component is united with ground_node
    vector<bool> anchored(TOTAL_VOXELS, false);
    int ground_root = dsu.find(ground_node);
    for (int i = 0; i < TOTAL_VOXELS; ++i) {
        int lidx = local_of_global[i];
        if (lidx < 0) continue;
        if (dsu.find(lidx) == ground_root) {
            anchored[i] = true;
        }
    }

    // Ground is always anchored
    for (int x = 0; x < WS; ++x)
        for (int z = 0; z < WS; ++z)
            anchored[vidx(x, 0, z)] = true;

    return anchored;
}

// ---------------------------------------------------------------------------
// Strategy C: local split BFS from neighbours of destroyed voxels.
// We don't know which voxel was destroyed in the incremental model,
// so we do a limited BFS from the ground up: essentially the same as A
// but we limit BFS depth to 1 chunk radius (8).
// This is a SIMPLIFIED version: for fairness, we'll do BFS from all
// ground voxels with a depth limit of 32 (the world height).
// In practice, this degenerates to A for small worlds; the difference
// is that we DON'T pre-mark all ground — we start BFS from the base
// and let it propagate.
// ---------------------------------------------------------------------------
vector<bool> strategy_C_local_split_bfs(const uint8_t *grid) {
    return compute_cc_from_ground(grid); // identical to A for this simple case
}

// ---------------------------------------------------------------------------
// Strategy D: lightweight stress topple detection
// ---------------------------------------------------------------------------
vector<bool> strategy_D_lightweight_stress_topple(const uint8_t *grid) {
    vector<bool> geometric = compute_cc_from_ground(grid);
    vector<CCInfo> ccs = compute_ccs(grid, geometric);
    return stress_topple_detached(grid, geometric, ccs, 5);
}

// ---------------------------------------------------------------------------
// Strategy E: hybrid AABB — same as B for now (AABB optimization is not
// meaningful in single-chunk tree; the real hybrid would run in mainline
// where tree fits in 1 chunk but world has many chunks.)
// ---------------------------------------------------------------------------
vector<bool> strategy_E_hybrid_aabb(const uint8_t *grid) {
    return strategy_B_hierarchical_dsu(grid);
}

// ---------------------------------------------------------------------------
// Scene generators — each builds a tree centred at (16, 0, 16)
// ---------------------------------------------------------------------------
void clear_grid(uint8_t *grid) {
    memset(grid, V_AIR, TOTAL_VOXELS * sizeof(uint8_t));
    for (int x = 0; x < WS; ++x)
        for (int z = 0; z < WS; ++z)
            grid[vidx(x, 0, z)] = V_GROUND;
}

void set_v(uint8_t *grid, int x, int y, int z, uint8_t v) {
    if (in_bounds(x, y, z)) grid[vidx(x, y, z)] = v;
}

// Helper: fill a box with a material
void fill_box(uint8_t *grid, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t v) {
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                set_v(grid, x, y, z, v);
}

void build_deciduous_tree(uint8_t *grid) {
    clear_grid(grid);
    // Trunk: 4x4 from Y=1 to Y=16
    fill_box(grid, 15, 1, 15, 18, 16, 18, V_WOOD);
    // Crown: 12x6x12 ellipsoid at Y=12 to Y=18
    int cx = 16, cy = 15, cz = 16;
    for (int x = cx-6; x <= cx+6; ++x) {
        for (int y = cy-3; y <= cy+3; ++y) {
            for (int z = cz-6; z <= cz+6; ++z) {
                if (!in_bounds(x, y, z)) continue;
                double dx2 = ((double)(x - cx) / 6.0);
                double dy2 = ((double)(y - cy) / 4.0);
                double dz2 = ((double)(z - cz) / 6.0);
                if (dx2*dx2 + dy2*dy2 + dz2*dz2 <= 1.0) {
                    if (grid[vidx(x, y, z)] == V_AIR)
                        set_v(grid, x, y, z, V_LEAF);
                }
            }
        }
    }
    // Three main boughs (WOOD in crown)
    for (int i = -3; i <= 3; i += 3) {
        for (int j = -3; j <= 3; j += 3) {
            if (i == 0 && j == 0) continue;
            set_v(grid, cx + i, 14 - abs(i), cz + j, V_WOOD);
        }
    }
}

void build_coniferous_pine(uint8_t *grid) {
    clear_grid(grid);
    // Thin central trunk: 1x1 from Y=1 to Y=24
    fill_box(grid, 16, 1, 16, 16, 24, 16, V_WOOD);
    // 6 needle layers of expanding radius
    int layers[6][4] = {
        {4, 4, 3, 3},  // Y_base, Y_top, radius
        {8, 8, 5, 5},
        {12, 12, 7, 7},
        {16, 16, 9, 9},
        {20, 20, 7, 7},
        {24, 24, 5, 5}
    };
    for (int l = 0; l < 6; ++l) {
        int y = layers[l][0];
        int r = layers[l][2];
        for (int x = 16 - r; x <= 16 + r; ++x) {
            for (int z = 16 - r; z <= 16 + r; ++z) {
                if (!in_bounds(x, y, z)) continue;
                double dx = (double)(x - 16);
                double dz = (double)(z - 16);
                if (dx*dx + dz*dz <= (double)(r*r)) {
                    if (grid[vidx(x, y, z)] == V_AIR)
                        set_v(grid, x, y, z, V_LEAF);
                }
            }
        }
    }
}

void build_bush(uint8_t *grid) {
    clear_grid(grid);
    // Trunk: 4x4 from Y=1 to Y=4
    fill_box(grid, 15, 1, 15, 18, 4, 18, V_WOOD);
    // 8 radial branches
    for (int d = 0; d < 8; ++d) {
        double angle = d * 3.14159 / 4.0;
        int dx = (int)round(cos(angle) * 3.0);
        int dz = (int)round(sin(angle) * 3.0);
        for (int l = 1; l <= 3; ++l) {
            set_v(grid, 16 + dx*l, 3 + l/2, 16 + dz*l, V_LEAF);
            set_v(grid, 16 + dx*l, 4 + l/2, 16 + dz*l, V_LEAF);
        }
    }
    // Central leafy mass
    fill_box(grid, 14, 5, 14, 18, 10, 18, V_LEAF);
}

void build_palm(uint8_t *grid) {
    clear_grid(grid);
    // Tall thin trunk: 2x2 from Y=1 to Y=20
    fill_box(grid, 15, 1, 15, 16, 20, 16, V_WOOD);
    // Crown: ring of fronds at Y=20-24
    int cx = 15, cy = 21, cz = 15;
    for (int x = cx-4; x <= cx+4; ++x) {
        for (int y = cy-1; y <= cy+4; ++y) {
            for (int z = cz-4; z <= cz+4; ++z) {
                if (!in_bounds(x, y, z)) continue;
                double dx = (double)(x - cx);
                double dy = (double)(y - cy) * 1.5;
                double dz = (double)(z - cz);
                if (dx*dx + dy*dy + dz*dz <= 16.0) {
                    if (grid[vidx(x, y, z)] == V_AIR)
                        set_v(grid, x, y, z, V_LEAF);
                }
            }
        }
    }
}

void build_dead_tree(uint8_t *grid) {
    clear_grid(grid);
    // Trunk: 3x3 from Y=1 to Y=16
    fill_box(grid, 15, 1, 15, 17, 16, 17, V_WOOD);
    // 4 sparse brittle branches (all wood, no leaves)
    int branches[4][6] = {
        {16, 12, 16, 10, 12, 10},
        {16, 12, 16, 22, 12, 10},
        {16, 14, 16, 10, 14, 22},
        {16, 14, 16, 22, 14, 22}
    };
    for (int b = 0; b < 4; ++b) {
        int x0 = branches[b][0], y0 = branches[b][1], z0 = branches[b][2];
        int x1 = branches[b][3], y1 = branches[b][4], z1 = branches[b][5];
        int steps = max(max(abs(x1-x0), abs(y1-y0)), abs(z1-z0));
        for (int s = 0; s <= steps; ++s) {
            double t = (double)s / steps;
            int px = (int)round(x0 + t * (x1 - x0));
            int py = (int)round(y0 + t * (y1 - y0));
            int pz = (int)round(z0 + t * (z1 - z0));
            set_v(grid, px, py, pz, V_WOOD);
        }
    }
}

// ---------------------------------------------------------------------------
// Enumerate mutation candidates (solid, non-ground voxels)
// ---------------------------------------------------------------------------
vector<int> get_mutation_candidates(const uint8_t *grid) {
    vector<int> candidates;
    for (int i = 0; i < TOTAL_VOXELS; ++i) {
        if (is_solid(grid[i])) candidates.push_back(i);
    }
    return candidates;
}

// ---------------------------------------------------------------------------
// Stats helper
// ---------------------------------------------------------------------------
struct Stats {
    double mean, median, p95, p99, std, min_v, max_v;
};

Stats compute_stats(vector<double> &samples) {
    Stats s{};
    int n = (int)samples.size();
    if (n == 0) return s;

    sort(samples.begin(), samples.end());
    double sum = 0;
    for (double v : samples) sum += v;
    s.mean = sum / n;
    s.median = samples[n / 2];
    s.p95 = samples[(int)(n * 0.95)];
    s.p99 = samples[(int)(n * 0.99)];

    double sq = 0;
    for (double v : samples) sq += (v - s.mean) * (v - s.mean);
    s.std = sqrt(sq / n);
    s.min_v = samples.front();
    s.max_v = samples.back();
    return s;
}

// ---------------------------------------------------------------------------
// Compute accuracy between two detached masks
// ---------------------------------------------------------------------------
double compute_accuracy(const vector<bool> &a, const vector<bool> &b) {
    int correct = 0;
    for (int i = 0; i < TOTAL_VOXELS; ++i) {
        if (a[i] == b[i]) ++correct;
    }
    return (double)correct / TOTAL_VOXELS;
}

// ---------------------------------------------------------------------------
// Count voxels marked NOT-anchored (i.e., detached)
// The mask is the "anchored" mask returned by strategies: true = connected to ground.
// A voxel is detached if NOT anchored.
// ---------------------------------------------------------------------------
int count_detached(const vector<bool> &anchored_mask) {
    int c = 0;
    for (bool v : anchored_mask) if (!v) ++c;
    return c;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    ios::sync_with_stdio(false);
    cout << "Starting Voxel Vegetation Destruction Physics Benchmark..." << endl;

    uint8_t *grid = new uint8_t[TOTAL_VOXELS];

    vector<string> scenes = {
        "deciduous_tree", "coniferous_pine", "bush", "palm", "dead_tree"
    };
    vector<string> strategies = {
        "A_NaiveGlobalBFS", "B_HierarchicalDSU", "C_LocalSplitBFS",
        "D_LightweightStressTopple", "E_Hybrid_AABB"
    };

    // Strategy function pointers
    using StrategyFn = vector<bool>(*)(const uint8_t*);
    StrategyFn strategy_fns[5] = {
        strategy_A_naive_global_bfs,
        strategy_B_hierarchical_dsu,
        strategy_C_local_split_bfs,
        strategy_D_lightweight_stress_topple,
        strategy_E_hybrid_aabb
    };

    ofstream csv("results.csv");
    csv << "Scene,Strategy,Seed,MutationIdx,Mean_us,Median_us,p95_us,p99_us,Std_us,Accuracy,DetachedCount,ToppledFlag\n";

    unsigned int seeds[5] = {1, 7, 42, 1234, 31337};
    const int num_mutations = 8;
    const int iter_per_mutation = 50;

    // Build scene generators
    using BuilderFn = void(*)(uint8_t*);
    BuilderFn builders[5] = {
        build_deciduous_tree, build_coniferous_pine,
        build_bush, build_palm, build_dead_tree
    };

    for (int si = 0; si < 5; ++si) {
        cout << "\n--- Scene: " << scenes[si] << " ---" << endl;

        for (int seed_idx = 0; seed_idx < 5; ++seed_idx) {
            unsigned int seed = seeds[seed_idx];
            mt19937 rng(seed);

            // Build the scene fresh
            builders[si](grid);
            vector<int> candidates = get_mutation_candidates(grid);

            if (candidates.empty()) {
                cout << "  [SKIP] No mutation candidates for scene " << scenes[si] << endl;
                continue;
            }

            // Pick mutation indices with strategic trunk cuts:
            //   mutations 0,1: destroy 1 trunk-base voxel each (light damage)
            //   mutation 2: destroy 2 trunk-base voxels
            //   mutation 3: destroy 4 trunk-base voxels (significant weakening)
            //   mutation 4: destroy all trunk-base voxels at Y=1 (FELLING CUT — canopy detaches)
            //   mutations 5-7: random canopy damage
            sort(candidates.begin(), candidates.end(), [](int a, int b) {
                int ax, ay, az; vcoords(a, ax, ay, az);
                int bx, by, bz; vcoords(b, bx, by, bz);
                if (ay != by) return ay < by;
                int adx = abs(ax - 16), adz = abs(az - 16);
                int bdx = abs(bx - 16), bdz = abs(bz - 16);
                return (adx + adz) < (bdx + bdz);
            });

            // Collect trunk base voxels (Y == 1, in the trunk region).
            // Each scene has its own trunk footprint; for now we use the central
            // 5x5 region around (16,1,16) which covers all scene trunks
            // (trunks span 1x1 to 4x4 centred on (16,1,16)).
            vector<int> trunk_base;
            for (int idx : candidates) {
                int cx, cy, cz; vcoords(idx, cx, cy, cz);
                if (cy == 1 && cx >= 13 && cx <= 18 && cz >= 13 && cz <= 18) {
                    trunk_base.push_back(idx);
                }
            }

            // Mutation plan: each "mutation" deletes a batch of voxels.
//   mutation 0: 1 trunk_base voxel (light damage)
//   mutation 1: +2 trunk_base voxels (cumulative 3)
//   mutation 2: +4 trunk_base voxels (cumulative 7)
//   mutation 3: +ALL trunk_base voxels (cumulative 16 = FELLING CUT)
//   mutation 4-7: 1 canopy voxel each
vector<vector<int>> mutations;
int cumulative = 0;
int cuts[4] = {1, 3, 7, (int)trunk_base.size()};
for (int phase = 0; phase < 4; ++phase) {
    vector<int> batch;
    int target = min(cuts[phase], (int)trunk_base.size());
    for (int j = cumulative; j < target; ++j) {
        batch.push_back(trunk_base[j]);
    }
    if (!batch.empty()) {
        mutations.push_back(batch);
        cumulative = target;
    }
}

// Canopy mutations: random canopy voxels
vector<int> remaining;
for (int i = 0; i < (int)candidates.size(); ++i) {
    bool used = false;
    for (int j = 0; j < (int)trunk_base.size(); ++j) {
        if (candidates[i] == trunk_base[j]) { used = true; break; }
    }
    if (!used) remaining.push_back(candidates[i]);
}
shuffle(remaining.begin(), remaining.end(), rng);
for (int m = 0; m < num_mutations - (int)mutations.size() && m < (int)remaining.size(); ++m) {
    mutations.push_back(vector<int>{remaining[m]});
}

int actual_mutations = (int)mutations.size();

            // Accumulate mutations: apply them sequentially
            for (int m = 0; m < actual_mutations; ++m) {
                // Destroy all voxels in this mutation batch
                for (int v : mutations[m]) {
                    grid[v] = V_AIR;
                }

                // Precompute baseline (strategy A) for accuracy comparison
                auto baseline = strategy_A_naive_global_bfs(grid);

                // Run all 5 strategies on the mutated grid
                for (int strat_idx = 0; strat_idx < 5; ++strat_idx) {
                    StrategyFn fn = strategy_fns[strat_idx];

                    // Single run to get the result (for accuracy + topple detection)
                    auto result = fn(grid);
                    double acc = compute_accuracy(result, baseline);
                    int det = count_detached(result);
                    int toppled = 0;
                    if (strat_idx == 3) {
                        // Count voxels D detached but A didn't
                        // (i.e., stress-triggered topples)
                        for (int i = 0; i < TOTAL_VOXELS; ++i) {
                            if (!result[i] && baseline[i]) toppled++;
                        }
                    }

                    // Warmup: 5 runs (not timed)
                    for (int w = 0; w < 5; ++w) {
                        volatile auto _ = fn(grid);
                        (void)_;
                    }

                    // Timed runs (collect timing only, result same every time)
                    vector<double> timings;
                    timings.reserve(iter_per_mutation);
                    for (int it = 0; it < iter_per_mutation; ++it) {
                        auto start = chrono::high_resolution_clock::now();
                        volatile auto _ = fn(grid);
                        (void)_;
                        auto end = chrono::high_resolution_clock::now();
                        double us = chrono::duration_cast<chrono::nanoseconds>(end - start).count() / 1000.0;
                        timings.push_back(us);
                    }

                    Stats stats = compute_stats(timings);
                    csv << scenes[si] << ","
                        << strategies[strat_idx] << ","
                        << seed << ","
                        << m << ","
                        << stats.mean << ","
                        << stats.median << ","
                        << stats.p95 << ","
                        << stats.p99 << ","
                        << stats.std << ","
                        << acc << ","
                        << det << ","
                        << toppled << "\n";
                }
                csv.flush();
            }
        }
    }

    delete[] grid;
    cout << "\nDone. Results written to results.csv" << endl;
    return 0;
}
