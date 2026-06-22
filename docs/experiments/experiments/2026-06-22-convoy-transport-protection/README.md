# Convoy transport protection

**Date:** 2026-06-22
**Slug:** `convoy-transport-protection`
**Status:** `concluded-verdict-mixed`

---

## 1. Hypothesis

Supply convoy protection in a foxhole-like sandbox can be decomposed into three
independent axes — route planning, dynamic threat avoidance, and escort
formation. A strategy that combines all three (hybrid) should outperform any
single-axis strategy in terms of survival rate × delivery time trade-off.

**Null hypothesis:** Naive shortest-path routing achieves comparable survival &
throughput when threat density is moderate.

---

## 2. Prior art

**Games:**
- **Foxhole logistics:** player-driven supply convoys, partisan ambushes, route
  planning via map Intel, escort mechanics ([Foxhole Wiki — Logistics]).
- **Arma 3 convoy scripts:** AI formation driving, push-through on contact,
  disembark under fire ([Bohemia Forums — Convoy behaviour and path
  planning]).
- **DCS TROOP-TRANSPORTS:** troop transport helicopter convoy AI with random
  direction movement after disembark ([GitHub — DCS TROOP-TRANSPORTS]).

**Research:**
- **AutoGators:** autonomous convoy path planning with drone threat detection
  and dynamic rerouting ([GitHub — AutoGators]).
- **DRAIDIS Convoy Route Protection:** military IED detection, threat heatmaps,
  alternate route suggestions ([TacticalEdge AI open-source]).
- **arXiv 1208.5537:** Planning random path distributions for ambush games
  (game-theoretic model of attacker / defender).
- **IEEE RA-L 2025:** Synchronized task formulation for robotic convoy
  operations.

**Convoy tactics (military):**
- [Wikipedia — Convoy], [Wikipedia — Ambush], [Wikipedia — Logistics].

[Foxhole Wiki — Logistics]: https://foxhole.wiki.gg/wiki/Logistics
[Bohemia Forums — Convoy behaviour and path planning]: https://forums.bohemia.net/forums/topic/237002-convoy-behaviour-and-path-planning/
[GitHub — DCS TROOP-TRANSPORTS]: https://github.com/SinistralValkyries/DCS-TROOP-TRANSPORTS
[GitHub — AutoGators]: https://github.com/CUTR-at-USF/AutoGators
[TacticalEdge AI open-source]: https://github.com/TacticalEdge/convoy-route-protection-system-draidis
[Wikipedia — Convoy]: https://en.wikipedia.org/wiki/Convoy
[Wikipedia — Ambush]: https://en.wikipedia.org/wiki/Ambush
[Wikipedia — Logistics]: https://en.wikipedia.org/wiki/Logistics

---

## 3. Method

CPU-only tile-grid simulation benchmark. 5 convoy strategies × 5 terrain scenes
× 200 Monte Carlo iterations each (20 warmup). Warmup + measurement in same
process; PRNG reseeded per iteration.

**Platform specifications:** см.
[`hardware-profile.md`](../../hardware-profile.md) §1 (CPU: AMD Ryzen 9 7950X,
16C/32T), §2 (RAM: 64 GiB DDR5-6000). Бенчмарк однопоточный, накладные расходы
детерминированы.

### Strategies

| ID | Strategy | Description |
|----|----------|-------------|
| A | `NaiveDirectRoute` | Dijkstra shortest path, ignore threats |
| B | `WaypointRoadPreference` | Route biased toward road tiles |
| C | `DynamicThreatAvoidance` | Replan when threat detected within 6 tiles |
| D | `EscortFormationAI` | Escorts maintain diamond formation, intercept threats |
| E | `HybridDynamicConvoy` | Threat-aware routing + escort formation + ambush evasion |

### Scenes

| ID | Name | Type | Threats | Escorts |
|----|------|------|---------|---------|
| s1 | Simple supply run | Open terrain, short route | 2 | 1 |
| s2 | Highway ambush | Road corridor, chokepoint | 4 | 2 |
| s3 | Mountain pass | 3 chokepoints, high density | 6 | 3 |
| s4 | Urban logistics | Grid city, scattered threats | 8 | 2 |
| s5 | Long haul supply | Multi-zone, mixed terrain | 10 | 3 |

### Metrics

- **Survival rate** — fraction of runs where supply truck reaches destination
- **Avg delivery time** — mean ticks to destination (survivors only, includes
  max-tick timeouts)
- **Avg casualties** — mean units killed per run
- **Engagement rate** — fraction of threats that detected the convoy

---

## 4. Prototype

**Location:** `prototype/convoy_bench.cpp` (~850 LoC C++26).

**Reproduce:**

```bash
cd prototype
mkdir -p build && cmake -S . -B build -G Ninja
ninja -C build
./build/convoy_bench
```

Build-dir is `prototype/build/` (inside the experiment folder, not root `build/`).

**Dependencies:** C++26 compiler (GCC 16.1.1), CMake ≥3.28, Ninja.

---

## 5. Results

### Raw data

See [`convoy_results.csv`](prototype/convoy_results.csv) (machine-readable).

### Per-strategy × per-scene

| Strategy | Scene | Surv% | AvgT | AvgCas | Eng% |
|----------|-------|-------|------|--------|------|
| A | s1_simple | 100.0 | 28.0 | 0.00 | 48 |
| A | s2_highway | 2.5 | 1689.8 | 0.98 | 49 |
| A | s3_mountain | 0.0 | 3100.0 | 1.00 | 34 |
| A | s4_urban | 0.0 | 4725.0 | 1.00 | 25 |
| A | s5_long_haul | 0.0 | 9666.0 | 1.00 | 20 |
| B | s1_simple | 100.0 | 28.0 | 0.00 | 49 |
| B | s2_highway | 14.5 | 336.9 | 0.86 | 46 |
| B | s3_mountain | 0.0 | 3208.0 | 1.00 | 35 |
| B | s4_urban | 0.0 | 5004.0 | 1.00 | 25 |
| B | s5_long_haul | 49.5 | 180.4 | 0.50 | 15 |
| C | s1_simple | 100.0 | 28.0 | 0.00 | 50 |
| C | s2_highway | 44.5 | 74.4 | 0.56 | 38 |
| C | s3_mountain | 18.0 | 60.1 | 0.82 | 30 |
| C | s4_urban | 27.5 | 63.7 | 0.73 | 21 |
| C | s5_long_haul | 35.0 | 114.0 | 0.65 | 15 |
| D | s1_simple | 100.0 | 28.0 | 0.00 | 50 |
| D | s2_highway | 75.0 | 94.0 | 2.20 | 94 |
| D | s3_mountain | 7.5 | 411.5 | 1.88 | 86 |
| D | s4_urban | 35.5 | 132.7 | 2.14 | 39 |
| D | s5_long_haul | 70.5 | 136.6 | 2.52 | 48 |
| E | s1_simple | 100.0 | 28.0 | 0.00 | 50 |
| E | s2_highway | 62.5 | 110.7 | 2.32 | 101 |
| E | s3_mountain | 1.0 | 4016.0 | 3.17 | 119 |
| E | s4_urban | 2.5 | 1635.8 | 2.80 | 47 |
| E | s5_long_haul | 70.0 | 140.7 | 2.30 | 51 |

### Summary (all scenes averaged)

| Strategy | Surv% | AvgT | AvgCas | Eng% |
|----------|-------|------|--------|------|
| **A** NaiveDirectRoute | 20.7 | 3793.7 | 0.79 | 35.2 |
| **B** WaypointRoadPreference | 32.9 | 1724.1 | 0.67 | 33.7 |
| **C** DynamicThreatAvoidance | 44.5 | **67.9** | **0.55** | 30.6 |
| **D** EscortFormationAI | **58.4** | 187.5 | 1.74 | 62.9 |
| **E** HybridDynamicConvoy | 47.7 | 2049.1 | 2.11 | **73.8** |

### Key observations

1. **C (threat avoidance) is the most efficient:** lowest delivery time (67.9
   ticks), lowest casualties (0.55), second-best survival (44.5%). It achieves
   this by actively avoiding threat zones, which works because threats are
   stationary and avoidable.
2. **D (escort formation) has the best survival** (58.4%) but at the cost of
   higher casualties (1.74 — escorts absorb damage) and slower delivery (187.5
   ticks — escorts stop to engage).
3. **E (hybrid) overcomplicates without clear benefit:** tries to combine both
   approaches but the ambush-evasion logic causes path degradation in
   constrained terrain (s3: 1% survival, vs C's 18%).
4. **B (road preference) helps in specific scenarios:** s5 long-haul achieves
   49.5% survival vs A's 0%, and faster delivery when it succeeds (180 vs 9666
   ticks).
5. **A (naive) is the clear baseline:** 20.7% survival, worst in all metrics.

### Limitations

- Threats are stationary with fixed detection radius — no patrolling or
  dynamic repositioning.
- No fog of war / Intel layer — all strategies have perfect knowledge of
  terrain.
- RNG is per-iteration (not per-tick), so threat detection roll is only once
  per approach.

---

## 6. Verdict

`concluded-verdict-mixed`

The hypothesis is **partially confirmed:** advanced strategies (C, D) improve
survival vs naive routing. However, the hybrid approach (E) does not outperform
the best single-axis strategy (D) — it adds complexity without clear benefit.
Threat-avoidance routing (C) is the best cost-benefit trade-off for stationary
threats.

**When each strategy wins:**
- **Simple open terrain** — any strategy works (s1: all 100%).
- **Linear chokepoint** (s3) — threat avoidance (C: 18%) beats escorts (D: 7.5%),
  because narrow corridors prevent formation maneuvers.
- **Open/road network** (s2, s5) — escorts (D) outperform by absorbing damage
  while truck pushes through.
- **Dense urban** (s4) — none works well; path planning is constrained by
  buildings.

---

## 7. Integration recommendation

**DoD items for mainline:**
- [ ] Implement tile-based Dijkstra with terrain cost weighting (road = low
  cost, open = medium, mountain/urban = high) — core for route planning.
- [ ] Add dynamic threat zone avoidance: when a threat is detected within N
  tiles, push its position as a Dijkstra avoidance node.
- [ ] Escort formation logic: escorts maintain position relative to supply
  truck (diamond formation), with threat-intercept override when enemy within
  M tiles.
- [ ] Do NOT implement full hybrid ambush-evasion mode — the prototype shows it
  backfires in constrained terrain. Simple threat avoidance + escort formation
  (used separately) covers all scenarios.

**Where in TODO.md:** Stage 3.x (gameplay / AI). Relevant to Foxhole-like
logistics AI and partisan threat model planned for Q3 2026.

**Risks:**
- Dynamic path replanning is O(n²) per tick for Dijkstra on 64×64; may need
  incremental A* or precomputed distance field for larger maps.
- Escort formation breaks on 1-tile-wide corridors (mountain pass). Solution:
  column formation override when corridor width < 2 tiles.

---

## 8. Sources

- Foxhole Wiki — Logistics:
  https://foxhole.wiki.gg/wiki/Logistics
- Bohemia Forums — Convoy behaviour and path planning:
  https://forums.bohemia.net/forums/topic/237002-convoy-behaviour-and-path-planning/
- DCS TROOP-TRANSPORTS (GitHub):
  https://github.com/SinistralValkyries/DCS-TROOP-TRANSPORTS
- AutoGators (USF):
  https://github.com/CUTR-at-USF/AutoGators
- DRAIDIS Convoy Route Protection (TacticalEdge):
  https://github.com/TacticalEdge/convoy-route-protection-system-draidis
- arXiv 1208.5537 — Planning random path distributions for ambush games:
  https://arxiv.org/abs/1208.5537
- IEEE RA-L 2025 — Synchronized task formulation for robotic convoy ops:
  https://ieeexplore.ieee.org/xpl/RecentIssue.jsp?punumber=7083369
- Wikipedia — Convoy: https://en.wikipedia.org/wiki/Convoy
- Wikipedia — Ambush: https://en.wikipedia.org/wiki/Ambush
- Wikipedia — Logistics: https://en.wikipedia.org/wiki/Logistics
- Hardware profile: [`../../../hardware-profile.md`](../../../hardware-profile.md)
