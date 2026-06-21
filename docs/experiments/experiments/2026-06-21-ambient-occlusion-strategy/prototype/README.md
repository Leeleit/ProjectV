# Prototype — `ao_sim.cpp`

Standalone C++26 CPU Ambient Occlusion Strategy simulator.

## Build

```bash
cd docs/experiments/experiments/2026-06-21-ambient-occlusion-strategy/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
  ao_sim.cpp -o build/ao_sim
```

## Run

```bash
./build/ao_sim           # default: 1000 iter + 10 warmup = 175,000 measurements
./build/ao_sim 100 10     # custom iter + warmup (for quick smoke test)
```

## Output

- `build/results.csv` — 175 rows = 1 header + 7 strategies × 5 scenes × 5 seeds averaged
  - Columns: strategy, scene, seed, cost_mean_ms, cost_median_ms, cost_p95_ms, cost_p99_ms,
    cost_std_ms, cost_min_ms, cost_max_ms, psnr_db, darkening_consistency, vram_mib, n_iter
- Console log: progress messages to stderr

## Hardware baseline

- **CPU:** Zen 3 5800X dev host `obvium`, governor=`powersave` per `hardware-profile.md §1`
- **GPU ref (analytical):** RTX 3060 Ti GA104, 14.7 TFLOPS FP32 / 448 GB/s GDDR6 per `hardware-profile.md §3`
- **Vulkan extension ref:** `VK_KHR_ray_query` rev 1 + `VK_KHR_acceleration_structure` rev 13 per `hardware-profile.md §4`

## Method

7 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **175,000 main measurements**.

- **Cost model:** analytical per-strategy shader cost, calibrated to RTX 3060 Ti reference (14.7 TFLOPS / 448 GB/s @ 1080p).
  Sources: Crassin 2011 GIVoxels §6 + Imagination Tech 2021 Vulkan SSAO article + Jimenez 2016 GTAO + MircoWerner 2023 VDCAO.
- **PSNR model:** analytical projection per published paper measurements (Crassin 2011 Fig. 13 + Jimenez 2016 GTAO Fig. 7 + MircoWerner 2023).
- **Darkening consistency:** synthetic voxel corner/crevice detection + GT-AO brute-force ray-march (32 directions × 24 steps per voxel).
- **VRAM overhead:** half-res R8G8 UNORM AO target + bent-normal R8G8B8A8 optional.

## Caveats

- CPU-only synthetic, no real GPU dispatch.
- Quality model = analytical PSNR projection, not real framebuffer measurement.
- Synthetic voxel scenes representative, NOT real ProjectV chunk content.
- Cross-vendor matrix = analytical projection, single vendor measured (NVIDIA RTX 3060 Ti).
- Mutation cost out of scope.
- Visual QA in real gameplay required to confirm quality.