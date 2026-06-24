# STATUS — 2026-06-21-countermeasure-dispenser

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-21 — Concluded experiment. Standalone C++26 benchmark completed and run. Verified E_SmartDecoy_ContinuousWithReserve as universal recommended default; B_ALE47 as fallback; C_Programmed REJECTED; D_DualMode niche opt-in.
**Next tick:** по запросу оператора
**Blocker:** нет

---

## Progress log

- 2026-06-21 — Opened experiment, claimed slug per AGENTS.md §13.1 + §13.7 sentinel
  clean (sibling experiments: cable-winch-towing, tracy-gpu-vs-manual, gpu-fluid-ca-atomic-strategy, factory-production-system — all orth axes).
- 2026-06-21 — Web-research complete via DuckDuckGo HTML endpoint (Exa HTTP 429
  persistent per the web_search fallback chain): 12+ primary
  sources verified. sources.md complete.
- 2026-06-21 — Prototype design complete: 5 strategies × 5 scenes × 5 seeds × 1000
  iter + 10 warmup = 125,000 main + 12,500 warmup. Parametric decoy model
  P(success) = P_base × angular × ECCM × timing (DCS-validated per r/hoggit Foka
  2022).
- 2026-06-21 — README.md complete (all 9 sections including §5 Results, §6 Verdict,
  §7 Integration recommendation).
- 2026-06-21 — Prototype build green 0 warnings (Clang 22.1.6 -O3 -march=native
  -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic), 1 fix iteration: 10 unused-parameter
  warnings → marked `[[maybe_unused]]`. 1 bug fix: const_cast on time_to_impact
  removed, scene now passed as non-const reference. 1 link error fix: main moved
  outside namespace.
- 2026-06-21 — Run completed: 125,000 main measurements in <2 sec on dev host obvium
  Zen 3 5800X governor=powersave. Wall time 0.45-0.73 µs/iter (5 strategies).
- 2026-06-21 — RESULTS.md complete with per-strategy + per-scene tables, headline
  findings, 5-10% threshold evaluation, hypothesis evaluation, comparison to closed
  experiments, caveats.
- 2026-06-21 — Verdict = mixed (yes for E/B/D, no for C, A restricted). README §6
  finalized.
- 2026-06-21 — Integration recommendation complete (3-step migration ~380 LoC, S
  effort, deferred to Stage 6+ military sandbox activation per `agent/workspace.md
  §2` line 36).

---

## Notes

- **Surprising finding:** at ECCM=0.7, brute force "dump everything" (A) is
  competitive with all smart strategies. The DCS F/A-18C pilot consensus "quantity >
  timing" is validated. Sub-hypothesis 1 ("pattern matters") is REJECTED.
- **Real win is inventory management** for sustained pressure: E wins +2% on
  sustained_patrol vs A, with 50% inventory savings. Sub-hypothesis 3 partially
  confirmed.
- **D's niche is MAWS-ambiguous mode** (low classification confidence), NOT universal
  default. Use as opt-in via `PROJECTV_CM_STRATEGY=DUALMODE`.
- **Closed experiment cross-references:** this experiment orthogonally complements
  closed `radar-detection-system-simulation` [yes] (chaff from sensor side vs
  dispenser side), `aircraft-damage-model` [yes] (pre-hit survival vs post-hit
  state), `fixed-wing-flight-model-simulation` [yes] (kinematic state input), and
  `ballistic-projectile-simulation` [yes] (missile threat input).
- **Open prerequisite:** for open sibling `electronic-warfare-jamming` [m Tier 2,
  closed-mixed planning] + `stealth-signature-reduction` [m Tier 2, closed-mixed
  planning] which are active vs passive EW axes.
- **No need to reduce ITER=1000** — wall time <2 sec on dev host even with full grid.
  CPU prototype is fast.
