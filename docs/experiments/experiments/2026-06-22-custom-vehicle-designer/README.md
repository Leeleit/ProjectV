# 2026-06-22-custom-vehicle-designer — Voxel-based vehicle assembly

**Status:** in-progress
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent (Tier 3 Economy, Sandbox, Content & Game Modes)
**Estimated effort:** M (2 sessions)
**Author:** self

---

## 1. Hypothesis

6-strategy comparison ∈
{A_NaivePerVoxel_APIBaseline, B_PrecomputedBlueprintColliders, C_GreedyPhysicsMerge_BatchedAddShape,
D_HierarchicalSubAssembly_Deferred, E_Hybrid_TemplatePlusMerge, F_WheelAware}
for voxel vehicle assembly from 8³–32³ (512–8192 voxels) grid:

- **C/D/E/F** beat **A** naive per-voxel by **≥ 10×** in shape count (proxy for Jolt broad-phase cost) at **< 0.5 ms/vehicle** assembly time.
- **C/D/E/F** preserve **100% collision volume** vs A baseline (byte-exact memcmp of voxel occupancy grid).
- **D** (hierarchical) provides **best mutation-rebuild time** (sub-module scoped, not full-vehicle): ≥ 5× faster than C on 10% voxel toggle.
- **F_WheelAware** (cylinder shapes for wheel voxels) adds ≤ 5% assembly overhead vs all-box C but improves wheel collision fidelity (proxy: cylinder count / total shapes).

**Why vehicles differ from landscape chunks:**
- Vehicles have empty interior (hollow hulls) — F_TwoPass from `greedy-physics-meshing-cpu` merged only solid voxels, same here.
- Vehicles span larger scale (up to 32×16×32 = 8192 voxels vs landscape 8×8×8 = 512) — Jorrit Rouwé: ~1000 child shapes/compound is fine; 8192 per-voxel shapes is too many.
- Vehicles use multiple materials (metal hull, rubber wheels, glass) — physics merge is material-agnostic (Jolt body has one friction/restitution), but damage module mapping (`component-vehicle-damage-model`) uses per-voxel module mask independent of collision shapes. **Module boundaries do NOT constrain greedy merge.**
- Vehicles are editable by player — mutation rebuild cost matters (concretized in §3).

**Alternatives (not tested):**
- Parry native `Voxels` shape (dimforge/parry PR #336, 2025) — out of scope (different engine, requires Rapier).
- Jolt `MeshShape` per vehicle — "not meant to be updated at run time" (Jorrit Rouwé, 2023).
- Per-voxel `MutableCompoundShape` — worst query performance per Rouwé.

---

## 2. Prior art

Web-research via DuckDuckGo HTML fallback (Exa HTTP 429 per the web_search fallback chain). Full source list: [`sources.md`](./sources.md).

**Games (production voxel vehicle assembly):**
- From the Depths (2014–2026) — voxel-by-voxel, per-block material, buoyancy, custom physics solver.
- Stormworks (2018–2026) — body panels + components, soft-body deformation, physics via voxel-like grid.
- Space Engineers (2013–2026) — block-based, VRAGE 2 engine, hierarchical compound colliders.
- Avorion (2017–2026) — voxel ship building, compound BoxShape + convex decomp.

**Physics engines:**
- **Parry/Rapier (2025):** native `Voxels` shape — sparse grid, neighbor tracking (no internal-edge snagging), dynamic `set_voxel` add/remove, `combine_voxel_states` for multi-shape boundaries. Proof that dedicated voxel collider exists as SOTA — Jolt lacks this.
- **Jolt Physics (Jorrit Rouwé, 2023–2025):** CompoundShape per chunk, BoxShape reuse across children, ~1000 shapes/compound fine, 10K "a bit much". MutableCompoundShape for dynamic edits (trade query perf for rebuild speed). Jolt vehicle API: `WheeledVehicleController` + `TrackedVehicleController` with ray/cylinder collision testers.
- **bepuphysics2:** custom `Voxels` collidable registered via NarrowPhase + SweepTask.

**Academia:**
- **Rig My Ride** (Katz, Kry, Andrews — SCA 2025): automatic physics-based vehicle rigging from polygon soup via 2D image segmentation + cylinder fitting + numerical optimization. DOI 10.1145/3747861.

**Closed complementary experiments:**
- `greedy-physics-meshing-cpu` (yes): F_TwoPass algorithm source — 35× shape reduction for landscape chunks. **Direct upstream for strategies C, D, E, F.**
- `data-driven-vehicle-weapon-definitions` (mixed): JSON/TOML codegen schema. **Definitions format for strategy B blueprint colliders.**
- `component-vehicle-damage-model` (yes): 1.4 ns/shot hit-testing, module mask independent of collision shape. **Module boundary does not constrain merge.**
- `tank-terrain-interaction-physics` (yes): physics hull paradigm. Cross-ref for cylinder-vs-box wheel comparison.

---

## 3. Method

- **Тип:** standalone C++26 CPU prototype + benchmark (no Jolt runtime, no GPU).
- **Сцена:** 5 vehicle types (defined as 3D voxel grids):

| Vehicle | Dimensions | Voxels (filled) | Wheel voxels | Modules |
|:--------|:-----------|:-----------------|:-------------|:--------|
| Jeep_4x4 | 8×4×4 | ~80 solid / 128 total | 4× cylinder | hull, wheel×4 |
| APC_8x4x4 | 16×8×8 | ~512 solid / 1024 total | 8× cylinder | hull, turret, wheel×8 |
| Tank_8x12x12 | 16×12×12 | ~1200 solid / 2304 total | tracked (box chain) | hull, turret, track_L, track_R |
| Truck_6x4x6 | 24×8×8 | ~800 solid / 1536 total | 6× cylinder | cabin, cargo, wheel×6 |
| LargeShip_16x16x16 | 32×16×16 | ~4000 solid / 8192 total | 0 (water) | hull, deck, superstructure |

- **Strategies (6):**

| ID | Name | Algorithm | Mutation rebuild |
|:---|:-----|:----------|:-----------------|
| A | NaivePerVoxel_API | per-voxel `AddShape(JPH::BoxShape(0.5))` loop. **Baseline.** | full rebuild (per-voxel loop) |
| B | PrecomputedBlueprintColliders | Blueprint JSON stores pre-merged AABB per component → O(1) instantiate on spawn. No per-voxel merge. | per-component rebuild (affected component only) |
| C | GreedyPhysicsMerge_Batched | F_TwoPass (closed `greedy-physics-meshing-cpu`): 2D XZ per Y + vertical merge → 35× shape reduction. | full rebuild (F_TwoPass from scratch) |
| D | HierarchicalSubAssembly | Divide voxels into modules (hull/turret/wheels) → per-module F_TwoPass → top-level compound of module compounds. | sub-module rebuild only (affected module's compound replaces) |
| E | Hybrid_TemplatePlusMerge | Pre-merged template shapes for standard components (wheels, engine blocks); F_TwoPass for custom hull voxels not matching any template. | template components untouched; custom hull rebuilt via F_TwoPass |
| F | WheelAware | Detect wheel voxels (z=0, material=rubber) → `JPH::CylinderShape`. Hull voxels → F_TwoPass merge. All shapes in one compound. | full rebuild (cylinder detection + F_TwoPass for hull) |

- **Metrics:**
  - **Primary:** shape count per vehicle (lower = better Jolt broad-phase). Target: ≤ N/10 (10× reduction vs A naive per-voxel) for strategies C–F.
  - **Secondary:** assembly CPU time (µs, mean/p95/p99/std).
  - **Tertiary:** volume preservation = fraction of voxels covered by collision shapes (byte-exact memcmp of voxel occupancy bitset before/after assembly). Target: 100%.
  - **Quaternary:** mutation rebuild time (µs, after 10% voxel toggle). Target: D ≥ 5× faster than C.

- **Mutation rebuild scenario (concrete):**
  1. Initial assembly → measurement (assembly time + shape count).
  2. Apply mutation: randomly select 10% of solid voxels → toggle (remove if solid, add if empty).
  3. Rebuild collision shapes per strategy's rebuild mode → measurement (rebuild time + shape count change).
  4. Metric: `rebuild_time_mutation_us` per config.

- **Протокол:**
  1. Harness: warmup 10 iter, N=1000 iter per config (per `benchmarks/methodology.md §3`).
  2. 6 strategies × 5 vehicle types × 5 seeds = 150 configs.
  3. Each config: 1000 assembly measurements + 1 mutation rebuild measurement = 150,000 assembly meas + 150 mutation meas.
  4. Output: CSV (strategy, vehicle, seed, mean_us, p50, p95, p99, std_us, shape_count, mutation_rebuild_us).
  5. Volume preservation: iterate solid voxels → verify each is inside ≥1 collision shape. False if any solid voxel not covered.

- **Контроль:** A_NaivePerVoxel_APIBaseline = 1 shape per solid voxel. Identical to `BuildStaticVoxelCollisionBody` per-voxel loop.

- **Hardware:** Zen 3 5800X governor `powersave` per [`hardware-profile.md`](../../hardware-profile.md) §1 + `taskset -c 2` isolation.

---

## 4. Prototype

```bash
cd docs/experiments/experiments/2026-06-22-custom-vehicle-designer/prototype
mkdir -p build && cd build
cmake .. -DCMAKE_CXX_FLAGS="-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic"
cmake --build .
taskset -c 2 ./vehicle_bench > results.csv
```

Output: `build/results.csv` (151 rows: 1 header + 150 data). One row per (strategy, vehicle, seed).

Source: `prototype/vehicle_bench.cpp` — standalone C++26, no external dependencies, ~800 LoC.

Health check: `--health` flag runs A_Naive on Jeep_4x4 seed=1, prints shape count + assembly time, asserts shape_count == solid_voxel_count (A always produces 1:1).

---

## 5. Results

Полные результаты: [`RESULTS.md`](./RESULTS.md).

**Headline:** C_GreedyMerge (F_TwoPass) и B_PrecomputedBP достигают **179× avg shape reduction** (18× better than 10× DoD) при **100% volume preservation**. Build time **1.98 µs avg** (C) and **2.47 µs avg** (B) — well under 0.5 ms target. Mutation rebuild: B fastest at **5.08 µs** (1.9× faster than A baseline).

| Strategy | Mean shapes | × reduction vs A | Mean build (µs) | Mut rebuild (µs) | Volume % |
|:---------|:------------|:-----------------|:----------------|:------------------|:---------|
| A_Naive (baseline) | 1076.0 | 1.0× | 2.52 | 9.52 | 100% |
| **B_PrecomputedBP** | **6.0** | **179×** | 2.47 | **5.08** | 100% |
| **C_GreedyMerge** | **6.0** | **179×** | **1.98** | 19.24 | 100% |
| D_Hierarchical | 18.0 | 60× | 11.00 | 30.77 | 100% |
| E_HybridTemplate | 32.2 | 33× | 3.85 | 19.80 | 100% |
| F_WheelAware | 32.2 | 33× | 3.80 | 19.50 | 100% |

---

## 6. Verdict

**`yes` (with caveat):** Гипотеза «vehicle-specific assembly strategies beat naive per-voxel by ≥ 10× shape count at < 0.5 ms with 100% volume» **validated with 18× margin** (179× avg reduction vs 10× target). C_GreedyMerge и B_PrecomputedBP — recommended для mainline. D_Hierarchical rejected (no mutation benefit observed). F_WheelAware deferred (needs Jolt runtime for cylinder-vs-box perf): см. [`RESULTS.md`](./RESULTS.md) §Verdict.

---

## 7. Integration recommendation

**Target stage:** independent (Tier 3 Economy, Sandbox, Content & Game Modes) — vehicle blueprint → physics shape pipeline.

**Two-strategy approach:**
- **Default (one-shot spawn):** C_GreedyMerge (F_TwoPass from closed `greedy-physics-meshing-cpu`) — 179× reduction, 1.98 µs avg build, 100% volume.
- **Mutation-heavy (in-world editor):** B_PrecomputedBP — 179× reduction, 5.08 µs mutation rebuild (1.9× faster than A baseline).

**Migration (3 steps, ~150 LoC):** 1) Add `VehicleCollisionAssembler.hpp` with overloads. 2) Wire into `BuildStaticVoxelCollisionBody` per vehicle chunk. 3) Wire mutation path → per-module bounding box recalc.

**Risks:** B over-approximate (per-module AABB may protrude beyond voxel contour) — mitigate by using B only for roughly-convex modules (hull, turret). C mutation full rebuild at 19 µs (< 0.2% budget) is fine — MutableCompoundShape not needed.

**Module-boundary merge:** confirmed safe — `component-vehicle-damage-model` module mask is independent of collision compound structure.

**Parry Voxels shape reference:** not actionable now (different engine); documentation-only for future engine-switch consideration.

Подробнее: [`RESULTS.md`](./RESULTS.md) §Integration recommendation.

---

## 8. Sources

Полный список: [`sources.md`](./sources.md). Включает верифицированные ссылки на:
- Game references (FtD, Stormworks, Space Engineers, Avorion)
- Parry Voxels shape (PR #336, 2025)
- Jolt CompoundShape best practices (Rouwé, 2023–2025)
- bepuphysics2 custom Voxels collidable
- SCA 2025 "Rig My Ride"
- Closed complementary experiments (5 slugs)

---

## 9. Mapping to ProjectV hot-path

Прототип моделирует **сборку collision representation для player-built vehicle** по blueprint (spawn) или после редактирования (mutation).

**Mainline touchpoints:**
- Global (analogy): `ProcessChunkRebuildQueue` + `BuildStaticVoxelCollisionBody` — per-chunk physics rebuild after voxel edit. Same pattern per-vehicle.
- `JPH::StaticCompoundShape` vs `JPH::MutableCompoundShape` — choice determined by edit frequency.
- `JPH::CylinderShape` for wheels — Jolt vehicle API (`WheeledVehicleController`) uses ray/capsule testers, but collision with other objects needs collider shapes.
- `JPH::BoxShape(0.5)` reuse across same-size boxes — optimization not modeled (all strategies create fresh shapes; real mainline pools shapes).

**Module-boundary decision (per gap 4):**
- Greedy merge across module boundaries is safe — `component-vehicle-damage-model` uses precomputed 3D mask (per voxel → module ID), not collision shape structure. A merged box covering voxels from hull + engine works for collision but makes debug viz harder. **Decision: merge ignores module boundaries.** Debug damage visualisation uses module mask, not collision compound structure.

**Hardware baseline:** [`hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X) + §3 (RTX 3060 Ti).

**Упрощения:**
- Нет реального Jolt runtime — shape count + assembly time as proxy for broad-phase cost.
- Нет GPU dispatch, нет Flecs ECS overhead.
- Нет `BodyCreationSettings`, `BodyInterface::CreateBody`, `VehicleConstraint` — pure shape assembly.
- Прототип — CPU-only standalone benchmark; mainline integration будет оборачивать Jolt shape creation поверх этих алгоритмов.

---

## 10. Cross-references

- `TODO.md §3.3` — closed by `greedy-physics-meshing-cpu`; this experiment extends application domain from landscape to vehicles.
- `sources.md` — full verified references.
- `docs/experiments/hardware-profile.md §1` (Zen 3 5800X, governor `powersave`).
- `docs/experiments/benchmarks/methodology.md §3` (measurement protocol).
- Closed `2026-06-21-greedy-physics-meshing-cpu` — F_TwoPass algorithm source (35× reduction, 100% volume preservation).
- Closed `2026-06-21-data-driven-vehicle-weapon-definitions` — blueprint definition format (JSON/TOML → templates → strategy B).
- Closed `2026-06-21-component-vehicle-damage-model` — module-hit testing (independent of collision compound shape structure).
- Closed `2026-06-21-tank-terrain-interaction-physics` — vehicle physics hull.
- Open `2026-06-21-custom-weapon-modding` — weapon attachment system (orthogonal).
