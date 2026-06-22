// 2026-06-21-wildfire-propagation standalone CPU benchmark.
//
// Standalone C++26 CPU prototype (no Vulkan, no Flecs, no ProjectV mainline).
// Build: clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic wildfire_bench.cpp -o wildfire_bench
//
// Measures 5 wildfire propagation strategies across 5 scenes with 5 seeds x 1000 iterations.
//
// The model: 8x8x8 voxel chunks (matching ProjectV chunkSize=8). Voxel types classify
// flammability; an overlay uint8_t fire_state tracks per-voxel burning progress.
// Strategies differ in HOW they update the fire_state every tick; scenes differ in
// fuel distribution. Wall time per tick is the primary metric; secondary metrics
// include spread coverage and false-spread rate.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace wfire {

// ---- Voxel material types (0-9) ----
enum Material : uint8_t {
    MAT_AIR = 0,
    MAT_STONE = 1,
    MAT_DIRT = 2,
    MAT_DRY_GRASS = 3,
    MAT_DRY_WOOD = 4,
    MAT_LIVING_WOOD = 5,
    MAT_LEAVES = 6,
    MAT_OIL = 7,
    MAT_AMMO = 8,
    MAT_WATER = 9,
    MAT_ASH = 10
};

// ---- Per-material fire properties ----
struct FuelProps {
    float ignition_temp;   // 0..1; probability of igniting when neighbor burns
    float burn_rate;      // fire_state consumed per tick (out of 255)
    float heat_output;     // 0..1; how much it heats neighbors
    float fuel_density;    // 0..255; total fuel (max fire_state duration)
    bool  leaves_ash;      // becomes MAT_AIR (true) or MAT_ASH (false) when burned
    bool  explosive;       // ammo: pops when fully burned (out of scope to simulate, just marker)
    const char* name;
};

constexpr FuelProps fuel_table[] = {
    // ignition burn heat fuel  leaves_ash  explosive name
    {  0.00f,  0.0f, 0.00f,    0.0f, false, false, "AIR"        },
    {  0.00f,  0.0f, 0.00f,    0.0f, false, false, "STONE"      },
    {  0.00f,  0.0f, 0.00f,    0.0f, false, false, "DIRT"       },
    {  0.05f, 12.0f, 0.30f,  255.0f, true,  false, "DRY_GRASS"  },  // fast spread, low fuel
    {  0.50f,  4.0f, 0.85f,  255.0f, false, false, "DRY_WOOD"   },  // hot, slow spread, high fuel
    {  0.10f,  3.0f, 0.40f,  255.0f, false, false, "LIVING_WOOD"},  // harder to ignite, slow
    {  0.30f,  8.0f, 0.60f,  255.0f, true,  false, "LEAVES"     },  // catches fire, burns fast
    {  0.95f, 20.0f, 1.00f,  255.0f, false, false, "OIL"        },  // very hot, persistent
    {  0.80f, 25.0f, 0.95f,  255.0f, false, true,  "AMMO"       },  // cook-off (we don't explode, just mark)
    {  0.00f,  0.0f, 0.00f,    0.0f, false, false, "WATER"      },  // extinguishes fire
    {  0.00f,  0.0f, 0.00f,    0.0f, false, false, "ASH"        },  // burned-out residue
};

// ---- 3D chunk ----
constexpr int CHUNK_SIZE = 8;  // matches ProjectV chunkSize=8 per agent/knowledge.md
constexpr int CHUNK_VOL = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;  // 512 voxels

// ---- World: 8x8x8 = 512 chunks => 64^3 = 262,144 voxels ----
constexpr int WORLD_CHUNKS = 8;
constexpr int WORLD_SIZE = WORLD_CHUNKS * CHUNK_SIZE;  // 64 voxels per axis
constexpr int WORLD_VOL = WORLD_SIZE * WORLD_SIZE * WORLD_SIZE;

struct World {
    std::array<uint8_t, WORLD_VOL> material{};  // MAT_*
    std::array<uint8_t, WORLD_VOL> fire{};     // 0 = no fire, 1..255 = fire intensity
    float wind_x = 0.0f, wind_y = 0.0f, wind_z = 0.0f;
    float ambient_humidity = 0.5f;  // 0=dry, 1=wet
    float ambient_temperature = 0.5f;
};

// ---- Coordinate helpers ----
constexpr int chunk_idx3(int cx, int cy, int cz) {
    return cx + WORLD_CHUNKS * (cy + WORLD_CHUNKS * cz);
}
constexpr int world_idx3(int x, int y, int z) {
    return x + WORLD_SIZE * (y + WORLD_SIZE * z);
}

inline uint8_t& mat_ref(World& w, int x, int y, int z) {
    return w.material[world_idx3(x, y, z)];
}
inline uint8_t& fire_ref(World& w, int x, int y, int z) {
    return w.fire[world_idx3(x, y, z)];
}

// ---- Wind advection: returns per-axis spread probability multiplier ----
inline float wind_factor_x(float wind_x, int dx) {
    if ((dx > 0 && wind_x > 0) || (dx < 0 && wind_x < 0)) return 1.0f + std::abs(wind_x) * 2.0f;
    if (dx == 0) return 1.0f;
    return std::max(0.1f, 1.0f - std::abs(wind_x) * 0.5f);
}
inline float wind_factor_y(float wind_y, int dy) {
    // Vertical wind (updraft) does not strongly bias spread; small effect
    if (dy > 0) return 1.0f + std::abs(wind_y) * 0.3f;
    if (dy < 0) return std::max(0.5f, 1.0f - std::abs(wind_y) * 0.2f);
    return 1.0f;
}
inline float wind_factor_z(float wind_z, int dz) {
    if ((dz > 0 && wind_z > 0) || (dz < 0 && wind_z < 0)) return 1.0f + std::abs(wind_z) * 2.0f;
    if (dz == 0) return 1.0f;
    return std::max(0.1f, 1.0f - std::abs(wind_z) * 0.5f);
}

// ---- Strategy A: NoFire (baseline) ----
// All chunks scanned, fire_state = 0 forever, zero changes
// Used to measure: pure scan overhead + correct absence of any false spread
struct StrategyA {
    static constexpr const char* name = "A_NoFire";
    static void ignite([[maybe_unused]] World& w, [[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int z, [[maybe_unused]] uint8_t intensity) {
        // No-op: fire is disabled in this strategy
    }
    static void tick([[maybe_unused]] World& w, [[maybe_unused]] std::mt19937& rng) {
        // No-op
    }
};

// ---- Strategy B: SimpleDrosselSchwabl_CA ----
// Direct translation of Drossel-Schwabl 1992 fire model to 3D voxels with
// per-material ignition probability. Burning voxel -> AIR (or ASH), neighbor
// ignites with prob p_ignition. Wind scales ignition probability.
struct StrategyB {
    static constexpr const char* name = "B_DrosselSchwabl_CA";
    static void ignite(World& w, int x, int y, int z, uint8_t intensity) {
        if (mat_ref(w, x, y, z) != MAT_AIR) fire_ref(w, x, y, z) = intensity;
    }
    static void tick(World& w, std::mt19937& rng) {
        // Two-pass: first compute new fire states, then commit
        World next;
        next.material = w.material;
        std::fill(next.fire.begin(), next.fire.end(), (uint8_t)0);
        // For each burning voxel, decide spread to neighbors + decay
        for (int z = 0; z < WORLD_SIZE; ++z) {
            for (int y = 0; y < WORLD_SIZE; ++y) {
                for (int x = 0; x < WORLD_SIZE; ++x) {
                    uint8_t cur = w.fire[world_idx3(x, y, z)];
                    if (cur == 0) continue;
                    Material m = static_cast<Material>(w.material[world_idx3(x, y, z)]);
                    FuelProps fp = fuel_table[m];
                    // Decay fire
                    if (cur > fp.burn_rate) {
                        next.fire[world_idx3(x, y, z)] = static_cast<uint8_t>(cur - fp.burn_rate);
                    } else {
                        // Voxel fully burned
                        if (fp.leaves_ash) {
                            next.material[world_idx3(x, y, z)] = MAT_AIR;
                        } else {
                            next.material[world_idx3(x, y, z)] = MAT_ASH;
                        }
                        next.fire[world_idx3(x, y, z)] = 0;
                        continue;
                    }
                    // Spread to 26 neighbors
                    for (int dz = -1; dz <= 1; ++dz) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                if (dx == 0 && dy == 0 && dz == 0) continue;
                                int nx = x + dx, ny = y + dy, nz = z + dz;
                                if (nx < 0 || nx >= WORLD_SIZE || ny < 0 || ny >= WORLD_SIZE || nz < 0 || nz >= WORLD_SIZE) continue;
                                if (w.fire[world_idx3(nx, ny, nz)] > 0) continue;
                                Material nm = static_cast<Material>(w.material[world_idx3(nx, ny, nz)]);
                                FuelProps nfp = fuel_table[nm];
                                if (nfp.fuel_density == 0.0f) continue;
                                float p_ignite = nfp.ignition_temp * fp.heat_output;
                                p_ignite *= wind_factor_x(w.wind_x, dx) * wind_factor_y(w.wind_y, dy) * wind_factor_z(w.wind_z, dz);
                                p_ignite *= (1.0f - w.ambient_humidity * 0.7f);
                                std::uniform_real_distribution<float> uni(0.0f, 1.0f);
                                if (uni(rng) < p_ignite * 0.5f) {
                                    next.fire[world_idx3(nx, ny, nz)] = 255;
                                }
                            }
                        }
                    }
                }
            }
        }
        w.material = next.material;
        w.fire = next.fire;
    }
};

// ---- Strategy C: RothermelFuelModel_ReactionDiffusion ----
// Rothermel 1972 surface fire spread rate:
//   R = R0 * (1 + phi_w + phi_s)
// where phi_w = wind coefficient, phi_s = slope coefficient
// Combined with reaction-diffusion-like continuous fire intensity update.
struct StrategyC {
    static constexpr const char* name = "C_RothermelFuelModel_RD";
    static void ignite(World& w, int x, int y, int z, uint8_t intensity) {
        if (mat_ref(w, x, y, z) != MAT_AIR) fire_ref(w, x, y, z) = intensity;
    }
    // Rothermel base spread rate (m/min) per fuel model category
    static float rothermel_base(Material m) {
        switch (m) {
            case MAT_DRY_GRASS:   return 0.40f;
            case MAT_DRY_WOOD:    return 0.15f;
            case MAT_LIVING_WOOD: return 0.08f;
            case MAT_LEAVES:      return 0.35f;
            case MAT_OIL:         return 0.80f;
            case MAT_AMMO:        return 0.70f;
            default: return 0.0f;
        }
    }
    // Wind coefficient phi_w = C * (1 + U/6.9)^something for fuel; simplified linear
    static float wind_coeff(float U) {
        return std::min(2.0f, std::abs(U) * 0.6f);
    }
    static void tick(World& w, std::mt19937& rng) {
        // Single-pass: update fire_state in place using deltas (RD-like)
        std::vector<std::tuple<int, int, int, uint8_t>> ignitions;
        for (int z = 0; z < WORLD_SIZE; ++z) {
            for (int y = 0; y < WORLD_SIZE; ++y) {
                for (int x = 0; x < WORLD_SIZE; ++x) {
                    uint8_t cur = w.fire[world_idx3(x, y, z)];
                    if (cur == 0) continue;
                    Material m = static_cast<Material>(w.material[world_idx3(x, y, z)]);
                    FuelProps fp = fuel_table[m];
                    if (cur > fp.burn_rate * 0.5f) {
                        w.fire[world_idx3(x, y, z)] = static_cast<uint8_t>(cur - fp.burn_rate * 0.5f);
                    } else {
                        if (fp.leaves_ash) {
                            w.material[world_idx3(x, y, z)] = MAT_AIR;
                        } else {
                            w.material[world_idx3(x, y, z)] = MAT_ASH;
                        }
                        w.fire[world_idx3(x, y, z)] = 0;
                        continue;
                    }
                    float R0 = rothermel_base(m);
                    if (R0 == 0.0f) continue;
                    float phi_w = wind_coeff(w.wind_x + w.wind_z);
                    float spread_rate = R0 * (1.0f + phi_w);
                    spread_rate *= (1.0f - w.ambient_humidity * 0.6f);
                    float p_ignite_per_neighbor = std::min(0.5f, spread_rate * 0.3f);
                    for (int dz = -1; dz <= 1; ++dz) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                if (dx == 0 && dy == 0 && dz == 0) continue;
                                int nx = x + dx, ny = y + dy, nz = z + dz;
                                if (nx < 0 || nx >= WORLD_SIZE || ny < 0 || ny >= WORLD_SIZE || nz < 0 || nz >= WORLD_SIZE) continue;
                                if (w.fire[world_idx3(nx, ny, nz)] > 0) continue;
                                Material nm = static_cast<Material>(w.material[world_idx3(nx, ny, nz)]);
                                FuelProps nfp = fuel_table[nm];
                                if (nfp.fuel_density == 0.0f) continue;
                                float p = p_ignite_per_neighbor * nfp.ignition_temp;
                                std::uniform_real_distribution<float> uni(0.0f, 1.0f);
                                if (uni(rng) < p) {
                                    ignitions.emplace_back(nx, ny, nz, static_cast<uint8_t>(200));
                                }
                            }
                        }
                    }
                }
            }
        }
        for (auto& [nx, ny, nz, intensity] : ignitions) {
            w.fire[world_idx3(nx, ny, nz)] = intensity;
        }
    }
};

// ---- Strategy D: WindAdvectedCA_Bresenham3D ----
// Spot ignitions projected along wind direction via 3D Bresenham-like sampling.
struct StrategyD {
    static constexpr const char* name = "D_WindAdvectedCA_Bresenham3D";
    static void ignite(World& w, int x, int y, int z, uint8_t intensity) {
        if (mat_ref(w, x, y, z) != MAT_AIR) fire_ref(w, x, y, z) = intensity;
    }
    static void tick(World& w, std::mt19937& rng) {
        World next;
        next.material = w.material;
        std::fill(next.fire.begin(), next.fire.end(), (uint8_t)0);
        for (int z = 0; z < WORLD_SIZE; ++z) {
            for (int y = 0; y < WORLD_SIZE; ++y) {
                for (int x = 0; x < WORLD_SIZE; ++x) {
                    uint8_t cur = w.fire[world_idx3(x, y, z)];
                    if (cur == 0) continue;
                    Material m = static_cast<Material>(w.material[world_idx3(x, y, z)]);
                    FuelProps fp = fuel_table[m];
                    if (cur > fp.burn_rate * 0.7f) {
                        next.fire[world_idx3(x, y, z)] = static_cast<uint8_t>(cur - fp.burn_rate * 0.7f);
                    } else {
                        if (fp.leaves_ash) {
                            next.material[world_idx3(x, y, z)] = MAT_AIR;
                        } else {
                            next.material[world_idx3(x, y, z)] = MAT_ASH;
                        }
                        continue;
                    }
                    // Spot fire: project up to N voxels downwind
                    float wind_mag = std::sqrt(w.wind_x*w.wind_x + w.wind_y*w.wind_y + w.wind_z*w.wind_z);
                    if (wind_mag > 0.3f) {
                        int spot_distance = static_cast<int>(wind_mag * 8.0f);
                        if (spot_distance > 0) {
                            float dx_n = w.wind_x / wind_mag;
                            float dy_n = w.wind_y / wind_mag;
                            float dz_n = w.wind_z / wind_mag;
                            int samples = spot_distance;
                            bool ignited_spot = false;
                            for (int i = 1; i <= samples && !ignited_spot; ++i) {
                                int sx = x + static_cast<int>(dx_n * i);
                                int sy_ = y + static_cast<int>(dy_n * i);
                                int sz_ = z + static_cast<int>(dz_n * i);
                                if (sx < 0 || sx >= WORLD_SIZE || sy_ < 0 || sy_ >= WORLD_SIZE || sz_ < 0 || sz_ >= WORLD_SIZE) break;
                                Material sm = static_cast<Material>(w.material[world_idx3(sx, sy_, sz_)]);
                                FuelProps sfp = fuel_table[sm];
                                if (sfp.fuel_density == 0.0f) continue;
                                float p_spot = sfp.ignition_temp * (1.0f - float(i) / float(samples + 1));
                                std::uniform_real_distribution<float> uni(0.0f, 1.0f);
                                if (uni(rng) < p_spot * 0.4f) {
                                    next.fire[world_idx3(sx, sy_, sz_)] = 200;
                                    ignited_spot = true;
                                }
                            }
                        }
                    }
                    // Standard neighbor spread (lower base probability)
                    for (int dz = -1; dz <= 1; ++dz) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                if (dx == 0 && dy == 0 && dz == 0) continue;
                                int nx = x + dx, ny = y + dy, nz = z + dz;
                                if (nx < 0 || nx >= WORLD_SIZE || ny < 0 || ny >= WORLD_SIZE || nz < 0 || nz >= WORLD_SIZE) continue;
                                if (w.fire[world_idx3(nx, ny, nz)] > 0) continue;
                                Material nm = static_cast<Material>(w.material[world_idx3(nx, ny, nz)]);
                                FuelProps nfp = fuel_table[nm];
                                if (nfp.fuel_density == 0.0f) continue;
                                float p = nfp.ignition_temp * fp.heat_output * 0.3f;
                                std::uniform_real_distribution<float> uni(0.0f, 1.0f);
                                if (uni(rng) < p) {
                                    next.fire[world_idx3(nx, ny, nz)] = 255;
                                }
                            }
                        }
                    }
                }
            }
        }
        w.material = next.material;
        w.fire = next.fire;
    }
};

// ---- Strategy E: ChunkLazy_Bitmask ----
// Only process active chunks (chunks with at least one burning voxel);
// spread across chunk boundary is via 1-cell halo of active chunks.
struct StrategyE {
    static constexpr const char* name = "E_ChunkLazy_Bitmask";
    static void ignite(World& w, int x, int y, int z, uint8_t intensity) {
        if (mat_ref(w, x, y, z) != MAT_AIR) fire_ref(w, x, y, z) = intensity;
    }
    static void tick(World& w, std::mt19937& rng) {
        // Build active chunk bitmask by scanning fire arrays
        std::array<bool, WORLD_CHUNKS*WORLD_CHUNKS*WORLD_CHUNKS> active{};
        for (int cz = 0; cz < WORLD_CHUNKS; ++cz) {
            for (int cy = 0; cy < WORLD_CHUNKS; ++cy) {
                for (int cx = 0; cx < WORLD_CHUNKS; ++cx) {
                    int start = (cx*CHUNK_SIZE) + WORLD_SIZE * ((cy*CHUNK_SIZE) + WORLD_SIZE * (cz*CHUNK_SIZE));
                    bool has_fire = false;
                    for (int i = 0; i < CHUNK_VOL; ++i) {
                        if (w.fire[start + i] > 0) { has_fire = true; break; }
                    }
                    active[chunk_idx3(cx, cy, cz)] = has_fire;
                }
            }
        }
        // Expand bitmask to 1-cell halo (so spread across chunk boundary works)
        std::array<bool, WORLD_CHUNKS*WORLD_CHUNKS*WORLD_CHUNKS> halo{};
        for (int cz = 0; cz < WORLD_CHUNKS; ++cz) {
            for (int cy = 0; cy < WORLD_CHUNKS; ++cy) {
                for (int cx = 0; cx < WORLD_CHUNKS; ++cx) {
                    if (active[chunk_idx3(cx, cy, cz)]) {
                        for (int dz = -1; dz <= 1; ++dz) {
                            for (int dy = -1; dy <= 1; ++dy) {
                                for (int dx = -1; dx <= 1; ++dx) {
                                    int ncx = cx + dx, ncy = cy + dy, ncz = cz + dz;
                                    if (ncx < 0 || ncx >= WORLD_CHUNKS) continue;
                                    if (ncy < 0 || ncy >= WORLD_CHUNKS) continue;
                                    if (ncz < 0 || ncz >= WORLD_CHUNKS) continue;
                                    halo[chunk_idx3(ncx, ncy, ncz)] = true;
                                }
                            }
                        }
                    }
                }
            }
        }
        World next;
        next.material = w.material;
        std::fill(next.fire.begin(), next.fire.end(), (uint8_t)0);
        for (int z = 0; z < WORLD_SIZE; ++z) {
            for (int y = 0; y < WORLD_SIZE; ++y) {
                for (int x = 0; x < WORLD_SIZE; ++x) {
                    int ccx = x / CHUNK_SIZE, ccy = y / CHUNK_SIZE, ccz = z / CHUNK_SIZE;
                    if (!halo[chunk_idx3(ccx, ccy, ccz)]) continue;
                    uint8_t cur = w.fire[world_idx3(x, y, z)];
                    if (cur == 0) continue;
                    Material m = static_cast<Material>(w.material[world_idx3(x, y, z)]);
                    FuelProps fp = fuel_table[m];
                    if (cur > fp.burn_rate) {
                        next.fire[world_idx3(x, y, z)] = static_cast<uint8_t>(cur - fp.burn_rate);
                    } else {
                        if (fp.leaves_ash) {
                            next.material[world_idx3(x, y, z)] = MAT_AIR;
                        } else {
                            next.material[world_idx3(x, y, z)] = MAT_ASH;
                        }
                        continue;
                    }
                    for (int dz = -1; dz <= 1; ++dz) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                if (dx == 0 && dy == 0 && dz == 0) continue;
                                int nx = x + dx, ny = y + dy, nz = z + dz;
                                if (nx < 0 || nx >= WORLD_SIZE || ny < 0 || ny >= WORLD_SIZE || nz < 0 || nz >= WORLD_SIZE) continue;
                                if (w.fire[world_idx3(nx, ny, nz)] > 0) continue;
                                int ncx = nx / CHUNK_SIZE, ncy = ny / CHUNK_SIZE, ncz = nz / CHUNK_SIZE;
                                if (!halo[chunk_idx3(ncx, ncy, ncz)]) continue;
                                Material nm = static_cast<Material>(w.material[world_idx3(nx, ny, nz)]);
                                FuelProps nfp = fuel_table[nm];
                                if (nfp.fuel_density == 0.0f) continue;
                                float p = nfp.ignition_temp * fp.heat_output;
                                p *= wind_factor_x(w.wind_x, dx) * wind_factor_y(w.wind_y, dy) * wind_factor_z(w.wind_z, dz);
                                p *= (1.0f - w.ambient_humidity * 0.7f);
                                std::uniform_real_distribution<float> uni(0.0f, 1.0f);
                                if (uni(rng) < p * 0.5f) {
                                    next.fire[world_idx3(nx, ny, nz)] = 255;
                                }
                            }
                        }
                    }
                }
            }
        }
        w.material = next.material;
        w.fire = next.fire;
    }
};

// ---- Scene generators ----
struct Scene {
    std::string name;
    std::function<void(World&)> setup;
    int expected_min_spread;  // 0 = fire should NOT spread; >0 = minimum voxels that should burn
    int expected_max_spread;  // 0 = no max; >0 = max voxels that should burn (sanity)
    std::string description;
};

void make_uniform_floor(World& w) {
    // All air; ignition point should not spread (no fuel)
    for (int z = 0; z < WORLD_SIZE; ++z) {
        for (int y = 0; y < WORLD_SIZE; ++y) {
            for (int x = 0; x < WORLD_SIZE; ++x) {
                mat_ref(w, x, y, z) = MAT_AIR;
            }
        }
    }
    // Place a stone floor at y=0
    for (int z = 0; z < WORLD_SIZE; ++z) {
        for (int x = 0; x < WORLD_SIZE; ++x) {
            mat_ref(w, x, 0, z) = MAT_STONE;
        }
    }
    w.wind_x = 0.1f; w.wind_y = 0.0f; w.wind_z = 0.0f;
    w.ambient_humidity = 0.5f;
}

void make_forest_lush(World& w) {
    // Mostly living wood and leaves; should NOT spread easily (high humidity, low wind)
    for (int z = 0; z < WORLD_SIZE; ++z) {
        for (int y = 0; y < WORLD_SIZE; ++y) {
            for (int x = 0; x < WORLD_SIZE; ++x) {
                if (y < 2) {
                    mat_ref(w, x, y, z) = MAT_DIRT;
                } else if (y < 4) {
                    mat_ref(w, x, y, z) = (x+y+z) % 3 == 0 ? MAT_LIVING_WOOD : MAT_DIRT;
                } else if (y < 6) {
                    mat_ref(w, x, y, z) = ((x+z) % 2 == 0) ? MAT_LIVING_WOOD : MAT_LEAVES;
                } else if (y < 7) {
                    mat_ref(w, x, y, z) = MAT_LEAVES;
                } else {
                    mat_ref(w, x, y, z) = MAT_AIR;
                }
            }
        }
    }
    w.wind_x = 0.05f; w.wind_y = 0.0f; w.wind_z = 0.0f;
    w.ambient_humidity = 0.7f;  // wet, fire hard to spread
}

void make_forest_dry_windy(World& w) {
    // Dry wood + dry grass, strong wind; should spread aggressively
    for (int z = 0; z < WORLD_SIZE; ++z) {
        for (int y = 0; y < WORLD_SIZE; ++y) {
            for (int x = 0; x < WORLD_SIZE; ++x) {
                if (y == 0) mat_ref(w, x, y, z) = MAT_DIRT;
                else if (y == 1) mat_ref(w, x, y, z) = (x+z) % 4 == 0 ? MAT_DRY_GRASS : MAT_DIRT;
                else if (y == 2) mat_ref(w, x, y, z) = (x+z) % 2 == 0 ? MAT_DRY_WOOD : MAT_DRY_GRASS;
                else if (y == 3) mat_ref(w, x, y, z) = (x+z) % 3 == 0 ? MAT_DRY_WOOD : MAT_LEAVES;
                else if (y == 4) mat_ref(w, x, y, z) = (x+z) % 2 == 0 ? MAT_LEAVES : MAT_DRY_WOOD;
                else if (y == 5) mat_ref(w, x, y, z) = MAT_AIR;
                else mat_ref(w, x, y, z) = MAT_AIR;
            }
        }
    }
    w.wind_x = 0.7f; w.wind_y = 0.0f; w.wind_z = 0.5f;  // strong horizontal wind
    w.ambient_humidity = 0.1f;  // very dry
}

void make_urban_periphery(World& w) {
    // Mixed: buildings (stone), trees, grass
    for (int z = 0; z < WORLD_SIZE; ++z) {
        for (int y = 0; y < WORLD_SIZE; ++y) {
            for (int x = 0; x < WORLD_SIZE; ++x) {
                if (y == 0) {
                    mat_ref(w, x, y, z) = MAT_DIRT;
                } else if (y < 4 && (x < 8 || x > 55) && (z < 8 || z > 55)) {
                    // Building corners
                    mat_ref(w, x, y, z) = MAT_STONE;
                } else if (y == 1) {
                    mat_ref(w, x, y, z) = (x+z) % 3 == 0 ? MAT_DRY_GRASS : MAT_DIRT;
                } else if (y == 2 || y == 3) {
                    mat_ref(w, x, y, z) = (x+z) % 5 == 0 ? MAT_DRY_WOOD : MAT_AIR;
                } else {
                    mat_ref(w, x, y, z) = MAT_AIR;
                }
            }
        }
    }
    w.wind_x = 0.3f; w.wind_y = 0.0f; w.wind_z = 0.2f;
    w.ambient_humidity = 0.4f;
}

void make_ammunition_dump(World& w) {
    // High fuel density, multiple ignition points
    for (int z = 0; z < WORLD_SIZE; ++z) {
        for (int y = 0; y < WORLD_SIZE; ++y) {
            for (int x = 0; x < WORLD_SIZE; ++x) {
                if (y == 0) mat_ref(w, x, y, z) = MAT_DIRT;
                else if (y < 4 && x >= 16 && x < 48 && z >= 16 && z < 48) {
                    // Ammunition stockpile area
                    if ((x+z) % 2 == 0) mat_ref(w, x, y, z) = MAT_AMMO;
                    else if (y == 1) mat_ref(w, x, y, z) = MAT_OIL;
                    else mat_ref(w, x, y, z) = MAT_DRY_WOOD;
                } else if (y == 1) {
                    mat_ref(w, x, y, z) = (x+z) % 2 == 0 ? MAT_OIL : MAT_DIRT;
                } else {
                    mat_ref(w, x, y, z) = MAT_AIR;
                }
            }
        }
    }
    w.wind_x = 0.5f; w.wind_y = 0.0f; w.wind_z = 0.3f;
    w.ambient_humidity = 0.2f;
}

const std::vector<Scene> scenes = {
    {"uniform_floor",     make_uniform_floor,       0,    1,    "No fuel; fire should NOT spread (0 false positives)"},
    {"forest_lush",        make_forest_lush,         5,    500,  "Living wood + leaves; should slowly spread (humid)"},
    {"forest_dry_windy",   make_forest_dry_windy,    100,  50000,"Dry wood + grass + strong wind; should spread aggressively"},
    {"urban_periphery",    make_urban_periphery,     20,   20000,"Mixed buildings/grass/wood; moderate spread"},
    {"ammunition_dump",    make_ammunition_dump,     50,   60000,"High fuel density; cook-off + oil; very aggressive"},
};

// ---- Counting helpers ----
int count_burning(const World& w) {
    int count = 0;
    for (int i = 0; i < WORLD_VOL; ++i) {
        if (w.fire[i] > 0) count++;
    }
    return count;
}
int count_ash(const World& w) {
    int count = 0;
    for (int i = 0; i < WORLD_VOL; ++i) {
        if (w.material[i] == MAT_ASH) count++;
    }
    return count;
}
int count_flammable_initial(const World& w) {
    int count = 0;
    for (int i = 0; i < WORLD_VOL; ++i) {
        if (fuel_table[w.material[i]].fuel_density > 0.0f) count++;
    }
    return count;
}

// ---- Ignition helper: find first flammable voxel in the world ----
struct IgnitionPoint {
    int x, y, z;
    bool found;
};

IgnitionPoint find_ignition(const World& w) {
    IgnitionPoint ip{WORLD_SIZE/2, WORLD_SIZE/2, WORLD_SIZE/2, false};
    // Prefer center XZ column if it has fuel
    int cx = WORLD_SIZE/2, cz = WORLD_SIZE/2;
    for (int y = 0; y < WORLD_SIZE; ++y) {
        Material m = static_cast<Material>(w.material[world_idx3(cx, y, cz)]);
        if (fuel_table[m].fuel_density > 0.0f) {
            ip.x = cx; ip.y = y; ip.z = cz; ip.found = true;
            return ip;
        }
    }
    // Fallback: scan whole world for any flammable voxel (urban_periphery has
    // flammable voxels at non-center positions, e.g. perimeter trees/grass)
    for (int z = 0; z < WORLD_SIZE && !ip.found; ++z) {
        for (int y = 0; y < WORLD_SIZE && !ip.found; ++y) {
            for (int x = 0; x < WORLD_SIZE && !ip.found; ++x) {
                Material m = static_cast<Material>(w.material[world_idx3(x, y, z)]);
                if (fuel_table[m].fuel_density > 0.0f) {
                    ip.x = x; ip.y = y; ip.z = z; ip.found = true;
                }
            }
        }
    }
    return ip;
}

// ---- Template runner ----
template<typename Strategy>
double run_strategy(const Scene& scene, int seed, int iters, int warmup) {
    World w;
    scene.setup(w);
    IgnitionPoint ip = find_ignition(w);
    if (ip.found) Strategy::ignite(w, ip.x, ip.y, ip.z, 200);
    std::mt19937 rng(seed);
    for (int i = 0; i < warmup; ++i) {
        Strategy::tick(w, rng);
    }
    w = World{};
    scene.setup(w);
    ip = find_ignition(w);
    if (ip.found) Strategy::ignite(w, ip.x, ip.y, ip.z, 200);
    rng = std::mt19937(seed);
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        Strategy::tick(w, rng);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double total_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    return total_ns / double(iters);
}

struct FinalStats {
    int burning;
    int ash;
    int flammable_initial;
    int ignition_x, ignition_y, ignition_z;
};

template<typename Strategy>
FinalStats run_strategy_final_stats(const Scene& scene, int seed, int iters, int warmup) {
    World w;
    scene.setup(w);
    int flammable = count_flammable_initial(w);
    IgnitionPoint ip = find_ignition(w);
    if (ip.found) Strategy::ignite(w, ip.x, ip.y, ip.z, 200);
    std::mt19937 rng(seed);
    for (int i = 0; i < warmup; ++i) Strategy::tick(w, rng);
    w = World{};
    scene.setup(w);
    ip = find_ignition(w);
    if (ip.found) Strategy::ignite(w, ip.x, ip.y, ip.z, 200);
    rng = std::mt19937(seed);
    for (int i = 0; i < iters; ++i) Strategy::tick(w, rng);
    return {count_burning(w), count_ash(w), flammable, ip.x, ip.y, ip.z};
}

}  // namespace wfire

int main() {
    using namespace wfire;
    constexpr int ITERS = 1000;
    constexpr int WARMUP = 10;
    constexpr int NUM_SEEDS = 5;
    int seeds[NUM_SEEDS] = {1, 7, 42, 1234, 31337};

    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,iter,ns_per_tick,burning_final,ash_count,flammable_initial,expected_min_consumed,expected_max_consumed\n";
    std::cout << "strategy,scene,seed,ns_per_tick,burning_final,ash_count\n";

    for (const auto& scene : scenes) {
        // Run A_NoFire
        for (int s = 0; s < NUM_SEEDS; ++s) {
            double ns = run_strategy<StrategyA>(scene, seeds[s], ITERS, WARMUP);
            FinalStats fs = run_strategy_final_stats<StrategyA>(scene, seeds[s], ITERS, WARMUP);
            csv << StrategyA::name << "," << scene.name << "," << seeds[s] << "," << ITERS << ","
                << std::fixed << std::setprecision(2) << ns << "," << fs.burning << ","
                << fs.ash << "," << fs.flammable_initial << ","
                << scene.expected_min_spread << "," << scene.expected_max_spread << "\n";
            std::cout << StrategyA::name << "," << scene.name << "," << seeds[s] << ","
                      << std::fixed << std::setprecision(2) << ns << "," << fs.burning << "," << fs.ash << "\n";
        }
        // Run B_DrosselSchwabl_CA
        for (int s = 0; s < NUM_SEEDS; ++s) {
            double ns = run_strategy<StrategyB>(scene, seeds[s], ITERS, WARMUP);
            FinalStats fs = run_strategy_final_stats<StrategyB>(scene, seeds[s], ITERS, WARMUP);
            csv << StrategyB::name << "," << scene.name << "," << seeds[s] << "," << ITERS << ","
                << std::fixed << std::setprecision(2) << ns << "," << fs.burning << ","
                << fs.ash << "," << fs.flammable_initial << ","
                << scene.expected_min_spread << "," << scene.expected_max_spread << "\n";
            std::cout << StrategyB::name << "," << scene.name << "," << seeds[s] << ","
                      << std::fixed << std::setprecision(2) << ns << "," << fs.burning << "," << fs.ash << "\n";
        }
        // Run C_RothermelFuelModel_RD
        for (int s = 0; s < NUM_SEEDS; ++s) {
            double ns = run_strategy<StrategyC>(scene, seeds[s], ITERS, WARMUP);
            FinalStats fs = run_strategy_final_stats<StrategyC>(scene, seeds[s], ITERS, WARMUP);
            csv << StrategyC::name << "," << scene.name << "," << seeds[s] << "," << ITERS << ","
                << std::fixed << std::setprecision(2) << ns << "," << fs.burning << ","
                << fs.ash << "," << fs.flammable_initial << ","
                << scene.expected_min_spread << "," << scene.expected_max_spread << "\n";
            std::cout << StrategyC::name << "," << scene.name << "," << seeds[s] << ","
                      << std::fixed << std::setprecision(2) << ns << "," << fs.burning << "," << fs.ash << "\n";
        }
        // Run D_WindAdvectedCA_Bresenham3D
        for (int s = 0; s < NUM_SEEDS; ++s) {
            double ns = run_strategy<StrategyD>(scene, seeds[s], ITERS, WARMUP);
            FinalStats fs = run_strategy_final_stats<StrategyD>(scene, seeds[s], ITERS, WARMUP);
            csv << StrategyD::name << "," << scene.name << "," << seeds[s] << "," << ITERS << ","
                << std::fixed << std::setprecision(2) << ns << "," << fs.burning << ","
                << fs.ash << "," << fs.flammable_initial << ","
                << scene.expected_min_spread << "," << scene.expected_max_spread << "\n";
            std::cout << StrategyD::name << "," << scene.name << "," << seeds[s] << ","
                      << std::fixed << std::setprecision(2) << ns << "," << fs.burning << "," << fs.ash << "\n";
        }
        // Run E_ChunkLazy_Bitmask
        for (int s = 0; s < NUM_SEEDS; ++s) {
            double ns = run_strategy<StrategyE>(scene, seeds[s], ITERS, WARMUP);
            FinalStats fs = run_strategy_final_stats<StrategyE>(scene, seeds[s], ITERS, WARMUP);
            csv << StrategyE::name << "," << scene.name << "," << seeds[s] << "," << ITERS << ","
                << std::fixed << std::setprecision(2) << ns << "," << fs.burning << ","
                << fs.ash << "," << fs.flammable_initial << ","
                << scene.expected_min_spread << "," << scene.expected_max_spread << "\n";
            std::cout << StrategyE::name << "," << scene.name << "," << seeds[s] << ","
                      << std::fixed << std::setprecision(2) << ns << "," << fs.burning << "," << fs.ash << "\n";
        }
    }
    csv.close();
    std::cout << "\nWrote build/results.csv\n";
    return 0;
}
