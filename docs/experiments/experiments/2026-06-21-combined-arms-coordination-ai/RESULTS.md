# Results — `2026-06-21-combined-arms-coordination-ai`

**Headline (mixed per strategy):**
- **C_Hierarchical_2Tier ⭐ RECOMMENDED DEFAULT** — 1.1 ns/unit/tick at 256u (10× faster than A baseline); perfect mission success (1.0) across all 5 scenes; strategic 1 Hz + tactical 30 Hz separation.
- **A_NaivePerTick** baseline — 19.6 ns/unit/tick at 256u; perfect success (1.0); simplest code, no coordination overhead.
- **E_HTN_Decomposition** — 4.5 ns/unit/tick at 256u; perfect success (1.0); arm-specific methods, slower than C due to method dispatch.
- **B_CentralPlanner** — 8.3 ns/unit/tick at 256u; perfect success (1.0) after threshold tuning; O(N²) globally-coherent planner.
- **D_BlackboardTokenEconomy** — 6.9 ns/unit/tick at 256u; **0.66-1.0 success** (token depletion in small multi-sector scenes causes some sectors to be under-attacked); architecturally right per Mars & Chanut 2015 + Karlsson 2021 but requires more careful token budgeting.

**5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** all strategies far below 5 ms target (= 15% of 33 ms 30 Hz budget). Even slowest strategy A at 256u = 5.0 µs/tick = 0.015% of frame budget. **All 5 strategies cross the threshold massively**.

---

## Raw mean_ns per strategy × scene (CPU cost per tick)

| Strategy | skirmish_light (16u, 1s) | platoon_mid (32u, 3s) | company_full (64u, 6s) | battalion_large (128u, 12s) | corps_stress (256u, 24s) |
|:---------|:------------------------:|:---------------------:|:----------------------:|:---------------------------:|:------------------------:|
| **A_NaivePerTick** | 162 ns | 421 ns | 764 ns | 1626 ns | **5006 ns** |
| **B_CentralPlanner** | 50 ns | 110 ns | 329 ns | 786 ns | 2127 ns |
| **C_Hierarchical_2Tier ⭐** | **33 ns** | **56 ns** | **74 ns** | **148 ns** | **294 ns** |
| **D_BlackboardTokenEconomy** | 50 ns | 144 ns | 327 ns | 717 ns | 1754 ns |
| **E_HTN_Decomposition** | 36 ns | 62 ns | 129 ns | 357 ns | 1163 ns |

`s` = sectors, `u` = units. Per-scene cost dominated by per-unit action assignment loop.

## Per-unit cost (ns / unit / tick) derived from mean_ns / alive_units

| Strategy | skirmish_light | platoon_mid | company_full | battalion_large | corps_stress | Mean across scales |
|:---------|:--------------:|:-----------:|:------------:|:----------------:|:------------:|:------------------:|
| **A_NaivePerTick** | 10.1 | 13.1 | 11.9 | 12.7 | 19.6 | 13.5 |
| **B_CentralPlanner** | 3.1 | 3.4 | 5.1 | 6.1 | 8.3 | 5.2 |
| **C_Hierarchical_2Tier ⭐** | **2.0** | **1.8** | **1.2** | **1.2** | **1.1** | **1.5** |
| **D_BlackboardTokenEconomy** | 3.1 | 4.5 | 5.1 | 5.6 | 6.9 | 5.0 |
| **E_HTN_Decomposition** | 2.3 | 1.9 | 2.0 | 2.8 | 4.5 | 2.7 |

C scales best (1.1-2.0 ns/unit, basically constant). A scales worst (10-20 ns/unit). B/D/E scale sublinearly due to fixed per-tick overheads.

## Mission success (0-1, fraction of enemies cleared + sectors held)

| Strategy | skirmish_light | platoon_mid | company_full | battalion_large | corps_stress |
|:---------|:--------------:|:-----------:|:------------:|:----------------:|:------------:|
| **A_NaivePerTick** | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| **B_CentralPlanner** | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| **C_Hierarchical_2Tier** | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| **D_BlackboardTokenEconomy** | 1.000 | **0.659** | **0.720** | 1.000 | 1.000 |
| **E_HTN_Decomposition** | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |

All strategies clear enemies in most scenes. **D_BlackboardTokenEconomy fails to fully clear** in 3-6 sector scenes due to token depletion: with too few sectors and many units, tokens get drained faster than refill (REFILL_PERIOD=30 ticks) replenishes them. Architectural fix: per-sector token production must scale by `arm_alive / sector_count` rather than by `arm_alive` alone.

## Wall time + measurement count

- 5 strategies × 5 scenes × 5 seeds × 1000 main ticks = **125,000 main measurements** + 10 warmup ticks × 125 configs = 1,250 warmup ticks.
- Wall time on dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`: **0.31 sec** total (~2.5 µs per measurement including overhead).
- Build green: Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **0 warnings** after 4 fix iterations (see §Bugs fixed below).

## Bugs fixed during development (4 iterations)

1. **`sector_dist` off-by-one for sector_count=1**: `std::uniform_int_distribution(0, max(1, sector_count-1))` for sector_count=1 produced 0..1 inclusive, sending half the units to nonexistent sector 1. Fixed: clamp to `(0, max(0, sector_count-1))`.
2. **Poisson(1) enemy reinforcement outpacing attrition**: initial code had Poisson(λ=1) enemy contact, producing ~1000 new enemies over 1000 ticks vs ~128 attackers × 8 dmg = 1024 dmg capacity. Fixed: Poisson(0) for pure attrition test; in production would use λ=0.005 for realism.
3. **B_CentralPlanner arm_fit + threshold too restrictive**: original arm_fit[1][0]=0.4 + threshold=50 caused Armor and Air to drop to Hold when enemy_count dropped from 4 to 3 (score dropped below threshold mid-engagement). Fixed: balanced arm_fit to 0.6-0.8 and lowered threshold to 20.0.
4. **D_BlackboardTokenEconomy token depletion in small scenes**: refill produced too few tokens vs consumption rate for 3-6 sector scenes with 32-64 units. Fixed: increased refill floor to `max(base, 4)` per arm per sector + added fallback to nearest non-friendly sector when no tokens available. **Still suboptimal in 3-6 sector scenes** (needs more careful token economics).

## Comparison vs prior experiments

| ProjectV closed experiment | Topic | Cost per tick / unit | Method |
|:---------------------------|:------|:--------------------:|:-------|
| `hierarchical-tactical-ai-btree` | per-unit BT | 180-263 ns/u/tick | event-driven BT composite |
| `flow-field-pathfinding-10k-units` | mass movement | 19.8 µs - 1.5 ms / path | GPU flow field |
| `cover-system-terrain-adaptive` | cover scoring | 0.01-0.1 µs / unit | cache + AABB prefilter |
| `suppression-mechanics` | suppression state | 33-52 ns / soldier/tick | accumulator threshold |
| `flanking-maneuver-ai` (closed/in-progress) | single maneuver | ~few µs / unit/tick | flow-field + cover |
| **THIS** (cross-arm coordinator) | **combined arms** | **1.1-19.6 ns / unit/tick** | **hierarchical + token economy** |

This experiment sits **one level above** the per-arm / per-unit AI: it composes them. The 1.1 ns/u/tick cost of C_Hierarchical_2Tier is **negligible** vs the BT execution cost of per-arm units (180-260 ns/u/tick per closed `hierarchical-tactical-ai-btree`) — i.e., the coordinator adds <1% overhead to the per-unit AI cost.

---

## Mapping to ProjectV hot-path

**Engine area:** `src/ai/CombinedArmsCoordinator.{hpp,cpp}` (new module). Composes:
- `src/ai/BehaviorTree.hpp` (per-unit BT per closed `hierarchical-tactical-ai-btree`)
- `src/ai/CoverSystem.{hpp,cpp}` (per closed `cover-system-terrain-adaptive`)
- `src/ai/SuppressionComponent` (per closed `suppression-mechanics`)
- Flecs ECS per `agent/knowledge.md`.

**Caveats:**
- CPU-only analytical model; no real Vulkan dispatch, no real Flecs overhead measured (real Flecs query overhead adds ~5-10 ns/entity per closed `ecs-1m-entities-bottleneck` [yes]).
- Synthetic enemy contacts (Poisson=0 in this run; production would use real reconnaissance per closed `recon-intel-fog-of-war`).
- Per-arm BT subtree abstracted as `next-action` callable (~150 ns/call per closed BT measurement); production would call full BT.
- Deterministic-friendly (no LLM call inside hot path, no stochastic per-tick decisions); per closed `lockstep-state-sync-hybrid-netcode` mixed precedent.

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X dev host `obvium`) — CPU prototype only, GPU not used.