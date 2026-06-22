#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>
#include <map>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <chrono>

// === Voxel primitives ===

struct Voxel { uint8_t x, y, z, mat; };
using Tree = std::vector<Voxel>;

static constexpr uint8_t CHUNK = 8;
static constexpr uint8_t MAT_TRUNK = 1;
static constexpr uint8_t MAT_LEAF = 2;

// === PCG-style fast RNG ===

struct Rng {
    uint64_t state;
    explicit Rng(uint64_t seed) : state(seed) {}
    uint32_t next() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + 1442695040888963407ULL;
        uint32_t x = ((old >> 18u) ^ old) >> 27u;
        uint32_t rot = old >> 59u;
        return (x >> rot) | (x << ((-rot) & 31u));
    }
    float uniform() { return (next() >> 8) * 0x1.0p-24f; }
    float range(float lo, float hi) { return lo + uniform() * (hi - lo); }
    int irange(int lo, int hi) { return lo + int(uniform() * (hi - lo + 1)); }
};

// === Simple hash-based noise (not full simplex — good enough for guidance) ===

static float hash_noise3(int32_t x, int32_t y, int32_t z) {
    uint64_t h = uint64_t(x) * 374761393U + uint64_t(y) * 668265263U + uint64_t(z) * 1274126177U;
    h = (h ^ (h >> 13)) * 1274126177U;
    h = h ^ (h >> 16);
    return (h & 0xFFFFFF) * 0x1.0p-24f;
}

static float noise_guided(float x, float y, float z) {
    int32_t ix = int32_t(std::floor(x)), iy = int32_t(std::floor(y)), iz = int32_t(std::floor(z));
    float fx = x - ix, fy = y - iy, fz = z - iz;
    float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy), sz = fz * fz * (3 - 2 * fz);
    float n000 = hash_noise3(ix, iy, iz), n100 = hash_noise3(ix+1, iy, iz);
    float n010 = hash_noise3(ix, iy+1, iz), n110 = hash_noise3(ix+1, iy+1, iz);
    float n001 = hash_noise3(ix, iy, iz+1), n101 = hash_noise3(ix+1, iy, iz+1);
    float n011 = hash_noise3(ix, iy+1, iz+1), n111 = hash_noise3(ix+1, iy+1, iz+1);
    float nx00 = n000 + (n100 - n000) * sx, nx10 = n010 + (n110 - n010) * sx;
    float nx01 = n001 + (n101 - n001) * sx, nx11 = n011 + (n111 - n011) * sx;
    float nxy0 = nx00 + (nx10 - nx00) * sy, nxy1 = nx01 + (nx11 - nx01) * sy;
    return nxy0 + (nxy1 - nxy0) * sz;
}

// === Tree configuration per type ===

struct TreeConfig {
    std::string name;
    int trunk_h;
    int leaf_r;
    float canopy_y_offset;
    float taper;
    float branch_angle;
    float branch_prob;
    int lsys_iterations;
};

static const std::array<TreeConfig, 5> TREE_TYPES = {{
    {"oak",      3, 2, 3.0f, 0.7f, 30.0f, 0.6f, 3},
    {"pine",     4, 1, 4.0f, 0.8f, 60.0f, 0.4f, 4},
    {"palm",     4, 1, 4.0f, 0.9f, 75.0f, 0.3f, 3},
    {"dead",     2, 1, 2.0f, 0.6f, 25.0f, 0.2f, 2},
    {"bush",     1, 2, 1.0f, 0.5f, 35.0f, 0.8f, 2},
}};

// === Strategy A: TrunkOnly (baseline) ===

static Tree gen_trunk_only(const TreeConfig& cfg, uint64_t) {
    Tree t;
    int cx = CHUNK / 2, cz = CHUNK / 2;
    for (int y = 0; y < cfg.trunk_h; ++y)
        t.push_back({uint8_t(cx), uint8_t(y), uint8_t(cz), MAT_TRUNK});
    int cy = int(cfg.canopy_y_offset);
    for (int dx = -cfg.leaf_r; dx <= cfg.leaf_r; ++dx)
        for (int dy = -cfg.leaf_r; dy <= cfg.leaf_r; ++dy)
            for (int dz = -cfg.leaf_r; dz <= cfg.leaf_r; ++dz) {
                int d = dx*dx + dy*dy + dz*dz;
                if (d <= cfg.leaf_r * cfg.leaf_r + 1) {
                    int x = cx + dx, y = cy + dy, z = cz + dz;
                    if (x >= 0 && x < CHUNK && y >= 0 && y < CHUNK && z >= 0 && z < CHUNK)
                        t.push_back({uint8_t(x), uint8_t(y), uint8_t(z), MAT_LEAF});
                }
            }
    return t;
}

// === L-system internals ===

struct Turtle {
    float x, y, z, dx, dy, dz, w;
};

static void apply_tropism(float& dx, float& dy, float& dz, float strength) {
    dy += strength * 0.5f;
    float len = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (len > 0) { dx /= len; dy /= len; dz /= len; }
}

static void rotate_y(float& dx, float&, float& dz, float deg) {
    float rad = deg * 3.14159265f / 180.0f;
    float c = std::cos(rad), s = std::sin(rad);
    float ndx = dx * c - dz * s;
    float ndz = dx * s + dz * c;
    dx = ndx; dz = ndz;
}

static void rotate_x(float&, float& dy, float& dz, float deg) {
    float rad = deg * 3.14159265f / 180.0f;
    float c = std::cos(rad), s = std::sin(rad);
    float ndy = dy * c - dz * s;
    float ndz = dy * s + dz * c;
    dy = ndy; dz = ndz;
}

// === L-system building helper ===
// Rules encoded as direct C++ (not string-parsed) for speed.

struct LsysState {
    Turtle t;
    float length;
    float width;
    int depth;
};

static void lsys_grow(std::vector<LsysState>& stack, Tree& out,
                      const TreeConfig& cfg, Rng& rng, int max_depth) {
    if (stack.empty()) return;
    LsysState cur = stack.back(); stack.pop_back();
    if (cur.depth >= max_depth) return;

    float len = cur.length;
    float w = cur.width;
    float t_ang = cfg.branch_angle;
    float taper = cfg.taper;
    float p_branch = cfg.branch_prob;

    apply_tropism(cur.t.dx, cur.t.dy, cur.t.dz, 0.3f);

    int steps = std::max(1, int(len * 2));
    for (int s = 0; s < steps; ++s) {
        float frac = float(s) / steps;
        float cx = cur.t.x + cur.t.dx * frac;
        float cy = cur.t.y + cur.t.dy * frac;
        float cz = cur.t.z + cur.t.dz * frac;
        int ix = int(std::round(cx)), iy = int(std::round(cy)), iz = int(std::round(cz));
        if (ix >= 0 && ix < CHUNK && iy >= 0 && iy < CHUNK && iz >= 0 && iz < CHUNK) {
            int tw = std::max(1, int(w * (1 - frac * 0.5f)));
            for (int dwx = -tw/2; dwx <= tw/2; ++dwx)
                for (int dwz = -tw/2; dwz <= tw/2; ++dwz) {
                    int px = ix + dwx, pz = iz + dwz;
                    if (px >= 0 && px < CHUNK && pz >= 0 && pz < CHUNK)
                        out.push_back({uint8_t(px), uint8_t(iy), uint8_t(pz), MAT_TRUNK});
                }
        }
    }

    cur.t.x += cur.t.dx * len;
    cur.t.y += cur.t.dy * len;
    cur.t.z += cur.t.dz * len;

    // Sub-branches
    float next_len = len * taper;
    float next_w = w * taper;

    auto push_branch = [&](float ang_x, float ang_y) {
        if (next_len < 0.3f) return;
        Turtle bt = cur.t;
        rotate_x(bt.dx, bt.dy, bt.dz, ang_x);
        rotate_y(bt.dx, bt.dy, bt.dz, ang_y);
        stack.push_back({bt, next_len, next_w, cur.depth + 1});
    };

    if (cur.depth == 0) {
        if (rng.uniform() < p_branch)
            push_branch(-t_ang, 0);
        if (rng.uniform() < p_branch * 0.7f)
            push_branch(t_ang, 90);
        if (rng.uniform() < p_branch * 0.7f)
            push_branch(-t_ang, 180);
        if (rng.uniform() < p_branch * 0.5f)
            push_branch(t_ang, -90);
        lsys_grow(stack, out, cfg, rng, max_depth);
    } else if (cur.depth == 1) {
        if (rng.uniform() < p_branch * 0.8f)
            push_branch(-t_ang * 1.2f, 137.5f);
        if (rng.uniform() < p_branch * 0.8f)
            push_branch(t_ang * 1.2f, -137.5f);
        lsys_grow(stack, out, cfg, rng, max_depth);
    } else {
        if (rng.uniform() < p_branch * 0.5f) {
            Turtle bt = cur.t;
            rotate_x(bt.dx, bt.dy, bt.dz, -t_ang * rng.range(0.5f, 1.5f));
            rotate_y(bt.dx, bt.dy, bt.dz, rng.range(0, 360));
            stack.push_back({bt, next_len * 0.5f, next_w * 0.5f, cur.depth + 1});
        }
    }
}

// === Strategy B: LSystem_Deterministic ===

static Tree gen_lsystem_deterministic(const TreeConfig& cfg, uint64_t) {
    Tree t;
    int cx = CHUNK / 2, cz = CHUNK / 2;
    // Fixed seed for determinism
    Rng rng(42);
    float start_len = std::max(1.0f, float(cfg.trunk_h) * 0.4f);
    std::vector<LsysState> stack;
    Turtle turtle{float(cx), 0, float(cz), 0, 1, 0, 1.0f};
    stack.push_back({turtle, start_len, 1.5f, 0});
    lsys_grow(stack, t, cfg, rng, cfg.lsys_iterations);

    // Add leaves at terminal branch tips
    int cy = int(cfg.canopy_y_offset);
    for (int dx = -cfg.leaf_r; dx <= cfg.leaf_r; ++dx)
        for (int dy = -cfg.leaf_r; dy <= cfg.leaf_r; ++dy)
            for (int dz = -cfg.leaf_r; dz <= cfg.leaf_r; ++dz) {
                int d = dx*dx + dy*dy + dz*dz;
                if (d <= cfg.leaf_r * cfg.leaf_r + 1) {
                    int x = cx + dx, y = cy + dy, z = cz + dz;
                    if (x >= 0 && x < CHUNK && y >= 0 && y < CHUNK && z >= 0 && z < CHUNK)
                        t.push_back({uint8_t(x), uint8_t(y), uint8_t(z), MAT_LEAF});
                }
            }
    return t;
}

// === Strategy C: LSystem_Stochastic ===

static Tree gen_lsystem_stochastic(const TreeConfig& cfg, uint64_t seed) {
    Tree t;
    int cx = CHUNK / 2, cz = CHUNK / 2;
    Rng rng(seed);
    float start_len = std::max(1.0f, float(cfg.trunk_h) * 0.4f);
    std::vector<LsysState> stack;
    Turtle turtle{float(cx), 0, float(cz), 0, 1, 0, 1.0f};
    stack.push_back({turtle, start_len, 1.5f, 0});
    lsys_grow(stack, t, cfg, rng, cfg.lsys_iterations);

    // Stochastic leaf placement
    int cy = int(cfg.canopy_y_offset) + rng.irange(-1, 1);
    int lr = std::max(1, cfg.leaf_r + rng.irange(-1, 1));
    for (int dx = -lr; dx <= lr; ++dx)
        for (int dy = -lr; dy <= lr; ++dy)
            for (int dz = -lr; dz <= lr; ++dz) {
                int d = dx*dx + dy*dy + dz*dz;
                if (d <= lr * lr + 1 && rng.uniform() < 0.85f) {
                    int x = cx + dx, y = cy + dy, z = cz + dz;
                    if (x >= 0 && x < CHUNK && y >= 0 && y < CHUNK && z >= 0 && z < CHUNK)
                        t.push_back({uint8_t(x), uint8_t(y), uint8_t(z), MAT_LEAF});
                }
            }
    return t;
}

// === Strategy D: Space Colonization ===

struct AttrPoint {
    float x, y, z;
    bool active;
};

static Tree gen_space_colonization(const TreeConfig& cfg, uint64_t seed) {
    Tree t;
    int cx = CHUNK / 2, cz = CHUNK / 2;
    Rng rng(seed);

    // Attraction points in ellipsoid
    std::vector<AttrPoint> points;
    int n_points = 20 + cfg.trunk_h * 8;
    float rx = 2.0f + cfg.leaf_r * 0.8f;
    float ry = 1.5f + cfg.trunk_h * 0.3f;
    float rz = 2.0f + cfg.leaf_r * 0.8f;
    float ceny = cfg.canopy_y_offset;

    for (int i = 0; i < n_points; ++i) {
        float px, py, pz;
        do {
            px = rng.range(-rx, rx);
            py = rng.range(-ry, ry);
            pz = rng.range(-rz, rz);
        } while ((px*px)/(rx*rx) + (py*py)/(ry*ry) + (pz*pz)/(rz*rz) > 1.0f);
        points.push_back({cx + px, ceny + py, cz + pz, true});
    }

    // Branch nodes
    struct BNode { float x, y, z; int parent; };
    std::vector<BNode> nodes;
    nodes.push_back({float(cx), 0, float(cz), -1});

    float seg_len = 0.6f + cfg.trunk_h * 0.1f;
    float infl_rad = 3.0f;
    float kill_rad = 1.0f;
    int max_iter = 80;

    // Simple spatial grid: CHUNK cells per dim (cell = 1 voxel)
    struct Cell { std::vector<int> node_indices; };
    auto cell_idx = [](float v) -> int { return int(std::floor(v)); };
    std::map<int, Cell> grid;

    auto add_to_grid = [&](int ni, float x, float y, float z) {
        int gx = cell_idx(x), gy = cell_idx(y), gz = cell_idx(z);
        int key = (gx & 7) | ((gy & 7) << 3) | ((gz & 7) << 6);
        grid[key].node_indices.push_back(ni);
    };
    add_to_grid(0, float(cx), 0, float(cz));

    for (int iter = 0; iter < max_iter && !points.empty(); ++iter) {
        std::vector<std::vector<int>> node_points(nodes.size());
        for (int pi = 0; pi < int(points.size()); ++pi) {
            if (!points[pi].active) continue;
            // Search in 3x3x3 cell neighborhood
            int gx = cell_idx(points[pi].x), gy = cell_idx(points[pi].y), gz = cell_idx(points[pi].z);
            float best_d = infl_rad * infl_rad;
            int best_ni = -1;
            for (int dx = -1; dx <= 1; ++dx)
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dz = -1; dz <= 1; ++dz) {
                        int key = ((gx+dx) & 7) | (((gy+dy) & 7) << 3) | (((gz+dz) & 7) << 6);
                        auto it = grid.find(key);
                        if (it == grid.end()) continue;
                        for (int ni : it->second.node_indices) {
                            if (ni >= int(nodes.size())) continue;
                            auto& n = nodes[ni];
                            float ddx = n.x - points[pi].x, ddy = n.y - points[pi].y, ddz = n.z - points[pi].z;
                            float d = ddx*ddx + ddy*ddy + ddz*ddz;
                            if (d < best_d) { best_d = d; best_ni = ni; }
                        }
                    }
            if (best_ni >= 0 && best_d > kill_rad * kill_rad)
                node_points[best_ni].push_back(pi);
        }

        bool grew = false;
        int pre_grow_count = int(nodes.size());
        for (int ni = 0; ni < pre_grow_count; ++ni) {
            auto& pts = node_points[ni];
            if (pts.empty()) continue;
            auto& n = nodes[ni];
            float dir_x = 0, dir_y = 0, dir_z = 0;
            for (int pi : pts) {
                auto& p = points[pi];
                float ddx = p.x - n.x, ddy = p.y - n.y, ddz = p.z - n.z;
                float d = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
                if (d > 0) { dir_x += ddx/d; dir_y += ddy/d; dir_z += ddz/d; }
                if (d < kill_rad) p.active = false;
            }

            float len = std::sqrt(dir_x*dir_x + dir_y*dir_y + dir_z*dir_z);
            if (len > 0) {
                apply_tropism(dir_x, dir_y, dir_z, 0.2f);
                dir_x /= len; dir_y /= len; dir_z /= len;
                float new_x = n.x + dir_x * seg_len;
                float new_y = n.y + dir_y * seg_len;
                float new_z = n.z + dir_z * seg_len;
                int new_ni = int(nodes.size());
                nodes.push_back({new_x, new_y, new_z, ni});
                add_to_grid(new_ni, new_x, new_y, new_z);
                grew = true;
            }
        }

        points.erase(std::remove_if(points.begin(), points.end(),
            [](auto& p) { return !p.active; }), points.end());

        if (!grew) break;
    }

    // Rasterize branches to voxels
    for (size_t i = 1; i < nodes.size(); ++i) {
        int parent = nodes[i].parent;
        if (parent < 0) continue;
        auto& p = nodes[parent];
        auto& c = nodes[i];
        int steps = std::max(2, int(std::sqrt(
            (c.x-p.x)*(c.x-p.x) + (c.y-p.y)*(c.y-p.y) + (c.z-p.z)*(c.z-p.z)) * 2));
        for (int s = 0; s <= steps; ++s) {
            float frac = float(s) / steps;
            int ix = int(std::round(p.x + (c.x-p.x) * frac));
            int iy = int(std::round(p.y + (c.y-p.y) * frac));
            int iz = int(std::round(p.z + (c.z-p.z) * frac));
            if (ix >= 0 && ix < CHUNK && iy >= 0 && iy < CHUNK && iz >= 0 && iz < CHUNK)
                t.push_back({uint8_t(ix), uint8_t(iy), uint8_t(iz), MAT_TRUNK});
        }
    }

    // Leaves at terminal nodes (leaf nodes = no children)
    std::vector<bool> has_child(nodes.size(), false);
    for (size_t i = 1; i < nodes.size(); ++i)
        if (nodes[i].parent >= 0 && size_t(nodes[i].parent) < has_child.size())
            has_child[nodes[i].parent] = true;

    for (size_t i = 0; i < nodes.size(); ++i) {
        if (has_child[i]) continue;
        auto& n = nodes[i];
        int lr = std::max(1, cfg.leaf_r);
        for (int dx = -lr; dx <= lr; ++dx)
            for (int dy = -lr; dy <= lr; ++dy)
                for (int dz = -lr; dz <= lr; ++dz) {
                    int d = dx*dx + dy*dy + dz*dz;
                    if (d <= lr * lr) {
                        int x = int(std::round(n.x)) + dx;
                        int y = int(std::round(n.y)) + dy;
                        int z = int(std::round(n.z)) + dz;
                        if (x >= 0 && x < CHUNK && y >= 0 && y < CHUNK && z >= 0 && z < CHUNK)
                            t.push_back({uint8_t(x), uint8_t(y), uint8_t(z), MAT_LEAF});
                    }
                }
    }

    return t;
}

// === Strategy E: NoiseGuided_Growth ===

static Tree gen_noise_guided(const TreeConfig& cfg, uint64_t seed) {
    Tree t;
    int cx = CHUNK / 2, cz = CHUNK / 2;
    Rng rng(seed);
    float noise_scale = 2.0f + rng.uniform() * 2.0f;

    std::vector<LsysState> stack;
    float start_len = std::max(1.0f, float(cfg.trunk_h) * 0.35f);
    Turtle turtle{float(cx), 0, float(cz), 0, 1, 0, 1.5f};
    stack.push_back({turtle, start_len, 1.5f, 0});

    // Custom noise-guided growth (overrides lsys_grow direction with noise gradient)
    auto& s = stack;
    while (!s.empty()) {
        LsysState cur = s.back(); s.pop_back();
        if (cur.depth >= 6) continue;

        float len = cur.length;
        float w = cur.width;

        // Evaluate noise to steer direction
        float ox = cur.t.x + cur.t.y * 0.5f;
        float oy = cur.t.y + cur.t.z * 0.5f;
        float oz = cur.t.z + cur.t.x * 0.5f;
        float n = noise_guided(ox * noise_scale, oy * noise_scale, oz * noise_scale);

        float angle_offset = (n - 0.5f) * 60.0f;
        rotate_x(cur.t.dx, cur.t.dy, cur.t.dz, angle_offset * 0.3f);
        rotate_y(cur.t.dx, cur.t.dy, cur.t.dz, angle_offset * 0.5f);
        apply_tropism(cur.t.dx, cur.t.dy, cur.t.dz, 0.3f);

        // Draw branch segment
        int steps = std::max(2, int(len * 3));
        for (int s = 0; s < steps; ++s) {
            float frac = float(s) / steps;
            float pf = frac * len;
            float px = cur.t.x + cur.t.dx * pf;
            float py = cur.t.y + cur.t.dy * pf;
            float pz = cur.t.z + cur.t.dz * pf;
            // Add thickness
            float thick = w * (1 - frac * 0.6f);
            int tw = std::max(1, int(thick));
            for (int dwx = -tw/2; dwx <= tw/2; ++dwx)
                for (int dwz = -tw/2; dwz <= tw/2; ++dwz) {
                    int ix = int(std::round(px)) + dwx;
                    int iy = int(std::round(py));
                    int iz = int(std::round(pz)) + dwz;
                    if (ix >= 0 && ix < CHUNK && iy >= 0 && iy < CHUNK && iz >= 0 && iz < CHUNK)
                        t.push_back({uint8_t(ix), uint8_t(iy), uint8_t(iz), MAT_TRUNK});
                }
        }

        // Branch tip advanced
        cur.t.x += cur.t.dx * len;
        cur.t.y += cur.t.dy * len;
        cur.t.z += cur.t.dz * len;

        float next_len = len * cfg.taper;
        float next_w = w * cfg.taper;
        if (next_len < 0.3f) continue;

        // Noise-guided branching: branch where noise is high
        float nb = noise_guided(cur.t.x * noise_scale, cur.t.y * noise_scale, cur.t.z * noise_scale);
        float branch_chance = cfg.branch_prob * (0.5f + nb);
        float rots[] = {0, 90, 180, -90};
        for (float rot : rots) {
            if (rng.uniform() >= branch_chance) continue;
            Turtle bt = cur.t;
            rotate_x(bt.dx, bt.dy, bt.dz, -cfg.branch_angle * rng.range(0.8f, 1.2f));
            rotate_y(bt.dx, bt.dy, bt.dz, rot + rng.range(-20, 20));
            s.push_back({bt, next_len * rng.range(0.6f, 0.9f), next_w * 0.7f, cur.depth + 1});
        }
    }

    // Leaves: sparse noise-guided placement
    int cy = int(cfg.canopy_y_offset);
    for (int dx = -cfg.leaf_r; dx <= cfg.leaf_r; ++dx)
        for (int dy = -cfg.leaf_r; dy <= cfg.leaf_r; ++dy)
            for (int dz = -cfg.leaf_r; dz <= cfg.leaf_r; ++dz) {
                int d = dx*dx + dy*dy + dz*dz;
                if (d > cfg.leaf_r * cfg.leaf_r) continue;
                float nv = noise_guided((cx+dx)*0.5f, (cy+dy)*0.5f, (cz+dz)*0.5f);
                if (nv < 0.4f) continue;
                int x = cx + dx, y = cy + dy, z = cz + dz;
                if (x >= 0 && x < CHUNK && y >= 0 && y < CHUNK && z >= 0 && z < CHUNK)
                    t.push_back({uint8_t(x), uint8_t(y), uint8_t(z), MAT_LEAF});
            }

    return t;
}

// === Plausibility scoring ===

static double branch_coverage(const Tree& tree, const TreeConfig& cfg) {
    if (tree.empty()) return 0;
    int cx = CHUNK / 2, cz = CHUNK / 2;
    int cy = int(cfg.canopy_y_offset);
    // Crown AABB
    int x0 = cx - cfg.leaf_r, x1 = cx + cfg.leaf_r;
    int y0 = cy - cfg.leaf_r, y1 = cy + cfg.leaf_r;
    int z0 = cz - cfg.leaf_r, z1 = cz + cfg.leaf_r;
    int total = (x1-x0+1)*(y1-y0+1)*(z1-z0+1);
    if (total == 0) return 0;

    // Build occupancy grid
    std::vector<bool> occ(CHUNK*CHUNK*CHUNK, false);
    for (auto& v : tree) occ[v.x + v.y*CHUNK + v.z*CHUNK*CHUNK] = true;

    int filled = 0;
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z) {
                if (x >= 0 && x < CHUNK && y >= 0 && y < CHUNK && z >= 0 && z < CHUNK)
                    if (occ[x + y*CHUNK + z*CHUNK*CHUNK]) ++filled;
            }
    return double(filled) / total;
}

static double taper_ratio(const Tree& tree) {
    // trunk taper: measure trunk voxels at bottom vs top
    int bottom_count = 0, top_count = 0;
    int cx = CHUNK / 2, cz = CHUNK / 2;
    for (auto& v : tree) {
        if (v.mat != MAT_TRUNK) continue;
        if (v.x == cx && v.z == cz) {
            if (v.y == 0) ++bottom_count;
            if (v.y == 1) ++top_count;
        }
    }
    if (bottom_count == 0) return 0;
    return double(top_count) / bottom_count;
}

static double self_similarity(const Tree& tree) {
    // Measure length distribution of horizontal spans
    if (tree.empty()) return 0;
    std::vector<int> spans;
    for (int y = 0; y < CHUNK; ++y)
        for (int z = 0; z < CHUNK; ++z) {
            int run = 0;
            for (int x = 0; x < CHUNK; ++x) {
                bool has = false;
                for (auto& v : tree)
                    if (v.x == x && v.y == y && v.z == z) { has = true; break; }
                if (has) ++run;
                else { if (run > 1) spans.push_back(run); run = 0; }
            }
            if (run > 1) spans.push_back(run);
        }
    if (spans.size() < 2) return 0.5;
    double mean = std::accumulate(spans.begin(), spans.end(), 0.0) / spans.size();
    double var = 0;
    for (int s : spans) var += (s - mean) * (s - mean);
    var /= spans.size();
    return 1.0 / (1.0 + var / (mean * mean + 0.01));
}

static double plausibility_score(const Tree& tree, const TreeConfig& cfg) {
    double bc = branch_coverage(tree, cfg);
    double tr = taper_ratio(tree);
    double ss = self_similarity(tree);
    // Weighted combination: coverage matters most, then taper, then self-similarity
    return bc * 0.50 + tr * 0.25 + ss * 0.25;
}

// === Strategy dispatch ===

using StrategyFn = Tree (*)(const TreeConfig&, uint64_t);

struct Strategy {
    std::string name;
    StrategyFn fn;
};

static const std::array<Strategy, 5> STRATEGIES = {{
    {"A_TrunkOnly",     gen_trunk_only},
    {"B_LSysDet",       gen_lsystem_deterministic},
    {"C_LSysStoch",     gen_lsystem_stochastic},
    {"D_SpaceColonize", gen_space_colonization},
    {"E_NoiseGuided",   gen_noise_guided},
}};

// === Statistics ===

struct Stats {
    double mean, median, p95, stddev, min_val, max_val;
};

static Stats compute_stats(std::span<const double> data) {
    Stats s{};
    if (data.empty()) return s;
    std::vector<double> sorted(data.begin(), data.end());
    std::sort(sorted.begin(), sorted.end());
    s.min_val = sorted.front();
    s.max_val = sorted.back();
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[size_t(sorted.size() * 0.95)];
    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    s.mean = sum / sorted.size();
    double sq = 0;
    for (double v : sorted) sq += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(sq / sorted.size());
    return s;
}

// === Timer ===

using Clock = std::chrono::high_resolution_clock;

// === Main ===

int main() {
    std::FILE* csv = std::fopen("build/results.csv", "w");
    if (!csv) { std::fprintf(stderr, "Cannot open build/results.csv\n"); return 1; }

    std::fprintf(csv, "strategy,scene,seed,time_us_mean,time_us_p95,voxel_count,plausibility,"
                      "branch_coverage,taper_ratio,self_similarity\n");

    constexpr int WARMUP = 10;
    constexpr int MEASURE = 1000;

    for (auto& strat : STRATEGIES) {
        for (auto& scene : TREE_TYPES) {
            for (int seed = 0; seed < 5; ++seed) {
                // Warmup
                for (int w = 0; w < WARMUP; ++w) {
                    auto tree = strat.fn(scene, uint64_t(seed * 1000 + w));
                    (void)tree;
                }

                // Measure
                std::vector<double> times(MEASURE);
                for (int m = 0; m < MEASURE; ++m) {
                    auto start = Clock::now();
                    auto tree = strat.fn(scene, uint64_t(seed * 1000 + m));
                    auto end = Clock::now();
                    double us = std::chrono::duration<double, std::micro>(end - start).count();
                    times[m] = us;
                }

                // One more run for quality metrics (last seed config)
                auto tree = strat.fn(scene, uint64_t(seed * 1000 + 999));
                int voxel_count = int(tree.size());
                double bc = branch_coverage(tree, scene);
                double tr = taper_ratio(tree);
                double ss = self_similarity(tree);
                double plaus = plausibility_score(tree, scene);

                auto st = compute_stats(times);
                std::fprintf(csv, "%s,%s,%d,%.4f,%.4f,%d,%.4f,%.4f,%.4f,%.4f\n",
                    strat.name.c_str(), scene.name.c_str(), seed,
                    st.mean, st.p95, voxel_count, plaus, bc, tr, ss);
            }
        }
    }

    std::fclose(csv);

    // Print summary
    std::printf("%-18s %-10s %5s  %8s  %8s  %6s  %8s\n",
                "Strategy", "Scene", "Seed", "Mean_us", "P95_us", "Voxels", "Plaus");
    std::printf("%s\n", std::string(75, '-').c_str());

    // Re-read and print
    csv = std::fopen("build/results.csv", "r");
    if (csv) {
        char line[256];
        std::fgets(line, sizeof(line), csv); // skip header
        while (std::fgets(line, sizeof(line), csv)) {
            char strat[20], scene[20];
            int seed; double mu, p95, pl, bc, tr, ss;
            int vc;
            std::sscanf(line, "%[^,],%[^,],%d,%lf,%lf,%d,%lf,%lf,%lf,%lf",
                        strat, scene, &seed, &mu, &p95, &vc, &pl, &bc, &tr, &ss);
            std::printf("%-18s %-10s %5d  %8.4f  %8.4f  %6d  %8.4f\n",
                        strat, scene, seed, mu, p95, vc, pl);
        }
        std::fclose(csv);
    }

    return 0;
}
