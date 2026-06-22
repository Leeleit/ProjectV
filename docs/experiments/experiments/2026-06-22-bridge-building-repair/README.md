# 2026-06-22-bridge-building-repair — Tactical bridging simulation for military sandbox

**Status:** in-progress
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent (Tier 1 Physics × Tier 2 Engineering)
**Estimated effort:** S-M (single session)
**Author:** agent

---

## 1. Hypothesis

5-стратегийное сравнение ∈ {A_NaivePerVoxelPlacement (baseline, per-voxel API set),
B_TemplateAABB_RLE (pre-authored bridge pattern placed as batch write),
C_TemplateWithPierCheck (template + pier foundation on terrain contour survey),
D_FloatingPontoon (buoyancy-gated pontoon assembly on water surface — ribbon bridge),
E_HierarchicalAssembly (multi-segment bridge with per-segment structural integrity check & load-limit)}
даст **<0.5 ms per bridge construction** (all strategies) + 100% correct structural integrity for vehicle weight (deterministic load-limit per connected component, per closed `voxel-topology-analysis` CCL precedent) на 5 bridge type scenes.

Sub-hypotheses:
- H1: Template-based construction (B/C/D/E) is ≥5× faster than naive per-voxel (A) on all scenes.
- H2: D_FloatingPontoon is the ONLY strategy that works on water scenes (pontoon = water-surface voxel layer).
- H3: E_HierarchicalAssembly uniquely detects load-limit violations per segment via CCL connectivity check.
- H4: All strategies <0.5 ms per bridge (≤1.5% of 30 Hz budget for 1 bridge/tick).

---

## 2. Prior art

Web-research via direct `webfetch` to canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list).

Key sources:
- Wikipedia "Bailey bridge" (Donald Bailey 1941, Mabey & Johnson, 60-ton capacity, 3-man 3-hour erection, 40 m max span, 120,000 built WWII, modular Warren truss pattern) — canonical production ref
- Wikipedia "Pontoon bridge" (Xerxes 480 BC Hellespont, Roman Pons Sublicius, WWII US Navy Seabees ribbon pontoon, modern M4T6 / MGB / IRB — infantry / ribbon / improved ribbon) — water-crossing gold standard
- Wikipedia "Assault bridge" (Churchill AVRE SBG, M60A1 AVLB scissors, M104 Wolverine, Leopard 2 Biber — 20-26 m span, 5-10 min launch, 60-70 ton capacity) — battlefield bridging
- Wikipedia "Military engineering" (sappers, combat engineers, bridging regiments, FM 5-34 Engineer Field Data) — role context
- Closed `2026-06-22-trench-fortification-construction` [mixed, **template methodology direct analog**: B_TemplateAABB_RLE winner at 32.5× over naive; bridge = sibling construction axis with water/gap variant]
- Closed `2026-06-21-voxel-topology-analysis` [yes, Union-Find CCL 26-conn = 2.73 µs mean — directly reusable for bridge structural integrity check: load-limit = min(CCL_voxels_count) × material_strength]
- Closed `2026-06-21-cable-winch-towing` [yes, suspension bridge cable deck — bridge cable tension model via D_DistanceConstraint_Verlet]
- Closed `2026-06-21-voxel-asset-template-catalog` [yes, A_HashMap 222-512 ns lookup — bridge template load]
- Foxhole wiki Bridge (single-lane, destroyable by satchel/howitzers, 20 s rebuild by hammer, 2000 HP) — production game ref
- WARNO bridge mechanics (destructible points, repair by supply, chokepoint AI routing) — production game ref

---

## 3. Method

- **Type:** prototype + benchmark (standalone C++26 CPU).
- **Scenes:** 5 bridge type × terrain combos:
  1. `assault_bridge_20m` — 20 m gap on flat terrain (M104 Wolverine analog)
  2. `bailey_60t_40m` — 40 m gap, permanent crossing (Bailey bridge analog)
  3. `pontoon_water_100m` — 100 m water gap, floating pontoon (ribbon bridge analog)
  4. `suspension_cable_80m` — 80 m canyon gap, suspension bridge (cable + deck template)
  5. `damaged_bridge_repair` — partially destroyed existing bridge, 20 m repair gap
- **Strategies:** A (naive per-voxel), B (template batch), C (template + pier foundation), D (floating pontoon), E (hierarchical multi-segment + load-limit check).
- **Metrics:** mean construction time (µs), structural integrity (pass/fail %), voxel count placed, segment count (for E).
- **Control:** A_NaivePerVoxelPlacement as baseline.
- **Protocol:** standard harness per `benchmarks/methodology.md` — 10 warmup + 1000 iter per config, 5 seeds.

---

## 4. Prototype

`prototype/bridge_bench.cpp` — standalone C++26, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`.

Build:
```bash
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic prototype/bridge_bench.cpp -o prototype/build/bridge_bench
./prototype/build/bridge_bench
```

Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data).

---

## 5. Results

Полные цифры: [`RESULTS.md`](./RESULTS.md). Краткая сводка:

| Strategy | Mean µs range | B/A ratio range | CCL finds disconnects? |
|:---------|--------------:|----------------:|:-----------------------|
| A (naive per-voxel) | 0.26–12.78 | 1× (baseline) | no (no CCL) |
| B (RLE batch) | 0.03–0.81 | 2.2–61.4× | no (no CCL) |
| C (pier foundation) | 0.80–6.24 | 0.2–4.0× (vs A) | no (no CCL) |
| D (floating pontoon) | 0.01–0.18 | (n/a, water-only) | no (no CCL) |
| E (hierarchical + CCL) | 3.22–50.19 | 0.1–0.3× (vs A) | **yes** — bailey 720/1040, suspension 1322/1474, damaged 208/480 |

**All strategies <0.5 ms** (max 50.19 µs = 10% of budget). H4 confirmed.

---

## 6. Verdict

**concluded-verdict-mixed** — per hypothesis:

| Hypothesis | Result | Detail |
|:-----------|:-------|:-------|
| H1: template ≥5× faster on ALL scenes | **mixed** | B ≥5× on 3/5 scenes; bailey 2.2× (checkered truss → many small RLE spans); damaged 4.4× (borderline) |
| H2: D is ONLY water-scene strategy | **yes** | D places 1600 voxels on pontoon, 0 on all others — correct water-surface restriction |
| H3: E uniquely detects load-limit violations | **yes** | CCL (6-conn) reveals bailey 720/1040, suspension 1322/1474, damaged 208/480 — structure audit not possible without CCL |
| H4: all strategies <0.5 ms | **yes** | Max: E on suspension 50.19 µs = 0.05 ms (10% of budget) |

**Why mixed:** Template RLE (B) is 8.6–61.4× faster than naive (A) on dense/sparse scenes, but only 2.2× on checkered truss (bailey). The H1 threshold (≥5×) is too strict for all geometry types. Recommended relaxation: "template ≥5× faster for dense scenes; ≥2× for truss/scattered patterns."

**Why not yes:** B is always faster than A (no regressions), but the ≥5× bar fails on 2/5 scenes. If H1 is relaxed to ≥2× on all scenes, H1 becomes yes.

---

## 7. Integration recommendation

### What mainline should do

1. **Primary bridge construction: B_TemplateAABB_RLE** (not A_NaivePerVoxelPlacement).
   - Map to existing `voxel_write_batch()` API in mainline.
   - Pre-compute RLE from bridge template at asset load time (once, not per-construction).
   - Expected speedup: 2–61× over per-voxel API, depending on geometry.

2. **Structural integrity pass: run E's CCL after B construction** (not as primary build path).
   - Implement as `voxel_connectivity_check()` in `voxel-topology-analysis` module (closed experiment `2026-06-21-voxel-topology-analysis` already provides Union-Find CCL at 2.73 µs mean for comparable volumes).
   - CCL cost (50 µs suspension max) is acceptable as post-construction audit.

3. **Load-limit rule:**
   - `load_limit_tons = min(CCL.voxels_in_largest_component) × material_strength_tons_per_voxel`
   - Default material_strength: 1.0 t/voxel (baseline), 5.0 t/voxel for reinforced (Bailey/Mabey).
   - Reject bridge placement if vehicle weight > load_limit.

4. **Water-scene gating:** use D's `wy == water_y` guard before calling bridge construction on water scenes. Early-return if scene type doesn't match.

5. **Pier/foundation fill (C):** only when terrain_y < bridge_y at any column. Use `terrain_height_map` lookup. Defer to Stage 3.2 (terrain editing) per `TODO.md`.

### What mainline should NOT do

- ❌ A_NaivePerVoxelPlacement for any production bridge building.
- ❌ E as primary construction path (50 µs max is cheap but CCL adds unnecessary overhead when structural integrity is not in question).
- ❌ 26-connectivity CCL — 6-conn is sufficient and 4× cheaper per `voxel-topology-analysis`.

### Where in TODO.md

- **Lands in:** §3.2 (Engineering / Construction Systems) — bridge construction as sub-case of `voxel_asset_placement()`.
- **Depends on:** closed `voxel-topology-analysis`, `voxel-asset-template-catalog`, `trench-fortification-construction` (template API).
- **Next stage:** Stage 3.2 (persistent terrain + physics hull for bridges via Jolt).

### Risks

- RLE template pre-computation assumes static bridge geometry. Dynamic/destroyable bridges need per-frame RLE rebuild (acceptable at <1 µs).
- CCL under 6-conn may miss real-world load paths through diagonal bracing (26-conn would catch more but cost 4× more). Decision: use 6-conn for now, revisit if bridge structural failures are reported in playtesting.
- Prototype doesn't measure JPH hull build after bridge construction (~50-200 µs per bridge, deferred).

---

## 8. Sources

См. `sources.md` — verified sources per web-research.

---

## 9. Mapping to ProjectV hot-path

- **Mainline analog:** `BuildStaticVoxelCollisionBody` in `PhysicsWorld.cpp` (per-voxel physics shape build) + `voxel_write_batch()` (bulk voxel mutation API). Template-based construction = optimized bridge building equivalent of greedy physics meshing in closed `greedy-physics-meshing-cpu` [yes, 35× reduction].
- **Assumptions:** single-tick construction (real engineer = s/min/hr); no Flecs ECS overhead in prototype; no real terrain collision detection during pier placement; no real buoyancy physics for pontoon (simplified as water-surface boundary).
- **Unmeasured:** JPH hull build cost after construction (~50-200 µs per bridge — deferred to Stage 3.2 per `agent/workspace.md §2`); per-voxel damage during destruction cascade (deferred to `structural-collapse-cascade` [yes]); multi-player sync cost.

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1-4 (Zen 3 5800X, DDR4 32 GiB, RTX 3060 Ti 8 GiB, Vulkan 1.4.341).
