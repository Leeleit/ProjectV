# STATUS — 2026-06-21-procedural-military-terrain-gen

**Phase:** 4 of 4 (CLOSED dirty per operator policy)
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21 (single session)
**Blocker:** нет
**Verdict:** `mixed` — C и E validated как scene-specific winners, per-scene adaptive dispatcher recommended

---

## Final state

Closed dirty per `AGENTS.md §5.4` + operator "close dirty without prompt" directive. **No commit performed** (per protocol). All 4 phases complete in single session (~3h).

## Phase log

- **Phase 0 (2026-06-21):** reservation per `AGENTS.md §13.1` — moved from `research/backlog.md §Open` → `§In progress`. Sentinel §13.7 confirmed no duplicate.
- **Phase 1 (2026-06-21):** web research complete (20+ primary sources via Exa `web_search`).
- **Phase 2 (2026-06-21):** prototype built (C++26, Clang 22.1.6, build green 2 cosmetic warnings on unused constants).
- **Phase 3 (2026-06-21):** full sweep complete — 6,250 main measurements in 17s wall time (xargs -P 8 parallel).
- **Phase 4 (2026-06-21, current):** RESULTS.md + README §5/§6/§7 populated; INDEX.md + backlog.md sync pending.

## Headline (6,250 measurements)

| Strategy | Time (µs) | Features (per km²) | Verdict |
|---|---:|---:|---|
| A_PureNoise_OpenSimplex2 | 16,384 | 1,471 | baseline |
| B_CellularAutomata_Ridges | 17,390 (+6.3%) | 636 (-57%) | **NOT recommended** for rich terrain |
| C_StampLibrary_Military | 16,875 (+3.0%) | 1,544 (+5%) | **Universal safe default** |
| D_TacticalWFC (placeholder) | 16,724 (+2.1%) | 1,478 (≈0%) | Placeholder; real WFC deferred |
| E_Hybrid_CA_Stamps | 17,996 (+10.1%) | 772 (-48%) | **Best for poor terrain** (3-9x on flat/urban) |

**Key finding:** No single strategy wins on all scenes. **Per-scene adaptive dispatcher is the right architecture.**

## Next tick (operator decides)

- Operator: review README §6 verdict + §7 integration recommendation
- If approved: schedule mainline integration session (Stage 4.1 world gen extension, ~600 LoC, 2-3 sessions, M effort)
- D_TacticalWFC real implementation deferred to follow-up session
- Cross-axis consumers: `cover-system-terrain-adaptive` (in-progress Tier 2), `scenario-mission-editor` (backlog open)

## Risks / known issues

- **CPU prototype only** — no GPU dispatch, no NanoVDB integration. GPU port deferred.
- **D_TacticalWFC is placeholder** — current D just adds 5m to local maxes; real WFC impl (Piepenbrink 2025 nutWFC) is significant work.
- **Detector thresholds tuned** — divisors (60, 30, 200, 4, 50, 100, 50 cells/feature) are prototype estimates; production tuning would need visual confirmation.
- **Mutation cost not measured** — per-chunk feature regen on voxel edit out of scope.
- **No viewshed GPU ray query** — firing position detector uses local max + elevation proxy, not real GPU viewshed.
