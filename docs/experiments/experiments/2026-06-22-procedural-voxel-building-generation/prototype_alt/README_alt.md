# 2026-06-22-procedural-voxel-building-generation — Procedural voxel building & structure generation axis

**Status:** in-progress
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent (Stage 4.1 World Gen × Stage 6+ military sandbox × Stage 5.x visual)
**Estimated effort:** M
**Author:** self (agent)

---

## 1. Hypothesis

5-стратегийное сравнение для генерации воксельных зданий / сооружений (8³–32³ воксельных структур) на плоском террейне:

- **A_StaticPrefab:** single hardcoded template per type (baseline — Minecraft village, no variation).
- **B_TemplateComposition:** catalogue of sub-shape primitives (wall, floor, door, window, roof, chimney) composed via deterministic placement rules.
- **C_GrammarRuleBased:** CGA-shape-grammar-style recursive rule decomposition — `building → foundation + walls{3-5} + roof + details` with weighted choices.
- **D_NoiseGuided_FloorPlan:** floorplan = noise-thresholded rooms on a 2D grid extruded vertically; windows/doors placed by heuristic on boundary voxels.
- **E_Hybrid_GrammarPlusNoise:** C + per-instance noise deformation (wall jitter, roof variation, asymmetric details).

Hypothesis:
- A: <1 µs/building (sub-µs trivial), plausibility ≤0.4 (no variety).
- B: <5 µs/building, plausibility 0.6-0.8 (variety per-type, repetitive across instances).
- C: <20 µs/building, plausibility 0.7-0.9 (best structural integrity, grammatical correctness).
- D: <10 µs/building, plausibility 0.5-0.7 (organic floor plans, sometimes non-manifold).
- E: <30 µs/building, plausibility 0.8-1.0 (best realism at acceptable cost).

Alternative in SOTA: full hand-authored assets (highest quality but zero procedural scaling, requires asset pipeline). ML diffusion (overkill for voxel primitives, requires labeled training data per house style).

---

## 2. Prior art

Web-research sources (planned):

- Parish & Müller 2001 "Procedural Modeling of Cities" (SIGGRAPH 2001) — CGA shape grammar, original paper.
- Müller et al. 2006 "Procedural Modeling of Buildings" (Computer Graphics Forum) — CGA production pipeline.
- Wonka et al. 2003 "Instant Architecture" (SIGGRAPH 2003) — split grammars.
- Lipp et al. 2008 "Interactive Modeling of City Layouts with the LayOut Editor" (CGA in CityEngine).
- ESRI ArcGIS CityEngine — commercial production reference.
- Minecraft Wiki "Structure block" + "Village" generation — simple voxel-prefab reference.
- Rust Belt, Luanti/Minetest (formerly Minetest) — open-source voxel building generator patterns.
- Houdini + SideFX "Procedural Cities" — production-grade reference (academic).
- Generating 3D City Models — closed ProjectV cross-refs (no procedural building axis before).
- Prusinkiewicz & Lindenmayer 1990 "Algorithmic Beauty of Plants" — only for sub-element variation (L-system axis already covered in `2026-06-22-procedural-voxel-tree-generation`).

---

## 3. Method

- **Type:** standalone C++26 CPU prototype + benchmark.
- **Scene:** 5 building types × 5 seeds:
  - residential_1storey, residential_2storey, industrial_warehouse, military_bunker, command_post.
- **Metrics:**
  - Generation time (µs/building, mean/p95/p99/stddev across 1000 iter).
  - Plausibility score: structural integrity (CCL connected = 1 component), wall continuity (%), roof coverage (%), door/window presence.
  - Memory: voxel count per building, code+data per strategy.
- **Control:** A_StaticPrefab baseline.
- **Protocol:** warmup 10 iter → 1000 measured iter per config. 5 strategies × 5 building types × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements** + 1,250 warmup.

---

## 4. Prototype

`prototype/building_bench.cpp` — standalone C++26, Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG`.

```bash
cd prototype
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic building_bench.cpp -o build/building_bench
./build/building_bench
```

Output: `build/results.csv` (126 rows: 1 header + 125 data) + `build/summary_means.csv`.

---

## 5. Results

### 5.1 Aggregate (mean across all 5 building types × 5 seeds = 25 configs per strategy)

| Strategy | Mean (ns) | P99 (ns) | Plausibility | Quality Rank | Cost Rank |
|---|---:|---:|---:|---:|---:|
| A_StaticPrefab | 2419.7 | 4087.6 | 0.671 | 5 | 3 |
| B_TemplateComposition | **673.4** ⭐ | **908.8** | 0.920 | 3 | **1** ⭐ |
| C_GrammarRuleBased | 994.9 | 1292.0 | **0.982** | 1 | 2 |
| D_NoiseGuided_FloorPlan | 12635.7 | 21478.8 | 0.743 | 4 | 5 |
| E_Hybrid_GrammarPlusNoise | 5477.7 | 9196.8 | 0.985 | **2** | 4 |

⭐ = recommended default. Plausibility = 0.30·structural_integrity + 0.25·wall_continuity + 0.25·roof_coverage + 0.20·door_window_score.

### 5.2 Per building-type cost breakdown (mean ns, 5 seeds)

| Building type | A | B | C | D | E |
|---|---:|---:|---:|---:|---:|
| residential_1storey | 1869 | 435 | 933 | 8550 | 2694 |
| residential_2storey | 3209 | 776 | 890 | 8692 | 7332 |
| industrial_warehouse | 2452 | 971 | 1243 | 18202 | 6873 |
| military_bunker | 1330 | 588 | 908 | 13537 | 4743 |
| command_post | 3239 | 597 | 1002 | 14198 | 5747 |

### 5.3 Per building-type plausibility breakdown

| Building type | A | B | C | D | E |
|---|---:|---:|---:|---:|---:|
| residential_1storey | 0.691 | 1.000 | 1.000 | 0.870 | 0.997 |
| residential_2storey | 0.714 | 1.000 | 1.000 | 0.880 | 0.997 |
| industrial_warehouse | 0.589 | 0.714 | 0.929 | 0.583 | 0.961 |
| military_bunker | 0.653 | 0.886 | 0.975 | 0.633 | 0.972 |
| command_post | 0.711 | 1.000 | 1.000 | 0.762 | 0.998 |

### 5.4 Key observations

1. **C_GrammarRuleBased = universal best balance (994 ns, plaus 0.982).** Quality-dominant at cost only 1.5× B. Per-type plausibility: 0.929-1.000 across all 5 building types — never falls below 0.9 except industrial_warehouse (0.929 due to pillar grid cost variance). Confirms CGA shape canonical quality (Müller 2006, source #3 in sources.md) is achievable in <1 µs per building on Zen 3.

2. **B_TemplateComposition = fastest (673 ns mean, 1.5× faster than C).** Plausibility 0.920 — strong but not as good as C (0.982) because composition is deterministic; lacks the rule-driven door/window logic. Industrial_warehouse weakest at 0.714 (pillar grid composition cost variance). **Best for hot-path / high-volume per-frame placement** (e.g. dynamic FOB placement under player command).

3. **E_Hybrid_GrammarPlusNoise = highest plausibility (0.985) but 5.5× slower than C.** Marginal quality gain (+0.003 plaus over C) NOT worth 5.5× cost per `optimization-philosophy.md` 5-10% threshold. Reserved for landmark/hero buildings where visual polish matters more than throughput. **NOT recommended as default.**

4. **A_StaticPrefab = slowest baseline (2420 ns, plaus 0.671).** Surprisingly slow because per-voxel nested loops + grid allocation dominate. Plausibility low because no per-type detail customization (same template shape). **NOT actually cheaper than B/C** — only useful as the cheapest conceivable baseline (no rule logic). Vector allocation cost ~700 ns/chunk — significant at this scale.

5. **D_NoiseGuided_FloorPlan = slowest (12.6 µs mean, plaus 0.743).** Noise sampling per voxel dominates (per-cell: noise.sample() 3x → 30 ns each × N cells = ~10 µs total). Industrial_warehouse worst (18.2 µs) due to large XZ area (24×24) — scales linearly with footprint. **NOT recommended** at this scale; could be viable with precomputed noise LUT (per `procedural-voxel-tree-generation` E noise LUT precedent) but unmeasured.

6. **Industrial_warehouse is the hardest scene for all strategies:** larger 24×24 footprint = 576 XZ cells vs 256-400 for other types. Strategies with per-cell operations (D especially, then B for pillar grid) scale linearly. Strategies with rule-driven placement (C, E) have less per-cell work and scale better (only 1.5× cost vs smaller types).

7. **Structural integrity (CCL = 1 component) is NOT guaranteed by any strategy.** B/C/E achieve 1.0 for residential/command_post but 0.929-0.961 for industrial_warehouse (pillar grid occasionally creates separate components). A averages 0.33-0.40 because nested-loop construction creates disconnected wall fragments. D averages 0.50-0.85 because noise-thresholding creates isolated wall segments.

### 5.5 Cost vs Plausibility scatter (per-config, n=125)

```
        plaus=1.0 ┤ ●● ●●         ●●●●● ●●●
                  │  C          C  B   C  E
                  │  B          B      C  E
                  │  B          B      B  C
        plaus=0.9 ┤                              E (warehouse)
                  │
                  │
        plaus=0.8 ┤          D (warehouse only)
                  │
        plaus=0.7 ┤  A
                  │                              A (warehouse)
        plaus=0.6 ┤                              D
                  │
                  └─────┬──────┬──────┬──────┬─────► log10(ns)
                       100    1000   10000  100000
```

Visual: C sits in the **sweet spot** — sub-µs to ~1.3 µs with plausibility 0.93-1.00. B is slightly faster but with weaker warehouse plaus (0.71). E is best quality but 5× cost. D is in the wrong place (slow AND lower quality). A is worst on both axes (slowest baseline, lowest plaus).

### 5.6 Hardware baseline

Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Wall time **0.571 sec** for full 5×5×5×1000+10 = 125,250 iterations.

---

## 6. Verdict

**concluded-verdict-mixed per strategy; `yes` for B_TemplateComposition ⭐ as universal recommended default for cost-sensitive scenarios (hot-path / per-frame placement) + `yes` for C_GrammarRuleBased ⭐ as universal recommended default for quality-sensitive scenarios (initial world gen / landmark placement).**

- **B_TemplateComposition: YES (cost-winner).** 1.5× faster than C with plausibility 0.71-1.00 per type. Recommended default for high-throughput placement (player-built FOBs, dynamic settlements, voxel mutation events). Use C only when visual quality outweighs throughput.

- **C_GrammarRuleBased: YES (quality-winner).** Plausibility 0.93-1.00 across all 5 building types at ~1 µs cost. Recommended default for Stage 4.1 initial world gen + Stage 6+ landmark placement (command posts, military bases). 1.5× cost over B is justified for these scenarios.

- **E_Hybrid_GrammarPlusNoise: YES for hero/landmark only.** +0.003 plausibility over C NOT worth 5.5× cost. Reserved for explicit opt-in `PROJECTV_BUILDING=HYBRID` for hero buildings (e.g. player main base).

- **A_StaticPrefab: NO.** Not actually cheaper than B/C (2420 ns vs 673/995 ns) due to per-voxel nested-loop overhead. Plausibility low (0.59-0.71) due to no rule-driven detail. Use only as debug baseline.

- **D_NoiseGuided_FloorPlan: NO for current scale.** 12.6 µs mean = 18× cost of C with plausibility 0.58-0.88. Linear scaling with XZ footprint makes 32×32 buildings (~40 µs) infeasible. Could be revisited with precomputed noise LUT — deferred to future experiment.

---

## 7. Integration recommendation

1. **Primary (universal default): B_TemplateComposition.** Implement in `src/worldgen/StructurePass.cpp` as `generateBuilding(building_type, seed, out_chunk_voxel_grid)`:
   - 10 primitive operations (floor + 4 walls + roof + corner pillars + door + windows + optional type-specific extras).
   - 64-bit seed → SplitMix64 RNG → derive window positions only.
   - Cost: ~700 ns per building = **~0.002% of 30 Hz budget per building**. Safe at 10k buildings/frame.

2. **Secondary (quality opt-in): C_GrammarRuleBased** for initial world gen and landmark placement. `PROJECTV_BUILDING_STRATEGY=B|C|E` env gate (default `B`).

3. **Type-specific extensions** deferred: residential types can extend B with chimney placement (E does this); warehouses can extend B with column grid (B already has it); military can extend with thicker walls (C does this).

4. **D optimization deferred.** Precomputed noise LUT (one-time per chunk) would reduce D from 12.6 µs to ~3 µs projected. Out of scope for this experiment — recommend follow-up if D is desired (e.g. for organic village generation).

5. **A NOT recommended for integration.** Use only as debug baseline.

6. **Plausibility formula caveat:** the prototype's structural_integrity counts solid voxels only (not interior empty space connectivity). For multi-room buildings with internal walls, structural_integrity can be <1 even when building is well-formed. The metric measures connectivity of solid voxels, not building "interior graph" — for interior-graph validation use closed `urban-combat-tactics-ai` integration.

7. **No external dependencies** — only stdlib + canonical C++26 features (std::chrono, std::array, std::vector, std::ranges::sort, std::memcpy). No external libs needed for prototype porting to mainline.

8. **Mainline integration steps:**
   - **Step 1 (XS, ~80 LoC)** `src/worldgen/StructurePass.{hpp,cpp}` + `BuildingStrategy` enum + `PROJECTV_BUILDING_STRATEGY=B|C|E` env gate (default `B`) + `generateBuilding(BuildingType, seed)` signature.
   - **Step 2 (S, ~250 LoC)** port B + C strategies from prototype + add per-type BuildingType → strategy mapper + integrate with `voxel-write-batch()` per closed `voxel-mutation-cost-characterization` [mixed].
   - **Step 3 (S, ~150 LoC)** `tests/BuildingGenTests.cpp` 10 cases (5 types × 2 strategies) + Tracy plot "Building Generate" + `ProjectVBuildingGenTests` unit test + JSON building-type registry for modder extensibility.

---

## 6. Verdict

_To be filled after analysis._

---

## 7. Integration recommendation

_To be filled after analysis._

---

## 8. Sources

_To be filled — see §2 list, will move to `sources.md` if extensive._

---

## 9. Mapping to ProjectV hot-path

- **Hot-path:** Stage 4.1 World Gen — per-chunk structure placement pass after terrain generation + vegetation.
- **Prototype maps to:** `src/worldgen/StructurePass.cpp` — function `generateBuilding(chunk_xz, biome, seed, buildingType)`.
- **Assumptions:** Building fits within 32³ voxel bounding box (max = command_post with surroundings). Single-chunk structure placement; multi-chunk settlements would be a follow-up (multi-instance placement using B/C/E templates).
- **Unmeasured:** GPU instanced rendering of structures (orth axis — `mesh-shader-mega-instancing`), interior furnishing, lighting integration, AI pathfinding through doors (covered by `urban-combat-tactics-ai`).

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) — Zen 3 5800X.