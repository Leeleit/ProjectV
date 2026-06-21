// cover_bench.cpp — C++26 CPU prototype: voxel terrain cover extraction + classification + scoring
// 5 strategies x 5 scenes x 5 seeds x 1000 iter + 10 warmup = 125,000 main measurements
// Clang 22.1.6: clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic -o cover_bench cover_bench.cpp

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <random>
#include <vector>
#include <span>
#include <chrono>
#include <cmath>
#include <string>
#include <charconv>

// === Configuration ===
constexpr int CHUNK_SIZE = 8;
constexpr int CHUNK_VOXELS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
constexpr int COVER_MAX_POINTS = 256;

// === Cover types ===
enum class CoverType : uint8_t {
    NONE = 0,
    FULL,      // blocks entire body (>=2 voxels tall)
    PARTIAL,   // blocks lower body only (1 voxel)
    LEAN,      // corner peeking
    OVERHEAD,  // ceiling/overhang protection
    SLOPE      // diagonal defilade
};

const char* CoverTypeName(CoverType t) {
    switch (t) {
        case CoverType::FULL:    return "FULL";
        case CoverType::PARTIAL: return "PARTIAL";
        case CoverType::LEAN:    return "LEAN";
        case CoverType::OVERHEAD:return "OVERHEAD";
        case CoverType::SLOPE:   return "SLOPE";
        default:                 return "NONE";
    }
}

// === Cover point ===
struct CoverPoint {
    int x, y, z;          // position in chunk-local coords (0..7)
    CoverType type;
    float score;          // 0..1 quality
    uint8_t directions;   // bitmask: which directions provide cover (bit 0=-X, 1=+X, 2=-Y, 3=+Y, 4=-Z, 5=+Z)
};

// === Voxel chunk (8^3) ===
using Chunk = std::array<uint8_t, CHUNK_VOXELS>;

// === Scene generators ===
// All use uint8_t: 0 = air, 1+ = solid materials

static void gen_uniform_floor(Chunk& c, std::mt19937_64&) {
    // bottom 4 layers solid, top 4 air
    for (int i = 0; i < CHUNK_VOXELS; ++i) {
        int y = (i / (CHUNK_SIZE * CHUNK_SIZE)) % CHUNK_SIZE;  // column-major: z*64 + y*8 + x
        c[i] = (y < 4) ? 1 : 0;
    }
}

static void gen_forest_floor(Chunk& c, std::mt19937_64& rng) {
    // floor (bottom 2 layers) + scattered columns (tree trunks)
    std::fill(c.begin(), c.end(), 0);
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int idx = z * CHUNK_SIZE * CHUNK_SIZE + x; // y=0
            c[idx] = 1;
            int idx2 = z * CHUNK_SIZE * CHUNK_SIZE + CHUNK_SIZE + x; // y=1
            c[idx2] = 1;
            // tree trunks at ~1/4 density
            if ((x % 2 == 0 && z % 2 == 0) || (x % 3 == 0 && z % 3 == 1)) {
                for (int y = 2; y < CHUNK_SIZE; ++y) {
                    int idx3 = z * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + x;
                    c[idx3] = (y < CHUNK_SIZE - 1) ? 2 : 0; // trunk with top open
                }
            }
        }
    }
    // some random surface rocks
    std::uniform_int_distribution<int> dist(0, CHUNK_SIZE - 1);
    for (int i = 0; i < 4; ++i) {
        int rx = dist(rng), rz = dist(rng);
        int y = 1;
        int idx = rz * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + rx;
        if (c[idx] == 0) c[idx] = 1; // extend floor
    }
}

static void gen_cave_stress(Chunk& c, std::mt19937_64& rng) {
    // solid with tunnel network
    std::fill(c.begin(), c.end(), 1);
    // carve tunnels
    std::uniform_int_distribution<int> dist(1, CHUNK_SIZE - 2);
    int cx = dist(rng), cz = dist(rng);
    for (int i = 0; i < 6; ++i) {
        int dx = (dist(rng) % 3) - 1;
        int dz = (dist(rng) % 3) - 1;
        cx = std::clamp(cx + dx, 1, CHUNK_SIZE - 2);
        cz = std::clamp(cz + dz, 1, CHUNK_SIZE - 2);
        for (int y = 1; y < CHUNK_SIZE - 1; ++y) {
            // carve 2x2 tunnel
            for (int ox = 0; ox < 2; ++ox) {
                for (int oz = 0; oz < 2; ++oz) {
                    int tx = std::clamp(cx + ox, 0, CHUNK_SIZE - 1);
                    int tz = std::clamp(cz + oz, 0, CHUNK_SIZE - 1);
                    int idx = tz * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + tx;
                    c[idx] = 0;
                }
            }
        }
    }
    // enlarge a cavern
    for (int y = 3; y < 6; ++y) {
        for (int x = 3; x < 6; ++x) {
            for (int z = 3; z < 6; ++z) {
                int idx = z * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + x;
                c[idx] = 0;
            }
        }
    }
}

static void gen_mixed_biome(Chunk& c, std::mt19937_64& rng) {
    // random 50% fill with some structure
    std::uniform_int_distribution<int> dist(0, 99);
    for (int i = 0; i < CHUNK_VOXELS; ++i) {
        c[i] = (dist(rng) < 50) ? 1 : 0;
    }
    // ensure bottom layer is mostly solid
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int idx = z * CHUNK_SIZE * CHUNK_SIZE + x;
            c[idx] = 1;
        }
    }
}

static void gen_building_interior(Chunk& c, std::mt19937_64&) {
    // walls + corners + rooms (2 rooms with a corridor)
    std::fill(c.begin(), c.end(), 0);
    // outer walls
    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            int idx_f = 0 * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + x; // z=0 wall
            int idx_b = 7 * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + x; // z=7 wall
            c[idx_f] = 1;
            c[idx_b] = 1;
        }
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int idx_l = z * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + 0; // x=0 wall
            int idx_r = z * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + 7; // x=7 wall
            c[idx_l] = 1;
            c[idx_r] = 1;
        }
    }
    // interior wall dividing two rooms, with doorway
    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int idx = z * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + 4; // x=4 wall
            if (z >= 3 && z <= 4 && y >= 0 && y <= 3) continue; // doorway
            c[idx] = 1;
        }
    }
    // ceiling (y=7)
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int idx = z * CHUNK_SIZE * CHUNK_SIZE + 7 * CHUNK_SIZE + x;
            if (c[idx] == 0) c[idx] = 1; // ceiling
        }
    }
}

using SceneGen = void(*)(Chunk&, std::mt19937_64&);

struct SceneDef {
    const char* name;
    SceneGen gen;
};

static constexpr SceneDef SCENES[] = {
    {"uniform_floor",    gen_uniform_floor},
    {"forest_floor",     gen_forest_floor},
    {"cave_stress",      gen_cave_stress},
    {"mixed_biome",      gen_mixed_biome},
    {"building_interior", gen_building_interior}
};
constexpr int N_SCENES = 5;

// === Inline helpers ===
inline int voxel_idx(int x, int y, int z) {
    return z * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + x;
}

inline bool is_solid(const Chunk& c, int x, int y, int z) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return true; // out of bounds = solid (world is continuous)
    return c[voxel_idx(x, y, z)] != 0;
}

// === Strategy A: NaiveBoundary — simple solid-air boundary at chunk face level ===
// Walks all solid voxels, checks each face neighbor; if neighbor is air, marks a cover point
// on the solid face with direction pointing away from the solid.
static int extract_cover_A(const Chunk& c, CoverPoint* out, int max_out) {
    int count = 0;
    // 6 face directions: (-X, +X, -Y, +Y, -Z, +Z)
    constexpr int dx[6] = {-1, 1, 0, 0, 0, 0};
    constexpr int dy[6] = {0, 0, -1, 1, 0, 0};
    constexpr int dz[6] = {0, 0, 0, 0, -1, 1};

    for (int x = 0; x < CHUNK_SIZE && count < max_out; ++x) {
        for (int z = 0; z < CHUNK_SIZE && count < max_out; ++z) {
            for (int y = 0; y < CHUNK_SIZE && count < max_out; ++y) {
                int idx = voxel_idx(x, y, z);
                if (c[idx] == 0) continue; // air, skip
                uint8_t dirs = 0;
                for (int d = 0; d < 6; ++d) {
                    int nx = x + dx[d], ny = y + dy[d], nz = z + dz[d];
                    if (!is_solid(c, nx, ny, nz)) {
                        dirs |= (1 << d);
                    }
                }
                if (dirs == 0) continue; // no exposed face

                CoverType type = CoverType::PARTIAL;
                // check if there's a solid neighbor above to make it full cover
                bool has_above = is_solid(c, x, y + 1, z);
                // check if there are multiple stacked solid voxels
                // Actually check: 2+ solid voxels in column qualifies as full
                int solid_count = 0;
                for (int sy = y; sy < CHUNK_SIZE; ++sy) {
                    if (c[voxel_idx(x, sy, z)] != 0) ++solid_count;
                    else break;
                }
                type = (solid_count >= 2) ? CoverType::FULL : CoverType::PARTIAL;

                // if corner (dirs has bits on perpendicular axes), mark LEAN
                bool has_x = (dirs & 0x03) != 0;
                bool has_z = (dirs & 0x30) != 0;
                if (has_x && has_z) type = CoverType::LEAN;

                float score = (type == CoverType::FULL) ? 0.8f :
                              (type == CoverType::LEAN) ? 0.6f : 0.4f;

                out[count++] = {x, y, z, type, score, dirs};
            }
        }
    }
    return count;
}

// === Strategy B: EdgeWalking — scan solid-air edges, find overhangs and ledges ===
static int extract_cover_B(const Chunk& c, CoverPoint* out, int max_out) {
    int count = 0;
    // First pass: find all solid-air boundary voxels (same as A)
    constexpr int dx[6] = {-1, 1, 0, 0, 0, 0};
    constexpr int dy[6] = {0, 0, -1, 1, 0, 0};
    constexpr int dz[6] = {0, 0, 0, 0, -1, 1};

    for (int x = 0; x < CHUNK_SIZE && count < max_out; ++x) {
        for (int z = 0; z < CHUNK_SIZE && count < max_out; ++z) {
            for (int y = 0; y < CHUNK_SIZE && count < max_out; ++y) {
                int idx = voxel_idx(x, y, z);
                if (c[idx] == 0) continue;

                uint8_t dirs = 0;
                for (int d = 0; d < 6; ++d) {
                    int nx = x + dx[d], ny = y + dy[d], nz = z + dz[d];
                    if (!is_solid(c, nx, ny, nz)) {
                        dirs |= (1 << d);
                    }
                }
                if (dirs == 0) continue;

                // Edge-aware classification
                CoverType type = CoverType::PARTIAL;
                float score = 0.3f;

                // Check overhang: is there air below this voxel?
                bool air_below = (y > 0) && (c[voxel_idx(x, y-1, z)] == 0);
                // If air below and solid above + around = this is an overhang ceiling
                if (air_below && (dirs & (1 << 1)) == 0) { // no air above
                    // check lateral coverage
                    bool covered_dir_x = (dirs & 0x03) == 0; // covered on both X sides
                    bool covered_dir_z = (dirs & 0x30) == 0; // covered on both Z sides
                    if (covered_dir_x || covered_dir_z) {
                        type = CoverType::OVERHEAD;
                        score = 0.7f;
                    }
                }

                // Check for ledge (y=0, air below is out-of-bounds — treat as ledge)
                if (y == 0 && (c[voxel_idx(x, 0, z)] != 0)) {
                    // floor-level cover
                    int solid_count = 0;
                    for (int sy = 0; sy < CHUNK_SIZE; ++sy) {
                        if (c[voxel_idx(x, sy, z)] != 0) ++solid_count;
                        else break;
                    }
                    if (solid_count >= 3) { type = CoverType::FULL; score = 0.85f; }
                    else { type = CoverType::PARTIAL; score = 0.45f; }
                }

                // Corner detection for LEAN
                bool has_x = (dirs & 0x03) != 0;
                bool has_z = (dirs & 0x30) != 0;
                if (has_x && has_z && type != CoverType::OVERHEAD) {
                    type = CoverType::LEAN;
                    score = 0.65f;
                }

                // Slope detection: check diagonal neighbors
                // A slope has solid on one diagonal but not the cardinal
                for (int d = 0; d < 6; ++d) {
                    int nx = x + dx[d], ny = y + dy[d], nz = z + dz[d];
                    if (!is_solid(c, nx, ny, nz)) {
                        // check diagonal below this direction
                        int ny2 = ny - 1;
                        if (ny2 >= 0) {
                            // diagonal-down check
                            for (int axis = 0; axis < 3; ++axis) {
                                // Simple slope heuristic
                            }
                        }
                    }
                }
                // Simplified slope: check if this voxel has a diagonal air neighbor
                // that would create a stair-step profile
                bool is_slope = false;
                for (int ox = -1; ox <= 1 && !is_slope; ++ox) {
                    for (int oz = -1; oz <= 1 && !is_slope; ++oz) {
                        if (ox == 0 && oz == 0) continue;
                        int nx = x + ox, nz = z + oz;
                        if (nx < 0 || nx >= CHUNK_SIZE || nz < 0 || nz >= CHUNK_SIZE) continue;
                        if (y > 0) {
                            int idx_diag = voxel_idx(nx, y-1, nz);
                            if (c[idx_diag] == 0) {
                                // air diagonal-below + solid here = stair step
                                // check if the cardinal direction is also solid
                                int idx_card = voxel_idx(nx, y, nz);
                                if (c[idx_card] == 0) {
                                    is_slope = true;
                                }
                            }
                        }
                    }
                }
                if (is_slope && type != CoverType::OVERHEAD) {
                    type = CoverType::SLOPE;
                    score = 0.5f;
                }

                out[count++] = {x, y, z, type, score, dirs};
            }
        }
    }
    return count;
}

// === Strategy C: OverhangDetect — explicit overhang/ledge detection (from topology-analysis pattern) ===
// Focuses on finding ceiling cover and ledge cover using column analysis
static int extract_cover_C(const Chunk& c, CoverPoint* out, int max_out) {
    int count = 0;
    // Column-based analysis: for each (x,z), scan y column to find overhangs
    for (int x = 0; x < CHUNK_SIZE && count < max_out; ++x) {
        for (int z = 0; z < CHUNK_SIZE && count < max_out; ++z) {
            // Walk y from bottom to top
            int top_solid = -1;
            int bottom_air = -1;
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                int idx = voxel_idx(x, y, z);
                if (c[idx] != 0) {
                    top_solid = y;
                } else if (top_solid >= 0) {
                    // air above solid = potential overhang ceiling at top_solid
                    // Check contiguous solid segment above
                    int seg_start = top_solid;
                    while (seg_start > 0 && c[voxel_idx(x, seg_start-1, z)] != 0) --seg_start;
                    int seg_len = top_solid - seg_start + 1;
                    if (seg_len >= 1 && seg_len <= 4) {
                        // ceiling cover point on the underside
                        // The air voxel below provides overhead cover from below
                        int ceiling_y = y; // the air voxel
                        if (ceiling_y > 0 && count < max_out) {
                            // check lateral neighbors for solid (to verify overhang)
                            bool lat_solid_x = is_solid(c, x+1, ceiling_y, z) || is_solid(c, x-1, ceiling_y, z);
                            bool lat_solid_z = is_solid(c, x, ceiling_y, z+1) || is_solid(c, x, ceiling_y, z-1);
                            if (lat_solid_x || lat_solid_z) {
                                CoverType ct = CoverType::OVERHEAD;
                                float sc = 0.75f;
                                if (seg_len >= 2) sc = 0.85f; // thicker ceiling = better cover
                                out[count++] = {x, ceiling_y, z, ct, sc, 0x3C}; // covers from all lateral + above
                            }
                        }
                    }
                    top_solid = -1;
                }
            }

            // Floor cover (from bottom solid)
            int floor_top = -1;
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                int idx = voxel_idx(x, y, z);
                if (c[idx] != 0) { floor_top = y; break; }
            }
            if (floor_top >= 0) {
                // Check how many consecutive solid voxels
                int solid_count = 0;
                for (int y = floor_top; y < CHUNK_SIZE; ++y) {
                    if (c[voxel_idx(x, y, z)] != 0) ++solid_count;
                    else break;
                }
                if (solid_count >= 2 && count < max_out) {
                    CoverType ct = (solid_count >= 3) ? CoverType::FULL : CoverType::PARTIAL;
                    float sc = (solid_count >= 3) ? 0.8f : 0.5f;
                    out[count++] = {x, floor_top, z, ct, sc, 0x08}; // +Y direction cover
                }
            }
        }
    }
    return count;
}

// === Strategy D: CornerDetect — detect convex/concave corners with directional protection ===
static int extract_cover_D(const Chunk& c, CoverPoint* out, int max_out) {
    int count = 0;
    // For each solid-air boundary, check if it's a corner (convex or concave)
    for (int x = 0; x < CHUNK_SIZE && count < max_out; ++x) {
        for (int z = 0; z < CHUNK_SIZE && count < max_out; ++z) {
            for (int y = 0; y < CHUNK_SIZE && count < max_out; ++y) {
                int idx = voxel_idx(x, y, z);
                if (c[idx] == 0) continue;

                // Count exposed faces
                uint8_t exposed = 0;
                uint8_t dirs = 0;
                constexpr int dx[6] = {-1, 1, 0, 0, 0, 0};
                constexpr int dy[6] = {0, 0, -1, 1, 0, 0};
                constexpr int dz[6] = {0, 0, 0, 0, -1, 1};
                for (int d = 0; d < 6; ++d) {
                    int nx = x + dx[d], ny = y + dy[d], nz = z + dz[d];
                    if (!is_solid(c, nx, ny, nz)) {
                        exposed++;
                        dirs |= (1 << d);
                    }
                }
                if (exposed == 0) continue;
                if (exposed > 3) continue; // too exposed to be useful cover

                // Check direction mask for corner patterns
                bool has_Xneg = (dirs & 1);   // -X
                bool has_Xpos = (dirs & 2);   // +X
                [[maybe_unused]] bool has_Yneg = (dirs & 4);   // -Y
                bool has_Zneg = (dirs & 16);  // -Z
                bool has_Zpos = (dirs & 32);  // +Z

                CoverType type = CoverType::PARTIAL;
                float score = 0.3f;

                // Convex corner: two perpendicular faces exposed
                // e.g. (-X & -Z) = outside corner
                bool is_convex_corner = false;
                if (has_Xneg && has_Zneg) is_convex_corner = true;
                if (has_Xneg && has_Zpos) is_convex_corner = true;
                if (has_Xpos && has_Zneg) is_convex_corner = true;
                if (has_Xpos && has_Zpos) is_convex_corner = true;

                if (is_convex_corner) {
                    type = CoverType::LEAN;
                    score = 0.7f;
                }

                // Concave corner: one face exposed with solid on both perpendicular neighbors
                bool is_concave = false;
                for (int d = 0; d < 6; ++d) {
                    int nx = x + dx[d], ny = y + dy[d], nz = z + dz[d];
                    if (!is_solid(c, nx, ny, nz)) {
                        // Check perpendicular directions
                        int p1 = (d % 2 == 0) ? d + 1 : d - 1; // opposite
                        // The two perpendicular axes
                        for (int axis = 0; axis < 3; ++axis) {
                            int a1 = axis * 2;
                            int a2 = axis * 2 + 1;
                            if (a1 == d || a1 == p1 || a2 == d || a2 == p1) continue;
                            if (is_solid(c, x + dx[a1], y + dy[a1], z + dz[a1]) &&
                                is_solid(c, x + dx[a2], y + dy[a2], z + dz[a2])) {
                                is_concave = true;
                            }
                        }
                    }
                }
                if (is_concave && !is_convex_corner) {
                    // Alcove/recess — good cover
                    type = CoverType::FULL;
                    score = 0.9f;
                }

                // Check vertical clearance for FULL vs PARTIAL
                if (type != CoverType::LEAN && type != CoverType::FULL) {
                    int solid_count = 0;
                    for (int sy = y; sy < CHUNK_SIZE; ++sy) {
                        if (c[voxel_idx(x, sy, z)] != 0) ++solid_count;
                        else break;
                    }
                    if (solid_count >= 2) { type = CoverType::FULL; score = 0.8f; }
                }

                out[count++] = {x, y, z, type, score, dirs};
            }
        }
    }
    return count;
}

// === Strategy E: HybridCover — combine all methods + scoring ===
// Merge cover points from all strategies, deduplicate, score and rank
static int extract_cover_E(const Chunk& c, CoverPoint* out, int max_out) {
    CoverPoint temp[COVER_MAX_POINTS * 4];
    int nA = extract_cover_A(c, temp, COVER_MAX_POINTS);
    int nB = extract_cover_B(c, temp + nA, COVER_MAX_POINTS);
    int nC = extract_cover_C(c, temp + nA + nB, COVER_MAX_POINTS);
    int nD = extract_cover_D(c, temp + nA + nB + nC, COVER_MAX_POINTS);
    int total = nA + nB + nC + nD;

    // Deduplicate by (x,y,z), keep highest scored
    CoverPoint deduped[COVER_MAX_POINTS];
    int nd = 0;
    for (int i = 0; i < total && nd < max_out; ++i) {
        const auto& tp = temp[i];
        // Check if we already have this position
        bool dupe = false;
        for (int j = 0; j < nd; ++j) {
            if (deduped[j].x == tp.x && deduped[j].y == tp.y && deduped[j].z == tp.z) {
                if (tp.score > deduped[j].score) deduped[j] = tp;
                dupe = true;
                break;
            }
        }
        if (!dupe) deduped[nd++] = tp;
    }

    // Scoring pass: refine scores based on multi-factor evaluation
    for (int i = 0; i < nd; ++i) {
        auto& pt = deduped[i];
        // Base score from type
        switch (pt.type) {
            case CoverType::FULL:    pt.score = 0.7f; break;
            case CoverType::LEAN:    pt.score = 0.6f; break;
            case CoverType::OVERHEAD:pt.score = 0.5f; break;
            case CoverType::SLOPE:   pt.score = 0.4f; break;
            case CoverType::PARTIAL: pt.score = 0.3f; break;
            default:                 pt.score = 0.0f; break;
        }

        // Bonus for exposed direction count (sheltered from more sides = better)
        int exposed = std::popcount((unsigned)pt.directions);
        if (exposed <= 1) pt.score += 0.15f; // sheltered on 5+ sides
        else if (exposed == 2) pt.score += 0.05f;
        else pt.score -= 0.1f;

        // Penalty for being too high in the chunk (air above = sky exposure risk)
        if (pt.y >= CHUNK_SIZE - 2) pt.score -= 0.05f;

        // Bonus for ground-level (y=0 or y=1)
        if (pt.y <= 1) pt.score += 0.05f;

        // Clamp
        pt.score = std::clamp(pt.score, 0.0f, 1.0f);
    }

    // Sort by score descending
    std::sort(deduped, deduped + nd, [](const CoverPoint& a, const CoverPoint& b) {
        return a.score > b.score;
    });

    int copy = std::min(nd, max_out);
    std::memcpy(out, deduped, copy * sizeof(CoverPoint));
    return copy;
}

// === Benchmark harness ===
struct BenchResult {
    double mean_us;
    double median_us;
    double p95_us;
    double p99_us;
    double std_us;
    int64_t total_ns;
    int n_iter;
    int cover_count;
};

using CoverStrategy = int(*)(const Chunk&, CoverPoint*, int);

struct StrategyDef {
    const char* name;
    CoverStrategy fn;
};

static constexpr StrategyDef STRATEGIES[] = {
    {"A_NaiveBoundary",  extract_cover_A},
    {"B_EdgeWalking",    extract_cover_B},
    {"C_OverhangDetect", extract_cover_C},
    {"D_CornerDetect",   extract_cover_D},
    {"E_HybridCover",    extract_cover_E}
};
constexpr int N_STRATEGIES = 5;

static BenchResult run_bench(const StrategyDef& sd, const Chunk& scene) {
    constexpr int WARMUP = 10;
    constexpr int ITER = 1000;

    std::vector<int64_t> samples;
    samples.reserve(ITER);
    CoverPoint cover_buf[COVER_MAX_POINTS];
    int cover_count = 0;

    // Warmup
    for (int i = 0; i < WARMUP; ++i) {
        cover_count = sd.fn(scene, cover_buf, COVER_MAX_POINTS);
    }

    // Measurement
    for (int i = 0; i < ITER; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        cover_count = sd.fn(scene, cover_buf, COVER_MAX_POINTS);
        auto t1 = std::chrono::steady_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        samples.push_back(ns);
    }

    // Stats
    std::sort(samples.begin(), samples.end());
    int64_t total_ns = std::accumulate(samples.begin(), samples.end(), int64_t{0});
    double mean_us = (double)total_ns / ITER / 1000.0;
    double median_us = samples[ITER / 2] / 1000.0;
    double p95_us = samples[(int)(ITER * 0.95)] / 1000.0;
    double p99_us = samples[(int)(ITER * 0.99)] / 1000.0;

    double sum_sq = 0;
    for (auto s : samples) {
        double diff = (double)s / 1000.0 - mean_us;
        sum_sq += diff * diff;
    }
    double std_us = std::sqrt(sum_sq / ITER);

    return {mean_us, median_us, p95_us, p99_us, std_us, total_ns, ITER, cover_count};
}

int main() {
    std::printf("strategy,scene,seed,mean_us,median_us,p95_us,p99_us,std_us,cover_count\n");

    constexpr int SEEDS[] = {1, 7, 42, 1234, 31337};
    constexpr int N_SEEDS = 5;

    for (int si = 0; si < N_SCENES; ++si) {
        for (int seed_idx = 0; seed_idx < N_SEEDS; ++seed_idx) {
            int seed = SEEDS[seed_idx];
            std::mt19937_64 rng(seed);
            Chunk chunk;
            SCENES[si].gen(chunk, rng);

            for (int st = 0; st < N_STRATEGIES; ++st) {
                auto res = run_bench(STRATEGIES[st], chunk);
                std::printf("%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n",
                    STRATEGIES[st].name, SCENES[si].name, seed,
                    res.mean_us, res.median_us, res.p95_us, res.p99_us,
                    res.std_us, res.cover_count);
            }
        }
    }

    return 0;
}
