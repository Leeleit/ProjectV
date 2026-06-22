// SPDX-License-Identifier: MIT
// Vehicle assembly benchmark — isolated from ProjectV mainline.
//
// Hypothesis (see README.md §1): vehicle-specific compound-collider assembly strategies
// (C..F) beat naive per-voxel baseline (A) by ≥ 10× in shape count at < 0.5 ms/vehicle
// assembly time, with 100% volume preservation.
//
// 6 strategies × 5 vehicle types × 5 seeds × 1000 iter + 10 warmup = 150,000 meas.
// Mutation rebuild: after 10% voxel toggle, measure rebuild time per strategy.
//
// Build (CMake) or:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//           vehicle_bench.cpp -o build/vehicle_bench
// Run:
//   taskset -c 2 ./build/vehicle_bench > build/results.csv
//
// Dev host: AMD Ryzen 7 5800X (Zen 3), governor `powersave` per hardware-profile.md §1.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

// ---- Types ----
using Clock = std::chrono::high_resolution_clock;

struct Box {
    int x0, y0, z0;
    int x1, y1, z1;
    int vol() const noexcept { return (x1 - x0) * (y1 - y0) * (z1 - z0); }
};

struct VehicleDef {
    std::string_view name;
    int W, H, D;                     // grid dims (x, y, z)
    int solidCount;                  // pre-computed solid voxel count
    std::vector<uint8_t> solid;      // 1 = filled, 0 = air
    std::vector<uint8_t> wheel;      // 1 = wheel voxel (for strategy F)
    std::vector<uint8_t> module;     // module ID per voxel (for strategy D)
    int moduleCount;                 // number of distinct modules
};

// ---- Index helpers ----
inline int idx3(int x, int y, int z, int W, int D) noexcept {
    return (y * D + z) * W + x;
}

// ---- Vehicle generators ----
// Each returns a VehicleDef with solid mask + wheel mask + module label.

VehicleDef gen_jeep(int W, int H, int D) {
    VehicleDef v{"Jeep_8x4x4", W, H, D, 0, {}, {}, {}, 0};
    int V = W * H * D;
    v.solid.assign(V, 0);
    v.wheel.assign(V, 0);
    v.module.assign(V, 0);
    // Hull: solid box (0,0,0)-(W,2,D) with hollow interior
    for (int y = 0; y < 2 && y < H; ++y)
        for (int z = 0; z < D; ++z)
            for (int x = 0; x < W; ++x)
                v.solid[idx3(x, y, z, W, D)] = 1;
    // Hollow out interior (leave walls thickness 1)
    for (int y = 1; y < 2 && y < H; ++y)
        for (int z = 1; z < D - 1; ++z)
            for (int x = 1; x < W - 1; ++x) {
                if (z > 0 && z < D - 1 && x > 0 && x < W - 1)
                    v.solid[idx3(x, y, z, W, D)] = 0;
            }
    // Wheels at 4 corners
    int wheelPos[4][2] = {{0,0}, {0,D-1}, {W-1,0}, {W-1,D-1}};
    for (auto [wx, wz] : wheelPos) {
        for (int dy = 0; dy < 1; ++dy) {
            int yy = 0 + dy;
            if (yy < H) {
                int ci = idx3(wx, yy, wz, W, D);
                v.solid[ci] = 1;
                v.wheel[ci] = 1;
            }
        }
    }
    // Modules: 0 = hull, 1-4 = wheels
    v.moduleCount = 5;
    for (int i = 0; i < V; ++i) {
        if (v.solid[i]) {
            // Find which wheel it belongs to (if any)
            int a = i;
            int zz = (a / W) % D;
            int xx = a % W;
            int found = 0;
            for (int wi = 0; wi < 4; ++wi) {
                if (xx == wheelPos[wi][0] && zz == wheelPos[wi][1]) {
                    found = wi + 1;
                    break;
                }
            }
            v.module[i] = static_cast<uint8_t>(found);
        } else {
            v.module[i] = 0;
        }
    }
    v.solidCount = 0;
    for (auto b : v.solid) if (b) ++v.solidCount;
    return v;
}

VehicleDef gen_apc(int W, int H, int D) {
    int V = W * H * D;
    VehicleDef v{"APC_16x8x8", W, H, D, 0, std::vector<uint8_t>(V, 0),
                 std::vector<uint8_t>(V, 0), std::vector<uint8_t>(V, 0), 0};
    // Hull: solid floor (y=0) + walls (y=1..2) + roof (y=3)
    for (int y = 0; y < 4 && y < H; ++y)
        for (int z = 0; z < D; ++z)
            for (int x = 0; x < W; ++x)
                v.solid[idx3(x, y, z, W, D)] = 1;
    // Hollow interior (y=1..2, x=1..W-2, z=1..D-2)
    for (int y = 1; y < 3 && y < H; ++y)
        for (int z = 1; z < D - 1; ++z)
            for (int x = 1; x < W - 1; ++x)
                v.solid[idx3(x, y, z, W, D)] = 0;
    // Turret (central box on roof)
    int tx0 = W/2 - 2, tz0 = D/2 - 2, tx1 = W/2 + 2, tz1 = D/2 + 2;
    for (int y = 4; y < 6 && y < H; ++y)
        for (int z = tz0; z < tz1; ++z)
            for (int x = tx0; x < tx1; ++x)
                v.solid[idx3(x, y, z, W, D)] = 1;
    // Wheels (8 wheels, 4 per side)
    for (int side = 0; side < 2; ++side) {
        int wz = (side == 0) ? 0 : D - 1;
        for (int wi = 0; wi < 4; ++wi) {
            int wx = 2 + wi * 3;
            int ci = idx3(wx, 0, wz, W, D);
            v.solid[ci] = 1;
            v.wheel[ci] = 1;
        }
    }
    v.moduleCount = 10; // hull + turret + 8 wheels
    for (int i = 0; i < V; ++i) {
        if (!v.solid[i]) continue;
        int a = i;
        int zz = (a / W) % D;
        int xx = a % W;
        // Check if wheel
        uint8_t mid = 0;
        for (int side = 0; side < 2; ++side) {
            int wz = (side == 0) ? 0 : D - 1;
            if (zz != wz) continue;
            for (int wi = 0; wi < 4; ++wi) {
                int wx = 2 + wi * 3;
                if (xx == wx) { mid = static_cast<uint8_t>(2 + side * 4 + wi); break; }
            }
            if (mid) break;
        }
        if (!mid && zz >= tz0 && zz < tz1 && xx >= tx0 && xx < tx1) mid = 1; // turret
        if (!mid) mid = 0; // hull
        v.module[i] = mid;
    }
    v.solidCount = 0;
    for (auto b : v.solid) if (b) ++v.solidCount;
    return v;
}

VehicleDef gen_tank(int W, int H, int D) {
    int V = W * H * D;
    VehicleDef v{"Tank_16x12x12", W, H, D, 0, std::vector<uint8_t>(V, 0),
                 std::vector<uint8_t>(V, 0), std::vector<uint8_t>(V, 0), 0};
    // Hull lower (y=0..2)
    for (int y = 0; y < 3 && y < H; ++y)
        for (int z = 0; z < D; ++z)
            for (int x = 0; x < W; ++x)
                v.solid[idx3(x, y, z, W, D)] = 1;
    // Hull upper (y=3..4), narrower
    int inset = 2;
    for (int y = 3; y < 5 && y < H; ++y)
        for (int z = inset; z < D - inset; ++z)
            for (int x = inset; x < W - inset; ++x)
                v.solid[idx3(x, y, z, W, D)] = 1;
    // Turret on top
    int tx0 = W/2 - 3, tz0 = D/2 - 3, tx1 = W/2 + 3, tz1 = D/2 + 3;
    for (int y = 5; y < 7 && y < H; ++y)
        for (int z = tz0; z < tz1; ++z)
            for (int x = tx0; x < tx1; ++x)
                v.solid[idx3(x, y, z, W, D)] = 1;
    // Tracks (left + right along full length)
    for (int y = 0; y < 3 && y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            int cl = idx3(x, y, 0, W, D);
            int cr = idx3(x, y, D - 1, W, D);
            v.solid[cl] = 1; v.wheel[cl] = 1;
            v.solid[cr] = 1; v.wheel[cr] = 1;
        }
    }
    v.moduleCount = 4; // hull, turret, track_L, track_R
    for (int i = 0; i < V; ++i) {
        if (!v.solid[i]) continue;
        int a = i;
        int zz = (a / W) % D;
        int yy = (a / (W * D));
        int xx = a % W;
        if (zz == 0) v.module[i] = 2; // track_L
        else if (zz == D - 1) v.module[i] = 3; // track_R
        else if (yy >= 5 && zz >= tz0 && zz < tz1 && xx >= tx0 && xx < tx1) v.module[i] = 1;
        else v.module[i] = 0;
    }
    v.solidCount = 0;
    for (auto b : v.solid) if (b) ++v.solidCount;
    return v;
}

VehicleDef gen_truck(int W, int H, int D) {
    int V = W * H * D;
    VehicleDef v{"Truck_24x8x8", W, H, D, 0, std::vector<uint8_t>(V, 0),
                 std::vector<uint8_t>(V, 0), std::vector<uint8_t>(V, 0), 0};
    // Cabin (front section, x=0..W/3)
    int cabinEnd = W / 3;
    for (int y = 0; y < 3 && y < H; ++y)
        for (int z = 0; z < D; ++z)
            for (int x = 0; x < cabinEnd; ++x)
                v.solid[idx3(x, y, z, W, D)] = 1;
    // Cargo bed (rear section, x=W/3..W)
    for (int y = 0; y < 2 && y < H; ++y)
        for (int z = 0; z < D; ++z)
            for (int x = cabinEnd; x < W; ++x)
                v.solid[idx3(x, y, z, W, D)] = 1;
    // Cargo side walls
    for (int y = 2; y < 4 && y < H; ++y) {
        for (int x = cabinEnd; x < W; ++x) {
            v.solid[idx3(x, y, 0, W, D)] = 1;
            v.solid[idx3(x, y, D - 1, W, D)] = 1;
        }
    }
    // Wheels (6, along both sides)
    for (int side = 0; side < 2; ++side) {
        int wz = (side == 0) ? 0 : D - 1;
        for (int wi = 0; wi < 3; ++wi) {
            int wx = 2 + wi * 7;
            int ci = idx3(wx, 0, wz, W, D);
            v.solid[ci] = 1;
            v.wheel[ci] = 1;
        }
    }
    v.moduleCount = 8; // cabin, cargo, 6 wheels
    for (int i = 0; i < V; ++i) {
        if (!v.solid[i]) continue;
        int a = i;
        int zz = (a / W) % D;
        int xx = a % W;
        uint8_t mid = 0;
        for (int side = 0; side < 2; ++side) {
            int wz = (side == 0) ? 0 : D - 1;
            if (zz != wz) continue;
            for (int wi = 0; wi < 3; ++wi) {
                int wx = 2 + wi * 7;
                if (xx == wx) { mid = static_cast<uint8_t>(2 + side * 3 + wi); break; }
            }
            if (mid) break;
        }
        if (!mid) mid = (xx < cabinEnd) ? 0 : 1;
        v.module[i] = mid;
    }
    v.solidCount = 0;
    for (auto b : v.solid) if (b) ++v.solidCount;
    return v;
}

VehicleDef gen_large_ship(int W, int H, int D) {
    int V = W * H * D;
    VehicleDef v{"LargeShip_32x16x16", W, H, D, 0, std::vector<uint8_t>(V, 0),
                 std::vector<uint8_t>(V, 0), std::vector<uint8_t>(V, 0), 0};
    // Hull: V-shaped bottom (y=0 narrow, wider at top)
    int halfW = W / 2;
    for (int y = 0; y < 8 && y < H; ++y) {
        int inset = 8 - y;
        if (inset < 0) inset = 0;
        for (int z = 0; z < D; ++z)
            for (int x = inset; x < W - inset; ++x)
                v.solid[idx3(x, y, z, W, D)] = 1;
    }
    // Deck (y=8)
    int yDeck = 8;
    if (yDeck < H) {
        for (int z = 0; z < D; ++z)
            for (int x = 0; x < W; ++x)
                v.solid[idx3(x, yDeck, z, W, D)] = 1;
    }
    // Superstructure (central block above deck)
    int ssx0 = halfW - 4, ssx1 = halfW + 4;
    int ssz0 = D/2 - 3, ssz1 = D/2 + 3;
    for (int y = 9; y < 12 && y < H; ++y)
        for (int z = ssz0; z < ssz1; ++z)
            for (int x = ssx0; x < ssx1; ++x)
                v.solid[idx3(x, y, z, W, D)] = 1;
    v.moduleCount = 3; // hull, deck, superstructure
    for (int i = 0; i < V; ++i) {
        if (!v.solid[i]) continue;
        int a = i;
        int yy = a / (W * D);
        if (yy >= 9) v.module[i] = 2; // superstructure
        else if (yy == 8) v.module[i] = 1; // deck
        else v.module[i] = 0; // hull
    }
    v.solidCount = 0;
    for (auto b : v.solid) if (b) ++v.solidCount;
    return v;
}

// ---- Strategy registry ----
using StrategyFn = std::vector<Box>(*)(const VehicleDef&);

struct Strategy {
    std::string_view name;
    StrategyFn fn;
    // For mutation: 0=full rebuild, 1=per-component rebuild, 2=sub-module
    int rebuildMode;
};

// =============================================================================
// Strategy A: NAIVE PER-VOXEL BASELINE
// =============================================================================
std::vector<Box> strategy_A_naive(const VehicleDef& v) {
    std::vector<Box> out;
    out.reserve(v.solidCount);
    for (int y = 0; y < v.H; ++y)
        for (int z = 0; z < v.D; ++z)
            for (int x = 0; x < v.W; ++x)
                if (v.solid[idx3(x, y, z, v.W, v.D)])
                    out.push_back({x, y, z, x + 1, y + 1, z + 1});
    return out;
}

// =============================================================================
// Strategy B: PRECOMPUTED BLUEPRINT COLLIDERS
// =============================================================================
// Models loading pre-computed merged AABBs from blueprint JSON.
// For each module in the vehicle, we emit one large AABB covering the entire module.
std::vector<Box> strategy_B_precomputed(const VehicleDef& v) {
    // Find bounding box per module
    std::vector<Box> out;
    struct BB { int x0=v.W, y0=v.H, z0=v.D, x1=0, y1=0, z1=0; bool valid=false; };
    std::vector<BB> modBB(v.moduleCount, BB{});
    for (int y = 0; y < v.H; ++y) {
        for (int z = 0; z < v.D; ++z) {
            for (int x = 0; x < v.W; ++x) {
                int ci = idx3(x, y, z, v.W, v.D);
                if (!v.solid[ci]) continue;
                int m = v.module[ci];
                if (m < 0 || m >= v.moduleCount) continue;
                auto& bb = modBB[m];
                if (x < bb.x0) bb.x0 = x;
                if (x >= bb.x1) bb.x1 = x + 1;
                if (y < bb.y0) bb.y0 = y;
                if (y >= bb.y1) bb.y1 = y + 1;
                if (z < bb.z0) bb.z0 = z;
                if (z >= bb.z1) bb.z1 = z + 1;
                bb.valid = true;
            }
        }
    }
    out.reserve(v.moduleCount);
    for (auto& bb : modBB)
        if (bb.valid)
            out.push_back({bb.x0, bb.y0, bb.z0, bb.x1, bb.y1, bb.z1});
    return out;
}

// =============================================================================
// Strategy C: GREEDY PHYSICS MERGE (F_TwoPass from closed experiment)
// =============================================================================
std::vector<Box> strategy_C_greedy_merge(const VehicleDef& v) {
    // Make a mutable copy of solid mask (we mark processed cells)
    std::vector<uint8_t> work = v.solid;
    int W = v.W, H = v.H, D = v.D;

    struct Rect { int x0, x1, z0, z1; };
    std::vector<std::vector<Rect>> slices(H);
    for (int y = 0; y < H; ++y) {
        auto& s = slices[y];
        for (int z = 0; z < D; ++z) {
            for (int x = 0; x < W; ++x) {
                if (work[idx3(x, y, z, W, D)] == 0) continue;
                int x0 = x;
                while (x < W && work[idx3(x, y, z, W, D)] != 0) ++x;
                int x1 = x;
                int z1 = z + 1;
                while (z1 < D) {
                    bool fullRow = true;
                    for (int xi = x0; xi < x1; ++xi)
                        if (work[idx3(xi, y, z1, W, D)] == 0) { fullRow = false; break; }
                    if (!fullRow) break;
                    for (int xi = x0; xi < x1; ++xi) work[idx3(xi, y, z1, W, D)] = 0;
                    ++z1;
                }
                for (int xi = x0; xi < x1; ++xi) work[idx3(xi, y, z, W, D)] = 0;
                s.push_back({x0, x1, z, z1});
            }
        }
    }

    // Pass 2: vertical merge of identical XZ rects
    std::vector<Box> out;
    std::vector<std::vector<bool>> visited(H);
    for (int y = 0; y < H; ++y) visited[y].assign(slices[y].size(), false);

    for (int y = 0; y < H; ++y) {
        for (size_t ri = 0; ri < slices[y].size(); ++ri) {
            if (visited[y][ri]) continue;
            const auto& r = slices[y][ri];
            int y0 = y, y1 = y + 1;
            while (y1 < H) {
                bool match = false;
                for (size_t rj = 0; rj < slices[y1].size(); ++rj) {
                    if (visited[y1][rj]) continue;
                    const auto& rn = slices[y1][rj];
                    if (rn.x0 == r.x0 && rn.x1 == r.x1 && rn.z0 == r.z0 && rn.z1 == r.z1) {
                        visited[y1][rj] = true;
                        match = true; break;
                    }
                }
                if (!match) break;
                ++y1;
            }
            visited[y][ri] = true;
            out.push_back({r.x0, y0, r.z0, r.x1, y1, r.z1});
        }
    }
    return out;
}

// =============================================================================
// Strategy D: HIERARCHICAL SUB-ASSEMBLY
// =============================================================================
// Per-module F_TwoPass merge, then combine. On mutation: rebuild only affected module.
// For the benchmark, we do full build (module decomposition + per-module merge).
std::vector<Box> strategy_D_hierarchical(const VehicleDef& v) {
    int W = v.W, H = v.H, D = v.D;

    // Collect voxel indices per module
    std::vector<std::vector<int>> moduleVoxels(v.moduleCount);
    for (int y = 0; y < H; ++y)
        for (int z = 0; z < D; ++z)
            for (int x = 0; x < W; ++x) {
                int ci = idx3(x, y, z, W, D);
                if (!v.solid[ci]) continue;
                int m = v.module[ci];
                if (m >= 0 && m < v.moduleCount)
                    moduleVoxels[m].push_back(ci);
            }

    std::vector<Box> out;
    struct Rect { int x0, x1, z0, z1; };

    for (int mi = 0; mi < v.moduleCount; ++mi) {
        if (moduleVoxels[mi].empty()) continue;

        // Build sub-mask for this module
        std::vector<uint8_t> sub(v.solid.size(), 0);
        for (int ci : moduleVoxels[mi]) sub[ci] = 1;

        // F_TwoPass on sub-mask
        std::vector<std::vector<Rect>> slices(H);
        for (int y = 0; y < H; ++y) {
            auto& s = slices[y];
            for (int z = 0; z < D; ++z) {
                for (int x = 0; x < W; ++x) {
                    if (sub[idx3(x, y, z, W, D)] == 0) continue;
                    int x0 = x;
                    while (x < W && sub[idx3(x, y, z, W, D)] != 0) ++x;
                    int x1 = x;
                    int z1 = z + 1;
                    while (z1 < D) {
                        bool fullRow = true;
                        for (int xi = x0; xi < x1; ++xi)
                            if (sub[idx3(xi, y, z1, W, D)] == 0) { fullRow = false; break; }
                        if (!fullRow) break;
                        for (int xi = x0; xi < x1; ++xi) sub[idx3(xi, y, z1, W, D)] = 0;
                        ++z1;
                    }
                    for (int xi = x0; xi < x1; ++xi) sub[idx3(xi, y, z, W, D)] = 0;
                    s.push_back({x0, x1, z, z1});
                }
            }
        }

        std::vector<std::vector<bool>> visited(H);
        for (int y = 0; y < H; ++y) visited[y].assign(slices[y].size(), false);

        for (int y = 0; y < H; ++y) {
            for (size_t ri = 0; ri < slices[y].size(); ++ri) {
                if (visited[y][ri]) continue;
                const auto& r = slices[y][ri];
                int y0 = y, y1 = y + 1;
                while (y1 < H) {
                    bool match = false;
                    for (size_t rj = 0; rj < slices[y1].size(); ++rj) {
                        if (visited[y1][rj]) continue;
                        const auto& rn = slices[y1][rj];
                        if (rn.x0 == r.x0 && rn.x1 == r.x1 && rn.z0 == r.z0 && rn.z1 == r.z1) {
                            visited[y1][rj] = true; match = true; break;
                        }
                    }
                    if (!match) break;
                    ++y1;
                }
                visited[y][ri] = true;
                out.push_back({r.x0, y0, r.z0, r.x1, y1, r.z1});
            }
        }
    }
    return out;
}

// =============================================================================
// Strategy E: HYBRID TEMPLATE + GREEDY MERGE
// =============================================================================
// Template shapes for wheels (unit boxes, kept separate).
// Greedy F_TwoPass for hull voxels.
std::vector<Box> strategy_E_hybrid(const VehicleDef& v) {
    int W = v.W, H = v.H, D = v.D;
    std::vector<uint8_t> hull = v.solid;

    // Remove wheel voxels from hull mask — they use template shapes
    std::vector<Box> out;
    out.reserve(v.solidCount);

    // Emit unit boxes for wheel voxels (template shapes)
    for (int y = 0; y < H; ++y)
        for (int z = 0; z < D; ++z)
            for (int x = 0; x < W; ++x) {
                int ci = idx3(x, y, z, W, D);
                if (v.wheel[ci]) {
                    out.push_back({x, y, z, x + 1, y + 1, z + 1});
                    hull[ci] = 0; // remove from hull mask
                }
            }

    // F_TwoPass on remaining hull voxels
    std::vector<uint8_t> work = hull;
    struct Rect { int x0, x1, z0, z1; };
    std::vector<std::vector<Rect>> slices(H);
    for (int y = 0; y < H; ++y) {
        auto& s = slices[y];
        for (int z = 0; z < D; ++z) {
            for (int x = 0; x < W; ++x) {
                if (work[idx3(x, y, z, W, D)] == 0) continue;
                int x0 = x;
                while (x < W && work[idx3(x, y, z, W, D)] != 0) ++x;
                int x1 = x;
                int z1 = z + 1;
                while (z1 < D) {
                    bool fullRow = true;
                    for (int xi = x0; xi < x1; ++xi)
                        if (work[idx3(xi, y, z1, W, D)] == 0) { fullRow = false; break; }
                    if (!fullRow) break;
                    for (int xi = x0; xi < x1; ++xi) work[idx3(xi, y, z1, W, D)] = 0;
                    ++z1;
                }
                for (int xi = x0; xi < x1; ++xi) work[idx3(xi, y, z, W, D)] = 0;
                s.push_back({x0, x1, z, z1});
            }
        }
    }

    std::vector<std::vector<bool>> visited(H);
    for (int y = 0; y < H; ++y) visited[y].assign(slices[y].size(), false);
    for (int y = 0; y < H; ++y) {
        for (size_t ri = 0; ri < slices[y].size(); ++ri) {
            if (visited[y][ri]) continue;
            const auto& r = slices[y][ri];
            int y0 = y, y1 = y + 1;
            while (y1 < H) {
                bool match = false;
                for (size_t rj = 0; rj < slices[y1].size(); ++rj) {
                    if (visited[y1][rj]) continue;
                    const auto& rn = slices[y1][rj];
                    if (rn.x0 == r.x0 && rn.x1 == r.x1 && rn.z0 == r.z0 && rn.z1 == r.z1) {
                        visited[y1][rj] = true; match = true; break;
                    }
                }
                if (!match) break;
                ++y1;
            }
            visited[y][ri] = true;
            out.push_back({r.x0, y0, r.z0, r.x1, y1, r.z1});
        }
    }
    return out;
}

// =============================================================================
// Strategy F: WHEEL-AWARE (cylinders for wheels, F_TwoPass for hull)
// =============================================================================
// Identical to E in assembly output (unit boxes for wheels, merged boxes for hull).
// The difference is in Jolt shape TYPE (CylinderShape vs BoxShape).
// For the CPU prototype, we track count of cylinder-eligible shapes as a metric.
std::vector<Box> strategy_F_wheel_aware(const VehicleDef& v) {
    // Same algorithm as E_Hybrid; output is identical boxes.
    // The cylinder-vs-box distinction is a Jolt runtime metric not measurable here.
    return strategy_E_hybrid(v);
}

// ---- Strategy table ----
constexpr std::array<Strategy, 6> kStrategies = {{
    {"A_NaivePerVoxel", &strategy_A_naive, 0},
    {"B_PrecomputedBP", &strategy_B_precomputed, 1},
    {"C_GreedyMerge", &strategy_C_greedy_merge, 0},
    {"D_Hierarchical", &strategy_D_hierarchical, 2},
    {"E_HybridTemplat", &strategy_E_hybrid, 0},
    {"F_WheelAware", &strategy_F_wheel_aware, 0},
}};

// ---- Vehicle registry ----
struct VehicleEntry {
    std::string_view name;
    VehicleDef (*gen)();
};

// Each vehicle generates with its canonical dimensions
VehicleDef gen_jeep_std() { return gen_jeep(8, 4, 4); }
VehicleDef gen_apc_std()  { return gen_apc(16, 8, 8); }
VehicleDef gen_tank_std() { return gen_tank(16, 12, 12); }
VehicleDef gen_truck_std() { return gen_truck(24, 8, 8); }
VehicleDef gen_ship_std() { return gen_large_ship(32, 16, 16); }

constexpr std::array<VehicleEntry, 5> kVehicles = {{
    {"Jeep_8x4x4", &gen_jeep_std},
    {"APC_16x8x8", &gen_apc_std},
    {"Tank_16x12", &gen_tank_std},
    {"Truck_24x8", &gen_truck_std},
    {"Ship_32x16", &gen_ship_std},
}};

const std::array<uint64_t, 5> kSeeds = {1, 7, 42, 1234, 31337};

// ---- Volume coverage check ----
// Returns fraction of solid voxels whose center is inside at least one output shape.
double coveragePct(const VehicleDef& v, const std::vector<Box>& boxes) {
    if (v.solidCount == 0) return 100.0;
    int covered = 0;
    int W = v.W, D = v.D;
    for (int y = 0; y < v.H; ++y) {
        for (int z = 0; z < v.D; ++z) {
            for (int x = 0; x < v.W; ++x) {
                int ci = idx3(x, y, z, W, D);
                if (!v.solid[ci]) continue;
                bool inside = false;
                for (const auto& b : boxes) {
                    if (x >= b.x0 && x < b.x1 && y >= b.y0 && y < b.y1 && z >= b.z0 && z < b.z1) {
                        inside = true; break;
                    }
                }
                if (inside) ++covered;
            }
        }
    }
    return 100.0 * static_cast<double>(covered) / static_cast<double>(v.solidCount);
}

// ---- Mutation: toggle 10% of solid voxels ----
// Returns NEW VehicleDef with mutated mask (does not modify original).
VehicleDef applyMutation(const VehicleDef& src, uint64_t seed) {
    VehicleDef out = src;
    int V = src.W * src.H * src.D;
    out.solid = src.solid;
    out.wheel = src.wheel;
    out.module = src.module;

    // Collect indices of currently solid voxels
    std::vector<int> solidIdx;
    solidIdx.reserve(src.solidCount);
    for (int i = 0; i < V; ++i)
        if (src.solid[i])
            solidIdx.push_back(i);

    int toToggle = std::max(1, static_cast<int>(solidIdx.size()) / 10);
    std::mt19937_64 rng(seed);
    std::shuffle(solidIdx.begin(), solidIdx.end(), rng);

    for (int t = 0; t < toToggle && t < static_cast<int>(solidIdx.size()); ++t) {
        int ci = solidIdx[t];
        // Remove voxel
        out.solid[ci] = 0;
        out.wheel[ci] = 0;
        out.module[ci] = 0;
    }

    // Also add some voxels in empty space
    std::vector<int> emptyIdx;
    emptyIdx.reserve(V - src.solidCount);
    for (int i = 0; i < V; ++i)
        if (!src.solid[i])
            emptyIdx.push_back(i);
    std::shuffle(emptyIdx.begin(), emptyIdx.end(), rng);
    int toAdd = std::min(toToggle, static_cast<int>(emptyIdx.size()));
    for (int t = 0; t < toAdd; ++t) {
        int ci = emptyIdx[t];
        out.solid[ci] = 1;
        // Assign to module nearest neighbor (hull by default)
        out.module[ci] = 0;
    }

    out.solidCount = 0;
    for (auto b : out.solid) if (b) ++out.solidCount;
    return out;
}

// ---- Mutation rebuild ----
// For strategy with rebuildMode=2 (sub-module), only rebuild affected modules.
// For rebuildMode=1 (component-level), rebuild affected components.
// For rebuildMode=0 (full rebuild), rebuild from scratch.
std::vector<Box> rebuildMutated(const VehicleDef& original, const VehicleDef& mutated,
                                const Strategy& strat) {
    // Full rebuild: just run the strategy on mutated data
    if (strat.rebuildMode == 0)
        return strat.fn(mutated);

    if (strat.rebuildMode == 1) {
        // Component-level rebuild: find which modules changed, rebuild only those
        // For simplicity, do full B_precomputed on mutated (which is O(modules) anyway)
        return strategy_B_precomputed(mutated);
    }

    if (strat.rebuildMode == 2) {
        // Sub-module: find changed modules, rebuild those only
        // Identify modules that changed: any module with at least one voxel toggle
        int V = original.W * original.H * original.D;
        std::vector<bool> changedModules(original.moduleCount, false);
        for (int i = 0; i < V; ++i) {
            if (original.solid[i] != mutated.solid[i]) {
                int m = original.module[i];
                if (m >= 0 && m < original.moduleCount)
                    changedModules[m] = true;
                // Also check mutated's module for new voxels
                m = mutated.module[i];
                if (m >= 0 && m < original.moduleCount)
                    changedModules[m] = true;
            }
        }

        // Rebuild changed modules via F_TwoPass, keep unchanged
        std::vector<Box> out;

        // Pre-compute which modules DON'T change — use their original shapes
        // For simplicity: rebuild original with D, but only for changed modules
        // Actually: simplest correct approach — full D on mutated data
        // (sub-module rebuild in real mainline would cache unchanged module compounds)
        return strategy_D_hierarchical(mutated);
    }

    return strat.fn(mutated);
}

// ---- Measurement ----
struct Measurement {
    std::string_view strategy;
    std::string_view vehicle;
    uint64_t seed;
    int solidCount;
    int shapeCount;
    double buildUs;
    double coveragePct;
    double mutationUs;
    double shapeReduction;
};

Measurement runOne(const Strategy& strat, const VehicleDef& v, uint64_t seed,
                   int iters, int warmup) {
    // Warmup
    for (int i = 0; i < warmup; ++i) {
        (void)strat.fn(v);
    }

    // Measurement: time each iter
    std::vector<double> perIterUs;
    perIterUs.reserve(iters);
    for (int i = 0; i < iters; ++i) {
        auto t0 = Clock::now();
        auto boxes = strat.fn(v);
        auto t1 = Clock::now();
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        perIterUs.push_back(us);
    }

    double meanUs = std::accumulate(perIterUs.begin(), perIterUs.end(), 0.0) / iters;

    // Final run for coverage + shape count
    auto finalBoxes = strat.fn(v);
    double cover = coveragePct(v, finalBoxes);
    int sc = static_cast<int>(finalBoxes.size());
    double reduction = (v.solidCount > 0)
        ? static_cast<double>(sc) / static_cast<double>(v.solidCount) : 0.0;

    // Mutation rebuild
    auto mutated = applyMutation(v, seed);
    auto t0 = Clock::now();
    auto mutBoxes = rebuildMutated(v, mutated, strat);
    auto t1 = Clock::now();
    double mutUs = std::chrono::duration<double, std::micro>(t1 - t0).count();

    return Measurement{
        strat.name, v.name, seed, v.solidCount, sc, meanUs, cover, mutUs, reduction
    };
}

// ---- CLI ----
struct Config {
    bool all = true;
    std::vector<std::string> strategyFilter;
    std::vector<std::string> vehicleFilter;
    int iters = 1000;
    int warmup = 10;
    std::string outputPath = "results.csv";
    bool healthCheck = false;
};

void printUsage() {
    std::fprintf(stderr,
        "Usage: vehicle_bench [options]\n"
        "  --all                  run all (default)\n"
        "  --strategy=NAME        filter (comma-sep)\n"
        "  --vehicle=NAME         filter (comma-sep)\n"
        "  --iters=N              iter per config (default 1000)\n"
        "  --warmup=N             warmup iter (default 10)\n"
        "  --output=PATH          CSV path (default results.csv)\n"
        "  --health               smoke test (A on Jeep seed=1)\n"
        "  --help                 this help\n");
}

Config parseArgs(int argc, char** argv) {
    Config c;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") { printUsage(); std::exit(0); }
        else if (arg == "--all") { c.all = true; }
        else if (arg == "--health") { c.healthCheck = true; }
        else if (arg.starts_with("--strategy=")) {
            c.all = false;             c.strategyFilter.clear();
            {
                std::string list = std::string(arg.substr(11));
                size_t pos = 0;
                while ((pos = list.find(',')) != std::string::npos) {
                    std::string tok = list.substr(0, pos);
                    c.strategyFilter.push_back(tok);
                    list.erase(0, pos + 1);
                }
                if (!list.empty()) c.strategyFilter.push_back(std::string(list));
            }
        } else if (arg.starts_with("--vehicle=")) {
            c.all = false; c.vehicleFilter.clear();
            std::string list = std::string(arg.substr(10));
            size_t pos = 0;
            while ((pos = list.find(',')) != std::string::npos) {
                c.vehicleFilter.push_back(list.substr(0, pos));
                list.erase(0, pos + 1);
            }
            if (!list.empty()) c.vehicleFilter.push_back(list);
        } else if (arg.starts_with("--iters=")) {
            c.iters = std::atoi(arg.substr(8).data());
        } else if (arg.starts_with("--warmup=")) {
            c.warmup = std::atoi(arg.substr(9).data());
        } else if (arg.starts_with("--output=")) {
            c.outputPath = std::string(arg.substr(9));
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", std::string(arg).c_str());
            printUsage(); std::exit(1);
        }
    }
    return c;
}

bool isFiltered(const std::vector<std::string>& filter, std::string_view name) {
    if (filter.empty()) return true;
    for (auto& f : filter) if (f == name) return true;
    return false;
}

} // namespace

int main(int argc, char** argv) {
    Config cfg = parseArgs(argc, argv);

    // Health check
    if (cfg.healthCheck) {
        auto v = gen_jeep_std();
        auto boxes = strategy_A_naive(v);
        double cov = coveragePct(v, boxes);
        std::fprintf(stderr, "[health] Jeep_8x4x4 solid=%d shapes=%zu coverage=%.2f%%\n",
                     v.solidCount, boxes.size(), cov);
        return (std::abs(cov - 100.0) < 0.01 && static_cast<int>(boxes.size()) == v.solidCount) ? 0 : 1;
    }

    std::FILE* out = std::fopen(cfg.outputPath.c_str(), "w");
    if (!out) { std::fprintf(stderr, "Cannot open: %s\n", cfg.outputPath.c_str()); return 1; }

    std::fprintf(out, "strategy,vehicle,seed,solid_count,shape_count,shape_reduction,build_us_mean,coverage_pct,mutation_rebuild_us\n");
    std::fprintf(stderr, "[bench] writing to %s, iters=%d, warmup=%d\n",
                 cfg.outputPath.c_str(), cfg.iters, cfg.warmup);

    size_t totalConfigs = 0;
    auto tStart = Clock::now();
    for (const auto& s : kStrategies) {
        if (!isFiltered(cfg.strategyFilter, s.name)) continue;
        for (const auto& ve : kVehicles) {
            if (!isFiltered(cfg.vehicleFilter, ve.name)) continue;
            for (uint64_t seed : kSeeds) {
                ++totalConfigs;
                auto vehicle = ve.gen();
                Measurement m = runOne(s, vehicle, seed, cfg.iters, cfg.warmup);
                std::fprintf(out, "%.*s,%.*s,%llu,%d,%d,%.6f,%.4f,%.4f,%.4f\n",
                    static_cast<int>(m.strategy.size()), m.strategy.data(),
                    static_cast<int>(m.vehicle.size()), m.vehicle.data(),
                    static_cast<unsigned long long>(m.seed),
                    m.solidCount, m.shapeCount, m.shapeReduction,
                    m.buildUs, m.coveragePct, m.mutationUs);
                if (totalConfigs % 15 == 0) std::fflush(out);
            }
        }
    }
    std::fflush(out);
    std::fclose(out);
    auto tEnd = Clock::now();
    double totalSec = std::chrono::duration<double>(tEnd - tStart).count();
    std::fprintf(stderr, "[bench] %zu configs × %d iter = %zu main meas done in %.2f s\n",
                 totalConfigs, cfg.iters, totalConfigs * static_cast<size_t>(cfg.iters), totalSec);
    return 0;
}
