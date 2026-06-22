# STATUS — 2026-06-22-squad-fire-team-command

**Phase:** *concluded-verdict-mixed* (per strategy) / `yes` for **B_SlotRole_Cached ⭐ as universal recommended default** + **E_Hierarchical_2Tier ⭐ as cost-sensitive fallback**
**Date closed:** 2026-06-22 (single session, ~2h)

---

## Phase tracker

- Phase 0 (reservation per §13.1): DONE — `research/backlog.md §In progress` + folder `experiments/2026-06-22-squad-fire-team-command/`.
  - Sentinel §13.7 clean (parallel agents on stealth-signature-reduction + urban-combat-tactics-ai + fire-coordination-multiple-units + missile-guidance-laws-simulation verified before claim).
- Phase 1 (web-research): DONE — 8 primary Wikipedia sources verified in `sources.md` (Fireteam / Squad leader / Bounding overwatch / Close-quarters battle / Behavior tree / F.E.A.R. / Squad video game / Arma 3).
- Phase 2 (prototype): DONE — `prototype/squad_fire_team_bench.cpp` ~480 LoC (Clang 22.1.6, build green **0 warnings**).
- Phase 3 (bench): DONE — 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time < 0.1 sec.
- Phase 4 (write-up): DONE — README + RESULTS + sources + STATUS.
- Phase 5 (single-pass sync per §13.5): TODO — INDEX.md §5 Active → §6 Recent closed + backlog.md §In progress → §Closed.

---

## Headline numbers

| Strategy | mean ns/tick (25 configs) | ratio vs A | per-squad @ 3×9 | % of 30 Hz budget |
|----------|---------------------------|------------|-----------------|-------------------|
| **A: Naive_NoMemory** | 5274.0 | 1.0× baseline | 7650 | 0.158% |
| **B: SlotRole_Cached** ⭐ | **343.6** | **15.3×** | **498** | 0.010% |
| **C: BT_Sequence_Chained** | 462.1 | 11.4× | 678 | 0.014% |
| **D: Blackboard_Shared** | 655.0 | 8.0× | 975 | 0.020% |
| **E: Hierarchical_2Tier** ⭐ | 430.7 | 12.2× | 617 | 0.013% |

**B_SlotRole_Cached = UNIVERSAL RECOMMENDED DEFAULT** (wins all 5 scenes, 15.3× mean speedup, simplest code).
**E_Hierarchical_2Tier = cost-sensitive fallback** (12.2× speedup, architecturally cleanest).
**D_Blackboard_Shared = REJECTED for sustained_combat** (O(N²) at 12+ enemies).
**A_Naive_NoMemory = REJECTED as production default** (1.5-3× slower than non-baselines).

---

## Cross-axis

- **Orth** to all closed Tier 2 AI per-unit (BT / cover / suppression / flow / AOI) + Tier 1 Physics + Tier 1 Netcode.
- **Complementary** to:
  - `hierarchical-tactical-ai-btree` [mixed, Tier 2] — per-unit BT = tactical layer; squad orchestrates
  - `cover-system-terrain-adaptive` [mixed, Tier 2] — cover score input
  - `suppression-mechanics` [mixed, Tier 2] — suppression state input
  - `group-formation-maneuver-axis` [closed mixed, Tier 2] — formation positioning (slot is orth)
  - `flanking-maneuver-ai` [closed mixed, Tier 2] — flank route (per-squad target)
  - `combined-arms-coordination-ai` [closed mixed, Tier 2] — cross-arm coordinator (squad = arm atomic unit)
  - `recon-intel-fog-of-war` [closed yes, Tier 2] — intel visibility input
  - `ballistic-projectile-simulation` [closed yes, Tier 1] — weapon spec data
  - `infantry-soldier-sim` [closed yes, Tier 1] — per-soldier physical sim
  - `lockstep-state-sync-hybrid-netcode` [closed mixed, Tier 1] — squad state = lockstep node
  - `urban-combat-tactics-ai` [in-progress, Tier 2] — interior graph for CLEAR_ROOM order
  - `fire-coordination-multiple-units` [in-progress, Tier 2] — focus fire consumer
  - `stealth-signature-reduction` [in-progress, Tier 2] — passive EW sibling

---

## Outputs

- `prototype/squad_fire_team_bench.cpp` (~480 LoC, Clang 22.1.6 build green 0 warnings)
- `prototype/build/squad_fire_team_bench` (35 KB binary)
- `prototype/build/results.csv` (126 rows = 1 header + 125 data)
- `prototype/build/summary_means.csv` (26 rows = 1 header + 25 data)
- `prototype/build/results.txt` (human-readable headline)
- `README.md` (8 sections: hypothesis, prior art, method, prototype, results, verdict, integration recommendation, sources)
- `RESULTS.md` (full per-strategy + per-scene analysis)
- `sources.md` (8 primary Wikipedia + 14 closed ProjectV cross-references)

---

## Sync (per §13.5)

- `backlog.md §In progress` → `§Closed` (with full closure note + reservation record removed)
- `INDEX.md §5 Active` → `§6 Recent closed` (table row + entry)
- This STATUS.md (closure note)
- `agent/workspace.md`: NOT in scope (this is for mainline agent per `docs/experiments/AGENTS.md §2`)

---

## Wall time summary

- Sentinel §13.7 + claim: < 1 min
- Web research (8 sources, 8 webfetches): ~ 5 min
- Prototype authoring + build: ~ 10 min
- Benchmark run: < 1 sec
- Write-up (README + RESULTS + sources + STATUS): ~ 15 min
- Total single session: ~ 35 min
