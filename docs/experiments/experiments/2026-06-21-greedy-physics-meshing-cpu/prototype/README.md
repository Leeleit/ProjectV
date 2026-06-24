# greedy_physics_bench — Greedy Physics Meshing CPU benchmark

Standalone C++26 CPU prototype для `2026-06-21-greedy-physics-meshing-cpu` experiment. **Изолирован от
ProjectV mainline** (no Vulkan, no Jolt, no GLSL) — реализует только **merge → AABB list** часть
(mainline `BuildStaticVoxelCollisionBody` заменяется на merge dispatch).

## Build

```bash
# Direct clang++ (faster):
cd docs/experiments/experiments/2026-06-21-greedy-physics-meshing-cpu/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
    -o greedy_physics_bench greedy_physics_bench.cpp

# Or via CMake:
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

Требования: Clang 22.1.6+ (per `agent/knowledge.md`), C++26 mode.

## Run

```bash
# Default: 6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup
# = 150 configs × 1000 = 150,000 main measurements
./greedy_physics_bench --all > results.csv

# Filter
./greedy_physics_bench --strategy=A_Naive,D_3D --scene=uniform_floor,cave_stress --iters=5000

# Custom output
./greedy_physics_bench --all --output=build/results_full.csv
```

Per `benchmarks/methodology.md §3`: запускать с `taskset -c 2` для isolated core + governor `powersave`
consistent с `hardware-profile.md §1`.

```bash
taskset -c 2 ./greedy_physics_bench --all > results.csv
```

## Output format (CSV)

```csv
strategy,scene,seed,solid_count,shape_count,volume_emitted,volume_expected,volume_match_pct,shape_reduction_ratio,build_us_mean
A_Naive,uniform_floor,1,64,64,64,64,100.0000,1.0000,0.42
B_1DZ,uniform_floor,1,64,8,64,64,100.0000,0.1250,0.38
...
```

**Column meanings:**

- `strategy` — strategy name (A_Naive / B_1DZ / C_2DXZ / D_3D / E_Octree / F_TwoPass)
- `scene` — scene name (uniform_floor / uniform_half / forest_floor / cave_stress / mixed_biome)
- `seed` — RNG seed (1, 7, 42, 1234, 31337)
- `solid_count` — total solid voxels in input chunk
- `shape_count` — AABB count emitted by strategy (= N for A_Naive, < N for greedy)
- `volume_emitted` — sum of AABB volumes (in voxel units)
- `volume_expected` — solid_count (= 1 voxel = 1 unit volume)
- `volume_match_pct` — 100 × volume_emitted / volume_expected. **MUST be 100.0%** (else false positive/negative).
- `shape_reduction_ratio` — shape_count / solid_count. **Lower = better.** DoD `TODO.md §3.3`:
  ≤ 0.25 (= ≥ 4× reduction) на typical terrain.
- `build_us_mean` — mean per-call wall time (µs), averaged over iters.

## Strategies (6)

| ID | Name | Algorithmic class | Expected reduction | Expected cost |
|:---|:-----|:------------------|:-------------------|:--------------|
| A  | Naive (baseline) | per-voxel loop | 1.0× | O(N), fastest |
| B  | 1D Z-axis merge | 1D run-length scan | 2-4× | O(N) |
| C  | 2D XZ per Y | Lysenko per-axis 2D | 4-16× | O(N) per Y |
| D  | 3D full greedy | Mikola-Lysenko 3D extension | 8-32× | O(N^2) worst case |
| E  | Hierarchical octree | top-down recursive | 4-16× | O(N log N) |
| F  | TwoPass | C + vertical merge | 4-32× | O(N) + O(K^2) post |

## Scenes (5)

| ID | Name | Pattern | Strategy test focus |
|:---|:-----|:--------|:---------------------|
| 1 | uniform_floor | 8×8 solid layer at Y=0 | A=64, B=8 (best case 1D) |
| 2 | uniform_half | 4×4×4 solid bottom half | E=8 (octree leaf), D=1 (full 3D) |
| 3 | forest_floor | 3 Y-levels + 4 random pillars | mixed: 2D floor + 1D pillars |
| 4 | cave_stress | shell + 3 random chambers | fragmented: D/E worst case |
| 5 | mixed_biome | stone/grass/glass layers | non-uniform Y per material |

## Reading the results

**Primary metric:** `shape_reduction_ratio` < 0.25 (≥ 4×) per `TODO.md §3.3` DoD.

**Sanity check:** `volume_match_pct` = 100.0 (any deviation = false positive/negative merge = collision bug).

**Secondary metric:** `build_us_mean` < 200 µs (vs 50 µs Stage 4.1 budget per `TODO.md §4.1`).

**Composite winners:**

- D_3D if `shape_reduction_ratio` < 0.10 AND `build_us_mean` < 100 µs
- E_Octree if `shape_reduction_ratio` < 0.15 AND `build_us_mean` < 50 µs AND `volume_match_pct` = 100%
- F_TwoPass if `shape_reduction_ratio` < 0.10 AND `build_us_mean` < 200 µs AND `volume_match_pct` = 100%

**Per-scene winners:** see `RESULTS.md` (generated after benchmark run).
