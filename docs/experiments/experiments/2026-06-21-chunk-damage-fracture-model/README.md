# 2026-06-21-chunk-damage-fracture-model — Voxel chunk fracture model

**Status:** `concluded-verdict-mixed`
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** TODO.md §3.x (Physics & Simulation / interaction)
**Estimated effort:** S (~150 LoC)
**Author:** self (agent)

---

## 1. Hypothesis

Greedy-group fracture (pre-grouped connected components as fracture chunks) reduces Jolt debris body count by 10-100× vs voxel-by-voxel with sub-millisecond fragment generation. Precomputed Voronoi tessellation patterns add 2-5× storage overhead but produce more natural-looking fractures.

**Alternatives:**
- **Voxel-by-voxel** (naive baseline): each destroyed voxel → separate Jolt body. Simple but O(n) debris bodies.
- **Greedy-group / CCL** (this experiment): CCL-connected voxel groups → one merged AABB body per component.
- **Greedy3D merge**: 3D greedy meshing-style merge → minimal AABB count per connected region.
- **Precomputed Voronoi patterns** (quality): store N fracture templates per material, apply on damage.

---

## 2. Prior art

Web research (June 2026, 20+ sources across 3 search waves):

- **Leon's Notes (2026-06-03)** — "Voxels That Scale and Break": damage byte per voxel + explosion cubemap shadow bake (6K rays < 1 ms); deferred re-mesh only on lethal threshold; CCL flood-fill for structural separation. Direct validation of CCL approach.
- **Teardown / Dennis Gustafsson (80.lv, 2026-03-17)** — Production voxel destruction: voxel volumes on regular grid → simplifies physics + graphics; SIMD + multithreaded; deterministic multiplayer via destruction commands (not voxel data).
- **Gram-Schmidt voxel constraints (Purdue, 2024)** — Breakable face-to-face voxel constraints for soft-body destruction; 8-byte constraint with 3 Gauss-Seidel partitions; encourages chunk formation (not single-voxel fracture).
- **Nintendo / Donkey Kong Bananza (2026-03-12)** — Voxel-based destruction with material-specific properties (density, brittleness, friction); dynamic LOD + culling; voxel count tied to physics + audio feedback.
- **BoxCutter Unity asset (2026-05-14)** — 5 fragmentation modes (Standard, Radial, Slab, Splinter, Cluster); connected-component extraction; KD-tree spatial acceleration; occlusion-aware greedy meshing; multithreaded Burst pipeline.
- **Kugelhaufen/VoxelEngine (GitHub)** — Open-source voxel destruction for Unity: ConnectedComponentPhysics with multiple extractor algorithms (largest chunk static / gravity extractor); VoxelObjAmountManager for debris limits.
- **Reddit r/VoxelGameDev** — CCL flood-fill for broken-off parts: flood-fill from invalidated voxels → find detached volumes; largest group = original object, smaller groups = debris.
- **Minecraft Explosion (Bukkit/mc-dev)** — BFS-flood within radius with power decay; block resistance threshold; O(n) per explosion.
- **Multiple Voronoi libraries** (three-pinata, VoronoiShatter, Destronoi, UE5 Chaos): Voronoi cells as fracture patterns; pre-bake cache for performance; 2.5D vs 3D modes.

---

## 3. Method

- **Type:** prototype + benchmark (standalone C++26 CPU).
- **Scenes:** 5 scenes (uniform_floor, forest_floor, cave_stress, mixed_biome, custom_fracture) × 5 seeds (1, 7, 42, 1234, 31337) = 25 configs per strategy.
- **Strategies:**
  - **A_NaiveVoxel** (baseline): each solid voxel after damage → separate AABB body.
  - **B_CCL**: Union-Find CCL 26-conn → merged AABB per connected component.
  - **C_Greedy3D**: 3D greedy meshing merge (per-axis scan, like F_TwoPass from physics meshing).
  - **D_Voronoi**: 8 precomputed Voronoi 8³ templates, best-fit chosen per config.
  - **E_Hybrid**: CCL for small fragments (<4 voxels), Voronoi for large components.
- **Damage model:** Minecraft-style explosion (radius ~3 voxels, power ~10, stone hardness 3) at random position in chunk.
- **Metrics:** body count (fragments), generation time (µs), reduction vs naive.
- **Protocol:** per-strategy × per-config × 1000 iter + 10 warmup.

---

## 4. Prototype

`prototype/fracture_bench.cpp` ~480 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`). Build green 0 warnings.

```bash
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
  prototype/fracture_bench.cpp -o prototype/build/fracture_bench
./prototype/build/fracture_bench
```

Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data).

---

## 5. Results

**Per-strategy summary (mean across 25 configs):**

| Strategy      | Mean time (µs) | Mean bodies | Reduction vs naive |
|:--------------|:---------------|:------------|:-------------------|
| A_Naive       | 0.859          | 431.1       | 1.0× (baseline)    |
| B_CCL         | 25.53          | 1.0         | **431×**           |
| C_Greedy3D    | 2.88           | 52.4        | **8.2×**           |
| D_Voronoi     | 1.48           | 4.9         | **88×**            |
| E_Hybrid      | 26.23          | 5.4         | **80×**            |

**Key observations:**

1. **B_CCL always produces 1 component** on all 25 configs — the explosion never fully severs all connections through the 8³ chunk; all remaining solid voxels stay connected. For debris generation, CCL alone is insufficient without cross-chunk connectivity (static world anchor check).

2. **C_Greedy3D is the practical sweet spot**: 2.88 µs (0.009% of 33 ms frame), 8.2× body reduction. Algorithm is simple, topology-preserving, and 100% voxel-accurate.

3. **D_Voronoi gives highest reduction** (88×) at 1.48 µs, but fragment boundaries are artificial (not topology-aware). Suitable for visual-only destruction.

4. **E_Hybrid** (CCL + Voronoi) adds CCL's cost (26 µs) without benefit over pure Voronoi on 8³ chunks — CCL always finds 1 component, so Voronoi always handles the single "large" component.

5. **All strategies well within budget**: even B_CCL at 25.5 µs = 0.077% of 33 ms frame.

**Critical insight:** For realistic chunk fracture, the approach must be:
1. After damage, run CCL → find connected components.
2. Identify "main body" (largest component, or connected to world anchor) → keep as static chunk.
3. Only spawn debris for components disconnected from main body.

In this prototype, all 25 configs produce exactly 1 component (all solid voxels still connected), confirming that on an 8³ scale, explosions rarely produce disconnected floating fragments without cross-chunk context.

---

## 6. Verdict

**`mixed`.** Hypothesis partially validated: greedy-group approaches (Greedy3D 8.2× reduction, Voronoi 88× reduction) massively reduce debris body count vs naive voxel-by-voxel, well within frame budget. CCL's 431× reduction is misleading — it finds 1 component (no debris) for all single-chunk explosions.

The mixed verdict stems from the insight that **8³ chunk explosion fracture fundamentally differs from mesh fracture**: on an 8³ grid, explosions leave all remaining voxels connected through the undamaged part of the chunk. True structural separation requires either:
- Cross-chunk damage (explosion spans chunk boundaries, severing inter-chunk connections), or
- Thin structures (walls/floors 1-2 voxels thick where explosion fully punches through).

Both scenarios are outside this experiment's single-8³-chunk scope.

---

## 7. Integration recommendation

- **Target stage:** TODO.md §3.x (post-Stage 3.3 Greedy Physics Meshing, as a follow-up when chunk damage/debris is scheduled).
- **Changes:**
  - Step 1 (XS, ~40 LoC): Add `FractureChunk(greedyPhysicsMergedFragments, explosionOrigin)` helper in `src/physics/PhysicsWorld.cpp`. Uses existing GreedyPhysicsMerger (Stage 3.3 closed) output as fragment inputs.
  - Step 2 (S, ~80 LoC): CCL pass on damaged voxels → separate "main body" (keep static) from "debris" (spawn dynamic Jolt bodies). Only trigger when voxels are actually destroyed.
  - Step 3 (XS, ~30 LoC): Env gate `PROJECTV_CHUNK_FRACTURE=NONE|GREEDY3D|CCL` (default GREEDY3D when Stage 3.3 active).
- **Risks:** Jolt body count explosion if naive per-voxel debris used. Greedy3D bounds this at 8× reduction.
- **Dependencies:** Stage 3.3 Greedy Physics Meshing (closed) for fragment AABB generation; Stage 3.2 Incremental Jolt (closed) for per-chunk body management.
- **Deferred until:** chunk damage / destruction gameplay is implemented (not yet in mainline). Current mainline: chunk either exists or is entirely removed → no fracture needed.

**Re-evaluation triggers:** When per-voxel damage (health/damage byte) is added to the voxel format, this experiment's results directly apply: use Greedy3D for debris body consolidation.

---

## 8. Sources

1. Leon's Notes — "Voxels That Scale and Break" (2026-06-03) https://leonsnotes.ca/2026/06/03/voxels-that-scale-and-break/
2. 80.lv — Teardown Developer Interview (2026-03-17) https://80.lv/articles/teardown-developer-breaks-down-multiplayer-and-voxel-destruction-tech
3. McGraw et al. — Gram-Schmidt voxel constraints for real-time destructible soft bodies (Purdue, SIGGRAPH 2024)
4. Overcentral — Nintendo Donkey Kong Bananza voxel physics (2026-03-12) https://overcentral.com/en/nintendo-developers-detail-voxel-physics-system-behind-donkey-kong-bananzas-destructive-gameplay/
5. BoxCutter Unity Asset (2026-05-14) https://assetstore.unity.com/packages/tools/physics/boxcutter-realtime-voxel-destruction-331249
6. Kugelhaufen/VoxelEngine (GitHub) https://github.com/Kugelhaufen/VoxelEngine
7. Reddit r/VoxelGameDev — "How to isolate broken off parts" (2021) https://www.reddit.com/r/VoxelGameDev/comments/nv28hy/
8. Bukkit/mc-dev — Minecraft Explosion.java https://github.com/Bukkit/mc-dev/blob/master/net/minecraft/server/Explosion.java
9. three-pinata Voronoi fracture library (GitHub) https://github.com/dgreenheck/three-pinata
10. UE5 Chaos Destruction (2025) https://nbertoa.com/2025/04/14/unreal-5-5-chaos-destruction/
11. sinanata/unity-mesh-fracture (GitHub, 2026) https://github.com/sinanata/unity-mesh-fracture

---

## 9. Mapping to ProjectV hot-path

- **Corresponds to:** `src/physics/PhysicsWorld.cpp::BuildStaticVoxelCollisionBody` + `BuildChunkStaticCollisionBody` (Stage 3.2/3.3) — debris body generation on per-voxel damage.
- **Assumptions:** CPU-only prototype, synthetic 8³ fracture volumes with Minecraft-style explosion model, no Jolt broad-phase timing.
- **Unmeasured:** GPU mesh rebuild cost after chunk damage, Jolt constraint destruction, sound/particle feedback at fracture time, multi-chunk fracture coherence.
- **Limitation:** Single 8³ chunk scope — cross-chunk fracture (damage spanning chunk boundaries) requires follow-up experiment.

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X, governor=powersave) + §3 (RTX 3060 Ti Ampere, 8 GiB VRAM) — used for prototype timing but CPU-only measurement makes GPU data informational only.
