#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <print>
#include <random>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

using f32  = float;
using u8   = uint8_t;
using u16  = uint16_t;
using u32  = uint32_t;
using u64  = uint64_t;
using i32  = int32_t;

static constexpr f32 kWorldX = 256.0f;
static constexpr f32 kWorldZ = 256.0f;
static constexpr u32 kNumMines = 1'000;
static constexpr u32 kNumTicksPerRun = 1000;
static constexpr u32 kWarmupTicks = 10;
static constexpr u32 kPathLength = kNumTicksPerRun;
static constexpr u32 kNumSeeds = 5;

enum class Strategy : u8 {
    A_NoMines,
    B_SimpleProximity,
    C_PatternedField,
    D_TimedDetonation,
    E_ClearableMines,
    kCount
};

enum class Scene : u8 {
    LinearTrenchBreach,
    OpenFieldRandom,
    DefensivePerimeter,
    UrbanCorridor,
    MixedTerrainObstacle,
    kCount
};

static constexpr std::string_view kStrategyNames[] = {
    "A_NoMines", "B_SimpleProximity", "C_PatternedField",
    "D_TimedDetonation", "E_ClearableMines"
};

static constexpr std::string_view kSceneNames[] = {
    "linear_trench_breach", "open_field_random",
    "defensive_perimeter_pattern", "urban_corridor",
    "mixed_terrain_obstacle"
};

struct alignas(64) Mine {
    f32 px, py, pz;
    f32 trigger_radius;
    f32 kill_radius;
    u8 type;           // 0=AT, 1=AP
    u8 trigger_mech;   // 0=pressure, 1=tilt-rod, 2=magnetic, 3=command, 4=timed
    u8 flags;          // bit0=armed, bit1=triggered, bit2=cleared, bit3=detected
    u8 pad;
    u32 arm_delay;     // ticks until armed (for timed)
};

static constexpr f32 kDefaultTriggerAT   = 3.0f;
static constexpr f32 kDefaultTriggerAP   = 1.5f;
static constexpr f32 kDefaultKillAT      = 15.0f;
static constexpr f32 kDefaultKillAP      = 8.0f;

static constexpr u8 kFlagArmed    = 0x01;
static constexpr u8 kFlagTriggered = 0x02;
static constexpr u8 kFlagCleared  = 0x04;
static constexpr u8 kFlagDetected = 0x08;

// ---------------------------------------------------------------------------
// RNG helpers
// ---------------------------------------------------------------------------

struct Xoroshiro128Plus {
    u64 s[2];

    explicit Xoroshiro128Plus(u64 seed) {
        splitmix(seed);
        splitmix(seed);
    }
    void splitmix(u64 &seed) {
        u64 z = (seed += 0x9e3779b97f4a7c15uLL);
        z  = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9uLL;
        z  = (z ^ (z >> 27)) * 0x94d049bb133111ebuLL;
        s[0] = z ^ (z >> 31);
        z = (seed += 0x9e3779b97f4a7c15uLL);
        z  = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9uLL;
        z  = (z ^ (z >> 27)) * 0x94d049bb133111ebuLL;
        s[1] = z ^ (z >> 31);
    }
    u64 next() {
        u64 r = s[0] + s[1];
        s[1] ^= s[0];
        s[0] = std::rotl(s[0], 24) ^ s[1] ^ (s[1] << 16);
        s[1] = std::rotl(s[1], 37);
        return r;
    }
    f32 uniform() {
        return static_cast<f32>(next() >> 40) * 0x1p-24f;
    }
    f32 range(f32 lo, f32 hi) { return lo + uniform() * (hi - lo); }
};

// ---------------------------------------------------------------------------
// Scene generation
// ---------------------------------------------------------------------------

void gen_linear_trench_breach(std::span<Mine> mines, Xoroshiro128Plus &rng) {
    // Narrow corridor (z=20..40, x=0..256), mines in rows perpendicular to advance (along X)
    // AT mines center, AP mines edges
    f32 cz = kWorldZ * 0.5f;
    u32 half = kNumMines / 2;
    for (u32 i = 0; i < kNumMines; ++i) {
        auto &m = mines[i];
        if (i < half) {
            // AT mines in center lane
            m.px = rng.range(0.0f, kWorldX);
            m.py = 0.0f;
            m.pz = cz + rng.range(-8.0f, 8.0f);
            m.type = 0;
            m.trigger_radius = kDefaultTriggerAT;
            m.kill_radius = kDefaultKillAT;
        } else {
            // AP mines on edges
            m.px = rng.range(0.0f, kWorldX);
            m.py = 0.0f;
            m.pz = cz + (rng.uniform() < 0.5f ? rng.range(12.0f, 20.0f) : rng.range(-20.0f, -12.0f));
            m.type = 1;
            m.trigger_radius = kDefaultTriggerAP;
            m.kill_radius = kDefaultKillAP;
        }
        m.trigger_mech = static_cast<u8>(rng.uniform() < 0.6f ? 0 : 2);
        m.flags = kFlagArmed;
        m.arm_delay = 0;
    }
}

void gen_open_field_random(std::span<Mine> mines, Xoroshiro128Plus &rng) {
    for (u32 i = 0; i < kNumMines; ++i) {
        auto &m = mines[i];
        m.px = rng.range(4.0f, kWorldX - 4.0f);
        m.py = 0.0f;
        m.pz = rng.range(4.0f, kWorldZ - 4.0f);
        m.type = static_cast<u8>(rng.uniform() < 0.4f ? 0 : 1);
        m.trigger_radius = (m.type == 0) ? kDefaultTriggerAT : kDefaultTriggerAP;
        m.kill_radius = (m.type == 0) ? kDefaultKillAT : kDefaultKillAP;
        m.trigger_mech = static_cast<u8>(std::min(3, static_cast<int>(rng.uniform() * 4.99f)));
        m.flags = kFlagArmed;
        m.arm_delay = 0;
    }
}

void gen_defensive_perimeter(std::span<Mine> mines, Xoroshiro128Plus &rng) {
    // Concentric rings around (128, 0, 128)
    f32 cx = kWorldX * 0.5f, cz = kWorldZ * 0.5f;
    u32 i = 0;
    f32 radii[] = {20.0f, 40.0f, 60.0f, 85.0f, 110.0f};
    for (f32 r : radii) {
        u32 ring_count = static_cast<u32>(r * 0.35f + 1);
        ring_count = std::min(ring_count, kNumMines - i);
        for (u32 j = 0; j < ring_count && i < kNumMines; ++j, ++i) {
            f32 angle = 6.2831855f * static_cast<f32>(j) / static_cast<f32>(ring_count);
            angle += rng.range(-0.15f, 0.15f);
            f32 rad = r + rng.range(-3.0f, 3.0f);
            auto &m = mines[i];
            m.px = cx + rad * std::cos(angle);
            m.pz = cz + rad * std::sin(angle);
            m.py = 0.0f;
            m.type = (r > 60.0f) ? static_cast<u8>(0) : static_cast<u8>(1);
            m.trigger_radius = (m.type == 0) ? kDefaultTriggerAT : kDefaultTriggerAP;
            m.kill_radius = (m.type == 0) ? kDefaultKillAT : kDefaultKillAP;
            m.trigger_mech = static_cast<u8>(std::min(3, static_cast<int>(rng.uniform() * 4.99f)));
            m.flags = kFlagArmed;
            m.arm_delay = 0;
        }
    }
    while (i < kNumMines) {
        auto &m = mines[i++];
        m.px = rng.range(4.0f, kWorldX - 4.0f);
        m.pz = rng.range(4.0f, kWorldZ - 4.0f);
        m.py = 0.0f;
        m.type = 1;
        m.trigger_radius = kDefaultTriggerAP;
        m.kill_radius = kDefaultKillAP;
        m.trigger_mech = 0;
        m.flags = kFlagArmed;
        m.arm_delay = 0;
    }
}

void gen_urban_corridor(std::span<Mine> mines, Xoroshiro128Plus &rng) {
    // City grid: streets at z=32, 64, 96, 128, 160, 192, 224
    // Mines placed at intersections and along street medians
    u32 i = 0;
    for (f32 street_z = 32.0f; street_z < kWorldZ - 16.0f && i < kNumMines; street_z += 32.0f) {
        for (f32 street_x = 16.0f; street_x < kWorldX - 16.0f && i < kNumMines; street_x += 32.0f) {
            auto &m = mines[i++];
            m.px = street_x + rng.range(-4.0f, 4.0f);
            m.pz = street_z + rng.range(-4.0f, 4.0f);
            m.py = 0.0f;
            m.type = static_cast<u8>(rng.uniform() < 0.5f ? 0 : 1);
            m.trigger_radius = (m.type == 0) ? kDefaultTriggerAT : kDefaultTriggerAP;
            m.kill_radius = (m.type == 0) ? kDefaultKillAT : kDefaultKillAP;
            m.trigger_mech = 0;
            m.flags = kFlagArmed;
            m.arm_delay = 0;
        }
    }
    // Fill remaining
    while (i < kNumMines) {
        auto &m = mines[i++];
        m.px = rng.range(8.0f, kWorldX - 8.0f);
        m.pz = rng.range(8.0f, kWorldZ - 8.0f);
        m.py = 0.0f;
        m.type = 1;
        m.trigger_radius = kDefaultTriggerAP;
        m.kill_radius = kDefaultKillAP;
        m.trigger_mech = 0;
        m.flags = kFlagArmed;
        m.arm_delay = 0;
    }
}

void gen_mixed_terrain(std::span<Mine> mines, Xoroshiro128Plus &rng) {
    // AT belt at x=128 (chokepoint) + AP scatter in z<64 (tall grass)
    // + command-detonated near bridge at (192, 128) + random fill
    u32 i = 0;
    // AT belt
    u32 belt = kNumMines / 4;
    for (; i < belt; ++i) {
        auto &m = mines[i];
        m.px = rng.range(120.0f, 136.0f);
        m.py = 0.0f;
        m.pz = rng.range(16.0f, kWorldZ - 16.0f);
        m.type = 0;
        m.trigger_radius = kDefaultTriggerAT;
        m.kill_radius = kDefaultKillAT;
        m.trigger_mech = static_cast<u8>(rng.uniform() < 0.7f ? 0 : 4);
        m.flags = kFlagArmed;
        m.arm_delay = m.trigger_mech == 4 ? static_cast<u32>(rng.range(50.0f, 200.0f)) : 0u;
    }
    // AP scatter in forward area
    u32 scatter = kNumMines / 3;
    for (; i < belt + scatter && i < kNumMines; ++i) {
        auto &m = mines[i];
        m.px = rng.range(0.0f, kWorldX);
        m.pz = rng.range(0.0f, 64.0f);
        m.py = 0.0f;
        m.type = 1;
        m.trigger_radius = kDefaultTriggerAP;
        m.kill_radius = kDefaultKillAP;
        m.trigger_mech = 0;
        m.flags = kFlagArmed;
        m.arm_delay = 0;
    }
    // Bridge command-detonated
    u32 bridge = kNumMines / 6;
    for (; i < belt + scatter + bridge && i < kNumMines; ++i) {
        auto &m = mines[i];
        m.px = rng.range(180.0f, 204.0f);
        m.pz = rng.range(116.0f, 140.0f);
        m.py = 0.0f;
        m.type = 0;
        m.trigger_radius = kDefaultTriggerAT;
        m.kill_radius = kDefaultKillAT;
        m.trigger_mech = 3;  // command
        m.flags = kFlagArmed;
        m.arm_delay = 0;
    }
    // Random mixed fill
    while (i < kNumMines) {
        auto &m = mines[i++];
        m.px = rng.range(4.0f, kWorldX - 4.0f);
        m.pz = rng.range(4.0f, kWorldZ - 4.0f);
        m.py = 0.0f;
        m.type = static_cast<u8>(rng.uniform() < 0.3f ? 0 : 1);
        m.trigger_radius = (m.type == 0) ? kDefaultTriggerAT : kDefaultTriggerAP;
        m.kill_radius = (m.type == 0) ? kDefaultKillAT : kDefaultKillAP;
        m.trigger_mech = static_cast<u8>(std::min(3, static_cast<int>(rng.uniform() * 4.99f)));
        m.flags = kFlagArmed;
        m.arm_delay = 0;
    }
}

using SceneGenFn = void (*)(std::span<Mine>, Xoroshiro128Plus &);
static constexpr SceneGenFn kSceneGens[] = {
    gen_linear_trench_breach, gen_open_field_random,
    gen_defensive_perimeter, gen_urban_corridor,
    gen_mixed_terrain
};

// ---------------------------------------------------------------------------
// Entity path generation
// ---------------------------------------------------------------------------

struct PathPoint { f32 x, y, z; };

void generate_path(std::span<PathPoint> path, Scene scene, Xoroshiro128Plus &rng) {
    f32 cz = kWorldZ * 0.5f;
    f32 sx, sy, sz, ex, ez;
    sy = 1.0f;
    switch (scene) {
        case Scene::LinearTrenchBreach:
            sx = 0.0f; sz = cz;
            ex = kWorldX; ez = cz;
            break;
        case Scene::OpenFieldRandom:
            sx = 4.0f; sz = 4.0f;
            ex = kWorldX - 4.0f; ez = kWorldZ - 4.0f;
            break;
        case Scene::DefensivePerimeter:
            sx = 4.0f; sz = cz;
            ex = kWorldX - 4.0f; ez = cz;
            break;
        case Scene::UrbanCorridor:
            sx = 4.0f; sz = cz;
            ex = kWorldX - 4.0f; ez = cz;
            break;
        case Scene::MixedTerrainObstacle:
            sx = 4.0f; sz = 4.0f;
            ex = kWorldX - 4.0f; ez = kWorldZ - 4.0f;
            break;
        case Scene::kCount: break;
    }
    auto n = static_cast<f32>(path.size());
    for (u32 i = 0; i < path.size(); ++i) {
        f32 t = static_cast<f32>(i) / (n - 1);
        path[i].x = sx + (ex - sx) * t + rng.range(-1.5f, 1.5f);
        path[i].y = sy;
        path[i].z = sz + (ez - sz) * t + rng.range(-1.5f, 1.5f);
    }
}

// ---------------------------------------------------------------------------
// Strategy detection functions
// ---------------------------------------------------------------------------

struct DetResult {
    u32 triggered_count;
    u32 cleared_count;
    u32 total_checked;
};

DetResult detect_A(std::span<Mine> mines, const PathPoint &) {
    (void)mines;
    return {0, 0, 0};
}

DetResult detect_B(std::span<Mine> mines, const PathPoint &entity) {
    u32 triggered = 0;
    for (const auto &m : mines) {
        if (!(m.flags & kFlagArmed) || (m.flags & kFlagCleared))
            continue;
        f32 dx = m.px - entity.x;
        f32 dz = m.pz - entity.z;
        f32 dist2 = dx * dx + dz * dz;
        if (dist2 <= m.trigger_radius * m.trigger_radius)
            ++triggered;
    }
    return {triggered, 0, static_cast<u32>(mines.size())};
}

// Spatial grid helper for C's chain reaction
struct GridCell { std::vector<u32> indices; };

static constexpr u32 kGridDim = 16; // 16×16 cells → 16m cell size for 256m world

auto build_grid(std::span<const Mine> mines) {
    std::array<GridCell, kGridDim * kGridDim> grid;
    f32 cell_size = kWorldX / static_cast<f32>(kGridDim);
    for (u32 i = 0; i < mines.size(); ++i) {
        u32 cx = std::min(static_cast<u32>(mines[i].px / cell_size), kGridDim - 1u);
        u32 cz = std::min(static_cast<u32>(mines[i].pz / cell_size), kGridDim - 1u);
        grid[cz * kGridDim + cx].indices.push_back(i);
    }
    return grid;
}

DetResult detect_C(std::span<Mine> mines, const PathPoint &entity,
                   const std::array<GridCell, kGridDim * kGridDim> &grid) {
    std::vector<u32> newly_triggered;
    u32 total_checked = 0;
    f32 cell_size = kWorldX / static_cast<f32>(kGridDim);
    for (u32 i = 0; i < mines.size(); ++i) {
        auto &m = mines[i];
        if (!(m.flags & kFlagArmed) || (m.flags & kFlagCleared) || (m.flags & kFlagTriggered))
            continue;
        f32 dx = m.px - entity.x;
        f32 dz = m.pz - entity.z;
        f32 dist2 = dx * dx + dz * dz;
        if (dist2 <= m.trigger_radius * m.trigger_radius) {
            m.flags |= kFlagTriggered;
            newly_triggered.push_back(i);
        }
        ++total_checked;
    }

    std::vector<u32> pending = std::move(newly_triggered);
    while (!pending.empty()) {
        auto idx = pending.back();
        pending.pop_back();
        auto &src = mines[idx];
        // Only check mines in 3×3 cell neighbourhood around src
        u32 cxs = std::min(static_cast<u32>(src.px / cell_size), kGridDim - 1u);
        u32 czs = std::min(static_cast<u32>(src.pz / cell_size), kGridDim - 1u);
        i32 cxs0 = std::max(i32{0}, static_cast<i32>(cxs) - 1);
        i32 cxs1 = std::min(static_cast<i32>(kGridDim) - 1, static_cast<i32>(cxs) + 1);
        i32 czs0 = std::max(i32{0}, static_cast<i32>(czs) - 1);
        i32 czs1 = std::min(static_cast<i32>(kGridDim) - 1, static_cast<i32>(czs) + 1);
        for (i32 cz = czs0; cz <= czs1; ++cz) {
            for (i32 cx = cxs0; cx <= cxs1; ++cx) {
                for (u32 j : grid[static_cast<u32>(cz) * kGridDim + static_cast<u32>(cx)].indices) {
                    auto &tgt = mines[j];
                    if (!(tgt.flags & kFlagArmed) || (tgt.flags & kFlagCleared) || (tgt.flags & kFlagTriggered))
                        continue;
                    f32 dx = src.px - tgt.px;
                    f32 dz = src.pz - tgt.pz;
                    f32 dist2 = dx * dx + dz * dz;
                    if (dist2 <= src.kill_radius * src.kill_radius) {
                        tgt.flags |= kFlagTriggered;
                        pending.push_back(j);
                    }
                    ++total_checked;
                }
            }
        }
    }

    u32 triggered = 0;
    for (const auto &m : mines)
        if (m.flags & kFlagTriggered) ++triggered;
    for (auto &m : mines)
        m.flags &= ~kFlagTriggered;
    return {triggered, 0, total_checked};
}

DetResult detect_D(std::span<Mine> mines, const PathPoint &entity) {
    u32 triggered = 0;
    for (const auto &m : mines) {
        if (!(m.flags & kFlagArmed) || (m.flags & kFlagCleared))
            continue;
        if (m.trigger_mech == 4 && m.arm_delay > 0) {
            ++triggered;
            continue;
        }
        f32 dx = m.px - entity.x;
        f32 dz = m.pz - entity.z;
        f32 dist2 = dx * dx + dz * dz;
        if (dist2 <= m.trigger_radius * m.trigger_radius)
            ++triggered;
    }
    return {triggered, 0, static_cast<u32>(mines.size())};
}

DetResult detect_E(std::span<Mine> mines, const PathPoint &entity) {
    u32 triggered = 0;
    for (const auto &m : mines) {
        if (!(m.flags & kFlagArmed) || (m.flags & kFlagCleared))
            continue;
        f32 dx = m.px - entity.x;
        f32 dz = m.pz - entity.z;
        f32 dist2 = dx * dx + dz * dz;
        if (dist2 <= m.trigger_radius * m.trigger_radius)
            ++triggered;
    }
    u32 cleared = 0;
    for (auto &m : mines) {
        if (!(m.flags & kFlagArmed) || (m.flags & kFlagCleared))
            continue;
        f32 dx = m.px - entity.x;
        f32 dz = m.pz - entity.z;
        f32 dist = std::sqrt(dx * dx + dz * dz);
        if (dist < 10.0f)
            m.flags |= kFlagDetected;
        if (dist < 2.0f && std::abs(entity.x - m.px) < 4.0f) {
            m.flags |= kFlagCleared;
            ++cleared;
            continue;
        }
        if (dist < 1.0f && (m.flags & kFlagDetected)) {
            m.flags |= kFlagCleared;
            ++cleared;
        }
    }
    for (auto &m : mines)
        m.flags &= ~kFlagDetected;
    return {triggered, cleared, static_cast<u32>(mines.size())};
}



// ---------------------------------------------------------------------------
// Benchmark harness
// ---------------------------------------------------------------------------

struct TickMeasurement {
    u64 elapsed_ns;
    u32 triggered;
    u32 cleared;
    u32 total_checked;
};

struct RunResult {
    double mean_ns;
    double median_ns;
    double p95_ns;
    double mean_ns_per_mine;
    double clearance_rate;
    double trigger_prob;
};

using Clock = std::chrono::high_resolution_clock;

RunResult run_benchmark(Strategy strat, Scene scene, u32 seed) {
    // 1. Seed RNG
    Xoroshiro128Plus rng(static_cast<u64>(seed) ^ 0x12345678 + static_cast<u64>(scene) * 31337 + static_cast<u64>(strat) * 0x9e3779b9);

    // 2. Generate mines
    std::vector<Mine> mines(kNumMines);
    kSceneGens[static_cast<size_t>(scene)](mines, rng);

    // 3. Generate entity path (1000 positions)
    std::vector<PathPoint> path(kPathLength);
    generate_path(path, scene, rng);

    // 4. Build spatial grid for C
    auto grid = build_grid(mines);

    // 5. Dispatch helper
    auto run_one = [&](std::span<Mine> copy, const PathPoint &pt) -> DetResult {
        switch (strat) {
            case Strategy::A_NoMines:         return detect_A(copy, pt);
            case Strategy::B_SimpleProximity: return detect_B(copy, pt);
            case Strategy::C_PatternedField:  return detect_C(copy, pt, grid);
            case Strategy::D_TimedDetonation: return detect_D(copy, pt);
            case Strategy::E_ClearableMines:  return detect_E(copy, pt);
            case Strategy::kCount: break;
        }
        return {0,0,0};
    };

    // 6. Warmup
    for (u32 t = 0; t < kWarmupTicks; ++t) {
        auto copy = mines;
        run_one(copy, path[t % kPathLength]);
    }

    // 7. Measured ticks
    std::vector<TickMeasurement> measurements;
    measurements.reserve(kNumTicksPerRun);

    for (u32 t = 0; t < kNumTicksPerRun; ++t) {
        auto copy = mines;
        auto start = Clock::now();
        auto res = run_one(copy, path[t % kPathLength]);
        auto end = Clock::now();
        auto ns = static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        measurements.push_back({ns, res.triggered_count, res.cleared_count, res.total_checked});
    }

    // 7. Compute statistics
    std::vector<u64> ns_vals;
    ns_vals.reserve(measurements.size());
    u64 sum_ns = 0;
    u32 total_triggered = 0;
    u32 total_cleared = 0;
    for (const auto &m : measurements) {
        ns_vals.push_back(m.elapsed_ns);
        sum_ns += m.elapsed_ns;
        total_triggered += m.triggered;
        total_cleared += m.cleared;
    }
    std::ranges::sort(ns_vals);

    double mean_ns = static_cast<double>(sum_ns) / static_cast<double>(ns_vals.size());
    double median_ns = static_cast<double>(ns_vals[ns_vals.size() / 2]);
    double p95_ns = static_cast<double>(ns_vals[static_cast<size_t>(ns_vals.size() * 0.95)]);

    // Per-mine cost: total_ns / (num_ticks * num_mines_checked)
    u64 total_checks = 0;
    for (const auto &m : measurements)
        total_checks += m.total_checked;
    double mean_ns_per_mine = (total_checks > 0)
        ? static_cast<double>(sum_ns) / static_cast<double>(total_checks)
        : 0.0;

    double clearance_rate = static_cast<double>(total_cleared)
        / static_cast<double>(kNumMines * kNumTicksPerRun);
    double trigger_prob = static_cast<double>(total_triggered)
        / static_cast<double>(kNumMines * kNumTicksPerRun);

    return {mean_ns, median_ns, p95_ns, mean_ns_per_mine, clearance_rate, trigger_prob};
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    std::print("strategy,scene,seed,mean_ns,median_ns,p95_ns,mean_ns_per_mine,clearance_rate,trigger_prob\n");

    for (u32 s = 0; s < static_cast<u32>(Strategy::kCount); ++s) {
        auto strat = static_cast<Strategy>(s);
        for (u32 sc = 0; sc < static_cast<u32>(Scene::kCount); ++sc) {
            auto scene = static_cast<Scene>(sc);
            for (u32 seed = 0; seed < kNumSeeds; ++seed) {
                auto res = run_benchmark(strat, scene, seed);
                std::print("{},{},{},{:.1f},{:.1f},{:.1f},{:.6f},{:.6f},{:.6f}\n",
                    kStrategyNames[s], kSceneNames[sc], seed,
                    res.mean_ns, res.median_ns, res.p95_ns,
                    res.mean_ns_per_mine, res.clearance_rate, res.trigger_prob);
            }
        }
    }

    return 0;
}
