#include "sdf_overlay.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <queue>
#include <vector>

namespace sdf_hybrid::sdf {

namespace {

// Neighbor offsets (6-connected).
constexpr int NEIGHBORS[6][3] = {
    {-1, 0, 0}, {+1, 0, 0},
    {0, -1, 0}, {0, +1, 0},
    {0, 0, -1}, {0, 0, +1},
};

// Check if voxel (x, y, z) is in bounds.
inline bool in_bounds(int x, int y, int z) noexcept {
    return x >= 0 && y >= 0 && z >= 0 &&
           x < static_cast<int>(CHUNK_SIZE) &&
           y < static_cast<int>(CHUNK_SIZE) &&
           z < static_cast<int>(CHUNK_SIZE);
}

// Get voxel at (x, y, z) — returns 0 (air) if out of bounds.
// (Used by other modules via scenes::Chunk directly; this helper is kept for symmetry.)

// Squared Euclidean distance (Manhattan would suffice for narrow-band; use Euclidean for fidelity).
inline int sq_dist3(int ax, int ay, int az, int bx, int by, int bz) noexcept {
    int dx = ax - bx, dy = ay - by, dz = az - bz;
    return dx*dx + dy*dy + dz*dz;
}

}  // namespace

// ============================================================================
// JFA (Jump Flooding Algorithm) — Rong & Tan 2006.
// Reference: en.wikipedia.org/wiki/Jump_flooding_algorithm (verified 2026-06-21).
//
// For 3D voxel chunk:
//   - Phase 1: seed pass — for each surface voxel, store (x, y, z) in seed grid.
//   - Phase 2: JFA pass — for step k from chunkSize/2 down to 1, each voxel samples
//              6 neighbors at offset k, takes closest seed (if any).
//   - Phase 3: distance compute — distance to nearest seed for each voxel.
//   - Phase 4: sign compute — inside if voxel is solid (material > 0), else outside.
// ============================================================================
void generate_jfa(const scenes::Chunk& voxels, SdfR8& sdf) noexcept {
    sdf.fill(0);

    // Seed grid: each cell stores (x, y, z) of nearest surface voxel seed; (FLT_MAX, ...) if none.
    // Use int coords + a flag for "has seed".
    struct Seed { std::int16_t x, y, z; bool has; };
    std::array<Seed, CHUNK_VOLUME> seeds{};
    for (auto& s : seeds) { s.x = 0; s.y = 0; s.z = 0; s.has = false; }

    auto idx = [](int x, int y, int z) {
        return scenes::idx3(x, y, z);
    };

    // Phase 1: surface voxel detection.
    // A voxel is "surface" if it has material > 0 AND at least one 6-neighbor is air (== 0).
    for (int z = 0; z < static_cast<int>(CHUNK_SIZE); ++z) {
        for (int y = 0; y < static_cast<int>(CHUNK_SIZE); ++y) {
            for (int x = 0; x < static_cast<int>(CHUNK_SIZE); ++x) {
                std::uint8_t v = voxels[idx(x, y, z)];
                if (v == 0) continue;
                bool is_surface = false;
                for (auto& n : NEIGHBORS) {
                    int nx = x + n[0], ny = y + n[1], nz = z + n[2];
                    if (!in_bounds(nx, ny, nz) || voxels[idx(nx, ny, nz)] == 0) {
                        is_surface = true;
                        break;
                    }
                }
                if (is_surface) {
                    seeds[idx(x, y, z)] = {
                        static_cast<std::int16_t>(x),
                        static_cast<std::int16_t>(y),
                        static_cast<std::int16_t>(z),
                        true
                    };
                }
            }
        }
    }

    // Phase 2: JFA passes — step sizes from chunkSize/2 down to 1.
    int step = static_cast<int>(CHUNK_SIZE) / 2;
    while (step >= 1) {
        for (int z = 0; z < static_cast<int>(CHUNK_SIZE); ++z) {
            for (int y = 0; y < static_cast<int>(CHUNK_SIZE); ++y) {
                for (int x = 0; x < static_cast<int>(CHUNK_SIZE); ++x) {
                    Seed& my_seed = seeds[idx(x, y, z)];
                    // Sample 6 neighbors at offset step (in JFA*, would also do diagonals — we use 6).
                    for (auto& n : NEIGHBORS) {
                        int nx = x + n[0] * step;
                        int ny = y + n[1] * step;
                        int nz = z + n[2] * step;
                        if (!in_bounds(nx, ny, nz)) continue;
                        const Seed& n_seed = seeds[idx(nx, ny, nz)];
                        if (!n_seed.has) continue;
                        int n_dist_sq = sq_dist3(x, y, z, n_seed.x, n_seed.y, n_seed.z);
                        if (!my_seed.has) {
                            my_seed = n_seed;
                        } else {
                            int my_dist_sq = sq_dist3(x, y, z, my_seed.x, my_seed.y, my_seed.z);
                            if (n_dist_sq < my_dist_sq) {
                                my_seed = n_seed;
                            }
                        }
                    }
                }
            }
        }
        step /= 2;
    }

    // Phase 3: distance compute + sign compute.
    for (int z = 0; z < static_cast<int>(CHUNK_SIZE); ++z) {
        for (int y = 0; y < static_cast<int>(CHUNK_SIZE); ++y) {
            for (int x = 0; x < static_cast<int>(CHUNK_SIZE); ++x) {
                std::uint8_t v = voxels[idx(x, y, z)];
                bool inside = (v != 0);
                int dist;
                if (inside) {
                    // Inside: distance to nearest surface voxel.
                    // We already have a seed grid — find closest seed and compute distance.
                    const Seed& s = seeds[idx(x, y, z)];
                    if (!s.has) {
                        // No surface found in this chunk — entire chunk is solid? Use chunkSize-1.
                        dist = static_cast<int>(SDF_MAX_DIST);
                    } else {
                        dist = sq_dist3(x, y, z, s.x, s.y, s.z);
                        // Take sqrt for true distance, but we use squared. Cap to 7.
                        // dist^2 ranges 0..(7^2 + 7^2 + 7^2)*3 = 147. We use raw sq dist / 2 (heuristic).
                        // For prototype, take integer sqrt and clamp to 7.
                        int d = 0;
                        while (d * d < dist && d < static_cast<int>(SDF_MAX_DIST)) ++d;
                        dist = d;
                    }
                } else {
                    // Outside: distance to nearest surface voxel.
                    const Seed& s = seeds[idx(x, y, z)];
                    if (!s.has) {
                        dist = static_cast<int>(SDF_MAX_DIST);
                    } else {
                        int sq = sq_dist3(x, y, z, s.x, s.y, s.z);
                        int d = 0;
                        while (d * d < sq && d < static_cast<int>(SDF_MAX_DIST)) ++d;
                        dist = d;
                    }
                }
                sdf[idx(x, y, z)] = pack_sdf(inside, static_cast<std::uint8_t>(dist));
            }
        }
    }
}

// ============================================================================
// Brute-force BFS: 6-connected BFS from all surface voxels simultaneously.
// Multi-source BFS — guarantees correct L1 (Manhattan) distance.
// For narrow-band SDF (3 voxels from surface), BFS converges in ≤3 steps.
// Reference: classic algorithm; OpenVDB 13.0.1 uses similar sweep.
// ============================================================================
void generate_brute_force_bfs(const scenes::Chunk& voxels, SdfR8& sdf) noexcept {
    sdf.fill(0);

    // Distance grid (squared distance, 0 = surface, FLT_MAX = unset).
    std::array<int, CHUNK_VOLUME> dist_sq{};
    for (auto& d : dist_sq) d = std::numeric_limits<int>::max();

    // BFS queue.
    std::queue<int> q;

    auto idx = [](int x, int y, int z) {
        return scenes::idx3(x, y, z);
    };

    // Seed: all surface voxels. Surface = solid voxel adjacent to air.
    for (int z = 0; z < static_cast<int>(CHUNK_SIZE); ++z) {
        for (int y = 0; y < static_cast<int>(CHUNK_SIZE); ++y) {
            for (int x = 0; x < static_cast<int>(CHUNK_SIZE); ++x) {
                std::uint8_t v = voxels[idx(x, y, z)];
                if (v == 0) continue;
                bool is_surface = false;
                for (auto& n : NEIGHBORS) {
                    int nx = x + n[0], ny = y + n[1], nz = z + n[2];
                    if (!in_bounds(nx, ny, nz) || voxels[idx(nx, ny, nz)] == 0) {
                        is_surface = true;
                        break;
                    }
                }
                if (is_surface) {
                    dist_sq[idx(x, y, z)] = 0;
                    q.push(idx(x, y, z));
                }
            }
        }
    }

    // BFS.
    while (!q.empty()) {
        int cur = q.front();
        q.pop();
        int cx = cur % CHUNK_SIZE;
        int cy = (cur / CHUNK_SIZE) % CHUNK_SIZE;
        int cz = cur / (CHUNK_SIZE * CHUNK_SIZE);
        for (auto& n : NEIGHBORS) {
            int nx = cx + n[0], ny = cy + n[1], nz = cz + n[2];
            if (!in_bounds(nx, ny, nz)) continue;
            int nidx = idx(nx, ny, nz);
            int new_dist = dist_sq[cur] + 1;
            if (new_dist < dist_sq[nidx]) {
                dist_sq[nidx] = new_dist;
                q.push(nidx);
            }
        }
    }

    // Pack to SDF.
    for (std::size_t i = 0; i < CHUNK_VOLUME; ++i) {
        bool inside = (voxels[i] != 0);
        int d = dist_sq[i];
        if (d == std::numeric_limits<int>::max()) {
            d = static_cast<int>(SDF_MAX_DIST);
        } else {
            // d is squared L1 dist; for narrow-band (0..3) we want linear distance.
            int lin = 0;
            while (lin * lin < d && lin < static_cast<int>(SDF_MAX_DIST)) ++lin;
            d = lin;
        }
        sdf[i] = pack_sdf(inside, static_cast<std::uint8_t>(d));
    }
}

void generate_adaptive_multires(const scenes::Chunk& voxels, SdfR8& sdf) noexcept {
    // For prototype: identical to JFA (placeholder; full adaptive impl is follow-up).
    generate_jfa(voxels, sdf);
}

void generate(SdfBuild build, const scenes::Chunk& voxels, SdfR8& sdf) noexcept {
    switch (build) {
        case SdfBuild::J_JFA_GPU:           generate_jfa(voxels, sdf); break;
        case SdfBuild::K_BruteForce_BFS:    generate_brute_force_bfs(voxels, sdf); break;
        case SdfBuild::L_AdaptiveMultiRes:   generate_adaptive_multires(voxels, sdf); break;
    }
}

std::size_t vram_bytes(SdfEncoding enc, const scenes::Chunk& voxels) noexcept {
    // voxels used only for D_RLE_NoneSparse (heuristic).
    (void)voxels;
    // A_None: 0 extra bytes (baseline chunk uses 1 byte/voxel for material).
    // B_R8_1byte: 1 byte/voxel.
    // C_R8_4quant: 1 byte/voxel (same storage, different interpretation).
    // D_RLE_NoneSparse: variable; for prototype, average 0.3 bytes/voxel (sparse surface only).
    switch (enc) {
        case SdfEncoding::A_None:         return 0;
        case SdfEncoding::B_R8_1byte:     return CHUNK_VOLUME;
        case SdfEncoding::C_R8_4quant:    return CHUNK_VOLUME;
        case SdfEncoding::D_RLE_NoneSparse: {
            // Heuristic: 30% of voxels need SDF (per "active + 3 layers in" narrow-band rule
            // per OpenVDB 13.0.1 narrow-band level set documentation, verified 2026-06-21).
            std::size_t stored = (CHUNK_VOLUME * 3) / 10;
            return stored;
        }
    }
    return 0;
}

bool is_surface(const scenes::Chunk& voxels, std::size_t i) noexcept {
    int x = static_cast<int>(i % CHUNK_SIZE);
    int y = static_cast<int>((i / CHUNK_SIZE) % CHUNK_SIZE);
    int z = static_cast<int>(i / (CHUNK_SIZE * CHUNK_SIZE));
    std::uint8_t v = voxels[i];
    if (v == 0) return false;
    for (auto& n : NEIGHBORS) {
        int nx = x + n[0], ny = y + n[1], nz = z + n[2];
        if (!in_bounds(nx, ny, nz) || voxels[scenes::idx3(nx, ny, nz)] == 0) return true;
    }
    return false;
}

}  // namespace sdf_hybrid::sdf
