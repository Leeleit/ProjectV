# Prototype — surface micro-detail (single-file, no CMake needed)

**Single-source standalone C++26 CPU benchmark.** Compiles with one `clang++` invocation.

## Build

```bash
cd docs/experiments/experiments/2026-06-22-surface-micro-detail/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
    micro_detail_bench.cpp -o build/micro_detail_bench
```

## Run

```bash
./build/micro_detail_bench
```

Writes `build/results.csv` (one row per `(strategy, scene)`) and prints a one-line summary per row
to stdout. Wall time is typically <5 sec on Zen 3 5800X governor=`powersave` for the full
**5 strategies × 45 scenes × (10 warmup + 1000 main)** matrix = **225,000 main measurements**.

## Output schema

`build/results.csv` columns:

| Column | Description |
|:-------|:------------|
| `strategy` | A_None / B_WorldHash / C_TangentFBM2D / D_Worley2D / E_DerivativeNormal |
| `scene` | `<material>_<view_angle>_<roughness>` (e.g. `stone_45deg_rough0.5`) |
| `mean_ns_frag` | Mean per-fragment cost in nanoseconds (128×72 fragment buffer; 1080p projection is in `cost_pct_of_30hz_1080p` column) |
| `median_ns_frag` | Median |
| `p95_ns_frag` | 95th percentile |
| `p99_ns_frag` | 99th percentile |
| `std_ns_frag` | Standard deviation |
| `min_ns_frag` | Minimum |
| `max_ns_frag` | Maximum |
| `psnr_db_vs_a` | Mean per-fragment PSNR vs A_None reference render (dB) |
| `delta_e_2000_proxy` | Mean absolute L1 RGB difference × 100 (proxy) |
| `alu_inst_approx` | Analytical ALU instruction count per strategy (cross-validated with Auburn/FastNoiseLite README benchmarks) |
| `cost_pct_of_30hz_1080p` | Total per-frame ALU cost as % of 30 Hz × 1080p frame budget |

## Source layout

```
prototype/
├── height_field.hpp       # 5 strategy kernels (A_None, B_WorldHash, C_TangentFBM2D, D_Worley2D, E_DerivativeNormal)
├── lighting.hpp           # GGX/lambertian BRDF + tangent frame + perturb_normal + PSNR utilities
├── micro_detail_bench.cpp # main harness, scene builder, stats accumulator, CSV output
└── build/                 # output dir (created by build command above)
    ├── micro_detail_bench # binary
    └── results.csv        # output
```

## What it measures

**Per-fragment ALU cost** = wall time of one render of a 128×72 fragment buffer (a CPU-friendly
test surface; 1080p projection is in `cost_pct_of_30hz_1080p` column), divided by fragment count.
This is the *CPU* cost on Zen 3 5800X; GPU cost on RTX 3060 Ti is analytically
projected by multiplying by the `alu_inst_approx` column and a per-architecture IPC factor
(GA104 Ampere ~1.0 IPC for scalar ALU, ~2.0 for FMA, ~0.5 for transcendentals per `agent/knowledge.md` cross-vendor matrix precedent).

**PSNR vs A_None** is the analytical quality metric: for each scene, we render once with the strategy
applied, render once with A_None (no perturbation), and compute per-fragment PSNR. Higher = more
visual difference from the flat baseline. We do not produce an actual visual image — only the
per-fragment linear RGB. **A real GPU visual smoke test is reserved for mainline integration.**

## Why no GPU dispatch

This is a CPU-only prototype (per `2026-06-20-gpu-procedural-noise-compute-kernels` precedent:
CPU is sufficient to establish the per-call ALU cost of the algorithm itself, and the GPU
projection is straightforward). The full visual smoke test (per-fragment shader on a real
voxel face) is the **mainline integration step** (per `agent/knowledge.md` Step 2).
