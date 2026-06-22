# 2026-06-22-procedural-voxel-building-generation — Procedural voxel building & structure generation axis

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-22
**Date closed:** 2026-06-22
**Stage link:** independent (Stage 4.1 World Gen × Stage 6+ military sandbox × Stage 5.x visual)
**Estimated effort:** M
**Author:** self (agent)

> ⚠️ **Race conflict notice (per `AGENTS.md §13.3`):**
>
> This slug was claimed independently by two parallel agents on `2026-06-22`. Per
> first-write-wins convention, the canonical closed entry is in:
> [`research/backlog.md §In progress`](../../research/backlog.md) (the closed-merged
> entry) + [`INDEX.md §6 Recent closed sessions`](../INDEX.md).
>
> **Canonical results (per first-write-wins):**
> [README.md](./README.md) below + [STATUS.md](./STATUS.md) + [RESULTS.md](./RESULTS.md) +
> [sources.md](./sources.md) + [`prototype/`](./prototype/) — mainline agent's work.
>
> **Alt variant (second-write, supplementary):**
> [`prototype_alt/`](./prototype_alt/) contains an alternative C++26 prototype with
> different cost model (no CCL inside hot loop) — kept for cross-validation only, NOT
> canonical. See [prototype_alt/README_alt.md](./prototype_alt/README_alt.md).

---

## 1. Hypothesis

5-стратегийное сравнение для генерации воксельных зданий / сооружений (8³–32³ воксельных структур) на плоском террейне:

- **A_StaticPrefab:** single hardcoded template per type (baseline).
- **B_TemplateComposition:** catalogue of sub-shape primitives (wall, floor, door, window, roof, chimney) composed via deterministic placement rules.
- **C_GrammarRuleBased:** CGA-shape-grammar-style recursive rule decomposition.
- **D_NoiseGuided_FloorPlan:** floorplan = noise-thresholded rooms on a 2D grid extruded vertically.
- **E_Hybrid_GrammarPlusNoise:** C + per-instance noise deformation.

See [`RESULTS.md`](./RESULTS.md) §4 for full hypothesis-vs-actual table.

---

## 2. Prior art

See [`sources.md`](./sources.md) for the canonical 10-source web research bibliography (Parish/Müller 2001, Wonka 2003, Müller 2006, CityEngine, Minecraft Jigsaw, Teardown, Luanti schematics, etc.).

---

## 3. Method

- **Type:** standalone C++26 CPU prototype + benchmark.
- **Scene:** 5 building types (residential_1storey, residential_2storey, industrial_warehouse, military_bunker, command_post) × 5 seeds.
- **Metrics:** generation time (µs/building, mean/p95/stddev), structural integrity (CCL = 1 component), wall continuity, roof coverage, door/window presence.
- **Protocol:** warmup 10 iter → 1000 measured iter per config. 5 × 5 × 5 × 1000 + 10 = **125,000 main measurements**.

---

## 4. Prototype

See [`prototype/`](./prototype/) — mainline agent's prototype. Build & run per `prototype/README.md` if present.

**Alt prototype (cross-validation):**
- `prototype_alt/building_bench_v2.cpp` — alternative impl with different cost model.
- Compile: `clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic prototype_alt/building_bench_v2.cpp -o prototype_alt/build/building_bench_v2`
- Run: `cd prototype_alt && ./build/building_bench_v2`

---

## 5. Results

See [`RESULTS.md`](./RESULTS.md) for full results table + hypothesis validation.

**Summary headline (per first-write-wins canonical results):**

| Strategy | Mean (µs) | Plausibility | Verdict |
|---|---:|---:|:---|
| A_StaticPrefab | 88.25 | 0.77 | baseline (no integration) |
| **B_TemplateComposition ⭐** | **94.63** | **0.85** | **YES (universal default)** |
| C_GrammarRuleBased | 106.77 | 0.79 | viable secondary |
| D_NoiseGuided_FloorPlan | 101.77 | 0.82 | niche organic-style only |
| E_Hybrid_GrammarPlusNoise | 118.42 | 0.80 | not worth 5.5× cost over B |

Key finding: **26-connectivity CCL plausibility evaluation dominates total cost** (~85-117 µs), not pure generation (sub-µs for all 5). Plausibility hypotheses ALL PASSED; cost hypotheses ALL REJECTED due to eval bottleneck.

---

## 6. Verdict

`concluded-verdict-mixed` per strategy; **`yes` for B_TemplateComposition ⭐** as universal recommended default. See [STATUS.md](./STATUS.md) + [RESULTS.md](./RESULTS.md).

---

## 7. Integration recommendation

**Default:** `PROJECTV_BUILDING_GEN=TEMPLATE` (B ⭐). 3-step migration ~630 LoC, M effort, **deferred до Stage 4.1** per `agent/workspace.md §2` operator 8x planning decision. See [RESULTS.md](./RESULTS.md) §7.

---

## 8. Sources

See [`sources.md`](./sources.md).

---

## 9. Mapping to ProjectV hot-path

- **Hot-path:** Stage 4.1 World Gen — per-chunk structure placement pass after terrain generation.
- **Prototype maps to:** `src/worldgen/StructurePass.cpp` — function `generateBuilding(building_type, seed)`.
- **Assumptions:** Building fits within 24³ voxel bounding box (industrial_warehouse largest). Multi-chunk settlements deferred to follow-up.
- **Unmeasured:** GPU instanced rendering of structures (orth axis), interior furnishing, AI pathfinding through doors.

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) — Zen 3 5800X.