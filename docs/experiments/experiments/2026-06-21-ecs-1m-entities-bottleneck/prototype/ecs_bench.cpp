#include <flecs.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

// ProjectV-like components
struct Position { float x, y, z; };
struct Velocity { float x, y, z; };
struct Rotation { float w, x, y, z; };
struct Scale { float x, y, z; };
struct MeshInstance { uint64_t meshID; };
struct Health { float hp; };
struct AIState { uint8_t state; float timer; };
struct LightSource { float radius; uint32_t color; };
struct Item { uint32_t itemID; uint16_t count; };
struct ParticleEmitter { float rate; uint32_t maxCount; };
struct RigidBody { uint32_t bodyID; float mass; };
struct Collider { float radius; float height; };
struct Inventory { uint32_t items[8]; uint16_t counts[8]; };
struct PlayerTag {};
struct EnemyTag {};
struct ProjectileTag {};
struct StaticTag {};

enum ArchetypePattern : uint8_t {
    PAT_SIMPLE   = 0, // Pos + Vel
    PAT_FULL     = 1, // Pos + Rot + Scale + MeshInstance
    PAT_PHYSICS  = 2, // Pos + Rot + Scale + RigidBody + Collider + Vel
    PAT_GAMEPLAY = 3, // Pos + Health + AIState + Inventory
    PAT_LIGHT    = 4, // Pos + LightSource
    PAT_PARTICLE = 5, // Pos + Vel + ParticleEmitter
    PAT_COUNT
};

static constexpr const char* PAT_NAMES[] = {
    "simple", "full", "physics", "gameplay", "light", "particle"
};

struct Measurement {
    double mean, median, p95, p99, std;
    double nsPerOp; // nanoseconds per operation
};

static double nowMs() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static Measurement measure(int warmup, int niter, auto fn) {
    std::vector<double> times;
    times.reserve(niter);

    for (int i = 0; i < warmup + niter; ++i) {
        double start = nowMs();
        fn();
        double end = nowMs();
        if (i >= warmup) times.push_back(end - start);
    }

    std::sort(times.begin(), times.end());
    double sum = std::accumulate(times.begin(), times.end(), 0.0);
    double mean = sum / niter;
    double median = times[niter / 2];
    double p95 = times[static_cast<int>(niter * 0.95)];
    double p99 = times[static_cast<int>(niter * 0.99)];
    double sq = 0;
    for (double t : times) { double d = t - mean; sq += d * d; }
    return {mean, median, p95, p99, std::sqrt(sq / niter), 0};
}

static void populateWorld(flecs::world& world, int count, ArchetypePattern pat) {
    world.defer_begin();
    for (int i = 0; i < count; ++i) {
        auto e = world.entity();
        switch (pat) {
        case PAT_SIMPLE:
            e.set<Position>({0,0,0}).set<Velocity>({0,0,0}); break;
        case PAT_FULL:
            e.set<Position>({0,0,0}).set<Rotation>({1,0,0,0})
             .set<Scale>({1,1,1}).set<MeshInstance>({0}); break;
        case PAT_PHYSICS:
            e.set<Position>({0,0,0}).set<Rotation>({1,0,0,0})
             .set<Scale>({1,1,1}).set<RigidBody>({0,1})
             .set<Collider>({0.5,1}).set<Velocity>({0,0,0}); break;
        case PAT_GAMEPLAY:
            e.set<Position>({0,0,0}).set<Health>({100})
             .set<AIState>({0,0}).set<Inventory>({}); break;
        case PAT_LIGHT:
            e.set<Position>({0,0,0}).set<LightSource>({8,0xFFFFFF}); break;
        case PAT_PARTICLE:
            e.set<Position>({0,0,0}).set<Velocity>({0,0,0})
             .set<ParticleEmitter>({10,100}); break;
        case PAT_COUNT: break;
        }
    }
    world.defer_end();
    world.progress(); // merge deferred
}

static void populateFragmented(flecs::world& world, int count) {
    // Use each component independently (random 50% inclusion each)
    // This creates up to 2^10 = 1024 unique archetypes — worst-case fragmentation
    std::mt19937 rng(42);
    std::bernoulli_distribution coin(0.5);
    world.defer_begin();
    for (int i = 0; i < count; ++i) {
        auto e = world.entity();
        e.set<Position>({0,0,0});
        if (coin(rng)) e.set<Velocity>({0,0,0});
        if (coin(rng)) e.set<Rotation>({1,0,0,0});
        if (coin(rng)) e.set<Scale>({1,1,1});
        if (coin(rng)) e.set<MeshInstance>({0});
        if (coin(rng)) e.set<Health>({100});
        if (coin(rng)) e.set<AIState>({0,0});
        if (coin(rng)) e.set<LightSource>({8,0xFFFFFF});
        if (coin(rng)) e.set<Item>({1,1});
        if (coin(rng)) e.add<PlayerTag>();
        if (coin(rng)) e.add<EnemyTag>();
    }
    world.defer_end();
    world.progress();
}

int main() {
    std::printf("=== ECS 1M Entity Bottleneck Benchmark ===\n");
    std::printf("Flecs v%d.%d.%d  |  Zen 3 5800X\n\n",
        FLECS_VERSION_MAJOR, FLECS_VERSION_MINOR, FLECS_VERSION_PATCH);

    constexpr int W = 5;  // warmup
    constexpr int N = 15; // iterations
    constexpr int SCALES[] = {10000, 100000, 1000000};
    constexpr int N_SCALES = 3;

    // --- BENCH 1: Create throughput (separate world each measurement to get clean timing) ---
    std::printf("--- BENCH 1: Create throughput (deferred) ---\n");
    std::printf("%-12s %-10s %-10s %-10s %-10s %-10s\n",
        "pattern", "count", "mean_ms", "median", "p95", "ns/ent");
    for (int p = 0; p < PAT_COUNT; ++p) {
        for (int s = 0; s < N_SCALES; ++s) {
            int count = SCALES[s];
            auto m = measure(W, N, [&]() {
                flecs::world w;
                populateWorld(w, count, static_cast<ArchetypePattern>(p));
            });
            m.nsPerOp = (m.mean * 1000000.0) / count;
            std::printf("%-12s %-10d %-10.3f %-10.3f %-10.3f %-10.1f\n",
                PAT_NAMES[p], count, m.mean, m.median, m.p95, m.nsPerOp);
        }
    }

    // --- BENCH 2: Iteration cost (single archetype, measure body only) ---
    std::printf("\n--- BENCH 2: Query iteration (Pos+Vel only, patterns with both) ---\n");
    std::printf("%-12s %-10s %-10s %-10s %-10s %-10s\n",
        "pattern", "count", "mean_ms", "median", "p95", "ns/ent");
    for (int p : {PAT_SIMPLE, PAT_PARTICLE}) {
        for (int s = 0; s < N_SCALES; ++s) {
            int count = SCALES[s];
            flecs::world w;
            populateWorld(w, count, static_cast<ArchetypePattern>(p));
            auto q = w.query<Position, Velocity>();
            // warmup
            for (int i = 0; i < 3; ++i) q.each([](Position& p, Velocity& v) { p.x += v.x; });

            auto m = measure(W, N, [&]() {
                q.each([](Position& p, Velocity& v) { p.x += v.x; });
            });
            m.nsPerOp = (m.mean * 1000000.0) / count;
            std::printf("%-12s %-10d %-10.5f %-10.5f %-10.5f %-10.2f\n",
                PAT_NAMES[p], count, m.mean, m.median, m.p95, m.nsPerOp);
        }
    }

    // --- BENCH 3: Fragmentation impact ---
    std::printf("\n--- BENCH 3: Fragmentation impact ---\n");
    std::printf("%-5s %-10s %-10s %-10s %-10s %-12s %-10s\n",
        "type", "count", "mean_ms", "median", "p95", "tables", "ns/ent");
    for (int s = 0; s < N_SCALES; ++s) {
        int count = SCALES[s];

        // Single archetype
        flecs::world ws;
        populateWorld(ws, count, PAT_FULL);
        auto qs = ws.query<Position>();
        for (int i = 0; i < 3; ++i) qs.each([](Position&) {});
        auto ms = measure(W, N, [&]() { qs.each([](Position&) {}); });
        ms.nsPerOp = (ms.mean * 1000000.0) / count;
        int ts = ws.get_info()->table_count;
        std::printf("single   %-10d %-10.5f %-10.5f %-10.5f %-12d %-10.2f\n",
            count, ms.mean, ms.median, ms.p95, ts, ms.nsPerOp);

        // Fragmented
        flecs::world wf;
        populateFragmented(wf, count);
        auto qf = wf.query<Position>();
        for (int i = 0; i < 3; ++i) qf.each([](Position&) {});
        auto mf = measure(W, N, [&]() { qf.each([](Position&) {}); });
        mf.nsPerOp = (mf.mean * 1000000.0) / count;
        int tf = wf.get_info()->table_count;
        double slowdown = mf.mean / ms.mean;
        std::printf("fragmnt  %-10d %-10.5f %-10.5f %-10.5f %-12d %-10.2f  (%.2f×)\n",
            count, mf.mean, mf.median, mf.p95, tf, mf.nsPerOp, slowdown);
    }

    // --- BENCH 4: Add/remove component cost ---
    std::printf("\n--- BENCH 4: Add/remove component (archetype move) ---\n");
    std::printf("%-10s %-10s %-10s %-10s %-10s\n",
        "count", "mean_ms", "median", "p95", "ns/op");
    for (int s = 0; s < N_SCALES; ++s) {
        int count = SCALES[s];
        // Prepare: create entities with Position only
        flecs::world w;
        w.defer_begin();
        for (int i = 0; i < count; ++i) w.entity().set<Position>({1,2,3});
        w.defer_end(); w.progress();

        auto m = measure(W, N, [&]() {
            // Read existing entities, add Velocity then remove it
            auto q = w.query<Position>();
            q.each([](flecs::entity e, Position&) { e.set<Velocity>({0,0,0}); });
            w.progress();
            q.each([](flecs::entity e, Position&) { e.remove<Velocity>(); });
            w.progress();
        });
        m.nsPerOp = (m.mean * 1000000.0) / (count * 2);
        std::printf("%-10d %-10.3f %-10.3f %-10.3f %-10.2f\n",
            count, m.mean, m.median, m.p95, m.nsPerOp);
    }

    // --- BENCH 5: Entity deletion ---
    std::printf("\n--- BENCH 5: Entity deletion ---\n");
    std::printf("%-12s %-10s %-10s %-10s %-10s\n",
        "pattern", "count", "mean_ms", "median", "p95");
    for (int p : {PAT_SIMPLE, PAT_FULL, PAT_PHYSICS}) {
        for (int s = 0; s < N_SCALES; ++s) {
            int count = SCALES[s];
            auto m = measure(W, N, [&]() {
                flecs::world w;
                populateWorld(w, count, static_cast<ArchetypePattern>(p));
                // Delete all entities
                auto q = w.query<Position>();
                q.each([](flecs::entity e, Position&) { e.destruct(); });
                w.progress();
            });
            std::printf("%-12s %-10d %-10.3f %-10.3f %-10.3f\n",
                PAT_NAMES[p], count, m.mean, m.median, m.p95);
        }
    }

    // --- BENCH 6: Live gameplay cycle (create + iterate + delete per frame) ---
    std::printf("\n--- BENCH 6: Live gameplay cycle (100K ents, 100 frames simulated) ---\n");
    {
        flecs::world w;
        populateWorld(w, 100000, PAT_FULL);
        w.progress();
        auto q = w.query<Position, Velocity>();
        // Simulate 100 frames: iterate + small mutation each frame
        auto m = measure(2, 10, [&]() {
            for (int f = 0; f < 100; ++f) {
                q.each([](Position& p, Velocity& v) {
                    p.x += v.x; p.y += v.y; p.z += v.z;
                });
                // Spawn a few entities per frame (simulating particle spawns)
                w.defer_begin();
                for (int i = 0; i < 10; ++i) {
                    w.entity().set<Position>({0,0,0}).set<Velocity>({0,0,0});
                }
                w.defer_end();
            }
            w.progress();
        });
        double usPerFrame = (m.mean * 1000.0) / 100.0;
        std::printf("100 frames total: %.3f ms  (%.2f µs/frame)\n", m.mean, usPerFrame);
    }

    // --- BENCH 7: Memory overhead ---
    std::printf("\n--- BENCH 7: Memory overhead (tables + archetypes at 100K entities) ---\n");
    std::printf("%-12s %-10s %-10s\n", "pattern", "tables", "components");
    for (int p = 0; p < PAT_COUNT; ++p) {
        flecs::world w;
        populateWorld(w, 100000, static_cast<ArchetypePattern>(p));
        std::printf("%-12s %-10d %-10d\n",
            PAT_NAMES[p], w.get_info()->table_count, w.get_info()->component_id_count);
    }

    std::printf("\nDone.\n");
    return 0;
}
