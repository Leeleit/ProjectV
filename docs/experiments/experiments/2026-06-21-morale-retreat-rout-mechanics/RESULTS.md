# Results — `morale-retreat-rout-mechanics`

**Captured:** 2026-06-22 (host runs `prototype/build/morale_bench`, Zen 3 5800X, Clang 22.1.6, `-O3 -march=native`).
**Methodology:** see [`../../../benchmarks/methodology.md`](../../../benchmarks/methodology.md) §3 (5 warmup + adaptive 1-500 measurement runs, 5 seeds per config).
**Reproduce:** `cd prototype/build && ./morale_bench` (writes `results.csv`, ~30 s on this host).

---

## 1. Headline table (per strategy, mean across 5 seeds)

Wall-clock cost is the **mean** over 5 seeds; rout/retreat counts are summed across 5 seeds × scene size.

| Strategy            | s1 (20u, 60s)         | s2 (48u, 180s)        | s3 (64u, 300s)        | s4 (200u, 600s)       | s5 (1024u, 900s)        |
|---------------------|-----------------------|-----------------------|-----------------------|-----------------------|-------------------------|
| **A NaiveThresh.**  | 0.22 µs/tk, 11 ns/u  | 0.55 µs/tk, 11 ns/u  | 0.75 µs/tk, 12 ns/u  | 2.26 µs/tk, 11 ns/u  | 17.0 µs/tk, 17 ns/u     |
| **B LinearAccum.**  | 0.18 µs/tk, 9 ns/u   | 0.47 µs/tk, 10 ns/u  | 0.71 µs/tk, 11 ns/u  | 2.06 µs/tk, 10 ns/u  | 11.3 µs/tk, 11 ns/u     |
| **C CombatFatigue** | 0.22 µs/tk, 11 ns/u  | 0.55 µs/tk, 12 ns/u  | 0.77 µs/tk, 12 ns/u  | 2.38 µs/tk, 12 ns/u  | 22.3 µs/tk, 22 ns/u     |
| **D TieredCohesn.** | 0.21 µs/tk, 11 ns/u  | 0.52 µs/tk, 11 ns/u  | 0.74 µs/tk, 12 ns/u  | 2.25 µs/tk, 11 ns/u  | 21.3 µs/tk, 21 ns/u     |
| **E AdaptiveFlow**  | 0.18 µs/tk, 9 ns/u   | 0.48 µs/tk, 10 ns/u  | 0.64 µs/tk, 10 ns/u  | 2.01 µs/tk, 10 ns/u  | 10.8 µs/tk, 11 ns/u     |

**Read:** the wall-time per tick grows linearly with N, as expected. The cheapest strategy is **B/E (linear-accumulator family)** at ~10 ns/unit/tick. The most expensive is **C/D** at ~20 ns/unit/tick for the largest scene, because they carry history state and call `std::clamp` and `std::min/max` chains that defeat some auto-vectorization.

**Per-tick cost for 1024 units:**
- s5 with strategy E: 10.8 µs/tick → at 30 Hz tick this is 0.03% of a 33 ms frame.
- Strategy C/D at 22 µs/tick → still 0.07% of frame.
- All strategies are well below the 300 ns/unit × 1024 = 307 µs/tick budget from the README.

## 2. Behavioral outcome (rout/retreat rates)

| Strategy            | s1 routed / 20u | s2 routed / 48u | s3 routed / 64u | s4 routed / 200u | s5 routed / 1024u |
|---------------------|-----------------|-----------------|-----------------|------------------|-------------------|
| **A NaiveThresh.**  | 0               | 1-4 (2.6 avg)   | 39-44 (40.4)    | 0-2 (1.0)        | **992-995 (993)** |
| **B LinearAccum.**  | 18-19 (18.6)    | **48 (48.0)**   | **64 (64.0)**   | **200 (200.0)**  | **1024 (1024)**   |
| **C CombatFatigue** | 0               | 0               | 0               | 2-3 (2.6)        | **1024 (1024)**   |
| **D TieredCohesn.** | 0               | 0               | 0               | 0                | **0-1 (0.2)**     |
| **E AdaptiveFlow**  | 14-17 (16.0)    | **48 (48.0)**   | **64 (64.0)**   | **200 (200.0)**  | **1024 (1024)**   |

**Retreat rate (5+ nearby casualties + suppression>50):** zero across all strategies. See §5 for analysis.

**Mean final morale (s1–s5, last tick):**

| Strategy            | s1   | s2   | s3   | s4   | s5   |
|---------------------|------|------|------|------|------|
| A                   | 88.3 | 57.9 | 24.5 | 69.4 | 2.3  |
| B                   | 6.95 | 0.0  | 0.0  | 0.0  | 0.0  |
| C                   | 94.2 | 67.3 | 53.5 | 29.6 | 0.0  |
| D                   | 90.6 | 73.2 | 72.8 | 80.1 | 70.2 |
| E                   | 20.0 | 0.0  | 0.0  | 0.0  | 0.0  |

## 3. Findings

### 3.1 Strategy D (Tiered Cohesion Index) is the clear winner on behavioral stability

Across the 5 hardest scenes (s3–s5, 64–1024 units, 300–900 s), only **D** keeps rout rate near zero:
- s3: 0/64 (others: A=40, B=64, C=0, E=64)
- s4: 0/200 (others: A=1, B=200, C=2.6, E=200)
- s5: 0.2/1024 (others: A=993, B=1024, C=1024, E=1024)

The "D" tier cascade — Panicked → Routed only when leader is dead OR already Panicked — provides
the **hysteresis** that prevents cascade collapse. Other strategies either have no history
(A, brittle) or accumulate stress monotonically (B, E over-stress; C is calibrated wrong for s5).

### 3.2 Strategy C (Marshall 1947 + Appel 200-240 day) is calibrated for medium combat, breaks at long/high-intensity

C is the **most academically faithful** to the cited sources (Marshall's 25% cohesion factor, Appel's
200-240 day limit) but it has a known weakness in the prototype: the duration_ratio scaling means that
beyond ~50% of the 18 000-tick reference window, cohesion starts collapsing at 30 points per 0.1 of
ratio. For s5 (27 000 ticks), `duration_ratio` reaches 1.5 by tick 27 000, so cohesion drops by 30
points just from time alone. This is a calibration bug: Appel's 200-day limit was 200 *days*, our
scaling is per-tick not per-day, and the constant 18000 was a placeholder. **Fixable**: scale by
real elapsed wall-time, not ticks.

### 3.3 Strategy B and E are indistinguishable in behavioral outcome, E is slightly faster

B (linear accumulator) and E (B + D + C combined) both over-accumulate stress for sustained combat
because they share the same accumulation math (E uses gentler weights but the decay rate is the same).
The D-style tier cascade in E only fires at morale<5 and <25, which means once the accumulator pushes
morale below 25, the cascade does engage — but for s1 with 5+ minutes of skirmish, B/E rout 14-19/20
units while D keeps all 20. The "best of breed" claim for E in the README was wrong.

### 3.4 Retreat rate is zero across the board

The retreat threshold (`5+ nearby casualties + suppression>50`) requires **simultaneous** high
casualties and high suppression. In practice these rarely co-occur in the synthetic scenes:
- Casualty rate is 0.001-0.015 per unit per tick → expected 0.06-0.96 casualties per unit over
  60 s. With ~5 nearby units per individual, an individual sees a "5+ casualties" event only when
  100% of its neighbors fall in the same tick — vanishingly rare.
- Suppression reaches 50 only after sustained exposure (~6+ suppression events on a unit).

**Retreats need a different trigger model** — likely cumulative casualties in a wider area, not
"5+ buddies die in this exact tick". This is a known design issue flagged in the README.

### 3.5 Per-unit cost is well within budget for all strategies

Target was 300 ns/unit/tick (`AGENTS.md §4` DoD) = 307 µs/tick for 1024 units. Observed:
- s5 fastest (E): 10.8 µs/tick total = 10.5 ns/unit/tick
- s5 slowest (C): 22.7 µs/tick total = 22.2 ns/unit/tick

**All strategies are 13-28× under the per-unit cost budget.** The DoD target is met trivially even
for the most expensive strategy on the largest scene.

## 4. Performance scaling

Linear fit of `us/tick` vs `units` (excluding s1 outlier at 20u):

| Strategy    | slope (ns/u) | intercept (µs) | R² (eyeball) |
|-------------|--------------|----------------|--------------|
| A           | ~15          | ~0.0           | linear       |
| B           | ~10          | ~0.0           | linear       |
| C           | ~20          | ~0.0           | linear       |
| D           | ~20          | ~0.0           | linear       |
| E           | ~10          | ~0.0           | linear       |

All strategies scale **linearly** with unit count, confirming the O(N×degree) per-tick cost. The
adjacency precomputation (positions static) eliminates the O(N²) cost that would otherwise dominate
at N>512.

## 5. Limitations

1. **Static positions.** This benchmark fixes unit positions at init. Real game has moving units,
   which would invalidate the precomputed adjacency. Production code needs an incremental spatial
   index (uniform grid or BVH) with the same O(N×degree) cost on average. The current prototype
   is a **best-case** performance measurement.
2. **Synthetic event rates.** Suppression, casualty, and isolation rates are hand-tuned constants,
   not derived from gameplay data. Real game has bursts (suppressive fire vs. flanking), not
   uniform random.
3. **Single global RNG stream.** All unit state is mutated by one `std::mt19937`. A production
   implementation should use one RNG per scene-thread or per spatial region for determinism
   on multi-core.
4. **No leader-follower graph.** Officer/NCO role is per-unit modifier only; the actual
   leadership-loss cascade in HoI4 / Warno propagates through a chain-of-command. Our `leader_alive`
   is a global flag, not per-platoon.
5. **Retreat threshold is untested in practice** (zero triggers). Needs a redesign per §3.4.
6. **Calibration of C is off** for long scenes (s5). Per §3.2.

## 6. Verdict

**`concluded-verdict-yes` (with reservations).** The benchmark works, produces reliable numbers,
all strategies meet performance budget by 13-28×, and the behavioral comparison is striking:
**strategy D is empirically the most stable, with no routs in any scene except s5 seed 4 (1/1024)**.

Mainline can safely adopt D as the **default per-unit morale update** when morale is implemented
in `Walk` (Stage 2.x per `TODO.md`). The performance cost is non-trivial (20 ns/u/tick vs 10
for B/E) but absolutely within budget, and the behavioral stability gain is the difference
between "platoons rout under realistic stress" and "platoons hold".

## 7. Integration recommendation

- **Adopt strategy D (Tiered Cohesion Index) as the default** for per-unit morale in Walk.
- **Do NOT adopt B/E** (linear accumulator family) as the only model — they over-stress
  in sustained combat, which would cascade into "everyone routs after 60 s" UX.
- **Adopt C's calibration philosophy (duration-pressure) but fix the per-day scaling.** Appel's
  200-240 day limit should be applied in real wall-time, not tick counts.
- **Redesign the retreat trigger.** Current 5+ buddies die in one tick is too tight. Use
  cumulative casualties in a 30 s window + suppression>50 (a la CoH3).
- **Performance budget is met 13-28×.** No need to optimize further at this stage.
- **Replace the adjacency precomputation** with an incremental uniform-grid spatial index when
  unit positions become dynamic in `Walk`.

## 8. Sources (cross-ref)

- [`sources.md`](./sources.md) — 13 verified web sources for morale/retreat/rout theory.
- [`../../../benchmarks/methodology.md`](../../../benchmarks/methodology.md) — measurement protocol.

## 9. Raw data

`prototype/build/results.csv` — 126 lines (1 header + 125 data rows, 5 strategies × 5 scenes × 5 seeds).
`prototype/build/run.log` — full annotated run log with per-config progress.

## 10. Reproduce

```bash
cd /home/le1t/ProjectV/docs/experiments/experiments/2026-06-21-morale-retreat-rout-mechanics/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
  morale_bench.cpp -o build/morale_bench
cd build
./morale_bench   # ~30 s on Zen 3 5800X, governor=powersave
# → results.csv (125 rows), run.log (annotated progress)
```
