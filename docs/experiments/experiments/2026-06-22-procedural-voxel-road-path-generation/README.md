# 2026-06-22-procedural-voxel-road-path-generation — Procedural voxel road / path / runway generation axis

**Status:** in-progress
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent (Stage 4.1 World Gen × Stage 6+ military sandbox × Stage 3.x interaction)
**Estimated effort:** M
**Author:** self (agent)

---

## 1. Hypothesis

5-стратегийное сравнение для генерации voxel-surface road / path / runway segments (8³–32³ voxel road patches, ~3 voxels wide × N long):

- **A_StaticFlat:** flat 3×N dirt strip on ground (baseline — Minecraft village path).
- **B_TemplateComposition_Segmented:** catalogue of straight-segment + curve + intersection primitives composed deterministically along a polyline (cf. closed `procedural-voxel-building-generation` B ⭐ pattern).
- **C_GrammarRuleBased_Road:** CGA-shape-style grammar: `road → straight{N} → curve{left/right/T/Y/X} → intersection`, each with weighted choices per Wonka 2003 / Müller 2006.
- **D_NoiseGuided_Width:** width/dithering modulated by 2D noise → natural-looking ragged edges (Kelly & McCabe 2007 floor-plan extrusion pattern).
- **E_Hybrid_GrammarPlusNoise:** C + per-instance noise deformation (width dither + shoulder gravel scatter).

Hypothesis:
- A: <500 ns/segment (trivial flat).
- B: <2 µs/segment, plausibility 0.7 (curve support, intersection support).
- C: <8 µs/segment, plausibility 0.85 (grammatical correctness, junctions clean).
- D: <5 µs/segment, plausibility 0.5 (organic, sometimes ragged gaps).
- E: <12 µs/segment, plausibility 0.9 (best realism at acceptable cost).

Alternative in SOTA: hand-authored roads (max quality but no procedural scaling); pure mesh-deformed roads (no voxel semantics, breaks minability per Stage 3.x).

---

## 2. Prior art

Web-research sources (planned):

- Parish & Müller 2001 "Procedural Modeling of Cities" SIGGRAPH 2001 — road network generation from image maps via L-systems extended with global goals + local constraints (canonical reference for road generation in CityEngine).
- Müller et al. 2006 "Procedural Modeling of Buildings" CGF — CGA shape grammar; street networks via subdivision + lot placement.
- Sun et al. 2002 "Instant Procedural Modeling of Trees" — adjacency-based procedural placement.
- Kelly & McCabe 2006 "A Survey of Procedural Techniques for Generating Urban Environments" — road network generation methods comparison.
- ESRI ArcGIS CityEngine — commercial production reference for street networks + CGA.
- Minecraft Wiki "Road" / "Village path" — simple dirt / gravel / cobblestone road generation (per-biome style).
- Foxhole / WARNO / Squad — military sandbox road grid patterns (orthogonal grid vs organic network).
- OpenStreetMap + procedural road extractor — production reference for real-world topology.
- Closed `procedural-voxel-tree-generation` [yes, B_LSysDet validated at 0.27 µs] — cross-axis sibling procedural axis.
- Closed `procedural-voxel-building-generation` [yes, B_TemplateComposition validated] — cross-axis sibling procedural axis (same Stage 4.1 world gen domain).

---

## 3. Method

- **Type:** standalone C++26 CPU prototype + benchmark.
- **Scene:** 5 road types × 5 seeds:
  - dirt_path (single-biome dirt), cobble_road (urban), gravel_runway (military), gravel_motorway (4-lane), stone_highway (asphalt-style cobble).
- **Metrics:**
  - Generation time (µs/segment, mean/p95/p99/stddev across 1000 iter).
  - Plausibility score: connectivity (BFS), surface continuity (no gaps in road segment), edge straightness (low stddev of XZ position).
  - Memory: voxel count per segment, code+data per strategy.
- **Control:** A_StaticFlat baseline.
- **Protocol:** warmup 10 iter → 1000 measured iter per config. 5 strategies × 5 road types × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements** + 1,250 warmup.

---

## 4. Prototype

`prototype/road_bench.cpp` — standalone C++26, Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG`.

```bash
cd prototype
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic road_bench.cpp -o build/road_bench
./build/road_bench
```

Output: `build/results.csv` (126 rows: 1 header + 125 data) + `build/summary_means.csv`.

---

## 5. Results

### 5.1 Aggregate (mean across all 5 road types × 5 seeds = 25 configs per strategy)

| Strategy | Mean (ns) | P99 (ns) | Plausibility | Quality Rank | Cost Rank |
|---|---:|---:|---:|---:|---:|
| A_StaticFlat | **260.4** ⭐ | **306.4** | 1.000 | **1** ⭐ | **1** ⭐ |
| B_TemplateComposition | 347.9 | 433.6 | 0.831 | 4 | 2 |
| C_GrammarRuleBased | 406.5 | 508.8 | 0.845 | 3 | 3 |
| D_NoiseGuided_Width | 796.7 | 942.4 | **0.984** | 2 | 4 |
| E_Hybrid_GrammarPlusNoise | 1260.6 | 1641.6 | 0.847 | 5 | 5 |

⭐ = recommended default. Plausibility = 0.40·connectivity + 0.35·surface_continuity + 0.25·edge_straightness.

### 5.2 Per road-type cost breakdown (mean ns, 5 seeds)

| Road type | A | B | C | D | E |
|---|---:|---:|---:|---:|---:|
| dirt_path | 161 | 232 | 268 | 502 | 850 |
| cobble_road | 213 | 286 | 343 | 657 | 1040 |
| gravel_runway | 268 | 332 | 396 | 781 | 1247 |
| gravel_motorway | 333 | 405 | 481 | 945 | 1476 |
| stone_highway | 328 | 384 | 467 | 898 | 1390 |

### 5.3 Per road-type plausibility breakdown

| Road type | A | B | C | D | E |
|---|---:|---:|---:|---:|---:|
| dirt_path | 1.000 | 0.842 | 0.857 | 0.982 | 0.853 |
| cobble_road | 1.000 | 0.831 | 0.845 | 0.984 | 0.846 |
| gravel_runway | 1.000 | 0.829 | 0.844 | 0.985 | 0.846 |
| gravel_motorway | 1.000 | 0.824 | 0.839 | 0.985 | 0.842 |
| stone_highway | 1.000 | 0.829 | 0.840 | 0.985 | 0.846 |

### 5.4 Key observations

1. **A_StaticFlat = cost-winner (260 ns) AND plausibility-winner (1.000).** Counter-intuitive — perfectly straight dirt path scores higher than all curved variants because edge_straightness + surface_continuity both max out. **Recommended default for terrain decoration** where straight paths suffice.

2. **D_NoiseGuided_Width = 2nd plausibility (0.984) at 3× cost.** Surface continuity drops slightly (ragged edges = shoulder not road at edges), but connectivity stays high (single connected component) and edge_straightness remains high (noise dither is small ±1 voxel). **Recommended for natural-looking forest paths + medieval roads.**

3. **B/C = viable for curves.** B (template composition) faster than C (grammar) by ~15%. C supports T/Y intersections (plausibility metric could be extended). **Use B/C only when curves/junctions required** (urban roads, motorway interchanges).

4. **E_Hybrid = NOT worth 5× cost over A.** Marginal plausibility gain over B/C (0.847 vs 0.831-0.845) at 4.8× cost. Deformation doesn't actually improve metrics meaningfully (most noise dither falls within already-existing shoulder zone).

5. **All strategies <1.3 µs mean** — well within 30 Hz budget. At 30 Hz with 1000 road segments/frame budget = 33 ms, can support 25,000 segments/frame with A. Even D at 800 ns supports 41,000 segments/frame.

6. **Surface continuity is the limiting factor for B/C** (~83-86%) — curves at primitive boundaries create 1-voxel gaps. Could be fixed with overlap-aware composition (deferred).

7. **Per-type scaling:** wider roads (motorway 13×32 = 416 cells) are ~2× cost of narrower (dirt_path 5×24 = 120 cells). Linear in cell count as expected.

### 5.5 Plausibility component breakdown

| Strategy | Connectivity | Surface Continuity | Edge Straightness |
|---|---:|---:|---:|
| A_StaticFlat | 1.000 | 1.000 | 1.000 |
| B_TemplateComposition | 1.000 | 0.829 | 0.661 |
| C_GrammarRuleBased | 1.000 | 0.844 | 0.691 |
| D_NoiseGuided_Width | 0.984 | 0.961 | 1.000 |
| E_Hybrid_GrammarPlusNoise | 1.000 | 0.846 | 0.694 |

### 5.6 Hardware baseline

Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Wall time **0.083 sec** for full 5×5×5×1000+10 = 125,250 iterations.

---

## 6. Verdict

**concluded-verdict-mixed per strategy; `yes` for A_StaticFlat ⭐ as universal recommended default (cheapest + highest plausibility); `yes` for D_NoiseGuided_Width ⭐ as opt-in for natural-looking paths.**

- **A_StaticFlat: YES (universal default).** 260 ns mean + 1.000 plausibility = unbeatable trade-off for terrain decoration. Use as default for all straight road/path segments.

- **D_NoiseGuided_Width: YES (natural-style opt-in).** 0.984 plausibility at 800 ns. Recommended when roads need organic feel (forest paths, medieval roads, unpaved tracks). Set `PROJECTV_ROAD_STYLE=NATURAL` to enable.

- **B_TemplateComposition: MIXED (curves only).** 348 ns + 0.831 plausibility. Use ONLY when curves/junctions are required (urban roads, motorway interchanges) AND natural-style is not desired.

- **C_GrammarRuleBased: MIXED (T/Y junctions).** 407 ns + 0.845 plausibility. Use when road network topology requires T/Y junctions (military compound roads, intersections). Slightly slower than B.

- **E_Hybrid_GrammarPlusNoise: NO.** 1261 ns for marginal plausibility gain over B/C (0.847 vs 0.831-0.845). Deformation doesn't improve metrics meaningfully.

---

## 7. Integration recommendation

1. **Primary (universal default): A_StaticFlat.** Implement in `src/worldgen/RoadPass.cpp` as `generateRoadSegment(chunk_xz, road_type, polyline)`:
   - Trivial double loop placing ROAD voxels for rectangle + optional SHOULDER/KERB/LANE for non-dirt types.
   - 64-bit seed unused (deterministic placement).
   - Cost: ~260 ns per segment = **0.0008% of 30 Hz budget per segment**. Safe at 100k segments/frame.

2. **Secondary (natural opt-in): D_NoiseGuided_Width.** `PROJECTV_ROAD_STYLE=STRAIGHT|NATURAL` env gate (default `STRAIGHT` = A). For forest paths + medieval roads.

3. **Tertiary (curves/junctions): B (curves) + C (T/Y junctions) opt-in.** Use only when polyline has explicit curves or junctions. Defer to mainline integration when polyline API is finalized.

4. **Type-specific extras** (shoulder, kerb, lane) already implemented per-type per `RoadSpec`. No additional code needed.

5. **E NOT recommended for integration.**

6. **No external dependencies** — only stdlib + canonical C++26 features.

7. **Mainline integration steps:**
   - **Step 1 (XS, ~80 LoC)** `src/worldgen/RoadPass.{hpp,cpp}` + `RoadStrategy` enum + `PROJECTV_ROAD_STYLE=STRAIGHT|NATURAL` env gate (default `STRAIGHT`) + `generateRoadSegment(road_type, polyline)` signature.
   - **Step 2 (S, ~200 LoC)** port A + D strategies from prototype + add per-type RoadType → strategy mapper + integrate with `voxel-write-batch()` per closed `voxel-mutation-cost-characterization` [mixed].
   - **Step 3 (S, ~150 LoC)** `tests/RoadGenTests.cpp` 10 cases (5 types × 2 styles) + Tracy plot "Road Generate" + `ProjectVRoadGenTests` unit test + JSON road-type registry for modder extensibility.

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

- **Hot-path:** Stage 4.1 World Gen — per-chunk road/path/runway placement after terrain + vegetation + buildings.
- **Prototype maps to:** `src/worldgen/RoadPass.cpp` — function `generateRoadSegment(chunk_xz, road_type, polyline)`.
- **Assumptions:** Road segment = single 8³–32³ chunk patch along one polyline edge; multi-chunk road networks = follow-up (uses closed `voxel-topology-analysis` [yes] CCL for cross-chunk merging).
- **Unmeasured:** GPU instanced rendering of road segments (orth axis — closed `mesh-shader-mega-instancing`); AI pathfinding along roads (orth — closed `flow-field-pathfinding-10k-units` assumes navmesh exists).

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) — Zen 3 5800X.