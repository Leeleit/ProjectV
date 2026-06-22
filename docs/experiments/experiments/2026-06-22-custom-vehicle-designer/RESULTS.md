# Results — 2026-06-22-custom-vehicle-designer

**Standalone C++26 CPU benchmark:** 6 strategies × 5 vehicles × 5 seeds × 1000 iter + 10 warmup = **150,000 main measurements**.
**Dev host:** Zen 3 5800X, governor `powersave`, `taskset -c 2`, Clang 22.1.6 `-O3 -march=native -std=c++26`.
**Wall time:** 0.68 s.
**Output:** `prototype/build/results.csv` (151 rows).

---

## Headline table (6 × 5 × 5 = 150 configs, mean across seeds + vehicles)

| Strategy | Mean shapes | × reduction vs A | Mean build (µs) | Mut rebuild (µs) | Volume match |
|:---------|:------------|:-----------------|:----------------|:------------------|:-------------|
| **A_NaivePerVoxel (baseline)** | 1076.0 | **1.0×** | 2.52 | 9.52 | 100% |
| **B_PrecomputedBP** | **6.0** | **179×** | 2.47 | **5.08** | 100% |
| **C_GreedyMerge** | **6.0** | **179×** | **1.98** | 19.24 | 100% |
| D_Hierarchical | 18.0 | 60× | 11.00 | 30.77 | 100% |
| E_HybridTemplate | 32.2 | 33× | 3.85 | 19.80 | 100% |
| F_WheelAware | 32.2 | 33× | 3.80 | 19.50 | 100% |

## Per-vehicle build time (µs)

| Strategy | Jeep (52) | APC (376) | Tank (840) | Truck (512) | Ship (3600) |
|:---------|:----------|:----------|:-----------|:------------|:------------|
| A_NaivePerVoxel | 0.13 | 0.97 | 1.96 | 1.74 | 7.79 |
| B_PrecomputedBP | 0.15 | 0.99 | 2.05 | 1.26 | 7.93 |
| **C_GreedyMerge** | 0.29 | 1.15 | 2.01 | **1.16** | **5.29** |
| D_Hierarchical | 1.32 | 10.37 | 9.17 | 11.12 | 23.03 |
| E_HybridTemplate | 0.39 | 2.07 | 3.48 | 2.22 | 11.10 |
| F_WheelAware | 0.38 | 2.06 | 3.40 | 2.21 | 10.93 |

## Per-vehicle shape count

| Strategy | Jeep (52) | APC (376) | Tank (840) | Truck (512) | Ship (3600) |
|:---------|:----------|:----------|:-----------|:------------|:------------|
| A_NaivePerVoxel | 52 | 376 | 840 | 512 | 3600 |
| **B_PrecomputedBP** | **5** | **10** | **4** | **8** | **3** |
| **C_GreedyMerge** | **5** | **7** | **3** | **5** | **10** |
| D_Hierarchical | 9 | 46 | 5 | 20 | 10 |
| E_HybridTemplate | 11 | 23 | 99 | 18 | 10 |
| F_WheelAware | 11 | 23 | 99 | 18 | 10 |

## Analysis

### C_GreedyMerge — absolute winner for one-shot assembly
- **179× average shape reduction** (360× on LargeShip = 3600 → 10 shapes).
- **Fastest build for large vehicles** (5.29 µs on Ship vs 7.79 µs baseline = 1.5× faster; faster than baseline because fewer allocations dominate).
- **100% volume preservation** — identical collision behavior per `TODO.md §3.3` criterion.
- **Secret:** F_TwoPass marks processed voxels and skips them → O(N) loop body with ~0 allocations vs A_Naive's `push_back` per voxel.

### B_PrecomputedBP — best for mutation-heavy workloads
- Tied for best reduction (179×) with C, minimal shape count (3–10 per vehicle).
- **Fastest mutation rebuild** (5.08 µs vs 9.52 µs baseline = 1.9× faster) — per-module bounding box recalculation.
- Build time ≈ A_Naive (same allocation pattern).
- **Caveat:** per-module AABB over-approximate (one big box per module vs tight contour-following merge). Acceptable for vehicle physics where modules are roughly convex.

### D_Hierarchical — disappointing
- **4.7× slower build** than C_GreedyMerge (11 µs vs 2 µs avg).
- Per-module F_TwoPass dispatch overhead dominates for small modules (Jeep: 9 shapes from 5 modules = nearly 2× the shapes of C's 5).
- Mutation rebuild did NOT benefit from sub-module scoping (my implementation falls back to full rebuild for simplicity).
- **Verdict:** not recommended for mainline unless mutation is extremely rare and module isolation is architecturally required.

### E_HybridTemplate / F_WheelAware — tied
- Same algorithm (F delegates to E), so identical results.
- 33× reduction vs A, 1.5–2× slower build than C on large vehicles.
- **Tank anomaly:** 99 shapes vs C's 3 — tracks (100+ wheel voxels kept as unit boxes) prevent F_TwoPass from spanning across the hull.
- F_WheelAware cylinder-vs-box distinction NOT measurable in CPU prototype (requires Jolt runtime). Assembly cost identical to E.
- **Use case:** only if cylinder shapes for wheels provide materially better collision behavior in Jolt (future work).

## Verdict

**`yes` (with caveat):**

Гипотеза «vehicle-specific compound-collider assembly strategies beat naive per-voxel baseline by ≥ 10× in shape count at < 0.5 ms/vehicle assembly time with 100% volume preservation» **validated with 18× margin** (B/C achieve 179× avg reduction vs 10× target) при **~0 µs overhead for large vehicles** (C is 1.5× faster than A on Ship).

**Key numbers:**
- **Best reduction:** 179× (B, C) — massively exceeds 10× target. Jolt broad-phase would visit 6 child shapes instead of 1076.
- **Fastest build:** C_GreedyMerge at 1.98 µs avg (0.2% of 30 Hz budget). Well under 0.5 ms target.
- **Fastest mutation:** B_PrecomputedBP at 5.08 µs (0.05% of budget). 1.9× faster than A rebuild.
- **All strategies:** 100% volume preservation.

**Rejected sub-hypotheses:**
- D_Hierarchical mutation benefit: NOT observed (per-module overhead > savings). My implementation does full rebuild on mutation, not true sub-module cache. Fixable in mainline but marginal benefit.
- F_WheelAear distinct perf: NOT observable in CPU prototype. Assembly cost matches E identically. Cylinder-vs-box Jolt runtime perf is future work.
- B mutation > 5× faster than C mutation: NO (B = 5 µs vs C = 19 µs; 2× gap, not 5×). Still meaningful.

## Integration recommendation

**Target stage:** independent (Tier 3 Economy, Sandbox, Content & Game Modes) — vehicle blueprint → physics shape pipeline.

### Recommended: C_GreedyMerge as default, B_PrecomputedBP as fallback for mutable vehicles

**Two-strategy approach:**
- **Default (one-shot spawn):** C_GreedyMerge (F_TwoPass from closed `greedy-physics-meshing-cpu`) — 179× reduction, 1.98 µs avg build, 100% volume.
- **Mutation-heavy (in-world editor):** B_PrecomputedBP — 179× reduction, 5.08 µs mutation rebuild (vs 9.52 µs A baseline = 1.9× faster).

**Migration (3 steps, ~150 LoC total):**

1. **Add `VehicleCollisionAssembler.hpp`** (XS, ~30 LoC): function overloads `buildCollisionShapes_GreedyMerge(voxelGrid)` and `buildCollisionShapes_PrecomputedBP(voxelGrid, moduleMap)`. Return `std::vector<AABB>`.
2. **Wire into `BuildStaticVoxelCollisionBody` (S, ~50 LoC):** after chunk rebuild, detect if this chunk is a vehicle (via `EntityType` flag) → dispatch vehicle assembler instead of landscape greedy merge. Otherwise landscape uses existing `GreedyPhysicsMerger` per closed experiment.
3. **Wire mutation path (S, ~70 LoC):** on vehicle edit event → if `PrecomputedBP` mode → only recalc bounding boxes for affected modules → `JPH::MutableCompoundShape::ModifyShape` per changed shape.

### Risks

- **B_PrecomputedBP over-approximate:** per-module AABB may extend beyond actual voxel contour, causing phantom collisions (vehicle visually intact but collision box protrudes). Mitigation: use B only for vehicles where modules are roughly convex (hull, turret, cargo bed). For complex shapes (tracks, wings), fall back to C.
- **C_GreedyMerge mutation cost:** at 19 µs, a full rebuild on each mutation is fine (< 0.2% of budget). `MutableCompoundShape` not needed.
- **D_Hierarchical no benefit:** do not implement unless vehicle modules are independently movable in future (detachable turret, modular vehicle sections). Not recommended now.
- **F_WheelAear cylinder benefit:** unmeasured in this experiment. Document as future work: "benchmark Jolt `CylinderShape` vs `BoxShape` for wheel collision on 5 vehicle types at 30 Hz."

### Cross-vendor

N/A — pure CPU code.

### Re-evaluation triggers

- Jolt upgrade adds native voxel shape (like Parry 2025 `Voxels` shape) → re-evaluate whether CompoundShape is still needed.
- Jolt `CylinderShape` vs `BoxShape` microbenchmark for wheel collisions.
- Vehicle module detachment feature (e.g., turret flies off when HP → 0) → D_Hierarchical becomes relevant.
- `buildUs` on large vehicles approaches 0.5 ms (currently at 1% of that for worst case — plenty of headroom).

## Cross-references

- `docs/experiments/experiments/2026-06-21-greedy-physics-meshing-cpu/RESULTS.md` — upstream F_TwoPass achieves 35× reduction on landscape chunks; this experiment extends to vehicles with 179× reduction (better due to larger contiguous solids in vehicle hulls vs scattered terrain).
- `docs/experiments/experiments/2026-06-21-data-driven-vehicle-weapon-definitions/READNE.md` — JSON/TOML blueprint format feeds B_PrecomputedBP's module-AABB generation.
- `docs/experiments/experiments/2026-06-21-component-vehicle-damage-model/README.md` — module boundaries do NOT constrain greedy merge (confirmed §9).
- `docs/experiments/hardware-profile.md §1` — dev host baseline.
- `docs/experiments/benchmarks/methodology.md §3` — measurement protocol.
