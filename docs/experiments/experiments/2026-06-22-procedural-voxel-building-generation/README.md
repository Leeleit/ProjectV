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

_To be filled after benchmark._

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