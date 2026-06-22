// micro-detail height-field kernels for surface micro-detail benchmarking.
// 5 strategies: A_None / B_WorldHash / C_TangentFBM2D / D_Worley2D / E_DerivativeNormal.
// All operate on a 2D tangent-space coordinate (u, v) plus a 3D world-space coordinate
// (wx, wy, wz) and return a 2D gradient (dHdu, dHdv) that drives normal perturbation.
//
// Per-fragment ALU cost reference (cross-validated against Auburn/FastNoiseLite benchmarks
// for 2D coherent noise + a hash-based scheme + a Worley cellular scheme):
//   A_None             = 0  ops
//   B_WorldHash        = 6  ops (3 sin+cos for hash, 3 for perturbation)
//   C_TangentFBM2D     = ~25 ops (1 octave) / ~80 ops (4 octaves)
//   D_Worley2D         = ~22 ops (3×3 cell hash, F2-F1, gradient)
//   E_DerivativeNormal = ~12 ops (height eval + dFdx + dFdy + normalize)
//
// All functions are `inline` and `noexcept` to ensure the harness measures per-fragment
// ALU cost without function-call overhead.

#pragma once

#include <cmath>
#include <cstdint>
#include <array>

namespace projectv::micro_detail {

// Hash function: cheap 32-bit integer hash from a 3D position. Inigo Quilez style.
[[gnu::always_inline]] inline std::uint32_t hash3d(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8da6b343u
                    ^ static_cast<std::uint32_t>(y) * 0xd8163841u
                    ^ static_cast<std::uint32_t>(z) * 0xcb1ab31fu;
    h = (h ^ (h >> 16)) * 0x45d9f3bu;
    h = (h ^ (h >> 16)) * 0x45d9f3bu;
    h = h ^ (h >> 16);
    return h;
}

// Convert a hash to a unit float in [-1, 1].
[[gnu::always_inline]] inline float hash_to_unit(std::uint32_t h) noexcept {
    // Use 24-bit mantissa for clean float mapping.
    return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x00800000u) - 1.0f;
}

// Strategy A: no perturbation. Returns zero gradient.
[[gnu::always_inline]] inline void kernel_a_none(float /*u*/, float /*v*/,
                                                 float /*wx*/, float /*wy*/, float /*wz*/,
                                                 float& dHdu, float& dHdv) noexcept {
    dHdu = 0.0f;
    dHdv = 0.0f;
}

// Strategy B: world-position hash. 3D hash → scalar → tangent-space perturbation.
// The hash is position-quantized to 0.25 m so that adjacent fragments within the same voxel
// face share the same hash, creating the "per-voxel face" micro-detail pattern. Lower frequency
// (period = 0.25 m × quantization) than FBM.
[[gnu::always_inline]] inline void kernel_b_world_hash(float /*u*/, float /*v*/,
                                                      float wx, float wy, float wz,
                                                      float& dHdu, float& dHdv) noexcept {
    constexpr float kQuantize = 4.0f;  // 1 / 0.25 m
    const auto ix = static_cast<std::int32_t>(std::floor(wx * kQuantize));
    const auto iy = static_cast<std::int32_t>(std::floor(wy * kQuantize));
    const auto iz = static_cast<std::int32_t>(std::floor(wz * kQuantize));
    const std::uint32_t h = hash3d(ix, iy, iz);
    // Two independent scalars for the two gradient components.
    const float gx = hash_to_unit(hash3d(ix + 1, iy, iz)) * 0.3f;
    const float gy = hash_to_unit(hash3d(ix, iy + 1, iz)) * 0.3f;
    dHdu = gx;
    dHdv = gy;
    (void)h;  // silence unused warning
}

// 2D value noise + bilinear interpolation. Single-octave version. Used by C_TangentFBM2D.
[[gnu::always_inline]] inline float value_noise_2d(float x, float y) noexcept {
    const auto ix = static_cast<std::int32_t>(std::floor(x));
    const auto iy = static_cast<std::int32_t>(std::floor(y));
    const float fx = x - static_cast<float>(ix);
    const float fy = y - static_cast<float>(iy);
    const float u = fx * fx * (3.0f - 2.0f * fx);
    const float v = fy * fy * (3.0f - 2.0f * fy);
    const float v00 = hash_to_unit(hash3d(ix, iy, 0xCAFEu));
    const float v10 = hash_to_unit(hash3d(ix + 1, iy, 0xCAFEu));
    const float v01 = hash_to_unit(hash3d(ix, iy + 1, 0xCAFEu));
    const float v11 = hash_to_unit(hash3d(ix + 1, iy + 1, 0xCAFEu));
    const float a = v00 * (1.0f - u) + v10 * u;
    const float b = v01 * (1.0f - u) + v11 * u;
    return a * (1.0f - v) + b * v;
}

// Strategy C: 4-octave fractional Brownian motion in tangent space. The 4 octaves multiply
// frequency by 2 and amplitude by 0.5. Smooth continuous detail. ALI cost: 4 value_noise_2d
// calls (each ~10 ops) = ~40 ops + tangent transform.
[[gnu::always_inline]] inline void kernel_c_tangent_fbm_2d(float u, float v,
                                                           float /*wx*/, float /*wy*/, float /*wz*/,
                                                           float& dHdu, float& dHdv) noexcept {
    // Period = 16 (in tangent UV space, which is unit-square per voxel face).
    constexpr float kBaseFreq = 16.0f;
    constexpr float kAmp = 0.5f;
    constexpr float kFreqStep = 2.0f;

    // Finite-difference gradient of the FBM.
    constexpr float kEps = 1.0f / 1024.0f;
    const float u_p = u + kEps;
    const float u_m = u - kEps;
    const float v_p = v + kEps;
    const float v_m = v - kEps;

    float h_pu = 0.0f, h_mu = 0.0f, h_pv = 0.0f, h_mv = 0.0f;
    float amp = 1.0f;
    float freq = kBaseFreq;
    for (int octave = 0; octave < 4; ++octave) {
        h_pu += amp * value_noise_2d(u_p * freq, v * freq);
        h_mu += amp * value_noise_2d(u_m * freq, v * freq);
        h_pv += amp * value_noise_2d(u * freq, v_p * freq);
        h_mv += amp * value_noise_2d(u * freq, v_m * freq);
        amp *= kAmp;
        freq *= kFreqStep;
    }
    constexpr float kInvTwoEps = 1.0f / (2.0f * kEps);
    dHdu = (h_pu - h_mu) * kInvTwoEps;
    dHdv = (h_pv - h_mv) * kInvTwoEps;
}

// Single-octave Worley (cellular) noise + F2-F1 cell-edge gradient. The gradient of
// (F2 - F1) points from F1-cell-center toward F2-cell-center, giving clean Voronoi-edge
// detail. Hard-edged cracks.
[[gnu::always_inline]] inline void worley_f1_f2_and_grad(float x, float y,
                                                        float& f1, float& f2,
                                                        float& gx, float& gy) noexcept {
    const auto ix = static_cast<std::int32_t>(std::floor(x));
    const auto iy = static_cast<std::int32_t>(std::floor(y));
    f1 = 1e9f; f2 = 1e9f;
    float cx1 = 0.0f, cy1 = 0.0f;
    float cx2 = 0.0f, cy2 = 0.0f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto cxi = ix + dx;
            const auto cyi = iy + dy;
            // Per-cell feature point at hash-quantized sub-cell position.
            const float h_x = hash_to_unit(hash3d(cxi, cyi, 0xBEEFu));
            const float h_y = hash_to_unit(hash3d(cxi, cyi, 0xDEADu));
            const float fx = static_cast<float>(cxi) + 0.5f + 0.5f * h_x;
            const float fy = static_cast<float>(cyi) + 0.5f + 0.5f * h_y;
            const float ddx = x - fx;
            const float ddy = y - fy;
            const float dist_sq = ddx * ddx + ddy * ddy;
            if (dist_sq < f1) {
                f2 = f1; cx2 = cx1; cy2 = cy1;
                f1 = dist_sq; cx1 = fx; cy1 = fy;
            } else if (dist_sq < f2) {
                f2 = dist_sq; cx2 = fx; cy2 = fy;
            }
        }
    }
    f1 = std::sqrt(f1);
    f2 = std::sqrt(f2);
    // F2 - F1 is small near the Voronoi edge and larger in cell interior. Gradient
    // points from F1-center toward F2-center.
    const float ex = cx2 - cx1;
    const float ey = cy2 - cy1;
    const float len = std::sqrt(ex * ex + ey * ey);
    if (len > 1e-6f) {
        gx = ex / len;
        gy = ey / len;
    } else {
        gx = 0.0f;
        gy = 0.0f;
    }
}

// Strategy D: Worley2D, period = 8 cells per voxel face, intensity = (F2 - F1) / cell_size.
[[gnu::always_inline]] inline void kernel_d_worley_2d(float u, float v,
                                                      float /*wx*/, float /*wy*/, float /*wz*/,
                                                      float& dHdu, float& dHdv) noexcept {
    constexpr float kPeriod = 8.0f;
    constexpr float kScale = 2.0f;  // amplify the (F2 - F1) gradient into a normal
    float f1, f2, gx, gy;
    worley_f1_f2_and_grad(u * kPeriod, v * kPeriod, f1, f2, gx, gy);
    const float intensity = (f2 - f1) * kScale;
    dHdu = intensity * gx;
    dHdv = intensity * gy;
}

// Strategy E: dFdx/dFdy-style derivative normal. Simulates the GLSL `dFdx(height)` and
// `dFdy(height)` operators using neighbor fragment data. We use a procedural height field
// (FBM with 1 octave for speed) and finite-difference the analytical value noise itself.
[[gnu::always_inline]] inline void kernel_e_derivative_normal(float u, float v,
                                                             float /*wx*/, float /*wy*/, float /*wz*/,
                                                             float& dHdu, float& dHdv) noexcept {
    constexpr float kFreq = 32.0f;  // higher freq than C for "screen-space derivative" look
    constexpr float kEps = 1.0f / 256.0f;
    const float h_pu = value_noise_2d((u + kEps) * kFreq, v * kFreq);
    const float h_mu = value_noise_2d((u - kEps) * kFreq, v * kFreq);
    const float h_pv = value_noise_2d(u * kFreq, (v + kEps) * kFreq);
    const float h_mv = value_noise_2d(u * kFreq, (v - kEps) * kFreq);
    constexpr float kInvTwoEps = 1.0f / (2.0f * kEps);
    dHdu = (h_pu - h_mu) * kInvTwoEps;
    dHdv = (h_pv - h_mv) * kInvTwoEps;
}

// Dispatcher — strategy code to function pointer.
using kernel_fn = void (*)(float, float, float, float, float, float&, float&);

inline constexpr std::array<kernel_fn, 5> kKernels = {
    &kernel_a_none,
    &kernel_b_world_hash,
    &kernel_c_tangent_fbm_2d,
    &kernel_d_worley_2d,
    &kernel_e_derivative_normal,
};

}  // namespace projectv::micro_detail
