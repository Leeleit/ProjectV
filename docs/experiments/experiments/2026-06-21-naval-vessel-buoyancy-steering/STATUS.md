# STATUS — Naval Vessel Buoyancy & Steering

**Closed:** 2026-06-21 (single session, ~1h)
**Agent:** self
**Verdict:** `mixed` (per strategy; `yes` for D_Voxel6DOFAddedMass as recommended default)

**Phase tracker:**
- Phase 0 (reservation per §13.1): DONE — `research/backlog.md §In progress` + `INDEX.md §5 Active` + folder `experiments/2026-06-21-naval-vessel-buoyancy-steering/`.
  - **Note:** Per §13.3 anti-duplicate recovery, this slug was selected after race-condition with parallel agent on `2026-06-21-aircraft-damage-model`. Adjacent h-priority chosen to preserve cross-axis relevance.
- Phase 1 (skeleton README/STATUS): DONE.
- Phase 2 (web-research): DONE — 4 primary + 6 cross-references verified in `sources.md`. Direct `webfetch` to canonical sources (Metacentric height + Added mass Wikipedia + canonical cross-refs).
- Phase 3 (prototype): DONE — `prototype/naval_vessel_bench.cpp` ~485 LoC (5 strategies, 5 scenes, 3 ship templates, 6-DOF solver with added mass).
- Phase 4 (build + run + collect results.csv): DONE — Clang 22.1.6 build green with 5 cosmetic warnings. Wall time 0.15 sec. Output `prototype/build/results.csv` (126 rows, 7 KB).
- Phase 5 (write-up RESULTS + finalize README + STATUS closure): DONE.
- Phase 6 (single-pass sync per §13.5: backlog §Closed + INDEX §6 + this STATUS): DONE.

**Blocker:** нет.

**Cross-axis:**
- **Orth** к closed `tank-terrain-interaction-physics` (yes, ground vehicle) + `fixed-wing-flight-model-simulation` (yes, flight dynamics) + `helicopter-rotor-physics` (in-progress) + `ballistic-projectile-simulation` (yes, naval AA upstream) + `aircraft-damage-model` (in-progress, ship AA damage) + `procedural-military-terrain-gen` (closed yes, depth maps) + `water-surface-rendering` (in-progress, naval rendering).
- **Complementary** к `after-action-replay-system` (closed mixed, buoyancy must be deterministic) + `lockstep-state-sync-hybrid-netcode` (closed mixed, ship state = lockstep node).

**Outputs:**
- `prototype/naval_vessel_bench.cpp` (485 LoC)
- `prototype/CMakeLists.txt` (optional; can also use direct clang++ command)
- `prototype/build/naval_vessel_bench` (74 KB binary)
- `prototype/build/results.csv` (126 rows × 8 cols)
- `RESULTS.md` (full synthesis: 5 strategies × 5 scenes × 5 seeds = 125 measurements, headline tables, per-template analysis, cross-axis observations, surprising findings, caveats)
- `sources.md` (4 primary + 6 cross-references)
- `README.md` (8 sections, complete with §5 Results, §6 Verdict, §7 Integration recommendation)

**Headline numbers:**
- D_Voxel6DOFAddedMass ⭐ = universal recommended default (9-20 ns/ship across 4-512 ships).
- 5-10% threshold per `optimization-philosophy.md`: all non-baseline strategies cross massively (per-ship cost 0.001% of 30 Hz frame budget).
- D adds 4× cost vs C but provides 6-DOF ship dynamics (roll, pitch, yaw, propeller, rudder).
- Total fleet cost <5 ms target exceeded by 4000× at 100 ships.

**Sync (per §13.5):**
- `backlog.md §In progress` → `§Closed` (with full closure note + reservation record removed)
- `INDEX.md §5 Active` → `§6 Recent closed` (table row + entry)
- This STATUS.md (closure note)
- `agent/workspace.md`: NOT in scope (this is for mainline agent per `docs/experiments/AGENTS.md §2`)
