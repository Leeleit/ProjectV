# STATUS — 2026-06-22-ambush-detection-reaction

## Phase

**Phase 0 (init) — DONE 2026-06-22 03:01**
**Phase 1 (web-research) — DONE 2026-06-22 (Exa working this session)**
**Phase 2 (prototype + benchmark) — DONE 2026-06-22 03:03**
**Phase 3 (analysis + close) — DONE 2026-06-22**

**Status:** `concluded-verdict-mixed` per strategy; **`yes` for D_BayesianSurprise ⭐
as universal recommended default + E_BayesianPlusBTPriorityInterrupt ⭐ as reaction
opt-in + B_SimpleThreshold as cheap fallback**; `no` for A_NoDetection (baseline)
and C_MovingAverageDeviation (80% FP on baseline patrol).

## Final outputs

- ✅ [`README.md`](./README.md) — 8-section template complete
  (Hypothesis, Prior art, Method, Prototype, Results, Verdict, Integration recommendation, Sources)
- ✅ [`RESULTS.md`](./RESULTS.md) — headline summary + per-strategy
  analysis + 5-10% threshold validation + caveats + mapping to hot-path
- ✅ [`sources.md`](./sources.md) — 10 Tier 1 Wikipedia + 4 academic
  Tier 2 + 1 Tier 3 cross-references = 15 verified sources
- ✅ `prototype/ambush_bench.cpp` ~363 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`,
  **build green 0 warnings**)
- ✅ `prototype/build/{ambush_bench (36 KB), results.csv (1.8 KB,
  26 rows), run.log (26 lines)}` — 125,000 main measurements (5 strats ×
  5 scenes × 5 seeds × 1000 iter + 10 warmup)
- ✅ `research/backlog.md` sync: §Open → §Closed per §13.5
- ✅ `INDEX.md` §5 Active → §6 Recent closed per §13.5

## Sync checklist (per §13.5 + §6 §10 DoD)

- [x] All 8 sections of `README.md` filled in (Hypothesis, Prior art,
      Method, Prototype, Results, Verdict, Integration recommendation, Sources).
- [x] `STATUS.md` reflects closed state.
- [x] `INDEX.md` updated: §5 Active entry removed, §6 Recent closed entry
      added (see `INDEX.md` diff at close).
- [x] `research/backlog.md` updated: entry moved from `§Open` to `§Closed`
      with full reservation block.
- [x] Prototype reproducible (commands in `README.md §4 Prototype`).
- [x] Integration recommendation written for mainline agent (3-step
      migration per `agent/knowledge.md` precedent, ~500 LoC,
      M effort, deferred до Stage 6+ military sandbox activation).

## Headline

- **D_BayesianSurprise ⭐** = universal recommended default (100% detection,
  **0% FP**, 1-2 tick latency, 0.08-1.23% of 30 Hz budget).
- **E_BayesianPlusBTPriorityInterrupt ⭐** = D + instantaneous BT interrupt
  (same detection, **-15.3% casualties on first-contact ambush** = 60
  fewer per 25,000 runs).
- **A/B/C rejected** — A has no detection, B has 100% FP, C has 80% FP
  on the baseline patrol scene.

## Last action

Experiment closed `2026-06-22`. All sync (§13.5) complete.

## Next tick

_None — experiment closed._
