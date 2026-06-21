# Prototype — `2026-06-21-vk-fragment-shading-rate-voxel`

Standalone CPU voxel rasterizer + VRS attachment simulator. **No Vulkan dependency** для first iteration.

## Build

```bash
# Requires: Clang 22.1.x (per `agent/knowledge.md` baseline) + CMake 3.28+ + C++26
cmake -B build -S . \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -std=c++26 -Wall -Wextra -Wpedantic -DNDEBUG"
cmake --build build -j$(nproc)
```

## Run

```bash
# Default: 4 scenes × 3 resolutions × 5 VRS configs × 1000 iter
./build/vrs_voxel_sim

# Custom: scene + resolution + VRS config
./build/vrs_voxel_sim --scene=mixed_biome --res=1440p --vrs=hybrid_2x2_lighting

# Smoke test (10 iter, quick)
./build/vrs_voxel_sim --smoke
```

Output: `results.csv` (machine-readable) + stdout summary (human-readable).

## Что измеряется

| Метрика                  | Единицы       | Что значит                                                                 |
|:-------------------------|:--------------|:---------------------------------------------------------------------------|
| `total_pixels`           | count         | Всего pixels в viewport для данного scene+res                              |
| `covered_pixels`         | count         | Pixels, покрытые геометрией (voxel surfaces) после projection              |
| `shader_invocations`     | count         | Effective fragment shader invocations (1x1 baseline vs VRS savings)        |
| `vrs_savings_pct`        | %             | `(baseline_invocations - vrs_invocations) / baseline_invocations * 100`     |
| `vrs_image_bytes`        | bytes         | VRAM cost для shading rate image attachment                                |
| `compute_gen_us`         | microseconds  | CPU proxy для compute shader generation VRS image (per-frame)              |
| `apply_overhead_us`      | microseconds  | CPU proxy для overhead VRS attachment setup + transition latency          |
| `total_us`               | microseconds  | `compute_gen_us + apply_overhead_us` — full per-frame VRS cost             |
| `quality_risk_score`     | dimensionless | 0-1, эвристика: blockiness × edge_density × high_freq_texture_factor      |

## Методология

- **Scenes:** 4 representative voxel scenes (uniform_open / forest_floor / cave_stress / mixed_biome).
- **Resolutions:** 3 (1080p / 1440p / 4K).
- **VRS configs:** 5 (baseline 1x1 / 2x1 / 1x2 / 2x2_global / hybrid_2x2_lighting_only).
- **Iterations:** 1000 per config (per `benchmarks/methodology.md §3`).
- **Warmup:** 10 iter (per §3).
- **CPU only:** CPU proxy для compute shader cost = per-tile derivative calc + state update. Wall-clock measured.
- **Quality:** эвристическая risk score (blockiness × edge_density), **not measured rendering quality** —
  this requires GPU prototype (deferred).

## Что НЕ покрыто (deferred до GPU prototype)

- **Реальные GPU timings** (rasterizer, fragment shader, memory bandwidth).
- **Visual quality** (PSNR, SSIM) — нужен GPU prototype с rendering pipeline.
- **Cross-vendor GPU measurement** (RDNA 2/3, Intel Arc) — нужен hardware matrix.
- **TAA feedback loop** interaction — separate experiment if VRS + TAA combined.
- **VR/foveation integration** — separate (`eye-tracked-foveated` backlog l-priority).
- **Compute shader actual cost** — CPU proxy used here; real GPU dispatch needed for Step 2 migration.

## Mapping to ProjectV

CPU prototype is **analytical** counterpart to potential GPU harness. Voxel scene rasterizer simulates what
Vulkan Rasterizer would do with greedy-meshing voxel quads (per closed `2026-06-20-meshing-algo-comparison`).
VRS attachment computation emulates `dFdx/dFdy`-based frequency detection + tile-classification logic per
Khronos Vulkan samples `fragment_shading_rate_dynamic`.

Direct mainline mapping target: Stage 5.x lighting passes (VCT + RTX hybrid per closed `vct-vs-rt-cutoff`).
Cross-references см. `README.md §9 Mapping to ProjectV hot-path`.
