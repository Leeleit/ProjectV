# RESULTS — salvage-recycling-system benchmark

## Environment

| Parameter        | Value                                     |
|:-----------------|:------------------------------------------|
| CPU              | AMD Ryzen 7 5800X (Zen 3, 8C/16T)         |
| Governor         | `powersave` (amd-pstate-epp)              |
| RAM              | 62.7 GiB DDR4 (3200-3600 MT/s)            |
| Compiler         | Clang 22.1.6                              |
| Flags            | `-O3 -march=native -DNDEBUG -std=c++26`   |
| Linker           | lld                                       |

Quantities: **125,000** main measurements (5 strategies × 5 scenes × 5 seeds × 1000 iter) + 1,250 warmup (10 iter per config).

---

## Throughput

### Mean latency (μs/iter, lower is better)

| Strategy                   | S1 Tank | S2 Air | S3 Build | S4 Naval | S5 Mixed | **Avg** |
|:---------------------------|--------:|-------:|---------:|---------:|---------:|--------:|
| A_NoSalvage (baseline)     | 0.16    | 0.06   | 0.22     | 0.04     | 0.81     | **0.26** |
| B_FixedPercentage          | 0.21    | 0.08   | 0.30     | 0.05     | 1.09     | **0.35** |
| C_DestructionMethodModifier| 0.21    | 0.08   | 0.31     | 0.05     | 1.09     | **0.35** |
| D_ComponentBasedScrap      | 0.31    | 0.13   | 0.41     | 0.07     | 1.59     | **0.50** |
| E_HybridSalvage            | 0.43    | 0.15   | 0.63     | 0.08     | 2.22     | **0.70** |

### Overhead relative to A (baseline)

| Strategy | Avg μs | ×A   | Worst-case (S5) |
|:---------|-------:|:----:|:-------:|
| A        | 0.26   | 1.0× | 0.81 μs |
| B        | 0.35   | 1.3× | 1.09 μs |
| C        | 0.35   | 1.3× | 1.09 μs |
| D        | 0.50   | 1.9× | 1.59 μs |
| E        | 0.70   | 2.7× | 2.22 μs |

**Key finding:** All strategies run in **sub-microsecond** average, worst-case is **2.22 μs** (E on 200-wreck mixed battlefield). In ProjectV context, salvage computation is triggered per-event (entity destruction) and is <3 μs even for the largest scene.

---

## Scrap yield

### Mean scrap mass (kg) per scene

| Strategy | S1 Tank | S2 Air  | S3 Build | S4 Naval | S5 Mixed | **Avg** |
|:---------|--------:|--------:|---------:|---------:|---------:|--------:|
| A        | 0       | 0       | 0        | 0        | 0        | **0**       |
| B        | 400,712 | 72,158  | 491,557  | 2,519,248| 909,633  | **878,662** |
| C        | 409,468 | 73,699  | 480,303  | 2,618,855| 1,029,942| **922,453** |
| D        | 370,933 | 65,926  | 492,558  | 2,817,405| 832,084  | **915,781** |
| E        | 311,009 | 55,573  | 392,563  | 2,204,078| 733,538  | **739,352** |

### Mean salvage value (credits) per scene

| Strategy | S1 Tank  | S2 Air  | S3 Build | S4 Naval | S5 Mixed | **Avg**    |
|:---------|---------:|--------:|---------:|---------:|---------:|-----------:|
| A        | 0        | 0       | 0        | 0        | 0        | **0**      |
| B        | 1,025,017| 117,442 | 721,943  | 3,474,634| 1,708,507| **1,409,509** |
| C        | 904,861  | 96,175  | 591,783  | 3,088,244| 1,636,858| **1,263,584** |
| D        | 833,125  | 83,442  | 690,358  | 2,701,968| 1,375,054| **1,136,789** |
| E        | 693,765  | 71,271  | 523,997  | 2,299,465| 1,191,220| **955,943** |

---

## Analysis

### 1. Computational cost is negligible for ProjectV

Even the most expensive strategy (E_HybridSalvage on S5_MixedBattlefield) takes **2.22 μs**. This is far below any meaningful budget in a 16.7 ms frame (60 FPS). Salvage is triggered per-event (entity destruction), and even with hundreds of simultaneous destruction events, total salvage compute would stay below **0.5 ms/frame**.

### 2. Strategy C is the best accuracy-effort tradeoff

| Criterion               | B      | C      | D      | E      |
|:------------------------|:-------|:-------|:-------|:-------|
| Throughput cost         | 1.3× baseline | 1.3× | 1.9× | 2.7× |
| Destruction-method aware| No     | **Yes**| No     | **Yes** |
| Component-aware         | No     | No     | **Yes**| **Yes** |
| Time decay              | No     | No     | No     | **Yes** |
| Team efficiency         | No     | No     | No     | **Yes** |
| Per-component randomness| No     | No     | **Yes**| **Yes** |

C provides **method-aware recovery** (explosion 45% vs structural failure 85%) at virtually zero additional cost over B. This alone creates meaningful gameplay differentiation without complexity.

### 3. Per-component RNG (D, E) adds variance without clear gameplay benefit

Strategies D and E use seeded RNG per component to simulate uneven damage. This adds:
- ~2× throughput overhead
- Non-deterministic yield (differs per seed)
- State requirements (seed tracking per wreck)

The variance is better achieved through the destruction-method modifier, which is deterministic and player-understandable.

### 4. Time decay and team efficiency (E) are important for game balance

While the throughput cost is higher (2.7×), the **absolute cost** is still only ~2.2 μs for 200 wrecks. If the game design requires:
- Salvage yields decreasing over time (forcing timely recovery)
- Team upgrades increasing recovery rates

Then E is the natural choice despite the higher relative cost.

---

## Mapping to ProjectV hot-path

In ProjectV, salvage is computed:
1. **At entity destruction:** `DestroyEntity` → compute salvage yield, store in wreck component.
2. **On interaction:** Player/vehicle interacts with wreck → UI/collection of materials.
3. **Lazy batch:** Periodic sweep for ruin decay (time-threshold recycling).

None of these pathways are on the critical rendering or physics path. Salvage computation is an I/O-bound game logic event. Even the most complex strategy (E) adds <0.5 ms for a full battle of 200 destroyed entities.

---

## Verdict

| Strategy | Recommendation | Rationale |
|:---------|:--------------:|:----------|
| A_NoSalvage | ❌ Reject | No game — removes an entire economy axis |
| B_FixedPercentage | ❌ Reject | No gameplay differentiation by kill method |
| C_DestructionMethodModifier | ✅ **Primary** | Best effort-accuracy; method-aware, near-zero cost |
| D_ComponentBasedScrap | ⚠️ Secondary | High overhead for marginal benefit; prefer C or E |
| E_HybridSalvage | ✅ **When needed** | Required for time-decay or team-efficiency features |

**Integration recommendation:** Implement C as the core salvage formula. If time-decay or team bonuses are needed, promote to E (both share the `DestructionMethodModifier` base, E adds decay + efficiency). D alone is never recommended — either C (simple, method-aware) or E (full simulation).
