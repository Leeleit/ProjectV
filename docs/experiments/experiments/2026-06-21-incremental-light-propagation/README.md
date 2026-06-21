# 2026-06-21-incremental-light-propagation — Incremental BFS light propagation with budget-limited ticking

**Status:** `open`
**Date opened:** 2026-06-21
**Date closed:** N/A
**Stage link:** Stage 3.x (lighting), independent
**Estimated effort:** M
**Author:** self (derived from Minecraft 1.12 + VoxelCore source analysis)

---

## 1. Hypothesis

Both Minecraft 1.12 and VoxelCore use BFS flood-fill for light propagation, but with different incremental strategies:
- **Minecraft 1.12** (`Chunk.java:1470-1510`): processes only 8 sub-columns per tick (round-robin through 4096 columns), spreading work across frames. Sky light uses simple column descent (no BFS needed for downward propagation).
- **VoxelCore** (`LightSolver.cpp:57-152`): two-phase BFS (removal then addition) with `array_queue`, processing all changes in one solve() call.

**Hypothesis:** A budget-limited incremental BFS light solver (max N queue entries per frame, deferred propagation) reduces per-frame lighting cost by 60-80% vs full-frame BFS, with <1 visual frame delay for typical block edits. The Minecraft 1.12 pattern of 8 columns/tick is too conservative; a budget based on queue entries (not columns) is more scene-adaptive.

**Alternatives:** full-frame BFS (current assumed mainline), GPU compute BFS (deferred to Stage 5.x), no runtime lighting (baked only).

---

## 2. Prior art

- **Minecraft 1.12 `Chunk.java:329-394, 494-600, 1470-1510`** — incremental sky light (column descent, 8 cols/tick round-robin) + block light relight scheduling.
- **VoxelCore `LightSolver.cpp:57-152`** — two-phase BFS (remove + add) with flood-fill through `lightPassing` blocks.
- **Minecraft 1.12 `Chunk.java:407-451`** — `recheckGaps()` for sky light gap recalculation with column dirty flags.
- **Laine & Karras 2010 "Efficient Sparse Voxel Octrees"** — GPU-parallel light propagation (not directly applicable to CPU BFS).
- **closed `2026-06-21-gpu-fluid-ca-atomic-strategy`** — GPU atomic operations for fluid CA; this is CPU light BFS (orthogonal).

---

## 3. Method

- **Type:** analytical + standalone C++26 CPU prototype
- **Scene:** 5 voxel scenes with varying light complexity (uniform_open, cave_system, indoor_room, multi_source, dense_foliage)
- **Metrics:** per-frame light propagation cost (µs), visual delay (frames), total convergence time (ms), queue peak size
- **Baseline:** full-frame BFS (process all dirty entries each frame)
- **Strategies:**
  - A_FullBFS: process entire queue each frame (baseline)
  - B_Budget8Col: Minecraft 1.12 pattern — 8 columns per tick
  - C_BudgetQueue: process up to N queue entries per frame (budget = 1024/2048/4096 entries)
  - D_BudgetQueueAdaptive: budget scales with queue size (10% of pending per frame, min 256, max 4096)

---

## 4. Prototype

Standalone C++26 CPU harness simulating light propagation across chunk grids with budget-limited BFS.

```bash
cd prototype && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/light_propagation_bench
```

---

## 5. Results

_TBD — experiment not started._

---

## 6. Verdict

`open` — hypothesis only, no measurements yet.

---

## 7. Integration recommendation

- **Target stage:** Stage 3.x (lighting system)
- **Конкретные изменения:** new `src/lighting/LightSolver.{hpp,cpp}` with budget-limited BFS; integration with existing chunk dirty tracking.
- **Подход:** replace full-frame BFS with per-frame budget queue drain; column-based dirty flags per Minecraft 1.12 pattern; sky light column descent (no BFS needed for downward propagation).
- **Риски:** visual artifacts during rapid block edits (torch placement/removal); multi-source interference during partial propagation.
- **Критерии приёмки:** >60% per-frame cost reduction vs full BFS; <2 frame visual delay for single torch placement.
- **Зависимости:** Stage 3.1 fluid CA (orthogonal), existing lightmap storage.
- **Estimated effort:** M (~350 LoC, 2-3 sessions)

---

## 8. Sources

- Minecraft 1.12 `Chunk.java:329-394, 494-600, 1470-1510` (local source)
- VoxelCore `LightSolver.cpp:57-152` (local source)
- VoxelCore `Lighting.cpp:42-103, 175-243` (local source)
- closed `2026-06-21-gpu-fluid-ca-atomic-strategy` (orthogonal GPU atomic axis)

---

## 9. Mapping to ProjectV hot-path

- **Target:** `src/shaders/lighting` (future), CPU light solver for chunk lighting.
- **Assumptions:** chunkSize=8 (512 voxels per chunk); 4-bit light channels; BFS through lightPassing blocks.
- **Unmeasured:** GPU light propagation cost (compute shader), driver overhead for lightmap buffer uploads.
