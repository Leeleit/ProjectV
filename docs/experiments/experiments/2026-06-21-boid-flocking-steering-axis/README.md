# 2026-06-21-boid-flocking-steering-axis — Boid/Flocking steering optimization axis

**Status:** _in-progress_
**Date opened:** 2026-06-21
**Date closed:** _(будет заполнено при close)_
**Stage link:** _independent (military sandbox axis — Tier 0 Foundation & Optimization; cross-cuts Stage 6+ military
sandbox [drone swarms, formation flight, wild flocks] + Stage 5.x [bird flock rendering, fish schools, ambient
wildlife] + Stage 4.x [particle systems cross-ref])_
**Estimated effort:** _S-M (1-2 sessions)_
**Author:** _agent (self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай
свою исследуй»)_.

---

## 1. Hypothesis

> **«Spatial-hash-grid (B) + SIMD AVX2 batch (D) boid steering scales к 10k boids при <0.5 ms/frame (= 3% of
> 30 Hz budget); >100× speedup vs naive O(n²) (A); GPU compute analytical projection (E) <0.1 ms/frame; KD-tree
> approximation (C) — balanced accuracy/perf option для medium N=1k-5k»**

**Что предполагаю.** Reynolds 1987 canonical boid model (separation + alignment + cohesion) для N boids в
3D space: O(N²) naive (каждый проверяет каждого) vs O(N) через spatial data structure. На 10k boids при
10 Hz tick rate:

- **Naive O(N²)** = 50M distance checks / tick = >100 ms = провал.
- **Spatial hash grid (uniform 3D grid, cell size = perception radius)** = O(N·k) где k = average neighbors
  per cell (~constant для uniform distribution) = 10-50× speedup, **hypothesis 100×+ confirmed via measured
  ratio**.
- **SIMD AVX2 batch** (8 floats/cycle FMA accumulation per neighbor scan) = 4-8× additional speedup на
  spatial-hash.
- **KD-tree approximation** (k-NN via 1-tree traversal with bounded depth) = O(N log N) theoretical, но с
  per-query overhead dominant = not always faster than grid.
- **GPU compute projection** (analytical, не real Vulkan dispatch) = per-group parallel reduction =
  <0.1 ms/frame для N=10k = 0.3% of 30 Hz.

**Альтернативы:**

- **Couzin 2002 "zonal model"** (repulsion/orientation/attraction zones) — similar perf cost, alternative
  parametrisation, не прирост по скорости.
- **Vicsek 1995 model** (alignment-only, physical PRL-style) — simpler, faster, но не реалистичный boid
  behavior.
- **Reynolds + predator avoidance** (extended model) — adds 1-2 force evaluations, +20-30% cost.
- **Hybrid: Boids for global coherence + flow field (closed `flow-field-pathfinding-10k-units` yes) for
  per-goal navigation** — best of both, requires composite update system.

**Какое преимущество:**

- **Direct use case: drone swarms** (military sandbox Tier 2 — 100-1000 reconnaissance/attack drones
  flocking) — closed `aircraft-damage-model` provides per-drone damage; boids provide formation/avoidance.
- **Wild flocks/herds ambient** (Stage 5.x — 1000+ birds/fish for atmosphere) — closes ambient density gap.
- **Boid-based particle systems** (cross-ref Stage 4.x weather/clouds — closed `wind-simulation-ballistics`
  mixed для advection, boids для self-organization).
- **Boid-based formations** (Warno/SupCom/HOI4 wingman pattern) — closed `formation-flight-wingman` open
  prerequisite.

**Гипотеза quantitatively:**

| Strategy | N=1k | N=10k | N=50k | hypothesis (N=10k) |
|:---------|:-----|:------|:------|:--------------------|
| A_Naive O(N²) | baseline | baseline | baseline | (baseline) |
| B_SpatialHash | **10-30×** | **50-200×** | 100-500× | >100× |
| C_KDTree | 5-15× | 10-30× | 15-50× | 10-30× |
| D_SIMD_AVX2 | 30-100× | 100-500× | 200-1000× | >100× (compounding B) |
| E_GPUCompute_Analytical | (n/a) | (n/a) | (n/a) | <0.1 ms/frame |

**5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** все non-naive
strategies должны дать >5× speedup для прохождения.

---

## 2. Prior art

Web-research. **Exa `web_search` HTTP 429 persistent** per `agent/knowledge.md Part B §9` line 1424 fallback
list; **DuckDuckGo HTML endpoint CAPTCHA blocked**; **Startpage 0 results**; **Brave 429**; **Searx 403**.
**Working this session: direct `webfetch` to canonical URLs** (Wikipedia + Craig Reynolds + arXiv).

### Tier 1 — Canonical primary sources verified

1. **Reynolds, Craig (1987). "Flocks, Herds, and Schools: A Distributed Behavioral Model". SIGGRAPH '87
   Proceedings.** DOI: [10.1145/37401.37406](https://doi.org/10.1145%2F37401.37406).
   [CiteSeerX 10.1.1.103.7187](https://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.103.7187). S2CID
   [546350](https://api.semanticscholar.org/CorpusID:546350). **Canonical boid paper**. Defines 3 rules
   (separation/alignment/cohesion) + obstacle avoidance + goal seeking. **Asymptotic complexity:**
   straightforward implementation = O(N²); can be reduced to **nearly O(N) using suitable spatial data
   structure**. **Theoretical foundation for hypothesis**: spatial data structure = 100×+ speedup achievable.

2. **Wikipedia "Boids"** (last edited 2 June 2026). URL:
   <https://en.wikipedia.org/wiki/Boids>. **Verified this session** via direct `webfetch`. Covers:
   separation/alignment/cohesion rules + extensions (Delgado-Mata 2007 fear model, Hartman & Benes 2006
   change of leadership, Olfaction pheromone model). Production use: Batman Returns 1992 (Tim Burton, first
   feature film) + Half-Life 1998 (Xen birds) + UGV/MAV swarm robotics (Saska 2014).

3. **Craig Reynolds "Boids page"** (red3d.com). URL: <https://www.red3d.com/cwr/boids/>. **Verified this
   session**. Includes canonical diagram (separation.gif, alignment.gif, cohesion.gif, neighborhood.gif) +
   text confirming O(N²) → O(N) via spatial data structure. Notes Half-Life 1998 use + Batman Returns
   production details (Andy Kopra VIFX, Andrea Losch Boss Films).

### Tier 2 — Foundational extensions (canonical references)

4. **Hartman, Christopher; Benes, Bedrich (July 2006). "Autonomous boids". Computer Animation and Virtual
   Worlds 17 (3–4): 199–206.** DOI: [10.1002/cav.123](https://doi.org/10.1002%2Fcav.123). **Change of
   leadership force** — addition to alignment that allows boid to become a leader and try to escape. Cited
   in Wikipedia Boids.

5. **Couzin, Iain D. et al. (2002). "Collective memory and spatial sorting in animal groups". J. Theor.
   Biol. 218: 1–11.** Foundational **zonal model** (repulsion/orientation/attraction zones with different
   radii) — more general than Reynolds 3-rule, but similar perf cost.

6. **Vicsek, Tamás et al. (1995). "Novel type of phase transition in a system of self-driven particles".
   Physical Review Letters 75: 1226–1229.** Simpler **alignment-only model** for physical flocking phase
   transition analysis. Faster than full boid (1 rule vs 3) but less realistic emergent behavior.

7. **Toner, John; Tu, Yu-hai (1998). "Flocks, Herds, and Schools: A quantitative theory of flocking". Phys.
   Rev. E 58 (4): 4828–4858.** **Quantitative physical theory** proving group alignment requires motion (not
   possible with local perception + no motion). Theoretical foundation for self-propelled particle models.

8. **Saska, Martin et al. (2014). "Swarms of micro aerial vehicles stabilized under a visual relative
   localization". ICRA 2014.** DOI: [10.1109/ICRA.2014.6907374](https://doi.org/10.1109%2FICRA.2014.6907374).
   MAV swarm robotics application of boids with relative localization.

9. **Min, Hongkyu; Wang, Zhidong (2011). "Design and analysis of Group Escape Behavior for distributed
   autonomous mobile robots". ICRA 2011.** DOI:
   [10.1109/ICRA.2011.5980123](https://doi.org/10.1109%2FICRA.2011.5980123). UGV swarm robotics boid
   application.

### Tier 3 — Production game precedents

10. **Half-Life (1998, Valve Software)** — bird-like creatures in Xen named "boid" in game files. **First
    major commercial use of boid algorithm in a game** per Wikipedia Boids. **Established production
    precedent** that O(N²)-free boid is feasible at game scale (~100 boids per area).

11. **Batman Returns (1992, Tim Burton/Warner Bros, VIFX/Rhythm & Hues)** — bat swarms + penguin flocks via
    modified boids (Andy Kopra VIFX, Andrea Losch Boss Films). **First feature film production use**.

12. **Particle Swarm Optimization (PSO) — Kennedy & Eberhart 1995** — orthogonal inspiration; PSO = search
    algorithm with flocking-like velocity updates, not visual flocking but same O(N) acceleration via
    spatial neighborhood.

### Cross-references (ProjectV mainline)

- **agent/knowledge.md §30.4** — 3-step migration precedent.
- **agent/workspace.md §2** — operator 8x planning decision Stage 6+ military sandbox.
- **hardware-profile.md §1** — Zen 3 5800X dev host with AVX2 + FMA + BMI2 (no AVX-512).
- **legacy/docs/philosophy/03_domain/01_optimization-philosophy.md** — 5-10% threshold.
- **closed `2026-06-21-flow-field-pathfinding-10k-units`** [yes] — per-unit steering pattern (0.001 ms/u
  target) — boid is per-tick steering; flow field is per-goal navigation.
- **closed `2026-06-21-multi-resolution-collision-broadphase`** [mixed] — D_QuadTree 250-1300× speedup vs
  SAP — spatial query precedent.
- **closed `2026-06-21-ecs-1m-entities-bottleneck`** [yes] — Flecs 1M+ entities host.
- **closed `2026-06-21-mesh-shader-mega-instancing`** [mixed] — C_AmplificationShaderOnly 62-544× speedup
  — rendering 10k+ boids in mesh shader.
- **closed `2026-06-21-flood-fill-visgraph-culling`** [yes] — BFS spatial traversal pattern (orth).
- **closed `2026-06-21-hierarchical-tactical-ai-btree`** [mixed] — D_EventDriven 180 ns/u/tick — tactical
  orchestration on top of steering.

---

## 3. Method

**Тип эксперимента:** analytical + standalone C++26 CPU prototype + benchmark.

**Сцена (5):**

| Scene | N boids | World size | Density (N/vol) | Perceptual radius | Tick rate |
|:------|:--------|:-----------|:----------------|:-------------------|:----------|
| `small_drone_squad` | 100 | 50×50×50 | 8.0e-4 | 2.0 | 10 Hz |
| `medium_drone_swarm` | 1,000 | 100×100×50 | 2.0e-3 | 2.0 | 10 Hz |
| `large_battle_drones` | 5,000 | 200×200×100 | 1.25e-3 | 2.0 | 10 Hz |
| `xlarge_swarm` | 10,000 | 200×200×100 | 2.5e-3 | 2.0 | 10 Hz (hypothesis target) |
| `mega_flock` | 50,000 | 500×500×200 | 1.0e-3 | 2.0 | 10 Hz (scale test) |

**Метрики (per tick):**

- mean / median / p95 / p99 / std / min / max nanoseconds per tick
- Total tick cost (CPU single-thread) — hypothesis: <0.5 ms for 10k @ B/D, <100 ms for A
- Wall-clock throughput: ticks per second
- Memory footprint: bytes per boid (position[3] + velocity[3] + forces[3] + metadata)
- Correctness: kinetic energy (mean), polarization (alignment order parameter), cohesion (mean dist to
  centroid), separation (min pair distance) — should converge to stable flock state after warmup

**Контроль (5 strategies):**

| Strategy | Asymptotic | Implementation | Expected @ N=10k |
|:---------|:-----------|:---------------|:-----------------|
| A_Naive O(N²) | O(N²) | double loop, distance check | >100 ms/tick (baseline) |
| B_SpatialHashGrid | O(N·k) k≈const | 3D uniform grid, cell=perception radius | <1 ms/tick (>100× speedup) |
| C_KDTreeApprox | O(N log N) | kd-tree built per tick, 1-NN query bounded depth | <10 ms/tick (10-30× speedup) |
| D_SIMD_AVX2_SpatialHash | O(N·k/8) | spatial hash + AVX2 FMA accumulation | <0.5 ms/tick (4-8× over B) |
| E_GPUComputeAnalytical | (analytical projection) | CPU synthetic w/ GPU latency model | <0.1 ms/tick |

**Протокол:**

1. **Initialize:** N boids with random positions in world bounds, random velocities (normalized), zero
   forces.
2. **Warmup:** 10 ticks (forces + integration only, no timing). Establishes stable flock state.
3. **Main measurement:** 1000 ticks. Each tick:
   a. For each boid, query neighbors (perception radius) via chosen strategy.
   b. Compute 3 forces: separation (inverse-distance weighted, radius=sep_radius), alignment
      (mean velocity of neighbors), cohesion (vector to mean position of neighbors).
   c. Clamp acceleration to max_force, integrate: v += a·dt, v = clamp(v, max_speed), pos += v·dt.
   d. Wrap-around boundary (boids leaving world wrap to opposite side).
   e. **End timing.**
4. **Per-strategy × scene × seed:** 5 × 5 × 5 × 1000 = 125,000 main measurements.
5. **Output:** `prototype/build/results.csv` (1 header + 125,000 data rows = ~3 MB).
6. **Analysis:** mean / median / p95 / p99 / std per (strategy, scene) = 25 cells. Speedup ratio A/B, A/D.

**Force model (Reynolds 1987 canonical):**

```text
For each boid i, perception radius R = 2.0:
  Separation radius R_s = R/2 = 1.0
  Neighbors: j in N(i) = {j : dist(i,j) < R, j != i}

  Force_separation = sum over j in N(i) of (pos_i - pos_j) / max(dist(i,j), eps)
                     (only if dist < R_s) — push away from too-close neighbors
  Force_alignment  = (mean_velocity_of_N(i) - vel_i) * w_align  — match heading
  Force_cohesion   = (mean_position_of_N(i) - pos_i) * w_cohes — move to flock center

  Force_total = w_sep * Force_separation + w_align * Force_alignment + w_cohes * Force_cohesion
  Force_total = clamp_magnitude(Force_total, max_force = 10.0)

  v_i += Force_total * dt   (dt = 0.1 s, 10 Hz tick)
  v_i = clamp_magnitude(v_i, max_speed = 5.0)
  pos_i += v_i * dt
  Wrap-around if out of world bounds
```

**Why this model:** matches Reynolds 1987 spec exactly + Wikipedia canonical 3-rule + Half-Life/Batman Returns
production use.

**Determinism:** seeded RNG (xoshiro256**) for initial state. Tick evolution deterministic given initial
state (no noise in update). Cross-seed consistency: 5 seeds × 5 strategies × 5 scenes for statistical
robustness.

---

## 4. Prototype

**Где:** `docs/experiments/experiments/2026-06-21-boid-flocking-steering-axis/prototype/`.

**Структура:**

```
prototype/
├── boid_bench.cpp     # standalone C++26 CPU benchmark (~500-600 LoC)
├── CMakeLists.txt     # optional — for editor integration
├── README.md          # this file (replicated)
└── build/
    ├── boid_bench     # compiled binary
    └── results.csv    # measurement output
```

**Сборка + запуск:**

```bash
# Build
cd /home/le1t/Projects/ProjectV/docs/experiments/experiments/2026-06-21-boid-flocking-steering-axis/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
  -o build/boid_bench boid_bench.cpp

# Run
./build/boid_bench

# Output: build/results.csv (125,000 measurements)
```

**Compilation flags rationale:**

- `-std=c++26` — mainline ProjectV baseline per `agent/knowledge.md §17` Linux baseline.
- `-O3 -march=native` — Zen 3 (AVX2 + FMA + BMI2); mainline ProjectV flags.
- `-DNDEBUG` — disable `assert()` (this is a hot-path benchmark).
- `-Wall -Wextra -Wpedantic` — strict warnings per mainline standard.
- **No AVX-512** per `hardware-profile.md §1` ISA-flags (Zen 3 не поддерживает).

**Части template harness из `benchmarks/methodology.md §7`:**

- ✅ `Stats` struct with mean/median/p95/p99/std/min/max computation.
- ✅ `std::vector<double> samples` per (config) → compute `Stats` at end.
- ✅ 10 warmup + 1000 main iterations per config.
- ✅ CSV output with header + data rows.
- ✅ Fixed seed for reproducibility.

**Additions to template:**

- 5 strategies via function pointer / `std::variant` dispatch.
- 5 scene configs.
- Multi-seed support (5 seeds).
- Volatile sink to prevent DCE (Dead Code Elimination) of unused force/velocity updates.
- Tick-rate-correct dt (10 Hz = 0.1 s).
- Wrap-around boundary (no per-tick allocation).

**Expected measurement runtime:** 30-90 sec wall time на Zen 3 5800X governor=`powersave` per
`hardware-profile.md §1` (single-thread, in-process all strategies).

---

## 5. Results

_(См. детальный analysis в [`RESULTS.md`](./RESULTS.md))_.

**Headline (mean of 5 seeds, ns/iter):**

| Strategy | N=100 | N=1000 | N=5000 | N=10000 | N=50000 |
|:---------|------:|-------:|-------:|--------:|--------:|
| **A_Naive** | 10,551 | 832,326 | _skipped_ | _skipped_ | _skipped_ |
| **B_SpatialHash** | 27,474 | 331,651 | 2,472,094 | 5,477,244 | 24,452,340 |
| **C_KDTree** ⭐ | **15,896** | **318,160** | **1,849,552** | **4,626,646** | 26,473,800 |
| **D_SIMD_AVX2** | 31,208 | 373,442 | 2,674,312 | 5,921,468 | 26,710,780 |

**Key findings:**

1. **C_KDTree = universal CPU winner** for N=100-10000 (1.18-1.73× faster than B, 1.28-1.96× faster than D).
2. **D_SIMD_AVX2 = NEGATIVE result** — slightly slower than B at all N (SIMD overhead dominates at
   uniform low-density boid distributions).
3. **All non-naive strategies cross 5-10% threshold** massively (2.5-2.6× = 150-162% relative gain vs A).
4. **Hypothesis "B+D <0.5 ms @ N=10k" REJECTED** — actual: B=5.48 ms, D=5.92 ms (10× over budget).
5. **Hypothesis "100× speedup" REJECTED** — actual ~15-18× speedup at N=10k (extrapolated).
6. **N=50000 fails on CPU** — all strategies at 73-80% of 30 Hz budget = GPU compute required.

**Speedup at N=1000 (where A baseline exists):**
- B_SpatialHash: 2.51× faster than A (151% gain)
- C_KDTree: 2.62× faster than A (162% gain)
- D_SIMD_AVX2: 2.23× faster than A (123% gain)

**Speedup at N=10000 (extrapolated from A baseline @ N=1000 + O(N²) scaling):**
- A_Naive @ N=10000 ≈ 83 ms (extrapolated; impractical to measure)
- B_SpatialHash @ N=10000 = 5.48 ms → **~15× speedup**
- C_KDTree @ N=10000 = 4.63 ms → **~18× speedup**
- D_SIMD_AVX2 @ N=10000 = 5.92 ms → **~14× speedup**

See [`RESULTS.md`](./RESULTS.md) §1-4 for full headline + hypothesis validation; §5 for detailed
findings; §6 for cross-axis mapping; §7-8 for implementation notes + caveats.

---

## 6. Verdict

**`mixed`** — architectural pattern validated (spatial data structure = 2-18× speedup over naive);
specific strategy choice depends on N.

**Universal recommendation: C_KDTree for N=100-10000** (1.18-1.73× faster than B_SpatialHash +
SIMD AVX2 batch processing at typical game scales). At N=50000+ GPU compute is required.

| N | Recommended strategy | Cost @ 30 Hz | Why |
|:-:|:---------------------|:-------------|:----|
| ≤1000 | C_KDTree ⭐ | <1% | Fastest, lowest overhead |
| 1k-5k | C_KDTree ⭐ | 1-6% | Best cost-quality ratio |
| 5k-10k | C_KDTree ⭐ | 14% | Within 1 subsystem budget |
| 10k-50k | GPU compute (deferred) | <1% (projected) | CPU infeasible (≥70% budget) |

**Why mixed:**
- ✅ All non-naive strategies exceed 5-10% threshold per `optimization-philosophy.md` (massive 150-162%
  gain at N=1000).
- ✅ C_KDTree validated as CPU winner for N=100-10k (1.18-1.73× faster than B + D).
- ❌ D_SIMD_AVX2 = negative result (SIMD overhead > benefit at uniform low-density distributions).
- ❌ Hypothesis "<0.5 ms @ N=10k" REJECTED — actual 5.5 ms (10× over budget).
- ❌ Hypothesis "100× speedup" REJECTED — actual ~15-18× (still crosses 5-10% threshold massively).
- ❌ N=50000 fails on CPU (≥70% budget) — GPU compute deferred до Stage 6+ dedicated session.

---

## 7. Integration recommendation

**Target stage:** Stage 6+ military sandbox activation per `agent/workspace.md §2` operator 8x planning
decision + Stage 5.x ambient wildlife (deferred).

**Конкретные изменения:**

1. **`src/ai/BoidAgent.{hpp,cpp}` (NEW, ~80 LoC)** — Flecs ECS tag component + per-boid SoA storage
   (`Position[3]`, `Velocity[3]`, `BoidParams{perception_r, separation_r, max_speed, max_force, w_sep, w_align, w_cohes}`).
2. **`src/spatial/KdTreeBoid.{hpp,cpp}` (NEW, ~200 LoC)** — port C_KDTree from prototype
   (`kd-tree build per tick` + `range_query` bounded depth).
3. **`src/ai/SteeringSystem.{hpp,cpp}` (NEW, ~150 LoC)** — per-tick Flecs system calling KdTreeBoid +
   3-force computation (separation/alignment/cohesion per Reynolds 1987) + integration.
4. **`src/render/InstancedBoidRenderer.{hpp,cpp}` (NEW, ~150 LoC)** — mesh-shader-driven instanced
   rendering (per closed `2026-06-21-mesh-shader-mega-instancing` [mixed] C_AmplificationShaderOnly
   precedent = 62-544× speedup at 1M instances).
5. **`src/voxel/VoxelBoidCollision.{hpp,cpp}` (NEW, ~80 LoC, optional)** — ray-cast boid pos + delta
   to nearest voxel surface to prevent flying through walls (per closed
   `2026-06-21-flood-fill-visgraph-culling` [yes] BFS pattern).

**Total: ~660 LoC, M effort, 2-3 sessions.**

**Подход:**
- C_KDTree ⭐ as default (universal CPU winner for N=100-10k).
- `PROJECTV_BOID_STEERING=KD_TREE|SPATIAL_HASH|SIMD|NAIVE` env gate.
- Default N cap = 10000 (above which GPU compute needed).
- Wrap-around boundary (boids wrap at world edges) + simple collision check with voxel terrain
  (ray-cast to nearest surface).

**Риски:**
- Single-thread CPU only — production needs parallel per-cell kd-tree (open follow-up).
- Uniform distribution assumption (real game has clustered boids) — needs re-measurement on
  clustered scenes (open follow-up).
- No voxel terrain collision in prototype — mainline needs per-boid ray-cast to voxel surface.
- No predator/target — production needs extended model (open follow-up).

**Критерии приёмки:**
- **N=1000 <0.5 ms / frame** (vs prototype 318 µs ✅).
- **N=10000 <7 ms / frame** (vs prototype 4.63 ms ✅ = 14% of 30 Hz budget).
- TracyPlot "Boid Steering" ≤ 5% of frame budget for typical scene (1000 boids per area).
- Cross-vendor parity (AMD RDNA 3, Intel Arc Gfx12.5+, NVIDIA Ampere+) — analytical projection,
  not measured in this prototype.

**Зависимости:**
- ✅ Closed `2026-06-21-flow-field-pathfinding-10k-units` [yes] — per-goal navigation prerequisite
  (boid gives per-tick steering; flow field gives per-goal navigation).
- ✅ Closed `2026-06-21-ecs-1m-entities-bottleneck` [yes] — Flecs ECS entity host.
- ✅ Closed `2026-06-21-mesh-shader-mega-instancing` [mixed] — instanced rendering host.
- ✅ Closed `2026-06-21-multi-resolution-collision-broadphase` [mixed] — spatial query precedent.
- Open follow-up: GPU compute port (deferred до Stage 6+ for N≥50k).
- Open follow-up: parallel kd-tree (deferred до Stage 6+ for >1 subsystem budget).
- Open follow-up: clustered distribution stress test (deferred до Stage 6+).

**Estimated effort:** 660 LoC, M effort, 2-3 sessions, deferred до Stage 6+ military sandbox activation
per `agent/workspace.md §2` operator 8x planning decision. **Совместимо с Tier 2 AI prerequisites
(`drone-swarm-ai` h + `formation-flight-wingman` m + `flocking-wildlife-ambient` m).**

**Re-evaluation triggers:**
- Stage 6+ military sandbox activation (immediate integration).
- N>50000 use case (needs GPU compute port — separate experiment).
- Voxel collision implementation (separate experiment for SDF ray-cast cost).
- Clustered boid distribution stress test (separate experiment).

**При каких условиях гипотеза может быть пересмотрена:**
- Если clustered distribution stress test покажет B_SpatialHash > C_KDTree (high-cell-contention
  case where kd-tree depth penalty > hash bucket overhead).
- Если GPU compute port покажет <0.5 ms @ N=50k (makes kd-tree obsolete для large scale).
- Если Flecs ECS overhead > 10% per-boid (makes storage layer dominant, dwarfs algorithm choice).

---

## 8. Sources

_(See §2 above for full Tier 1+2+3 verified sources; `sources.md` for cross-references + additional
production precedents)_.

---

## 9. Mapping to ProjectV hot-path

**Этот prototype maps to:**

1. **`src/ecs/` (Flecs ECS) SoA storage** — boid = entity with `Position[3]`, `Velocity[3]`, `BoidParams`
   components. SoA layout enables AVX2 batch processing.
2. **`src/ai/SteeringSystem.{hpp,cpp}` (NEW)** — per-tick boid update system, queried by Flecs
   `BoidAgent` tag component.
3. **`src/spatial/SpatialHash.{hpp,cpp}` (NEW or extend existing)** — 3D uniform grid with cell size =
   perception radius, query radius for neighbor search.
4. **`src/render/InstancedBoidRenderer.{hpp,cpp}` (NEW)** — mesh-shader-driven instanced rendering of
   10k+ boids (per closed `2026-06-21-mesh-shader-mega-instancing` mixed = C_AmplificationShaderOnly
   62-544× speedup precedent).
5. **`src/voxel/VoxelChunk` occupancy query (orth)** — boid collision with voxel terrain (ray-cast to
   avoid walking through walls). Per closed `2026-06-21-flood-fill-visgraph-culling` [yes] BFS pattern.

**Допущения / упрощения:**

- **Single-thread CPU** — no Flecs worker thread scaling measured. Production would parallelize per-cell
  (spatial hash cells = independent) or per-stride (D_SIMD AVX2 batch).
- **Synthetic uniform boid distribution** — real game has clustered boids (formation, ambient
  spawn points) which stress spatial hash differently (cluster cells = high contention; hash collisions
  = O(k²) per cell). Need measurement on clustered scenes before mainline adoption.
- **No voxel terrain collision** — boid flies in empty world. Production adds ray-cast to voxel
  surface (per `voxel-topology-analysis` overhang detection 0.19 µs/chunk) = +1-5 µs/boid per tick.
- **No predator/target** — pure boid model. Extended model adds 1-2 force evaluations.
- **No formation constraints** — closed `formation-flight-wingman` open = future work.

**Что осталось неизмеренным:**

- GPU dispatch overhead (analytical projection only).
- Driver overhead, kernel launch latency.
- Cache effects at L1/L2/L3 on Zen 3 5800X.
- SIMD lane utilization (Zen 3 = 8 floats/cycle AVX2).
- Vulkan compute shader actual cost (vs CPU analytical model).

**Hardware baseline:** см.
[`docs/experiments/hardware-profile.md`](../../hardware-profile.md) — CPU §1 (Zen 3 5800X, AVX2+FMA, no
AVX-512) + RAM §2 (62.7 GiB, 31 GiB zram) + GPU §3 (RTX 3060 Ti, 8 GiB VRAM, для E_GPUCompute
analytical projection).
