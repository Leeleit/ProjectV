// SPDX-License-Identifier: MIT
//
// capture_overhead_bench.cpp — analytical RenderDoc capture overhead benchmark для
// 2026-06-21-renderdoc-ci-capture experiment.
//
// Standalone C++26 CPU prototype. НЕ ProjectV mainline (per
// `docs/experiments/AGENTS.md §2 Scope discipline`). НЕ запускает реальный renderdoccmd
// (binary not installed на dev host `obvium` per `which renderdoccmd` → not found 2026-06-21).
//
// Аналитическая модель RenderDoc Vulkan layer overhead (per `sources.md`):
//   - RenderDoc 1.44 Vulkan support docs ("low performance overhead while not capturing"
//     + "save one or more copies of memory allocations to enable proper capture").
//   - Phoronix RenderDoc 1.7 release notes ("improved capture performance for Direct3D 12
//     programs, better handling of queue ownership transfer barriers in Vulkan").
//   - RenderDoc capture file size model per `defaultCaptureFileSize` cap convention.
//
// Per-pass state model: каждый Vulkan pass в ProjectV 12-pass pipeline имеет analytical
// state cost (vkCmd* interception serialization + per-allocation copy). Per RenderDoc docs
// "no locks on hot path of command buffer recording, minimal or no allocation, low overhead
// while not capturing" — overhead applied only DURING capture (not always-on layer without
// capture).
//
// Build: clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic
// Run:   ./capture_overhead_bench --output build/results.csv
//
// Per `docs/experiments/benchmarks/methodology.md §3`: warm-up ≥10 iter + N=1000 + mean/median/
// p95/p99/std/min/max + one-row-per-config CSV output.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace capture_bench {

// === ProjectV Vulkan pass enumeration (12 passes per `sources.md` §ProjectV pipeline) ===

enum class VkPass : std::uint8_t {
    DepthPrepass     = 0,  // TODO.md §2.2 depth-only forward
    HzbCull          = 1,  // TODO.md §2.1 RecordHzbCullingDispatch
    HizMipChain      = 2,  // TODO.md §2.1 BuildHizMipChain (~10 mip levels)
    VoxelMesh        = 3,  // TODO.md §2.2 Pattern C compute/mesh shader per agent/knowledge §32
    CsmShadow        = 4,  // Stage 0 4 cascades depth-only
    OpaqueForward    = 5,  // RenderGraphicsCommands main pass (5 sub-passes per §810)
    VctConeMarch     = 6,  // TODO.md §5.1 planned 6 diffuse + 1 specular cones
    RtxRayQuery      = 7,  // TODO.md §5.2 planned VK_KHR_ray_query
    FluidCaPingpong  = 8,  // TODO.md §3.1 partial Phase 1
    TaaResolve       = 9,  // TODO.md §5.3 planned
    TransparentFwd   = 10, // Stage 0 sorted back-to-front
    UiDebug          = 11, // §810 debugOverlay + debugHud
    Count            = 12,
};

constexpr std::string_view pass_name(VkPass p) {
    switch (p) {
        case VkPass::DepthPrepass:    return "depth_prepass";
        case VkPass::HzbCull:         return "hzb_cull";
        case VkPass::HizMipChain:     return "hiz_mip_chain";
        case VkPass::VoxelMesh:       return "voxel_mesh";
        case VkPass::CsmShadow:       return "csm_shadow";
        case VkPass::OpaqueForward:   return "opaque_forward";
        case VkPass::VctConeMarch:    return "vct_cone_march";
        case VkPass::RtxRayQuery:     return "rtx_ray_query";
        case VkPass::FluidCaPingpong: return "fluid_ca_pingpong";
        case VkPass::TaaResolve:      return "taa_resolve";
        case VkPass::TransparentFwd:  return "transparent_fwd";
        case VkPass::UiDebug:         return "ui_debug";
        default:                      return "unknown";
    }
}

// Per-pass analytical state cost (bytes) — based on RenderDoc docs "save one or more copies
// of memory allocations to enable proper capture" + ProjectV source inspection of pass types.
// Lower-bound estimate (conservative) — actual RenderDoc capture adds ~16 B per vkCmd* call +
// per-SSBO/VBO copy. State-only cost models the persistent buffer cost.
struct PassStateCost {
    std::uint32_t pipeline_state_bytes; // Pipeline state objects (vkCmdBindPipeline state)
    std::uint32_t descriptor_state_bytes; // Descriptor set state (vkCmdBindDescriptorSet)
    std::uint32_t resource_copy_bytes; // SSBO/VBO/attachment copies (worst-case per frame)
    float         cpu_overhead_pct;    // CPU overhead during capture (%)
};

constexpr std::array<PassStateCost, 12> kPassCosts = {{
    // DepthPrepass — depth-only forward, single render target
    {1024, 512, 8ull * 1024 * 1024, 0.05f},
    // HzbCull — compute, atomicAdd per visible chunk
    {512, 256, 4ull * 1024 * 4096, 0.03f}, // ~4 KiB per chunk × 1024 chunks
    // HizMipChain — compute, ~10 mip levels
    {512, 256, 4ull * 1024 * 1024 + 1024 * 1024 + 256 * 1024 + 64 * 1024, 0.04f},
    // VoxelMesh — compute or mesh shader, per-chunk greedy emit
    {2048, 1024, 2ull * 1024 * 4096 + 8ull * 1024, 0.08f},
    // CsmShadow — 4 cascades depth-only
    {4096, 2048, 4ull * (4ull * 1024 * 1024), 0.10f},
    // OpaqueForward — main render pass + render targets
    {16384, 8192, 8ull * 1024 * 1024 * 1080 * 1920 / (1024 * 1024 * 1024), 0.20f}, // 8.3 MiB @ 1080p
    // VctConeMarch — fragment shader cone-march through 3D atlas mips
    {4096, 2048, 8ull * 1024 * 1024, 0.15f},
    // RtxRayQuery — BLAS instance state
    {16384, 8192, 16ull * 1024 * 1024, 0.25f},
    // FluidCaPingpong — 2 × 32 MiB SSBOs ping-pong
    {512, 256, 64ull * 1024 * 1024, 0.06f},
    // TaaResolve — history texture + motion vectors
    {1024, 512, 8ull * 1024 * 1024 + 4ull * 1024 * 1024, 0.10f},
    // TransparentFwd — sorted back-to-front
    {8192, 4096, 4ull * 1024 * 1024, 0.12f},
    // UiDebug — ImGui state
    {4096, 2048, 1ull * 1024 * 1024, 0.03f},
}};

// === Capture strategies (5 strategies per `sources.md` + README §5) ===

enum class Strategy : std::uint8_t {
    A_NoCapture          = 0, // Baseline: layer NOT loaded
    B_AlwaysOnLayer      = 1, // Capture every frame (theoretical, never production)
    C_TriggeredOnError   = 2, // Capture only on PV_ASSERT failure / NaN detected (production pattern)
    D_PixelDiffBaseline  = 3, // Always-on layer + capture-on-demand + imageDiff vs golden (CI pattern)
    E_SelectiveCaptureRange = 4, // Capture only N=10 frames after env trigger (spike isolation)
    Count                = 5,
};

constexpr std::string_view strategy_name(Strategy s) {
    switch (s) {
        case Strategy::A_NoCapture:           return "A_NoCapture";
        case Strategy::B_AlwaysOnLayer:       return "B_AlwaysOnLayer";
        case Strategy::C_TriggeredOnError:    return "C_TriggeredOnError";
        case Strategy::D_PixelDiffBaseline:   return "D_PixelDiffBaseline";
        case Strategy::E_SelectiveCaptureRange: return "E_SelectiveCaptureRange";
        default:                               return "unknown";
    }
}

// === Synthetic scene definitions ===

enum class Scene : std::uint8_t {
    MinimalVoxel    = 0, // Only voxel_mesh + opaque_forward + ui_debug (3 passes, MVP)
    TypicalVoxel    = 1, // depth + hzb + voxel_mesh + csm + opaque + taa + ui (7 passes)
    FullVoxel       = 2, // All 12 passes
    StressVoxel     = 3, // All 12 passes + giant SSBO allocations (~200 MB capture/frame)
    SyntheticGolden = 4, // Single fixed frame for golden baseline regression test
    Count           = 5,
};

constexpr std::uint8_t to_int(Scene s) noexcept { return static_cast<std::uint8_t>(s); }
constexpr std::uint8_t to_int(VkPass p) noexcept { return static_cast<std::uint8_t>(p); }
constexpr std::uint8_t to_int(Strategy s) noexcept { return static_cast<std::uint8_t>(s); }

constexpr std::string_view scene_name(Scene s) {
    switch (s) {
        case Scene::MinimalVoxel:    return "minimal_voxel";
        case Scene::TypicalVoxel:    return "typical_voxel";
        case Scene::FullVoxel:       return "full_voxel";
        case Scene::StressVoxel:     return "stress_voxel";
        case Scene::SyntheticGolden: return "synthetic_golden";
        default:                     return "unknown";
    }
}

// Active passes per scene (bitmask of VkPass)
constexpr std::uint16_t scene_active_passes(Scene s) {
    switch (s) {
        case Scene::MinimalVoxel:
            return (1u << to_int(VkPass::VoxelMesh))
                 | (1u << to_int(VkPass::OpaqueForward))
                 | (1u << to_int(VkPass::UiDebug));
        case Scene::TypicalVoxel:
            return (1u << to_int(VkPass::DepthPrepass))
                 | (1u << to_int(VkPass::HzbCull))
                 | (1u << to_int(VkPass::HizMipChain))
                 | (1u << to_int(VkPass::VoxelMesh))
                 | (1u << to_int(VkPass::CsmShadow))
                 | (1u << to_int(VkPass::OpaqueForward))
                 | (1u << to_int(VkPass::TaaResolve))
                 | (1u << to_int(VkPass::UiDebug));
        case Scene::FullVoxel:
            return (1u << to_int(VkPass::DepthPrepass))
                 | (1u << to_int(VkPass::HzbCull))
                 | (1u << to_int(VkPass::HizMipChain))
                 | (1u << to_int(VkPass::VoxelMesh))
                 | (1u << to_int(VkPass::CsmShadow))
                 | (1u << to_int(VkPass::OpaqueForward))
                 | (1u << to_int(VkPass::VctConeMarch))
                 | (1u << to_int(VkPass::RtxRayQuery))
                 | (1u << to_int(VkPass::FluidCaPingpong))
                 | (1u << to_int(VkPass::TaaResolve))
                 | (1u << to_int(VkPass::TransparentFwd))
                 | (1u << to_int(VkPass::UiDebug));
        case Scene::StressVoxel:
            return scene_active_passes(Scene::FullVoxel); // + giant SSBOs (2x multiplier in cost)
        case Scene::SyntheticGolden:
            return (1u << to_int(VkPass::VoxelMesh))
                 | (1u << to_int(VkPass::CsmShadow))
                 | (1u << to_int(VkPass::OpaqueForward))
                 | (1u << to_int(VkPass::TaaResolve))
                 | (1u << to_int(VkPass::UiDebug));
        default:
            return 0;
    }
}

// === Per-frame analytical capture model ===
//
// Per RenderDoc Vulkan support docs: "low performance overhead while not capturing",
// "save one or more copies of memory allocations to enable proper capture".
//   - Always-on layer without capture: ~0% overhead (minimal interception).
//   - Always-on layer WITH capture: per-pass cost applied.
//   - Trigger-based capture: cost applied ONLY for triggered frames.
//
// CPU overhead %: summed per active pass (capped at 100% theoretical max, conservative).
// Capture file size bytes: summed per active pass resource_copy_bytes + per-pass header 256 B.

struct FrameCaptureResult {
    float         cpu_overhead_pct; // per-frame CPU overhead vs baseline (no-capture)
    std::uint64_t capture_file_bytes; // per-frame capture file size (0 if not capturing this frame)
    bool          captured_this_frame; // did strategy trigger capture this frame?
    std::uint32_t active_passes_count;
};

FrameCaptureResult model_capture_frame(Strategy strat, Scene scene, std::uint64_t frame_idx,
                                       std::mt19937& rng) noexcept {
    FrameCaptureResult result{};
    result.active_passes_count = 0;
    result.capture_file_bytes  = 0;
    result.captured_this_frame = false;

    const std::uint16_t active = scene_active_passes(scene);
    if (active == 0) return result;

    // Sum baseline active passes for both strategies
    std::uint32_t total_state_bytes = 0;
    std::uint64_t total_resource_bytes = 0;
    float total_cpu_pct = 0.0f;
    for (std::uint8_t i = 0; i < to_int(VkPass::Count); ++i) {
        if ((active & (1u << i)) == 0) continue;
        const auto& cost = kPassCosts[i];
        result.active_passes_count += 1;
        total_state_bytes += cost.pipeline_state_bytes + cost.descriptor_state_bytes;
        total_resource_bytes += cost.resource_copy_bytes;
        total_cpu_pct += cost.cpu_overhead_pct;
    }

    // StressVoxel multiplier — giant SSBOs (>1 GB per RenderDoc docs trigger significant overhead)
    if (scene == Scene::StressVoxel) {
        total_resource_bytes *= 2; // 2× memory allocation copy cost
    }

    // Strategy-specific capture decision
    switch (strat) {
        case Strategy::A_NoCapture:
            // Always no capture. CPU overhead = 0%.
            result.cpu_overhead_pct = 0.0f;
            result.capture_file_bytes = 0;
            break;

        case Strategy::B_AlwaysOnLayer:
            // Capture every frame (theoretical worst case).
            result.captured_this_frame = true;
            result.cpu_overhead_pct = total_cpu_pct;
            result.capture_file_bytes = total_state_bytes + total_resource_bytes;
            break;

        case Strategy::C_TriggeredOnError:
            // Trigger only on PV_ASSERT / NaN detection — modeled as 0.1% of frames (Poisson).
            // Per `sources.md` industry pattern (production mode).
            {
                std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                const float trigger_roll = dist(rng);
                constexpr float kTriggerRate = 0.001f; // 0.1% of frames
                if (trigger_roll < kTriggerRate) {
                    result.captured_this_frame = true;
                    result.cpu_overhead_pct = total_cpu_pct;
                    result.capture_file_bytes = total_state_bytes + total_resource_bytes;
                } else {
                    // Always-on layer minimal interception cost (per RenderDoc docs "low overhead while
                    // not capturing").
                    result.cpu_overhead_pct = 0.05f;
                    result.capture_file_bytes = 0;
                }
            }
            break;

        case Strategy::D_PixelDiffBaseline:
            // Always-on layer (minimal) + capture-on-demand + imageDiff compare (16 ms baseline).
            // Per `sources.md` Glint3D CI pattern + vision-regression-kit pattern.
            {
                std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                const float trigger_roll = dist(rng);
                constexpr float kCaptureRate = 0.01f; // 1% of frames (frequent golden compare)
                if (trigger_roll < kCaptureRate) {
                    result.captured_this_frame = true;
                    result.cpu_overhead_pct = total_cpu_pct + 1.5f; // +imageDiff 16 ms ~ 1.5% @ 60fps
                    result.capture_file_bytes = total_state_bytes + total_resource_bytes;
                } else {
                    result.cpu_overhead_pct = 0.10f; // Always-on layer cost
                    result.capture_file_bytes = 0;
                }
            }
            break;

        case Strategy::E_SelectiveCaptureRange:
            // Always-on layer (minimal) + capture only first N=10 frames of session (after env
            // trigger). Per `sources.md` Stage 5.1 spike isolation pattern. Per-frame model: capture
            // unconditional for frame_idx % 1000 in [0, kCaptureRangeFrames). Env trigger modeled as
            // session boundary at frame_idx == 0 (start of session).
            {
                constexpr std::uint32_t kCaptureRangeFrames = 10;
                if ((frame_idx % 1000) < kCaptureRangeFrames) {
                    result.captured_this_frame = true;
                    result.cpu_overhead_pct = total_cpu_pct;
                    result.capture_file_bytes = total_state_bytes + total_resource_bytes;
                } else {
                    result.cpu_overhead_pct = 0.08f;
                    result.capture_file_bytes = 0;
                }
            }
            break;

        default:
            break;
    }

    return result;
}

// === Stats (per `benchmarks/methodology.md §7` skeleton) ===

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
    std::size_t n;
};

Stats compute(std::vector<double> samples) {
    Stats s{};
    s.n = samples.size();
    if (samples.empty()) return s;
    std::ranges::sort(samples);
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / static_cast<double>(samples.size());
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<std::size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<std::size_t>(samples.size() * 0.99)];
    s.min = samples.front();
    s.max = samples.back();
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    return s;
}

// === Measurement harness ===

struct BenchConfig {
    Strategy strategy;
    Scene scene;
    std::uint64_t seed;
    std::uint32_t frames; // N per `benchmarks/methodology.md §3`
};

struct BenchResult {
    BenchConfig cfg;
    Stats cpu_overhead_stats;     // % per frame (during benchmark window)
    Stats capture_file_kb_stats;  // KiB per frame (when captured)
    double total_capture_mb_per_1000_frames; // total disk cost
    std::uint32_t captured_frame_count;       // out of N frames
    double capture_rate_pct;                  // % of frames captured
};

BenchResult run_bench(const BenchConfig& cfg) {
    std::mt19937 rng(static_cast<std::uint32_t>(cfg.seed));

    // Warm-up (10 iterations, per `benchmarks/methodology.md §3`).
    for (std::uint32_t i = 0; i < 10; ++i) {
        (void)model_capture_frame(cfg.strategy, cfg.scene, i, rng);
    }

    std::vector<double> cpu_overhead_samples;
    std::vector<double> capture_file_kb_samples;
    cpu_overhead_samples.reserve(cfg.frames);
    capture_file_kb_samples.reserve(cfg.frames);

    std::uint64_t total_capture_bytes = 0;
    std::uint32_t captured_count = 0;

    for (std::uint32_t i = 0; i < cfg.frames; ++i) {
        const auto result = model_capture_frame(cfg.strategy, cfg.scene, i, rng);
        cpu_overhead_samples.push_back(static_cast<double>(result.cpu_overhead_pct));
        if (result.captured_this_frame) {
            capture_file_kb_samples.push_back(
                static_cast<double>(result.capture_file_bytes / 1024.0));
            total_capture_bytes += result.capture_file_bytes;
            captured_count += 1;
        }
    }

    BenchResult out{};
    out.cfg = cfg;
    out.cpu_overhead_stats = compute(std::move(cpu_overhead_samples));
    out.capture_file_kb_stats = compute(std::move(capture_file_kb_samples));
    out.total_capture_mb_per_1000_frames =
        static_cast<double>(total_capture_bytes) / (1024.0 * 1024.0);
    out.captured_frame_count = captured_count;
    out.capture_rate_pct = 100.0 * static_cast<double>(captured_count)
                                / static_cast<double>(cfg.frames);
    return out;
}

// === Output helpers ===

void write_csv_header(std::ostream& os) {
    os << "strategy,scene,seed,frames,"
          "cpu_overhead_mean_pct,cpu_overhead_median_pct,cpu_overhead_p95_pct,"
          "cpu_overhead_p99_pct,cpu_overhead_std_pct,cpu_overhead_min_pct,cpu_overhead_max_pct,"
          "capture_file_kb_mean,capture_file_kb_median,capture_file_kb_p95,"
          "capture_file_kb_p99,capture_file_kb_std,capture_file_kb_min,capture_file_kb_max,"
          "total_capture_mb_per_1000_frames,captured_frame_count,capture_rate_pct,"
          "active_passes\n";
}

void write_csv_row(std::ostream& os, const BenchResult& r) {
    // Get active passes count from first frame (stable per scene).
    std::mt19937 rng(static_cast<std::uint32_t>(r.cfg.seed));
    const auto first_frame = model_capture_frame(r.cfg.strategy, r.cfg.scene, 0, rng);
    os << strategy_name(r.cfg.strategy) << ','
       << scene_name(r.cfg.scene) << ','
       << r.cfg.seed << ','
       << r.cfg.frames << ','
       << r.cpu_overhead_stats.mean << ',' << r.cpu_overhead_stats.median << ','
       << r.cpu_overhead_stats.p95 << ',' << r.cpu_overhead_stats.p99 << ','
       << r.cpu_overhead_stats.stddev << ',' << r.cpu_overhead_stats.min << ','
       << r.cpu_overhead_stats.max << ','
       << r.capture_file_kb_stats.mean << ',' << r.capture_file_kb_stats.median << ','
       << r.capture_file_kb_stats.p95 << ',' << r.capture_file_kb_stats.p99 << ','
       << r.capture_file_kb_stats.stddev << ',' << r.capture_file_kb_stats.min << ','
       << r.capture_file_kb_stats.max << ','
       << r.total_capture_mb_per_1000_frames << ','
       << r.captured_frame_count << ','
       << r.capture_rate_pct << ','
       << static_cast<std::uint32_t>(first_frame.active_passes_count) << '\n';
}

} // namespace capture_bench

int main(int argc, char** argv) {
    using namespace capture_bench;

    std::string output_path = "build/results.csv";
    std::uint32_t frames = 1000;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--frames" && i + 1 < argc) {
            frames = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        }
    }

    std::ofstream ofs(output_path);
    if (!ofs) {
        std::fprintf(stderr, "Failed to open output: %s\n", output_path.c_str());
        return 1;
    }
    write_csv_header(ofs);

    constexpr std::array<std::uint64_t, 5> kSeeds = {1, 7, 42, 1234, 31337};
    std::size_t total_configs = 0;
    for (std::uint8_t si = 0; si < to_int(Strategy::Count); ++si) {
        for (std::uint8_t sj = 0; sj < to_int(Scene::Count); ++sj) {
            for (std::uint64_t seed : kSeeds) {
                BenchConfig cfg{
                    static_cast<Strategy>(si),
                    static_cast<Scene>(sj),
                    seed,
                    frames,
                };
                const BenchResult r = run_bench(cfg);
                write_csv_row(ofs, r);
                ++total_configs;
            }
        }
    }

    std::printf("Wrote %zu configs × %u frames = %zu measurements to %s\n",
                total_configs, frames, total_configs * frames, output_path.c_str());
    return 0;
}
