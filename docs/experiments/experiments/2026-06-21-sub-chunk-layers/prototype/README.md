# Prototype — sub-chunk-layers benchmark

Standalone C++26 CPU prototype для 5 chunk layout designs.

## Files

- `sub_chunk_bench.cpp` (~870 LoC) — main benchmark: 5 chunk designs × 5 synthetic scenes × measurement
  harness + greedy meshing + CSV output.
- `CMakeLists.txt` — `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++`

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j$(nproc)
```

Requires `clang++ 22.1.6+` for full C++26 support per `hardware-profile.md §6`.

## Run

### Full sweep (5 scenes × 4 designs × 5 seeds × 1000 iter = 100 measurements):

```bash
./build/sub_chunk_bench --all --iters 1000 --seeds 5 --output build/results_all.csv
```

### Single config:

```bash
./build/sub_chunk_bench --scene mixed_biome --design C_FixedLayer_L2 \
    --iters 1000 --seeds 5 --output build/results_one.csv
```

### All scenes for single design:

```bash
./build/sub_chunk_bench --scene all --design B_Palette --output build/results_palette.csv
```

### Smoke test:

```bash
./build/sub_chunk_bench --scene mixed_biome --design C_FixedLayer_L2 \
    --iters 100 --seeds 1 --output build/results_smoke.csv --quiet
```

## Outputs

- `build/results_all.csv` — full per-config measurements (100 rows).
- `build/summary_means.csv` — aggregated means per scene × design (20 rows).
- Stdout per-config inline output (suppress with `--quiet`).

## CSV columns

`scene,design,seed,bytes_per_chunk,effective_bytes,build_us_mean,build_us_p95,build_us_stddev,mutate_us_mean,mutate_us_p95,mutate_us_stddev,mesh_quad_count,mesh_vertex_count,layer_boundary_count`

## Designs

| Design | Layout | bytes struct | Notes |
|:-------|:-------|-------------:|:------|
| `A_Monolithic` | `chunkSize³ × 1 byte` | 512 | ProjectV-like baseline. No palette, no layers. |
| `B_Palette` | `Palette[16] + adaptive bits/voxel` | 531 | Minecraft-1.18+ ChunkSection pattern. Adaptive 1/2/4/8 bits. |
| `C_FixedLayer_L2` | `4 layers × 8×2×8 voxels with per-layer palette` | 600 | 4 sub-layers per chunk (biome granularity). |
| `D_FixedLayer_L4` | `2 layers × 8×4×8 voxels with per-layer palette` | 568 | 2 sub-layers per chunk (coarser bands). |

## Scenes

| Scene | Materials | Description |
|:------|:---------:|:------------|
| `uniform_air` | 1 (Air) | All empty. Trivial case. |
| `uniform_floor` | 1 (FloorWhite) | Single-material slab. |
| `forest_floor` | 2 (FloorWhite, Glass) | 70% floor + 30% trees. |
| `cave_stress` | 2 (Air, FloorWhite) | 80% air + 20% stone (cave network). |
| `mixed_biome` | 4 (FloorWhite, Glass, FloorGray, Air) | Banded: Forest L=0-1, Stone L=2-3, Cave L=4-7. |

## Caveats (per `docs/experiments/benchmarks/methodology.md §3`)

- **No Vulkan / GPU dispatch** — CPU-only prototype. VRAM analysis = memory-layer only.
- **No sparse tree** — flat per-chunk arrays. Sparse64Tree integration separate.
- **Naive face counter** — no greedy merging. True greedy meshing per `2026-06-20-meshing-algo-comparison`
  verdict=mixed reduces face count further.
- **Synthetic scenes** — real player movement may favor different layer heights.
- **Single-threaded** — multi-threaded populate would benefit C/D designs.
- **CPU dev host:** Zen 3 5800X (8C/16T, governor=`powersave`), 62.7 GiB RAM per
  [`docs/experiments/hardware-profile.md`](../../../hardware-profile.md) §1+§2.
