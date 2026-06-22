# STATUS — 2026-06-22-ambush-detection-reaction

**Phase:** closed (Phase 4 of 4 — wrap-up complete)
**Last action:** 2026-06-22 — Phase 4 wrap-up done (RESULTS.md + STATUS + README + sources + prototype + build all green; backlog + INDEX sync per §13.5).
**Next tick:** none — **closed `2026-06-22` (single session, ~2h, claim + web-research + prototype + bench + close).**
**Blocker:** нет.

---

## Progress log

- 2026-06-22 — opened. Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (sector activity + Bayesian surprise + BT priority interrupt = first dedicated AI ambush detection axis в 140+ closed experiments; orth ко всем 18 in-progress parallel на 2026-06-22).
- 2026-06-22 — Phase 0 scaffold: folder + README + STATUS + backlog.md claim + INDEX.md §5 Active entry done.
- 2026-06-22 — Phase 1 web-research: 4 Tier 1 Wikipedia + 3 Tier 2 cross-refs (Champandard 2012 + Isla 2005 + Colledanchise 2018) = 7 sources verified per `sources.md`. Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list.
- 2026-06-22 — Phase 2 prototype: `prototype/ambush_bench.cpp` ~430 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic -fconstexpr-steps=1000000000`, **build green 0 warnings**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **11.27 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (26 rows = 1 header + 25 data) + `prototype/build/run.log` (32 lines).
- 2026-06-22 — Phase 3 benchmark: completed. Headline per `RESULTS.md`:
  - **A_NoDetection** = 0% TPR, 100% casualties (baseline).
  - **B_SimpleThreshold** = 100% TPR, 100% FPR (noisy threshold).
  - **C_MovingAverageDeviation** = 100% TPR, 80% FPR (MA+3σ ловит шумовые spikes).
  - **D_BayesianSurprise ⭐** = 100% TPR, **0% FPR**, latency 1-2 ticks (realistic ramp).
  - **E_BayesianPlusBTPriorityInterrupt ⭐** = same as D + **10-18% casualties reduction** (reaction behavior).
- 2026-06-22 — Phase 4 wrap-up: `RESULTS.md` written + STATUS.md updated + README.md sections 5, 6, 7 finalized. Sync per §13.5 (backlog.md §Closed + INDEX.md §6 Recent closed) **DONE**.

---

## Notes

**Verdict: mixed per strategy; yes for E_BayesianPlusBTPriorityInterrupt ⭐ as universal recommended default + D_BayesianSurprise as detection-only alternative.**

**5-10% threshold per `optimization-philosophy.md`:** E vs A casualties = -15.2% (60 saved of 393) ✅; D vs B FPR = -100% (0% vs 100%) ✅; D vs A TPR = +∞ (0% → 100%) ✅. **Mainline 3-step migration ~520 LoC, M effort, deferred до Stage 6+ per `agent/workspace.md §2` line 36 operator 8x planning decision.**

**Cross-axis:** orth ко всем 18 in-progress parallel на 2026-06-22; complementary к closed `hierarchical-tactical-ai-btree` [mixed, BT reaction consumer] + `recon-intel-fog-of-war` [yes, sector activity] + `cover-system-terrain-adaptive` [mixed, take-cover reaction] + `flanking-maneuver-ai` [mixed, ambushers = inverse of flankers] + `combined-arms-coordination-ai` [mixed, ambush doctrine] + `suppression-mechanics` [mixed, ambush trigger] + `fire-coordination-multiple-units` [closed, focus fire on ambusher] + `indirect-fire-artillery-fdc` [closed, call-for-fire reaction] + `radar-detection-system-simulation` [yes, sensor activity] + `irst-thermal-imaging-detection` [closed, sensor activity] + `acoustic-detection-system` [closed, sensor activity] + `lockstep-state-sync-hybrid-netcode` [closed, surprise events as lockstep nodes] + `after-action-replay-system` [closed, surprise triggers as replay highlights] + `ecs-1m-entities-bottleneck` [yes, Flecs registry] + `data-driven-vehicle-weapon-definitions` [closed, enemy noise profile]; prerequisite для open `ambush-design-ai` [m Tier 2].

**New axis:** first dedicated AI ambush detection / Bayesian surprise / sector activity level axis в 140+ closed experiments; opens Stage 6+ Tier 2 AI for anti-ambush tactics.

См. [README.md](./README.md) + [RESULTS.md](./RESULTS.md) + [sources.md](./sources.md) + `prototype/{ambush_bench.cpp (~430 LoC), build/{ambush_bench (32 KB), results.csv (26 rows), run.log (32 lines)}}`.

