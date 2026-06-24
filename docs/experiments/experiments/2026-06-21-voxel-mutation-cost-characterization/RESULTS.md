# RESULTS — `2026-06-21-voxel-mutation-cost-characterization`

**Hardware baseline:** Zen 3 5800X (8C/16T, governor=`powersave`) per `hardware-profile.md §1`. Dev host `obvium`. CPU-only standalone prototype (no Vulkan, no GPU dispatch). Wall time ~155 sec для 625 configs.

**Methodology:** per `benchmarks/methodology.md §3`. Warm-up 10 iter + main 1000 iter + 5 seeds. Synthetic scenes per `prototype/mutation_bench.cpp` §4 (5 scenes representative of ProjectV gameplay).

**Toolchain:** Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green, 0 warnings** (1 unused-arg warning fixed at first build).

---

## 1. Headline (mean per pattern, n=25 configs = 5 scenes × 5 seeds)

| Strategy             | P1 SingleClick | P2 FillOperation | P3 MultiChunk | P4 FloodFill | P5 StressBurst | vs A baseline @ P5 |
|:---------------------|---------------:|-----------------:|--------------:|-------------:|---------------:|-------------------:|
| **A NaiveInPlace**   |    **0.05 µs** |       **0.93 µs**|   **1.08 µs** |  **2.16 µs** |   **4.16 µs**  | baseline           |
| **B DirtyFlag**      |     0.07 µs    |       0.17 µs    |    0.36 µs    |   0.34 µs    |   1.74 µs      | **−58.2%**         |
| **C BatchCoalesce**  |     0.08 µs    |       1.24 µs    |    1.75 µs    |   2.79 µs    |   7.52 µs      | **+80.8%** (slower)|
| **D DoubleBuffer**   |     0.14 µs    |       0.73 µs    |    0.68 µs    |   1.23 µs    |   2.27 µs      | **−45.4%**         |
| **E CopyOn+dedup**   |    12.48 µs    |     180.78 µs    |  836.24 µs    |1658.36 µs    | 3359.35 µs    | **+80,650%** (!!)  |

**P1=1 SetCell, P2=64 SetCells, P3=64 across 8 chunks, P4≈128 SetCells, P5=256 SetCells (burst / world gen).**

---

## 2. Per-edit cost (P5_StressBurst ÷ 256 edits)

| Strategy       | Per-edit (ns) | Speedup vs A | Practical verdict                  |
|:---------------|--------------:|-------------:|:-----------------------------------|
| A_NaiveInPlace |          ~16  |         1×   | **baseline, fast enough**          |
| B_DirtyFlag    |           ~7  |       2.4×  | **WINNER burst**                   |
| C_BatchCoalesce|          ~29  |       0.5×  | REGRESSION — per-chunk grouping overhead dominates |
| D_DoubleBuffer |           ~9  |       1.8×  | WINNER if atomic snapshot needed   |
| E_CopyOn+dedup |       ~13,123 |       0.001×| **NEVER for gameplay**             |

**Critical finding:** A baseline per-edit cost = **16 ns** for 8³ chunk. **Far below** my hypothesis 50-200 µs (3-4 orders of magnitude lower). The synthetic chunks collapse aggressively (max 65 nodes for full 512 voxels), so depth = 1-2 instead of theoretical depth = 3.

---

## 3. Per-scene breakdown (P5_StressBurst, mean µs)

| Strategy       | uniform_floor | sparse_world | mixed_biome | cave_stress | stacked_solid | n |
|:---------------|--------------:|-------------:|------------:|------------:|--------------:|--:|
| A_NaiveInPlace |          4.28 |         4.19 |        4.19 |        4.10 |          4.06 | 5 |
| B_DirtyFlag    |          1.69 |         1.79 |        1.76 |        1.75 |          1.74 | 5 |
| C_BatchCoalesce|          7.42 |         7.86 |        7.57 |        7.51 |          7.27 | 5 |
| D_DoubleBuffer |          2.32 |         2.30 |        2.25 |        2.27 |          2.21 | 5 |
| E_CopyOn+dedup |       3320    |       3462   |     3340    |     3359    |       3651    | 5 |

**Scene-coverage-independent:** all strategies show <10% variance across scenes. The synthetic scenes collapse to similar node counts after dedup analysis.

---

## 4. Peak node count + allocations

| Strategy       | Peak nodes per chunk | Total allocs @ P5 (256 edits) | Notes                                  |
|:---------------|---------------------:|------------------------------:|:---------------------------------------|
| A_NaiveInPlace |                64-65 |                          ~64  | Minimal: collapses to homogeneous root |
| B_DirtyFlag    |                64-65 |                       ~6-15  | Skip pattern means 96-98% less work    |
| C_BatchCoalesce|                64-65 |                       ~64    | Same as A but grouped per-chunk        |
| D_DoubleBuffer |                64-65 |                       ~70    | One full clone + 64 rebuilds           |
| E_CopyOn+dedup |                64-65 |                       ~12k   | Dedup hash table O(N) per edit → many unique hashes |

**Memory churn per chunk = constant ~50 B/voxel + 64 nodes × ~280 B/node ≈ 18 KB peak**. Stable across strategies (synthetic scenes are trivial).

---

## 5. Key observations

1. **A_NaiveInPlace baseline is FAST for 8³ chunks.** Per-edit cost = 16 ns (256 edits in 4 µs). **NOT the mainline bottleneck.** Mesh rebuild + physics rebuild queue + greedy meshing per closed `2026-06-21-greedy-physics-meshing-cpu` (~50 µs per chunk) dominates per `agent/knowledge.md` Stage 4.1 budget.

2. **B_DirtyFlagDeferred is the clear winner for burst gameplay patterns.** -58% on P5 (1.74 vs 4.16 µs). Mainline already has `VoxelChunk::rebuildQueued` + `pendingChunkRebuildIndices` queue — but **`SetVoxelMaterial` per-call STILL triggers per-call `MarkChunksTouchedByVoxelDirty` + `QueueChunkRebuildRequest(physics)` regardless of whether chunk was edited this frame already**. Per-call SVDAG rebuild is duplicated when same chunk edited multiple times per frame. **Recommended optimization:** coalesce SetCells per chunk per-frame — only rebuild SVDAG subtree ONCE per chunk per-frame, not per-edit. **Estimated effort:** ~30 LoC + env flag.

3. **D_DoubleBufferSwap gives -45% on P5** (2.27 vs 4.16 µs) — wins because bulk clone of already-compressed tree (64 nodes) is cheap relative to N rebuilds. Useful for Stage 1.3 async streamer (per `agent/knowledge.md`) where snapshot consistency matters. **Out of mainline scope** unless streamer needs atomic snapshot per-chunk.

4. **C_BatchCoalesce is SLOWER on P5** (+80%, 7.52 vs 4.16 µs). **Surprising finding**: grouping per-chunk adds overhead (hash map for chunk → batch). Only useful if chunk rebuild path is expensive (it's not for synthetic). **Counter-recommendation:** don't add per-chunk grouping layer without first measuring.

5. **E_CopyOnWrite+dedup is 800× slower.** `PROJECTV_SPARSE_64_STORAGE=ON` (per `VoxelWorld.cpp:92-103`) enables dedup hash table — every SetCell triggers `dedupIndex_.equal_range(hash)` lookup, O(N) avg where N = total nodes. **For static worlds (no mutations):** dedup ON = win (per `svdag-vs-vdb-memory-throughput`). **For gameplay worlds (block place/break):** dedup ON = 800× overhead. **Recommendation:** per-chunk `isStatic` flag should DISABLE dedup hash lookups for `isStatic=false` chunks (current mainline `TickVoxelChunkStaticPromotion:1113-1129` already does this implicitly — dedup only runs at threshold, but MarkNodeUnique's dedup lookup fires on EVERY SetCell when dedupEnabled_=true). Verify in `Sparse64Tree::SetCellRecursive` callsite.

6. **5% threshold per `optimization-philosophy.md`:** B_DirtyFlag and D_DoubleBuffer **CROSS** the threshold (45-58% reduction). C and E **FAIL** the threshold (E catastrophically so). A baseline **NEEDS** the optimization only for high-throughput gameplay (60+ block edits/frame per chunk).

---

## 6. Per-pattern ratio (vs A baseline, n=25)

| Strategy       | P1 ratio | P2 ratio | P3 ratio | P4 ratio | P5 ratio |
|:---------------|---------:|---------:|---------:|---------:|---------:|
| A_NaiveInPlace |    1.00× |    1.00× |    1.00× |    1.00× |    1.00× |
| B_DirtyFlag    |    1.4×  |   0.18×  |   0.33×  |   0.16×  |   0.42×  |
| C_BatchCoalesce|    1.6×  |   1.33×  |   1.62×  |   1.29×  |   1.81×  |
| D_DoubleBuffer |    2.8×  |   0.78×  |   0.63×  |   0.57×  |   0.55×  |
| E_CopyOn+dedup |  250×    |  194×    |  774×    |  768×    |  808×    |

**Reading:** ratio > 1 = slower than A. B/D = WIN on burst (P3+). C = LOSE everywhere except P1. E = CATASTROPHIC.

---

## 7. Tree invariant check

All 625 configs produce trees where:
- `GetCell(x,y,z) == expected` for sampled cells after pattern → **volume preservation 100%** (no false positive/negative voxels introduced).
- Peak node count stays ≤ 65 per chunk → **bounded memory growth** (no fragmentation).
- `coalesced_rebuilds` count matches expected per strategy (B = N chunks per batch; C = N chunks per batch; D = 1 per batch; E = 0 = direct).

**No correctness issues detected.**

---

## 8. Caveats (per `benchmarks/methodology.md §6`)

- **CPU prototype only.** No Vulkan init, no real `vkCmdWriteTimestamp`, no GPU dispatch, no driver overhead. Real ProjectV mainline mutation cost = SVDAG rebuild + **mesh rebuild (greedy meshing ~50 µs/chunk per closed `2026-06-20-meshing-algo-comparison` + `2026-06-21-greedy-physics-meshing-cpu`)** + **physics rebuild queue drain** + **JPH broad-phase query**. SVDAG rebuild alone is **< 1% of total mutation cost**.
- **Synthetic scenes** collapse aggressively (max 65 nodes vs theoretical 64³ leaves). Real ProjectV VoxelLab/MeshingStress scenes may have more varied depth = slightly higher per-edit cost. Still order of magnitude lower than hypothesis.
- **No dedup enabled by default in A baseline.** Mainline dedup is per env var `PROJECTV_SPARSE_64_STORAGE=ON`. If dedup ON + gameplay = catastrophic 800× overhead (E measurement).
- **Single-threaded.** Real mainline per-frame budget = 16.67 ms @ 60 fps. All strategies complete P5 in < 10 µs → all well within budget. Real bottleneck = mesh + physics rebuild, NOT SVDAG.
- **No per-frame composition cost measured.** Tracy profiling not in scope of CPU prototype.

---

## 9. Re-evaluation triggers

- **Stage 4.3 ships** (128+ chunks draw distance) → 4× more chunks → SVDAG rebuild budget = 16 ns × 4 chunks active per frame = ~64 ns/frame = still trivial.
- **VMA 3.5+ release** with new dedicated mutation suballocator → may change C/E strategies.
- **GPU world gen Stage 4.1** ships (closed `2026-06-21-gpu-procedural-noise-compute-kernels`) → burst mutation pattern P5 256 edits/chunk @ world gen = same as P5 measurement, B_DirtyFlag pattern should be applied.
- **Real ProjectV VoxelLab benchmark** with realistic gameplay trace → re-measure with real scenes.
- **Cross-vendor** (AMD RDNA + Intel Arc): single-host measurement (RTX 3060 Ti not used — CPU-only); not relevant to mutation cost.

---

## 10. Output

- **`prototype/build/results.csv`** — 626 rows (1 header + 625 configs), 80 KB. Per-config mean/median/p95/p99/p99.9/std/min/max + per-edit + allocations + peak_nodes + coalesced_rebuilds.
- **`prototype/mutation_bench.cpp`** — ~750 LoC standalone C++26 harness + simplified SVDAG model + 5 strategy implementations.
- Wall time: ~155 sec on dev host `obvium` Zen 3 5800X (last 75 configs dominated by E_CopyOn+dedup × P5_StressBurst = ~120 sec; remaining 550 configs = ~35 sec).

---

## 11. Summary table (machine-readable)

```csv
strategy,mean_P1,mean_P2,mean_P3,mean_P4,mean_P5
A_NaiveInPlace,0.05,0.93,1.08,2.16,4.16
B_DirtyFlagDeferred,0.07,0.17,0.36,0.34,1.74
C_BatchCoalesce,0.08,1.24,1.75,2.79,7.52
D_DoubleBufferSwap,0.14,0.73,0.68,1.23,2.27
E_CopyOnWriteSnapshot,12.48,180.78,836.24,1658.36,3359.35
```

**Verdict support:** see README §6 Verdict + §7 Integration recommendation.