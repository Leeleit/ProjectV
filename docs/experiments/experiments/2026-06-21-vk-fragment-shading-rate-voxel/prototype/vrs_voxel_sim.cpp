// SPDX-License-Identifier: MIT
// Standalone CPU voxel rasterizer + VRS attachment simulator.
// Standalone — NOT linked to ProjectV mainline. Per docs/experiments/AGENTS.md §9.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vrs_sim {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;

struct Vec2 { f32 x{}, y{}; };
struct Vec3 { f32 x{}, y{}, z{}; };
struct Vec4 { f32 x{}, y{}, z{}, w{}; };

enum class VrsConfig : u8 {
    Baseline_1x1        = 0,
    Vrs_2x1             = 1,
    Vrs_1x2             = 2,
    Vrs_2x2_Global      = 3,
    Vrs_Hybrid_2x2_Light = 4,
};

constexpr std::string_view VrsConfigName(VrsConfig c) {
    switch (c) {
        case VrsConfig::Baseline_1x1:        return "baseline_1x1";
        case VrsConfig::Vrs_2x1:             return "vrs_2x1";
        case VrsConfig::Vrs_1x2:             return "vrs_1x2";
        case VrsConfig::Vrs_2x2_Global:      return "vrs_2x2_global";
        case VrsConfig::Vrs_Hybrid_2x2_Light: return "vrs_hybrid_2x2_lighting";
    }
    return "unknown";
}

struct Resolution {
    u32 width;
    u32 height;
    constexpr std::string_view name() const {
        if (width == 1920 && height == 1080) return "1080p";
        if (width == 2560 && height == 1440) return "1440p";
        if (width == 3840 && height == 2160) return "4k";
        return "custom";
    }
};

enum class SceneKind : u8 {
    UniformOpen  = 0,
    ForestFloor  = 1,
    CaveStress   = 2,
    MixedBiome   = 3,
};

constexpr std::string_view SceneKindName(SceneKind s) {
    switch (s) {
        case SceneKind::UniformOpen: return "uniform_open";
        case SceneKind::ForestFloor: return "forest_floor";
        case SceneKind::CaveStress:  return "cave_stress";
        case SceneKind::MixedBiome:  return "mixed_biome";
    }
    return "unknown";
}

struct VoxelScene {
    SceneKind kind;
    u32 dim = 64;
    std::vector<u8> voxels;

    u8 at(i32 x, i32 y, i32 z) const {
        if (x < 0 || y < 0 || z < 0) return 0;
        if (x >= static_cast<i32>(dim) || y >= static_cast<i32>(dim) || z >= static_cast<i32>(dim)) return 0;
        return voxels[static_cast<u32>(x) + static_cast<u32>(y) * dim + static_cast<u32>(z) * dim * dim];
    }

    void set(i32 x, i32 y, i32 z, u8 v) {
        if (x < 0 || y < 0 || z < 0) return;
        if (x >= static_cast<i32>(dim) || y >= static_cast<i32>(dim) || z >= static_cast<i32>(dim)) return;
        voxels[static_cast<u32>(x) + static_cast<u32>(y) * dim + static_cast<u32>(z) * dim * dim] = v;
    }
};

VoxelScene build_scene(SceneKind kind, u32 seed) {
    VoxelScene s;
    s.kind = kind;
    s.voxels.assign(static_cast<size_t>(s.dim) * s.dim * s.dim, 0);
    std::mt19937 rng(seed);

    switch (kind) {
        case SceneKind::UniformOpen: {
            for (u32 x = 0; x < s.dim; ++x)
                for (u32 z = 0; z < s.dim; ++z)
                    for (u32 y = 0; y < s.dim / 4; ++y)
                        s.voxels[x + y * s.dim + z * s.dim * s.dim] = 1;
            for (u32 i = 0; i < s.dim * s.dim; ++i) {
                f32 h = s.dim / 4.0f + 1.0f * std::sin(static_cast<f32>(i) * 0.05f);
                if (static_cast<u32>(h) < s.dim) {
                    u32 y = static_cast<u32>(h);
                    s.voxels[i % s.dim + y * s.dim + (i / s.dim) * s.dim * s.dim] = 1;
                }
            }
            break;
        }
        case SceneKind::ForestFloor: {
            for (u32 x = 0; x < s.dim; ++x)
                for (u32 z = 0; z < s.dim; ++z) {
                    for (u32 y = 0; y < s.dim / 6; ++y)
                        s.voxels[x + y * s.dim + z * s.dim * s.dim] = 1;
                    std::uniform_real_distribution<f32> jitter(0.0f, 2.0f);
                    for (u32 y = s.dim / 6; y < s.dim / 6 + 3; ++y)
                        s.voxels[x + y * s.dim + z * s.dim * s.dim] = 2;
                }
            std::uniform_int_distribution<u32> tree_x(2, s.dim - 3);
            std::uniform_int_distribution<u32> tree_z(2, s.dim - 3);
            for (u32 t = 0; t < 32; ++t) {
                u32 tx = tree_x(rng), tz = tree_z(rng);
                for (u32 dy = 0; dy < 12; ++dy)
                    for (i32 dx = -1; dx <= 1; ++dx)
                        for (i32 dz = -1; dz <= 1; ++dz)
                            s.set(static_cast<i32>(tx) + dx, static_cast<i32>(s.dim / 6) + 3 + static_cast<i32>(dy),
                                  static_cast<i32>(tz) + dz, 3);
            }
            break;
        }
        case SceneKind::CaveStress: {
            std::uniform_real_distribution<f32> u(0.0f, 1.0f);
            for (u32 x = 0; x < s.dim; ++x)
                for (u32 y = 0; y < s.dim; ++y)
                    for (u32 z = 0; z < s.dim; ++z) {
                        f32 n1 = std::sin(static_cast<f32>(x) * 0.18f) * std::cos(static_cast<f32>(y) * 0.21f) * std::sin(static_cast<f32>(z) * 0.15f);
                        f32 n2 = std::sin(static_cast<f32>(x) * 0.07f + 1.3f) * std::cos(static_cast<f32>(z) * 0.09f);
                        f32 v = n1 + n2 * 0.5f;
                        if (v > 0.2f) s.voxels[x + y * s.dim + z * s.dim * s.dim] = 4;
                        else if (v > -0.1f) s.voxels[x + y * s.dim + z * s.dim * s.dim] = 1;
                    }
            break;
        }
        case SceneKind::MixedBiome: {
            std::uniform_real_distribution<f32> u(0.0f, 1.0f);
            for (u32 x = 0; x < s.dim; ++x)
                for (u32 z = 0; z < s.dim; ++z) {
                    f32 height =
                        0.30f * std::sin(static_cast<f32>(x) * 0.10f) * std::cos(static_cast<f32>(z) * 0.13f) +
                        0.15f * std::sin(static_cast<f32>(x + z) * 0.05f);
                    u32 base = static_cast<u32>(s.dim * (0.35f + height));
                    for (u32 y = 0; y < base && y < s.dim; ++y)
                        s.voxels[x + y * s.dim + z * s.dim * s.dim] = 1;
                    if (base < s.dim)
                        s.voxels[x + base * s.dim + z * s.dim * s.dim] = 2;
                    if (base + 1 < s.dim)
                        s.voxels[x + (base + 1) * s.dim + z * s.dim * s.dim] = 5;
                    if (u(rng) < 0.02f && base + 5 < s.dim) {
                        for (u32 dy = 0; dy < 5; ++dy)
                            s.voxels[x + (base + 2 + dy) * s.dim + z * s.dim * s.dim] = 3;
                    }
                }
            std::uniform_int_distribution<u32> rx(2, s.dim - 3);
            std::uniform_int_distribution<u32> rz(2, s.dim - 3);
            for (u32 t = 0; t < 16; ++t) {
                u32 cx = rx(rng), cz = rz(rng);
                for (i32 dx = -4; dx <= 4; ++dx)
                    for (i32 dz = -4; dz <= 4; ++dz)
                        for (u32 y = 0; y < 4; ++y) {
                            i32 xx = static_cast<i32>(cx) + dx;
                            i32 zz = static_cast<i32>(cz) + dz;
                            i32 yy = static_cast<i32>(y);
                            if (dx * dx + dz * dz <= 16)
                                s.set(xx, yy, zz, 6);
                        }
            }
            break;
        }
    }
    return s;
}

struct Camera {
    Vec3 eye{32.0f, 24.0f, 32.0f};
    Vec3 target{32.0f, 16.0f, 16.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    f32 fov_y{60.0f};
    f32 near{0.1f};
    f32 far{256.0f};
};

struct RasterStats {
    u64 total_pixels = 0;
    u64 covered_pixels = 0;
    u64 edge_pixels = 0;
    u64 high_freq_pixels = 0;
    u64 specular_likely_pixels = 0;
    f32 coverage_pct = 0.0f;
    f32 edge_density = 0.0f;
    f32 high_freq_ratio = 0.0f;
};

RasterStats rasterize_cpu(const VoxelScene& s, const Camera& cam, Resolution res,
                              std::vector<u8>& coverage, std::vector<u8>& edge) {
    RasterStats r;
    r.total_pixels = static_cast<u64>(res.width) * res.height;
    coverage.assign(r.total_pixels, 0);
    edge.assign(r.total_pixels, 0);

    f32 aspect = static_cast<f32>(res.width) / static_cast<f32>(res.height);
    f32 tan_half_fov = std::tan(cam.fov_y * 3.14159265f / 360.0f);

    Vec3 forward{ cam.target.x - cam.eye.x, cam.target.y - cam.eye.y, cam.target.z - cam.eye.z };
    f32 fl = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    forward = { forward.x / fl, forward.y / fl, forward.z / fl };
    Vec3 right{ forward.z, 0.0f, -forward.x };
    f32 rl = std::sqrt(right.x * right.x + right.z * right.z);
    if (rl > 1e-6f) { right.x /= rl; right.z /= rl; }
    Vec3 up_v{ forward.y * right.z, forward.z * right.x - forward.x * right.z, -forward.y * right.x };
    f32 ul = std::sqrt(up_v.x * up_v.x + up_v.y * up_v.y + up_v.z * up_v.z);
    if (ul > 1e-6f) { up_v.x /= ul; up_v.y /= ul; up_v.z /= ul; }

    const u32 pixel_step = 4;
    constexpr f32 ray_step = 1.0f;
    constexpr f32 ray_max = 128.0f;

    for (u32 sy = 0; sy < res.height; sy += pixel_step) {
        for (u32 sx = 0; sx < res.width; sx += pixel_step) {
            f32 ndc_x = (static_cast<f32>(sx) + 0.5f) / res.width * 2.0f - 1.0f;
            f32 ndc_y = 1.0f - (static_cast<f32>(sy) + 0.5f) / res.height * 2.0f;
            f32 dir_x = ndc_x * aspect * tan_half_fov * right.x + ndc_y * tan_half_fov * up_v.x + forward.x;
            f32 dir_y = ndc_x * aspect * tan_half_fov * right.y + ndc_y * tan_half_fov * up_v.y + forward.y;
            f32 dir_z = ndc_x * aspect * tan_half_fov * right.z + ndc_y * tan_half_fov * up_v.z + forward.z;
            f32 dl = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
            dir_x /= dl; dir_y /= dl; dir_z /= dl;

            f32 px = cam.eye.x, py = cam.eye.y, pz = cam.eye.z;
            i32 prev_mat = 0;
            f32 dist = 0.0f;
            bool hit = false;
            while (dist < ray_max) {
                i32 vx = static_cast<i32>(std::floor(px));
                i32 vy = static_cast<i32>(std::floor(py));
                i32 vz = static_cast<i32>(std::floor(pz));
                if (vx < 0 || vy < 0 || vz < 0 || vx >= static_cast<i32>(s.dim) ||
                    vy >= static_cast<i32>(s.dim) || vz >= static_cast<i32>(s.dim)) {
                    if (hit) break;
                    px += dir_x * ray_step; py += dir_y * ray_step; pz += dir_z * ray_step; dist += ray_step;
                    continue;
                }
                u8 m = s.at(vx, vy, vz);
                if (m != 0) {
                    if (prev_mat != 0 && prev_mat != m) {
                        edge[sy * res.width + sx] = 1;
                    }
                    prev_mat = m;
                    hit = true;
                    coverage[sy * res.width + sx] = 1;
                    break;
                }
                px += dir_x * ray_step; py += dir_y * ray_step; pz += dir_z * ray_step; dist += ray_step;
            }
        }
    }

    u64 covered = 0, edge_count = 0;
    for (u64 i = 0; i < r.total_pixels; ++i) {
        if (coverage[i]) ++covered;
        if (edge[i]) ++edge_count;
    }

    r.covered_pixels = covered;
    r.edge_pixels = edge_count;
    r.high_freq_pixels = edge_count;
    r.coverage_pct = 100.0f * static_cast<f32>(covered) / static_cast<f32>(r.total_pixels);
    r.edge_density = covered > 0 ? static_cast<f32>(edge_count) / static_cast<f32>(covered) : 0.0f;
    r.high_freq_ratio = r.edge_density;
    r.specular_likely_pixels = covered / 8;

    return r;
}

struct VrsAttachment {
    u32 tile_w = 16;
    u32 tile_h = 16;
    u32 img_w = 0;
    u32 img_h = 0;
    std::vector<u8> tiles;
};

VrsAttachment build_vrs_image(const VoxelScene& s, const Camera& cam, Resolution res,
                              const std::vector<u8>& coverage, const std::vector<u8>& edge_map) {
    VrsAttachment a;
    a.tile_w = 16;
    a.tile_h = 16;
    a.img_w = (res.width + a.tile_w - 1) / a.tile_w;
    a.img_h = (res.height + a.tile_h - 1) / a.tile_h;
    a.tiles.assign(static_cast<size_t>(a.img_w) * a.img_h, 0);

    for (u32 ty = 0; ty < a.img_h; ++ty) {
        for (u32 tx = 0; tx < a.img_w; ++tx) {
            u32 cov = 0, total = 0, edg = 0;
            for (u32 dy = 0; dy < a.tile_h; ++dy) {
                for (u32 dx = 0; dx < a.tile_w; ++dx) {
                    u32 px = tx * a.tile_w + dx;
                    u32 py = ty * a.tile_h + dy;
                    if (px >= res.width || py >= res.height) continue;
                    ++total;
                    if (coverage[py * res.width + px]) ++cov;
                    if (edge_map[py * res.width + px]) ++edg;
                }
            }
            if (cov == 0 || total == 0) { a.tiles[ty * a.img_w + tx] = 0; continue; }
            f32 cov_ratio = static_cast<f32>(cov) / static_cast<f32>(total);
            f32 edge_ratio = static_cast<f32>(edg) / static_cast<f32>(cov);
            if (cov_ratio < 0.50f || edge_ratio > 0.10f) {
                a.tiles[ty * a.img_w + tx] = 1;
            } else if (cov_ratio < 0.85f || edge_ratio > 0.03f) {
                a.tiles[ty * a.img_w + tx] = 1;
            } else {
                a.tiles[ty * a.img_w + tx] = 2;
            }
        }
    }
    return a;
}

u64 effective_invocations_baseline(const RasterStats& r) {
    return r.covered_pixels;
}

u64 effective_invocations_2x1(const RasterStats& r, const VrsAttachment& vrs) {
    return (r.covered_pixels + 1) / 2;
}

u64 effective_invocations_1x2(const RasterStats& r, const VrsAttachment& vrs) {
    return (r.covered_pixels + 1) / 2;
}

u64 effective_invocations_2x2_global(const RasterStats& r, const VrsAttachment& vrs) {
    return (r.covered_pixels + 3) / 4;
}

u64 effective_invocations_hybrid(const RasterStats& r, const VrsAttachment& vrs) {
    u64 high_detail_tiles = 0, low_detail_tiles = 0;
    for (u8 t : vrs.tiles) {
        if (t == 1) ++high_detail_tiles;
        else if (t == 2) ++low_detail_tiles;
    }
    u64 tile_pixels = static_cast<u64>(vrs.tile_w) * vrs.tile_h;
    u64 high_pixels = high_detail_tiles * tile_pixels;
    u64 low_pixels = low_detail_tiles * tile_pixels;
    u64 total_classified = high_pixels + low_pixels;
    if (total_classified > r.covered_pixels && total_classified > 0) {
        f64 ratio = static_cast<f64>(r.covered_pixels) / static_cast<f64>(total_classified);
        high_pixels = static_cast<u64>(high_pixels * ratio);
        low_pixels = r.covered_pixels > high_pixels ? r.covered_pixels - high_pixels : 0;
    }
    return high_pixels + (low_pixels + 3) / 4;
}

u64 invocations_for(VrsConfig cfg, const RasterStats& r, const VrsAttachment& vrs) {
    switch (cfg) {
        case VrsConfig::Baseline_1x1:        return effective_invocations_baseline(r);
        case VrsConfig::Vrs_2x1:             return effective_invocations_2x1(r, vrs);
        case VrsConfig::Vrs_1x2:             return effective_invocations_1x2(r, vrs);
        case VrsConfig::Vrs_2x2_Global:      return effective_invocations_2x2_global(r, vrs);
        case VrsConfig::Vrs_Hybrid_2x2_Light: return effective_invocations_hybrid(r, vrs);
    }
    return r.covered_pixels;
}

f64 cpu_proxy_compute_gen_us(const VrsAttachment& vrs) {
    constexpr f64 ns_per_tile = 7.0;
    f64 total_ns = static_cast<f64>(vrs.tiles.size()) * ns_per_tile;
    return total_ns / 1000.0;
}

f64 cpu_proxy_apply_overhead_us(VrsConfig cfg) {
    constexpr f64 base_overhead_us = 5.0;
    constexpr f64 hybrid_extra_us = 8.0;
    constexpr f64 nas_transition_latency_us = 50.0;
    f64 overhead = base_overhead_us + nas_transition_latency_us;
    if (cfg == VrsConfig::Vrs_Hybrid_2x2_Light) overhead += hybrid_extra_us;
    return overhead;
}

f32 quality_risk_score(const VoxelScene& s, const RasterStats& r, VrsConfig cfg) {
    if (cfg == VrsConfig::Baseline_1x1) return 0.0f;

    f32 edge_factor = std::min(1.0f, r.edge_density * 8.0f);

    f32 spec_factor = 0.0f;
    if (r.covered_pixels > 0)
        spec_factor = std::min(1.0f, static_cast<f32>(r.specular_likely_pixels) /
                                       static_cast<f32>(r.covered_pixels) * 8.0f);

    f32 rate_factor = 0.0f;
    switch (cfg) {
        case VrsConfig::Vrs_2x1:
        case VrsConfig::Vrs_1x2:
            rate_factor = 0.25f; break;
        case VrsConfig::Vrs_2x2_Global:
            rate_factor = 0.50f; break;
        case VrsConfig::Vrs_Hybrid_2x2_Light:
            rate_factor = 0.45f; break;
        default: rate_factor = 0.0f;
    }

    f32 small_tri_penalty = 0.0f;
    if (s.kind == SceneKind::CaveStress || s.kind == SceneKind::MixedBiome) {
        small_tri_penalty = 0.15f;
    }

    f32 score = rate_factor * 0.55f + edge_factor * 0.20f + spec_factor * 0.15f + small_tri_penalty;
    return std::min(1.0f, score);
}

struct BenchResult {
    std::string scene;
    std::string res;
    std::string vrs_cfg;
    u64 total_pixels = 0;
    u64 covered_pixels = 0;
    f32 coverage_pct = 0.0f;
    f32 edge_density = 0.0f;
    u64 shader_invocations = 0;
    f32 vrs_savings_pct = 0.0f;
    u32 vrs_image_bytes = 0;
    f64 compute_gen_us = 0.0;
    f64 apply_overhead_us = 0.0;
    f64 total_us = 0.0;
    f32 quality_risk = 0.0f;
};

BenchResult run_one(SceneKind scene_kind, Resolution res, VrsConfig cfg, u32 seed) {
    VoxelScene scene = build_scene(scene_kind, seed);
    Camera cam;

    std::vector<u8> coverage;
    std::vector<u8> edge;
    RasterStats raster = rasterize_cpu(scene, cam, res, coverage, edge);

    auto vrs_start = std::chrono::steady_clock::now();
    VrsAttachment vrs = build_vrs_image(scene, cam, res, coverage, edge);
    u64 invocations = invocations_for(cfg, raster, vrs);
    f64 gen_us = cpu_proxy_compute_gen_us(vrs);
    f64 apply_us = cpu_proxy_apply_overhead_us(cfg);
    auto vrs_end = std::chrono::steady_clock::now();
    f64 real_vrs_us = std::chrono::duration<f64, std::micro>(vrs_end - vrs_start).count();

    BenchResult br;
    br.scene = std::string(SceneKindName(scene_kind));
    br.res = std::string(res.name());
    br.vrs_cfg = std::string(VrsConfigName(cfg));
    br.total_pixels = raster.total_pixels;
    br.covered_pixels = raster.covered_pixels;
    br.coverage_pct = raster.coverage_pct;
    br.edge_density = raster.edge_density;
    br.shader_invocations = invocations;
    br.vrs_image_bytes = static_cast<u32>(vrs.tiles.size());

    u64 baseline_invocations = effective_invocations_baseline(raster);
    br.vrs_savings_pct = baseline_invocations > 0
        ? 100.0f * static_cast<f32>(baseline_invocations - invocations) / static_cast<f32>(baseline_invocations)
        : 0.0f;

    br.compute_gen_us = gen_us;
    br.apply_overhead_us = apply_us;
    br.total_us = real_vrs_us + apply_us;
    br.quality_risk = quality_risk_score(scene, raster, cfg);
    return br;
}

struct Args {
    u32 warmup = 10;
    u32 iters = 100;
    bool smoke = false;
    bool help = false;
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string_view s = argv[i];
        if (s == "--smoke") { a.smoke = true; a.warmup = 2; a.iters = 5; }
        else if (s == "--iters") {
            if (i + 1 < argc) a.iters = static_cast<u32>(std::stoul(argv[++i]));
        }
        else if (s == "--help" || s == "-h") a.help = true;
    }
    return a;
}

void print_help() {
    std::printf("vrs_voxel_sim — Standalone CPU voxel rasterizer + VRS attachment simulator\n");
    std::printf("Usage: vrs_voxel_sim [options]\n");
    std::printf("  --smoke          quick run (2 warmup, 5 iter)\n");
    std::printf("  --iters N        measurement iterations (default 1000)\n");
    std::printf("  --help, -h       this help\n");
}

}

int main(int argc, char** argv) {
    using namespace vrs_sim;

    Args args = parse_args(argc, argv);
    if (args.help) { print_help(); return 0; }

    std::printf("# vrs_voxel_sim\n");
    std::printf("# warmup=%u, iters=%u\n", args.warmup, args.iters);
    std::printf("# scenes: uniform_open / forest_floor / cave_stress / mixed_biome\n");
    std::printf("# resolutions: 1080p / 1440p / 4k\n");
    std::printf("# vrs configs: baseline_1x1 / vrs_2x1 / vrs_1x2 / vrs_2x2_global / vrs_hybrid_2x2_lighting\n");
    std::printf("# output: results.csv (machine-readable)\n");

    constexpr std::array<SceneKind, 4> scenes = {
        SceneKind::UniformOpen, SceneKind::ForestFloor, SceneKind::CaveStress, SceneKind::MixedBiome
    };
    constexpr std::array<Resolution, 3> resolutions = {
        Resolution{1920, 1080}, Resolution{2560, 1440}, Resolution{3840, 2160}
    };
    constexpr std::array<VrsConfig, 5> configs = {
        VrsConfig::Baseline_1x1, VrsConfig::Vrs_2x1, VrsConfig::Vrs_1x2,
        VrsConfig::Vrs_2x2_Global, VrsConfig::Vrs_Hybrid_2x2_Light
    };

    std::vector<BenchResult> results;
    results.reserve(scenes.size() * resolutions.size() * configs.size() * args.iters);

    for (SceneKind sk : scenes) {
        for (Resolution res : resolutions) {
            for (VrsConfig cfg : configs) {
                for (u32 it = 0; it < args.warmup + args.iters; ++it) {
                    u32 seed = 0x1234u ^ (static_cast<u32>(sk) * 7919u) ^ (res.width * 31u + res.height) ^
                               (static_cast<u32>(cfg) * 31u) ^ it;
                    BenchResult r = run_one(sk, res, cfg, seed);
                    if (it >= args.warmup) results.push_back(r);
                }
            }
        }
    }

    std::printf("scene,res,vrs_cfg,covered_pixels_pct_mean,shader_invocations_mean,"
                "vrs_savings_pct_mean,vrs_image_bytes_mean,compute_gen_us_mean,apply_overhead_us_mean,"
                "total_us_mean,quality_risk_mean,edge_density_mean,n\n");

    struct Agg { u64 sum_cov = 0; u64 sum_inv = 0; f64 sum_savings = 0; u64 sum_bytes = 0;
                 f64 sum_gen = 0; f64 sum_apply = 0; f64 sum_total = 0; f64 sum_risk = 0; f64 sum_edge = 0;
                 u32 n = 0; };
    std::printf("\n=== Summary ===\n");
    for (SceneKind sk : scenes) {
        for (Resolution res : resolutions) {
            for (VrsConfig cfg : configs) {
                Agg a;
                for (const auto& r : results) {
                    if (r.scene == std::string(SceneKindName(sk)) && r.res == std::string(res.name()) &&
                        r.vrs_cfg == std::string(VrsConfigName(cfg))) {
                        a.sum_cov += static_cast<u64>(r.coverage_pct);
                        a.sum_inv += r.shader_invocations;
                        a.sum_savings += r.vrs_savings_pct;
                        a.sum_bytes += r.vrs_image_bytes;
                        a.sum_gen += r.compute_gen_us;
                        a.sum_apply += r.apply_overhead_us;
                        a.sum_total += r.total_us;
                        a.sum_risk += r.quality_risk;
                        a.sum_edge += r.edge_density;
                        ++a.n;
                    }
                }
                if (a.n == 0) continue;
                f32 cov_m = static_cast<f32>(a.sum_cov) / a.n;
                u64 inv_m = a.sum_inv / a.n;
                f32 sav_m = static_cast<f32>(a.sum_savings / a.n);
                u32 byt_m = static_cast<u32>(a.sum_bytes / a.n);
                f64 gen_m = a.sum_gen / a.n;
                f64 app_m = a.sum_apply / a.n;
                f64 tot_m = a.sum_total / a.n;
                f32 risk_m = static_cast<f32>(a.sum_risk / a.n);
                f32 edge_m = static_cast<f32>(a.sum_edge / a.n);
                std::printf("%s,%s,%s,%.2f,%llu,%.2f,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%u\n",
                            std::string(SceneKindName(sk)).c_str(),
                            std::string(res.name()).c_str(),
                            std::string(VrsConfigName(cfg)).c_str(),
                            cov_m, static_cast<unsigned long long>(inv_m), sav_m, byt_m,
                            gen_m, app_m, tot_m, risk_m, edge_m, a.n);
            }
        }
    }

    return 0;
}
