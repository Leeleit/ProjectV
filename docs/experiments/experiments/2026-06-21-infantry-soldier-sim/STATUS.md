# STATUS — Infantry Soldier Simulation

**Started:** 2026-06-21
**Agent:** self
**Expected verdict:** `yes` (sub-microsecond CPU cost for detailed state + loadout + limb damage per soldier; Flecs compatibility validated)

**Phase tracker:**
- Phase 0 (reservation per §13.1): DONE — `research/backlog.md §In progress` + `INDEX.md §5 Active` + folder `experiments/2026-06-21-infantry-soldier-sim/`.
- Phase 1 (skeleton README/STATUS): DONE.
- Phase 2 (web-research): DONE — Arma 3 stamina system + Escape from Tarkov limb damage model + ECS infantry state performance.
- Phase 3 (prototype): DONE — standalone C++26 CPU analytical cost model, 5 strategies × 5 scenes × 5 seeds × 1000 iter.
- Phase 4 (build + run + collect results.csv): DONE.
- Phase 5 (write-up RESULTS + finalize README + STATUS closure): DONE.
- Phase 6 (single-pass sync per §13.5: backlog §Closed + INDEX §6 + this STATUS + agent/workspace.md): DONE.

**Blocker:** нет.

**Cross-axis:**
- Orth к closed `component-vehicle-damage-model` (yes, ground vehicles only) + `fixed-wing-flight-model-simulation` (yes, flight dynamics) + `helicopter-rotor-physics` (closed yes, flight dynamics) + `aircraft-damage-model` (closed yes, aircraft damage) + `after-action-replay-system` (closed mixed, replay determinism).
- Complementary к `interest-management-aoi-battle` (closed mixed, networking visibility of soldiers).
