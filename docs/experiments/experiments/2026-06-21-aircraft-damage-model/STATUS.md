# STATUS — Aircraft Damage Model

**Started:** 2026-06-21
**Agent:** self
**Expected verdict:** `mixed` or `yes` (component hit-table architecture validated; cascading failure cost; GPU particle proxy cost)

**Phase tracker:**
- Phase 0 (reservation per §13.1): DONE — `research/backlog.md §In progress` + `INDEX.md §5 Active` + folder `experiments/2026-06-21-aircraft-damage-model/`.
- Phase 1 (skeleton README/STATUS): DONE.
- Phase 2 (web-research): PENDING — War Thunder damage model + DCS + IL-2 Sturmovik + hit-table architecture literature.
- Phase 3 (prototype): PENDING — standalone C++26 CPU analytical cost model, 5 strategies × 5 scenes × 5 seeds × 1000 iter.
- Phase 4 (build + run + collect results.csv): PENDING.
- Phase 5 (write-up RESULTS + finalize README + STATUS closure): PENDING.
- Phase 6 (single-pass sync per §13.5: backlog §Closed + INDEX §6 + this STATUS + agent/workspace.md): PENDING.

**Blocker:** нет.

**Cross-axis (claimed in §In progress references):**
- Orth к closed `component-vehicle-damage-model` (yes, ground vehicles only) + `fixed-wing-flight-model-simulation` (yes, flight model = upstream input) + `ballistic-projectile-simulation` (yes, projectile sim = upstream input) + `after-action-replay-system` (mixed, determinism = required for damage cascades) + `lockstep-state-sync-hybrid-netcode` (mixed, damage state must be deterministic) + `recon-intel-fog-of-war` (closed yes, smoke/fire visual output) + `volumetric-fog-atmosphere-rendering` (mixed, fire/smoke dispersion).
- Complementary к `wind-simulation-ballistics` (in-progress, fire spread direction) + `helicopter-rotor-physics` (in-progress, shared component model).
