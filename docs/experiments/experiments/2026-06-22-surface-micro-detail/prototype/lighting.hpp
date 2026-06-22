// Lighting + PSNR utilities for surface micro-detail benchmarking.
// Lambertian diffuse + isotropic GGX specular BRDF, single directional light, single-bounce,
// no GI. We use this as the *downstream consumer* of the per-fragment perturbed normal — the
// same lighting model the ProjectV mainline `voxel.frag` uses (analytical, validated
// against Akenine-Möller Real-Time Rendering 4th Ed. Ch. 9).

#pragma once

#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace projectv::micro_detail {

// 3D vector struct with the three operations we need: dot, normalize, reflect.
struct Vec3 {
    float x, y, z;
    [[gnu::always_inline]] inline Vec3 operator+(Vec3 o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    [[gnu::always_inline]] inline Vec3 operator-(Vec3 o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    [[gnu::always_inline]] inline Vec3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    [[gnu::always_inline]] inline Vec3 operator*(Vec3 o) const noexcept { return {x * o.x, y * o.y, z * o.z}; }
    [[gnu::always_inline]] inline Vec3 operator/(float s) const noexcept { return {x / s, y / s, z / s}; }
    [[gnu::always_inline]] static inline float dot(Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
    [[gnu::always_inline]] static inline Vec3 normalize(Vec3 v) noexcept {
        const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (l < 1e-7f) return {0.0f, 1.0f, 0.0f};
        return {v.x / l, v.y / l, v.z / l};
    }
};

// Build a tangent frame (T, B, N) from a per-fragment world normal. The micro-detail
// gradients (dHdu, dHdv) are then converted to a world-space normal perturbation
// `N' = normalize(N - strength * (dHdu * T + dHdv * B))`. This matches the "Blinn
// 1978" perturbation formula referenced in Wikipedia "Bump mapping".
struct TangentFrame {
    Vec3 T;
    Vec3 B;
    Vec3 N;
    [[gnu::always_inline]] static inline TangentFrame from_normal(Vec3 N) noexcept {
        // Pick T perpendicular to N via the "least aligned with N" branch.
        Vec3 T;
        if (std::abs(N.x) > 0.5f) {
            T = Vec3::normalize({-N.z, 0.0f, N.x});
        } else {
            T = Vec3::normalize({0.0f, -N.z, N.y});
        }
        const Vec3 B = Vec3::normalize(Vec3{T.y * N.z - T.z * N.y,
                                            T.z * N.x - T.x * N.z,
                                            T.x * N.y - T.y * N.x});
        return {T, B, N};
    }
};

// GGX / Trowbridge-Reitz normal distribution. Akenine-Möller RTR4 Eq. 9.36.
[[gnu::always_inline]] inline float ggx_ndf(float NoH, float alpha2) noexcept {
    const float denom = NoH * NoH * (alpha2 - 1.0f) + 1.0f;
    return alpha2 / (3.14159265f * denom * denom);
}

// Smith geometry term, height-correlated, Schlick-GGX. RTR4 Eq. 9.46 + 9.47 simplified.
[[gnu::always_inline]] inline float smith_ggx(float NoV, float NoL, float alpha2) noexcept {
    const float k = (alpha2 + 1.0f);
    const float k2 = 0.25f * k * k;
    const float gv = NoV * std::sqrt(NoV * NoV * (1.0f - k2) + k2);
    const float gl = NoL * std::sqrt(NoL * NoL * (1.0f - k2) + k2);
    return 0.5f / (gv + gl + 1e-7f);
}

// Schlick Fresnel approximation. RTR4 Eq. 9.41.
[[gnu::always_inline]] inline Vec3 fresnel_schlick(float VoH, Vec3 F0) noexcept {
    const float f = std::pow(1.0f - VoH, 5.0f);
    return {F0.x + (1.0f - F0.x) * f,
            F0.y + (1.0f - F0.y) * f,
            F0.z + (1.0f - F0.z) * f};
}

// Full analytical PBR BRDF (no IBL, single directional light). Returns linear RGB.
[[gnu::always_inline]] inline Vec3 brdf(Vec3 N, Vec3 V, Vec3 L, Vec3 albedo,
                                         float roughness, Vec3 F0) noexcept {
    const Vec3 H = Vec3::normalize(V + L);
    const float NoL = std::max(0.0f, Vec3::dot(N, L));
    const float NoV = std::max(1e-4f, Vec3::dot(N, V));
    const float NoH = std::max(0.0f, Vec3::dot(N, H));
    const float VoH = std::max(0.0f, Vec3::dot(V, H));
    const float a = std::max(0.005f, roughness);
    const float a2 = a * a;
    const float D = ggx_ndf(NoH, a2);
    const float G = smith_ggx(NoV, NoL, a2);
    const Vec3 F = fresnel_schlick(VoH, F0);
    const Vec3 spec = F * (D * G / std::max(0.25f, 4.0f * NoV * NoL));
    const Vec3 kd = {1.0f - F.x, 1.0f - F.y, 1.0f - F.z};
    const Vec3 diff = kd * albedo * (1.0f / 3.14159265f);
    return (diff + spec) * NoL;
}

// Scene parameter: material + view + light + base color + roughness.
struct SceneParams {
    std::string name;
    Vec3 albedo;     // linear sRGB
    float roughness; // 0..1
    Vec3 view_dir;   // from surface toward camera
    Vec3 light_dir;  // from surface toward sun
    Vec3 light_color; // linear, normalized
};

// Per-fragment perturb: convert (dHdu, dHdv) into a perturbed world-space normal.
[[gnu::always_inline]] inline Vec3 perturb_normal(Vec3 N_world, float dHdu, float dHdv,
                                                  float strength) noexcept {
    const TangentFrame f = TangentFrame::from_normal(N_world);
    Vec3 dN = f.T * dHdu * strength + f.B * dHdv * strength;
    Vec3 Np = {N_world.x - dN.x, N_world.y - dN.y, N_world.z - dN.z};
    return Vec3::normalize(Np);
}

// PSNR computation. Assumes linear RGB, range [0, 1]. Returns dB.
[[gnu::always_inline]] inline float psnr_db(const Vec3& a, const Vec3& b) noexcept {
    const float dr = a.x - b.x;
    const float dg = a.y - b.y;
    const float db_ = a.z - b.z;
    const float mse = (dr * dr + dg * dg + db_ * db_) / 3.0f;
    if (mse < 1e-10f) return 100.0f;  // identical
    return 10.0f * std::log10(1.0f / mse);
}

// Mean over 1920×1080 fragment buffer.
struct RenderStats {
    float psnr_vs_a;
    float delta_e_2000_proxy;  // analytical: |a-b|_1 average over channel
};

// Compute mean per-fragment PSNR vs the A_None reference render (linear RGB).
inline RenderStats compute_stats(const std::vector<Vec3>& ref, const std::vector<Vec3>& cur) noexcept {
    if (ref.size() != cur.size() || ref.empty()) return {0.0f, 0.0f};
    double sum_psnr = 0.0;
    double sum_delta_e = 0.0;
    const std::size_t n = ref.size();
    for (std::size_t i = 0; i < n; ++i) {
        sum_psnr += psnr_db(ref[i], cur[i]);
        const float dr = std::abs(ref[i].x - cur[i].x);
        const float dg = std::abs(ref[i].y - cur[i].y);
        const float db_ = std::abs(ref[i].z - cur[i].z);
        sum_delta_e += (dr + dg + db_) / 3.0f;
    }
    return {static_cast<float>(sum_psnr / static_cast<double>(n)),
            static_cast<float>(sum_delta_e / static_cast<double>(n) * 100.0f)};
}

}  // namespace projectv::micro_detail
