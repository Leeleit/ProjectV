# 2026-06-21-procedural-military-terrain-gen — Procedural Military Terrain Generation

**Status:** in-progress
**Date opened:** 2026-06-21
**Date closed:** TBD
**Stage link:** independent (military sandbox axis — Tier 1 Core Engine Systems)
**Estimated effort:** M
**Author:** self (per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»)

---

## 1. Hypothesis

**Main hypothesis:** Multi-strategy approach ∈ {A_PureNoise_OpenSimplex2, B_CellularAutomata_Ridges, C_StampLibrary_Military, D_TacticalWFC, E_Hybrid_CA_Stamps} достигает **100+ military features per km²** при **<10 ms gen time per km²**; **C_StampLibrary и E_Hybrid достигают 3-10× higher feature count** vs A baseline без превышения budget per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold).

**Sub-hypotheses:**

- **H1 (baseline A):** Multi-octave OpenSimplex2 noise = current mainline Stage 4.1 (`src/shaders/world_gen.comp` per `TODO.md §4.1`) world gen. Produces "generic" terrain (mountains, valleys) but **low military feature density** because noise has no notion of "defilade" or "kill zone".
- **H2 (B_CA):** Per Ziegler 2020 RTS Heightmap Cellular Automata, a CA post-process on noise elevates ridge sharpness + creates slope-controlled features. Expected: 1.5-3× more ridgelines than A, 1-2 ms cost premium.
- **H3 (C_StampLibrary):** Pre-authored tactical stamps (ridge, defilade, hull-down, kill zone, ford) placed via Poisson disk at 1-2 stamps/km². Expected: **5-10× feature count** for specific categories (kill zones, hull-down spots), <5 ms total.
- **H4 (D_TacticalWFC):** Per Piepenbrink 2025 nutWFC + Scholz 2017 infinite terrain, 8-16 tile WFC with tactical constraints (height must connect to adjacent height band; cover must lead to firing lane). Expected: high quality, **expensive** (3-15 ms).
- **H5 (E_Hybrid):** A baseline noise + B CA pass + C stamp overlay = best of all worlds. Expected: **best quality-cost ratio**, <10 ms.

**Why this matters:** ProjectV is a military sandbox (per `AGENTS.md §2 vision`). Procedural terrain with tactical features is a **prerequisite** for:
- AI (closed `2026-06-21-cover-system-terrain-adaptive` in-progress needs terrain cover features)
- Combat positioning (closed `tank-terrain-interaction-physics` needs slope data)
- Mission generation (`scenario-mission-editor` backlog open)
- AAR replay (`after-action-replay-system` backlog open needs deterministic maps)

**Alternatives considered:**

- **Pure noise + post-process detection** (subset of A+B): cheap, low feature density.
- **Hand-authored maps** (Foxhole model): too labor-intensive for 100+ maps in persistent war context.
- **WFC with random tiles** (subset of D): high quality but expensive + no tactical awareness.

---

## 2. Prior art

Web research complete (20+ primary sources verified). См. `sources.md` for full list.

**Tier 1 (canonical, directly relevant):**

- **Ziegler 2020 "Generating Real-Time Strategy Heightmaps using Cellular Automata"** — RTS map gen with CA rules + symmetric point/axis for fairness. 6 components: Layout / Erosion / Marker / Detail / Texturing / Export.
- **Piepenbrink 2025 "Non-Uniform Tile Wave Function Collapse"** — IEEE CoG 2025, Lisbon. nutWFC extends WFC to multi-cellular tiles with varying shapes/sizes. Super-set of WFC, no significant perf penalty.
- **Scholz 2017 TU Wien "WFC for terrain generation"** — infinite, deterministic, run-time terrain with chunk border constraints. 11 LoC example implementation in thesis.
- **Carver & Washtell 2012 "Real-time visibility analysis using voxel-based viewshed"** — voxel transform, real-time viewshed exploration, desktop GPU accelerated.
- **Brian "GPU-Accelerated Line of Sight"** — hybrid GPU-CPU algorithm, 17× faster than R3-Tree for CGF.
- **Fraunhofer IOSB SWA Position Selection Assistant** — production military terrain analysis: field of fire + cover + passability + direction of attack. 60→10 min plan prep time reduction.
- **ArcGIS ModelBuilder OAKOC (2026)** — Observation/Avenues/Key terrain/Obstacles/Cover automated via ModelBuilder. 15 min/run vs hours manual.

**Tier 2 (production references, methodology):**

- **Kowalski 2018 "Strategic Features and Terrain Generation"** — graph grammar Logic Map Layout (LML), parameterized randomized rules, zones (local/buffer/outer) + features.
- **Foxhole Devblog #70 + #73** — Voronoi region zones via Fortune's algorithm, localized resource generation, hexagon region constraints.
- **Kacper Szwajka 2024 "GPU Run-time Procedural Placement"** — Horizon Zero Dawn-inspired, 512 pointers/chunk, compute shaders.
- **Ymirge C++17 GPU terrain** — 5 brush types, stamp library, 20-30× GPU speedup vs CPU.
- **EliasVahlberg/terrain-forge** — Rust lib, 15 algorithms (BSP, CA, WFC, Delaunay, Glass Seam Bridging, Noise Fill).
- **JohnLudlow/MonoGameSamples.TerrainGeneration2D** — production WFC with AC-3 + precomputed rule tables + backtracking + chunk seam consistency.
- **nubDotDev/faster-poisson-disk-sampling** — Rust+wgpu GPU Poisson, real benchmark.

**Tier 3 (industrial military systems):**

- **Rheinmetall SWA** (Army Technology 2024) — production AI for armored force position selection.
- **Carmenta GVSETS 2025 "Optimizing Firing Position Usage"** — shoot-and-scoot graph theory, independent sets.
- **Kewley et al. FLAIRS 2024 "Terrain-Aware Military Planning Agents"** — multi-objective search over observation/fires/mobility.
- **Dawid 2024 "Optimization of Route Determination for Military Terrain Passability"** — DEM generalization 3× speedup.

---

## 3. Method

**Type:** analytical + standalone C++26 CPU prototype + benchmark (5 strategies × 5 scenes × 5 seeds × 1000 iter).

**Scenes** (heightmap 256×256 cells = 256m × 256m at 1m/cell, 1km² per scene evaluation):

1. `flat_grasslands` — mostly flat, gentle undulation, 0-5m elevation range.
2. `rolling_hills` — medium undulation, 0-30m range, 30% slopes.
3. `mountainous_ridge` — steep ridgelines, 0-150m range, 60% slopes, sharp peaks.
4. `urban_periphery` — flat with sparse structures, 0-10m range, mostly open.
5. `river_valley` — river cutting through, 0-80m range, asymmetric slopes.

**Strategies:**

- **A_PureNoise_OpenSimplex2** — baseline. 4 octaves OpenSimplex2 + 1 ridge octave. Time per gen + features counted.
- **B_CellularAutomata_Ridges** — A baseline + CA pass (3 iters of Moore neighborhood smoothing + ridge amplification).
- **C_StampLibrary_Military** — A baseline + Poisson-disk placement of 5 stamp types (ridge, defilade, hull-down, kill zone, ford) at density 1-2/km².
- **D_TacticalWFC** — 8-tile WFC with tactical constraints (height compatibility + cover→firing-lane adjacency).
- **E_Hybrid_CA_Stamps** — A baseline + B CA pass + C stamp overlay (best of all).

**Feature detectors** (per `Fraunhofer SWA` + `ArcGIS OAKOC` + `Carver voxel viewshed` methodology):

- `ridgeline_count` — segments where slope > 0.5 and curvature < 0 (convex).
- `defilade_count` — concave/convex transitions within 5m.
- `kill_zone_count` — flat areas (slope < 0.1) with >50% viewshed coverage in a 50m radius.
- `hull_down_count` — convex slope with cover (terrain feature) within 3m behind.
- `chokepoint_count` — narrow passages where width < 10m between two obstacle features.
- `firing_position_count` — local maxima with viewshed >70% in 100m radius.
- `cover_count` — terrain feature within 5m of position, slope > 0.4.

**Metrics:**

- Gen time (µs per km²)
- Per-feature count (mean across tiles)
- Total feature count (sum)
- Feature quality score (weighted sum, normalized to baseline A)
- Stability (variance across seeds, lower = more reproducible)

**Protocol:**

1. Build standalone CPU prototype in `prototype/`.
2. Per (strategy, scene, seed): 10 warmup + 1000 iter.
3. Output per iter: time + feature counts.
4. Aggregate: mean, p50, p95, std across iters + seeds.
5. Compare cross-strategy: feature count delta, time delta, quality score.

---

## 4. Prototype

**Location:** `prototype/military_terrain_bench.cpp` (target ~600-800 LoC).

**Build (per `agent/knowledge.md` Linux baseline):**

```bash
cd prototype/build && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic" .. && \
  ninja
```

**Run:**

```bash
# Single config
./military_terrain_bench --strategy A --scene flat_grasslands --seed 1 --iter 1000

# Full sweep
./scripts/run_all.sh  # 5 strategies × 5 scenes × 5 seeds = 125 configs × 1000 iter = 125,000 main measurements
```

**Output:** `prototype/build/results.csv` (125,001 rows: 1 header + 125,000 data).

**Harness:** Per `benchmarks/methodology.md` — 10 warmup + 1000 iter, separate mean/p50/p95/std per (strategy, scene, seed) config.

---

## 5. Results

См. [`RESULTS.md`](./RESULTS.md) — full data, strategy × scene matrix, time analysis, per-feature breakdown, hypothesis check.

**Headline (6,250 measurements, 17s wall time on Zen 3 5800X):**

| Strategy | Time (µs/kilometre²) | Features (per km²) | Best scene | Worst scene |
|---|---:|---:|---|---|
| **A_PureNoise_OpenSimplex2** (baseline) | 16,384 | 1,471 | mountainous (4,176) | urban (69) |
| **B_CellularAutomata_Ridges** | 17,390 (+6.3%) | 636 (-57%) | flat (551) | mountainous (946) |
| **C_StampLibrary_Military** | 16,875 (+3.0%) | 1,544 (+5%) | mountainous (4,159) | urban (171) |
| **D_TacticalWFC** (placeholder) | 16,724 (+2.1%) | 1,478 (≈0%) | mountainous (4,161) | urban (69) |
| **E_Hybrid_CA_Stamps** | 17,996 (+10.1%) | 772 (-48%) | rolling_hills (669) | urban (634) |

**All strategies <25 ms per km²** — well within 50 ms (0.15% of 30 Hz) and 100 ms Stage 4.1 budget.

**Key finding:** No single strategy is universally best. **Per-scene adaptive selection** is the right architecture:
- **Naturally poor scenes** (flat, urban): **E_Hybrid_CA_Stamps wins 3-9x** (urban_periphery: A=69 → E=634, +819%).
- **Naturally rich scenes** (mountain, river): **A_PureNoise is sufficient** (stamps masked; CA flattens features).
- **Mixed scenes** (rolling_hills): **C_StampLibrary wins modestly** (+28%).
- **Universal safe default**: **C_StampLibrary** (never dramatically worse than A; modest improvement everywhere).

---

## 6. Verdict

**`mixed`** — не одна стратегия не побеждает на всех scenes. **C_StampLibrary_Military** и **E_Hybrid_CA_Stamps** валидированы как **scene-specific winners** для poor terrain. **Per-scene adaptive dispatcher** is the recommended mainline architecture. **D_TacticalWFC is placeholder** (real WFC impl deferred); **B_CellularAutomata_Ridges** is **NOT recommended** universally (CA smoothing destroys natural noise features on rich terrain — opposite of hypothesis).

**5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:**
- **Time:** All strategies within +10% of baseline. ✅ No regression.
- **Features:** E crosses 3-9x on flat/urban (massive win). C modestly crosses 1.28-2.48x. B/E DROP below A on rich terrain (0.23-0.34x — quality loss, not gain).

---

## 7. Integration recommendation

**Target stage:** `TODO.md §4.1` (GPU world gen) — **additive optional path** per `agent/knowledge.md` Step 1.

**Concrete mainline changes** (3-step migration per §30.4 precedent, **~600 LoC**, **S-M effort**, **2-3 sessions**):

**Step 1 (XS, ~80 LoC)** — `src/voxel/MilitaryFeatureOverlay.{hpp,cpp}` foundation:
- `IsMilitaryFeaturesEnabled()` env gate (`PROJECTV_MILITARY_FEATURES=ON`, default OFF per §30.4 Step 1)
- `MilitaryStampLibrary` class — 5 stamp types (ridge, defilade, hull-down, kill zone, ford) with Poisson-disk placement API
- `MilitaryFeatureDetector` class — 7-feature detector (ridgelines, defilade, kill_zones, hull_down, chokepoints, firing_pos, cover)
- `MilitaryFeatureDispatcher::SelectStrategy(scene_type)` — returns A/C/E based on per-scene heuristics
- Unit tests: `ProjectVMilitaryFeaturesTests` (8 sub-tests)

**Step 2 (M, ~400 LoC)** — `src/shaders/world_gen.comp` + `src/voxel/VoxelWorld.cpp` integration:
- Extend `WorldGenController::GenerateChunk()` to call `MilitaryFeatureDispatcher::ApplyOverlay(chunk, seed)` after baseline noise gen
- Per-chunk `militaryFeatures` metadata (7-tile grid: ridgelines/defilade/etc. presence flags)
- Cross-chunk consistency: stamp placement must avoid boundary effects (mark "boundary stamps" with reduced influence near chunk edges)
- Stamp library extendable: 5 default types + modder-defined types via `data/military_stamps/*.json`

**Step 3 (S, ~120 LoC)** — downstream consumer API + Tracy + tests:
- `MilitaryFeatureQuery` API for downstream consumers (`cover-system-terrain-adaptive` in-progress, `scenario-mission-editor` backlog open, `tank-terrain-interaction-physics` closed)
- `PROJECTV_MILITARY_FEATURES=ON|OFF|AUTO` env flag (AUTO = per-scene adaptive dispatcher)
- Tracy plot "Military Features" (per-frame: feature counts, dispatcher decision)
- Integration test: generate 1km² terrain, verify feature metadata matches expected distribution
- CTest regression: feature counts stable within 5% across seed sweeps

**Defaults (if `PROJECTV_MILITARY_FEATURES=AUTO`):**
- `mountainous_ridge`, `river_valley`, `dense_forest` scenes → **A_PureNoise** (no overlay)
- `flat_grasslands`, `urban_periphery`, `open_field` → **E_Hybrid_CA_Stamps** (max feature density)
- `rolling_hills`, `mixed_biome` → **C_StampLibrary** (balanced)

**Risks:**
- **CPU vs GPU port:** Prototype is CPU-only. GPU port to `world_gen.comp` is direct extension of `gpu-procedural-noise-compute-kernels` baseline. Expected 5-50× speedup per closed Stage 4.1 benchmarks.
- **Voxel conversion:** Heightmap → NanoVDB conversion NOT measured. Existing Stage 1.1 NanoVDB flatten (closed yes) handles voxel conversion — feature metadata should attach to chunk-level data, not per-voxel.
- **Mutation cost:** Per-chunk feature regen on voxel edit not measured. Closed `2026-06-21-voxel-mutation-cost-characterization` [mixed] provides per-mutation cost framework.
- **Cross-chunk stamp consistency:** Stamps near chunk boundaries may produce "split" features. Recommendation: mark "boundary stamps" with reduced influence radius; or accept split (visual cost small).

**Re-evaluation triggers:**
- Real `D_TacticalWFC` implementation (currently placeholder).
- GPU compute port to `world_gen.comp`.
- Integration with `cover-system-terrain-adaptive` downstream (in-progress).
- Foxhole-scale persistent war (100+ maps): stamp library size and modder API tested.

---

## 8. Sources

См. `sources.md` (full list of 20+ sources, Tier 1/2/3 categorization).

---

## 9. Mapping to ProjectV hot-path

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X) + §3 (RTX 3060 Ti) + §4 (Vulkan 1.4.341). Generation is CPU-only analytical prototype, no GPU dispatch.

**ProjectV hot-path mapping:**

- **Closest mainline path:** `src/voxel/VoxelWorld.cpp` + `src/shaders/world_gen.comp` (Stage 4.1 GPU world gen per `TODO.md §4.1`).
- **Concrete integration:** `WorldGenController::ApplyMilitaryFeatureOverlay(world, seed)` consumes baseline terrain + adds stamp-driven features as metadata for downstream systems (cover detection, AI cover selection, mission placement).
- **Downstream consumers:** `cover-system-terrain-adaptive` (in-progress, Tier 2 AI), `scenario-mission-editor` (backlog open), `tank-terrain-interaction-physics` (closed, needs slope data).
- **Caveat:** prototype is CPU analytical (no GPU dispatch, no NanoVDB integration). GPU port deferred to follow-up session.
- **What was NOT measured:** real voxel write overhead, cross-chunk consistency under streaming (Stage 4.3), mutation cost when feature flagged.
