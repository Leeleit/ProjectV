# Prototype — 2026-06-21-mesh-shader-mega-instancing

Standalone C++26 CPU analytical cost model. Builds in `build/`.

## Build

```bash
cd docs/experiments/experiments/2026-06-21-mesh-shader-mega-instancing/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
    -o build/mesh_shader_sim mesh_shader_sim.cpp
```

## Run

```bash
./build/mesh_shader_sim > build/results.csv
wc -l build/results.csv  # 125001 rows
```

## Output

`build/results.csv` (125,001 rows = 1 header + 125,000 data, ~5 MB):

```
strategy,scene,seed,iter,total_ms,cpu_ms,gpu_cull_ms,gpu_mesh_ms,gpu_raster_ms,vram_bytes,visible,psnr_db
A_TraditionalDrawIndexed,scattered_1k,1,0,34.415997,34.128731,0.000000,0.102386,0.184880,64000,700,8.00
A_TraditionalDrawIndexed,scattered_1k,1,1,40.558979,40.220438,0.000000,0.120661,0.217880,64000,700,8.00
...
```

## Files

- `mesh_shader_sim.cpp` — main benchmark harness (5 strategies × 5 scenes × 5 seeds × 1000 iter)
- `stats.hpp` — Stats helper per `benchmarks/methodology.md §3, §7`
- `scenes.hpp` — 5 representative military-sandbox scene configurations
- `strategies.hpp` — 5 strategy implementations (A_Traditional, B_ComputeCull,
  C_AmplificationShaderOnly, D_IndirectDrawMeshTasks_Generic, E_StaticBatch_Legacy)
- `build/mesh_shader_sim` — binary
- `build/results.csv` — measurements (125,001 rows)
- `README.md` — full experiment documentation
- `RESULTS.md` — summary statistics + analysis
- `sources.md` — 15+ verified web sources
- `STATUS.md` — experiment status

## Caveats (per RESULTS.md §8)

CPU analytical model. Per-strategy costs calibrated against validated production references
(GameDev.net 2024 + XRReady 2026 + DEV.to 2026 + AMD GDC 2024 + Vulkanised 2023). No Vulkan init
in scope (research-agent per `AGENTS.md §2`).
