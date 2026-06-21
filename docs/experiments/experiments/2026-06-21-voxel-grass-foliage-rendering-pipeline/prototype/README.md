# Prototype — voxel-grass-foliage-rendering-pipeline

Standalone C++26 CPU analytical cost model для grass/foliage rendering pipeline. **NOT**
linked to ProjectV mainline. Build dir = `./build/` per `experiments/AGENTS.md §2`.

## Files

- `grass_bench.cpp` — 6 strategies × 6 biomes × 5 seeds × 1000 iters analytical model
  (~370 LoC, Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`,
  build green **0 warnings, 0 errors**).
- `build/grass_bench` — compiled binary.
- `build/results.csv` — 181 rows (1 header + 180 data, 36 unique configs).

## Build + run

```bash
cd prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
        -o build/grass_bench grass_bench.cpp
./build/grass_bench
# Output: build/results.csv + console summary (strategy + biome tables)
```

Wall time: ~5 ms for 180,000 main measurements on dev host `obvium` Zen 3 5800X
governor=`powersave` per `hardware-profile.md §1`.

## What it measures

For each (biome, strategy) config:
- **placement_ns** — per-chunk scan / GPU compute placement
- **frustum_cull_ns** — per-blade frustum test (compute-placement strategies only)
- **vertex_shader_ns** — per-vert grass vert shader
- **raster_ns** — per-triangle rasterization
- **pixel_shade_ns** — per-pixel grass frag shader
- **wind_ns** — per-blade wind animation
- **mesh_dispatch_ns** — per-mesh-shader-work-group launch (E/F only)
- **vram_bytes_total** — per-chunk positions + per-scene textures
- **pct_of_30hz_budget** — total_ns / 33,333,333
- **quality_score** — 0..1 normalized (0 = billboard, 1.0 = mesh-shader Bezier with wind + LOD)

See `../README.md §5 Results` for the full headline findings.

## SOTA sources (verified)

Cost coefficients calibrated against (see `../sources.md`):
- AMD GPUOpen "Procedural grass rendering" (March 2024)
- rcm7133/Modern-Grass-Rendering (Unity URP, Jan 2026)
- NVIDIA GPU Gems Ch 7 "Countless Blades of Waving Grass" (Pelzer 2004)
- NVIDIA GPU Gems 3 Ch 6 "GPU-Generated Procedural Wind Animations for Trees" (Zioma 2008)

## Caveats (per `benchmarks/methodology.md §6`)

- CPU analytical model only — no Vulkan init / GPU dispatch / driver overhead.
- Per-vert / per-tri / per-pixel cost coefficients calibrated against SOTA; real-world
  may vary ±2x.
- No visual QA — quality score is analytical, not perceptual.
- No mutation cost (out of Stage 5.x scope).
- Per-patch dispatch overhead is a projection (800 ns median per Vulkanised 2023).
