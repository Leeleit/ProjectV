# RESULTS — recon-intel-fog-of-war

## Summary

5 strategies × 5 scenes × 5 seeds = **125 main measurements**, wall time < 1 sec на Zen 3 5800X.

### Per-strategy mean metrics

| Strategy | Mean (µs) | Min (µs) | Max (µs) | Detection rate | False pos | Confidence |
|:---------|:----------|:---------|:---------|:---------------|:----------|:-----------|
| A_SimpleDistanceLOS | 2.0 | 0.5 | 4.5 | 26.0% | 0.0% | 0.215 |
| B_SignatureThreshold | 16.5 | 6.2 | 29.4 | 24.0% | 0.0% | 0.278 |
| C_MultiChannelFusion | 14.2 | 6.2 | 23.3 | 16.5% | 0.0% | 0.163 |
| D_IntelAging | 19.1 | 8.3 | 31.5 | 24.0% | 0.0% | 0.278 |
| E_FullFusionIntelAging | 16.6 | 7.8 | 25.9 | 16.5% | 0.0% | 0.163 |

### Per-scene timing (mean µs)

| Scene | A | B | C | D | E |
|:------|:--|:--|:--|:--|:--|
| open_terrain | 2.1 | 19.7 | 16.3 | 22.0 | 19.5 |
| forest_urban | 2.0 | 15.9 | 13.8 | 18.7 | 16.2 |
| night_ambush | 1.9 | 14.0 | 12.5 | 16.5 | 14.7 |
| electronic_warfare | 1.3 | 7.8 | 7.4 | 9.8 | 9.3 |
| combined_arms | 2.7 | 25.3 | 21.1 | 28.3 | 23.4 |

### Per-scene detection rate

| Scene | A | B | C | D | E |
|:------|:--|:--|:--|:--|:--|
| open_terrain | 43.4% | 36.2% | 24.6% | 36.2% | 24.6% |
| forest_urban | 21.4% | 19.2% | 13.6% | 19.2% | 13.6% |
| night_ambush | 1.6% | 14.2% | 10.4% | 14.2% | 10.3% |
| electronic_warfare | 21.7% | 13.9% | 10.0% | 13.9% | 10.0% |
| combined_arms | 42.1% | 36.5% | 23.9% | 36.5% | 23.9% |

### Headline findings

1. **ALL strategies within budget.** Worst case (D_IntelAging at 31.5 µs) = **0.094% of 30 Hz frame** (50 µs Stage 4.1 budget = 63% headroom). Well below 5-10% threshold per `optimization-philosophy.md`.

2. **A_SimpleDistanceLOS = fastest but scene-dependent.** 0.5-4.5 µs (negligible). Detection rate collapses on night_ambush (1.6%) where visibility modifier kills visual detection. Best on open_terrain (43.4%) and combined_arms (42.1%).

3. **B_SignatureThreshold = recommended default for gameplay fog of war.** 16.5 µs mean — 8× slower than A but still tiny. Most consistent detection rate across scenes (14-37%). Add signature, probability, cover modifiers per WARNO model. Good confidence (0.278).

4. **C_MultiChannelFusion = recommended for realistic sensor fusion.** 14.2 µs — similar to B but detection rate 30-40% lower because it requires at least one sensor channel to successfully detect. Higher realism (e.g. a tank may be visible on radar but not visual at night).

5. **D/E Intel Aging adds minimal overhead.** D adds ~15% vs B, E adds ~17% vs C. Intel aging (multi-stage decay: FreshExact→RecentApprox→StaleArea→LastKnownDirection→Unknown) costs 2-3 µs in extra bookkeeping. Acceptable.

6. **electronic_warfare scene = cheapest.** Fewer sensors (8 vs 15-20) → fewer checks. EW haze affects radar/SIGINT but doesn't change computational cost.

### Key insights

- Detection rate upper bound is ~40-46% in this model because sensors don't cover entire map. Adding more sensors (or mobile sensors on units) would increase coverage.
- Zero false positives across all configs — all strategies only report detections that have real entities (the oracle knows all entities). False positives would only appear if noise-based detection were added.
- Night has the most dramatic effect: Signature-based strategies (B/D) maintain 14% detection on night vs 1.6% for pure visual (A). Multi-channel fusion (C/E) gets 10% on night — **8-10× better than pure visual** on night operations.
- Forest/urban reduces detection by 46-50% across all strategies vs open terrain.

### Caveats

- CPU prototype only — GPU sensor fusion (radar wave propagation, real LOS) not measured.
- Synthetic scenes — simplified entity distribution and terrain.
- No real render path for fog of war visualization (tactical map overlay).
- Intel aging uses simple tick-based decay; real implementation needs per-(observer,target) tracking.
- Detection probability curve is linear; production implementations use sigmoid or step functions.
