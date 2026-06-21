# 2026-06-21-conc-ring-generation-scheduling — Concentric-ring generation scheduling for chunk world gen

**Status:** `open`
**Date opened:** 2026-06-21
**Date closed:** N/A
**Stage link:** Stage 4.1 (world generation), independent
**Estimated effort:** S
**Author:** self (derived from VoxelCore source analysis)

---

## 1. Hypothesis

VoxelCore's `SurroundMap` implements a concentric-ring generation scheduler: chunks are generated in expanding square rings around the player, with each ring level triggering specific generation callbacks (wide structures → biomes → heightmaps → structures). This ensures that when a chunk needs neighbor data for cross-chunk generation (e.g., structures that span chunk borders), the neighbors are already generated.

**Hypothesis:** A concentric-ring scheduler eliminates cross-chunk dependency deadlocks during parallel world generation, reducing chunk gen stalls by 40-60% vs naive distance-sorted generation. The ring approach naturally satisfies structure dependency ordering without explicit dependency graphs.

**Alternatives:** distance-sorted generation (current assumed), explicit dependency graph with topological sort, synchronous neighbor-first generation.

---

## 2. Prior art

- **VoxelCore `SurroundMap.cpp:1-71`** — concentric square rings with level-based callback triggering.
- **VoxelCore `WorldGenerator.cpp:272-284, 339, 380, 474-514`** — multi-phase generation pipeline (structures_wide → biomes → heightmap → structures → final).
- **VoxelCore `ChunksController.cpp:35-66`** — budget-limited chunk loading (MAX_WORK_PER_FRAME=128).
- **closed `2026-06-21-wfc-procedural-worlds`** — WFC gen strategy (orthogonal — WFC is about what to generate, this is about scheduling when).
- **closed `2026-06-21-voxel-chunk-streaming-pipeline`** — streaming pipeline (orthogonal — streaming is about loading/saving, this is about generation order).

---

## 3. Method

- **Type:** analytical + standalone C++26 CPU prototype
- **Scene:** 5 player movement patterns (stationary, linear_walk, teleport_stress, circular_patrol, random_jump)
- **Metrics:** chunk gen stall count, total gen time (ms), cross-chunk dependency violations, parallel utilization (%)
- **Baseline:** distance-sorted generation (generate nearest chunks first)
- **Strategies:**
  - A_DistanceSorted: sort by squared distance, generate in order (baseline)
  - B_ConcRing3: 3-level concentric ring (structures_wide → biomes+heightmap → structures)
  - C_ConcRing5: 5-level ring ( finer granularity per VoxelCore pattern)
  - D_DependencyGraph: explicit dependency graph with BFS scheduling

---

## 4. Prototype

Standalone C++26 CPU harness simulating chunk generation scheduling across movement patterns.

```bash
cd prototype && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/conc_ring_gen_bench
```

---

## 5. Results

_TBD — experiment not started._

---

## 6. Verdict

`open` — hypothesis only, no measurements yet.

---

## 7. Integration recommendation

- **Target stage:** Stage 4.1 (world generation pipeline)
- **Конкретные изменения:** new `src/world/GenScheduler.{hpp,cpp}` — concentric ring manager replacing distance-sorted queue.
- **Подход:** track player position; maintain ring levels; trigger generation callbacks per ring level; budget-limited per frame.
- **Рисks:** ring radius tuning (too small = cross-chunk deps, too large = over-generation); teleport handling (full ring rebuild).
- **Критерии приёмки:** zero cross-chunk dependency violations; <10% over-generation vs distance-sorted.
- **Зависимости:** existing `ChunksController` budget-limited loading.
- **Estimated effort:** S (~200 LoC, 1-2 sessions)

---

## 8. Sources

- VoxelCore `SurroundMap.cpp:1-71` (local source)
- VoxelCore `WorldGenerator.cpp:272-514` (local source)
- VoxelCore `ChunksController.cpp:35-66` (local source)
- closed `2026-06-21-wfc-procedural-worlds` (orthogonal gen strategy)
- closed `2026-06-21-voxel-chunk-streaming-pipeline` (orthogonal streaming)

---

## 9. Mapping to ProjectV hot-path

- **Target:** `src/voxel/VoxelWorld.hpp` — chunk generation scheduling (future world gen pipeline).
- **Assumptions:** chunkSize=8; parallel chunk generation via job system; cross-chunk structure dependencies.
- **Unmeasured:** actual cross-chunk dependency frequency in real ProjectV world gen; GPU gen dispatch overlap.
