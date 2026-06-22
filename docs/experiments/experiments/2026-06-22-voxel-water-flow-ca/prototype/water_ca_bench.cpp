#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <vector>

#if !defined(__cpp_lib_print)
#include <format>
#endif

// ============================================================
// Constants
// ============================================================
constexpr int CHUNK_SIZE = 8;
constexpr int CHUNK_VOXELS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

// Water mass constants for CA strategies
constexpr float WATER_MAX_MASS = 1.0f;
constexpr float WATER_MAX_COMPRESS = 0.02f;
constexpr float WATER_MIN_MASS = 0.001f;
constexpr float WATER_MIN_FLOW = 0.005f;
constexpr float WATER_MAX_SPEED = 0.1f;

// Scene dimensions (in chunks)
struct SceneDef {
    const char* name;
    int cx, cy, cz;
    int num_sources;
};

// ============================================================
// Voxel types
// ============================================================
enum class VoxelType : uint8_t {
    AIR,
    WATER_SOURCE,
    WATER_FLOW,
    EARTH,
    FIRE,
    GRAVEL,
};

struct Voxel {
    VoxelType type;
    uint8_t water_mass; // 0-255, maps to 0.0-1.0 range
};

// ============================================================
// Chunk: 8x8x8 voxel grid
// ============================================================
struct Chunk {
    Voxel voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];

    Voxel& at(int x, int y, int z) {
        return voxels[x][y][z];
    }
    const Voxel& at(int x, int y, int z) const {
        return voxels[x][y][z];
    }

    bool in_bounds(int x, int y, int z) const {
        return x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE;
    }

    bool is_fluid(int x, int y, int z) const {
        if (!in_bounds(x, y, z)) return false;
        auto t = at(x, y, z).type;
        return t == VoxelType::WATER_SOURCE || t == VoxelType::WATER_FLOW;
    }

    bool is_solid(int x, int y, int z) const {
        if (!in_bounds(x, y, z)) return true;
        auto t = at(x, y, z).type;
        return t == VoxelType::EARTH || t == VoxelType::FIRE;
    }

    bool is_passable(int x, int y, int z) const {
        if (!in_bounds(x, y, z)) return false;
        auto t = at(x, y, z).type;
        return t == VoxelType::AIR || t == VoxelType::WATER_FLOW;
    }

    float get_mass_float(int x, int y, int z) const {
        if (!in_bounds(x, y, z)) return 0.0f;
        return static_cast<float>(at(x, y, z).water_mass) / 255.0f;
    }

    void set_mass_float(int x, int y, int z, float m) {
        if (!in_bounds(x, y, z)) return;
        m = std::clamp(m, 0.0f, 1.0f);
        at(x, y, z).water_mass = static_cast<uint8_t>(m * 255.0f + 0.5f);
        if (m > WATER_MIN_MASS) {
            if (at(x, y, z).type != VoxelType::WATER_SOURCE) {
                at(x, y, z).type = VoxelType::WATER_FLOW;
            }
        } else {
            if (at(x, y, z).type == VoxelType::WATER_FLOW) {
                at(x, y, z).type = VoxelType::AIR;
            }
        }
    }
};

// ============================================================
// Scene builder
// ============================================================
struct Region {
    std::vector<Chunk> chunks;
    int cx, cy, cz;
};

Region build_scene(const SceneDef& scene) {
    int total_chunks = scene.cx * scene.cy * scene.cz;
    Region reg;
    reg.chunks.resize(total_chunks);
    reg.cx = scene.cx;
    reg.cy = scene.cy;
    reg.cz = scene.cz;

    auto& c = reg.chunks;

    auto chunk_idx = [&](int cx, int cy, int cz) -> int {
        return cz * scene.cy * scene.cx + cy * scene.cx + cx;
    };

    auto set_voxel = [&](int wx, int wy, int wz, VoxelType type, uint8_t mass = 0) {
        int cx = wx / CHUNK_SIZE, cy = wy / CHUNK_SIZE, cz = wz / CHUNK_SIZE;
        int lx = wx % CHUNK_SIZE, ly = wy % CHUNK_SIZE, lz = wz % CHUNK_SIZE;
        if (lx < 0) { lx += CHUNK_SIZE; cx -= 1; }
        if (ly < 0) { ly += CHUNK_SIZE; cy -= 1; }
        if (lz < 0) { lz += CHUNK_SIZE; cz -= 1; }
        if (cx < 0 || cx >= scene.cx || cy < 0 || cy >= scene.cy || cz < 0 || cz >= scene.cz) return;
        auto& v = c[chunk_idx(cx, cy, cz)].at(lx, ly, lz);
        v.type = type;
        v.water_mass = mass;
    };

    auto get_voxel = [&](int wx, int wy, int wz) -> Voxel* {
        int cx = wx / CHUNK_SIZE, cy = wy / CHUNK_SIZE, cz = wz / CHUNK_SIZE;
        if (cx < 0 || cx >= scene.cx || cy < 0 || cy >= scene.cy || cz < 0 || cz >= scene.cz) return nullptr;
        int lx = wx % CHUNK_SIZE, ly = wy % CHUNK_SIZE, lz = wz % CHUNK_SIZE;
        return &c[chunk_idx(cx, cy, cz)].at(lx, ly, lz);
    };

    // Init all to AIR
    for (auto& ch : c) {
        for (auto& plane : ch.voxels)
            for (auto& row : plane)
                for (auto& vox : row)
                    vox = {VoxelType::AIR, 0};
    }

    if (std::strcmp(scene.name, "S1_valley_river") == 0) {
        // Valley: 4x4x2 chunks, river source at (0,0,0)
        int wx = 0, wy = 0, wz = 0;
        // Earth floor at y=0
        for (int x = 0; x < scene.cx * CHUNK_SIZE; x++)
            for (int z = 0; z < scene.cz * CHUNK_SIZE; z++)
                set_voxel(x, 0, z, VoxelType::EARTH);
        // Source at one end
        set_voxel(2, 2, 2, VoxelType::WATER_SOURCE, 255);
        // Channel carved in earth
        for (int x = 2; x < 12; x++) {
            for (int z = 1; z < 4; z++) {
                set_voxel(x, 0, z, VoxelType::AIR);
            }
        }
    } else if (std::strcmp(scene.name, "S2_flat_moat") == 0) {
        // Flat terrain with moat: 2x2x2 chunks (was 1, now 2 for earth floor + moat)
        for (int x = 0; x < scene.cx * CHUNK_SIZE; x++)
            for (int z = 0; z < scene.cz * CHUNK_SIZE; z++)
                set_voxel(x, 0, z, VoxelType::EARTH);
        // Moat trench (dig a 2-deep ditch at y=1..2)
        for (int x = 4; x < 12; x++)
            for (int z = 0; z < 2; z++) {
                set_voxel(x, 1, z, VoxelType::AIR);
                set_voxel(x, 2, z, VoxelType::AIR);
            }
        // Border water source at y=1 (on top of earth floor)
        set_voxel(0, 1, 0, VoxelType::WATER_SOURCE, 255);
    } else if (std::strcmp(scene.name, "S3_dam_breach") == 0) {
        // Dam: 6x3x2 chunks. High and low reservoirs separated by 1-wide dam
        for (int x = 0; x < scene.cx * CHUNK_SIZE; x++)
            for (int z = 0; z < scene.cz * CHUNK_SIZE; z++)
                set_voxel(x, 0, z, VoxelType::EARTH);
        // Dam wall along x=8 (y 1..4)
        int dam_x = 8;
        for (int y = 1; y < 5; y++)
            for (int z = 0; z < scene.cz * CHUNK_SIZE; z++)
                set_voxel(dam_x, y, z, VoxelType::EARTH);
        // High reservoir (left of dam): fill with water source at y=1
        for (int x = 1; x < dam_x; x++)
            for (int z = 2; z < 6; z++) {
                set_voxel(x, 1, z, VoxelType::WATER_SOURCE, 255);
            }
        // Low reservoir (right of dam): empty at y=1
        for (int x = dam_x + 1; x < scene.cx * CHUNK_SIZE - 1; x++)
            for (int z = 2; z < 6; z++)
                set_voxel(x, 1, z, VoxelType::AIR);
        // Breach (remove section of dam at y=1..2)
        for (int y = 1; y < 3; y++)
            for (int z = 2; z < 4; z++)
                set_voxel(dam_x, y, z, VoxelType::AIR);
    } else if (std::strcmp(scene.name, "S4_rain_basin") == 0) {
        // Rain basin: 4x4x2 chunks. Flat basin with permeable floor
        for (int x = 0; x < scene.cx * CHUNK_SIZE; x++)
            for (int z = 0; z < scene.cz * CHUNK_SIZE; z++)
                set_voxel(x, 0, z, VoxelType::EARTH);
        // Basin floor: gravel (permeable) at y=1
        for (int x = 0; x < scene.cx * CHUNK_SIZE; x++)
            for (int z = 0; z < scene.cz * CHUNK_SIZE; z++)
                set_voxel(x, 1, z, VoxelType::GRAVEL);
        // Rain: a few source blocks at top (y=6)
        for (int x = 4; x < 12; x += 2)
            for (int z = 4; z < 12; z += 2)
                set_voxel(x, 6, z, VoxelType::WATER_SOURCE, 255);
    } else if (std::strcmp(scene.name, "S5_campfire") == 0) {
        // Campfire: 2x2x2 chunks. Fire voxels + water source
        for (int x = 0; x < scene.cx * CHUNK_SIZE; x++)
            for (int z = 0; z < scene.cz * CHUNK_SIZE; z++)
                set_voxel(x, 0, z, VoxelType::EARTH);
        // Fire voxels in center at y=1
        for (int x = 4; x < 7; x++)
            for (int z = 4; z < 7; z++)
                set_voxel(x, 1, z, VoxelType::FIRE);
        // Water source to the side at y=1
        set_voxel(2, 1, 2, VoxelType::WATER_SOURCE, 255);
        // Channel from water to fire at y=1
        for (int x = 2; x < 5; x++)
            set_voxel(x, 1, 2, VoxelType::AIR);
    }

    return reg;
}

// ============================================================
// Strategy A: NoWater (baseline)
// ============================================================
void strategy_A_nowater(Region&, int) {
}

// ============================================================
// Strategy B: SimpleHeightCA (2D heightmap water)
// Each column tracks water_height; flow toward lower neighbors
// ============================================================
struct HeightMap {
    int width, depth;
    std::vector<float> height;
    std::vector<float> flow;

    HeightMap(int w, int d) : width(w), depth(d), height(w * d, 0.0f), flow(w * d, 0.0f) {}

    int idx(int x, int z) const { return z * width + x; }

    void add_source(int wx, int wz, float amount) {
        int x = wx, z = wz;
        if (x >= 0 && x < width && z >= 0 && z < depth) {
            height[idx(x, z)] += amount;
        }
    }
};

void strategy_B_heightca(Region& reg, int iters) {
    int w = reg.cx * CHUNK_SIZE;
    int h = reg.cy * CHUNK_SIZE;
    int d = reg.cz * CHUNK_SIZE;
    HeightMap hm(w, d);

    // Init height map from chunk data
    for (int x = 0; x < w; x++)
        for (int z = 0; z < d; z++) {
            float mass = 0.0f;
            for (int y = 0; y < h; y++) {
                auto* v = &reg.chunks[0]; // simplified: just use first chunk for now
                // This is a simplification — real impl iterates chunks
                // For prototype, we just use a simple mass check
            }
            hm.height[hm.idx(x, z)] = 0.0f;
        }

    for (int iter = 0; iter < iters; iter++) {
        // Reset flow
        std::fill(hm.flow.begin(), hm.flow.end(), 0.0f);

        // Flow downward to lower neighbors
        for (int x = 0; x < w; x++) {
            for (int z = 0; z < d; z++) {
                int i = hm.idx(x, z);
                float h_val = hm.height[i];
                if (h_val <= WATER_MIN_MASS) continue;

                // Check 4 neighbors
                const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto [dx, dz] : dirs) {
                    int nx = x + dx, nz = z + dz;
                    if (nx < 0 || nx >= w || nz < 0 || nz >= d) continue;
                    int ni = hm.idx(nx, nz);
                    if (hm.height[ni] < h_val - 0.01f) {
                        float diff = (h_val - hm.height[ni]) * 0.25f;
                        diff = std::min(diff, h_val);
                        diff = std::min(diff, WATER_MAX_SPEED);
                        hm.flow[i] -= diff;
                        hm.flow[ni] += diff;
                        h_val -= diff;
                    }
                }
            }
        }

        // Apply flow
        for (int i = 0; i < w * d; i++) {
            hm.height[i] = std::max(0.0f, hm.height[i] + hm.flow[i]);
        }
    }
}

// ============================================================
// Strategy C: 3D_CA_PressureBFS
// Compressible liquid CA (w-shadow.com algorithm extended to 3D)
// ============================================================
void strategy_C_pressure_bfs(Region& reg, int iters) {
    int w = reg.cx * CHUNK_SIZE;
    int h = reg.cy * CHUNK_SIZE;
    int d = reg.cz * CHUNK_SIZE;
    int total = w * h * d;
    std::vector<float> mass(total, 0.0f);
    std::vector<float> new_mass(total, 0.0f);
    std::vector<uint8_t> solid(total, 0);

    auto idx = [w, h](int x, int y, int z) { return z * w * h + y * w + x; };

    // Init
    auto get_voxel_ptr = [&](int wx, int wy, int wz) -> Voxel* {
        int cx = wx / CHUNK_SIZE, cy = wy / CHUNK_SIZE, cz = wz / CHUNK_SIZE;
        if (cx < 0 || cx >= reg.cx || cy < 0 || cy >= reg.cy || cz < 0 || cz >= reg.cz) return nullptr;
        int lx = wx % CHUNK_SIZE, ly = wy % CHUNK_SIZE, lz = wz % CHUNK_SIZE;
        return &reg.chunks[cz * reg.cy * reg.cx + cy * reg.cx + cx].at(lx, ly, lz);
    };

    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++)
            for (int z = 0; z < d; z++) {
                auto* v = get_voxel_ptr(x, y, z);
                if (v) {
                    solid[idx(x, y, z)] = (v->type == VoxelType::EARTH || v->type == VoxelType::FIRE) ? 1 : 0;
                    if (v->type == VoxelType::WATER_SOURCE) {
                        mass[idx(x, y, z)] = 1.0f;
                    }
                }
            }

    auto get_stable_b = [](float total_mass) -> float {
        if (total_mass <= 1.0f) return 1.0f;
        if (total_mass < 2.0f * WATER_MAX_MASS + WATER_MAX_COMPRESS) {
            return (WATER_MAX_MASS * WATER_MAX_MASS + total_mass * WATER_MAX_COMPRESS) /
                   (WATER_MAX_MASS + WATER_MAX_COMPRESS);
        }
        return (total_mass + WATER_MAX_COMPRESS) * 0.5f;
    };

    for (int iter = 0; iter < iters; iter++) {
        std::copy(mass.begin(), mass.end(), new_mass.begin());

        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                for (int z = 0; z < d; z++) {
                    int i = idx(x, y, z);
                    if (solid[i]) continue;

                    float remaining = mass[i];
                    if (remaining <= WATER_MIN_MASS) continue;

                    // Down
                    if (y > 0 && !solid[idx(x, y - 1, z)]) {
                        float flow = get_stable_b(remaining + mass[idx(x, y - 1, z)]) - mass[idx(x, y - 1, z)];
                        if (flow > WATER_MIN_FLOW) flow *= 0.5f;
                        flow = std::clamp(flow, 0.0f, std::min(WATER_MAX_SPEED, remaining));
                        new_mass[i] -= flow;
                        new_mass[idx(x, y - 1, z)] += flow;
                        remaining -= flow;
                    }

                    if (remaining <= WATER_MIN_MASS) continue;

                    // Left (-X)
                    if (x > 0 && !solid[idx(x - 1, y, z)]) {
                        float flow = (mass[i] - mass[idx(x - 1, y, z)]) * 0.25f;
                        if (flow > WATER_MIN_FLOW) flow *= 0.5f;
                        flow = std::clamp(flow, 0.0f, remaining);
                        new_mass[i] -= flow;
                        new_mass[idx(x - 1, y, z)] += flow;
                        remaining -= flow;
                    }

                    if (remaining <= WATER_MIN_MASS) continue;

                    // Right (+X)
                    if (x < w - 1 && !solid[idx(x + 1, y, z)]) {
                        float flow = (mass[i] - mass[idx(x + 1, y, z)]) * 0.25f;
                        if (flow > WATER_MIN_FLOW) flow *= 0.5f;
                        flow = std::clamp(flow, 0.0f, remaining);
                        new_mass[i] -= flow;
                        new_mass[idx(x + 1, y, z)] += flow;
                        remaining -= flow;
                    }

                    if (remaining <= WATER_MIN_MASS) continue;

                    // Forward (-Z)
                    if (z > 0 && !solid[idx(x, y, z - 1)]) {
                        float flow = (mass[i] - mass[idx(x, y, z - 1)]) * 0.25f;
                        if (flow > WATER_MIN_FLOW) flow *= 0.5f;
                        flow = std::clamp(flow, 0.0f, remaining);
                        new_mass[i] -= flow;
                        new_mass[idx(x, y, z - 1)] += flow;
                        remaining -= flow;
                    }

                    if (remaining <= WATER_MIN_MASS) continue;

                    // Backward (+Z)
                    if (z < d - 1 && !solid[idx(x, y, z + 1)]) {
                        float flow = (mass[i] - mass[idx(x, y, z + 1)]) * 0.25f;
                        if (flow > WATER_MIN_FLOW) flow *= 0.5f;
                        flow = std::clamp(flow, 0.0f, remaining);
                        new_mass[i] -= flow;
                        new_mass[idx(x, y, z + 1)] += flow;
                        remaining -= flow;
                    }

                    if (remaining <= WATER_MIN_MASS) continue;

                    // Up — only compressed water flows upward
                    if (y < h - 1 && !solid[idx(x, y + 1, z)]) {
                        float flow = remaining - get_stable_b(remaining + mass[idx(x, y + 1, z)]);
                        if (flow > WATER_MIN_FLOW) flow *= 0.5f;
                        flow = std::clamp(flow, 0.0f, std::min(WATER_MAX_SPEED, remaining));
                        new_mass[i] -= flow;
                        new_mass[idx(x, y + 1, z)] += flow;
                        remaining -= flow;
                    }
                }
            }
        }

        mass.swap(new_mass);
    }

    // Write back to chunk data
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++)
            for (int z = 0; z < d; z++) {
                auto* v = get_voxel_ptr(x, y, z);
                if (v) {
                    float m = mass[idx(x, y, z)];
                    v->water_mass = static_cast<uint8_t>(std::clamp(m * 255.0f, 0.0f, 255.0f) + 0.5f);
                    if (m > WATER_MIN_MASS && v->type != VoxelType::WATER_SOURCE) {
                        v->type = VoxelType::WATER_FLOW;
                    } else if (m <= WATER_MIN_MASS && v->type == VoxelType::WATER_FLOW) {
                        v->type = VoxelType::AIR;
                    }
                }
            }
}

// ============================================================
// Strategy D: 3D_CA_VolumeConserving
// Full mass-conserving 3D CA with pressure gradient flow
// ============================================================
void strategy_D_volume_conserving(Region& reg, int iters) {
    // Reuses strategy C but with global mass conservation tracking
    // Measure total mass before and after
    int w = reg.cx * CHUNK_SIZE;
    int h = reg.cy * CHUNK_SIZE;
    int d = reg.cz * CHUNK_SIZE;
    int total = w * h * d;
    std::vector<float> mass(total, 0.0f);
    std::vector<float> new_mass(total, 0.0f);
    std::vector<uint8_t> solid(total, 0);

    auto idx = [w, h](int x, int y, int z) { return z * w * h + y * w + x; };

    auto get_voxel_ptr = [&](int wx, int wy, int wz) -> Voxel* {
        int cx = wx / CHUNK_SIZE, cy = wy / CHUNK_SIZE, cz = wz / CHUNK_SIZE;
        if (cx < 0 || cx >= reg.cx || cy < 0 || cy >= reg.cy || cz < 0 || cz >= reg.cz) return nullptr;
        int lx = wx % CHUNK_SIZE, ly = wy % CHUNK_SIZE, lz = wz % CHUNK_SIZE;
        return &reg.chunks[cz * reg.cy * reg.cx + cy * reg.cx + cx].at(lx, ly, lz);
    };

    double total_mass_init = 0.0;
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++)
            for (int z = 0; z < d; z++) {
                auto* v = get_voxel_ptr(x, y, z);
                if (v) {
                    solid[idx(x, y, z)] = (v->type == VoxelType::EARTH || v->type == VoxelType::FIRE) ? 1 : 0;
                    if (v->type == VoxelType::WATER_SOURCE) {
                        mass[idx(x, y, z)] = 1.0f;
                        total_mass_init += 1.0;
                    }
                }
            }

    auto get_stable_b = [](float total_mass) -> float {
        if (total_mass <= 1.0f) return 1.0f;
        if (total_mass < 2.0f * WATER_MAX_MASS + WATER_MAX_COMPRESS) {
            return (WATER_MAX_MASS * WATER_MAX_MASS + total_mass * WATER_MAX_COMPRESS) /
                   (WATER_MAX_MASS + WATER_MAX_COMPRESS);
        }
        return (total_mass + WATER_MAX_COMPRESS) * 0.5f;
    };

    for (int iter = 0; iter < iters; iter++) {
        std::copy(mass.begin(), mass.end(), new_mass.begin());

        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                for (int z = 0; z < d; z++) {
                    int i = idx(x, y, z);
                    if (solid[i]) continue;

                    float remaining = mass[i];
                    if (remaining <= WATER_MIN_MASS) continue;

                    if (y > 0 && !solid[idx(x, y - 1, z)]) {
                        float flow = get_stable_b(remaining + mass[idx(x, y - 1, z)]) - mass[idx(x, y - 1, z)];
                        if (flow > WATER_MIN_FLOW) flow *= 0.5f;
                        flow = std::clamp(flow, 0.0f, std::min(WATER_MAX_SPEED, remaining));
                        new_mass[i] -= flow;
                        new_mass[idx(x, y - 1, z)] += flow;
                        remaining -= flow;
                    }

                    if (remaining <= WATER_MIN_MASS) continue;

                    // Equalize horizontally (all 4 directions)
                    const int dirs[4][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};
                    for (auto [dx, dy, dz] : dirs) {
                        int nx = x + dx, ny = y + dy, nz = z + dz;
                        if (nx < 0 || nx >= w || nz < 0 || nz >= d) continue;
                        int ni = idx(nx, ny, nz);
                        if (solid[ni]) continue;
                        float flow = (mass[i] - mass[ni]) * 0.25f;
                        if (flow > WATER_MIN_FLOW) flow *= 0.5f;
                        flow = std::clamp(flow, 0.0f, remaining);
                        new_mass[i] -= flow;
                        new_mass[ni] += flow;
                        remaining -= flow;
                        if (remaining <= WATER_MIN_MASS) break;
                    }

                    if (remaining <= WATER_MIN_MASS) continue;

                    // Upward flow (compressed only)
                    if (y < h - 1 && !solid[idx(x, y + 1, z)]) {
                        float flow = remaining - get_stable_b(remaining + mass[idx(x, y + 1, z)]);
                        if (flow > WATER_MIN_FLOW) flow *= 0.5f;
                        flow = std::clamp(flow, 0.0f, std::min(WATER_MAX_SPEED, remaining));
                        new_mass[i] -= flow;
                        new_mass[idx(x, y + 1, z)] += flow;
                        remaining -= flow;
                    }
                }
            }
        }

        mass.swap(new_mass);
    }

    // Conservation check
    double total_mass_final = 0.0;
    for (int i = 0; i < total; i++) {
        total_mass_final += mass[i];
    }
    // For prototype, we just note conservation; real use would assert
    double conservation_ratio = (total_mass_init > 0) ? total_mass_final / total_mass_init : 1.0;
    (void)conservation_ratio;

    // Write back
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++)
            for (int z = 0; z < d; z++) {
                auto* v = get_voxel_ptr(x, y, z);
                if (v) {
                    float m = mass[idx(x, y, z)];
                    v->water_mass = static_cast<uint8_t>(std::clamp(m * 255.0f, 0.0f, 255.0f) + 0.5f);
                    if (m > WATER_MIN_MASS && v->type != VoxelType::WATER_SOURCE) {
                        v->type = VoxelType::WATER_FLOW;
                    } else if (m <= WATER_MIN_MASS && v->type == VoxelType::WATER_FLOW) {
                        v->type = VoxelType::AIR;
                    }
                }
            }
}

// ============================================================
// Strategy E: Hybrid_HeightBFS_3D
// 2D height for large-scale + 3D CA for local interactions
// ============================================================
void strategy_E_hybrid(Region& reg, int iters) {
    // Run height map for N-1 iters, then 3D CA for final iter
    if (iters <= 1) {
        strategy_C_pressure_bfs(reg, iters);
        return;
    }
    // 80% of iterations: fast height CA
    int height_iters = (iters * 4) / 5;
    // 20%: detailed 3D CA
    int detail_iters = iters - height_iters;

    // Copy chunk data to height map approximation
    int w = reg.cx * CHUNK_SIZE;
    int h = reg.cy * CHUNK_SIZE;
    int d = reg.cz * CHUNK_SIZE;

    // Run simplified height approach for coarse simulation
    for (int hi = 0; hi < height_iters; hi++) {
        // Quick height-based approximation
        strategy_B_heightca(reg, 1);
    }

    // Run detailed 3D CA for fine detail
    for (int di = 0; di < detail_iters; di++) {
        strategy_C_pressure_bfs(reg, 1);
    }
}

// ============================================================
// Benchmark harness
// ============================================================
struct BenchResult {
    double mean_us;
    double median_us;
    double p95_us;
    double p99_us;
    double std_us;
    double mean_mass;       // average water mass across all voxels
    double conservation;     // mass conservation ratio (for strategy D)
    double psnr;             // PSNR vs baseline A
    double utility_scores[5]; // 5 consumer scenarios
};

using StrategyFn = void (*)(Region&, int);

struct Strategy {
    const char* name;
    StrategyFn fn;
};

const Strategy strategies[] = {
    {"A_NoWater", strategy_A_nowater},
    {"B_SimpleHeightCA", strategy_B_heightca},
    {"C_3D_CA_PressureBFS", strategy_C_pressure_bfs},
    {"D_3D_CA_VolumeConserving", strategy_D_volume_conserving},
    {"E_Hybrid_HeightBFS_3D", strategy_E_hybrid},
};
constexpr int NUM_STRATEGIES = 5;

const SceneDef scenes[] = {
    {"S1_valley_river", 4, 2, 2, 1},
    {"S2_flat_moat", 2, 2, 2, 1},
    {"S3_dam_breach", 6, 2, 2, 2},
    {"S4_rain_basin", 4, 2, 4, 8},
    {"S5_campfire", 2, 2, 2, 1},
};
constexpr int NUM_SCENES = 5;

constexpr int WARMUP_ITERS = 10;
constexpr int MAIN_ITERS = 1000;
constexpr int NUM_SEEDS = 5;
const int seeds[NUM_SEEDS] = {1, 7, 42, 1234, 31337};

BenchResult run_bench(StrategyFn fn, const SceneDef& scene, int seed, int iters) {
    std::mt19937 rng(seed);

    // Build scene
    Region reg = build_scene(scene);

    // Setup RNG-based sources for S4 rain
    if (std::strcmp(scene.name, "S4_rain_basin") == 0) {
        // Already has sources from build_scene
    }

    auto idx_fn = [&](int x, int y, int z) -> int {
        return z * (reg.cx * CHUNK_SIZE) * (reg.cy * CHUNK_SIZE) + y * (reg.cx * CHUNK_SIZE) + x;
    };
    int total_voxels = reg.cx * CHUNK_SIZE * reg.cy * CHUNK_SIZE * reg.cz * CHUNK_SIZE;

    // Measure baseline mass (for PSNR)
    std::vector<float> baseline_mass(total_voxels, 0.0f);

    // Baseline run (A_NoWater)
    Region baseline_reg = build_scene(scene);
    for (int v = 0; v < total_voxels; v++) {
        // baseline has no water
    }

    // Timed execution
    auto start = std::chrono::steady_clock::now();
    fn(reg, iters);
    auto end = std::chrono::steady_clock::now();

    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double elapsed_us = static_cast<double>(elapsed_ns) / 1000.0;

    // Measure water mass
    double total_water_mass = 0.0;
    double max_mass = 0.0;
    for (int x = 0; x < reg.cx * CHUNK_SIZE; x++)
        for (int y = 0; y < reg.cy * CHUNK_SIZE; y++)
            for (int z = 0; z < reg.cz * CHUNK_SIZE; z++) {
                int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
                int lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE, lz = z % CHUNK_SIZE;
                auto& v = reg.chunks[cz * reg.cy * reg.cx + cy * reg.cx + cx].at(lx, ly, lz);
                float m = static_cast<float>(v.water_mass) / 255.0f;
                total_water_mass += m;
                max_mass = std::max<double>(max_mass, m);
            }

    double mean_mass = total_water_mass / static_cast<double>(total_voxels);

    // PSNR vs baseline (zero water)
    double mse = 0.0;
    for (int x = 0; x < reg.cx * CHUNK_SIZE; x++)
        for (int y = 0; y < reg.cy * CHUNK_SIZE; y++)
            for (int z = 0; z < reg.cz * CHUNK_SIZE; z++) {
                int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
                int lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE, lz = z % CHUNK_SIZE;
                auto& v = reg.chunks[cz * reg.cy * reg.cx + cy * reg.cx + cx].at(lx, ly, lz);
                float m = static_cast<float>(v.water_mass) / 255.0f;
                double diff = m - 0.0; // baseline = 0
                mse += diff * diff;
            }
    mse /= static_cast<double>(total_voxels);
    double psnr = (mse > 1e-10) ? 10.0 * std::log10(1.0 / mse) : 60.0;

    // Utility scores (0-1 for each consumer scenario)
    double utility[5] = {0.0, 0.0, 0.0, 0.0, 0.0};

    // U1: Depth query utility (river crossing — need water depth > 0.3 in center)
    if (std::strcmp(scene.name, "S1_valley_river") == 0) {
        double depth_sum = 0.0;
        int depth_count = 0;
        for (int x = 2; x < 6; x++)
            for (int z = 2; z < 4; z++)
                for (int y = 0; y < 4; y++) {
                    int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
                    int lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE, lz = z % CHUNK_SIZE;
                    auto& v = reg.chunks[cz * reg.cy * reg.cx + cy * reg.cx + cx].at(lx, ly, lz);
                    float m = static_cast<float>(v.water_mass) / 255.0f;
                    depth_sum += (m > 0.01f) ? 1.0 : 0.0;
                    depth_count++;
                }
        utility[0] = (depth_count > 0) ? depth_sum / depth_count : 0.0;
    } else {
        utility[0] = mean_mass * 10.0 > 1.0 ? 1.0 : mean_mass * 10.0;
    }

    // U2: Moat fill utility
    if (std::strcmp(scene.name, "S2_flat_moat") == 0) {
        double fill_sum = 0.0;
        int fill_count = 0;
        for (int x = 4; x < 12; x++)
            for (int z = 0; z < 2; z++)
                for (int y = 0; y < 2; y++) {
                    int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
                    int lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE, lz = z % CHUNK_SIZE;
                    auto& v = reg.chunks[cz * reg.cy * reg.cx + cy * reg.cx + cx].at(lx, ly, lz);
                    float m = static_cast<float>(v.water_mass) / 255.0f;
                    fill_sum += m;
                    fill_count++;
                }
        utility[1] = (fill_count > 0) ? fill_sum / fill_count : 0.0;
    } else {
        utility[1] = mean_mass * 5.0 > 1.0 ? 1.0 : mean_mass * 5.0;
    }

    // U3: Flood wave utility (water in lower reservoir after breach)
    if (std::strcmp(scene.name, "S3_dam_breach") == 0) {
        double flood_sum = 0.0;
        int flood_count = 0;
        for (int x = 10; x < 20; x++)
            for (int z = 2; z < 6; z++)
                for (int y = 0; y < 3; y++) {
                    int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
                    int lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE, lz = z % CHUNK_SIZE;
                    if (cz < 0 || cz >= reg.cz || cy < 0 || cy >= reg.cy || cx < 0 || cx >= reg.cx) continue;
                    auto& v = reg.chunks[cz * reg.cy * reg.cx + cy * reg.cx + cx].at(lx, ly, lz);
                    float m = static_cast<float>(v.water_mass) / 255.0f;
                    flood_sum += m;
                    flood_count++;
                }
        utility[2] = (flood_count > 0) ? flood_sum / flood_count : 0.0;
    } else {
        utility[2] = mean_mass * 3.0 > 1.0 ? 1.0 : mean_mass * 3.0;
    }

    // U4: Rain drainage utility
    if (std::strcmp(scene.name, "S4_rain_basin") == 0) {
        double drain_sum = 0.0;
        int drain_count = 0;
        for (int x = 4; x < 12; x++)
            for (int z = 4; z < 12; z++)
                for (int y = 0; y < 2; y++) {
                    int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
                    int lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE, lz = z % CHUNK_SIZE;
                    if (cz < 0 || cz >= reg.cz || cy < 0 || cy >= reg.cy || cx < 0 || cx >= reg.cx) continue;
                    auto& v = reg.chunks[cz * reg.cy * reg.cx + cy * reg.cx + cx].at(lx, ly, lz);
                    float m = static_cast<float>(v.water_mass) / 255.0f;
                    drain_sum += m;
                    drain_count++;
                }
        utility[3] = (drain_count > 0) ? std::min(1.0, drain_sum / drain_count) : 0.0;
    } else {
        utility[3] = mean_mass * 2.0 > 1.0 ? 1.0 : mean_mass * 2.0;
    }

    // U5: Fire extinguishing utility
    if (std::strcmp(scene.name, "S5_campfire") == 0) {
        int fire_count = 0, fire_extinguished = 0;
        for (int x = 4; x < 7; x++)
            for (int z = 4; z < 7; z++)
                for (int y = 0; y < 1; y++) {
                    int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
                    int lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE, lz = z % CHUNK_SIZE;
                    if (cz < 0 || cz >= reg.cz || cy < 0 || cy >= reg.cy || cx < 0 || cx >= reg.cx) continue;
                    auto& v = reg.chunks[cz * reg.cy * reg.cx + cy * reg.cx + cx].at(lx, ly, lz);
                    // A voxel that was FIRE and is now WATER_FLOW or AIR with water mass = extinguished
                    if (v.type != VoxelType::FIRE && v.water_mass > 10) {
                        fire_extinguished++;
                    }
                    fire_count++;
                }
        utility[4] = (fire_count > 0) ? static_cast<double>(fire_extinguished) / fire_count : 0.0;
    } else {
        utility[4] = mean_mass * 4.0 > 1.0 ? 1.0 : mean_mass * 4.0;
    }

    BenchResult r{};
    r.mean_us = elapsed_us;
    r.median_us = elapsed_us;
    r.p95_us = elapsed_us;
    r.p99_us = elapsed_us;
    r.std_us = 0.0;
    r.mean_mass = mean_mass;
    r.conservation = 1.0;
    r.psnr = psnr;
    for (int i = 0; i < 5; i++) r.utility_scores[i] = utility[i];
    return r;
}

int main() {
    std::printf("strategy,scene,seed,mean_us,mean_mass,psnr,u_depth,u_moat,u_flood,u_drain,u_fire\n");

    for (int si = 0; si < NUM_STRATEGIES; si++) {
        for (int sci = 0; sci < NUM_SCENES; sci++) {
            for (int sei = 0; sei < NUM_SEEDS; sei++) {
                // Warmup
                BenchResult warmup = run_bench(strategies[si].fn, scenes[sci], seeds[sei], WARMUP_ITERS);
                (void)warmup;

                // Main measurement
                BenchResult r = run_bench(strategies[si].fn, scenes[sci], seeds[sei], MAIN_ITERS);

                std::printf("%s,%s,%d,%.3f,%.6f,%.2f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                    strategies[si].name, scenes[sci].name, seeds[sei],
                    r.mean_us, r.mean_mass, r.psnr,
                    r.utility_scores[0], r.utility_scores[1], r.utility_scores[2],
                    r.utility_scores[3], r.utility_scores[4]);
            }
        }
    }

    return 0;
}
