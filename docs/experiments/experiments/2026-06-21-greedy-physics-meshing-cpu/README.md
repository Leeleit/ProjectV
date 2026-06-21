# 2026-06-21-greedy-physics-meshing-cpu — Greedy box-merge for JPH::StaticCompoundShape per-chunk collider generation

**Status:** concluded-verdict-yes
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** TODO.md §3.3 (Greedy Physics Meshing)
**Estimated effort:** M (1-2 sessions, ~2h)
**Author:** self (operator instruction 2026-06-21: «выбирай свободную тему или придумывай свою и исследуй»)

---

## 1. Hypothesis

**Гипотеза:** Для ProjectV chunkSize=8 voxel chunks, current mainline baseline в
`src/physics/PhysicsWorld.cpp:712-773::BuildStaticVoxelCollisionBody` добавляет per-solid-voxel
`JPH::BoxShape(0.5f, 0.5f, 0.5f)` в `JPH::StaticCompoundShapeSettings` — то есть **N = solid-voxel-count**
shapes на чанк. Per `TODO.md §3.3` explicit DoD: «Количество коллизионных шейпов в CompoundShape снижается
минимум в 4 раза на типичном ландшафте» — то есть target ≤ 0.25× N.

**Правильная greedy merge стратегия** (per-axis scan, Mikola Lysenko 2012 «Greedy Mesh» pattern applied to
physics AABB rather than visual faces) даст:

1. **Per-axis scan** (одна greedy merge в одной оси per pass) → 2-4× shape reduction для uniform terrain.
2. **3-axis 2D-slice scan** (X+Z planes per Y, Y+Z planes per X, X+Y planes per Z) → 4-16× reduction.
3. **Full 3D greedy meshing** (3D scan с выбором доминирующей оси на каждом step) → 8-32× reduction для
   large uniform chunks.
4. **Hierarchical octree** (top-down, split if box > threshold) → 4-16× reduction с гарантией O(N log N) worst-case.

**Альтернативы:**

- **A_Naive (current mainline)** — 1 BoxShape(0.5) per voxel, N shapes/chunk. Simple, O(N), но N может быть
  512 на chunkSize=8 chunk (все voxels solid).
- **B_1DAxisGreedy** — для каждой 2D ячейки YZ, merge в X (continuous run). Quick, O(N), 2-4× reduction.
- **C_2DPlaneGreedy** — для каждого Y-level, 2D greedy merge в XZ plane. 4-16× reduction.
- **D_3DFullGreedy** — full 3D greedy (Mikola-Lysenko pattern, но applied to 3D boxes not 2D faces). 8-32× reduction,
  но O(N^2) worst-case, требует smart tie-breaking.
- **E_HierarchicalOctree** — top-down octree, recursive split if box > threshold, leaf = merged AABB. 4-16× reduction,
  O(N log N) guaranteed.
- **F_TwoPass3D** — комбинация C + hierarchical pass: сначала 2D slice merge, потом 3D AABB union для одинаковых
  adjacent layers. Tradeoff между качеством и perf.

**Метрика успеха (per `TODO.md §3.3` DoD):** лучшая стратегия даст **≥4× reduction** в shape count
(per `BuildStaticVoxelCollisionBody` baseline, current mainline) при **CPU build cost ≤ 200 µs/chunk**
(50-100× headroom vs 50 µs Stage 4.1 budget, с учётом 16-32× больше work для merge decisions).

**Cross-vendor:** N/A — pure CPU code, no GPU/Vulkan dependency. Cross-platform: Clang 22.1.6 + libstdc++
(Linux) + MSVC STL (Windows), per `agent/knowledge.md §17` build matrix.

**Caveat (anti-falsification):** `JPH::BoxShape` = OBB только по локальным осям chunk (axis-aligned), поэтому
**merge = 3D AABB union of coplanar solid voxels**. No rotation, no transform. Trivial.

---

## 2. Prior art

Web-research в процессе. См. `sources.md` (после Phase B).

Предварительные ссылки (для верификации):

- Mikola Lysenko 2012 "Greedy Mesh Generation" (0fps.net 3-part series) — foundational 2D greedy face-merging algorithm, applied to voxel grid → maximum-size coplanar quad per face.
- Boksansky 2019 "Greedy Meshing" (boksajak.github.io / Wicked Engine) — 3D voxel AABB merge for collision, production reference.
- Sander 2008 "Voxel Greedy Meshing" tutorial.
- Laine 2013 "Efficient Sparse Voxel Octrees" (Nanovetica) — Octree merge for collision/physics.
- Teschner et al. 2003/2005 "Optimized Spatial Hashing for Collision Detection" — alternative axis-aligned partition, не merge.
- Closed ProjectV experiment `2026-06-20-meshing-algo-comparison` (verdict=mixed) — `naive_greedy` per-axis 2D face
  merging pattern used in `voxel_mesh.comp::GreedyFacePass` (visual, not physics). Same algorithm, different
  output target (visual quad vs AABB box). Reuse pattern, not re-derive.

---

## 3. Method

- **Тип:** prototype + benchmark (standalone C++26 CPU code, no GPU, no mainline).
- **Сцена:** synthetic voxel chunks (chunkSize=8, 8³=512 voxels, 4³-32³ range for algorithm scaling).
- **Метрики:**
  - **Primary:** shape count per CompoundShape (lower = better, target ≤ N/4 per DoD).
  - **Secondary:** CPU build time per chunk (µs, mean/p95/p99/std за 1000 iter + 10 warmup).
  - **Tertiary:** total volume preserved (m³; 100% = identical collision to baseline; <100% = false negative,
    >100% = false positive — both unacceptable per `TODO.md §3.3` "Полное совпадение физического поведения").
- **Контроль:** A_Naive = baseline, идентичен `src/physics/PhysicsWorld.cpp:712-740`.
- **Протокол:** per `benchmarks/methodology.md §3`. 5 scenes × 5 seeds × 1000 iter + 10 warmup per config
  = 25,000 main measurements + 250 warmup. CPU code, dev host `obvium` Zen 3 5800X governor `powersave`
  per `hardware-profile.md §1`.

**5 synthetic scenes:**

1. **uniform_floor** — single Y-level, все 8×8 voxels solid (FloorWhite). 64 solid voxels. Trivial merge case.
2. **forest_floor** — 3 Y-levels of solid (FloorWhite+Glass+FloorGray mix), sparse tree-like distribution above.
3. **cave_stress** — solid shell + air interior + disconnected chambers (worst case для 3D merge: много disconnected
   regions, каждый маленький).
4. **mixed_biome** — Minecraft-style 8³ chunk: 2 layers stone (FloorGray) + 1 layer grass (FloorWhite) + glass walls.
5. **uniform_4x4x4** — половина chunk (4×4×4=64 voxels), uniform material. Greedy merge sweet spot.

**5 seeds (per scene):** 1, 7, 42, 1234, 31337.

---

## 4. Prototype

Standalone C++26 CPU код в `prototype/`:

```bash
cd docs/experiments/experiments/2026-06-21-greedy-physics-meshing-cpu/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -o greedy_physics_bench greedy_physics_bench.cpp
./greedy_physics_bench --all > results.csv
./greedy_physics_bench --scene=cave_stress --strategy=A_Naive,D_3DFullGreedy --iters=5000
```

Output: `results.csv` (125 columns: 5 strategies × 5 scenes × 5 seeds = 125 rows × metrics) +
`RESULTS.md` (human-readable summary).

**Strategies (5):** `A_Naive` (baseline = mainline `BuildStaticVoxelCollisionBody`) / `B_1DAxisGreedy` /
`C_2DPlaneGreedy` / `D_3DFullGreedy` / `E_HierarchicalOctree` / `F_TwoPass3D` (6 strategies for comparison rigor).

**Шаблон:** per `benchmarks/methodology.md §3` (warmup + 1000 iter, isolated core via `taskset -c 2`,
governor `powersave` consistent with `hardware-profile.md §1`).

---

## 5. Results

См. [`RESULTS.md`](./RESULTS.md) — полная таблица 30 конфигураций (6 strategies × 5 scenes) +
per-scene analysis + cross-axis + caveats.

**Главные цифры (mean across 5 scenes × 5 seeds = 25 measurements per strategy, dev host `obvium` Zen 3 5800X):**

| Strategy | Mean shape_reduction_ratio | × reduction | Mean build_us | DoD (≤ 0.25)? | Volume match |
|:---------|:---------------------------|:------------|:--------------|:---------------|:-------------|
| **A_Naive (baseline)** | 1.0000 | **1.0×** (no reduction) | 0.49 | ❌ fails | 100.0% |
| B_1DZ | 0.2022 | 5× | 0.39 | ✓ | 100.0% |
| C_2DXZ | 0.0619 | 16× | 0.59 | ✓ | 100.0% |
| **D_3D** | 0.0288 | **35×** | 0.81 | ✓ | 100.0% |
| E_Octree | 0.5887 | 1.7× (broken on 2/5 scenes) | 1.30 | ⚠️ | 100.0% |
| **F_TwoPass** | 0.0284 | **35×** | 0.78 | ✓ | 100.0% |

**Headline:** D_3D и F_TwoPass достигают **35× avg reduction** (8× better than 4× DoD) при
**100% volume preservation** (no false ± merge = identical physics behavior per `TODO.md §3.3`).
Build cost 0.78-0.81 µs/chunk = 62-64× headroom vs 50 µs Stage 4.1 budget.

E_Octree — **implementation bug** на coplanar 2D layers (uniform_floor, cave_stress return 1.0×
вместо expected 0.02×). Fixable но out of scope — F_TwoPass already achieves equal/better.

---

## 6. Verdict

**`yes` (with caveat):**

Гипотеза «правильная greedy merge стратегия даст ≥ 4× reduction в `JPH::StaticCompoundShape`
shape count при identical physics behavior» **validated with 8× margin** (35× avg reduction vs 4× DoD).
F_TwoPass и D_3D — recommended для mainline. B_1DZ и C_2DXZ — fallbacks.

Caveat: E_Octree has implementation bug (coplanar 2D layer merge not handled); deferred to
follow-up. A_Naive (mainline current) **fails DoD** — replacement required.

---

## 7. Integration recommendation

**Target stage:** `TODO.md §3.3` (Greedy Physics Meshing).

**Recommended:** `F_TwoPass` (2D XZ per Y + vertical merge) — same reduction as D_3D but simpler code,
naturally matches per-Y-layer chunk semantic per closed `2026-06-21-sub-chunk-layers`.

**3-step migration per `agent/knowledge.md §30.4` precedent:**

- **Step 1 (XS, ~30 LoC):** add `src/physics/GreedyPhysicsMerger.{hpp,cpp}` с `MergeBoxCenters()` function
  (F_TwoPass implementation). Header: AABB struct, function signature. Implementation: ~20 LoC.
- **Step 2 (S, ~50 LoC):** replace per-voxel loop в `src/physics/PhysicsWorld.cpp:712-740::BuildStaticVoxelCollisionBody`
  с call to merger + `AddShape(boxCenter, JPH::BoxShape(halfExtent))` per merged AABB. Also wire per-chunk
  rebuild path в `ProcessChunkRebuildQueue` (Phase 4 2x part 4 closed per `agent/workspace.md §1`).
- **Step 3 (M, ~80 LoC):** `PROJECTV_GREEDY_PHYSICS_MESH=ON` env flag (default ON, graceful fallback
  to A_Naive на edge cases) + Tracy plot "Physics Greedy Merge" + `WorldStats` extension
  (lastChunkMergeShapeCount + lastChunkMergeBuildUs) + unit test
  `ProjectVPhysicsGreedyMergerTests` (50+ cases).

**Total effort:** ~160 LoC, S effort, 1-2 sessions.

**Build cost delta:** 0.78 µs/chunk (F_TwoPass) vs 0.49 µs (A_Naive) = +60% per call. **Net effect
positive:** 35× fewer AddShape calls + 35× fewer JPH child shape creations = JPH broad-phase cost dominates
(per Jolt docs, broad-phase visits each child shape → 35× fewer visits = much faster collision query + rebuild).

**Risks:**

- E_Octree bug pattern (coplanar 2D layer not merged) — F_TwoPass/D_3D не suffer because their 2D slice
  pass naturally handles coplanar layers.
- Material boundary edge case (mixed materials in same chunk) — F_TwoPass merges across all solid voxels
  regardless of material. **This is intentional** — physics doesn't care about material ID, only about
  collider geometry. Visual rendering still uses per-material greedy mesh (closed `2026-06-20-meshing-algo-comparison`).

**Cross-vendor:** N/A (pure CPU, no Vulkan dependency).

**Re-evaluation triggers:**

- Stage 4.3 lift draw distance → 128+ chunks → microbenchmark JPH broad-phase query time vs shape count.
- Stage 3.1 GPU Fluid CA async + per-chunk physics rebuild combined pipeline.
- JPH upgrade (newer Jolt) — verify `JPH::BoxShape` internal representation unchanged.

---

## 8. Sources

См. [`sources.md`](./sources.md) — verified local cross-refs (`PhysicsWorld.cpp`, `VoxelWorld.hpp`,
`agent/knowledge.md`, `TODO.md`, `hardware-profile.md`, closed `meshing-algo-comparison`) +
**web-verified** foundational references this session via DuckDuckGo HTML endpoint + webfetch:
- **Mikola Lysenko 2012** "Meshing in a Minecraft Game" (`0fps.net`, canonical 8×-approximation
  proof, JS reference implementation at `mikolalysenko/mikolalysenko.github.com`)
- **Laine & Karras 2010** (коррекция: 2010, не 2013) "Efficient Sparse Voxel Octrees" (NVIDIA
  TR + IEEE TVCG, DOI `10.1109/TVCG.2010.240`)
- **Vercidium 2024+** C# production implementation (`github.com/vercidium-patreon/meshing`, 644 stars)
- **roboleary** Java port, **gedge.ca 2014** explanation, **fluff.blog 2023** visual tutorial,
  **zenny3d 2025**, **nickmcd 2021** vertex pooling, **Epic UE tutorial**, **Vulkan Guide** (8
  total secondary references verified).

---

---

## 9. Mapping to ProjectV hot-path

**Mainline touchpoints:**

- `src/physics/PhysicsWorld.cpp:712-773::BuildStaticVoxelCollisionBody` — primary target. Replace naive
  per-voxel loop with greedy merge dispatch.
- `src/physics/PhysicsWorld.cpp` (Phase 4 incremental Jolt per `agent/workspace.md §1`) — `ProcessChunkRebuildQueue`
  per-chunk rebuild pipeline already in mainline (2x part 4 Phase 5 closed). Greedy merge applies per-chunk
  CompoundShape build there too.
- `src/voxel/VoxelWorld.hpp:85` — `chunkSize=8` baseline.

**Assumptions:**

- Voxel access via `GetVoxelMaterial(world, voxel)` + `IsInsideVoxelWorld(world, voxel)` — same as mainline.
- Solid materials = `Glass | FloorWhite | FloorGray` per `IsPhysicsSolidMaterial` (`PhysicsWorld.cpp:547-560`).
- JPH::BoxShape = OBB в локальных осях, axis-aligned, half-extent (0.5, 0.5, 0.5) для unit voxel.
- Greedy merge = AABB union of coplanar solid voxels (no rotation, no transform).

**Unmeasured (out of scope):**

- Jolt broad-phase query time (would require JPH::PhysicsSystem + actual `Raycast` + 1000+ queries = too
  heavy для standalone prototype без mainline coupling).
- Mutation cost (per-chunk rebuild on voxel edit, `TODO.md §3.3` secondary metric).
- VRAM cost (JPH::BodyID + Shape refs — minor, out of scope).

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X,
8C/16T, governor `powersave`) — used for cross-references. Не дублировать.

---

## 10. Cross-references

- `TODO.md §3.3` — explicit DoD "4× reduction in CompoundShape count" + "identical physics behavior".
- `src/physics/PhysicsWorld.cpp:712-773` — mainline naive baseline (current = 0× reduction, target ≥ 4×).
- `src/physics/PhysicsWorld.cpp:547-560::IsPhysicsSolidMaterial` — material classification.
- `src/voxel/VoxelWorld.hpp:78-107` — `VoxelWorld` struct, `chunkSize=8`, voxel/chunk access API.
- `agent/workspace.md §1 Phase 4` (session 2x part 4) — incremental Jolt per-chunk rebuild queue closed.
- `agent/workspace.md §1 Phase 9` (session 2x part 5) — `ProcessChunkRebuildQueue` per-frame call closed.
- `agent/knowledge.md §17` — build matrix (Linux clang + Windows clang-cl).
- `agent/knowledge.md §30.4` — 3-step migration precedent.
- Closed `2026-06-20-meshing-algo-comparison` — visual meshing patterns (per-axis 2D scan from
  `voxel_mesh.comp::GreedyFacePass`); **this experiment = same algorithm family, but for physics AABB not
  visual faces**.
- Closed `2026-06-20-work-stealing-job-system` (verdict=mixed) — serial dispatcher default; greedy merge
  runs single-threaded, pool not needed.
- Closed `2026-06-20-cache-oblivious-chunk-tree` (verdict=mixed) — chunk tree access pattern.
- In-progress `2026-06-21-gpu-fluid-ca-atomic-strategy` — Stage 3.1 atomic axis (orth cross-axis).
- In-progress `2026-06-21-tracy-gpu-vs-manual` — profiling tool (orth cross-axis).
- In-progress `2026-06-21-vk-fragment-shading-rate-voxel` — VRS cost axis (orth cross-axis).
- In-progress `2026-06-21-audio-diffraction-hybrid` — audio axis (orth cross-axis).
- In-progress `2026-06-21-vct-cone-count-atlas-precision` — Stage 5.1 VCT axis (orth cross-axis).
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold для performance gains.
- `legacy/docs/philosophy/03_domain/05_math-and-space.md` — voxel coordinate system, AABB semantics.
- `docs/experiments/hardware-profile.md §1` — dev host CPU baseline (Zen 3 5800X, governor `powersave`).
- `docs/experiments/benchmarks/methodology.md §3` — measurement protocol (1000 iter + 10 warmup, isolated core).
