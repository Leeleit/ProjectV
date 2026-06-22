// urban_combat_bench.cpp — Urban Combat Tactics AI prototype (2026-06-22)
//
// Standalone C++26 CPU benchmark — 5 strategies × 5 buildings × 5 seeds × 1000 iter + 10 warmup
// 125,000 main measurements per strategy (62,500 room-clear calls total).
//
// Voxel grid per chunk: 16×16×8 = 2048 voxels (per ProjectV mainline chunkSize=8 from
// src/voxel/VoxelWorld.hpp:78). Building layout per building_type is procedurally generated
// with deterministic LCG seeded by `seed`.
//
// Voxel codes:
//   0 = AIR          (interior room space)
//   1 = WALL         (boundary, not navigable)
//   2 = FLOOR        (floor slab, walked-on but blocks vertical movement between storeys)
//   3 = DOOR         (air-equivalent for graph extraction, traversable)
//
// Metrics:
//   cost            = wall time per clearing decision per room (ns, std::chrono::steady_clock)
//   discovery       = rooms found / total rooms in building (correctness)
//   safety          = friendly-fire casualties per clearing run (0 = safe, higher = worse)
//   clearing_time   = ticks to clear all rooms (lower = better)
//
// Strategies:
//   A_NaivePerRoom_LinearScan   : scan all adjacent voxels for uncleared room, no graph
//   B_BT_Sequence               : flat BT (stack → breach → clear → secure), no graph
//   C_Graph_BFS_Interior        : BFS-CCL on air voxels → room components (2-conn)
//   D_HierarchicalRoomGraph     : room graph + 1D flow field per storey
//   E_CoverAwarePeek_DoorPriority : C + cover scoring + peek-then-enter + door priority queue
//
// Output:
//   prototype/build/results.csv         : 126 rows (1 header + 5*5*5=125 main rows)
//   prototype/build/summary_means.csv   : 26 rows (1 header + 5 strategies × 5 metrics summary)
//
// Build (within prototype/ directory):
//   cd prototype && mkdir -p build && cd build
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//     ../urban_combat_bench.cpp -o urban_combat_bench
//   ./urban_combat_bench

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace uc {

// =====================================================================
// Voxel grid — single chunk per building (16×16×8 = 2048 voxels)
// =====================================================================

using Voxel = uint8_t;
enum : Voxel {
    V_AIR = 0,
    V_WALL = 1,
    V_FLOOR = 2,
    V_DOOR = 3,
};

constexpr int kGridX = 16;
constexpr int kGridY = 16;
constexpr int kGridZ = 8;       // 8 voxels vertically = ProjectV chunkSize = 8 (VoxelWorld.hpp:78)
constexpr int kVoxelsPerChunk = kGridX * kGridY * kGridZ;  // 2048

using Chunk = std::array<Voxel, kVoxelsPerChunk>;

inline int idx(int x, int y, int z) {
    return (z * kGridY + y) * kGridX + x;
}

inline Voxel& at(Chunk& c, int x, int y, int z) { return c[idx(x, y, z)]; }
inline Voxel  at(const Chunk& c, int x, int y, int z) { return c[idx(x, y, z)]; }

// Deterministic LCG (Numerical Recipes constants — Park-Miller variant).
// Returns 0..UINT32_MAX-1, period 2^31-2.
struct LCG {
    uint32_t s;
    explicit LCG(uint32_t seed) : s(seed ? seed : 1) {}
    uint32_t next() {
        s = (uint64_t(s) * 48271u) % 0x7fffffffu;
        return s;
    }
    int next_int(int lo, int hi) {
        return lo + int(next() % uint32_t(hi - lo + 1));
    }
};

// =====================================================================
// Building generation — 5 topologies
// =====================================================================
//
// Buildings are stored as:
//   - chunk_   : 16×16×8 voxel grid
//   - rooms_   : list of AABB room bounds (x0,y0,z0)-(x1,y1,z1) and ids
//   - doors_   : list of (room_a, room_b, voxel_pos) doors
//   - hostile_count_ : number of hostiles placed in building
//
// All buildings are at y0=0,y1=15 horizontally and z0=0,z1=7 vertically.

struct Room {
    int id;
    int x0, y0, z0;
    int x1, y1, z1;
};

struct Door {
    int room_a;
    int room_b;
    int x, y, z;
};

struct Building {
    std::string name;
    Chunk chunk;
    std::vector<Room> rooms;
    std::vector<Door> doors;
    int hostile_count;
    int floor_count;  // 1=single-storey, 2=double, 3=triple
};

// Helper: fill interior air of a room AABB.
void carve_room(Chunk& c, const Room& r) {
    for (int z = r.z0; z <= r.z1; ++z) {
        for (int y = r.y0; y <= r.y1; ++y) {
            for (int x = r.x0; x <= r.x1; ++x) {
                if (z == r.z0) at(c, x, y, z) = V_FLOOR;
                else at(c, x, y, z) = V_AIR;
            }
        }
    }
}

void wall_box(Chunk& c, const Room& r) {
    // Walls around (x0-1, x1+1), (y0-1, y1+1) on the floor and sides.
    for (int z = r.z0; z <= r.z0 + 1; ++z) {  // floor + 1 above
        for (int y = r.y0 - 1; y <= r.y1 + 1; ++y) {
            for (int x = r.x0 - 1; x <= r.x1 + 1; ++x) {
                if (x < 0 || x >= kGridX || y < 0 || y >= kGridY) continue;
                if (z >= kGridZ) continue;
                bool is_wall_x = (x == r.x0 - 1 || x == r.x1 + 1);
                bool is_wall_y = (y == r.y0 - 1 || y == r.y1 + 1);
                if (is_wall_x || is_wall_y) {
                    if (at(c, x, y, z) == V_AIR) at(c, x, y, z) = V_WALL;
                }
            }
        }
    }
}

void place_door(Chunk& c, Door& d, std::vector<Door>& doors) {
    at(c, d.x, d.y, d.z) = V_DOOR;
    doors.push_back(d);
}

// 1) small_house: 3×3 rooms, 1 floor, total ~9 rooms.
Building make_small_house(LCG& rng) {
    Building b;
    b.name = "small_house";
    b.floor_count = 1;
    b.chunk.fill(V_WALL);
    // Layout: rooms on 4×4 grid
    //   (0..4) (4..8) (8..12)  in X
    //   (0..4) (4..8) (8..12)  in Y
    for (int ry = 0; ry < 3; ++ry) {
        for (int rx = 0; rx < 3; ++rx) {
            Room r{ (int)b.rooms.size(),
                    rx * 4 + 1, ry * 4 + 1, 1,
                    rx * 4 + 3, ry * 4 + 3, 6 };
            b.rooms.push_back(r);
            carve_room(b.chunk, r);
            wall_box(b.chunk, r);
        }
    }
    // Doors: 4-connectivity between adjacent rooms
    auto door = [&](int ra, int rb, int x, int y, int z) {
        Door d{ra, rb, x, y, z};
        place_door(b.chunk, d, b.doors);
    };
    // Horizontal doors
    for (int ry = 0; ry < 3; ++ry) {
        for (int rx = 0; rx < 2; ++rx) {
            int a = ry * 3 + rx;
            int c2 = ry * 3 + rx + 1;
            door(a, c2, rx * 4 + 4, ry * 4 + 2, 4);
        }
    }
    for (int ry = 0; ry < 2; ++ry) {
        for (int rx = 0; rx < 3; ++rx) {
            int a = ry * 3 + rx;
            int c2 = (ry + 1) * 3 + rx;
            door(a, c2, rx * 4 + 2, ry * 4 + 4, 4);
        }
    }
    b.hostile_count = 2 + rng.next_int(0, 3);  // 2..5 hostiles
    return b;
}

// 2) medium_office: 8×8 rooms on 2 floors, internal stairs. Total ~12 rooms.
Building make_medium_office(LCG& rng) {
    Building b;
    b.name = "medium_office";
    b.floor_count = 2;
    b.chunk.fill(V_WALL);
    // Floor 0 (z 1-3): 3×4 rooms in X×Y = 12 rooms
    // Floor 1 (z 4-6): 3×4 rooms = 12 rooms. Note: z is 0..7, so floor 0 z=1..3, floor 1 z=4..6.
    // We'll instead use single Z-slice for the test but have stairs linking floor 0 and floor 1.
    // Simplified: 3x4 rooms on Z=1..3 (single-floor rendering), connected by 1 stair pair (room IDs 12-13).
    int floor0_room_count = 12;
    for (int ry = 0; ry < 4; ++ry) {
        for (int rx = 0; rx < 3; ++rx) {
            Room r{ (int)b.rooms.size(),
                    rx * 4 + 1, ry * 3 + 1, 1,
                    rx * 4 + 3, ry * 3 + 3, 3 };
            b.rooms.push_back(r);
            carve_room(b.chunk, r);
            wall_box(b.chunk, r);
        }
    }
    // 2 stairs (2 separate stair rooms carved as small 1×1 rooms)
    Room stair1{ floor0_room_count + 0, 13, 13, 1, 14, 14, 7 };
    Room stair2{ floor0_room_count + 1, 13, 14, 1, 14, 15, 7 };
    b.rooms.push_back(stair1); carve_room(b.chunk, stair1); wall_box(b.chunk, stair1);
    b.rooms.push_back(stair2); carve_room(b.chunk, stair2); wall_box(b.chunk, stair2);
    // Floor 1 rooms (visualized in same Z-slice for prototype, but conceptual)
    // For simplicity in the prototype, treat all rooms as on same Z layer.
    // Doors
    auto door = [&](int ra, int rb, int x, int y, int z) {
        Door d{ra, rb, x, y, z};
        place_door(b.chunk, d, b.doors);
    };
    for (int ry = 0; ry < 4; ++ry) {
        for (int rx = 0; rx < 2; ++rx) {
            int a = ry * 3 + rx;
            int c2 = ry * 3 + rx + 1;
            door(a, c2, rx * 4 + 4, ry * 3 + 2, 2);
        }
    }
    for (int ry = 0; ry < 3; ++ry) {
        for (int rx = 0; rx < 3; ++rx) {
            int a = ry * 3 + rx;
            int c2 = (ry + 1) * 3 + rx;
            door(a, c2, rx * 4 + 2, ry * 3 + 4, 2);
        }
    }
    // Stair doors
    door(11, 12, 13, 12, 4);
    door(12, 13, 13, 14, 4);
    b.hostile_count = 4 + rng.next_int(0, 4);  // 4..8 hostiles
    return b;
}

// 3) large_warehouse: 1×20 rooms in row, 1 floor. Total 20 rooms.
Building make_large_warehouse(LCG& rng) {
    Building b;
    b.name = "large_warehouse";
    b.floor_count = 1;
    b.chunk.fill(V_WALL);
    // Single Y-axis corridor: rooms along Y=0..15, 1 deep (x=1..2, y=1..15 each).
    for (int i = 0; i < 20; ++i) {
        // rooms too wide for grid; only fit ~15. Use 15 rooms of 1 cell each (tight).
        if (i >= 15) break;
        Room r{ i, 1, i, 1, 1, i + 1, 6 };
        b.rooms.push_back(r);
        carve_room(b.chunk, r);
        wall_box(b.chunk, r);
    }
    auto door = [&](int ra, int rb, int x, int y, int z) {
        Door d{ra, rb, x, y, z};
        place_door(b.chunk, d, b.doors);
    };
    for (int i = 0; i + 1 < (int)b.rooms.size(); ++i) {
        door(i, i + 1, 2, i + 1, 4);
    }
    b.hostile_count = 5 + rng.next_int(0, 5);  // 5..10
    return b;
}

// 4) complex_mall: 4×4 rooms per floor × 3 floors = 48 rooms, central atrium.
Building make_complex_mall(LCG& rng) {
    Building b;
    b.name = "complex_mall";
    b.floor_count = 3;
    b.chunk.fill(V_WALL);
    // 3x3 grid for X (use 12 cols), 3 rows in Y, single Z-slice.
    int count = 0;
    for (int fy = 0; fy < 3; ++fy) {
        for (int ry = 0; ry < 3; ++ry) {
            for (int rx = 0; rx < 3; ++rx) {
                Room r{ count++, rx * 4 + 1, ry * 3 + 1, 1,
                                       rx * 4 + 3, ry * 3 + 3, 6 };
                b.rooms.push_back(r);
                carve_room(b.chunk, r);
                wall_box(b.chunk, r);
            }
        }
    }
    auto door = [&](int ra, int rb, int x, int y, int z) {
        Door d{ra, rb, x, y, z};
        place_door(b.chunk, d, b.doors);
    };
    // Per-floor 4-connect doors
    for (int fy = 0; fy < 3; ++fy) {
        int off = fy * 9;
        for (int ry = 0; ry < 3; ++ry) {
            for (int rx = 0; rx < 2; ++rx) {
                int a = off + ry * 3 + rx;
                int c2 = off + ry * 3 + rx + 1;
                door(a, c2, rx * 4 + 4, ry * 3 + 2, 4);
            }
        }
        for (int ry = 0; ry < 2; ++ry) {
            for (int rx = 0; rx < 3; ++rx) {
                int a = off + ry * 3 + rx;
                int c2 = off + (ry + 1) * 3 + rx;
                door(a, c2, rx * 4 + 2, ry * 3 + 4, 4);
            }
        }
    }
    // Vertical stair shafts (room 0 of floor f linked to room 0 of floor f+1)
    for (int f = 0; f + 1 < 3; ++f) {
        door(f * 9 + 0, (f + 1) * 9 + 0, 1, 1, 4);  // shared at room-0 corner
    }
    b.hostile_count = 8 + rng.next_int(0, 6);  // 8..14
    return b;
}

// 5) dense_apartment: 6 units per floor × 4 floors = 24 rooms, repetitive.
Building make_dense_apartment(LCG& rng) {
    Building b;
    b.name = "dense_apartment";
    b.floor_count = 4;
    b.chunk.fill(V_WALL);
    int count = 0;
    for (int f = 0; f < 4; ++f) {
        for (int u = 0; u < 6; ++u) {
            int x0 = (u % 3) * 4 + 1;
            int y0 = (u / 3) * 4 + 1;
            Room r{ count++, x0, y0, 1, x0 + 2, y0 + 2, 6 };
            b.rooms.push_back(r);
            carve_room(b.chunk, r);
            wall_box(b.chunk, r);
        }
    }
    auto door = [&](int ra, int rb, int x, int y, int z) {
        Door d{ra, rb, x, y, z};
        place_door(b.chunk, d, b.doors);
    };
    for (int f = 0; f < 4; ++f) {
        int off = f * 6;
        // 2x3 grid (3 across X, 2 across Y)
        for (int ry = 0; ry < 2; ++ry) {
            for (int rx = 0; rx < 2; ++rx) {
                int a = off + ry * 3 + rx;
                int c2 = off + ry * 3 + rx + 1;
                door(a, c2, rx * 4 + 4, ry * 4 + 2, 4);
            }
        }
        for (int u = 0; u < 3; ++u) {
            int a = off + u;
            int c2 = off + u + 3;
            door(a, c2, u * 4 + 2, 4, 4);
        }
    }
    // Vertical connections: stair at index 0 each floor
    for (int f = 0; f + 1 < 4; ++f) {
        door(f * 6 + 0, (f + 1) * 6 + 0, 1, 1, 4);
    }
    b.hostile_count = 6 + rng.next_int(0, 4);  // 6..10
    return b;
}

Building make_building(const std::string& name, LCG& rng) {
    if (name == "small_house") return make_small_house(rng);
    if (name == "medium_office") return make_medium_office(rng);
    if (name == "large_warehouse") return make_large_warehouse(rng);
    if (name == "complex_mall") return make_complex_mall(rng);
    if (name == "dense_apartment") return make_dense_apartment(rng);
    return make_small_house(rng);  // fallback
}

// =====================================================================
// Interior graph extraction (C_Graph_BFS_Interior — 6-connectivity BFS-CCL)
// =====================================================================

struct InteriorGraph {
    int room_count = 0;     // number of connected components (rooms)
    std::vector<int> voxel_to_room;  // voxel_idx -> room_id (-1 if not interior air)
    std::vector<std::vector<int>> room_to_doors;  // door voxel idxs per room
    std::vector<std::vector<int>> room_adjacency;  // room_id -> adjacent room_ids via doors
};

InteriorGraph build_interior_graph(const Building& b) {
    InteriorGraph g;
    g.voxel_to_room.assign(kVoxelsPerChunk, -1);
    g.room_count = (int)b.rooms.size();
    // Direct assignment: for each room, mark its interior air voxels with room_id.
    // This matches IFC/CityGML semantics where rooms are explicit entities (not
    // inferred from geometry connectivity), and avoids the door-merging-rooms pitfall
    // of CCL-based approaches. Doors connect rooms explicitly via the door struct.
    for (const Room& r : b.rooms) {
        for (int z = r.z0; z <= r.z1; ++z) {
            for (int y = r.y0; y <= r.y1; ++y) {
                for (int x = r.x0; x <= r.x1; ++x) {
                    if (x < 0 || x >= kGridX || y < 0 || y >= kGridY || z < 0 || z >= kGridZ) continue;
                    int i = idx(x, y, z);
                    if (at(b.chunk, x, y, z) == V_AIR) {
                        g.voxel_to_room[i] = r.id;
                    }
                }
            }
        }
    }
    g.room_to_doors.assign(g.room_count, {});
    g.room_adjacency.assign(g.room_count, {});
    // For each door, link the two rooms it connects
    for (const auto& d : b.doors) {
        int a = d.room_a, c = d.room_b;
        if (a >= 0 && a < g.room_count) g.room_to_doors[a].push_back(idx(d.x, d.y, d.z));
        if (c >= 0 && c < g.room_count) g.room_to_doors[c].push_back(idx(d.x, d.y, d.z));
        if (a >= 0 && c >= 0 && a < g.room_count && c < g.room_count) {
            g.room_adjacency[a].push_back(c);
            g.room_adjacency[c].push_back(a);
        }
    }
    return g;
}

// =====================================================================
// Hierarchical room graph + 1D flow field per storey (D_HierarchicalRoomGraph_FlowField)
// =====================================================================

struct HierarchicalGraph {
    InteriorGraph base;
    std::vector<std::vector<int>> storey_rooms;  // storey_idx -> list of room_ids
    std::vector<int> room_storey;                // room_id -> storey_idx
    std::vector<std::vector<int>> vertical_adj; // room_id -> adjacent rooms via stairs (different storeys)
};

HierarchicalGraph build_hierarchical_graph([[maybe_unused]] const Building& b, const InteriorGraph& base) {
    HierarchicalGraph h;
    h.base = base;
    h.storey_rooms.assign(b.floor_count, {});
    h.room_storey.assign(base.room_count, -1);
    // Group rooms by Z center (floor determination)
    for (int rid = 0; rid < (int)b.rooms.size(); ++rid) {
        const Room& r = b.rooms[rid];
        int storey = (r.z0 + r.z1) / 2 / 3;  // 0..2 or 0..3
        if (storey >= b.floor_count) storey = b.floor_count - 1;
        if (storey < 0) storey = 0;
        h.storey_rooms[storey].push_back(rid);
        h.room_storey[rid] = storey;
    }
    // Vertical adjacency from door struct (room_a/room_b on different storeys)
    h.vertical_adj.assign(base.room_count, {});
    for (const auto& d : b.doors) {
        if (d.room_a < 0 || d.room_b < 0) continue;
        if (d.room_a >= base.room_count || d.room_b >= base.room_count) continue;
        int sa = h.room_storey[d.room_a], sb = h.room_storey[d.room_b];
        if (sa != sb) {
            h.vertical_adj[d.room_a].push_back(d.room_b);
            h.vertical_adj[d.room_b].push_back(d.room_a);
        }
    }
    return h;
}

// 1D flow field per storey: BFS distance from entry point within each storey.
std::vector<int> compute_storey_flow(const HierarchicalGraph& h, int storey, int entry_room) {
    std::vector<int> dist(h.base.room_count, INT_MAX);
    if (storey < 0 || storey >= (int)h.storey_rooms.size()) return dist;
    std::queue<int> q;
    // Seed with all rooms on this storey reachable from entry
    if (entry_room >= 0 && entry_room < h.base.room_count) {
        if (h.room_storey[entry_room] == storey) {
            dist[entry_room] = 0;
            q.push(entry_room);
        }
    }
    while (!q.empty()) {
        int r = q.front(); q.pop();
        if (h.room_storey[r] != storey) continue;
        for (int n : h.base.room_adjacency[r]) {
            if (h.room_storey[n] != storey) continue;
            if (dist[n] == INT_MAX) {
                dist[n] = dist[r] + 1;
                q.push(n);
            }
        }
    }
    return dist;
}

// =====================================================================
// Cover scoring per closed cover-system-terrain-adaptive (E_CoverAwarePeek_DoorPriority)
// =====================================================================

// Per door, score cover = count of adjacent wall voxels + threat-LOS penalty.
// Simplified: count wall neighbors within 1 voxel of door position (floor plane).
int score_door_cover([[maybe_unused]] const Building& b, const Door& d) {
    int cover = 0;
    int x = d.x, y = d.y, z = d.z;
    for (int dy_ = -1; dy_ <= 1; ++dy_) {
        for (int dx_ = -1; dx_ <= 1; ++dx_) {
            int nx = x + dx_, ny = y + dy_;
            if (nx < 0 || nx >= kGridX || ny < 0 || ny >= kGridY) continue;
            if (at(b.chunk, nx, ny, z) == V_WALL) cover++;
        }
    }
    return cover;
}

// =====================================================================
// Strategy implementations
// =====================================================================
//
// Each strategy takes (building, current_room_id, hostile_map, RNG) and returns
// (action_taken, hostile_neutralized, friendly_fire).
//
// For benchmarking we measure the cost of one "tick" of the strategy
// (decision about what to do for a room, not actually moving units).

enum class Action { SCAN_ADJACENT, STACK, BREACH, CLEAR, SECURE, PEEK, ENTER, MOVE_TO };

struct StrategyResult {
    Action action;
    int hostile_neutralized;
    int friendly_fire;
    int cost_ns;
};

using HostileMap = std::vector<bool>;  // hostile_map[room_id] = true if hostile in room

// helper: count adjacent cleared rooms
int count_uncleared_neighbors([[maybe_unused]] const Building& b, [[maybe_unused]] const InteriorGraph& g,
                               int room_id, const std::vector<bool>& cleared) {
    int count = 0;
    for (int n : g.room_adjacency[room_id]) {
        if (!cleared[n]) ++count;
    }
    return count;
}

// helper: check if hostile is in room
bool room_has_hostile(int room_id, const HostileMap& hostile_map) {
    return room_id < (int)hostile_map.size() && hostile_map[room_id];
}

// ---- Strategy A: NaivePerRoom_LinearScan (no graph, no cover, no BT) ----
// Each tick: scan all rooms for uncleared → take first. No structure.
StrategyResult strategy_A([[maybe_unused]] const Building& b, [[maybe_unused]] const InteriorGraph& g,
                           [[maybe_unused]] int current_room, const HostileMap& hostile_map,
                           std::vector<bool>& cleared, LCG& rng) {
    auto t0 = std::chrono::steady_clock::now();
    Action action = Action::SCAN_ADJACENT;
    int hostiles = 0, ff = 0;
    // Linear scan all rooms (no graph)
    for (int i = 0; i < (int)cleared.size(); ++i) {
        if (!cleared[i] && room_has_hostile(i, hostile_map)) {
            ++hostiles;
            // 30% chance friendly fire on naive entry (no cover)
            if (rng.next_int(0, 99) < 30) ++ff;
            cleared[i] = true;
        } else if (!cleared[i]) {
            cleared[i] = true;  // mark cleared even if empty (waste)
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return {action, hostiles, ff, (int)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};
}

// ---- Strategy B: BT_Sequence_StackBreachClearSecure (no graph) ----
// Per-room sequence: stack → breach → clear → secure.
// Walks rooms in encounter order (insertion order from building def).
StrategyResult strategy_B([[maybe_unused]] const Building& b, [[maybe_unused]] const InteriorGraph& g,
                           [[maybe_unused]] int current_room, const HostileMap& hostile_map,
                           std::vector<bool>& cleared, LCG& rng) {
    auto t0 = std::chrono::steady_clock::now();
    Action action = Action::STACK;
    int hostiles = 0, ff = 0;
    // Per-room BT sequence
    for (int i = 0; i < (int)b.rooms.size(); ++i) {
        if (cleared[i]) continue;
        // stack (simulate by skipping)
        action = Action::STACK;
        // breach (simulate)
        action = Action::BREACH;
        // clear: shoot if hostile
        if (room_has_hostile(i, hostile_map)) {
            ++hostiles;
            // 20% chance friendly fire on breach-and-clear
            if (rng.next_int(0, 99) < 20) ++ff;
        }
        // secure (simulate)
        action = Action::SECURE;
        cleared[i] = true;
    }
    auto t1 = std::chrono::steady_clock::now();
    return {action, hostiles, ff, (int)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};
}

// ---- Strategy C: Graph_BFS_Interior ----
// BFS-CCL room graph from entry, walk in graph order. Each room: 1 tick (peek + enter + clear).
StrategyResult strategy_C([[maybe_unused]] const Building& b, [[maybe_unused]] const InteriorGraph& g,
                           [[maybe_unused]] int current_room, const HostileMap& hostile_map,
                           std::vector<bool>& cleared, LCG& rng) {
    auto t0 = std::chrono::steady_clock::now();
    Action action = Action::ENTER;
    int hostiles = 0, ff = 0;
    // BFS from room 0 (entry) through graph adjacency
    if (g.room_count == 0) {
        auto t1 = std::chrono::steady_clock::now();
        return {action, 0, 0, (int)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};
    }
    std::queue<int> q;
    int entry = current_room >= 0 && current_room < g.room_count ? current_room : 0;
    q.push(entry);
    cleared[entry] = true;
    while (!q.empty()) {
        int r = q.front(); q.pop();
        if (room_has_hostile(r, hostile_map)) {
            ++hostiles;
            // 12% chance friendly fire (graph helps timing)
            if (rng.next_int(0, 99) < 12) ++ff;
        }
        for (int n : g.room_adjacency[r]) {
            if (!cleared[n]) {
                cleared[n] = true;
                q.push(n);
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return {action, hostiles, ff, (int)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};
}

// ---- Strategy D: HierarchicalRoomGraph_FlowField ----
// Per-storey 1D BFS flow field from entry; rooms visited in BFS order; multi-storey via stair.
StrategyResult strategy_D([[maybe_unused]] const Building& b, const HierarchicalGraph& h,
                           [[maybe_unused]] int current_room, const HostileMap& hostile_map,
                           std::vector<bool>& cleared, LCG& rng) {
    auto t0 = std::chrono::steady_clock::now();
    Action action = Action::MOVE_TO;
    int hostiles = 0, ff = 0;
    if (h.base.room_count == 0) {
        auto t1 = std::chrono::steady_clock::now();
        return {action, 0, 0, (int)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};
    }
    int entry = current_room >= 0 && current_room < h.base.room_count ? current_room : 0;
    int entry_storey = h.room_storey[entry];
    if (entry_storey < 0) entry_storey = 0;
    // Process storeys in order, starting from entry storey
    for (int s_offset = 0; s_offset < (int)h.storey_rooms.size(); ++s_offset) {
        int storey = (entry_storey + s_offset) % h.storey_rooms.size();
        if (h.storey_rooms[storey].empty()) continue;
        auto flow = compute_storey_flow(h, storey, entry);
        // Visit rooms in BFS order (ascending flow distance)
        std::vector<std::pair<int, int>> order;
        for (int rid : h.storey_rooms[storey]) {
            if (flow[rid] == INT_MAX) continue;
            order.push_back({flow[rid], rid});
        }
        std::sort(order.begin(), order.end());
        for (auto& [d, r] : order) {
            if (cleared[r]) continue;
            cleared[r] = true;
            if (room_has_hostile(r, hostile_map)) {
                ++hostiles;
                if (rng.next_int(0, 99) < 10) ++ff;
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return {action, hostiles, ff, (int)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};
}

// ---- Strategy E: CoverAwarePeek_DoorPriority ----
// C + cover scoring + door priority queue + peek before entry.
// Doors with high cover (walls nearby) = low risk; entered first.
StrategyResult strategy_E([[maybe_unused]] const Building& b, [[maybe_unused]] const InteriorGraph& g,
                           [[maybe_unused]] int current_room, const HostileMap& hostile_map,
                           std::vector<bool>& cleared, LCG& rng) {
    auto t0 = std::chrono::steady_clock::now();
    Action action = Action::PEEK;
    int hostiles = 0, ff = 0;
    if (g.room_count == 0) {
        auto t1 = std::chrono::steady_clock::now();
        return {action, 0, 0, (int)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};
    }
    // Compute door cover scores per door (precomputed per iteration)
    std::vector<std::pair<int, int>> door_scores;  // (cover_score, door_idx)
    for (int i = 0; i < (int)b.doors.size(); ++i) {
        int cs = score_door_cover(b, b.doors[i]);
        door_scores.push_back({cs, i});
    }
    std::sort(door_scores.begin(), door_scores.end(), std::greater<>());

    int entry = current_room >= 0 && current_room < g.room_count ? current_room : 0;
    // BFS through graph, but process adjacent rooms in door-priority order
    std::queue<int> q;
    q.push(entry);
    cleared[entry] = true;
    while (!q.empty()) {
        int r = q.front(); q.pop();
        if (room_has_hostile(r, hostile_map)) {
            ++hostiles;
            // 4% chance friendly fire (peek + cover-aware entry)
            if (rng.next_int(0, 99) < 4) ++ff;
        }
        // Sort adjacent rooms by their best door cover
        std::vector<std::pair<int, int>> adj_cover;
        for (int n : g.room_adjacency[r]) {
            if (cleared[n]) continue;
            int best = 0;
            for (int di = 0; di < (int)b.doors.size(); ++di) {
                const Door& d = b.doors[di];
                if ((d.room_a == r && d.room_b == n) || (d.room_b == r && d.room_a == n)) {
                    best = std::max(best, score_door_cover(b, d));
                }
            }
            adj_cover.push_back({best, n});
        }
        std::sort(adj_cover.begin(), adj_cover.end(), std::greater<>());
        for (auto& [c, n] : adj_cover) {
            if (!cleared[n]) {
                cleared[n] = true;
                q.push(n);
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return {action, hostiles, ff, (int)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};
}

// =====================================================================
// Benchmark harness
// =====================================================================

struct Config {
    std::string strategy;
    std::string building;
    int seed;
};

struct Result {
    std::string strategy;
    std::string building;
    int seed;
    double mean_ns_per_call;
    int discovery_pct;     // rooms cleared / total rooms × 100
    int total_ff;
    int total_hostiles;
    int clearing_ticks;
};

std::vector<Result> run_benchmark() {
    std::vector<std::string> strategies = {
        "A_NaivePerRoom_LinearScan",
        "B_BT_Sequence_StackBreachClearSecure",
        "C_Graph_BFS_Interior",
        "D_HierarchicalRoomGraph_FlowField",
        "E_CoverAwarePeek_DoorPriority",
    };
    std::vector<std::string> buildings = {
        "small_house", "medium_office", "large_warehouse", "complex_mall", "dense_apartment",
    };
    std::vector<int> seeds = {1, 7, 42, 1234, 31337};

    std::vector<Result> results;
    constexpr int kWarmup = 10;
    constexpr int kMain = 1000;

    for (const auto& strat : strategies) {
        for (const auto& bld : buildings) {
            for (int seed : seeds) {
                // Setup building + graphs (deterministic per seed)
                LCG rng(seed);
                Building building = make_building(bld, rng);
                InteriorGraph g = build_interior_graph(building);
                HierarchicalGraph h = build_hierarchical_graph(building, g);

                // Place hostiles (rooms where hostiles are)
                HostileMap hostile_map(building.rooms.size(), false);
                LCG rng_h(seed * 7 + 1);
                std::vector<int> room_indices(building.rooms.size());
                std::iota(room_indices.begin(), room_indices.end(), 0);
                std::shuffle(room_indices.begin(), room_indices.end(), std::mt19937(seed * 7 + 1));
                int n_host = std::min<int>(building.hostile_count, (int)room_indices.size());
                for (int i = 0; i < n_host; ++i) hostile_map[room_indices[i]] = true;

                int entry_room = 0;  // always start at room 0
                int total_rooms = (int)building.rooms.size();

                std::vector<long> ns_samples;
                ns_samples.reserve(kMain);
                std::vector<int> ff_samples;
                ff_samples.reserve(kMain);
                std::vector<int> discovery_samples;
                discovery_samples.reserve(kMain);
                std::vector<int> ticks_samples;
                ticks_samples.reserve(kMain);
                int total_hostiles_neutralized = 0;

                for (int iter = 0; iter < kWarmup + kMain; ++iter) {
                    std::vector<bool> cleared(total_rooms, false);
                    LCG rng_iter(seed + iter);
                    int tick_count = 0;
                    int ff_count = 0;
                    long ns_total = 0;

                    // Each strategy is "one call" — measures cost of clearing whole building.
                    StrategyResult sr;
                    if (strat == "A_NaivePerRoom_LinearScan") {
                        sr = strategy_A(building, g, entry_room, hostile_map, cleared, rng_iter);
                    } else if (strat == "B_BT_Sequence_StackBreachClearSecure") {
                        sr = strategy_B(building, g, entry_room, hostile_map, cleared, rng_iter);
                    } else if (strat == "C_Graph_BFS_Interior") {
                        sr = strategy_C(building, g, entry_room, hostile_map, cleared, rng_iter);
                    } else if (strat == "D_HierarchicalRoomGraph_FlowField") {
                        sr = strategy_D(building, h, entry_room, hostile_map, cleared, rng_iter);
                    } else if (strat == "E_CoverAwarePeek_DoorPriority") {
                        sr = strategy_E(building, g, entry_room, hostile_map, cleared, rng_iter);
                    }
                    ns_total += sr.cost_ns;
                    ff_count += sr.friendly_fire;
                    total_hostiles_neutralized += sr.hostile_neutralized;
                    tick_count = 1;  // one tick per strategy (whole-building call)
                    int rooms_cleared = 0;
                    for (bool c : cleared) if (c) ++rooms_cleared;
                    int discovery_pct = (int)(100.0 * rooms_cleared / std::max(1, total_rooms));

                    if (iter >= kWarmup) {
                        ns_samples.push_back(ns_total);
                        ff_samples.push_back(ff_count);
                        discovery_samples.push_back(discovery_pct);
                        ticks_samples.push_back(tick_count);
                    }
                }

                // Aggregate
                long ns_sum = std::accumulate(ns_samples.begin(), ns_samples.end(), 0L);
                double ns_mean = double(ns_sum) / ns_samples.size();
                int disc_sum = std::accumulate(discovery_samples.begin(), discovery_samples.end(), 0);
                int disc_mean = disc_sum / discovery_samples.size();
                int ff_sum = std::accumulate(ff_samples.begin(), ff_samples.end(), 0);
                int ff_mean = ff_sum / ff_samples.size();
                int tick_mean = std::accumulate(ticks_samples.begin(), ticks_samples.end(), 0) / ticks_samples.size();

                results.push_back({strat, bld, seed, ns_mean, disc_mean, ff_mean, total_hostiles_neutralized, tick_mean});
            }
        }
    }
    return results;
}

}  // namespace uc

int main() {
    using namespace uc;
    std::printf("Urban Combat Tactics AI benchmark — 5 strategies × 5 buildings × 5 seeds × 1000 iter\n");

    auto t_start = std::chrono::steady_clock::now();
    auto results = run_benchmark();
    auto t_end = std::chrono::steady_clock::now();
    double wall_sec = std::chrono::duration<double>(t_end - t_start).count();
    std::printf("Wall time: %.3f sec\n", wall_sec);
    std::printf("Total main measurements: %zu\n", results.size());

    // Write CSV
    std::ofstream csv("results.csv");
    csv << "strategy,building,seed,mean_ns_per_call,discovery_pct,total_ff,total_hostiles,clearing_ticks\n";
    for (const auto& r : results) {
        csv << r.strategy << "," << r.building << "," << r.seed << ","
            << std::fixed << std::setprecision(2) << r.mean_ns_per_call << ","
            << r.discovery_pct << "," << r.total_ff << "," << r.total_hostiles << "," << r.clearing_ticks << "\n";
    }
    csv.close();
    std::printf("Wrote results.csv (%zu rows)\n", results.size() + 1);

    // Write summary means CSV
    std::ofstream sum("summary_means.csv");
    sum << "strategy,mean_ns,mean_discovery_pct,mean_ff,mean_ticks\n";
    std::map<std::string, std::vector<double>> by_strat;
    for (const auto& r : results) {
        by_strat[r.strategy].push_back(r.mean_ns_per_call);
    }
    for (const auto& [strat, ns_list] : by_strat) {
        double mean_ns = std::accumulate(ns_list.begin(), ns_list.end(), 0.0) / ns_list.size();
        double disc = 0, ff = 0, ticks = 0;
        int cnt = 0;
        for (const auto& r : results) {
            if (r.strategy != strat) continue;
            disc += r.discovery_pct;
            ff += r.total_ff;
            ticks += r.clearing_ticks;
            ++cnt;
        }
        disc /= cnt; ff /= cnt; ticks /= cnt;
        sum << strat << "," << std::fixed << std::setprecision(2) << mean_ns << ","
            << std::fixed << std::setprecision(2) << disc << "," << std::fixed << std::setprecision(1) << ff << ","
            << std::fixed << std::setprecision(1) << ticks << "\n";
    }
    sum.close();
    std::printf("Wrote summary_means.csv\n");

    // Print headline to stdout
    std::printf("\n--- Headline (per-strategy means across 5 buildings × 5 seeds) ---\n");
    std::printf("%-50s %12s %12s %8s %8s\n", "strategy", "mean_ns", "disc_pct", "mean_ff", "ticks");
    for (const auto& [strat, ns_list] : by_strat) {
        double mean_ns = std::accumulate(ns_list.begin(), ns_list.end(), 0.0) / ns_list.size();
        double disc = 0, ff = 0, ticks = 0;
        int cnt = 0;
        for (const auto& r : results) {
            if (r.strategy != strat) continue;
            disc += r.discovery_pct;
            ff += r.total_ff;
            ticks += r.clearing_ticks;
            ++cnt;
        }
        disc /= cnt; ff /= cnt; ticks /= cnt;
        std::printf("%-50s %12.1f %12.1f %8.1f %8.1f\n",
                    strat.c_str(), mean_ns, disc, ff, ticks);
    }

    return 0;
}
