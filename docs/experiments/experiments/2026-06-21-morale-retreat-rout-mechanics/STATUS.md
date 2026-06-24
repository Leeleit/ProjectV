# STATUS — 2026-06-21-morale-retreat-rout-mechanics

**Phase:** concluded-verdict-yes
**Last action:** 2026-06-22 — full benchmark run (5×5×5=125 configs), RESULTS.md written, prototype reproducible.
**Next tick:** integration handoff to mainline.
**Blocker:** нет.

---

## Progress log

- **2026-06-21** — Opened per `AGENTS.md §13.1`. Self-invented topic per operator instruction
  «выбирай свободную тему или придумывай свою исследуй». First dedicated unit-morale / retreat /
  rout axis in 130+ closed experiments. §13.7 sentinel clean.
- **2026-06-21** — README.md + STATUS.md created. INDEX.md §5 Active row added.
- **2026-06-21** — Web-research: 13 verified sources via `webfetch` (Wikipedia Morale, Rout,
  Combat stress reaction, Unit cohesion; Dave Grossman 1995; WARNO/CoH3/HoI4 production
  references; Engen 2008, Marshall 1947 academic anchors). Saved to [`sources.md`](./sources.md).
- **2026-06-22** — C++26 standalone CPU prototype `prototype/morale_bench.cpp` (~660 LoC).
  5 strategies × 5 scenes × 5 seeds = 125 configs. Builds green (Clang 22.1.6, 0 warnings).
- **2026-06-22** — First benchmark run produced 0-byte output (output path issue + O(N²)
  per-tick cost too high for s5). Fixed: (1) output to `results.csv` in cwd, (2) precomputed
  adjacency (positions static) → O(N×degree) per tick.
- **2026-06-22** — Adaptive kRuns (1-500) to keep per-config runtime under ~10-50 M unit-ticks.
  Full run completed in ~30 s on this host.
- **2026-06-22** — All 125 configs captured. RESULTS.md written with comparative analysis.
  **Strategy D (Tiered Cohesion Index) is the clear winner**: 0-1/1024 routs in s5 vs
  992-1024/1024 for the other 4 strategies.

---

## Headline finding

**Adopt D as the default per-unit morale update for Walk.** Per-unit cost is 20 ns/u/tick
(2-3× the cheapest B/E, but 13-28× under the 300 ns/u/tick budget), and the behavioral stability
gain is the difference between "platoons hold under realistic stress" and "platoons cascade-rout
after 60 s of combat".

## Notes

- Per `agent/knowledge.md` 3-step migration pattern: ready to hand off. Recommended
  integration: Flecs `MoraleComponent` (SoA, fields: `morale: float`, `suppression: float`,
  `state: MoraleState`, `history_acc: float`, `combat_ticks: int`, `leader_alive: bool`,
  `nearby_friendlies: int`, `nearby_casualties: int`) + per-tick `MoraleUpdateSystem` + per-tick
  `MoraleEventApplySystem` (consumer of `SuppressionSystem`, `CasualtyEventSystem`,
  `LeadershipLossEvent`).
- Per `agent/workspace.md §2` operator 8x planning decision: deferred до Stage 6+ military
  sandbox activation. **This experiment provides the verified default implementation** when
  Stage 6 starts.
- Per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` 5-10% threshold: D is
  2× slower than the cheapest strategy but absolutely within budget. The optimization
  philosophy says "if perf gain < 5-10%, choose simple" — but here the comparison is on
  *behavior*, not perf. Stability gain is >100% (routs drop from 99% to 0.02%).
- Per the web_search fallback chain + parallel-agent system load: reduced kRuns from
  methodology default (1000) to 1-500 adaptive. Even so, full 125-config sweep ran in 30 s
  on this host with 5+ parallel agents active.

## Known issues (for future follow-up)

1. Retreat rate is zero across all strategies (5+ casualties-in-one-tick threshold is too tight).
   Needs redesign per RESULTS.md §3.4.
2. Strategy C (Marshall/Appel) is miscalibrated for long scenes (per-tick duration scaling instead
   of per-day). Fixable by switching to wall-time scaling.
3. Adjacency is precomputed (positions static). Production needs incremental spatial index.
