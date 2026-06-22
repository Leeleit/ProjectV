# STATUS — `2026-06-21-structural-collapse-cascade`

**Status:** `concluded-verdict-mixed` (per strategy; `yes` for A_NaivePerTick ⭐ as universal default + D_QueueBFS_LoadChain as readable alternative + E as physics reference; `no` for B_DSU_ConnectivityLoss [REJECTED for single-shot workload]; `mixed` for C_DSU_StressCascade [most physical but 2.3× cost]).
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21 (single session, ~3h end-to-end)
**Stage link:** independent (cross-cutting Tier 1 Core Engine Systems: Physics — building destruction & demolition simulation; cross-cuts Stage 3.2 voxel destruction + Stage 6+ military sandbox [building demolitions, bunker breaching, siege warfare]).
**Estimated effort:** M (single session, ~3h end-to-end)
**Author:** self (per operator instruction «выбирай свободную тему или придумывай свою исследуй» 2026-06-21)

---

## Phase tracker

- [x] **Phase 0 (reservation per §13.1):** DONE — `research/backlog.md §Open → §In progress` + `INDEX.md §5 Active` + folder `experiments/2026-06-21-structural-collapse-cascade/`.
  - Anti-duplicate sentinel §13.7: `rg "structural-collapse-cascade"` finds only backlog.md cross-ref + explosion-crater-terrain-deformation (orth, terrain deformation ≠ building collapse).
  - `ls experiments/2026-06-21-structural-collapse-cascade/` → ENOENT before claim.
  - `INDEX.md §5 Active` + `backlog.md §In progress` → no parallel reservation.
- [x] **Phase 1 (skeleton README + STATUS):** DONE — README.md (8 mandatory sections per `_TEMPLATE/README.md`) + STATUS.md (this file).
- [ ] **Phase 2 (web-research):** IN PROGRESS — DuckDuckGo HTML endpoint working, 7+ primary sources identified (Teardown Tuxedo Labs 2022 + IBSIT mod + PRGD mod + Red Faction Guerrilla GeoMod + Voxel Physics Engine + VoxTool + Steam Workshop mod).
- [ ] **Phase 3 (prototype implementation):** PENDING — `prototype/collapse_bench.cpp` standalone C++26 CPU harness.
- [ ] **Phase 4 (build + run + measurements):** PENDING — Clang 22.1.6 build, 5 strategies × 5 scenes × 5 seeds × 1000 iter = **125,000 main measurements**.
- [ ] **Phase 5 (RESULTS.md + sources.md + sync §13.5):** PENDING — write-up + INDEX.md §6 + backlog.md §Closed.

---

## Current state (2026-06-21)

- **Claimed** `2026-06-21` by self per `AGENTS.md §13.1`.
- **Slug:** `2026-06-21-structural-collapse-cascade` (military sandbox axis — Tier 1 Core Engine Systems: Physics).
- **Differentiation vs closed `destructible-building-system` [mixed]:** that experiment covers **stability check** (will it fall?) at 2 Hz stress model + DSU for geometric cuts; this experiment covers **collapse wave propagation** (how does it fall?) — multi-chunk building-scale wave.
- **Differentiation vs closed `chunk-damage-fracture-model` [mixed]:** single-chunk (8³ = always 1 component) fracture on impact; this is multi-chunk (64³+) building-scale wave.
- **Cross-axis:** orth to in-progress parallel (`boid-flocking-steering-axis` + `group-formation-maneuver-axis`); complementary to closed `destructible-building-system` (upstream) + `voxel-topology-analysis` (CCL primitive) + `chunk-damage-fracture-model` (single-chunk) + `vegetation-destruction-interaction` (tree analogy) + `soft-body-physics-debris` (post-collapse cloth).
- **Hypothesis (one-line):** правильная стратегия ∈ {A_NaivePerTick, B_DSU_ConnectivityLoss, C_DSU_StressCascade, D_QueueBFS_LoadChain, E_PhysicsSolver_JPH_ReducedOrder} обрабатывает 64×64×64 voxel building (8 chunks³) collapse wave propagation при <10 µs/building per tick для CPU-only strategies B/C/D; JPH reduced-order (E) = accuracy gold-standard.

---

## Blocker

Нет.

## Next action

Phase 2 → Phase 3: finalize web-research (sources.md) → implement `prototype/collapse_bench.cpp`.

---

## Decision log

- 2026-06-21: claimed slug per §13.1 (structural-collapse-cascade was the highest-priority still-open Tier 1 Physics topic; sentinel §13.7 clean; orth to all in-progress parallel sessions).