#include "vct_cone_march.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace sdf_hybrid::vct {

namespace {

inline Vec3 make_vec3(float x, float y, float z) noexcept {
    return Vec3{x, y, z};
}

inline Vec3 vec_normalize(Vec3 v) noexcept {
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-6f) return make_vec3(0.0f, 1.0f, 0.0f);
    return make_vec3(v.x / len, v.y / len, v.z / len);
}

inline Vec3 vec_scale(Vec3 v, float s) noexcept {
    return make_vec3(v.x * s, v.y * s, v.z * s);
}

inline Vec3 vec_add(Vec3 a, Vec3 b) noexcept {
    return make_vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline float vec_dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Trilinear SDF lookup (continuous position in [0, CHUNK_SIZE)).
// Returns signed distance (positive = outside, negative = inside).
float sdf_lookup_trilinear(
    const sdf::SdfR8& sdf,
    float fx, float fy, float fz
) noexcept {
    // Clamp to [0, CHUNK_SIZE-1].
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

// Nearest voxel lookup (returns material id or 0 for out-of-bounds).
inline std::uint8_t voxel_at(const scenes::Chunk& voxels, int x, int y, int z) noexcept {
    if (x < 0 || y < 0 || z < 0) return 0;
    if (x >= static_cast<int>(scenes::CHUNK_SIZE) ||
        y >= static_cast<int>(scenes::CHUNK_SIZE) ||
        z >= static_cast<int>(scenes::CHUNK_SIZE)) return 0;
    return voxels[scenes::idx3(x, y, z)];
}

// Fibonacci sphere direction generator.
Vec3 fibonacci_dir(int i, int n) noexcept {
    float phi = std::acos(1.0f - 2.0f * (i + 0.5f) / n);
    float theta = std::acos(1.0f - 2.0f * (i + 0.5f) / n);  // placeholder
    theta = 3.14159265f * (1.0f + std::sqrt(5.0f)) * (i + 0.5f);
    float x = std::cos(theta) * std::sin(phi);
    float y = std::sin(theta) * std::sin(phi);
    float z = std::cos(phi);
    return make_vec3(x, y, z);
}

}  // namespace

// ============================================================================
// T_VoxelDiscrete: DDA through voxel grid, terminate at first solid voxel.
// Reference: classic voxel DDA, projectV current behavior.
// ============================================================================
ConeHit march_voxel(
    const scenes::Chunk& voxels,
    Vec3 origin, Vec3 direction,
    float max_dist
) noexcept {
    ConeHit hit{};
    hit.hit = false;
    hit.distance = max_dist;
    hit.normal_x = 0; hit.normal_y = 0; hit.normal_z = 0;
    hit.steps = 0;

    direction = vec_normalize(direction);
    float t = 0.0f;
    const float step = 0.25f;  // 1/4 voxel steps for accuracy.

    while (t < max_dist) {
        Vec3 p = vec_add(origin, vec_scale(direction, t));
        int x = static_cast<int>(p.x);
        int y = static_cast<int>(p.y);
        int z = static_cast<int>(p.z);
        hit.steps++;
        if (x < 0 || y < 0 || z < 0 ||
            x >= static_cast<int>(scenes::CHUNK_SIZE) ||
            y >= static_cast<int>(scenes::CHUNK_SIZE) ||
            z >= static_cast<int>(scenes::CHUNK_SIZE)) {
            t += step;
            continue;
        }
        if (voxels[scenes::idx3(x, y, z)] != 0) {
            hit.hit = true;
            hit.distance = t;
            // Approximate normal from which face was hit (use 6-neighbor check).
            float nx = 0, ny = 0, nz = 0;
            if (voxel_at(voxels, x - 1, y, z) == 0) nx = -1.0f;
            else if (voxel_at(voxels, x + 1, y, z) == 0) nx = +1.0f;
            if (voxel_at(voxels, x, y - 1, z) == 0) ny = -1.0f;
            else if (voxel_at(voxels, x, y + 1, z) == 0) ny = +1.0f;
            if (voxel_at(voxels, x, y, z - 1) == 0) nz = -1.0f;
            else if (voxel_at(voxels, x, y, z + 1) == 0) nz = +1.0f;
            hit.normal_x = nx; hit.normal_y = ny; hit.normal_z = nz;
            return hit;
        }
        t += step;
    }
    return hit;
}

// ============================================================================
// T_SDFSmooth: SDF distance-based smooth termination.
// Sphere tracing: take step = max(SDF value, epsilon) per iteration.
// Reference: Quilez 2008, RTSDF 2022.
// ============================================================================
ConeHit march_sdf(
    const sdf::SdfR8& sdf,
    Vec3 origin, Vec3 direction,
    float max_dist
) noexcept {
    ConeHit hit{};
    hit.hit = false;
    hit.distance = max_dist;
    hit.normal_x = 0; hit.normal_y = 0; hit.normal_z = 0;
    hit.steps = 0;

    direction = vec_normalize(direction);
    float t = 0.0f;
    const float epsilon = 0.05f;

    while (t < max_dist && hit.steps < 256) {
        Vec3 p = vec_add(origin, vec_scale(direction, t));
        float d = sdf_lookup_trilinear(sdf, p.x, p.y, p.z);
        hit.steps++;
        if (d <= epsilon) {
            // Inside surface.
            hit.hit = true;
            hit.distance = t;
            // Normal from SDF gradient (finite differences).
            float h = 0.1f;
            float dx = sdf_lookup_trilinear(sdf, p.x + h, p.y, p.z) -
                       sdf_lookup_trilinear(sdf, p.x - h, p.y, p.z);
            float dy = sdf_lookup_trilinear(sdf, p.x, p.y + h, p.z) -
                       sdf_lookup_trilinear(sdf, p.x, p.y - h, p.z);
            float dz = sdf_lookup_trilinear(sdf, p.x, p.y, p.z + h) -
                       sdf_lookup_trilinear(sdf, p.x, p.y, p.z - h);
            float nlen = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (nlen > 1e-6f) {
                hit.normal_x = dx / nlen;
                hit.normal_y = dy / nlen;
                hit.normal_z = dz / nlen;
            } else {
                hit.normal_x = 0; hit.normal_y = 1; hit.normal_z = 0;
            }
            return hit;
        }
        // Sphere tracing: take step = d.
        t += std::max(d, epsilon);
    }
    return hit;
}

// ============================================================================
// T_Hybrid: SDF for first 2 voxel-distances, then voxel DDA.
// Rationale: SDF is precise near surface; voxel DDA is fast for far-field.
// ============================================================================
ConeHit march_hybrid(
    const scenes::Chunk& voxels,
    const sdf::SdfR8& sdf,
    Vec3 origin, Vec3 direction,
    float max_dist
) noexcept {
    ConeHit hit{};
    hit.hit = false;
    hit.distance = max_dist;
    hit.normal_x = 0; hit.normal_y = 0; hit.normal_z = 0;
    hit.steps = 0;

    direction = vec_normalize(direction);
    float t = 0.0f;
    const float sdf_cutoff = 2.0f;  // SDF precision window.
    const float epsilon = 0.05f;

    // Phase 1: SDF march until t > sdf_cutoff OR hit.
    while (t < sdf_cutoff && t < max_dist && hit.steps < 64) {
        Vec3 p = vec_add(origin, vec_scale(direction, t));
        float d = sdf_lookup_trilinear(sdf, p.x, p.y, p.z);
        hit.steps++;
        if (d <= epsilon) {
            hit.hit = true;
            hit.distance = t;
            float h = 0.1f;
            float dx = sdf_lookup_trilinear(sdf, p.x + h, p.y, p.z) -
                       sdf_lookup_trilinear(sdf, p.x - h, p.y, p.z);
            float dy = sdf_lookup_trilinear(sdf, p.x, p.y + h, p.z) -
                       sdf_lookup_trilinear(sdf, p.x, p.y - h, p.z);
            float dz = sdf_lookup_trilinear(sdf, p.x, p.y, p.z + h) -
                       sdf_lookup_trilinear(sdf, p.x, p.y, p.z - h);
            float nlen = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (nlen > 1e-6f) {
                hit.normal_x = dx / nlen;
                hit.normal_y = dy / nlen;
                hit.normal_z = dz / nlen;
            } else {
                hit.normal_x = 0; hit.normal_y = 1; hit.normal_z = 0;
            }
            return hit;
        }
        t += std::max(d, epsilon);
    }

    // Phase 2: voxel DDA from t to max_dist.
    const float step = 0.25f;
    while (t < max_dist) {
        Vec3 p = vec_add(origin, vec_scale(direction, t));
        int x = static_cast<int>(p.x);
        int y = static_cast<int>(p.y);
        int z = static_cast<int>(p.z);
        hit.steps++;
        if (x >= 0 && y >= 0 && z >= 0 &&
            x < static_cast<int>(scenes::CHUNK_SIZE) &&
            y < static_cast<int>(scenes::CHUNK_SIZE) &&
            z < static_cast<int>(scenes::CHUNK_SIZE) &&
            voxels[scenes::idx3(x, y, z)] != 0) {
            hit.hit = true;
            hit.distance = t;
            return hit;
        }
        t += step;
    }
    return hit;
}

ConeHit march(
    TermStrategy strategy,
    const scenes::Chunk& voxels,
    const sdf::SdfR8& sdf,
    Vec3 origin, Vec3 direction,
    float max_dist
) noexcept {
    switch (strategy) {
        case TermStrategy::T_VoxelDiscrete:
            return march_voxel(voxels, origin, direction, max_dist);
        case TermStrategy::T_SDFSmooth:
            return march_sdf(sdf, origin, direction, max_dist);
        case TermStrategy::T_Hybrid:
            return march_hybrid(voxels, sdf, origin, direction, max_dist);
    }
    return ConeHit{};
}

FragmentResult aggregate_cones(
    TermStrategy strategy,
    const scenes::Chunk& voxels,
    const sdf::SdfR8& sdf,
    Vec3 fragment_normal,
    int num_cones,
    float max_dist,
    Vec3 origin
) noexcept {
    FragmentResult fr{};
    fr.hit_count = 0;
    fr.mean_distance = 0.0f;
    fr.total_steps = 0;
    float dist_sum = 0.0f;
    for (int i = 0; i < num_cones; ++i) {
        // Generate cone direction in upper hemisphere around fragment_normal.
        Vec3 d = fibonacci_dir(i, num_cones);
        // Reflect d to upper hemisphere if it's below normal.
        if (vec_dot(d, fragment_normal) < 0.0f) {
            d.x = -d.x; d.y = -d.y; d.z = -d.z;
        }
        ConeHit h = march(strategy, voxels, sdf, origin, d, max_dist);
        if (h.hit) {
            fr.hit_count++;
            dist_sum += h.distance;
        }
        fr.total_steps += h.steps;
    }
    if (fr.hit_count > 0) {
        fr.mean_distance = dist_sum / static_cast<float>(fr.hit_count);
    }
    return fr;
}

float irradiance_proxy(const FragmentResult& fr, int num_cones) noexcept {
    if (num_cones == 0) return 0.0f;
    return static_cast<float>(fr.hit_count) / static_cast<float>(num_cones);
}

float psnr(float measured, float reference) noexcept {
    // PSNR for scalar comparison: 10 * log10(MAX^2 / MSE).
    // For 0-1 range: MAX = 1.0.
    float mse = (measured - reference) * (measured - reference);
    if (mse < 1e-10f) return 99.0f;  // perfect match
    return 10.0f * std::log10(1.0f / mse);
}

}  // namespace sdf_hybrid::vct
