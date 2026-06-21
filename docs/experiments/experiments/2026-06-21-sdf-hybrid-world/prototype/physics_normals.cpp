#include "physics_normals.hpp"

#include <algorithm>
#include <cmath>

namespace sdf_hybrid::physics {

namespace {

// Reuse trilinear SDF lookup from vct_cone_march.
float sdf_lookup(const sdf::SdfR8& sdf, float fx, float fy, float fz) noexcept {
    float cx = std::clamp(fx, 0.0f, static_cast<float>(sdf::CHUNK_SIZE) - 1.0f);
    float cy = std::clamp(fy, 0.0f, static_cast<float>(sdf::CHUNK_SIZE) - 1.0f);
    float cz = std::clamp(fz, 0.0f, static_cast<float>(sdf::CHUNK_SIZE) - 1.0f);
    int x0 = static_cast<int>(cx);
    int y0 = static_cast<int>(cy);
    int z0 = static_cast<int>(cz);
    int x1 = std::min(x0 + 1, static_cast<int>(sdf::CHUNK_SIZE) - 1);
    int y1 = std::min(y0 + 1, static_cast<int>(sdf::CHUNK_SIZE) - 1);
    int z1 = std::min(z0 + 1, static_cast<int>(sdf::CHUNK_SIZE) - 1);
    float tx = cx - x0;
    float ty = cy - y0;
    float tz = cz - z0;

    auto val = [&](int x, int y, int z) {
        std::uint8_t s = sdf[scenes::idx3(x, y, z)];
        return static_cast<float>(sdf::sdf_value(s));
    };

    float v000 = val(x0, y0, z0), v100 = val(x1, y0, z0);
    float v010 = val(x0, y1, z0), v110 = val(x1, y1, z0);
    float v001 = val(x0, y0, z1), v101 = val(x1, y0, z1);
    float v011 = val(x0, y1, z1), v111 = val(x1, y1, z1);

    float v00 = v000 * (1 - tx) + v100 * tx;
    float v10 = v010 * (1 - tx) + v110 * tx;
    float v01 = v001 * (1 - tx) + v101 * tx;
    float v11 = v011 * (1 - tx) + v111 * tx;
    float v0 = v00 * (1 - ty) + v10 * ty;
    float v1 = v01 * (1 - ty) + v11 * ty;
    return v0 * (1 - tz) + v1 * tz;
}

}  // namespace

vct::Vec3 normal_voxel(
    const scenes::Chunk& voxels,
    int hit_x, int hit_y, int hit_z
) noexcept {
    vct::Vec3 n{0, 1, 0};  // default up
    // Find first empty 6-neighbor → that face is the contact face.
    auto vox = [&](int x, int y, int z) -> std::uint8_t {
        if (x < 0 || y < 0 || z < 0) return 0;
        if (x >= static_cast<int>(scenes::CHUNK_SIZE) ||
            y >= static_cast<int>(scenes::CHUNK_SIZE) ||
            z >= static_cast<int>(scenes::CHUNK_SIZE)) return 0;
        return voxels[scenes::idx3(x, y, z)];
    };
    if (vox(hit_x - 1, hit_y, hit_z) == 0) n = {-1, 0, 0};
    else if (vox(hit_x + 1, hit_y, hit_z) == 0) n = {+1, 0, 0};
    else if (vox(hit_x, hit_y - 1, hit_z) == 0) n = {0, -1, 0};
    else if (vox(hit_x, hit_y + 1, hit_z) == 0) n = {0, +1, 0};
    else if (vox(hit_x, hit_y, hit_z - 1) == 0) n = {0, 0, -1};
    else if (vox(hit_x, hit_y, hit_z + 1) == 0) n = {0, 0, +1};
    return n;
}

vct::Vec3 normal_sdf(
    const sdf::SdfR8& sdf,
    float hit_x, float hit_y, float hit_z
) noexcept {
    // SDF gradient via central differences.
    float h = 0.1f;
    float dx = sdf_lookup(sdf, hit_x + h, hit_y, hit_z) -
               sdf_lookup(sdf, hit_x - h, hit_y, hit_z);
    float dy = sdf_lookup(sdf, hit_x, hit_y + h, hit_z) -
               sdf_lookup(sdf, hit_x, hit_y - h, hit_z);
    float dz = sdf_lookup(sdf, hit_x, hit_y, hit_z + h) -
               sdf_lookup(sdf, hit_x, hit_y, hit_z - h);
    float nlen = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (nlen < 1e-6f) return vct::Vec3{0, 1, 0};
    return vct::Vec3{dx / nlen, dy / nlen, dz / nlen};
}

float angular_error(vct::Vec3 a, vct::Vec3 b) noexcept {
    float dot = a.x * b.x + a.y * b.y + a.z * b.z;
    dot = std::clamp(dot, -1.0f, 1.0f);
    return std::acos(dot) * 180.0f / 3.14159265f;
}

ContactResult measure_contact(
    const scenes::Chunk& voxels,
    const sdf::SdfR8& sdf,
    ContactPoint cp
) noexcept {
    // The sphere center is at the contact point. The hit voxel is the nearest solid voxel
    // along the inward direction (-expected_normal), then offset by 0.5 voxel into the surface.
    int hit_x = static_cast<int>(cp.x - cp.expected_normal.x * 0.5f);
    int hit_y = static_cast<int>(cp.y - cp.expected_normal.y * 0.5f);
    int hit_z = static_cast<int>(cp.z - cp.expected_normal.z * 0.5f);
    // For contacts outside the chunk (-0.5 etc), the nearest solid voxel is the boundary voxel.
    hit_x = std::clamp(hit_x, 0, static_cast<int>(scenes::CHUNK_SIZE) - 1);
    hit_y = std::clamp(hit_y, 0, static_cast<int>(scenes::CHUNK_SIZE) - 1);
    hit_z = std::clamp(hit_z, 0, static_cast<int>(scenes::CHUNK_SIZE) - 1);

    vct::Vec3 v_normal = normal_voxel(voxels, hit_x, hit_y, hit_z);
    vct::Vec3 s_normal = normal_sdf(sdf, cp.x, cp.y, cp.z);

    return ContactResult{
        angular_error(v_normal, cp.expected_normal),
        angular_error(s_normal, cp.expected_normal),
    };
}

}  // namespace sdf_hybrid::physics
