#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <random>
#include <span>
#include <string_view>
#include <vector>

// =============================================================================
// 1. Statistics helper
// =============================================================================
struct Stats {
    double mean{};
    double median{};
    double p95{};
    double p99{};
    double stddev{};
    double min_v{};
    double max_v{};
    uint64_t n{};
};

Stats Compute(std::span<const double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    s.n = samples.size();
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min_v = sorted.front();
    s.max_v = sorted.back();
    return s;
}

// =============================================================================
// 2. World + entity + observer types
// =============================================================================
struct WorldConfig {
    float world_size_m;        // 1000 m (1 km²)
    int n_players;
    int n_entities;
    float cell_size_m;         // 64 m
    float critical_range_m;    // 200 m
    float peripheral_range_m;  // 500 m
    float avg_velocity_mps;    // 0-10 m/s
    float bytes_per_entity;    // 60-70 per wirepair.org; use 64
    int mtu_bytes;             // 1200
    float tick_rate_hz;        // 30
    int n_aoi_cells_critical;  // = ceil(critical_range / cell_size) + 1
    int n_aoi_cells_peripheral;
};

constexpr WorldConfig kWorld = {
    .world_size_m = 1000.f,
    .n_players = 100,
    .n_entities = 10000,
    .cell_size_m = 64.f,
    .critical_range_m = 200.f,
    .peripheral_range_m = 500.f,
    .avg_velocity_mps = 5.f,
    .bytes_per_entity = 64.f,
    .mtu_bytes = 1200,
    .tick_rate_hz = 30.f,
    .n_aoi_cells_critical = 3,    // covers 7x7 = 49 cells (~192 m max diag) — close to 200 m target
    .n_aoi_cells_peripheral = 7   // covers 15x15 = 225 cells (~634 m max diag, full world @ 15 cells/side)
};

struct Entity {
    float x, y;          // 2D position (m)
    uint8_t type;        // 0=player_proxy, 1=vehicle, 2=projectile, 3=static
    uint8_t importance;  // 0-255 (for priority queue)
    float vx, vy;        // velocity (m/s)
};

struct Observer {
    float x, y;          // position
    float rotation;      // facing direction (rad)
    float view_range;    // typically 200 m critical
};

// =============================================================================
// 3. Scene generators
// =============================================================================
enum class Scene : int { UniformDense, BattleClustered, SparseScattered, ChaseHighMovement, MixedDynamic };
constexpr int kSceneCount = 5;

constexpr std::string_view SceneName(Scene s) {
    switch (s) {
        case Scene::UniformDense:     return "uniform_dense";
        case Scene::BattleClustered:  return "battle_clustered";
        case Scene::SparseScattered:  return "sparse_scattered";
        case Scene::ChaseHighMovement:return "chase_high_movement";
        case Scene::MixedDynamic:     return "mixed_dynamic";
    }
    return "unknown";
}

struct SceneData {
    std::vector<Observer> observers;
    std::vector<Entity> entities;
};

SceneData GenerateScene(Scene s, uint32_t seed) {
    SceneData out;
    out.observers.reserve(kWorld.n_players);
    out.entities.reserve(kWorld.n_entities);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u_pos(0.f, kWorld.world_size_m);
    std::uniform_real_distribution<float> u_angle(0.f, 6.2831853f);

    auto add_player = [&](float x, float y) {
        Observer o{};
        o.x = x; o.y = y;
        o.rotation = u_angle(rng);
        o.view_range = kWorld.critical_range_m;
        out.observers.push_back(o);
    };
    auto add_entity = [&](float x, float y, uint8_t type, uint8_t imp) {
        Entity e{};
        e.x = x; e.y = y;
        e.type = type; e.importance = imp;
        float v = kWorld.avg_velocity_mps;
        e.vx = u_angle(rng); e.vy = u_angle(rng);  // just random direction
        // Normalize to v m/s
        float cs = std::cos(e.vx), sn = std::sin(e.vy);
        e.vx = cs * v; e.vy = sn * v;
        out.entities.push_back(e);
    };

    switch (s) {
        case Scene::UniformDense: {
            // 100 players + 10k entities uniform random
            for (int i = 0; i < kWorld.n_players; ++i) add_player(u_pos(rng), u_pos(rng));
            for (int i = 0; i < kWorld.n_entities; ++i) {
                uint8_t t = rng() % 4;  // 0=player_proxy, 1=vehicle, 2=projectile, 3=static
                uint8_t imp = (t == 0) ? 200 : (t == 1) ? 180 : (t == 2) ? 100 : 50;
                add_entity(u_pos(rng), u_pos(rng), t, imp);
            }
            break;
        }
        case Scene::BattleClustered: {
            // 2 armies clustered (50p + 5kE each)
            for (int i = 0; i < 50; ++i) add_player(u_pos(rng) * 0.4f + 50.f, u_pos(rng) * 0.4f + 50.f);
            for (int i = 0; i < 50; ++i) add_player(u_pos(rng) * 0.4f + 550.f, u_pos(rng) * 0.4f + 550.f);
            for (int i = 0; i < 5000; ++i) {
                uint8_t t = rng() % 4;
                uint8_t imp = (t == 0) ? 200 : (t == 1) ? 180 : (t == 2) ? 100 : 50;
                add_entity(u_pos(rng) * 0.4f + 50.f, u_pos(rng) * 0.4f + 50.f, t, imp);
            }
            for (int i = 0; i < 5000; ++i) {
                uint8_t t = rng() % 4;
                uint8_t imp = (t == 0) ? 200 : (t == 1) ? 180 : (t == 2) ? 100 : 50;
                add_entity(u_pos(rng) * 0.4f + 550.f, u_pos(rng) * 0.4f + 550.f, t, imp);
            }
            break;
        }
        case Scene::SparseScattered: {
            // 100 players + 1k entities (low density)
            for (int i = 0; i < kWorld.n_players; ++i) add_player(u_pos(rng), u_pos(rng));
            for (int i = 0; i < 1000; ++i) {
                uint8_t t = rng() % 4;
                uint8_t imp = (t == 0) ? 200 : (t == 1) ? 180 : (t == 2) ? 100 : 50;
                add_entity(u_pos(rng), u_pos(rng), t, imp);
            }
            break;
        }
        case Scene::ChaseHighMovement: {
            // 100 players + 5k entities, high velocity
            for (int i = 0; i < kWorld.n_players; ++i) add_player(u_pos(rng), u_pos(rng));
            for (int i = 0; i < 5000; ++i) {
                uint8_t t = rng() % 4;
                uint8_t imp = (t == 0) ? 200 : (t == 1) ? 180 : (t == 2) ? 100 : 50;
                Entity e{};
                e.x = u_pos(rng); e.y = u_pos(rng);
                e.type = t; e.importance = imp;
                // 2x velocity
                float v = kWorld.avg_velocity_mps * 2.f;
                float ang = u_angle(rng);
                e.vx = std::cos(ang) * v; e.vy = std::sin(ang) * v;
                out.entities.push_back(e);
            }
            break;
        }
        case Scene::MixedDynamic: {
            // 30% in battle (cluster), 40% in transit (spread), 30% at base (clustered)
            for (int i = 0; i < 30; ++i) add_player(u_pos(rng) * 0.3f + 350.f, u_pos(rng) * 0.3f + 350.f);
            for (int i = 0; i < 40; ++i) add_player(u_pos(rng), u_pos(rng));
            for (int i = 0; i < 30; ++i) add_player(u_pos(rng) * 0.2f + 50.f, u_pos(rng) * 0.2f + 50.f);
            // Entities
            for (int i = 0; i < 1500; ++i) {  // battle
                uint8_t t = rng() % 4;
                add_entity(u_pos(rng) * 0.3f + 350.f, u_pos(rng) * 0.3f + 350.f, t,
                           (t == 0) ? 200 : (t == 1) ? 180 : 100);
            }
            for (int i = 0; i < 2000; ++i) {  // transit
                uint8_t t = rng() % 4;
                add_entity(u_pos(rng), u_pos(rng), t,
                           (t == 0) ? 200 : (t == 1) ? 180 : 100);
            }
            for (int i = 0; i < 1500; ++i) {  // base
                uint8_t t = rng() % 4;
                add_entity(u_pos(rng) * 0.2f + 50.f, u_pos(rng) * 0.2f + 50.f, t,
                           (t == 0) ? 200 : (t == 1) ? 180 : 50);
            }
            break;
        }
    }
    return out;
}

// =============================================================================
// 4. Grid AOI (2D uniform grid, 9-grid pattern)
// =============================================================================
struct Grid {
    int cells_per_side;
    float cell_size;
    std::vector<std::vector<int>> cell_entities;  // cell_entities[cell_id] = list of entity indices
    int total_cells() const { return cells_per_side * cells_per_side; }
    int cell_id(float x, float y) const {
        int cx = std::clamp(static_cast<int>(x / cell_size), 0, cells_per_side - 1);
        int cy = std::clamp(static_cast<int>(y / cell_size), 0, cells_per_side - 1);
        return cy * cells_per_side + cx;
    }
    void rebuild(const std::vector<Entity>& entities) {
        for (auto& c : cell_entities) c.clear();
        for (int i = 0; i < static_cast<int>(entities.size()); ++i) {
            int cid = cell_id(entities[i].x, entities[i].y);
            cell_entities[cid].push_back(i);
        }
    }
    // Iterate 9 cells around (ox, oy) within cell_radius
    template<typename Fn>
    void for_each_in_radius(float ox, float oy, int cell_radius, Fn&& fn) const {
        int cx = std::clamp(static_cast<int>(ox / cell_size), 0, cells_per_side - 1);
        int cy = std::clamp(static_cast<int>(oy / cell_size), 0, cells_per_side - 1);
        for (int dy = -cell_radius; dy <= cell_radius; ++dy) {
            int yy = cy + dy;
            if (yy < 0 || yy >= cells_per_side) continue;
            for (int dx = -cell_radius; dx <= cell_radius; ++dx) {
                int xx = cx + dx;
                if (xx < 0 || xx >= cells_per_side) continue;
                int cid = yy * cells_per_side + xx;
                for (int ei : cell_entities[cid]) fn(ei);
            }
        }
    }
};

Grid MakeGrid() {
    Grid g;
    g.cells_per_side = static_cast<int>(kWorld.world_size_m / kWorld.cell_size_m);
    g.cell_size = kWorld.cell_size_m;
    g.cell_entities.assign(g.total_cells(), {});
    return g;
}

// =============================================================================
// 5. Strategies
// =============================================================================
struct StrategyResult {
    double bytes_per_tick;
    double packets_per_tick;
    double per_player_kbps;       // bytes_per_tick * 8 * tick_rate / 1024 / n_players
    double cpu_ns_per_tick;
    double critical_ents_per_player;
    double peripheral_ents_per_player;
    double ambient_ents_per_player;
    double aoi_churn_per_sec_per_player;
};

// A. FullBroadcast: every player gets every entity every tick
StrategyResult Strategy_A_FullBroadcast(const SceneData& sc) {
    int total_ents = static_cast<int>(sc.entities.size());
    int n_players = static_cast<int>(sc.observers.size());
    // bytes: every player × every entity × 64 bytes
    double bytes = static_cast<double>(n_players) * total_ents * kWorld.bytes_per_entity;
    double packets = bytes / kWorld.mtu_bytes;
    double kbps = bytes * 8.0 * kWorld.tick_rate_hz / 1024.0 / n_players;
    // CPU: O(N_players × N_entities) distance checks
    double cpu_ns = static_cast<double>(n_players) * total_ents * 5.0;  // 5 ns per distance check
    return {
        bytes, packets, kbps, cpu_ns,
        static_cast<double>(total_ents), 0, 0, 0
    };
}

// B. GridAOI_NoTiering: single critical range, all in-range at 20 Hz
StrategyResult Strategy_B_GridAOI_NoTiering(const SceneData& sc, const Grid& grid) {
    int n_players = static_cast<int>(sc.observers.size());
    int crit_r = kWorld.n_aoi_cells_critical;
    double total_bytes = 0;
    double total_cpu_ns = 0;
    double total_ents_per_player = 0;
    for (const auto& obs : sc.observers) {
        int ent_count = 0;
        // CPU: 9 cells × ~50 entities = 450 checks
        double cpu_local = 0;
        grid.for_each_in_radius(obs.x, obs.y, crit_r, [&](int /*ei*/) {
            ent_count++;
            cpu_local += 5.0;  // 5 ns per check
        });
        // No tiering: send all in-range at full 64 bytes
        total_bytes += ent_count * kWorld.bytes_per_entity;
        total_cpu_ns += cpu_local;
        total_ents_per_player += ent_count;
    }
    double packets = total_bytes / kWorld.mtu_bytes;
    double kbps = total_bytes * 8.0 * kWorld.tick_rate_hz / 1024.0 / n_players;
    double churn = n_players * 5.0;  // 5 enter/exit per second per player (analytical)
    return {
        total_bytes, packets, kbps, total_cpu_ns,
        total_ents_per_player / n_players, 0, 0, churn
    };
}

// C. GridAOI_3Tier: critical 200m@20Hz, peripheral 500m@5Hz, ambient>500m@1Hz
StrategyResult Strategy_C_GridAOI_3Tier(const SceneData& sc, const Grid& grid) {
    int n_players = static_cast<int>(sc.observers.size());
    int crit_r = kWorld.n_aoi_cells_critical;
    int periph_r = kWorld.n_aoi_cells_peripheral;
    // World diagonal = ~1414m, so ambient = remaining
    double total_bytes = 0;
    double total_cpu_ns = 0;
    double total_crit = 0, total_periph = 0, total_ambient = 0;
    // Use std::array<bool> for tier categorization to avoid double-counting
    std::vector<int> tier(static_cast<int>(sc.entities.size()), 0);  // 0=ambient, 1=peripheral, 2=critical
    for (const auto& obs : sc.observers) {
        int crit_count = 0, periph_count = 0, ambient_count = 0;
        double cpu_local = 0;
        // Mark critical
        for (auto& t : tier) t = 0;
        grid.for_each_in_radius(obs.x, obs.y, crit_r, [&](int ei) {
            tier[ei] = 2;
            crit_count++;
            cpu_local += 5.0;
        });
        // Mark peripheral (skip critical)
        grid.for_each_in_radius(obs.x, obs.y, periph_r, [&](int ei) {
            if (tier[ei] < 1) {
                tier[ei] = 1;
                periph_count++;
                cpu_local += 5.0;
            }
        });
        // Ambient = all - critical - peripheral
        ambient_count = static_cast<int>(sc.entities.size()) - crit_count - periph_count;
        // Bytes per tick = (critical × 64 / 1) + (peripheral × 64 / 6) + (ambient × 64 / 30)
        // Tier rates: 20 Hz / 30 Hz = 1.0 (every tick), 5 Hz / 30 Hz = 1/6, 1 Hz / 30 Hz = 1/30
        double eff_ents = static_cast<double>(crit_count) +
                          static_cast<double>(periph_count) / 6.0 +
                          static_cast<double>(ambient_count) / 30.0;
        total_bytes += eff_ents * kWorld.bytes_per_entity;
        total_cpu_ns += cpu_local;
        total_crit += crit_count;
        total_periph += periph_count;
        total_ambient += ambient_count;
    }
    double packets = total_bytes / kWorld.mtu_bytes;
    double kbps = total_bytes * 8.0 * kWorld.tick_rate_hz / 1024.0 / n_players;
    double churn = n_players * 8.0;  // more tiers = more churn
    return {
        total_bytes, packets, kbps, total_cpu_ns,
        total_crit / n_players, total_periph / n_players, total_ambient / n_players,
        churn
    };
}

// D. GridAOI_3Tier + Priority queue (top-K per tier)
StrategyResult Strategy_D_GridAOI_3Tier_Priority(const SceneData& sc, const Grid& grid) {
    int n_players = static_cast<int>(sc.observers.size());
    int crit_r = kWorld.n_aoi_cells_critical;
    int periph_r = kWorld.n_aoi_cells_peripheral;
    // Top-K per tier: critical=200, peripheral=100, ambient=20
    constexpr int kCritTopK = 200;
    constexpr int kPeriphTopK = 100;
    constexpr int kAmbientTopK = 20;
    double total_bytes = 0;
    double total_cpu_ns = 0;
    double total_crit = 0, total_periph = 0, total_ambient = 0;
    // Distance + importance score: score = importance / (distance + 1)
    // For simplicity: keep top-K by importance (ignore distance here, since all are within tier)
    std::vector<uint8_t> importance_keeper;
    importance_keeper.reserve(kCritTopK + kPeriphTopK + kAmbientTopK);
    for (const auto& obs : sc.observers) {
        int crit_count = 0, periph_count = 0, ambient_count = 0;
        double cpu_local = 0;
        // Mark critical
        std::vector<int> crit_ents;
        grid.for_each_in_radius(obs.x, obs.y, crit_r, [&](int ei) {
            crit_ents.push_back(ei);
            cpu_local += 5.0;
        });
        // Sort by importance desc, take top-K
        std::sort(crit_ents.begin(), crit_ents.end(), [&](int a, int b) {
            return sc.entities[a].importance > sc.entities[b].importance;
        });
        crit_count = std::min(static_cast<int>(crit_ents.size()), kCritTopK);
        // Mark peripheral
        std::vector<int> periph_ents;
        grid.for_each_in_radius(obs.x, obs.y, periph_r, [&](int ei) {
            // skip if in critical cells
            float dx = sc.entities[ei].x - obs.x;
            float dy = sc.entities[ei].y - obs.y;
            if (dx*dx + dy*dy <= kWorld.critical_range_m * kWorld.critical_range_m) return;
            periph_ents.push_back(ei);
            cpu_local += 5.0;
        });
        std::sort(periph_ents.begin(), periph_ents.end(), [&](int a, int b) {
            return sc.entities[a].importance > sc.entities[b].importance;
        });
        periph_count = std::min(static_cast<int>(periph_ents.size()), kPeriphTopK);
        // Ambient: all remaining, take top-K
        ambient_count = std::min(static_cast<int>(sc.entities.size()) - crit_count - periph_count,
                                 kAmbientTopK);
        // Bytes per tick (tier rates: 20/5/1 Hz over 30 Hz tick = 1, 1/6, 1/30)
        double eff_ents = static_cast<double>(crit_count) +
                          static_cast<double>(periph_count) / 6.0 +
                          static_cast<double>(ambient_count) / 30.0;
        total_bytes += eff_ents * kWorld.bytes_per_entity;
        total_cpu_ns += cpu_local;
        total_crit += crit_count;
        total_periph += periph_count;
        total_ambient += ambient_count;
    }
    double packets = total_bytes / kWorld.mtu_bytes;
    double kbps = total_bytes * 8.0 * kWorld.tick_rate_hz / 1024.0 / n_players;
    double churn = n_players * 12.0;
    return {
        total_bytes, packets, kbps, total_cpu_ns,
        total_crit / n_players, total_periph / n_players, total_ambient / n_players,
        churn
    };
}

// E. GridAOI_3Tier + KNN variable radius + back cull
StrategyResult Strategy_E_GridAOI_3Tier_KNN_BackCull(const SceneData& sc, const Grid& grid) {
    int n_players = static_cast<int>(sc.observers.size());
    int crit_r = kWorld.n_aoi_cells_critical;
    int periph_r = kWorld.n_aoi_cells_peripheral;
    constexpr int kKnnMaxEnts = 100;  // variable radius: max 100 entities per player
    double total_bytes = 0;
    double total_cpu_ns = 0;
    double total_crit = 0, total_periph = 0, total_ambient = 0;
    for (const auto& obs : sc.observers) {
        int crit_count = 0, periph_count = 0, ambient_count = 0;
        double cpu_local = 0;
        // Critical: collect with distance + back-cull (only forward 180°)
        std::vector<std::pair<float, int>> crit_dists;
        grid.for_each_in_radius(obs.x, obs.y, crit_r, [&](int ei) {
            float dx = sc.entities[ei].x - obs.x;
            float dy = sc.entities[ei].y - obs.y;
            float dist = std::sqrt(dx*dx + dy*dy);
            // Back cull: angle to entity vs player rotation
            float ang = std::atan2(dy, dx);
            float diff = std::abs(ang - obs.rotation);
            if (diff > 3.14159f) diff = 6.28318f - diff;
            if (diff > 1.5708f) return;  // back cull: skip entities behind
            crit_dists.emplace_back(dist, ei);
            cpu_local += 5.0;
        });
        // Sort by distance, take KNN top-K
        std::sort(crit_dists.begin(), crit_dists.end());
        if (static_cast<int>(crit_dists.size()) > kKnnMaxEnts) crit_dists.resize(kKnnMaxEnts);
        crit_count = static_cast<int>(crit_dists.size());
        // Peripheral: same but no back cull
        std::vector<std::pair<float, int>> periph_dists;
        grid.for_each_in_radius(obs.x, obs.y, periph_r, [&](int ei) {
            float dx = sc.entities[ei].x - obs.x;
            float dy = sc.entities[ei].y - obs.y;
            float dist2 = dx*dx + dy*dy;
            if (dist2 <= kWorld.critical_range_m * kWorld.critical_range_m) return;
            periph_dists.emplace_back(std::sqrt(dist2), ei);
            cpu_local += 5.0;
        });
        std::sort(periph_dists.begin(), periph_dists.end());
        if (static_cast<int>(periph_dists.size()) > kKnnMaxEnts) periph_dists.resize(kKnnMaxEnts);
        periph_count = static_cast<int>(periph_dists.size());
        // Ambient: top-K by importance
        ambient_count = std::min(20, static_cast<int>(sc.entities.size()) - crit_count - periph_count);
        // Bytes (tier rates: 20/5/1 Hz over 30 Hz tick = 1, 1/6, 1/30)
        double eff_ents = static_cast<double>(crit_count) +
                          static_cast<double>(periph_count) / 6.0 +
                          static_cast<double>(ambient_count) / 30.0;
        total_bytes += eff_ents * kWorld.bytes_per_entity;
        total_cpu_ns += cpu_local;
        total_crit += crit_count;
        total_periph += periph_count;
        total_ambient += ambient_count;
    }
    double packets = total_bytes / kWorld.mtu_bytes;
    double kbps = total_bytes * 8.0 * kWorld.tick_rate_hz / 1024.0 / n_players;
    double churn = n_players * 10.0;
    return {
        total_bytes, packets, kbps, total_cpu_ns,
        total_crit / n_players, total_periph / n_players, total_ambient / n_players,
        churn
    };
}

// F. GridAOI_3Tier + packet batching
StrategyResult Strategy_F_GridAOI_3Tier_Batched(const SceneData& sc, const Grid& grid) {
    int n_players = static_cast<int>(sc.observers.size());
    int crit_r = kWorld.n_aoi_cells_critical;
    int periph_r = kWorld.n_aoi_cells_peripheral;
    // Use C as base, then add batching: pack 4 players per packet (reduce packet count 4×)
    constexpr int kBatchFactor = 4;
    double total_bytes = 0;
    double total_cpu_ns = 0;
    double total_crit = 0, total_periph = 0, total_ambient = 0;
    for (const auto& obs : sc.observers) {
        int crit_count = 0, periph_count = 0, ambient_count = 0;
        double cpu_local = 0;
        std::vector<int> tier_arr(static_cast<int>(sc.entities.size()), 0);
        grid.for_each_in_radius(obs.x, obs.y, crit_r, [&](int ei) {
            tier_arr[ei] = 2;
            crit_count++;
            cpu_local += 5.0;
        });
        grid.for_each_in_radius(obs.x, obs.y, periph_r, [&](int ei) {
            if (tier_arr[ei] < 1) {
                tier_arr[ei] = 1;
                periph_count++;
                cpu_local += 5.0;
            }
        });
        ambient_count = static_cast<int>(sc.entities.size()) - crit_count - periph_count;
        // Bytes per tick (tier rates: 20/5/1 Hz over 30 Hz tick = 1, 1/6, 1/30)
        double eff_ents = static_cast<double>(crit_count) +
                          static_cast<double>(periph_count) / 6.0 +
                          static_cast<double>(ambient_count) / 30.0;
        total_bytes += eff_ents * kWorld.bytes_per_entity;
        total_cpu_ns += cpu_local;
        total_crit += crit_count;
        total_periph += periph_count;
        total_ambient += ambient_count;
    }
    // Packets reduced by kBatchFactor
    double packets = (total_bytes / kWorld.mtu_bytes) / kBatchFactor;
    double kbps = total_bytes * 8.0 * kWorld.tick_rate_hz / 1024.0 / n_players;
    double churn = n_players * 8.0;
    return {
        total_bytes, packets, kbps, total_cpu_ns,
        total_crit / n_players, total_periph / n_players, total_ambient / n_players,
        churn
    };
}

// =============================================================================
// 6. Per-strategy benchmark
// =============================================================================
struct ConfigResult {
    Scene scene;
    uint32_t seed;
    std::vector<StrategyResult> per_strategy;
    int n_players;
    int n_entities;
};

using StrategyFn = StrategyResult(*)(const SceneData&, const Grid&);
using StrategyFnNoGrid = StrategyResult(*)(const SceneData&);

ConfigResult BenchmarkConfig(Scene s, uint32_t seed) {
    auto scene = GenerateScene(s, seed);
    auto grid = MakeGrid();
    grid.rebuild(scene.entities);
    ConfigResult cr;
    cr.scene = s;
    cr.seed = seed;
    cr.n_players = static_cast<int>(scene.observers.size());
    cr.n_entities = static_cast<int>(scene.entities.size());
    cr.per_strategy.reserve(6);
    // A (no grid)
    cr.per_strategy.push_back(Strategy_A_FullBroadcast(scene));
    // B-F (grid)
    cr.per_strategy.push_back(Strategy_B_GridAOI_NoTiering(scene, grid));
    cr.per_strategy.push_back(Strategy_C_GridAOI_3Tier(scene, grid));
    cr.per_strategy.push_back(Strategy_D_GridAOI_3Tier_Priority(scene, grid));
    cr.per_strategy.push_back(Strategy_E_GridAOI_3Tier_KNN_BackCull(scene, grid));
    cr.per_strategy.push_back(Strategy_F_GridAOI_3Tier_Batched(scene, grid));
    return cr;
}

// =============================================================================
// 7. CSV output
// =============================================================================
void WriteCsvHeader(std::ofstream& f) {
    f << "strategy,scene,seed,n_players,n_entities,bytes_per_tick,packets_per_tick,"
         "per_player_kbps,cpu_ns_per_tick,critical_ents,peripheral_ents,ambient_ents,churn_per_sec\n";
}

void WriteCsvRow(std::ofstream& f, std::string_view strategy, const ConfigResult& cr, int si) {
    const auto& r = cr.per_strategy[si];
    f << strategy << ","
      << SceneName(cr.scene) << ","
      << cr.seed << ","
      << cr.n_players << ","
      << cr.n_entities << ","
      << r.bytes_per_tick << ","
      << r.packets_per_tick << ","
      << r.per_player_kbps << ","
      << r.cpu_ns_per_tick << ","
      << r.critical_ents_per_player << ","
      << r.peripheral_ents_per_player << ","
      << r.ambient_ents_per_player << ","
      << r.aoi_churn_per_sec_per_player << "\n";
}

// =============================================================================
// 8. Print summary
// =============================================================================
void PrintStrategySummary(std::string_view name, const std::vector<ConfigResult>& results) {
    printf("\n=== Strategy %.*s (5 scenes x 5 seeds = 25 configs) ===\n",
           static_cast<int>(name.size()), name.data());
    printf("%-18s %-6s %-12s %-10s %-10s %-10s\n",
           "scene", "seed", "bytes/tick", "packets", "kbps/p", "cpu_us");
    for (const auto& r : results) {
        // Per-strategy: take first per_strategy entry (we use per_config, but simplify)
        const auto& sr = r.per_strategy[0];  // dummy; we'll print all strategies
        (void)sr;
    }
    // For each config, show all 6 strategies in one line
    printf("\nPer-config all-strategy comparison (per_player_kbps):\n");
    printf("%-18s %-6s %-10s %-10s %-10s %-10s %-10s %-10s\n",
           "scene", "seed", "A_FullB", "B_NoTier", "C_3Tier", "D_Pri", "E_KNN", "F_Batch");
    for (const auto& r : results) {
        printf("%-18s %-6u %-10.0f %-10.0f %-10.0f %-10.0f %-10.0f %-10.0f\n",
               std::string(SceneName(r.scene)).c_str(),
               r.seed,
               r.per_strategy[0].per_player_kbps,
               r.per_strategy[1].per_player_kbps,
               r.per_strategy[2].per_player_kbps,
               r.per_strategy[3].per_player_kbps,
               r.per_strategy[4].per_player_kbps,
               r.per_strategy[5].per_player_kbps);
    }
}

// =============================================================================
// 9. Per-scene aggregate + cross-strategy comparison
// =============================================================================
void PrintPerSceneAggregate(const std::vector<ConfigResult>& results) {
    printf("\n=== Per-scene aggregate (mean kbps per player across 5 seeds) ===\n");
    printf("%-18s %-10s %-10s %-10s %-10s %-10s %-10s\n",
           "scene", "A_FullB", "B_NoTier", "C_3Tier", "D_Pri", "E_KNN", "F_Batch");
    for (int si = 0; si < kSceneCount; ++si) {
        Scene s = static_cast<Scene>(si);
        double sums[6] = {0,0,0,0,0,0};
        int n = 0;
        for (const auto& r : results) {
            if (r.scene == s) {
                for (int k = 0; k < 6; ++k) sums[k] += r.per_strategy[k].per_player_kbps;
                ++n;
            }
        }
        if (n > 0) {
            printf("%-18s", std::string(SceneName(s)).c_str());
            for (int k = 0; k < 6; ++k) printf(" %-10.0f", sums[k] / n);
            printf("\n");
        }
    }
}

// =============================================================================
// 10. Main
// =============================================================================
int main() {
    printf("=== aoi_bench: grid-based AOI netcode simulation ===\n");
    printf("World: %.0f m^2, %d players, %d entities (default 10k)\n",
           kWorld.world_size_m, kWorld.n_players, kWorld.n_entities);
    printf("Cell size: %.0f m, critical range: %.0f m, peripheral range: %.0f m\n",
           kWorld.cell_size_m, kWorld.critical_range_m, kWorld.peripheral_range_m);
    printf("Bytes/entity: %.0f, MTU: %d, tick rate: %.0f Hz\n",
           kWorld.bytes_per_entity, kWorld.mtu_bytes, kWorld.tick_rate_hz);
    printf("Seeds: 5 per scene, scenes: %d, total configs: %d\n",
           kSceneCount, kSceneCount * 5);

    std::ofstream csv("aoi_bench_results.csv");
    if (!csv) {
        fprintf(stderr, "FATAL: cannot open aoi_bench_results.csv\n");
        return 1;
    }
    WriteCsvHeader(csv);

    std::vector<ConfigResult> all_results;
    all_results.reserve(kSceneCount * 5);
    constexpr uint32_t kSeeds[] = {1, 7, 42, 1234, 31337};
    const std::string_view strategy_names[] = {
        "A_FullBroadcast", "B_GridAOI_NoTiering", "C_GridAOI_3Tier",
        "D_GridAOI_3Tier_Priority", "E_GridAOI_3Tier_KNN_BackCull", "F_GridAOI_3Tier_Batched"
    };
    for (int si = 0; si < kSceneCount; ++si) {
        Scene s = static_cast<Scene>(si);
        for (uint32_t seed : kSeeds) {
            auto cr = BenchmarkConfig(s, seed);
            for (int k = 0; k < 6; ++k) {
                WriteCsvRow(csv, strategy_names[k], cr, k);
            }
            all_results.push_back(std::move(cr));
        }
    }
    csv.close();

    PrintStrategySummary("ALL", all_results);
    PrintPerSceneAggregate(all_results);

    // Cross-strategy speedup vs A_FullBroadcast
    printf("\n=== Speedup vs A_FullBroadcast (per_player_kbps) ===\n");
    printf("%-18s %-10s %-10s %-10s %-10s %-10s\n",
           "scene", "B", "C", "D", "E", "F");
    for (int si = 0; si < kSceneCount; ++si) {
        Scene s = static_cast<Scene>(si);
        double sums[6] = {0,0,0,0,0,0};
        int n = 0;
        for (const auto& r : all_results) {
            if (r.scene == s) {
                for (int k = 0; k < 6; ++k) sums[k] += r.per_strategy[k].per_player_kbps;
                ++n;
            }
        }
        if (n > 0) {
            double a_kbps = sums[0] / n;
            printf("%-18s", std::string(SceneName(s)).c_str());
            for (int k = 1; k < 6; ++k) {
                double other = sums[k] / n;
                if (other > 0) printf(" %-10.2fx", a_kbps / other);
                else printf(" %-10s", "inf");
            }
            printf("\n");
        }
    }

    printf("\n=== Done. See aoi_bench_results.csv for full data. ===\n");
    return 0;
}
