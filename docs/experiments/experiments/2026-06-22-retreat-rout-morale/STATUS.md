# STATUS — 2026-06-22-retreat-rout-morale

## Current phase

**Phase 5: CLOSED — `concluded-verdict-mixed`** (per-strategy mixed; `yes` for **D_StackedBreakpoint ⭐ as universal recommended default**).

## Phases

| Phase | Description | Status |
|:------|:------------|:-------|
| 0 | Folder + README + STATUS + backlog reservation + INDEX sync | ✅ done |
| 1 | Web research (sources.md: 8 verified sources) | ✅ done |
| 2 | Prototype (C++26 harness + 5 strategies) | ✅ done |
| 3 | Build & benchmark (625k measurements) | ✅ done |
| 4 | Analysis & verdict (RESULTS.md) | ✅ done |
| 5 | Close (STATUS, INDEX §6, backlog sync, results) | ✅ done |

## Blocker

Нет.

## Chronology

- 2026-06-22 — Opened, Phase 0 complete (folder + README + STATUS + reservation). Sentinel §13.7 clean.
- 2026-06-22 — Phase 1 (sources.md) complete: 8 sources verified (WARNO + Total War + HoI4 + ARMA 3 + Foxhole + Wikipedia "Morale" + "Rout" + "Combat stress reaction").
- 2026-06-22 — Phase 2 (prototype) complete: `morale_bench.cpp` 270 LoC implementing 5 strategies (A_NaiveLinear, B_SigmoidThreshold, C_AccumulatorDecay, D_StackedBreakpoint, E_Hybrid), 5 scenarios (s1_steady_patrol, s2_under_fire, s3_heavy_casualties, s4_isolated_squad, s5_mixed_combined_arms), 5×5 unit_counts×casualty_rates.
- 2026-06-22 — Phase 3 (build+bench) complete: Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG` build green 0 errors. 5×5×5×5×5×1000 + 10 warmup = 625,000 main measurements, wall time 1.45 sec.
- 2026-06-22 — Phase 4 (results) complete. **Verdict: `concluded-verdict-mixed`.** Headline: D_StackedBreakpoint ⭐ wins (191.2 ns mean = 0.96 ns/unit, state machine + explicit recovery). B_Sigmoid + E_Hybrid rejected (sigmoid cost 3.7× without benefit).
- 2026-06-22 — Phase 5 (close) complete. См. RESULTS.md + sources.md + prototype/. Moved to §Closed.

## Cross-refs

- See [README.md](./README.md) for hypothesis, method, prototype structure.
- See [sources.md](./sources.md) for web-research citations (8 verified sources).