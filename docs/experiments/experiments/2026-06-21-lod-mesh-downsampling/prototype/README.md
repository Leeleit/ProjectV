# Prototype — lod-mesh-downsampling benchmark

Standalone C++26 CPU prototype для **Stage 4.2 chunk 2 (LOD uniform downsampling)**. 4 downsample
kernels × 3 stitch strategies × 5 scenes × 4 LOD levels = 1200 main measurements + 75 T-junction
detection measurements.

## Files

- `lod_bench.cpp` (~840 LoC) — main benchmark: 4 kernels + 3 stitchers + 5 synthetic scenes +
  T-junction detector + Stats + CSV output.
- `CMakeLists.txt` — `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++`

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j$(nproc)
```

Requires `clang++ 22.1.6+` for full C++26 support per `hardware-profile.md §6`.

## Run

### Full sweep (4 kernels × 3 stitchers × 5 scenes × 4 LOD levels × 5 seeds = 1200 measurements + 75 T-junction):

```bash
./build/lod_bench --all --iters 1000 --warmup 10 --seeds 5 \
    --output build/results.csv --quiet
```

### Single config:

```bash
./build/lod_bench --scene mixed_biome --kernel B --stitch Z --lod 1 \
    --iters 1000 --output build/results_one.csv
```

### T-junction only (75 measurements):

```bash
./build/lod_bench --tjunc-only --output build/tjunc_only.csv
```

## Outputs

- `build/results.csv` — per-config measurements (1200 rows + header).
- `build/results_tjunc.csv` — T-junction hole counts (75 rows + header).
- Stdout per-config inline output (suppress with `--quiet`).

## CSV columns

### results.csv

`scene,kernel,stitch,lod,seed,downsample_us_mean,downsample_us_p95,downsample_us_stddev,mesh_quad_count_total,mesh_quad_count_interior,mesh_quad_count_boundary,mesh_vertex_count_total`

### results_tjunc.csv

`scene,kernel,lod,seed,hole_count,boundary_face_count,hole_ratio`

## Designs

### Kernels (4)

| Kernel | Rule | Visual character |
|:-------|:-----|:-----------------|
| `A_Majority3D` | Most common material wins; tie-break prefer non-Air | Balanced, lossy at boundaries |
| `B_SurfacePreserve` | If all same, output that; else majority of non-Air only | Conservative, **preserves surface** |
| `C_SolidOnly` | Output Air unless ALL 8/64/512 are non-Air; else majority | Aggressive shrink, **collapses caves** |
| `D_MaxPool` | Output non-Air if ANY is non-Air; else Air; material = majority | Aggressive fill, **fills gaps** |

### Stitchers (3)

| Stitcher | Behavior at chunk boundary |
|:---------|:--------------------------|
| `X_None` | Standard re-mesh after downsample; accept T-junctions (baseline) |
| `Y_TJunctionPad` | Pad each boundary face by 1/2 voxel toward higher-LOD neighbor (Z-fight risk) |
| `Z_NeighborLocked` | Re-emit boundary faces to match higher-LOD neighbor's face layout (no T-junction) |

### Scenes (5)

| Scene | Material distribution | Description |
|:------|:---------------------|:------------|
| `uniform_air` | 100% Air | Trivial case (zero quads) |
| `uniform_floor` | 100% FloorWhite | Solid slab (max quads at boundary) |
| `forest_floor` | 70% FloorWhite + 30% Glass (trees) | Mixed solid |
| `cave_stress` | 80% Air + 20% FloorWhite | **Cave network — kernel-differentiating scene** |
| `mixed_biome` | 4 materials banded by Y (Forest / Stone / Cave) | Multi-material stratified |

### LOD levels (4)

| LOD | Size | Voxel count | Triangle reduction vs LOD 0 |
|:----|:-----|:------------|:-----------------------------|
| 0 | 8³ | 512 | 1× (baseline) |
| 1 | 4³ | 64 | 5.94× (geometric lower bound 4×) |
| 2 | 2³ | 8 | 31.8× (geometric lower bound 16×) |
| 3 | 1³ | 1 | 169× (geometric lower bound 64×) |

## Caveats (per `docs/experiments/benchmarks/methodology.md §3`)

- **No Vulkan / GPU dispatch** — CPU-only prototype. GPU downsample of the SSBO
  (`world_gen.comp` or `voxel_mesh.comp`) would use the same kernel logic at the
  compute stage; cross-vendor validation deferred to a separate Stage 4.2 GPU integration
  experiment (per `sub-chunk-layers` precedent: CPU prototype first, GPU second).
- **No sparse tree** — flat per-chunk arrays. Sparse64Tree / NanoVDB integration separate.
- **Naive face counter** — no greedy merging. True greedy meshing per
  `2026-06-20-meshing-algo-comparison` verdict=mixed reduces face count further but
  is layout-orthogonal (same relative ratios).
- **Synthetic scenes** — real player movement may favor different kernel choices.
- **Stitch cost not isolated in CSV** — measured downsample only; the mesh+stitch
  step is in the same iteration loop but its wall time is comparable to or less
  than the downsample (~1-2 µs for LOD 1).
- **Naive mesh_ext boundary classification** — for X_None and Y_TJunctionPad,
  `neighbor_voxels` is `nullptr` (no actual stitch operation performed, just
  counted the same as a standard naive mesh). For Z_NeighborLocked, neighbor_voxels
  is the LOD-0 chunk with a different seed (representative of "the closer neighbor
  is available").
- **T-junction detector** is a one-sided check (high-LOD vs low-LOD) and counts
  every mismatch as a hole. In practice, some "holes" are not visible if the
  camera angle doesn't expose them.
- **CPU dev host:** Zen 3 5800X (8C/16T, governor=`powersave`), 62.7 GiB RAM per
  [`docs/experiments/hardware-profile.md`](../../../hardware-profile.md) §1+§2.
