#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <vector>

// ---- config ----
constexpr int CHUNK_W = 16;
constexpr int WORLD_W = 4;
constexpr int CHUNK_VOL = CHUNK_W * CHUNK_W * CHUNK_W;
constexpr int WORLD_VX = WORLD_W * CHUNK_W; // 64
constexpr int WORLD_VY = 1 * CHUNK_W;       // 16
constexpr int WORLD_VZ = WORLD_W * CHUNK_W; // 64

enum Voxel : uint8_t { AIR = 0, GROUND = 1, BUILDING = 2, VEHICLE = 3, UNIT = 4, STONE = 5, GRASS = 6, ROAD = 7 };

struct Vec3 { float x, y, z; };
struct VoxelWorld {
    std::vector<uint8_t> v;
    VoxelWorld() : v(WORLD_VX * WORLD_VY * WORLD_VZ, AIR) {}
    uint8_t& at(int x, int y, int z) { return v[(y * WORLD_VZ + z) * WORLD_VX + x]; }
    const uint8_t& at(int x, int y, int z) const { return v[(y * WORLD_VZ + z) * WORLD_VX + x]; }
    bool in_bounds(int x, int y, int z) const { return x>=0 && x<WORLD_VX && y>=0 && y<WORLD_VY && z>=0 && z<WORLD_VZ; }
    bool nonempty_chunk(int cx, int cz) const {
        int bx = cx * CHUNK_W, bz = cz * CHUNK_W;
        for (int dy = 0; dy < WORLD_VY; ++dy)
            for (int dz = 0; dz < CHUNK_W; ++dz)
                for (int dx = 0; dx < CHUNK_W; ++dx)
                    if (at(bx+dx, dy, bz+dz) != AIR) return true;
        return false;
    }
};

// ---- scene generators ----
void gen_open_terrain(VoxelWorld& w, int seed) {
    std::mt19937 rng(seed);
    for (int x = 0; x < WORLD_VX; ++x)
        for (int z = 0; z < WORLD_VZ; ++z)
            for (int y = 0; y < 3; ++y)
                w.at(x, y, z) = GROUND;
    for (int x = 0; x < WORLD_VX; ++x)
        for (int z = 0; z < WORLD_VZ; ++z)
            if ((x/4 + z/4 + seed) & 1)
                w.at(x, 3, z) = GRASS;
}

void gen_urban(VoxelWorld& w, int seed) {
    gen_open_terrain(w, seed);
    std::mt19937 rng(seed + 1);
    for (int bi = 0; bi < 3; ++bi) {
        int bx = rng() % (WORLD_VX - 12) + 4;
        int bz = rng() % (WORLD_VZ - 12) + 4;
        int bh = rng() % 6 + 4;
        for (int dx = 0; dx < 10; ++dx)
            for (int dz = 0; dz < 10; ++dz)
                for (int dy = 0; dy < bh; ++dy)
                    w.at(bx+dx, 3+dy, bz+dz) = BUILDING;
    }
}

void gen_vehicle_encounter(VoxelWorld& w, int seed) {
    gen_open_terrain(w, seed);
    std::mt19937 rng(seed + 2);
    for (int vi = 0; vi < 5; ++vi) {
        int vx = rng() % (WORLD_VX - 6) + 3;
        int vz = rng() % (WORLD_VZ - 6) + 3;
        int vl = rng() % 4 + 3, vw = rng() % 2 + 1, vh = rng() % 2 + 1;
        for (int dx = 0; dx < vl; ++dx)
            for (int dz = 0; dz < vw; ++dz)
                for (int dy = 0; dy < vh; ++dy)
                    w.at(vx+dx, 2+dy, vz+dz) = VEHICLE;
    }
}

void gen_mixed_battlefield(VoxelWorld& w, int seed) {
    gen_open_terrain(w, seed);
    std::mt19937 rng(seed + 3);
    for (int bi = 0; bi < 3; ++bi) {
        int bx = rng() % (WORLD_VX - 8) + 2;
        int bz = rng() % (WORLD_VZ - 8) + 2;
        int bh = rng() % 4 + 3;
        for (int dx = 0; dx < 6; ++dx)
            for (int dz = 0; dz < 6; ++dz)
                for (int dy = 0; dy < bh; ++dy)
                    w.at(bx+dx, 3+dy, bz+dz) = BUILDING;
    }
    for (int ui = 0; ui < 8; ++ui) {
        int ux = rng() % (WORLD_VX - 2) + 1;
        int uz = rng() % (WORLD_VZ - 2) + 1;
        w.at(ux, 2, uz) = UNIT;
        w.at(ux, 3, uz) = UNIT;
    }
    for (int vi = 0; vi < 3; ++vi) {
        int vx = rng() % (WORLD_VX - 5) + 2;
        int vz = rng() % (WORLD_VZ - 5) + 2;
        for (int dx = 0; dx < 4; ++dx)
            for (int dz = 0; dz < 2; ++dz)
                for (int dy = 0; dy < 2; ++dy)
                    w.at(vx+dx, 2+dy, vz+dz) = VEHICLE;
    }
}

void gen_cave(VoxelWorld& w, int seed) {
    for (int x = 0; x < WORLD_VX; ++x)
        for (int z = 0; z < WORLD_VZ; ++z)
            for (int y = 0; y < WORLD_VY; ++y)
                w.at(x, y, z) = STONE;
    std::mt19937 rng(seed + 4);
    for (int ci = 0; ci < 3; ++ci) {
        int cx = rng() % (WORLD_VX - 16) + 8;
        int cz = rng() % (WORLD_VZ - 16) + 8;
        int cy = rng() % 6 + 3;
        int cr = rng() % 4 + 3;
        for (int dx = -cr; dx <= cr; ++dx)
            for (int dz = -cr; dz <= cr; ++dz)
                for (int dy = -cr; dy <= cr; ++dy)
                    if (dx*dx + dy*dy + dz*dz <= cr*cr)
                        if (w.in_bounds(cx+dx, cy+dy, cz+dz))
                            w.at(cx+dx, cy+dy, cz+dz) = AIR;
    }
}

using SceneGen = void(*)(VoxelWorld&, int);
SceneGen scene_gens[5] = { gen_open_terrain, gen_urban, gen_vehicle_encounter, gen_mixed_battlefield, gen_cave };
const char* scene_names[5] = { "s1_open_terrain", "s2_urban_building", "s3_vehicle_encounter", "s4_mixed_battlefield", "s5_underground_cave" };

// ---- strategies ----
struct PingResult {
    int64_t ns;
    int voxels_traversed;
    uint8_t detected_voxel; // what we detected (0 = none)
    uint8_t expected_voxel; // what we should have detected
};

// A: baseline — no ping, 0 cost
PingResult strat_A_no_ping(const VoxelWorld&, Vec3, Vec3, Vec3 target, uint8_t expected) {
    return {0, 0, 0, expected};
}

// B: point marker — just output position, no traversal
PingResult strat_B_point_marker(const VoxelWorld&, Vec3, Vec3, Vec3 target, uint8_t expected) {
    return {0, 0, 0, expected};
}

// C: Amanatides-Woo 3D DDA full voxel traversal
PingResult strat_C_amanatides_woo(const VoxelWorld& w, Vec3 origin, Vec3 dir, Vec3, uint8_t expected) {
    auto t0 = std::chrono::steady_clock::now();

    // Step into grid voxel space
    float ox = origin.x, oy = origin.y, oz = origin.z;
    float dx = dir.x, dy = dir.y, dz = dir.z;

    // Starting voxel
    int ix = int(std::floor(ox));
    int iy = int(std::floor(oy));
    int iz = int(std::floor(oz));

    // Step direction
    int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    int sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    int sz = dz > 0 ? 1 : (dz < 0 ? -1 : 0);

    // tMax: distance to next voxel boundary along each axis
    float tMaxX = sx > 0 ? (ix + 1 - ox) / dx : (sx < 0 ? (ix - ox) / dx : 1e30f);
    float tMaxY = sy > 0 ? (iy + 1 - oy) / dy : (sy < 0 ? (iy - oy) / dy : 1e30f);
    float tMaxZ = sz > 0 ? (iz + 1 - oz) / dz : (sz < 0 ? (iz - oz) / dz : 1e30f);

    float tDeltaX = sx != 0 ? 1.0f / (dx * sx) : 1e30f;
    float tDeltaY = sy != 0 ? 1.0f / (dy * sy) : 1e30f;
    float tDeltaZ = sz != 0 ? 1.0f / (dz * sz) : 1e30f;

    int traversed = 0;
    uint8_t detected = 0;
    constexpr int MAX_STEPS = 200;

    for (int step = 0; step < MAX_STEPS; ++step) {
        if (w.in_bounds(ix, iy, iz)) {
            uint8_t v = w.at(ix, iy, iz);
            ++traversed;
            if (v != AIR) { detected = v; break; }
        }
        if (tMaxX < tMaxY) {
            if (tMaxX > 50.0f) break;
            ix += sx; tMaxX += tDeltaX;
        } else if (tMaxY < tMaxZ) {
            if (tMaxY > 50.0f) break;
            iy += sy; tMaxY += tDeltaY;
        } else {
            if (tMaxZ > 50.0f) break;
            iz += sz; tMaxZ += tDeltaZ;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    return {std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count(), traversed, detected, expected};
}

// D: Hierarchical DDA — test chunk AABBs first, then DDA within hit chunk
PingResult strat_D_hierarchical(const VoxelWorld& w, Vec3 origin, Vec3 dir, Vec3 target, uint8_t expected) {
    auto t0 = std::chrono::steady_clock::now();

    // Chunk-level traversal
    float ox = origin.x, oy = origin.y, oz = origin.z;
    float dx = dir.x, dy = dir.y, dz = dir.z;

    int cx = int(std::floor(ox / CHUNK_W));
    int cy = 0; // only 1 chunk high
    int cz = int(std::floor(oz / CHUNK_W));

    int scx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    int scy = 0; // y chunk count = 1, no stepping
    int scz = dz > 0 ? 1 : (dz < 0 ? -1 : 0);

    float tMaxCX = scx > 0 ? ((cx + 1) * CHUNK_W - ox) / dx : (scx < 0 ? (cx * CHUNK_W - ox) / dx : 1e30f);
    float tMaxCZ = scz > 0 ? ((cz + 1) * CHUNK_W - oz) / dz : (scz < 0 ? (cz * CHUNK_W - oz) / dz : 1e30f);

    float tDeltaCX = scx != 0 ? 1.0f / (dx / CHUNK_W) : 1e30f;
    float tDeltaCZ = scz != 0 ? 1.0f / (dz / CHUNK_W) : 1e30f;

    int traversed = 0;
    uint8_t detected = 0;
    bool found = false;

    for (int cstep = 0; cstep < 20; ++cstep) {
        if (cx >= 0 && cx < WORLD_W && cz >= 0 && cz < WORLD_W) {
            if (w.nonempty_chunk(cx, cz)) {
                // Find entry point into this chunk
                float t_entry = 0;
                if (cstep == 0) t_entry = 0; // started in this chunk
                else t_entry = std::min({tMaxCX, tMaxCZ, 50.0f});

                float ex = ox + dx * t_entry;
                float ey = oy + dy * t_entry;
                float ez = oz + dz * t_entry;

                int ix = int(std::floor(ex));
                int iy = int(std::floor(ey));
                int iz = int(std::floor(ez));

                int sx_v = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
                int sy_v = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
                int sz_v = dz > 0 ? 1 : (dz < 0 ? -1 : 0);

                float tMaxX_v = sx_v > 0 ? (ix + 1 - ex) / dx : (sx_v < 0 ? (ix - ex) / dx : 1e30f);
                float tMaxY_v = sy_v > 0 ? (iy + 1 - ey) / dy : (sy_v < 0 ? (iy - ey) / dy : 1e30f);
                float tMaxZ_v = sz_v > 0 ? (iz + 1 - ez) / dz : (sz_v < 0 ? (iz - ez) / dz : 1e30f);

                float tDeltaX_v = sx_v != 0 ? 1.0f / (dx * sx_v) : 1e30f;
                float tDeltaY_v = sy_v != 0 ? 1.0f / (dy * sy_v) : 1e30f;
                float tDeltaZ_v = sz_v != 0 ? 1.0f / (dz * sz_v) : 1e30f;

                int chunk_bx = cx * CHUNK_W, chunk_bz = cz * CHUNK_W;

                for (int vstep = 0; vstep < 100; ++vstep) {
                    if (w.in_bounds(ix, iy, iz)) {
                        uint8_t v = w.at(ix, iy, iz);
                        ++traversed;
                        if (v != AIR) { detected = v; found = true; break; }
                    }
                    // Check if we left this chunk
                    if (ix < chunk_bx || ix >= chunk_bx + CHUNK_W || iz < chunk_bz || iz >= chunk_bz + CHUNK_W)
                        break;

                    if (tMaxX_v < tMaxY_v) {
                        if (tMaxX_v > 50.0f) break;
                        ix += sx_v; tMaxX_v += tDeltaX_v;
                    } else if (tMaxY_v < tMaxZ_v) {
                        if (tMaxY_v > 50.0f) break;
                        iy += sy_v; tMaxY_v += tDeltaY_v;
                    } else {
                        if (tMaxZ_v > 50.0f) break;
                        iz += sz_v; tMaxZ_v += tDeltaZ_v;
                    }
                }
                if (found) break;
            }
        }
        // Advance chunk step
        if (tMaxCX < tMaxCZ) {
            if (tMaxCX > 50.0f) break;
            cx += scx; tMaxCX += tDeltaCX;
        } else {
            if (tMaxCZ > 50.0f) break;
            cz += scz; tMaxCZ += tDeltaCZ;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    return {std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count(), traversed, detected, expected};
}

// E: Multi-sample area ping — 5 rays, majority vote
PingResult strat_E_multi_sample(const VoxelWorld& w, Vec3 origin, Vec3 dir, Vec3 target, uint8_t expected) {
    auto t0 = std::chrono::steady_clock::now();

    // Generate 5 ray directions: center + 4 offset samples in cone
    float spread = 0.05f; // small angular spread
    float offsets[5][2] = {{0,0}, {spread,0}, {-spread,0}, {0,spread}, {0,-spread}};

    std::array<uint8_t, 5> results = {};
    int total_traversed = 0;
    bool found_any = false;
    uint8_t majority = 0;

    for (int si = 0; si < 5; ++si) {
        Vec3 sdir = {dir.x + offsets[si][0], dir.y + offsets[si][1], dir.z};
        float len = std::sqrt(sdir.x*sdir.x + sdir.y*sdir.y + sdir.z*sdir.z);
        sdir.x /= len; sdir.y /= len; sdir.z /= len;

        float ox = origin.x, oy = origin.y, oz = origin.z;
        float dx = sdir.x, dy = sdir.y, dz = sdir.z;

        int ix = int(std::floor(ox));
        int iy = int(std::floor(oy));
        int iz = int(std::floor(oz));

        int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
        int sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
        int sz = dz > 0 ? 1 : (dz < 0 ? -1 : 0);

        float tMaxX = sx > 0 ? (ix + 1 - ox) / dx : (sx < 0 ? (ix - ox) / dx : 1e30f);
        float tMaxY = sy > 0 ? (iy + 1 - oy) / dy : (sy < 0 ? (iy - oy) / dy : 1e30f);
        float tMaxZ = sz > 0 ? (iz + 1 - oz) / dz : (sz < 0 ? (iz - oz) / dz : 1e30f);

        float tDeltaX = sx != 0 ? 1.0f / (dx * sx) : 1e30f;
        float tDeltaY = sy != 0 ? 1.0f / (dy * sy) : 1e30f;
        float tDeltaZ = sz != 0 ? 1.0f / (dz * sz) : 1e30f;

        int traversed = 0;
        uint8_t detected = 0;

        for (int step = 0; step < 200; ++step) {
            if (w.in_bounds(ix, iy, iz)) {
                uint8_t v = w.at(ix, iy, iz);
                ++traversed;
                if (v != AIR) { detected = v; break; }
            }
            if (tMaxX < tMaxY) {
                if (tMaxX > 50.0f) break;
                ix += sx; tMaxX += tDeltaX;
            } else if (tMaxY < tMaxZ) {
                if (tMaxY > 50.0f) break;
                iy += sy; tMaxY += tDeltaY;
            } else {
                if (tMaxZ > 50.0f) break;
                iz += sz; tMaxZ += tDeltaZ;
            }
        }
        results[si] = detected;
        total_traversed += traversed;
        if (detected != 0) found_any = true;
    }

    // Majority vote among non-zero results
    if (found_any) {
        int best_count = 0;
        for (int a = 1; a <= 7; ++a) {
            int cnt = 0;
            for (int si = 0; si < 5; ++si)
                if (results[si] == a) ++cnt;
            if (cnt > best_count) { best_count = cnt; majority = uint8_t(a); }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    return {std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count(), total_traversed, majority, expected};
}

using StrategyFunc = PingResult(*)(const VoxelWorld&, Vec3, Vec3, Vec3, uint8_t);
constexpr const char* strat_names[5] = {"A_NoPing", "B_PointMarker_NoContext", "C_AmanatidesWoo_DDA", "D_Hierarchical_DDA", "E_MultiSample_AreaPing"};
StrategyFunc strat_funcs[5] = {strat_A_no_ping, strat_B_point_marker, strat_C_amanatides_woo, strat_D_hierarchical, strat_E_multi_sample};

// ---- benchmark harness ----
struct Sample {
    int64_t ns;
    int voxels;
    uint8_t detected, expected;
};

struct ConfigResult {
    const char* strat;
    const char* scene;
    int seed;
    double mean_ns, median_ns, p95_ns, p99_ns, mean_voxels;
    double accuracy; // 0-1
    int64_t min_ns, max_ns;
    int n;
};

double pct(std::span<Sample> s, double p) {
    auto idx = size_t(std::round(p * (s.size() - 1)));
    std::nth_element(s.begin(), s.begin() + ptrdiff_t(idx), s.end(),
        [](auto& a, auto& b) { return a.ns < b.ns; });
    return double(s[idx].ns);
}

int main() {
    std::vector<ConfigResult> results;
    constexpr int SEEDS = 5;
    constexpr int ITERS = 1000;
    constexpr int WARMUP = 10;
    int seeds[SEEDS] = {1, 7, 42, 1234, 31337};

    std::printf("strat,scene,seed,mean_ns,median_ns,p95_ns,p99_ns,min_ns,max_ns,mean_voxels,accuracy,n\n");

    for (int si = 0; si < 5; ++si) {
        auto gen = scene_gens[si];
        for (int seed_idx = 0; seed_idx < SEEDS; ++seed_idx) {
            int seed = seeds[seed_idx];
            VoxelWorld world;
            gen(world, seed);

            // Generate random ping points
            std::mt19937 rng(seed + 100);
            std::uniform_int_distribution<int> rx(2, WORLD_VX - 3);
            std::uniform_int_distribution<int> ry(2, WORLD_VY - 3);
            std::uniform_int_distribution<int> rz(2, WORLD_VZ - 3);

            struct PingTarget { Vec3 pos; uint8_t expected; };
            std::vector<PingTarget> targets;
            targets.reserve(ITERS);
            for (int i = 0; i < ITERS; ++i) {
                int px = rx(rng), py = ry(rng), pz = rz(rng);
                targets.push_back({{float(px), float(py), float(pz)}, world.at(px, py, pz)});
            }

            // Camera position (fixed, looking toward world center)
            Vec3 cam = {2.0f, 8.0f, 2.0f};

            for (int strat_idx = 0; strat_idx < 5; ++strat_idx) {
                auto func = strat_funcs[strat_idx];
                std::vector<Sample> samples;
                samples.reserve(ITERS);

                // Warmup
                for (int w = 0; w < WARMUP; ++w) {
                    auto& t = targets[w % targets.size()];
                    Vec3 dir = {t.pos.x - cam.x, t.pos.y - cam.y, t.pos.z - cam.z};
                    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
                    dir.x /= len; dir.y /= len; dir.z /= len;
                    func(world, cam, dir, t.pos, t.expected);
                }

                // Main measurements
                for (int i = 0; i < ITERS; ++i) {
                    auto& t = targets[i];
                    Vec3 dir = {t.pos.x - cam.x, t.pos.y - cam.y, t.pos.z - cam.z};
                    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
                    dir.x /= len; dir.y /= len; dir.z /= len;
                    auto r = func(world, cam, dir, t.pos, t.expected);
                    samples.push_back({r.ns, r.voxels_traversed, r.detected_voxel, r.expected_voxel});
                }

                // Compute stats
                std::sort(samples.begin(), samples.end(), [](auto& a, auto& b) { return a.ns < b.ns; });
                double mean = std::accumulate(samples.begin(), samples.end(), 0.0,
                    [](double s, auto& x) { return s + x.ns; }) / samples.size();
                double median = samples[samples.size()/2].ns;
                double p95v = pct(samples, 0.95);
                double p99v = pct(samples, 0.99);
                double mean_vox = std::accumulate(samples.begin(), samples.end(), 0.0,
                    [](double s, auto& x) { return s + x.voxels; }) / samples.size();
                int correct = 0;
                for (auto& s : samples)
                    if (s.detected == s.expected) ++correct;
                double accuracy = double(correct) / samples.size();
                auto [min_s, max_s] = std::minmax_element(samples.begin(), samples.end(),
                    [](auto& a, auto& b) { return a.ns < b.ns; });

                std::printf("%s,%s,%d,%.1f,%.1f,%.1f,%.1f,%ld,%ld,%.1f,%.4f,%zu\n",
                    strat_names[strat_idx], scene_names[si], seed,
                    mean, median, p95v, p99v,
                    (long)min_s->ns, (long)max_s->ns,
                    mean_vox, accuracy, samples.size());
            }
        }
    }

    return 0;
}
