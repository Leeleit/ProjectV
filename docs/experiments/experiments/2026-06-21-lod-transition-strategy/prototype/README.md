# Prototype Build — `lod_transition_bench.cpp`

Standalone C++26 CPU prototype. No external dependencies beyond stdlib.

## Build

```bash
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
  lod_transition_bench.cpp -o lod_transition_bench
```

## Run

```bash
./lod_transition_bench
# Outputs results.csv in current directory
```

## What it measures

5 strategies × 5 scenes × 5 seeds × 1000 iter:
- **build_us**: time to build the LOD transition bundle (per chunk)
- **memory_bytes**: VRAM cost per chunk
- **triangle_count**: triangles per chunk in transition zone
- **psnr_db**: PSNR vs continuous-LOD reference (Hoppe 1997 formula)
- **vertex_disc_max**: max per-vertex discontinuity in transition zone (voxels)

## Strategies

- **A_Pop**: discrete jump at t>=0.5 (current ProjectV pattern)
- **B_Crossfade**: alpha blend LOD 0 + LOD 1 (doubled triangle count)
- **C_Geomorph**: per-vertex interpolation between LOD 0 and LOD 1 (Hoppe 1997)
- **D_PreComputedMorphTargets**: pre-baked per-vertex delta vectors
- **E_HZB_Stitch**: HZB-aware conservative Z-test (ProjectV-specific)
