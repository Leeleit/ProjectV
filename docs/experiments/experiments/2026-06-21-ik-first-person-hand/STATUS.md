# STATUS — ik-first-person-hand

**Status:** `concluded-verdict-mixed`

## Log

| Date       | Event                                                                 |
|:-----------|:----------------------------------------------------------------------|
| 2026-06-21 | Experiment created, reservation set, web research complete            |
| 2026-06-21 | C++26 CPU prototype built (6 strategies × 5 scenes × 5 seeds × 1000 iter) |
| 2026-06-21 | Analytic solver bugs fixed (FK sign mismatch, FK-overwrite, timing)   |
| 2026-06-21 | Benchmark complete — FABRIK is best single strategy; hybrid wins      |
| 2026-06-21 | Verdict: yes (hybrid analytic+FABRIK), no (pure analytic/CCD)        |

## Verdict summary

| Strategy              | Time (µs) | Error (cm) | Converged | Recommendation |
|:----------------------|:----------|:-----------|:----------|:---------------|
| Analytic two-bone     | ~0.17     | 3.5–7.3    | rare      | fast failback  |
| CCD (unconstrained)   | ~3–4      | 4–15       | rare      | no             |
| FABRIK (unconstrained)| ~0.2–0.7  | <1         | ~99%      | main strategy  |
| FABRIK (constrained)  | ~0.3–1.2  | <1         | ~99%      | with limits    |
| CCD (constrained)     | ~9–12     | 4–16       | rare      | no             |

**Winner:** FABRIK (D) — <1 µs, <1 cm error. Add analytic two-bone as first-pass failback for out-of-reach targets.

## Blocker
- None. Ready for integration recommendation.
