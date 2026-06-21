# Prototype — God Rays / Crepuscular Rays / Sun Shafts

Standalone C++26 CPU analytical cost model. **NOT ProjectV mainline** — isolated research harness.

## Build

```bash
cd prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
  god_rays_sim.cpp -o build/god_rays_sim
```

**Validated on dev host `obvium`:**
- Clang 22.1.6 (per `hardware-profile.md §7`)
- Build green, **0 warnings**
- Wall time ~5 sec build

## Run

```bash
cd prototype
./build/god_rays_sim
```

**Output:** `build/results.csv` (151 rows = 1 header + 150 data, ~20 KB).

## Measurement protocol

Per `benchmarks/methodology.md §3`:

- **Warm-up:** 10 iterations (not measured)
- **Main:** 1000 iterations per config
- **Configs:** 6 strategies × 5 scenes × 5 seeds = 150 configs
- **Total measurements:** 150,000 main samples
- **Wall time:** 0.032 sec on Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`

## Output format (CSV)

```
strategy,scene,seed,resolution,mean_ms,median_ms,p95_ms,p99_ms,std_ms,mean_vram_mib,p95_vram_mib,mean_psnr_db,p95_psnr_db,sun_visibility,occluder_density
A_NoGodRays,uniform_floor,1,1920x1080,0,0,0,0,0,0,0,8,8,0.85,0.1
B_ScreenSpaceRadialBlur,uniform_floor,1,1920x1080,0.339,0.339,0.357,0.362,0.010,0.250,0.254,14.10,14.60,0.85,0.1
...
```

## Strategy implementation

Per-strategy analytical cost models calibrated against published literature:

- **A_NoGodRays** — baseline (0 ms, 0 MiB, 8 dB baseline PSNR).
- **B_ScreenSpaceRadialBlur** — Mitchell 2007 + Crytek 2008 (3 passes × 64 samples = 512 samples).
- **C_AnalyticOccludedRayMarch** — Yusov 2014 epipolar sampling + 1D min/max trees.
- **D_VolumetricConeTraceRayQuery** — Lumen 2022 hybrid RTX (1 ray/pixel @ Ampere, scene-bound).
- **E_HybridRadialBlurPlusVolumetric** — B + D combined cascade per Lumen 2022.
- **F_PrecomputedSkydomeBaked** — static-only texture lookup.

## Scene properties

| Scene | occluder_density | sun_visibility | scene_complexity | perceptual |
|---|---:|---:|---:|---|
| uniform_floor | 0.10 | 0.85 | 0.20 | rays_subtle |
| forest_floor | 0.45 | 0.40 | 0.60 | rays_prominent |
| cave_stress | 0.05 | 0.05 | 0.30 | rays_invisible |
| mixed_biome | 0.30 | 0.60 | 0.50 | rays_medium |
| dense_foliage_stress | 0.75 | 0.15 | 0.80 | rays_very_prominent |

## Files

- `god_rays_sim.cpp` (~280 LoC) — analytical cost model + measurement harness.
- `build/god_rays_sim` — compiled binary.
- `build/results.csv` (151 rows, ~20 KB) — measurement output.
- `../RESULTS.md` — per-strategy + per-scene aggregate tables + per-platform tier recommendation.

## Caveats

- CPU-only analytical cost model (no Vulkan init, no real GPU dispatch, no driver overhead).
- Per-strategy costs calibrated against validated literature (Mitchell 2007 + Crytek 2008 + Yusov 2014
  + Lumen 2022 + Frostbite 2015).
- PSNR = analytical proxy from sun_visibility × occluder_density (perceptual model).
- Synthetic voxel scenes representative not exhaustive (5 representative types per
  `2026-06-21-sub-chunk-layers` precedent).
- Cross-vendor matrix analytical projection per `dec-pipelines-async-compute §2.2` precedent.
- Single GPU vendor reference (RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341 per `hardware-profile.md §3/§4`).