# STATUS — `2026-06-21-flanking-maneuver-ai`

**Phase:** 6 (closed per `AGENTS.md §13.5` single-pass sync)

**Started:** 2026-06-21
**Closed:** 2026-06-21 (single session, ~2h)
**Agent:** self
**Verdict:** `mixed` per scene tier; `yes` for C_CoverWeightedFlow as universal recommended default + E_HierarchicalBTSplit when ≥2-unit squads available.

**Phase tracker:**

- [x] **Phase 0 (reservation per §13.1):** DONE — `research/backlog.md §In progress` + this folder + `experiments/2026-06-21-flanking-maneuver-ai/{README.md,STATUS.md}`. Anti-duplicate sentinel clean per §13.7 (`rg "flanking.maneuver"` → 0 dedicated experiments; cross-refs only).
- [x] **Phase 1 (web-research):** DONE — 5 primary + 3 supplementary + 4 cross-axis closed ProjectV experiments = 12 verified references в [`sources.md`](./sources.md): Reynolds 1987 BOIDS + Isla 2005 Halo 2 BT + Colledanchise 2018 BT book + Colledanchise 2014 stochastic BT + Agis 2020 event-driven BT + Champandard 2012 BT Starter Kit + Lim 2010 DEFCON evolved BT + Reynolds 1999 steering behaviors.
- [x] **Phase 2 (design strategies + scenes):** DONE — 5 strategies (A_NoFlank / B_GeometricLShaped / C_CoverWeightedFlow / D_BayesianThreat / E_HierarchicalBTSplit) × 5 scenes (open_field / light_cover / urban_corridor / dense_urban / defensive_line).
- [x] **Phase 3 (prototype):** DONE — `prototype/flanking_bench.cpp` ~470 LoC standalone C++26 CPU prototype с synthetic cover map + threat map + Dijkstra flow field.
- [x] **Phase 4 (build + run + collect results.csv):** DONE — Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green 0 warnings (after 1 fix: `[[maybe_unused]]` cover parameter + `u8` typedef). 5 strategies × 5 scenes × 5 seeds × 5 units × 100 iter + 5 warmup = **62,500 main + 3,125 warmup = 65,625 plan calls**, wall time 6:58 на Zen 3 5800X. Output `build/results.csv` (126 rows = 1 header + 125 data, 9.3 KB) + `build/run.log` (17.3 KB).
- [x] **Phase 5 (write-up RESULTS + finalize README):** DONE — [`RESULTS.md`](./RESULTS.md) full headline analysis + per-strategy verdict + caveats. [`README.md`](./README.md) finalized with §1-§9 all sections populated.
- [x] **Phase 6 (single-pass sync per §13.5: backlog §Closed + INDEX §6 + this STATUS):** DONE.

**Blocker:** нет.

**Headline summary:**

| Strategy | Plan time (µs) | Exposure in defensive_line | Verdict |
|:--|--:|--:|:--|
| **A_NoFlank** (baseline) | 8.23-9.42 | 99.75 | `yes` for open_field only; `no` for cover-rich |
| **B_GeometricLShaped** | 16.13-16.72 | 35.67 | `no` — 2× slower than A, only modest benefit |
| **C_CoverWeightedFlow** ⭐ | 8.79-9.53 | **0.19** | `yes` — UNIVERSAL RECOMMENDED DEFAULT |
| **D_BayesianThreat** | 10.47-10.87 | 22.08 | `mixed` — Gaussian smoothing reduces discrimination |
| **E_HierarchicalBTSplit** ⭐ | 16.98-17.56 | **0.19** | `yes` for ≥2-unit squads — LOWEST exposure in every scene |

**Hypothesis validation:**
- ✅ Cover-aware flow achieves <500 µs/plan (max 17.56 µs = 28× headroom)
- ✅ C achieves 99.8% exposure reduction in defensive_line vs A
- ✅ A=C в open_field (no cover benefit) correctly validated
- ⚠️ B confirmed (64% reduction) but C/E vastly superior
- ❌ D rejected (Gaussian smoothing reduces discrimination vs binary threshold)

**Cross-axis (final):**

- Orth к closed Tier 2 AI per-unit (BT / cover / suppression / flow / AOI) + Tier 1 Physics + Tier 1 Netcode.
- Complementary к `hierarchical-tactical-ai-btree` [mixed, BT runtime] + `cover-system-terrain-adaptive` [mixed, cover score grid 0.2 µs/unit] + `suppression-mechanics` [mixed, suppress state for E split] + `flow-field-pathfinding-10k-units` [yes, BFS flow field foundation] + `radar-detection-system-simulation` [yes, sensor data upstream] + `ballistic-projectile-simulation` [yes, fire support layer].

**Anti-duplicate verification (§13.7):**
- `rg "flanking.maneuver"` over `INDEX.md` + `experiments/2026-06-21-*/` → 0 dedicated experiments.
- Parallel agents currently working on: `voxel-asset-template-catalog` (Phase 0), `lua-game-rules-scripting` (claimed), `sdf-subtractive-modeling-ui` (claimed). No overlap.