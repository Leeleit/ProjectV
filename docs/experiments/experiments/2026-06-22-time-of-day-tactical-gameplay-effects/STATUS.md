# STATUS — time-of-day-tactical-gameplay-effects

**Phase:** wrap-up
**Last action:** 2026-06-22 — Phase 0-4 complete: claim → web-research → prototype → benchmark → analysis → docs
**Next tick:** (none)
**Blocker:** нет

---

## Progress log

- 2026-06-22 — opened. Claim per `AGENTS.md §13.1`. Sentinel §13.7 clean.
- 2026-06-22 — web research: 4 Wikipedia canonical fetches (Circadian / Night vision / Background noise / Equal-loudness) + 7 closed experiment cross-refs verified.
- 2026-06-22 — C++26 prototype built: `tod_tactical_bench.cpp` ~440 LoC (Clang 22.1.6 `-O3 -march=native`, build green 0 warnings).
- 2026-06-22 — first benchmark run: detected hour-asymmetric cost bug (loop-invariants recomputed per-soldier).
- 2026-06-22 — fixed: hoisted `fatigue_curve()`, `ai_accuracy_mult()`, `ai_cohesion_mult()`, `sound_propagation_mult()` outside per-entity loops (mainline quality).
- 2026-06-22 — final benchmark: 125,000 main measurements, wall time 14.6 sec на Zen 3 5800X.
- 2026-06-22 — conclusions written, verdict = `mixed per strategy / yes for C ⭐ as universal default`.

---

## Key results

| Strategy                  | Mean (ns/tick) | vs A | Hypothesis Status |
|:--------------------------|---------------:|-----:|:------------------|
| A_NoTimeEffects           |          22.7 |  1.0× | baseline |
| B_VisibilityOnly          |         612.6 | 27.0× | detection only |
| **C_VisibilityPlusAI ⭐** |         878.2 | 38.7× | H2+H4 confirmed, RECOMMENDED |
| D_VisibilityPlusAISound   |         911.1 | 40.1× | adds H3 (partial) |
| E_FullCircadian           |         951.5 | 41.9× | adds civilian + warmup |

**3-clause hypothesis validation:**
- ✅ H1 cost <10 µs/tick: CONFIRMED MASSIVELY (worst 951 ns = 10× under)
- ✅ H2 ≥2× detection range spread: CONFIRMED (4.71× actual, 2.35× over)
- ⚠️ H3 ≥1.5× sound amplification: PARTIAL (1.35× actual, below threshold)
- ✅ H4 ≥15% AI accuracy degradation at 0200-0500: CONFIRMED (32.7% actual, 2.2× over)

**Verdict:** `concluded-verdict-mixed` per strategy; `yes` for C ⭐ as universal recommended default for Stage 6+ military sandbox, with optional upgrades to D (audio games) or E (civilian simulation games).

**Integration:** 3-step migration ~120-180 LoC, S-M effort, 1-2 sessions, defaults `PROJECTV_TOD_EFFECTS=AI`. See README §7 for details.