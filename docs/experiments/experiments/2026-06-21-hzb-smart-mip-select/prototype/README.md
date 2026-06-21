# Prototype — HZB Smart Mip Selection benchmark

Standalone C++26 CPU cull simulator.

## Build

```bash
cd prototype
mkdir -p build && cd build
cmake .. -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j$(nproc)
```

Compiler flags (per `agent/knowledge.md §17`):
- Clang 22.1.6
- `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`

## Run

```bash
./hzb_smart_mip_bench \
  --scenes uniform_floor,forest_floor,cave_stress,mixed_biome,view_dolly_stress \
  --seeds 1,7,42,1234,31337 \
  --strategies A_UniformMip0,B_UniformMipGlobal,C_PerChunkStaticMip,D_PerChunkDynamicDispatch \
  --iter 1000 \
  --warmup 10 \
  --output ../results.csv
```

## What it measures

Per chunk, per strategy:
- `culled` (0/1) — does HZB cull this chunk?
- `actually_visible` (0/1) — ground truth (camera-raycast against synthetic depth)
- `false_negative` (0/1) — `culled && actually_visible` (must be 0)
- `mip_level_used` (int) — which mip level
- `texels_touched` (int) — GPU cost proxy (texelFetch iterations)
- `compute_ns` (int) — CPU-side simulated dispatch cost (analytical)

Aggregate per scene × strategy:
- `cull_rate` — fraction of chunks culled
- `false_negative_count` — total holes (must be 0)
- `total_texels_touched` — total GPU bandwidth
- `mean_compute_us` — mean dispatch cost
- `PSNR` — computed vs ground-truth visibility (∞ if no false-negatives)

## Output

CSV (`results.csv`):

```csv
scene,seed,strategy,chunk_count,culled_count,false_negative_count,cull_rate,total_texels_touched,mean_compute_us,psnr_db
uniform_floor,1,A_UniformMip0,1024,512,0,0.5,...
```

Per-row `false_negative_count` validation: **must be 0 across all rows**. If any row > 0, strategy broken (false cull = holes).

## Scenes

See `scenes.hpp`:
- `uniform_floor` — 1024 chunks, all at z=0 (homogeneous terrain)
- `forest_floor` — 1024 chunks with varied heights, occlusion-heavy
- `cave_stress` — 2048 chunks, many occluded behind thin walls (worst-case false-negative risk)
- `mixed_biome` — 1024 chunks, mixed near + far (typical gameplay)
- `view_dolly_stress` — 1024 chunks, camera dollies forward rapidly (per-frame mip recompute stress)
