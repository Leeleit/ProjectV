// formation_bench.cpp — 2026-06-21-group-formation-maneuver-axis
// Standalone C++26 CPU harness per benchmarks/methodology.md
// 6 strategies × 5 scenes × 4 unit_counts × 5 seeds × 1000 iter = 600,000 main measurements
// Reference: Reynolds 1987/1999, van den Berg ORCA 2008, Isla Halo 2 2005 (sources.md)
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
//        -o build/formation_bench formation_bench.cpp
// Run:   ./build/formation_bench > build/results.csv

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <random>
#include <span>
#include <string_view>
#include <vector>

namespace fb {

constexpr int kWarmup = 5;
constexpr int kIter = 100;
constexpr int kTicksPerRun = 30;
constexpr float kDt = 0.1F;
constexpr float kUnitRadius = 0.5F;
constexpr float kMoveSpeed = 3.0F;
constexpr float kWorldSize = 256.0F;

struct Vec2 {
    float x{};
    float y{};
    friend constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
    friend constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
    friend constexpr Vec2 operator*(Vec2 a, float s) noexcept { return {a.x * s, a.y * s}; }
    constexpr float dot(Vec2 o) const noexcept { return x * o.x + y * o.y; }
    constexpr float length() const noexcept { return std::sqrt(x * x + y * y); }
    constexpr Vec2 normalized() const noexcept {
        float l = length();
        if (l < 1e-6F) return {0.0F, 0.0F};
        return {x / l, y / l};
    }
};

class XorShift64 {
   public:
    explicit XorShift64(uint64_t s) : state_(s == 0 ? 0x9E3779B97F4A7C15ULL : s) {}
    uint64_t nextU64() noexcept {
        uint64_t x = state_;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        state_ = x;
        return x;
    }
    float nextF01() noexcept {
        return static_cast<float>(nextU64() >> 11) * (1.0F / 9007199254740992.0F);
    }
    float nextFRange(float a, float b) noexcept { return a + (b - a) * nextF01(); }
    int nextIRange(int a, int b) noexcept {
        return a + static_cast<int>(nextU64() % static_cast<uint64_t>(b - a + 1));
    }

   private:
    uint64_t state_;
};

enum class Scene : int {
    OpenPlains = 0,
    ForestScattered = 1,
    UrbanGrid = 2,
    HillTerrain = 3,
    DefensiveLine = 4,
};

enum class Strategy : int {
    ANaive = 0,
    BVirtualAnchor = 1,
    CHierarchical = 2,
    DPotential = 3,
    EOrcaSimple = 4,
    FHybrid = 5,
};

struct SceneObstacles {
    std::vector<Vec2> tree_positions;
    std::vector<std::array<Vec2, 2>> building_aabbs;
    std::vector<std::array<Vec2, 2>> hill_aabbs;
    float slope_penalty = 1.0F;
};

struct SceneDesc {
    SceneObstacles obs;
    Vec2 start{};
    Vec2 goal{};
};

SceneDesc buildScene(Scene s, uint64_t seed) {
    XorShift64 rng(seed);
    SceneDesc d;
    d.start = {kWorldSize * 0.1F, kWorldSize * 0.5F};
    d.goal = {kWorldSize * 0.9F, kWorldSize * 0.5F};
    switch (s) {
        case Scene::OpenPlains:
            break;
        case Scene::ForestScattered:
            for (int i = 0; i < 64; ++i) {
                d.obs.tree_positions.push_back(
                    {rng.nextFRange(20.0F, kWorldSize - 20.0F),
                     rng.nextFRange(20.0F, kWorldSize - 20.0F)});
            }
            break;
        case Scene::UrbanGrid:
            for (int gy = 0; gy < 4; ++gy) {
                for (int gx = 0; gx < 4; ++gx) {
                    float x = 50.0F + gx * 45.0F;
                    float y = 50.0F + gy * 45.0F;
                    d.obs.building_aabbs.push_back(
                        {{Vec2{x, y}, Vec2{x + 8.0F, y + 8.0F}}});
                }
            }
            break;
        case Scene::HillTerrain:
            for (int i = 0; i < 4; ++i) {
                float cx = rng.nextFRange(60.0F, kWorldSize - 60.0F);
                float cy = rng.nextFRange(60.0F, kWorldSize - 60.0F);
                d.obs.hill_aabbs.push_back(
                    {{Vec2{cx, cy}, Vec2{cx + 30.0F, cy + 30.0F}}});
            }
            d.obs.slope_penalty = 2.0F;
            break;
        case Scene::DefensiveLine:
            for (int i = 0; i < 32; ++i) {
                float x = kWorldSize * 0.6F;
                float y = 20.0F + i * 7.0F;
                d.obs.building_aabbs.push_back(
                    {{Vec2{x, y}, Vec2{x + 4.0F, y + 4.0F}}});
            }
            break;
    }
    return d;
}

Vec2 wedgeSlot(int idx, int n) {
    int row = 0;
    int pos = idx;
    while (pos >= 2 * row + 1 && row < 16) {
        pos -= 2 * row + 1;
        row += 1;
    }
    float spacing = 2.0F;
    float offset = static_cast<float>(pos - row) * spacing;
    float depth = static_cast<float>(row) * spacing * 0.866F;
    return {offset, -depth};
}

float pathCost(Vec2 a, Vec2 b, const SceneObstacles& obs) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float base = std::sqrt(dx * dx + dy * dy);
    for (const auto& t : obs.tree_positions) {
        float ddx = a.x - t.x;
        float ddy = a.y - t.y;
        if (ddx * ddx + ddy * ddy < 16.0F) base += 5.0F;
    }
    for (const auto& bb : obs.building_aabbs) {
        float cx = (bb[0].x + bb[1].x) * 0.5F;
        float cy = (bb[0].y + bb[1].y) * 0.5F;
        float ddx = a.x - cx;
        float ddy = a.y - cy;
        if (std::abs(ddx) < 6.0F && std::abs(ddy) < 6.0F) base += 8.0F;
    }
    for (const auto& h : obs.hill_aabbs) {
        float cx = (h[0].x + h[1].x) * 0.5F;
        float cy = (h[0].y + h[1].y) * 0.5F;
        float ddx = a.x - cx;
        float ddy = a.y - cy;
        if (std::abs(ddx) < 18.0F && std::abs(ddy) < 18.0F) base *= obs.slope_penalty;
    }
    return base;
}

struct SimResult {
    double total_ns = 0.0;
    double crossings = 0.0;
    double avg_path_quality = 0.0;
    int unit_count = 0;
};

bool blockedAt(Vec2 p, const SceneObstacles& obs) {
    for (const auto& bb : obs.building_aabbs) {
        if (p.x > bb[0].x - 1.0F && p.x < bb[1].x + 1.0F &&
            p.y > bb[0].y - 1.0F && p.y < bb[1].y + 1.0F) {
            return true;
        }
    }
    return false;
}

SimResult runNaive(int n, const SceneDesc& scene, uint64_t seed) {
    XorShift64 rng(seed);
    std::vector<Vec2> units(n);
    for (int i = 0; i < n; ++i) {
        units[i] = {scene.start.x + static_cast<float>(i) * 0.5F, scene.start.y};
    }
    std::vector<Vec2> target_slots(n);
    for (int i = 0; i < n; ++i) {
        Vec2 s = wedgeSlot(i, n);
        target_slots[i] = {scene.goal.x + s.x, scene.goal.y + s.y};
    }
    double crossings = 0.0;
    double path_quality = 0.0;
    auto t0 = std::chrono::steady_clock::now();
    for (int tick = 0; tick < kTicksPerRun; ++tick) {
        std::vector<Vec2> next = units;
        for (int i = 0; i < n; ++i) {
            Vec2 to_target = target_slots[i] - units[i];
            float dist = to_target.length();
            if (dist < 0.01F) continue;
            Vec2 dir = to_target * (1.0F / dist);
            Vec2 step = dir * kMoveSpeed * kDt;
            for (int attempt = 0; attempt < 4; ++attempt) {
                Vec2 cand = next[i] + step;
                if (!blockedAt(cand, scene.obs)) {
                    next[i] = cand;
                    break;
                }
                step = step * 0.5F;
            }
            float base = pathCost(units[i], target_slots[i], scene.obs);
            path_quality += (dist > 0.0F) ? base / dist : 0.0;
        }
        units = std::move(next);
    }
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            Vec2 d = units[i] - units[j];
            if (d.dot(d) < (2.0F * kUnitRadius) * (2.0F * kUnitRadius)) crossings += 1.0;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    SimResult r;
    r.total_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    r.crossings = crossings;
    r.avg_path_quality = path_quality / (kTicksPerRun * n);
    r.unit_count = n;
    return r;
}

SimResult runVirtualAnchor(int n, const SceneDesc& scene, uint64_t seed) {
    XorShift64 rng(seed);
    std::vector<Vec2> units(n);
    for (int i = 0; i < n; ++i) {
        units[i] = {scene.start.x + static_cast<float>(i) * 0.5F, scene.start.y};
    }
    Vec2 anchor = scene.start;
    Vec2 goal = scene.goal;
    std::vector<Vec2> slot_offsets(n);
    for (int i = 0; i < n; ++i) slot_offsets[i] = wedgeSlot(i, n);
    double crossings = 0.0;
    double path_quality = 0.0;
    auto t0 = std::chrono::steady_clock::now();
    for (int tick = 0; tick < kTicksPerRun; ++tick) {
        Vec2 to_goal = goal - anchor;
        float dist = to_goal.length();
        if (dist > 0.01F) {
            Vec2 step = to_goal.normalized() * kMoveSpeed * kDt;
            Vec2 cand = anchor + step;
            if (!blockedAt(cand, scene.obs)) anchor = cand;
        }
        std::vector<Vec2> next = units;
        for (int i = 0; i < n; ++i) {
            Vec2 target = anchor + slot_offsets[i];
            Vec2 to_target = target - units[i];
            float d = to_target.length();
            if (d < 0.01F) continue;
            Vec2 dir = to_target * (1.0F / d);
            Vec2 step = dir * kMoveSpeed * kDt;
            for (int attempt = 0; attempt < 4; ++attempt) {
                Vec2 cand = next[i] + step;
                if (!blockedAt(cand, scene.obs)) {
                    next[i] = cand;
                    break;
                }
                step = step * 0.5F;
            }
        }
        units = std::move(next);
        float base = pathCost(anchor, goal, scene.obs);
        path_quality += (dist > 0.0F) ? base / dist : 0.0;
    }
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            Vec2 d = units[i] - units[j];
            if (d.dot(d) < (2.0F * kUnitRadius) * (2.0F * kUnitRadius)) crossings += 1.0;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    SimResult r;
    r.total_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    r.crossings = crossings;
    r.avg_path_quality = path_quality / kTicksPerRun;
    r.unit_count = n;
    return r;
}

SimResult runHierarchical(int n, const SceneDesc& scene, uint64_t seed) {
    XorShift64 rng(seed);
    std::vector<Vec2> units(n);
    for (int i = 0; i < n; ++i) {
        units[i] = {scene.start.x + static_cast<float>(i) * 0.5F, scene.start.y};
    }
    int squads = std::max(1, n / 8);
    int fireteams = std::max(1, squads / 2);
    std::vector<Vec2> squad_anchors(squads, scene.start);
    std::vector<Vec2> ft_anchors(fireteams, scene.start);
    std::vector<Vec2> slot_offsets(n);
    for (int i = 0; i < n; ++i) slot_offsets[i] = wedgeSlot(i, n);
    double crossings = 0.0;
    double path_quality = 0.0;
    auto t0 = std::chrono::steady_clock::now();
    for (int tick = 0; tick < kTicksPerRun; ++tick) {
        Vec2 ft_goal = scene.goal;
        for (int f = 0; f < fireteams; ++f) {
            Vec2 to = ft_goal - ft_anchors[f];
            float d = to.length();
            if (d > 0.01F) {
                Vec2 step = to.normalized() * kMoveSpeed * kDt;
                Vec2 cand = ft_anchors[f] + step;
                if (!blockedAt(cand, scene.obs)) ft_anchors[f] = cand;
            }
        }
        for (int s = 0; s < squads; ++s) {
            int ft_idx = std::min(s / 2, fireteams - 1);
            Vec2 to = ft_anchors[ft_idx] - squad_anchors[s];
            float d = to.length();
            if (d > 0.01F) {
                Vec2 step = to.normalized() * kMoveSpeed * kDt;
                Vec2 cand = squad_anchors[s] + step;
                if (!blockedAt(cand, scene.obs)) squad_anchors[s] = cand;
            }
        }
        std::vector<Vec2> next = units;
        for (int i = 0; i < n; ++i) {
            int s = i / 8;
            if (s >= squads) s = squads - 1;
            Vec2 target = squad_anchors[s] + slot_offsets[i];
            Vec2 to_target = target - units[i];
            float d = to_target.length();
            if (d < 0.01F) continue;
            Vec2 dir = to_target * (1.0F / d);
            Vec2 step = dir * kMoveSpeed * kDt;
            for (int attempt = 0; attempt < 4; ++attempt) {
                Vec2 cand = next[i] + step;
                if (!blockedAt(cand, scene.obs)) {
                    next[i] = cand;
                    break;
                }
                step = step * 0.5F;
            }
        }
        units = std::move(next);
        path_quality += 1.0;
    }
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            Vec2 d = units[i] - units[j];
            if (d.dot(d) < (2.0F * kUnitRadius) * (2.0F * kUnitRadius)) crossings += 1.0;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    SimResult r;
    r.total_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    r.crossings = crossings;
    r.avg_path_quality = path_quality / kTicksPerRun;
    r.unit_count = n;
    return r;
}

SimResult runPotential(int n, const SceneDesc& scene, uint64_t seed) {
    XorShift64 rng(seed);
    std::vector<Vec2> units(n);
    std::vector<Vec2> velocities(n, {0.0F, 0.0F});
    for (int i = 0; i < n; ++i) {
        units[i] = {scene.start.x + static_cast<float>(i) * 0.5F, scene.start.y};
    }
    double crossings = 0.0;
    double path_quality = 0.0;
    float sep_radius = 2.0F * kUnitRadius;
    float sep_radius_sq = sep_radius * sep_radius;
    auto t0 = std::chrono::steady_clock::now();
    for (int tick = 0; tick < kTicksPerRun; ++tick) {
        std::vector<Vec2> next = units;
        for (int i = 0; i < n; ++i) {
            Vec2 sep{0.0F, 0.0F};
            Vec2 ali{0.0F, 0.0F};
            Vec2 coh{0.0F, 0.0F};
            int neighbor_count = 0;
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                Vec2 d = units[i] - units[j];
                float d2 = d.dot(d);
                if (d2 < sep_radius_sq && d2 > 1e-4F) {
                    sep = sep + d * (1.0F / std::sqrt(d2));
                }
                if (d2 < 100.0F) {
                    ali = ali + velocities[j];
                    coh = coh + units[j];
                    neighbor_count += 1;
                }
            }
            Vec2 goal = scene.goal - units[i];
            Vec2 goal_dir = goal.normalized();
            Vec2 accel = sep * 4.0F;
            if (neighbor_count > 0) {
                Vec2 ali_avg = ali * (1.0F / neighbor_count);
                Vec2 coh_avg = (coh * (1.0F / neighbor_count)) - units[i];
                accel = accel + ali_avg * 0.5F + coh_avg * 0.1F;
            }
            accel = accel + goal_dir * 1.5F;
            for (const auto& t : scene.obs.tree_positions) {
                Vec2 d = units[i] - t;
                float d2 = d.dot(d);
                if (d2 < 9.0F && d2 > 1e-4F) accel = accel + d * (1.0F / std::sqrt(d2)) * 6.0F;
            }
            for (const auto& bb : scene.obs.building_aabbs) {
                Vec2 closest{std::clamp(units[i].x, bb[0].x, bb[1].x),
                             std::clamp(units[i].y, bb[0].y, bb[1].y)};
                Vec2 d = units[i] - closest;
                if (d.dot(d) < 4.0F && d.dot(d) > 1e-4F) {
                    accel = accel + d * (1.0F / std::sqrt(d.dot(d))) * 12.0F;
                }
            }
            velocities[i] = velocities[i] + accel * kDt;
            float vl = velocities[i].length();
            if (vl > kMoveSpeed) velocities[i] = velocities[i] * (kMoveSpeed / vl);
            Vec2 step = velocities[i] * kDt;
            Vec2 cand = next[i] + step;
            if (!blockedAt(cand, scene.obs)) next[i] = cand;
        }
        units = std::move(next);
        path_quality += 1.0;
    }
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            Vec2 d = units[i] - units[j];
            if (d.dot(d) < (2.0F * kUnitRadius) * (2.0F * kUnitRadius)) crossings += 1.0;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    SimResult r;
    r.total_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    r.crossings = crossings;
    r.avg_path_quality = path_quality / kTicksPerRun;
    r.unit_count = n;
    return r;
}

SimResult runOrcaSimple(int n, const SceneDesc& scene, uint64_t seed) {
    XorShift64 rng(seed);
    std::vector<Vec2> units(n);
    std::vector<Vec2> velocities(n, {0.0F, 0.0F});
    std::vector<Vec2> preferred(n);
    for (int i = 0; i < n; ++i) {
        units[i] = {scene.start.x + static_cast<float>(i) * 0.5F, scene.start.y};
    }
    double crossings = 0.0;
    double path_quality = 0.0;
    float tau = 1.0F;
    auto t0 = std::chrono::steady_clock::now();
    for (int tick = 0; tick < kTicksPerRun; ++tick) {
        for (int i = 0; i < n; ++i) {
            Vec2 to_goal = scene.goal - units[i];
            preferred[i] = to_goal.normalized() * kMoveSpeed;
        }
        std::vector<Vec2> new_vel = preferred;
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                Vec2 rel_pos = units[j] - units[i];
                Vec2 rel_vel = preferred[i] - velocities[j];
                float dist = rel_pos.length();
                if (dist < 0.001F) continue;
                float combined_radius = 2.0F * kUnitRadius;
                if (rel_vel.dot(rel_vel) < 1e-6F) continue;
                std::array<float, 2> v = {rel_vel.x, rel_vel.y};
                std::array<float, 2> p = {rel_pos.x / dist, rel_pos.y / dist};
                float dot_vp = v[0] * p[0] + v[1] * p[1];
                float u = (combined_radius - dot_vp) / (v[0] * v[0] + v[1] * v[1]);
                if (u < 0.0F || u > tau) continue;
                std::array<float, 2> vo_boundary_point{u * v[0], u * v[1]};
                float proj = vo_boundary_point[0] * p[0] + vo_boundary_point[1] * p[1];
                if (proj > 0.0F) continue;
                float dist_to_line = std::sqrt(std::max(0.0F,
                    (vo_boundary_point[0] * vo_boundary_point[0] + vo_boundary_point[1] * vo_boundary_point[1]) -
                    proj * proj));
                if (dist_to_line > combined_radius) continue;
                std::array<float, 2> fix;
                if (dist_to_line < 1e-4F) {
                    fix = {p[1], -p[0]};
                } else {
                    fix = {(vo_boundary_point[0] - proj * p[0]) / dist_to_line,
                           (vo_boundary_point[1] - proj * p[1]) / dist_to_line};
                }
                float adjust = (combined_radius - dist_to_line) * 0.5F;
                new_vel[i] = new_vel[i] - Vec2{fix[0], fix[1]} * adjust;
                float vl = new_vel[i].length();
                if (vl > kMoveSpeed) new_vel[i] = new_vel[i] * (kMoveSpeed / vl);
            }
        }
        std::vector<Vec2> next = units;
        for (int i = 0; i < n; ++i) {
            velocities[i] = new_vel[i];
            Vec2 step = velocities[i] * kDt;
            Vec2 cand = next[i] + step;
            if (!blockedAt(cand, scene.obs)) next[i] = cand;
        }
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                Vec2 d = next[i] - next[j];
                if (d.dot(d) < (2.0F * kUnitRadius) * (2.0F * kUnitRadius)) crossings += 1.0;
            }
        }
        units = std::move(next);
        path_quality += 1.0;
    }
    auto t1 = std::chrono::steady_clock::now();
    SimResult r;
    r.total_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    r.crossings = crossings;
    r.avg_path_quality = path_quality / kTicksPerRun;
    r.unit_count = n;
    return r;
}

SimResult runHybrid(int n, const SceneDesc& scene, uint64_t seed) {
    XorShift64 rng(seed);
    std::vector<Vec2> units(n);
    for (int i = 0; i < n; ++i) {
        units[i] = {scene.start.x + static_cast<float>(i) * 0.5F, scene.start.y};
    }
    Vec2 anchor = scene.start;
    Vec2 goal = scene.goal;
    std::vector<Vec2> slot_offsets(n);
    for (int i = 0; i < n; ++i) slot_offsets[i] = wedgeSlot(i, n);
    std::vector<Vec2> velocities(n, {0.0F, 0.0F});
    double crossings = 0.0;
    double path_quality = 0.0;
    auto t0 = std::chrono::steady_clock::now();
    for (int tick = 0; tick < kTicksPerRun; ++tick) {
        Vec2 to_goal = goal - anchor;
        float dist = to_goal.length();
        if (dist > 0.01F) {
            Vec2 step = to_goal.normalized() * kMoveSpeed * kDt;
            Vec2 cand = anchor + step;
            if (!blockedAt(cand, scene.obs)) anchor = cand;
        }
        int local_count = 0;
        Vec2 center{0.0F, 0.0F};
        for (int i = 0; i < n; ++i) {
            center = center + units[i];
        }
        center = center * (1.0F / n);
        std::vector<Vec2> next = units;
        for (int i = 0; i < n; ++i) {
            Vec2 target = anchor + slot_offsets[i];
            Vec2 to_target = target - units[i];
            float d = to_target.length();
            Vec2 dir = (d > 0.01F) ? to_target * (1.0F / d) : Vec2{0.0F, 0.0F};
            Vec2 step = dir * kMoveSpeed * kDt;
            if (i % 4 == 0) {
                Vec2 repulse{0.0F, 0.0F};
                for (int j = 0; j < n; ++j) {
                    if (i == j) continue;
                    Vec2 dd = units[i] - units[j];
                    float d2 = dd.dot(dd);
                    if (d2 < 4.0F * kUnitRadius * kUnitRadius && d2 > 1e-4F) {
                        repulse = repulse + dd * (1.0F / std::sqrt(d2)) * 0.5F;
                    }
                }
                step = step + repulse * 2.0F;
            }
            for (int attempt = 0; attempt < 4; ++attempt) {
                Vec2 cand = next[i] + step;
                if (!blockedAt(cand, scene.obs)) {
                    next[i] = cand;
                    break;
                }
                step = step * 0.5F;
            }
        }
        units = std::move(next);
        float base = pathCost(anchor, goal, scene.obs);
        path_quality += (dist > 0.0F) ? base / dist : 0.0;
    }
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            Vec2 d = units[i] - units[j];
            if (d.dot(d) < (2.0F * kUnitRadius) * (2.0F * kUnitRadius)) crossings += 1.0;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    SimResult r;
    r.total_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    r.crossings = crossings;
    r.avg_path_quality = path_quality / kTicksPerRun;
    r.unit_count = n;
    return r;
}

SimResult runStrategy(Strategy s, int n, const SceneDesc& scene, uint64_t seed) {
    switch (s) {
        case Strategy::ANaive: return runNaive(n, scene, seed);
        case Strategy::BVirtualAnchor: return runVirtualAnchor(n, scene, seed);
        case Strategy::CHierarchical: return runHierarchical(n, scene, seed);
        case Strategy::DPotential: return runPotential(n, scene, seed);
        case Strategy::EOrcaSimple: return runOrcaSimple(n, scene, seed);
        case Strategy::FHybrid: return runHybrid(n, scene, seed);
    }
    return {};
}

std::string_view strategyName(Strategy s) {
    switch (s) {
        case Strategy::ANaive: return "A_Naive_PerUnit";
        case Strategy::BVirtualAnchor: return "B_VirtualAnchor_SlotGrid";
        case Strategy::CHierarchical: return "C_HierarchicalAnchor";
        case Strategy::DPotential: return "D_PotentialField_Reynolds";
        case Strategy::EOrcaSimple: return "E_ORCA_Simple";
        case Strategy::FHybrid: return "F_Hybrid_B_E";
    }
    return "?";
}

std::string_view sceneName(Scene s) {
    switch (s) {
        case Scene::OpenPlains: return "open_plains";
        case Scene::ForestScattered: return "forest_scattered";
        case Scene::UrbanGrid: return "urban_grid";
        case Scene::HillTerrain: return "hill_terrain";
        case Scene::DefensiveLine: return "defensive_line";
    }
    return "?";
}

}  // namespace fb

int main() {
    using namespace fb;
    std::printf(
        "strategy,scene,unit_count,seed,iter_idx,total_ns,crossings,path_quality,per_unit_ns\n");
    constexpr std::array<Strategy, 6> strategies = {
        Strategy::ANaive, Strategy::BVirtualAnchor, Strategy::CHierarchical,
        Strategy::DPotential, Strategy::EOrcaSimple, Strategy::FHybrid,
    };
    constexpr std::array<Scene, 5> scenes = {
        Scene::OpenPlains, Scene::ForestScattered, Scene::UrbanGrid,
        Scene::HillTerrain, Scene::DefensiveLine,
    };
    constexpr std::array<int, 4> unit_counts = {32, 64, 128, 256};
    constexpr std::array<uint64_t, 5> seeds = {1ULL, 7ULL, 42ULL, 1234ULL, 31337ULL};
    for (Strategy s : strategies) {
        for (Scene sc : scenes) {
            for (int n : unit_counts) {
                for (uint64_t base_seed : seeds) {
                    uint64_t scene_seed = base_seed * 1000003ULL + static_cast<uint64_t>(sc) * 1009ULL;
                    SceneDesc desc = buildScene(sc, scene_seed);
                    for (int w = 0; w < kWarmup; ++w) {
                        runStrategy(s, n, desc, base_seed + static_cast<uint64_t>(w));
                    }
                    for (int it = 0; it < kIter; ++it) {
                        uint64_t iter_seed = base_seed + static_cast<uint64_t>(it) * 17ULL;
                        SimResult r = runStrategy(s, n, desc, iter_seed);
                        double per_unit = (r.unit_count > 0)
                            ? r.total_ns / (static_cast<double>(kTicksPerRun) * r.unit_count)
                            : 0.0;
                        std::printf(
                            "%.*s,%.*s,%d,%llu,%d,%.2f,%.0f,%.4f,%.2f\n",
                            static_cast<int>(strategyName(s).size()), strategyName(s).data(),
                            static_cast<int>(sceneName(sc).size()), sceneName(sc).data(),
                            n, static_cast<unsigned long long>(base_seed), it,
                            r.total_ns, r.crossings, r.avg_path_quality, per_unit);
                    }
                }
            }
        }
    }
    return 0;
}
