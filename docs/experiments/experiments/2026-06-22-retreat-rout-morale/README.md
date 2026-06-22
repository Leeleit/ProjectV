# 2026-06-22-retreat-rout-morale — Unit Morale System: Tactical Retreat and Rout Mechanics

**Status:** in-progress
**Date opened:** 2026-06-22
**Date closed:** N/A
**Stage link:** independent (Stage 6+ military sandbox — Tier 2 AI: Tactical & Warfare)
**Estimated effort:** M
**Author:** self (per operator instruction «выбирай свободную тему или придумывай свою исследуй»)

---

## 1. Hypothesis

Unit morale in military sandbox combat is driven by: (1) casualties in unit / nearby units, (2) suppression from incoming fire (closed `suppression-mechanics` mixed), (3) isolation (no friendly units within squad radius), (4) leader alive/killed, (5) terrain cover.

We hypothesize:
- **H1 cost:** morale update per unit <0.3 µs/tick (10000+ units at 30 Hz = 0.9% of frame budget).
- **H2 dynamics:** at morale <20 → tactical retreat (ordered, maintains formation); at <5 → rout (disordered flee, abandon equipment). Critical threshold transitions validated against WARNO/Total War reference behavior.
- **H3 stability:** cascade rout prevention — single unit rout doesn't propagate to all nearby units (local influence only).
- **H4 recovery:** morale can recover when threats removed (casualties stop, suppression clears) but slowly (e.g., +1 morale/sec vs -10 morale/sec on casualty).

5 strategies compared:
- A_NaiveLinearDecay: `morale -= delta_casualties * w_casualties + suppression * w_suppression` per tick.
- B_SigmoidThreshold: `morale = sigmoid(sum(weighted_inputs))` smooth response curve.
- C_AccumulatorDecay: separate per-cause accumulators (casualty_acc, suppression_acc, isolation_acc) with exponential decay.
- D_StackedBreakpoint: state machine with breakpoints (Steady → Shaken → Breaking → Rout) per HoI4 organization system.
- E_Hybrid_SigmoidWithStateMachine: sigmoid for continuous value + state machine for behavior transitions.

---

## 2. Prior art

Web-research (planned sources.md):
- Wikipedia "Military morale" — overview of psychological factors.
- Wikipedia "Rout (military)" — definition, historical examples.
- WARNO devblog — Eugen Systems morale + suppression + retreat mechanics.
- Total War (Sega) — rout behavior, unit morale cascade.
- Hearts of Iron IV (Paradox) — organization system, breakpoint calculations.
- ARMA 3 AI behavior — morale, surrender, retreat triggers.
- Foxhole "Soldier Stamina" — stamina affecting combat effectiveness.

---

## 3. Method

- **Type:** standalone C++26 CPU prototype + benchmark.
- **Scene:** 5 scenarios × 5 unit counts × 5 casualty rates × 5 seeds:
  - s1_steady_patrol_64u: baseline steady state, low casualties.
  - s2_under_fire_64u: high suppression, no casualties.
  - s3_heavy_casualties_64u: many casualties, moderate suppression.
  - s4_isolated_squad_32u: surrounded, no nearby friendlies.
  - s5_mixed_combined_arms_128u: combined arms, varied morale drivers.
- **Metrics:** morale update time (ns/unit/tick), state transition accuracy (vs reference behavior), cascade prevention (% units reaching Rout), recovery rate, memory footprint (bytes).
- **Control:** A_NaiveLinearDecay baseline.
- **Protocol:** 5 strategies × 5 scenarios × 5 unit_counts × 5 casualty_rates × 5 seeds × 1000 iter + 10 warmup = 625,000 main measurements.

---

## 4. Prototype

`prototype/morale_bench.cpp` — standalone C++26, Clang 22.1.6, `-O3 -march=native -std=c++26 -DNDEBUG`.

```bash
cd prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic morale_bench.cpp -o build/morale_bench
./build/morale_bench
```

Output: `build/results.csv` (3126 rows) + `build/summary_means.csv` (6 rows).

---

## 5. Results

_To be filled after benchmark._

---

## 6. Verdict

_To be filled after analysis._

---

## 7. Integration recommendation

_To be filled after analysis._

---

## 8. Sources

_To be filled — see §2 list, will move to `sources.md` if extensive._

---

## 9. Mapping to ProjectV hot-path

- **Hot-path:** Stage 6+ military sandbox — Tier 2 AI: Tactical & Warfare. Morale update occurs per unit per tick in Flecs system.
- **Prototype maps to:** `src/ai/MoraleSystem.{hpp,cpp}` — function `updateMorale(unit, casualtyCount, suppression, isolation, dt)` returning morale state.
- **Assumptions:** Per-unit morale as Flecs component; shared suppressors from closed `suppression-mechanics` mixed; nearby unit query via Flecs spatial query.
- **Unmeasured:** GPU compute path (orth to closed `dec-pipelines-async-compute`); AI BT integration for retreat/rout action nodes per closed `hierarchical-tactical-ai-btree` mixed; multiplayer synchronization per closed `lockstep-state-sync-hybrid-netcode` mixed.

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) — Zen 3 5800X (per §1).