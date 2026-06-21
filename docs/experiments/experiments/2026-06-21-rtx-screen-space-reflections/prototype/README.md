# 2026-06-21-rtx-screen-space-reflections

Standalone C++26 CPU reflection cost simulator (no Vulkan init, no GPU dispatch).

## Build

```bash
cd prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
    reflection_sim.cpp -o build/reflection_sim
```

Build verified green, **0 warnings** (Clang 22.1.6).

## Run

```bash
./build/reflection_sim
```

**Outputs:**

- `build/results.csv` (175,001 rows = 1 header + 175,000 data rows)
- `build/run.log` (per-strategy + per-scene summary)
- `build/reflection_sim` (binary, ~45 KB)

**Wall time:** 0.14 sec on Zen 3 5800X (governor=`powersave` per `hardware-profile.md §1`).

## What it measures

Per `benchmarks/methodology.md §3`:

- 7 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **175,000 main measurements**
- Stats: mean / median / p95 / p99 / std / min / max per (strategy, scene)
- CSV per row: `(strategy, scene, seed, iter, cost_ms, psnr_db, reflection_completeness, vram_mib)`

## Cost model calibration

Reference: **RTX 3060 Ti GA104 Ampere** (`hardware-profile.md §3`):

- 14.7 TFLOPS / 448 GB/s memory bandwidth
- 38 RT cores, 1-2 rays/pixel limited per closed `2026-06-20-rt-shadows-vs-csm` mixed
- D_RT_1Ray base cost = 1.5 ms @ 1080p
- Subgroup compaction savings = 15% per Iago Calvo Lista 2026 (Vulkanised)

PSNR calibrated to:

- Yu 2016 (SSR HiZ trace): 24-28 dB analytical
- Wolfenstein Youngblood GDC 2019: 32-38 dB single-ray RT
- Stachowiak 2015 (stochastic SSR): 35-42 dB 4-ray GGX
- Lumen SIGGRAPH 2022 (hybrid screen + RT): 30-35 dB

## Scene profiles

Per `2026-06-21-sub-chunk-layers` precedent scene definitions:

| Scene          | SSR coverage | RT tri count | VCT fallback ratio | Mean roughness | Metal ratio | Temporal bonus |
|----------------|--------------|--------------|--------------------|----------------|-------------|----------------|
| uniform_floor  | 80%          | 1.2K         | 30%                | 0.45           | 10%         | +5.0 dB        |
| uniform_air    | 0%           | 1.0K         | 50%                | 0.60           | 5%          | +3.0 dB        |
| forest_floor   | 60%          | 1.4K         | 50%                | 0.55           | 25%         | +4.0 dB        |
| cave_stress    | 25%          | 2.0K         | 70%                | 0.40           | 35%         | +6.0 dB        |
| mixed_biome    | 50%          | 1.5K         | 40%                | 0.50           | 30%         | +5.0 dB        |

## Cross-vendor matrix projection

| Vendor | RTX-class RT | `VK_KHR_ray_query` | `VK_KHR_acceleration_structure` |
|--------|---------------|--------------------|---------------------------------|
| NVIDIA RTX 3060 Ti (dev) | 1-2 rays/pixel | ✅ rev 1 | ✅ rev 13 |
| NVIDIA RTX 40 Ada | 2-4 rays/pixel | ✅ | ✅ |
| NVIDIA RTX 50 Blackwell | 4-12 rays/pixel (2× Ada) | ✅ | ✅ |
| AMD RDNA 2 | No HW RT | ❌ | ⚠️ |
| AMD RDNA 3/4 | Native | ✅ (Mesa RADV 2024-2025) | ✅ |
| Intel Arc Battlemage Xe2 | Full (SIMD16) | ✅ (Mesa ANV 2025+) | ✅ |
| Mobile (Mali/Adreno) | Software fallback | ⚠️ (`VK_QCOM_tile_shading`) | ⚠️ |

## Caveats

- CPU prototype, no real GPU dispatch
- Cost model = analytical projection per closed `rt-shadows-vs-csm` mixed
- PSNR = analytical model from published paper measurements
- Synthetic voxel scenes = 5 representative types per `sub-chunk-layers` precedent (not exhaustive)
- Single GPU vendor measurement (RTX 3060 Ti) + analytical cross-vendor projection
- Mutation cost (per-frame SSR rebuild on voxel edit) out of scope
- `voxel.frag` requires bent-normal + tangent frame for D/E/F (out of scope)

## See also

- [`../README.md`](../README.md) — experiment overview, hypothesis, verdict, integration recommendation
- [`../sources.md`](../sources.md) — 15 primary + 10 supplementary web-research sources
- [`../STATUS.md`](../STATUS.md) — current state + progress log
- [`../RESULTS.md`](../RESULTS.md) — per-strategy detailed analysis