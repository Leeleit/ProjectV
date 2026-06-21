#pragma once

#include "scenes.hpp"
#include "sdf_overlay.hpp"
#include "vct_cone_march.hpp"

namespace sdf_hybrid::physics {

// Compute collision normal at voxel hit (face normal — step function).
vct::Vec3 normal_voxel(
    const scenes::Chunk& voxels,
    int hit_x, int hit_y, int hit_z
) noexcept;

// Compute collision normal via SDF gradient (smooth, C¹ continuous).
vct::Vec3 normal_sdf(
    const sdf::SdfR8& sdf,
    float hit_x, float hit_y, float hit_z
) noexcept;

// Angular error between two normals (in degrees).
float angular_error(vct::Vec3 a, vct::Vec3 b) noexcept;

// Test point: known contact location (sphere touching surface).
struct ContactPoint {
    float x, y, z;          // sphere center
    vct::Vec3 expected_normal;  // expected smooth normal at contact
};

// 8 representative contact points: 6 face centers + 2 edge midpoints.
// Sphere center is positioned at chunk face boundary (sphere radius 0.5).
// For chunkSize=8, faces are at x=0, x=8, y=0, y=8, z=0, z=8.
inline constexpr std::array<ContactPoint, 8> STANDARD_CONTACTS = {{
    ContactPoint{-0.5f, 4.0f, 4.0f, vct::Vec3{-1.0f, 0.0f, 0.0f}},    // -X face (outside)
    ContactPoint{8.5f, 4.0f, 4.0f, vct::Vec3{1.0f, 0.0f, 0.0f}},     // +X face
    ContactPoint{4.0f, -0.5f, 4.0f, vct::Vec3{0.0f, -1.0f, 0.0f}},    // -Y face
    ContactPoint{4.0f, 8.5f, 4.0f, vct::Vec3{0.0f, 1.0f, 0.0f}},     // +Y face
    ContactPoint{4.0f, 4.0f, -0.5f, vct::Vec3{0.0f, 0.0f, -1.0f}},   // -Z face
    ContactPoint{4.0f, 4.0f, 8.5f, vct::Vec3{0.0f, 0.0f, 1.0f}},     // +Z face
    ContactPoint{-0.5f, -0.5f, 4.0f, vct::Vec3{-0.7071f, -0.7071f, 0.0f}},   // edge -X -Y
    ContactPoint{-0.5f, 4.0f, -0.5f, vct::Vec3{-0.7071f, 0.0f, -0.7071f}},   // edge -X -Z
}};

// Per contact point: compute voxel normal + SDF normal, measure angular error.
struct ContactResult {
    float voxel_angular_error;  // degrees
    float sdf_angular_error;    // degrees
};

ContactResult measure_contact(
    const scenes::Chunk& voxels,
    const sdf::SdfR8& sdf,
    ContactPoint cp
) noexcept;

}  // namespace sdf_hybrid::physics
