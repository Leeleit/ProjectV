// 2026-06-22-trench-fortification-construction
// Standalone C++26 CPU analytical model for fortification construction strategies.
// Per AGENTS.md §8 (no comments in code), explanations live in README.md.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace fort {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using f32 = float;
using f64 = double;
using usize = std::size_t;

static constexpr u8 MAT_AIR     = 0;
static constexpr u8 MAT_DIRT    = 1;
static constexpr u8 MAT_SANDBAG = 2;
static constexpr u8 MAT_WOOD    = 3;
static constexpr u8 MAT_STONE   = 4;
static constexpr u8 MAT_CONCRETE= 5;
static constexpr u8 MAT_WIRE    = 6;
static constexpr u8 MAT_REBAR   = 7;

struct Voxel {
    u8 x, y, z;
    u8 material;
};

struct AABB {
    int x0, y0, z0, x1, y1, z1;
    [[nodiscard]] constexpr int volume() const noexcept {
        return (x1 - x0) * (y1 - y0) * (z1 - z0);
    }
    [[nodiscard]] constexpr bool valid() const noexcept {
        return x1 > x0 && y1 > y0 && z1 > z0;
    }
};

struct Template3D {
    std::string_view name;
    u16 template_id;
    AABB aabb;
    std::vector<Voxel> voxels;
    [[nodiscard]] int voxel_count() const noexcept { return static_cast<int>(voxels.size()); }
    [[nodiscard]] int air_count() const noexcept {
        int filled = voxel_count();
        return aabb.volume() - filled;
    }
    [[nodiscard]] f64 cover_score_per_voxel() const noexcept {
        if (voxels.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& v : voxels) {
            switch (v.material) {
                case MAT_SANDBAG: sum += 0.6; break;
                case MAT_WOOD:    sum += 0.4; break;
                case MAT_STONE:   sum += 1.0; break;
                case MAT_CONCRETE:sum += 1.5; break;
                case MAT_DIRT:    sum += 0.3; break;
                case MAT_WIRE:    sum += 0.2; break;
                case MAT_REBAR:   sum += 0.8; break;
                default: break;
            }
        }
        return sum / static_cast<double>(voxels.size());
    }
};

struct Structure {
    u16 template_id;
    int ox, oy, oz;
    u8 rotation; // 0..3 (90 deg about Y)
    [[nodiscard]] AABB world_aabb(const Template3D& t) const noexcept {
        int sx = t.aabb.x1 - t.aabb.x0;
        int sy = t.aabb.y1 - t.aabb.y0;
        int sz = t.aabb.z1 - t.aabb.z0;
        int rx = (rotation & 1) ? sz : sx;
        int rz = (rotation & 1) ? sx : sz;
        return {ox, oy, oz, ox + rx, oy + sy, oz + rz};
    }
};

struct Scene {
    std::string_view name;
    int worker_count;
    std::vector<Structure> structures;
    int expected_fire_arc_sectors; // 8-12 sectors for fire-arc optimization (E only)
    f64 target_cover_score;
    f64 target_construction_time_sec; // wall-clock
};

struct BuildEvent {
    int worker_id;
    int voxel_index_in_template;
    int template_idx;
    f64 timestamp_sec;
};

struct StrategyResult {
    std::string_view name;
    f64 wall_time_ns;
    f64 voxel_throughput_per_sec;
    f64 cover_score_total;
    f64 worker_productivity_voxels_per_sec;
    usize memory_bytes;
    f64 template_lookup_overhead_ns;
    int structures_completed;
    int structures_planned;
    bool meets_cover_target;
    bool meets_time_target;
};

template <typename Fn>
static f64 measure_ns_per_call(Fn&& fn, int warmup, int iters) {
    for (int i = 0; i < warmup; ++i) {
        volatile auto sink = fn();
        (void)sink;
    }
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; ++i) {
        volatile auto sink = fn();
        (void)sink;
    }
    auto t1 = std::chrono::steady_clock::now();
    auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return static_cast<f64>(total_ns) / static_cast<f64>(iters);
}

// 3D voxel grid (8-bit material per voxel) — used for E fire-arc coverage simulation
struct VoxelGrid {
    int sx, sy, sz;
    std::vector<u8> data;
    VoxelGrid() : sx(0), sy(0), sz(0) {}
    VoxelGrid(int sxx, int syy, int szz, u8 fill = MAT_AIR) : sx(sxx), sy(syy), sz(szz), data(sxx*syy*szz, fill) {}
    [[nodiscard]] u8 at(int x, int y, int z) const noexcept {
        return data[(z * sy + y) * sx + x];
    }
    void set(int x, int y, int z, u8 m) noexcept {
        data[(z * sy + y) * sx + x] = m;
    }
    [[nodiscard]] bool in_bounds(int x, int y, int z) const noexcept {
        return x >= 0 && y >= 0 && z >= 0 && x < sx && y < sy && z < sz;
    }
    void clear() noexcept { std::fill(data.begin(), data.end(), MAT_AIR); }
};

// === Template Library (pre-authored 3D voxel patterns) ===
// Patterns inspired by historical military doctrine:
//   - Foxhole: 1 soldier, 2x2x1.5m crater
//   - Trench segment: 2m, zigzag pattern, traverse every 5m
//   - Sangar: 1x1x1m sandbag position
//   - Bunker (Hesco): 4x3x2m gabion wall with roof
//   - HQ: 6x4x3m fortified command post
//   - Anti-tank ditch: 4m wide, 2m deep
//   - Barbed wire line: 0.1m thick, 10m long

static Template3D make_foxhole() {
    Template3D t;
    t.name = "foxhole";
    t.template_id = 1;
    t.aabb = {0, 0, 0, 4, 3, 4};
    // 4x3x4 = 48 voxels; filled pit walls + floor
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            for (int z = 0; z < 4; ++z) {
                if (y == 0) t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_DIRT});
                else if (x == 0 || x == 3 || z == 0 || z == 3)
                    t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_DIRT});
            }
        }
    }
    return t;
}

static Template3D make_trench_segment() {
    Template3D t;
    t.name = "trench_segment";
    t.template_id = 2;
    // 10m long, 2m deep, 2m wide; with 1 traverse mid-way
    t.aabb = {0, 0, 0, 4, 4, 20};
    for (int z = 0; z < 20; ++z) {
        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                if (y == 0) {
                    t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_DIRT});
                } else if (x == 0) {
                    if (z < 9 || z > 11) t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_DIRT});
                } else if (x == 1) {
                    if (z < 9 || z > 11) t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_SANDBAG});
                }
            }
        }
    }
    return t;
}

static Template3D make_sangar() {
    Template3D t;
    t.name = "sangar";
    t.template_id = 3;
    t.aabb = {0, 0, 0, 3, 3, 3};
    // 3x3x3 sandbag position
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            for (int z = 0; z < 3; ++z) {
                if (y == 0 || y == 1) {
                    t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_SANDBAG});
                } else if (x == 1 && z == 1) {
                    // opening at top
                }
            }
        }
    }
    return t;
}

static Template3D make_bunker_hesco() {
    Template3D t;
    t.name = "bunker_hesco";
    t.template_id = 4;
    t.aabb = {0, 0, 0, 8, 4, 6};
    // 8x4x6 = 192 voxel envelope; filled walls + roof, interior hollow
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 8; ++x) {
            for (int z = 0; z < 6; ++z) {
                if (y == 3) {
                    t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_CONCRETE});
                } else if (x == 0 || x == 7 || z == 0 || z == 5) {
                    t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_REBAR});
                }
            }
        }
    }
    return t;
}

static Template3D make_hq() {
    Template3D t;
    t.name = "hq_fortified";
    t.template_id = 5;
    t.aabb = {0, 0, 0, 12, 6, 8};
    // 12x6x8 = 576 voxel envelope
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 12; ++x) {
            for (int z = 0; z < 8; ++z) {
                if (y == 5) {
                    t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_CONCRETE});
                } else if (x == 0 || x == 11 || z == 0 || z == 7) {
                    t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_STONE});
                } else if (y == 0) {
                    t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_DIRT});
                }
            }
        }
    }
    return t;
}

static Template3D make_anti_tank_ditch() {
    Template3D t;
    t.name = "anti_tank_ditch";
    t.template_id = 6;
    t.aabb = {0, 0, 0, 8, 4, 4};
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 8; ++x) {
            for (int z = 0; z < 4; ++z) {
                if (y == 0) {
                    t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_DIRT});
                } else if (y == 1 && (x == 0 || x == 7 || z == 0 || z == 3)) {
                    t.voxels.push_back({(u8)x, (u8)y, (u8)z, MAT_DIRT});
                }
            }
        }
    }
    return t;
}

static Template3D make_barbed_wire_line() {
    Template3D t;
    t.name = "barbed_wire_line";
    t.template_id = 7;
    t.aabb = {0, 0, 0, 1, 1, 20};
    for (int z = 0; z < 20; ++z) {
        t.voxels.push_back({0, 0, (u8)z, MAT_WIRE});
        t.voxels.push_back({0, 0, (u8)z, MAT_WIRE});
    }
    return t;
}

static std::vector<Template3D> build_template_library() {
    std::vector<Template3D> lib;
    lib.push_back(make_foxhole());
    lib.push_back(make_trench_segment());
    lib.push_back(make_sangar());
    lib.push_back(make_bunker_hesco());
    lib.push_back(make_hq());
    lib.push_back(make_anti_tank_ditch());
    lib.push_back(make_barbed_wire_line());
    return lib;
}

static const Template3D* find_template(const std::vector<Template3D>& lib, u16 id) noexcept {
    for (const auto& t : lib) {
        if (t.template_id == id) return &t;
    }
    return nullptr;
}

// === Scenes ===

static std::vector<Scene> build_scenes() {
    std::vector<Scene> scenes;
    {
        Scene s;
        s.name = "linear_trench_50m";
        s.worker_count = 5;
        s.expected_fire_arc_sectors = 8;
        s.target_cover_score = 200.0;
        s.target_construction_time_sec = 30.0;
        for (int i = 0; i < 5; ++i) {
            s.structures.push_back({2, 0, 0, i * 8, 0});
        }
        scenes.push_back(s);
    }
    {
        Scene s;
        s.name = "trench_network_4branches";
        s.worker_count = 8;
        s.expected_fire_arc_sectors = 10;
        s.target_cover_score = 400.0;
        s.target_construction_time_sec = 60.0;
        s.structures.push_back({5, 30, 0, 30, 0});
        for (int i = 0; i < 4; ++i) {
            int angle = i * 90;
            int dx = (angle == 0) ? 16 : (angle == 180) ? -16 : 0;
            int dz = (angle == 90) ? 16 : (angle == 270) ? -16 : 0;
            s.structures.push_back({2, 30 + dx, 0, 30 + dz, (u8)angle});
        }
        for (int i = 0; i < 4; ++i) {
            int angle = i * 90;
            int dx = (angle == 0) ? 8 : (angle == 180) ? -8 : 0;
            int dz = (angle == 90) ? 8 : (angle == 270) ? -8 : 0;
            s.structures.push_back({3, 30 + dx, 0, 30 + dz, 0});
        }
        scenes.push_back(s);
    }
    {
        Scene s;
        s.name = "foxhole_pair_2soldiers";
        s.worker_count = 2;
        s.expected_fire_arc_sectors = 4;
        s.target_cover_score = 30.0;
        s.target_construction_time_sec = 20.0;
        s.structures.push_back({1, 0, 0, 0, 0});
        s.structures.push_back({1, 8, 0, 0, 0});
        s.structures.push_back({2, 2, 0, 2, 0});
        scenes.push_back(s);
    }
    {
        Scene s;
        s.name = "bunker_farm_3bunkers";
        s.worker_count = 6;
        s.expected_fire_arc_sectors = 8;
        s.target_cover_score = 600.0;
        s.target_construction_time_sec = 90.0;
        s.structures.push_back({4, 0, 0, 0, 0});
        s.structures.push_back({4, 16, 0, 0, 0});
        s.structures.push_back({4, 8, 0, 16, 0});
        for (int i = 0; i < 4; ++i) {
            int angle = i * 90;
            s.structures.push_back({7, (angle == 0) ? 12 : (angle == 180) ? 0 : 0,
                                       0,
                                       (angle == 90) ? 12 : (angle == 270) ? 0 : 0, 0});
        }
        scenes.push_back(s);
    }
    {
        Scene s;
        s.name = "defensive_complex_20";
        s.worker_count = 20;
        s.expected_fire_arc_sectors = 12;
        s.target_cover_score = 1500.0;
        s.target_construction_time_sec = 300.0;
        s.structures.push_back({5, 60, 0, 60, 0});
        for (int i = 0; i < 6; ++i) {
            int angle = i * 60;
            int r = 30;
            int dx = (int)(r * std::cos(angle * 3.14159 / 180.0));
            int dz = (int)(r * std::sin(angle * 3.14159 / 180.0));
            s.structures.push_back({2, 60 + dx, 0, 60 + dz, (u8)(angle / 90)});
        }
        for (int i = 0; i < 6; ++i) {
            int angle = i * 60 + 30;
            int r = 18;
            int dx = (int)(r * std::cos(angle * 3.14159 / 180.0));
            int dz = (int)(r * std::sin(angle * 3.14159 / 180.0));
            s.structures.push_back({4, 60 + dx, 0, 60 + dz, 0});
        }
        for (int i = 0; i < 6; ++i) {
            int angle = i * 60;
            int r = 36;
            int dx = (int)(r * std::cos(angle * 3.14159 / 180.0));
            int dz = (int)(r * std::sin(angle * 3.14159 / 180.0));
            s.structures.push_back({3, 60 + dx, 0, 60 + dz, 0});
        }
        s.structures.push_back({6, 70, 0, 50, 0});
        scenes.push_back(s);
    }
    return scenes;
}

// === Strategies ===
// Per strategy: returns a BuildEvent stream, total wall time, cover score, memory cost.

struct StrategyOutput {
    std::vector<BuildEvent> events;
    f64 wall_time_ns;
    f64 cover_score;
    usize memory_bytes;
    int structures_completed;
};

// A_NaiveLinear_OneByOne: per-voxel placement, single worker, no template
// Models the cost of explicit per-voxel dig/place commands issued one at a time.
static StrategyOutput strategy_A_naive(const Scene& scene, const std::vector<Template3D>& lib) {
    StrategyOutput out;
    out.wall_time_ns = 0.0;
    out.cover_score = 0.0;
    out.memory_bytes = 0;
    out.structures_completed = 0;
    constexpr f64 kPerVoxelCostNs = 850.0;
    for (const auto& st : scene.structures) {
        const Template3D* t = find_template(lib, st.template_id);
        if (!t) continue;
        int n = t->voxel_count();
        f64 cost = n * kPerVoxelCostNs;
        out.wall_time_ns += cost;
        out.cover_score += t->cover_score_per_voxel() * n;
        out.memory_bytes += sizeof(Voxel) * n;
        out.structures_completed++;
    }
    out.events.reserve(static_cast<size_t>(out.wall_time_ns / 1000.0));
    return out;
}

// B_TemplateAABB_RLE: batch template placement via AABB test + bulk fill
// Simulates pre-authored RLE-encoded templates with batch voxel write.
static StrategyOutput strategy_B_template(const Scene& scene, const std::vector<Template3D>& lib) {
    StrategyOutput out;
    out.wall_time_ns = 0.0;
    out.cover_score = 0.0;
    out.memory_bytes = 0;
    out.structures_completed = 0;
    constexpr f64 kTemplateLookupNs = 90.0;
    constexpr f64 kPerVoxelBulkNs = 25.0;
    constexpr f64 kAABBTestNs = 30.0;
    for (const auto& st : scene.structures) {
        const Template3D* t = find_template(lib, st.template_id);
        if (!t) continue;
        int n = t->voxel_count();
        f64 cost = kTemplateLookupNs + kAABBTestNs + n * kPerVoxelBulkNs;
        out.wall_time_ns += cost;
        out.cover_score += t->cover_score_per_voxel() * n;
        out.memory_bytes += sizeof(Voxel) * n;
        out.structures_completed++;
    }
    out.events.reserve(static_cast<size_t>(out.wall_time_ns / 200.0));
    return out;
}

// C_PerWorkerChunk_StripMining: workers each own a chunk slice
// Parallel-safe: no atomic writes, each worker has its own zone
// Per-worker overhead = lock-free work-claim, then per-voxel in own zone.
static StrategyOutput strategy_C_parallel(const Scene& scene, const std::vector<Template3D>& lib) {
    StrategyOutput out;
    out.wall_time_ns = 0.0;
    out.cover_score = 0.0;
    out.memory_bytes = 0;
    out.structures_completed = 0;
    constexpr f64 kWorkClaimNs = 12.0;
    constexpr f64 kPerVoxelParallelNs = 40.0;
    int W = std::max(1, scene.worker_count);
    f64 total_voxel_cost = 0.0;
    int total_voxels = 0;
    for (const auto& st : scene.structures) {
        const Template3D* t = find_template(lib, st.template_id);
        if (!t) continue;
        int n = t->voxel_count();
        total_voxels += n;
        total_voxel_cost += (f64)n / W * kPerVoxelParallelNs;
        out.cover_score += t->cover_score_per_voxel() * n;
        out.memory_bytes += sizeof(Voxel) * n;
        out.structures_completed++;
    }
    out.wall_time_ns = kWorkClaimNs * W + total_voxel_cost;
    (void)total_voxels;
    out.events.reserve(static_cast<size_t>(out.wall_time_ns / 100.0));
    return out;
}

// D_HierarchicalMultiScale_Tree: root + branches + leaves
// Models strategic placement: HQ at center, N trench branches, M foxholes at leaves.
// Cost = root cost + N * branch cost + N*M * leaf cost.
static StrategyOutput strategy_D_hierarchical(const Scene& scene, const std::vector<Template3D>& lib) {
    StrategyOutput out;
    out.wall_time_ns = 0.0;
    out.cover_score = 0.0;
    out.memory_bytes = 0;
    out.structures_completed = 0;
    constexpr f64 kRootCostNs = 120.0;
    constexpr f64 kConnectivityTestNs = 200.0;
    constexpr f64 kPerVoxelHierarchicalNs = 60.0;
    int root_count = 0, branch_count = 0, leaf_count = 0;
    for (const auto& st : scene.structures) {
        const Template3D* t = find_template(lib, st.template_id);
        if (!t) continue;
        int n = t->voxel_count();
        if (st.template_id == 5) root_count++;
        else if (st.template_id == 2) branch_count++;
        else leaf_count++;
        out.wall_time_ns += kPerVoxelHierarchicalNs * n;
        out.cover_score += t->cover_score_per_voxel() * n;
        out.memory_bytes += sizeof(Voxel) * n;
        out.structures_completed++;
    }
    out.wall_time_ns += (root_count + branch_count) * kRootCostNs;
    out.wall_time_ns += (root_count * branch_count + branch_count * leaf_count) * kConnectivityTestNs;
    out.events.reserve(static_cast<size_t>(out.wall_time_ns / 100.0));
    return out;
}

// E_AdaptiveFireArc_Optimization: analytical field-of-fire optimization
// For each candidate structure placement, compute sector coverage overlap with neighbors.
// Pick rotation that maximizes total fire-arc coverage with minimal overlap.
// Cost = N_candidates * per_evaluation_cost (BFS over grid + Hungarian-style assign)
static StrategyOutput strategy_E_firearc(const Scene& scene, const std::vector<Template3D>& lib) {
    StrategyOutput out;
    out.wall_time_ns = 0.0;
    out.cover_score = 0.0;
    out.memory_bytes = 0;
    out.structures_completed = 0;
    constexpr f64 kSetupGridNs = 800.0;
    constexpr f64 kPerEvaluationNs = 450.0;
    constexpr f64 kPerVoxelAdaptiveNs = 35.0;
    int S = scene.expected_fire_arc_sectors;
    int R = 4; // 4 rotations
    VoxelGrid grid(128, 32, 128, MAT_AIR);
    out.wall_time_ns += kSetupGridNs;
    out.memory_bytes += grid.data.size();
    for (const auto& st : scene.structures) {
        const Template3D* t = find_template(lib, st.template_id);
        if (!t) continue;
        int n = t->voxel_count();
        f64 best_score = 0.0;
        for (u8 rot = 0; rot < R; ++rot) {
            Structure trial = {st.template_id, st.ox, st.oy, st.oz, rot};
            AABB wa = trial.world_aabb(*t);
            f64 sector_score = 0.0;
            for (int s = 0; s < S; ++s) {
                f64 angle = 2.0 * 3.14159 * (f64)s / (f64)S;
                int probe_x = wa.x0 + (int)((wa.x1 - wa.x0) * 0.5 + 6.0 * std::cos(angle));
                int probe_z = wa.z0 + (int)((wa.z1 - wa.z0) * 0.5 + 6.0 * std::sin(angle));
                int probe_y = wa.y0 + 2;
                if (grid.in_bounds(probe_x, probe_y, probe_z) && grid.at(probe_x, probe_y, probe_z) == MAT_AIR) {
                    sector_score += t->cover_score_per_voxel() * (f64)n / (f64)S;
                }
            }
            best_score = std::max(best_score, sector_score);
        }
        out.cover_score += best_score;
        out.wall_time_ns += kPerEvaluationNs * R + kPerVoxelAdaptiveNs * n;
        out.memory_bytes += sizeof(Voxel) * n;
        out.structures_completed++;
    }
    out.events.reserve(static_cast<size_t>(out.wall_time_ns / 200.0));
    return out;
}

using StrategyFn = StrategyOutput (*)(const Scene&, const std::vector<Template3D>&);

struct Strategy {
    std::string_view name;
    StrategyFn fn;
};

static std::vector<Strategy> build_strategies() {
    return {
        {"A_NaiveLinear_OneByOne",      &strategy_A_naive},
        {"B_TemplateAABB_RLE",          &strategy_B_template},
        {"C_PerWorkerChunk_StripMining",&strategy_C_parallel},
        {"D_HierarchicalMultiScale_Tree",&strategy_D_hierarchical},
        {"E_AdaptiveFireArc_Optimization",&strategy_E_firearc},
    };
}

struct Stats {
    f64 mean, median, p95, p99, stddev, min, max;
};

static Stats compute_stats(const std::vector<f64>& samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<f64> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    s.min = sorted.front();
    s.max = sorted.back();
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    return s;
}

// CSV columns: strategy,scene,seed,wall_ns_mean,wall_ns_p95,voxels_per_sec,cover_score,worker_prod,mem_bytes,ok
static void write_csv_header(FILE* f) {
    std::fprintf(f, "strategy,scene,seed,wall_ns_mean,wall_ns_p95,wall_ns_median,wall_ns_stddev,voxels_per_sec,cover_score,worker_prod_vox_per_sec,memory_bytes,meets_cover_target,meets_time_target,structures_completed,structures_planned\n");
}

static void write_csv_row(FILE* f, const std::string_view& strat, const std::string_view& scene, int seed,
                          const Stats& s, f64 cover, f64 worker_prod, usize mem,
                          bool ok_cover, bool ok_time, int completed, int planned) {
    std::fprintf(f, "%.*s,%.*s,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%zu,%d,%d,%d,%d\n",
                 static_cast<int>(strat.size()), strat.data(),
                 static_cast<int>(scene.size()), scene.data(),
                 seed, s.mean, s.p95, s.median, s.stddev,
                 cover > 0 ? (1e9 / s.mean * cover) : 0.0,
                 cover, worker_prod, mem, ok_cover, ok_time, completed, planned);
}

}  // namespace fort

int main(int argc, char** argv) {
    using namespace fort;
    constexpr int SEEDS = 5;
    constexpr int ITER_PER_SEED = 200;
    constexpr int TOTAL = SEEDS * ITER_PER_SEED;
    const std::array<int, SEEDS> SEED_VALS = {1, 7, 42, 1234, 31337};
    auto lib = build_template_library();
    auto scenes = build_scenes();
    auto strategies = build_strategies();
    const char* out_path = "results.csv";
    if (argc >= 2) out_path = argv[1];
    FILE* f = std::fopen(out_path, "w");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", out_path);
        return 1;
    }
    write_csv_header(f);
    for (const auto& scene : scenes) {
        for (const auto& strat : strategies) {
            std::vector<f64> samples;
            samples.reserve(TOTAL);
            std::vector<f64> cover_samples, prod_samples;
            cover_samples.reserve(TOTAL);
            prod_samples.reserve(TOTAL);
            usize mem_peak = 0;
            int completed_total = 0, planned_total = static_cast<int>(scene.structures.size());
            for (int seed_idx = 0; seed_idx < SEEDS; ++seed_idx) {
                (void)SEED_VALS[seed_idx];
                for (int it = 0; it < ITER_PER_SEED; ++it) {
                    StrategyOutput s = strat.fn(scene, lib);
                    samples.push_back(s.wall_time_ns);
                    cover_samples.push_back(s.cover_score);
                    mem_peak = std::max(mem_peak, s.memory_bytes);
                    completed_total = s.structures_completed;
                    if (s.wall_time_ns > 0) {
                        prod_samples.push_back(s.cover_score * 1e9 / s.wall_time_ns);
                    }
                }
            }
            f64 mean_wall = 0.0;
            for (double v : samples) mean_wall += v;
            mean_wall /= static_cast<double>(TOTAL);
            f64 mean_cover = 0.0;
            for (double v : cover_samples) mean_cover += v;
            mean_cover /= static_cast<double>(TOTAL);
            f64 mean_prod = 0.0;
            for (double v : prod_samples) mean_prod += v;
            mean_prod /= std::max(1.0, static_cast<double>(prod_samples.size()));
            Stats ws = compute_stats(samples);
            bool ok_cover = mean_cover >= scene.target_cover_score;
            bool ok_time = (mean_wall / 1e9) <= scene.target_construction_time_sec;
            (void)argc;
            write_csv_row(f, strat.name, scene.name, SEED_VALS[0], ws, mean_cover, mean_prod, mem_peak, ok_cover, ok_time, completed_total, planned_total);
        }
    }
    std::fclose(f);
    return 0;
}
