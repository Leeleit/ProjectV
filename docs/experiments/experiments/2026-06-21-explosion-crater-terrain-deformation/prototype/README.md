# crater_bench prototype

Standalone C++26 CPU prototype for sphere-SDF voxel crater formation benchmark.

**Maps to ProjectV:** see [`../README.md` §9](../README.md) — corresponds to crater formation
on 8³ chunks per `src/voxel/VoxelWorld.hpp:78` (chunkSize=8).

## Build (direct clang++)

```bash
cd prototype
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
  crater_bench.cpp -o build/crater_bench
```

## Build (CMake)

```bash
cd prototype/build
cmake .. -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

Expected: Clang 22.1.6 (per `hardware-profile.md §6`) or GCC 16.x, **build green 0 warnings** on
`-Wall -Wextra -Wpedantic`.

## Run

```bash
./crater_bench > build/results.csv 2>/tmp/crater_stderr.txt
```

Outputs:
- `build/results.csv` — 1505 lines = 3 intro + 1 empty + 1 header + 1500 data rows
  (300 configs × 5 strategies). Columns: `strategy,scene,seed,radius,position,carved_count,
  boundary_ok,time_us_mean,time_us_median,time_us_p95,time_us_p99,time_us_std`.
- stderr — per-strategy summary (300/300 boundary_ok, 0 mismatches / 153,600 voxel-checks).

Wall time: <1 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.

## Strategies (5)

- **A_NaivePerVoxel** — 3 nested loops, dist² < r² per voxel. **Baseline** (1.00×).
- **B_AABBPreFilter** — chunk-level AABB test (sphere ∩ chunk AABB) → skip if disjoint.
  **Does NOT help at 8³** (0.96× — overhead > savings).
- **C_BlockBased2x** — 2×2×2 = 8 sub-blocks (each 4³ = 64 voxels); test sub-block AABB
  (8 corners inside sphere → bulk set 0); partial overlap → per-voxel fallback.
  **1.33×** (good secondary).
- **D_BlockBased4x** — 4×4×4 = 64 sub-blocks (each 2³ = 8 voxels); finer granularity.
  **Does NOT help at 8³** (0.98× — AABB test cost > per-voxel savings).
- **E_RasterizedSphereMarch** ⭐ — per-column pre-skip (`xzd² > r² → continue`),
  pre-computed dx²/dz² hoisted, L1-cache-friendly 8-iter inner loop. **Universal winner
  (1.82× speedup vs A)**.

## Cross-vendor / GPU

CPU-only analytical prototype. GPU projection from literature (Unity mesh-to-sdf
0.18-0.22 ms at 32³ on RTX 3090/2080S — see [`../sources.md`](../sources.md) Tier 2).
Estimated GPU compute shader time: 0.05-0.10 µs/chunk (5-10× faster from SIMT parallelism).
At 1000 chunks affected per explosion cluster (5-10 explosions, radius 4, each touching
~10-30 chunks), GPU cost = 50-100 µs total per tick.
