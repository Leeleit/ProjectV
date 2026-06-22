# 2026-06-22-procedural-voxel-tree-generation — Procedural voxel tree generation via parametric L-systems

**Status:** in-progress
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent (Stage 4.1 World Gen — forest decoration)
**Estimated effort:** M
**Author:** self (agent)

---

## 1. Hypothesis

5-стратегийное сравнение для генерации 8³-воксельных деревьев:

- **A_TrunkOnly:** single-voxel trunk, no branching (baseline).
- **B_LSystem_Deterministic:** classic parametric L-system with fixed production rules per tree type (Prusinkiewicz & Lindenmayer 1990).
- **C_LSystem_Stochastic:** L-system with weighted rule selection for variation.
- **D_SpaceColonization:** Runions et al. 2005 space colonization algorithm — grow branches toward randomly distributed attraction points.
- **E_NoiseGuided_Growth:** 3D simplex-noise-guided branch direction with tropism + light-competition heuristic.

Hypothesis:
- B/C (L-system): <10 µs/tree, plausibility ≥0.6 (branch coverage, taper ratio, self-similarity).
- D (SpaceColonization): best biomimetic quality (plaus ≥0.8) but 5-10× cost vs L-system.
- E (NoiseGuided): most expensive (≥50 µs) but best for organic/weird shapes (dead trees, mangroves, magical flora).

Alternative approach in SOTA: Minecraft hardcoded tree generators (per-biome switch on trunk height + leaf radius) — extremely fast (<1 µs) but zero variety within biome.

---

## 2. Prior art

Web-research next (Phase 1). Planned sources:

- Prusinkiewicz & Lindenmayer 1990 "The Algorithmic Beauty of Plants" — canonical L-system reference.
- Wikipedia "L-system" — parametric, stochastic, context-sensitive variants.
- Runions, Lane, Prusinkiewicz 2005 "Modeling Trees with a Space Colonization Algorithm" (Eurographics 2005 best paper) — alternative to L-system.
- OpenSimplex noise for growth guidance fields.
- Minecraft Wiki "Tree" — oak, pine, jungle, dark oak, acacia generators as voxel tree reference.
- Closed `voxel-grass-foliage-rendering-pipeline` [mixed] — rendering, not generation.
- Closed `vegetation-destruction-interaction` [yes] — tree destruction, not generation.

---

## 3. Method

- **Type:** standalone C++26 CPU prototype + benchmark.
- **Scene:** 5 tree types × 5 seeds:
  - oak (broad deciduous), pine (conical), palm (tropical), dead (bare branches), bush (dense shrub).
- **Metrics:**
  - Generation time (µs/tree, mean/p95/stddev across 1000 iter).
  - Plausibility score: branch coverage (fraction of crown AABB filled) + taper ratio (base:tip diameter) + self-similarity (branch length distribution entropy).
  - Memory: voxel count per tree, code+data per strategy.
- **Control:** A_TrunkOnly baseline.
- **Protocol:** warmup 10 iter → 1000 measured iter per config. 5 strategies × 5 tree types × 5 seeds × 1000 iter = 125,000 main measurements.

---

## 4. Prototype

`prototype/tree_bench.cpp` — standalone C++26, Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG`.

```bash
cd prototype
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic tree_bench.cpp -o tree_bench
./tree_bench
```

Output: `build/results.csv` (126 rows: 1 header + 125 data).

---

## 5. Results

*Pending benchmark execution.*

---

## 6. Verdict

*Pending.*

---

## 7. Integration recommendation

*Pending.*

---

## 8. Sources

*Pending web-research (Phase 1).*

---

## 9. Mapping to ProjectV hot-path

- **Hot-path:** Stage 4.1 World Gen — per-chunk tree decoration pass after terrain generation.
- **Prototype maps to:** `src/worldgen/VegetationPass.cpp` — function `generateTree(chunkX, chunkY, chunkZ, biomeType, seed)`.
- **Assumptions:** 8³ chunk = single tree max. Real trees may need multi-chunk (jungle 4×4 trunk).
- **Unmeasured:** GPU tree instancing (rendering is orth axis, covered by `voxel-grass-foliage-rendering-pipeline`), per-voxel material cost (trunk wood + leaf + block entity).

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) — Zen 3 5800X.
