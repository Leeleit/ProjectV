#pragma once

#include "scenes.hpp"
#include "sdf_overlay.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace sdf_hybrid::vct {

// 3D vector (float).
struct Vec3 {
    float x, y, z;
};

// VCT termination strategies.
enum class TermStrategy : std::uint8_t {
    T_VoxelDiscrete = 0,  // current mainline: binary voxel termination
    T_SDFSmooth,           // SDF distance-based smooth termination
    T_Hybrid,              // SDF for near, voxel for far
};

constexpr const char* term_name(TermStrategy t) noexcept {
    switch (t) {
        case TermStrategy::T_VoxelDiscrete: return "T_VoxelDiscrete";
        case TermStrategy::T_SDFSmooth:     return "T_SDFSmooth";
        case TermStrategy::T_Hybrid:        return "T_Hybrid";
    }
    return "unknown";
}

// Result of a single cone-march.
struct ConeHit {
    bool    hit;          // true = ray hit something
    float   distance;     // distance to hit (or max_dist if no hit)
    int     steps;        // number of raymarch steps
    float   normal_x;     // normal at hit (or 0,0,0 if no hit)
    float   normal_y;
    float   normal_z;
};

// Per-fragment result (aggregated over N cones).
struct FragmentResult {
    int     hit_count;        // number of cones that hit something
    float   mean_distance;    // mean distance to hit (only for hit cones)
    int     total_steps;      // total steps across all cones
};

// Cone-march for a single ray, with given strategy.
// Voxels-only: ignores SDF (for T_VoxelDiscrete).
ConeHit march_voxel(
    const scenes::Chunk& voxels,
    Vec3 origin, Vec3 direction,
    float max_dist
) noexcept;

// Cone-march using only SDF (for T_SDFSmooth).
ConeHit march_sdf(
    const sdf::SdfR8& sdf,
    Vec3 origin, Vec3 direction,
    float max_dist
) noexcept;

// Hybrid: SDF for first 2 voxel-distances, then voxel DDA.
ConeHit march_hybrid(
    const scenes::Chunk& voxels,
    const sdf::SdfR8& sdf,
    Vec3 origin, Vec3 direction,
    float max_dist
) noexcept;

// Dispatcher.
ConeHit march(
    TermStrategy strategy,
    const scenes::Chunk& voxels,
    const sdf::SdfR8& sdf,
    Vec3 origin, Vec3 direction,
    float max_dist
) noexcept;

// Aggregate N cone-marches (Fibonacci sphere) for a fragment.
// N=6 = projectV default per closed vct-cone-count-atlas-precision verdict (6×R16F sweet spot).
// N=1024 = brute-force reference for PSNR.
// origin = cone origin (default chunk center; pass air voxel for valid measurements).
FragmentResult aggregate_cones(
    TermStrategy strategy,
    const scenes::Chunk& voxels,
    const sdf::SdfR8& sdf,
    Vec3 fragment_normal,
    int num_cones,
    float max_dist,
    Vec3 origin = {4.0f, 4.0f, 4.0f}
) noexcept;

// Compute irradiance proxy = mean hit_count / num_cones (fraction of visible hemisphere).
// Used as "ground truth" for PSNR measurement.
float irradiance_proxy(const FragmentResult& fr, int num_cones) noexcept;

// Compute PSNR between measured fragment and reference fragment.
float psnr(float measured, float reference) noexcept;

}  // namespace sdf_hybrid::vct
