# 2026-06-22-obstacle-construction — Obstacle Construction System (Concrete Barriers, Dragon's Teeth, Abatis, Berms)

**Status:** in-progress
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent (Stage 6+ military sandbox — Tier 1 Physics × Tier 2 Engineering)
**Estimated effort:** M
**Author:** self (per operator instruction «выбирай свободную тему или придумывай свою исследуй»)

---

## 1. Hypothesis

Man-made defensive obstacles (concrete barriers, anti-tank ditches, Czech hedgehogs / dragon's teeth, abatis, berms) form a strategic layer on top of existing `trench-fortification-construction` [mixed] + `field-fortifications-system` [closed mixed] + `bridge-building-repair` [closed mixed] experiments. We hypothesize that:

- **H1 cost:** obstacle construction per unit costs <0.5 ms (matching closed `trench-fortification-construction` B_TemplateAABB_RLE winner at 2.55× speedup over naive), since obstacles are voxel-pattern templates with primitive physics hull.
- **H2 layering:** obstacles can be layered (wire → dragon's teeth → concrete barrier → bunker) as a strategic template, achieving 4× cost vs single obstacle due to overlap-detection + sort-by-density.
- **H3 anti-vehicle:** obstacles block vehicle movement (Czech hedgehog blocks tank tread), reducing vehicle reach by 60-80% vs no obstacles.
- **H4 anti-infantry:** obstacles channel infantry movement through gaps (anti-tank ditch blocks wheels but infantry can cross), reducing effective combat width by 30-50%.

5 strategies compared:
- A_NaivePerObstacle: place each obstacle sequentially with full BFS validation.
- B_TemplateAABB_RLE: template-based, RLE-compressed AABB overlap detection (proven winner in `trench-fortification-construction`).
- C_ParallelZoneSplit: Flecs worker-pool zone split, parallel placement.
- D_DependencyLayeredSort: topological sort by layer (wire→ditch→teeth→bunker), then batch construction.
- E_StrategicTemplate_Composite: pre-composed layered defense template (drill program from military field manuals).

---

## 2. Prior art

Web-research (planned sources.md):
- Wikipedia "Dragon's teeth" — WWII anti-tank obstacles, 1940 Siegfried Line / Atlantic Wall.
- Wikipedia "Czech hedgehog" — 1935 František Skupa design, WWII Soviet T-34 stopper.
- Wikipedia "Barbed wire" — 1874 Glidden invention, WWI/Ypres trench warfare.
- Wikipedia "Concertina wire" — modern barbed wire evolution.
- Wikipedia "Hesco bastion" — 1980s British military, gabion-style rapid wall.
- Wikipedia "Anti-tank ditch" — Russian hedgehog lineage, Normandy bocage.
- Wikipedia "Field fortification" — overview.
- WWII field manuals (US Army FM 5-15 Field Fortifications).
- War Thunder "Fortifications" — game reference for layered defense.
- Foxhole game — Foxhole Devblog #73 Voronoi region zones for strategic templates.
- ARMA 3 — fortification building system.

---

## 3. Method

- **Type:** standalone C++26 CPU prototype + benchmark.
- **Scene:** 5 obstacle types (concrete_barrier, anti_tank_ditch, czech_hedgehog, abatis, berm) × 5 densities (5/10/25/50/100 obstacles) × 5 layering modes (single/layered_2/layered_3/layered_4/full_template).
- **Metrics:** construction time (µs), placement validity (% overlap-free), anti-vehicle blocking %, anti-infantry blocking %, memory footprint (bytes), cost (resource units consumed).
- **Control:** A_NaivePerObstacle baseline.
- **Protocol:** warmup 10 iter → 1000 measured iter per config. 5 strategies × 5 obstacle types × 5 densities × 5 layering modes × 5 seeds × 1000 iter + 10 warmup = 625,000 main measurements.

---

## 4. Prototype

`prototype/obstacle_bench.cpp` — standalone C++26, Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG`.

```bash
cd prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic obstacle_bench.cpp -o build/obstacle_bench
./build/obstacle_bench
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

- **Hot-path:** Stage 6+ military sandbox — Tier 1 Physics × Tier 2 Engineering. Obstacle construction occurs during deployment phase (player-placed) and AI-defensive preparation phase.
- **Prototype maps to:** `src/fortification/ObstacleSystem.{hpp,cpp}` — function `placeObstacle(chunk_xz, type, rotation, density)`, `validateLayering(obstacles)`.
- **Assumptions:** Obstacles fit within 8³-32³ voxel bounding box. Single-chunk obstacle placement; multi-chunk obstacles would be follow-up. Voxel-only obstacles (no cable fences, no steel beams).
- **Unmeasured:** GPU instanced rendering of obstacles (orth axis — closed `mesh-shader-mega-instancing` mixed), physics JPH coupling (closed `multi-resolution-collision-broadphase` mixed), AI pathfinding re-routing around obstacles (closed `voxel-navmesh-graph-generation` yes).

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) — Zen 3 5800X (per §1), 62.7 GiB RAM (§2), RTX 3060 Ti 8 GiB (§3).