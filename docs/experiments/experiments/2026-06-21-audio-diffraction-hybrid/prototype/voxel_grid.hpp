#pragma once
//
// voxel_grid.hpp — synthetic voxel scene representation for audio diffraction prototype
//
// Standalone C++26 CPU prototype, no GPU deps, no ProjectV mainline dependency.
// Pattern: cube-based bool grid (simpler than ProjectV SVDAG-on-64-tree, representative enough for
// audio propagation measurements).
//
// 3 scenes: cave_stress (tight rooms + corridors), open_plains (minimal occlusion, few edges),
// multi_room (typical Stage 4.3 gameplay area, multiple edges).
//
// Reference: closed `experiments/2026-06-21-audio-raytracing-voxel-sdf/prototype/voxel_grid.hpp`
// (similar pattern, simpler because no SVO/NanoVDB complexity).
//

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace audio_diffraction {

using Vec3 = std::array<double, 3>;

struct AABB {
    Vec3 min{};
    Vec3 max{};

    [[nodiscard]] constexpr AABB() noexcept = default;
    [[nodiscard]] constexpr AABB(const Vec3& lo, const Vec3& hi) noexcept : min(lo), max(hi) {}

    [[nodiscard]] constexpr Vec3 center() const noexcept {
        return Vec3{0.5 * (min[0] + max[0]), 0.5 * (min[1] + max[1]), 0.5 * (min[2] + max[2])};
    }

    [[nodiscard]] constexpr Vec3 extents() const noexcept {
        return Vec3{max[0] - min[0], max[1] - min[1], max[2] - min[2]};
    }
};

struct Ray {
    Vec3 origin{};
    Vec3 direction{};  // unit vector
};

struct EdgeProbe {
    Vec3 edge_point{};
    Vec3 edge_normal{};
    Vec3 edge_tangent{};  // edge direction (perpendicular to normal)
    double length{};
};

class VoxelGrid {
public:
    static constexpr int kDim = 64;
    static constexpr double kCellSize = 1.0;  // 1 m per voxel
    static constexpr int kTotalCells = kDim * kDim * kDim;

    VoxelGrid() : solid_(static_cast<size_t>(kTotalCells), false) {}

    [[nodiscard]] bool is_solid(int x, int y, int z) const noexcept {
        if (x < 0 || y < 0 || z < 0 || x >= kDim || y >= kDim || z >= kDim) return true;
        return solid_[idx(x, y, z)];
    }

    void set_solid(int x, int y, int z, bool v) noexcept { solid_[idx(x, y, z)] = v; }

    [[nodiscard]] AABB aabb() const noexcept {
        return AABB{Vec3{0, 0, 0}, Vec3{static_cast<double>(kDim), static_cast<double>(kDim),
                                         static_cast<double>(kDim)}};
    }

    // DDA voxel traversal (Amanatides-Woo 1987) — returns first hit distance, or -1 if no hit.
    [[nodiscard]] double ray_distance(const Ray& ray, double max_distance = 1e6) const noexcept {
        // Convert world coords to voxel coords.
        double ox = ray.origin[0];
        double oy = ray.origin[1];
        double oz = ray.origin[2];
        const double dx = ray.direction[0];
        const double dy = ray.direction[1];
        const double dz = ray.direction[2];

        int ix = static_cast<int>(std::floor(ox));
        int iy = static_cast<int>(std::floor(oy));
        int iz = static_cast<int>(std::floor(oz));

        if (ix < 0 || iy < 0 || iz < 0 || ix >= kDim || iy >= kDim || iz >= kDim) {
            // Outside grid: skip; for prototype assume origin inside grid
            return -1.0;
        }

        const int step_x = (dx > 0) ? 1 : -1;
        const int step_y = (dy > 0) ? 1 : -1;
        const int step_z = (dz > 0) ? 1 : -1;

        const double t_delta_x = std::abs(1.0 / dx);
        const double t_delta_y = std::abs(1.0 / dy);
        const double t_delta_z = std::abs(1.0 / dz);

        double t_max_x = ((dx > 0) ? (ix + 1.0 - ox) : (ox - ix)) * t_delta_x;
        double t_max_y = ((dy > 0) ? (iy + 1.0 - oy) : (oy - iy)) * t_delta_y;
        double t_max_z = ((dz > 0) ? (iz + 1.0 - oz) : (oz - iz)) * t_delta_z;

        double t = 0.0;
        while (t <= max_distance) {
            if (is_solid(ix, iy, iz)) {
                return t;
            }
            if (t_max_x < t_max_y) {
                if (t_max_x < t_max_z) {
                    ix += step_x;
                    t = t_max_x;
                    t_max_x += t_delta_x;
                } else {
                    iz += step_z;
                    t = t_max_z;
                    t_max_z += t_delta_z;
                }
            } else {
                if (t_max_y < t_max_z) {
                    iy += step_y;
                    t = t_max_y;
                    t_max_y += t_delta_y;
                } else {
                    iz += step_z;
                    t = t_max_z;
                    t_max_z += t_delta_z;
                }
            }
            if (ix < 0 || iy < 0 || iz < 0 || ix >= kDim || iy >= kDim || iz >= kDim) {
                return -1.0;
            }
        }
        return -1.0;
    }

    // Find edges in voxel grid: 3 axis-aligned edge orientations per cell, 6 candidate faces.
    // Returns list of edge segments where surface normal transitions (silhouette edges).
    [[nodiscard]] std::vector<EdgeProbe> find_edges() const noexcept {
        std::vector<EdgeProbe> edges;
        edges.reserve(2048);
        for (int z = 0; z < kDim; ++z) {
            for (int y = 0; y < kDim; ++y) {
                for (int x = 0; x < kDim; ++x) {
                    if (!solid_[idx(x, y, z)]) continue;
                    // X-aligned edges (top, bottom in Z; check Y boundary)
                    if (y == 0 || !solid_[idx(x, y - 1, z)]) {
                        // top edge of (x, y, z) in -Y direction
                        edges.push_back(EdgeProbe{
                            Vec3{static_cast<double>(x) + 0.5, static_cast<double>(y),
                                 static_cast<double>(z) + 0.5},
                            Vec3{0, -1, 0},
                            Vec3{1, 0, 0},
                            1.0,
                        });
                    }
                    if (y == kDim - 1 || !solid_[idx(x, y + 1, z)]) {
                        // bottom edge in +Y direction
                        edges.push_back(EdgeProbe{
                            Vec3{static_cast<double>(x) + 0.5, static_cast<double>(y) + 1.0,
                                 static_cast<double>(z) + 0.5},
                            Vec3{0, 1, 0},
                            Vec3{1, 0, 0},
                            1.0,
                        });
                    }
                    // Z-aligned edges
                    if (z == 0 || !solid_[idx(x, y, z - 1)]) {
                        edges.push_back(EdgeProbe{
                            Vec3{static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5,
                                 static_cast<double>(z)},
                            Vec3{0, 0, -1},
                            Vec3{1, 0, 0},
                            1.0,
                        });
                    }
                    if (z == kDim - 1 || !solid_[idx(x, y, z + 1)]) {
                        edges.push_back(EdgeProbe{
                            Vec3{static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5,
                                 static_cast<double>(z) + 1.0},
                            Vec3{0, 0, 1},
                            Vec3{1, 0, 0},
                            1.0,
                        });
                    }
                }
            }
        }
        return edges;
    }

    // Precompute a depth-mip chain for Tsingos-style sampling. 1 mip per octave (mip 0 = 1x1, mip
    // 1 = 2x2, ..., mip 5 = 32x32). For each mip, store min-distance to nearest solid voxel.
    // We use 2D cube faces for simplicity (6 faces, each 64x64 → mip down to 8x8 = 3 mip levels).
    struct DepthMip {
        int mip_level{};
        int size{};
        std::vector<double> face_x_pos;  // +X face, 64x64 → min
        std::vector<double> face_x_neg;  // -X face
        std::vector<double> face_y_pos;
        std::vector<double> face_y_neg;
        std::vector<double> face_z_pos;
        std::vector<double> face_z_neg;
    };

    [[nodiscard]] std::vector<DepthMip> build_depth_mips() const noexcept {
        std::vector<DepthMip> chain;
        for (int mip = 0; mip <= 3; ++mip) {
            DepthMip d{};
            d.mip_level = mip;
            d.size = kDim >> mip;
            const int n = d.size;
            const int total = n * n;
            d.face_x_pos.assign(total, 1e6);
            d.face_x_neg.assign(total, 1e6);
            d.face_y_pos.assign(total, 1e6);
            d.face_y_neg.assign(total, 1e6);
            d.face_z_pos.assign(total, 1e6);
            d.face_z_neg.assign(total, 1e6);
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    // +X face: ray from (kDim-1, y<<mip + (1<<mip)/2, z<<mip + (1<<mip)/2) into -X
                    int cx = kDim - 1;
                    int cy = (y << mip) + (1 << mip) / 2;
                    int cz = (x << mip) + (1 << mip) / 2;
                    if (cy < kDim && cz < kDim && !is_solid(cx, cy, cz)) {
                        Ray r{Vec3{static_cast<double>(cx) + 0.5, static_cast<double>(cy) + 0.5,
                                   static_cast<double>(cz) + 0.5},
                             Vec3{-1, 0, 0}};
                        double d_ = ray_distance(r, kDim);
                        d.face_x_pos[y * n + x] = (d_ < 0) ? 1e6 : d_;
                    }
                    // ... similar for other faces (omitted for prototype brevity, use +X as
                    // canonical face for measurement)
                }
            }
            chain.push_back(std::move(d));
        }
        return chain;
    }

    // Scene generators (3 scenes per closed `audio-raytracing-voxel-sdf`).
    [[nodiscard]] static VoxelGrid cave_stress(uint32_t seed) noexcept {
        VoxelGrid v;
        // Base solid: walls + floor + ceiling
        for (int z = 0; z < kDim; ++z) {
            for (int y = 0; y < kDim; ++y) {
                for (int x = 0; x < kDim; ++x) {
                    bool s = (x < 2 || x >= kDim - 2 || y < 2 || y >= kDim - 2 || z < 2 ||
                              z >= kDim - 2);
                    v.set_solid(x, y, z, s);
                }
            }
        }
        // Carve corridors
        for (int y = 5; y < kDim - 5; ++y) {
            for (int z = 5; z < kDim - 5; ++z) {
                v.set_solid(kDim / 2, y, z, false);
            }
        }
        for (int x = 5; x < kDim - 5; ++x) {
            for (int z = 5; z < kDim - 5; ++z) {
                v.set_solid(x, kDim / 2, z, false);
            }
        }
        // Add pillars
        for (int p = 0; p < 8; ++p) {
            int px = 8 + (p * 7) % (kDim - 16);
            int pz = 8 + (p * 11) % (kDim - 16);
            for (int y = 0; y < kDim; ++y) {
                v.set_solid(px, y, pz, true);
            }
        }
        (void)seed;
        return v;
    }

    [[nodiscard]] static VoxelGrid open_plains(uint32_t seed) noexcept {
        VoxelGrid v;
        // Sparse pillars only
        for (int p = 0; p < 16; ++p) {
            int px = 4 + (p * 5 + seed * 3) % (kDim - 8);
            int pz = 4 + (p * 9 + seed * 7) % (kDim - 8);
            for (int y = 0; y < 8; ++y) {
                v.set_solid(px, y, pz, true);
            }
        }
        // Floor only
        for (int z = 0; z < kDim; ++z) {
            for (int x = 0; x < kDim; ++x) {
                v.set_solid(x, 0, z, true);
            }
        }
        return v;
    }

    [[nodiscard]] static VoxelGrid multi_room(uint32_t seed) noexcept {
        VoxelGrid v;
        // Walls forming rooms
        for (int y = 0; y < kDim; ++y) {
            for (int z = 0; z < kDim; ++z) {
                v.set_solid(kDim / 3, y, z, true);
                v.set_solid(2 * kDim / 3, y, z, true);
            }
        }
        // Doorways (carve openings in walls)
        for (int x_offset : std::vector<int>{kDim / 3, 2 * kDim / 3}) {
            for (int door = 0; door < 3; ++door) {
                int z_pos = (kDim / 4) * door + kDim / 8 + (seed % 5);
                for (int y = 4; y < 8; ++y) {
                    v.set_solid(x_offset, y, z_pos, false);
                }
            }
        }
        // Floor + ceiling
        for (int z = 0; z < kDim; ++z) {
            for (int x = 0; x < kDim; ++x) {
                v.set_solid(x, 0, z, true);
                v.set_solid(x, kDim - 1, z, true);
            }
        }
        return v;
    }

private:
    [[nodiscard]] static constexpr int idx(int x, int y, int z) noexcept {
        return x + y * kDim + z * kDim * kDim;
    }
    std::vector<bool> solid_;
};

}  // namespace audio_diffraction
