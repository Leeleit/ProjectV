# 2026-06-21-eye-tracked-foveated — prototype

Standalone C++26 CPU foveation density map simulator. NOT ProjectV mainline.

## Build

```bash
cd docs/experiments/experiments/2026-06-21-eye-tracked-foveated/prototype
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic foveation_sim.cpp -o foveation_sim
```

Build flags per `hardware-profile.md §6` toolchain (Clang 22.1.6, libstdc++ 16.1.1).

## Run

```bash
./foveation_sim 2>&1 | tee run.log
```

Output:
- `build/results.csv` — 75 configs × 23 cols (1 header + 75 data rows)
- `run.log` — human-readable progress

## Measurement matrix

- **4 strategies:** A_None (baseline uniform 1x1) / B_FixedFoveation2x (center 30% 1x1 + periphery 2x2, no gaze) / C_GazeFoveation2x (gaze-driven 1x1 + 2x2 + 4x4) / D_GazeFoveation4x (gaze-driven aggressive, same algorithm as C for clarity)
- **5 scenes:** uniform_floor / forest_floor / cave_stress / mixed_biome / uniform_air (per `2026-06-21-sub-chunk-layers` precedent for direct comparability)
- **5 seeds:** 1, 7, 42, 1234, 31337
- **3 extents:** 1080p (1920×1080) / 1440p (2560×1440) / 4K (3840×2160)
- **Iterations:** 1000 + 10 warmup per config (per `benchmarks/methodology.md §3`)

**Total:** 4 × 5 × 5 × 3 = **75 configs** × 1000 = **75,000 main measurements**.

## Model

Each viewport is divided into **16×16 pixel tiles** (matches Vulkan min tile size on most hardware). Each tile gets a single **density value**:
- **1.0** = 1×1 fragment shading (every pixel gets a fragment invocation)
- **0.25** = 2×2 shading (1 fragment per 4 pixels)
- **0.0625** = 4×4 shading (1 fragment per 16 pixels)

Effective fragment count per viewport = sum over all tiles of (density × tile_pixel_count).

**Cost ratio** vs baseline = effective / total_pixels.
**Savings %** = (1 − cost_ratio) × 100.

## Synthetic gaze

Real gaze input would come from `XR_EXT_eye_gaze_interaction` (OpenXR 1.0+ ratified rev 2, 2024). For this CPU prototype we synthesize gaze position per-frame from a per-scene Gaussian distribution centered at (0.5, 0.5) with scene-dependent σ:
- uniform_floor / uniform_air: σ = 0.05 (calm, centered gaze)
- forest_floor / mixed_biome: σ = 0.10 (medium exploration)
- cave_stress: σ = 0.15 (active eye movement to walls/details)

Gaze clamped to [0.05, 0.95] (never at edge).

## Cross-vendor validation (analytical projection)

The model does not require GPU dispatch. Cross-vendor savings depend on whether the hardware supports the relevant density values. Per `NVK Mesa DeepWiki` and `NVIDIA Developer Vulkan Driver`:
- **NVIDIA Turing+ (RTX 3060 Ti = Ampere):** supports all densities 1×1, 2×2, 4×4 ✓
- **AMD RDNA 2+:** supports 1×1, 2×2, 4×4 ✓
- **Intel Arc Gfx12.5+:** supports 1×1, 2×2, 4×4 ✓
- **Mobile (Arm Mali, Qualcomm Adreno):** supports via `VK_QCOM_fragment_density_map_offset` (Tile Offset per Meta Quest ETFR production)

All vendors can realize the savings predicted by the model.

## Caveats

- CPU-only synthetic, no real GPU dispatch.
- Synthetic gaze (not real OpenXR `XR_EXT_eye_gaze_interaction` input).
- Per-fragment cost = constant (no ALU/memory simulation). Real cost depends on specific fragment shader (VCT vs TAA vs RTX shadow). For full cost analysis use the GPU prototype in `voxel.frag` (deferred to mainline integration).
- Tile size 16×16 matches Vulkan min but actual tile size is implementation-defined.
