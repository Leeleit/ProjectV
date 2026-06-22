# Prototype — SSS Benchmark

Standalone C++26 CPU analytical cost model for translucent voxel material subsurface scattering.

## Build

```bash
cd /home/le1t/Projects/ProjectV/docs/experiments/experiments/2026-06-21-subsurface-scattering-voxel-materials/prototype
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic -o build/sss_bench sss_bench.cpp
```

## Run

```bash
cd /home/le1t/Projects/ProjectV/docs/experiments/experiments/2026-06-21-subsurface-scattering-voxel-materials/prototype
./build/sss_bench
```

Output: `build/results.csv` (126 rows = 1 header + 125 data, 8.5 KB).

## What it measures

Per-fragment BSSRDF evaluation cost for 5 strategies across 5 material classes:

- **Strategies:** A_None (opaque Lambert baseline) / B_BeerLambert_Analytical / C_PrecomputedDipoleLUT (Jensen 2001) / D_MultipoleAnalytical (d'Eon 2011 3-pole) / E_ScreenSpaceSeparableDiff (Jimenez 2015 CPU proxy)
- **Materials:** human_skin / foliage_leaves / wax_candle / ice_block / blood_drop
- **Seeds:** 1, 7, 42, 1234, 31337
- **Iterations:** 1000 main + 10 warmup per config
- **Total:** 5 × 5 × 5 × 1000 = **125,000 main measurements**

## Hardware baseline

Zen 3 5800X governor=`powersave` per `docs/experiments/hardware-profile.md §1`. Wall time: <0.5 sec.

## Source code

- `sss_bench.cpp` (~355 LoC) — all 5 strategies, Jensen 2001 dipole formula, 32-sample LUT precomputation, d'Eon 2011 3-pole sum, Beer-Lambert, Jimenez 2015 CPU proxy.
- No external dependencies (just C++26 standard library).

## Reproducibility

The build command above produces bit-exact results for the same CPU and Clang version. PSNR is clamped at 60 dB (to avoid log(0) artifacts in identity cases). Per-fragment cost may vary by ±5% across runs due to governor fluctuation; means are stable within ±2%.
