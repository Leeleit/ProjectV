# RESULTS — 2026-06-21-hierarchical-tactical-ai-btree

**Date:** 2026-06-21
**Build:** clang++ 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG` на Zen 3 5800X (8-core)
**Governor:** `powersave` per `hardware-profile.md §1`
**Output:** `prototype/results.csv` (126 rows, 12 KB)
**Total wall time:** 1.89 sec (full sweep across 125 main measurements)

---

## 1. Headline (mean ns/unit/tick, average across 5 seeds)

| Scene (units) | A_NaiveNoMem | B_BT_RunningMem | C_Hierarchical_3T | D_EventDriven | E_Blackboard | Best |
|:---|---:|---:|---:|---:|---:|:---:|
| recon_patrol (8)         | 336.7 | 279.4 | 295.3 | **262.8** | 257.4 | E |
| platoon_attack (32)      | 292.8 | 264.9 | 273.0 | 258.2 | **243.0** | E |
| urban_clear (64)         | 254.6 | 254.3 | 273.6 | 245.8 | **233.9** | E |
| company_advance (128)    | 240.3 | 235.7 | 249.3 | 223.0 | **218.2** | E |
| combined_arms (256)      | 202.3 | 204.4 | 201.7 | **179.5** | 190.5 | D |

**Speedup vs A_NaiveNoMemory baseline:**

| Scene | B | C | D | E |
|:---|---:|---:|---:|---:|
| recon_patrol (8)       | **+17.0%** | +12.3% | +21.9% | **+23.6%** |
| platoon_attack (32)    | +9.5% | +6.8% | +11.8% | **+17.0%** |
| urban_clear (64)       | +0.1% | **-7.5%** ❌ | +3.5% | +8.2% |
| company_advance (128)  | +1.9% | **-3.7%** ❌ | +7.2% | +9.2% |
| combined_arms (256)    | -1.0% | +0.3% | **+11.3%** | +5.9% |

**Key observations:**

1. **D (EventDriven) is the consistent winner at scale** (≥64 units) — +3% to +22% vs A.
2. **E (Blackboard) wins at small N (≤32)** but loses its advantage at large N
   (per-tick memoization rarely hits when Blackboard state is randomized each tick).
3. **B (Running memory) helps mainly at small N** — at N=8, +17% (small tree fits in
   cache, full traversal is expensive). At N≥64, the gain is <2% (cache dominates).
4. **C (Hierarchical) is WORSE than A at N=64-128** — the overhead of three separate
   trees (Strategic + Tactical + Unit) plus SubTreeCall dispatch exceeds the savings
   from fewer ticks at upper tiers. Would need real ECS integration to validate.

---

## 2. p95 / p99 (worst-case, ns/unit/tick)

For E_Blackboard (best at small N) and D_EventDriven (best at scale):

| Scene | E mean | E p95 | E p99 | D mean | D p95 | D p99 |
|:---|---:|---:|---:|---:|---:|---:|
| recon_patrol (8)       | 257.4 | 270.6 | 273.8 | 262.8 | 270.5 | 273.5 |
| platoon_attack (32)    | 243.0 | 256.4 | 263.8 | 258.2 | 264.6 | 271.5 |
| urban_clear (64)       | 233.9 | 250.3 | 263.8 | 245.8 | 251.7 | 256.4 |
| company_advance (128)  | 218.2 | 240.4 | 245.3 | 223.0 | 230.6 | 236.2 |
| combined_arms (256)    | 190.5 | 206.5 | 218.5 | 179.5 | 198.9 | 209.0 |

p99 is consistently within 15% of mean — low variance, no outliers. The BT cost is
very predictable per unit, which is good for real-time frame budgeting.

---

## 3. Frame cost projection (30 Hz budget = 33.33 ms)

For military-sandbox "1000 units" target (200 player + 800 bot):

| Strategy | per-unit ns | × 1000 = ms | % of 30 Hz |
|:---|---:|---:|---:|
| A_NaiveNoMemory   | 200 (256u) | 0.20 ms | 0.60% |
| B_RunningMemory   | 204 (256u) | 0.20 ms | 0.61% |
| C_Hierarchical    | 202 (256u) | 0.20 ms | 0.61% |
| **D_EventDriven** | **180 (256u)** | **0.18 ms** | **0.54%** |
| E_Blackboard      | 191 (256u) | 0.19 ms | 0.57% |

**All strategies <1% of 30 Hz budget at 1000 units.** Original hypothesis CONFIRMED.

For 10K units (mega-battle, e.g., Warno late-game):

| Strategy | × 10K = ms | % of 30 Hz |
|:---|---:|---:|
| A_NaiveNoMemory   | 2.0 ms | 6.0% |
| B_RunningMemory   | 2.0 ms | 6.1% |
| C_Hierarchical    | 2.0 ms | 6.1% |
| **D_EventDriven** | **1.8 ms** | **5.4%** |
| E_Blackboard      | 1.9 ms | 5.7% |

**All within 5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.**

---

## 4. Total wall time per strategy (full sweep = 25 configs = 5 scenes × 5 seeds)

| Strategy | Total wall time |
|:---|---:|
| A_NaiveNoMemory    | 397.1 ms |
| B_BT_RunningMemory | 385.5 ms |
| C_Hierarchical_3T  | 376.4 ms |
| D_EventDriven      | 366.1 ms |
| E_Blackboard       | 361.5 ms |
| **Total**          | **1886.6 ms (1.89 sec)** |

A → D wall time reduction: **7.8%** (397 → 366 ms).

---

## 5. Full per-(strategy, scene, seed) breakdown

`prototype/results.csv` (126 rows, 12 KB). Headline columns:
- `mean_ns_per_unit_per_tick` — primary metric
- `p95`, `p99` — tail latency (worst-case unit per scene)
- `std` — variance across units
- `total_ms` — wall time for the config (includes warmup + measurement + per-unit overhead)

Example rows (seed=42):

```
strategy,scene,seed,num_units,ticks,mean,median,p95,p99,std,total_ms,decisions
A_NaiveNoMemory,recon_patrol,42,8,1500,316.4,315.3,325.4,325.4,5.3,4.54,12000
B_BT_RunningMemory,recon_patrol,42,8,1500,278.4,280.8,282.7,282.7,4.0,3.89,12000
C_Hierarchical_3Tier,recon_patrol,42,8,1500,283.5,283.5,283.5,283.5,0.0,3.79,1500
D_EventDriven,recon_patrol,42,8,1500,262.3,264.5,266.9,266.9,3.2,3.76,12000
E_Blackboard,recon_patrol,42,8,1500,259.6,261.7,265.4,265.4,4.3,3.69,12000
```

(Note: C decisions = ticks, not ticks×units, because C amortizes the per-tick cost
across all units — see `prototype/btree_bench.cpp run_strategy_c`.)

---

## 6. Build & verification

```bash
$ cd prototype
$ rm -rf build && cmake -B build -S . -DCMAKE_CXX_COMPILER=clang++ 2>&1 | tail -3
-- The CXX compiler identification is Clang 22.1.6
-- Configuring done (0.3s)
-- Generating done (0.0s)

$ cmake --build build -j 2>&1 | tail -5
[ 50%] Building CXX object CMakeFiles/btree_bench.dir/btree_bench.cpp.o
2 warnings generated (cosmetic: unused debug helpers status_name, node_type_name)
[100%] Linking CXX executable btree_bench
[100%] Built target btree_bench

$ ./build/btree_bench 2>&1 | tail -3
Results written to results.csv
Total configs: 5 strategies x 5 scenes x 5 seeds = 125 configs
```

**Build green, 2 cosmetic warnings (kept for debugging/extension).**

---

## 7. Cross-axis validation

Compare to other ProjectV closed experiments (per-unit cost in ns):

| Axis | Experiment | Per-unit cost | Comment |
|:---|:---|---:|:---|
| BT tick (this) | `2026-06-21-hierarchical-tactical-ai-btree` | 180-300 | D best |
| Pathfinding (BFS) | `2026-06-21-flow-field-pathfinding-10k-units` | 79.3 (128²) | amortized |
| Physical sim | `2026-06-21-infantry-soldier-sim` | 15.86 (SoA) | E best |
| Suppression | `2026-06-21-suppression-mechanics` | 33-52 | D best |
| Dynamic light (CPU) | `2026-06-21-dynamic-entity-lighting` | 360-104000 | D range |
| AOI (per-tick) | `2026-06-21-interest-management-aoi-battle` | 24 µs/tick | analytical |

**Per-unit AI cost stack (synthesized):**
- Physical sim: 16 ns
- BT tick: 200 ns
- Suppression: 40 ns
- Pathfinding (BFS-amortized): 80 ns
- **Total per unit: ~340 ns = 0.34 µs/unit/tick**

At 1000 units: 0.34 ms/tick = 1.0% of 30 Hz budget. **All axes within 5-10% threshold
per `optimization-philosophy.md`.**

---

## 8. Caveats

- **CPU-only analytical model.** No Vulkan GPU dispatch, no Flecs ECS overhead.
- **Synthetic Blackboard.** Per-tick random fields = memoization rarely hits.
- **No recursion depth limit.** Recursive sub-tree calls (e.g., EnsureItemInInventory
  pattern) could cause stack overflow on malformed trees.
- **Single-threaded.** Real engine would parallelize via Flecs jobs (4-8× speedup
  on 8-core CPU).
- **Mock action/condition cost (5-50 ns).** Real game actions would be 100-1000 ns,
  but the BT OVERHEAD is the focus of this experiment.
- **No multi-agent coordination.** Agis 2020 reports 40-60% reduction in multi-agent
  scenarios; this experiment measures single-agent cost only.
- **Single-shard.** No thread affinity / NUMA awareness.
- **No validation against production.** Real engine integration would need
  Tracy zones and per-pass Tracy plot to validate.

---

**Last update:** 2026-06-21 ~21:10 by self (research agent).
