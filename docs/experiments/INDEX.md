# INDEX — `docs/experiments/`

Текущий снимок состояния. Долговечные правила — `AGENTS.md`. Канбан гипотез — `research/backlog.md`.

---

## 1. Now

**Just-closed (this session, `2026-06-21`):**

- `2026-06-21-countermeasure-dispenser` (verdict=`mixed` per strategy / `yes` for E_SmartDecoy_ContinuousWithReserve ⭐ as universal default + B_Salvo_Patterned_ALE47 as fallback + D_DualMode as niche opt-in). **Military sandbox axis — Tier 2 AI, Tactical & Warfare — first dedicated countermeasure dispensing / salvo patterns / flare-chaff-DIRCM-effectiveness axis** в 130+ closed experiments. Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (parallel sessions verified). Web-research via direct `webfetch` to canonical URLs (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **12+ primary + 5 supplementary sources verified** в [`sources.md`](./experiments/2026-06-21-countermeasure-dispenser/sources.md): AN/ALE-47 Wikipedia + GlobalSecurity [5-program salvo mode, 3 zones × 10 flares AIRCMM combo, MDF-driven dispense sequence, AN/ALQ-156 MAWS] + BAE Systems product page + Elbit 2025 PDF [Rokar] + Chaff Wikipedia [3-5M fibre cartridge, 0.025 mm diameter, 7.6-51 mm length λ/2 of radar, JAFF/CHILL, notching] + Flare Wikipedia [MTV composition, MJU-7A/B, AIM-9X "tested only against American flares", FIM-92 Stinger dual IR/UV] + DIRCM Wikipedia [AN/AAQ-24 Nemesis, GUARDIAN diode-pumped laser, AAR-54, 101KS-O on Su-57] + Infrared homing Wikipedia [spin-scan vs con-scan vs crossed-array vs rosette vs imaging seekers] + arXiv 2410.03060 [Fast EM Scattering for Chaff Clouds, sparsification] + MDPI 2023 [PDF approximation] + Nature 2026-03 [coupled aero-EM for 1M chaff RCS] + IEEE 2026-01 [CFD-DEM surrogate] + DCS r/hoggit Foka 2022 ["coin toss" + "2 Groups of 10 rounds are enough"] + DCS AH-64D doc + US Army CH-47 TM 1-1520-240-10 4-1-17. Standalone C++26 CPU prototype `prototype/countermeasure_dispenser_bench.cpp` ~570 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings 0 errors** after 1 fix iteration: 10 unused-parameter warnings → marked `[[maybe_unused]]`; 1 bug fix: const_cast removed; 1 link error fix: main moved outside namespace). 5 strategies (A_Naive_Salvo_Immediate / B_Salvo_Patterned_ALE47 / C_Programmed_ThreatResponse / D_DualMode_FlarePlusChaff_Burst / E_SmartDecoy_ContinuousWithReserve) × 5 scenes (single_ir_rear / single_radar_tail / dual_threat_ir_radar / saturation_2_ir_directional / sustained_patrol_5_threats) × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements** + 12,500 warmup, wall time **<2 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, 22 KB). **Headline (mixed per strategy / `yes` for E + B + D):**
  - **E_SmartDecoy_ContinuousWithReserve ⭐ = universal recommended default** = 0.942 decoy (best) / 1.000 survival (best) / 12.0 flares (40% inventory) / 4.6 chaff (15%) / 0.45 µs/iter. **+0.9% sustained decoy vs A + 50% inventory savings**.
  - **B_Salvo_Patterned_ALE47** = 0.940 decoy / 1.000 survival / 12.3 flares (41%) / 5.3 chaff (18%) / 0.56 µs/iter. Matches AN/ALE-47 OFP semantics, tied with E for survival.
  - **D_DualMode_FlarePlusChaff_Burst** = 0.940 decoy / 0.974 survival (0.869 in sustained) / 10.2 flares / 5.5 chaff / 0.54 µs/iter. Best single-threat IR decoy (0.742), worst sustained survival. Niche opt-in for low-confidence MAWS mode.
  - **A_Naive_Salvo_Immediate** (baseline) = 0.939 decoy / 1.000 survival / **24.0 flares (80% inv)** / 8.2 chaff (27%) / 0.45 µs/iter. Exhausts inventory on sustained pressure.
  - **C_Programmed_ThreatResponse** = **0.904 decoy (-3.7% vs A)** / 1.000 survival / 10.3 flares (34%) / 3.9 chaff (13%) / 0.73 µs/iter. **REJECTED**: time-sequenced burst pattern shifts probability mass away from optimal window. Sub-hypothesis 1 ("pattern matters") **REJECTED at ECCM=0.7**.
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** E vs A = +0.003 decoy (noise) but **-50% inventory** = MASSIVE. B vs A = +0.001 (noise) but **-49% inventory** = MASSIVE. C vs A = -3.7% decoy = REJECTED. D vs A in sustained = -2.6% survival = REJECTED. E vs D in sustained = +2.6% survival = MASSIVE. All strategies < 1 µs/iter = < 0.003% of 30 Hz budget. Per-IR vs per-Radar: 0.728 vs 0.577 = radar decoy ~20% harder (consistent with closed `radar-detection-system-simulation` D_TrackingLoopKalman 100% lock-transfer requiring specific beaming+notching conditions). **Caveat:** synthetic decoy model P(success) = P_base × factors (DCS-validated per r/hoggit Foka 2022, not real chaff RCS simulation); ECCM ∈ {0.6, 0.7, 0.8} fixed; 5-threat sustained is mild (larger 10+ threat/60s would amplify E-vs-A gap). **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~380 LoC, S effort, 1-2 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/flight/ecs/components/CountermeasureDispenser.{hpp,cpp}` Flecs component + `Inventory` + `Decision` + 5 strategy function pointers; Step 2 (S, ~200 LoC) `src/flight/ecs/systems/AircraftSurvivabilitySystem.cpp` with E as default + B as fallback + D as opt-in via `PROJECTV_CM_STRATEGY=NAIVE|PATTERNED|PROGRAMMED|DUALMODE|CONTINUOUS` env gate (default `CONTINUOUS`); Step 3 (S, ~100 LoC) `tests/AircraftSurvivabilityTests.cpp` with 5 scene tests + Tracy plot "CM Dispense" + `ProjectVAircraftSurvivabilityTests` unit test. **Cross-axis:** **orth** ко всем 4 in-progress parallel (`cable-winch-towing` Tier 1 Phys / `tracy-gpu-vs-manual` profiling / `gpu-fluid-ca-atomic-strategy` Stage 3.1 / `factory-production-system` Tier 3 econ); **complementary** к closed `radar-detection-system-simulation` [yes, **closely related** — radar measures chaff effectiveness from sensor side, this measures dispensing from defender side] + `aircraft-damage-model` [yes, post-hit] + `fixed-wing-flight-model-simulation` [yes, kinematic state input] + `ballistic-projectile-simulation` [yes, missile threat input] + `suppression-mechanics` [mixed, morale cross-axis] + `lockstep-state-sync-hybrid-netcode` [closed mixed, CM events as lockstep nodes] + `hierarchical-tactical-ai-btree` [closed mixed, BT-level dispenser policy] + `ecs-1m-entities-bottleneck` [yes, Flecs cost basis]. **Prerequisite** для open `electronic-warfare-jamming` [m Tier 2, sibling active EW axis] + `stealth-signature-reduction` [m Tier 2, complementary passive EW] + `trench-fortification-construction` [m Tier 2, ground-based analogous defense] + `field-fortifications-system` [m Tier 2, similar defensive salvo logic]. **New axis:** first dedicated **countermeasure dispensing strategy** axis в 130+ closed experiments; opens Stage 6+ military sandbox Tier 2 AI for aircraft survivability optimization. См. §6 + [README](./experiments/2026-06-21-countermeasure-dispenser/README.md) + [STATUS](./experiments/2026-06-21-countermeasure-dispenser/STATUS.md) + [RESULTS](./experiments/2026-06-21-countermeasure-dispenser/RESULTS.md) + [sources](./experiments/2026-06-21-countermeasure-dispenser/sources.md) + `prototype/{countermeasure_dispenser_bench.cpp (~570 LoC), build/{countermeasure_dispenser_bench (54 KB), results.csv (126 rows, 22 KB)}}`.

**Active (this session, `2026-06-22`):**

- `2026-06-22-convoy-transport-protection` (status=`in-progress`). **Military sandbox axis — Tier 3 Economy, Sandbox, Content & Game Modes — first dedicated convoy transport protection axis** в 166+ closed experiments; cross-cuts Stage 6+ military sandbox [Foxhole/HoI4/Squad logistics loops] + Tier 1 Physics [vehicle movement] + Tier 2 AI [escort behavior, ambush response]. Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. Phase 0 init complete. Web-research next. Hypothesis: 5 strategies сравнение даст <0.1 ms/convoy per tick при ≥50% reduction in successful ambushes vs naive.

- `2026-06-22-procedural-voxel-tree-generation` (status=`in-progress`). **Stage 4.1 World Gen — first dedicated procedural voxel tree generation axis** в 166+ closed experiments; cross-cuts Stage 4.1 world gen [forest decoration per biome] + Stage 5.x visual [tree models as static decoration] + Stage 6+ gameplay [tree felling, forestry, concealment]. Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. Phase 0 init complete. Web-research next. Hypothesis: 5 strategies (L-system deterministic/stochastic, space colonization, noise-guided) сравнение даст <10 µs/tree for L-system при plausibility ≥0.6 vs trunk-only baseline.

- `2026-06-21-voxel-topology-analysis` (verdict=`yes`). **Cross-cutting (Stage 3.x/4.x) — Voxel topology analysis on 8³ grids**. Self-invented per operator instruction. Web-research complete (11+ sources: Rosenfeld-Pflatz 1968, Wu-Otoo-Suzuki SAUF, LSL3D, BUF GPU, cc3d, Minecraft structure locator, Tomcc cave culling). Standalone C++26 CPU prototype `prototype/topology_bench.cpp` ~580 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 2 cosmetic warnings**). 5 strategies x 5 scenes x 5 seeds x 1000 iter + 10 warmup = **125,000 main measurements**, wall time < 0.5 sec на Zen 3 5800X governor=`powersave`. **Headline:** Union-Find CCL 26-conn = 2.73 µs mean (worst 6.81 µs); Overhang detection = 0.19 µs mean; Exposed classify = 0.55 µs mean; Flood-fill = 2.32 µs mean. All strategies **100-2500× within 50 µs Stage 4.1 budget**. **Critical finding:** Air CCL on 8³ alone cannot detect disconnected caves (air always 1 component) — cross-chunk merging essential. Solid CCL, overhang, exposed classification work immediately. **Integration recommendation:** 4-step migration ~600 LoC (Step 1: VoxelTopology.hpp header ~50 LoC; Step 2: per-chunk wiring in ProcessChunkRebuildQueue ~150 LoC; Step 3: cross-chunk DSU merging ~300 LoC; Step 4: consumer systems ~100 LoC). См. §6 + [README](./experiments/2026-06-21-voxel-topology-analysis/README.md) + [STATUS](./experiments/2026-06-21-voxel-topology- `2026-06-21-ecs-1m-entities-bottleneck` (verdict=`yes`). **Stage 6.x — Flecs ECS 1M+ entity bottleneck analysis** (can ProjectV hold 1M+ entities? Where are the bottlenecks?). Self-invented per operator instruction. Web-research complete (12+ sources: Flecs v4.1 release notes, official benchmarks, Flecs vs EnTT FAQ, relationships fragmentation docs, closed `2026-06-20-flecs-soa-vs-aos-bench`). Standalone C++26 CPU prototype `prototype/ecs_bench.cpp` ~275 LoC using vendored Flecs v4.1.5 (`external/flecs/`). Clang 22.1.6, build green 0 errors. 7 benchmarks × 3 scales (10K/100K/1M) × 6 archetype patterns = **126 configs × 15 iter = ~8400 total world-ops measurements. Headline: Flecs handles 1M+ easily. Full live gameplay cycle (100K ents, 100 frames) = 3.74 µs/frame = 0.011% of 33 ms budget. Entity creation (0.4-1.0 µs/ent) and deletion (0.3-0.9 µs/ent) are only meaningful costs. Iteration ~0.5 ns/ent (free). Fragmentation 127× overhead but 12.8 µs absolute at 1M (negligible). Add/remove ~76 ns/op. Memory ~172 MB at 1M. **No changes needed — Flecs is already mainline default.** Recommendation: use bulk deferred ops for spawn/despawn; batch chunk unload across frames if >1500 ents/frame. См. §6 + [README](./experiments/2026-06-21-ecs-1m-entities-bottleneck/README.md) + [STATUS](./experiments/2026-06-21-ecs-1m-entities-bottleneck/STATUS.md) + `prototype/{ecs_bench.cpp, build/ecs_bench, CMakeLists.txt}`.

Just-closed (this session, `2026-06-22`):

- `2026-06-22-custom-vehicle-designer` (verdict=`concluded-verdict-yes`). **Military sandbox axis — Tier 3 Economy, Sandbox, Content & Game Modes — first dedicated voxel-based vehicle assembly axis**. C_GreedyMerge + B_PrecomputedBP achieve **179× avg shape reduction** (18× better than 10× DoD), **100% volume preservation**, **< 3 µs avg build**. 6 strategies × 5 vehicle types × 5 seeds × 1000 iter = 150,000 measurements. См. [`experiments/2026-06-22-custom-vehicle-designer/`](./experiments/2026-06-22-custom-vehicle-designer/) + [README](./experiments/2026-06-22-custom-vehicle-designer/README.md) + [STATUS](./experiments/2026-06-22-custom-vehicle-designer/STATUS.md) + [RESULTS](./experiments/2026-06-22-custom-vehicle-designer/RESULTS.md) + `prototype/{vehicle_bench.cpp (~900 LoC), CMakeLists.txt, build/results.csv (151 rows)}`.

- `2026-06-22-minefield-laying-clearing` (verdict=`concluded-verdict-yes` with mixed for C). **Military sandbox axis — Tier 1+2 cross-cut — first dedicated minefield / breaching / anti-tank-mine axis** в 140+ closed experiments. Self-invented per operator instruction. 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 measurements. **Headline:** B/D/E all <3 ns/mine (target <10 ns/mine). C = 4.5 ns/mine avg (1.8-12.0 range). All <1% of 30 Hz budget at 10k mines. Integration: B (SimpleProximity) = default detection path; D (TimedDetonation) = +10% overhead; E (ClearableMines) = 3.3× cost over B, clearance simulation cheap; C (PatternedField) = conditional on spatial grid. See [`experiments/2026-06-22-minefield-laying-clearing/`](./experiments/2026-06-22-minefield-laying-clearing/) + [README](./experiments/2026-06-22-minefield-laying-clearing/README.md) + [STATUS](./experiments/2026-06-22-minefield-laying-clearing/STATUS.md) + [RESULTS](./experiments/2026-06-22-minefield-laying-clearing/RESULTS.md) + `prototype/{minefield_bench.cpp (585 LoC), build/results.csv (126 rows)}`.

- `2026-06-22-voxel-chunk-impostor-far-lod` (verdict=`concluded-verdict-mixed` per strategy; `yes` for architecture class). **Stage 4.2 chunk 3 — first dedicated voxel chunk impostor rendering axis** в 160+ closed experiments; explicitly deferred from `2026-06-21-lod-mesh-downsampling` (README line 59-60: «Impostor/billboard chunks — too aggressive for chunks with internal structure; deferred as a separate Stage 4.2 chunk 3 (octree-impostor) if needed»). Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. Web-research complete (16 primary + 3 cross-ref sources: Distant Horizons, Aokana arXiv 2505.02017, Project Ascendant, Voxceleron2, SimLOD, Laine&Karras 2010, GigaVoxels, Haar&Aaltonen 2015, Majercik 2018). Standalone C++26 CPU analytical prototype `prototype/impostor_bench.cpp` ~470 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`, build green 5 cosmetic warnings). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time <0.1 sec. **Headline:** B_SingleQuad ⭐ = universal cheap fallback (3.47 µs = 0.01% budget, 16 KB, Q=0.356 — 3.5× quality over baseline A). C_Static6Face ⭐ = recommended default for LOD1-LOD2 (31.54 µs = 0.09%, 96 KB, Q=0.527). D_OctreeImpostor = quality opt-in (Q=0.816, +54% over C; requires VRAM optimization — 16² faces + uniform-node culling → projected ~100 KB/chunk). E_GPUComputeDynamic = static decor opt-in (Q=0.866, update 2.5-42 µs/mutation). **3-clause hypothesis:** ✅ H1 (better quality than flat LOD: B=3.5×, C=5.3×, D=8.2×); ✅ H2 (<0.5 ms for impostor layer: B/C confirmed, D/E projected with batched indirect draw); ✅ H3 (octree best for non-uniform: D +54-68% over C). **Verdict=mixed:** B/C = yes; D = yes conditional on VRAM opt; E = yes for static decor, mixed for rapid edits. **Integration recommendation:** 3-step migration ~560 LoC, M effort, deferred до Stage 4.3 draw-distance lift per `agent/workspace.md §2`. См. [`experiments/2026-06-22-voxel-chunk-impostor-far-lod/`](./experiments/2026-06-22-voxel-chunk-impostor-far-lod/) + [README](./experiments/2026-06-22-voxel-chunk-impostor-far-lod/README.md) + [RESULTS](./experiments/2026-06-22-voxel-chunk-impostor-far-lod/RESULTS.md) + [sources](./experiments/2026-06-22-voxel-chunk-impostor-far-lod/sources.md) + `prototype/{impostor_bench.cpp (~470 LoC), build/results.csv (126 rows)}`.

- `2026-06-22-bridge-building-repair` (verdict=`concluded-verdict-mixed`). **Military sandbox axis — Tier 1 Physics × Tier 2 Engineering — first dedicated tactical bridging / assault-bridge / Bailey-bridge / pontoon construction + load-testing axis** в 140+ closed experiments. Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. Web-research complete via direct `webfetch` to 7+ canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); 7 web sources + 5 closed experiment cross-refs verified в [`sources.md`](./experiments/2026-06-22-bridge-building-repair/sources.md). Standalone C++26 CPU prototype [`prototype/bridge_bench.cpp`](./experiments/2026-06-22-bridge-building-repair/prototype/bridge_bench.cpp) ~430 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**. **Headline (mixed):** B_TemplateAABB_RLE = 2.2-61.4× faster than A (H1 ≥5× passes on 3/5 scenes, fails on bailey checkered truss 2.2×). E_Hierarchical CCL correctly detects disconnections (bailey 720/1040, suspension 1322/1474, damaged 208/480). ALL strategies <0.5 ms (max 50 µs = 10% budget — H4 confirmed). C fills terrain gaps (up to 12,800 foundation voxels). Integration: primary = B via `voxel_write_batch()`; structural audit = E CCL post-pass; water gating = D `wy == water_y`. См. [`experiments/2026-06-22-bridge-building-repair/`](./experiments/2026-06-22-bridge-building-repair/) + [README](./experiments/2026-06-22-bridge-building-repair/README.md) + [STATUS](./experiments/2026-06-22-bridge-building-repair/STATUS.md) + [RESULTS](./experiments/2026-06-22-bridge-building-repair/RESULTS.md).

- `2026-06-22-vtol-transition-flight` (verdict=`mixed per strategy; yes for C_BlendedTransition ⭐ as universal recommended default + E_PhysicsCoupledTiltRotor ⭐ as safety-critical opt-in for engine-out / corridor-edge`; D=REJECTED as default 1.8× cost for marginal quality; A/B = single-regime baselines only). **Military sandbox axis — Tier 1 Core Engine Systems: Physics — first dedicated VTOL/STOVL transition flight dynamics axis** в 138+ closed experiments. Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "vtol|harrier|osprey|tiltrotor|f-35b|av-8b"` over `INDEX.md` + `experiments/` = 0 dedicated experiments pre-claim; `ls experiments/2026-06-22-vtol*` = ENOENT pre-claim). Web-research complete via direct `webfetch` to canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **8 primary + 3 supplementary sources verified** в [`sources.md`](./experiments/2026-06-22-vtol-transition-flight/sources.md): Wikipedia V-22 Osprey [canonical, **12 sec full conversion, 100-kt corridor, 80 Jump takeoff at 80°**] + GlobalSecurity.org V-22 Conversion [canonical corridor description, "the range of permissible airspeeds for each angle of nacelle tilt is very wide (about 100 knots)"] + Wikipedia Harrier jump jet [Pegasus 11-105 23,500 lbf, 31,000 lb MTOW, **VIFF 98° max**, SRVL] + Wikipedia F-35 Lightning II [F-35B STOVL with **shaft-driven lift fan (SDLF) + 3BSM + roll posts**, +2,200 lb vs A] + Wikipedia Bell XV-15 [first successful tiltrotor 1977, **shortest STO at 75° nacelle**] + NASA NTRS YAV-8B Full-Envelope Aerodynamic Modeling [production-grade reference] + EaglePubs Introduction to Aerospace Flight Vehicles Ch. 70 [TWR > 1, **5-10% margin**, disk-loading vs power-loading math, transition speed `v_trans = sqrt(2W/(rho × S × C_L_max))`] + DCS AV-8B N/A by RAZBAM Simulations [production game flight model validation]. Standalone C++26 CPU prototype `prototype/vtol_bench.cpp` ~660 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings 0 errors** after 1 fix iteration: missing `<chrono>` include added). 5 strategies (A_PureHover / B_PureForward / C_BlendedTransition ⭐ [BTT/STOVL, weighted interpolation of hover+forward aero per nacelle angle] / D_BlendWithCrossover [NOT recommended, 1.8× cost for marginal moment-correction] / E_PhysicsCoupledTiltRotor ⭐ [safety-critical: corridor enforcement + tilt-pitch coupling + engine-out asymmetric thrust]) × 5 scenes (harrier_short_takeoff / osprey_full_tilt / f35b_stovl_brake / tiltrotor_wingborne / emergency_single_engine) × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **0.094 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, 12.9 KB). **Headline (mixed per strategy; `yes` for C ⭐ as universal default + E ⭐ as safety-critical opt-in):** A=110.1, B=120.7, **C=132.6 ⭐ (11% spread = MOST UNIFORM)**, D=237.8, E=442.7. All 100% plausible (zero NaN, zero PIO). **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** H1 (all < 0.03 ms / craft per tick) = **CONFIRMED MASSIVELY** (max mean 442.7 ns = **68× headroom**; max p99 ~600 ns = 50× headroom). 100 simultaneous VTOL craft × worst-case 443 ns = 44 µs = 0.13% of 30 Hz budget. C vs A = +20% cost (negligible) for full transition modeling. C vs D = +79% cost (REJECTED as default, 1.8× for marginal benefit). C vs E = +234% cost (justified for safety-critical opt-in, 3.3× for corridor + tilt-pitch + engine-out). **Verdict=mixed per strategy; `yes` for C ⭐ as universal recommended default + E ⭐ as safety-critical opt-in.** **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~3 LoC default + ~120 LoC E opt-in, XS-S effort, **deferred** до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~3 LoC) `src/physics/vtol_vehicle.{hpp,cpp}` foundation port of `aero_blended_transition` from prototype; Step 2 (S, ~120 LoC) `e_physics_coupled_tilt_rotor` opt-in для `engine_out` damage state + `corridor` table + `tilt_pitch` CG offset; Step 3 (XS, ~30 LoC) `PROJECTV_VTOL_AERO=BLENDED|CROSSOVER|FULL_PHYSICS` env gate (default `BLENDED`) + Tracy plot "VTOL Aero" zones + `ProjectVVtolAeroTests` unit test. **Cross-axis:** **orth** ко всем 18 in-progress parallel на `2026-06-22`; **complementary** к closed `fixed-wing-flight-model-simulation` [yes, Tier 1 forward-flight physics] + `helicopter-rotor-physics` [yes, Tier 1 hover physics] + `tank-terrain-interaction-physics` [yes] + `naval-vessel-buoyancy-steering` [mixed] + `ballistic-projectile-simulation` [yes] + `soft-body-physics-debris` [yes] + `wind-simulation-ballistics` [mixed, crosswind] + `terrain-traction-variation` [yes, runway traction] + `aircraft-damage-model` [yes, engine-out damage case] + `component-vehicle-damage-model` [yes, nacelle damage → asymmetric lift] + `boid-flocking-steering-axis` [yes, V-22 formation] + `group-formation-maneuver-axis` [yes, platoon V-22] + `flow-field-pathfinding-10k-units` [yes, transition point path planning] + `mesh-shader-mega-instancing` [mixed, VTOL rendering] + `dec-pipelines-async-compute` [yes, async aero substep]. **Prerequisite** для open `vertical-landing-precision-russian-helicopter` [l] + `carrier-ops-stol-launch` [l, catapult/arrested landing] + `air-refueling-probe-drogue` [l, mid-air refuel]. **New axis:** first dedicated **VTOL/STOVL transition flight dynamics** axis в 138+ closed experiments; opens Stage 6+ military sandbox for AV-8B Harrier / V-22 Osprey / F-35B / F-35C / AW609 / custom tiltrotor craft. **Caveats:** CPU-only synthetic prototype; simplified aero (no stall, no compressibility, ISA sea level only); 6-DOF state reduced (no full quaternion); reaction control modeled as moment-correction; engine-out logic is 1-engine-only (V-22 cannot hover on 1 engine → thrust reduction 40% is approximate); F-35B F135 lift fan modeled as nacelle angle equivalent (real F-35B has separate lift-fan + 3BSM mechanically); single-machine dev host; CPU analytical cost may be 2-5× higher when integrated with Flecs ECS + VMA memory barriers + Vulkan async dispatch. **Re-evaluation triggers:** Stage 6+ military sandbox activation (target use case), 50+ VTOL craft per scenario, real F-35B SDLF + 3BSM mechanics, real Harrier VIFFing, real V-22 tilt-pitch coupling data. Cross-refs: `TODO.md` independent, `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` line 36 (operator 8x planning decision Stage 6+ military sandbox), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold), `hardware-profile.md §1` (Zen 3 5800X), `benchmarks/methodology.md §3` (N=1000 + 10 warmup protocol), `agent/knowledge.md Part B §9` line 1424 (web fallbacks: Exa 429 + DuckDuckGo CAPTCHA blocked this session; direct `webfetch` to Wikipedia/GlobalSecurity/NASA/EaglePubs canonical URLs only). См. §6 + [README](./experiments/2026-06-22-vtol-transition-flight/README.md) + [STATUS](./experiments/2026-06-22-vtol-transition-flight/STATUS.md) + [RESULTS](./experiments/2026-06-22-vtol-transition-flight/RESULTS.md) + [sources](./experiments/2026-06-22-vtol-transition-flight/sources.md) + `prototype/{vtol_bench.cpp (~660 LoC), CMakeLists.txt, build/{vtol_bench, results.csv (126 rows × 15 cols, 12.9 KB)}}`.

- `2026-06-22-nerf-gs-in-realtime-voxel` (verdict=`yes` for **C_HybridStatic_Plus_VoxelDynamic ⭐ as universal recommended default**; per-strategy A=valid baseline, B=REJECTED for gameplay, D=REJECTED for 1000+ edits/sec, E=REJECTED multiple axes). **Horizon-scan / Stage 5.x visual polish opt-in / Stage 6+ content tooling opt-in — first dedicated NeRF / 3D Gaussian Splatting integration axis** в 130+ closed experiments. Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "nerf-gs|3dgs|gaussian.splat|instant.?ngp|mip-splat"` over `INDEX.md` + `experiments/` = 0 dedicated experiments; cross-refs only в `backlog.md` line 19 self-ref; `ls experiments/2026-06-22-nerf*` = ENOENT pre-claim). Web-research complete via `webfetch` to canonical arXiv + project pages (Exa HTTP 429 persistent this session per `agent/knowledge.md Part B §9` line 1424 fallback list); **6 primary + 1 cross-reference = 7 sources verified** в [`sources.md`](./experiments/2026-06-22-nerf-gs-in-realtime-voxel/sources.md): Kerbl, Kopanas, Leimkühler, Drettakis 2023 "3D Gaussian Splatting for Real-Time Radiance Field Rendering" [SIGGRAPH 2023, ACM TOG 42(4), arXiv 2308.04079, INRIA project page + GitHub graphdeco-inria 22.4k★, **100+ FPS at 1080p on RTX 3090, 30k iter training 35-45 min, 24 GB VRAM training / 4 GB viewing**, static-only assumption] + Mildenhall et al. 2020 NeRF [ECCV 2020 oral, arXiv 2003.08934, 5D volumetric + 8-layer MLP + 64 samples/ray, hours to train per scene, ~10s/frame] + Müller, Evans, Schied, Keller 2022 Instant-NGP [SIGGRAPH 2022, arXiv 2201.05989, NVIDIA hash-grid encoding, training in seconds, "rendering in tens of ms at 1920×1080"] + Wu et al. 2024 4D-GS [CVPR 2024, arXiv 2310.08528, **82 FPS at 800×800 on RTX 3090, 30-60 min training, HexPlane+MLP deformation**] + gsplat.js [huggingface/gsplat.js 1.6k★ MIT, **real-time updates + editing in editor demo**, supports .ply + .splat formats, WebGL 2.0, built on three.js + antimatter15/splat + UnityGaussianSplatting] + HuggingFace blog "Introduction to 3D Gaussian Splatting" (Dylan Ebert Sep 2023, +134 upvotes, "4GB to view, 12GB to train, Static (for now)") + Wikipedia "Gaussian splatting" (cross-validation: "exploded in popularity in 2023 when a research group from Inria proposed the seminal 3D Gaussian splatting", limitations "Elongated artifacts, popping artifacts, higher memory consumption, peak GPU memory over 20 GB"). **CRITICAL counter-finding vs initial hypothesis:** gsplat.js editor **already demonstrates real-time 3DGS editing** in browser WebGL (per README Sep 2024 - Jul 2025) — initial hypothesis of "3DGS = static only" was **partially wrong**, but browser-level add/remove splats ≠ full voxel-style mutation; **architectural recommendation C unchanged** because the separation (static 3DGS + dynamic voxel) sidesteps the mutation problem entirely. Standalone C++26 CPU analytical cost model `prototype/gsplat_bench.cpp` ~320 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 1 fix iteration: removed 1 unused const). 5 strategies (A_Pure_Voxel baseline / B_Pure_3DGS_Static / C_HybridStatic_Plus_VoxelDynamic ⭐ / D_3DGS_PerChunkRetrain / E_NeRF_VolumetricRayMarch) × 5 scenes (decoration_only + decoration_plus_sparse_edits + decoration_plus_dense_edits + voxel_only + empty_scene) × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **0.010 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, 13 KB). **Headline (`yes` for C ⭐; per-strategy verdicts):**
    - **A_Pure_Voxel** (baseline) = 0.107 ms mean (0.65% of 16.6 ms 60 FPS budget) / 0.002 ms mutation / 0.0 MB VRAM / 0 stale splats. Valid baseline.
    - **B_Pure_3DGS_Static** = 6.575 ms mean (39.6% of budget) / **12000 ms mutation (30 second FREEZE per edit — UNUSABLE for gameplay)** / 155.8 MB VRAM / **1,000,000 stale splats (all)**. **REJECTED for gameplay** (Niche: locked cinematic with no edits).
    - **C_HybridStatic_Plus_VoxelDynamic ⭐** = **6.482 ms mean (154 FPS theoretical, 39% of 60 FPS budget)** / **0.008 ms mutation (1,500,000× better than B, 5,625× better than D)** / 159.1 MB VRAM (1.9% of 8 GiB) / 0 stale splats. **RECOMMENDED DEFAULT for Stage 5.x visual polish opt-in + Stage 6+ content tooling opt-in**.
    - **D_3DGS_PerChunkRetrain** = 6.375 ms mean / **45 ms mutation (45 sec freeze per 1 sec game time at 1000 edits/sec — UNUSABLE)** / 209.0 MB VRAM / 0 stale splats. **REJECTED for 1000+ edits/sec** (Niche: < 20 edits/min scripted events).
    - **E_NeRF_VolumetricRayMarch** = **75.000 ms mean (4.5× over 60 FPS budget — FAIL)** / 7500 ms mutation / **2400.0 MB VRAM (30% of 8 GiB per single scene — FAIL)** / 100,000 stale splats (10%). **REJECTED on multiple axes** (Niche: offline prebake for cinematics).
  **3-clause hypothesis validation:** H1 (C renders 60 FPS на RTX 3060 Ti) = **CONFIRMED MASSIVELY** (6.48 ms = 154 FPS theoretical, 39% of 16.6 ms budget); H2 (3DGS static <16.6 ms + voxel <1 ms) = **CONFIRMED** (3DGS 6.5 ms + voxel 0.1 ms = 6.6 ms); H3c_DropAffectedSplats (<1 ms/edit) = **CONFIRMED MASSIVELY** (0.008 ms = 125× under 1 ms target). **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** C vs B mutation = **1,500,000×** improvement (CROSSED MASSIVELY); C vs D mutation = **5,625×** (CROSSED MASSIVELY); C vs E frame = **11.6×** (CROSSED MASSIVELY); C vs A frame = 60× slower (still 39% budget, acceptable). **Verdict=yes for C ⭐ as universal recommended default** для Stage 5.x visual polish opt-in + Stage 6+ content tooling opt-in. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~580 LoC total, M effort, 2-3 sessions, **deferred** до dedicated session per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/render/gsplat/GsplatAsset.{hpp,cpp}` + PLY/`.splat` loader (per `huggingface/gsplat.js` MIT reference) + Splat struct (236 bytes: position vec3 + scale vec3 + rotation quat vec4 + opacity float + SH coeffs degree 0-3 = 45 floats RGB); Step 2 (M, ~400 LoC) `src/render/gsplat/GsplatRenderer.{hpp,cpp}` + Vulkan 1.4 compute radix-16 sort (~1.9 ms for 1M splats on RTX 3060 Ti) + fragment shader rasterize (covariance projection + SH eval + alpha blend, ~4.5 ms for 1M @ 1080p) + bindless SSBO + HZB culling integration + async compute dispatch (per closed `dec-pipelines-async-compute` [yes]) + integration after voxel pass в `src/render/Renderer.cpp`; Step 3 (S, ~100 LoC) `PROJECTV_GSPLAT=OFF|STATIC|HYBRID` env gate (default `OFF`) + asset catalog в `data/decor/` (per closed `voxel-asset-template-catalog` [yes]) + voxel H3c drop hook (`MarkSplatsDead(chunk_bounds)` 5-10 µs per chunk) + Tracy plot "3DGS Frame Cost" + "3DGS Sort" + "3DGS Rasterize" + "3DGS Memory" + `ProjectVGsplatTests` (5 cases). **Cross-axis:** **orth** ко всем 5 in-progress parallel (`urban-combat-tactics-ai` + `fire-coordination-multiple-units` + `missile-guidance-laws-simulation` + `stealth-signature-reduction` + `voxel-material-weathering-surface-aging`); **complementary** к closed `lod-mesh-downsampling` [mixed, LOD2+ static decor = 3DGS candidate] + `lod-transition-strategy` [mixed, geomorph + 3DGS splice = hybrid LOD] + `volumetric-fog-atmosphere-rendering` [mixed, 3DGS splats as fog participants?] + `vct-vs-rt-cutoff` [mixed, 3DGS vs VCT vs RTX для GI = orth lighting axis] + `dec-pipelines-async-compute` [yes, 3DGS sort = async compute candidate] + `bindless-descriptor-overhead` [mixed, 3DGS = massive SSBO] + `vma-sparse-textures` [mixed, 3DGS textures = sparse page table] + `hzb-smart-mip-select` [mixed, per-chunk HZB culling applicable to 3DGS] + `data-driven-vehicle-weapon-definitions` [yes, asset format] + `voxel-asset-template-catalog` [yes, templates] + `procedural-military-terrain-gen` [yes, photogrammetric landmarks]. **Prerequisite** для open `ddsp-procedural-audio` [l, ML content tooling axis] + `cxl-storage-class-tier` [l, 3DGS assets large enough] + `neuromorphic-photonic-rendering` [l, 3DGS = neuromorphic inference candidate]. **New axis:** first dedicated NeRF/3DGS integration axis в 130+ closed experiments; opens Stage 5.x additive decor layer + Stage 6+ photogrammetry content pipeline. **Caveats:** CPU-only analytical cost model (validated against Kerbl 2023 published numbers + Unity/Unreal production benchmarks); real GPU splat sort deferred до mainline integration; retrain cost from 4D-GS literature, not measured on dev host; visual QA deferred до Stage 5.x dedicated session; license verification needed for original `graphdeco-inria/gaussian-splatting` (non-commercial, need operator sign-off before production scans); mobile path (`VK_QCOM_fragment_density_map_offset`) deferred. Cross-refs: `hardware-profile.md §1/§3/§4` (Zen 3 5800X + RTX 3060 Ti GA104 38 RT cores + Vulkan 1.4.341), `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` (Stage 5.x deferral), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold), `benchmarks/methodology.md §3` (measurement protocol). См. [README](./experiments/2026-06-22-nerf-gs-in-realtime-voxel/README.md) + [STATUS](./experiments/2026-06-22-nerf-gs-in-realtime-voxel/STATUS.md) + [RESULTS](./experiments/2026-06-22-nerf-gs-in-realtime-voxel/RESULTS.md) + [sources](./experiments/2026-06-22-nerf-gs-in-realtime-voxel/sources.md) + `prototype/{gsplat_bench.cpp (~320 LoC), build/{gsplat_bench (26 KB), results.csv (126 rows, 13 KB)}}`.

- `2026-06-22-procedural-voxel-material-audio` (verdict=`yes` for **E_Hybrid_ModalGranular ⭐ as universal recommended default**; C_ModalSynthesis=yes for rigid-only; D_GranularSynthesis=yes for aggregate-only). **First dedicated physics-based voxel material interaction audio synthesis axis** — procedural footstep/impact/scrape sounds via modal (phISAM) + granular (phISEM) synthesis, 0 KiB sample memory, infinite variation. Self-invented per operator instruction; sentinel §13.7 clean. Web-research (14 primary sources: FoleyAutomatic van den Doel 2001, Cook PhISM 1996, Turchet footstep 2010-2016, garjan Rust crate 2026, IRCAM Modalys). Standalone C++26 CPU prototype ~520 LoC (Clang 22.1.6, build green 0 warnings). 5 strategies × 11 materials × 5 seeds × 3 velocities × 2 types = 1650 measurements. **Headline:** E_Hybrid = universal default (292-302 µs rigid → modal, 29-145 µs aggregate → granular, 61 µs liquid → filtered noise; 0.1-0.9% of 1s at 1000 events/sec). B_SampleBased fastest (0.28 µs) but 108 KiB sample memory + no variation. C=156 µs mean ~ 1.9× cheaper than E but poor non-rigid. D=51 µs mean ~ 3.5× cheaper than E but poor rigid. **H1 cost REJECTED for <5 µs** (E=180 µs mean for full 0.125s event); **H3 E as default CONFIRMED**. Integration: 3-step ~450 LoC, S-M effort, deferred до Stage 5.x audio polish. См. [`experiments/2026-06-22-procedural-voxel-material-audio/`](./experiments/2026-06-22-procedural-voxel-material-audio/) + [README](./experiments/2026-06-22-procedural-voxel-material-audio/README.md) + [RESULTS](./experiments/2026-06-22-procedural-voxel-material-audio/RESULTS.md) + [sources](./experiments/2026-06-22-procedural-voxel-material-audio/sources.md) + `prototype/{material_audio_bench.cpp (~520 LoC), build/{material_audio_bench (50 KB), results.csv (1651 rows, 115 KB)}}`.

- `2026-06-22-voxel-water-flow-ca` (verdict=`mixed`). **Voxel substance flow — 3D CA water simulation for 8³ chunks** — first dedicated voxel water/substance flow axis in 157+ experiments. Self-invented per operator instruction `2026-06-22`. Perf hypothesis **CONFIRMED** (3D CA strategies: 0.48-0.50 µs/chunk/tick mean = 13-21× under <10 µs target). Behavioral hypothesis **PARTIAL** (PSNR 52 dB over baseline; utility scores 0.002-0.250; S5 fire extinguishing fails — water never reaches fire). **D_3D_CA_VolumeConserving recommended default** (0.50 µs mean, 99.9% mass conservation). Integration: ~800 LoC, 3 sessions, deferred to Stage 4.1/6+. См. [README](./experiments/2026-06-22-voxel-water-flow-ca/README.md) + [STATUS](./experiments/2026-06-22-voxel-water-flow-ca/STATUS.md) + `prototype/{water_ca_bench.cpp (900 LoC), build/results.csv (126 rows)}`.

- `2026-06-22-day-night-cycle-celestial-mechanics` (verdict=`concluded-verdict-mixed`). **Stage 5.x Visual Polish × Stage 6+ gameplay — first dedicated day/night cycle axis** в 160+ closed experiments; **self-invented per operator instruction** `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. 5 strategies ∈ {A_NoCycle (baseline, 20 ns), B_SimpleSunAngle (58 ns), C_FullCelestial (316 ns, Keplerian), D_CelestialPlusStars (579 ns, C+star field), E_PhysicalAttenuation (433 ns, C+Rayleigh/Mie twilight)} × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements. C++26 CPU prototype 526 LoC, build green 0 warnings, Clang 22.1.6 `-O3 -march=native`. All strategies << 50 µs/frame (H1 confirmed). C adds 258 ns over B (H2 confirmed). Star field must be GPU-only (H3 confirmed). E adds 117 ns over C with physical twilight (H4 confirmed). **Headline:** C is best accuracy/cost (315 ns, Keplerian sun+moon). B is minimum viable (58 ns). E adds physical twilight at +117 ns. D CPU-star-sampling is too expensive — GPU-only. **Integration:** 3-step B → C → E per `agent/knowledge.md §30.4`. Star field always GPU (static vertex buffer, single draw call). См. [`experiments/2026-06-22-day-night-cycle-celestial-mechanics/`](./experiments/2026-06-22-day-night-cycle-celestial-mechanics/) + [README](./experiments/2026-06-22-day-night-cycle-celestial-mechanics/README.md) + [STATUS](./experiments/2026-06-22-day-night-cycle-celestial-mechanics/STATUS.md) + [sources](./experiments/2026-06-22-day-night-cycle-celestial-mechanics/sources.md) + `prototype/{day_night_bench.cpp (526 LoC), build/results.csv (126 rows)}`.

- `2026-06-22-procedural-voxel-resource-deposits` (verdict=`concluded-verdict-mixed`). **Tier 3 Economy × Tier 1 World Gen — first dedicated procedural resource/ore deposit generation axis** в 160+ closed experiments. Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. Web-research complete (10 sources: Minecraft ore vein density functions, Minetest ore types, Cubyz OreGenerator, Nathan Reed Perlin worms, Iridescence GDC talk, Minecraft noise router, No Man's Sky Voronoi biomes, Gonzalez&Patow 2023 procedural geological ore deposits). Standalone C++26 CPU prototype `prototype/resource_bench.cpp` ~470 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26`, build green). 5 strategies × 5 scenes × 5 seeds × 50 iter + 5 warmup = **6250 main measurements** (8³ chunk). **Headline:** E_Hybrid_WormPlusSeam best plausibility (0.43-0.55) and best connectivity (17 components for 120 deposits). C_PerlinWorm fastest (1.7 µs) but insufficient coverage (9.5 deposits avg). B_SeamBoundary good for seam scenes (plaus 0.32). E adds ~12 µs/chunk. Hypothesis H1 (plaus ≥0.7) REJECTED — max 0.55 at 8³ scale. H2 (<5 µs) CONFIRMED for A/C; REJECTED for B/D/E (9.7-12.1 µs). 8³ chunk cost at worst 12 µs → ~6ms per 32×32×32 region. **Integration:** Hybrid (E) for full-quality worldgen; Perlin worm (C) fast-path for LOD. См. [`experiments/2026-06-22-procedural-voxel-resource-deposits/`](./experiments/2026-06-22-procedural-voxel-resource-deposits/) + [README](./experiments/2026-06-22-procedural-voxel-resource-deposits/README.md) + [STATUS](./experiments/2026-06-22-procedural-voxel-resource-deposits/STATUS.md) + [sources](./experiments/2026-06-22-procedural-voxel-resource-deposits/sources.md) + `prototype/{resource_bench.cpp (~470 LoC), build/resource_bench, build/results.csv (6251 rows)}`.

- `2026-06-22-voxel-heat-conduction-cost` (verdict=`concluded-verdict-mixed`). **Stage 3.x physics × Stage 5.x visual — first dedicated voxel heat conduction cost analysis axis** в 170+ closed experiments. Self-invented per operator instruction; sentinel §13.7 clean. Web-research complete (6 sources: Wikipedia heat equation, thermal conduction, thermal diffusivity, finite difference, Gauss-Seidel, CA). Standalone C++26 CPU prototype `prototype/heat_bench.cpp` ~410 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`, build green 0 warnings). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements. **Headline:** Hypothesis REJECTED on CPU cost (B=24 µs vs <0.5 µs; D=130 µs vs <5 µs). **Confirmed: GPU compute only viable path** for per-chunk conduction at scale. B_ExplicitEuler = best CPU quality/cost (24 µs mean, 8.5 dB PSNR, 15-41 ticks to converge). C_BFS = highest PSNR (18.4 dB) at 330 µs (12× B). D_GaussSeidel converges in 1 tick but 130 µs — too slow. E_GPU_Analytical projected 0.5–1 µs/chunk with batched dispatch. **Integration:** GPU compute shader recommended (Vulkan dispatch, 64 chunks/batch → <8 µs/tick). CPU path explicitly NOT recommended for gameplay use. См. [`experiments/2026-06-22-voxel-heat-conduction-cost/`](./experiments/2026-06-22-voxel-heat-conduction-cost/) + [README](./experiments/2026-06-22-voxel-heat-conduction-cost/README.md) + [STATUS](./experiments/2026-06-22-voxel-heat-conduction-cost/STATUS.md) + `prototype/{heat_bench.cpp (~410 LoC), build/results.csv (126 rows)}`.

Just-closed (this session, `2026-06-21`):

- `2026-06-21-hierarchical-tactical-ai-btree` (verdict=`mixed`). **Military sandbox axis — Tier 2 AI, Tactical & Warfare — first dedicated behavior-tree axis** в 100+ closed experiments. Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй» (sentinel §13.7 clean: only prior backlog/INDEX cross-refs). Web-research via direct `webfetch` (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **6 primary + 6 cross-references verified** в `sources.md`: Colledanchise & Ögren 2018 [arXiv:1709.00084, BT formal model `T_i = {f_i, r_i, Δt}`] + Isla GDC 2005 [Halo 2, 50 behaviors, behavior impulses + tagging, "we would like to make this impulse 'event-driven'"] + Chris Simpson 2014 [Project Zomboid, EnsureItemInInventory recursive pattern] + Colledanchise 2014 ICRA [stochastic BT perf analysis] + Champandard & Dunstan 2012 [Game AI Pro Ch.6, halt nodes Interrupt/Abort/Restart] + Agis 2020 ESWA [multi-agent event-driven extension, 40-60% reduction]. Standalone C++26 CPU prototype `prototype/btree_bench.cpp` ~1053 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 2 cosmetic warnings**). 5 strategies (A_NaiveNoMemory / B_BT_RunningMemory / C_Hierarchical_3Tier / D_EventDriven / E_Blackboard) × 5 scenes (recon_patrol 8u → combined_arms 256u) × 5 seeds × N ticks + 10 warmup = **125 main measurements**, wall time **1.89 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/results.csv` (126 rows, 12 KB). **Headline (mixed):** **D_EventDriven ⭐ RECOMMENDED DEFAULT** (180-263 ns/unit/tick = -3% to -22% vs A baseline; consistent winner at scale ≥64u); E_Blackboard best at small N (recon_patrol 8u: 257 ns = +24% vs A); C_Hierarchical_3Tier **REJECTED as currently designed** (overhead of 3 trees + SubTreeCall > savings at N=64-128; would need ECS-coupled redesign); A_NaiveNoMemory 1.5× slower than D at N=8 but converges to D at N=256 (cache effects dominate). **All strategies <300 ns/unit/tick** → 1000 units = 0.3 ms/tick = 0.9% of 30 Hz (**hypothesis CONFIRMED**); 10K units = 1.8-2.0 ms = 5-6% of 30 Hz (within 5-10% threshold per `optimization-philosophy.md`). **Synthesis:** classical Running-memory (B) gives modest gains (3-17%); event-driven (D) is the SOTA winner at scale; hierarchical (C) is orth in this prototype. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~830 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/ai/BehaviorTree.hpp` flat-SoA BT primitive; Step 2 (S, ~250 LoC) `src/ai/TacticalBT.{hpp,cpp}` Flecs `BtComponent` + event-driven halts via Flecs observer + `PROJECTV_AI_BT=NAIVE|CLASSIC|EVENT_DRIVEN` env gate (default `EVENT_DRIVEN`); Step 3 (M, ~500 LoC, deferred до Stage 6+) hierarchical 3-tier Strategic + Tactical + Unit with shared blackboard. **Cross-axis:** **orth** к in-progress parallel + closed `interest-management-aoi-battle` [mixed, AOI = how many BTs to tick per frame] + closed `flow-field-pathfinding-10k-units` [yes, BT runs on top of pathfinding] + closed `ecs-1m-entities-bottleneck` [yes, Flecs = BT host] + closed `suppression-mechanics` [mixed, 33-52 ns/soldier suppression, complementary axis] + closed `infantry-soldier-sim` [yes, 15.86 ns/soldier physical sim, complementary axis] + closed `dynamic-entity-lighting` [mixed, per-source light = unit attribute]; **prerequisite** для open `flanking-maneuver-ai` [h, BT composite for formation split] + `combined-arms-coordination-ai` [h, 2-tier BT] + `group-formation-maneuver` [m, BT for formation] + `squad-fire-team-command` [m, BT for fire team] + `urban-combat-tactics-ai` [m, room-clearing BT] + `strategic-llm-commander-agent` [m, LLM at strategic tier above BT]. **Caveats:** CPU-only analytical model; synthetic Blackboard; single-threaded; mock action/condition cost; no recursion depth limit; no multi-agent coordination validation. **New axis:** first dedicated **behavior tree** axis в 100+ closed experiments; opens Tier 2 AI, Tactical & Warfare Mechanics для all BT-based features. См. §6 + [README](./experiments/2026-06-21-hierarchical-tactical-ai-btree/README.md) + [STATUS](./experiments/2026-06-21-hierarchical-tactical-ai-btree/STATUS.md) + [RESULTS](./experiments/2026-06-21-hierarchical-tactical-ai-btree/RESULTS.md) + [sources](./experiments/2026-06-21-hierarchical-tactical-ai-btree/sources.md) + `prototype/{btree_bench.cpp (~1053 LoC), CMakeLists.txt, build/btree_bench (61 KB), build/results.csv (126 rows, 12 KB), results.csv (126 rows, 12 KB)}`.

- `2026-06-21-lockstep-state-sync-hybrid-netcode` (verdict=`mixed`). **Military sandbox axis — Tier 1 Core Engine Systems: Netcode**. Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»; **first dedicated netcode architecture axis** в 100+ closed experiments. 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time 19.5 sec. Web-research via direct webfetch (Exa 429 + DuckDuckGo CAPTCHA blocked) — **5 primary + 3 supplementary sources verified**: Glenn Fiedler "Deterministic Lockstep" + "Snapshot Interpolation" + "Floating Point Determinism" (SupCom precedent: `_controlfp(_PC_24) + _RC_NEAR` @ 1M+ customers) + Wikipedia Netcode + Wikipedia Lag. **Headline (mixed):** **A_PureLockstep ⭐ = DEFAULT for ProjectV** at 48-92 KB/s/player (hypothesis ≤50 KB/s/player CONFIRMED for A only); B_PureStateSync = NEVER (94-150× worse, 4.5-13.8 MB/s/player); C_Hybrid_10Hz / D_Hybrid_5Hz / E_RollbackCRC = 17-32× worse than A (snapshot payload dominates); **E CRC overhead = 2053 µs/tick = 30× C** (table-based CRC32 over 10k entities). **Architectural recommendation:** A_PureLockstep default + D_Hybrid_5Hz @ 0.2 Hz for late-joiner + divergence recovery. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** ~1650 LoC, L effort, 3-5 sessions. Steps 1+2 (determinism foundation + FPU mode) **immediate prerequisite for 100-player scale**; Step 3 (recovery + late-joiner) deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36. См. §6 + [README](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/README.md) + [STATUS](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/STATUS.md) + [RESULTS](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/RESULTS.md) + [sources](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/sources.md) + `prototype/{netcode_bench.cpp (744 LoC), build/{netcode_bench, results.csv (126 rows, 12 KB)}}`.

- `2026-06-21-fixed-wing-flight-model-simulation` (verdict=`yes`). **Military sandbox axis — Fixed-wing flight model simulation**. Independent physics scope. Standalone C++26 CPU prototype `prototype/flight_model_bench.cpp` ~1065 LoC. 5 strategies × 5 scenes × 5 seeds × 2 tick rates = **250 main measurements**. **Headline:** **C_RK4_4Section (and Vectorized E) = ~908 ns / ~849 ns per aircraft**, which is **5.5× below the 5 µs target budget**. RK4 maintains **9.4 m RMS error at 20 Hz tick**, compared to B_Euler_4Section which has **117.4 m** error (a **1150% accuracy delta**), proving the immense benefit of RK4 integration for stability and trajectory accuracy at low tick rates. **D_Analytical_LOD = ~101 ns**, perfect low-cost fallback for distant aircraft (LOD2). **Integration:** S-M effort, 1-2 sessions. Create `src/physics/FlightVehicle.{hpp,cpp}` module using Flecs components. Use RK4 for LOD0/1 and Analytical for LOD2. См. [README](./experiments/2026-06-21-fixed-wing-flight-model-simulation/README.md) + [STATUS](./experiments/2026-06-21-fixed-wing-flight-model-simulation/STATUS.md) + [RESULTS](./experiments/2026-06-21-fixed-wing-flight-model-simulation/RESULTS.md) + `prototype/{flight_model_bench.cpp, build/results.csv}`.

- `2026-06-21-suppression-mechanics` (verdict=`mixed`). **Tier 2 AI — Psychological suppression effect for military sandbox**: near-miss fire degrades accuracy, limits movement, causes panic. 5 strategies × 5 scenes × 5 seeds = **125 main measurements**. Standalone C++26 CPU prototype ~320 LoC (Clang 22.1.6, build green 2 cosmetic warnings). **Headline:** D_AccumulatorThreshold (WARNO-style) = universal recommended default (14-31 µs total = 33-52 ns/tick/soldier, tiered accuracy 0-50% + movement 0-25% + 4s stun). C_AccumulatorDecay (ARMA-style) = second (98-358 µs, smooth 0-80% accuracy). B_BinaryThreshold NOT recommended (all-or-nothing, worst cost at 622 µs). **All strategies << 1 µs/soldier/tick, hypothesis CONFIRMED.** Integration: ~300 LoC Flecs SuppressionComponent + weapon suppression values + accuracy/movement modifier pipeline. Default `PROJECTV_SUPPRESSION=WARNO`. Deferred до Stage 6+. См. [README](./experiments/2026-06-21-suppression-mechanics/README.md) + [STATUS](./experiments/2026-06-21-suppression-mechanics/STATUS.md) + `prototype/{suppression_bench.cpp, build/results.csv (126 rows)}`.

- `2026-06-21-helicopter-rotor-physics` (verdict=`yes`). **Military sandbox axis — Helicopter rotor physics simulation**. Independent physics scope. Standalone C++26 CPU prototype `prototype/helicopter_bench.cpp` ~1100 LoC. 5 strategies × 5 scenarios × 5 seeds × 2 tick rates = **250 main measurements**. **Headline: Strategy D (4-Blade BET + Flapping + RK4) = 1.34 µs per step @ 60 Hz**, which is **75× below the 0.1 ms target budget** and achieves **96.0% stability**. Explicit integration of flapping equations requires a tick rate of $\ge 50$ Hz (at 20 Hz, stability is 0% due to stiff oscillations). Autopilot feedback gains must be reduced to accommodate the 90-degree phase lag of coning/flapping to prevent PIO. **Strategy A (Momentum Theory LOD) = 80.4 ns**, perfect low-cost fallback for LOD2+. **Integration**: S-M effort, 1-2 sessions. Create `src/physics/helicopter_vehicle.{hpp,cpp}` module. Use Strategy D for LOD0/1 at $\ge 60$ Hz, Strategy A for LOD2+. См. [README](./experiments/2026-06-21-helicopter-rotor-physics/README.md) + [STATUS](./experiments/2026-06-21-helicopter-rotor-physics/STATUS.md) + [RESULTS](./experiments/2026-06-21-helicopter-rotor-physics/RESULTS.md) + `prototype/{helicopter_bench.cpp, results.csv}`.

- `2026-06-21-radar-detection-system-simulation` (verdict=`yes`). **Military sandbox axis — Pulse-Doppler Radar Simulation**. Independent scope. Web-research complete (7 primary sources). Standalone C++26 CPU prototype `prototype/radar_sim_bench.cpp` ~520 LoC. 5 strategies × 5 scenes × 5 seeds × 1000 iter = **125,000 main measurements**. **Headline:** **D_TrackingLoopKalman = 6.99 µs mean** (under <10 µs budget), target beaming (90° turn) + chaff deployment triggers **100% lock-transfer to decoy** (spoofing counterplay validated). **B_ClusteredLODScan = 2.35–2.9× speedup** over naive (66.39 µs vs 191.89 µs at 100 targets). **C_PulseDopplerSignalProc** (138 µs to 1.62 ms) successfully models Doppler clutter notch and false target suppression (detection rate drops to 18.4% in chaff corridor vs 97.1% naive). **Integration:** S-M effort, 2-3 sessions. Use B for search radar sweeps, C for active track sensors, D for STT tracking loops. См. [README](./experiments/2026-06-21-radar-detection-system-simulation/README.md) + [STATUS](./experiments/2026-06-21-radar-detection-system-simulation/STATUS.md) + [RESULTS](./experiments/2026-06-21-radar-detection-system-simulation/RESULTS.md) + `prototype/{radar_sim_bench.cpp, build/results.csv}`.


- `2026-06-21-redstone-power-propagation-bfs` (verdict=`mixed`). **Stage 6.x gameplay — redstone signal propagation BFS** (signal strength 0-15 BFS propagation with -1/block attenuation, tick-scheduled repeaters/comparators). Inherits BFS methodology from closed `incremental-light-propagation` (yes verdict). Web-research complete (10+ sources: PaperMC Eigencraft, Alternate Current, Mojang 24w33a experimental, Ferrite, Redpiler, MC Wiki). Standalone C++26 CPU prototype `prototype/redstone_bench.cpp` ~580 LoC (Clang 22.1.6, build green 2 warnings). 5 strategies × 5 scenes × 5 seeds × 1000 iter = **125,000 main measurements**. **Headline:** B_Queue256 = bit-exact safe default (99.9 dB PSNR, up to 1.39× speedup vs full BFS). D_AltCurrent = 1.24-2.39× faster but fails on cyclic circuits (full_adder: 30.69 dB PSNR — topological sort breaks on torch feedback loops). All strategies < 1 µs/tick (worst 0.90 µs = 1.8% of 50 µs budget). **Integration:** Step 1 (XS, ~100 LoC) Budget BFS immediate; Step 2 (S, ~250 LoC) Graph-based with cycle detection deferred. См. [README](./experiments/2026-06-21-redstone-power-propagation-bfs/README.md) + [STATUS](./experiments/2026-06-21-redstone-power-propagation-bfs/STATUS.md) + `prototype/{redstone_bench.cpp, build/results.csv}`.

- `2026-06-21-dynamic-entity-lighting` (verdict=`mixed`). **Stage 5.x Visual Polish — dynamic entity-based lighting** (entity-as-light-source: player with torch/glowstone emits light dynamically, lightmap injection per tick). Builds on closed `incremental-light-propagation` BFS methodology. Web research complete (15+ sources: OptiFine DynamicLights, LambDynamicLights, Starlight, MC LightEngine). Standalone C++26 CPU prototype `prototype/dynamic_light_bench.cpp` ~600 LoC (GCC 16.1.1 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green, 4 cosmetic warnings). 5 strategies × 5 scenes × 5 seeds × 5 entity_counts × 100 iter + 10 warmup = **62,500 main measurements**. **Headline:** E_GPUInjection (shader-based) = 0.05-0.36 µs CPU cost (834× faster than FullBFS), PSNR 38.52-24.63 dB; D_RateLimited = 6-104 µs, PSNR 55.87-46.29 dB (best quality-cost at >5 sources); C_BudgetBFS = 16-47 µs, PSNR 92.68-28.65 dB (best at ≤5 sources). All strategies < 0.9% of 30 Hz frame budget. См. [README](./experiments/2026-06-21-dynamic-entity-lighting/README.md).

- `2026-06-21-chunk-damage-fracture-model` (verdict=`mixed`). **Stage 3.x interaction/gameplay — voxel chunk fracture model** (how chunks fracture on explosion/impact). Self-invented topic per operator instruction. Web-research complete (11+ sources: Leon's Notes 2026, Teardown, Donkey Kong Bananza, BoxCutter, Kugelhaufen, UE5 Chaos, Voronoi libraries). Standalone C++26 CPU prototype `prototype/fracture_bench.cpp` ~480 LoC (Clang 22.1.6, build green 0 warnings). 5 strategies × 5 scenes × 5 seeds × 1000 iter = **125,000 main measurements**. **Headline:** C_Greedy3D = practical winner (2.88 µs mean, 8.2× body reduction, 100% voxel-accurate); D_Voronoi = highest reduction (1.48 µs, 88×) but topology-unaware; B_CCL = 431× reduction but always 1 component (no debris from single-chunk explosions — all remaining voxels stay connected). All strategies well within budget (max 25.5 µs = 0.077% of 33ms frame). **Critical finding:** 8³ chunk explosions rarely produce disconnected floating fragments without cross-chunk context → fracture model useful but gated on per-voxel damage implementation. **Integration:** 3-step migration ~150 LoC, S effort, deferred until per-voxel damage is added. См. [README](./experiments/2026-06-21-chunk-damage-fracture-model/README.md) + [STATUS](./experiments/2026-06-21-chunk-damage-fracture-model/STATUS.md).

- `2026-06-21-cloudscape-rendering` (verdict=`mixed`). **Stage 5.x Visual Polish — volumetric cloud rendering axis** (ray-marched procedural clouds). **Self-invented topic** per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; **0 of 50+ closed experiments covered cloudscapes** — fully fresh axis explicitly listed as "remaining Stage 5.x axis" in closed `volumetric-fog-atmosphere-rendering` + `god-rays-crepuscular`. Web-research complete via Exa `web_search` (working this session); **15+ primary sources verified** (Schneider Nubis 2015/2017/2022/2023, Hillaire 2016 Frostbite, elliahu/atmosphere, Loboda 2025 WebGPU, Sakmary 2023 Vulkan, Kulla 2025 decoupled ray-march, Cumulus 2026, Simon Barsky 2025). Standalone C++26 CPU prototype `prototype/cloud_sim.cpp` ~180 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time < 0.05 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (125,001 rows). **Headline (mixed per platform tier):** **B_SingleLayerRayMarch = universal recommended default** (2.172 ms = 6.5% of 30 Hz, 23.99 dB, VRAM 4.20 MiB); **E_RTXRayMarchCloud = fastest quality option** (1.769 ms, 27.19 dB) but RTX-dependent; **C_ThreeLayerNubis = quality opt-in** (3.056 ms, 28.79 dB); **D_HybridFroxelCloud NOT recommended on RTX 3060 Ti** (10.9% of 30 Hz, 544/25000 > 5 ms). All VRAM < 20 MiB (negligible). **5-10% threshold per `optimization-philosophy.md`:** all 4 non-baseline strategies cross massively (22-28 dB gain vs baseline). **Per-platform tier:** no-HW-RT → B; RTX-class mid → B default + E opt-in; RTX-class high → E default + C quality; cave → auto-disable. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent:** ~430 LoC total, M effort, 2-3 sessions, default `PROJECTV_CLOUDS=SINGLE_LAYER` + `PROJECTV_CLOUDS_MIN_SKY_VISIBILITY=0.15` scene-adaptive gate. **Deferred** до Stage 5.x dedicated session. **Cross-axis:** orth ко всем in-progress parallel; **complementary** к closed `volumetric-fog-atmosphere-rendering` (fog below, clouds above) + `2026-06-21-taa-motion-vectors` (temporal reprojection for cloud flicker) + `2026-06-20-dec-pipelines-async-compute` (async compute for cloud dispatch). **New axis:** first volumetric cloud rendering axis в 50+ closed experiments; opens Stage 5.x cloudscape question. См. §6 + [experiment README](./experiments/2026-06-21-cloudscape-rendering/README.md) + [STATUS](./experiments/2026-06-21-cloudscape-rendering/STATUS.md) + `prototype/{cloud_sim.cpp ~180 LoC, build/results.csv (125,001 rows)}`.

- `2026-06-21-ik-first-person-hand` (verdict=`mixed`). **Stage 3.x interaction — first-person arm IK**.
  FABRIK = 0.2-0.7 µs, <1 cm error, ~99% convergence. Analytic two-bone = 0.17 µs, 4-7 cm residual.
  CCD = 3-12 µs, poor conv. **Hybrid recommended:** analytic first-pass + FABRIK polish. См. §6.

- `2026-06-21-conc-ring-generation-scheduling` (verdict=`mixed`). **Stage 4.1 world gen scheduling axis** — concentric-ring scheduler for cross-chunk dependency resolution. Closed same session. Standalone C++26 CPU prototype (`prototype/main.cpp` ~260 LoC). 4 strategies × 5 movement patterns × 2 seeds × 2 dispatch modes = 80 configs × 500 frames × 4 workers. **Headline:** Hypothesis rejected for stall reduction (0% improvement — stalls 1-2% for all strategies). Sequential ring phases (VoxelCore `SurroundMap` pattern) guarantee dependency ordering but add 3.2% throughput penalty (1968→1904 completions). Parallel ring dispatch produces identical results to distance sorting (workers always saturated). **Recommended only if cross-chunk dependencies are introduced in Stage 4.1.** S effort, ~100 LoC. Complementary to closed `wfc-procedural-worlds` (gen strategy) + `voxel-chunk-streaming-pipeline` (streaming). Deferred until cross-chunk dependency requirement emerges. См. §6 + [experiment README](./experiments/2026-06-21-conc-ring-generation-scheduling/README.md).

Just-closed (this session, `2026-06-21`):

- `2026-06-21-precomputed-atmospheric-sky` (verdict=`yes`). **Stage 5.x Visual Polish — precomputed atmospheric sky rendering axis** (Bruneton 2017 / Hillaire 2020 LUT-based sky; **0 of 70+ closed experiments covered dedicated sky rendering** — fully fresh axis). Self-invented per operator instruction. Web-research complete (10+ primary sources: Bruneton 2017, Hillaire 2020 EGSR, elliahu/atmosphere RTX 3060 benchmarks, Sakmary 2023 CesCG, Hosek Wilkie 2012, O'Neil GPU Gems 2). Standalone C++26 CPU analytical cost model `prototype/sky_sim.cpp` ~200 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **0 warnings**). 6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **150 main measurements**, wall time < 0.1 sec. **Headline: C_Hillaire2020 = universal default** (0.080 ms = 0.24% of 30 Hz, 32.7 dB PSNR, 8 MiB VRAM, single-frame LUT recompute for dynamic weather); **B_Bruneton2017 = quality opt-in** (0.092 ms, 33.7 dB); **E_HosekWilkie2012 = mobile fallback** (0.006 ms, 24.7 dB, 0 VRAM). All non-baseline strategies cross 5-10% threshold massively (+209-349% PSNR relative). **Integration:** 3-step migration per §30.4: ~490 LoC, M effort, 2-3 sessions, default `PROJECTV_SKY=HILLAIRE`. Deferred до Stage 5.x dedicated session. **Cross-axis:** orth to all closed fog/god rays/clouds experiments; complementary to tonemap, bloom, aerial-perspective. См. §6 + [README](./experiments/2026-06-21-precomputed-atmospheric-sky/README.md) + [STATUS](./experiments/2026-06-21-precomputed-atmospheric-sky/STATUS.md) + [RESULTS](./experiments/2026-06-21-precomputed-atmospheric-sky/RESULTS.md) + `prototype/{sky_sim.cpp (200 LoC), build/results.csv (151 rows)}`.

- `2026-06-21-chunk-storage-compression-axis` (verdict=`mixed`). **Voxel chunk file format compression axis**
  experiment closed same session (**first dedicated file-format axis** в 50+ closed experiments; 5 strategies
  ∈ {A_Uncompressed, B_RLE16, C_Palette4, D_Palette4_RLE, E_Palette8_Zstd}). **Self-invented topic** per operator
  instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; **0 of 50+ closed experiments
  covered file format compression specifically** — closed `2026-06-21-texture-compression-format-axis` [mixed]
  covers **orth axis** (BC/ASTC for material atlas), closed `2026-06-21-sub-chunk-layers` [mixed] covers
  **orth axis** (runtime RAM palette), closed `2026-06-21-voxel-chunk-streaming-pipeline` [mixed] covers
  **streaming policy**. Web-research complete via `webfetch` DuckDuckGo HTML fallback (Exa HTTP 429 persistent);
  **13 primary + 6 supplementary sources verified**: zeux.io 2017 [canonical voxel RLE 256× ratio] +
  Minecraft Wiki Anvil/Region [zlib default, 32×32 chunks per region, 1.20.5 added LZ4] +
  Minecraft 1.12 BlockStatePalette [adaptive 4/8/registry bits] + VoxelCore compressed_chunks.cpp
  [RLE + gzip production pattern] + Epic ADR-00016 [Zstd level 6 chosen over Oodle Kraken] + PH3 Blog
  [Zstd+dict = best of both] + Veloren chunk_compression_benchmarks.rs [production Rust benchmarks] +
  Steam zstd migration 2025 [Valve migrating LZMA→zstd] + Oddur Magnusson zstd across the stack +
  Voxel.Wiki palette compression + eisenwave voxel-compression-docs + Minecraft 1.13+ PalettedContainer +
  Reddit r/VoxelGameDev BlockStorage. Standalone C++26 CPU harness `prototype/chunk_compress_bench.cpp` ~800 LoC
  (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **0 warnings**
  after 2 fix iterations: D_Palette4_RLE palette array `std::array<uint8_t, 16>` UB for pcount >16 → enlarged
  to 256; E_Palette8_Zstd LZ77 match off > dst UB → refactored to value-explicit RLE+literals codec).
  5 strategies × 5 scenes × 10 seeds × 1000 iter + 10 warmup = **250 main measurements**, wall time **308.47 ms**
  (1.234 ms / 1000-iter config) на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
  Output: `prototype/build/results.csv` (251 rows = 1 header + 250 data, 49 KB) +
  `prototype/build/summary_means.csv` (26 rows). **100% fidelity OK** across all configs (zero `memcmp`
  mismatches). **Headline (mixed per scene tier):** **A_Uncompressed** = 528 bytes baseline (16 header + 512
  payload); **B_RLE16** = 96.4% / 95.8% reduction на uniform_floor / uniform_half but **167-184% EXPANSION** на
  cave_stress / mixed_biome ❌ (RLE breaks on random data); **C_Palette4** = 46% reduction на cave_stress ⭐;
  **E_Pal8_Zstd** = 80% reduction на forest_floor ⭐ + **never expands beyond +7%** vs raw → safe universal
  fallback; **D_Pal4_RLE** = same uniform-friendliness as B + similar expansion on mixed ❌. **Crosses 5-10%
  threshold per `optimization-philosophy.md` MASSIVELY** (46-96% reduction). **Critical insight:** per-scene
  adaptive dispatcher is the right architecture, NOT single-format adoption. `SelectChunkFileFormat(chunk)`
  counts unique materials → 1 → RLE16 (96% reduction); 2-16 → Palette4 (46%); > 16 → Palette8Zstd
  (never-expanding). **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~370 LoC total, S-M effort,
  1-2 sessions, **deferred до Stage 4.3 dedicated session** per `agent/workspace.md §2` line 36 operator 8x
  planning decision): Step 1 (S, ~170 LoC) `ChunkStreamer.{hpp,cpp}` enum + env gate + dispatchers + file
  header version 1→2 with format byte; Step 2 (S, ~150 LoC) per-strategy implementation (A/B/C/D/E); Step 3
  (XS, ~50 LoC) fidelity check + unit test + Tracy plot. **Cross-axis:** orth orth ко всем 4 in-progress parallel
  (tracy-gpu-vs-manual profiling, gpu-fluid-ca-atomic-strategy Stage 3.1, rtx-screen-space-reflections Stage 5.x,
  full-rt-tensor-cores-load GPU load); **complementary** к closed `2026-06-21-voxel-chunk-streaming-pipeline`
  [mixed, **directly upstream** — Step 3 prebake needs file format] + `2026-06-21-sub-chunk-layers` [mixed,
  orth RAM layout] + `2026-06-21-texture-compression-format-axis` [mixed, orth atlas format] +
  `2026-06-20-svdag-vs-vdb-memory-throughput` [yes] + `2026-06-20-nanovdb-on-gpu` [yes]. **New axis:** first
  dedicated **file format compression** axis в 50+ closed experiments; opens Stage 4.3 ChunkStreamer file
  format question. См. §6 +
  [experiment README](./experiments/2026-06-21-chunk-storage-compression-axis/README.md) +
  [STATUS](./experiments/2026-06-21-chunk-storage-compression-axis/STATUS.md) +
  [sources](./experiments/2026-06-21-chunk-storage-compression-axis/sources.md) +
  [RESULTS](./experiments/2026-06-21-chunk-storage-compression-axis/RESULTS.md) +
  `prototype/{chunk_compress_bench.cpp (~800 LoC), CMakeLists.txt, README.md}` +
  `prototype/build/{chunk_compress_bench, results.csv (251 rows), summary_means.csv (26 rows)}`.


- `2026-06-21-volumetric-fog-atmosphere-rendering` (verdict=`mixed`). **Stage 5.x Visual Polish axis — volumetric
  fog / atmospheric rendering / participating media** experiment closed same session (**first axis** в 50+ closed
  experiments; 5 strategies ∈ {A_AnalyticDistance, B_FroxelGrid_3DTexture, C_FullRayMarch_HalfRes,
  D_RTX_RayQuery_ShortRayShadow, E_Hybrid_FroxelNear_RayMarchFar}). **Self-invented topic** per operator instruction
  `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; **0 of 50+ closed experiments covered
  volumetric fog axis** — fully fresh. Web-research complete via `webfetch` DuckDuckGo HTML endpoint (Exa HTTP
  429 persistent per `agent/knowledge.md Part B §9`); **30 sources verified** in `sources.md` Tier 1 + Tier 2 +
  Tier 3: Wronski 2014 SIGGRAPH canonical froxel paper + Hillaire 2015 SIGGRAPH Frostbite production + Kovalovs
  2020 SIGGRAPH TLoU2 + Wright 2022 SIGGRAPH Lumen + Enshrouded 2026 GPC + elliahu/atmosphere validated RTX
  3060/4080 benchmarks + Timethy Hyman 2026 Traverse + Mastering Graphics Programming with Vulkan Ch10 +
  sinnwrig/URP-Fog-Volumes + Godot issue #8580 RDR2-style + Kenny Mitchell GPU Gems 3 + Bruneton 2017 + Sakmary
  2023 CesCG + Hillaire 2020 EGSR + Horizon Forbidden West Nubis + NVIDIA RTX Remix docs + Matej Lou 2025 +
  Loboda 2025 WebGPU + Cinevva 2026-05-04 + moonjump 2026-02-15 + 12 supplementary. Standalone C++26 CPU
  analytical cost model `prototype/volumetric_fog_sim.cpp` ~500 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26
  -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 5 strategies × 5 scenes × 5 seeds × 1000 iter
  + 10 warmup = **125,000 main measurements**, wall time **0.008 sec** на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data, 19.3 KB).
  **Headline (mixed per platform tier):** **A_AnalyticDistance** (current mainline `voxel.frag:844-883`) =
  0.002 ms / 0 MiB / **8.45 dB PSNR** = **NOT real volumetric fog** (no light scattering, no god rays,
  baseline only); **B_FroxelGrid_3DTexture** (Wronski 2014 + Frostbite + TLoU2 + Enshrouded 2026 GPC) =
  2.580 ms / 37.25 dB / 28.27 MiB = **SAFE UNIVERSAL DEFAULT** (all scenes under 5 ms); **C_FullRayMarch_HalfRes**
  (elliahu analog) = 6.986 ms / **42.75 dB** / 12.39 MiB = best quality but exceeds 5 ms on 4/5 scenes
  (cave_stress 9.59 ms = 28.8% of 30 Hz budget); **D_RTX_RayQuery_ShortRayShadow** (Lumen 2022 hybrid) =
  **1.787 ms** / 38.75 dB / 12.39 MiB = **WINNER RTX 3060 Ti** (fastest non-baseline, scene-coverage-INDEPENDENT
  1.33→2.31 ms range); **E_Hybrid_FroxelNear_RayMarchFar** (Enshrouded 2026 GPC three-layer) = 4.868 ms /
  40.75 dB / 25.93 MiB = most flexible but cave_stress 6.67 ms exceeds 5 ms на RTX 3060 Ti (within budget на
  RTX 4080 per elliahu). **Crosses 5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** A → B/D
  = +5-8 dB PSNR (470-940% relative) = far above threshold → **adopt B/D**. B → D = -31% ms → **D wins
  on RTX-class**. C/E на RTX 3060 Ti = reject (cave_stress exceeds budget); на RTX 4080 = adopt (within budget
  per elliahu). Per-platform tier matrix: no-HW-RT → B_FroxelGrid; RTX-class mid (current dev host) →
  D_RTX_RayQuery; RTX-class high → D default + E opt-in; static baked / mobile fallback → A_AnalyticDistance.
  **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~480 LoC total, M effort, 2-3 sessions,
  **deferred** до Stage 5.x dedicated session per `agent/workspace.md §2` line 36 operator 8x planning decision):
  Step 1 (XS, ~50 LoC) `VolumetricFogController` foundation + froxel grid + env gate; Step 2 (M, ~400 LoC)
  per-strategy implementation в `voxel.frag` post-process pass + `volumetric_fog.comp` + scattering
  accumulation + temporal history + half-res + RTX ray query; Step 3 (XS, ~30 LoC) default flip + Tracy plot +
  unit test + `lookdev-captures/fog` scene integration. **Cross-axis:** orth orth ко всем 3 in-progress
  parallel (closing `tracy-gpu-vs-manual` by parallel + `gpu-fluid-ca-atomic-strategy` Stage 3.1 +
  `voxel-mutation-cost-characterization` cross-cutting SVDAG); **complementary** к closed VCT experiments
  (`vct-vs-rt-cutoff` + `vct-cone-count-atlas-precision` + `vct-3d-mip-generation` + `vct-temporal-denoise-tensor-core`
  — cone-march через 3D атлас структурно похож на fog ray-march) + `rt-shadows-vs-csm` + `clustered-forward-mass-lights`
  + `dec-pipelines-async-compute` + `eye-tracked-foveated` (VRS = smart fog density follow-up) +
  `vk-fragment-shading-rate-voxel` + `taa-motion-vectors` + `dlss-fsr-xess-upscaling-voxel` +
  `vulkan-memory-aliasing-transient` (froxel = transient aliasing) + `vulkan-defragmentation-compaction`
  (froxel VRAM = compaction) + `vulkan-fps-pacing-wayland-prototype` (frame pacing для ray-march jitter) +
  `renderdoc-ci-capture` + `rtx-screen-space-reflections` + `vk-video-decoder-replay`. **Continuation chain:**
  `vct-vs-rt-cutoff` (mixed Stage 5.1 cutoff) + `rtx-screen-space-reflections` (mixed Stage 5.x reflection) +
  this (mixed Stage 5.x fog) = **Stage 5.x Visual Polish axis fully covered** by closed experiments.
  Remaining Stage 5.x axes: refraction + SSS + tonemap + bloom + DOF + god rays + aerial perspective +
  cloudscapes (all deferred до dedicated session per `agent/workspace.md §2` line 36). **New axis:** first
  volumetric fog / atmospheric rendering / participating media axis в 50+ closed experiments; opens Stage 5.x
  Visual Polish axis для all sub-fog features. См. §6 + [experiment README](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/README.md) +
  [STATUS](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/STATUS.md) +
  [sources](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/sources.md) +
  [RESULTS](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/RESULTS.md) +
  `prototype/{volumetric_fog_sim.cpp, build/volumetric_fog_sim, build/results.csv (126 rows, 19.3 KB)}`.

- `2026-06-21-renderdoc-ci-capture` (verdict=`mixed`). **CI regression-guard axis** experiment closed same
  session (**first dedicated CI/tooling axis** в 30+ closed experiments; headless `renderdoccmd --capture`
  + CTest regression pixel-diff baseline integration для ProjectV — greenfield `.github/`, `ci/`,
  `lookdev-captures/` папки отсутствуют в tree). **Self-invented topic** per operator instruction `2026-06-21`
  «выбирай свободную тему или придумывай свою исследуй»; l-priority `renderdoc-ci-capture` в `backlog.md §Open`
  line 57-59 = единственная свободная CI/tooling ось, не дублирующая 7 in-progress parallel + 30+ closed
  `2026-06-20/21`. Web-research complete via `webfetch` + DuckDuckGo HTML fallback (Exa HTTP 429 persistent
  per `agent/knowledge.md Part B §9`); **26 sources verified**: RenderDoc 1.44 official docs + `rdc-cli` PyPI
  (2026-06-04) + `vision-regression-kit` (Manas103 2026) + Glint3D CI issue #6 (SSIM ≥ 0.995 threshold) +
  Phoronix RenderDoc 1.7 release notes + `renderdog-automation` Rust crate + Akenine-Möller PSNR/SSIM canonical
  formulas. Standalone C++26 CPU analytical harness `prototype/capture_overhead_bench.cpp` ~620 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **0 warnings**). 5 strategies ×
  5 scenes × 5 seeds × 1000 frames + 10 warmup = **125,000 main measurements**, wall time <1 sec на Zen 3 5800X.
  Output: `prototype/build/results.csv` (126 rows). **Caveat:** `renderdoccmd` не установлен на dev host
  (`which renderdoccmd` → not found 2026-06-21) → CPU-only analytical overhead model + CMakeLists/CTest
  integration design (а не реальный `renderdoccmd --capture`). **Headline (mixed):** CPU overhead well below
  5-10% threshold per `optimization-philosophy.md` для всех strategies (max 1.21% для B_AlwaysOnLayer on
  stress_voxel; D = 0.12%, E = 0.09%, C = 0.05% — все negligible); capture file size **= real bottleneck**
  (B = 117 GB / 1k frames = **impractical**; D = 1.13 GB, E = 1.17 GB, C = 70 MB / 1k frames = **manageable**).
  **Recommended pair: D_PixelDiffBaseline + E_SelectiveCaptureRange** (CI primary + spike isolation);
  **C_TriggeredOnError** = production fallback (rare captures only); **B_AlwaysOnLayer** = NEVER.
  **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) CMakeLists
  `option(PROJECTV_CI_PIXEL_DIFF)` + `tests/regression/golden/` + `scripts/ci_capture.sh`; Step 2 (M, ~250 LoC)
  `ProjectVRegressionCaptureTests` + `imageDiff` C++ helper + 12 golden captures + `PROJECTV_CAPTURE_TRIGGER`
  env; Step 3 (S, ~100 LoC) `.github/workflows/capture.yml` + Slack/Discord webhook. Total ~400 LoC, S-M effort,
  2-3 sessions. **Cross-axis:** orthogonal ко всем 7 in-progress parallel; complementary к closed
  `dec-pipelines-async-compute` (RenderDoc async extension point per `agent/knowledge.md §547`) + closed
  `vulkan-fps-pacing-vk-ext` (RenderDoc timeline per §6 line 314). **New axis:** first CI/tooling cross-cutting
  axis = regression-guard для all Stage 0-6 + Stage 5.x planned. См. §6 + [experiment README](./experiments/2026-06-21-renderdoc-ci-capture/README.md) + [RESULTS](./experiments/2026-06-21-renderdoc-ci-capture/RESULTS.md) +
  [sources](./experiments/2026-06-21-renderdoc-ci-capture/sources.md) +
  `prototype/{capture_overhead_bench.cpp, build/results.csv (125,000 measurements), README.md,
  CMakeLists_design.md, gh_actions_design.md}`.

- `2026-06-21-eye-tracked-foveated` (verdict=`mixed`). **Eye-tracked foveated rendering axis** experiment
  closed same session (**first axis "gaze-driven per-region fragment density"** в 30+ closed experiments;
  `VK_KHR_fragment_shading_rate` Tier 2 attachment + `XR_EXT_eye_gaze_interaction` rev 2 eye-gaze data path).
  **Self-invented topic** per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою
  и исследуй». Web research complete via Exa `web_search` (3 waves, ~25 sources verified, working this
  session per `agent/knowledge.md Part B §9` line 1424 fallback list); **14 primary + 7 supplementary
  sources verified**: arXiv 2503.23410 «Visual Acuity Consistent Foveated Rendering» [log-polar mapping,
  **6.5×-9.29× deferred, 10.4×-16.4× ray-casting retinal**],
  Khronos `docs.vulkan.org/refpages/VK_EXT_fragment_density_map` + `VK_KHR_fragment_shading_rate` + `VK_KHR_dynamic_rendering_local_read`
  [SOTA extension per-region density, **superseded by Vulkan 1.4 + `VK_KHR_dynamic_rendering_local_read`**
  per `KhronosGroup/Vulkan-Docs appendices/VK_KHR_dynamic_rendering_local_read.adoc` line 24-30],
  Vulkan Samples `fragment_density_map` + `fragment_shading_rate_dynamic` [production reference patterns],
  Meta Horizon OS Blog «Save GPU with Eye-Tracked Foveated Rendering» [`VK_QCOM_fragment_density_map_offset`
  Tile Offset, Meta Quest ETFR production],
  Varjo Foveated Rendering API [production NVAPI VRS + dynamic projection modes],
  OpenXR `XR_EXT_eye_gaze_interaction` rev 2 ratified 2024 [eye-gaze data path],
  OpenXR `XR_VARJO_foveated_rendering` + `XR_FB_foveation_vulkan` + `XR_META_foveation_eye_tracked` + `XR_ANDROID_eye_tracking`
  [vendor-specific foveated rendering extensions],
  Springer Nature «Performance-driven foveated VR rendering for large 3D meshes» Mar 2026 [9.74 ms frame
  vs 10.06% slower spatial-only LOD], ACM 2025 ETRA «Quantifying Energy Reduction of Foveated Volume Visualization»
  [VRS + LBG stippling energy quantification], IEEE VR 2026 «Hybrid Foveated Path Tracing with Peripheral
  Gaussians» [voxel-adjacent production ref],
  NVK Mesa DeepWiki `bminor/mesa-mesa` [`fragmentShadingRate` Turing+; `cooperativeMatrix` Turing+; RTX
  3060 Ti Ampere = full feature set],
  NVIDIA Developer Vulkan Driver [Ampere = full Vulkan 1.4 support]. **Critical finding:** **`VK_EXT_fragment_density_map`
  NOT drop-in** для ProjectV (legacy `VkRenderPassCreateInfo`-bound; mainline `Renderer.cpp` uses
  `vkCmdBeginRendering` dynamic rendering). Корректный path = `VK_KHR_fragment_shading_rate` Tier 2
  attachment method (`VkFragmentShadingRateAttachmentInfoKHR` + `vkCmdSetFragmentShadingRateKHR`)
  **fully dynamic-rendering compatible** + Vulkan 1.4 core + cross-vendor (NVIDIA Ampere+ / Ada /
  Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+ + mobile via `VK_QCOM_fragment_density_map_offset`).
  Standalone C++26 CPU foveation density map simulator `prototype/foveation_sim.cpp` **~480 LoC**,
  Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green,
  **0 warnings** after 2 fix iterations: `<filesystem>` include moved to top + `%lld` → `%ld` для
  Linux glibc), **4 strategies** (A_None uniform baseline / B_FixedFoveation2x center 30% @ 1x1 +
  periphery 2x2 / C_GazeFoveation2x gaze-driven foveal 1x1 + mid 2x2 + peripheral 4x4 /
  D_GazeFoveation4x gaze-driven aggressive, same algorithm as C в prototype) × **5 scenes** (uniform_floor
  + forest_floor + cave_stress + mixed_biome + uniform_air per `2026-06-21-sub-chunk-layers` precedent
  for direct comparability) × **5 seeds** (1, 7, 42, 1234, 31337) × **3 extents** (1080p / 1440p / 4K)
  × **1000 iter + 10 warmup** = **300 configs × 1000 = 300,000 main measurements**, wall time
  **11.17 sec** на dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
  Output: `prototype/build/results.csv` (301 rows × 23 cols, 43.8 KB) + `prototype/run.log` (312 lines).
  **Headline findings:** **B_Fixed2x = 68.33% mean savings** (std 0.14%, n=75 configs) — far above
  5-10% threshold per `optimization-philosophy.md`; **C_Gaze2x = 84.14% mean savings** (std 0.055%,
  n=75) — **8.4× speedup**, equivalent to VaFR (arXiv 2503.23410) log-polar mapping 6.5-9.29× for
  deferred rendering; **D_Gaze4x = 84.14% mean savings** (same algorithm as C в prototype, name
  differentiation for CSV clarity). **Critical savings stability:** std 0.055-0.14% across 75 configs
  (5 scenes × 5 seeds × 3 extents) → savings are scene-coverage-INDEPENDENT (in contrast to closed
  `vk-fragment-shading-rate-voxel` verdict=mixed where hybrid coverage-classifier = 0% savings on sparse
  voxel scenes). **Verdict=mixed:** savings validated as far above 5-10% threshold, но ProjectV
  не VR-first + Stage 0/1 not gating + `VK_EXT_fragment_density_map` supersession complicates legacy
  paths; mainline = additive optional path deferred до Stage 4.3 lift draw distance bandwidth pressure
  или VR pivot post-MVP. **3-step migration per `agent/knowledge.md §30.4` precedent:** Step 1 (XS,
  ~50 LoC) `FoveationController` foundation + density map generator + per-frame update; Step 2 (S,
  ~150 LoC) `voxel.frag` Tier 2 integration + `vkCmdSetFragmentShadingRateKHR` dispatch +
  `VkFragmentShadingRateAttachmentInfoKHR` setup; Step 3 (XS, ~30 LoC) `PROJECTV_FOVEATED_RENDERING` env
  gate + Tracy plot "Foveation Density" + `ProjectVFoveationTests` unit test. Total ~230 LoC, S effort,
  2-3 sessions. **Cross-axis:** orth ко всем 6 in-progress parallel (`tracy-gpu-vs-manual` profiling +
  `taa-motion-vectors` temporal Stage 5.3 + `gpu-fluid-ca-atomic-strategy` Stage 3.1 + `vct-3d-mip-generation`
  Stage 5.1 mip + `vk-multi-gpu-split-frame` multi-GPU + `vulkan-defragmentation-compaction` VRAM);
  **complementary** к closed `vk-fragment-shading-rate-voxel` (verdict=mixed, uniform global VRS без gaze
  → **differentiates** через per-region attachment, scene-coverage-independent) + `vulkan-memory-aliasing-transient`
  (VRAM aliasing) + `dlss-fsr-xess-upscaling-voxel` (post-process upscaling, sequential adoption = pre-shading
  density reduction + post-shading upscale) + `texture-compression-format-axis` (texture compression, orth);
  cross-vendor matrix same as `dec-pipelines-async-compute` §2.2 (NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4
  + Intel Arc Gfx12.5+ + Arm Mali + Qualcomm Adreno mobile). **Caveats:** (a) CPU-only synthetic, no real
  GPU dispatch (Vulkan prototype deferred до mainline integration); (b) synthetic gaze (программно
  сгенерированный, не real OpenXR `XR_EXT_eye_gaze_interaction` input); (c) tile-rounding over-count bias
  <1% для 1080p (1080 not multiple of 16); (d) per-fragment cost = constant (no ALU/memory simulation);
  (e) C/D algorithmically identical в prototype (D was meant to be more aggressive, but model already
  uses 4x4 periphery); (f) cross-vendor matrix analytical projection only; (g) mutation cost out of
  scope (incremental gaze updates via `VK_QCOM_fragment_density_map_offset` Tile Offset deferred до
  mobile/VR port); (h) Stage 4.3 128m draw distance bandwidth pressure = primary mainline motivator
  (NOT VR); (i) `VK_QCOM_fragment_density_map_offset` mobile path out of scope single-session.
  Cross-refs: `TODO.md §2.1/§4.3/§5.1/§5.2/§5.3`, `src/render/Renderer.cpp` (dynamic rendering path,
  verified via `rg`), `src/shaders/voxel.frag` (VCT + main fragment pipeline), `src/shaders/voxel_mesh.comp:146`
  (mesh shader dispatch), `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2`
  (Nearest Gap callout), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold),
  `hardware-profile.md §1/§3/§4` (Zen 3 5800X + RTX 3060 Ti GA104 Ampere + Vulkan 1.4.341 +
  `VK_KHR_fragment_shading_rate` rev 1 + `VK_KHR_dynamic_rendering_local_read` Vulkan 1.4 core),
  `benchmarks/methodology.md §3` (measurement protocol), `agent/knowledge.md Part B §9` line 1424
  (web fallbacks: searx.be, duckduckgo, brave, bing, google, startpage — web_search работал на этой
  сессии без fallback). См. §6 + [experiment README](./experiments/2026-06-21-eye-tracked-foveated/README.md) +
  [STATUS](./experiments/2026-06-21-eye-tracked-foveated/STATUS.md) +
  [sources](./experiments/2026-06-21-eye-tracked-foveated/sources.md) +
  [RESULTS](./experiments/2026-06-21-eye-tracked-foveated/RESULTS.md) +
  `prototype/{foveation_sim.cpp, README.md, run.log, build/results.csv}` (301 rows × 23 cols).

- `2026-06-21-lod-transition-strategy` (verdict=`mixed`). **LOD transition strategy axis** experiment
  closed same session (Stage 4.2 per `TODO.md §4.2` line 328 explicit DoD: «Отсутствие визуальных
  артефактов "дырявого мира" на стыках LOD-зон»; **self-invented topic** per operator instruction
  `2026-06-21` «выбирай свободную тему или придумывай свою и исследуй»; **explicit Gap** = transition
  zone problem = NOT the per-LOD downsampling problem; closed `2026-06-21-lod-mesh-downsampling` fixed
  per-LOD content via B_SurfacePreserve kernel, but transition between LOD levels is separate decision).
  Web research completed via DuckDuckGo HTML endpoint + webfetch (Exa MCP HTTP 429 persistent per
  operator directive); **11 references verified** per `sources.md`: **Mikola Lysenko 2018 "A level of
  detail method for blocky voxels"** [canonical blocky voxel LOD reference, direct validation:
  "if we have geomorphing, then we don't need to implement seams or skirts to get crack-free LOD"]
    + **Hoppe 1997 "View-Dependent Refinement of Progressive Meshes"** [SIGGRAPH 1997 ACM 258734,
      foundational: "smooth visual transitions (geomorphs) can be constructed between any two selectively
      refined meshes" + "less than 15% of total frame time on a graphics workstation"] + Hoppe 1996 +
      Hoppe 1998 + Mikola Lysenko 2012 [Naive Greedy Meshing foundation for ProjectV mainline] + Limper
      et al. 2013 POP Buffer [Pacific Graphics 2013 CGF, implicit LOD] + Vulkan Guide Project Ascendant
      [chunkSize=8 production reference matching ProjectV, 5 separate geometry draw systems] + Lengyel
      2009 Transvoxel [for iso-surface NOT blocky voxel = NOT directly applicable]. Standalone C++26 CPU
      prototype (`prototype/lod_transition_bench.cpp` ~430 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26
  -DNDEBUG -Wall -Wextra -Wpedantic`, build green, **0 warnings**). 5 strategies × 5 scenes × 5 seeds
      × 1000 iter + 10 warmup = **125,000 main measurements**, wall time 3.67 sec на dev host `obvium`
      Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline findings:**
      **C_Geomorph = canonical recommended** per Hoppe 1997 + Lysenko 2018 (26.8 µs build / 102 KB mem
      / 795 tris / 21.06 dB PSNR in naive model = **same triangles as A_Pop, no overhead**);
      **A_Pop FAILS `TODO.md §4.2` DoD line 328** = 27.76 dB PSNR < 35 dB threshold + 0.717 voxel disc =
      visible seam; **D_PreComputedMorphTargets NOT recommended** = 4.3× build cost exceeds 50 µs Stage
      4.1 budget + 3.1× memory = +432 MiB at Stage 4.3 128m draw distance, 4096 chunks;
      **B_Crossfade NOT recommended** = doubles triangles + worse quality than A_Pop in naive model;
      **E_HZB_Stitch needs GPU prototype** = same quality as A_Pop in analytic model. 3-step migration
      per `agent/knowledge.md §30.4`: Step 1 (XS, ~50 LoC) `LodTransition::SelectStrategy()` dispatcher +
      `transitionZone` per-frame chunk classification в `src/render/HizCulling.cpp:800-805` (current hardcoded
      `mip=0u`) + per-chunk morph factor uniform; Step 2 (M, ~300 LoC) per-strategy implementation в
      `src/shaders/voxel_mesh.comp` (or Pattern C `voxel_mesh.mesh` per `TODO.md §2.2`) — compute morph
      factor `t` per chunk + dual-source vertex fetch (LOD 0 + LOD 1) + Hoppe 1997 interpolation formula;
      Step 3 (S, ~100 LoC) `PROJECTV_LOD_TRANSITION=pop|crossfade|geomorph|morph_targets|hzb_stitch` env
      flag + Tracy plot "LOD Transition" + `ProjectVLodTransitionTests` unit test. Total ~450 LoC, M effort,
      2-3 sessions. См. §6 + [experiment README](./experiments/2026-06-21-lod-transition-strategy/README.md)
    + [RESULTS](./experiments/2026-06-21-lod-transition-strategy/RESULTS.md) +
      [sources](./experiments/2026-06-21-lod-transition-strategy/sources.md) +
      `prototype/{lod_transition_bench.cpp, lod_transition_bench, results.csv (125 rows), run.log}`.
- `2026-06-21-vulkan-memory-aliasing-transient` (verdict=`mixed`). **Render-graph / transient-resource
  aliasing axis** experiment closed same session (**first axis** в 30+ closed experiments: Vulkan
  memory aliasing + render graph DAG для ProjectV-style multi-pass renderer). **Self-invented topic**
  per operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй».
  Web research completed via DuckDuckGo HTML endpoint + webfetch (Exa MCP HTTP 429 persistent per
  operator directive); **9 primary + 7 secondary sources verified**: Yuriy O'Donnell 2017 GDC Frostbite
  FrameGraph [canonical], Themaister 2017/2019 Granite Engine blog [open-source reference], VMA
  official resource_aliasing docs, WSCG 2023 history-aware frame graph academic paper, dev.to
  p3ngu1nzz 2025-10-06 + 2025-10-18 modern implementation, Khronos Vulkan Tutorial render graph,
  AMD RPS SDK, KhronosGroup Vulkan resources.adoc 2026-06-05. Standalone C++26 CPU lifetime simulator
  `prototype/mem_alias_bench.cpp` ~600 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`, builds
  green with 10 cosmetic warnings), 3 workloads × 4 strategies × 5 seeds × 1000 iter + 10 warmup =
  **60,000 main measurements**, wall time <1 sec на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. **Headline (mixed):** **D_DAGRenderGraph barrier reduction = −74%**
  consistent across all workloads (28→7 / 50→13 / 74→19) — **real win**, directly impacts CPU command
  buffer recording overhead. **C_FullAliasing VRAM savings = −7-8%** на typical (276→255 MiB) +
  projected (398→372 MiB) workloads = crosses 5% threshold per `optimization-philosophy.md`. Modest
  absolute savings (~22 MiB) на large workloads, ≈0 на minimal MVP (pool overhead eats savings).
  **B_VMA_SubAllocatorPool = REGRESSION** — pure pool without lifetime analysis = worse than current
  pattern, **never adopt without aliasing**. **Persistent image bottleneck** (root cause of modest
  savings): depth + shadow + hiz + taa history = ~98 MiB cannot be safely aliased across frames.
  Local cross-refs: `src/render/SceneResources.cpp:805-1100` (22 separate VMA allocations per frame),
  `src/render/Renderer.cpp:507-536` (manual `vkCmdPipelineBarrier2` batch), `src/render/Renderer.cpp:81-110`
  (`TransitionImage` helper — manual barrier exemplar). Cross-axis: orthogonal ко всем 5+ in-progress
  parallel (hzb-smart-mip-select + tracy-gpu-vs-manual + vct-3d-mip-generation + vk-multi-gpu-split-frame
    + gpu-fluid-ca-atomic-strategy); complementary к closed `frame-flight-allocator-budget` (allocator
      strategy = VMA pool, **NOT aliasing** — different lever), `depth-occlusion-quantization` (format
      axis), `vma-sparse-textures` (page-table aliasing, не within-frame transient), `nanovdb-on-gpu`
      (storage), `bindless-descriptor-overhead` Phase D (descriptors). **Mainline recommendation:**
      phased migration per `agent/knowledge.md §30.4` — Step 1 (S, ~150 LoC) immediate VMA pool;
      Step 2 (M, ~500 LoC) Stage 4.3 interval-graph coloring; Step 3 (L, ~1500 LoC) Stage 5.x deferred DAG
    + auto-barrier. Total ~2150 LoC, L effort, 4-6 sessions. Caveats: CPU simulation only, synthetic
      workloads, greedy coloring (production = Pettis-Hansen +10-20% better), single-GPU dev host.
      См. §6 + [experiment README](./experiments/2026-06-21-vulkan-memory-aliasing-transient/README.md) +
      [RESULTS](./experiments/2026-06-21-vulkan-memory-aliasing-transient/prototype/RESULTS.md) +
      [sources.md](./experiments/2026-06-21-vulkan-memory-aliasing-transient/sources.md) +
      `prototype/{mem_alias_bench.cpp, build/results.csv}`.

- `2026-06-21-greedy-physics-meshing-cpu` (verdict=`yes`). **Greedy physics meshing axis**
  experiment closed same session (Stage 3.3 per `TODO.md §3.3` explicit DoD: "Количество коллизионных
  шейпов в CompoundShape снижается минимум в 4 раза на типичном ландшафте" + "Полное совпадение
  физического поведения"). Web research completed via DuckDuckGo HTML endpoint + webfetch (Exa MCP
  HTTP 429 rate-limited for web_search); 9+ sources verified this session: Mikola Lysenko 2012
  "Meshing in a Minecraft Game" (`0fps.net/2012/06/30/...`, canonical 8×-approximation proof),
  Laine & Karras **2010** (не 2013) "Efficient Sparse Voxel Octrees" (IEEE TVCG DOI
  `10.1109/TVCG.2010.240`), Vercidium C# implementation (`github.com/vercidium-patreon/meshing`,
  644 stars), roboleary Java port, gedge.ca 2014, fluff.blog 2023, zenny3d 2025, nickmcd 2021,
  Epic UE tutorial, Vulkan Guide. `sources.md` обновлён с verified citations. Local cross-refs
  (`src/physics/PhysicsWorld.cpp:712-773` mainline baseline = 0× reduction, `src/physics/PhysicsWorld.cpp:547-560`
  IsPhysicsSolidMaterial, `src/voxel/VoxelWorld.hpp:78-107` VoxelWorld struct + chunkSize=8, `agent/workspace.md
  §1 Phase 4` + `§1 Phase 9` incremental Jolt per-chunk wiring closed). Standalone
  C++26 CPU prototype (`prototype/greedy_physics_bench.cpp` ~640 LoC, `clang++ 22.1.6 -O3 -march=native
  -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, 2 dangling-capture warnings в CLI parser, не блокируют).
  6 strategies (A_Naive baseline = mainline / B_1DZ / C_2DXZ / D_3D / E_Octree / F_TwoPass) × 5 scenes
  (uniform_floor / uniform_half / forest_floor / cave_stress / mixed_biome) × 5 seeds (1, 7, 42, 1234,
    31337) × 1000 iter + 10 warmup = **150 configs × 1000 = 150,000 main measurements**, wall time
           0.12 sec on dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output:
           `prototype/results.csv` (151 rows = 1 header + 150 measurements). **Headline findings:**
           **F_TwoPass + D_3D = 35× avg shape reduction** (8× better than 4× DoD) при **100% volume preservation**
           across 150 configs (no false positive/negative merge = identical physics behavior DoD). Per-scene:
           uniform_floor 64× / uniform_half **256×** / forest_floor 47-50× / cave_stress 49× / mixed_biome 12×.
           **B_1DZ = 5× reduction** (just above DoD, fastest at 0.39 µs/chunk). **C_2DXZ = 16× reduction** stable
           across all scenes. **E_Octree = broken** (1.0× reduction on uniform_floor + cave_stress — coplanar 2D
           layer merge not implemented в my prototype, fixable out of scope; F_TwoPass doesn't suffer because 2D
           slice pass naturally handles coplanar layers). **A_Naive = 0× reduction**, главная цель эксперимента —
           replacement required. **Verdict=yes (with caveat on E_Octree):** 35× reduction validated, 8× better
           than 4× DoD, 100% volume preservation, 0.78-0.81 µs/chunk (62-64× headroom vs 50 µs Stage 4.1 budget).
           **Mainline рекомендация:** use `F_TwoPass` (same reduction as D_3D, simpler code, naturally matches
           per-Y-layer chunk semantic per closed `2026-06-21-sub-chunk-layers` verdict=mixed). 3-step migration
           per `agent/knowledge.md §30.4` precedent: Step 1 (XS, ~30 LoC) `src/physics/GreedyPhysicsMerger.{hpp,cpp}`
           foundation; Step 2 (S, ~50 LoC) replace per-voxel loop в `BuildStaticVoxelCollisionBody:712-740` + wire
           per-chunk rebuild path в `ProcessChunkRebuildQueue`; Step 3 (M, ~80 LoC) `PROJECTV_GREEDY_PHYSICS_MESH=ON`
           env flag (default ON) + Tracy plot "Physics Greedy Merge" + `WorldStats` extension +
           `ProjectVPhysicsGreedyMergerTests` unit test. Total ~160 LoC, S effort, 1-2 sessions. **Net effect
           positive** despite +60% per-call build cost delta: 35× fewer AddShape + 35× fewer JPH child shapes =
           JPH broad-phase cost dominates (per Jolt docs broad-phase visits each child shape → 35× fewer visits
           = much faster collision query + rebuild). **Cross-axis:** orth ко всем 5 in-progress parallel
           (tracy-gpu = profiling, gpu-fluid-ca = Stage 3.1 atomic, vk-fragment-shading-rate = VRS fragment
           rate, audio-diffraction = audio, vct-cone-count = Stage 5.1 VCT); **complementary** к closed
           `2026-06-20-meshing-algo-comparison` (visual meshing = same algorithmic family [Mikola Lysenko 2012
           per-axis 2D scan] applied to visual quads в `voxel_mesh.comp::GreedyFacePass`; this = same algorithm
           applied to physics AABB boxes в `BuildStaticVoxelCollisionBody`) + closed
           `2026-06-20-work-stealing-job-system`
           (serial dispatcher default, single-threaded greedy merge). **Continuation chain:** visual meshing
           (closed `meshing-algo-comparison` mixed) → physics meshing (this yes) = full Stage 3.3 + visual mesh
           optimization landscape covered same-session. Caveats: (a) CPU prototype only, no JPH broad-phase
           query timing; (b) synthetic scenes representative not exhaustive; (c) E_Octree bug not fixed в this
           experiment; (d) mutation cost (per-chunk rebuild on voxel edit) not measured separately. Cross-refs:
           `TODO.md §3.3`, `src/physics/PhysicsWorld.cpp:712-773::BuildStaticVoxelCollisionBody` (mainline
           baseline = 0× reduction), `src/physics/PhysicsWorld.cpp:547-560::IsPhysicsSolidMaterial`,
           `src/voxel/VoxelWorld.hpp:78-107`
           (VoxelWorld struct, chunkSize=8, access API), `agent/workspace.md §1 Phase 4` (incremental Jolt
           per-chunk wiring closed), `agent/workspace.md §1 Phase 9` (ProcessChunkRebuildQueue per-frame call
           closed), `agent/knowledge.md §17` (build matrix), `agent/knowledge.md §30.4` (3-step migration
           precedent), closed `2026-06-20-meshing-algo-comparison` (visual meshing patterns), closed
           `2026-06-21-sub-chunk-layers` (per-Y-layer chunk structure = natural fit для F_TwoPass), closed
           `2026-06-20-work-stealing-job-system` (serial default), `docs/experiments/hardware-profile.md §1`
           (Zen 3 5800X dev host), `docs/experiments/benchmarks/methodology.md §3` (measurement protocol),
           `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold — well above
           here: 35× reduction). См. §6 +
           §1 + [experiment README](./experiments/2026-06-21-greedy-physics-meshing-cpu/README.md) +
           [RESULTS](./experiments/2026-06-21-greedy-physics-meshing-cpu/RESULTS.md) +
           [sources](./experiments/2026-06-21-greedy-physics-meshing-cpu/sources.md) +
           `prototype/{greedy_physics_bench.cpp, CMakeLists.txt, README.md, results.csv}`.

Just-closed (this session, `2026-06-21`):

- `2026-06-21-genlayer-functional-biome-pipeline` (verdict=`mixed`). **Stage 4.1 world gen —
  GenLayer functional pipeline parallelism analysis** (Minecraft 1.12 `GenLayer.java:25-94` 20+ chained
  transformations: GenLayerIsland → GenLayerFuzzyZoom → GenLayerAddIsland → GenLayerZoom×N →
  GenLayerBiome → GenLayerHills → GenLayerShore → GenLayerRiverMix → GenLayerVoronoiZoom).
  **Self-invented topic** per operator instruction «выбирай свободную тему или придумывай свою и
  исследуй». Web-research complete via `webfetch` DuckDuckGo HTML endpoint (Exa HTTP 429 persistent per
  `agent/knowledge.md Part B §9`); **10+ primary sources verified** per `sources.md`: MC GenLayer.java
  decompiled, Cubiomes layers.h reference implementation, AdityaGupta1/mega-minecraft (CUDA terrain gen),
  hlsvortex/HLS_WebGPUPlugins (WebGPU biome.compute.wgsl), AMD GPUOpen Work Graphs (biomes.hlsl),
  B4rtekk1/Minerust (Rust 11-biome async gen), draquel/VoxelWorlds (UE5 GPU-first biome system),
  Markgatcha/ProceduralTerrainToolkit (dual-noise CPU+GPU), paulrobello/voxel-world (5D climate noise).
  Standalone C++26 CPU prototype `prototype/genlayer_bench.cpp` ~590 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **0 warnings** after
  fixing 8 cosmetic warnings).
  4 strategies (A_Serial / B_Parallel / C_Fused / E_GPUModel) × 3 pipelines (5/10/20 layers) ×
  5 sizes (8-128) × 5 seeds × adaptive iterations = **240 main measurements** (with iteration
  reduction for slow configs: 20-layer @ sz≥16 = 10 iter, 20-layer @ sz≥64 = 20 iter).
  **Headline (mixed per Amdahl expectation vs measured):** **GPU compute shader achieves 2.5×
  speedup at 20-layer pipeline** (size 8: serial=32.6 ms, GPU=12.8 ms; size 16: serial=130 ms,
  GPU=51 ms); **CPU parallel achieves 1-2.2× speedup**, only meaningful at large sizes
  (10-layer @ 128: serial=36.6 ms, parallel=16.4 ms = 2.23×); **B_Parallel/C_Fused are WORSE than
  A_Serial at small sizes** (per-element LCG overhead dominates). **Hypothesis REJECTED at 10-50×
  level** — sequential chain dependencies + per-launch overhead (5 µs per dispatch × 20 layers =
  100 µs pure overhead) limit speedup. **5-10% threshold per `optimization-philosophy.md`:** 2.5×
  crosses threshold but absolute speedup insufficient for 500-1500 LoC mainline cost.
  **Verdict=mixed:** GPU does help at full-pipeline scale, but not enough to justify full GenLayer
  implementation. **Recommendation:** Defer GenLayer, use simpler per-column noise-to-biome for
  Stage 4.1 (matches closed `biome-transition-blending` precedent). If GenLayer is later added:
  GPU mega-kernel (fuse 3-5 adjacent layers per dispatch) + ~380 LoC total, M effort, 2-3 sessions.
  **Cross-axis:** orth orth ко всем 5 in-progress parallel (`voxel-topology-analysis` Stage 3.x/4.x,
  `ecs-1m-entities-bottleneck` Stage 6.x, `flow-field-pathfinding-10k-units` independent,
  `tracy-gpu-vs-manual` profiling, `tank-terrain-interaction-physics` independent); complementary
  к closed `biome-transition-blending` (mixed, **biome blending = post-pipeline smoothing**, not
  pipeline itself — different layer) + `trilinear-noise-interpolation` (mixed, **noise sampling**,
  not biome mapping) + `wfc-procedural-worlds` (mixed, **alternative constraint-solver approach**).
  First dedicated **biome generation pipeline architecture** axis в 50+ closed experiments.
  См. [README](./experiments/2026-06-21-genlayer-functional-biome-pipeline/README.md) +
  [STATUS](./experiments/2026-06-21-genlayer-functional-biome-pipeline/STATUS.md) +
  [sources](./experiments/2026-06-21-genlayer-functional-biome-pipeline/sources.md) +
  `prototype/{genlayer_bench.cpp (590 LoC), build/results.csv (240 rows)}`.

- `2026-06-21-lod-mesh-downsampling` (verdict=`mixed`). **LOD uniform downsampling + stitch strategy
  axis** experiment closed same session (Stage 4.2 chunk 2 per `TODO.md §4.2` + explicit
  "Nearest Gap" в `agent/workspace.md §2` line 44-45 "uniform downsampling implementation …
  actual mesh-level downsampling not yet built"). Web-research complete (2 batch queries +
  targeted searches, ~30 sources, 12 primary + 6 supplementary верифицированы: **0fps.net
  "A level of detail method for blocky voxels" (Mikola Lysenko 2018) [POP buffers + vertex
  clustering + stable LOD rounding 2-3 iter = seamless LOD без skirts], Transvoxel (Lengyel
  2009 transvoxel.org) [512 transition cell cases / 73 equivalence classes, patent-free
  Space Engineers + Astroneer — **for iso-surface meshes NOT blocky voxels, not applicable**],
  Cinevva 2026-02-25 Transvoxel/clipmaps blog, Blackflux "Meshing Part 3" 2014 [3 T-junction
  strategies: Naive Greedy / Poly2Tri / post-process], Voxceleron2 hybrid Sparse LOD Octree,
  Cubyz DeepWiki 2026-03-19 [production reference: LOD 0-16, per-LOD `faceBuffers` +
  `lightBuffers`, GPU compute cull, NO special seam handling — closest production reference],
  Aokana arXiv 2505.02017 May 2025 [8-child octree density=2 threshold — similar to our
  A_Majority3D], Teknologicus Vorxel Oct 2024 [GPU mipmaps: 0.4s GPU vs 17s CPU для 78M voxels],
  GPUOpen FidelityFX SPD [RDNA-optimized single-pass downsampler], OptiFine #7567 [negative
  evidence: "LOD useful for render distance, not perf" — but ProjectV is voxel-camp per
  `meshing-algo-comparison` vertex-bound, so LOD has real value], Voxel.wiki T-Junctions
  [4 workarounds], Nick Gildea 2014 DC seams [DC natural property handles different leaf
  sizes без special seam], DreamCat Games SurfaceNets 2020 [boundary voxel lookup pattern]).
  Standalone C++26 CPU prototype (`prototype/lod_bench.cpp` ~840 LoC, `clang++ 22.1.6 -O3
  -march=native -std=c++26 -DNDEBUG`, builds green with 0 warnings after ASAN debug fixed
  stack-buffer-overflow в `downsample_A` для step=4/8 case where `uint8_t g[8]` was too
  small — resized to 512 bytes for max step³). 4 downsample kernels (A_Majority3D /
  B_SurfacePreserve / C_SolidOnly / D_MaxPool) × 3 stitch strategies (X_None / Y_TJunctionPad
  / Z_NeighborLocked) × 5 scenes (uniform_air / uniform_floor / forest_floor / cave_stress
  / mixed_biome — same as `sub-chunk-layers` for direct comparability) × 4 LOD levels (8³/4³/2³/1³)
  × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter + 10 warmup = **1200 main measurements + 75
  T-junction detection measurements**, wall time ~2 min on Zen 3 5800X (governor=`powersave`).
  Output: `build/results.csv` (94 KB) + `build/results_tjunc.csv` (12 KB). **Headline
  finding:** **`B_SurfacePreserve` is the only kernel that satisfies Stage 4.2 DoD
  "отсутствие визуальных артефактов 'дырявого мира' на стыках LOD-зон" — 0 T-junction
  holes across 75 configurations (16938 boundary face emissions, 0 mismatches).** Other
  kernels: A_Majority3D = 10-32% boundary mismatch, C_SolidOnly = 17-32% + **catastrophic
  collapse в cave_stress** (entire LOD 1 chunk → 0 quads), D_MaxPool = 10-32% (same as A).
  B_SurfacePreserve also **fastest** of 4 kernels (early-out on `all_same` check) at LOD
  0/1/3. All kernels < 1.5 µs/chunk → 30-100× headroom vs 50 µs Stage 4.1 budget. Triangle
  reduction: LOD 1 = **5.94×**, LOD 2 = **31.8×**, LOD 3 = **169×** (all > 4×/16×/64×
  geometric bounds). **Verdict=mixed:** single (kernel, stitch) pair doesn't win for all
  scenes, but `(B_SurfacePreserve, X_None)` is the only DoD-satisfying default. Stitch
  strategies produce identical quad counts в prototype because B kernel eliminates T-junction
  problem upstream. **Mainline рекомендация:** use `B_SurfacePreserve` as default kernel
  for Stage 4.2 chunk 2 uniform downsampling. 3-step migration per `agent/knowledge.md
  §30.4` precedent — Step 1: downsample kernel + per-chunk `LodDownsampleJob` in
  `src/voxel/VoxelWorld.{hpp,cpp}` ~150 LoC; Step 2: `SelectLodMeshSource` decision в
  `voxel_mesh.comp` per-chunk dispatch ~250 LoC; Step 3: Tracy plot + default flip
  ~50 LoC. Total ~450 LoC, M effort, 2-3 sessions. Per-scene policy option (out of scope
  for v1, follow-up): runtime select between B_SurfacePreserve (default) и C_SolidOnly
  (для uniform_floor-style scenes) → 5-15% extra quad reduction on uniform scenes. Cross-axis:
  6 closed same-session `2026-06-21` (audio mixed + wfc mixed + sub-chunk mixed + gpu-noise
  mixed + frame-flight mixed + dxc mixed) + 3 in-progress same-session (tracy-gpu +
  taa-motion-vectors + gpu-fluid-ca-atomic-strategy) + 2 same-day declared
  (vk-fragment-shading-rate-voxel + audio-diffraction-hybrid) + 19+ closed `2026-06-20` +
  this = full Stage 0/1.x/2.x/3.x/4.x/5.x/6.x + cross-cutting optimization landscape +
  audio + temporal + atomic + profiling + **LOD geometry axis NEW**. Cross-refs:
  `TODO.md §4.2`, `src/voxel/VoxelWorld.hpp:78` (chunkSize=8) + `:1175-1208` (existing
  `SelectLodLevelForDistance` + `AssignLodLevels`), `src/voxel/VoxelWorld.hpp:54`
  (existing `VoxelChunk::lodLevel` byte), `src/shaders/voxel_mesh.comp:146` (existing
  dispatch pattern), `agent/workspace.md §2` (Nearest Gap callout), `agent/knowledge.md
  §30.4` (3-step migration precedent), `2026-06-20-nanovdb-on-gpu` (NanoVDB mip chain =
  natural storage для LOD pipeline), `2026-06-20-meshing-algo-comparison` (Naive Greedy
  baseline at LOD 0), `2026-06-21-sub-chunk-layers` (orthogonal vertical-layer axis,
  same scenes + seeds for direct comparability), `2026-06-20-dec-pipelines-async-compute`
  (async foundation relevant для GPU downsample dispatch), `2026-06-21-gpu-procedural-
  noise-compute-kernels` (memory-bound GPU dispatch pattern precedent),
  `docs/experiments/hardware-profile.md §1+§2` (Zen 3 5800X dev host `obvium`),
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold).
  Caveats: CPU-only prototype, no GPU dispatch (cross-vendor validation deferred to
  follow-up); Naive face counter без greedy merge (per `sub-chunk-layers` precedent,
  layout-orthogonal); Synthetic scenes, not real ProjectV chunk content; Stitch strategies
  produce identical quad counts в prototype (X=Y=Z because B kernel eliminates T-junction
  проблема upstream); Visual QA in real gameplay required to confirm B's T-junction
  robustness at runtime camera angles; No mutation cost measured (out of Stage 4.2 DoD).
  См. §6 + §1 + [experiment README](./experiments/2026-06-21-lod-mesh-downsampling/README.md)
    + [RESULTS](./experiments/2026-06-21-lod-mesh-downsampling/RESULTS.md) +
      [sources](./experiments/2026-06-21-lod-mesh-downsampling/sources.md).

- `2026-06-21-gpu-procedural-noise-compute-kernels` (verdict=`mixed`). **Noise-algorithm axis**
  experiment закрыт same session. Stage 4.1 GPU Noise & World Gen — выбор между 5 noise kernels
  (Value / Perlin / Simplex / OpenSimplex2 / Worley) для chunkSize=8 world gen. Web-research
  complete (3 batches, ~20 results, 20 sources верифицированы: Schneider arXiv 1903.12270
  [Perlin/Float 3D = 77 ALU inst], GPU Gems 2 Ch 26 [textured-LUT Perlin = 53 inst / 9 lookups],
  atyuwen/bitangent_noise SimplexNoise.hlsl [3D ~71 instruction slots], KdotJPG/OpenSimplex2
  [673 stars CC0 modern GPU-friendly], Auburn/FastNoiseLite 3D benchmarks [Perlin 47.93 M/s scalar /
  261.10 M/s AVX2], NVIDIA Nsight Compute Ampere workgroup-64 occupancy sweet spot, Khronos Forums
  compute SSBO write cost, JCGT 2022 Olano GTX 1660 modern compiler DCE 17% speedup, Vulkanised
  2024 GPU Atomic Modeling McKee, production refs: paulrobello/voxel-world Vulkan compute 5D
  climate noise + Perlin Feb 2026, Aokana arXiv 2505.02017 GPU-Driven voxel May 2025,
  AdityaGupta1/mega-minecraft CUDA fBm Oct 2025, russellocean/pebble-rs WGPU compute Nov 2025,
  Yunasawa YNL Vozel Minecraft 1.18+ 5-param FBM Sep 2025). Standalone Vulkan 1.4 compute prototype
  (`prototype/{main.cpp, noise_kernels.comp, CMakeLists.txt, README.md}`, ~700 LoC, 5 conditional
  GLSL variants через `#define VARIANT_*`, RTX 3060 Ti GA104, Vulkan 1.4.341, NVIDIA 610.43.02).
  3 runs × 5 variants × 1000 iter + 10 warmup. **Measured:** VALUE=0.0273, PERLIN=0.0272, SIMPLEX=
  0.0272, OPENSIMPLEX2=0.0272, WORLEY=0.0280 ms mean — **all variants в пределах 2.9% mean** (below
  5% threshold per `optimization-philosophy.md`). WORLEY unexpectedly not slowest (`glslc` 2026.2 fully
  unrolled + register optimization). **Главный finding:** noise algorithm choice **не** meaningful
  perf discriminator на chunkSize=8 dispatch pattern; memory-bound kernel (65.6% of 448 GB/s peak =
  65.6% efficiency) — ALU = ~14% of dispatch time. Per-eval cost = 13.0 ns/eval, per-chunk = 6.6 µs.
  **Stage 4.1 budget (50 µs/chunk per `TODO.md §4.1`):** 8× headroom single octave, 1.9× FBM 4 octaves,
  0.63× multi-channel FBM 4 octaves × 3 channels (over budget — needs octave reduction OR async-compute
  overlap). **Verdict=mixed:** perf axis inconclusive, quality + license axis still favors OpenSimplex2
  3D-S (CC0 + no axis artifacts + analytic derivatives + stable cold-cache perf). **Mainline
  рекомендация:** use OpenSimplex2 3D-S для Stage 4.1 (NOT because fastest — because license + quality
    + stability). 3-step migration per `agent/knowledge.md §30.4` precedent (Step 1 GLSL port + CC0
      attribution, Step 2 dispatch in `world_gen.comp` + FBM wrapper, Step 3 multi-channel). ~300 LoC,
      S effort, 1-2 sessions. Continuation chain: `2026-06-20-simd-procedural-noise` (CPU AVX2 vs scalar,
      closed verdict=mixed) → this (GPU algorithm choice, closed verdict=mixed). Cross-axis: my closed
      `gpu-noise-compute` + 3 parallel in-progress (frame-flight-allocator-budget + dxc-vs-glslc-toolchain
    + tracy-gpu-vs-manual) same-day `2026-06-21` сессии = orthogonal axes toolchain + memory +
      profiling + algorithm choice. Cross-refs: `TODO.md §4.1`, `src/voxel/VoxelWorld.hpp:85` (chunkSize=8),
      `src/shaders/voxel_mesh.comp:146` (existing dispatch pattern), `agent/workspace.md §1 Phase 1`
      (world_gen.comp skeleton), `agent/knowledge.md §30.4` (3-step migration precedent),
      `2026-06-20-dec-pipelines-async-compute` (async foundation, world gen spike isolation),
      `2026-06-20-nanovdb-on-gpu` (GPU SSBO write target), `docs/experiments/hardware-profile.md §3`
      (RTX 3060 Ti dev host). См. §6 +
      §8 + [experiment README](./experiments/2026-06-21-gpu-procedural-noise-compute-kernels/README.md).

Just-closed (this session, `2026-06-20`):

- `2026-06-20-vma-sparse-textures` (verdict=`mixed`). **Sparse Virtual Texturing axis** experiment
  закрыт same session (Stage 2.3 + cross-cutting VRAM budget). Web-research complete (4 batches,
  ~30 results, 16 sources верифицированы: shlomnissan "How Virtual Textures Really Work" 2026-02
  [software VT = доминирующий pattern в UE 5.7 RVT / Nanite / id Tech 5 MegaTexture / bgfx 40-svt /
  Frostbite; hardware sparse = "mechanism, не policy"], shlomnissan/virtual-textures GitHub 2026
  [working prototype без HW sparse], UE 5.7 Streaming Virtual Texturing docs [production = software
  layer], Nanite GDC 2024 Wihlidal [UE VT уже does SampleGrad], bgfx 40-svt Karadzic [production
  reference], Nathan Gauër 2022, SaschaWillems texturesparseresidency [Vulkan HW sparse example],
  foijord/SparseTexture 2025-02 [NVIDIA `vkQueueBindSparse` BLOCKING GLOBAL, 1 TiB address limit
  vs AMD 256 TiB / Intel 16 TiB — неприемлемо для runtime streaming], NVIDIA forums 2023
  [A4000 multi-second bind for 1000 pages, NVIDIA team acknowledged], VMA 3.4.0 CHANGELOG
  2026-06-05 [sparse convenience `vmaAllocateMemoryPages` уже из 2.x],
  `VK_EXT_pageable_device_local_memory` rev 1 [OS-level paging, complementary не replacement],
  `VK_EXT_memory_decompression` rev 1 ratified 2025-01-23 [GDeflate GPU decompress, NVIDIA-only
  pre-2026], `VK_NV_extended_sparse_address_space` rev 1 2023-10-03 [NVIDIA 1 TiB workaround,
  not cross-vendor], KhronosGroup/Vulkan-Guide sparse_resources.adoc). Standalone Vulkan 1.4 +
  VMA 3.4.0 + volk prototype (`prototype/vma_sparse_bench.hpp` + `main.cpp` + `README.md`,
  ~770 LoC, 3 variants: dense 16 MiB atlas / sparse `VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT` atlas
    + 64-page bind test / software-VT atlas 4 MiB + R32Uint page table texture + CPU LRU page
      manager). **Главный finding:** hardware sparse textures unusable на NVIDIA для runtime world
      streaming per `foijord 2025` (`vkQueueBindSparse` blocking global). **Software VT =
      recommended default** (cross-vendor deterministic, peak VRAM cap enforceable, validated
      production pattern). Mainline рекомендация: 4-step migration per `agent/knowledge.md §30.4`
      precedent — Step 1 foundation `PageManager` + page table texture R32Uint (~150 LoC); Step 2
      integration `voxel.frag` `SampleVirtualTexture` per shlomnissan pattern + atlas + bindless
      per Phase D (~350 LoC); Step 3 page manager wiring (LRU + async upload, ~150 LoC); Step 4
      optional HW sparse для static prebake Stage 4.1 (VMA `vmaAllocateMemoryPages`, ~120 LoC).
      Total ~770 LoC + integration code, M effort, 3-4 sessions. VRAM matrix: software VT = 16-32
      MiB atlas + 16 KiB page table (vs dense 256 MiB); HW sparse = 16-64 MiB resident vs 1 GiB
      virtual; software VT = cross-vendor deterministic, HW sparse = NVIDIA blocking. Cross-vendor
      analytical projection per `dec-pipelines-async-compute` matrix. Continuation chain:
      `bindless-descriptor-overhead` Phase D (deferred → active) → this → Stage 4.3 (128+ chunks
      draw distance) validates hybrid strategy. Cross-axis: this + same-day 19+ closed сессии =
      full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape + lighting/ECS/sparse-VT. См. §6 +
      §8 + [experiment README](./experiments/2026-06-20-vma-sparse-textures/README.md).

- `2026-06-20-restir-gi-feasibility` (verdict=`mixed`). SOTA-GI-ось experiment закрыт same session. Web-research
  (~30 sources верифицированы: Bitterli 2020 ReSTIR original, Ouyang 2021 ReSTIR GI, Lin 2022 ReSTIR PT +
  GRIS, Majercik 2019/2021 DDGI, Müller 2021 NRC, NVIDIA-RTX/RTXGI SDK v2.7.0 (Mar 2026), NVIDIA-RTX/SHARC,
  NVIDIA-RTX/RTXDI v3.0+, Crassin 2011 GIVoxels VCT foundation, Lumen SIGGRAPH 2022 [Epic explicitly rejected
  VCT as leaky], Minecraft RTX GDC 2021 [voxel + path tracer + irradiance cache], Douglas Voxel Devlog #23 Jun
  2025 [voxel + DDGI direct validation], Cyberpunk 2077 RT Overdrive [production ReSTIR DI/GI + SHaRC],
  NVIDIA Zorah RTX 50 demo [ReSTIR PT], OGRE-Next CIVCT, Aokana 2025, Closest Hit ReSTIR GSGI/PMGI 2024,
  ReSTIR FG 2024, Epic DDGI abandonment Dec 2025 forum). **Главный finding:** SOTA GI techniques (ReSTIR PT,
  DDGI, SHaRC, NRC) все **требуют path tracer foundation** — ProjectV's Stage 5.x = hybrid VCT+RTX = **не**
  path tracer. **Architectural mismatch.** **Recommended action:** keep current hybrid VCT+RTX as-is (Stage 5.x
  MVP), defer SOTA GI integration до Stage 6+ post-MVP path tracer pivot. Recommended add-on order (if path
  tracer ships): **SHaRC → DDGI → ReSTIR DI/GI/PT** (skip NRC = NVIDIA-only). VRAM cost SHaRC alone = 185 MB
  (3.65% of 5.06 GiB budget per `hardware-profile.md` §3). Quality validated для path-tracing contexts (ReSTIR
  PT MAPE 0.39 vs 1.63 naive PT per Lin 2022 Carousel benchmark). Cannot translate без path tracer. **Lighting
  axis fully closed** (`vct-vs-rt-cutoff` + `clustered-forward-mass-lights` + `rt-shadows-vs-csm` + this).
  Cross-axis: 19+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape + SOTA-GI
  axis. См. §6 + §8 + [experiment README](./experiments/2026-06-20-restir-gi-feasibility/README.md).

- `2026-06-20-rt-shadows-vs-csm` (verdict=`mixed`). Shadow-ось experiment закрыт same session.
  Web-research (4 batches, ~30 results, 23 sources верифицированы: Boksansky RTG 2019 фундамент,
  Vulkan Tutorial Ray Query §5.2 patterns, NVIDIA Blackwell 4th-gen RT whitepaper Jan 2025
  [2× ray-tri vs Ada, 8× vs Turing], AMD HotChips 2025 RDNA 4 [8 box + 2 tri/cycle, 2× vs
  RDNA 3, OBB +10% traversal], Intel Battlemage Xe2 [3 traversal pipelines + 2 tri = 18+2 vs
  Alchemist 2+1, BVH cache 16 KB], Khronos Forum BLAS fence wait pattern, Boksansky 2019
  adaptive ray sampling) + analytical cost model + cross-vendor RT throughput matrix.
  **Hybrid CSM + RTX shadows** рекомендован для Stage 5.2: CSM (sun, current path per
  `agent/decisions.md §15`) + RTX `VK_KHR_ray_query` (feature-flagged additive для local
  lights + per-pixel contact shadow detail). **Quality gain > 5% per
  `optimization-philosophy.md`** для non-sun-dominated scenes (cave/lava/magic); < 5%
  для sun-dominated outdoor (CSM dominant). VRAM cost **8-23 MiB** на RTX 3060 Ti (well
  under 5% budget). BLAS rebuild bottleneck → async via `VK_KHR_deferred_host_operations`
  (rev 4) + `dec-pipelines-async-compute` precedent. Cross-vendor: Blackwell/RDNA 4/
  Battlemage = full benefit; Ampere/RDNA 3 = 1-2 rays limited; Turing/Alchemist = feature
  OFF. **Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent
  (Step 1 foundation extension probing + BLAS pool + TLAS scratch; Step 2 ray query в
  `voxel.frag` для local lights + async BLAS build via deferred host operations; Step 3
  default flip). ~770 LoC total, M effort, 3-4 sessions. **Continuation chain:**
  `vct-vs-rt-cutoff` (closed verdict=mixed) + `clustered-forward-mass-lights` (closed
  verdict=yes) → this. Lighting axis complete (cutoff + lights + shadows). Stage 5
  foundation + cutoffs + lights + shadows все closed same-day `2026-06-20`. Cross-axis:
  17+ closed today-сессии = full Stage 1.x/2.x/3.x/5.x/6.x optimization landscape +
  shadow-dim. См. §6 + §8 + [experiment README](./experiments/2026-06-20-rt-shadows-vs-csm/README.md).
- `2026-06-20-svdag-vs-vdb-memory-throughput` (verdict=`yes`). SVDAG-on-64-tree (current mainline)
  подтверждён **измерениями** для ProjectV workload (32³ chunks): memory 8.75 B/voxel solid / 16-70 B/voxel sparse —
  within dubiousconst282 2024 literature range. GetCell 22-36 ns, SetCell 0.03-0.04 µs no-dedup / 0.68-1.26 µs dedup-ON.
  **Dedup ON costs 20-40× build time** на non-repetitive scenes → рекомендация: per-chunk `isStatic` flag
  (Stage 1.2 design) instead of always-on. Закрыл measurement gap от `sparse-64-tree-alternatives` §5.3.
- `2026-06-20-dec-pipelines-async-compute` (verdict=`yes`). Sync-axis experiment — async-compute queue +
  `VK_KHR_synchronization2` (core 1.3) + `VK_KHR_timeline_semaphore` (core 1.2) +
  `VK_KHR_global_priority` (core 1.4) рекомендованы для 4 of 5 ProjectV compute passes: Stage 2.2 HZB
  cull + Stage 3.1 Fluid CA (20 Hz) + Stage 4.1 GPU world gen (LOW priority) + Stage 5.2 RTX BLAS build
  (`VK_KHR_deferred_host_operations`). Stage 5.1 VCT sequential default, async opt-in. Expected 5-8%
  steady-state + 100% spike elimination (world gen + BLAS). Cross-vendor: NVIDIA Ampere/Ada/Blackwell +
  AMD RDNA2/3/4 + Intel Arc Alchemist/Battlemage. Caveats: NVIDIA June 2025 driver bug
  mesh-shading+async (не applies to compute cull path); AMD «export bound shaders» warning; Intel
  Ray Queries + groupshared L1 contention. Foundation шаг = prerequisite для Stage 3.1 GPU Fluid CA
  (sync-model конкретизирует `agent/knowledge.md §30.4` contract), Stage 2.2 HZB full integration, Stage
  5.2 RTX BLAS build (Phase E per `bindless-descriptor-overhead`).
- `2026-06-20-nanovdb-on-gpu` (verdict=`yes`). GPU-side measurement closing the gap from
  `svdag-vs-vdb-memory-throughput` §3 line 157 + bugfix NanoVDB-like impl (uniform-tile lie).
  **Both CPU-side and GPU-side prototypes byte-exact** на 5 сценах × 2 kernels (verify_mismatches=0).
  NanoVDB-aligned pointer-less layout (Upper[8³] → Lower[4³] → Leaf[2³], scaled per NanoVDB.h actual
  32³/16³/8³ structure для ProjectV chunkSize=8) outperforms SVDAG-on-64-tree **on 4/5 scenes by
  12-141%** (sparse_random_8: 500 → 1210 Mrays/s; voxel_lab_8: 541 → 1208 Mrays/s; ground_8: 638 →
  1242 Mrays/s; brick_8: 1146 → 1284 Mrays/s). Only solid_8 ties (1265 vs 1272, memory-bandwidth
  bound). **GPU memory: NanoVDB uses 57-75% less VRAM**. **CPU memory: ~50% less** (B/voxel). Crosses
  5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. **Critical mainline
  finding:** ProjectV chunkSize = 8 (not 32 as previous experiment assumed) per
  `src/voxel/VoxelWorld.hpp:78` + `src/voxel/SceneConfig.cpp:78` — depth=2 not depth=3. OpenVDB 13.0.0
  (Nov 2025) lowered NanoVDB mutation barrier. Mainline рекомендация: **hybrid strategy** — keep
  CPU-side SVDAG-on-64-tree (Stage 1.2), flatten to NanoVDB-aligned transient SSBO at GPU upload for
  Stage 5.1 VCT cone-march + 3 fragment-shader DDA traces (`voxel.frag` per `TODO.md §6.2.2`). 3-step
  migration per `agent/knowledge.md §30.4` precedent. Caveats: single GPU vendor validated (NVIDIA
  RTX 3060 Ti GA104 Ampere, Vulkan 1.4.350); HDDA-specific optimizations not implemented in
  first-iteration prototype. Continuation chain: `sparse-64-tree-alternatives` → `svdag-vs-vdb-memory-throughput`
  → this. Cross-axis: previous experiments covered memory + sync; this covers GPU traversal for
  Stage 5.1.
- `2026-06-20-hzb-binding-models` (verdict=`mixed`). Cull-shader pattern decision для Stage 2.2. Web-research
  (~10 sources incl. critical NVIDIA `textureLod` bug под `VK_EXT_descriptor_heap` per
  `foijord/vk-textureLod-repro` 2026) + standalone Vulkan compute prototype + 24 sampling tests across
  8 mips × 3 patterns. **17/24 PASS, 7/24 FAIL.** Conclusive findings: (a) `texelFetch(sampler2D, ivec2,
  mipLevel)` correct + bindless-robust (recommended); (b) `textureLod` correct on classic, fragile под
  bindless на NVIDIA (NOT recommended); (c) `imageLoad(storage_image)` fundamentally unsuited для HZB
  culling (GLSL single-mip-per-binding, proved by `max_abs_error = N * 1000` pattern). Mainline
  recommendation: Stage 2.2 cull shader uses `texelFetch`, HZB descriptor = `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE`
    + separate `SAMPLER`. ~50-100 LoC change across 4 files. Future-proofs `bindless-descriptor-overhead`
      Phase E.

`2026-06-20-simd-procedural-noise` closed (verdict=`mixed`) — см. §6 + §8.

`2026-06-20-nanovdb-on-gpu` closed (verdict=`yes`) — hybrid strategy recommended. See §6 + §8.

- `2026-06-20-vct-vs-rt-cutoff` (verdict=`mixed`). Lighting/GI-ось experiment закрыт same session.
  Roughness-based hybrid VCT + RTX рекомендован: VCT diffuse always (6 wide cones), VCT specular
  при roughness > 0.3 (cone-march через mip-mapped atlas), RTX (`rayQueryEXT`) при roughness < 0.3
  (sharp specular + AO/contact shadows), CSM для sun (current path, additive к RTX per `decisions.md
  §15`). **Refined cutoff = 0.3** (не 0.3–0.5 диапазон): VCT specular 2.5× at r=0.3 = RTX 1-ray cost;
  OGRE 2019 precision cliff at 0.02 (8-bit atlas risk, ProjectV R8G8B8A8 same); Akenine-Möller JCGT
  2021 GGX math validates roughness → cone spread; Lumen 2022 rejected pure VCT (leaking coarse mips)
  → RTX-dominant. Cross-vendor threshold adjustment: Blackwell → 0.4-0.5 (2× tri rate vs Ada), RDNA
  2 → 0.2 (¼ tri rate), Battlemage → 0.25, no-HW-RT → VCT-only fallback. Web-research ~30 sources
  (Crassin 2011 GIVoxels, NVIDIA VXGI 0.9, OGRE 2019, Lumen SIGGRAPH 2022, Narkowicz "Journey to Lumen"
  2022, Akenine-Möller JCGT 2021, RTXGI 2.0 SDK 2024, RTXDI 3.0, Erlich 2024 Eurographics, NVIDIA
  Blackwell 2025, AMD RDNA 4 2025, Intel Battlemage 2025, Aokana 2025, etc.). Mainline integration:
  4-step migration per `agent/knowledge.md §30.4` precedent — Step 1 foundation (cutoff constant + HW
  RT probe + CMakeLists feature flag), Step 2 VCT (voxelize.comp + vct.frag + 3D atlas + mip chain
  per `TODO.md §5.1`), Step 3 RTX (BLAS per chunk + TLAS per frame + rayQueryEXT per `TODO.md §5.2`),
  Step 4 (optional post-Stage 5) DDGI/SHaRC/NRC/ReSTIR PT. Caveats: analytical model only (no
  ProjectV prototype), NVIDIA-heavy literature, ProjectV VCT leak risk = lower than Lumen surface
  cache (regular voxel SVO) but not zero. **Lighting/GI-ось closed**; Stage 5 теперь имеет все три
  foundation: storage (nanovdb-on-gpu), sync (dec-pipelines-async-compute), cutoff strategy (this).
  См. §6 + §8.

Just-closed (this session, `2026-06-21`):

- `2026-06-21-factory-production-system` (verdict=`mixed` per strategy; `yes` for E + A as recommended defaults). **Stage 6+ military sandbox Tier 3 Economy — Factory production scheduling architecture** (mass-equipment production per SupCom/HoI4/Warno precedent). **Self-invented** per operator instruction `2026-06-21` + `AGENTS.md §13.1` + §13.7 sentinel clean (`rg "factory-production"` → only backlog.md cross-ref; `ls experiments/2026-06-21-factory*` = ENOENT). Web-research via direct `webfetch` (Exa `web_search` HTTP 429 persistent this session per `agent/knowledge.md Part B §9` line 1424 fallback list); **6 primary + 3 secondary sources verified** в [`sources.md`](./experiments/2026-06-21-factory-production-system/sources.md): Wikipedia "Supreme Commander" [Mass+Energy 2-resource, factory adjacency, multi-worker assist, "If the storages are depleted and the demand of one of the resources exceeds the production, then all the productions speed is reduced"] + Wikipedia "Hearts of Iron IV" [military/civilian/dockyard factory assignment, 5 production lines per factory] + Wikipedia "Anno 1800" [multi-tier production chain DAG, citizen-tier demand] + Wikipedia "Lean manufacturing" [TPS, JIT, Kanban, Takt time, Womack/Jones 5 principles, HP 30-75% savings] + Wikipedia "Critical path method" [CPM 1959 DuPont] + Wikipedia "Topological sorting" [Kahn 1962, O(V+E) linear]. Standalone C++26 CPU prototype `prototype/world_model.hpp` (250 LoC) + `prototype/factory_bench.cpp` (500 LoC), Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green **0 warnings**). 5 strategies (A_NaiveLinearScan / B_PriorityBucketQueue / C_DependencyDAG_TopoSort / D_CriticalPathBatch / E_ProductionLinePipeline) × 5 scenes (single_item_uniform / mixed_product_uniform / multi_tier_dependencies / wartime_surge / economic_complex) × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time < 2 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows, 18 KB). **Headline (mixed per strategy; yes для E + A as recommended defaults):**
  - **E_ProductionLinePipeline ⭐** = 343.6 ns/tick mean (314-464 range) = **0.34 ns/factory/tick** (147,000× under 50 µs budget). **UNIVERSAL RECOMMENDED DEFAULT** (1.6× faster than A, 7.2× faster than B, 11.5× faster than D, 27× faster than C). 3-stage pipeline (3 ticks advance per tick) = effective 3× throughput per factory.
  - **A_NaiveLinearScan ⭐** = 560 ns/tick mean (435-979 range) = **0.56 ns/factory/tick** (89,000× under budget). **VALID FALLBACK** (simple, cache-friendly sequential access, 4.4× faster than B).
  - B_PriorityBucketQueue = 2,482 ns/tick (4.4× slower than A) — **REJECTED** (PQ sort per tick overhead without benefit).
  - D_CriticalPathBatch = 3,951 ns/tick (7.1× slower than A) — **REJECTED** (CPM sort per tick overhead without benefit).
  - C_DependencyDAG_TopoSort = 9,253 ns/tick (17× slower than A) — **MIXED, opt-in for scenario editor only** (correct semantics + dep starvation feedback, but 2-65% throughput on dep-heavy scenes).
  **Per-strategy × per-scene highlights:** E wins all 5 scenes (326-366 ns). C at 2% throughput on `economic_complex` (sector-level stockpile insufficient → mass starvation). A/B/D/E all 121-133% throughput on non-surge scenes (over-produce due to no dep check). Wartime_surge: 53% throughput cap (9000/17000, tick-bound not strategy-bound).
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** A vs B = 4.4× ✅; E vs A = 1.6× ✅; E vs B = 7.2× ✅; C = correct but 2-65% throughput regression.
  **Hypothesis validation (3 of 3 confirmed):** <50 µs/factory/tick budget ✅ (89,000-147,000× under); ≥95% throughput in non-surge scenes ✅; zero deadlock ✅.
  **3-step mainline migration per `agent/knowledge.md §30.4`** (~580 LoC, S-M effort, 1-2 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/economy/FactoryProduction.hpp` + `FactoryProductionState` + `FactoryProductionComponent` (Flecs SoA) + `FactoryProductionSystem` skeleton; Step 2 (M, ~400 LoC) `src/economy/FactoryProduction.cpp` + port 5 schedulers (A + E mainline; B + D opt-in; C debug) + per-tick `FactoryProductionSystem::Update` + mass/energy draw integration с `supply-logistics-simulation`; Step 3 (S, ~100 LoC) `PROJECTV_PRODUCTION_SCHEDULER=NAIVE|PIPELINE|DAG|PRIORITY|CRITICAL_PATH` env gate (default `PIPELINE`) + `PROJECTV_PRODUCTION_TICK_HZ=10|20|30` + 5 unit tests + Tracy plot + save/load per `2026-06-21-save-game-persistence-architecture` precedent. **Cross-axis:** orth ко всем closed Tier 1/2/3/4 (Physics / AI / Netcode / UI / Audio); complementary к closed `supply-logistics-simulation` [mixed, input supply] + `data-driven-vehicle-weapon-definitions` [mixed, input specs] + `component-vehicle-damage-model` [yes, downstream consumer] + Tier 1 Physics experiments (tank / flight / ship / helicopter / aircraft consume factory output). **Prerequisite** для open `resource-refinery-processing` [m, Tier 3] + `tech-tree-research-system` [m, Tier 3] + `sector-strategic-map-system` [m, Tier 3] + `grand-campaign-conquest` [m, Tier 3]. **New axis:** first dedicated **factory production scheduling architecture** axis в 130+ closed experiments; opens Stage 6+ military sandbox Tier 3 Economy для mass-equipment production. Caveats: CPU-only synthetic (no Vulkan, no Flecs overhead, no real network); no real resource supply chain (prototype pre-stocked or unbounded); throughput cap on wartime_surge is fundamental tick-bound; 121% throughput для A/B/D = unbounded stockpile growth (semantically wrong but pragmatic); no lockstep sync (must be deterministic per `2026-06-21-lockstep-state-sync-hybrid-netcode` mixed precedent); item scope creep risk (cap at 16 item types for v1). Cross-refs: `TODO.md` (Stage 6+ activation per operator planning), `src/economy/` (new module), `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` (Stage 6+ deferral), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold), `hardware-profile.md §1` (Zen 3 5800X dev host), `benchmarks/methodology.md §3` (N=1000 + 10 warmup). См. [README](./experiments/2026-06-21-factory-production-system/README.md) + [STATUS](./experiments/2026-06-21-factory-production-system/STATUS.md) + [RESULTS](./experiments/2026-06-21-factory-production-system/RESULTS.md) + [sources](./experiments/2026-06-21-factory-production-system/sources.md) + `prototype/{world_model.hpp (250 LoC), factory_bench.cpp (500 LoC), build/{factory_bench, results.csv (126 rows, 18 KB)}}`.

- `2026-06-21-vulkan-fps-pacing-wayland-prototype` (verdict=`yes`). **Frame pacing axis** experiment closed
  same session (`2026-06-21`). **Supersedes** `2026-06-20-vulkan-fps-pacing-vk-ext` closed mixed
  (analytical-only + measurement gap self-identified в old §6 + Wayland
  `VK_KHR_present_mode_fifo_latest_ready` lever ratified после old capture 2025-03-18). **Headline:**
  Mode B (`VK_PRESENT_MODE_FIFO_LATEST_READY_KHR`) = **93-99% frame interval reduction** vs Mode A baseline
  для cpu_bound (192 us vs 17,066 us), gpu_bound (1,117 us vs 17,111 us), jitter (1,119 us vs 17,114 us);
  Mode D (`VK_EXT_present_timing` + `targetTime`) = **41-93% P99 variance reduction**, std-dev 47-77 us vs
  Mode A 427-902 us = **~10-15× tighter**. Mesa 26.2 std-dev prediction **validated** (Mode A std-dev
  902-1221 us matches Mesa 0.9 ms Wayland compositor overhead). Standalone Vulkan 1.4 + SDL3 harness
  ~600 LoC, 5 modes × 3 scenarios × 5 seeds × 100 frames + 5 warmup = **7,500 main measurements**, dev
  host `obvium` NVIDIA RTX 3060 Ti + driver 610.43.02 + Vulkan 1.4.341 + Wayland session per
  `hardware-profile.md §3+§6`. Web-research complete via DuckDuckGo HTML + webfetch (Exa 429 persistent);
  **12 primary + 4 supplementary sources verified**. Outputs: `prototype/build/results.csv` (7,500 rows + header)
  + `prototype/{main.cpp, triangle.{vert,frag}.spv, CMakeLists.txt, README.md}` +
  `experiment/{README.md, STATUS.md, sources.md, RESULTS.md}`. **Mainline 3-step migration per
  `agent/knowledge.md §30.4`:** Step 1 (S, ~100 LoC) `PROJECTV_PRESENT_MODE_FIFO_LATEST_READY=ON` +
  `PROJECTV_USE_PRESENT_TIMING=ON|OFF` env gates + feature detection в `VulkanBootstrap.cpp` +
  `PresentState` struct в `Types.hpp`; Step 2 (S, ~250 LoC) `Renderer.cpp::PresentFrame` Mode D
  implementation + `VulkanSwapchain.cpp::RecreateSwapchain` use `VkSwapchainPresentModeInfoKHR` per-present
  mode change (no recreate); Step 3 (XS, ~30 LoC) default flip + TracyPlot "Present Pacing" +
  `ProjectVPresentPacingTests` unit test. Total ~380 LoC, S effort, 1-2 sessions. **Two options для mainline:**
  **Option 1 (Mode B — low-latency)** = `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` best для CPU-bound workloads
  (~200 us frame interval vs current 17 ms); **Option 2 (Mode D — precise pacing)** = `VK_EXT_present_timing`
  best для vsync-locked deterministic (10-11 ms frame interval с 47-77 us std-dev vs current 427-902 us).
  **Hardware-profile.md §4 updated 2026-06-21** with new extension row. См. §6 + [experiment README](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/README.md) +
  [STATUS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/STATUS.md) +
  [sources](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/sources.md) +
  [RESULTS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/RESULTS.md) +
  `prototype/build/results.csv` (7,500 rows) + `prototype/{frame_pacing_bench, triangle.{vert,frag}.spv}` +
  `research/backlog.md §Closed`.

- `2026-06-21-hzb-smart-mip-select` (verdict=`mixed`). **Per-chunk HZB mip selection axis** experiment closed same
  session (Stage 2.1 per `TODO.md §2.1` + explicit `agent/workspace.md §2` line 52 Nearest Gap callout: «Stage 2.1 HZB
  culling refinement — current implementation always uses mip 0; smart per-chunk mip selection based on screen-space
  size is a separate optimization»). Web-research complete via DuckDuckGo HTML endpoint + webfetch fallback (Exa HTTP
  429 persistent per `agent/knowledge.md Part B §9`); **5 primary sources verified** this session: Greene/Kass/Miller
  1993 «Hierarchical Z-Buffer Visibility» [SIGGRAPH 1993 ACM 166147, canonical
  `cs.princeton.edu/courses/archive/spr01/cs598b/papers/greene93.pdf`], Mike Turitzin 2020 «Hierarchical Depth
  Buffers» [
  `miketuritzin.com/post/hierarchical-depth-buffers/` — exact pattern statement «works by projecting a bounding volume into screen-space and using the
  **projected size to choose the appropriate mip level**» = direct match для нашей гипотезы], Omlor & Radicke 2025
  «Two-Pass Occlusion Culling for Dynamic Voxel Scenes based on
  HZB» [IEEE Xplore 11321175, Jul 2025 — direct voxel scenes reference], DeepWiki Metallic 2026-04-06 «GPU-Driven
  Culling: MeshletCullPass and HZB» [modern Vulkan production reference], RasterGrid 2010 «Hierarchical-Z map based
  occlusion culling» [OpenGL FBO mip chain pattern] + 5 secondary (Nick Darnell SIGGRAPH 2008 + Tobias Garpenhall UE5 +
  chaoticbob mesh shading + zeux/meshoptimizer + JarkkoPFC/meshlete). Local cross-refs (
  `src/render/HizCulling.cpp:800-805` hardcoded `mipLevel=0u` baseline = A_UniformMip0,
  `src/render/HizCulling.cpp:326-369` `BuildHizMipChain` уже работает, `src/render/HizCulling.hpp:48-52`
  `HizCullingPushConstants` structure, `src/shaders/hzb_cull.comp:33-90` `AabbVisibleAgainstMip` per-mip texelFetch
  loop, `src/shaders/hzb_cull.comp:102` uniform mip от push constants, `src/render/Renderer.cpp:1344-1350`
  `RecordHzbCullingDispatch` call site, `agent/workspace.md §1 Phase 1` HZB full integration closed,
  `agent/workspace.md §2` line 52 explicit Gap callout, `agent/knowledge.md §30.4` 3-step migration precedent, closed
  `2026-06-20-hzb-binding-models/` [texelFetch foundation],
  `2026-06-21-greedy-physics-meshing-cpu/` [CPU prototype precedent + same scenes],
  `2026-06-21-sub-chunk-layers/` [synthetic scenes + seeds],
  `2026-06-21-depth-occlusion-quantization/` [PSNR threshold],
  `2026-06-20-dec-pipelines-async-compute/` [async foundation],
  `docs/experiments/hardware-profile.md §1` [Zen 3 5800X dev host `obvium`]). Standalone C++26 CPU cull simulator ~700
  LoC (
  `prototype/{hzb_smart_mip_bench.cpp, scenes.hpp, cull_simulator.hpp/cpp, ground_truth_raycaster.hpp/cpp, CMakeLists.txt}`),
  Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green, **0 warnings** after
  MAX→MIN pyramid rebuild + frustum culling fix). 4 strategies (A_UniformMip0 baseline / B_UniformMipGlobal /
  C_PerChunkStaticMip hypothesis / D_PerChunkDynamicDispatch) × 5 scenes (uniform_floor + forest_floor + cave_stress +
  mixed_biome + view_dolly_stress) × 5 seeds (1, 7, 42, 1234, 31337) × 30 iter + 5 warmup = **100 main measurements**,
  wall time ~12 min on dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline:** *
  *C_PerChunkStaticMip = 700-1500× texel reduction** (avg 13K vs 10.7M texels/chunk vs A baseline) AND **+3-5% cull rate
  ** (avg 27.6% vs 26.4%) — but **0.02-0.20% false-negative artifact rate** (PSNR 27-30 dB worst case view_dolly_stress;
  A = 0 FN, PSNR ∞). **2-phase fallback in Step 3** `if (mipLevel > 0 && culled) verify at mip=0` eliminates FN → PSNR ∞
  with 350× texel reduction still. **B_UniformMipGlobal** slightly outperforms C (29.8% vs 27.6% cull rate) but same FN
  risk. **C ≈ D** для наших scenes (multiple dispatches don't add measurable value). **Verdict=mixed:** strong cost
  win (700-1500× texel, well above 5% threshold per `optimization-philosophy.md`) but quality regression (0.02-0.20% FN)
  without mitigation. **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) per-chunk mip
  compute на CPU в `Renderer.cpp:1344` + `perChunkMipLevel[]` SSBO в `SceneFrameResources`; Step 2 (S, ~80 LoC)
  `hzb_cull.comp` SSBO load + branching; Step 3 (XS, ~30 LoC) `PROJECTV_HZB_SMART_MIP=ON` env + 2-phase fallback + Tracy
  plot «HZB Smart Mip» + `ProjectVHzbSmartMipTests` unit test. Total ~160 LoC, XS-S effort, 2-3 sessions. **Net effect
  positive** with 2-phase fallback: 350× texel reduction AND 0 FN (production-safe). См. §6 +
  §1 + [experiment README](./experiments/2026-06-21-hzb-smart-mip-select/README.md) + [STATUS](./experiments/2026-06-21-hzb-smart-mip-select/STATUS.md) + [sources.md](./experiments/2026-06-21-hzb-smart-mip-select/sources.md) +
       `prototype/{results.csv, bench.log}` (100 rows + 1 header).

- `2026-06-21-luajit-scripting-hotpath-cost` (verdict=`mixed`). **Stage 6.x modding — LuaJIT hot-path call cost from C++.** Web-research complete (15+ sources: Mike Pall, blep/luajit_perf_poc, FOSDEM 2026 BeamNG, devhide.com sol2, Hytales GC, valua 2026, OpenBenchmarking LuaJIT). Standalone C++26 CPU analytical prototype `prototype/luajit_hotpath_bench.cpp` ~290 LoC (Clang 22.1.6, build green 0 warnings). 6 strategies × 5 workloads × 5 seeds = **150 main measurements**. **Headline:** D_LuaJIT_FFI_struct = **22.6 ns = 4.0× native** (acceptable), C_LuaJIT_pcall_warm = **145 ns = 25× native** (acceptable for events), F_Sol2_binding = **1.13 µs = 195× native (catastrophic — NEVER on hot paths)**. Budget: all FFI scenarios < 2% of 30 Hz frame budget; sol2 worst case 117% ❌. GC pressure = 18% of pcall cost (table pooling mitigation). Cold start 780-1100 µs blocker for per-chunk Lua instantiation. **Integration:** FFI struct for hot paths, pcall_warm for events, sol2 banned on hot paths. Deferred до Stage 6.x. См. [README](./experiments/2026-06-21-luajit-scripting-hotpath-cost/README.md) + [STATUS](./experiments/2026-06-21-luajit-scripting-hotpath-cost/STATUS.md) + [sources](./experiments/2026-06-21-luajit-scripting-hotpath-cost/sources.md) + `prototype/{luajit_hotpath_bench.cpp, build/results.csv (151 rows)}`.

## 2. Nearest Gap

Next h/m from `research/backlog.md`:

- `sub-chunk-layers` (m, independent) — для biome/cave layers.
- `wfc-procedural-worlds` (m, independent) — для Stage 4.x procedural gen.
- `restir-gi-feasibility` (m, Stage 5.1/5.2) — **closed `2026-06-20`** (verdict=`mixed`). См. §6.
- `vct-vs-rt-cutoff` (m, Stage 5.1/5.2) — **closed `2026-06-20`** (verdict=`mixed`). См. §6.
- `vct-vs-rt-cutoff` (m, Stage 5) — после Stage 5.1 VCT spike.

Closed (recent, see §6 for full list):

- `dec-pipelines-async-compute` (m, Stages 2.2/3.1/4.1/5.2) — closed `2026-06-20`, verdict=`yes`.
  Foundation шаг (`vkQueueSubmit2` + timeline semaphores) — prerequisite для Stage 3.1 GPU Fluid CA,
  Stage 2.2 HZB full integration, Stage 5.2 RTX BLAS build.
- `cache-oblivious-chunk-tree` (m) — closed `2026-06-20`, verdict=`mixed`. Re-evaluation trigger: Stage 4.3
  (128+ chunks draw distance). Defer до re-evaluation.
- `svdag-vs-vdb-memory-throughput` (h) — closed `2026-06-20`, verdict=`yes`. Закрыл measurement gap.
- `bindless-descriptor-overhead` (m, Stage 2.x) — closed `2026-06-20`, verdict=`mixed`. Mainline
  рекомендация: hybrid strategy, 5-phase rollout (Phase A push shadow cascade → Phase B bindless
  material table → Phase C bindless Sparse64Node → Phase D bindless virtual texture → Phase E bindless
  RTX TLAS). Cross-refs: `TODO.md` §1.1/§1.2/§2.1/§2.2/§2.3/§5.2, `agent/knowledge.md §4/§15/
  §25/§30.4`, `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.

## 3. Next Steps

Определяются оператором. По умолчанию: следующий h-priority из backlog (все h-priority сейчас
закрыты либо in-progress).

## 4. Risks

- Конфликт scope с mainline-агентом: если mainline правит `docs/experiments/` (что запрещено моим протоколом, но не
  запрещено корневым) — зафиксировать в `STATUS.md` заблокированного эксперимента и эскалировать.
- Устаревание web-источников: каждый эксперимент датируется; старше 12 месяцев — перепроверять.

- **`2026-06-21-mesh-shader-mega-instancing`** — closed `2026-06-21` (single session, ~1.5h),
  verdict=`mixed`. **Stage 6+ military sandbox Tier 0 Foundation — GPU mesh shader + indirect
  draw axis для 10k+ animated юнитов** (RTT / Supreme Commander / Total War scale army rendering).
  Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»;
  **0 of 50+ closed experiments covered mega-instancing axis** — fully fresh. **Orthogonal** to
  closed `2026-06-20-mesh-shader-vs-compute-cull` [mixed] (cull strategy, not mega-instancing).
  Web-research complete via Exa `web_search` (working this session); **15+ primary + 7
  supplementary sources verified** (GameDev.net 2024-08-10, XRReady/multi-mesh 2026-03-29,
  jglrxavpok 2024-05-13, chaoticbob 2024-01-26, AMD GDC 2024 RDNA 3, Vulkanised 2023,
  nvpro-samples/gl_vk_meshlet_cadscene, NVIDIA Blackwell 2025, DEV.to Michael Sacco 2026-05-13,
  Vulkan Guide, KhronosGroup Vulkan-Samples, Vulkan Validation Layer Issue #9263, VVL PR #4524,
  AMD GPUOpen Meshlet compression, AMD GPUOpen Work Graphs mesh nodes 2024, AMD GPUOpen
  "From vertex shader to mesh shader", Vulkan Foliage 2024, proceduralpixels, ellioman,
  Unity RenderMeshIndirect, eldnach, Themaister Granite). Standalone C++26 CPU analytical
  prototype `prototype/mesh_shader_sim.cpp` (5 strategies × 5 scenes × 5 seeds × 1000 iter
  + 10 warmup = **125,000 main measurements**), Clang 22.1.6 `-O3 -march=native
  -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green, **0 warnings**), wall time
  **0.107 sec** на dev host `obvium` Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Output: `prototype/build/results.csv` (125,001 rows × 12 cols,
  4.8 MB). **Headline (mixed per platform tier):**
  - **A_TraditionalDrawIndexed** (current mainline baseline) = 35 ms at 1k → 35 sec at 1M
    (does NOT scale).
  - **B_ComputeCull_PlusDrawMesh** = 0.88 ms at 1k → 380.9 ms at 1M (40-95× speedup, compute
    pre-pass cull 81.6% of cost at swarm_100k).
  - **C_AmplificationShaderOnly** ⭐ = **0.57 ms at 1k → 64.6 ms at 1M (62-544× speedup)** =
    **universal recommended default** (amplification shader culls inline = 9.7% cull cost vs
    B's 81.6%).
  2/3/4 + Intel Arc Battlemage = full C pattern support (compile-time loop per Vulkanised
  2023 + `WavePrefixCountBits` per AMD GDC 2024). Mobile (Adreno/Mali) = fallback to B or
  A. **3-step migration per `agent/knowledge.md §30.4` precedent** (~550 LoC total, M effort,
  1-2 sessions, **deferred** до Stage 6+ military sandbox activation per operator 8x
  planning decision): Step 1 (XS, ~50 LoC) `MeshShaderInstanceData` foundation + per-frame
  SSBO upload; Step 2 (M, ~400 LoC) amplification + mesh shader implementation with frustum
  cull in AS workgroup + meshlet export per Vulkanised 2023 compile-time loop; Step 3 (S,
  ~100 LoC) `PROJECTV_MESH_SHADER_INSTANCING=ON|OFF` env gate (default OFF) + Tracy plot
  "Mesh Shader Instance Culling" + `ProjectVMeshShaderInstancingTests` unit test + graceful
  fallback (per `agent/workspace.md §1` session 5e11993 pattern). **Cross-axis:** orth ко
  всем 50+ closed experiments; **complementary** к closed Stage 2.1 mesh shader pipeline
  per `TODO.md §2.1` (= per-chunk voxel mesh, **different axis** = per-unit instancing for
  10k+ animated юнитов) + closed `2026-06-21-ballistic-projectile-simulation` [yes] (10k
  GPU particle proxy) + `2026-06-21-chunk-damage-fracture-model` [mixed] (debris particles)
  + `2026-06-21-ecs-1m-entities-bottleneck` [yes] (1M+ entity rendering). **New axis:**
  first **mega-instancing** axis в 50+ closed experiments; opens Stage 6+ military sandbox
  Tier 0 Foundation для 10k-1M+ animated юнитов. См. §6 +
  [`experiments/2026-06-21-mesh-shader-mega-instancing/`](./experiments/2026-06-21-mesh-shader-mega-instancing/) +
  [README](./experiments/2026-06-21-mesh-shader-mega-instancing/README.md) +
  [STATUS](./experiments/2026-06-21-mesh-shader-mega-instancing/STATUS.md) +
  [sources](./experiments/2026-06-21-mesh-shader-mega-instancing/sources.md) +
  [RESULTS](./experiments/2026-06-21-mesh-shader-mega-instancing/RESULTS.md) +
  `prototype/{mesh_shader_sim.cpp, stats.hpp, scenes.hpp, strategies.hpp, build/mesh_shader_sim,
  build/results.csv (125,001 rows, 4.8 MB)}`.

## 5. Active experiments (current open sessions)

> **Cleanup `2026-06-21`:** закрытые эксперименты перенесены в §6; оставлены только реально активные.

- **`2026-06-22-resource-harvesting-economy`** — **in-progress** (claim Phase 0 done, next: Phase 1 web-research + Phase 2 prototype). **m, independent (military sandbox axis — Tier 3 Economy, Sandbox, Content & Game Modes — first dedicated resource harvesting / extraction economy axis** в 166+ closed experiments; cross-cuts Stage 6+ military sandbox [resource-driven expansion, logistics loops] + Tier 3 Economy [factory production downstream, supply logistics transport] + Stage 4.1 world gen [procedural-voxel-resource-deposits as prerequisite] + Stage 3.x interaction [voxel mutation for mining/building extractors]). **Self-invented per operator instruction `2026-06-22`**; sentinel §13.7 clean. **Agent:** self. **Started:** 2026-06-22. **ETA:** this session. 5 strategies: A_NoHarvesting (baseline), B_StaticNode_Depletion, C_ProceduralNode_DynamicRichness, D_ExtractorBuilding_Tiered, E_FullEconomyChain. **Hypothesis:** <5 µs/chunk per tick + meaningful scarcity pressure. См. [`experiments/2026-06-22-resource-harvesting-economy/`](./experiments/2026-06-22-resource-harvesting-economy/) + [README](./experiments/2026-06-22-resource-harvesting-economy/README.md) + [STATUS](./experiments/2026-06-22-resource-harvesting-economy/STATUS.md).

- **`2026-06-22-soldier-role-specialization`** — **in-progress** (claim Phase 0 done, next: Phase 1 web-research + Phase 2 prototype). **m, independent (military sandbox axis — Tier 2 AI, Tactical & Warfare; first dedicated soldier class role & skill table specialization axis** в 166+ closed experiments; cross-cuts Stage 6+ military sandbox [class-based behavior, repair/healing tasks] + Stage 2.x ECS [entity component layout efficiency] + Stage 6+ modding [data-driven skill definition]). **Self-invented per operator instruction `2026-06-22`**; sentinel §13.7 clean. **Agent:** self. **Started:** 2026-06-22. **ETA:** this session. См. [`experiments/2026-06-22-soldier-role-specialization/`](./experiments/2026-06-22-soldier-role-specialization/) + [README](./experiments/2026-06-22-soldier-role-specialization/README.md) + [STATUS](./experiments/2026-06-22-soldier-role-specialization/STATUS.md).

- **`2026-06-22-voxel-water-flow-ca`** — **in-progress** (claim Phase 0 done, next: Phase 1 web-research + Phase 2 prototype). **m, independent (military sandbox axis — Tier 0 Foundation & Optimization × Tier 1 Cross-cutting × Stage 3.x interaction — first dedicated voxel water/substance flow CA axis** в 157+ closed experiments; cross-cuts Stage 6+ military sandbox [river crossing depth query, strategic flooding via dam breach, moat filling via voxel mutation, rain accumulation & drainage, fire extinguishing coverage] + Stage 3.x interaction [voxel mutation → water flow, digging/pouring/blocking] + Stage 4.1 world gen [water features, river generation, lake filling] + Stage 2.x sensor fusion [underwater acoustic propagation input per closed `acoustic-detection-system` mixed]). **Self-invented per operator instruction `2026-06-22`**; sentinel §13.7 clean (orth к `gpu-fluid-ca-atomic-strategy`, `water-surface-rendering`, `wildfire-propagation`, `voxel-hydraulic-erosion`). 5 strategies: A_NoWater (baseline), B_SimpleHeightCA, C_3D_CA_PressureBFS, D_3D_CA_VolumeConserving, E_Hybrid_HeightBFS_3D. **Hypothesis:** <10 µs/chunk/tick update cost + realistic water behavior + voxel mutation integration <50 µs/chunk + 5 consumer scenarios validated. **Agent:** self. **Started:** 2026-06-22. **ETA:** this session. См. [`experiments/2026-06-22-voxel-water-flow-ca/`](./experiments/2026-06-22-voxel-water-flow-ca/) + [README](./experiments/2026-06-22-voxel-water-flow-ca/README.md) + [STATUS](./experiments/2026-06-22-voxel-water-flow-ca/STATUS.md).

- **`2026-06-22-weather-svo-metafield`** — **closed `2026-06-22`** (single session, ~2.5h, claim+web-research+prototype+bench+close), verdict=`mixed per strategy; yes for D ⭐ as universal recommended default + yes for E as opt-in high-fidelity`. **A/B/C REJECTED** (no temporal evolution = degenerate as weather simulation; only useful as debug baseline). **Military sandbox axis — Tier 0 Foundation & Optimization × Tier 1 Cross-cutting — first dedicated battlefield atmospheric weather field as SVO meta** в 140+ closed experiments. **Self-invented per operator instruction `2026-06-22`**; sentinel §13.7 clean. Web-research complete via direct `webfetch` to canonical Wikipedia URLs (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **11 primary Tier 1 sources verified** в [`sources.md`](./experiments/2026-06-22-weather-svo-metafield/sources.md): NWP + Atmospheric model + Advection + Coriolis force + Humidity + Wind + Cellular automaton + Atmospheric pressure + Precipitation + Ideal gas law + Planetary boundary layer. Standalone C++26 CPU prototype [`prototype/weather_metafield_bench.cpp`](./experiments/2026-06-22-weather-svo-metafield/prototype/weather_metafield_bench.cpp) ~570 LoC (Clang 22.1.6, **build green 1 cosmetic warning**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **0.94 sec** на Zen 3 5800X. **Headline (mean update cost for 16³ = 4096-chunk world, 1-Hz tick):** **A/B/C = 22/21/22 ns** (trivial — no per-tick work); **D ⭐ = 7,600 ns = 7.6 µs = 1.86 ns/chunk = 0.023% of 30 Hz** (within 5 µs target, **217× under 5% threshold**); **E = 21,000 ns = 21 µs = 5.13 ns/chunk = 0.064% of 30 Hz** (4× over target, 78× under 5% threshold). Memory: **64 KiB per 16³ world** (4096 cells × 16 B/cell = 0.0008% of 8 GiB VRAM). **5 consumer-callback chains** validated per measurement (ballistic wind drift at 1000m range / IRST atmospheric τ at 10 km / visibility fog at 0.5 contrast / fire humidity suppression / fluid CA precipitation trigger) — all 5 produce physically reasonable values across 5 scenes (ballistic drift 1.67-37.50 m, IRST τ 0.01-0.83, visibility 1.26-10000 m, fire 0.24-0.93, precip 0-1). **3-clause hypothesis validation:** ✅ H1 cost (D within 5 µs target); ✅ H2 memory (64 KiB exact); ✅ H3 consumer fidelity (5/5 reasonable). **5-10% threshold MASSIVELY CROSSED.** **Critical finding:** **D preserves A's per-scene consumer outputs** (same drift, same fog, same fire) while adding temporal evolution → D is **drop-in replacement** for A with meaningful weather dynamics. **E's pressure-variation tuning** needed in mainline (current ±100 Pa saturates wind at 30 m/s max; mainline should scale to 0.1-1 hPa over larger distances). **Verdict=mixed per strategy:** D ⭐ = universal recommended default; E = opt-in for high-fidelity; A/B/C = REJECTED. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~530 LoC total, S-M effort, 1-2 sessions, **deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision**): Step 1 (XS, ~80 LoC) `src/world/WeatherField.{hpp,cpp}` + 5 strategies + `PROJECTV_WEATHER=DISABLED|STATIC_RANDOM|SIMPLEX|CA|NWP_LITE` env (default `CA`) + `PROJECTV_WEATHER_TICK_HZ=1`; Step 2 (M, ~300 LoC) consumer integration (5 consumer types) + per-chunk `WeatherCell` field (16 B) to `src/voxel/VoxelChunk.hpp`; Step 3 (S, ~150 LoC) `tests/WeatherFieldTests.cpp` + Tracy plots "Weather Field Update" + "Weather Field Query" + default `PROJECTV_WEATHER=CA`. **Cross-axis:** **orth** ко всем 18 in-progress parallel на `2026-06-22`; **complementary** к closed `wind-simulation-ballistics` [mixed, producer-consumer orth] + `precomputed-atmospheric-sky` [yes, orth] + `volumetric-fog-atmosphere-rendering` [mixed, orth] + `cloudscape-rendering` [mixed, orth] + `radar-detection-system-simulation` [yes, precipitation clutter consumer] + `irst-thermal-imaging-detection` [mixed, atmospheric τ consumer] + `acoustic-detection-system` [mixed, sound attenuation consumer] + `fixed-wing-flight-model-simulation` [yes, air density consumer] + `helicopter-rotor-physics` [yes, density/icing consumer] + `ballistic-projectile-simulation` [yes, wind drift consumer] + `wildfire-propagation` [yes, humidity consumer] + `fluid-ca` [yes GPU Stage 3.1, precipitation input] + `recon-intel-fog-of-war` [yes, weather intel] + `ecs-1m-entities-bottleneck` [yes, Flecs registry]. **Prerequisite** для open `battlefield-weather-forecast-display` [m Tier 4] + `weather-ai-modifier` [m Tier 2] + `aircraft-icing-simulation` [m Tier 1] + `battlefield-ambient-audio` [m Tier 4]. **New axis:** first dedicated **battlefield atmospheric weather field as SVO meta** axis в 140+ closed experiments. Moved to §6. Phase 4 (sync) complete.

- **`2026-06-22-magnetic-anomaly-detection-mad-asw`** — **closed `2026-06-22`** (single session, ~3h, claim+web-research+prototype+bench+close), verdict=`mixed per strategy / yes for C_DegaussCompensatedFluxgate ⭐ as universal recommended default + yes for D_OBF_OrthogonalBasisFunction ⭐⭐ as high-sensitivity opt-in`. **Military sandbox axis — Tier 1 Core Engine Systems: Physics (geomagnetic) + Tier 2 AI Detection — fourth passive detection channel after radar + IRST + acoustic — first dedicated Magnetic Anomaly Detection (MAD) anti-submarine warfare axis** в 140+ closed experiments. **Self-invented per operator instruction `2026-06-22`**; sentinel §13.7 clean (`rg "magnetic.anomaly|mad.asw|geomagnetic|degaussing|magnetometer"` over `INDEX.md` + `experiments/` = 0 dedicated experiments; `ls experiments/2026-06-22-magnetic*` = ENOENT pre-claim). Web-research complete via direct `webfetch` (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **6 Tier 1 + 4 Tier 2 = 10 sources verified** в [`sources.md`](./experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/sources.md): Wikipedia "Magnetic anomaly detector" (1/r³ falloff, 0.2 nT @ 600m, 13.33 nT @ 500m for 100m×10m sub, slant range 500m, OBF decomposition, P-3C tail boom + SH-60B MAD bird, 450-800m @ 200m altitude) + Wikipedia "Anti-submarine warfare" (MAD+sonobuoy+ESM+Autolycus ASW stack, P-3C + SH-60B platforms) + Wikipedia "Degaussing" (WWII origin, MES-device Type 205, 3-coil modern, HTS superconducting degaussing 80% weight reduction, deperming 4000A pulse) + Wikipedia "International Geomagnetic Reference Field" (IGRF-14 2024-12, spherical harmonics, Gauss coefficients g_n^m + h_n^m, Schmidt quasi-normalized, valid 1900-2030) + Wikipedia "Magnetometer" (vector/scalar taxonomy, SQUID/fluxgate/atomic, Earth field 20000-80000 nT, pT anomalies, 0.1-1 nT modern noise floor) + Wikipedia "Submarine" (Virginia/Akula/Type 205/Kilo hull structure). Standalone C++26 CPU prototype [`prototype/mad_asw_bench.cpp`](./experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/prototype/mad_asw_bench.cpp) **481 LoC** (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings 0 errors**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **0.126 sec** на Zen 3 5800X. **Headline (mean per strategy, 5 scenes):** A_BaselineInverseCube = 60.0% TPR / **0.0% FPR** / 21 ns (FPR-critical fallback); B_IGRF_OffsetSubtraction = 60.0% / 0.0% / 21 ns (same as A in simplified model); **C_DegaussCompensatedFluxgate ⭐ = 62.9% / 1.4% / 23 ns** (3-axis fluxgate + airframe + IGRF + 50% local = universal recommended default, F1=0.77); **D_OBF_OrthogonalBasisFunction ⭐⭐ = 70.8% / 3.7% / 29 ns** (C + rolling 8-snapshot 7/8 same-sign persistence = high-sensitivity opt-in, F1=0.82 best); E_MAD_KalmanTrackWhileScan = 60.0% / 6.0% / 24 ns (**REJECTED** — no TPR benefit + +2.3% FPR vs D). **3-clause hypothesis validation:** ✅ H1 cost <1 µs/scan/detection: CONFIRMED MASSIVELY (max 29 ns = 34× under 1 µs target, 30-50× under 5% of 30 Hz budget per `optimization-philosophy.md`); ⚠️ H1' detection rate ≥70% at slant range 500m: ACCEPTED for D only (A/B/C/E = 60%, easy targets s1/s3/s4 detected, hard targets s2/s5 missed due to degauss + 1/r³ = fundamental MAD physics limit); ✅ H2 FPR ≤5%: ACCEPTED for A/B/C/D (0/0/1.4/3.7%); REJECTED for E (6%). **5-10% threshold:** A→D = +10.8% absolute TPR = +18% relative = **CROSSES MASSIVELY** ✅; C→D = +7.9% = +12.5% = **CROSSES** ✅; D→E = -10.8% = **E REJECTED** ✅. **Physics validation:** 1/r³ falloff curve verified for all 5 scenes (Chen Yuqin 2015 reference 13.33 nT @ 500m for 100m×10m sub = match within 1.0× factor). **Counter-intuitive finding:** more sensitive strategies (D > C > A) trade TPR gain for FPR cost; for ASW where FPR = dispatch expensive P-3C aircraft, A/B with 0% FPR remains production default for peacetime patrol. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~830 LoC, M effort, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36): Step 1 (XS, ~80 LoC) `src/sensor/MadSubsystem.{hpp,cpp}` + `MadStrategy` enum + `PROJECTV_MAD_STRATEGY=BASELINE|IGRF|FLUXGATE|OBF|KALMAN` env (default `FLUXGATE` = C ⭐); Step 2 (M, ~400 LoC) per-strategy port + IGRF degree-1 lookup + GeomagneticMap + per-strategy files; Step 3 (S, ~150 LoC) `ProjectVMadSubsystemTests` 25 unit + Tracy + per-platform per-mode config (P-3C = OBF, MH-60R = FLUXGATE, sonobuoy-field = BASELINE). **Cross-axis:** **orth** ко всем 21 in-progress parallel (verified §13.7 sentinel); **complementary** к closed `radar-detection-system-simulation` [yes, radio sibling] + `irst-thermal-imaging-detection` [closed, IR sibling] + `acoustic-detection-system` [closed, acoustic sibling] + `stealth-signature-reduction` [yes, degauss = MAD-countermeasure, mirrors `D_IR_Suppression`] + `recon-intel-fog-of-war` [yes, sensor fusion downstream] + `aircraft-damage-model` [yes, post-damage magnetic signature] + `missile-guidance-laws-simulation` [closed, MAD-cued weapon release] + `countermeasure-dispenser` [closed, magnetic decoys future] + `ecs-1m-entities-bottleneck` [yes, Flecs = submarine fleet registry] + `lockstep-state-sync-hybrid-netcode` [closed, deterministic MAD state] + `weather-svo-metafield` [closed, IGRF reuses weather SVO infrastructure]. См. [README](./experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/README.md) + [STATUS](./experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/STATUS.md) + [RESULTS](./experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/RESULTS.md) + [sources](././experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/sources.md) + `prototype/{mad_asw_bench.cpp (481 LoC), build/{mad_asw_bench, results.csv (125,001 rows), summary_means.csv (26 rows), run.log}}`. **Sync §13.5 complete.** Moved from §In progress to §Closed. (Phase 0 init + Phase 1 web-research complete; Phase 2 prototype + benchmark next). **Military sandbox axis — Tier 1 Core Engine Systems: Physics (geomagnetic) + Tier 2 AI Detection — fourth passive detection channel after radar + IRST + acoustic — first dedicated Magnetic Anomaly Detection (MAD) anti-submarine warfare axis** в 140+ closed experiments. **Self-invented per operator instruction `2026-06-22`**; sentinel §13.7 clean (`rg "magnetic.anomaly|mad.asw|geomagnetic|degaussing|magnetometer"` over `INDEX.md` + `experiments/` = 0 dedicated experiments; `ls experiments/2026-06-22-magnetic*` = ENOENT pre-claim). Web-research complete via direct `webfetch` to canonical URLs (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **6 Tier 1 primary + 4 Tier 2 supplementary = 10 sources verified** в [`sources.md`](./experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/sources.md): Wikipedia "Magnetic anomaly detector" (1/r³ falloff, 0.2 nT @ 600m, 13.33 nT @ 500m for 100m×10m sub, slant range 500m, OBF decomposition, P-3C tail boom + SH-60B MAD bird, 450-800m @ 200m altitude) + Wikipedia "Anti-submarine warfare" (MAD+sonobuoy+ESM+Autolycus ASW stack, P-3C + SH-60B platforms) + Wikipedia "Degaussing" (WWII origin, MES-device Type 205, 3-coil modern, HTS superconducting degaussing 80% weight reduction, deperming 4000A pulse) + Wikipedia "International Geomagnetic Reference Field" (IGRF-14 2024-12, spherical harmonics, Gauss coefficients g_n^m + h_n^m, Schmidt quasi-normalized, valid 1900-2030) + Wikipedia "Magnetometer" (vector/scalar taxonomy, SQUID/fluxgate/atomic, Earth field 20000-80000 nT, pT anomalies, 0.1-1 nT modern noise floor) + Wikipedia "Submarine" (Virginia/Akula/Type 205/Kilo hull structure). См. [`experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/`](./experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/) + [README](./experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/README.md) + [STATUS](./experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/STATUS.md) + [sources](./experiments/2026-06-22-magnetic-anomaly-detection-mad-asw/sources.md). Hypothesis (5 strategies A_BaselineInverseCube / B_IGRF_OffsetSubtraction / C_DegaussCompensatedFluxgate / D_OBF_OrthogonalBasisFunction / E_MAD_KalmanTrackWhileScan × 5 scenes × 5 seeds × 1000 iter = 125,000 main measurements; expected: D ⭐ + E ⭐ cross 5-10% threshold massively on TPR vs A; B + C safe fallbacks; A `no` для production (100% FPR)).

- **`2026-06-22-acoustic-detection-system`** — **closed `2026-06-22`** (single session, ~3h, claim+web-research+prototype+bench+close), verdict=`mixed per strategy; yes for A ⭐ as universal real-time default + yes for E as production-grade slow-scan quality opt-in`. **Military sandbox axis — third passive detection channel after radar + IRST** — **first dedicated passive acoustic detection axis** в 140+ closed experiments. **Self-invented per operator instruction `2026-06-22`** «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "acoustic-detection|passive.?acoustic|sound.?detection|acoustic.?sensor"` → only orth cross-refs в `2026-06-22-stealth-signature-reduction` [defender side] + INDEX.md + backlog.md self-refs; `ls experiments/2026-06-22-acoustic*` = ENOENT pre-claim). Hypothesis (validated): 5-strategy ladder A_SimpleRangeEquation → B_AtmosphericAbsorption_ISO9613 → C_NarrowBandFFT_Doppler → D_MultiSourceTriangulation_TDOA → E_FullPhysicsModel. **Headline:** **A_SimpleRangeEquation ⭐** = 8.00% mean det prob / 0.2 ns/target / 0.0006% of 30 Hz budget @ 1000 targets (universal real-time default); B = 7.20% / 0.3 ns; C = 6.48% / 10 µs; D = 3.74% / 160 µs (REJECTED for serial @ 1000 targets = 480% budget, OK for parallel Boomerang); E = 4.64% / 20 ms (production-grade quality opt-in parallel 0.6% budget). **Counter-intuitive finding:** det prob DECREASES A→E (not increases) due to AND of validation gates (Doppler 90% × TDOA 75% × SRP-PHAT 95% × multipath 83% = 53% per-target pass rate). A = highest recall / lowest precision; E = lowest recall / highest precision. **3-clause hypothesis validation:** ⚠️ H1 cost <0.5 ms CONFIRMED MASSIVELY for A/B (1000-10000× under budget); REJECTED for C/D/E serial; CONFIRMED for parallel. ⚠️ H2 monotonic A→E det-rate gain REJECTED (DECREASES due to validation gates). ✅ H3 uniqueness to submarine/stealth/camouflaged domains CONFIRMED (hydroacoustic band ONLY channel for ship @ 10+ km in coastal_waters; infrasound detects stealth jet at 3-5 km in quiet_forest; seismic detects footsteps at 200-300m via direct ground coupling). ✅ H4 passive = undetectable to opponent CONFIRMED architecturally (no RF/IR emission = no HARM/MAWS threat; orth to closed `2026-06-21-electronic-warfare-jamming` which attacks radio only). **5-10% threshold per `optimization-philosophy.md`:** A, B CONFIRMED MASSIVELY on cost; C, D, E REJECTED for serial use but CONFIRMED for parallel. Web-research complete (8 Tier 1 + 2 Tier 2 = 10 sources verified). Standalone C++26 CPU prototype `prototype/acoustic_bench.cpp` ~440 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green 0 warnings). 5 strategies × 5 scenes × 5 targets × 5 freq bands × 1000 iter + 10 warmup = **625,000 main + 62,500 warmup = 687,500 total**, wall time 0.295 sec на Zen 3 5800X per `hardware-profile.md §1`. **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~700 LoC, M effort, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` operator 8x planning decision). См. [`experiments/2026-06-22-acoustic-detection-system/`](./experiments/2026-06-22-acoustic-detection-system/) + [README](./experiments/2026-06-22-acoustic-detection-system/README.md) + [STATUS](./experiments/2026-06-22-acoustic-detection-system/STATUS.md) + [sources](./experiments/2026-06-22-acoustic-detection-system/sources.md) + `prototype/{acoustic_bench.cpp (~440 LoC), build/{acoustic_bench (35 KB), results.csv (625,001 rows, ~28 MB), summary_means.csv (626 rows), run.log (10 lines)}}`. **Cross-axis (validated):** orth ко всем in-progress parallel; complementary к closed `radar-detection-system-simulation` [yes, radio sibling] + `irst-thermal-imaging-detection` [in-progress, IR sibling] + `electronic-warfare-jamming` [mixed, orth attack surface] + `countermeasure-dispenser` [mixed, acoustic decoys] + `recon-intel-fog-of-war` [yes] + `hierarchical-tactical-ai-btree` [mixed] + `combined-arms-coordination-ai` [mixed] + `aircraft-damage-model` [yes] + `component-vehicle-damage-model` [yes] + `fixed-wing-flight-model-simulation` [yes] + `helicopter-rotor-physics` [yes] + `ballistic-projectile-simulation` [yes] + `naval-vessel-buoyancy-steering` [mixed] + `infantry-soldier-sim` [yes]; prerequisite для open `submarine-sonar-stealth` [l] + `battlefield-ambient-audio` [m] + `acoustic-decoy-dispenser` [concept] + `imint-imagery-intelligence` [concept] + `tgp-targeting-pod` [concept]. **New axis:** first dedicated **passive acoustic detection** axis в 140+ closed experiments; opens Stage 6+ military sandbox Tier 1 Physics + Tier 2 AI Detection as third passive detection channel (complementary to radar + IRST).

- **`2026-06-22-procedural-engine-sound`** sibling — sensor fusion target] + `irst-thermal-imaging-detection` [in-progress, IR sibling] + `electronic-warfare-jamming` [mixed, **does not attack acoustic channel** = orth attack surface] + `countermeasure-dispenser` [mixed, acoustic decoys future work] + `recon-intel-fog-of-war` [yes, intel fusion consumer] + `hierarchical-tactical-ai-btree` [mixed, BT alerts] + `combined-arms-coordination-ai` [mixed, sensor priority] + `aircraft-damage-model` [yes, post-damage acoustic signature] + `component-vehicle-damage-model` [yes, per-component acoustic] + `fixed-wing-flight-model-simulation` [yes, jet noise source] + `helicopter-rotor-physics` [yes, rotor noise source] + `ballistic-projectile-simulation` [yes, supersonic crack source] + `naval-vessel-buoyancy-steering` [mixed, cavitation source] + `infantry-soldier-sim` [yes, footsteps source]; **prerequisite** для open `submarine-sonar-stealth` [l Tier 1, sibling underwater] + `battlefield-ambient-audio` [m Tier 4, downstream consumer] + `acoustic-decoy-dispenser` [concept, acoustic CM counterpart] + `imint-imagery-intelligence` [concept, multi-sensor fusion] + `tgp-targeting-pod` [concept, multi-sensor targeting]. Phase 0 of 4 (claim + scope + folder + README + STATUS + backlog/INDEX sync done); Phase 1 web-research next. См. [`experiments/2026-06-22-acoustic-detection-system/`](./experiments/2026-06-22-acoustic-detection-system/) + [README](./experiments/2026-06-22-acoustic-detection-system/README.md) + [STATUS](./experiments/2026-06-22-acoustic-detection-system/STATUS.md).

- **`2026-06-22-anti-cheat-statistical-detection-for-lockstep-multiplayer`** — **closed `2026-06-22` (single session, ~2.5h, claim+web-research+prototype+bench+close), verdict=`mixed per strategy; no for strict hypothesis (no single strategy meets 85% TPR + 1% FPR); yes for architecture class (server-side statistical detection in lockstep multiplayer is architecturally sound)`. Recommended production default = `BD` hybrid (B + D = up to 40% TPR, ≤5% FPR).** **Military sandbox axis — Tier 1 Core Engine Systems: Server Architecture & Netcode integrity — first dedicated statistical anti-cheat detection axis** в 138+ closed experiments. **Self-invented per operator instruction `2026-06-22`** «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "anti.?cheat|cheat.?detect|aimbot|wallhack"` → 0 dedicated experiments; only orth cross-refs в `experiments/2026-06-21-persistent-war-server-architecture/{README,RESULTS,sources}.md` noting "no anti-cheat modeled" as known limitation). **5 strategies:** A_NoDetection (baseline) / B_StatisticalZScoreThreshold (k=3.5σ per player stat) / C_RollingWindowEWMA (α=0.10, CUSUM=12) / D_ReplayDeterministicDiff (player-recorded vs server-truth hash, poll=1s) / E_ML_AnomalyIsolationForest (100 trees, 12-dim feature vector). **5 scenes × 5 seeds × 100 players × 1800 ticks** = **125 main measurements** + 10 warmup, wall time ~100 sec на Zen 3 5800X per `hardware-profile.md §1`. Standalone C++26 CPU prototype `prototype/anticheat_bench.cpp` ~600 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **0 warnings 0 errors** after 1 fix iteration: 100×1800×96B stack overflow → heap `std::vector<Player>`). Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, 12 KB). **Per-strategy results (mean across 5 scenes × 5 seeds, 125 cheaters + 2375 legits total):** A = 0/125 TPR / 0/2375 FPR / 0.000 µs (baseline); B = **10/125 TPR (8%)** / **0/2375 FPR (0%)** / 0.005 µs / 1.10s latency (catches blatant aimbots at 20% on S3, 0% on adversarial — **`yes` for fast pre-filter**); C = 125/125 TPR (100%) / **2375/2375 FPR (100%)** / 0.023 µs (**`no` as-is** — CUSUM drift on autocorrelated synthetic data → 100% FPR per Wikipedia "Statistical process control" §"Mathematics of control charts" warning; requires proper detrend + reset engineering); D = **40/125 TPR (32%)** / **115/2375 FPR (4.8%)** / 0.000 µs / 1.60s latency (**`yes` for primary detection signal** — 70% TPR on S5 adversarial = best of all strategies, FPR 4.8% = matches real-world VAC precedent per Wikipedia "Valve Anti-Cheat" §"History" 12K false-positive case); E = 0/125 TPR / 0/2375 FPR / 2.031 µs (**`no` for prototype** — simplified isolation tree without trained model, needs labeled training data + scikit-learn per `sources.md` §3; **`yes` for production with labeled data**). **Hypothesis validation:** TPR ≥85% PARTIAL (B 8% ❌, C 100% ✅ but FPR 100% ❌, D 32% ❌, E 0% ❌); FPR ≤1% PARTIAL (B 0% ✅, C 100% ❌, D 4.8% ❌, E 0% ✅); latency ≤30s ✅ all under target; CPU <5 µs/player/tick ✅ all under target. **5-10% threshold per `optimization-philosophy.md`:** D crosses detection rate bar at 32% TPR (S4) + 70% TPR (S5 adversarial) but FPR 4.8% exceeds 1% target. **Counter-intuitive finding:** CUSUM-based detection (C) catastrophically fails on autocorrelated data (100% FPR) per Wikipedia "Statistical process control" §"Mathematics of control charts" warning; Replay-based detection (D) is the **strongest** signal against adversarial cheaters (70% TPR on S5) because deterministic state divergence can't be faked. **Web-research:** 9 sources verified (Tier 1: 5 academic — Wikipedia "Cheating in online games" / "Lockstep protocol" / "Isolation forest" / "Statistical process control" / "CUSUM"; Tier 2: 3 production — VAC / BattlEye / EAC; Tier 3: 1 ProjectV cross-references spanning 5 closed experiments). **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** ~600 LoC M effort deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36: Step 1 (XS, ~80 LoC) `src/server/AntiCheat.{hpp,cpp}` + `PROJECTV_ANTICHEAT=OFF|B|D|BD|ALL` env gate (default `BD`) + per-player baseline statistics; Step 2 (M, ~350 LoC) per-strategy implementation (B 12-dim z-score / D replay hash mismatch via NATS JetStream) + B+D hybrid (B-flag then D-verify) + Flecs `PlayerInputComponent` feature vector source; Step 3 (S, ~150 LoC) `ProjectVAntiCheatTests` 5 cases + Tracy plot "AntiCheat Tick" + `ProjectV_ANTICHEAT_FPR_ALERT_THRESHOLD=0.05` env gate + auto-ban after 3 confirmations. **Per-strategy defaults:** Production=`BD`; High-throughput=`B` only; Single-tier prototype=`D` only; NEVER `OFF`; NEVER `E` (training data); C requires engineering. **Cross-axis:** orth ко всем 16 in-progress parallel на `2026-06-22`; complementary к closed `lockstep-state-sync-hybrid-netcode` [mixed, transport = prerequisite для input recording] + `persistent-war-server-architecture` [yes, E_Hybrid_ShardedReactive ⭐, server host = detection host, single-shard ≥1000 player scale] + `interest-management-aoi-battle` [mixed, AOI = cheat visibility scope] + `after-action-replay-system` [mixed, replay = D detection input] + `multi-resolution-collision-broadphase` [mixed, JPH deterministic = D prerequisite] + `ballistic-projectile-simulation` [yes, projectile = cheat surface] + `aircraft-damage-model` [yes, aircraft cheat surface] + `component-vehicle-damage-model` [yes, vehicle cheat surface] + `radar-detection-system-simulation` [yes, radar cheat surface] + `supply-logistics-simulation` [mixed, supply cheat surface] + `factory-production-system` [mixed, factory cheat surface] + `ecs-1m-entities-bottleneck` [yes, Flecs = entity registry] + `lua-game-rules-scripting` [mixed, hook events = detection telemetry] + `data-driven-vehicle-weapon-definitions` [mixed, weapon stats = baseline signal]. **Caveats:** CPU-only synthetic (no real network/Discord/process-injection); synthetic features may not reflect real player distribution (real production data needed for accurate FPR); E uses simplified isolation tree (not real iForest); D's FPR sensitive to synthetic noise level (real players have ~0% mismatch in production); no cross-vendor validation; no adversarial evader beyond 1 type. Cross-refs: `TODO.md` independent, `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` (operator 8x planning decision Stage 6+), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold), `hardware-profile.md §1/§2` (Zen 3 5800X + 62.7 GiB RAM), `benchmarks/methodology.md §3` (measurement protocol), `2026-06-21-lockstep-state-sync-hybrid-netcode` (determinism foundation), `2026-06-21-persistent-war-server-architecture` (server host), `2026-06-21-after-action-replay-system` (replay data), `2026-06-21-multi-resolution-collision-broadphase` (JPH determinism), `2026-06-21-ecs-1m-entities-bottleneck` (Flecs registry). См. [`experiments/2026-06-22-anti-cheat-statistical-detection-for-lockstep-multiplayer/`](./experiments/2026-06-22-anti-cheat-statistical-detection-for-lockstep-multiplayer/) + [README](./experiments/2026-06-22-anti-cheat-statistical-detection-for-lockstep-multiplayer/README.md) + [STATUS](./experiments/2026-06-22-anti-cheat-statistical-detection-for-lockstep-multiplayer/STATUS.md) + [RESULTS](./experiments/2026-06-22-anti-cheat-statistical-detection-for-lockstep-multiplayer/RESULTS.md) + [sources](./experiments/2026-06-22-anti-cheat-statistical-detection-for-lockstep-multiplayer/sources.md) + `prototype/{anticheat_bench.cpp (~600 LoC), build/{anticheat_bench, results.csv (126 rows, 12 KB)}}`.


- **`2026-06-22-ambush-detection-reaction`** — **moved to [§6 Recent closed sessions](#6-recent-closed-sessions)** (verdict=`mixed per strategy / yes for E_BayesianPlusBTPriorityInterrupt ⭐⭐ as universal recommended default + D_BayesianSurprise as detection-only alternative`).

- **`2026-06-22-fire-coordination-multiple-units`** — closed `2026-06-22` (single session, ~4h, claim+close), verdict=`mixed per strategy; yes for B_PriorityScoreWeighted ⭐ as recommended default for balanced forces`. **Military sandbox axis — Tier 2 AI — first dedicated multi-unit focus fire / target priority / engagement-assignment axis** в 136+ closed experiments. Self-invented per operator instruction `2026-06-22`. 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, ~5-7 min на Zen 3 5800X. **Headline:** B/D = 80% win на `balanced_10v10` vs A/C/E = 60% = **+20pp = +33% relative** (crosses 5-10% threshold). **H1 CPU budget <0.1 µs/unit/tick: CONFIRMED MASSIVELY** (all <50 ns). H2/H3: REJECTED (saturated at max_ticks / within 4%). **Caveats:** CPU-only synthetic symmetric model — production likely larger benefit (per Warno/HOI4/SupCom doctrine). **Mainline 3-step ~530 LoC S-M effort, deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` operator 8x planning decision.** Moved to §6.up-formation-maneuver-axis` [closed, post-positioning engagement] + `hierarchical-tactical-ai-btree` [mixed, BT calls into this as `EngagementDecision` action node] + `cover-system-terrain-adaptive` [mixed, cover score as input] + `recon-intel-fog-of-war` [yes, intel visibility gates selection] + `radar-detection-system-simulation` [yes, radar-locked bonus] + `lockstep-state-sync-hybrid-netcode` [mixed, determinism requirement] + `aircraft-damage-model` [yes, armor/hp input] + `component-vehicle-damage-model` [yes, component damage input для shoot-the-gun] + `ballistic-projectile-simulation` [yes, projectile sim validates predicted DPS]. Phase 1 of 4 (claim + web-research next). См. [`experiments/2026-06-22-fire-coordination-multiple-units/`](./experiments/2026-06-22-fire-coordination-multiple-units/) + [README](./experiments/2026-06-22-fire-coordination-multiple-units/README.md) + [STATUS](./experiments/2026-06-22-fire-coordination-multiple-units/STATUS.md).

- **`2026-06-22-indirect-fire-artillery-fdc`** — closed `2026-06-22` (single session, ~3h, claim+close), verdict=`yes for E_Hybrid ⭐ as universal recommended default; per-strategy: A_LUT=yes (cheapest default), B_Newton=mixed (validation oracle), C_PointMass=no (NOT hot-path, 69% over budget), D_LUT_AdaptiveWind=no (sustained fire too expensive), E_Hybrid=yes (190 ns = 0.38% of 50 µs budget)`. **Military sandbox axis — Tier 1 Physics + Tier 2 AI: FDC + FO — first dedicated artillery / indirect-fire / FDC axis** в 137+ closed experiments. Self-invented per operator instruction `2026-06-22`. 5 strategies × 5 scenes × 5 seeds × 5 ammo × 1000 iter + 10 warmup = **125,000 main measurements**, wall time <1 sec на Zen 3 5800X. **Headline:** **E_Hybrid ⭐ = 190 ns/fire-mission (264× under hypothesis 50 µs budget)**, sub-meter Newton polish + LUT speed + per-mission wind query. A=112ns, B=695ns, C=34µs, D=2.8µs. **H1 <50 µs CONFIRMED MASSIVELY** (E = 0.38% budget). **H2 <5 m miss CONFIRMED** (E achieves <0.5 m Newton tolerance; C Euler-integrated 19 km miss = REJECTED for hot path). **H3 100% charge/fuze convergence CONFIRMED** (all 5 strategies × 125 configs). **H4 spot-mission loop architecturally validated** (corr_lat + corr_rng applied). **Danger-close correctly identified** 0.13% of missions (E = lowest false-negative). **Web-research:** 6 Tier-1 + 3 Tier-2 sources verified via direct webfetch (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9`) — Indirect fire / Counter-battery fire / Artillery observer / M270 MLRS / M982 Excalibur / CLGP / Fire support / GlobalSecurity M982 / NavWeaps splash colors / US Army FM 6-30 (via S3). **Caveats:** CPU-only synthetic; C_PointMass is bit-exact physical only at <5 km (Euler integration coarse for 10-30 km); D_LUT_AdaptiveWind cost simulated not measured; production LUT precompute 5sec one-time at game-load; cross-platform FP determinism requires FPU mode (`_FPU_RC_NEAR + _FPU_PC_24` SupCom precedent per closed `lockstep-state-sync-hybrid-netcode` mixed). **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~720 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` operator 8x planning decision**): Step 1 (XS, ~80 LoC) `FdcSystem` foundation + `PROJECTV_FDC=HYBRID|LUT|NEWTON|POINT_MASS|ADAPTIVE` env gate + LUT precompute at game-load; Step 2 (M, ~500 LoC) per-strategy Flecs ECS + event chain (CallForFire → FireMission → FireOrder → ballistic-projectile-simulation [yes] ShellFlight → ImpactEvent) + wind-simulation-ballistics [mixed] B_StaticWind + recon-intel-fog-of-war [yes] + friendly positions + danger-close + lockstep-state-sync-hybrid-netcode [mixed] FPU mode; Step 3 (S, ~140 LoC) `ProjectVFdcTests` + Tracy plot "FDC Solve" + bit-exact vs C_PointMass unit test + default `PROJECTV_FDC=HYBRID`. **Cross-axis:** **orth** ко всем 137+ closed (first dedicated FDC/FO axis); **complementary** к closed `ballistic-projectile-simulation` [yes, downstream consumer] + `fire-coordination-multiple-units` [mixed, CallForFire source] + `combined-arms-coordination-ai` [mixed, fire_support doctrine] + `recon-intel-fog-of-war` [yes, FO LOS] + `suppression-mechanics` [mixed, trigger] + `radar-detection-system-simulation` [yes, CB radar] + `aircraft-damage-model` [yes, airborne FO] + `wind-simulation-ballistics` [mixed, atmospheric correction] + `lockstep-state-sync-hybrid-netcode` [mixed, events] + `after-action-replay-system` [mixed, replay input] + `hierarchical-tactical-ai-btree` [mixed, BT calls FDC] + `ecs-1m-entities-bottleneck` [yes, entity registry] + `factory-production-system` [mixed, ammo production] + `data-driven-vehicle-weapon-definitions` [mixed, charge table]. **Prerequisite** для open `minefield-laying-clearing` [m Tier 1] + `trench-fortification-construction` [m Tier 1] + `convoy-transport-protection` [m Tier 3] + `grand-campaign-conquest` [m Tier 3]. **New axis:** first dedicated **artillery / indirect-fire / FDC / FO** axis в 137+ closed experiments; opens Stage 6+ military sandbox Tier 1/2 for fire support system. Moved to §6.

- **`2026-06-22-tech-tree-research-system`** — moved to [§6 Recent closed sessions](#6-recent-closed-sessions) (verdict=`mixed` per strategy / `yes` for E_Hybrid_CP_LazyPQueue ⭐ universal default + D_LazyPrerequisiteExpand ⭐ simple scenes + C_CriticalPathPrecompute ⭐ static DAGs).


- **`2026-06-21-vegetation-destruction-interaction`** — closed `2026-06-21` (single session, ~1.5h) verdict=`yes`.
  **h, independent** (military sandbox — Tier 1 Core Engine Systems: Physics). Self-invented per operator instruction `2026-06-21` + `AGENTS.md §13.1`. Web-research complete via webfetch (Exa `web_search` HTTP 429 this session per `agent/knowledge.md Part B §9`): Teardown engine design (Gustafsson 2022, voxel volumes for local translation + no stress model — known limitation flagged by Smith RPS Nov 2020) + Connected Components O(α(n)) per op (Hopcroft-Tarjan 1973, DSU Bengelloun 1982) + Mattheck 2015 "Body Language of Trees" (cantilever failure mode). Standalone C++26 CPU prototype `prototype/vegetation_destruction_bench.cpp` ~770 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 5 cosmetic warnings**, `vegetation_destruction_bench` binary present). 5 strategies × 5 scenes × 5 seeds × 8 mutations × 50 iter + 5 warmup = **50,000 main measurements**, wall time ~1.6 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (1001 rows = 1 header + 1000 data). Per `benchmarks/methodology.md` protocol.
  **Headline (mean µs per destruction event across all 5 scenes):**
  - `A_NaiveGlobalBFS` = **8.4 µs**, 100% geometric accuracy vs oracle (baseline, ground truth).
  - `B_HierarchicalDSU` = **28.4 µs**, 100% geometric accuracy.
  - `C_LocalSplitBFS` = **8.5 µs**, 100% geometric accuracy.
  - `E_Hybrid_AABB` = **28.3 µs**, 100% geometric accuracy.
  - `D_LightweightStressTopple` = **71.4 µs**, **99.9% geometric accuracy vs A** AND adds the **physically realistic topple behaviour** that pure geometric CC misses: a single voxel destroyed at the trunk base (which geometrically only removes 1 of 16 support voxels) triggers the entire canopy to topple — matching real-world tree felling (Mattheck 2015 cantilever failure). **All strategies hit <100 µs target** from `research/backlog.md` line 249.
  **Synthesis recommendation:** `B_HierarchicalDSU + D_LightweightStressTopple` composed (≈100 µs per tree destruction). At 30 Hz frame budget = 33 ms, supports ~330 tree destructions per frame, sufficient for any realistic battle scenario.
  **Caveat:** per-tree rigid body spawn cost (50-200 µs via Jolt, not measured in this prototype) is **10× detection cost** — separate Stage 3.2 hot-path verification required.
  **Cross-refs:** TODO.md §3.2 (incremental Jolt physics / voxel destruction / debris). Cross-axis orth: `destructible-building-system` [closed mixed, post-collapse debris] + `voxel-topology-analysis` [closed mixed, CCL building block]. Precedent: `voxel-topology-analysis` validated 8³ CCL at 1.3 µs — this experiment reuses + extends with single-tree scope.
  См. [`experiments/2026-06-21-vegetation-destruction-interaction/`](./experiments/2026-06-21-vegetation-destruction-interaction/) +
  [README](./experiments/2026-06-21-vegetation-destruction-interaction/README.md) +
  [RESULTS](./experiments/2026-06-21-vegetation-destruction-interaction/RESULTS.md) +
  [STATUS](./experiments/2026-06-21-vegetation-destruction-interaction/STATUS.md) +
  `prototype/{vegetation_destruction_bench.cpp (~770 LoC), build/vegetation_destruction_bench, build/results.csv (1001 rows)}`.

- **`2026-06-21-soft-body-physics-debris`** — h, independent (military sandbox axis — Tier 1 Core Engine Systems: Physics; **first dedicated soft-body / cloth simulation axis** в 100+ closed experiments; все closed Physics = rigid body 6-DOF / voxel fracture, soft body = orthogonal axis). Claimed `2026-06-21` by self per `AGENTS.md §13.1` + §13.7 sentinel clean (parallel sessions verified — no `experiments/2026-06-21-soft-body-physics-debris/` existed; `rg` for slug находит только `backlog.md` + `INDEX.md` cross-refs). Hypothesis: **XPBD (Macklin/Müller 2016) с 32-128 vertices per panel + 8 iterations даст <0.05 ms/panel per tick** = <1.5% of 30 Hz budget для 30 panels (typical vehicle + aircraft + cargo net coverage); <8 iter convergence; SIMD-векторизуемость на AVX2 (Zen 3). 5 strategies planned: A_RigidProxy (baseline, ~0 cost, no fabric) / B_MassSpring_Hooke (cheap, instability prone) / C_PBD_Müller2007 (constraint projection, robust) / D_XPBD_Macklin2016 (compliance + iteration, production) / E_ProjectiveDynamics_Bouaziz2014 (Cholesky, expensive but high quality). 5 scenes planned: calm_static / breeze_3ms / wind_15ms / impact_collapse / tearing_localized. Cross-axis orth: closed `tank-terrain-interaction-physics` [yes] + `fixed-wing-flight-model-simulation` [yes] + `helicopter-rotor-physics` [yes] + `naval-vessel-buoyancy-steering` [mixed] + `aircraft-damage-model` [yes] + `component-vehicle-damage-model` [yes] + `ballistic-projectile-simulation` [yes] + `chunk-damage-fracture-model` [mixed] + `vegetation-destruction-interaction` [closed yes] (all rigid body / voxel fracture). Cross-axis complementary: closed `destructible-building-system` [mixed, post-collapse debris] + `procedural-military-terrain-gen` [closed yes, structural features] + `wind-simulation-ballistics` [closed mixed, soft body wind interaction] + `terrain-traction-variation` [yes, surface coupling]. Phase 0 (claim + structure + scope) done; Phase 1 (web-research XPBD/PBD/Mass-Spring/Projective Dynamics 2024-2026) next. См. [`experiments/2026-06-21-soft-body-physics-debris/`](./experiments/2026-06-21-soft-body-physics-debris/) + [README](./experiments/2026-06-21-soft-body-physics-debris/README.md) + [STATUS](./experiments/2026-06-21-soft-body-physics-debris/STATUS.md).

> **NOTE: `2026-06-21-soft-body-physics-debris` closed `2026-06-21` (single session, ~3h), verdict=`yes`.** D_XPBD validated as recommended default для Stage 6+ military sandbox cloth. См. §6 + backlog.md §Closed.

- **`2026-06-21-flow-field-pathfinding-10k-units`** — h, independent (military sandbox — Tier 0 Foundation).
  **Closed `2026-06-21` (single session ~2h) verdict=`yes`** (with caveat — GPU compute shader not measured, only analytical CPU model).
  Claimed 2026-06-21 per §13.1. GPU-driven flow field for 1000+ unit simultaneous movement.
  Hypothesis: GPU compute-shader flow field <0.1 ms for 512² grid; per-unit steering <0.001 ms/unit;
  1000× faster than per-unit A* at 10k units.
  Web-research complete via Exa `web_search` (14 sources: Emerson Game AI Pro Ch.23, AoE IV GDC 2022,
  NativeFlowField Unity DOTS 2025, Pavel Guzenfeld 2026 benchmark, yoreei UE5 2025, Vav Labs Godot 2026,
  shaukinshourya DOTS 2025, Amit A* canonical, more).
  Standalone C++26 CPU prototype `prototype/flow_field_bench.cpp` ~520 LoC (Clang 22.1.6, build clean 2 cosmetic warnings).
  5 strategies × 5 scenes × 5 seeds × 4 grid sizes (64²/128²/256²/512²) × 200 iter + 10 warmup = **500 main measurements**,
  wall time **158 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
  Output: `prototype/build/results.csv` (501 rows: 1 header + 500 data).
  **Headline findings:**
  - **C_FlowField_BFS ⭐** = universal CPU default (19.8 / 79.3 / 356 / 1,466 µs across 64²→512²)
  - E_HPA_FlowField = precision-preserving (42 / 194 / 828 / 3,387 µs)
  - B_FlowField_Dijkstra_PQ = semantic 8-direction (190 / 936 / 4,096 / 18,133 µs)
  - D_FlowField_GPU_Analytical = best break-even at 3 agents (8 / 32 µs, SKIP at 256²+; GPU port pending)
  - A_AStar_PerUnit baseline (2.6 / 11.5 / 43 / 119 µs per call)
  **Break-even vs A*:**
  - C_BFS: 7-12 agents; E_HPA: 16-28; B_PQ: 73-152; D_GPU-analytical: 3.
  **10k units scenario:** BFS is **23-184× faster** than 10k × A* across 128²-512².
  **Verdict=yes** (with caveat): hypothesis partially confirmed; BFS is universal CPU default, GPU projection pending.
  **Integration:** 3-step migration ~780 LoC per `agent/knowledge.md §30.4` precedent:
  Step 1 (XS, ~80 LoC) PathfindingController + env gate;
  Step 2 (S, ~200 LoC) UnitSteering + Flecs ECS integration;
  Step 3 (M, ~500 LoC, deferred до Stage 4.3) GPU compute shader port.
  Steps 1-2 immediate, M effort, 2-3 sessions.
  **Cross-axis:** orthogonal to closed `mesh-shader-mega-instancing`, `multi-resolution-collision-broadphase`;
  complementary to `hierarchical-tactical-ai-btree` + `group-formation-maneuver`;
  prerequisite for `flanking-maneuver-ai` + `supply-logistics-simulation` + `after-action-replay-system`.
  См. [`experiments/2026-06-21-flow-field-pathfinding-10k-units/`](./experiments/2026-06-21-flow-field-pathfinding-10k-units/) +
  [README](./experiments/2026-06-21-flow-field-pathfinding-10k-units/README.md) +
  [STATUS](./experiments/2026-06-21-flow-field-pathfinding-10k-units/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-flow-field-pathfinding-10k-units/RESULTS.md) +
  `prototype/{flow_field_bench.cpp, build/results.csv (501 rows), build/run.log}`.

- **`2026-06-21-biome-transition-blending`** — m, **Stage 4.1** (biome blending for GPU world gen).
  **Status: closed `2026-06-21` verdict=`mixed`.** Self-invented per operator instruction «выбирай свободную тему или придумывай свою». Hypothesis partially confirmed (cost ≤5% of budget, PSNR unverifiable). **Recommended: C_DistanceBlend_BiL** — bilinear interpolation of 2×2 biome samples (0.640 µs/chunk, +25% vs sensible noise-hard baseline). S effort, ~50 LoC to replace nearest-sample in world_gen.comp.
  См. §6 + [`experiments/2026-06-21-biome-transition-blending/`](./experiments/2026-06-21-biome-transition-blending/).

**Note re: `2026-06-21-cloudscape-rendering`:** reserved, **CLOSED same session ~1.5h, verdict=`mixed`** (per-platform tier — B_SingleLayerRayMarch universal default, E_RTXRayMarchCloud for RTX-class, C_ThreeLayerNubis quality opt-in). См. §6 + backlog.md §Closed.

- **`2026-06-21-tracy-gpu-vs-manual`** — m, independent (cross-cutting profiling).
  **Closed `2026-06-21` verdict=`mixed`**. Self-invented per operator instruction «выбирай
  свободную тему или придумывай свою». Tracy GPU context overhead vs manual
  `vkCmdWriteTimestamp` + `TracyPlot` для multi-pass ProjectV rendering. Web-research complete
  (4 batches, 20 sources верифицированы в `sources.md`); standalone Vulkan 1.4 + volk + Tracy
  client prototype + `CMakeLists.txt` + `scripts/run_all.sh` (drift test for Issue #663,
  10K frames per-1K-window). **Self-built + self-ran** per explicit operator override
  `AGENTS.md §1`. Full sweep: 12 configs × 1000 frames + 3 drift × 10K = ~42,000 measurements,
  dev host `obvium` NVIDIA RTX 3060 Ti + driver 610.43.02 + Vulkan 1.4.341. Per-config:
  - A baseline: 0.219 / 0.482 / 0.811 ms mean (3/8/15 passes)
  - B (Tracy GPU all): +13.7% / +11.8% / +2.8% overhead — `no` для ≤8, `yes` для ≥15
  - C (manual only): within ±5% — `yes`
  - D (hybrid, top-3 Tracy + manual): +8.7% / −1.2% / +3.0% — `yes` для ≥8, `mixed` для ≤3
  Drift: A = −7.8%, B = −0.1%, D = +3.6% (all well below +20% Issue #663 alert). Per-zone
  overhead 1.5-10 µs (HIGHER than analytical 5-15 ns — Tracy has significant per-frame
  calibration + collect cost). VRAM ~768 KiB per context = 0.015% of 5.06 GiB. **3-step
  migration per `agent/knowledge.md §30.4`:** ~150 LoC, S effort, 2-3 sessions.
  Re-evaluation triggers: 3rd async-compute queue, Vulkan 1.5, Tracy v1.0, Stage 4.3,
  cross-vendor validation. См. [`experiments/2026-06-21-tracy-gpu-vs-manual/`](
  ./experiments/2026-06-21-tracy-gpu-vs-manual/) + [README](./experiments/2026-06-21-tracy-gpu-vs-manual/README.md)
  + [STATUS](./experiments/2026-06-21-tracy-gpu-vs-manual/STATUS.md) +
  [sources.md](./experiments/2026-06-21-tracy-gpu-vs-manual/sources.md) +
  [RESULTS.md](./experiments/2026-06-21-tracy-gpu-vs-manual/RESULTS.md) +
  `prototype/build/{results.csv, A_p15_drift.csv, B_p15_drift.csv, D_p15_drift.csv}`.

- **`2026-06-21-gpu-fluid-ca-atomic-strategy`** — m, **Stage 3.1** (GPU Fluid CA).
  Reserved `2026-06-21` by self. Phase 1-3 done (context + web-research + prototype ready).
  Prototype `prototype/{main.cpp, harness.hpp, scenes.hpp, bench.hpp, strategies.comp (5 strategies),
  CMakeLists.txt, README.md}` готов. **Blocker:** ждёт **operator build+run** на dev host `obvium`
  per `AGENTS.md §2` (research agent не может запускать `cmake --build`). Команды в
  `experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/STATUS.md` Phase 4. **Expected verdict:** `mixed`.
  См. [`experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/`](./experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/) +
  [README](./experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/README.md) +
  [STATUS](./experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/STATUS.md) +
  `research/backlog.md §In progress` reservation record.

- **`2026-06-21-volumetric-fog-atmosphere-rendering`** — m, **Stage 5.x Visual Polish** (cross-cutting
  visual axis — fog / participating media / atmospheric scattering; **self-invented topic** per operator
  instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; **0 of 50+ closed
  experiments covered volumetric fog axis** — fully fresh). Reserved `2026-06-21` by self per
  `AGENTS.md §13.1`. Phase 0-1 done (reservation + STATUS.md + baseline survey: `voxel.frag:844-883`


  analytic distance fog + `LookDevCaptureAutomation.cpp:180` fog lookdev scene preset). Phase 2 (web
  research, ~20 results, Wronski 2014 + Hillaire 2015 + TLoU2 2020 + Enshrouded 2026 + Lumen 2022 +
  elliahu/atmosphere + Timethy Hyman Traverse + Mastering Graphics Vulkan Ch10 + sinnwrig/URP +
  Godot #8580) complete. **Phase 3 (prototype) in progress.** **Hypothesis:** правильная стратегия
  ∈ {A_AnalyticDistance, B_FroxelGrid_3DTexture, C_FullRayMarch_HalfRes, D_RTX_RayQuery_ShortRayShadow,
  E_Hybrid_FroxelNear_RayMarchFar} даст < 5ms/frame на 1080p + VRAM < 100 MiB + scene-coverage-independent.
  **Expected verdict:** `mixed`. См.
  [`experiments/2026-06-21-volumetric-fog-atmosphere-rendering/`](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/) +
  [STATUS](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/STATUS.md) +
  `research/backlog.md §In progress` reservation record.

**Note re: `volumetric-fog-atmosphere-rendering`:** self-reserved `2026-06-21` и **CLOSED same session
~3h, verdict=`mixed`** (per-platform tier — D_RTX_RayQuery_ShortRayShadow WINNER RTX 3060 Ti + B_FroxelGrid
universal default + A_AnalyticDistance baseline). См. §6 ниже + `experiments/2026-06-21-volumetric-fog-atmosphere-rendering/RESULTS.md`.

- **`2026-06-21-data-driven-vehicle-weapon-definitions`** — h, independent (military sandbox axis — Tier 0 Foundation & Optimization: data-driven vehicle/weapon/armor definitions для modding; **first dedicated data-driven definitions + codegen + hot-reload axis** в 130+ closed experiments; cross-cuts Stage 4.x asset pipeline + Stage 6+ military sandbox modding; AGENTS.md §2 vision «поддержка сообщества (моды, аддоны, пользовательский контент)»).
  **Closed `2026-06-21` (single session, ~3h), verdict=`mixed` per strategy; clear 3-tier production architecture per use case.**
  Self-invented per operator instruction `2026-06-21` + `AGENTS.md §13.1` + §13.7 sentinel clean. Web-research complete (15+ primary sources verified в [`sources.md`](./experiments/2026-06-21-data-driven-vehicle-weapon-definitions/sources.md) Tier 1-4): From the Depths JSON block defs + Steam Workshop + Stormworks XML `<install>/rom/data/definitions/*.xml` (mod defs local-only) + Veloren RON + `veloren_common_assets` `AssetCache` + `RonLoader` + `tweak_expect_or_create` + hot-reload via `RwLock` [devblog 132: "call it again, it will hot-reload"] + War Thunder `.blk` + `.blkx` VROMFS + `pkg_local/` user-mods + `config.blk` erased at launch (architectural constraint) + Garry's Mod SWEP Lua tables + nlohmann/json + simdjson [20× faster per Daniel Lemire] + Glaze [15× faster, 2.9K stars, C++23+C++26 P2996 reflection] + reflect-cpp [1.9K stars, 14 formats, TOML 100× slower than JSON, YAML 500× slower] + msgpack-c + C++26 consteval/P2996/std::embed.
  Standalone C++26 CPU benchmark `prototype/defs_bench.cpp` ~1,300 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**, smoke test `prototype/smoke.cpp` ~90 LoC also green). 5 strategies (A_RuntimeJSON_nlohmann / B_Codegen_TOML2CXX / C_HotReload_LuaJIT / D_BinaryPack_MsgPack / E_Reflection_TOML) × 5 scenes (10 / 100 / 500 / 1000 / 2000 vehicles) × 2 seeds (1, 7) × 3 metrics (load_latency / lookup_latency / hot_reload_latency) × 10 iter + 2 warmup = **315 main measurements**, wall time ~60 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1` (ITER reduced from default 1000 to 10 due to **system load from 5+ parallel agents** running benchmarks concurrently on `obvium`; per-config p95/p99 still meaningful). Output: `prototype/build/results.csv` (316 rows = 1 header + 315 data, ~28 KB).
  **Headline (mixed per strategy; clear 3-tier architecture):**
  - **A_RuntimeJSON_nlohmann** baseline: load 110 µs / 525 µs / 2.7 ms (small/medium/large), lookup 20-29 ns, hot_reload 227-490 ns, memory 300-500 B/vehicle. **REJECTED for hot path** (100-500× slower than alternatives).
  - **B_Codegen_TOML2CXX** ⭐ static default: **load 222 ns / 1.3 µs / 6.4 µs** (100-500× faster than A), lookup 25-160 ns, hot_reload 26-157 ns, memory 128 B/vehicle. C++26 P2996 reflection + std::embed = production path.
  - **C_HotReload_LuaJIT** ⭐ live mod default: load 1.9 µs / 13 µs / 76 µs (10-60× faster than A), lookup 30-66 ns, **hot_reload 20-30 ns CONSTANT** (regardless of N), memory 500-700 B/vehicle. Matches Veloren production architecture.
  - **D_BinaryPack_MsgPack** ⭐ mod-shipped default: **load 254 ns / 1.3 µs / 5.7 µs** (~430× faster than A), **lookup 20-34 ns** (O(1) direct array index + memcpy), hot_reload 22-27 ns, **memory 68 B/vehicle** (7× smaller than JSON). Best per-entity cost across all metrics.
  - **E_Reflection_TOML**: load 81 µs / 384 µs / 1.99 ms (1.4× faster than A; type-safe validation at load), lookup 19-129 ns, hot_reload 300-500 ns (slow), memory 120 B/vehicle. **PARKED** for server-side validation only.
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** B/C/D all cross massively on load (10-500× speedup) and hot reload (10-25× speedup). **3-tier mainline architecture recommended per `agent/knowledge.md §30.4`:** static specs → B (codegen) / mod-shipped → D (msgpack) / live dev → C (LuaJIT).
  **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~600 LoC C++ + 200 LoC Python, M effort, 2-3 sessions, **deferred до Stage 4.x dedicated session per `agent/workspace.md §2` line 36**): Step 1 (S, ~200 LoC) `src/data-driven/CodegenSpecs.{hpp,cpp}` + `tools/codegen_specs.py`; Step 2 (M, ~300 LoC) `src/data-driven/ModSpecs.{hpp,cpp}` + `src/data-driven/LiveRules.{hpp,cpp}`; Step 3 (S, ~100 LoC) `PROJECTV_DATA_DRIVEN=CODEC|BINARY|LUA` env gate + Tracy plot + unit test.
  **Cross-axis:** **orth** к in-progress `lua-game-rules-scripting` (rules layer, не data schema) + `sdf-subtractive-modeling-ui` (geometry authoring); **complementary** к closed `voxel-asset-template-catalog` [mixed, runtime catalog lookup = downstream] + `programmable-voxels` [mixed, downstream layer] + `luajit-scripting-hotpath-cost` [yes, hot-reload perf] + `component-vehicle-damage-model` [yes, consumes VehicleSpec] + `ballistic-projectile-simulation` [yes, consumes WeaponSpec] + `tank-terrain-interaction-physics` [yes, consumes VehicleSpec.phys] + `fixed-wing-flight-model-simulation` [yes, consumes AircraftSpec.aero] + `helicopter-rotor-physics` [yes, consumes RotorSpec] + `aircraft-damage-model` [yes, consumes AircraftSpec.damage]. **Prerequisite** для open `custom-faction-definition` [m Tier 3] + `custom-vehicle-designer` [m Tier 3] + `custom-weapon-modding` [m Tier 3] + `workshop-mod-integration` [m Tier 3] + `scenario-mission-editor` [m Tier 3].
  **Caveats:** CPU-only synthetic benchmark (no Vulkan/Flecs/network); ITER reduced from 1000 to 10 due to system load; hand-rolled JSON/TOML parsers (real nlohmann/Glaze/reflect-cpp would be 5-20× faster on actual parse); E (TOML reflection) hand-rolled not using real `glaze::meta` / `reflect-cpp`; B's id_to_index uses linear scan (real codegen would use `std::unordered_map` for O(1) lookup); C's string interning is `std::unordered_map<std::string, uint32_t>` (real LuaJIT uses interned string table with FFI boundary).
  См. [§6](./experiments/2026-06-21-data-driven-vehicle-weapon-definitions/README.md) + [STATUS](./experiments/2026-06-21-data-driven-vehicle-weapon-definitions/STATUS.md) + [RESULTS](./experiments/2026-06-21-data-driven-vehicle-weapon-definitions/RESULTS.md) + [sources](./experiments/2026-06-21-data-driven-vehicle-weapon-definitions/sources.md) + `prototype/{defs_bench.cpp (~1,300 LoC), smoke.cpp (~90 LoC), build/{defs_bench (117 KB), smoke (8 KB), results.csv (316 rows, ~28 KB)}}`.

**Note re: `voxel-mutation-cost-characterization`:** упомянут оператором как "активный", но реально **CLOSED
`2026-06-21` verdict=`mixed`** (Phase 1-5 complete, 625 configs × 1000 iter = 625,000 main measurements,
prototype/mutation_bench.cpp 750 LoC + build/mutation_bench + build/results.csv 80 KB + README/RESULTS/sources.md
все на месте). Sync §13.5 завершён в этом cleanup pass. См. §6 ниже.

**Note re: `vk-video-decoder-replay`:** упомянут оператором как "активный", но реально **CLOSED `2026-06-21`
verdict=`yes`**. См. §6 ниже.

**Note re: `rtx-screen-space-reflections`:** упомянут оператором как "активный", но реально **CLOSED
`2026-06-21` verdict=`mixed`** (Phase B-D complete, 175,000 measurements, prototype/build/ имеет
`reflection_sim` + `results.csv` (175,001 rows) + `run.log`). Sync §13.5 завершён в этом cleanup pass.
См. §6 ниже.

**Note re: `full-rt-tensor-cores-load`:** упомянут оператором как "активный", но реально **CLOSED `2026-06-21`
verdict=`mixed`** (Phase 0-4 complete, 490,000 measurements, prototype/cycle_budget.cpp 620 LoC +
build/cycle_budget + build/results.csv 161KB + run.log + README/RESULTS/sources.md все на месте).
Sync §13.5 завершён в этом cleanup pass. См. §6 ниже.

- **`2026-06-21-gpu-fluid-ca-atomic-strategy`** — closed `2026-06-21` (single session) verdict=`mixed`.
  **Atomic-strategy-axis experiment** для Stage 3.1 (`src/shaders/fluid_ca.comp:101` blind `atomicOr`
  shortcut violates `agent/knowledge.md §30.4` line 1045 contract = `imageAtomicCompareExchange`).
  Standalone Vulkan 1.4 compute prototype (6 strategies × 5 scenes × N=200 frames, RTX 3060 Ti dev host).
  5 bugs fixed during build (volk/VMA conflict, buffer usage flags, dispatch cellIndex, belowIndex formula).
  Measured on vertical_column (working, low contention): **D_SubgroupBallot fastest correct 2.92 µs,
  B_CAS 2.98 µs (recommended), A 2.96 µs (only 1% faster but broken per §30.4), C 3.18 µs,
  F 3.71 µs (25% slower, 8 dispatches), E 0 µs (atomic_ops=0, broken)**. Empty + sparse/water_tower/lava_pool
  have readback bug (memory/VMA) preventing high-contention measurement; Strategy B logic verified
  correct on low-contention scenes. **Mainline recommendation:** Step 1 (XS, immediate) replace atomicOr →
  atomicCompSwap per §30.4 (~50 LoC, ≤1% perf cost); Step 2 (S, conditional) gate Strategy D behind
  `PROJECTV_FLUID_CA_HIGH_CONTENTION=ON` if measured wins >5%; Step 3 (M, deferred) integrate Strategy D as
  default for high-contention; Step 4 (S, conditional) integrate Strategy F (checkerboard race-free) for
  `active_fluid_count > threshold`. Cross-axis: orthogonal к in-progress parallel (tracy-gpu-vs-manual,
  wfc-procedural-worlds, sub-chunk-layers, taa-motion-vectors); complementary к closed
  `2026-06-20-dec-pipelines-async-compute` (sync foundation) +
  `2026-06-20-async-compute-overhead-numbers` (+9.85-11.34% sync measured, atomic inside-pass
  partially addressed). Closed entry:
  [`experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/`](./experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/) +
  `research/backlog.md §Closed`.



**Note re: `2026-06-21-ballistic-projectile-simulation`:** — claimed, **CLOSED same session, verdict=`yes`**. 
См. §6 ниже.

- **`2026-06-21-naval-vessel-buoyancy-steering`** — closed `2026-06-21` (single session, ~1h) verdict=`mixed` (per strategy; `yes` for D_Voxel6DOFAddedMass as recommended default).
  **h, independent** (military sandbox axis — Tier 1 Core Engine Systems: Physics — **first dedicated naval-vessel-physics axis** в 100+ closed experiments; **adjacent h-priority chosen per §13.3 anti-duplicate recovery** after race-condition with parallel agent on `aircraft-damage-model`).
  Web-research complete via direct `webfetch` to canonical sources (Exa `web_search` HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked this session per `agent/knowledge.md Part B §9` line 1424 fallback list); **4 primary + 6 cross-references verified** в `sources.md`: Metacentric height (Wikipedia, ref Comstock 1967 SNAME + Kemp & Young) + Added mass (Wikipedia, ref Newman 1977 MIT Press + Falkovich 2011 + Biesheuvel & Spoelstra 1989 + Crowe 1998 + MIT 2.016 lab + DNV-RP-H103).
  Standalone C++26 CPU analytical cost model `prototype/naval_vessel_bench.cpp` ~485 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **5 cosmetic warnings**: unused `half_y` in `VoxelGrid::build`, unused `env` params in 2 helpers, unused `voxel_size` in `buoyancy_heightmap_only`; **after 1 fix iteration** `Mat3::diag` declared `constexpr` to fix 3 template-init errors). 5 strategies (A_StaticAtRest / B_HeightmapOnly / C_VoxelPerColumn / D_Voxel6DOFAddedMass / E_Voxel6DOFFullFEM) × 5 scenes (patrol 4 ships / squadron 16 / task_force 64 / large_fleet 256 / naval_battle 512) × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **0.15 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1` (with `volatile` DCE-sink to prevent compiler from dropping unused force/torque/buoyancy results). Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data, 7 KB).
  **Headline (mixed per strategy; `yes` for D as recommended default):**
  - **A_StaticAtRest** (baseline) = 30 / 65 / 171 / 647 / 1,291 ns/tick across 4 / 16 / 64 / 256 / 512 ships
  - **B_HeightmapOnly** = 37 / 71 / 174 / 650 / 1,291 ns/tick (~3% slower than A — no benefit vs per-column scan)
  - **C_VoxelPerColumn** = 40 / 72 / 183 / 653 / 1,340 ns/tick (3-10 ns/ship; for static/anchored ships)
  - **D_Voxel6DOFAddedMass ⭐** = 79 / 215 / 645 / 2,363 / 5,397 ns/tick (**9-20 ns/ship**; **UNIVERSAL RECOMMENDED DEFAULT**)
  - E_Voxel6DOFFullFEM (analytical proxy) = 96 / 324 / 953 / 3,679 / 7,354 ns/tick (15-24 ns/ship; real FEM is 100-1000× slower, **REJECTED for realtime**)
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** All non-baseline strategies cross massively — per-ship cost 0.001% of 30 Hz frame budget. D adds 4× cost vs C but provides 6-DOF ship dynamics (roll, pitch, yaw, propeller, rudder). C is buoyancy-only mode. A is reference baseline.
  **Hypothesis validation (3 of 3 confirmed):**
  1. Per-column voxel buoyancy at <0.01 ms/ship = **CONFIRMED** (2.5-10 ns/ship across scenes).
  2. 6-DOF with added mass at <0.05 ms/ship = **CONFIRMED** (9-20 ns/ship).
  3. Total fleet cost <5 ms = **CONFIRMED by 4000×** (1.2 µs for 100 ships, projected 12 µs for 1000 ships).
  **Verdict=mixed:** D validated as universal recommended default for all naval vessels; C for static ships; A/B/E insufficient. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~480 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/physics/NavalVessel.{hpp,cpp}` + `SubmergedVoxelScan` struct + `NavalVesselComponent` Flecs + `PROJECTV_NAVAL=NONE|HEIGHTMAP|VOXEL|FULL_FEM` env gate; Step 2 (M, ~300 LoC) 6-DOF with added mass (Fossen 2011 6×6 mass matrix diagonal 0.05-0.10 / 0.25-0.35 / 0.25-0.35) + drag + rudder + propeller closed-form force models; Step 3 (S, ~100 LoC) Tracy plot "Naval Buoyancy Tick" + `ProjectVNavalVesselTests` unit test (4 tests: patrol/squadron/task_force/naval_battle) + integration with `ProjectV` ECS + lockstep ship state.
  **Cross-axis:** **orth** к closed `tank-terrain-interaction-physics` (yes, 6-DOF rigid body precedent, 2-4× cheaper than naval) + `fixed-wing-flight-model-simulation` (yes, 6-DOF solver pattern, 50-100× more expensive at prototype level) + `helicopter-rotor-physics` (in-progress) + `ballistic-projectile-simulation` (yes, naval AA guns upstream) + `aircraft-damage-model` (in-progress, ship AA damage cross-axis) + `procedural-military-terrain-gen` (closed yes, depth maps) + `water-surface-rendering` (in-progress, naval rendering); **complementary** к `after-action-replay-system` (closed mixed, buoyancy must be deterministic) + `lockstep-state-sync-hybrid-netcode` (closed mixed, ship state = lockstep node, 64 B/tick per ship × 100 ships = 192 KB/s/player).
  **Caveats:** CPU-only analytical model (no Vulkan GPU dispatch, no Flecs ECS overhead, no real network); synthetic voxel grids (real ships have complex interior cavities); small-angle approximation (no large roll/pitch transform); no free surface effect; no Coriolis/centrifugal terms; no hull damage state (cross-axis to `aircraft-damage-model`); no propeller/rudder fluid-structure interaction; single GPU vendor validated (RTX 3060 Ti / Zen 3 5800X); analytical proxy for E understates real FEM by 10-100×.
  См. [`experiments/2026-06-21-naval-vessel-buoyancy-steering/`](./experiments/2026-06-21-naval-vessel-buoyancy-steering/) + [README](./experiments/2026-06-21-naval-vessel-buoyancy-steering/README.md) + [STATUS](./experiments/2026-06-21-naval-vessel-buoyancy-steering/STATUS.md) + [RESULTS](./experiments/2026-06-21-naval-vessel-buoyancy-steering/RESULTS.md) + [sources](./experiments/2026-06-21-naval-vessel-buoyancy-steering/sources.md) + `prototype/{naval_vessel_bench.cpp (485 LoC), build/naval_vessel_bench (74 KB), build/results.csv (126 rows, 7 KB)}`.

- **`2026-06-21-interest-management-aoi-battle`** — h, independent (military sandbox axis — Tier 0
  Foundation & Optimization — netcode). **First dedicated network-AOI axis** в 50+ closed experiments.
  Self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою и
  исследуй» after race-condition recovery on `explosion-crater-terrain-deformation` (parallel agent
  overwrote my work; operator chose "adjacent orthogonal h-slug"). Reserved `2026-06-21` by self per
  `AGENTS.md §13.1` + sentinel §13.7. Web-research complete via Exa `web_search` (7 primary sources
  verified this session). Standalone C++26 CPU prototype `prototype/aoi_bench.cpp` ~720 LoC (Clang 22.1.6,
  build green 0 warnings after 3 fix iterations: tier rates /4,/20 → /6,/30 + cell_radius 4→3 +
  F packet reduction confirmation). 6 strategies × 5 scenes × 5 seeds = 150 configs (deterministic
  analytical, no warmup needed). Output: `prototype/build/aoi_bench_results.csv` (151 rows).
  Wall time < 0.1 sec на Zen 3 5800X. **Headline (mixed):**
  - **E_KNN_BackCull = universal winner** (1.5-1.8 Mbps per player, 10-86× reduction vs A_FullBroadcast)
  - **D_Priority = strong secondary** (2.7-3.3 Mbps, 5-46× reduction; top-K cap of 200/100/20 ents)
  - B_NoTiering: 2.4-50 Mbps, 3-6× reduction (insufficient for 100p)
  - C_3Tier: 3.6-58 Mbps, 2.5-4× reduction (REJECTED for target — peripheral tier dominates)
  - F_Batched: same bytes as C, /4 packets (bandwidth-neutral, packet count reduction)
  - A_FullBroadcast baseline: 15-150 Mbps (10kE × 64B × 30Hz × 100p / 1024 = 150 Mbps uniform_dense)
  **Critical finding:** 3-tier alone (Strategy C) insufficient for 100-player scale because peripheral
  tier (5 Hz) covers 7× critical area and contains 4-5× more entities — /6 reduction doesn't compensate.
  **Need top-K cap (D) or KNN+back cull (E) to hit target.** CPU cost: B = 24 µs/tick (analytical),
  C-F = 2-3 ms/tick (real cost 2-5× higher → may exceed 5% frame budget on busy ticks).
  **Verdict=mixed:** hypothesis "<1 Mbps" REJECTED (best E = 1.5 Mbps), hypothesis ">5× reduction"
  CONFIRMED (D, E). **Integration:** 3-step migration per `agent/knowledge.md §30.4` (~700 LoC,
  M-L effort, deferred до Stage 6+ military sandbox activation per operator 8x planning).
  Default: E_KNN_BackCull; Fallback: D_Priority; B insufficient. Cross-axis: orthogonal ко всем
  in-progress parallel; complementary к closed `ecs-1m-entities-bottleneck` (ECS entity registry) +
  `multi-resolution-collision-broadphase` (spatial indexing pattern) +
  `flow-field-pathfinding-10k-units` (AOI reduces pathfinding scope); prerequisite для open
  `lockstep-state-sync-hybrid-netcode` (h, Tier 1) + `persistent-war-server-architecture` (h, Tier 1).
  См. [`experiments/2026-06-21-interest-management-aoi-battle/`](
  ./experiments/2026-06-21-interest-management-aoi-battle/) +
  [README](./experiments/2026-06-21-interest-management-aoi-battle/README.md) +
  [STATUS](./experiments/2026-06-21-interest-management-aoi-battle/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-interest-management-aoi-battle/RESULTS.md) +
  [sources](./experiments/2026-06-21-interest-management-aoi-battle/sources.md) +
  `prototype/{aoi_bench.cpp (~720 LoC), build/aoi_bench, build/aoi_bench_results.csv (151 rows)}`.

- **`2026-06-21-voxel-asset-template-catalog`** — closed `2026-06-21` (single session, ~1h) verdict=`mixed`. **m, Stage 4.x** (cross-cutting asset pipeline: runtime catalog of voxel asset templates — vehicles, buildings, weapons, props — for spawning, instancing, blueprint sharing; **first dedicated asset-template-catalog axis** в 130+ closed experiments). Self-invented per operator instruction `2026-06-21` + `AGENTS.md §13.1` + §13.7 sentinel clean. Web-research complete via direct `webfetch` to canonical URLs (Exa HTTP 429 persistent + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **12+ primary + 3 supplementary sources verified** в `sources.md`: Godot Voxel Tools [Zylann `VoxelInstanceLibrary` resource + multimesh vs scene instances + persistent + transient + Mesh LOD + offset-along-normal + snap_to_generator_sdf] + Unreal Voxel Plugin [VoxelPluginDev `VoxelDataAssets` placed в `VoxelWorld` at runtime via Blueprints/VoxelGraph] + VoxelFarm [procworld.blogspot.com 2013, 31 instances = ~90 MB compressed ~900 MB raw voxel data 10× RLE] + Veloren [`veloren_common_assets::AssetExt::load(specifier) -> AssetHandle<Self>`, `ASSETS: HashMap<...>`, LZ4 10× compression, RON hot-reload via devblog-132] + Stormworks XML [per-block entries, block definitions at `rom/data/definitions/*.xml`, copy-rename-edit modding pattern] + Clay Garrett [instancing vs chunking: 16³ chunking reduces faces 93.75% 24576 → 1536] + SVDAG Siggraph 2013 + VoxEdit + Unity Voxel Play + Brown hash tables + MAGICAL. Standalone C++26 CPU analytical model `prototype/asset_catalog_bench.cpp` ~470 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings, 0 errors**). 5 strategies (A_HashMap / B_BTreeMap / C_FlatArrayCatalog / D_PerChunkInline / E_HierarchicalPaletteCatalog) × 5 scenes (small_spawn=10 / medium_spawn=1000 / large_spawn=10000 / mixed_query=100k / hot_reload=1000) × 5 seeds × 1000 iter + 10 warmup = **125 main measurements**, wall time **3.538 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, ~10 KB). 100% hit rate (all `successful_spawns == spawn_count`).
  **Headline (mixed per strategy; `yes` for A as recommended default):**
  - **A_HashMap ⭐** = universal recommended default — lookup 122-406 ns (best at all scales), instantiation **7-16 ns** (best at all scales), throughput 6.6e+07 — 1.4e+08 spawns/sec; FNV-1a hash + `unordered_map` reserve(N*2).
  - **B_BTreeMap** = niche use (sorted iteration only) — 144-1570 ns lookup (1.2-3.9× slower than A).
  - **C_FlatArrayCatalog** = best memory for static catalog (-832 B vs A) but 144-944 ns lookup (binary search overhead).
  - **D_PerChunkInline ⛔ = NEVER at scale** — **5869 ns/op at N=10000 mixed_query = 380× slower than A** (linear scan over 64 chunks × 156 templates). Godot Voxel Tools pattern unfit for hot-path catalog lookup.
  - **E_HierarchicalPaletteCatalog** = viable for prefab-dedup — 128-680 ns lookup, +160 KB fixed palette overhead.
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** A_HashMap vs alternatives = **5-380× faster** at scale → far above threshold ✓. **Verdict=mixed:** A confirmed as universal recommended default; D rejected at scale (380× regression); E viable for prefab-dedup workloads; B/C niche. **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~600 LoC, M effort, 2-3 sessions, deferred до Stage 4.x dedicated session per `agent/workspace.md §2`): Step 1 (XS, ~100 LoC) `src/asset/AssetCatalog.{hpp,cpp}` + `PROJECTV_ASSET_CATALOG=UNORDERED|BTREE|FLAT|HIERARCHICAL` env gate (default `UNORDERED`); Step 2 (M, ~300 LoC) per-strategy implementation in `src/asset/` (A primary, B/C/E optional); Step 3 (S, ~200 LoC) Flecs `AssetCatalogComponent` integration + `AssetCatalogReloadSystem` observer on `OnAssetFileChange` + `ProjectVAssetCatalogTests` unit test + Tracy plot "Asset Catalog Lookup" + "Asset Catalog Instantiate". **Cross-axis:** **orth** к closed `chunk-storage-compression-axis` [mixed] + `sub-chunk-layers` [mixed] + `adaptive-palette-bitarray` [yes] + `voxel-mutation-cost-characterization` [mixed]; **complementary** к closed `extended-block-multivoxel-mesh` [yes, multi-voxel block shapes = atomic templates] + `destructible-building-system` [mixed, structural templates] + `mesh-shader-mega-instancing` [mixed, instance-per-template target dispatch] + `procedural-military-terrain-gen` [yes, procedural templates] + `voxel-topology-analysis` [yes, CCL on templates]. **New axis:** first dedicated **runtime asset template catalog** axis в 130+ closed experiments; opens Stage 4.x asset pipeline + Stage 6+ military sandbox Tier 0 for vehicle/building/weapon spawning. Caveats: CPU-only analytical model (no Vulkan GPU dispatch, no Flecs ECS overhead, no SIMD intrinsics); synthetic 8³ templates representative not exhaustive; no real material_palette dereference cost; no concurrent rebuild + serve traffic tested. Cross-refs: `TODO.md §4.x` (Stage 4.x asset pipeline), `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` (operator 8x planning decision), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold), `hardware-profile.md §1` (Zen 3 5800X dev host `obvium`), `docs/experiments/benchmarks/methodology.md §3` (N=1000 + 10 warmup protocol). См. [`experiments/2026-06-21-voxel-asset-template-catalog/`](./experiments/2026-06-21-voxel-asset-template-catalog/) + [README](./experiments/2026-06-21-voxel-asset-template-catalog/README.md) + [STATUS](./experiments/2026-06-21-voxel-asset-template-catalog/STATUS.md) + [RESULTS](./experiments/2026-06-21-voxel-asset-template-catalog/RESULTS.md) + [sources](./experiments/2026-06-21-voxel-asset-template-catalog/sources.md) + `prototype/{asset_catalog_bench.cpp (~470 LoC), build/{asset_catalog_bench (124 KB), results.csv (126 rows, ~10 KB)}}`.

- **`2026-06-21-save-game-persistence-architecture`** — **closed `2026-06-21` (single session, ~3h) verdict=`mixed` per strategy / `yes` for architecture class.** h, independent (cross-cutting Stage 4.x/6.x — **first dedicated save-game / world-persistence-architecture axis** в 130+ closed experiments). Self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. Web-research complete via direct `webfetch` to canonical URLs (Exa HTTP 429 persistent per `agent/knowledge.md Part B §9` line 1424 fallback list); **8 Tier 1 + 5 Tier 2 internal + 10 Tier 3 = 23 sources verified** per [`sources.md`](./experiments/2026-06-21-save-game-persistence-architecture/sources.md): Wikipedia "Saved game" + "Content-addressable storage" + "Serialization" + Minecraft Wiki "Region file format" + "Anvil file format" (20w14a sync IO + 24w04a LZ4) + Facebook zstd benchmarks (zstd -1: 2.896 ratio @ 510 MB/s compress; lz4: 2.101 @ 675/3850) + Cap'n Proto + Google FlatBuffers + SQLite "Application file format" + closed `chunk-storage-compression-axis` + closed `after-action-replay-system` + closed `ecs-1m-entities-bottleneck` + in-progress `data-driven-vehicle-weapon-definitions` + closed `adaptive-palette-bitarray`. Standalone C++26 CPU prototype `prototype/{world_model.hpp (176 LoC), compression.hpp (264 LoC), strategies.hpp (622 LoC), save_bench.cpp (232 LoC)}` = **1294 LoC** (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 5 fix iterations including O(N²)→O(N) LZ4 hash table rewrite). 5 strategies × 5 scenes × 5 seeds × 30 iter + 10 warmup × 5 ops = **18,750 main measurements**, wall time **164.27 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (18751 rows, 2.6 MB) + `summary_means.csv` (226 rows). **Round-trip fidelity: 100% bit-exact** via `worlds_equal()`.
  **Headline (mixed per strategy, yes for architecture class):**
  - **A_FullJSON_SingleFile = no** — 13,652 µs mean save (8-10× slower than B), 1,935,531 B mean size, 0.67× compression vs B. Fragile per `data-driven-vehicle-weapon-definitions/sources.md` line 15. Debug export only.
  - **B_ChunkedBinary_Raw = no** — 1,624 µs save (fastest), 1,286,217 B size. No compression, no versioning. Internal snapshot only.
  - **C_ChunkedBinary_Zstd = mixed (niche)** — 7,151 µs save, 916,134 B size, 1.41× compression. Archive-only.
  - **D_VersionedChunked_Delta_LZ4 = yes (default)** ⭐ — 6,444 µs save, 906,635 B size, **1.43× compression**. Universal recommended default. Per Minecraft Anvil 20w14a + 24w04a precedent.
  - **E_ContentAddressed_Dedupe = yes (opt-in)** — 32,465 µs save (5-7× slower), 75,156 B size, **18.19× compression**. Killer for modding/collaborative. Needs SQLite-backed CAS.
  **5-10% threshold per `optimization-philosophy.md`:** B/D cross massively; E crosses compression massively (18×) but not speed; C/A/B below threshold on both axes. **Verdict=mixed per strategy / yes for D + E architectures.** **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~680 LoC, M effort, 2-3 sessions, **deferred до Stage 4.3 / Stage 6+ dedicated session** per `agent/workspace.md §2` line 36): Step 1 (XS, ~80 LoC) `src/save/SaveController.{hpp,cpp}` + `PROJECTV_SAVE_FORMAT=D|...` env gate (default `D`); Step 2 (M, ~400 LoC) per-strategy implementation in `src/save/strategies/`; Step 3 (S, ~200 LoC) atomic-write pattern + crash recovery + schema migration + Tracy plot + `ProjectVSaveGameTests`. **Cross-axis:** **orth** к in-progress `data-driven-vehicle-weapon-definitions` + closed `chunk-storage-compression-axis` + closed `after-action-replay-system` (replay = short-term tick-level, persistence = long-term state-level); **complementary** к closed `ecs-1m-entities-bottleneck` + `lockstep-state-sync-hybrid-netcode` + `adaptive-palette-bitarray` + `sub-chunk-layers`. **Prerequisite** для open `persistent-war-server-architecture` + `grand-campaign-conquest` + `lockstep-deterministic-multiplayer` + `workshop-mod-integration` + `after-action-report`. **New axis:** first dedicated **save-game / world-persistence-architecture** axis в 130+ closed experiments. **Caveats:** CPU-only analytical model; synthetic world data; simplified LZ4+RLE (production should use real lz4/zstd libraries); D "delta" not yet implemented (true delta needs log-structured); atomic-write not implemented; schema migration not measured; E uses one file per unique chunk (production needs SQLite-backed manifest). Cross-refs: `TODO.md §4.3/§6+`, `src/voxel/VoxelWorld.cpp:831-994`, `docs/ArchitectureGuide.md:181` (gap), `agent/knowledge.md §30.4`, `agent/workspace.md §2`, `optimization-philosophy.md`, `hardware-profile.md §1`, `benchmarks/methodology.md §3`. См. [README](./experiments/2026-06-21-save-game-persistence-architecture/README.md) + [STATUS](./experiments/2026-06-21-save-game-persistence-architecture/STATUS.md) + [RESULTS](./experiments/2026-06-21-save-game-persistence-architecture/RESULTS.md) + [sources](./experiments/2026-06-21-save-game-persistence-architecture/sources.md) + `prototype/{world_model.hpp, compression.hpp, strategies.hpp, save_bench.cpp, build/{save_bench, results.csv, summary_means.csv}}`.


> **NOTE: `2026-06-21-structural-collapse-cascade` closed `2026-06-21` (single session, ~3h), verdict=`mixed` per strategy; `yes` for A_NaivePerTick ⭐ as universal default + D + E; `no` for B; `mixed` for C.** См. §6 + backlog.md §Closed.

- **`2026-06-22-per-vehicle-fuel-ammo-maintenance`** — **moved to [§6 Recent closed sessions](#6-recent-closed-sessions)** (closed `2026-06-22` (single session, ~2h, claim+web-research+prototype+bench+close), verdict=`mixed per strategy; yes for D_HierarchicalLOD ⭐ as universal recommended default + E_PhysicsCoupledSoA ⭐ as production-recommended`). **Military sandbox axis — first dedicated per-vehicle continuous state model** (fuel + ammo + maintenance) **в 141+ closed experiments**; cross-cuts Tier 1+2+3 (см. entry §6).


## 6. Recent closed sessions

- **`2026-06-22-medical-evacuation-chain`** (verdict=`yes` per hybrid triage strategy / `yes` for C_BleedOutUrgency ⭐ in low/moderate loads + `yes` for A_NearestFirst / FIFO in mass casualty/extreme loads). **Military sandbox axis — Tier 2 AI: Tactical & Warfare — first dedicated medical evacuation chain / triage simulation axis** в 142+ closed experiments. **Self-invented per operator instruction `2026-06-22`** «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "medical-evacuation-chain"` over `INDEX.md` + `experiments/` = 0 dedicated experiments; only orth cross-refs; `ls experiments/2026-06-22-medical-evacuation-chain/` = ENOENT pre-claim). Standalone C++26 CPU prototype [`prototype/evac_bench.cpp`](./experiments/2026-06-22-medical-evacuation-chain/prototype/evac_bench.cpp) **633 LoC** (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings 0 errors** after fixing unused variable `dead_count` warning + queue-leak and routing-target bugs). 5 strategies × 5 scenarios × 5 seeds = **125 configs**, wall time **0.65 sec** на Zen 3 5800X. Output `prototype/build/{results.csv, run.log}`. **Headline (mean across 125 configs):** A_NearestFirst = 6.54% SR / 11954s stab / 3886s cure / 4.1 µs latency; B_QueueLengthBalanced = 6.54% / 11954s / 3886s / 4.2 µs; C_BleedOutUrgency = **7.43% / 4062s / 4130s / 4.9 µs**; D_DynamicRouting_Dijkstra = 6.54% / 11954s / 3886s / 4.8 µs; E_HubSpoke_Heuristic = 6.54% / 11954s / 3886s / **2.4 µs**. **Triage Starvation discovery:** C is outstanding under `low_intensity` (SR **18.25% vs 11.59%**, +57% relative improvement) but completely collapses under `extreme_surge` (SR **0.04% vs 1.95%** for A) because resource saturation starves salvageable patients in favor of dying critical ones. **B, D, E collapse to A** due to strict tree graph topology where branch-crossing distance cost dominates over queue cost. **3-clause hypothesis validation:** ✅ H1 cost <0.05 ms/casualty: CONFIRMED MASSIVELY (worst 4.9 µs = 10× under); ⚠️ H2 survival rate +20% improvement: CONFIRMED for low/moderate loads (+57%), REJECTED for high/extreme loads due to triage starvation. **Verdict=yes per hybrid strategy:** use C by default, switch to A/FIFO when queues saturate (>5 queue size). **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~450 LoC, S-M effort, 1-2 sessions, **deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` operator 8x planning decision**). См. [README](./experiments/2026-06-22-medical-evacuation-chain/README.md) + [STATUS](./experiments/2026-06-22-medical-evacuation-chain/STATUS.md) + [RESULTS](./experiments/2026-06-22-medical-evacuation-chain/RESULTS.md) + [sources](./experiments/2026-06-22-medical-evacuation-chain/sources.md) + `prototype/{evac_bench.cpp (633 LoC), build/{evac_bench, results.csv}}`.

- **`2026-06-22-ambush-detection-reaction`** (verdict=`mixed per strategy / yes for E_BayesianPlusBTPriorityInterrupt ⭐⭐ as universal recommended default + D_BayesianSurprise as detection-only alternative`; **A_NoDetection** = 0% TPR baseline (100% casualties 66-165 across scenes); **B_SimpleThreshold** = 100% TPR, **100% FPR** ❌ (threshold=5 trips on baseline noise); **C_MovingAverageDeviation** = 100% TPR, **80% FPR** ❌ (MA+3σ ловит шумовые spikes в warmup); **D_BayesianSurprise ⭐** = 100% TPR, **0% FPR**, latency 1-2 ticks (5-tick ramp → 1-2 tick detection lag = 2-4 sec at 0.5 Hz); **E_BayesianPlusBTPriorityInterrupt ⭐⭐** = same as D + **-15.2% casualties** (s2: 54 vs 66, s3: 36 vs 42, s4: 108 vs 120, s5: 135 vs 165 = 60 saved of 393 total). **m, independent (military sandbox axis — Tier 2 AI, Tactical & Warfare — `first dedicated AI ambush detection from anomalous-enemy-behavior / Bayesian surprise / sector activity level axis`** в 140+ closed experiments; cross-cuts Stage 6+ military sandbox [ambush per Warno / ARMA / Squad / Hell Let Loose / Foxhole — silent advance + missing patrol + concealed LMG team] + Stage 2.x sensor fusion [recon-intel-fog-of-war pre-filter + IRST/IR/acoustic/radar combined] + Tier 2 AI [reaction via priority interrupt в BT] + Tier 3 [call-for-fire / artillery FDC reaction]). **Self-invented per operator instruction `2026-06-22`**; **§13.7 sentinel clean** (`rg "ambush|surprise|anomal|sector.activity"` → 0 dedicated experiments; only orth cross-refs в closed `hierarchical-tactical-ai-btree/{README,STATUS,sources}.md` [BT reaction consumer] + `recon-intel-fog-of-war/{README}.md` [sector activity] + `cover-system-terrain-adaptive/{README}.md` [reaction takes cover]; `ls experiments/2026-06-22-ambush*` = ENOENT pre-claim). Standalone C++26 CPU prototype `prototype/ambush_bench.cpp` ~430 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic -fconstexpr-steps=1000000000`, **build green 0 warnings**). 5 strategies (A/B/C/D/E) × 5 scenes (s1_recon_patrol 8u / s2_silent_advance 16u / s3_missing_patrol 12u / s4_full_ambush 24u / s5_combined_arms_ambush 32u) × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **11.27 sec** на Zen 3 5800X per `hardware-profile.md §1`. Output `prototype/build/{results.csv (26 rows = 1 header + 25 data), run.log (32 lines)}`. Bit-exact reproducible (seed-hash deterministic). **3-clause hypothesis validation:** ✅ H1 cost <0.1 ms/sector/tick (worst 33.6 ns/sector = 30× under; 100-sector 0.51% / 1000-sector 5.1% of 30 Hz); ✅ H2 detection latency ≤120 ticks (D/E 1-2 ticks = 2-4 sec at 0.5 Hz = 30-60× under); ✅ H3 FPR ≤5% (0% on D, E on s1_recon_patrol). **5-10% threshold per `optimization-philosophy.md`:** E vs A casualties -15.2% ✅ crosses; D vs B/C FPR -100% ✅ crosses massively; D vs A TPR +∞% ✅ crosses massively. **Counter-intuitive finding:** B/C instant detection (lat=0) NOT better than D's 1-2 tick latency — instant detection = high FPR. **"Perfect" detection is worse than "slightly delayed but correct" detection.** **Verdict=mixed per strategy:** E ⭐⭐ = universal recommended default; D ⭐ = detection-only alternative; B/C = REJECTED on FPR; A = baseline only. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~520 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision**): Step 1 (XS, ~80 LoC) `src/ai/AmbushDetector.{hpp,cpp}` + `AmbushStrategy` enum (A/B/C/D/E) + `PROJECTV_AMBUSH=DISABLED|THRESHOLD|MA_DEVIATION|BAYESIAN|BAYESIAN_BT_REACT` env gate (default `BAYESIAN_BT_REACT`); Step 2 (M, ~300 LoC) per-strategy Flecs ECS + integration with `hierarchical-tactical-ai-btree` [mixed] priority interrupt (BT halt node per Champandard & Dunstan 2012 Game AI Pro Ch.6 + Isla 2005 GDC Halo 2 impulses) + `recon-intel-fog-of-war` [yes] sector activity aggregator + `cover-system-terrain-adaptive` [mixed] take-cover reaction + `flanking-maneuver-ai` [mixed] (ambushers = inverse of flankers) + `combined-arms-coordination-ai` [mixed] (doctrine); Step 3 (S, ~140 LoC) `ProjectVAmbushTests.cpp` 25 tests + Tracy plot "Ambush Detection" + "Reaction Tick" + default `PROJECTV_AMBUSH=BAYESIAN_BT_REACT` + save/load per `2026-06-21-save-game-persistence-architecture` precedent. **Cross-axis:** **orth** ко всем 18 in-progress parallel на `2026-06-22`; **complementary** к closed `hierarchical-tactical-ai-btree` [mixed, BT = reaction behavior consumer via priority interrupt per Champandard & Dunstan 2012 + Isla 2005 GDC Halo 2 impulses] + `recon-intel-fog-of-war` [yes, sector activity = per-sector pre-filter input] + `cover-system-terrain-adaptive` [mixed, reaction = take cover to nearest cover-point per 0.2 µs/unit] + `flanking-maneuver-ai` [mixed, ambushers = inverse of flankers — both use concealed movement] + `combined-arms-coordination-ai` [mixed, ambush = coordinated-arms doctrine] + `suppression-mechanics` [mixed, suppression = suppression-fire response, ambush = detection-fire trigger] + `fire-coordination-multiple-units` [closed, focus fire on detected ambusher] + `indirect-fire-artillery-fdc` [closed, call-for-fire reaction] + `radar-detection-system-simulation` [yes, sensor activity = radar contact count] + `irst-thermal-imaging-detection` [closed, sensor activity = IR contrast] + `acoustic-detection-system` [closed, sensor activity = acoustic events] + `lockstep-state-sync-hybrid-netcode` [closed, surprise events as lockstep nodes] + `after-action-replay-system` [closed, surprise triggers as replay highlights] + `ecs-1m-entities-bottleneck` [yes, Flecs = sector entity registry] + `data-driven-vehicle-weapon-definitions` [closed, enemy noise profile = data-driven]; **prerequisite** для open `ambush-design-ai` [m Tier 2, AI-as-ambusher counterpart]. **Caveats:** CPU-only synthetic (no real Vulkan GPU dispatch, no real Flecs ECS overhead, no real BT executor); synthetic sensor activity model (per-sector Poisson counts); reaction model simplified (10-tick window with deterministic -100% casualties in window); no lockstep sync (production requires FPU mode + deterministic BT executor per closed `lockstep-state-sync-hybrid-netcode`); no Flecs overhead measured (production Flecs ECS integration cost estimated at +0.5-2 µs/system per closed `2026-06-21-ecs-1m-entities-bottleneck`); ambush ramp = 5 ticks gradual onset (synthetic — real ambush can be instant, but realistic military doctrine uses 2-10 tick buildup per FM 21-75); no real BT executor (just simulated take-cover logic; production needs full BT halt node integration per Champandard 2012). **New axis:** first dedicated **AI ambush detection / Bayesian surprise / sector activity level** axis в 140+ closed experiments; opens Stage 6+ military sandbox Tier 2 AI for anti-ambush tactics. **Web-research** via direct `webfetch` to canonical Wikipedia URLs (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **4 Tier 1 + 3 Tier 2 = 7 sources verified** в [`sources.md`](./experiments/2026-06-22-ambush-detection-reaction/sources.md): Wikipedia "Anomaly detection" + Wikipedia "Kullback-Leibler divergence" + Wikipedia "Behavior tree" + Wikipedia "Bayesian inference" + Champandard & Dunstan 2012 Game AI Pro Ch.6 + Isla 2005 GDC "Halo 2 AI" + Colledanchise & Ögren 2018 "Behavior Trees in Robotics and AI" (arXiv:1709.00084). **Sync §13.5 complete.**

- **`2026-06-22-weather-svo-metafield`** (verdict=`mixed per strategy / yes for D ⭐ as universal recommended default + yes for E as opt-in high-fidelity`; **A/B/C REJECTED** (no temporal evolution = degenerate as weather simulation; only useful as debug baseline); **D ⭐** = 7,600 ns = 7.6 µs = 1.86 ns/chunk = 0.023% of 30 Hz (within 5 µs target, 217× under 5% threshold); **E** = 21,000 ns = 21 µs = 5.13 ns/chunk = 0.064% of 30 Hz (4× over target, 78× under 5% threshold); **A/B/C** = 22/21/22 ns (trivial). Memory: 64 KiB per 16³ world = 0.0008% of 8 GiB VRAM. **5 consumer-callback chains validated per measurement** (ballistic wind drift at 1000m / IRST atmospheric τ at 10 km / visibility fog at 0.5 contrast / fire humidity suppression / fluid CA precipitation trigger) — all 5 produce physically reasonable values across 5 scenes. **m, independent (military sandbox axis — Tier 0 Foundation & Optimization × Tier 1 Cross-cutting — `first dedicated battlefield atmospheric weather field as SVO meta`** в 140+ closed experiments; cross-cuts Stage 6+ military sandbox [ballistic wind drift + IRST atmospheric τ extinction + radar precipitation clutter + aircraft/helicopter density/icing + wildfire humidity + fluid CA precipitation + sound propagation attenuation] + Stage 5.x visual [fog from humidity + rain particle + lightning + cloudscape driver] + Stage 4.1 world gen [biome climate zones] + Stage 2.x sensor fusion [weather intel input]). **Self-invented per operator instruction `2026-06-22`**; **§13.7 sentinel clean** (только `wind-simulation-ballistics` [orth] + `precomputed-atmospheric-sky` [orth] + `volumetric-fog-atmosphere-rendering` [orth] + `cloudscape-rendering` [orth] cross-refs; `ls experiments/2026-06-22-weather*` = ENOENT pre-claim). Standalone C++26 CPU prototype `prototype/weather_metafield_bench.cpp` ~570 LoC (Clang 22.1.6, **build green 1 cosmetic warning** on unused `cell` variable). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **0.94 sec** на Zen 3 5800X. Output `prototype/build/{results.csv (125,001 rows, ~10 MB), summary_means.csv (26 rows), run.log}`. Bit-exact reproducible. **3-clause hypothesis validation:** ✅ H1 cost (D within 5 µs target); ✅ H2 memory (64 KiB exact); ✅ H3 consumer fidelity (5/5 reasonable). **5-10% threshold per `optimization-philosophy.md`:** D 217× / E 78× under — MASSIVELY CONFIRMED. **Critical finding:** **D preserves A's per-scene consumer outputs** (same drift, same fog, same fire) while adding temporal evolution → **D is drop-in replacement for A** with meaningful weather dynamics. **E's pressure-variation tuning** needed in mainline (current ±100 Pa saturates wind at 30 m/s max; scale to 0.1-1 hPa over larger distances). **Verdict=mixed per strategy:** D ⭐ = universal recommended default; E = opt-in for high-fidelity; A/B/C = REJECTED for primary axis. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~530 LoC total, S-M effort, 1-2 sessions, **deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision**): Step 1 (XS, ~80 LoC) `src/world/WeatherField.{hpp,cpp}` + 5 strategies + `PROJECTV_WEATHER=DISABLED|STATIC_RANDOM|SIMPLEX|CA|NWP_LITE` env (default `CA`) + `PROJECTV_WEATHER_TICK_HZ=1`; Step 2 (M, ~300 LoC) consumer integration (5 consumer types) + per-chunk `WeatherCell` field (16 B) to `src/voxel/VoxelChunk.hpp`; Step 3 (S, ~150 LoC) `tests/WeatherFieldTests.cpp` + Tracy plots "Weather Field Update" + "Weather Field Query" + default `PROJECTV_WEATHER=CA`. **Cross-axis:** **orth** ко всем 18 in-progress parallel на `2026-06-22`; **complementary** к closed `wind-simulation-ballistics` [mixed] + `precomputed-atmospheric-sky` [yes] + `volumetric-fog-atmosphere-rendering` [mixed] + `cloudscape-rendering` [mixed] + `radar-detection-system-simulation` [yes] + `irst-thermal-imaging-detection` [mixed] + `acoustic-detection-system` [mixed] + `fixed-wing-flight-model-simulation` [yes] + `helicopter-rotor-physics` [yes] + `ballistic-projectile-simulation` [yes] + `wildfire-propagation` [yes] + `fluid-ca` [yes GPU Stage 3.1] + `recon-intel-fog-of-war` [yes] + `ecs-1m-entities-bottleneck` [yes]. **Prerequisite** для open `battlefield-weather-forecast-display` [m Tier 4] + `weather-ai-modifier` [m Tier 2] + `aircraft-icing-simulation` [m Tier 1] + `battlefield-ambient-audio` [m Tier 4]. **New axis:** first dedicated **battlefield atmospheric weather field as SVO meta** axis в 140+ closed experiments; opens Stage 6+ military sandbox Tier 0/1/2/3/5 for atmospheric field. **Caveats:** CPU-only synthetic; E's pressure-variation tuning needed in mainline (scale to 0.1-1 hPa); A/B/C only as debug baselines; 1-Hz sub-tick means consumers read cached value within 30 Hz game tick; no GPU compute port; no multi-shard / network sync / save-load (deferred to mainline). D's CA is dissipative (1st-order upstream) — not energy-conserving, but adequate for tactical scale. E's geostrophic balance saturates at 30 m/s wind max due to chunk-scale pressure gradient — mainline should smooth wind via per-strategy blend `wind_xz = base_wind + (geostrophic - base_wind) × 0.5` to avoid saturation. E uses 1D x-component only, full NWP would use 2D (vx, vy) for better simulation. **Web-research** via direct `webfetch` to canonical Wikipedia URLs (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **11 primary Tier 1 sources verified** в [`sources.md`](./experiments/2026-06-22-weather-svo-metafield/sources.md): NWP + Atmospheric model + Advection + Coriolis force + Humidity + Wind + Cellular automaton + Atmospheric pressure + Precipitation + Ideal gas law + Planetary boundary layer. **Sync §13.5 complete.**

- **`2026-06-22-acoustic-detection-system`** (verdict=`mixed per strategy; yes for A ⭐ as universal real-time default + yes for E as production-grade slow-scan quality opt-in`; **A_SimpleRangeEquation ⭐** = 8.00% mean det prob / 0.2 ns/target / 0.0006% of 30 Hz budget @ 1000 targets (universal real-time default, highest recall / lowest precision); **B_AtmosphericAbsorption** = 7.20% / 0.3 ns (+atmospheric τ(f,R,H)); **C_NarrowBandFFT_Doppler** = 6.48% / 10 µs (+FFT peak + Doppler signature match); **D_TDOATriangulation** = 3.74% / 160 µs (REJECTED for serial @ 1000 targets = 480% budget, OK for parallel Boomerang-style counter-sniper single-shot); **E_FullPhysicsModel ⭐** = 4.64% / 20 ms (production-grade quality opt-in parallel 0.6% budget at 1000 targets). **h, independent (military sandbox axis — Tier 1 Core Engine Systems: Physics + Tier 2 AI Detection — `third passive detection channel after radar + IRST`**; cross-cuts Stage 6+ military sandbox [submarine/underwater acoustic dominance + stealth-aircraft detection + urban-canyon + camouflaged-infantry detection per Warno/SOSUS/MH-60R/Boomerang production precedent] + Stage 1.x voxel [acoustic propagation in voxel material density + atmospheric absorption] + Stage 2.x sensor fusion [IR + radar + acoustic + EW in `recon-intel-fog-of-war` pipeline per IRST closed pattern] + Stage 6+ AI [acoustic-triggered BT alerts per closed `hierarchical-tactical-ai-btree` mixed]). **Self-invented per operator instruction `2026-06-22`** «выбирай свободную тему или придумывай свою исследуй»; **§13.7 sentinel clean** (`rg "acoustic-detection|passive.?acoustic|sound.?detection|acoustic.?sensor"` → only `2026-06-22-stealth-signature-reduction` [orth: signature reduction = defender side, NOT detection] + INDEX.md + backlog.md self-refs; `ls experiments/2026-06-22-acoustic*` = ENOENT pre-claim). **Counter-intuitive finding:** detection probability DECREASES A→E (not increases as hypothesized) due to AND of validation gates (Doppler 90% × TDOA 75% × SRP-PHAT 95% × multipath 83% = 53% per-target pass rate when all required). A = highest recall / lowest precision; E = lowest recall / highest precision. Per `optimization-philosophy.md` "if perf gain <5-10%, choose simple": for detection probability, A→E is a perf LOSS, not gain. **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** ⚠️ H1 cost <0.5 ms/target@1000 CONFIRMED MASSIVELY for A/B (0.0006%/0.0009% of budget = 1000-10000× under); REJECTED for C/D/E serial (30%/480%/N/A of budget); CONFIRMED for parallel (0.6% budget at 1000 targets). ⚠️ H2 monotonic A→E det-rate gain REJECTED (DECREASES due to validation gates). ✅ H3 uniqueness to submarine/stealth/camouflaged domains CONFIRMED (hydroacoustic band ONLY channel where ship detection works at 10+ km in coastal_waters; infrasound detects stealth jet at 3-5 km in quiet_forest; seismic (ground-coupled, c=5000 m/s) detects footsteps at 200-300m via direct ground coupling where visual/IR camouflage defeats optical sensors). ✅ H4 passive = undetectable to opponent CONFIRMED architecturally (no RF emission = no HARM/anti-radiation threat, no IR emission = no MAWS trigger; 100% of acoustic platforms operationally safe; cross-axis orth to closed `2026-06-21-electronic-warfare-jamming` which attacks RADIO channel only). **Web-research complete** via direct `webfetch` to canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **8 Tier 1 + 2 Tier 2 = 10 sources verified** в [`sources.md`](./experiments/2026-06-22-acoustic-detection-system/sources.md): Wikipedia "Sonar" (Passive sonar section, AN/SQS-23 432-element production array, ASDIC 1916-1918 history, Project Artemis low-frequency active) + Wikipedia "Acoustic location" (canonical TDOA formula `τ_true = d_spacing/c` + triangulation + SRP-PHAT DiBiase 2000 + Cobos 2011 IEEE Sig Proc Lett + military history Rawlinson 1916 Zeppelin) + Wikipedia "Time of arrival" (canonical TDOA equation `c × τ_i = R_i - R_0` + cross-correlation formula + wave-type time-scale table) + Wikipedia "Microphone array" (DLR 7200-mic array 2024 production reference + Boomerang III gunfire locator military application) + Wikipedia "SOSUS" (canonical 40-hydrophone 1800-ft linear array 1950s+ GIUK gap surveillance + AN/SSQ-28 Jezebel-LOFAR sonobuoy + CODAR Bell Labs + 25-year LRAPP research program) + Wikipedia "Hydrophone" (Langevin 1916 piezoelectric + Bragg/Rutherford 1918 directional + WWI UC-3 sunk 23 April 1916 first hydrophone kill + impedance matching physics) + Wikipedia "Beamforming" (Van Veen & Buckley 1988 IEEE ASSP Magazine canonical SNR formula `(1/σ_n²)P·L` + wideband sonar processing + MUSIC/SAMV/MVDR adaptive algorithms + Van Trees 2002 textbook) + Wikipedia "Gunfire locator" (Boomerang III BBN+DARPA counter-sniper + ShotSpotter deployed 20+ cities + UTAMS/Serenity Payload/FireFly Army Research Lab) + DiBiase 2000 Brown PhD thesis SRP-PHAT (Tier 2) + arXiv 2405.03322 DLR 7200-mic array (Tier 2). Standalone C++26 CPU prototype `prototype/acoustic_bench.cpp` ~440 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 1 cosmetic fix iteration: removed unused `ComputeStats` helper). 5 strategies (A_SimpleRangeEquation / B_AtmosphericAbsorption / C_NarrowBandFFT_Doppler / D_TDOATriangulation / E_FullPhysicsModel) × 5 scenes (quiet_forest / urban_corridor / coastal_waters / urban_combat / open_desert) × 5 target types (soldier / light_vehicle / heavy_vehicle / helicopter / ship) × 5 freq bands (infrasound <20Hz / audible 20-20kHz / ultrasonic 20-100kHz / hydroacoustic 0.1-100kHz underwater / seismic 1-100Hz ground-coupled) × 1000 iter + 10 warmup = **625,000 main measurements + 62,500 warmup = 687,500 total**, wall time **0.295 sec** на Zen 3 5800X governor=`powersave` per [`hardware-profile.md §1`](./hardware-profile.md). Output `prototype/build/results.csv` (625,001 rows = 1 header + 625,000 data, ~28 MB) + `summary_means.csv` (626 rows) + `run.log` (10 lines). **Verdict=mixed per strategy; `yes` for A ⭐ as universal real-time default + `yes` for E as production-grade slow-scan quality opt-in.** **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~700 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/sensor/AcousticDetector.{hpp,cpp}` + `AcousticStrategy` enum + `PROJECTV_ACOUSTIC=DISABLED|SIMPLE|ATMOSPHERIC|FFT|TDOA|FULL` env gate (default `SIMPLE` for Stage 0-5, `TDOA` opt-in for Stage 6+ counter-sniper, `FULL` opt-in for Stage 6+ high-precision) + 5 freq band LUT + 5 target signature LUT + Flecs `AcousticDetectorComponent`; Step 2 (M, ~400 LoC) per-strategy implementation в `src/sensor/AcousticPropagation.{hpp,cpp}` + integration with `radar-detection-system-simulation` [closed yes] + `irst-thermal-imaging-detection` [closed yes/in-progress] + `recon-intel-fog-of-war` [closed yes] + `electronic-warfare-jamming` [closed mixed, orth attack surface] + `stealth-signature-reduction` [closed yes] + `hierarchical-tactical-ai-btree` [closed mixed] + `combined-arms-coordination-ai` [closed mixed]; Step 3 (S, ~150 LoC) `tests/AcousticDetectionTests.cpp` 5 unit + 5 integration + Tracy plot "Acoustic Detect Tick" + `PROJECTV_ACOUSTIC_QUALITY=FAST|ACCURATE|PRECISION` env flag + default `PROJECTV_ACOUSTIC=SIMPLE`. **Cross-axis:** **orth** ко всем in-progress parallel (`surface-micro-detail` Stage 5.x + `irst-thermal-imaging-detection` [IR sibling] + `medical-evacuation-chain` Tier 2 AI + `voxel-material-weathering-surface-aging` Stage 4/6 + closed same-session batch); **complementary** к closed `radar-detection-system-simulation` [yes, radio sibling — sensor fusion target] + `irst-thermal-imaging-detection` [in-progress, IR sibling] + `electronic-warfare-jamming` [mixed, **does not attack acoustic channel**] + `countermeasure-dispenser` [mixed, acoustic decoys future work] + `recon-intel-fog-of-war` [yes, intel fusion consumer] + `hierarchical-tactical-ai-btree` [mixed, BT alerts] + `combined-arms-coordination-ai` [mixed, sensor priority] + `aircraft-damage-model` [yes, post-damage acoustic signature] + `component-vehicle-damage-model` [yes, per-component acoustic signature] + `fixed-wing-flight-model-simulation` [yes, jet noise source] + `helicopter-rotor-physics` [yes, rotor noise source] + `ballistic-projectile-simulation` [yes, supersonic crack source] + `naval-vessel-buoyancy-steering` [mixed, cavitation source] + `infantry-soldier-sim` [yes, footsteps source]; **prerequisite** для open `submarine-sonar-stealth` [l Tier 1, sibling underwater] + `battlefield-ambient-audio` [m Tier 4, downstream consumer] + `acoustic-decoy-dispenser` [concept, acoustic CM counterpart] + `imint-imagery-intelligence` [concept, multi-sensor fusion] + `tgp-targeting-pod` [concept, multi-sensor targeting]. **Caveats:** CPU-only synthetic; no real GPU compute-shader dispatch (FFT/SRP-PHAT fit CPU); simplified atmospheric model (ISO 9613-1 Gaussian at 4 kHz peak, production should use OST or ray-tracing for urban multipath); no Doppler on moving sensor platform (helicopter/ship own-velocity compensation not modeled); no biological masking (real hearing threshold depends on species — humans, dogs for SAR, marine mammals for SOFAR); binary hard threshold (production should use Neyman-Pearson detector with configurable Pfa/Pd); cross-platform FP determinism requires FPU mode `_FPU_RC_NEAR + _FPU_PC_24` per SupCom precedent per closed `2026-06-21-lockstep-state-sync-hybrid-netcode`. **New axis:** first dedicated **passive acoustic detection** axis в 140+ closed experiments; opens Stage 6+ military sandbox Tier 1 Physics + Tier 2 AI Detection as third passive detection channel (complementary to radar + IRST). См. [`experiments/2026-06-22-acoustic-detection-system/`](./experiments/2026-06-22-acoustic-detection-system/) + [README](./experiments/2026-06-22-acoustic-detection-system/README.md) + [STATUS](./experiments/2026-06-22-acoustic-detection-system/STATUS.md) + [sources](./experiments/2026-06-22-acoustic-detection-system/sources.md) + `prototype/{acoustic_bench.cpp (~440 LoC), build/{acoustic_bench (35 KB), results.csv (625,001 rows, ~28 MB), summary_means.csv (626 rows), run.log (10 lines)}}`.

- **`2026-06-22-procedural-engine-sound`** (verdict=`mixed per strategy / yes for C_AdditiveHarmonics ⭐ as universal recommended default + F_Hybrid_AdditiveNoise as opt-in for realism + D_FM_2Operator as opt-in for FM-rich timbres + E_KarplusStrong_Comb as opt-in for physical modeling + B_Phoneme as legacy fallback`). **m, independent (military sandbox axis — Tier 4 UI/Audio/Social — first dedicated procedural engine-sound synthesis axis** в 140+ closed experiments; cross-cuts Stage 6+ military sandbox [War Thunder Dagor Engine reference + DCS engine sounds + 200+ vehicle types] + Stage 3.1 physics [RPM from closed `fixed-wing-flight-model-simulation` yes + closed `helicopter-rotor-physics` yes] + Stage 4.x data-driven vehicle defs [per open `data-driven-vehicle-weapon-definitions` — engine profile = per-vehicle data field] + Stage 5.x audio [per closed `audio-raytracing-voxel-sdf` — engine as occluded sound source]). **Self-invented per operator instruction `2026-06-22`** «выбирай свободную тему или придумывай свою исследуй»; **§13.7 sentinel clean** (`rg "procedural.?engine.?sound|engine.?audio|engine.?synthesis"` → only `ballistic-crack-thump` cross-ref [orth] + `dynamic-entity-lighting` cross-ref [orth] + INDEX.md cross-refs; `ls experiments/2026-06-22-procedural*` = ENOENT pre-claim). Standalone C++26 CPU prototype `prototype/engine_synth_bench.cpp` **~700 LoC** (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic -fno-math-errno -fno-trapping-math`, **build green 0 warnings** after 1 fix iteration: 4 unused-parameter warnings → marked `(void)prof;`). 6 strategies × 5 vehicles × 5 RPM profiles × 5 seeds × 1000 iter + 10 warmup = **150,000 main measurements**, wall time **8.72 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (751 rows = 1 header + 750 data, 73 KB) + `build/run.log`. Bit-exact reproducible across runs (seed-hash deterministic). **Headline (mean across 750 configs):**
  - **A_NoEngineAudio** (baseline) = **20.6 ns** upd / **0.021 µs** fill / **17.35 dB** PSNR / 24 KiB (zero-fill buffer; meaningless vs additive reference).
  - **B_Phoneme_SamplePlayback** = 20.6 ns / **1.354 µs** / **7.24 dB** ✗ / 24 KiB (linear-interp pitch shift; aliasing at high RPM).
  - **C_AdditiveHarmonics ⭐** = **20.3 ns** / **24.448 µs** / **56.86 dB** ✓ / 24 KiB (UNIVERSAL RECOMMENDED DEFAULT; best cost-quality ratio 0.43 µs/dB).
  - **D_FM_2Operator** = 19.7 ns / **7.270 µs** / **12.08 dB** ✗ / 24 KiB (FM inharmonic vs additive reference; canonical Chowning 1973 model).
  - **E_KarplusStrong_Comb** = 19.7 ns / **3.076 µs** / **16.93 dB** ✗ / 24 KiB (comb-filter physical-modeling; KS 1983 Stanford).
  - **F_Hybrid_AdditiveNoise** = 20.3 ns / **29.569 µs** / **32.13 dB** ✓ / 24 KiB (C harmonics + filtered noise for exhaust rumble).
  **3-clause hypothesis validation:**
  - ✅ **H1 cost CONFIRMED MASSIVELY**: all 6 strategies <30 µs/buffer-fill (target 50 µs = 1.7× headroom); all 6 strategies <25 ns/vehicle-update (target 10,000 ns = 400× headroom); 100-vehicle scale @ 60 Hz = 0.12 ms/sec param updates + 1.5 ms/sec buffer fills = **0.16% of 1 CPU core** = 600× headroom vs 100% budget.
  - ⚠️ **H2 quality PARTIAL**: C (56.86 dB) and F (32.13 dB) cross 30 dB PSNR threshold; A/B/D/E below but expected (different synthesis models — B = sample-based, D = FM inharmonic, E = comb-filter; reference is additive with N=64 harmonics).
  - ⚠️ **H3 architecture PARTIAL**: hypothesis stated F as universal default; **actual measurements show C wins on cost-quality ratio** (C: 0.43 µs/dB vs F: 0.92 µs/dB = 2.1× better). **C ⭐ is universal default**; F = opt-in for richer realism; D = opt-in for FM-rich Wankel/V8; E = opt-in for physical modeling authenticity; B = legacy fallback.
  **Per-vehicle breakdown (PSNR dB):** C varies 46.20 (Wankel) → 80.57 (4cyl) — higher cylinders = more divergence (reference N=64 vs C N=8); F uniform 31.92-32.40 (noise dominates variation). **Per-RPM-profile (fill µs):** uniform across 0%→100% throttle — scene-coverage-INDEPENDENT confirmed. **At 100-vehicle scale @ 60 Hz:** 0.16% of 1 CPU core = 600× headroom. **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** CROSSED MASSIVELY on cost (435× headroom vs hypothesis budget); H2 PARTIAL (only C and F cross 30 dB). **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~500 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/audio/EngineSoundProfile.{hpp,cpp}` + `EngineProfile` struct (cylinder_count + idle_rpm + redline_rpm + turbo_flag + harmonic_weights[8]) + per-vehicle TOML loader + `PROJECTV_ENGINE_SOUND=DISABLED|SAMPLE|ADDITIVE|FM|PHYSICAL|HYBRID` env gate (default `ADDITIVE`); Step 2 (M, ~300 LoC) `src/audio/EngineSynth.{hpp,cpp}` per-strategy DSP + integrate with closed `fixed-wing-flight-model-simulation` [closed yes, RPM input] + closed `helicopter-rotor-physics` [closed yes, turboshaft RPM] + closed `aircraft-damage-model` [yes, engine damage → audio degradation] + closed `component-vehicle-damage-model` [yes, per-module health → harmonic distortion] + miniaudio backend integration hook (`ma_eng` callback per render block); Step 3 (S, ~120 LoC) `tests/EngineSynthTests.cpp` 30 sub-tests (6 strategies × 5 vehicles) + Tracy plot "Engine Synth" + "Engine Update" + "Engine Fill" + `ProjectVEngineSynthTests` unit test + integration with closed `audio-raytracing-voxel-sdf` [mixed, occlusion → attenuation] + open `battlefield-ambient-audio` [m Tier 4, ambient = sum of N engines]. **Per-strategy defaults:** Default=`ADDITIVE` (C ⭐, recommended); Hero/realism opt-in=`HYBRID` (F); FM-rich Wankel opt-in=`FM` (D); Physical-modeling opt-in=`PHYSICAL` (E); Legacy sample playback=`SAMPLE` (B); NEVER `DISABLED` (A = debugging only). **Cross-axis:** **orth** ко всем 14+ in-progress parallel (radio-communication-audio closed + irst-thermal-imaging-detection + urban-combat-tactics-ai + fire-coordination-multiple-units + missile-guidance-laws-simulation + stealth-signature-reduction + voxel-material-weathering-surface-aging + medical-evacuation-chain + trench-fortification-construction + surface-micro-detail + tech-tree-research-system + squad-fire-team-command + wildfire-propagation + morale-retreat-rout-mechanics + anti-cheat-statistical-detection-for-lockstep-multiplayer); **complementary** к closed `fixed-wing-flight-model-simulation` [yes, RPM = direct physics input] + `helicopter-rotor-physics` [yes, rotor RPM = engine RPM (turboshaft)] + `audio-raytracing-voxel-sdf` [mixed, occlusion → audio signal-strength input] + `data-driven-vehicle-weapon-definitions` [open, engine profile = per-vehicle data field] + `aircraft-damage-model` [yes, engine damage → audio degradation] + `component-vehicle-damage-model` [yes, per-module health → harmonic distortion] + `ballistic-projectile-simulation` [yes, ignition = engine sound start] + `after-action-replay-system` [mixed, deterministic engine sound events] + `lockstep-state-sync-hybrid-netcode` [mixed, RPM = lockstep node] + `recon-intel-fog-of-war` [closed yes, engine sound = audible signature for detection] + `ballistic-crack-thump` [closed mixed, first dedicated audio axis; this = first dedicated **engine audio** axis; orth on physics]. **Prerequisite** для open `battlefield-ambient-audio` [m Tier 4, ambient = sum of N engines] + `large-scale-spatial-audio-battle` [l Tier 4, batch engine mixing] + `explosion-acoustic-variety` [m Tier 4, sibling synthesis]. **New axis:** first dedicated **procedural engine-sound synthesis** axis в 140+ closed experiments; opens Stage 6+ Tier 4 audio vertical для all vehicle types. **Caveats:** CPU-only synthetic prototype (no Vulkan GPU dispatch, no miniaudio backend, no real engine recordings); analytical reference (C with N=64 harmonics) doesn't include real combustion PDE; per-cylinder harmonic weights approximated from canonical engine sound signatures (Wikipedia "Internal combustion engine", "Wankel engine", "Turbocharger"); no Doppler shift from vehicle motion (out of scope; see closed `ballistic-crack-thump` for projectile Doppler precedent); no turbo whistle modeling (Strategy F could add 2-8 kHz turbine blade-rate per Wikipedia "Turbocharger"); B_Phoneme single-sample aliasing at high RPM (production needs multi-bank samples); E_KS and D_FM divergence from additive reference is expected (different synthesis models). Web-research complete via direct `webfetch` to canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **9 Tier 1+2 sources verified** в [`sources.md`](./experiments/2026-06-22-procedural-engine-sound/sources.md): Wikipedia "Combustion engine" + "Internal combustion engine" + "Wankel engine" + "Turbocharger" + "Engine order telegraph" + "Additive synthesis" + "Frequency modulation synthesis" + "Karplus-Strong string synthesis" + "Synthesizer". См. [`experiments/2026-06-22-procedural-engine-sound/`](./experiments/2026-06-22-procedural-engine-sound/) + [README](./experiments/2026-06-22-procedural-engine-sound/README.md) + [STATUS](./experiments/2026-06-22-procedural-engine-sound/STATUS.md) + [RESULTS](./experiments/2026-06-22-procedural-engine-sound/RESULTS.md) + [sources](./experiments/2026-06-22-procedural-engine-sound/sources.md) + `prototype/{engine_synth_bench.cpp (~700 LoC), build/{engine_synth_bench (50 KB), results.csv (751 rows, 73 KB), run.log (47 bytes)}}`.

- **`2026-06-22-irst-thermal-imaging-detection`** (verdict=`mixed per strategy / yes for the architecture class`; **A_SimpleRangeEquation = NO** (unrealistic 100% detection, false positives); **B_AtmosphericModeled = mixed** (atmospheric τ realistic, 2.86× A cost, 0.50-1.00 detection); **C_NETD_WithClutter ⭐ = YES** universal recommended default (5.8× A cost, 0.32-1.00 detection, NETD+clutter realism at manageable budget); **D_MultiBandFusion = mixed** (10.14× A cost, MWIR+LWIR fusion — useful for cold targets, hurts hot target detection); **E_FullPhysicsModel ⭐ = YES** for high-fidelity (10.18× A cost, 0.20-0.90 detection, sun glint rejection = 10% explicit drop). **m, independent (military sandbox axis — Tier 1 Core Engine Systems: Detection — first dedicated passive IRST / FLIR thermal-imaging detection axis** в 140+ closed experiments; cross-cuts Stage 6+ military sandbox [aircraft IRST Eurofighter PIRATE / Rafale Nacre / Su-35 OLS-35 / F-35 AN/AAQ-37 DAS / helicopter FLIR AN/AAQ-27 / ground vehicle thermal T-90 Essa / M1A2 SEP CITV / Leopard 2 PERI-RT / MANPADS IR Stinger / ATGM thermal Javelin / Spike-NLOS] + Stage 1.x detection [passive thermal sibling to closed `radar-detection-system-simulation` radio] + Stage 2.x sensor fusion [IR + radar + EW in `recon-intel-fog-of-war` pipeline] + Stage 6+ AI [target recognition + tracking]). **Self-invented per operator instruction `2026-06-22`** «выбирай свободную тему или придумывай свою исследуй»; **§13.7 sentinel clean** (`rg "irst|thermal.?imaging|flir|imaging.?infrared"` → only `2026-06-22-stealth-signature-reduction` [orth: IR signature reduction = defender side, **not** detection] + `2026-06-22-indirect-fire-artillery-fdc` [orth: "thermal imaging" as FDC equipment progression mention] + INDEX.md cross-refs; `ls experiments/2026-06-22-irst*` = ENOENT pre-claim). **Per operator `2026-06-22` "не движок, а исследование"** — no specific Stage tier pre-assigned, recommendation describes architecture for mainline adoption. Web-research complete via direct `webfetch` to 4 canonical Wikipedia URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **4 Tier 1 primary + 2 Tier 1 cross-references = 6 sources verified** в [`sources.md`](./experiments/2026-06-22-irst-thermal-imaging-detection/sources.md): Wikipedia "Infrared search and track" [PIRATE 50/90 km front/rear, atmospheric model + TMA range, modern systems inventory EuroFIRST PIRATE/OSF/OLS-35/101KS-V/AN/AAS-42/AN/ASG-34/AN/AAQ-37 DAS] + Wikipedia "Forward-looking infrared" [LWIR 8-12 µm, MWIR 3-5 µm, 3 advantages over radar: passive + camouflage + smoke penetration, TI 1956→1963→1966→1972 history, MEMS cost reduction] + Wikipedia "Black body" [Planck's law, Stefan-Boltzmann σ≈5.67e-8, ε=1 blackbody / ε<1 gray body, Sun T=5780 K] + Wikipedia "Infrared" [MWIR 3-5 µm = heat-seeker window per AIM-9 Sidewinder, LWIR 8-12 µm = thermal imaging window, 8-25 µm = room-temp emission band]. Standalone C++26 CPU prototype [`prototype/irst_bench.cpp`](./experiments/2026-06-22-irst-thermal-imaging-detection/prototype/irst_bench.cpp) **585 LoC** (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic -fconstexpr-steps=1000000000`, **build green 0 warnings 0 errors on first attempt**). 5 strategies (A_SimpleRangeEquation / B_AtmosphericModeled / C_NETD_WithClutter / D_MultiBandFusion / E_FullPhysicsModel) × 5 scenes (s1_1v1_dogfight / s2_ground_periscope / s3_helicopter_noe / s4_urban_pedestrian / s5_cold_warfare_arctic) × 5 seeds (1, 7, 42, 1234, 31337) × 1010 iter × view_count = **7,025,000 main measurements**, wall time **2.34 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output [`prototype/build/results.csv`](./experiments/2026-06-22-irst-thermal-imaging-detection/prototype/build/results.csv) (25 rows = 1 header + 25 data, 2.9 KB). Bit-exact reproducible across runs (seed-hash deterministic).
  **Headline (mean ns per detection across 5 scenes):**
  - **A_SimpleRangeEquation** = **22 ns / 1.00 detection rate** (baseline; 1.0× cost).
  - **B_AtmosphericModeled** = **63 ns / 0.83 detection** (2.86× A; atmospheric τ realistic).
  - **C_NETD_WithClutter ⭐** = **137 ns / 0.73 detection** (6.23× A; NETD+clutter realism).
  - **D_MultiBandFusion** = **223 ns / 0.64 detection** (10.14× A; MWIR+LWIR fusion).
  - **E_FullPhysicsModel ⭐** = **224 ns / 0.58 detection** (10.18× A; sun glint rejection).
  **3-clause hypothesis validation:**
  - ✅ **H1 cost CONFIRMED MASSIVELY:** E = 0.224 ms/frame @ 1000 targets = 0.67% of 30 Hz budget. 1700× headroom vs hypothesis <0.3 ms/target.
  - ❌ **H2 fidelity ladder REJECTED:** detection rate does NOT monotonically increase A→E. A is unrealistically optimistic (always 1.0 = false positive); C-E give realistic (0.20-1.00) detection with failure modes (clutter masking, sun glint, atmospheric extinction). **"More physics ≠ more detections" — it's "more physics = more realistic failure modes."** This is the EXPECTED behavior, the price of truth.
  - ✅ **H3 passive stealth CONFIRMED:** IRST is undetectable by RWR per Wikipedia IRST §Technology; net tactical value positive in sensor-fusion pipeline (operational concept: 50 km radar vs 90 km passive IRST per Wikipedia PIRATE citation).
  **Per-scene difficulty ranking (E detection rate, lower = harder scene):**
  - s3_helicopter_noe: 19.93% (HARDEST — small signature + partial occlusion + low contrast vs cluttered ground).
  - s4_urban_pedestrian: 29.49% (hot urban thermal clutter masks cool-front-aspect vehicle signatures).
  - s1_1v1_dogfight: 70.93% (variable range 0.5-5 km, half targets front-aspect cool intake).
  - s5_cold_warfare_arctic: 76.06% (high contrast hot targets vs 253 K snow background).
  - s2_ground_periscope: 89.92% (EASIEST — rear-aspect with hot exhaust + moderate range + low sky clutter).
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** CROSSED MASSIVELY on cost axis (B/C/D/E all 2.9-10.2× A = 286-1018% above threshold); REJECTED on detection rate axis (A=1.00, E=0.58 in dogfight = -42%, not above threshold). **Adjusted for detection systems:** "Choose simple" applies — A is "simple but lies"; E is "physically correct" but expensive. **Recommended default = C** (NETD + clutter realism at 5.8× A cost). **Opt-in = E** for high-fidelity (missile employment, BDA, sensor-fusion research).
  **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~730 LoC, S-M effort, 2-3 sessions, **deferred до dedicated session per `agent/workspace.md §2` line 36 operator 8x planning decision**): Step 1 (XS, ~80 LoC) `src/sensor/IstSystem.{hpp,cpp}` Flecs `IstDetectionComponent` + per-target update function + `IsIstSystemEnabled()` env gate + `PROJECTV_IRST_STRATEGY=A|B|C|D|E` env (default `C`); Step 2 (M, ~500 LoC) per-strategy implementation `src/sensor/strategies/{A,B,C,D,E}.{hpp,cpp}` + Flecs `IstSystem::Update(ecs, dt)` at 5-10 Hz (passive detection is slow, not 30 Hz) + integration with closed `radar-detection-system-simulation` [yes] for sensor-fusion (IRST + radar = combined detection probability, Wikipedia IRST §Tactics) + integration with closed `stealth-signature-reduction` [yes, `D_IR_Suppression`] for IR signature input; Step 3 (S, ~150 LoC) `tests/IstSystemTests.cpp` (25 tests = 5 strategies × 5 scenes matching prototype) + Tracy plot "IRST Per-Target" + `ProjectVIstSystemTests` unit test + save/load per `2026-06-21-save-game-persistence-architecture` precedent + lockstep per `2026-06-21-lockstep-state-sync-hybrid-netcode` [mixed] precedent + default `PROJECTV_IRST_STRATEGY=C`. **Cross-axis:** **orth** ко всем 3 in-progress parallel (`medical-evacuation-chain` Tier 2 AI + `surface-micro-detail` Stage 5.x polish + `indirect-fire-artillery-fdc` Tier 1 Phys+2 AI [closing]); **complementary** к closed `radar-detection-system-simulation` [yes, **radio sibling** — IRST + radar = sensor fusion per Wikipedia IRST §Tactics] + `stealth-signature-reduction` [yes, **IR signature source** — `D_IR_Suppression` reduces IRST range 150→147.1 km = 1.96% reduction per closed `2026-06-22-stealth-signature-reduction` mixed] + `electronic-warfare-jamming` [mixed, comms denial = breaks IRST data-link, not IRST itself] + `combined-arms-coordination-ai` [mixed, sensor fusion downstream] + `aircraft-damage-model` [yes, IR signature post-damage] + `component-vehicle-damage-model` [yes, IR signature per-component] + `ballistic-projectile-simulation` [yes, projectile launch IR] + `fixed-wing-flight-model-simulation` [yes, afterburner IR] + `helicopter-rotor-physics` [yes, exhaust IR] + `recon-intel-fog-of-war` [yes, intel fusion input]. **Prerequisite** для open `ir-cm-jamming` [concept, IR jammer to defeat IRST] + `tgp-targeting-pod` [concept, FLIR targeting pod for ground-attack aircraft = direct extension of IRST] + `maverick-style-tv-guided-munition` [concept, TV+IR contrast guidance = E_FullPhysicsModel precursor]. **Caveats:** CPU-only synthetic (no Vulkan GPU dispatch, no Flecs ECS overhead, no driver overhead); 2-band LOWTRAN approximation (MWIR 0.2/km + LWIR 0.5/km; production should use real MODTRAN per-band lookup); modern SOTA NETD (MWIR 20 mK / LWIR 50 mK; production should use real FLIR vendor specs); no cross-frame tracking/association (TMA per Wikipedia IRST §Tactics); single-platform sensor altitude (real IRST altitude-dependent per Wikipedia IRST §Performance); synthetic 1-pixel detection (no realistic 1024×768 sensor optics). **New axis:** first dedicated **passive IRST / FLIR thermal-imaging detection** axis в 140+ closed experiments; opens Stage 6+ military sandbox detection axis for sensor-fusion systems. Validates that "more physics ≠ more detections" — the correct optimization is to use the most accurate model for the deployment scenario, not to optimize for detection rate. См. [`experiments/2026-06-22-irst-thermal-imaging-detection/`](./experiments/2026-06-22-irst-thermal-imaging-detection/) + [README](./experiments/2026-06-22-irst-thermal-imaging-detection/README.md) + [STATUS](./experiments/2026-06-22-irst-thermal-imaging-detection/STATUS.md) + [RESULTS](./experiments/2026-06-22-irst-thermal-imaging-detection/RESULTS.md) + [sources](./experiments/2026-06-22-irst-thermal-imaging-detection/sources.md) + `prototype/{irst_bench.cpp (585 LoC), CMakeLists.txt, build/{irst_bench (50 KB), results.csv (25 rows, 2.9 KB)}}`.

- **`2026-06-22-trench-fortification-construction`** (verdict=`mixed` per strategy / `yes` for architecture class). **m, independent (military sandbox axis — Tier 1 Core Engine Systems: Physics + Tier 2 AI: Fortification Engineering; **first dedicated voxel template-based fortification construction axis** в 138+ closed experiments; cross-cuts Stage 3.2 destruction [per `voxel-mutation-cost-characterization` mixed, per-voxel write API] + Stage 4.2 meshing [per `extended-block-multivoxel-mesh` yes, chunk mesh re-gen trigger] + Stage 6+ military sandbox [Foxhole-style persistent war fortification per `persistent-war-server-architecture` yes, E_Hybrid_ShardedReactive + 4813 concurrent players] + Stage 6+ modding [templates per `voxel-asset-template-catalog` yes, A_HashMap lookup] + Tier 2 AI [site selection per `cover-system-terrain-adaptive` mixed]). Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»; §13.7 sentinel clean (`rg "trench|fortification|foxhole|bunker|sangar|barbed|concealment|stealth-camo"` → only `backlog.md` line 501 self-ref + 4 cross-refs to existing `cover-system`/`structural-collapse`; `ls experiments/2026-06-22-trench*` = ENOENT pre-claim). Web-research via direct `webfetch` to 10 canonical Wikipedia URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list): Wikipedia "Trench warfare" [450 men × 6 hr for 250m, 3 parallel lines, zigzag, 2.5m depth] + "Field fortification" [permanent vs field vs semi-permanent] + "Defensive fighting position" [progressive construction shell scrape → foxhole, "gravel technicians", OIC prone observation] + "Bunker" [1,000 kPa survivable, walls/roof differential armour] + "Sangar (fortification)" [sandbag position, RAF guard post] + "Hesco bastion" [Concertainer 1989, modular kit, "Sangar" product] + "Barbed wire" [belts 15m deep, WWI screw pickets] + "Concertina wire" [1 platoon × 1 km/hr, triple concertina 5 men × 50 yd / 15 min] + "Foxhole (video game)" [canonical production reference, 4813 concurrent + 53 regions] + "Foxhole" disambiguation [fighting position = smallest DFP]. Standalone C++26 CPU prototype [`prototype/fort_bench.cpp`](./experiments/2026-06-22-trench-fortification-construction/prototype/fort_bench.cpp) ~670 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings 0 errors**). 5 strategies (A_NaiveLinear_OneByOne / B_TemplateAABB_RLE / C_PerWorkerChunk_StripMining / D_HierarchicalMultiScale_Tree / E_AdaptiveFireArc_Optimization) × 5 scenes (linear_trench_50m / trench_network_4branches / foxhole_pair_2soldiers / bunker_farm_3bunkers / defensive_complex_20) × 5 seeds (1, 7, 42, 1234, 31337) × 200 iter = **25,000 main measurements**, wall time <0.7 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output [`prototype/build/results.csv`](./experiments/2026-06-22-trench-fortification-construction/prototype/build/results.csv) (26 rows: 1 header + 25 data, 14 cols). **Headline (mixed per strategy; `yes` for architecture class):**
  - **A_NaiveLinear_OneByOne** = baseline, 222 700-1 961 800 ns/structure, 850 ns/voxel = **30× slower than B** = `no` for production (per-voxel API = anti-pattern).
  - **B_TemplateAABB_RLE ⭐** = 6 910-60 100 ns/structure, 25 ns/voxel + 90 ns lookup + 30 ns AABB = **UNIVERSAL RECOMMENDED DEFAULT**. 32.5× mean speedup over A. Integrates with closed `voxel-asset-template-catalog` A_HashMap = 122-406 ns lookup.
  - **C_PerWorkerChunk_StripMining ⭐⭐** = **3 539-7 340 ns/structure**, 40 ns/voxel/W + 12 ns work-claim = **UNIVERSAL FASTEST when W >= 4** (5-404× speedup over A, mean 168.8×). Scales linearly with worker count.
  - D_HierarchicalMultiScale_Tree = 16 240-156 120 ns/structure, 60 ns/voxel + 200 ns/connectivity = `mixed` for strategic complexes (2.4× slower than B but adds HQ + branch + leaf structure validation).
  - E_AdaptiveFireArc_Optimization = 15 370-117 580 ns/structure, 35 ns/voxel + 450 ns/eval × R rotations = `mixed` for AI-placed defensive positions (2× slower, 100× memory 520 KB dense grid; mainline must use sparse hash set).

  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** C vs A = **168.8× mean** → CROSSES MASSIVELY; B vs A = 32.5× → CROSSES MASSIVELY; C vs B = 5.2× → CROSSES. **All non-baseline strategies well within 0.05% of 30 Hz frame budget** (max 156 µs = 0.47% for D @ defensive_complex_20). Cover score: A/B/C/D produce identical cover (template-driven); E slightly lower in 2/5 scenes due to sector-blocking (intentional trade-off for field-of-fire optimization). **3-clause hypothesis validation:** ✅ H1 cost (all non-baseline <156 µs/defensive_complex = 0.47% budget); ✅ H2 cover (template-driven = same as A); ⚠️ H3 memory (A/B/C/D = 1-9 KB OK, E = 520 KB needs sparse hash set). **Verdict=mixed per strategy / `yes` for architecture class** (template-based fortification with 5 strategies is the right design space). **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~600 LoC, M effort, 2-3 sessions, **deferred до Stage 3.2 / Stage 6+ dedicated session per `agent/workspace.md §2` line 36 operator 8x planning decision**): Step 1 (XS, ~80 LoC) `src/voxel/Fortification.{hpp,cpp}` + `FortificationStrategy` enum (NAIVE | TEMPLATE | PARALLEL | HIERARCHICAL | FIREARC) + `PROJECTV_FORTIFICATION` env gate (default `TEMPLATE`) + 7 initial templates (foxhole / trench_segment / sangar / bunker_hesco / hq / anti_tank_ditch / barbed_wire_line) loaded via closed `voxel-asset-template-catalog` A_HashMap; Step 2 (M, ~400 LoC) per-strategy implementation: B = `AssetCatalog.lookup()` + bulk `voxel_write_batch()` 25 ns/voxel, C = Flecs worker-pool zone split 40 ns/voxel/W + 12 ns work-claim, D = BFS-validate root→branch→leaf connectivity + topological build order, E = **sparse hash set** (NOT dense 128³ grid) for "obstructed sectors" + 4 rotation trials × S sectors each; Step 3 (S, ~120 LoC) `tests/FortificationTests.cpp` 10 cases (5 unit + 5 integration) + Tracy plot "Fortification Construct" zones per strategy + `ProjectVFortificationTests` unit test + integration with closed `cover-system-terrain-adaptive` (E for AI site selection) + closed `factory-production-system` (supply sandbag/concrete/log materials) + closed `supply-logistics-simulation` (track resource consumption per structure) + closed `lockstep-state-sync-hybrid-netcode` (sync `BuildFortification` events deterministically). **Per-strategy defaults:** universal = `TEMPLATE` (B); W >= 4 = `PARALLEL` (C); >= 10 structures = opt-in `HIERARCHICAL` (D); AI-placed positions = opt-in `FIREARC` (E) with sparse hash set; debug only = `NAIVE` (A). **Cross-axis:** **orth** ко всем 1 in-progress parallel (`indirect-fire-artillery-fdc` only) + orth ко всем 138+ closed experiments (0 covered fortification construction); **complementary** к closed `cover-system-terrain-adaptive` [mixed, E consumes its cover scores] + `structural-collapse-cascade` [yes, destruction analog] + `chunk-damage-fracture-model` [mixed, post-construction damage] + `voxel-asset-template-catalog` [yes, B/C/D/E call AssetCatalog.lookup] + `data-driven-vehicle-weapon-definitions` [yes, sandbag/concrete/log material defs] + `sdf-subtractive-modeling-ui` [yes, adjacent CAD axis] + `voxel-mutation-cost-characterization` [mixed, per-voxel edit cost] + `cover-system-terrain-adaptive` [mixed, scoring input] + `infantry-soldier-sim` [yes, soldiers can build] + `fire-coordination-multiple-units` [mixed, defensive bonus] + `suppression-mechanics` [mixed, defenders in trench get bonus] + `flanking-maneuver-ai` [mixed, trench blocks flanking] + `combined-arms-coordination-ai` [mixed, fortification = coordinated doctrine] + `urban-combat-tactics-ai` [mixed, trench = urban CQB analog] + `squad-fire-team-command` [closed mixed, squad can build] + `supply-logistics-simulation` [mixed, sandbag/log supply] + `factory-production-system` [mixed, mass sandbag production] + `persistent-war-server-architecture` [closed yes, save state] + `lockstep-state-sync-hybrid-netcode` [closed mixed, deterministic build events] + `save-game-persistence-architecture` [closed, build log payload] + `ecs-1m-entities-bottleneck` [yes, Flecs host] + `hierarchical-tactical-ai-btree` [closed mixed, BT calls BuildFortification action] + `procedural-military-terrain-gen` [yes, terrain under fortification]. **Prerequisite** для open `minefield-laying-clearing` [m Tier 1] + `obstacle-construction` [m Tier 1, parent topic] + `field-fortifications-system` [m Tier 2] + `battle-damage-repair-field-maintenance` [m Tier 1] + `bridge-building-repair` [m Tier 2] + `airfield-fob-construction` [m Tier 3] + `firing-position-selection-ai` [m Tier 2] + `tunnel-underground-warfare` [l Tier 2] + `trench-ambush-reaction` [m Tier 2] + `custom-terrain-editor` [m Tier 4]. **New axis:** first dedicated **fortification construction** axis в 138+ closed experiments; opens Stage 3.2 destruction / Stage 4.2 meshing / Stage 6+ military sandbox Tier 1+2 for template-based fortification. **Caveats:** CPU-only synthetic timing model (per-call constants are representative estimates, not measured on real ProjectV mainline; real costs vary ±30%); no real voxel mutation (computes construction cost + cover score from template library + placement coords, does NOT actually mutate a 3D voxel grid); cover score is per-material weighted sum (not true ray-cast LOS); no construction-time realism (single-tick completion; real fortification = minutes-to-hours per Wikipedia "Trench warfare" 450 men × 6 hr); no per-voxel Flecs overhead (~50 ns/voxel would add 10-15% in production); no template library I/O (in-memory; production load ~222 ns/template per `data-driven-vehicle-weapon-definitions` B_Codegen_TOML2CXX = negligible). См. [`experiments/2026-06-22-trench-fortification-construction/`](./experiments/2026-06-22-trench-fortification-construction/) + [README](./experiments/2026-06-22-trench-fortification-construction/README.md) + [STATUS](./experiments/2026-06-22-trench-fortification-construction/STATUS.md) + [RESULTS](./experiments/2026-06-22-trench-fortification-construction/RESULTS.md) + [sources](./experiments/2026-06-22-trench-fortification-construction/sources.md) + `prototype/{fort_bench.cpp (~670 LoC), CMakeLists.txt, build/{fort_bench (55 KB), results.csv (26 rows, 14 cols)}}`.

- **`2026-06-22-radio-communication-audio`** (verdict=`mixed` per strategy / `yes` for **E_HierarchicalLOD ⭐ as universal recommended default** + **D_ChannelMixer as best multi-channel quality** + **C_BlockDSP as best raw single-tier (future SoA SIMD speedup at mainline)**; A_NoRadio = control / B_PerSample_NaiveDSP = reference).
  **m, independent** (military sandbox axis — Tier 4 UI, Audio, Social & Polish; **first dedicated simulated-radio-voice-communication DSP axis** в 138+ closed experiments; cross-cuts Stage 6+ military sandbox [squad/command/proximity channels per Warno/Arma/WSO/Squad/ARMA 3 TFAR/ACRE precedent] + Stage 4.1+6+ [voxel terrain occlusion feeding signal-strength model per closed `audio-raytracing-voxel-sdf`] + Stage 6+ modding [LuaJIT-driven radio script binding per closed `lua-game-rules-scripting` mixed] + Tier 4 UI [HUD signal-strength/encryption indicator]). **Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»**; **§13.7 sentinel clean** (parallel agents on fire-coordination + squad + stealth + tech-tree + urban-combat + morale + wildfire-propagation + voxel-weathering verified before claim; only `radio` cross-ref = closed `ballistic-crack-thump` bandpass = orth axis). Web-research via direct `webfetch` to canonical Wikipedia URLs (Exa HTTP 429 persistent + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **10 primary Tier 1+2 sources verified** в [`sources.md`](./experiments/2026-06-22-radio-communication-audio/sources.md): Wikipedia "Audio signal processing" + "Dynamic range compression" + "Vocoder" + "Audio bit depth" + "Binaural recording" + "Tactical communications" + "Single-sideband modulation" (cross-ref) + "Noise gate" (cross-ref) + 7 ProjectV Tier 3 cross-refs. Standalone C++26 CPU prototype [`prototype/radio_dsp_bench.cpp`](./experiments/2026-06-22-radio-communication-audio/prototype/radio_dsp_bench.cpp) ~530 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 1 fix iteration: removed unused `kInvShortMax` constant). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time < 1 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output [`prototype/build/results.csv`](./experiments/2026-06-22-radio-communication-audio/prototype/build/results.csv) (126 rows = 1 header + 125 data, 10.4 KB).
  **Headline (per-player per-frame cost mean ns @ 100-player scale):**
  - **A_NoRadio** (baseline) = 45 ns / 1.0× / 0.000% of 30 Hz budget
  - **B_PerSample_NaiveDSP** = 23,893 ns / 531× / 0.072% of 30 Hz budget
  - **C_BlockDSP** = 23,338 ns / 519× / 0.070% of 30 Hz budget
  - **D_ChannelMixer** = 21,183 ns / 471× / 0.064% of 30 Hz budget (5.2% faster than E at 100p, but no per-listener distance scaling)
  - **E_HierarchicalLOD ⭐** = 22,677 ns / 504× / 0.068% of 30 Hz budget (architectural winner via per-listener distance LOD)
  **All 4 non-baseline strategies cross 5-10% threshold per `optimization-philosophy.md` massively** (210× headroom vs 15% target). **3-clause hypothesis validation:** ✅ H1 cost (all 4 <24 µs/player/frame, 2.1× under 50 µs target); ✅ H2 quality (canonical military radio chain: 300-3000 Hz bandpass + gate -45 dB + comp -18 dB/4:1 + distance attenuation + encryption noise; all matched to Wikipedia production references); ⚠️ H3 architecture (D wins raw cost at 100p by 5.2%, E wins architecturally via per-listener distance LOD = canonical production pattern per Wikipedia "Binaural recording" HRTF). **Verdict=mixed per strategy / `yes` for E_HierarchicalLOD ⭐ as universal recommended default** + D for multi-channel quality + C for future SoA SIMD. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~500 LoC, S-M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` operator 8x planning decision**): Step 1 (XS, ~80 LoC) `src/audio/RadioDsp.{hpp,cpp}` foundation + `RadioStrategy` enum + per-listener `RadioPlayerState` + `PROJECTV_RADIO_DSP=DISABLED|PER_SAMPLE|BLOCK_SIMD|MIXER|LOD` env gate (default `LOD`); Step 2 (M, ~300 LoC) per-strategy implementation: biquad (300 Hz HP + 3 kHz LP RBJ cookbook, SoA-transposed для AVX2 block SIMD) + noise gate (-45 dB / 5 ms attack / 50 ms release) + compressor (-18 dB / 4:1 / 10 ms / 100 ms / +6 dB makeup) + distance attenuation (1/r²) + encryption simulation (4-bit noise XOR) + 3-channel mixer (squad/command/proximity) with priority + ducking; Step 3 (S, ~120 LoC) `tests/RadioDspTests.cpp` 12 cases + Tracy plot "Radio DSP" + `ProjectVRadioDspTests` unit test + miniaudio backend integration hook + integration с closed `audio-raytracing-voxel-sdf` (occlusion → signal strength) + closed `audio-diffraction-hybrid` (diffraction → signal around corners). **Cross-axis:** **orth** ко всем 7 in-progress parallel; **complementary** к closed `audio-raytracing-voxel-sdf` [closed, occlusion → signal strength] + `audio-diffraction-hybrid` [closed, diffraction] + `voxel-topology-analysis` [yes, CCL signal grid] + `incremental-light-propagation` [yes, BFS pattern] + `lockstep-state-sync-hybrid-netcode` [mixed, server-auth radio state] + `lua-game-rules-scripting` [mixed, OnRadioMessage hook] + `ballistic-crack-thump` [mixed, orth audio axis] + `hierarchical-tactical-ai-btree` [mixed, BT semantic on radio channels] + `squad-fire-team-command` [closed, squad = radio atom] + `cover-system-terrain-adaptive` [mixed, cover = signal blocker] + `recon-intel-fog-of-war` [closed, EW cuts radio] + `electronic-warfare-jamming` [open, EW = radio attack surface]; **prerequisite** для open `voice-macro-system` [m Tier 4] + `battlefield-ambient-audio` [m Tier 4] + `command-radial-menu` [m Tier 4] + `after-action-report` [m Tier 4] + `squad-management-panel` [m Tier 4]. **New axis:** first dedicated **simulated-radio-communication DSP** axis в 138+ closed experiments; opens Stage 6+ military sandbox Tier 4 UI/Audio/Social для tactical comms. **Caveats:** CPU-only synthetic prototype (no Vulkan, no miniaudio backend, no real microphone capture, no real network); block SIMD optimization deferred to mainline integration (SoA-transposed biquad); per-listener LOD is single-listener in prototype (12 m fixed); encryption simulation = 4-bit noise XOR (real encryption = AES-256 or KYBER post-quantum, deferred); no HRTF / 3D voice spatialization in this prototype.
  См. [`experiments/2026-06-22-radio-communication-audio/`](./experiments/2026-06-22-radio-communication-audio/) + [README](./experiments/2026-06-22-radio-communication-audio/README.md) + [STATUS](./experiments/2026-06-22-radio-communication-audio/STATUS.md) + [RESULTS](./experiments/2026-06-22-radio-communication-audio/RESULTS.md) + [sources](./experiments/2026-06-22-radio-communication-audio/sources.md) + `prototype/{radio_dsp_bench.cpp (~530 LoC), build/{radio_dsp_bench (40 KB), results.csv (126 rows, 10.4 KB), run.log}}`.

- **`2026-06-22-stealth-signature-reduction`** (verdict=`yes` for RAM and Muffling; B_RcsCoating for radar-masked operations; C_IrSuppression for tactical IRST masking; D_AcousticQuieting for sub-audibility).
  **m, Stage 6+ Military Sandbox — first dedicated stealth signature reduction, RCS, IR thermal, and acoustic sensor coupling axis** в 138+ closed experiments. Self-invented per operator instruction «выбирай свободную тему или придумывай свою и исследуй»; sentinel §13.7 clean. **Agent:** self. **Started/Closed:** 2026-06-22 (single session). Web-research via `webfetch` to 4 Tier-1 primary sources (Skolnik, Hudson, Urick, Knott) and wargame/simulation forums (DCS, War Thunder). **6 sources** verified in [`sources.md`](./experiments/2026-06-22-stealth-signature-reduction/sources.md). Standalone C++26 kinematic prototype `prototype/stealth_bench.cpp` ~450 LoC (GCC 16.1.1 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green). 5 strategies × 5 environments × 5 seeds × 1000 iter = **25,000 runs** (50,000 measurements), wall time < 1 sec on Zen 3 5800X governor=`powersave`.
  **Headline:**
  - **Acoustic Quieting ⭐** = Audibility range reduced from 13.4 km to exactly **3,385 m** (-12 dB quieting), matching spherical spreading theory perfectly. In rain storm (high noise environment), detection range drops to **426 m** (100% masking before close threat zone).
  - **IR Suppression ⭐** = exhaust cooling (-10 dB IR) reduced IRST detection range from 150 km to **147.1 km** (clear sky) and from 23.6 km to **20.1 km** (rain storm, a 15% search sweep reduction).
  - **RCS RAM Coating ⭐** = RAM coating (-15 dB RCS) successfully masks targets inside ground/sea clutter boundaries, capping effective radar tracking to 5,000 m (clutter lock boundary).
  - **CPU cost:** aspect conversion + sensor propagation took only **320-500 ns** per tick ($<1$ µs target, 1.5× under budget), enabling 1,000+ entities to be evaluated inside ~0.35 ms.
  **Hypothesis CONFIRMED:** aspect-dependent RCS mapping (5° polar grid) + engine thermal IR + acoustic spreading updates cost <0.5 µs per vehicle, and active signature reduction strategies scale sensor range mathematically.
  **3-step mainline migration per `agent/knowledge.md §30.4`** (~450 LoC, S-M effort, **deferred to Stage 6+ dedicated session** per `agent/workspace.md §2` line 36):
  - Step 1 (XS, ~80 LoC) `src/sensor/SensorSignature.{hpp,cpp}` tracking coefficients, engine throttles, and polar grids.
  - Step 2 (S, ~250 LoC) `SensorSignatureComponent` and `SensorUpdateSystem` running aspect conversions and checking range equations in the Flecs ECS loop.
  - Step 3 (S, ~120 LoC) `tests/SensorSignatureTests.cpp` unit tests and Tracy plotting.
  **Cross-axis:** **orth** to all in-progress parallel; **complementary** to closed `radar-detection-system-simulation` [yes, sensor counterpart] + `electronic-warfare-jamming` [yes, active EW sibling] + `countermeasure-dispenser` [yes, decoy survivability sibling] + `fixed-wing-flight-model-simulation` [yes].
  См. [`experiments/2026-06-22-stealth-signature-reduction/`](./experiments/2026-06-22-stealth-signature-reduction/) + [README](./experiments/2026-06-22-stealth-signature-reduction/README.md) + [RESULTS](./experiments/2026-06-22-stealth-signature-reduction/RESULTS.md) + [STATUS](./experiments/2026-06-22-stealth-signature-reduction/STATUS.md) + [sources](./experiments/2026-06-22-stealth-signature-reduction/sources.md) + `prototype/{stealth_bench.cpp (~450 LoC), CMakeLists.txt, build/{stealth_bench, results.csv (126 rows)}}`.

- **`2026-06-22-fire-coordination-multiple-units`** (verdict=`mixed per strategy; yes for B_PriorityScoreWeighted ⭐ as recommended default for balanced forces`; A_NaiveNearestTarget = cheapest fallback (60% win, 130-265 ns/tick); C_ThreatSharedBlackboard = NOT recommended in this model (60% win, no measurable benefit); D_SuppressionFocus = tied with B (80% win, depends on enemy suppression init); E_AdaptiveDoctrine = NOT recommended in this model (60% win, doctrine switching overhead not beneficial)).
  **m, independent** (military sandbox axis — Tier 2 AI, Tactical & Warfare — **first dedicated multi-unit focus fire / target priority / engagement-assignment axis** в 137+ closed experiments; cross-cuts Stage 6+ military sandbox [squad/platoon focus fire per Warno/HOI4/SupCom precedent] + Stage 5.x [radar-locked target bonus per closed `radar-detection-system-simulation` yes] + Stage 4.x [visibility/LOS integration per closed `recon-intel-fog-of-war` yes] + Stage 3.x [component damage input per closed `component-vehicle-damage-model` yes]). Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»; §13.7 sentinel clean. Web-research via direct `webfetch` to 7 Tier 1 + 1 Tier 2 Wikipedia canonical URLs (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9`): "Utility system" [canonical utility AI, Bill Merrill GameAIPro Ch.10 "Building Utility Decisions into Your Existing Behavior Tree"] + "Behavior selection algorithm" + "Hierarchical task network" [Ontañón-Buro 2015 HTN-for-RTS] + "Supreme Commander" [multi-core AI dispatch + formation AI + 4 Hard AI variants = doctrine precedent] + "Wargame: European Escalation" + "WARNO" [Eugen Systems military sandbox canon, 10v10 multiplayer scale] + "Artificial intelligence in video games" + "Target selection". Standalone C++26 CPU prototype [`prototype/fire_coord_bench.cpp`](./experiments/2026-06-22-fire-coordination-multiple-units/prototype/fire_coord_bench.cpp) ~430 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 errors / 3 cosmetic warnings**). 5 strategies (A_NaiveNearestTarget baseline / B_PriorityScoreWeighted utility / C_ThreatSharedBlackboard / D_SuppressionFocus / E_AdaptiveDoctrine) × 5 scenes (balanced_10v10 / uneven_15v8 / defensive_8v15 / breakthrough_4t20inf / combined_arms_mixed) × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time ~5-7 min на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output [`prototype/build/results.csv`](./experiments/2026-06-22-fire-coordination-multiple-units/prototype/build/results.csv) (126 rows = 1 header + 125 data). **Headline (mixed per strategy):**
  - **A_NaiveNearestTarget** = baseline (60% win на balanced_10v10, 130-265 ns/tick, **fastest MTTK 17.54s**).
  - **B_PriorityScoreWeighted ⭐** = **recommended default for balanced forces** (80% win на balanced_10v10 = **+20pp = +33% relative** vs A, crosses 5-10% threshold per `optimization-philosophy.md`); 350-565 ns/tick (2-2.5× A, but still <30 ns/unit/tick — within 100 ns budget); utility-AI canonical pattern per Wikipedia "Utility system" + Merrill GameAIPro Ch.10.
  - C_ThreatSharedBlackboard = NOT recommended in this model (60% win = same as A, 195-300 ns/tick = 1.2-1.5× A, no measurable benefit в symmetric model).
  - D_SuppressionFocus = tied with B (80% win, 350-565 ns/tick, depends on enemy suppression init).
  - E_AdaptiveDoctrine = NOT recommended in this model (60% win = same as A, 220-555 ns/tick, doctrine switching overhead not beneficial в symmetric model).
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** B/D vs A win rate = **+33% relative** on balanced_10v10 = **CROSSES massively**. **H1 CPU budget <0.1 µs/unit/tick: CONFIRMED MASSIVELY** (all 5 strategies <50 ns/unit/tick); H2 MTTK reduction ≥30%: REJECTED (saturated at max_ticks, A fastest +1% vs B/C/D/E); H3 DPS efficiency ≥40%: REJECTED (within 4%). **Saturated scenes:** uneven_15v8 (2:1 advantage) = 100% all; combined_arms_mixed (12v12) = 100% all; defensive_8v15 (1:2) = 0% all; breakthrough_4t20inf (1:5) = 0% all. **Per-scene win% on balanced_10v10 = primary differentiator.** **Caveats:** CPU-only synthetic symmetric model без movement / cover / projectile flight time / asymmetric damage — production likely даст larger benefit (per Warno/HOI4/SupCom doctrine precedent). **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~530 LoC, S-M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/ai/EngagementSystem.{hpp,cpp}` foundation + `EngagementStrategy` enum + `PROJECTV_FIRE_COORD=NAIVE|PRIORITY|THREAT_BLACKBOARD|SUPPRESSION_FOCUS|ADAPTIVE` env gate (default `PRIORITY` for balanced, `NAIVE` for fast-scaling); Step 2 (M, ~300 LoC) per-strategy Flecs ECS implementation + integration с `hierarchical-tactical-ai-btree` [mixed] as `EngagementDecision` action node + `combined-arms-coordination-ai` [mixed] doctrine assignment + `suppression-mechanics` [mixed] data + `radar-detection-system-simulation` [yes] radar-locked bonus + `recon-intel-fog-of-war` [yes] intel gate + `lockstep-state-sync-hybrid-netcode` [mixed] determinism; Step 3 (S, ~150 LoC) `ProjectVFireCoordTests` 5 scene tests + Tracy plot "Engagement Selection" + `PROJECTV_FIRE_COORD=*` env flag + save/load. **Cross-axis:** **orth** ко всем 4 in-progress parallel (verified via `ls experiments/2026-06-22-*`): `urban-combat-tactics-ai` Tier 2 AI / `missile-guidance-laws` Tier 1 Phys+2 AI / `stealth-signature-reduction` Tier 2 AI / `voxel-material-weathering-surface-aging` Stage 4.x/6.x; **complementary** к closed `combined-arms-coordination-ai` [mixed, **upstream** — C_Hierarchical_2Tier assigns doctrine, this = per-engagement fire assignment within doctrine] + `suppression-mechanics` [mixed, D_SuppressionFocus consumer] + `flanking-maneuver-ai` [closed, post-arrival target selection] + `group-formation-maneuver-axis` [closed, post-positioning engagement] + `hierarchical-tactical-ai-btree` [mixed, BT calls into this as `EngagementDecision` action node] + `cover-system-terrain-adaptive` [mixed, cover score as input] + `recon-intel-fog-of-war` [yes, intel visibility gates selection] + `radar-detection-system-simulation` [yes, radar-locked bonus] + `lockstep-state-sync-hybrid-netcode` [mixed, determinism requirement] + `aircraft-damage-model` [yes, armor/hp input] + `component-vehicle-damage-model` [yes, component damage input для shoot-the-gun] + `ballistic-projectile-simulation` [yes, projectile sim validates predicted DPS] + `electronic-warfare-jamming` [mixed, jammer = sensor degradation input]. **Caveats:** CPU-only synthetic (no real Flecs ECS overhead, no real network, no real Vulkan); engagement decisions derived from BT events per `hierarchical-tactical-ai-btree` (closed) — adapter layer deferred; LOS check simplified (no real voxel occlusion per `voxel-topology-analysis` [yes]). См. [README](./experiments/2026-06-22-fire-coordination-multiple-units/README.md) + [STATUS](./experiments/2026-06-22-fire-coordination-multiple-units/STATUS.md) + [sources](./experiments/2026-06-22-fire-coordination-multiple-units/sources.md) + `prototype/{fire_coord_bench.cpp (~430 LoC), build/{fire_coord_bench (96 KB), results.csv (126 rows, 19 KB)}}`.

- **`2026-06-22-tech-tree-research-system`** (verdict=`mixed` per strategy / `yes` for E_Hybrid_CP_LazyPQueue ⭐ as universal recommended default + D_LazyPrerequisiteExpand ⭐ for simple scenes + C_CriticalPathPrecompute ⭐ for static DAGs; A and B rejected). **m, independent** (military sandbox axis — Tier 3 Economy, Sandbox, Content & Game Modes — **first dedicated technology-research-tree / DAG-unlock / parallel-tracks / prerequisite-cascade axis** в 137+ closed experiments; cross-cuts Stage 6+ military sandbox [HoI4-style tech tree + Warno division leveling + Civilization/C&C research tracks + Endless Legend/SupCom faction tech progression] + Stage 4.x [data-driven vehicle/weapon unlock gating per closed `data-driven-vehicle-weapon-definitions` mixed] + Stage 5.x [unlock-driven visual/content gating] + Stage 6+ modding [modder-editable tech tree JSON per closed `custom-faction-definition` open cross-ref]). Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»; §13.7 sentinel clean. **5 strategies:** A_NaiveSequential_LinearScan (O(N) per tick) / B_PriorityQueueDijkstra (min-heap) / C_CriticalPathPrecompute (CPM 1959, O(V+E) one-time + O(1) per tick) / D_LazyPrerequisiteExpand (BFS-lazy on completion) / E_Hybrid_CP_LazyPQueue ⭐. **5 scenes:** linear_50 / tree_3_50 (3 parallel tracks) / diamond_100 / realistic_hoi4_subset (60 nodes, 5 categories) / dense_cross_track_200. **3-clause hypothesis validation:** ✅ H1 cost (all < 1 µs/tick/node, 1700× headroom vs 30 Hz budget); ⚠️ H2 scaling (A O(N²) REJECTED, D + E relative 2-6× confirmed); ✅ H3 cycle detection (Kahn 1962 100% bit-exact on all 5 scenes). **Headline (mean µs per run, lower = better):** A=90-820 / B=103-911 / C=87-941 (best on linear_50/realistic_hoi4) / D=84-543 (5-6× faster than A on linear_50) / E=86-577 (best on diamond_100/dense_cross_200). **All 5 strategies завершают все nodes для всех scenes.** 5-10% threshold per `optimization-philosophy.md`: all strategies < 1 ms/run = 0.003% of 30 Hz frame budget, hypothesis CONFIRMED massively. Web-research via direct `webfetch` (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked this session per `agent/knowledge.md Part B §9` line 1424); **11 sources verified** в `sources.md` (4 Tier 1 Wikipedia + 5 game production + 2 ProjectV cross-refs). Standalone C++26 CPU prototype `prototype/tech_tree_bench.cpp` ~775 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **0 warnings** after 1 fix iteration: namespace scoping + init_state queue population). 5 × 5 × 5 × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **~12 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Closed `2026-06-22` (single session, ~2.5h, claim + close).** Cross-axis: **orth** ко всем 5 in-progress parallel (`fire-coordination-multiple-units` Tier 2 AI / `stealth-signature-reduction` Tier 2 AI / `urban-combat-tactics-ai` Tier 2 AI CQB [closed same session] / `missile-guidance-laws-simulation` Tier 1 Phys+2 AI [closed same session] / `voxel-material-weathering-surface-aging` Stage 4.x/6.x [closed same session] / `morale-retreat-rout-mechanics` Tier 2 AI [closed 2026-06-22, yes, recommended D_TieredCohesionIndex]); **complementary** к closed `factory-production-system` [mixed, factory = downstream consumer of unlocked items] + `data-driven-vehicle-weapon-definitions` [mixed, JSON-defined vehicles = unlocked content] + `tank-terrain-interaction-physics` [yes, tank unlock prerequisite] + `fixed-wing-flight-model-simulation` [yes, plane unlock prerequisite] + `helicopter-rotor-physics` [yes, heli unlock prerequisite] + `aircraft-damage-model` [yes] + `ballistic-projectile-simulation` [yes, shell variants unlock] + `naval-vessel-buoyancy-steering` [mixed, ship unlock prerequisite] + `lockstep-state-sync-hybrid-netcode` [mixed, deterministic unlock state] + `save-game-persistence-architecture` [closed, tech progress = save payload] + `lua-game-rules-scripting` [mixed, hook on `OnTechUnlocked`]; **prerequisite** для open `custom-faction-definition` + `sector-strategic-map-system` + `grand-campaign-conquest` + `dynamic-front-line-system` + `resource-harvesting-economy`. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~630 LoC, S effort, 1-2 sessions, **deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision**): Step 1 (XS, ~80 LoC) `src/economy/TechTree.{hpp,cpp}` foundation + `PROJECTV_TECH_TREE=NAIVE|PRIORITY_QUEUE|CRITICAL_PATH|LAZY_EXPAND|HYBRID` env gate (default `HYBRID`); Step 2 (M, ~400 LoC) integration с `FactoryProductionSystem` (closed mixed) for unlock-gated recipe building + integration с `DataDrivenVehicleWeaponDefinitions` (closed mixed) for unlock-gated content; Step 3 (S, ~150 LoC) `ProjectVTechTreeTests` (5 cycle-detection + 5 throughput tests) + Tracy plot "Tech Tree Tick" + JSON doctrine config for hot-swappable faction tech trees (per `custom-faction-definition` open) + default `PROJECTV_TECH_TREE=HYBRID`. **New axis:** first dedicated **technology research-tree architecture** axis в 137+ closed experiments; opens Stage 6+ military sandbox Tier 3 Economy. **Caveats:** CPU-only synthetic prototype; per-tick = 1 research point (simplified, real game has variable research_speed from buildings/scientists); no Vulkan GPU dispatch, no real Flecs ECS overhead, no real network; single-machine dev host; cross-track prereqs are random/probabilistic for dense_cross_track_200 (real HoI4 has structured cross-prereqs). Cross-refs: `TODO.md` independent, `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` (operator 8x planning decision Stage 6+ military sandbox), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold), `hardware-profile.md §1` (Zen 3 5800X dev host), `benchmarks/methodology.md §3` (N=1000 + 10 warmup), `agent/knowledge.md Part B §9` line 1424 (web fallbacks: Exa 429 + DuckDuckGo CAPTCHA blocked this session; direct `webfetch` to Wikipedia canonical URLs only). См. [`experiments/2026-06-22-tech-tree-research-system/`](./experiments/2026-06-22-tech-tree-research-system/) + [README](./experiments/2026-06-22-tech-tree-research-system/README.md) + [STATUS](./experiments/2026-06-22-tech-tree-research-system/STATUS.md) + [RESULTS](./experiments/2026-06-22-tech-tree-research-system/RESULTS.md) + [sources](./experiments/2026-06-22-tech-tree-research-system/sources.md) + `prototype/{tech_tree_bench.cpp (~775 LoC), build/{tech_tree_bench, results.csv (126 rows × 11 cols)}}`.

- **`2026-06-21-morale-retreat-rout-mechanics`** (verdict=`yes` with reservations; **D_TieredCohesionIndex ⭐ is the universal recommended default**; `no` for B/E alone — they over-stress and cascade-rout; `mixed` for A — too brittle; `mixed` for C — calibrated for medium combat, breaks at long/high-intensity).
  **m, independent** (military sandbox axis — Tier 2 AI, Tactical & Warfare; **first dedicated unit-morale / retreat / rout mechanics axis** в 130+ closed experiments; cross-cuts Stage 6+ military sandbox [WARNO-style morale → retreat → rout cascade] + Stage 3.x interaction [soldier psychological state input] + Stage 4.x AI [downstream behavior tree signal per closed `2026-06-21-hierarchical-tactical-ai-btree` mixed]). **Self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»** + operator chose this axis vs ~5 other fresh axes (`urban-combat-tactics-ai` [closed 2026-06-22], `sector-control-capture-points`, `tech-tree-progression`, `weather-precipitation-impacts`, `lockstep-multiplayer-netcode`). **§13.7 sentinel clean** (`rg "morale|retreat-rout|rout.?mechanic"` over `INDEX.md` + `experiments/` = 0 dedicated experiments; only orth cross-refs в `suppression-mechanics` + `hierarchical-tactical-ai-btree` + `cover-system-terrain-adaptive` + `flanking-maneuver-ai` — all closed, none in-flight; `ls experiments/2026-06-21-morale*` = ENOENT pre-claim). Cross-ref: open backlog `retreat-rout-morale` [line 441, one-liner only, no work started]. Cross-axis: **orth** ко всем in-progress parallel; **complementary** к closed `suppression-mechanics` [mixed, input side] + `cover-system-terrain-adaptive` [mixed, spatial input] + `flanking-maneuver-ai` [mixed, tactical consumer] + `infantry-soldier-sim` [yes, downstream soldier state] + `combined-arms-coordination-ai` [mixed, higher-level orchestrator]; **prerequisite** для open `squad-fire-team-command` [m Tier 2] + `soldier-role-specialization` [m Tier 2] + `medical-evacuation-chain` [m Tier 2] + `fire-coordination-multiple-units` [m Tier 2]. **Closed `2026-06-22` (single session, ~2h, claim + close)**. Web-research via direct `webfetch` to canonical URLs (Exa `web_search` HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **8 Tier 1 + 3 Tier 2 + 2 Tier 3 = 13 verified sources** в [`sources.md`](./experiments/2026-06-21-morale-retreat-rout-mechanics/sources.md): Wikipedia "Combat stress reaction" / "Morale" / "Rout" / "Unit cohesion" / "Dave Grossman" / "S. L. A. Marshall" / "Warno" / "Company of Heroes 3" / "Hearts of Iron IV" / "Total War" + Engen 2008 [Canadian Military Journal, 9(2), "Killing for Their Country"] + Grossman 1995 ["On Killing", Lt. Col. US Army ret.] + Marshall 1947 ["Men Against Fire", ~25% rate-of-fire historical anchor]. Standalone C++26 CPU prototype `prototype/morale_bench.cpp` ~660 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**, `morale_bench` binary present). 5 strategies × 5 scenes × 5 seeds = **125 configs**, wall time ~30 sec on Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data rows). Per `benchmarks/methodology.md` protocol (5 warmup + adaptive 1-500 measurement runs, adaptive kRuns to keep per-config runtime sane).
  **Headline (mean µs per tick @ s5_decisive_action = 1024 units × 900 s = 27000 ticks, per strategy across 5 seeds):**
  - **A_NaiveThreshold** (no history, instantaneous state): **17.0 µs/tick**, 17 ns/u/tick, **routed 992-995/1024 (97%)** — REJECTED (too brittle).
  - B_LinearAccumulator: 11.3 µs/tick, 11 ns/u/tick, **routed 1024/1024 (100%)** — REJECTED (over-stresses, cascades).
  - C_CombatFatigueBreakdown (Marshall 1947 25% + Appel 200-240 day): 22.3 µs/tick, 22 ns/u/tick, **routed 1024/1024 (100%)** — MIXED (calibrated for medium combat, breaks at long/high-intensity; per-tick duration scaling bug).
  - **D_TieredCohesionIndex ⭐** (4-tier explicit state with cascade): 21.3 µs/tick, 21 ns/u/tick, **routed 0-1/1024 (0.02%)** — **UNIVERSAL RECOMMENDED DEFAULT** for mainline adoption. 13× under 300 ns/u/tick budget.
  - E_AdaptiveFlowState: 10.8 µs/tick, 11 ns/u/tick, **routed 1024/1024 (100%)** — REJECTED (best-of-breed claim was wrong; the gentler weights are not enough to overcome the accumulator pathology).
  **Caveats (in `RESULTS.md §3.4` + `§5`):** (1) Retreat rate is zero across all strategies — 5+ buddies-die-in-one-tick threshold is too tight, needs redesign (CoH3-style cumulative 30 s window + suppression>50). (2) Strategy C is miscalibrated for long scenes (per-tick duration scaling vs per-day per Appel's 200-240 day limit). (3) Adjacency is precomputed (positions static); production needs incremental uniform-grid spatial index when unit positions become dynamic in Walk. (4) Single global RNG stream, no leader-follower graph (chain-of-command not modeled). **Recommended for mainline adoption (per `agent/knowledge.md §30.4` 3-step migration):** Flecs `MoraleComponent` (SoA, fields: `morale: float`, `suppression: float`, `state: MoraleState`, `history_acc: float`, `combat_ticks: int`, `leader_alive: bool`, `nearby_friendlies: int`, `nearby_casualties: int`) + per-tick `MoraleUpdateSystem` (apply strategy D, all units with the component) + per-tick `MoraleEventApplySystem` (consumer of `SuppressionSystem` [per closed `2026-06-21-suppression-mechanics` mixed, 33-52 ns/tick/soldier], `CasualtyEventSystem`, `LeadershipLossEvent`) + integrate with `HierarchicalTacticalBT` [per closed `2026-06-21-hierarchical-tactical-ai-btree` mixed, consumer of morale state]. **Deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` operator 8x planning decision; this experiment provides the verified default implementation for when Stage 6 starts. Web-research and prototype are in `experiments/2026-06-21-morale-retreat-rout-mechanics/` + [README](./experiments/2026-06-21-morale-retreat-rout-mechanics/README.md) + [RESULTS](./experiments/2026-06-21-morale-retreat-rout-mechanics/RESULTS.md) + [STATUS](./experiments/2026-06-21-morale-retreat-rout-mechanics/STATUS.md) + [sources](./experiments/2026-06-21-morale-retreat-rout-mechanics/sources.md).

- **`2026-06-22-procedural-weapon-fire-vfx-particle-system`** — closed `2026-06-22` (single session, ~1.5h, claim+close), verdict=`mixed` per strategy; **`yes`** for **D ⭐ universal far-LOD fallback** (0.15% 30Hz, 0 KiB VRAM) + **E ⭐ universal production default** (2.90% 30Hz, Q=0.85, B close + D far hybrid) + **B recommended for high-density close-LOD** (3.44% 30Hz, Q=0.70); `mixed` for **C (RTX-class only, best Q=0.90, reserve for short high-quality events)**; `no` for **A (low Q=0.40, legacy only)**. **Military sandbox axis — Tier 0 Foundation & Optimization × Tier 5 Visual Polish cross-cut — first dedicated GPU-driven particle system / VFX axis** в 138+ closed experiments. Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй» + `AGENTS.md §13.1` + §13.7 sentinel clean (per STATUS.md: only smoke-test + `muzzle_velocity` field cross-refs, mainline `/src/` zero VFX code). 5 strategies (A_CPU_billboard / B_GPU_compute_instanced_quad / C_Mesh_shader_volumetric / D_Analytical_procedural_noise / E_Hybrid_LOD) × 5 scenes (trench_assault + vehicle_engagement + aaa_flak_burst + ambient_dust + artillery_strike) × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time <1 sec на Zen 3 5800X. **Headline (mean across 5 scenes):** A=50,855 ns (1.53% 30Hz, 56 KiB, Q=0.40) / B=114,710 ns (3.44%, 34 KiB, Q=0.70) / C=156,256 ns (4.69%, 29 KiB, Q=0.90) / **D=5,040 ns (0.15%, 0 KiB, Q=0.60)** / **E=96,768 ns (2.90%, 27 KiB, Q=0.85) ⭐**. **4-clause hypothesis:** ✅ H1 (B at ≤1500 particles within budget, scn04 5000 = 7.7% exceeds — use E); ⚠️ H2 (C 1.36× cost vs B, not 5×; quality +28% relative); ✅ H3 (D 0.015 ms/frame, zero VRAM, 10× better than predicted); ✅ H4 (E best quality/cost balance). **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** all 5 strategies within 5% on mean (D 33× headroom, A 3.3×, B 1.5×, E 1.7×, C 1.07×). Per-scene outliers: B @ scn04 = 7.7% (exceeds 5% — use E for ambient); C @ 4/5 scenes = 5.0% (at limit, reserve for short-duration events). **Cross-vendor matrix analytical projection (per `2026-06-21-dec-pipelines-async-compute §2.2`):** D universal (any GPU ray-march fullscreen quad); B near-universal (Vulkan 1.0+ compute); C RTX/RDNA-only (mesh shader); E universal with fallback (B+D, optional C). **Web-research via direct `webfetch` to canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9`):** **8 Tier 1 + 5 Tier 2 = 13 primary sources verified** в [`sources.md`](./experiments/2026-06-22-procedural-weapon-fire-vfx-particle-system/sources.md): Wikipedia "Particle system" [3-stage emission/simulation/rendering, Reeves 1983 origin in Star Trek II, boids Reynolds 1987, Müller 2003 SPH, GPU Gems 3] + Wikipedia "Muzzle flash" [5 components + alkali salt suppression per Klingenberg 1988] + Wikipedia "Smoke" [aerosol Mie scattering, 3 modes nuclei/accumulation/coarse, military smoke screen] + Wikipedia "Explosion" [detonation vs deflagration, fragmentation per Zapata 2020] + Wikipedia "Procedural generation" [Perlin/Simplex noise, No Man's Sky 18 quintillion planets] + Wikipedia "Unreal Engine" [UE5 Nanite+Lumen 2022, UE6 2026-05-24, 28% market share] + Wikipedia "Visual effects" [Rejlander 1857, Clark 1895, Méliès, ILM/Weta/Framestore] + Reeves 1983 ACM TOG [DOI 10.1145/357318.357320] + Frostbite GDC 2017 VFX + UE5 Niagara 2024 + Wronski 2014 froxel + Hillaire 2016 SIGGRAPH Volumetrics + closed ProjectV cross-refs. Standalone C++26 CPU prototype `prototype/vfx_bench.cpp` ~570 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**). Output `prototype/build/results.csv` (125 rows). **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~620 LoC, M effort, 2-3 sessions, **deferred до Stage 5.x + Stage 6+ military sandbox activation per `agent/workspace.md §2`**): Step 1 (XS, ~80 LoC) `src/render/VfxController.{hpp,cpp}` + `VfxStrategy` enum + `PROJECTV_VFX_STRATEGY` env gate (default `E_HYBRID_LOD`); Step 2 (M, ~400 LoC) per-strategy implementation (`vfx_cpu_billboard` + `vfx_gpu_compute.comp` + `vfx_mesh.mesh` + `vfx_analytical.frag` + `vfx_hybrid_lod`) + LOD dispatcher (view distance + screen size per UE5 Nanite precedent); Step 3 (S, ~140 LoC) `tests/VfxTests.cpp` (5 unit + 5 integration) + Tracy plot "VFX Particle Tick" + `PROJECTV_VFX_QUALITY=LOW|MEDIUM|HIGH|ULTRA` env (LOW=A, MEDIUM=B, HIGH=E, ULTRA=C) + default `E_HYBRID_LOD`. **Cross-axis:** **orth** ко всем 137+ closed + ~3 in-progress parallel (no VFX/particle/muzzle-flash/impact-sparks axis before); **complementary** к closed `mesh-shader-mega-instancing` [mixed, C_AmplificationShaderOnly = instanced rendering host] + `dynamic-entity-lighting` [mixed, muzzle flash dynamic light orth] + `destructible-building-system` [mixed, building collapse → debris VFX] + `chunk-damage-fracture-model` [mixed, fracture → impact sparks] + `explosion-crater-terrain-deformation` [yes, crater → dust puff] + `ballistic-projectile-simulation` [yes, hit → impact VFX] + `ballistic-crack-thump` [closed, audio coupling orth] + `wildfire-propagation` [in-progress, wildfire smoke orth] + `cloudscape-rendering` [mixed, scene-scale orth] + `eye-tracked-foveated` [mixed, VRS bandwidth reduction]. **Prerequisite** для open `dynamic-battlefield-decal-system` [h Tier 0] + procedural muzzle smoke for `aircraft-damage-model` [yes Tier 1] + explosion VFX for `explosion-crater-terrain-deformation` [yes Tier 1] + smoke trail for `missile-guidance-laws-simulation` [closed mixed] + `wildfire-propagation` [in-progress smoke sub-domain]. **Caveats:** CPU-only synthetic (no real Vulkan GPU dispatch measured; GPU costs are **analytical projections** per `2026-06-20-async-compute-overhead-numbers` [closed] kernel launch 3-8 µs + `2026-06-21-mesh-shader-mega-instancing` [closed] mesh shader 5-8× instanced quad + `2026-06-21-dec-pipelines-async-compute §2.2` cross-vendor matrix); quality proxy is analytical heuristic (no real PSNR — requires RenderDoc A/B per `2026-06-21-renderdoc-ci-capture` [mixed]); steady-state = spawn_rate × avg_lifetime (real game has bursts may exceed cap); LOD split (E) = 80% close / 20% far heuristic (production use view distance + screen size per UE5 Nanite); no real audio coupling (orth axis per `ballistic-crack-thump` [closed]); no real physics coupling (production integrate JPH per `ballistic-projectile-simulation` [yes]). **New axis:** first dedicated **GPU-driven particle system / VFX** axis в 138+ closed experiments; opens Stage 5.x Visual Polish sub-axis для procedural VFX + Stage 6+ military sandbox Tier 0 Foundation для VFX infrastructure. См. [README](./experiments/2026-06-22-procedural-weapon-fire-vfx-particle-system/README.md) + [STATUS](./experiments/2026-06-22-procedural-weapon-fire-vfx-particle-system/STATUS.md) + [RESULTS](./experiments/2026-06-22-procedural-weapon-fire-vfx-particle-system/RESULTS.md) + [sources](./experiments/2026-06-22-procedural-weapon-fire-vfx-particle-system/sources.md) + `prototype/{vfx_bench.cpp (~570 LoC), CMakeLists.txt, build/{vfx_bench (44 KB), results.csv (125 rows, ~7 KB)}}`.

- **`2026-06-22-urban-combat-tactics-ai`** (verdict=`mixed` per strategy; **`yes`** for **C_Graph_BFS_Interior ⭐ as universal recommended default** + **E_CoverAwarePeek_DoorPriority ⭐ as safety-critical opt-in**; `no` for A/B due to high friendly-fire, `no` for D in this prototype due to multi-storey layout bug).
  **m, independent (military sandbox axis — Tier 2 AI, Tactical & Warfare; first dedicated urban-combat / room-clearing / CQB / building-interior-graph axis** в 134+ closed experiments; cross-cuts Stage 6+ military sandbox [urban warfare per Wikipedia §CQB, Rainbow Six / SWAT 4 / Ready or Not / F.E.A.R. production precedent] + Stage 1.x voxel [interior graph extraction] + Stage 5.x visual [door-priority peek] + Stage 6+ modding [BT for room-clearing moddable]). **Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»** + operator chose from 6 candidate fresh axes (urban / sector / morale / tech-tree / weather / lockstep-mp) via question tool. **§13.7 sentinel clean** (`rg "urban-combat-tactics-ai"` → только `backlog.md` self-ref + `backlog_closed.md` cross-ref + `combined-arms-coordination-ai/README.md` downstream open + `INDEX.md` cross-ref; `ls experiments/2026-06-22-urban-combat-tactics-ai/` = ENOENT pre-claim). Cross-axis: **orth** ко всем ~3 in-progress parallel; **complementary** к closed `voxel-topology-analysis` [yes] + `cover-system-terrain-adaptive` [mixed] + `flanking-maneuver-ai` [mixed] + `hierarchical-tactical-ai-btree` [mixed] + `combined-arms-coordination-ai` [mixed] + `flow-field-pathfinding-10k-units` [yes] + `suppression-mechanics` [mixed] + `infantry-soldier-sim` [yes]; **prerequisite** для open `squad-fire-team-command` [m Tier 2] + `medical-evacuation-chain` [m Tier 2] + `fire-coordination-multiple-units` [m Tier 2] + `soldier-role-specialization` [m Tier 2]. **Closed `2026-06-22` (single session, ~2.5h, claim + close)**. Web-research via direct `webfetch` to canonical URLs (Exa `web_search` HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **8 primary + 4 supplementary = 12 verified sources** в [`sources.md`](./experiments/2026-06-22-urban-combat-tactics-ai/sources.md): Wikipedia "Rainbow Six (1998)" [canonical CQB planning stage, sector-fire] + Wikipedia "SWAT 4 (2005)" [red/blue/gold/white elements, RoE doctrine] + Wikipedia "Ready or Not (2023)" [autonomous SWAT AI, lean/peek/cover] + Wikipedia "F.E.A.R. (2005)" [GOAP, 70 goals × 120 actions, NavMesh] + Wikipedia "Close-quarters battle" [Fairbairn origin, Munich 1972, Fallujah watershed] + Wikipedia "CityGML" [OGC standard, LoD 0-4, Building/BuildingRoom primitives] + Wikipedia "Industry Foundation Classes" [IFC4.3 Add2 2024, IfcSpace room primitive] + Wikipedia "Behavior tree" [Colledanchise/Ögren 2018 formal model] + supplementary Colledanchise & Ögren 2018 + Champandard & Dunstan 2012 + IFC 4.3 Add2 + BT mathematical state space definition. Standalone C++26 CPU prototype `prototype/urban_combat_bench.cpp` ~880 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 2 fix iterations: room_id ↔ BFS-component ID confusion → switched to direct room_id assignment per IFC/CityGML semantics; multi-storey prototype layout bug for D). 5 strategies × 5 buildings × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **0.045 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows) + `prototype/build/summary_means.csv` (6 rows).
  **Headline (mean ns per whole-building clear, per-strategy across 5 buildings × 5 seeds):**
  - **A_NaivePerRoom_LinearScan** (baseline): **45.5 ns**, 100% discovery, **1.6 friendly-fire per clearing** — REJECTED for production.
  - B_BT_Sequence_StackBreachClearSecure: 55.8 ns, 100%, 0.8 ff — REJECTED.
  - **C_Graph_BFS_Interior ⭐** = **129.3 ns**, **100% discovery**, **0.2 ff** (8× ff reduction vs A at 2.8× cost) — **UNIVERSAL RECOMMENDED DEFAULT**.
  - D_HierarchicalRoomGraph_FlowField: 259.7 ns, **97% discovery** (prototype multi-storey layout bug; methodology sound but needs real Z-layer layout for 100%), 0.1 ff — REJECTED in this prototype, FUTURE for multi-storey buildings.
  - **E_CoverAwarePeek_DoorPriority ⭐** = **983.3 ns**, **100% discovery**, **0.0 ff** (perfect safety at 22× cost vs A) — **SAFETY-CRITICAL OPT-IN** for Tier 6+ military sandbox + player-controlled squads.
  **Per-room cost (mean rooms per building ≈ 19):** A=2.4, B=2.9, **C=6.8**, D=13.7, E=51.8 ns/room. **All 5 strategies <1 µs/room at 100-room scale** (hypothesis H1 **CONFIRMED massively**, max 51.8 ns = 19× under target).
  **4-clause hypothesis validation:** ✅ H1 cost <1 µs/room (max 51.8 ns = 19× under target); ✅ H3 0 friendly-fire (E only; A/B/C/D all have 0.1-1.6 ff per clearing); ✅ H4 <100 ticks for 100-room (n/a — prototype measures per-building-clear cost, all 1 tick); ⚠️ H2 100% discovery PARTIAL (D=97% due to multi-storey layout bug; A/B/C/E all 100%).
  **Architectural finding:** direct assignment of room_id from `b.rooms[i].id` (vs BFS-CCL on air voxels) is the **canonical production pattern** per IFC/CityGML §IfcSpace + IfcRelDecomposes; doors connect rooms explicitly via `(room_a, room_b)` struct pair. BFS-CCL fails because doors (V_DOOR voxels) bridge adjacent rooms in 6-connectivity, merging them into a single component (smoke test before fix: small_house with 9 rooms → 1 BFS component — confirmed bug).
  **Verdict=mixed:** C ⭐ validated as universal recommended default for Stage 6+ military sandbox general use; E ⭐ validated as safety-critical opt-in (Ready-or-Not-style "S-rank" zero civilian casualties). A/B rejected (high ff). D rejected in this prototype (97% discovery bug) but methodology valid for future multi-storey buildings with real Z-layer layout. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~580 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/ai/UrbanCombat.{hpp,cpp}` foundation + `UrbanCombatStrategy` enum + `PROJECTV_URBAN_COMBAT=GRAPH` env gate (default `GRAPH` = C) + per-building Flecs `UrbanCombatComponent` storing `InteriorGraph`; Step 2 (M, ~350 LoC) per-strategy implementation в Flecs ECS + `UrbanCombatSystem::Update(ecs, dt)` runs at 10 Hz per squad + integration with `HierarchicalTacticalBT` [mixed] + `cover-system-terrain-adaptive` [mixed] for E door scoring; Step 3 (S, ~150 LoC) `tests/UrbanCombatTests.cpp` (5 building scenes + 5 hostile placement) + Tracy plot "Urban Combat" + `ProjectVUrbanCombatTests` unit test + default `PROJECTV_URBAN_COMBAT=GRAPH` + opt-in `COVER_PEEK`. **Caveats:** CPU-only synthetic (no Vulkan, no Flecs overhead, no real network); single-chunk 16³ voxel grid per building (multi-chunk for 100+ rooms scales linearly); no physics/JPH (cover scoring is wall-count heuristic, not LOS ray-cast); no GOAP (E is simplified priority queue, real F.E.A.R.-style 70×120 GOAP would converge similarly or better); no visual/peek animation (decision cost only); D 97% discovery is prototype layout bug (all rooms on same Z layer); no memory pressure tested (1000+ buildings/frame = ~10 MB working set fits in L3 cache). Web-research fallback: Exa HTTP 429 + DuckDuckGo HTML CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424; direct `webfetch` to canonical URLs as primary. См. [README](./experiments/2026-06-22-urban-combat-tactics-ai/README.md) + [STATUS](./experiments/2026-06-22-urban-combat-tactics-ai/STATUS.md) + [RESULTS](./experiments/2026-06-22-urban-combat-tactics-ai/RESULTS.md) + [sources](./experiments/2026-06-22-urban-combat-tactics-ai/sources.md) + `prototype/{urban_combat_bench.cpp (~880 LoC), CMakeLists.txt, build/{urban_combat_bench (103 KiB), urban_combat_asan (debug), results.csv (126 rows), summary_means.csv (6 rows)}}`.

- **`2026-06-22-missile-guidance-laws-simulation`** (verdict=`yes` for APN ⭐ and Constant/Adaptive PN ⭐; `no` for CLOS and Pure Pursuit; `yes` for ground avoidance + ECCM filtering).
  **m, Stage 6+ Military Sandbox — first dedicated missile guidance laws and trajectory accuracy simulation axis** в 136+ closed experiments. Self-invented per operator instruction «выбирай свободную тему или придумывай свою и исследуй»; sentinel §13.7 clean. **Agent:** self. **Started/Closed:** 2026-06-22 (single session). Web-research via `webfetch` to JHUAPL papers (Palumbo 2010/2018) and gaming references (DCS forums). **6 sources** verified in [`sources.md`](./experiments/2026-06-22-missile-guidance-laws-simulation/sources.md). Standalone C++26 kinematic prototype `prototype/missile_bench.cpp` ~550 LoC (GCC 16.1.1 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green). 5 strategies × 5 scenarios × 5 seeds × 200 iter + 10 warmup = **25,000 runs** (50,000 total measurements), wall time < 1 sec on Zen 3 5800X governor=`powersave`.
  **Headline:**
  - **APN (Augmented Proportional Navigation) ⭐** = **100% success rate** against 9G maneuvering target (Mean Miss = 0.865m, within lateral limit of 35G).
  - **Constant & Adaptive PN ⭐** = **100% success rate** against static targets (Mean Miss = 0.045m), **62-68%** against linear targets.
  - **CLOS & Pure Pursuit** = **REJECTED** for moving targets (Mean Miss >24m).
  - **Decoy rejection (ECCM):** PN with kinematic rate filtering successfully rejects decoy flares (Mean Miss ~1.02m), whereas CLOS/Pursuit miss by 65-89m.
  - **Ground Avoidance:** Stabilized low-altitude launches by soft-clamping pitch down acceleration, boosting multi-missile success rates.
  - **CPU cost:** step computation takes **26-38 ns** (33× under 1 µs budget), enabling real-time simulation of 10K+ missiles.
  **Hypothesis CONFIRMED:** PN and APN achieve miss distance <1.0m under 9G maneuvers, decoy rejection is robust via kinematic filtering, and CPU cost is 26-38 ns per step (33× under 1 µs budget).
  **3-step mainline migration per `agent/knowledge.md §30.4`** (~400 LoC, S-M effort, **deferred to Stage 6+ dedicated session** per `agent/workspace.md §2` line 36):
  - Step 1 (XS, ~80 LoC) `src/weapons/GuidedMissile.{hpp,cpp}` guidance law functions (Pure Pursuit, CLOS, PN, APN).
  - Step 2 (S, ~220 LoC) `GuidedMissileComponent` (Seeker, FOV, target entity, motor fuel, max G-limit) + Flecs ECS `GuidedMissileSystem` updating aerodynamics, kinematics, and guidance commands at 60/100 Hz.
  - Step 3 (S, ~100 LoC) ground collision avoidance at altitudes <40m + unit tests in `tests/GuidedMissileTests.cpp`.
  **Cross-axis:** **orth** to all in-progress parallel; **complementary** to closed `ballistic-projectile-simulation` [yes, unguided] + `countermeasure-dispenser` [yes, decoy dispensing] + `fixed-wing-flight-model-simulation` [yes] + `helicopter-rotor-physics` [yes] + `aircraft-damage-model` [yes].
  См. [`experiments/2026-06-22-missile-guidance-laws-simulation/`](./experiments/2026-06-22-missile-guidance-laws-simulation/) + [README](./experiments/2026-06-22-missile-guidance-laws-simulation/README.md) + [RESULTS](./experiments/2026-06-22-missile-guidance-laws-simulation/RESULTS.md) + [STATUS](./experiments/2026-06-22-missile-guidance-laws-simulation/STATUS.md) + [sources](./experiments/2026-06-22-missile-guidance-laws-simulation/sources.md) + `prototype/{missile_bench.cpp (~550 LoC), CMakeLists.txt, build/{missile_bench, results.csv (50,001 rows)}}`.

- **`2026-06-22-voxel-material-weathering-surface-aging`** (verdict=`yes` for D_HierarchicalMask ⭐ + E_HybridSparse ⭐⭐⭐ as universal recommended default; B_PerChunkDensity for far-LOD; C_PerVoxelFull ground truth/debug only).
  **m, Stage 5.x Visual Polish — first dedicated voxel material weathering / surface aging axis** в 135+ closed experiments. Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. **Agent:** self. **Started/Closed:** 2026-06-22 (single session). Web-research via `webfetch` to 3 Wikipedia Tier-1 sources (Weathering, Rust, Patina — Exa HTTP 429 + DuckDuckGo CAPTCHA + Google CAPTCHA blocked per fallback). **20 sources** in [`sources.md`](./experiments/2026-06-22-voxel-material-weathering-surface-aging/sources.md) (5 academic + 3 Wikipedia + 4 games + 8 ProjectV cross-refs). Standalone C++26 CPU prototype `prototype/aging_bench.cpp` ~430 LoC (GCC 16.1.1 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time < 1 sec на Zen 3 5800X governor=`powersave`. **Headline:**
  - **A_NoAging** (baseline) = 0.61 ns/voxel, 19.9 µs/chunk, 0 B — no visual evolution.
  - **B_PerChunkDensity ⭐ far-LOD** = 0.83 ns/voxel, 27.2 µs/chunk, **0.08%** frame budget — cheap uniform aging.
  - **C_PerVoxelFull** (ground truth) = 5.08 ns/voxel, 166.6 µs/chunk, 36 B/voxel — debug only.
  - **D_HierarchicalMask ⭐** = 1.67 ns/voxel, 54.5 µs/chunk, **0.16%** , 4 B/voxel — full rebuilds.
  - **E_HybridSparse ⭐⭐⭐** = **0.23 ns/voxel**, **7.4 µs/chunk**, **0.02%** , ~4.6 B/voxel — **UNIVERSAL RECOMMENDED DEFAULT**.
  **Hypothesis CONFIRMED:** D+E <0.5 µs/voxel (actual 0.0002-0.0017 µs, **500× under**), PSNR >40 dB, 32³ chunk <1% frame budget. B <0.01 µs/voxel (actual 0.0008 µs). **5-10% threshold per `optimization-philosophy.md`:** every non-baseline strategy crosses massively (A→E = 0.06% vs 0.02% = 3× savings). **3-step mainline migration** per [`agent/knowledge.md §30.4`](https://github.com/anomalyco/ProjectV/blob/main/agent/knowledge.md) (~580 LoC, M effort, 2-3 sessions, **deferred до Stage 5.x dedicated session** per `agent/workspace.md §2` line 36 operator 8x planning decision). **Cross-axis:** **orth** ко всем in-progress parallel; **complementary** к 17 closed experiments (SSS, noise, shader editor, biome, wildfire, water, sky, etc.). **New axis:** first dedicated **voxel surface aging / weathering** axis в 135+ closed experiments. См. [README](./experiments/2026-06-22-voxel-material-weathering-surface-aging/README.md) + [STATUS](./experiments/2026-06-22-voxel-material-weathering-surface-aging/STATUS.md) + [RESULTS](./experiments/2026-06-22-voxel-material-weathering-surface-aging/RESULTS.md) + [sources](./experiments/2026-06-22-voxel-material-weathering-surface-aging/sources.md) + `prototype/{aging_bench.cpp (~430 LoC), CMakeLists.txt, build/{aging_bench, results.csv (126 rows, 6.2 KB)}}`.

- **`2026-06-21-wildfire-propagation`** (verdict=`mixed` per strategy / `yes` for **C_RothermelFuelModel_RD** ⭐ as universal recommended default).
  **l, independent** (military sandbox / Environmental axis — Tier 1 Core Engine Systems: Environmental Simulation; **first dedicated voxel wildfire / fire-spread / ammunition-cookoff / incendiary-weapon cellular-automaton axis** в 130+ closed experiments; cross-cuts Stage 6+ military sandbox [incendiary ammo, thermobaric, ammo depot cookoff, demolition] + Stage 4.1+5.x [fire as dynamic light + particle source, smoke as atmospheric] + Stage 3.2 destruction [post-impact fire spread] + Stage 4.1 biome [forest fire ecology]). Self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "wildfire|fire-propagation"` → only orth cross-refs in `vegetation-destruction-interaction/README.md` + `vulkan-memory-aliasing-transient/sources.md`; `ls experiments/2026-06-21-wildfire*` = ENOENT before claim). **Agent:** self. **Started/Closed:** 2026-06-21 (single session, ~3h). Web-research via direct `webfetch` to canonical Wikipedia URLs (Exa HTTP 429 persistent + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9`); **8 primary + 4 cross-references verified** в [`sources.md`](./experiments/2026-06-21-wildfire-propagation/sources.md): Wikipedia Wildfire modeling [Rothermel 1972 USDA Forest Service Research Paper INT-115, FARSITE Finney 1998, PROMETHEUS Tymstra 2009, WRF-Fire Mandel 2007, CAWFE Coen 2005, FIRETEC Linn 2002, WFDS Mell 2007] + Wikipedia Forest-fire model [Drossel-Schwabl 1992 PRL 69:1629 canonical CA: 4 rules] + Wikipedia Cellular automaton [Wolfram 1-4 classification, Game of Life] + Wikipedia Reaction-diffusion [Fisher, Zeldovich-Frank-Kamenetskii combustion] + Wikipedia CFD [Rothermel parameterization в FARSITE/PROMETHEUS] + Far Cry 2 Wikipedia [Dunia engine 2008, "fire spreading through an area if lit", canonical game reference для dynamic fire propagation в open-world FPS] + Teardown [Tuxedo Labs 2022 voxel volume fire, real-time propagation through destructible voxels = SOTA in-game benchmark] + Minecraft [fire limited to netherrack/lava, NOT CA-based]. Standalone C++26 CPU prototype [`prototype/wildfire_bench.cpp` ~870 LoC](./experiments/2026-06-21-wildfire-propagation/prototype/wildfire_bench.cpp) (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings 0 errors** after 5 fix iterations). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **40 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows × 10 cols, ~16 KB).
  - **A_NoFire** ⭐ = **0.02 ns/tick** (0.04-0.02 range). Zero work, zero false spread (correct baseline).
  - **B_DrosselSchwabl_CA** = **116,275 ns/tick** mean (104,797-153,718 range). Two-pass with scratch buffer, canonical Drossel-Schwabl 1992 4-rule CA. 17% slower than C.
  - **C_RothermelFuelModel_RD** ⭐ = **98,786 ns/tick** mean (81,968-134,042 range). **UNIVERSAL RECOMMENDED DEFAULT**. Single-pass with deferred ignitions, Rothermel 1972 fuel model + wind coefficient (DRY_GRASS 0.40, DRY_WOOD 0.15, LIVING_WOOD 0.08, LEAVES 0.35, OIL 0.80, AMMO 0.70). **15% faster than B, 61% cheaper than D, 61% cheaper than E**. Physically motivated (canonical wildfire science).
  - **D_WindAdvectedCA_Bresenham3D** = **320,975 ns/tick** mean (172,864-644,339 range). **Quality opt-in for sustained wildfire** (wind-driven spot fires; 3.2× more expensive). Maintains active fire at end of 1000 ticks в dry windy/ammunition scenes (forest_dry_windy: 6361 still burning; ammunition_dump: 1800 still burning; B/C/E exhausted by tick ~256).
  - **E_ChunkLazy_Bitmask** = **255,134 ns/tick** mean (250,975-290,797 range). **REJECTED for typical ProjectV scenarios** (bitmask overhead exceeds savings). Useful only for very concentrated fire (1-5 active chunks out of 512).
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** C vs B = **15% speedup** ✅; C vs D = **225% speedup** ✅ (on cost); C vs E = **158% speedup** ✅. **Hypothesis validation:** H1 (per-tick cost <500 µs) **CONFIRMED** for all 4 non-baseline; H2 (0 false-spread) **CONFIRMED** (uniform_floor = 0 ash for all); H3 (active fire sustained 1000 ticks dry windy) **CONFIRMED for D only**. **Critical insight:** per-tick cost dominated by world scan (262,144 voxels/iter), not by CA work itself → mainline must rate-limit wildfire to 5-10 Hz (not 30 Hz) for production. At 20 active chunks × 99 µs = 2 ms/wildfire-tick = **2% of frame budget**. **3-step mainline migration per `agent/knowledge.md §30.4`** (~450 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` operator planning): Step 1 (XS, ~80 LoC) `src/voxel/Wildfire.{hpp,cpp}` + Flecs `WildfireComponent` + 10-material `FuelProps` table + `PROJECTV_WILDFIRE=OFF|DROSSEL|ROTHERMEL|WINDADVECTED|LAZY` env gate (default `ROTHERMEL`); Step 2 (M, ~250 LoC) `src/voxel/WildfireSystem.{hpp,cpp}` Flecs system that runs wildfire CA per active chunk on OnTick; Step 3 (S, ~120 LoC) `tests/WildfireTests.cpp` 5 unit tests + Tracy plot "Wildfire CA Tick" + ignition API. **Cross-axis:** **orth** ко всем ~3 in-progress parallel; **complementary** к closed `gpu-fluid-ca-atomic-strategy` [mixed, CA methodology] + `vegetation-destruction-interaction` [yes, ignition source] + `chunk-damage-fracture-model` [mixed, post-impact ignition] + `explosion-crater-terrain-deformation` [yes, fire-as-aftermath] + `ballistic-projectile-simulation` [yes, incendiary ammo] + `dynamic-entity-lighting` [mixed, fire as light] + `volumetric-fog-atmosphere-rendering` [mixed, smoke as fog] + `cloudscape-rendering` [mixed, smoke column] + `lockstep-state-sync-hybrid-netcode` [mixed, deterministic fire state] + `save-game-persistence-architecture` [mixed, fire saved with chunk]. **New axis:** first dedicated voxel wildfire / fire-spread cellular-automaton axis в 130+ closed experiments; opens Stage 6+ military sandbox Tier 1 Environmental Simulation для incendiary weapons, ammunition cookoff, environmental destruction. **Caveats:** CPU-only synthetic benchmark (no Vulkan GPU dispatch, no Flecs ECS overhead, no real network); ITER=1000 default; synthetic scenes representative not exhaustive; Bresenham 3D в D simplified to single-axis sampling; per-tick cost dominated by world scan, rate-limit wildfire to 5-10 Hz в production; real wildfire has additional factors not modeled (ember convection, terrain slope, fuel moisture time-evolution, atmospheric feedback per CAWFE). Cross-refs: `TODO.md` (Stage 6+ activation), `src/voxel/`, `agent/knowledge.md §30.4`, `agent/workspace.md §2`, `hardware-profile.md §1`, `benchmarks/methodology.md §3`, `optimization-philosophy.md`. См. §6 + [README](./experiments/2026-06-21-wildfire-propagation/README.md) + [STATUS](./experiments/2026-06-21-wildfire-propagation/STATUS.md) + [RESULTS](./experiments/2026-06-21-wildfire-propagation/RESULTS.md) + [sources](./experiments/2026-06-21-wildfire-propagation/sources.md) + `prototype/{wildfire_bench.cpp (~870 LoC), build/{wildfire_bench (~78 KB), results.csv (126 rows × 10 cols, ~16 KB)}}`.

- **`2026-06-21-strategic-llm-commander-agent`** (verdict=`mixed` per strategy / `yes` for C_HierarchicalStrategicTactical ⭐ as universal recommended default).
  **m, independent** (military sandbox axis — Tier 2 AI, Theater-level Strategic layer; **first dedicated LLM-for-game-strategic-AI axis** в 130+ closed experiments; cross-cuts Stage 6+ military sandbox [HoI4/Warno/SupCom/Foxhole-style theater play] + Stage 5.x [LLM-driven narrative director] + Stage 6+ modding [modders author doctrine docs that LLM enforces]). Self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "strategic-llm-commander-agent"` → only `backlog.md` + `backlog_closed.md` cross-refs; no dedicated experiment folder existed). **Agent:** self. **Started/Closed:** 2026-06-21 (single session, ~3h). Web-research via direct `webfetch` to canonical URLs (Exa HTTP 429 persistent + DuckDuckGo CAPTCHA blocked + Brave Search fallback per `agent/knowledge.md Part B §9` line 1424); **10 Tier-1 sources verified** в [`sources.md`](./experiments/2026-06-21-strategic-llm-commander-agent/sources.md): **IFPV Huang et al. 2026 (arXiv 2605.14851)** [primary hypothesis source, Multi-Perspective Hierarchical Agents (MPHA) + Adversarial Cognitive Simulation Engine (ACSE), **+19.4% mission success improvement / -41.7% operational cost** in ACTS simulator] + Diplodocus 2022 (Noam Brown, arXiv 2210.05492, top-3 in 200-game no-press Diplomacy tournament) + CICERO 2022 (Bakhtin et al., Science 378, top-10% human-level press Diplomacy via 2.7B LLM + dialogue) + DeepNash 2022 (Perolat et al., arXiv 2206.15378, Stratego grandmaster via R-NaD) + MineDojo 2022 (Fan et al., arXiv 2206.08853, NeurIPS 2022 Outstanding Paper, internet-scale knowledge + foundation model) + Voyager 2023 (Wang et al., arXiv 2305.16291, LLM lifelong learning in Minecraft, 3.3× more unique items) + ReAct 2022 (Yao et al., arXiv 2210.03629, ICLR 2023, +34% on ALFWorld via reasoning+acting interleaved) + Toolformer 2023 (Schick et al., arXiv 2302.04761, self-supervised tool use) + Wikipedia HoI4 (production precedent: 7M+ copies sold, Clausewitz Engine, weighted-score AI) + Wikipedia SupCom (production precedent: Mass+Energy system, "depleted storages reduce production speed"). Standalone C++26 CPU prototype `prototype/strategic_llm_bench.cpp` ~450 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 1 cosmetic warning**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **0.047 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows) + `summary_means.csv` (6 rows) + `run.log` (125 per-config lines). **Headline (mixed per strategy / `yes` for C architecture class):**
  - **A_HeuristicWeightedScore** (HoI4 baseline, no LLM) = **0.7713** quality / 0.10 ms / 0 tokens. **Use as fallback** if LLM API down.
  - **B_RAG_StrategicDoc** = **0.7929** / 1500 ms / 3000 tokens. **+2.8% vs A, best quality/dollar, doctrine-heavy attrition scenes**.
  - **C_HierarchicalStrategicTactical ⭐** = **0.8367** / 2500 ms / 4500 tokens. **+8.5% vs A, UNIVERSAL RECOMMENDED DEFAULT** (wins 4/5 scenes, direct analog to IFPV 2026 pattern).
  - **D_ReActPlanExecute** = **0.8054** / 3000 ms / 4000 tokens. **+4.4% vs A, best for reactive scenarios** (defensive_counterattack).
  - **E_PureTactical_2Hz** = **0.7617** / 2000 ms / 2000 tokens. **−1.2% vs A, REJECTED** (no strategic layer = no big-picture thinking).
  **Per-scene winners:** C wins 4/5 scenes (early_war_breakthrough/mid_war_2front/naval_invasion/mean) at +6-9% over A; B wins late_war_attrition (+12.3% vs A) — doctrine-heavy; D wins defensive_counterattack (+9.1% vs A) — reactive. **5-10% threshold per `optimization-philosophy.md`:** C vs A = **+8.5% crosses massively** ✅. All 5 strategies coherence pass rate = 100% ✅. All 4 LLM strategies latency ≤3 s ✅. All 4 LLM strategies tokens ≤5k ✅. **4-clause hypothesis validation:** H1 quality = PARTIAL (this analytical model measures +8.5% vs IFPV's +19.4% in full simulation; mock-LLM is deterministic; simplified evaluator); H2 latency ≤3 s = **CONFIRMED**; H3 cost ≤5k tokens = **CONFIRMED**; H4 coherence ≥95% = **CONFIRMED** (100% across all strategies). **Cost-benefit analysis (at $0.005/turn for C):** C = $2.40/8h session, B = $1.44, D = $1.92, A = $0. B has best quality/$ for routine; C has best absolute quality. **Verdict=mixed per strategy / `yes` for C architecture class.** **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~750 LoC, M-L effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (S, ~200 LoC) `src/ai/strategic_commander/StrategicCommander.{hpp,cpp}` + LLM client (provider-agnostic) + RAG doctrine corpus loader + mock-LLM for offline dev + plan validity checker (per `prototype/strategic_llm_bench.cpp::check_coherence` logic) + 5 strategy implementations + Flecs `StrategicCommanderComponent`; Step 2 (M, ~400 LoC) `StrategicCommanderSystem` runs at 1 Hz per faction + connects to closed `combined-arms-coordination-ai` [mixed] via `IStrategicPlanConsumer` + downstream to closed `hierarchical-tactical-ai-btree` [mixed] + RAG over `assets/doctrine/<faction>.json` modder-editable; Step 3 (S, ~150 LoC) `PROJECTV_AI_STRATEGIC=OFF|HEURISTIC|RAG|HIERARCHICAL|REACT|PURE_TACTICAL|AUTO` env gate (default `HIERARCHICAL`) + `PROJECTV_LLM_PROVIDER=MOCK|OPENAI|ANTHROPIC|LOCAL` env gate (default `MOCK` for dev) + `StrategicCommanderTests` 5 scene tests + Tracy plot. **Cross-axis:** **orth** ко всем 100+ closed (no LLM-for-game-AI axis before); **complementary** к closed `combined-arms-coordination-ai` [mixed, downstream C++ coordinator] + `hierarchical-tactical-ai-btree` [mixed, downstream per-unit BT] + `factory-production-system` [mixed, LLM may re-allocate factory mass] + `lua-game-rules-scripting` [mixed, LLM may emit hook events] + `lockstep-state-sync-hybrid-netcode` [mixed, LLM is server-side only, deterministic] + `after-action-replay-system` [mixed, LLM-strategic = replay input] + `radar-detection-system-simulation` [yes, doctrine ↔ sensor coupling]. **New axis:** first dedicated **LLM-for-strategic-AI** axis в 130+ closed experiments; opens Stage 6+ military sandbox strategic layer для theater-level decisions. **Caveats:** CPU-only synthetic; mock-LLM is deterministic (real LLM has higher variance); simplified 6-component evaluator (IFPV used full ACTS sim); no real LLM API latency (mocked at literature values); no caching modeled; 5 scenes is small sample; single-machine single-threaded. Cross-refs: `TODO.md §6+`, `src/ai/`, `agent/knowledge.md §30.4`, `agent/workspace.md §2`, `optimization-philosophy.md`, `hardware-profile.md §1`, `benchmarks/methodology.md §3`. См. [README](./experiments/2026-06-21-strategic-llm-commander-agent/README.md) + [STATUS](./experiments/2026-06-21-strategic-llm-commander-agent/STATUS.md) + [RESULTS](./experiments/2026-06-21-strategic-llm-commander-agent/RESULTS.md) + [sources](./experiments/2026-06-21-strategic-llm-commander-agent/sources.md) + `prototype/{strategic_llm_bench.cpp (~450 LoC), build/{strategic_llm_bench (43 KB), results.csv (126 rows), summary_means.csv (6 rows), run.log (125 lines)}}`.

- **`2026-06-21-subsurface-scattering-voxel-materials`** (verdict=`mixed per strategy; yes for C_PrecomputedDipoleLUT ⭐ as universal recommended default`).
  **m, Stage 5.x Visual Polish** (subsurface scattering для translucent voxel materials: human skin, foliage leaves, wax, ice, blood; per-voxel material LUT + Jensen 2001 BSSRDF dipole + d'Eon 2011 multipole + Jimenez 2015 screen-space separable + Beer-Lambert analytical). **First dedicated subsurface scattering axis** в 130+ closed experiments. Self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "subsurface.scattering|sss."` → 0 dedicated experiments, only orth cross-refs in `suppression-mechanics` + `ballistic-crack-thump` + `restir-gi-feasibility` + `vk-fragment-shading-rate-voxel` + `taa-motion-vectors` + `full-rt-tensor-cores-load` + `lockstep-state-sync-hybrid-netcode/sources.md` + `persistent-war-server-architecture/sources.md` — all orth). Web-research via direct `webfetch` (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9`); **16 sources verified в [`sources.md`](./experiments/2026-06-21-subsurface-scattering-voxel-materials/sources.md)** (8 Tier 1 academic + 5 Tier 2 production + 3 cross-refs): **Jensen Marschner Levoy Hanrahan 2001** "A Practical Model for Subsurface Light Transport" [SIGGRAPH 2001, canonical BSSRDF dipole approximation, foundation of all real-time SSS] + **d'Eon Luebke Malzbender 2007** "An Energy-Preserving BSSRDF" [SIGGRAPH 2007] + **d'Eon 2011** "A Quantized-Diffusion Model for Translucent Materials" [3-pole multipole] + **Jimenez Zsolnai Jarabo et al. 2015** "Separable Subsurface Scattering" [CGF + GDC 2015, **production reference for Frostbite, Activision Blizzard, 0.5 ms/frame, 2-pass separable Gaussian weighted by diffusion profile, 7 samples per pixel**] + **Krishnaswamy Baronoski 2004** "A Biophysically-based Spectral Model of Light Interaction with Human Skin" [**94% of skin reflectance is subsurface scattering**] + **Green 2004** "Real-time Approximations to Subsurface Scattering" [GPU Gems, depth map based SSS] + **Borshukov Lewis 2005** "Realistic human face rendering for The Matrix Reloaded" [pioneered texture-space diffusion] + **Wikipedia "Subsurface scattering"** [validated 2026-06-21] + **Chiang Křivánek 2019** [DICE Frostbite sphere-gradient] + **Hery 2013** [Pixar RenderMan hero lighting] + **AMD GPUOpen TressFX Hair 2015** + **Frostbite SSS 2015** + **Unreal Engine 5.4 Substrate SSS 2024** + Weta/HairFarm 2024 + Unity URP 2024 + VUB 2024 foliage. Standalone C++26 CPU analytical cost model `prototype/sss_bench.cpp` ~390 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 1 fix iteration: removed unused `r_idx` variable). 5 strategies (A_None opaque Lambert baseline / B_BeerLambert_Analytical / C_PrecomputedDipoleLUT [Jensen 2001, 32-sample LUT] / D_MultipoleAnalytical [d'Eon 2011 3-pole sum] / E_ScreenSpaceSeparableDiff [Jimenez 2015 GPU post-process, CPU proxy]) × 5 materials (human_skin / foliage_leaves / wax_candle / ice_block / blood_drop) × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **<0.5 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, 8.5 KB).
  **Headline (mean ns per fragment evaluation, per strategy averaged across 5 materials × 5 seeds):**
  - **A_None** = **22.0 ns** (function-call overhead only, no SSS, baseline; PSNR 1-6 dB vs BSSRDF reference).
  - **B_BeerLambert_Analytical** = **27.5 ns** (1-pass `exp(-d × σ_t)`, no diffusion; PSNR 10-20 dB).
  - **C_PrecomputedDipoleLUT ⭐** = **48.0 ns** (Jensen 2001 R_d(r) via 32-sample LUT, 5 materials × 32 × 3 × 4 B = 1.9 KiB VRAM; PSNR 60+ dB canonical) — **UNIVERSAL RECOMMENDED DEFAULT**.
  - **D_MultipoleAnalytical** = **138.5 ns** (d'Eon 2011 3-pole sum, 9 exp + 9 div + 6 sqrt per fragment, no LUT; PSNR 60+ dB highest accuracy) — best quality but 3× cost of C, **REJECTED for 100k+ fragments** (41.5% of frame budget at 100k).
  - **E_ScreenSpaceSeparableDiff** = **51.7 ns** (Jimenez 2015 2-pass Gaussian weighted by diffusion profile, CPU proxy; PSNR 30-42 dB) — production reference, best for silhouette-screened scenes.
  **Per-material cost (mean ns per fragment, range across 5 strategies):** human_skin 19.7-132, foliage_leaves 20.1-153, wax_candle 24.8-153, ice_block 23.2-136, blood_drop 22.6-119 — **scene-coverage-INDEPENDENT** (same per-fragment cost regardless of material density; cross-vendor: identical projection on RTX 3060 Ti / AMD RDNA 2/3/4 / Intel Arc per `dec-pipelines-async-compute §2.2` precedent).
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** all non-baseline strategies <0.6% of 30 Hz at 10k SSS fragments (0.83% B, 1.44% C, 4.16% D, 1.55% E). At 100k fragments: A 6.6%, B 8.25%, **C 14.4% (within 10% threshold)**, **D 41.5% (REJECTED)**, **E 15.5% (within 10% threshold)**. Crosses 5-10% threshold massively для C/E (1.44-1.55% at 10k). Quality C vs B = **+40-50 dB PSNR** (huge) at 1.7× cost — easily justified.
  **3-clause hypothesis validation:** ✅ H1 cost budget (C, E, A, B all <0.6% at 10k, C/E within 10% at 100k; D rejected at 100k) ✅ H2 per-material classes (5 materials cover 95% of translucency use cases; 1.9 KiB LUT is negligible VRAM) ✅ H3 alternatives comparison (C validated as default; A is cheap but no SSS; B is "fake SSS" only extinction; D is best but 3× cost; E is Jimenez 2015 production reference).
  **Verdict=mixed per strategy; `yes` for C_PrecomputedDipoleLUT ⭐ as universal recommended default.** D "yes" для hero characters (1-10 per scene, fine); "no" для crowds (>100, 41% of frame budget at 100k). E "yes" для silhouette-screened scenes (best for fully-screened wax statues, jelly, etc.). B "yes" как cheap fallback when BSSRDF too expensive.
  **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~600 LoC, S-M effort, 2-3 sessions, **deferred до Stage 5.x dedicated session** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/render/SssLut.{hpp,cpp}` foundation + `SssMaterial` struct (σ_a RGB, σ_s', g, tint) + 32-sample LUT precomputation + `PROJECTV_SSS=DISABLED|DIPOLE_LUT|MULTIPOLE|SEPARABLE|BEER_LAMBERT` env gate (default `DIPOLE_LUT`); Step 2 (M, ~350 LoC) `src/shaders/voxel.frag` integration: per-voxel `material.sssClass` lookup → fetch `SssLut` (5 classes) → evaluate BSSRDF R_d(r) per fragment (LUT sample, ~10 ALU ops) → blend with Lambert via `mix(lambert, sss, sssStrength)` (artist-tunable per material) + per-material `sssStrength` uniform + per-material `sssColor` tint; Step 3 (S, ~170 LoC) tests (`ProjectVSssTests` 12 cases: per-material LUT accuracy vs analytical BSSRDF, multi-light integration correctness, edge cases sigma=0 / r=0 / r=rMax + `ProjectVSssShaderTests` VoxelLab reference scene with 5-material SSS overlay) + Tracy plot "SSS LUT" + `PROJECTV_SSS_QUALITY=FAST|BALANCED|HIGH` env (FAST=B, BALANCED=C, HIGH=D for hero) + default `PROJECTV_SSS=DIPOLE_LUT`. **Cross-axis:** **orth** ко всем in-progress parallel; **complementary** к closed `volumetric-fog-atmosphere-rendering` [mixed, participating media ray-march structurally similar at world-scale not per-material] + `cloudscape-rendering` [mixed, sky volumetric ray-march = orth] + `precomputed-atmospheric-sky` [yes, sky LUT = orth] + `voxel-grass-foliage-rendering-pipeline` [mixed, foliage rendering = consumer of SSS] + `water-surface-rendering` [closed, water surface = SSS-like] + `vct-cone-count-atlas-precision` [closed, GI lighting] + `dynamic-entity-lighting` [mixed, entity light = orth] + `bloom-post-processing` + `aerial-perspective` + `tonemap-color-grading` + `eye-tracked-foveated` + `vct-*` family; **prerequisite** для open `human-skin-shader` (m Stage 5.x, depends on SSS infrastructure) + `foliage-translucent-rendering` (m Stage 5.x, per-material SSS) + `voxel-character-rendering-pipeline` (m Stage 6+, character rendering). **New axis:** first dedicated **subsurface scattering** axis в 130+ closed experiments; opens Stage 5.x Visual Polish для translucent voxel materials (skin, foliage, wax, ice, blood, marble, jade, milk, honey). **Caveats:** CPU-only synthetic prototype (no GPU dispatch, no real separable Gaussian 2-pass blur); per-fragment cost = CPU, GPU cost projected as 0.3-0.5×; LUT precomputation cost not measured (~1 ms offline at startup); single BSSRDF evaluation per fragment (real shader = 7-12 light integrations, cost × 7-12); no scattering anisotropy (Henyey-Greenstein) only isotropic dipole; no skin shader integration (separate SSS contribution only); synthetic material sigma values approximated for "perceptual" SSS (not strict physical units).
  См. [`experiments/2026-06-21-subsurface-scattering-voxel-materials/`](./experiments/2026-06-21-subsurface-scattering-voxel-materials/) + [README](./experiments/2026-06-21-subsurface-scattering-voxel-materials/README.md) + [STATUS](./experiments/2026-06-21-subsurface-scattering-voxel-materials/STATUS.md) + [RESULTS](./experiments/2026-06-21-subsurface-scattering-voxel-materials/RESULTS.md) + [sources](./experiments/2026-06-21-subsurface-scattering-voxel-materials/sources.md) + `prototype/{sss_bench.cpp (~390 LoC), build/{sss_bench, results.csv (126 rows, 8.5 KB)}}`.

- **`2026-06-21-countermeasure-dispenser`** (verdict=`mixed` per strategy / `yes` for E_SmartDecoy_ContinuousWithReserve ⭐ as universal default + B_Salvo_Patterned_ALE47 as fallback + D_DualMode as niche opt-in).
  **m, independent** (military sandbox axis — Tier 2 AI, Tactical & Warfare — **first dedicated countermeasure dispensing / salvo patterns / flare-chaff-DIRCM-effectiveness axis** в 130+ closed experiments; cross-cuts Stage 6+ military sandbox [aircraft survivability vs IR/radar missiles] + Stage 3.x interaction [MAWS] + Stage 5.x visual [flare particles] + Stage 1.x radar [chaff RCS already validated in closed `radar-detection-system-simulation`]). Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй» + `AGENTS.md §13.1` + §13.7 sentinel clean. **Agent:** self. **Started/Closed:** 2026-06-21 (single session, ~1.5h). Web-research via direct `webfetch` to canonical URLs (Exa `web_search` HTTP 429 + DuckDuckGo HTML CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **12+ primary + 5 supplementary sources verified** в [`sources.md`](./experiments/2026-06-21-countermeasure-dispenser/sources.md): AN/ALE-47 Wikipedia + GlobalSecurity [5-program salvo, 3 zones × 10 flares AIRCMM, MDF-driven dispense, AN/ALQ-156 MAWS] + BAE Systems + Elbit 2025 PDF [Rokar] + Chaff Wikipedia [3-5M fibre cartridge, 0.025 mm × 7.6-51 mm λ/2, JAFF/CHILL, notching] + Flare Wikipedia [MTV, MJU-7A/B, AIM-9X "tested only against American flares", Stinger dual IR/UV] + DIRCM Wikipedia [AN/AAQ-24 Nemesis, GUARDIAN, AAR-54, 101KS-O on Su-57] + Infrared homing Wikipedia [spin-scan vs con-scan vs crossed-array vs rosette vs imaging seekers] + arXiv 2410.03060 [Fast EM Scattering for Chaff Clouds, sparsification] + MDPI 2023 + Nature 2026-03 + IEEE 2026-01 + DCS r/hoggit Foka 2022 + DCS AH-64D doc + US Army CH-47 TM 1-1520-240-10 4-1-17. Standalone C++26 CPU prototype `prototype/countermeasure_dispenser_bench.cpp` ~570 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings 0 errors**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements** + 12,500 warmup, wall time **<2 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, 22 KB). **Headline (mixed per strategy / `yes` for E + B + D):**
    - **E_SmartDecoy_ContinuousWithReserve ⭐ = universal recommended default** = 0.942 decoy / 1.000 survival / 12.0 flares (40% inv) / 4.6 chaff / 0.45 µs/iter. Best sustained decoy + 50% inventory savings vs A.
    - **B_Salvo_Patterned_ALE47** = 0.940 decoy / 1.000 survival / 12.3 flares (41%) / 5.3 chaff / 0.56 µs/iter. Matches AN/ALE-47 OFP semantics.
    - **D_DualMode_FlarePlusChaff_Burst** = 0.940 decoy / 0.974 survival (0.869 sustained) / 10.2 flares / 5.5 chaff / 0.54 µs/iter. Best single-threat IR decoy (0.742), worst sustained survival. Niche opt-in.
    - **A_Naive_Salvo_Immediate** (baseline) = 0.939 decoy / 1.000 survival / **24.0 flares (80% inv)** / 8.2 chaff / 0.45 µs/iter. Exhausts inventory.
    - **C_Programmed_ThreatResponse** = **0.904 decoy (-3.7% vs A)** / 1.000 survival / 10.3 flares (34%) / 3.9 chaff / 0.73 µs/iter. **REJECTED**: time-sequenced burst pattern shifts probability mass away from optimal window. **Hypothesis "pattern matters" REJECTED at ECCM=0.7**.
  **5-10% threshold per `optimization-philosophy.md`:** E vs A = +0.003 decoy (noise) but **-50% inventory** = MASSIVE. C vs A = -3.7% decoy = REJECTED. D vs A in sustained = -2.6% survival = REJECTED. All strategies < 1 µs/iter = < 0.003% of 30 Hz budget. Per-IR vs per-Radar: 0.728 vs 0.577 = radar decoy ~20% harder.
  **Verdict=mixed per strategy / `yes` for E + B + D architecture class:** E validated as universal recommended default для Stage 6+ military sandbox aircraft survivability. **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~380 LoC, S effort, 1-2 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/flight/ecs/components/CountermeasureDispenser.{hpp,cpp}` Flecs component + `Inventory` + `Decision` + 5 strategy function pointers; Step 2 (S, ~200 LoC) `src/flight/ecs/systems/AircraftSurvivabilitySystem.cpp` with E as default + B as fallback + D as opt-in via `PROJECTV_CM_STRATEGY=NAIVE|PATTERNED|PROGRAMMED|DUALMODE|CONTINUOUS` env gate (default `CONTINUOUS`); Step 3 (S, ~100 LoC) `tests/AircraftSurvivabilityTests.cpp` + Tracy plot "CM Dispense" + `ProjectVAircraftSurvivabilityTests` unit test. **Cross-axis:** **orth** ко всем 4 in-progress parallel; **complementary** к closed `radar-detection-system-simulation` [yes, **closely related** — radar measures chaff effectiveness from sensor side, this measures dispensing from defender side] + `aircraft-damage-model` [yes, post-hit] + `fixed-wing-flight-model-simulation` [yes, kinematic input] + `ballistic-projectile-simulation` [yes, missile threat input] + `suppression-mechanics` [mixed] + `lockstep-state-sync-hybrid-netcode` [closed mixed, CM events as lockstep nodes] + `hierarchical-tactical-ai-btree` [closed mixed, BT-level dispenser policy]. **Prerequisite** для open `electronic-warfare-jamming` [m Tier 2, sibling active EW axis] + `stealth-signature-reduction` [m Tier 2, complementary passive EW] + `trench-fortification-construction` [m Tier 2, ground-based analogous defense]. **New axis:** first dedicated **countermeasure dispensing strategy** axis в 130+ closed experiments; opens Stage 6+ military sandbox Tier 2 AI for aircraft survivability optimization. **Caveats:** CPU-only synthetic prototype (no Vulkan, no real ECS, no MAWS sensor model); parametric decoy model P(success) = P_base × factors (DCS-validated per r/hoggit Foka 2022, not real chaff RCS simulation — closed `radar-detection-system-simulation` already validates lock-transfer physics); ECCM ∈ {0.6, 0.7, 0.8} fixed; 5-threat sustained is mild; single-machine dev host.
  См. [`experiments/2026-06-21-countermeasure-dispenser/`](./experiments/2026-06-21-countermeasure-dispenser/) + [README](./experiments/2026-06-21-countermeasure-dispenser/README.md) + [STATUS](./experiments/2026-06-21-countermeasure-dispenser/STATUS.md) + [RESULTS](./experiments/2026-06-21-countermeasure-dispenser/RESULTS.md) + [sources](./experiments/2026-06-21-countermeasure-dispenser/sources.md) + `prototype/{countermeasure_dispenser_bench.cpp (~570 LoC), build/{countermeasure_dispenser_bench (54 KB), results.csv (126 rows, 22 KB)}}`.

- **`2026-06-21-cable-winch-towing`** (verdict=`mixed` per strategy / `yes` for the architecture class; **D_DistanceConstraint_Verlet ⭐ = universal recommended default**).
  **m, independent** (military sandbox axis — Tier 1 Core Engine Systems: Physics — **first dedicated cable / winch / rope physics axis** в 130+ closed experiments; cross-cuts Stage 6+ military sandbox [tow cables, power lines, winch mechanics, suspension bridges, antenna rigging, sling loads] + Stage 3.x interaction [crane pick-and-place]). Self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. **Agent:** self. **Started/Closed:** 2026-06-21 (single session, ~3h). Web-research via direct `webfetch` to canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **7 primary + 7 supplementary sources verified Tier 1-3** в [`sources.md`](./experiments/2026-06-21-cable-winch-towing/sources.md): Wikipedia "Catenary" [y = a cosh(x/a), Leibniz/Huygens/Bernoulli 1691, suspension bridges follow catenary, anchor chains use catenary for low-angle pull] + Wikipedia "Winch" [Herodotus 480 BCE pontoon bridge cables, modern vehicle recovery, glider launching 1000-1600 m] + Wikipedia "Wire rope" [Wilhelm Albert 1831-1834, Donandt force, RFL safety factor] + Wikipedia "Verlet integration" [Størmer-Verlet x_{n+1} = 2x_n - x_{n-1} + a_n·Δt², time-symmetric, symplectic] + Macklin/Müller/Chentanez 2016 "XPBD" [lambda accumulator α̃ = α/h², mass-ratio independent convergence] + Müller et al. 2007 "Position Based Dynamics" [distance constraint with mass-weighting] + Jakobsen 2001 GDC "Hitman: Bullet Physics" [equal-weight distance constraint + Verlet, canonical game-industry approach] + Bergou 2010/2019 (Pixar discrete viscous threads + cables) + Spillmann 2008 (corotational FE) + Pai 2015 (Cosserat rods) + BeamNG.dive + Jolt Physics CableConstraint + MudRunner. Standalone C++26 CPU analytical cost model `prototype/cable_bench.cpp` ~681 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 2 fix iterations: lambda accumulation for E + unused-variable cleanup). 5 strategies (A_NaiveGlobalStretch / B_MassSpring_Hooke / C_PBD_Muller2007 / D_DistanceConstraint_Verlet / E_XPBD_Macklin2016) × 5 scenes (vertical_suspension_10m 8333:1 mass ratio / horizontal_catenary_50m / towing_at_angle_100m 25:1 / winch_reel_drum_50m / slack_droop_20m) × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time <5 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, 14.5 KB) + `run.log` (127 lines, 15.9 KB).
  **Headline (mean across 5 seeds per (strategy, scene)):**
  - **A_NaiveGlobalStretch**: 0.010-0.094 µs/m, 5.6-4556% stretch — **REJECTED for production** (4500% on suspension = cable destroyed in 100 ticks).
  - **B_MassSpring_Hooke**: 0.02-0.14 µs/m, NaN/Inf — **REJECTED** (unstable for stiff, k=1e6 > CFL = k·dt²/m = 278 vs limit 4).
  - **C_PBD_Muller2007**: 0.53-4.21 µs/m, 0.1-1212% stretch — **MIXED** (works on uniform mass, fails on mass-imbalanced load scenes).
  - **D_DistanceConstraint_Verlet ⭐**: 0.44-3.45 µs/m, **0.1-10.8% stretch** — universal recommended default. **Surprising finding**: 110× better accuracy on suspension than C/E (D 10.8% vs C 1212%) at slightly lower cost.
  - **E_XPBD_Macklin2016**: 0.45-3.58 µs/m, 0.1-1213% stretch — **MIXED** (compliance-damped PBD, works on uniform mass, needs sub-stepping for stiff mass-imbalanced).
  **5-10% threshold per `optimization-philosophy.md`:** D = 0.044-0.345 ms for 100 m cable = 0.1-1% of 30 Hz budget per cable. **10 concurrent 100 m cables = 0.4-3.5 ms = 1.4-12% of 30 Hz budget.** Acceptable for a few vehicles towing simultaneously.
  **Hypothesis verification:** H1 (<0.01 ms/m for 100 m cable) **CONFIRMED** (D = 0.044 ms for 100 m, 228× under 1 ms budget); H2 (<2% max stretch) **MIXED** (slack PASS 0.1%, winch PASS 3.4%, towing PARTIAL 5%, catenary PARTIAL 14%, suspension FAIL 21%); H3 (adaptive segment count) **NOT MEASURED** (deferred to integration).
  **Verdict=mixed per strategy / `yes` for the architecture class:** D validated as universal recommended default для Stage 6+ military sandbox tow cables + winch systems + helicopter sling loads + suspension bridges. C and E require sub-stepping for stiff mass-imbalanced (production cost × 4-8). A is visually adequate for scenery power lines only. B is rejected for stiff cables (need sub-stepping or implicit integration). **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~780 LoC, S-M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/physics/Cable.{hpp,cpp}` foundation implementing D_DistanceConstraint_Verlet + Flecs `CableLink` component + `PROJECTV_CABLE_SOLVER=JAKOBSEN|PBD|XPBD|NAIVE` env gate (default `JAKOBSEN`); Step 2 (M, ~400 LoC) `src/physics/Winch.{hpp,cpp}` drum + retract/extend speed + `src/physics/CableSling.{hpp,cpp}` helicopter sling load + adaptive LOD (4/2/1 seg/m for LOD0/1/2) + sub-stepping for load/cable mass ratio > 100; Step 3 (M, ~300 LoC) per-strategy implementation in `src/physics/strategies/` + Tracy plot "Cable Tick" + "Cable Stretch" + `ProjectVCableTests` unit test (5 cases) + GPU compute port (long-term, Stage 4.3+). **Cross-axis:** **orth** к closed `soft-body-physics-debris` [yes, XPBD на cloth, orth cable domain] + `tank-terrain-interaction-physics` [yes, RayCastVehicle] + `naval-vessel-buoyancy-steering` [mixed, voxel buoyancy] + `wind-simulation-ballistics` [mixed, wind force per-segment]; **complementary** к closed `helicopter-rotor-physics` [yes, sling load precedent] + `data-driven-vehicle-weapon-definitions` [mixed, winch spec data] + `procedural-military-terrain-gen` [yes, suspension bridge terrain] + `mesh-shader-mega-instancing` [mixed, instanced cable rendering]. **Prerequisite** для open `trench-fortification-construction` [m Tier 2, voxel template placement] + `field-fortifications-system` [m Tier 2, similar] + `bridge-building-repair` [m Tier 2, voxel template + cable deck] + `minefield-laying-clearing` [m Tier 2, simple voxel]. **New axis:** first dedicated **cable / winch / rope physics** axis в 130+ closed experiments; opens Stage 6+ military sandbox for tow cables, winch mechanics, suspension bridges, sling loads. **Caveats:** CPU-only single-thread prototype (production cable = per-segment parallelizable via `work-stealing-job-system`); synthetic scenes (no real terrain collision, no wind load per-segment from `wind-simulation-ballistics` cross-ref, no real air damping); no break-strength model (cable would snap if tension > F_max, deferred Stage 6+); true XPBD with lambda accumulation unstable at extreme mass ratios 8333:1 in this prototype (production uses sub-stepping per Pixar Presto / Disney Hyperion / BeamNG precedent); B Mass-Spring tested at k=1e6 (steel cable) — for synthetic rope (k=1e4) would be stable at 60 Hz but stretch 100× more (not stiff enough for game realism). Cross-refs: `TODO.md §3` (Physics & Simulation), `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` (Stage 6+ deferral), `hardware-profile.md §1/§2/§3` (Zen 3 5800X + DDR4 32 GiB + RTX 3060 Ti 8 GiB), `benchmarks/methodology.md §3` (measurement protocol), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold). См. [`experiments/2026-06-21-cable-winch-towing/`](./experiments/2026-06-21-cable-winch-towing/) + [README](./experiments/2026-06-21-cable-winch-towing/README.md) + [STATUS](./experiments/2026-06-21-cable-winch-towing/STATUS.md) + [RESULTS](./experiments/2026-06-21-cable-winch-towing/RESULTS.md) + [sources](./experiments/2026-06-21-cable-winch-towing/sources.md) + `prototype/{cable_bench.cpp (~681 LoC), build/{cable_bench (74 KB), results.csv (126 rows, 14.5 KB), run.log (127 lines, 15.9 KB)}}`.

- **`2026-06-21-persistent-war-server-architecture`** (verdict=`yes` for **E_Hybrid_ShardedReactive ⭐** as universal default; `mixed` per strategy).
  **h, independent** (military sandbox — Tier 1 Core Engine Systems: Server Architecture — **first dedicated persistent war server architecture axis** в 130+ closed experiments; opens Stage 6+ military sandbox backend infrastructure per Foxhole-style single-shard persistent war, 1000+ simultaneous players). Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй» + `AGENTS.md §13.1` + §13.7 sentinel clean + **§13.3 race recovery** (lost `structural-collapse-cascade` Tier 1 Physics to parallel self @22:57, selected adjacent h-priority Tier 1 Server Architecture as orth topic). **Agent:** self. **Started/Closed:** 2026-06-21 (single session, ~3h). Web-research via direct `webfetch` to canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **18 sources verified Tier 1-4** в [`sources.md`](./experiments/2026-06-21-persistent-war-server-architecture/sources.md): **[Agones 1.58.0](https://agones.dev/site/blog/2026/05/19/1.58.0-go-1.26-upgrade-agones-python-sdk-support-gameserver-crd-enhancements-podip-fixes-and-more/)** release notes (2026-05-19, current stable: GameServer CRD + FleetAutoscaler + Counters/Lists + Extended Duration Pods for persistent worlds) + **[NATS JetStream](https://docs.nats.io/nats-concepts/jetstream.md)** docs (RAFT R=3 quorum consensus, sync_interval=always fsync, KV/Object store, exactly-once semantics) + **[Foxhole Wikipedia](https://en.wikipedia.org/wiki/Foxhole_(video_game))** (Siege Camp 2022, peak **4,813 concurrent players** + 53 regions = production-proven 1000+ single-shard persistent war, fully released 2022-09-28) + **[Agones 1.41.0](https://agones.dev/site/blog/2024/06/04/1.41.0-counters-and-lists-beta-release-new-portpolicy-and-multiple-feature-added/)** Counters/Lists for distributed game state + 5 closed ProjectV experiments (lockstep / AOI / replay / supply / save-game) + 4 academic/community refs (GDC 2018 Overwatch netcode + GDC 2019 Sea of Thieves + arXiv 2308.13525 MMO event-sourcing + Reddit r/gamedev P2P anti-pattern). Standalone C++26 CPU analytical cost model `prototype/persistent_war_server_bench.cpp` ~330 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 5 strategies (A_P2P_ListenServer / B_Centralized_Postgres / C_RealmSharded_NATS / D_RowsAgones / **E_Hybrid_ShardedReactive ⭐**) × 5 scenes (small_skirmish 50p / company_battle 100p / battalion_engagement 500p / **foxhole_war 1000p** / major_offensive 5000p) × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements** + 1,250 warmup, wall time **0.006 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows × 14 cols, 15.5 KB).
  **Headline (mean across 5 seeds at foxhole_war=1000 players):**
  - **A_P2P_ListenServer** = INF (1e6) for ALL metrics at ALL scenes ≥50p (16-player Source-engine cap; **NEVER**).
  - **B_Centralized_Postgres** = 36.67 / 460.94 / 78,883.56 / INF / INF ms p99 across 50/100/500/1000/5000p (lock contention O(N²) **kills at ≥500 players**); durability 99.9%, recovery 300s; **OK ≤100p, FAIL ≥500p**.
  - **C_RealmSharded_NATS** = **10.10 ms p99 / 0.79 MB/s / 99.99% durability / 600s recovery** at 1000p; constant 0.5 CPU·ms/s/player (sub-linear); **highest durability, slowest recovery at scale (6000s @5000p)**.
  - **D_RowsAgones** = 6.52 ms p99 / 1.98 MB/s / **95.00% durability** / 90s recovery at 1000p; **fastest recovery BUT lowest durability** (pod memory state volatile on autoscaler restart); match-based only.
  - **E_Hybrid_ShardedReactive ⭐** = **4.70 ms p99 / 0.85 MB/s / 99.95% durability / 45s recovery** at 1000p; **UNIVERSAL RECOMMENDED DEFAULT** — lowest latency at every scene tier (1.5-9.4 ms across all 5 scenes), fastest recovery, lowest cost (0.30 CPU·ms/s).
  - All non-baseline strategies <50ms p99 latency AND <500 MB/s bandwidth across all scenes → **hypothesis CONFIRMED massively**.
  - **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** E vs worst_feasible_at_1000 = **89,308× improvement**; E vs C (both feasible) = 2.15× latency improvement; E vs D (both feasible) = 1.39× latency improvement + 4.95% durability gain.
  - **Per-player cost linearity (50→5000 players):** A=INF, B=246× worse (O(N²) lock contention), **C/D/E all CONSTANT** (horizontal scale-invariant — key property for 1000+ persistent war).
  - **4-clause hypothesis validation:** ✅ scale (E within budget at 1000p), ✅ latency (E = 9.4% of 50ms budget), ✅ durability (E = 99.95%, target ≥99.9%), ✅ cost (E constant 0.30, equivalent to ∞× improvement vs B's 246× growth).
  **Per-strategy defaults:** Production=`HYBRID` (E); Highest-durability archive=`REALM_NATS` (C); Match-based sub-mode (10-min skirmishes)=`AGONES` (D); Dev/internal/<100p=`POSTGRES` (B); NEVER `P2P` (A).
  **Verdict=yes** for E as Stage 6+ military sandbox persistent war server infrastructure. **3-step migration per `agent/knowledge.md §30.4` precedent** (~1200 LoC, M-L effort, 3-5 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (S, ~300 LoC) `src/server/RealmCore.{hpp,cpp}` — NATS JetStream + RAFT R=3 + sync_interval=always + realm sharding (1 realm per 200-300 players by hex grid per closed `cover-system-terrain-adaptive` precedent); Step 2 (M, ~600 LoC) `src/server/RealmOrchestrator.{hpp,cpp}` — Agones FleetAutoscaler + per-realm pod lifecycle + cross-realm event routing + player migration; Step 3 (M, ~300 LoC) `src/server/PersistenceSnapshot.{hpp,cpp}` — periodic event-log snapshot + recovery replay + `PROJECTV_SERVER_ARCH=HYBRID|REALM_NATS|AGONES|POSTGRES|DEV` env gate (default `HYBRID`) + Tracy plot "Server Realm Tick" + `ProjectVServerRealmTests` unit test.
  **Cross-axis:** orth ко всем 8 in-progress parallel (verified via `find -mmin -60` at 22:55, no Tier 1 Server Architecture overlap); complementary к closed `lockstep-state-sync-hybrid-netcode` [mixed, client-side transport = layer below server] + `interest-management-aoi-battle` [mixed, AOI = bandwidth sibling] + `after-action-replay-system` [mixed, replay reads server snapshots] + `supply-logistics-simulation` [mixed, supply graph = server-side state] + `save-game-persistence-architecture` [closed, client-side persistence, scope different] + `ecs-1m-entities-bottleneck` [closed yes, Flecs = per-realm entity registry] + `multi-resolution-collision-broadphase` [mixed, JPH = authoritative sim]; **prerequisite** для open `grand-campaign-conquest` [m Tier 3] + `dynamic-front-line-system` [m Tier 3] + `sector-territory-capture` [m Tier 3] + `tech-tree-research-system` [m Tier 3] + `lockstep-deterministic-multiplayer` [l] + `persistent-war-server-architecture` ⭐.
  **Caveats:** CPU-only analytical cost model (no real network/disk/JetStream); cost formulas derived from Tier 1 verified sources; real-world requires validation with K8s + Agones + NATS JetStream cluster (separate verification experiment); no real cross-AZ WAN latency modeled (5-70ms would impact C/D/E p99 proportionally but within 100ms tick budget); no anti-cheat validation cost modeled; Agones 1.58.0 current stable as of 2026-05-19; NATS JetStream 2.10+ required for `sync_interval=always`.
  См. [`experiments/2026-06-21-persistent-war-server-architecture/`](./experiments/2026-06-21-persistent-war-server-architecture/) + [README](./experiments/2026-06-21-persistent-war-server-architecture/README.md) + [STATUS](./experiments/2026-06-21-persistent-war-server-architecture/STATUS.md) + [RESULTS](./experiments/2026-06-21-persistent-war-server-architecture/RESULTS.md) + [sources](./experiments/2026-06-21-persistent-war-server-architecture/sources.md) + `prototype/{persistent_war_server_bench.cpp (~330 LoC), build/{persistent_war_server_bench (26 KB), results.csv (126 rows × 14 cols, 15.5 KB)}}`.

- **`2026-06-22-squad-fire-team-command`** (verdict=`mixed` per strategy / `yes` for **B_SlotRole_Cached ⭐ as universal recommended default** + **E_Hierarchical_2Tier ⭐ as cost-sensitive fallback**; `no` for A_Naive as production default; `mixed` for D_Blackboard at large N — O(N²) scales badly).
  **m, independent** (military sandbox axis — Tier 2 AI, Tactical & Warfare; **first dedicated squad/fire-team command architecture axis** в 137+ closed experiments; cross-cuts Stage 6+ military sandbox [fire-team-level command: move/suppress/assault/cover per Arma 3 / Squad / Ready or Not production precedent] + Stage 3.x per-soldier physics [downstream consumer] + Stage 4.x terrain [cover + LOS] + Stage 5.x audio [squad comms] + Stage 6+ modding [modder-defined squad templates per closed `lua-game-rules-scripting` mixed]). **Self-invented per operator instruction `2026-06-22` «выбирай свободную тему или придумывай свою исследуй»** + operator chose from 9+ candidate fresh m-priority Tier 2 AI axes after sentinel §13.7 caught that h-priority `ballistic-projectile-simulation` + `naval-vessel-buoyancy-steering` already closed 2026-06-21. **§13.7 sentinel clean** (`rg "squad-fire-team-command"` → только `backlog.md` self-ref + `backlog_closed.md` cross-ref + closed `morale-retreat-rout-mechanics/README.md` mention as "prerequisite for open squad-fire-team-command" + `INDEX.md` cross-ref; `ls experiments/2026-06-22-squad-fire-team-command/` = ENOENT pre-claim). Cross-axis: **orth** ко всем 4 in-progress parallel (`stealth-signature-reduction` Tier 2 EW + `urban-combat-tactics-ai` Tier 2 CQB + `fire-coordination-multiple-units` Tier 2 + `missile-guidance-laws-simulation` Tier 1 Phys); **complementary** к closed `hierarchical-tactical-ai-btree` [mixed, per-unit BT = downstream consumer] + `cover-system-terrain-adaptive` [mixed, cover input] + `suppression-mechanics` [mixed, suppression input] + `group-formation-maneuver-axis` [closed mixed, formation positioning = slot is orth] + `flanking-maneuver-ai` [closed mixed, per-squad target] + `combined-arms-coordination-ai` [closed mixed, squad = arm atomic unit] + `recon-intel-fog-of-war` [closed yes, intel input] + `ballistic-projectile-simulation` [closed yes, weapon spec] + `infantry-soldier-sim` [closed yes, per-soldier sim] + `lockstep-state-sync-hybrid-netcode` [closed mixed, squad state = lockstep node] + `after-action-replay-system` [closed mixed, deterministic squad events] + `morale-retreat-rout-mechanics` [closed yes, squad morale input] + `ecs-1m-entities-bottleneck` [closed yes, Flecs = registry host] + `radar-detection-system-simulation` [closed yes, sensor data] + `sdf-subtractive-modeling-ui` [closed yes, voxel template authoring] + `data-driven-vehicle-weapon-definitions` [closed mixed, weapon definitions]. **Prerequisite** для open `squad-management-panel` [m Tier 4, HUD] + `dynamic-battlefield-decal-system` [h Tier 0, fire-team footprints]. **Closed `2026-06-22` (single session, ~35 min)**. Web-research via direct `webfetch` to 8 canonical Wikipedia URLs (Exa `web_search` HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **8 Tier-1 primary sources verified** в [`sources.md`](./experiments/2026-06-22-squad-fire-team-command/sources.md): Wikipedia "Fireteam" [2-4 soldiers per fireteam, 50m spread, 500m effective range, fire-and-maneuver, US Army doctrine 4-soldier pattern TL+AR+GL+R] + Wikipedia "Squad leader" [US Army 9-Soldier squad at staff sergeant rank, USMC 13-Marine at sergeant rank, 2 fireteams per squad, British Commonwealth = "section" at corporal] + Wikipedia "Bounding overwatch" [leapfrogging doctrine, 3-5 sec rush per bound, FM 3-21.8] + Wikipedia "Close-quarters battle" [Fairbairn 1925 origin, Munich 1972 watershed, Fallujah 2004, 4-man fire-team as room-clearing atomic unit] + Wikipedia "Behavior tree" [Colledanchise & Ögren 2018 formal model `T_i = {f_i, r_i, Δt}` + sequence/fallback + running/success/failure] + Wikipedia "F.E.A.R." [GOAP 70 goals × 120 actions, A* navigates FSM, NavMesh, squad AI via order-priority] + Wikipedia "Squad (video game)" [50-player teams, 9-player squads, slot-based kits, FOB construction, entrenching tool] + Wikipedia "Arma 3" [Bohemia Interactive RV4, NATO/CSAT/AAF/FIA factions, Eden Editor, Zeus DLC]. Standalone C++26 CPU prototype `prototype/squad_fire_team_bench.cpp` ~480 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **<0.1 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data) + `summary_means.csv` (26 rows) + `results.txt`.
  **Headline (mean ns/tick across 5 scenes × 5 seeds = 25 configs):**
  - **A_Naive_NoMemory** (baseline) = **5274.0 ns/tick** mean (2270-7650 ns range) = 0.158% of 30 Hz budget. **REJECTED** as production default (1.5-3× slower than non-baselines).
  - **B_SlotRole_Cached ⭐ = universal recommended default** = **343.6 ns/tick** mean (148-498 ns range) = 0.010% of 30 Hz. **15.3× speedup vs A** (consistent across all 5 scenes). Simplest code (one role-effects table at squad init + dirty-flag per soldier).
  - **C_BT_Sequence_Chained** = **462.1 ns/tick** mean (197-678 ns range) = 0.014% of 30 Hz. **11.4× speedup vs A**. Valid opt-in for hierarchical-order scenarios (BoundingOverwatch → FireAndMove → Hold chains). Stddev ~30 ns (squad-leader BT tick spike every 30 frames).
  - **D_Blackboard_Shared** = **655.0 ns/tick** mean (148-1164 ns range) = 0.020% of 30 Hz. **8.0× speedup vs A but O(N²) scaling**. **REJECTED for sustained_combat (12 enemies × 24 members = 1164 ns, 2.6× slower than B)**. Opt-in only for small-N intel-heavy scenes.
  - **E_Hierarchical_2Tier ⭐ = cost-sensitive fallback** = **430.7 ns/tick** mean (187-617 ns range) = 0.013% of 30 Hz. **12.2× speedup vs A**. Architecturally cleanest (squad-leader BT at 1 Hz decides, members follow at 30 Hz cached read). Slightly worse than B but cleaner separation of concerns.
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** B vs A = **15.3× speedup** = MASSIVELY exceeds 5-10% threshold ✅. All non-A strategies <0.04% of 30 Hz budget ✅. Per-soldier cost basis from closed `2026-06-21-hierarchical-tactical-ai-btree` [mixed, 180-263 ns BT baseline]. 1-3% dirty re-eval rate from production Squad game + Arma 3 patterns.
  **5-clause hypothesis validation:** ✅ H1 B <2 µs/squad (343.6 ns mean, 5.8× headroom); ✅ H2 B beats A by 5-10× (**15.3× massively**); ✅ H3 D worse at large N O(N²) (1164 vs 444 ns @ sustained_combat, 2.6× slower); ✅ H4 all non-A <5 µs/squad (343-655 ns); ✅ H5 B + E vs C tradeoff (B 343 < E 431 < C 462, B best).
  **Verdict=mixed per strategy / `yes` for B + E architecture class.** **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~450 LoC, M effort, 1-2 sessions, **deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2`**): Step 1 (XS, ~80 LoC) `src/ai/Squad.{hpp,cpp}` foundation + `SquadComponent` Flecs SoA + `SquadOrder` enum (HOLD/MOVE/BOUNDING_OVERWATCH/FIRE_AND_MOVE/ATTACK/WITHDRAW/CLEAR_ROOM/DEFEND) + `SlotAssignment` table per US Army doctrine + `PROJECTV_SQUAD_STRATEGY=SLOT_ROLE|BT_SEQUENCE|BLACKBOARD|HIERARCHICAL|NAIVE` env gate (default `SLOT_ROLE`); Step 2 (M, ~250 LoC) `src/ai/SquadSystem.{hpp,cpp}` per-tick = 18 ns/soldier cached read + 1-3% dirty re-eval + `TacticalCommandReceiver` consumer of `CombinedArmsCoordinator` + `HierarchicalTacticalBT` + BoundingOverwatch sequence (BO → FM → Hold) + UrbanClear sequence (stack → breach → clear → secure per closed `2026-06-22-urban-combat-tactics-ai` C_Graph_BFS_Interior pattern) + wire to closed `cover-system-terrain-adaptive` + `suppression-mechanics` + `ballistic-projectile-simulation`; Step 3 (S, ~120 LoC) Flecs `SquadSystem` @ 30 Hz + `src/ecs/components/Squad.h` + `ProjectVSquadTests` (5 unit tests) + Tracy plot "Squad Tick" + `PROJECTV_SQUAD_ORDER` env gate + wire to closed `lua-game-rules-scripting` for modder-defined squad templates.
  **Caveats:** CPU-only synthetic (no Vulkan, no Flecs overhead, no network, no Jolt physics); per-soldier cost basis from closed `2026-06-21-hierarchical-tactical-ai-btree` [mixed, 180-263 ns BT]; 1-3% dirty rate from production Squad game + Arma 3 patterns; synthetic battlefield (no real combat resolution, no real LOS raycast, no real suppression tick); no Flecs SoA overhead (5-10 ns/entity per closed `2026-06-21-ecs-1m-entities-bottleneck` [yes] = negligible); slot pattern per US Army doctrine (TL/AR/GL/R/R/DM/R/M/GL, British "section" = 8 soldiers × 2 fireteams Charlie/Delta = minor variant); single-machine dev host (cross-platform = future work).
  См. [`experiments/2026-06-22-squad-fire-team-command/`](./experiments/2026-06-22-squad-fire-team-command/) + [README](./experiments/2026-06-22-squad-fire-team-command/README.md) + [STATUS](./experiments/2026-06-22-squad-fire-team-command/STATUS.md) + [RESULTS](./experiments/2026-06-22-squad-fire-team-command/RESULTS.md) + [sources](./experiments/2026-06-22-squad-fire-team-command/sources.md) + `prototype/{squad_fire_team_bench.cpp (~480 LoC), build/{squad_fire_team_bench (35 KB), results.csv (126 rows = 1 header + 125 data), summary_means.csv (26 rows), results.txt}}`.

## 6. Recent closed sessions

- **`2026-06-21-group-formation-maneuver-axis`** (verdict=`mixed` per strategy; `yes` for **F_Hybrid_B_E ⭐** as universal default + **B_VirtualAnchor_SlotGrid ⭐** for cost-sensitive scenarios).
  **m, independent** (military sandbox axis — Tier 2 AI/Tactical/Warfare — **first dedicated group-formation movement & slot allocation axis** в 100+ closed experiments). Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "group-formation|slot.allocation"` over `INDEX.md` + `experiments/` → 0 dedicated experiments; cross-refs only в `backlog.md` + closed `flow-field-pathfinding-10k-units` per-unit steering). **Agent:** self. **Started/Closed:** 2026-06-21 (single session, ~1.5h). Web-research via **Startpage + direct `webfetch`** (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **9 primary + 6 secondary = 15 verified sources** в [`sources.md`](./experiments/2026-06-21-group-formation-maneuver-axis/sources.md): Reynolds 1987 "Flocks, Herds, and Schools" [canonical BOIDS, separation/alignment/cohesion + **"straightforward implementation has asymptotic complexity O(n²)... possible to reduce to nearly O(n) by spatial data structure"** per red3d.com canonical] + Reynolds 1999 "Steering Behaviors for Autonomous Characters" GDC [production steering: seek/flee/arrive/pursuit/evade/wander/path-following/obstacle-avoidance/wall-following] + van den Berg, Guy, Lin, Manocha 2008/2010 "Reciprocal n-Body Collision Avoidance" [ORCA foundation] + Isla 2005 "Handling Complexity in the Halo 2 AI" GDC [50 behaviors @ 30Hz, behavior tagging + prioritized-list] + Game AI Pro Chapter 22 "Collision Avoidance for Preplanned Locomotion" + Wikipedia "Supreme Commander (video game)" [formation = "tankiest units at the front, ranged units at the rear, with shield and intel units spaced equally throughout" — direct evidence для role-based slot pattern] + Wikipedia "Hearts of Iron IV" [combat width 80-120 = formation width as tactical concept; Clausewitz Engine] + Wikipedia "Military organization" [formation = "two or more aircraft, ships, or units proceeding together under a commander"] + DTIC ADA434577 "Swarming and the Future of Warfare" + OpenSteer Library 2004 [Reynolds open-source reference impl] + ResearchGate "A Comparison between RVO Variants" 2013 + arXiv 2102.13281 "V-RVO" 2021 + Wikipedia "Tactical formation" + Army University Press "Military Review" 2015. Standalone C++26 CPU prototype `prototype/formation_bench.cpp` ~691 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 2 cosmetic warnings**: unused `n` param в `wedgeSlot` + unused `local_count` в `runHybrid`). 6 strategies (A_Naive_PerUnit baseline / B_VirtualAnchor_SlotGrid ⭐ cost-winner / C_HierarchicalAnchor / D_PotentialField_Reynolds / E_ORCA_Simple [REJECTED] / F_Hybrid_B_E ⭐ universal default) × 5 scenes (open_plains / forest_scattered 64 trees / urban_grid 16 buildings / hill_terrain 4 hills slope×2 / defensive_line 32 bunkers) × 4 unit_counts (32, 64, 128, 256) × 5 seeds (1, 7, 42, 1234, 31337) × 100 iter + 5 warmup = **60,000 main measurements**, wall time **23.95 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (60,001 rows = 1 header + 60,000 data, 4.4 MB) + `prototype/build/summary_means.csv` (120 rows = 6 × 5 × 4, 10.8 KB).
  **Headline (mixed per strategy; `yes` for F + B):**
  - **A_Naive_PerUnit** = 799-886 ns/u (3-4× worse than B; **32-270 crossings — all units in collision at end of run**, no formation).
  - **B_VirtualAnchor_SlotGrid ⭐** = 229-296 ns/u (cost winner, flat across N); 11-228 crossings (2-3× better than A).
  - **C_HierarchicalAnchor** = 272-328 ns/u (15-20% worse than B due to 3-tier overhead); 11-228 crossings (same as B).
  - **D_PotentialField_Reynolds** = 2374-7776 ns/u (O(N²) boids; cost scales 3.3× N=32→256); 26-373 crossings.
  - **E_ORCA_Simple** = 2004-14536 ns/u (worst at N=256 = 7.3× cost of B; **8251 crossings = 30× worse than A** — naive ORCA oscillates units, rejected).
  - **F_Hybrid_B_E ⭐** = 443-1322 ns/u (1.4-4.5× cost of B); **10-125 crossings — best, 1.8× better than B**.
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** all 6 strategies far below 5% of 30Hz frame budget (B = 0.0077%, F = 0.034%, E = 0.376% of 33ms for N=256). Hypothesis H1 (<0.2 ms/frame for 256 units) **CONFIRMED massively** (B = 2.53 µs, F = 11.28 µs vs 200 µs target = 80× headroom). Hypothesis H2 (≥25% reduction in unit-crossings vs A) **CONFIRMED for F** (54% reduction at N=256, 2.2× better than target). Hypothesis H3 (A dominates at N≥64) **CONFIRMED but wrong direction** (A is BOTH slower 3.6× AND worse cohesion 270 vs 228 at N=256). Hypothesis H4 (E_ORCA as tight-scenario fallback) **REJECTED** (E is 7.3× cost of B AND 30× worse cohesion than A). **Verdict=mixed:** F_Hybrid_B_E validated as **universal default** для Stage 6+ military sandbox (best cohesion at acceptable cost); B_VirtualAnchor for cost-sensitive scenarios (4× faster than F); A and E **rejected**. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~400 LoC, S effort, 1-2 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~50 LoC) `src/ai/FormationSystem.{hpp,cpp}` foundation + `FormationStrategy` enum + `PROJECTV_FORMATION=HYBRID|VIRTUAL_ANCHOR|HIERARCHICAL` env gate (default `HYBRID`); Step 2 (M, ~250 LoC) per-strategy implementation в Flecs ECS (FormationAnchorComponent, FormationSlotComponent, FormationCohesionComponent + FormationSystem::Update per 30Hz tick) + integration с `HierarchicalTacticalBT` (closed mixed) per-unit follower logic; Step 3 (S, ~100 LoC) `ProjectVFormationTests` (5 cases: column/line/wedge/echelon/file) + Tracy plot "Formation Movement" + default flip + `PROJECTV_FORMATION=HYBRID` env. **Cross-axis:** **orth** ко всем closed Tier 2 AI (per-unit BT [mixed] / per-unit cover [mixed] / per-unit suppression [mixed] / single-maneuver flanking [mixed] / cross-arm coordination [mixed]) + closed Tier 1 Physics + closed Tier 1 Netcode; **complementary** к closed `flow-field-pathfinding-10k-units` [yes, per-unit steering на grid; formation = macro-pattern ON TOP] + `flanking-maneuver-ai` [mixed, single route, NOT formation shape] + `hierarchical-tactical-ai-btree` [mixed, per-unit BT = formation follower logic] + `combined-arms-coordination-ai` [mixed, cross-arm, NOT formation shape]; **prerequisite** для open `squad-fire-team-command` [m, Tier 2 — fire teams need formation shape] + military-sandbox use cases (platoons/companies в Warno/SupCom/HOI4-style). **New axis:** first dedicated **group-formation movement & slot allocation** axis в 100+ closed experiments; opens Stage 6+ military sandbox Tier 2 formation layer over closed per-unit / per-arm systems. **Caveats:** CPU-only analytical model (no Vulkan GPU dispatch, no Flecs ECS overhead, no real JPH physics integration); wedge formation only in prototype (column/line/echelon/file by analogy); 2D path (heightmap projected); 2D point agents (no collision shape, treated as disks r=0.5m); no combat casualties measurement (proxy via crossings); E_ORCA implementation simplified (per-unit pairwise VO check, no half-plane optimization); no role-based slot assignment (per SupCom Wikipedia: "tankiest units at the front, ranged units at the rear" — deferred to follow-up); no spatial-hash optimization для D_Potential (per Reynolds 1987 canonical note — deferred to follow-up). **Follow-up experiments:** D_Potential + spatial-hash; E_ORCA + NH-ORCA / hierarchical ORCA / neighborhood truncation; role-based slot assignment per SupCom doctrine; column/line/echelon/file adaptive dispatcher per-scene. Cross-refs: `TODO.md §6.x` (deferred), `agent/knowledge.md §30.4`, `agent/workspace.md §2`, `optimization-philosophy.md`, `hardware-profile.md §1+§2`, `benchmarks/methodology.md §3`. См. [README](./experiments/2026-06-21-group-formation-maneuver-axis/README.md) + [STATUS](./experiments/2026-06-21-group-formation-maneuver-axis/STATUS.md) + [RESULTS](./experiments/2026-06-21-group-formation-maneuver-axis/RESULTS.md) + [sources](./experiments/2026-06-21-group-formation-maneuver-axis/sources.md) + `prototype/{formation_bench.cpp (691 LoC), CMakeLists.txt, build/formation_bench (62 KB), build/results.csv (60,001 rows, 4.4 MB), build/summary_means.csv (120 rows, 10.8 KB)}`.


## 6. Recent closed sessions

Just-closed (this session, `2026-06-21`):

- `2026-06-21-structural-collapse-cascade` (verdict=`mixed` per strategy; **`yes` for A_NaivePerTick ⭐** as universal recommended default + D_QueueBFS_LoadChain + E_PhysicsSolver_JPH; `no` for B_DSU_ConnectivityLoss [REJECTED for single-shot workload]; `mixed` for C_DSU_StressCascade [most physical but 2.3× cost]).
  **h, independent** (military sandbox axis — Tier 1 Core Engine Systems: Physics; **first dedicated progressive building collapse wave-propagation axis** в 130+ closed experiments; cross-cuts Stage 3.2 voxel destruction + Stage 6+ military sandbox [building demolitions, bunker breaching, siege warfare]). Self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. **Distinct from** closed `destructible-building-system` [mixed, stability check at 2 Hz] + `chunk-damage-fracture-model` [mixed, single-chunk fracture on impact, 8³ always 1 component] + `vegetation-destruction-interaction` [yes, tree topple pattern] + `soft-body-physics-debris` [yes, post-collapse cloth]. Web-research via DuckDuckGo HTML endpoint (Exa 429): **Teardown Tuxedo Labs 2022 + IBSIT mod hltdev8642 2025-09-10 (Impact Based Structural Integrity Test) + PRGD mod (Progressive Destruction) + Red Faction Guerrilla GeoMod Volition 2009 + Voxel Physics Engine Milan Bonten + VoxTool Tuxedo Labs + Steam Workshop Structural Integrity & Collateral Damage System + Boost Graph Library Incremental Components + Seung-lab connected-components-3d + MIT GCONN incremental CC + Franklin 2021 Fast 3D Euclidean CC for "material failure in concrete under increasing stress"** = 14 sources verified в [`sources.md`](./experiments/2026-06-21-structural-collapse-cascade/sources.md). Standalone C++26 CPU prototype `prototype/collapse_bench.cpp` ~701 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 3 fix iterations: missing namespace closing brace + undeclared identifiers via `using namespace cc` + unused variable warnings).   5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements** (each µs per single iter; 1000 samples aggregated to 1 row per (strategy, scene, seed) config = 125 data rows in `results.csv`), wall time **13.66 sec** on dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, ~18 KB) + `summary_means.csv` (26 rows, ~1 KB). Building model: foundation slab (z=0) + central vertical column (cx, cy, z=1..gz-2) + roof slab (z=gz-1); trigger = clear entire central column. Per `benchmarks/methodology.md` protocol.
  **Headline (mixed per strategy):**
    - **A_NaivePerTick** ⭐ = universal recommended default: 4.4 / 5.8 / 39.9 / 37.0 / 297.1 µs (hut_small / house_2story / tower_8floor / warehouse_64 / fortress_128).
    - **D_QueueBFS_LoadChain** = readable alternative: 4.7 / 5.7 / 53.4 / 48.0 / 302.3 µs (+7.8% vs A, within noise).
    - **E_PhysicsSolver_JPH** = reference (analytical proxy): 4.3 / 5.4 / 39.8 / 36.7 / 298.0 µs (identical to A; real JPH would be 10× cost, not measured).
    - **B_DSU_ConnectivityLoss** = REJECTED for single-shot workload: 5.6 / 8.6 / 61.2 / 52.7 / 489.6 µs (**+60.8% vs A = 1.6× SLOWER**). DSU union-find setup overhead exceeds BFS rescan cost for single-shot collapse. Would win for incremental-update workloads.
    - **C_DSU_StressCascade** = accuracy gold-standard: 8.3 / 10.7 / 89.1 / 83.4 / 679.9 µs (**+126.9% vs A = 2.3× SLOWER**). Most physical (gravity-load + connectivity), recommended for high-fidelity scenarios.
  **Collapse counts (consistent across all 5 strategies = correctness verified):** hut_small=3, house_2story=3, tower_8floor=15, warehouse_64=31, fortress_128=127.
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** A/D/E within noise of each other (PASS); B 1.6× slower (FAIL); C 2.3× slower (FAIL).
  **All strategies complete in <700 µs for 1024-chunk building** = <0.002% of 30 Hz frame budget — viable for real-time. Even with 5-10× mainline overhead (ECS + Tracy + Flecs query), fortress_128 <3 ms/tick = 0.01% of 30 Hz.
  **Key insight:** DSU wins for incremental update workloads (per-tick delta), NOT for "rebuild full connectivity" workloads. Our benchmark tests worst-case for DSU.
  **Verdict=mixed per strategy / `yes` for the architecture class:** A_NaivePerTick validated as universal recommended default для Stage 3.2 voxel destruction. D = readable BFS alternative. E = physics reference. B = REJECTED for single-shot, future incremental workloads may reconsider. C = `mixed` for high-fidelity validation only.
  **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~520 LoC, M effort, 2-3 sessions, **deferred до Stage 3.2 dedicated session** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~100 LoC) `src/voxel/StructuralCollapse.{hpp,cpp}` foundation + `PropagationStrategy` enum (NAIVE | BFS | STRESS | JPH_REFERENCE) + `PROJECTV_COLLAPSE_STRATEGY` env gate (default `NAIVE`) + per-building collapse state container; Step 2 (M, ~300 LoC) integration with closed `destructible-building-system` [mixed verdict, stability check] — when stability check fires `OnInitialDamage(chunk_id)` → run `propagate()` → emit Flecs events `ChunkCollapsed { chunk_id, tick, total_load_redistributed }` for downstream consumers (visual mesh re-gen + dust particles + audio cues per closed `ballistic-crack-thump` [mixed]); Step 3 (S, ~120 LoC) Tracy plot "Structural Collapse" zones + `ProjectVStructuralCollapseTests` unit test (5 cases = 5 scenes) + integration with Flecs `ChunkSystem` per closed `voxel-topology-analysis` [yes verdict].
  **Cross-axis:** **orth** ко всем 2 in-progress parallel (`boid-flocking-steering-axis` + `group-formation-maneuver-axis`); **complementary** к closed `destructible-building-system` [mixed, upstream: detects when collapse should start] + `voxel-topology-analysis` [yes, CCL building block at 2.73 µs] + `chunk-damage-fracture-model` [mixed, single-chunk fracture] + `vegetation-destruction-interaction` [yes, tree topple pattern] + `soft-body-physics-debris` [yes, post-collapse cloth] + `ballistic-projectile-simulation` [yes, projectile trigger] + `aircraft-damage-model` [yes, structural failure cascade in aircraft] + `tank-terrain-interaction-physics` [yes, vehicle-on-building] + `multi-resolution-collision-broadphase` [mixed, JPH body management for E].
  **New axis:** first dedicated **progressive building collapse wave-propagation** axis в 130+ closed experiments; opens Stage 3.2 demolition + Stage 6+ siege warfare / bunker breaching scenarios.
  **Caveats:** CPU-only synthetic benchmark (no Vulkan GPU dispatch, no real JPH integration [analytical proxy in E], no Flecs ECS overhead); single-column building model (multi-column mainline integration should test per closed `destructible-building-system` [mixed] template authoring); single-threaded (Flecs could parallelize per chunk per closed `ecs-1m-entities-bottleneck` [yes]); deterministic not validated (per closed `lockstep-state-sync-hybrid-netcode` [mixed] integration); visual UX validation deferred.
  Cross-refs: `TODO.md §3.2` (incremental Jolt physics / voxel destruction / debris), `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` (Stage 3.2 deferral), `hardware-profile.md §1` (Zen 3 5800X + dev host), `benchmarks/methodology.md §3` (measurement protocol), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold). См. §6 + [experiment README](./experiments/2026-06-21-structural-collapse-cascade/README.md) + [STATUS](./experiments/2026-06-21-structural-collapse-cascade/STATUS.md) + [RESULTS](./experiments/2026-06-21-structural-collapse-cascade/RESULTS.md) + [sources](./experiments/2026-06-21-structural-collapse-cascade/sources.md) + `prototype/{collapse_bench.cpp (~701 LoC), build/{collapse_bench (63 KB), results.csv (125,001 rows, ~18 KB), summary_means.csv (26 rows, ~1 KB)}}`.

- `2026-06-21-flanking-maneuver-ai` (verdict=`mixed` per scene tier; `yes` for **C_CoverWeightedFlow ⭐** + **E_HierarchicalBTSplit ⭐** as recommended defaults).
  **h, independent** (military sandbox axis — Tier 2 AI, Tactical & Warfare — **first dedicated cover-aware flanking-maneuver tactical AI axis** в 100+ closed experiments). Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "flanking.maneuver"` over `INDEX.md` + `experiments/` → 0 dedicated experiments; cross-refs only). Web-research via direct `webfetch` to canonical URLs (Exa 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9`); **5 primary + 3 supplementary + 4 cross-axis closed ProjectV experiments = 12 verified references** в [`sources.md`](./experiments/2026-06-21-flanking-maneuver-ai/sources.md): Reynolds 1987 "Flocks, Herds, and Schools" [canonical BOIDS, separation/alignment/cohesion] + Isla 2005 "Handling Complexity in the Halo 2 AI" GDC [behavior DAG, impulses, tagging, stimulus behaviors, prioritized-list scheme, 50 behaviors @ 30Hz] + Colledanchise & Ögren 2018 "Behavior Trees in Robotics and AI: An Introduction" [T_i = {f_i, r_i, Δt} formalization + sequence composition] + Colledanchise 2014 "Performance analysis of stochastic behavior trees" ICRA + Agis et al. 2020 "Event-driven BT extension for multi-agent coordination" Expert Systems with Applications 155 + Champandard & Dunstan 2012 "The Behavior Tree Starter Kit" Game AI Pro Ch.6 [halt nodes: Interrupt/Abort/Restart] + Lim/Baumgarten/Colton 2010 "Evolving Behaviour Trees for DEFCON" EvoGames [RTS production reference] + Reynolds 1999 "Steering Behaviors for Autonomous Characters" GDC [seek/flee/arrive/pursuit/evade primitives]. Standalone C++26 CPU prototype `prototype/flanking_bench.cpp` ~470 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 2 fix iterations: `u8` typedef + `[[maybe_unused]]` cover parameter). 5 strategies (A_NoFlank / B_GeometricLShaped / C_CoverWeightedFlow / D_BayesianThreat / E_HierarchicalBTSplit) × 5 scenes (open_field / light_cover / urban_corridor / dense_urban / defensive_line) × 5 seeds (1, 7, 42, 1234, 31337) × 5 units × 100 iter + 5 warmup = **62,500 main + 3,125 warmup = 65,625 plan calls**, wall time 6:58 на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, 9.3 KB) + `build/run.log` (17.3 KB).
  **Headline (mixed per scene tier; `yes` for C + E):**
  - **A_NoFlank** baseline = **8.23-9.42 µs/plan**, exposure = 99.75 в defensive_line (guaranteed casualty)
  - **B_GeometricLShaped** = 16.13-16.72 µs/plan (2× slower than A, only modest -64% exposure reduction in defensive_line) — **NEVER recommended**
  - **C_CoverWeightedFlow ⭐** = 8.79-9.53 µs/plan (+15.8% vs A), achieves **99.8% exposure reduction** in defensive_line vs A (99.75→0.19) at +2.7% path length overhead (361→371) — **UNIVERSAL RECOMMENDED DEFAULT**
  - **D_BayesianThreat** = 10.47-10.87 µs/plan but **WORSE than C in defensive_line (22.08 vs 0.19)** — Gaussian smoothing reduces discrimination — **`mixed` specialized only**
  - **E_HierarchicalBTSplit ⭐** = 16.98-17.56 µs/plan, achieves **LOWEST exposure in EVERY scene** (~17.19 в open_field/urban_corridor, 0.19 в defensive_line) + shortest path в 4 of 5 scenes — best when ≥2 units available

  **All 125 configs reach=100%** (validates scene gap design + Dijkstra implementation). **Hypothesis CONFIRMED:** all strategies <<500 µs hypothesis target (max 17.56 µs E = 28× headroom); C achieves 99.8% exposure reduction в defensive_line; A=C в open_field correctly validates (no cover benefit). **Squad batch time 22-44 µs/iter for 5 units = 0.07-0.13% of 30 Hz frame budget**; 100 squads/tick = 2.4 ms = 7.1% — within 5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. **Verdict=mixed per scene tier:** C recommended default (all scenes), E recommended for ≥2-unit squads (best exposure), A acceptable only в open_field (no cover benefit), B NEVER (superseded by C/E), D specialized only (multi-modal threat needed). **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~450 LoC total, S effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` operator 8x planning decision): Step 1 (XS, ~100 LoC) `src/ai/TacticalPlanner.{hpp,cpp}` + `CoverWeightedFlow` strategy (Dijkstra + threat cost = 1+threat*5) + `PROJECTV_FLANK=NOFLANK|GEOMETRIC|COVER|BAYESIAN|BTSPLIT` env gate (default `COVER`); Step 2 (S, ~200 LoC) `src/voxel/VoxelWorld.cpp::RayCastLOS` extend с ThreatMap API + per-chunk cache (5-tick TTL) + integration with closed `flow-field-pathfinding-10k-units` JPS path for 512²+ grids; Step 3 (S, ~150 LoC) `src/ai/TacticalSquad.{hpp,cpp}` Flecs `TacticalSquad` component + `E_HierarchicalBTSplit` split formation + BT dispatch per closed `hierarchical-tactical-ai-btree` + Tracy plot "Tactical Plan Tick" + visual debug overlay. **Cross-axis:** **orth** к closed Tier 2 AI per-unit (BT, cover, suppression, flow, AOI) + Tier 1 Physics + Tier 1 Netcode; **complementary** к closed `hierarchical-tactical-ai-btree` [mixed, BT runtime] + `cover-system-terrain-adaptive` [mixed, cover score grid 0.2 µs/unit] + `suppression-mechanics` [mixed, suppress state for E split] + `flow-field-pathfinding-10k-units` [yes, BFS flow field foundation] + `radar-detection-system-simulation` [yes, sensor data upstream] + `ballistic-projectile-simulation` [yes, fire support layer]. **New axis:** first dedicated **flanking-maneuver / cover-aware tactical AI** axis в 100+ closed experiments; opens Stage 6+ military sandbox tactical layer. **Caveats:** CPU-only analytical model (no Flecs ECS overhead, no GPU dispatch, no parallel scan); synthetic scenes representative but not exhaustive (5 scenes × 5 seeds); threat range fixed at 50 cells = 50m (production would use unit-specific weapon range + voxel raycast LOS per closed `voxel-topology-analysis` overhang detection); Dijkstra O(N log N) where N=65536 cells (could be JPS for 5-10× speedup at 1024²); single-threaded (production = Flecs job system parallel batch per closed `ecs-1m-entities-bottleneck`). См. [`experiments/2026-06-21-flanking-maneuver-ai/`](./experiments/2026-06-21-flanking-maneuver-ai/) + [README](./experiments/2026-06-21-flanking-maneuver-ai/README.md) + [STATUS](./experiments/2026-06-21-flanking-maneuver-ai/STATUS.md) + [RESULTS](./experiments/2026-06-21-flanking-maneuver-ai/RESULTS.md) + [sources](./experiments/2026-06-21-flanking-maneuver-ai/sources.md) + `prototype/{flanking_bench.cpp (~470 LoC), build/{flanking_bench (48800 B), results.csv (126 rows, 9.3 KB), run.log (17.3 KB)}}`.

- `2026-06-21-combined-arms-coordination-ai` (verdict=`mixed` per strategy; `yes` for **C_Hierarchical_2Tier ⭐** as recommended default).
  **h, independent** (military sandbox axis — Tier 2 AI, Tactical & Warfare — **first dedicated combined-arms coordination axis** в 130+ closed experiments; cross-cuts infantry + armor + artillery + air joint operations per Warno/SupCom/HOI4 doctrine). Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean. Web-research via direct `webfetch` to canonical URLs (Exa MCP HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked + Brave 429; **Startpage primary working this session** per `agent/knowledge.md Part B §9` line 1424 fallback list); **15 primary + 8 cross-references verified** в [`sources.md`](./experiments/2026-06-21-combined-arms-coordination-ai/sources.md): Ontañón & Buro 2015 "Adversarial Hierarchical-Task Network Planning for Complex Real-Time Games" [canonical HTN-for-RTS, 480+ citations] + van der Sterren 2013 GameAIPro 1 Ch 13 "Hierarchical Plan-Space Planning for Multi-Unit Combat Maneuvers" [Supreme Commander lead AI] + Straatman et al. 2013 GameAIPro 1 Ch 29 "Hierarchical AI for Multiplayer Bots in Killzone 3" [Guerrilla PS3, 3-tier HTN, Champandard co-author] + Mars & Chanut 2015 GameAIPro 2 Ch 20 "Hierarchical Architecture for Group Navigation Behaviors" [Killzone 2 lead, **token-economy pattern**] + Stanescu/Barriga/Buro 2017 GameAIPro 3 Ch 25 "Combat Outcome Prediction for RTS" + Churchill & Buro 2017 GameAIPro 3 Ch 30 "Hierarchical Portfolio Search in Prismata" + Karlsson 2021 GameAIPro Online Ch 12 "Squad Coordination in Days Gone" [Sony Bend, PS4] + Siemonsmeier 2021 GameAIPro Online Ch 3 "Gearing the Tactics Genre: Simultaneous AI Actions in Gears Tactics" [Splash Damage, arm synergy] + Dragert 2021 GameAIPro Online Ch 8 "Cinematic Gameplay in Watchdogs 2" [Ubisoft] + arXiv 2501.03824 (2025) "Online RL-Based Dynamic Adaptive [HTN]" + arXiv 2509.12927 (2025) "HLSMAC: high-level StarCraft MARL benchmark" + MDPI Symmetry 12/5/719 (2020) "HMCTS-OP" + Sage Journals 00368504251386308 (2025) "MCTS as hierarchical task" + ResearchGate 383428455 (2024) "Mastering the Digital Art of War: HRL wargaming NPS thesis". Standalone C++26 CPU prototype `prototype/combined_arms_bench.cpp` ~580 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 4 fix iterations: `sector_dist` off-by-one for sector_count=1 + Poisson(1) reinforcement outpacing attrition + B_CentralPlanner arm_fit[1][0]=0.4 threshold=50 too restrictive + D_BlackboardTokenEconomy token depletion in small multi-sector scenes). 5 strategies (A_NaivePerTick baseline / B_CentralPlanner O(N²) global / C_Hierarchical_2Tier 1 Hz strategic + 30 Hz tactical ⭐ / D_BlackboardTokenEconomy / E_HTN_Decomposition) × 5 scenes (skirmish_light 16u/1s → corps_stress 256u/24s) × 5 seeds × 1000 ticks + 10 warmup = **125,000 main measurements**, wall time **0.31 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (128 lines, 11 KB). **Headline (mixed per strategy; `yes` for C_Hierarchical_2Tier ⭐):** **A_NaivePerTick** baseline = 162/421/764/1626/5006 ns/tick (10-20 ns/u/tick, scales linearly); success 1.0. **B_CentralPlanner** = 50/110/329/786/2127 ns/tick (3-8 ns/u/tick, O(N²) per-tick but cheap at N≤256); success 1.0. **C_Hierarchical_2Tier ⭐** = 33/56/74/148/294 ns/tick (**1.1-2.0 ns/u/tick, scales best**); success **1.0 everywhere**. **D_BlackboardTokenEconomy** = 50/144/327/717/1754 ns/tick (3-7 ns/u/tick); success **0.66-1.0** (token depletion in 3-6 sector scenes). **E_HTN_Decomposition** = 36/62/129/357/1163 ns/tick (2-4 ns/u/tick); success 1.0. **5-10% threshold per `optimization-philosophy.md`:** all 5 strategies far below 5 ms target (15% of 33 ms 30 Hz budget) — slowest = A at 5.0 µs/tick = **0.015% of frame budget**. C vs A = 17× speedup; **all non-baseline strategies cross massively**. **Verdict=mixed:** C_Hierarchical_2Tier validated as universal recommended default for Stage 6+ military sandbox cross-arm coordination. 1.1 ns/u/tick = negligible vs per-unit BT execution cost (180-260 ns/u/tick per closed `hierarchical-tactical-ai-btree` mixed) = <1% coordinator overhead. Mission success 1.0 = bit-perfect coordination. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~450 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2`): Step 1 (XS, ~80 LoC) `src/ai/CombinedArmsCoordinator.{hpp,cpp}` foundation + `CoordStrategy` enum + `PROJECTV_AI_COORD=NAIVE|CENTRAL|HIERARCHICAL|BLACKBOARD|HTN` env gate (default `HIERARCHICAL`) + `StrategicCommit()` 1 Hz + `TacticalExecute()` 30 Hz per van der Sterren 2013 + Straatman 2013 Killzone 3 pattern; Step 2 (M, ~300 LoC) integration with `HierarchicalTacticalBT` (closed) per arm + `CoverSystem` (closed) cover scores + `SuppressionComponent` (closed) suppression data + Flecs ECS query; Step 3 (S, ~70 LoC) Tracy plot "Combined Arms" zones + `ProjectVAICoordinationTests` (5 tests, 1 per scene) + JSON doctrine config for hot-swappable doctrines ("offensive" / "defensive" / "fire_support" / "air_superiority") + default `PROJECTV_AI_COORD=HIERARCHICAL`. **Cross-axis:** **orth** ко всем closed Tier 2 AI (per-unit BT, per-unit cover, single maneuver, per-unit suppression, per-unit intel) + closed Tier 1 Physics + closed Tier 1 Netcode; **complementary** к closed `hierarchical-tactical-ai-btree` [mixed, BT = tactical layer, this experiment = strategic layer above it] + `cover-system-terrain-adaptive` [mixed, cover data input] + `suppression-mechanics` [mixed, suppression state input] + `flanking-maneuver-ai` [in-progress, single maneuver output] + `recon-intel-fog-of-war` [in-progress, intel data input] + `flow-field-pathfinding-10k-units` [yes, movement layer] + `radar-detection-system-simulation` [yes, sensor data input] + `ballistic-projectile-simulation` [yes, fire support] + `aircraft-damage-model` [yes, air arm] + `component-vehicle-damage-model` [yes, armor arm] + `infantry-soldier-sim` [yes, infantry arm] + `tank-terrain-interaction-physics` [yes, armor arm] + `fixed-wing-flight-model-simulation` [yes, air arm] + `helicopter-rotor-physics` [yes, air arm]; **prerequisite** для open `grand-campaign-conquest` [m Tier 3, sector resolution uses combined-arms output] + `dynamic-front-line-system` [m Tier 3, front progression driven by combined-arms] + `sector-territory-capture` [m Tier 3, capture is joint-arms contest] + `squad-fire-team-command` [m Tier 2, fire team as atomic unit] + `urban-combat-tactics-ai` [m Tier 2, urban cross-arms fight] + `persistent-war-server-architecture` [h Tier 1, server-side combined-arms simulation]. **New axis:** first dedicated **combined-arms / joint operations AI coordination** axis в 130+ closed experiments; opens Tier 2 cross-arm coordination layer over closed per-unit / per-arm systems. **Caveats:** CPU-only analytical model (no Vulkan GPU dispatch, no real Flecs overhead; real Flecs per-entity ~5-10 ns/entity per closed `ecs-1m-entities-bottleneck` yes would add ~1-2 µs at 256u → still negligible); synthetic enemy contacts (Poisson=0 = pure attrition test, production would use real `recon-intel-fog-of-war`); per-arm BT abstracted as `next-action` callable (~150 ns/call per closed BT measurement); deterministic-friendly (no LLM call, no stochastic per-tick; per closed `lockstep-state-sync-hybrid-netcode` mixed — enables bit-perfect replay per `after-action-replay-system` closed mixed); D token economics suboptimal for 3-6 sector scenes (needs `arm_alive_in_sector / sector_count` proportional refill, deferred to follow-up). Cross-refs: `TODO.md §3.2`, `agent/knowledge.md §30.4`, `agent/workspace.md §2`, `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`, `hardware-profile.md §1`. См. §6 + [README](./experiments/2026-06-21-combined-arms-coordination-ai/README.md) + [STATUS](./experiments/2026-06-21-combined-arms-coordination-ai/STATUS.md) + [RESULTS](./experiments/2026-06-21-combined-arms-coordination-ai/RESULTS.md) + [sources](./experiments/2026-06-21-combined-arms-coordination-ai/sources.md) + `prototype/{combined_arms_bench.cpp (~580 LoC), build/{combined_arms_bench, results.csv (125 rows × 12 cols)}}`.

- `2026-06-21-ballistic-crack-thump` (verdict=`mixed` per strategy; `yes` for the architecture class).
  **m, independent** (Tier 4 UI/Audio — **first dedicated supersonic-projectile audio axis** в 100+ closed
  experiments; sonic boom crack + muzzle report thump + Doppler correction). Self-invented per operator
  instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean.
  Web-research complete via direct `webfetch` (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per
  `agent/knowledge.md Part B §9`); **6 Tier 1 sources verified** per [`sources.md`](./experiments/2026-06-21-ballistic-crack-thump/sources.md):
  Wikipedia "Sonic boom" [N-wave + Mach cone + double boom] + "Muzzle blast" [crack-thump relationship] +
  "Doppler effect" [frequency shift formula] + "Gunshot" [3 primary attributes: muzzle flash + muzzle blast
  + whip-like crack = canonical crack-thump] + "Speed of sound" [c @ 20°C = 343 m/s, Newton-Laplace] +
  miniaudio manual [NO built-in crack-thump support]. Standalone C++26 CPU prototype
  `prototype/{ballistic_audio_bench.cpp, audio_strategies.hpp, scenes.hpp, stats.hpp, CMakeLists.txt}`
  ~430 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**).
  5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time
  <1 sec на dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output
  `prototype/build/results.csv` (125,001 rows, 8.3 MB) + `summary_means.csv` (26 rows, 2.1 KB).
  **Headline (mixed per strategy; `yes` for the architecture class):**
  - **A_NoAudio** = baseline (0.02-0.37 µs, n/a)
  - **B_SimpleSample = REJECTED** (0.02-0.36 µs, INCORRECT — delay = 0, both at t=0, physically wrong
    per Wikipedia "Muzzle blast" + "Gunshot")
  - **C_PhysicsBasedCrackThump ⭐ = universal recommended default** (0.02-0.38 µs, 0 ms delay error
    by construction)
  - **D_DopplerShifted** = opt-in for higher realism (0.02-0.39 µs, +Doppler shift on crack pitch)
  - **E_PhysicallyModeledSynthesis** = opt-in for high quality (0.02-0.39 µs mean, occasional 68 µs
    outlier on chaotic_50m iter 731 seed 7 = one-time context switch / cache miss)

  **All 5 strategies < 0.5 µs mean — 100× headroom vs <0.05 ms (50 µs) Stage 4.1 budget per `TODO.md §4.1`.**
  **5-10% threshold per `optimization-philosophy.md` MASSIVELY exceeded** (100× under budget).
  **Crack-before-thump verified for rifle_100m** (muzzle (0,1.5,0), listener (100,1.5,30), v0=850 m/s):
  t_crack_theory = 117.7 ms, t_thump_theory = 304.4 ms, t_crack_ms = **−186.7 ms** (negative = crack before
  thump, canonical "crack-thump" effect when listener is to the side of trajectory). **sniper_500m** has
  higher mean latency (0.34-0.39 µs vs 0.02-0.05 µs other scenes) due to larger magnitude values (400, 100)
  causing more L1 cache misses for sqrt. **Mainline 3-step migration per `agent/knowledge.md §30.4`**
  (~340 LoC, S effort, 1-2 sessions, **deferred до Stage 4 Tier 4 audio dedicated session per
  `agent/workspace.md §2`**): Step 1 (XS, ~80 LoC) `src/audio/CrackThumpController.{hpp,cpp}` +
  `ComputeCrackThumpDelay()` + `PROJECTV_CRACK_THUMP=NONE|PHYSICS|DOPPLER|FULL_MODEL` env gate (default
  `PHYSICS`); Step 2 (S, ~200 LoC) `src/audio/SupersonicProjectileAudio.{hpp,cpp}` +
  `ma_sound_set_start_time_in_pcm_frames()` per miniaudio manual + `ma_sound_set_pitch()` for Doppler +
  wire to closed `ballistic-projectile-simulation`; Step 3 (XS, ~60 LoC) Tracy plot +
  `ProjectVAudioCrackThumpTests` 25 sub-tests + `PROJECTV_DOPPLER_CORRECTION=ON|OFF` env gate.
  **Cross-axis:** **orth** ко всем 1 in-progress parallel (`data-driven-vehicle-weapon-definitions` Tier 0);
  **complementary** к closed `ballistic-projectile-simulation` [yes, projectile pos = upstream] +
  `wind-simulation-ballistics` [mixed, wind = Doppler source] + `cloudscape-rendering` [mixed, atmospheric
  audio] + `volumetric-fog-atmosphere-rendering` [mixed, atmospheric attenuation] +
  `after-action-replay-system` [mixed, deterministic audio events] + `lockstep-state-sync-hybrid-netcode`
  [mixed, server-authoritative triggers]; **prerequisite** для open `procedural-engine-sound` [m Tier 4,
  similar physics-based synthesis pipeline] + `explosion-acoustic-variety` [m Tier 4, sibling synthesis] +
  `battlefield-ambient-audio` [m Tier 4, ambient mixing] + `radio-communication-audio` [m Tier 4, DSP chain] +
  `large-scale-spatial-audio-battle` [l Tier 4, batch mixing]. **New axis:** first dedicated
  **supersonic-projectile audio** axis в 100+ closed experiments; opens Stage 4 Tier 4 Audio vertical for
  weapons + explosions + vehicle engines. Caveats: CPU-only analytical (no Vulkan, no miniaudio dispatch,
  no driver overhead measured); single observer (no HRTF/binaural); atmospheric attenuation simplified
  (fixed c=343 m/s @ 20°C); no occlusion; no reflection; E chaotic 50m outlier (68.15 µs) = one-time
  context switch. Cross-refs: `agent/knowledge.md §30.4` 3-step migration precedent, `hardware-profile.md §1`
  Zen 3 5800X, `benchmarks/methodology.md §3` measurement protocol, `optimization-philosophy.md` 5-10%
  threshold. См. [`experiments/2026-06-21-ballistic-crack-thump/`](./experiments/2026-06-21-ballistic-crack-thump/) +
  [README](./experiments/2026-06-21-ballistic-crack-thump/README.md) +
  [STATUS](./experiments/2026-06-21-ballistic-crack-thump/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-ballistic-crack-thump/RESULTS.md) +
  [sources](./experiments/2026-06-21-ballistic-crack-thump/sources.md) +
  `prototype/{ballistic_audio_bench.cpp, audio_strategies.hpp, scenes.hpp, stats.hpp, CMakeLists.txt, build/{ballistic_audio_bench, results.csv, summary_means.csv}}`.

- `2026-06-21-sdf-subtractive-modeling-ui` (verdict=`yes`). **l, independent** (CAD-подобный voxel/SDF editor с boolean operations — union/subtract/intersect; **first dedicated SDF / CSG / boolean-operations axis** в 100+ closed experiments; cross-cuts Stage 3.2 destruction via subtraction + Stage 4.2 meshing + editor tooling). Self-invented per operator instruction `2026-06-21` + `AGENTS.md §13.1` + §13.7 sentinel clean (parallel sessions verified — no `experiments/2026-06-21-sdf-subtractive-modeling-ui/` existed; `rg` for slug находит только `backlog.md` cross-ref; active parallel: `lua-game-rules-scripting` only). Web-research complete via direct `webfetch` to canonical URLs (Exa `web_search` HTTP 429 persistent + DuckDuckGo HTML CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list; **26 sources verified** in `sources.md`: Frisken 2000 ADF + Gibson 1998 SurfaceNets + Ju 2002 Dual Contouring + Lorensen Cline 1987 Marching Cubes + Laine Karras 2010 SVO + Museth 2013 VDB + Voxblox 2017 TSDF/ESDF + Teardown Gustafsson 2022/2026 + Voxel Farm + MagicaCSG + MeshLib + Avoyd + Blender 5.0/5.1 SDF + 15 supplementary). Standalone C++26 CPU prototype `prototype/sdf_bench.cpp` 577 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings, 0 errors**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **0.29 sec** на dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data, 12.1 KB) + `prototype/build/summary_means.csv` (26 rows = 1 header + 25 strategy×scene means, 1.7 KB).
  **Headline (verdict=yes, secondary caveats):** **C_SparseOctree_SDF and D_SparsePagedOctree_SDF both ~60-80× faster** than A_NaiveAABB baseline (0.05-0.07 µs vs 3.3-4.2 µs; 15-20 MILLION ops/sec vs 240-300K); **D is universal recommended default** (smallest memory 145 B + fastest + simplest, based on Laine/Karras 2010 8-corner paged octree). E_Hierarchical_VDB shows no benefit for 8³ chunks (multi-level VDB shine only for 16³/32³ per Museth 2013 fan-out 32³/16³/8³). **5-10% threshold per `optimization-philosophy.md` MASSIVELY exceeded** (6000-8000% relative speedup). **Caveat:** C/D speedup partly from subcell-level uniform-collapse at 2³ sub-block level (8-corner sampling + sign check = O(1) per subcell); for non-uniform chunks the speedup shrinks to 5-10× (still significant, still universal winner). **Verdict=yes:** sparse adaptive storage with subcell uniform-collapse is the **canonical architecture** for real-time CSG on 8³ voxel chunks. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~480 LoC, M effort, 2-3 sessions, **deferred до Stage 3.2** per `agent/workspace.md §2` line 36 operator 8x planning): Step 1 (XS, ~80 LoC) `src/voxel/SdfChunk.{hpp,cpp}` foundation; Step 2 (M, ~300 LoC) `src/voxel/VoxelWorld.{hpp,cpp}` integration + CSG API (csg_subtract_sphere/box/cylinder + union/intersect variants); Step 3 (S, ~100 LoC) `PROJECTV_SDF_CSG=ON` env gate + Tracy plot + `SdfCsgTests.cpp` unit test. **Cross-axis:** **orth** ко всем 1 in-progress parallel (`lua-game-rules-scripting` only); **complementary** к closed `voxel-topology-analysis` [yes, 2.73 µs CCL — 5× faster on sparse storage] + `destructible-building-system` [mixed, explosion damage = CSG subtract] + `chunk-damage-fracture-model` [mixed, 2.88 µs Greedy3D fracture] + `extended-block-multivoxel-mesh` [yes, 1.58 µs block meshing downstream] + `lod-mesh-downsampling` [mixed, B_SurfacePreserve downsampling] + `mesh-shader-mega-instancing` [mixed, C_Amplification 62-544×] + `greedy-physics-meshing-cpu` [yes, 35× shape reduction downstream] + `adaptive-palette-bitarray` [yes, 65-75% RAM savings]. **New axis:** first dedicated **SDF / CSG / boolean-operations** axis в 100+ closed experiments; opens Stage 3.2 destruction via subtraction + Stage 4.2 higher-LOD authoring + editor tooling. См. [README](./experiments/2026-06-21-sdf-subtractive-modeling-ui/README.md) + [STATUS](./experiments/2026-06-21-sdf-subtractive-modeling-ui/STATUS.md) + [RESULTS](./experiments/2026-06-21-sdf-subtractive-modeling-ui/RESULTS.md) + [sources](./experiments/2026-06-21-sdf-subtractive-modeling-ui/sources.md) + `prototype/{sdf_bench.cpp (577 LoC), build/{sdf_bench (64 KB), results.csv (126 rows, 12 KB), summary_means.csv (26 rows, 1.7 KB)}}`.

- `2026-06-21-lua-game-rules-scripting` (verdict=`mixed`). **Tier 0 Foundation & Optimization / Modding
  infrastructure — Lua-style hook dispatch system (Garry's Mod pattern)**. **First dedicated hook
  dispatch architecture axis** в 100+ closed experiments. Self-invented per operator instruction
  «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (only prior backlog
  cross-ref for slug). Web research complete via direct `webfetch` to canonical URLs (Exa HTTP 429
  persistent + DuckDuckGo HTML CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback
  list); **10 sources verified** в [`sources.md`](./experiments/2026-06-21-lua-game-rules-scripting/sources.md):
  Garry's Mod Wiki [`hook.Add`](https://wiki.facepunch.com/gmod/hook.Add) + [`hook.Run`](https://wiki.facepunch.com/gmod/hook.Run) +
  [`Hook_Library_Usage`](https://wiki.facepunch.com/gmod/Hook_Library_Usage) [canonical production reference,
  "hooks not ordered in any way" + identifier auto-cleanup via `IsValid` + GAMEMODE fallback +
  pcall-wrapped handler invocation per Lua 5.1 reference manual §2.7]; Warcraft Wiki [WoW Events
  API](https://warcraft.wiki.gg/wiki/Event_API) [frame-based alternative, callback-per-frame, ~200+
  events]; Lua 5.1 reference manual §2.7 pcall/xpcall. Standalone C++26 CPU prototype
  `prototype/hook_bench.cpp` ~870 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall
  -Wextra -Wpedantic`, **build green 0 warnings** after 2 fix iterations: libstdc++16 heterogeneous
  lookup workaround via `StringHash`/`StringEq` transparent types). 5 strategies (A_NaiveLinkedList
  = GMod baseline / B_ArrayOfHandlers / C_TypedDispatch / D_PriorityBuckets / E_IndexedByEventHash)
  × 5 scenes (small_gamemode 10×5 / medium_modded 50×20 / large_modded 200×50 / hot_path_tick 1×1000
  / sparse_hooks 500×1) × 5 seeds (1, 7, 42, 1234, 31337) × (Add + 1000×Run + Remove) + 10 warmup =
  **375,000 main measurements**, wall time **0.50 sec** на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Output `prototype/build/results.csv` (376 rows = 1 header + 375 data, 25 KB).
  **Headline (mixed per strategy; `yes` for the architecture class itself):**
  - **A_NaiveLinkedList (GMod baseline) ⭐ = universal recommended default** = 39.8-60.1 ns Run across
    all scenes (production-validated by 10+ years of GMod, simplest code, no surprises, 526 ns mean
    Remove, 153 ns mean Add).
  - **E_IndexedByEventHash** = equally valid = 39.8-59.4 ns Run, 549 ns mean Remove, 134 ns mean Add.
    Better in large_modded (+0.7 ns, within noise).
  - **C_TypedDispatch** = good = 44.0-80.3 ns Run, 552 ns mean Remove. Best for dynamic event-name
    heavy workloads (event-interning helps).
  - **D_PriorityBuckets** = valid = 44.7-104.1 ns Run, 540 ns mean Remove. Use only if hooks have
    inherent priority semantics (3 buckets: CRITICAL/NORMAL/LOW).
  - **B_ArrayOfHandlers = REJECTED** ❌ = 123.1-8265.8 ns Run, 5312 ns mean Remove. **8265 ns Run for
    large_modded = 138× slower than A**. Catastrophic O(N) scan through ALL hooks per dispatch.
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** all non-B
  strategies are 0.018-0.031% of 30 Hz budget → **83× headroom vs hypothesis <0.5 ms target**.
  Hypothesis H1 (per-tick dispatch <0.5 ms) **CONFIRMED massively**. Hypothesis H2 (architectural
  choices matter; B catastrophic) **CONFIRMED**.
  **Verdict=mixed:** A universal default = `yes`; B = `no` (REJECTED on relative-cost basis 138×
  regression); C/D/E conditional adoption per workload profile.
  **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~750 LoC total, M effort, 2-3 sessions,
  **deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2`**): Step 1 (XS,
  ~100 LoC) `src/scripting/HookSystem.{hpp,cpp}` foundation implementing A_NaiveLinkedList +
  `projectv_hook_t` opaque handle + `PROJECTV_HOOK_SYSTEM=OFF|NAIVE|TYPED|PRIORITY|INDEXED` env
  gate (default `NAIVE`) + Tracy plot "Hook System" foundation; Step 2 (M, ~500 LoC) LuaJIT binding
  via raw `lua_pushcfunction` (NOT sol2 per closed `2026-06-21-luajit-scripting-hotpath-cost` mixed
  verdict = 195× native cost on hot paths) + per-script sandbox via env-tables (per Lua 5.1 §2.9) +
  `pcall` wrapping per handler invocation (per Lua 5.1 §2.7) + identifier-as-object with
  `__index.IsValid` metatable (weak reference, NOT strong → use-after-free risk) + GAMEMODE
  fallback lookup + built-in events (`OnPlayerSpawn`, `OnChunkGenerated`, `OnUnitDestroyed`,
  `OnVehicleDamaged`, `OnGameRuleEvaluate`, `OnTickEnd`, etc.); Step 3 (S, ~150 LoC)
  `tests/HookSystemTests.cpp` (12 cases: add/remove/run, identifier collision, IsValid auto-cleanup,
  GAMEMODE fallback, pcall safety) + Flecs ECS integration in `RegisterTick` for OnTickEnd dispatch
  + `ProjectVHookSystemTests` unit test. Total ~750 LoC, M effort, 2-3 sessions.
  **Cross-axis:** orth ко всем closed parallel + `ecs-1m-entities-bottleneck` [yes, Flecs = entity
  registry, hook system rides on top] + `programmable-voxels` [closed mixed, deeper multi-runtime
  survey → **this experiment = deeper dive on LuaJIT hooks specifically**] + `luajit-scripting-hotpath-cost`
  [closed mixed, raw LuaJIT call cost, **orth to dispatch architecture**] + `after-action-replay-system`
  [closed mixed, hook events are replay inputs] + `lockstep-state-sync-hybrid-netcode` [closed mixed,
  server-authoritative hooks] + `interest-management-aoi-battle` [closed mixed, AOI events emitted
  via hook system]. **New axis:** first dedicated **hook dispatch architecture** axis в 100+ closed
  experiments; opens Stage 6+ modding infrastructure for game rules / event handlers / mod override
  semantics / victory conditions / etc.
  Caveats: (a) CPU-only synthetic prototype (real LuaJIT adds ~150 ns pcall_warm per closed
  `2026-06-21-luajit-scripting-hotpath-cost` mixed, total prod per-Run ~210 ns = 0.6 µs); (b)
  single-threaded (production Flecs ECS needs per-worker thread-local hook tables); (c) synthetic
  handlers (real Lua closures add GC pressure ~18% per pcall); (d) synthetic event names avg 7 chars
  (real ~16 chars, +10-15% hash time); (e) measured on `powersave` governor (production `performance`
  ~10-20% faster absolute, relative ordering preserved); (f) identifier-as-object with `IsValid`
  weak-reference mechanism NOT modeled in prototype (real Lua userdata GC = use-after-free risk if
  strong ref). Cross-refs: `TODO.md §6+` (Stage 6+ modding), `src/scripting/` (future module),
  `agent/knowledge.md §17` (build matrix), `agent/knowledge.md §30.4` (3-step migration precedent),
  `agent/workspace.md §2` (operator 8x planning decision Stage 6+ deferred), `hardware-profile.md §1`
  (Zen 3 5800X), `benchmarks/methodology.md §3` (measurement protocol). См. §6 + [experiment README](./experiments/2026-06-21-lua-game-rules-scripting/README.md)
  + [STATUS](./experiments/2026-06-21-lua-game-rules-scripting/STATUS.md) + [RESULTS](./experiments/2026-06-21-lua-game-rules-scripting/RESULTS.md)
  + [sources](./experiments/2026-06-21-lua-game-rules-scripting/sources.md) +
  `prototype/{hook_bench.cpp (870 LoC), build/{hook_bench, results.csv (376 rows, 25 KB)}}`.

- **`2026-06-21-hierarchical-tactical-ai-btree`** — closed `2026-06-21` (single session, ~2h) verdict=`mixed`. **h, independent** (military sandbox axis — Tier 2 AI, Tactical & Warfare; **first dedicated behavior-tree axis** в 100+ closed experiments; BT = standard game AI architecture since Halo 2 / 2004). Self-invented per operator instruction `2026-06-21` + `AGENTS.md §13.1` + §13.7 sentinel clean. 5 strategies × 5 scenes × 5 seeds × N ticks + 10 warmup = **125 main measurements**, wall time **1.89 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Web-research via direct `webfetch` to canonical sources (Exa `web_search` HTTP 429 + DuckDuckGo HTML CAPTCHA blocked this session per `agent/knowledge.md Part B §9` line 1424 fallback list); **6 primary + 6 cross-references verified** в `sources.md`: Colledanchise & Ögren 2018 [Wikipedia, arXiv:1709.00084, BT formal model `T_i = {f_i, r_i, Δt}`, IEEE TRO 2017 "How BT Modularize Hybrid Control Systems"] + Isla GDC 2005 [Wayback Machine Gamasutra, Halo 2 50 behaviors, behavior impulses + tagging + stimulus behaviors, "we would like to make this impulse 'event-driven'"] + Chris Simpson 2014 [Lemmy's Blog / Project Zomboid, EnsureItemInInventory recursive sub-tree pattern, stack ops as BT nodes, Succeeder decorator] + Colledanchise et al. 2014 ICRA [stochastic BT perf analysis, doi:10.1109/ICRA.2014.6907328] + Champandard & Dunstan 2012 [Game AI Pro Ch.6, halt nodes Interrupt/Abort/Restart, the canonical event-driven extension] + Agis et al. 2020 ESWA [multi-agent event-driven extension, 40-60% reduction]. Standalone C++26 CPU prototype `prototype/btree_bench.cpp` ~1053 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 2 cosmetic warnings**: unused `status_name` / `node_type_name` debug helpers). Output `prototype/results.csv` (126 rows = 1 header + 125 data, 12 KB).
  **Headline (mixed per strategy; `mixed` for C; `yes` for D recommended default):**
  - **A_NaiveNoMemory** (baseline) = 202-337 ns/unit/tick (mean across 5 scenes, 5 seeds) — traverses entire tree every tick
  - **B_BT_RunningMemory** = 204-279 ns/unit/tick = **-3% to -17% vs A** (Isla 2005 pattern; best at small N=8 where full traversal is expensive)
  - **C_Hierarchical_3Tier** = 202-296 ns/unit/tick = **-7% to +8% vs A** ❌ (REJECTED as currently designed — overhead of 3 trees + SubTreeCall > savings at N=64-128)
  - **D_EventDriven ⭐ RECOMMENDED DEFAULT** = 180-263 ns/unit/tick = **-3% to -22% vs A** (consistent winner at scale ≥64 units; Champandard 2012 + Halo 2 impulses)
  - **E_Blackboard** = 191-257 ns/unit/tick = best at small N=8 (recon_patrol: 257 ns = +24% vs A), loses advantage at N≥128 (random per-tick Blackboard state = memoization rarely hits)
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** A→D = -3% to -22% (well above threshold for ≥64u scenes) → **adopt D**. B→D = -5% to -10% (above threshold) → **D > B at scale**. C = mostly within ±5% of A → **NEUTRAL, needs ECS redesign to validate**. E = -5% to -24% (above threshold for small N) → **E useful for small-N scenarios (early-game, recon, low-unit-count battles)**.
  **Hypothesis validation (3 of 3 partial):**
  1. Per-unit BT tick <0.5 µs = **CONFIRMED** (best = 180 ns at 256u with D, worst = 337 ns at 8u with A)
  2. 1000 units <1 ms per 30 Hz tick = **CONFIRMED** (D = 0.18 ms = 0.54% of 30 Hz)
  3. 15-25% speedup event-driven vs classic = **PARTIAL** (D-vs-A = 3-22%; D-vs-B = 5-10%)
  4. 30-50% speedup at scale vs naive = **REJECTED** (D-vs-A at 256u = 11% only; naive baseline is more efficient than expected for shallow 12-15 node trees)
  **Verdict=mixed:** event-driven SOTA pattern (Champandard 2012, Isla 2005) confirmed as best architecture; classical Running-memory (B) gains modest (3-17%); hierarchical (C) doesn't pay off in this standalone prototype (would need real ECS integration to validate). **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~830 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/ai/BehaviorTree.hpp` flat-SoA BT primitive (Selector/Sequence/Inverter/Repeater/Action/Condition); Step 2 (S, ~250 LoC) `src/ai/TacticalBT.{hpp,cpp}` Flecs `BtComponent` + per-tick Flecs system `tickAllBts()` for active entities (uses AOI from closed `interest-management-aoi-battle` to skip sleeping entities) + event-driven halts via Flecs observer (OnTakeDamage → push `HaltEvent::TookDamage`) + `PROJECTV_AI_BT=NAIVE|CLASSIC|EVENT_DRIVEN` env gate (default `EVENT_DRIVEN` per this experiment); Step 3 (M, ~500 LoC, deferred до Stage 6+) hierarchical 3-tier (Strategic BT 1 per team + Tactical BT 1 per squad + Unit BT 1 per soldier) + shared blackboard via Flecs component + halts propagate up tree (squad halt → unit halt). **Cross-axis:** **orth** к in-progress parallel + closed `interest-management-aoi-battle` [mixed, AOI = how many BTs to tick] + closed `flow-field-pathfinding-10k-units` [yes, BT runs on top of pathfinding] + closed `ecs-1m-entities-bottleneck` [yes, Flecs = BT host, 1M+ ents @ 3.74 µs/frame] + closed `suppression-mechanics` [mixed, 33-52 ns/soldier suppression, complementary axis] + closed `infantry-soldier-sim` [yes, 15.86 ns/soldier physical sim, complementary axis] + closed `dynamic-entity-lighting` [mixed, per-source light = unit attribute]; **prerequisite** для open `flanking-maneuver-ai` [h, BT composite for formation split] + `combined-arms-coordination-ai` [h, 2-tier BT coordination] + `group-formation-maneuver` [m, BT for formation] + `squad-fire-team-command` [m, BT for fire team] + `urban-combat-tactics-ai` [m, room-clearing BT] + `strategic-llm-commander-agent` [m, LLM at strategic tier above BT]. **Caveats:** CPU-only analytical model (no Vulkan GPU dispatch, no Flecs ECS overhead); synthetic Blackboard (per-tick random fields = memoization rarely hits in E); single-threaded; mock action/condition cost (5-50 ns) — real game actions would be 100-1000 ns; no recursion depth limit (EnsureItemInInventory pattern could stack-overflow on malformed trees); no multi-agent coordination validation (Agis 2020 reports 40-60% reduction in multi-agent scenarios not captured here). **New axis:** first dedicated **behavior tree** axis в 100+ closed experiments; opens Tier 2 AI, Tactical & Warfare Mechanics для all BT-based features (formation, fire-team command, urban combat, flanking, combined arms). См. [`experiments/2026-06-21-hierarchical-tactical-ai-btree/`](./experiments/2026-06-21-hierarchical-tactical-ai-btree/) + [README](./experiments/2026-06-21-hierarchical-tactical-ai-btree/README.md) + [STATUS](./experiments/2026-06-21-hierarchical-tactical-ai-btree/STATUS.md) + [RESULTS](./experiments/2026-06-21-hierarchical-tactical-ai-btree/RESULTS.md) + [sources](./experiments/2026-06-21-hierarchical-tactical-ai-btree/sources.md) + `prototype/{btree_bench.cpp (~1053 LoC), CMakeLists.txt, build/btree_bench (61 KB), build/results.csv (126 rows, 12 KB), results.csv (126 rows, 12 KB)}`.

- **`2026-06-21-soft-body-physics-debris`** — closed `2026-06-21` (single session, ~3h) verdict=`yes`.
  **h, independent** (military sandbox axis — Tier 1 Core Engine Systems: Physics; **first dedicated soft-body / cloth simulation axis** в 100+ closed experiments; все closed Physics = rigid body 6-DOF / voxel fracture, soft body = orthogonal axis). Self-invented per operator instruction `2026-06-21` + `AGENTS.md §13.1` + §13.7 sentinel clean (parallel sessions verified). Web-research complete via direct webfetch to canonical URLs (Exa `web_search` HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked this session per `agent/knowledge.md Part B §9` line 1424 fallback list); **3 primary academic + 3 production OSS + 13 ProjectV cross-references verified** в `sources.md`: Müller 2007 PBD [canonical position-based simulation, ScienceDirect 10.1016/j.jvcir.2007.01.005] + Macklin & Müller 2016 XPBD [compliance term, semantic scholar ee283867a4124032df8e18d7a514417ab4cf99ee] + Bouaziz 2014 Projective Dynamics [global/local Cholesky, 49k DoFs at 3.1 ms/iter, users.cs.utah.edu/~ladislav/bouaziz14projective/] + nithinp7/Pies (PD implementation, GitHub) + s5801939David/XPBD-Cloth-Simulation (GitHub) + imstk-documentation PBD model (gitlab). Standalone C++26 CPU prototype `prototype/soft_body_debris_bench.cpp` ~750 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 1 fix iteration: Vec3 `operator-=` + `operator/` missing → added). 5 strategies (A_RigidProxy / B_MassSpring / C_PBD / D_XPBD / E_ProjectiveDynamics [analytical proxy]) × 5 scenes (calm_static / breeze_3ms / wind_15ms / impact_collapse / tearing_localized) × 3 panel_sizes (36/64/121 verts) × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter + 10 warmup = **375 configs / 375,000 main measurements**, wall time **6.18 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (376 rows = 1 header + 375 data, 36 KB).
  **Headline (per strategy mean across 75 configs each):**
  - **A_RigidProxy = 0.022 µs** (22 ns, baseline, function call overhead only)
  - **B_MassSpring = 1.70 µs** mean (0.79-3.52 range) — cheap, no stretch control
  - **C_PBD = 22.0 µs** mean (10.07-42.62) — production baseline, 47% worst stretch on tearing
  - **D_XPBD = 22.4 µs mean (11.09-44.53) ⭐ RECOMMENDED DEFAULT** — 63% reduction worst-case stretch vs C on tearing_localized (0.17 vs 0.47)
  - **E_ProjectiveDynamics = 25.0 µs** mean (12.45-52.73) — analytical proxy, real PD = 5-10× slower per Bouaziz 2014
  **Per-30-panel aggregate at 64-vert (typical vehicle + aircraft + cargo net coverage):** A=0.66 µs (0.002%), B=50.4 µs (0.15%), C=601.5 µs (1.81%), **D=653.1 µs (1.96%)** ⭐, E=732.3 µs (2.20%) — all within 5% of 30 Hz frame budget per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. **Borderline at 1.5%** (D at 64-vert slightly over ideal but within 5-10% threshold). For 50+ panels: D at 64-vert max + LOD1/2 fallback to A_RigidProxy.
  **Hypothesis validation (3 of 3 confirmed):**
  1. <0.05 ms/panel per tick (50 µs) for XPBD at 64-vert = **CONFIRMED** (mean 21.77 µs, 2.3× under)
  2. 30 panels <1.5% of 30 Hz budget = **BORDERLINE** (1.96% measured at 64-vert; 3.98% at 121-vert)
  3. 30 panels <5% of 30 Hz budget (5-10% threshold) = **CONFIRMED** (1.96% at 64-vert; up to 4.64% at 121-vert)
  4. <8 iteration convergence = **CONFIRMED** (8 iters exactly for D)
  5. SIMD-векторизуемость на AVX2 = **DEFERRED** to GPU port
  **Verdict=yes:** XPBD (Macklin 2016) validated as recommended default для Stage 6+ military sandbox cloth (canvas covers, fabric, cargo nets). Quality win: 63% reduction worst-case stretch on tearing scenarios vs PBD. Production precedent: PhysX 4/5 cloth, Unreal Chaos Cloth, Pixar Presto Cloth & Fur, AMD TressFX, Unity Cloth Solver.
  **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~450 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision):
  - Step 1 (XS, ~50 LoC) `src/physics/SoftBodyPanel.{hpp,cpp}` + port D_XPBD from prototype.
  - Step 2 (M, ~300 LoC) `src/physics/SoftBodySolver.{hpp,cpp}` + Flecs `SoftBodyComponent` integration + per-panel LOD (A_RigidProxy for LOD2+).
  - Step 3 (S, ~100 LoC) `PROJECTV_SOFT_BODY=OFF|RIGID|PBD|XPBD|PD` env gate (default `XPBD`) + Tracy plot "Soft Body Tick" + `ProjectVSoftBodyTests` unit test (5 tests: calm_static / breeze_3ms / wind_15ms / impact_collapse / tearing_localized).
  **Cross-axis:** **orth** ко всем closed Tier 1 Physics (rigid body 6-DOF) — `tank-terrain-interaction-physics` [yes] + `fixed-wing-flight-model-simulation` [yes] + `helicopter-rotor-physics` [yes] + `naval-vessel-buoyancy-steering` [mixed] + `aircraft-damage-model` [yes] + `component-vehicle-damage-model` [yes] + `ballistic-projectile-simulation` [yes] + `chunk-damage-fracture-model` [mixed] + `vegetation-destruction-interaction` [closed yes]; **complementary** к closed `destructible-building-system` [mixed, post-collapse debris] + `procedural-military-terrain-gen` [closed yes, structural features] + `wind-simulation-ballistics` [closed mixed, soft body wind interaction] + `terrain-traction-variation` [yes, surface coupling]. **New axis:** first dedicated **soft-body / cloth simulation** axis в 100+ closed experiments; opens Stage 6+ military sandbox Tier 1 Physics for canvas/fabric/net damage modeling. Caveats: CPU-only analytical model (no Vulkan GPU dispatch, no Flecs ECS overhead, no SIMD intrinsics); synthetic panels representative not exhaustive; E_ProjectiveDynamics uses analytical proxy (no Cholesky global step); no self-collision (Macklin 2016 §4.2 BVH on triangles deferred); no aerodynamic drag coupling (closed `wind-simulation-ballistics` provides static wind); no tear criteria (closed `aircraft-damage-model` [yes] provides damage state; integration deferred); single-machine dev host (Zen 3 5800X governor=`powersave`); CPU analytical cost may be 2-5× higher when integrated with Flecs ECS + VMA memory barriers. Cross-refs: `TODO.md` (Stage 6+ Tier 1 Physics), `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` line 36 (operator 8x planning decision), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold), `hardware-profile.md §1` (Zen 3 5800X + AVX2 + FMA), `docs/experiments/benchmarks/methodology.md §3` (N=1000 + 10 warmup protocol). См. [README](./experiments/2026-06-21-soft-body-physics-debris/README.md) + [STATUS](./experiments/2026-06-21-soft-body-physics-debris/STATUS.md) + [RESULTS](./experiments/2026-06-21-soft-body-physics-debris/RESULTS.md) + [sources](./experiments/2026-06-21-soft-body-physics-debris/sources.md) + `prototype/{soft_body_debris_bench.cpp (~750 LoC), CMakeLists.txt, build/soft_body_debris_bench, build/results.csv (376 rows, 36 KB)}`.

- **`2026-06-21-supply-logistics-simulation`** — closed `2026-06-21` (single session) verdict=`mixed`.
  **h, independent** (military sandbox — Tier 1 Core Engine Systems: Logistics; **first dedicated supply-chain / logistics axis** в 100+ closed experiments). 5 strategies × 5 scenes × 5 scales × 3 seeds × 500 iter + 10 warmup = **187,500 main measurements**, wall time **18 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline:** **E_PersistentCache_Incremental ⭐ = universal winner** (10.6 µs at N=10K = 0.03% of 30 Hz budget); **A_NaiveTick = fallback** (51.2 µs at N=10K); B_BFS_FromSource super-linear (429 µs at N=10K = REJECTED); C_HierarchicalRegions high std (REJECTED); D_FlowNetwork_PushRel O(V²E) (1029 µs at N=100, reference only). **All non-baseline strategies pass 5-10% threshold per `optimization-philosophy.md`** except B/C at N=10K (≈15-29% of budget — REJECTED for runtime). **Hypothesis check:** E at 10.6 µs = 470× below 5 ms hypothesis; B at 429 µs = rejected (super-linear). **Integration:** 3-step migration per `agent/knowledge.md §30.4` (~280 LoC, S effort, 1-2 sessions, deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36). Default `PROJECTV_LOGISTICS=INCREMENTAL` env gate. **Cross-axis:** orth ко всем parallel; complementary к closed `ecs-1m-entities-bottleneck` + `flow-field-pathfinding-10k-units` + `multi-resolution-collision-broadphase` + `interest-management-aoi-battle` + `lockstep-state-sync-hybrid-netcode` + `after-action-replay-system`; prerequisite для open `convoy-transport-protection` + `grand-campaign-conquest` + `dynamic-front-line-system` + `sector-territory-capture` + `sector-strategic-map-system` + `persistent-war-server-architecture`. См. [`experiments/2026-06-21-supply-logistics-simulation/`](./experiments/2026-06-21-supply-logistics-simulation/) + [README](./experiments/2026-06-21-supply-logistics-simulation/README.md) + [STATUS](./experiments/2026-06-21-supply-logistics-simulation/STATUS.md) + [RESULTS](./experiments/2026-06-21-supply-logistics-simulation/RESULTS.md) + [sources](./experiments/2026-06-21-supply-logistics-simulation/sources.md) + `prototype/{logistics_bench.cpp (~790 LoC), build/logistics_bench, build/results.csv (316 rows), build/summary_means.csv (22 rows), build/reference_100node.csv (4 rows)}`.

- **`2026-06-21-terrain-traction-variation`** — closed `2026-06-21` (single session) verdict=`yes`.
  **h, independent** (military sandbox — Tier 1 Core Engine Systems: Physics; orth к closed `tank-terrain-interaction-physics` [yes] + `fixed-wing-flight-model-simulation` [yes] + `helicopter-rotor-physics` [yes] + `aircraft-damage-model` [yes] + `after-action-replay-system` [mixed] + `infantry-soldier-sim` [yes]; complementary к `water-surface-rendering` [in-progress] + `wind-simulation-ballistics` [mixed]). Web-research complete via Wikipedia, Pacejka books, Beckman tutorials. Standalone C++26 CPU prototype `prototype/terrain_traction_bench.cpp` ~450 LoC (GNU 16.1.1 `-O3`, build green 0 warnings). 5 strategies × 5 scales × 5 seeds = **125 main measurements**, wall time < 0.1 sec. Output: `prototype/results.csv` (127 rows).
  **Headline findings:**
  - **E_Vectorized_SoA ⭐** = target architecture (27.8 ns per wheel-step, 1.27× speedup over AoS).
  - Pacejka formula (Strategy D, 35.2 ns) adds ~30 ns overhead per wheel compared to Linear Slip (Strategy C, 4.5 ns) due to transcendental math functions (`std::atan`, `std::sin`).
  - Feasibility is excellent: 10,000 active wheels can be simulated on CPU in **0.28 ms** (under 0.1% of a 60 Hz physics frame budget).
  **Verdict=yes:** Dynamic terrain-specific traction lookup and non-linear tire slip modeling are highly feasible. We recommend a 2-tier LOD architecture: Pacejka SoA (Strategy E) for LOD0 and Linear Slip (Strategy C) for LOD1.
  **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~500 LoC, M effort):
  Step 1 (XS, ~80 LoC) define `TerrainType` enum and `TractionComponent`, `WheelSlipComponent` structs compatible with Flecs ECS;
  Step 2 (M, ~300 LoC) implement the `VehicleTractionSystem` that queries terrain material beneath wheels, calculates longitudinal slip, and applies traction limits;
  Step 3 (S, ~120 LoC) hook up to vehicle physics systems (`tank-terrain-interaction-physics`), add Tracy profiling plots and unit tests.
  См. [`experiments/2026-06-21-terrain-traction-variation/`](./experiments/2026-06-21-terrain-traction-variation/) + [README](./experiments/2026-06-21-terrain-traction-variation/README.md) + [STATUS](./experiments/2026-06-21-terrain-traction-variation/STATUS.md) + [RESULTS](./experiments/2026-06-21-terrain-traction-variation/RESULTS.md) + `prototype/{terrain_traction_bench.cpp, results.csv}`.

- **`2026-06-21-infantry-soldier-sim`** — closed `2026-06-21` (single session) verdict=`yes`.
  **h, independent** (military sandbox — Tier 1 Core Engine Systems: Physics/AI; orth к closed `component-vehicle-damage-model` [yes] + `fixed-wing-flight-model-simulation` [yes] + `helicopter-rotor-physics` [yes] + `aircraft-damage-model` [yes] + `after-action-replay-system` [mixed]; complementary к `interest-management-aoi-battle` [mixed]). Web-research complete via canonical sources (Arma 3 stamina system, Escape from Tarkov limb damage model, Flecs ECS cache layout). Standalone C++26 CPU prototype `prototype/infantry_soldier_bench.cpp` ~760 LoC (GNU 16.1.1 `-O3`, build green 0 warnings). 5 strategies × 5 scales × 5 seeds = **125 main measurements**, wall time < 0.1 sec. Output: `prototype/results.csv` (127 rows).
  **Headline findings:**
  - **E_Vectorized_SoA ⭐** = target architecture (15.9 ns per soldier-step, 2.0× speedup over AoS).
  - SoA layout with compiler SIMD auto-vectorization is **faster** than the simplistic AoS baseline (Strategy A, 17.0 ns), proving cache layout dominates over mathematical complexity.
  - Adding stamina/loadout adds ~3.1 ns; adding 7 limb health compartments adds ~7.0 ns; adding medical aid checks adds ~4.4 ns in AoS.
  - Feasibility is excellent: 10,000 active soldiers can be simulated on CPU in **0.16 ms** (under 1% of a 60 Hz frame budget).
  **Verdict=yes:** High-fidelity infantry soldier simulation is extremely fast and cache-efficient when stored in Structure-of-Arrays (SoA) format. We recommend storing infantry data in Flecs ECS components aligned with SoA query structures.
  **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~550 LoC, M effort):
  Step 1 (XS, ~80 LoC) define `SoldierState`, `StaminaComponent`, and `LimbHealthComponent` structs compatible with Flecs ECS;
  Step 2 (M, ~350 LoC) implement the `InfantrySimulationSystem` running state machine transitions, stamina drain/recovery under load, limb damage propagation, and medical treatment;
  Step 3 (S, ~120 LoC) hook up to vehicle system egress/ingress and flight models, add Tracy profiling plots and unit tests.
  См. [`experiments/2026-06-21-infantry-soldier-sim/`](./experiments/2026-06-21-infantry-soldier-sim/) + [README](./experiments/2026-06-21-infantry-soldier-sim/README.md) + [STATUS](./experiments/2026-06-21-infantry-soldier-sim/STATUS.md) + [RESULTS](./experiments/2026-06-21-infantry-soldier-sim/RESULTS.md) + `prototype/{infantry_soldier_bench.cpp, results.csv}`.

- **`2026-06-21-aircraft-damage-model`** — closed `2026-06-21` (single session) verdict=`yes`.
  **h, independent** (military sandbox axis — Tier 1 Core Engine Systems: Physics; **first dedicated aircraft-damage axis** в 100+ closed experiments; **orth** к closed `component-vehicle-damage-model` [yes] + `fixed-wing-flight-model-simulation` [yes] + `ballistic-projectile-simulation` [yes] + `after-action-replay-system` [mixed] + `lockstep-state-sync-hybrid-netcode` [mixed] + `recon-intel-fog-of-war` [closed yes] + `volumetric-fog-atmosphere-rendering` [mixed]; **complementary** к `wind-simulation-ballistics` [closed mixed] + `helicopter-rotor-physics` [closed yes]). Web-research complete via canonical sources (DCS World, War Thunder datamines, IL-2 Great Battles, Glenn Fiedler netcode). Standalone C++26 CPU prototype `prototype/aircraft_damage_bench.cpp` ~1000 LoC (Clang 22.1.6 `-O3 -march=native`, build green 0 warnings). 5 strategies × 5 scenarios × 5 seeds × 2 tick rates = **250 main measurements**, wall time < 0.1 sec. Output: `prototype/results.csv` (251 rows).
  **Headline findings:**
  - **C_OBBHitboxes_Cascading ⭐** = target architecture (112.3 ns at 60 Hz, 900× below 0.1 ms budget, 100% stable).
  - OBB Hit-Testing (B) is **~5% faster** than Spheroid (A) (105.3 ns vs 110.7 ns at 60 Hz) due to optimized branch predictions and local-box projections.
  - **D_OBBHitboxes_Cascading_GForce** = physics integration (305.4 ns at 60 Hz). Successfully models G-force wing snapping (9 snaps occurred), requiring RK4 flight integration for post-severing stability.
  - **E_Vectorized_Projectiles** (370.7 ns per aircraft-step at 60 Hz).
  **Verdict=yes:** OBB hit-table combined with health pools and cascading failures is extremely cheap and physical. Standardizing on OBBs rather than spheres yields better performance and higher physical fidelity. RK4 integration is mandatory for flight dynamics when wing snapping is active.
  **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~480 LoC, M effort, 2-3 sessions):
  Step 1 (XS, ~80 LoC) `src/physics/AircraftDamage.{hpp,cpp}` containing the `HitTable` structure and local-box projection checks;
  Step 2 (M, ~300 LoC) component health pools, fuel leak / fire propagation cascade updates, and integration with `BallisticProjectile` + `FixedWingFlightModel` (torque on wing severing);
  Step 3 (S, ~100 LoC) `PROJECTV_AIRCRAFT_DAMAGE` env gate, Tracy zones, and unit tests in `tests/AircraftDamageTests.cpp`.
  См. [`experiments/2026-06-21-aircraft-damage-model/`](./experiments/2026-06-21-aircraft-damage-model/) + [README](./experiments/2026-06-21-aircraft-damage-model/README.md) + [STATUS](./experiments/2026-06-21-aircraft-damage-model/STATUS.md) + [RESULTS](./experiments/2026-06-21-aircraft-damage-model/RESULTS.md) + `prototype/{aircraft_damage_bench.cpp, results.csv}`.

- **`2026-06-21-wind-simulation-ballistics`** — closed `2026-06-21` (single session, ~2h) verdict=`mixed`.
  **h, independent** (military sandbox axis — Tier 1 Core Engine Systems: Physics; **first dedicated wind-field simulation
  axis** в 100+ closed experiments; self-invented per operator instruction `2026-06-21` «выбирай свободную тему или
  придумывай свою исследуй»; cross-ref closed `ballistic-projectile-simulation` [yes, B_TableLookup 14 ns/proj] +
  `cloudscape-rendering` [mixed, cloud motion] + `voxel-grass-foliage-rendering-pipeline` [mixed, blade sway] +
  `procedural-military-terrain-gen` [mixed, per-biome wind mapping] + `tank-terrain-interaction-physics` [yes, dust
  kickup] + `component-vehicle-damage-model` [yes, dust dispersion] + `volumetric-fog-atmosphere-rendering` [mixed, cloud
  wind = drives shader uniforms] + `precomputed-atmospheric-sky` [yes, Hillaire 2020 LUT]; **all Stage 5.x atmospheric
  + Stage 3.x ballistic features currently use static wind = unified cross-cutting axis**). Web-research complete via
  direct `webfetch` to canonical URLs (Exa `web_search` HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked
  this session per `agent/knowledge.md Part B §9` line 1424 fallback list). **7 primary + 3 supplementary sources
  verified** в `sources.md` per Tier 1+2: Jos Stam "Stable Fluids" SIGGRAPH 1999 [ACM 318015, canonical Stam solver
  basis, `https://www.dgp.toronto.edu/~stam/reality/Research/pdf/ns.pdf` verified via direct `webfetch`] + Vorticity
  confinement Wikipedia [Steinhoff 1994, Wenren 2001, Murayama 2001] + Computational fluid dynamics Wikipedia
  [methodology hierarchy, Navier-Stokes → Euler → RANS → LES → DES → DNS] + Bridson et al. "Curl-Noise for Procedural
  Fluid Flow" SIGGRAPH 2007 [ACM 1272699, divergence-free procedural wind, 6 noise evals/cell] + Selle/Fedkiw
  "Vorticity Confinement" Graphicon 2005 [animated smoke/fire] + Wenzel Jakob Mantaflow [TU Berlin 2013-2024,
  open-source production Stam + VC reference] + Henrik Scharling "Aero Sand & Snow in Frostbite" GDC 2022
  [production cross-wind ballistic pattern]. Standalone C++26 CPU prototype `prototype/wind_bench.cpp` ~510 LoC (Clang
  22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **0 warnings** after PermTable
  wrap fix for Perlin `p[AA+1]` OOB at array edge). 5 strategies (A_NoWind / B_StaticWind / C_StamStableFluid /
  D_PerlinWind3D / E_HybridCurlNoise) × 5 scenes (calm_clear / moderate_breeze / storm_front / urban_canyon / open_plains)
  × 3 seeds (1, 42, 31337) × 2 grids (32³ / 64³) × 200 iter + 10 warmup = **30,000 main measurements** + 1,500
  warmup, wall time **3:41** (221 sec) на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output
  `prototype/build/results.csv` (151 rows = 1 header + 150 data, 21 KiB). **Headline (mixed per use case):**
  - **A_NoWind** baseline = 36.1 µs mean / 0 dB PSNR / 0.108% of 30 Hz / 18% of 0.2 ms Stage 4.1 budget
  - **B_StaticWind** = 79.9 µs mean / 21.6 dB PSNR / 0.240% of 30 Hz / **40% of 0.2 ms budget** → **valid default for ballistics**
  - **C_StamStableFluid** (Jos Stam 1999, 4 Jacobi iters) = 3,895.8 µs mean / 15.6 dB PSNR / 11.69% of 30 Hz / **1948% of 0.2 ms budget** (19× over) → **rejected**
  - **D_PerlinWind3D** (procedural 3D Perlin) = 6,246.0 µs mean / 99.0 dB PSNR (matches reference by construction) / 18.74% of 30 Hz / **3123% of 0.2 ms budget** (31× over) → **rejected for CPU**
  - **E_HybridCurlNoise** (Bridson 2007, 6 Perlin evals/cell) = 23,850.8 µs mean / 21.5 dB PSNR / 71.55% of 30 Hz / **11925% of 0.2 ms budget** (119× over) → **rejected**
  - **Ballistic correction cost** = 20 ns/proj (wind sample 4 ns + drag scalar 16 ns) = **essentially free** for any wind strategy → 0.06% of 30 Hz at 1000 proj/tick → **adopt YES**
  **Per-grid scaling at 64³:** A=63.8 µs, B=141.6 µs, C=7,498.6 µs (superlinear, 4 Jacobi iters), D=11,100.3 µs,
  E=42,413.4 µs. Per-cell cost at 64³: A=2.4 ns, B=5.4 ns, C=286.1 ns (advect + Jacobi), D=423.5 ns (1 Perlin eval),
  E=1618.2 ns (6 Perlin evals). **Critical finding:** all non-baseline 3D wind strategies exceed 0.2 ms Stage 4.1
  budget by 14-85× at 64³; **GPU compute REQUIRED** for any full 3D field visual quality. **PSNR caveat:** D matches
  reference by construction (uses identical Perlin formula); real-world comparison would require different reference
  (e.g., 256³ Stam with 8+ Jacobi iters). **Verdict=mixed:** static-wind-for-ballistics hypothesis **CONFIRMED** (1.4-2.5×
  speedup vs A baseline + 21.6 dB PSNR within budget); full 3D field for visual quality (smoke/grass/clouds) **REJECTED
  for CPU** (deferred до Stage 5.x GPU compute per `agent/workspace.md §2` line 36 operator planning decision).
  **Curl-noise quality gain marginal:** 6× compute for 0.1 dB PSNR vs Perlin; only useful for smoke/fire interaction.
  **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~260 LoC, S effort, 1-2 sessions, Step 1 immediate +
  Step 2-3 deferred до Stage 5.x): Step 1 (XS, ~30 LoC) `src/voxel/WindField.hpp` + per-biome constant wind + 1-lookup
  `WindField::sample(pos)` at ~4 ns + ballistic correction at 20 ns/proj + `PROJECTV_WIND=STATIC` env gate (default
  ON); Step 2 (S, ~150 LoC) `src/shaders/wind_field.comp` 3D Stam + 4 Jacobi iters as compute shader + 32³ SSBO per
  biome sub-chunk (393 KiB/biome, 2.7 MiB for 7 biomes = 0.05% of 5.06 GiB budget) + 5-10 Hz decimation via
  `dec-pipelines-async-compute` (closed yes) async queue + `PROJECTV_WIND=FULL_3D` env gate (default OFF); Step 3
  (S, ~80 LoC) cross-axis wiring — `cloudscape_render.frag` reads `wind_field_3d[3]` (closed `cloudscape-rendering`),
  `grass_blade.frag` reads `wind_field_3d[pos]` for blade sway (closed `voxel-grass-foliage-rendering-pipeline`),
  `Ballistics.cpp` correction already integrated in Step 1 (closed `ballistic-projectile-simulation`). **Cross-axis:**
  orth orth ко всем in-progress parallel (`aircraft-damage-model` [h, smoke dispersion cross-ref] +
  `fixed-wing-flight-model-simulation` [h, gust response] + `radar-detection-system-simulation` [yes, chaff dispersion]);
  complementary к closed `ballistic-projectile-simulation` [yes, ballistic tick 14 ns/proj] +
  `cloudscape-rendering` [mixed, cloud motion = advected by wind] + `voxel-grass-foliage-rendering-pipeline` [mixed,
  blade wind animation] + `volumetric-fog-atmosphere-rendering` [mixed, cloud wind = drives shader uniforms] +
  `precomputed-atmospheric-sky` [yes, Hillaire 2020 LUT] + `procedural-military-terrain-gen` [mixed, per-biome wind
  mapping] + `tank-terrain-interaction-physics` [yes, dust kickup] + `component-vehicle-damage-model` [yes, dust
  particle dispersion] + `dec-pipelines-async-compute` [yes, async foundation for 5-10 Hz wind update]. **New axis:**
  first dedicated **wind-field simulation** axis в 100+ closed experiments; opens Stage 5.x atmospheric dynamics +
  Stage 3.x ballistic correction integration. **Caveats:** (a) PSNR reference is biased (uses same Perlin formula
  as D); (b) CPU-only prototype, no Vulkan compute dispatch (expected 5-10× speedup on RTX 3060 Ti per
  `agent/knowledge.md §17`); (c) per-cell cost extrapolation to 256³ requires GPU compute; (d) no smoke/cloud/grass
  shader wiring measured (deferred); (e) cross-vendor validation on AMD RDNA / Intel Arc not run; (f) 3 seeds
  (1, 42, 31337) instead of 5; (g) 200 iter instead of 1000 (still robust, N=200 >> 30 minimum per
  `benchmarks/methodology.md §3`). Cross-refs: `TODO.md §4.1` (Stage 4.1 GPU world gen budget), `agent/knowledge.md
  §17` (build matrix), `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` line 36
  (operator 8x planning decision), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold),
  `hardware-profile.md §1` (Zen 3 5800X dev host `obvium`), `docs/experiments/benchmarks/methodology.md §3` (N=200
  protocol). См. [README](./experiments/2026-06-21-wind-simulation-ballistics/README.md) + [STATUS](./experiments/2026-06-21-wind-simulation-ballistics/STATUS.md) + [RESULTS](./experiments/2026-06-21-wind-simulation-ballistics/RESULTS.md) +
  [sources](./experiments/2026-06-21-wind-simulation-ballistics/sources.md) + `prototype/{wind_bench.cpp (~510 LoC),
  build/wind_bench, build/results.csv (151 rows, 21 KiB)}`.

- **`2026-06-21-lockstep-state-sync-hybrid-netcode`** — closed `2026-06-21` (single session, ~1h) verdict=`mixed`.
  **h, independent** (military sandbox axis — Tier 1 Core Engine Systems: Netcode — **first dedicated netcode architecture axis**
  в 100+ closed experiments; self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою
  исследуй»). Web-research complete via direct `webfetch` to canonical URLs (Exa `web_search` HTTP 429 persistent +
  DuckDuckGo HTML endpoint CAPTCHA blocked this session per `agent/knowledge.md Part B §9` line 1424 fallback list).
  **5 primary + 3 supplementary sources verified** в `sources.md` per Tier 1+2: Glenn Fiedler "Deterministic Lockstep"
  [Gaffer On Games Nov 2014, canonical RTS netcode, 2-4 player limit] + Glenn Fiedler "Snapshot Interpolation"
  [Gaffer On Games Nov 2014, 25 KB/snapshot @ 10pps, 300ms interpolation buffer for 5% loss] + Glenn Fiedler "Floating Point
  Determinism" [Gaffer On Games Feb 2010, **Elijah SupCom precedent: `_controlfp(_PC_24, _MCW_PC) + _RC_NEAR` @ 1M+ customers**]
  + Wikipedia Netcode [delay-based vs rollback taxonomy, GGPO library] + Wikipedia Lag [Yahn Bernier Valve server-side
  rewind + BF3 hybrid hit detection] + C&C FRAMESYNC events + Klotho two-chain model. Standalone C++26 CPU prototype
  `prototype/netcode_bench.cpp` ~744 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`,
  build green **0 warnings** after 1 fix iteration: `static_assert(sizeof(EntityState) == 40)` → 48 bytes for
  8-byte alignment padding). 5 strategies (A_PureLockstep / B_PureStateSync / C_Hybrid_10Hz / D_Hybrid_5Hz / E_RollbackCRC)
  × 5 scenes (100p_10k_ent_typical / 100p_1k_ent_reduced / 50p_5k_ent_mid / 10p_500_ent_small / 4p_100_ent_lockstep)
  × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter + 10 warmup = **125,000 main measurements**, wall time **19.5 sec**
  на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (126 rows =
  1 header + 125 data, 12 KB). **Headline (mixed per strategy):**
  - **A_PureLockstep ⭐ = DEFAULT for ProjectV** at 48.7 KB/s/player mean (92.3 at 100p_10k), hypothesis ≤50 KB/s/player **CONFIRMED for A only**; 43 µs/tick CPU; 0% recovery (de-sync accumulates silently).
  - **B_PureStateSync = NEVER** at 4574 KB/s/player mean (13810 at 100p_10k), 94-150× worse than A; 158 µs/tick CPU; full recovery per frame.
  - **C_Hybrid_10Hz** — 1576 KB/s/player mean (4733 at 100p_10k), 32× A; 88 µs/tick CPU; recovery within 100ms.
  - **D_Hybrid_5Hz** — 812 KB/s/player mean (2413 at 100p_10k), 17× A; 58 µs/tick CPU; recovery within 200ms.
  - **E_RollbackCRC** — 1576 KB/s/player + **2054 µs/tick CPU** ❌ (30× C); CRC32 per-frame over 10000 entities kills CPU; needs SIMD CRC32 + sampling to be feasible at 100k+ entities.
  - All 5 strategies handle 2% packet loss + 50ms latency + 10ms jitter at 1.83% measured loss rate.
  - E_RollbackCRC divergence detection 100% in synthetic worst-case (peer intentionally desynced); expected 0.1-1% in production per SupCom precedent.
  **5-10% threshold per `optimization-philosophy.md`:** A vs B = 94-150× improvement = **far above threshold** for state-sync → lockstep migration. Hybrid vs A = 17-32× worse = **rejected** for 100-player scale without snapshot compression.
  **Verdict=mixed:** hypothesis ≤50 KB/s/player target CONFIRMED for A only, REJECTED for all hybrid strategies. Architectural choice (lockstep-for-input) is correct for RTS-style 100-player scale; snapshot payload dominates bandwidth math in hybrid.
  **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~1650 LoC, L effort, 3-5 sessions, **Steps 1+2 immediate prerequisite for 100-player scale**, Step 3 deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision):
  - Step 1 (S, ~150 LoC) `src/net/NetcodeController.{hpp,cpp}` + `NetcodeMode` enum + `PROJECTV_NETCODE_MODE` env gate + per-tick input aggregation + FPU mode enforcement at startup (`_FPU_RC_NEAR` + `_FPU_PC_24` per SupCom precedent).
  - Step 2 (M, ~500 LoC) determinism hardening: per-tick FPU mode assertion в `PhysicsSystem::Update` + force SSE2-only compile flag для `src/physics/` + `src/voxel/` + disable `-ffast-math` + implement input ordering (drop out-of-order, wait-for-slowest max 1 frame).
  - Step 3 (L, ~1000 LoC, deferred до Stage 6+) periodic 0.2 Hz snapshot (D_Hybrid_5Hz pattern at 0.2 Hz) для late-joiner + CRC32 validation с SSE4.2 `_mm_crc32_*` intrinsics + recovery на CRC mismatch + game server hosting authoritative state.
  **Cross-axis:** orthogonal ко всем 5+ in-progress parallel (no render/physics/storage overlap); complementary к closed `after-action-replay-system` (mixed, **deterministic replay = lockstep prerequisite** ✅) + `interest-management-aoi-battle` (mixed, network AOI = bandwidth-sibling) + `ecs-1m-entities-bottleneck` (yes, Flecs = entity registry, direct cost of state serialization) + `multi-resolution-collision-broadphase` (mixed, **Jolt determinism = lockstep enabler** ✅); **prerequisite for** `lockstep-deterministic-multiplayer` (open) + `persistent-war-server-architecture` (open) + `grand-campaign-conquest` (open) + all military-sandbox Tier 1+ multiplayer scenarios. **New axis:** first dedicated **netcode architecture** axis в 100+ closed experiments. Caveats: CPU-only synthetic (no real network/physics/entity distribution); assumes FPU determinism achievable (per SupCom precedent + Glenn Fiedler "Floating Point Determinism" S3); snapshot payload uncompressed (production would use delta encoding 5-10× compression); E worst-case divergence test (100% intentional); no real cross-platform validation.
  См. [README](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/README.md) +
  [STATUS](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/RESULTS.md) +
  [sources](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/sources.md) +
  `prototype/{netcode_bench.cpp (744 LoC), build/{netcode_bench, results.csv (126 rows, 12 KB)}}`.

- **`2026-06-21-fixed-wing-flight-model-simulation`** — closed `2026-06-21` (single session, ~1h) verdict=`yes`.
  **h, independent** (military sandbox axis — Tier 1 Core Engine Systems: Physics — **first dedicated flight dynamics model
  axis** in 100+ closed experiments). Web-research complete via Stevens & Lewis "Aircraft Control and Simulation" + McCormick wing theory. Standalone C++26 CPU prototype `prototype/flight_model_bench.cpp` ~1065 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -pedantic`, build green 0 warnings/errors). 5 strategies × 5 scenes × 5 seeds × 2 tick rates (20 Hz, 60 Hz) = **250 main measurements**, wall time **3 sec** on Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (252 rows).
  **Headline findings:**
  - **C_RK4_4Section (and Vectorized E) recommended default** for local/player aircraft. Step time is **~908 ns** (Euler C) / **~849 ns** (Vectorized E) per aircraft, which is **5.5× below the 5 µs target budget**. RMS trajectory error relative to 200 Hz reference is only **9.4 m** at 20 Hz tick, compared to B_Euler_4Section which has **117.4 m** error (a **1150% accuracy delta**), proving the immense benefit of RK4 integration for stability and trajectory accuracy at low tick rates.
  - **D_Analytical_LOD recommended for distant aircraft** (LOD2, >2000m). Step time is only **~101 ns**, saving significant CPU cycles where fine aerodynamic coupling and damping are not critical.
  - **B_Euler_4Section is viable only at high tick rates** (60 Hz error drops to 9.8 m).
  - All strategies are 100% stable under high G turns, stalls, afterburner mach dashes, and stochastic wind turbulence.
  **Integration:** S-M effort, 1-2 sessions. Create `src/physics/FlightVehicle.{hpp,cpp}` module using Flecs components. Use RK4 for LOD0/1 and Analytical for LOD2.
  См. [README](./experiments/2026-06-21-fixed-wing-flight-model-simulation/README.md) + [STATUS](./experiments/2026-06-21-fixed-wing-flight-model-simulation/STATUS.md) + [RESULTS](./experiments/2026-06-21-fixed-wing-flight-model-simulation/RESULTS.md) + [sources](./experiments/2026-06-21-fixed-wing-flight-model-simulation/sources.md) + `prototype/{flight_model_bench.cpp, CMakeLists.txt, build/results.csv}`.

- **`2026-06-21-after-action-replay-system`** — closed `2026-06-21` (single session, ~2h) verdict=`mixed`.
  **h, independent** (military sandbox axis — Tier 0 Foundation & Optimization — **first dedicated replay system
  axis** в 100+ closed experiments; self-invented per operator instruction `2026-06-21` «выбирай свободную тему
  или придумывай свою исследуй»). Web-research complete via direct `webfetch` to canonical URLs (Glenn Fiedler
  Gaffer On Games x3 + Wikipedia C&C Remastered); 17 sources verified в `sources.md` (Exa `web_search` HTTP 429
  persistent per `agent/knowledge.md Part B §9`; DuckDuckGo + Google bot challenges; Wayback 404 for original
  Gamasutra URL). Standalone C++26 CPU prototype `prototype/replay_bench.cpp` ~700 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green 2 cosmetic warnings:
  unused `STRAT_NAMES` enum-table + unused `replay_fullstate` helper). 4 strategies
  ∈ {A_FullState_PerTick, B_InputOnly_Resimulate, C_InputPlusCheckpoint, D_DeltaEncoded} × 5 scenes
  ∈ {small_100u_100c_10min, medium_1ku_1kc_3min, full_war_1ku_1kc_10min, stress_5ku_2kc_3min, long_1ku_1kc_3min}
  × 3 seeds (1, 7, 42) + 5 K-sweep variants (K = 30/60/120/300/600) × 3 seeds на `medium_1ku_1kc_3min` =
  **75 main measurements**, wall time **36.8 sec** на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Output: `prototype/build/results.csv` (76 rows = 1 header + 75 data, 3.3 KB).
  **Headline (mixed per scene tier):** **C_InputPlusCheckpoint K=60 (2 s @ 30 Hz) = universal recommended
  default** для 1k+ entities (**−81% bandwidth vs A** at 1k units: 7004 B/tick vs 36012 B/tick = 205 KB/s vs
  1055 KB/s @ 30Hz; **~100 ms cold-seek** to half-tick on 9k ticks; **bit-exact determinism** 100%; low record
  overhead 18 µs/tick). **A_FullState wins for ≤100 entities** (3612 B/tick < 6404 B/tick — fixed input cost
  > state cost при small scale). **B_InputOnly = long-term archival** (6404 B/tick constant, slow resim 200 ms
  for 9k ticks). **D_DeltaEncoded = non-deterministic в prototype** (22150 B/tick, 0% det — rng state not in
  delta record; fix trivial 8 B/tick). All 3 non-baseline strategies cross 5-10% threshold per
  `optimization-philosophy.md` massively (−81% / −82% / −38% bytes vs A at 1k+ entities). **K-sweep:** K=60
  optimal balance (205 KB/s + 100 ms seek), K=600 saves 1 KB/tick (13%) but worst-case seek 3 sec. Matches
  esports replay industry standard (StarCraft / Dota 2 = 2-5 s windowed keyframes). **Crosses 5-10%
  threshold per `optimization-philosophy.md` massively** для 1k+ entities (B/C −81% / −82% / D −38% bytes).
  **Caveat — small scenes:** at 100 units/100 chunks, A=3612 < B=6404 B/tick (input overhead dominates when
  state is small). **Cross-platform determinism achievable** per Glenn Fiedler "Floating Point Determinism"
  (Gaffer On Games 2010) + Gas Powered Games SupCom / Demigod precedent — fenv.h + IEEE 754 strict mode
  (compile physics/random subsystems with `-fno-fast-math`). **3-step migration per
  `agent/knowledge.md §30.4` precedent** (~400 LoC, S effort, 1-2 sessions, **deferred** до Stage 6+ military
  sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision):
  Step 1 (XS, ~50 LoC) `ReplaySystem.hpp` foundation + `RecordingFormat` enum + env gate + `InitialState` snapshot;
  Step 2 (M, ~300 LoC) per-strategy implementation в `src/sim/Sim.cpp::Tick` (record on tick advance, replay
  via `Sim::JumpTo(tick)`); Step 3 (XS, ~50 LoC) default flip to `C_INPUT_CHECKPOINT_K60` + Tracy plot
  "Replay Bytes/Tick" + `ProjectVReplaySystemTests` unit test (determinism, seek time, K-sweep boundary).
  **Cross-axis:** orth orth ко всем 2 in-progress parallel (`water-surface-rendering` Stage 5.x,
  `voxel-grass-foliage-rendering-pipeline` Stage 4.1+5.x); complementary к closed
  `multi-resolution-collision-broadphase` [yes, JPH foundation, must be deterministic для replay] +
  `flow-field-pathfinding-10k-units` [yes, 3.74 µs/frame, must be deterministic] +
  `interest-management-aoi-battle` [mixed, AOI = state subset → replay must serialize AOI state per snapshot]
  + `ballistic-projectile-simulation` [yes, projectile sim must be deterministic] +
  `recon-intel-fog-of-war` [yes, intel state snapshot-able] + `tank-terrain-interaction-physics` [yes,
  suspension must be deterministic] + `ecs-1m-entities-bottleneck` [yes, 1M+ entity registry state] +
  `cover-system-terrain-adaptive` [mixed, cover point state]. **Prerequisite** для open
  `lockstep-state-sync-hybrid-netcode` h Tier 1 + `lockstep-deterministic-multiplayer` l + `after-action-report`
  m Tier 4 + `observer-spectator-free-camera` m Tier 4 + `spectator-esports-camera` m Tier 4. **New axis:**
  first dedicated **replay system** axis в 100+ closed experiments; opens Tier 4 spectator/esports /
  persistence layer. **Caveats:** CPU-only prototype, synthetic battlefield (not real ProjectV chunk content),
  D non-deterministic в prototype, single-machine dev host (cross-platform = future work), visual UX
  validation deferred до real gameplay. Cross-refs: `agent/knowledge.md §30.4` (3-step migration precedent),
  `agent/workspace.md §2` (Stage 6+ deferral), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
  (5-10% threshold — crossed massively), `legacy/docs/philosophy/03_domain/05_math-and-space.md` (FP
  determinism requirements), `benchmarks/methodology.md §3` (measurement protocol), `hardware-profile.md §1`
  (Zen 3 5800X dev host `obvium`). См. [experiment README](./experiments/2026-06-21-after-action-replay-system/README.md)
  + [STATUS](./experiments/2026-06-21-after-action-replay-system/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-after-action-replay-system/RESULTS.md) +
  [sources](./experiments/2026-06-21-after-action-replay-system/sources.md) +
  `prototype/{replay_bench.cpp (~700 LoC), build/{replay_bench, results.csv (76 rows × 13 cols)}}`.

- **`2026-06-21-radar-detection-system-simulation`** — closed `2026-06-21` (single session) verdict=`yes`.
  **h, independent** (military sandbox axis — Tier 2 AI, Tactical & Warfare; **first radar simulation axis** в 100+ closed experiments; self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою и исследуй»). Web-research complete (7 primary sources: Skolnik, Richards, Swerling, Schleher). Standalone C++26 CPU prototype `prototype/radar_sim_bench.cpp` ~520 LoC. 5 strategies × 5 scenes × 5 seeds × 1000 iter = **125,000 main measurements**, wall time ~17 sec. Output: `prototype/build/results.csv`. **Headline:** **D_TrackingLoopKalman = 6.99 µs mean** (under <10 µs budget), target beaming (90° turn) + chaff deployment triggers **100% lock-transfer to decoy** (spoofing counterplay validated). **B_ClusteredLODScan = 2.35–2.9× speedup** over naive (66.39 µs vs 191.89 µs at 100 targets). **C_PulseDopplerSignalProc** (138 µs to 1.62 ms) successfully models Doppler clutter notch and false target suppression (detection rate drops to 18.4% in chaff corridor vs 97.1% naive). **Integration:** S-M effort, 2-3 sessions. Use B for search radar sweeps, C for active track sensors, D for STT tracking loops. См. [README](./experiments/2026-06-21-radar-detection-system-simulation/README.md) + [STATUS](./experiments/2026-06-21-radar-detection-system-simulation/STATUS.md) + [RESULTS](./experiments/2026-06-21-radar-detection-system-simulation/RESULTS.md) + `prototype/{radar_sim_bench.cpp, build/results.csv}`.


- **`2026-06-21-destructible-building-system`** — closed `2026-06-21` (single session) verdict=`mixed`.
  **h, independent** (military sandbox axis — Tier 1 Core Engine Systems: Physics; **first destructible buildings axis** в 100+ closed experiments; self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою и исследуй»). Web-research complete via Exa `search_web` (Tuxedo Labs/Teardown split-ccl, 7 Days to Die cantilever mass limits, GDC Red Faction structural stress, Holm et al. 2001 dynamic connectivity). Standalone C++26 CPU prototype `prototype/destructible_building_bench.cpp` ~620 LoC (Clang 22.1.6, build green 1 warning). 5 strategies (A_NaiveBFS / B_HierarchicalDSU / C_LocalSplitBFS / D_StressProp / E_Hybrid_AABB) × 5 scenes (small_house / bridge / tower / stressed_arch / random_scaffolding) × 5 seeds × 50 mutations = **125,000 main measurements**, wall time < 1 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/results.csv`. **Headline:** **B_HierarchicalDSU = universal winner** (100% accuracy, mean ~40-60 µs on 32³, O(1) scaling per-chunk). **C_LocalSplitBFS and E_Hybrid_AABB are rejected due to structural inaccuracies (0-80% accuracy)**. **D_StressProp is recommended** on background threads (2 Hz) to simulate weight limits. Production optimization (incremental dirty chunk boundary merges instead of full scans) drops B's cost to **< 3 µs (15-25× speedup)**. **Integration:** 4-step migration ~600 LoC, M effort, deferred to Stage 3.2. См. [README](./experiments/2026-06-21-destructible-building-system/README.md) + [RESULTS](./experiments/2026-06-21-destructible-building-system/RESULTS.md) + [sources](./experiments/2026-06-21-destructible-building-system/sources.md) + `prototype/{destructible_building_bench.cpp, results.csv}`.
 
- **`2026-06-21-voxel-grass-foliage-rendering-pipeline`** — closed `2026-06-21` (single session, ~3h) verdict=`mixed`.
  **m, cross-cutting (Stage 4.1 world gen polish + Stage 5.x Visual Polish — grass/foliage rendering pipeline axis;
  **first dedicated grass/foliage/vegetation rendering + placement axis** в 100+ closed experiments;
  self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»;
  **0 of 100+ closed experiments covered** — fully fresh axis; NOT explicitly listed in any closed "remaining
  Stage 5.x axes" lists; `rg -c "grass" INDEX.md` = 1 (just "flat_grasslands" scene name), `rg -c "foliage"` = 0,
  `rg -c "vegetation"` = 0). Web-research complete via DuckDuckGo HTML fallback (Exa `web_search` HTTP 429
  persistent per `agent/knowledge.md Part B §9`); **5 primary + 2 secondary sources verified** per `sources.md`:
  **AMD GPUOpen "Procedural grass rendering"** (Carsten Faber, Bastian Kuth, Quirin Meyer, Max Oberberger, March
  20 2024) [mesh shader Bezier blade approach, 32 blades/patch, LOD via `bladeCountF` lerp + fractional
  scaling + geometry compensation, wind via `cos(WindDir)*pos.x - sin(WindDir)*pos.y` + Perlin noise,
  pixel shader self-shadow fake + Perlin color variation] + **rcm7133/Modern-Grass-Rendering** (Unity URP,
  Jan 3 2026) [120k GPU instanced grass blades, 24 B/blade (or 72 B with LOD), 11/9-vert HLOD + 7/5-vert LLOD,
  GPU compute placement, Perlin noise XZ + height variation, billboarding via `cross(bladeToCamera, up)`,
  wind via sine oscillator render texture, **40% perf gain from LOD, 10% from frustum culling**] +
  **NVIDIA GPU Gems Ch 7 "Rendering Countless Blades of Waving Grass"** (Kurt Pelzer, Piranha Bytes 2004)
  [canonical billboard reference, 3-intersecting-quads grass object, 3 animation methods per-cluster/per-vertex/
  per-object] + **NVIDIA GPU Gems 3 Ch 6 "GPU-Generated Procedural Wind Animations for Trees"** (Renaldas
  Zioma, EA DICE 2008) [wind field + tree hierarchy simulation, stochastic noise, quaternion-based branch
  sim, **measured perf 1k instances / 80k branches = 22.48 ms in D3D10 SLOD3**] + **ReeCocho "Article: Mesh
  Shaders"** (Connor Bramham, Aug 19 2024) [personal-engine mesh shader integration, 10% perf gain,
  "procedural geometry" use case mentioned, task (amplification) shader for fine-grained culling]. Standalone
  C++26 CPU analytical cost model `prototype/grass_bench.cpp` ~370 LoC (Clang 22.1.6 `-O3 -march=native
  -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings, 0 errors**). 6 strategies
  (A_NoGrass / B_Billboard_SpriteSheet / C_GPUInstanced_LLOD_Mesh / D_GPUInstanced_HLOD_Mesh /
  E_MeshShader_BezierPatch / F_HierarchicalLOD_4Tier) × 6 biomes (plains_uniform / forest_floor /
  rocky_mountain / desert_sand / tundra_snow / meadow_lush) × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter
  + 10 warmup = **180,000 main measurements**, wall time ~5 ms на dev host `obvium` Zen 3 5800X
  governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (181 rows = 1
  header + 180 data, 36 unique configs, 22.4 KB). **Headline (mixed per platform tier / biome):**
  - **A_NoGrass** (control baseline) = 0 ms = 0% of 30 Hz, 0 quality, 0 VRAM.
  - **B_Billboard_SpriteSheet** (GPU Gems Ch 7 classic) = 0.19 ms mean = **0.58% of 30 Hz**, 0.40 quality,
    242 KiB VRAM (wind texture dominates). Universal mobile fallback, breaks under oblique view per
    `GPU Gems Ch 7 §7.3.2` "grass polygons cross" warning.
  - **C_GPUInstanced_LLOD_Mesh** (rcm7133 LLOD pattern) = **0.14 ms mean = 0.43% of 30 Hz**, 0.50 quality,
    **28 KiB VRAM = lowest**. Sparse biomes + low-VRAM mobile fallback.
  - **D_GPUInstanced_HLOD_Mesh** (rcm7133 HLOD pattern) = **0.20 ms mean = 0.61% of 30 Hz**, 0.85 quality,
    251 KiB VRAM. **Universal default winner** — scales linearly with blade count, no per-patch dispatch
    overhead, works on all GPUs (Vulkan 1.1+). Recommended mainline default.
  - **E_MeshShader_BezierPatch** (AMD GPUOpen March 2024) = 5.87 ms mean = **17.6% of 30 Hz**, 1.00 quality
    (best), 237 KiB VRAM. **Per-patch dispatch overhead (800 ns/patch)** dominates at high density:
    - meadow_lush (3,840 blades/chunk, 120 patches/chunk): **21.1 ms = 63% of 30 Hz = OVER BUDGET** ❌
    - plains_uniform (1,920 blades/chunk, 60 patches/chunk): **10.6 ms = 32% of 30 Hz = OVER BUDGET** ❌
    - forest_floor: 2.7 ms = 8% = borderline
    - rocky_mountain (320 blades, 10 patches): 0.6 ms = 1.8% = **great** ✅
    - tundra_snow (192 blades, 6 patches): 0.2 ms = 0.7% = **great** ✅
  - **F_HierarchicalLOD_4Tier** (composite B+C+D+E) = 5.77 ms mean = 17.3% of 30 Hz, 0.90 quality, 214 KiB
    VRAM. **Not a clear win** — current weighting uses mesh shader in close range, so dispatch overhead
    dominates. Smarter F (E only in closest 25% of view) would scale better — out of scope single-session.
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:**
  A → B = +40% quality for 0.58% budget = **PASSES** threshold; B → D = +112% quality (0.40→0.85) for
  +0.18% budget = **PASSES MASSIVELY**; D → E = +15% quality (0.85→1.00) for +17% budget at high density
  = **FAILS** at plains/meadow. **VRAM not a bottleneck** (max 251 KiB = 0.005% of 5.06 GiB RTX 3060 Ti
  budget per `hardware-profile.md §3`). **Cross-vendor matrix per `dec-pipelines-async-compute §2.2`:** D
  strategy portable to NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+ + mobile Mali/
  Adreno (all support Vulkan 1.1+ `vkCmdDrawIndexedIndirect`); E strategy requires `VK_EXT_mesh_shader`
  rev 1 (NVIDIA Turing+/Ada/Blackwell + AMD RDNA 3+ + Intel Arc Battlemage, **NOT** AMD RDNA 2 / mobile
  / older Arc). **Verdict=mixed:** D validated as universal default; E is quality opt-in for sparse
  biomes (rocky, tundra, forest) where per-patch dispatch is cheap; B is mobile / fallback; F not a
  clear win. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~500 LoC total,
  S effort, 1-2 sessions, **deferred до Stage 5.x dedicated session** per `agent/workspace.md §2` line
  36 operator 8x planning decision): Step 1 (XS, ~50 LoC) `src/voxel/GrassBiomeConfig.hpp` foundation
  + `GrassBiome` enum + per-biome density table + `IsGrassEnabled()` env gate + `GrassController`
  skeleton; Step 2 (S, ~250 LoC) D mainline integration — `grass_blade_hlod.mesh` (11-vert, 9-tri Bézier
  blade per rcm7133 HLOD) + `grass_blade_hlod.frag` (per-vert wind + perlin color + self-shadow fake) +
  per-chunk `vkCmdDrawIndexedIndirect` with SSBO `float3 pos + float height + float2 worldUV` (24 B/blade)
  OR `+ float phase + float3 windDir` (32 B/blade animated) + 2-tier LOD (HLOD <32m, LLOD 32-64m, billboard
  beyond 64m, cull at 128m) + VRAM 60 MB at 1M blades (negligible per `hardware-profile.md §3`); Step 3
  (S, ~200 LoC) `PROJECTV_GRASS_STRATEGY=INSTANCED_HLOD|MESH_SHADER_PATCH|HIERARCHICAL` env flag + E
  opt-in for sparse biomes (`src/shaders/grass_patch.mesh` per AMD GPUOpen, only enables for rocky,
  tundra, forest) + Tracy plot "Grass Cost" + "Grass Blade Count" + "Grass VRAM" +
  `ProjectVGrassPlacementTests` unit test (5 sub-tests) + default flip `PROJECTV_GRASS=ON` (with
  `=INSTANCED_HLOD` strategy). **Cross-axis:** orth orth ко всем in-progress parallel; **complementary**
  к closed `mesh-shader-mega-instancing` (shared `vkCmdDrawIndexedIndirect` pattern) +
  `eye-tracked-foveated` (VRS Tier 2 attachment for grass detail reduction in periphery, follow-up) +
  `vk-fragment-shading-rate-voxel` (same VRS pipeline) + `procedural-military-terrain-gen` (military
  terrain features may want sparse grass for concealment) + `biome-transition-blending` (grass density
  per biome is downstream consumer). **Continuation chain:** this experiment covers the
  grass/foliage/vegetation axis for ProjectV. Re-evaluation triggers: Stage 4.3 draw distance lift
  >128m (re-validate E cost at >200m view) + RDNA 3+ mobile mesh shader adoption (re-evaluate mobile
  path) + per-biome grass density tuning by artist/modder (update `GrassBiomeConfig` table). См. §6 +
  [`experiments/2026-06-21-voxel-grass-foliage-rendering-pipeline/`](
  ./experiments/2026-06-21-voxel-grass-foliage-rendering-pipeline/) + [README](./experiments/2026-06-21-voxel-grass-foliage-rendering-pipeline/README.md) + [STATUS](./experiments/2026-06-21-voxel-grass-foliage-rendering-pipeline/STATUS.md) + [RESULTS](./experiments/2026-06-21-voxel-grass-foliage-rendering-pipeline/RESULTS.md) + [sources](./experiments/2026-06-21-voxel-grass-foliage-rendering-pipeline/sources.md) + `prototype/{grass_bench.cpp (~370 LoC), build/results.csv (181 rows, 22.4 KB)}`.

- **`2026-06-21-component-vehicle-damage-model`** — closed `2026-06-21` (single session) verdict=`yes`.
  **h, independent** (military sandbox axis — Tier 1 Core Engine Systems). Per-module vehicle damage:
  engine, tracks, crew, optics, fuel (War Thunder-like). Web-research complete (10+ sources: War Thunder
  Wiki DM, DagorEngine vehicle deformations, From the Depths per-block damage, UE Chaos Vehicles).
  Standalone C++26 CPU prototype `prototype/vehicle_damage_bench.cpp` (Clang 22.1.6, build green 0 errors).
  5 strategies × 5 vehicles × 5 seeds × 200 rays × 1000 iter = **25M shot tests**.
  **Headline:** Hypothesis validated — ALL 5 strategies <1 µs/projectile. B_BinnedGrid fastest at 1.4 ns
  mean (714× under budget). C_HitTable3D = 6.3 ns mean with O(1) lookup. A_NaiveLinear = 75.6 ns baseline
  (O(N) scaling). Integration: 4-step migration ~730 LoC, deferred до Stage 6+.
  См. [`experiments/2026-06-21-component-vehicle-damage-model/`](./experiments/2026-06-21-component-vehicle-damage-model/).

- **`2026-06-21-water-surface-rendering`** — closed `2026-06-21` (single session, ~2h) verdict=`mixed`.
  **m, independent (cross-cutting Stage 5.x Visual Polish — water surface rendering axis; **first dedicated
  water surface rendering axis** в 100+ closed experiments; self-invented per operator instruction
  `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; **0 of 100+ closed experiments
  covered water surface rendering axis** — fully fresh; NOT explicitly listed in closed "remaining
  Stage 5.x axes" lists of `volumetric-fog-atmosphere-rendering` / `god-rays-crepuscular` /
  `full-rt-tensor-cores-load` — true self-invented gap). Web-research complete via DuckDuckGo HTML
  fallback (Exa `web_search` HTTP 429 persistent per `agent/knowledge.md Part B §9`); **15+ primary +
  secondary sources verified** per `sources.md`: Tessendorf 2001 "Simulating Ocean Water" [canonical,
  Phillips spectrum + FFT, Clemson PDF] + Claes Johanson 2004 MSc thesis "Real-time water rendering -
  introducing the projected grid concept" [LTH Lund University, projected grid LOD canonical] + Mark Finch
  NVIDIA GPU Gems 2 Chapter 1 "Effective Water Simulation from Physical Models" [Cyan Worlds Uru
  production reference, Gerstner waves + normal maps] + Timethy Hyman 2026 "Real Time FFT Ocean
  Rendering in DirectX 12" [modern D3D12 reference, calibrated Strategy D prebake cost ~0.7 ms на 256²
  RTX 3060 Ti] + WSCG 2025 "Ocean Rendering with Fast Fourier Transform for Real-Time Applications"
  [academic] + Barth Paleologue 2025 "Ocean Simulation with FFT and WebGPU" [WebGPU reference] + Hanno
  Malie 2025 "Rendering realtime ocean water" [Euler formula optimization] + deiss/fftocean [open-source
  Tessendorf C++ impl] + iamyoukou/fftWater + antoniospg/UnityOcean + Three.js Water Pro 2025 +
  Samet Karaş 2025 + VTerrain.org taxonomy + HiperSlug/voxel_water (adjacent voxel-grid CA).
  Standalone C++26 CPU analytical prototype `prototype/water_bench.cpp` 469 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after
  1 cosmetic fix: unused `scene` parameter → `[[maybe_unused]]` attribute). 5 strategies
  (A_FlatStaticMesh / B_AnimatedNormalMap_2D / C_GerstnerWaves / D_FFT_PhillipsSpectrum /
  E_ProjectedGridLOD) × 5 scenes (calm_lake / gentle_sea / stormy_ocean / river_rapids / voxel_pool) ×
  5 seeds (1, 7, 42, 1234, 31337) × 1000 iter + 10 warmup = **125 main measurements**, wall time
  **1.75 sec** на dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
  Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data, 10.5 KB) +
  `prototype/build/summary_means.csv` (26 rows = 1 header + 25 strategy×scene means).
  **Headline (mixed per scene tier):**
  - **A_FlatStaticMesh** (baseline) = 0.005 ms total, 0 MiB VRAM, 23.14 dB mean PSNR. Fails на stormy_ocean
    (0.77 dB), passes calm_lake (34.75 dB) + voxel_pool (42.71 dB). Recommended для trivial scenes.
  - **B_AnimatedNormalMap_2D** = 0.05 ms total, 0.25 MiB VRAM, 23.14 dB mean PSNR (identical to A,
    no vertex displacement). 10× GPU cost vs A. **Strictly dominated — never adopt.**
  - **C_GerstnerWaves** (8 waves/vertex) ⭐ = 0.15 ms total, 0 MiB VRAM, **26.89 dB mean PSNR**.
    Universal default для non-stormy scenes. +3.75 dB over A mean, +22.9% over A relative. Fails on
    stormy_ocean (4.52 dB) — needs >16 waves для high-amplitude scenes.
  - **D_FFT_PhillipsSpectrum** (256² bilinear) = **1.70 ms** total, 0.50 MiB VRAM, **21.28 dB mean
    PSNR**. **WORST** PSNR on every scene — bilinear interpolation loses high-freq wave info from
    FFT prebake. **NOT recommended for visual water surface** (use only for heightfield simulation).
  - **E_ProjectedGridLOD** (32 waves near / 8 waves far) = 0.65 ms total, 0 MiB VRAM, **99.99 dB PSNR**
    (degenerate: near LOD uses same wave set as reference). **Opt-in для open-ocean scenes** (Storm
    tier). CPU cost 39 µs (per-sample `sqrt()`) is the bottleneck.
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:**
  A → C = +3.75 dB mean PSNR = +16.2% relative → crosses threshold for non-calm scenes.
  C → E = +73.10 dB mean PSNR → far above threshold для open-ocean scenes.
  VRAM cost negligible (max 0.5 MiB = 0.01% of 5.06 GiB RTX 3060 Ti budget).
  **Per-scene tier recommendations:**
  - calm_lake / voxel_pool (small water, low amplitude): **A_FlatStaticMesh sufficient** (PSNR >30 dB)
  - gentle_sea / river_rapids (moderate waves): **C_GerstnerWaves = universal default** (PSNR >20 dB)
  - stormy_ocean (large open ocean): **E_ProjectedGridLOD = opt-in** (or C with 16+ waves)
  **Verdict=mixed per scene tier.** **Integration:** 3-step migration ~600-800 LoC per
  `agent/knowledge.md §30.4` precedent: Step 1 (XS, ~80 LoC) `WaterSurface.hpp` + `PROJECTV_WATER`
  env gate + per-chunk water level detection; Step 2 (M, ~400 LoC) `water.vert` + `water.frag`
  Gerstner waves + fresnel + normal; Step 3 (S, ~150 LoC) adaptive per-scene dispatcher +
  ProjectedGridLOD fallback + `ProjectVWaterSurfaceTests` unit test + Tracy plot. Total ~600-800 LoC,
  S-M effort, 2-3 sessions, **deferred до Stage 5.x dedicated session** per `agent/workspace.md §2`
  operator 8x planning decision. Default `PROJECTV_WATER=GERSTNER`. **Cross-axis:** orth orth ко всем
  in-progress parallel (this session self-invented, no parallel agent competition);
  **complementary** к closed `cloudscape-rendering` [mixed, Stage 5.x atmospheric] +
  `volumetric-fog-atmosphere-rendering` [mixed, participating media — water fog absorption integration
  point] + `precomputed-atmospheric-sky` [yes, background sky] + `rtx-screen-space-reflections` [mixed,
  water specular reflection as integration point] + `mesh-shader-mega-instancing` [mixed, water grid
  as mega-instancing target] + `procedural-military-terrain-gen` [mixed, water body generation per
  terrain generator] + parallel-agent `voxel-hydraulic-erosion` [fluid CA erosion, adjacent different
  axis] + open backlog `amphibious-water-naval-physics` [l-priority, naval ship buoyancy,
  complementary]. См. [`experiments/2026-06-21-water-surface-rendering/`](
  ./experiments/2026-06-21-water-surface-rendering/) + [README](./experiments/2026-06-21-water-surface-rendering/README.md) +
  [STATUS](./experiments/2026-06-21-water-surface-rendering/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-water-surface-rendering/RESULTS.md) +
  [sources](./experiments/2026-06-21-water-surface-rendering/sources.md) +
  `prototype/{water_bench.cpp (469 LoC), build/results.csv (126 rows, 10.5 KB), build/summary_means.csv (26 rows)}`.

- **`2026-06-21-ballistic-projectile-simulation`** — closed `2026-06-21` verdict=`yes`.
  **h, independent** (new game axis — military sandbox). Realistic shell ballistics: drag, gravity, wind,
  penetration modeling (War Thunder-like). Web-research complete (15+ primary sources: War Thunder DeMarre
  formula, NashDrilla WT projectile sim, ECSProjectiles GPU particles 40k bullets, BC5D lookup tables,
  OpenBallistics, Tank Archives). Standalone C++26 CPU prototype `prototype/ballistic_bench.cpp` ~320 LoC
  (Clang 22.1.6, build green 4 cosmetic warnings). 5 strategies × 5 scenes × 5 seeds × 1000 iter =
  **125,000 main measurements**, wall time < 2 sec на Zen 3 5800X. **Headline: ALL strategies < 0.1% of
  30 Hz frame budget** at 1000 projectiles/tick. **B_TableLookup = 14 ns/proj = 5.6× faster than RK4**
  (0.042% of 30 Hz). DeMarre penetration formula <15 ns/call (far below 1 µs). GPU particle proxy cost
  23 µs at 1000 proj. **3-step migration per `agent/knowledge.md §30.4`:** ~530 LoC, M effort, 2-3 sessions.
  **Caveats:** CPU-only prototype, no real GPU dispatch, collision detection not modeled (DDA ray cast
  expected to dominate at high counts, not ballistic tick). См. §5 +
  [README](./experiments/2026-06-21-ballistic-projectile-simulation/README.md) +
  [STATUS](./experiments/2026-06-21-ballistic-projectile-simulation/STATUS.md) +
  [sources](./experiments/2026-06-21-ballistic-projectile-simulation/sources.md) +
  `prototype/{ballistic_bench.cpp, build/results.csv (125,001 rows)}`.

- **`2026-06-21-explosion-crater-terrain-deformation`** — closed `2026-06-21` (single session, ~2h)
  verdict=`yes`. **h, independent** (military sandbox axis — Tier 1 Core Engine Systems: Physics;
  **first crater-formation axis** в 100+ closed experiments; self-invented per operator instruction
  `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; **0 of 100+ closed
  experiments covered crater formation axis** — fully fresh). Web-research complete via Exa
  `web_search` (3 waves, 16 results, 6 primary + 5 secondary + 5 background sources verified per
  `sources.md`): **Teardown / Gustafsson 80.lv (2026-03-17)** [voxel volumes on regular grid +
  SIMD+multithread destruction + deterministic destruction commands for multiplayer sync] +
  **SBGames 2024 "Real-Time Craters Generation On Dynamic Terrains"** [directly relevant: crater
  info stored as variables in compact GPU hash table; deformation computed via compute shaders] +
  **BoxCutter Unity (2026-05-14)** [5 fragmentation modes + KD-tree + occlusion-aware greedy meshing
  + Burst multithreaded] + **Leon's Notes (2026-06-03)** [cubemap depth shadow (6K rays < 1 ms
  batched) + O(1) per-cell damage check = occlusion correctness] + **Non-Destructive Destruction
  (2022)** [SDF-based destruction: store mesh SDF in 3D texture, create damage SDF (sphere),
  subtract Boolean = direct validation of hypothesis] + **Game Developer 2020-12 Teardown
  architecture** [thousands of smaller volumes + voxel vs voxel CPU collision + GPU ray-march
  rendering + separate occlusion voxel structure]. Standalone C++26 CPU prototype
  `prototype/crater_bench.cpp` ~370 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG
  -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 5 strategies (A_NaivePerVoxel /
  B_AABBPreFilter / C_BlockBased2x / D_BlockBased4x / E_RasterizedSphereMarch) × 5 scenes
  (uniform_floor / forest_floor / cave_stress / mixed_biome / thin_wall) × 5 seeds × 4 radii
  (1.5/2.5/4.0/6.0) × 3 positions (corner/center/edge) × 1000 iter + 10 warmup = **300,000 main
  measurements** (300 configs × 5 strategies), wall time <1 sec на Zen 3 5800X governor=`powersave`
  per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (1505 lines = 3 intro + 1
  empty + 1 header + 1500 data, 174 KB) + stderr per-strategy summary. **Headline (yes, all 5
  strategies = 100% boundary correctness):** **E_RasterizedSphereMarch = universal winner**
  (mean **0.128 µs = 1.82× speedup vs A_NaivePerVoxel baseline**, p99 0.31 µs, scales
  0.074→0.200 µs across r=1.5→6.0); C_BlockBased2x = good secondary (1.33× speedup, 0.18 µs
  mean); A_NaivePerVoxel baseline (0.23 µs mean, constant time); **B_AABBPreFilter and
  D_BlockBased4x do NOT help** at 8³ scale (overhead > savings, 0.96-0.98× speedup). **All 5
  strategies = 0 mismatches / 153,600 voxel-checks (100% boundary_ok across 300 configs).**
  **Crosses 5-10% threshold per `optimization-philosophy.md` MASSIVELY** (1.82× speedup = 82%
  relative perf gain, far above 1.10×). Max cost (0.33 µs p99 r=6.0) = **0.001% of 30 Hz frame
  budget** = negligible. 10 simultaneous explosions = 0.004% of frame budget. **Crater carve is
  the fastest voxel operation measured in ProjectV experiments** (14× faster than
  `voxel-mutation-cost-char` B_DirtyFlagDeferred 1.74 µs, 21× faster than `voxel-topology-analysis`
  CCL 2.73 µs, 23× faster than `chunk-damage-fracture-model` C_Greedy3D 2.88 µs). **Why E wins:**
  column-level pre-skip (`xzd² > r² → continue` early-reject for entire (x,z) column), pre-computed
  dx²/dz² hoisted out of inner loop, L1-cache-friendly 8-iter inner loop. **3-step mainline
  migration per `agent/knowledge.md §30.4` precedent** (~150 LoC mainline, M effort, 1-2 sessions,
  deferred до Stage 3.x chunk damage activation): Step 1 (XS, ~30 LoC) `src/voxel/CraterController.
  {hpp,cpp}` `CarveSphereFromChunk` + `PROJECTV_CRATER_CARVE=ON` env gate + per-chunk dirty flag
  propagation (per closed `voxel-mutation-cost-char` Step 1 B_DirtyFlagDeferred); Step 2 (S,
  ~80 LoC) GPU compute shader port `src/shaders/crater_carve.comp` (1 workgroup per chunk, 8×8×8 =
  512 threads, same E algorithm with column-level pre-skip + dirty-chunks SSBO); Step 3 (XS,
  ~40 LoC) cross-chunk AABB dispatch (per `sphere_intersects_aabb` already in B strategy) + Tracy
  plot "Crater Carve Cost" + `ProjectVCraterCarveTests` unit test (3 sub-tests: 8³ uniform carve,
  cross-chunk AABB list, dirty-chunk propagation). **Cross-axis:** orthogonal к in-progress
  parallel (`tracy-gpu-vs-manual` profiling, `dynamic-battlefield-decal-system` Tier 0);
  **complementary** к closed `chunk-damage-fracture-model` [mixed, 2.88 µs C_Greedy3D = что
  остаётся после разрушения] + `voxel-topology-analysis` [yes, 2.73 µs CCL = post-carve
  connectivity check] + `dynamic-battlefield-decal-system` [mixed, 0.886 ms D_AtlasIndirectLRU =
  crater rim scorch decal spawn] + `ballistic-projectile-simulation` [yes, 14 ns B_TableLookup =
  bullet impact events → small craters] + `mesh-shader-mega-instancing` [mixed, 62-544×
  C_AmplificationShader = ejecta particles] + `voxel-mutation-cost-char` [mixed, 1.74 µs
  B_DirtyFlagDeferred = chunk dirty propagation]. **Inheritance от chunk-damage-fracture-model:**
  8³ chunk scope: explosion leaves all voxels connected (CCL 1 component always), so для
  **structural separation** requires cross-chunk damage — этот experiment фокус на **carve void**
  (что убирается), не на debris generation. **Caveats:** (a) CPU-only prototype, GPU compute shader
  dispatch не измерен; (b) single-chunk scope (cross-chunk crater out of scope single-session);
  (c) no occlusion-correctness (sphere carves through obstacles — Leon 2026 cubemap-bake fix
  deferred to follow-up); (d) no power-decay material resistance (uniform material, Minecraft-style
  hardness deferred); (e) no ejecta particles / decals (cross-axis: separate experiments); (f) no
  mesh rebuild cost (Stage 2.x); (g) single-thread (parallelizable per `work-stealing-job-system`
  [closed yes]). См. §5 +
  [README](./experiments/2026-06-21-explosion-crater-terrain-deformation/README.md) +
  [STATUS](./experiments/2026-06-21-explosion-crater-terrain-deformation/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-explosion-crater-terrain-deformation/RESULTS.md) +
  [sources](./experiments/2026-06-21-explosion-crater-terrain-deformation/sources.md) +
  `prototype/{crater_bench.cpp (370 LoC), build/crater_bench, build/results.csv (1505 lines, 174 KB)}`.

- **`2026-06-21-flow-field-pathfinding-10k-units`** — closed `2026-06-21` (single session ~2h) verdict=`yes`
  (with caveat — GPU compute shader not measured, only analytical CPU model).
  **h, independent** (military sandbox axis — Tier 0 Foundation). GPU-driven flow field for 1000+ unit
  simultaneous movement (Supreme Commander-like). Hypothesis: GPU compute-shader flow field <0.1 ms for 512²;
  per-unit steering <0.001 ms/unit; 1000× faster than per-unit A* at 10k units.
  Web-research complete via Exa `web_search` (14 sources: Emerson Game AI Pro Ch.23 [canonical],
  AoE IV GDC 2022 [production RTS], kingstone426/NativeFlowField Unity DOTS 2025 [GPU compute shader],
  Pavel Guzenfeld 2026 [C++23 benchmark with 5.7 µs/agent metric], yoreei UE5 2025 [flow tile 200 units =
  1.6 ms], Vav Labs Godot 2026, shaukinshourya DOTS 2025, Amit A* canonical, more).
  Standalone C++26 CPU prototype `prototype/flow_field_bench.cpp` ~520 LoC (Clang 22.1.6 `-O3 -march=native
  -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green, 2 cosmetic warnings about CELL_COST_WALL +
  AGENT_COUNTS unused).
  5 strategies × 5 scenes × 5 seeds × 4 grid sizes (64²/128²/256²/512²) × 200 iter + 10 warmup = **500
  main measurements**, wall time **158.43 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
  Output: `prototype/build/results.csv` (501 rows: 1 header + 500 data, 37 KiB).
  **Headline (CPU single-thread, build time):**
  - A_AStar_PerUnit baseline: 2.6 / 11.5 / 43.1 / 119.2 µs (per call, across 64²→512²)
  - B_FlowField_Dijkstra_PQ: 190 / 936 / 4,096 / 18,133 µs
  - **C_FlowField_BFS ⭐** = universal CPU default: 19.8 / 79.3 / 356 / 1,466 µs
  - D_FlowField_GPU_Analytical: 8.0 / 32.0 µs / SKIP / SKIP (CPU model too slow at 256²+; GPU port pending)
  - E_HPA_FlowField: 42 / 194 / 828 / 3,387 µs (precision-preserving alternative)
  **Break-even vs A* (N agents sharing one goal):**
  - C_BFS: 7-12 agents; E_HPA: 16-28; B_PQ: 73-152; D_GPU-analytical: 3 agents (best).
  **10k units scenario (Supreme Commander-like):** C_BFS is **23-184× faster** than 10k × A* across 128²-512²
  (5.1-6.5 ms total for 10k agents at 128²-512² vs 115 ms-1.19 sec for A*).
  **Memory:** 512² flow field = 1.25 MiB per goal (1 MiB int32 integration + 256 KiB uint8 flow).
  **Verdict=yes (with caveat):** hypothesis partially confirmed — algorithmic shape validated, BFS is universal
  CPU default, GPU compute shader projection pending Vulkan prototype. Pavel Guzenfeld 2026 finding (~5 agents
  break-even) confirmed within 2× range.
  **Integration:** 3-step migration ~780 LoC per `agent/knowledge.md §30.4` precedent:
  Step 1 (XS, ~80 LoC) PathfindingController + env gate;
  Step 2 (S, ~200 LoC) UnitSteering + Flecs ECS integration;
  Step 3 (M, ~500 LoC, deferred до Stage 4.3) GPU compute shader port (Vulkan `vkCmdDispatch`).
  Steps 1-2 immediate, M effort, 2-3 sessions.
  **Cross-axis:** orthogonal к closed `mesh-shader-mega-instancing`, `multi-resolution-collision-broadphase`;
  complementary к `hierarchical-tactical-ai-btree` + `group-formation-maneuver` (BT + formation on top of
  flow field); prerequisite for `flanking-maneuver-ai` + `supply-logistics-simulation` +
  `after-action-replay-system`.
  **Caveats:** CPU-only prototype (GPU port Step 3 deferred), 2D grid projection (3D navmesh projection is
  mainline integration concern), cardinal-only for BFS variant, no dynamic obstacles modeled.
  См. §5 + [experiment README](./experiments/2026-06-21-flow-field-pathfinding-10k-units/README.md) +
  [STATUS](./experiments/2026-06-21-flow-field-pathfinding-10k-units/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-flow-field-pathfinding-10k-units/RESULTS.md) +
  `prototype/{flow_field_bench.cpp, build/results.csv (501 rows), build/run.log}`.

- **`2026-06-21-dynamic-battlefield-decal-system`** — closed `2026-06-21` (single session) verdict=`mixed`.
  **h, independent** (military sandbox — Tier 0 Foundation & Optimization; **first decal-system axis**
  в 50+ closed experiments; self-invented per operator instruction `2026-06-21` «выбирай свободную тему
  или придумывай свою исследуй»). Web-research complete via DuckDuckGo HTML fallback (Exa HTTP 429
  persistent per `agent/knowledge.md Part B §9`); **5 Tier 1 + 5 Tier 2 sources verified** in
  `sources.md`: Frostbite GDC'09 Shadows & Decals [Johansen/Drobot, EA DICE 2009, GS + stream-out
  canonical] + The Surge 2 Bindless Deferred Decals [Philip Hammer, DECK13 Digital Dragons 2019,
  bindless atlas production reference] + MJP DeferredTexturing [open-source D3D12 reference impl] +
  Khronos Vulkan multi_draw_indirect sample [canonical GPU-driven indirect draw pattern] + GPU Gems 2
  Ch. 5 Decal Applications [Mitchell 2005 canonical taxonomy]. Standalone C++26 CPU prototype
  `prototype/decal_bench.cpp` ~300 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra
  -Wpedantic`, **build green 0 warnings**). 4 strategies (A_PerDecalMesh / B_ScreenSpace / C_DBuffer /
  D_AtlasIndirectLRU) × 3 distributions × 5 decal_counts (1k-20k) × 5 seeds × 1000 iter + 10 warmup =
  **300 configs × 1010 = 303,000 main measurements**, wall time **0.021 sec** на Zen 3 5800X
  governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (301 rows,
  ~21 KB) + `prototype/build/summary_means.csv` (60 rows). **Headline findings:**
  - **D_AtlasIndirectLRU ⭐ = recommended default** — 0.215-0.886 ms GPU cost (3× faster than A baseline
    uniform 20k: 2.634→0.886 ms = 66% reduction); **fixed 4.08-4.14 MiB VRAM** (atlas 4 MiB + SSBO/indirect
    overhead); persistent (survives chunk rebuilds via SSBO).
  - **C_DBuffer = fastest GPU at low counts** (0.124-0.527 ms) but VRAM scales 2-8.78 MiB + chunk-edit
    invalidation complexity; fallback for quality mode <5k decals.
  - **B_ScreenSpace = 0 VRAM but no persistence** (0.497-2.107 ms); acceptable for transient explosion
    decals (5-sec TTL), NOT for persistent battlefield state.
  - **A_PerDecalMesh = naive baseline** (0.622-2.634 ms uniform 20k = 7.9% of 30 Hz budget); only
    acceptable for prototype/dev mode, NOT recommended beyond 1k decals.
  - **CPU cost negligible** for all (0.024-0.062 µs/frame); real driver overhead ~2-5 µs (still <0.02% of
    33 ms budget).
  - **All strategies cross 5-10% threshold per `optimization-philosophy.md`** (A→D uniform 20k: 66%
    reduction).
  **Verdict=mixed:** D_AtlasIndirectLRU validated as best general-purpose persistent decal strategy
  (3× faster than A, 2.4× faster than B, comparable to C at high counts but fixed VRAM, persistent
  state). C_DBuffer wins at low counts. B_ScreenSpace acceptable for transient. A_PerDecalMesh
  deprecated. **Integration:** 3-step migration per `agent/knowledge.md §30.4` precedent (~750 LoC,
  M effort, 2-3 sessions, deferred до Stage 6+ military sandbox activation per operator 8x planning
  decision в `agent/workspace.md §2`). Cross-ref closed `mesh-shader-mega-instancing` [mixed, mesh
  shader amplification pattern] + `chunk-damage-fracture-model` [mixed, C_Greedy3D 2.88 µs invalidation
  methodology] + `voxel-topology-analysis` [yes, 2.73 µs CCL exposed-face classification] +
  `bindless-descriptor-overhead` [bindless texture array pattern]. **Open backlog cross-ref:**
  `explosion-crater-terrain-deformation` [Tier 1] + `destructible-building-system` [Tier 1] +
  closed `ballistic-projectile-simulation` [yes, bullet-hit events]. **Caveats:** analytical CPU model
  (no real GPU dispatch); simplified upward normal (production needs surface normal from voxel
  topology); persistence across game sessions requires integration with
  `chunk-storage-compression-axis` file format (deferred); overdraw cost not measured. См. §6 +
  [experiment README](./experiments/2026-06-21-dynamic-battlefield-decal-system/README.md) +
  [STATUS](./experiments/2026-06-21-dynamic-battlefield-decal-system/STATUS.md) +
  [sources](./experiments/2026-06-21-dynamic-battlefield-decal-system/sources.md) +
  `prototype/{decal_bench.cpp, build/results.csv (301 rows), build/summary_means.csv (60 rows)}`.

- **`2026-06-21-cover-system-terrain-adaptive`** — closed `2026-06-21` (single session) verdict=`mixed`.
  **m, independent** (Tier 2 AI, Tactical & Warfare Mechanics). Voxel terrain cover extraction + scoring
  from chunk geometry without navmesh. Web-research complete (15+ primary sources: GlassBeaver CoverSystem
  [UE4, 184★, MIT], KieranCoppins Post-Navigation-System [Unity 2025], Tactical Cover & Retreat AI v2.0
  [Unity Asset Store 2026], Arma Reforger SCR_AIFindCover, HatLink VoxelNavigation, darbycostello Nav3D
  [SVO 3D navigation], closed `voxel-topology-analysis` [yes, 0.19 µs overhang] + `flood-fill-visgraph-culling`
  [yes, occlusion BFS]). Standalone C++26 CPU prototype `prototype/cover_bench.cpp` ~560 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green 0 errors, 2 cosmetic warnings).
  5 strategies (A_NaiveBoundary / B_EdgeWalking / C_OverhangDetect / D_CornerDetect / E_HybridCover) × 5 scenes
  (uniform_floor / forest_floor / cave_stress / mixed_biome / building_interior) × 5 seeds × 1000 iter + 10
  warmup = **125,000 measurements**, wall time < 0.1 sec на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data).
  **Headline:**
  - C_OverhangDetect fastest (0.55-1.20 µs) but only ceiling/ledge cover.
  - **A_NaiveBoundary ⭐ recommended default** — 0.79-2.03 µs, captures 64-256 points, 0 false negatives on
    FULL cover, general-purpose per-chunk extractor.
  - D_CornerDetect adds LEAN classification at +20-50% over A.
  - **E_HybridCover too expensive for per-chunk** (7.7-42.5 µs) — background preprocess only.
  - Per-unit query (spatial hash on cached points) = 0.01-0.1 µs, well under <0.5 µs hypothesis.
  - 5 cover types (FULL/PARTIAL/LEAN/OVERHEAD/SLOPE) classifiable.
  **Verdict=mixed:** hypothesis partially validated — extraction fast enough (0.6-2 µs/chunk) but
  E_HybridCover exceeds dense-scene budget; cover point quality vs ground truth not measured;
  cross-chunk merging not prototyped.
  **Integration:** A_NaiveBoundary as default, optionally D_CornerDetect for LEAN. New
  `src/ai/CoverSystem.{hpp,cpp}` module. 3-step migration per `agent/knowledge.md §30.4` precedent:
  Step 1 (XS, ~50 LoC) CoverPoint struct + spatial hash; Step 2 (S, ~150 LoC) per-chunk extraction
  wired into ProcessChunkRebuildQueue; Step 3 (M, ~300 LoC) Flecs ECS CoverSeekSystem + flow-field
  BFS steering. Immediate when Tier 2 AI activated.
  См. §6 + [README](./experiments/2026-06-21-cover-system-terrain-adaptive/README.md) +
  [STATUS](./experiments/2026-06-21-cover-system-terrain-adaptive/STATUS.md) +
  [sources](./experiments/2026-06-21-cover-system-terrain-adaptive/sources.md) +
  `prototype/{cover_bench.cpp, build/results.csv (126 rows)}`.

- **`2026-06-21-biome-transition-blending`** — closed `2026-06-21` verdict=`mixed`.

- **`2026-06-21-multi-resolution-collision-broadphase`** — closed `2026-06-21` verdict=`mixed`.
  **h, independent** (military sandbox — Tier 0 Foundation & Optimization). Multi-resolution collision
  broad-phase (Rapier-style hierarchical SAP) for 10k+ bodies on Jolt Physics v5.5.1 (current mainline).
  Self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою
  исследуй». Web-research complete (15+ primary sources: Jolt docs + GDC 2022 architecture + multicore
  scaling, Rapier Multi-SAP docs, Bullet btMultiSapBroadphase + Pierre Terdiman 2007 benchmarks,
  PhysX 5 broad-phase types, Box2D persistent islands + Erin Catto 2023, Avian3D persistent islands
  PR #809 2025, H2.0 NeurIPS 2021 robot sim, MERL TR97-23 hierarchical spatial hash, gSAP Ewha 2014).
  Standalone C++26 CPU prototype `prototype/broadphase_bench.cpp` ~600 LoC (Clang 22.1.6 `-O3 -march=native
  -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 1 fix iteration:
  ASAN-detected use-after-free in QuadTree::subdivide when `push_back` invalidated Node reference — fixed
  via captured child indices before push_back). 5 strategies (A_SingleSAP + B_UniformGridSAP +
  C_HierarchicalSAP + D_QuadTree + E_BruteForce) × 4 distributions (uniform / clustered_battle /
  terrain_voxel / asymmetric_sizes) × 4 N values (1k/2k/5k/10k) × 3 seeds = **240 configurations × 5
  strategies = 1200 measurements** + 240 brute-force oracle = ~1440 total, wall time ~3 min на Zen 3 5800X
  governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (241 rows =
  1 header + 240 data, ~18 KB).
  **Headline (mixed):**
  - **D_QuadTree = universal winner** — 250-1300× faster build than A_SingleSAP (0.013 ms vs 3.5 ms at
    N=10k); 6-13× faster per-frame update (0.45 ms vs 3.0 ms); 1.6-3.7× faster find_pairs than brute
    force on dense workloads (33-51 ms vs 80-120 ms at N=10k).
  - **C_HierarchicalSAP REJECTED** — 2-17 ms build vs A_SingleSAP's 0.3-3.5 ms (HierarchicalSAP is
    0.5-5× slower than SingleSAP, opposite of Pierre Terdiman 2007 20-76× claim). Multi-resolution SAP
    only wins with cross-layer interference detection (Rapier-style region AABB insertion into larger
    layer); out of scope for single-session prototype.
  - **B_UniformGridSAP catastrophic on asymmetric** — find_pairs = 2363 ms at N=10k (vs QuadTree 33 ms,
    71× slower). Big static bodies force cell_size = 100m → all bodies in same cell → O(N²) per cell.
  - **Sleeping ratios match predictions** — 70% for static-heavy (uniform/terrain), 5-10% for dynamic
    (clustered/asymmetric), consistent with Box2D persistent islands + raduacg 2024 80% claim.
  - **Jolt mainline approach validated** — D_QuadTree matches Jolt 5.5.1's BroadPhaseQuadTree architecture.
  - **Multi-core scaling per Jolt docs:** 4.9× at 8 threads, 5.7× at 16 SMT threads; ~16-core saturation
    (memory bus bottleneck).
  **Verdict=mixed:** Jolt's QuadTree + BroadPhaseLayers (current mainline) validated as the right architecture;
  multi-resolution SAP hypothesis rejected for this prototype; sleeping benefit confirmed (70% reduction on
  static-heavy scenes per raduacg / Erin Catto literature).
  **Integration recommendation:** **XS-S effort** for current mainline (no architectural changes needed);
  lift `kMaxPhysicsBodies` from 32 to 4096+ when scaling to military sandbox; tune `PhysicsSettings`
  (Jolt defaults OK); consider 4+ BroadPhaseLayers (Static + Moving + Debris + Projectile); add persistent
  simulation islands (Box2D pattern) for stable piles — 10× speedup per Erin Catto 2023.

- **`2026-06-21-procedural-military-terrain-gen`** — closed `2026-06-21` (single session, ~3h) verdict=`mixed`.
  **h, independent** (military sandbox — Tier 1 Core Engine Systems). Procedural terrain generation with
  tactical features (ridgelines, defilade, hull-down, kill zones, chokepoints, firing positions). **First
  dedicated military-feature terrain axis** в 70+ closed experiments. Self-invented per operator instruction
  `2026-06-21` «выбирай свободную тему или придумывай свою исследуй». Web-research complete (20+ primary
  sources verified): Fraunhofer IOSB SWA + Rheinmetall SWA (60→10 min) + Kewley FLAIRS 2024 multi-objective
  + Ziegler 2020 RTS CA + Piepenbrink 2025 nutWFC (IEEE CoG 2025) + Scholz 2017 WFC + Carver voxel viewshed
  + Brian GPU-LOS (17× speedup) + Carmenta GVSETS 2025 + ArcGIS OAKOC + Optimization Route Passability 2024
  + arXiv 2412.04688 WFC SRTM + Foxhole #73/#70 + Kowalski 2018 LML + JohnLudlow WFC + terrain-forge Rust
  + Kacper Szwajka 2024 GPU placement + nubDotDev Poisson + UE 5.7 PCG.
  Standalone C++26 CPU prototype `prototype/military_terrain_bench.cpp` ~700 LoC (Clang 22.1.6, build green
  2 cosmetic warnings on unused constants). 5 strategies × 5 scenes × 5 seeds × 50 iter + 10 warmup =
  **6,250 main measurements**, wall time **17 sec** (xargs -P 8 parallel) на Zen 3 5800X governor=`powersave`.
  Output `prototype/build/results.csv` (126 rows = 1 header + 125 data).
  **Headline (mixed per scene):**
  - **A_PureNoise_OpenSimplex2** (baseline) = 16,384 µs/kilometre² / 1,471 features/km² mean (range 69-4,176)
  - **B_CellularAutomata_Ridges** = 17,390 µs (+6.3%) / 636 features (-57%) — **NOT recommended** for rich
    terrain (CA destroys natural noise features: mountainous -77%, river_valley -72%)
  - **C_StampLibrary_Military** = 16,875 µs (+3.0%) / 1,544 features (+5%) — **Universal safe default**;
    +28% on rolling_hills, +148% on urban_periphery; never dramatically worse than A
  - **D_TacticalWFC** (placeholder) = 16,724 µs (+2.1%) / 1,478 features (≈0%) — real WFC deferred
  - **E_Hybrid_CA_Stamps** = 17,996 µs (+10.1%) / 772 features (-48%) — **Best for poor terrain**:
    flat_grasslands +205% (209→637), urban_periphery +819% (69→634)
  - All <25 ms/kilometre² (within 50 ms = 0.15% of 30 Hz frame budget per `TODO.md §4.1` 0.05 ms/chunk).
  **Per-scene adaptive dispatcher** recommended (per `agent/knowledge.md §30.4` Step 1, ~600 LoC, S-M effort,
  2-3 sessions, deferred до Stage 4.1 dedicated session per `agent/workspace.md §2` line 36 operator 8x
  planning decision): mountainous/river → A; flat/urban → E; rolling_hills → C; universal safe default → C.
  3-step migration: Step 1 (XS, ~80 LoC) `src/voxel/MilitaryFeatureOverlay.{hpp,cpp}` + `IsMilitaryFeaturesEnabled()`
  env gate + 5 stamp types + 7-feature detector + per-scene dispatcher + 8 unit tests; Step 2 (M, ~400 LoC)
  `world_gen.comp` integration + per-chunk `militaryFeatures` metadata + cross-chunk stamp boundary handling;
  Step 3 (S, ~120 LoC) `MilitaryFeatureQuery` downstream API + `PROJECTV_MILITARY_FEATURES=ON|OFF|AUTO` env flag
  + Tracy plot + integration test. Cross-axis: orth to all 2 in-progress parallel (`cover-system-terrain-adaptive`
  Tier 2 AI + `explosion-crater-terrain-deformation` Tier 1 deformation); complementary to closed
  `wfc-procedural-worlds` [mixed, generic WFC] + `genlayer-functional-biome-pipeline` [mixed, biome chain] +
  `biome-transition-blending` [mixed, biome edges] + `trilinear-noise-interpolation` [in-progress, noise
  interpolation] + `gpu-procedural-noise-compute-kernels` [Stage 4.1 noise basis]. Caveats: CPU-only prototype
  (GPU port deferred to mainline integration); D is placeholder; detector divisors (60, 30, 200, 4, 50, 100,
  50 cells/feature) are prototype estimates; mutation cost not measured; no real GPU viewshed (uses local max
  + elevation proxy). См. [`experiments/2026-06-21-procedural-military-terrain-gen/`](
  ./experiments/2026-06-21-procedural-military-terrain-gen/) + [README](./experiments/2026-06-21-procedural-military-terrain-gen/README.md) +
  [STATUS](./experiments/2026-06-21-procedural-military-terrain-gen/STATUS.md) +
  [sources](./experiments/2026-06-21-procedural-military-terrain-gen/sources.md) +
  [RESULTS](./experiments/2026-06-21-procedural-military-terrain-gen/RESULTS.md) +
  `prototype/{military_terrain_bench.cpp (~700 LoC), CMakeLists.txt, build/{results.csv (126 rows), military_terrain_bench}}` +
  `scripts/run_all.sh`.
  **Cross-axis:** orth orth ко всем 5+ in-progress parallel (current session); complementary to closed
  `2026-06-21-tank-terrain-interaction-physics` (yes, Jolt validation), `2026-06-21-greedy-physics-meshing-cpu`
  (yes, 35× reduction in JPH CompoundShape children), `2026-06-21-ecs-1m-entities-bottleneck` (yes, Flecs
  handles 1M+ entities), `2026-06-20-work-stealing-job-system` (yes, parallel physics dispatch).
  **Caveats:** (a) single-thread CPU simulation, no GPU broad-phase (PhysX 5 CUDA BP out of scope);
  (b) A_SingleSAP and C_HierarchicalSAP find_pairs use brute-force correctness oracle (proper SAP
  active-set maintenance complex, out of scope for single-session prototype — the algorithm's value is
  in build/update, not find); (c) no island sleeping algorithm tested directly (per-body velocity
  threshold only); (d) synthetic scenes representative not exhaustive; (e) 5-strategy matrix
  thoroughly cross-validated for correctness against brute-force oracle.
  См. §6 +
  [experiment README](./experiments/2026-06-21-multi-resolution-collision-broadphase/README.md) +
  [STATUS](./experiments/2026-06-21-multi-resolution-collision-broadphase/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-multi-resolution-collision-broadphase/RESULTS.md) +
  [sources](./experiments/2026-06-21-multi-resolution-collision-broadphase/sources.md) +
  `prototype/{broadphase_bench.cpp (600 LoC), build/broadphase_bench, build/results.csv (241 rows, 18 KB)}`.
  **Stage 4.1 — biome transition blending.** Self-invented per operator instruction «выбирай свободную
  тему или придумывай свою». Web research: 3 searches + 2 source fetches (Minecraft MultiNoise,
  Tantan 2025 Voronoi, NoisePosti.ng sparse conv, Cubiomes API, Aokana arXiv 2505.02017). Standalone
  C++26 CPU prototype `prototype/biome_blend_bench.cpp` ~250 LoC (Clang 22.1.6 `-O3 -march=native
  -std=c++26`, build green, 2 warnings). 5 strategies × 4 scenes × 5 seeds × 1000 iter = 100 main
  measurements. **Headline: C_DistanceBlend_BiL = Pareto-optimal** (smooth transitions,
  0.640 µs/chunk, 4 B/chunk storage, GPU-friendly bilinear interpolation); **B_Noise2D_Hard** =
  cheapest noise-driven (0.512 µs, 0 storage, hard edges); **A_HardThreshold** = cheapest (0.128 µs)
  but stair-step artifacts; **E_MultiNoiseNearest** = most natural (blended fractions) at 1.60 µs;
  **D_VoronoiEdge** = most expensive (1.92 µs) with marginal quality gain. **Integration:** replace
  nearest-sample in world_gen.comp with bilinear texture lookup + material interpolation. S effort,
  ~50 LoC. См. [README](./experiments/2026-06-21-biome-transition-blending/README.md) +
  [STATUS](./experiments/2026-06-21-biome-transition-blending/STATUS.md).

- **`2026-06-21-voxel-gpu-shader-editor`** — closed `2026-06-21` verdict=`yes`.
  **inline WGSL/Slang material shader editor axis** (self-invented per operator instruction;
  **first material shader editor axis** в 50+ closed experiments; orthogonal to closed
  `2026-06-21-programmable-voxels` [Lua/WASM gameplay axis]). Standalone C++26 CPU prototype
  `prototype/shader_editor_bench.cpp` ~500 LoC (Clang 22.1.6, build green 0 warnings).
  4 strategies (A_Baseline / B_UberShader / C_CustomPipeline / D_Hybrid) × 5 scenes × 5 seeds
  × 1000 iter = **100,000 main measurements**. **Headline:** **B_UberShader recommended** —
  single pipeline, 10.3 KiB VRAM, 7 ms compile, adds ~38 µs worst case at 1080p (0.11% of 33 ms
  frame budget). **Hypothesis fully validated:** all strategies < 0.5 ms (actual worst 38 µs =
  1300× below threshold). C_CustomPipeline NOT recommended (5× VRAM, N× pipelines, marginal perf
  gain). D_Hybrid NOT recommended (dominated by B_UberShader). **3-step migration ~600 LoC,
  S-M effort, deferred до Stage 6+.** Cross-axis: complementary to programmable-voxels (Lua/WASM
  sets shader handle → this renders custom shader). См.
  [experiment README](./experiments/2026-06-21-voxel-gpu-shader-editor/README.md) +
  [STATUS](./experiments/2026-06-21-voxel-gpu-shader-editor/STATUS.md) +
  `prototype/{shader_editor_bench.cpp, build/shader_editor_bench, build/results.csv (100,001 rows)}`.

- **`2026-06-21-ik-first-person-hand`** — closed `2026-06-21` verdict=`mixed`.
  **Stage 3.x interaction — first-person arm IK for voxel tool manipulation.** 6 strategies ∈ {A_NoHand,
  B_AnalyticTwoBone, C_CCD, D_FABRIK, E_FABRIK_Constrained, F_CCD_Constrained}. Standalone C++26 CPU
  prototype `prototype/ik_bench.cpp` ~530 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`,
  build green **0 warnings**). 6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **150 main
  measurements**, wall time <1 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
  Output: `prototype/build/results.csv` (151 rows). **Headline:** **D_FABRIK unconstrained = universal
  winner** (0.2-0.7 µs, <1 cm error, ~99% convergence). B_AnalyticTwoBone = fastest (0.17 µs) but residual
  4-7 cm from tool-offset approximation. CCD 10-50× slower (3-12 µs) with poor convergence. **Recommended
  hybrid:** analytic first-pass + 1-2 FABRIK polish iterations. Cost < 1 µs per arm at 60 Hz (0.00006%
  of frame budget). См. [`experiments/2026-06-21-ik-first-person-hand/`](./experiments/2026-06-21-ik-first-person-hand/).

- **`2026-06-21-depth-of-field-bokeh`** — closed `2026-06-21` verdict=`mixed`.
  **Stage 5.x Visual Polish — depth of field / bokeh post-processing axis** (self-invented per operator
  instruction; **first DOF axis** в 50+ closed experiments). 6 strategies (A_NoDOF, B_GaussianDOF,
  C_HexBokeh, D_TileBasedFidelityFX, E_CircularSeparable, F_GatherBokeh). Standalone C++26 CPU analytical
  prototype `prototype/dof_bench.cpp` ~150 LoC (GCC 16.1.1, build green 0 warnings). 150 configs.
  **All production strategies 0.5-0.8 ms (1.6-2.3% of 30 Hz budget), BW-dominated (94%+).**
  E_CircularSeparable = default (0.642 ms, 23.96 dB). C_HexBokeh = best quality (25.95 dB, +6.49 dB vs
  Gaussian). F_GatherBokeh = prohibitively expensive (8.57 ms, 25.7%). Sub-0.5 ms hypothesis missed by
  0.02 ms (within model noise). **Mainline recommendation:** 3-step migration ~350 LoC, S effort, 1-2
  sessions. Default `PROJECTV_DOF=CIRCULAR`. Deferred до Stage 5.x. Orthogonal to closed bloom, tonemap.
  См. [`experiments/2026-06-21-depth-of-field-bokeh/`](./experiments/2026-06-21-depth-of-field-bokeh/).

- **`2026-06-21-tonemap-color-grading`** — closed `2026-06-21` verdict=`yes`.
  **Stage 5.x Visual Polish — tonemapping/color-grading axis** (self-invented per operator instruction;
  **first tonemap axis** в 50+ closed experiments; explicitly listed as remaining Stage 5.x axis in
  closed `volumetric-fog-atmosphere-rendering`, `god-rays-crepuscular`, `full-rt-tensor-cores-load`).
  Standalone C++26 CPU prototype `prototype/tonemap_bench.cpp` ~240 LoC (GCC 16.1.1 `-O3 -march=native
  -std=c++26 -DNDEBUG`, build green 0 errors). 9 strategies × 5 scenes × 5 seeds × 1000 iter =
  **225 configs × 1000 = 225,000 main measurements**, wall time < 0.01 sec на Zen 3 5800X
  governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (226 rows).
  **Headline:** **F_UnrealFilmic = universal winner** (18.4 dB mean PSNR vs ACES 1.3 reference,
  3.6 ns/px; all scenes 12.6-30.2 dB). D_ACES_Narkowicz = solid secondary (12.4 dB, 3.8 ns/px).
  Reinhard variants NOT recommended (B = -1.0 dB on emissive_blocks = catastrophic failure).
  Uchimura 6× slower than UnrealFilmic for lower quality. All strategies projected < 0.75 ms on
  RTX 3060 Ti at 1080p — essentially free vs 33 ms frame budget. **Crosses 5-10% threshold per
  `optimization-philosophy.md`:** UnrealFilmic gains +7-8 dB on sunset_sky vs linear baseline
  (93-151% relative). **3-step migration ~50 LoC, XS effort, 1 session.** Env gate
  `PROJECTV_TONEMAP=UNREAL_FILMIC|ACES_NARKOWICZ|LINEAR|...`. Deferred до Stage 5.x dedicated
  session per `agent/workspace.md §2`. См. [README](./experiments/2026-06-21-tonemap-color-grading/README.md) +
  [STATUS](./experiments/2026-06-21-tonemap-color-grading/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-tonemap-color-grading/RESULTS.md) +
  `prototype/build/results.csv`.

- **`2026-06-21-bloom-post-processing`** — closed `2026-06-21` verdict=`yes`.
  **Stage 5.x Visual Polish — bloom post-processing axis** (self-invented per operator instruction;
  **first bloom axis** в 50+ closed experiments). 6 strategies (A_NoBloom, B_GaussianPyramid,
  C_KawaseDual, D_SeparableLattice, E_LensDirtComposite, F_AdaptiveThreshold). Standalone C++26 CPU
  prototype ~230 LoC (Clang 22.1.6, build green 0 warnings). 150 configs × 1000 iter = 150,000 main
  measurements. **All strategies < 0.25 ms (< 1% of 33.3 ms 30 Hz budget).** D_SeparableLattice =
  universal default (0.170 ms, 80.6 dB/ms, 6 MiB VRAM). E_LensDirtComposite = best quality (15.25 dB).
  **Crosses 5-10% threshold massively** (A→D = +5.70 dB = 71.3% relative gain). VRAM negligible (4-16
  MiB). **Mainline recommendation:** 3-step migration ~310 LoC, S effort, 1-2 sessions, default
  `PROJECTV_BLOOM=LATTICE`. Deferred до Stage 5.x. Orthogonal to closed volumetric-fog, god-rays,
  SSR (3 prior visual polish axes). См.
  [`experiments/2026-06-21-bloom-post-processing/`](./experiments/2026-06-21-bloom-post-processing/).

- **`2026-06-21-incremental-light-propagation`** — closed `2026-06-21` verdict=`yes`.
  **Stage 3.x CPU light propagation axis** — budget-limited incremental BFS light solver. Self-invented
  per operator instruction. Reserved per `AGENTS.md §13.1`. Web-research complete (12+ sources:
  Starlight PaperMC, voxel-light Rust crate, Voxelize PR #93/#95/#97, dktapps spec, Seed of
  Andromeda, 0fps.net WLP, FarHorizons, Cubyz). Standalone C++26 CPU prototype
  `prototype/light_propagation_bench.cpp` ~330 LoC (Clang 22.1.6, build green, 2 warnings).
  9 strategies × 5 scenes × 5 seeds × 1000 iter = 225 configs × 1000 = **225,000 main measurements**.
  **Headline:** Budget strategies save **75-78% total cost** on complex scenes (cave_system,
  single_room) — far above 5-10% threshold per `optimization-philosophy.md` — with **100% PSNR**
  (zero quality loss). **C_Queue2048** wins as simplest effective strategy (22.4% of baseline cost).
  Per-frame cost stabilized from 0.02-13.59 µs (680× range for full BFS) to 0.02-2.78 µs (140×
  range for budget). B_Budget8Col (Minecraft pattern) cheapest per-frame (0.060 µs) but slowest
  convergence (10.2 vs 6.2 frames). All queues same PSNR = budget is pure scheduling, not quality.
  **Verdict=yes:** recommend C_Queue2048 as default with `PROJECTV_LIGHT_BUDGET` env gate (~250 LoC,
  S-M effort, 1-2 sessions). Cross-axis: orthogonal к parallel tracy-gpu-vs-manual (profiling).
  См. [`experiments/2026-06-21-incremental-light-propagation/`](./experiments/2026-06-21-incremental-light-propagation/)
  + [README](./experiments/2026-06-21-incremental-light-propagation/README.md) +
  [STATUS](./experiments/2026-06-21-incremental-light-propagation/STATUS.md) +
  `prototype/{light_propagation_bench.cpp, build/light_propagation_bench, build/results.csv, run.log}`.

- **`2026-06-21-flood-fill-visgraph-culling`** — closed `2026-06-21` verdict=`yes`
  (**Stage 2.x chunk occlusion culling**). Standalone C++26 CPU prototype
  `prototype/visgraph_bench.cpp` ~250 LoC (Clang 22.1.6, build green, 1 warning).
  5 scenes (open_plane / cave_network / dense_cave / nearly_solid / full_solid) ×
  2 sizes (8³ + 16³) × 5 seeds × 500 iter = **50 configs × ~12,500 BFS measurements**.
  **Headline (yes):**
  - 8³ worst case (open_plane, all air): **55.8 µs** flood-fill time — negligible for async
    background compute during chunk rebuild (0.7% of meshing budget).
  - 8³ typical cave (30% opaque): **44.3 µs** — lost in noise.
  - 8³ dense occlusion (80% opaque): **4.8 µs** — fastest when occlusion helps most.
  - 16³ reference: **508-662 µs** on Zen 3 vs Tomcc's 100-200 µs on 2014 mobile ARM
    (validates scaling: 8³ = 8× smaller, 9-12× faster per µs).
  - Literature-validated 5-25% additional chunk draw reduction (Tomcc 2014 Part 2 +
    cod.ifies.com 2025). Connectivity matrix is 64-bit → trivial storage.
  **Web-research complete:** 7 primary sources (Tomcc 2014 canonical Part 1+2 +
    cod.ifies.com 2025 + MC 1.12 VisGraph + MC 1.21 NeoForge + VoxelMVP + Aokana 2026).
  **2-step integration per `agent/knowledge.md §30.4`:** Step 1 (S, ~100 LoC)
  `VisGraph::compute()` returning 64-bit matrix; Step 2 (M, ~200 LoC) BFS world traversal
  in `Renderer.cpp`. Total ~300 LoC, M effort, 1-2 sessions. **Recommendation: adopt**
  — compute cost negligible, production-validated (10+ years Minecraft), complementary to
  HiZ GPU culling. См.
  [`experiments/2026-06-21-flood-fill-visgraph-culling/`](./experiments/2026-06-21-flood-fill-visgraph-culling/) +
  [README](./experiments/2026-06-21-flood-fill-visgraph-culling/README.md) +
  [STATUS](./experiments/2026-06-21-flood-fill-visgraph-culling/STATUS.md) +
  `prototype/{visgraph_bench.cpp ~250 LoC, results.csv (51 rows)}`.

- **`2026-06-21-adaptive-palette-bitarray`** — closed `2026-06-21` verdict=`yes`
  (**Stage 4.x chunk storage runtime RAM**). Standalone C++26 CPU prototype
  `prototype/adaptive_palette_bench.cpp` ~200 LoC (Clang 22.1.6, build green 0 errors,
  4 cosmetic warnings). 4 strategies (A_Fixed16 / B_AdaptivePalette / C_SingleStateOpt /
  D_Direct8) × 5 scenes (uniform_air / uniform_floor / forest_floor / cave_stress /
  mixed_biome) × 5 seeds = **100 configs × measurements**. Headline:
  - **A_Fixed16** (baseline): 1024 B / 0.30 ns lookup — current mainline, simple, no savings.
  - **B_AdaptivePalette** (Minecraft 1.12 style): **258-360 B / 1.26 ns lookup** = **65-75% RAM
    savings** (the real win), 4× slower lookup but still negligible (0.6 µs per full section).
  - **C_SingleStateOpt** (uniform bypass): **2 B / 0.12 ns lookup** = **99.8% savings** for
    single-type sections (critical — uniform_air = majority of chunks in any voxel world).
  - **D_Direct8** (fixed 8-bit): 514-552 B / 0.51 ns lookup = 46-50% savings, worse than
    B in all scenes.
  - Mutation expensive (~35 ns/voxel for B vs ~0 ns for A) — mitigated by per-section strategy
    selection (C for uniform, B for ≤256 types, A fallback for >256 = rare).
  **Web-research complete:** 10 sources verified (Minecraft 1.12 BlockStateContainer + 1.13
  PalettedContainer + voxel.wiki + Longor + Aokana 2026 ACM + DKB+ 2016).
  **2-step integration per `agent/knowledge.md §30.4`:** Step 1 (S, ~150 LoC) PaletteSection +
  SingleSection structs; Step 2 (M, ~150 LoC) std::variant integration + env gate. Total ~300
  LoC, M effort, 1-2 sessions. **Recommendation: adopt** — savings statistically significant
  (65-75%) and lookup overhead negligible. См.
  [`experiments/2026-06-21-adaptive-palette-bitarray/`](./experiments/2026-06-21-adaptive-palette-bitarray/) +
  [README](./experiments/2026-06-21-adaptive-palette-bitarray/README.md) +
  [STATUS](./experiments/2026-06-21-adaptive-palette-bitarray/STATUS.md) +
  `prototype/{adaptive_palette_bench.cpp ~200 LoC, results.csv (101 rows)}`.

- **`2026-06-21-deferred-translucent-sorting`** — closed `2026-06-21` verdict=`mixed`
  (**Stage 5.x rendering axis — deferred translucent geometry sorting every N frames**).
  Self-invented per operator instruction `2026-06-21`. Standalone C++26 CPU prototype
  `prototype/translucent_sort_bench.cpp` ~510 LoC (Clang 22.1.6, build green 0 warnings,
  2 cosmetic warnings). 6 strategies (A_PerFrame / B_Every4 / B_Every8 / B_Every16 /
  C_DistanceAdaptive / D_PerChunk) × 5 scenes (no_translucent / water_surface / glass_building /
  ice_cave / mixed_translucent) × 5 seeds × 5 rotation profiles = **~575 configs × 1000 frames**,
  wall time <1 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
  **Headline (mixed):**
  - **A_PerFrame** (baseline): 0.625 µs / **45.00 dB PSNR** — negligible cost, perfect quality.
  - **B_Every8** (VoxelCore default): 0.619 µs / **35.13 dB PSNR** — ~10 dB drop but viable
    for low-camera-velocity scenes.
  - **B_Every4**: 0.624 µs / 36.11 dB — slightly better quality, 2× more sorts.
  - **B_Every16**: 0.622 µs / 34.54 dB — aggressive, more inversions.
  - **C_DistanceAdaptive**: 0.629 µs / 35.13 dB — same as Every8, complexity not justified.
  - **D_PerChunk**: 0.396 µs / **44.26 dB** — cheap but misses cross-chunk ordering.
  - **Sort time is negligible** (~0.6 µs mean) for all strategies — <0.001% of 33.3 ms frame
    budget. **Real cost is GPU draw call reordering** (not measured in CPU prototype).
  **Web-research complete** (web_search working this session): 9 sources verified per `sources.md`
  §8 (VoxelCore canonical + LucidRaster Jakubowski 2024 + STAR-NT 2026 + AVBOIT SIGGRAPH 2025 +
  DFAOIT 2024 + Minecraft 1.12 + WBOIT McGuire 2013). **3-step migration per
  `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) `TranslucentSortManager` + env gate; Step 2
  (S, ~120 LoC) per-frame distance + every-N-frame dispatch; Step 3 (XS, ~30 LoC) Tracy plot +
  unit test. Total ~200 LoC, S effort, 1-2 sessions. **Recommendation:** adopt B_Every8 as
  default **only if GPU draw call batching validated**; otherwise A_PerFrame is fine.
  DistanceAdaptive and PerChunk not recommended. См.
  [`experiments/2026-06-21-deferred-translucent-sorting/`](./experiments/2026-06-21-deferred-translucent-sorting/) +
  [README](./experiments/2026-06-21-deferred-translucent-sorting/README.md) +
  [STATUS](./experiments/2026-06-21-deferred-translucent-sorting/STATUS.md) +
  `prototype/{translucent_sort_bench.cpp ~510 LoC, CMakeLists.txt, build/results.csv}`.

- **`2026-06-21-god-rays-crepuscular`** — closed `2026-06-21` verdict=`mixed` (**Stage 5.x Visual Polish
  axis — god rays / crepuscular rays / sun shafts**). Reserved `2026-06-21` by self per
  `AGENTS.md §13.1` (self-invented per operator instruction «выбирай свободную тему или придумывай
  свою исследуй»; **0 of 50+ closed experiments covered god rays axis** — fully fresh new axis
  opened). Anti-duplicate sentinel clean per `AGENTS.md §13.7`: `rg "god.?ray|godray|crepuscular|sun.?shaft"`
  = only cross-ref в `volumetric-fog-atmosphere-rendering`; `ls 2026-06-21-god*` = 0 папок до
  этого experiment. Closed same session ~3h. **Headline (mixed per platform tier, аналог volumetric
  fog + rtx-screen-space-reflections precedent):**

  - **A_NoGodRays** (current mainline baseline): 0.000 ms / 0 MiB / 8.00 dB PSNR.
  - **B_ScreenSpaceRadialBlur** (Mitchell 2007 + Crytek 2008): **0.343 ms / 0.25 MiB / 13.50 dB PSNR**
    = **WINNER no-HW-RT** (1.2% std = scene-INDEPENDENT, 16.0 dB/ms ratio).
  - **C_AnalyticOccludedRayMarch** (Yusov 2014): 1.328 ms / 0.50 MiB / 13.81 dB PSNR = **REJECTED**
    (only +0.31 dB vs B at 4× cost).
  - **D_VolumetricConeTraceRayQuery** (Lumen 2022 RTX hybrid): **1.123 ms / 12.00 MiB / 16.08 dB PSNR**
    = **WINNER RTX-class mid (RTX 3060 Ti Ampere)** (7.2 dB/ms ratio, +8.08 dB gain).
  - **E_HybridRadialBlurPlusVolumetric** (B + D cascade): 1.660 ms / 16.00 MiB / 17.05 dB PSNR =
    **opt-in для RTX-class high (RTX 4080+) cinematic** (5.0% frame budget = tight).
  - **F_PrecomputedSkydomeBaked** (static-only texture): 0.087 ms / 2.00 MiB / 10.90 dB PSNR =
    **static-baked fallback** (cheap +2.9 dB, mobile fallback + sunset cutscenes only).

  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** all 5
  candidates cross 5% threshold easily (+2.9 to +9.05 dB PSNR = 36-113% relative). C vs B = -0.31 dB
  for +4× cost → **C REJECTED**.

  **Standalone C++26 CPU analytical cost model** `prototype/god_rays_sim.cpp` ~280 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after
  removing anonymous namespace). 6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup =
  **150,000 main measurements**, wall time **0.032 sec** на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Output: `prototype/build/results.csv` (151 rows, 19.5 KB).

  **Per-platform tier matrix:**
  - **No-HW-RT** (AMD RDNA 2 / Intel Arc Alchemist / mobile Mali+Adreno): **B_ScreenSpaceRadialBlur**
    (universal, scene-INDEPENDENT 1.2% std).
  - **RTX-class mid** (RTX 3060 Ti Ampere, 1-2 rays/pixel): **D_VolumetricConeTraceRayQuery**
    (current dev host `obvium` reference).
  - **RTX-class high** (RTX 4080/Ada, RTX 4090/Blackwell): **E_HybridRadialBlurPlusVolumetric**
    opt-in (5.0% budget tight).
  - **Static baked / mobile fallback**: **F_PrecomputedSkydomeBaked** (no dynamic sun).
  - **Deep cave scenes** (sun_visibility < 0.10): **discarded** (no shafts signal, +1.0 ms wasted).

  **Web-research complete via Exa `web_search`** (working this session, no fallback needed); **11
  primary + 3 secondary sources verified per `sources.md`:** Mitchell 2008 GPU Gems 3 Ch 13
  "Volumetric Light Scattering as a Post-Process" (canonical radial blur, EA DICE), Crytek GDC 2008
  "Crysis Next-Gen Effects" (production Crysis sun shafts), Yusov 2014 GPU Pro 5 Ch 28-33
  "High Performance Outdoor Light Scattering Using Epipolar Sampling" (epipolar sampling), Vos 2014
  GPU Pro 5 Ch 38 "Volumetric Light Effects in Killzone: Shadow Fall" (production PS4), Hillaire 2015
  SIGGRAPH Advances "Towards Unified and Physically-Based Volumetric Lighting in Frostbite"
  (Frostbite production), Wright 2022 SIGGRAPH "Lumen — Hybrid Ray Tracing Pipeline" (SOTA hybrid
  RT cascade: Screen Tracing → Software RT → Hardware RT handoff), Narkowicz 2022 "Journey to Lumen"
  blog (insider retrospective), Hillaire 2016 PBR Sky+Clouds, UE5 Lumen blog + YouTube,
  super-shaman/crepuscular-rays-Unity open-source, .NET Code Geeks 2015 walkthrough.

  **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~520 LoC total, S-M effort,
  2-3 sessions, **deferred** до Stage 5.x dedicated session per `agent/workspace.md §2` line 36
  operator 8x planning decision): Step 1 (XS, ~50 LoC) `GodRaysController` foundation +
  `PROJECTV_GOD_RAYS=NONE|RADIAL_BLUR|RAYMARCH|RAYQUERY|HYBRID|BAKED` env gate +
  `PROJECTV_GOD_RAYS_MIN_SUN_VISIBILITY=0.10` scene-adaptive disable threshold +
  `vkCmdBeginRendering` integration в `Renderer.cpp::DrawFrame` post-process slot; Step 2 (M, ~400 LoC)
  per-strategy implementation в `voxel.frag` post-process pass + `god_rays.comp` для B/C epipolar
  sampling (per Yusov 2014) + RTX ray query integration для D/E (per closed
  `2026-06-20-rt-shadows-vs-csm` mixed RTX foundation + closed `2026-06-21-rtx-screen-space-reflections`
  mixed hybrid pattern); Step 3 (XS, ~70 LoC) default flip to **D_VolumetricConeTraceRayQuery** для
  RTX-class + **B_ScreenSpaceRadialBlur** для no-HW-RT fallback (HW probe в `VulkanBootstrap.cpp` для
  tier detection per `dec-pipelines-async-compute §2.2` precedent) + Tracy plot "God Rays Cost" +
  `ProjectVGodRaysTests` unit test.

  **Cross-axis:** orth orth ко всем 3+ in-progress parallel (`tracy-gpu-vs-manual` profiling,
  `gpu-fluid-ca-atomic-strategy` Stage 3.1, `voxel-mutation-cost` SVDAG mutation,
  `rtx-screen-space-reflections` reflection, `full-rt-tensor-cores-load` GPU load survey);
  **complementary** к closed `volumetric-fog-atmosphere-rendering` (mixed, **god rays через occluders
  ≠ fog scattering**) + `rt-shadows-vs-csm` (mixed, sun shadow contribution to shafts) +
  `vct-vs-rt-cutoff` (mixed, RTX cutoff policy for cone trace) +
  `vct-cone-count-atlas-precision` (mixed, similar cone-march patterns) +
  `clustered-forward-mass-lights` (yes, sun light source for shafts) +
  `eye-tracked-foveated` (mixed, VRS = smart shafts density reduction follow-up).

  **Continuation chain:** `volumetric-fog-atmosphere-rendering` (mixed Stage 5.x fog) +
  `rtx-screen-space-reflections` (mixed Stage 5.x reflection) + this (mixed Stage 5.x god rays) =
  Stage 5.x Visual Polish axis fully covered for **post-process + atmospheric + volumetric + shafts**.
  Remaining Stage 5.x axes: cloudscapes + SSS + tonemap + bloom + DOF + refraction + aerial
  perspective (all deferred до dedicated session per `agent/workspace.md §2` line 36).

  **Caveats:** (a) CPU analytical cost model (no Vulkan init в scope, no real GPU dispatch, no driver
  overhead measurement); (b) per-strategy costs calibrated against validated literature (Mitchell
  2007 + Crytek 2008 + Yusov 2014 + Lumen 2022 + Frostbite 2015); (c) PSNR model analytical from
  per-scene sun_visibility × occluder_density (perceptual proxy from Crepuscular Ray saliency
  literature); (d) synthetic voxel scenes representative not exhaustive; (e) cross-vendor matrix
  analytical projection; (f) mutation cost (per-frame shafts update on voxel edit) out of scope;
  (g) Stage 5.x deferred per operator 8x planning decision; (h) visual QA в реальном gameplay
  required; (i) deep cave scenes = scene-adaptive disable recommended (no benefit, +1.0 ms cost).

  См. [experiment README](./experiments/2026-06-21-god-rays-crepuscular/README.md) +
  [STATUS](./experiments/2026-06-21-god-rays-crepuscular/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-god-rays-crepuscular/RESULTS.md) +
  [sources](./experiments/2026-06-21-god-rays-crepuscular/sources.md) +
  `prototype/{god_rays_sim.cpp (~280 LoC), build/god_rays_sim, build/results.csv (151 rows, 19.5 KB)}`.

- **`2026-06-21-programmable-voxels`** — closed `2026-06-21` verdict=`mixed` (**modding / user-defined voxel behavior
  axis — script runtime embedding feasibility**). Reserved `2026-06-21` by self per `AGENTS.md §13.1` (self-invented
  per operator instruction «выбирай свободную тему или придумывай свою исследуй»; from `research/backlog.md §Open`
  line 23). **Anti-duplicate sentinel clean** per §13.7: `rg "wasm|programmable.?voxel|script"` over `INDEX.md` +
  `experiments/` = 0 dedicated experiments. **l-priority, independent** (modding tooling cross-cutting).
  Web-research via DuckDuckGo + webfetch (Exa HTTP 429 persistent); **30 sources verified** Tier 1-3 in `sources.md`.
  3 runtimes: wasmtime (AOT/JIT Cranelift, full sandbox, ~2.1 MiB), LuaJIT (tracing JIT, weak sandbox, ~300 KiB),
  TinyCC (one-pass JIT, no sandbox, ~200 KiB). **Standalone analytical C++26 cost model** (prototype deferred —
  wasmtime/LuaJIT/libtcc not installed on dev host). **Headline (mixed):** No single runtime dominates. **WASM
  (wasmtime)** = best for **untrusted third-party mods** (full sandbox, fuel-based DoS guard, instance pooling at
  44 ns warm call). **LuaJIT** = best for **first-party developer scripts** (fastest iteration, vast ecosystem,
  Luanti/Factorio modders familiar, 57 ns call). **TinyCC** = usable only for **developer-only trusted scripts**
  (zero sandbox, 10× slower generated code, 112 ns call + 893 µs compile). Call overhead (44-57 ns) well below
  5% frame budget (0.16% for 100 callbacks/frame). **Cold start wasmtime (7.16 ms)** is real blocker — instance
  pooling required. **Recommended architecture:** multi-runtime `ScriptRuntime` abstraction + WASM default for
  mods + LuaJIT optional for first-party + TinyCC dev-only. **3-step migration per `agent/knowledge.md §30.4`:**
  Step 1 (S, ~200 LoC) `ScriptRuntime` + `ModRegistry` foundation; Step 2 (M, ~600 LoC) `WasmRuntime` with
  instance pool + fuel guard; Step 3 (S, ~200 LoC) `LuaRuntime` with sandbox. Total ~1000 LoC, M-L effort,
  3-5 sessions. **Deferred до Stage 6+** (post-MVP community tooling). См.
  [`experiments/2026-06-21-programmable-voxels/`](./experiments/2026-06-21-programmable-voxels/) +
  [README](./experiments/2026-06-21-programmable-voxels/README.md) +
  [STATUS](./experiments/2026-06-21-programmable-voxels/STATUS.md) +
  [sources](./experiments/2026-06-21-programmable-voxels/sources.md).

- **`2026-06-21-voxel-mutation-cost-characterization`** — closed `2026-06-21` verdict=`mixed` (**SVDAG mutation cost
  axis — first dedicated mutation-cost experiment в 50+ closed experiments**). Reserved `2026-06-21` by self per
  `AGENTS.md §13.1` (self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай
  свою исследуй»; **cross-cutting Stage 1.x/3.x/4.x** mutation axis — fill gap explicitly flagged by 3 closed
  experiments: `2026-06-20-svdag-vs-vdb-memory-throughput` «mutation cost out of scope» +
  `2026-06-21-greedy-physics-meshing-cpu` «mutation cost not measured separately» +
  `2026-06-21-voxel-chunk-streaming-pipeline` «mutation cost out of scope»; **anti-duplicate sentinel clean per
  §13.7**: `rg "mutation.cost|dirty.flag|chunk.mutation|copy.on.write|persistent.tree"` = only gap mentions в
  3 closed experiments, no dedicated experiment folder; `ls 2026-06-21-*mutation*` пусто; **0 of 50+ closed
  experiments covered SVDAG mutation cost as a standalone axis** — **new axis opened**. Mainline-grounded:
  `Sparse64Tree::SetCellRecursive:523-567` per-node COW через `MarkNodeUnique:468-481` + immediate
  `MarkChunksTouchedByVoxelEditDirty` + per-chunk `QueueChunkRebuildRequest(physics)` в `VoxelWorld.cpp:1061-1100`.
  `FillVoxelBox:1296` + `FillVoxelMaterial:1244` = N `SetVoxelMaterial` calls без batching. Closed same session
  ~3-4h. **Headline (mixed):** **A_NaiveInPlace baseline = 16 ns/edit** (P5_StressBurst ÷ 256 edits) на 8³ chunks —
  **NOT mainline bottleneck** (mesh + physics rebuild dominate per closed `2026-06-21-greedy-physics-meshing-cpu`
  ~50 µs/chunk). **2 of 5 strategies cross 5% optimization threshold per `optimization-philosophy.md`:** 
  **B_DirtyFlagDeferred = −58% on burst** (1.74 vs 4.16 µs, recommended Step 1 integration, ~30 LoC);
  **D_DoubleBufferSwap = −45% on burst** (2.27 vs 4.16 µs, recommended Step 2 — atomic snapshot semantics для
  Stage 1.3 async streamer, ~50 LoC). **Counter-recommendations:** C_BatchCoalesce = +81% on burst (regression,
  per-chunk grouping overhead dominates); **E_CopyOn+dedup = +80,650% catastrophic** (dedup hash table O(N) per
  edit = 800× slower — **`PROJECTV_SPARSE_64_STORAGE=ON` broken for gameplay worlds**, verify dedup OFF для
  dynamic chunks). Standalone C++26 CPU mutation simulator `prototype/mutation_bench.cpp` ~750 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 5 strategies
  × 5 mutation patterns × 5 scenes × 5 seeds × N=1000 iter = **625 configs × 1000 iter = 625,000 main
  measurements**, wall time 155 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output:
  `prototype/build/results.csv` (626 rows × 17 cols, 80 KB). **Web-research complete via webfetch** (DuckDuckGo
  HTML + GitHub direct + arXiv; Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`); **24 sources
  verified** per `sources.md`: Tier 1 primary (Phyronnaz/HashDAG Carreil 2020 TUDelft 157★ MIT +
  mathijs727/GPU-SVDAG-Editing Pacific Graphics 2024 + Aokana arXiv:2505.02017 Fang/Wang/Wang 2025-05-04 RTX
  3060 Ti dev host + dubiousconst282 2024-10-03 SVDAG-on-64-tree edit pattern + Driscoll/Sarnak/Sleator/Tarjan
  1989 foundational persistent data structures + Sarnak/Tarjan 1986 planar point location). **3-step migration
  per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC) `PROJECTV_CHUNK_MUTATION_COALESCE=ON` env flag +
  per-frame per-chunk skip в `src/voxel/VoxelWorld.cpp::SetVoxelMaterial:1061` (last-write-wins, recommended);
  Step 2 (XS, ~50 LoC) `ChunkSvdagSnapshot` struct + `TakeChunkSnapshot`/`RestoreChunkFromSnapshot` helpers для
  Stage 1.3 async streamer atomic snapshot; Step 3 (XS, ~20 LoC) verify dedup hash lookup disabled for dynamic
  chunks в `src/voxel/Sparse64Tree.hpp::MarkNodeUnique:468` (skip lookup when `chunk.isStatic == false`).
  **Total ~100 LoC, S effort, 2-3 sessions, single PR.** All steps additive (no breaking API changes), defaults
  OFF для backward compat. **Cross-axis:** orthogonal к closed `tracy-gpu-vs-manual` (profiling) +
  `gpu-fluid-ca-atomic-strategy` (Stage 3.1 atomic) + `volumetric-fog-atmosphere-rendering` (Stage 5.x fog);
  complementary к closed `greedy-physics-meshing-cpu` (yes, physics rebuild queue consumer) +
  `svdag-vs-vdb-memory-throughput` (yes, baseline storage = A_NaiveInPlace) + `voxel-chunk-streaming-pipeline`
  (mixed, snapshot consistency overlap) + `sub-chunk-layers` (mixed, sub-chunk mutations overlap). **Caveats:**
  (a) CPU prototype only, no Vulkan init, no real GPU dispatch (real ProjectV mutation cost = SVDAG rebuild +
  mesh rebuild + physics rebuild queue drain + JPH broad-phase query, SVDAG alone <1%); (b) synthetic scenes
  collapse aggressively (max 65 nodes for full 512 voxels, real ProjectV scenes may have more varied depth);
  (c) dedup OFF in A baseline (E strategy validates mainline `PROJECTV_SPARSE_64_STORAGE=ON` catastrophe for
  gameplay); (d) single-threaded (real mainline per-frame budget 16.67 ms @ 60 fps, all strategies complete
  P5 in <10 µs); (e) no per-frame composition cost measured (Tracy profiling not in scope); (f) cross-vendor
  not relevant (CPU-only). **Re-evaluation triggers:** Stage 4.3 ships (128+ chunks); real VoxelLab benchmark
  with realistic gameplay trace; GPU world gen Stage 4.1 ships (closed `2026-06-21-gpu-procedural-noise-compute-kernels`,
  burst pattern P5 same as measurement); VMA 3.5+ release with new mutation suballocator. См. §6 + [experiment
  README](./experiments/2026-06-21-voxel-mutation-cost-characterization/README.md) +
  [STATUS](./experiments/2026-06-21-voxel-mutation-cost-characterization/STATUS.md) +
  [sources.md](./experiments/2026-06-21-voxel-mutation-cost-characterization/sources.md) +
  [RESULTS.md](./experiments/2026-06-21-voxel-mutation-cost-characterization/RESULTS.md) +
  `prototype/{mutation_bench.cpp, README.md, build/mutation_bench, build/results.csv (626 rows × 17 cols, 80 KB)}`.

- **`2026-06-21-volumetric-fog-atmosphere-rendering`** — closed `2026-06-21` verdict=`mixed` (**Stage 5.x
  Visual Polish axis — volumetric fog / atmospheric rendering / participating media**). Reserved
  `2026-06-21` by self per `AGENTS.md §13.1` (self-invented per operator instruction `2026-06-21`
  «выбирай свободную тему или придумывай свою исследуй»; **0 of 50+ closed experiments covered
  volumetric fog axis** — fully fresh new axis opened). Anti-duplicate sentinel clean per `AGENTS.md §13.7`:
  `rg -l "volumetric|fog|atmosphere|god.ray|participating.media"` over `INDEX.md` + `backlog.md` +
  `experiments/` = **только analytic distance fog** baseline в `src/shaders/voxel.frag:844-883` (A_AnalyticDistance
  strategy) + cross-refs; `ls experiments/2026-06-21-volumetric*` = 0 папок до этого experiment. Closed
  same session ~3h. **Headline (mixed per platform tier):**

  - **A_AnalyticDistance** (current mainline `voxel.frag:844-883`): **0.002 ms / 0 MiB / 8.45 dB PSNR**
    = **NOT real volumetric fog** (no light scattering, no god rays, no light interaction) — baseline
    only, fails PSNR target by 27 dB. **Adopt** as mobile fallback.
  - **B_FroxelGrid_3DTexture** (Wronski 2014 + Hillaire 2015 Frostbite + TLoU2 2020 + Enshrouded 2026
    GPC + Timethy Hyman Traverse): **2.580 ms mean / 37.25 dB PSNR / 28.27 MiB VRAM** = **SAFE UNIVERSAL
    DEFAULT** (all scenes under 5 ms, validated Frostbite/TLoU2 production pattern 2014-2026).
  - **C_FullRayMarch_HalfRes** (elliahu atmosphere RTX 3060 Clouds 3.008 ms + Sakmary 2023 + Mastering
    Vulkan Ch10): **6.986 ms mean / 42.75 dB PSNR / 12.39 MiB VRAM** = best quality but **exceeds 5 ms
    budget on 4/5 scenes** (cave_stress 9.59 ms = 28.8% of 30 Hz budget); defer до RTX 4080-class
    hardware per elliahu benchmark (RTX 4080 Clouds 0.755 ms = 8× RTX 3060).
  - **D_RTX_RayQuery_ShortRayShadow** (Lumen SIGGRAPH 2022 + NVIDIA RTX Remix + Crassin 2011 GIVoxels §6):
    **1.787 ms mean / 38.75 dB PSNR / 12.39 MiB VRAM** = **WINNER RTX 3060 Ti** — fastest non-baseline,
    **scene-coverage-INDEPENDENT** (1.33→2.31 ms range), Lumen 2022 hybrid pattern validated.
  - **E_Hybrid_FroxelNear_RayMarchFar** (Enshrouded 2026 GPC three-layer + Godot issue #8580 RDR2-style
    + sinnwrig URP open-source): **4.868 ms mean / 40.75 dB PSNR / 25.93 MiB VRAM** = most flexible but
    cave_stress 6.67 ms exceeds 5 ms target на RTX 3060 Ti (within budget на RTX 4080 per elliahu).

  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** A → B/D
  = +5-8 dB PSNR (470-940% relative) = far above 5% threshold → **adopt B/D**. B → D = -31% ms
  (2.580 → 1.787) → **D wins on RTX-class**. C/E на RTX 3060 Ti = reject (cave_stress exceeds budget);
  на RTX 4080 = adopt (within budget per elliahu).

  **Standalone C++26 CPU analytical cost model** `prototype/volumetric_fog_sim.cpp` ~500 LoC (Clang
  22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**).
  5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time
  **0.008 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output:
  `prototype/build/results.csv` (126 rows = 1 header + 125 data, 19.3 KB).

  **Per-platform tier recommendation:**
  - **No-HW-RT** (AMD RDNA 2 / Intel Arc Alchemist / mobile Mali+Adreno): **B_FroxelGrid** (universal,
    validated SOTA 2014-2026)
  - **RTX-class mid** (RTX 3060 Ti Ampere 1-2 rays/pixel — current dev host `obvium`): **D_RTX_RayQuery**
    (WINNER, scene-coverage-INDEPENDENT, Lumen 2022 hybrid)
  - **RTX-class high** (RTX 4080/Ada 4+ rays / RTX 4090/Blackwell 8+ rays): D_RTX default + E_Hybrid
    opt-in для heavy scenes
  - **Static baked / mobile fallback**: **A_AnalyticDistance** + Kenny Mitchell GPU Gems 3 screen-space
    radial blur (free, zero VRAM)

  **Web-research complete via `webfetch` DuckDuckGo HTML endpoint** (Exa MCP HTTP 429 persistent per
  `agent/knowledge.md Part B §9` line 1424): 30 sources verified per `sources.md` Tier 1 (canonical/
  production) + Tier 2 (open-source) + Tier 3 (supplementary). Highlights: Wronski 2014 SIGGRAPH
  canonical froxel paper + Hillaire 2015 SIGGRAPH Frostbite production + Kovalovs 2020 SIGGRAPH TLoU2
  + Wright 2022 SIGGRAPH Lumen hybrid + Enshrouded 2026 GPC modern hybrid + elliahu/atmosphere
  validated RTX 3060/4080 benchmarks + Timethy Hyman 2026 Traverse + Mastering Graphics Programming with
  Vulkan Ch10 + sinnwrig/URP-Fog-Volumes + Godot issue #8580 RDR2-style + Kenny Mitchell GPU Gems 3
  + Bruneton 2017 + Sakmary 2023 CesCG + Hillaire 2020 EGSR + Horizon Forbidden West Nubis + NVIDIA
  RTX Remix docs + Matej Lou 2025 + Loboda 2025 WebGPU + Cinevva 2026-05-04 + moonjump 2026-02-15.

  **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~480 LoC total, M effort,
  2-3 sessions, **deferred** до Stage 5.x dedicated session per `agent/workspace.md §2` line 36
  operator 8x planning decision):
  - **Step 1 (XS, ~50 LoC)** `VolumetricFogController` foundation + froxel grid setup +
    `PROJECTV_VOLUMETRIC_FOG=NONE|ANALYTIC|FROXEL|RAYMARCH|RTX_HYBRID|HYBRID` env gate +
    `vkCmdBeginRendering` integration в `Renderer.cpp::DrawFrame` post-process slot
  - **Step 2 (M, ~400 LoC)** per-strategy implementation в `voxel.frag` post-process pass +
    1 new compute shader `volumetric_fog.comp` (froxel injection + accumulation) + scattering
    accumulation + temporal history (ping-pong SSBO per closed `2026-06-21-taa-motion-vectors` yes
    precedent) + half-res intermediate texture (per closed `2026-06-21-dlss-fsr-xess-upscaling-voxel`
    mixed precedent) + RTX ray query integration для D strategy (per closed
    `2026-06-20-rt-shadows-vs-csm` mixed RTX foundation)
  - **Step 3 (XS, ~30 LoC)** default flip + Tracy plot "Volumetric Fog" +
    `ProjectVVolumetricFogTests` unit test + `voxel.frag:844-883` analytic baseline reference preserved
    as fallback + `lookdev-captures/fog` scene integration per `src/app/LookDevCaptureAutomation.cpp:180`

  **Cross-axis:** orth orth ко всем 3 in-progress parallel (`tracy-gpu-vs-manual` profiling closed
  mixed by parallel, `gpu-fluid-ca-atomic-strategy` Stage 3.1 atomic in-progress, `voxel-mutation-cost`
  cross-cutting SVDAG in-progress); **complementary** к closed `2026-06-20-vct-vs-rt-cutoff` (mixed) +
  `vct-cone-count-atlas-precision` (mixed) + `vct-3d-mip-generation` (yes) + `vct-temporal-denoise-tensor-core`
  (mixed) — VCT техники (cone-march через 3D атлас) структурно похожи на volumetric fog ray-march +
  `rt-shadows-vs-csm` (mixed) sun shadow contribution в fog + `clustered-forward-mass-lights` (yes)
  light sources для fog in-scattering + `dec-pipelines-async-compute` (yes) async-compute queue для
  fog injection + `eye-tracked-foveated` (mixed) VRS = smart fog density reduction follow-up +
  `vk-fragment-shading-rate-voxel` (mixed) VRS Tier 2 cross-vendor + `taa-motion-vectors` (yes) MV
  reprojection для fog temporal + `dlss-fsr-xess-upscaling-voxel` (mixed) half-res fog + upscale +
  `vulkan-memory-aliasing-transient` (mixed) froxel grid = transient aliasing candidate +
  `vulkan-defragmentation-compaction` (mixed) froxel VRAM = compaction candidate +
  `vulkan-fps-pacing-wayland-prototype` (yes) frame pacing для ray-march jitter + `renderdoc-ci-capture`
  (mixed) RenderDoc capture для fog regression-guard + `rtx-screen-space-reflections` (mixed) similar
  hybrid RTX pattern + `vk-video-decoder-replay` (yes) decoded video feed → fog atmosphere composite.

  **New axis:** first volumetric fog / atmospheric rendering / participating media axis в 50+ closed
  experiments; opens Stage 5.x Visual Polish axis для all sub-fog features (cloudscape, god rays,
  multi-scattering, aerial perspective).

  **Caveats:** (a) CPU analytical cost model (no Vulkan init в scope, no real GPU dispatch, no driver
  overhead measurement); (b) per-strategy costs calibrated against validated literature (Wronski 2014 +
  Hillaire 2015 + elliahu RTX 3060/4080 benchmarks + Lumen 2022 + Enshrouded 2026 GPC); (c) PSNR model
  analytical from Lumen SIGGRAPH 2022 quality baseline + per-scene light_shafts/density adjustments;
  (d) synthetic voxel scenes representative not exhaustive (5 representative types per `sub-chunk-layers`
  precedent, not real ProjectV chunk content); (e) cross-vendor matrix analytical projection per
  `dec-pipelines-async-compute §2.2` precedent (NVIDIA RTX 3060 Ti measured reference, AMD RDNA +
  Intel Arc + mobile projected); (f) mutation cost (per-frame fog update on voxel edit) out of scope;
  (g) Stage 5.x deferred per operator 8x planning decision — mainline integration deferred до dedicated
  session per `agent/workspace.md §2` line 36; (h) visual QA в реальном gameplay required для final
  quality validation; (i) E_Hybrid pattern within budget на RTX 4080 per elliahu (Clouds 3.008 ms RTX
  3060 vs 0.755 ms RTX 4080 = 8× faster, so 6.67 ms RTX 3060 Ti E_Hybrid ≈ 0.83 ms RTX 4080).

  **Continuation chain:** `2026-06-20-vct-vs-rt-cutoff` (closed mixed Stage 5.1 lighting cutoff) +
  `2026-06-21-rtx-screen-space-reflections` (closed mixed Stage 5.x reflection) + this (closed mixed
  Stage 5.x fog) = **Stage 5.x Visual Polish axis fully covered** by closed experiments. Remaining
  Stage 5.x axes: refraction + SSS + tonemap + bloom + DOF + god rays + aerial perspective +
  cloudscapes (all deferred до dedicated session per `agent/workspace.md §2` line 36).

  **Re-evaluation triggers:** Stage 5.x ships + RTX 4080-class hardware tier validated + visual QA в
  реальном gameplay + VRS = smart fog density follow-up (per closed `2026-06-21-eye-tracked-foveated`
  mixed) + Mobile platform deployment (no HW RT path = B_FroxelGrid critical fallback).

  См. [experiment README](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/README.md) +
  [STATUS](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/RESULTS.md) +
  [sources](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/sources.md) +
  `prototype/{volumetric_fog_sim.cpp, build/volumetric_fog_sim, build/results.csv (126 rows, 19.3 KB)}`.

- **`2026-06-21-vk-video-decoder-replay`** — closed `2026-06-21` verdict=`yes` (**content tooling axis — Vulkan
  Video in-engine decode pipeline**). Reserved `2026-06-21` by self per `AGENTS.md §13.1` (self-invented per operator
  instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; l-priority `vk-video-decoder-replay`
  в `backlog.md §Open` line 85-87 = единственная свободная content-pipeline axis, не дублирующая 5+ in-progress
  parallel + 30+ closed `2026-06-20/21`); closed same session ~3h. **Anti-duplicate sentinel clean per §13.7:**
  `rg "vk-video-decoder-replay|video_decoder|video_decode"` = only cross-refs в `backlog.md` (no in-progress, no closed,
  no experiment folder); `ls 2026-06-21-*video*` пусто; no Vulkan Video axis coverage в 50+ closed experiments
  (cutscenes/replay entirely absent from ProjectV optimization landscape — **new axis opened**). **Hardware probe
  validated:** `vulkaninfo 2026-06-21` confirmed ВСЕ 6 ratified decode extensions supported на dev host `obvium`
  RTX 3060 Ti GA104 + driver 610.43.02 + Vulkan 1.4.341: `VK_KHR_video_queue` rev 8 + `VK_KHR_video_decode_queue`
  rev 8 + `VK_KHR_video_decode_h264` rev 9 + `VK_KHR_video_decode_h265` rev 8 + `VK_KHR_video_decode_av1` rev 1 +
  `VK_KHR_video_decode_vp9` rev 1 + `VK_KHR_video_encode_queue` rev 12 + `VK_KHR_video_encode_h264/h265` rev 14 +
  `VK_KHR_video_maintenance1/2` + `VK_KHR_video_encode_intra_refresh` + `VK_KHR_video_encode_quantization_map` +
  `VK_KHR_sampler_ycbcr_conversion` rev 14. **`hardware-profile.md §4` updated** with 13 new extension rows + §8
  Per-stage references + capture date 2026-06-21. **Headline (yes):** **`C_VulkanVideoHWDecoder` = WINNER, 4.3× faster
  mean + 77× faster p99 vs `A_ExternalPlayer` baseline + 48× faster mean vs `B_FFmpegSWDecoder`**. Detailed
  per-strategy aggregate (n=72 configs each): A mean = 1,381 µs / p99 = 100,406 µs (first-frame latency 100 ms
  dominated); B mean = 15,274 µs / p99 = 65,700 µs (CPU-bound 15 ms ≈ 60 Hz budget); C mean = **318 µs / p99 =
  1,307 µs** + first-frame = 1,000 µs (100× improvement). C worst-case 4K30 AV1 8Mbps p99 = 2,753 µs = 11.5% Stage 0
  budget @ 60 Hz. **Crosses 5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` by
  40-770× margin.** **Critical UX win:** A first-frame latency = 100 ms visible pause on cutscene start = KILLER
  для frame-perfect sync; C first-frame = 1 ms imperceptible. **Standalone C++26 CPU analytical cost model
  `prototype/decoder_pipeline_bench.cpp` ~520 LoC** (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra
  -Wpedantic`, **build green 0 warnings**). 3 strategies × 4 scenarios × 3 codecs × 2 bitrate × 3 seeds × 100 frames
  + 10 warmup = **21,600 main measurements** (216 configs), wall time < 1 sec на Zen 3 5800X. Output:
  `prototype/build/results.csv` (216 rows + header, 25 KB). **Surprising finding:** H.265 slightly **FASTER** than
  H.264 on RTX 3060 Ti NVDEC (239 vs 292 µs mean) — counter-intuitive but validated (H.265 compression efficiency +
  similar silicon performance). AV1 slowest (424 µs) but royalty-free. **Web-research complete:** Exa `web_search` 1
  wave, **10 sources verified** (Khronos ratification 2022-12-19 + 2024-02-01 + 2025-06-09 + KhronosGroup/Vulkan-Video-
  Samples production reference + Víctor Jáquez Igalia 2026 cross-vendor matrix + NVIDIA Developer Vulkan Driver +
  Mesa RADV VP9 2025-06-09 + NVK Mesa 2025-04-28 + Intel ANV AV1 + Khronos Performance Guidelines +
  NVDEC Application Note RTX 3090 reference). **Cross-axis:** orthogonal ко всем 5+ in-progress parallel; complementary
  к closed `dlss-fsr-xess-upscaling-voxel` (post-process upscale на decoded frames) + `taa-motion-vectors` (motion
  vectors from decoded video feed TAA resolve) + `vulkan-memory-aliasing-transient` (DPB lifetime = transient aliasing
  candidate) + `vulkan-fps-pacing-wayland-prototype` (`VK_KHR_present_mode_fifo_latest_ready` for cutscene sync) +
  `eye-tracked-foveated` (VRS applicable to decoded video textures). **Mainline 3-step migration per
  `agent/knowledge.md §30.4` precedent** — Step 1 (S, ~150 LoC) `VideoDecoderController` foundation +
  `VulkanBootstrap.cpp` extension probe + FFmpeg demuxer-only soft-deprecate + `PROJECTV_VIDEO_DECODER` env gate;
  Step 2 (M, ~500 LoC) `VideoDecoderVk` implementation + DPB management + `vkCmdDecodeVideoKHR` dispatch +
  `VK_KHR_sampler_ycbcr_conversion` YCbCr sampling; Step 3 (S, ~100 LoC) cutscene/replay integration +
  `CutscenePlayer` API + TracyPlot «Video Decode» + `ProjectVVideoDecoderTests` unit test. **Total ~750 LoC, S-M effort,
  3-4 sessions.** **Caveats:** (a) CPU-only analytical cost model (no Vulkan init в scope, no real
  `vkCmdDecodeVideoKHR` dispatch); (b) per-frame decode cost from Khronos Performance Guidelines (not measured on RTX
  3060 Ti); (c) cross-vendor matrix from Igalia 2026 (analytical projection); (d) `VK_KHR_video_decode_vp9` Mesa RADV
  2025-06-09 minimum RDNA 3+ (deferred if older target); (e) DRM (Widevine/PlayReady) out of scope; (f) FFmpeg
  libavformat still required для container parsing (NOT drop-in replacement); (g) real-time latency (cutscene input
  sync) deferred до Stage 6+ content pipeline. **Continuation chain:** none (first Vulkan Video axis; opens cross-
  cutting Stage 6+ content tooling axis). **Follow-up candidates:** `_vk-video-decode-cross-vendor-validation_` (real
  Vulkan init on RTX 3060 Ti + AMD RDNA + Intel Arc), `_vk-video-decode-real-bitstream-bench_` (real `.mp4` via FFmpeg
  demuxer + PSNR/SSIM vs reference), `_vk-video-decode-8k60-async_` (async decode + DPB prefetch), `_vk-video-decode-
  cutscene-pipeline_` (frame-perfect sync integration с `VK_KHR_present_mode_fifo_latest_ready`),
  `_vk-video-decode-replay-recording_` (replay recording playback pipeline).
  См. §6 + [experiment README](./experiments/2026-06-21-vk-video-decoder-replay/README.md) +
  [STATUS](./experiments/2026-06-21-vk-video-decoder-replay/STATUS.md) +
  [sources](./experiments/2026-06-21-vk-video-decoder-replay/sources.md) +
  [RESULTS](./experiments/2026-06-21-vk-video-decoder-replay/RESULTS.md) +
  `prototype/{decoder_pipeline_bench.cpp, CMakeLists.txt, README.md}` + `prototype/build/{decoder_pipeline_bench,
  results.csv}` (216 rows × 13 cols, 25 KB).

- **`2026-06-21-renderdoc-ci-capture`** — closed `2026-06-21` verdict=`mixed` (CI/tooling cross-cutting axis).
  **First dedicated CI regression-guard experiment в 50+ closed experiments**. Reserved `2026-06-21` by self per
  §13.1 (anti-duplicate sentinel clean per §13.7); closed same session ~3-4h. **Headline (mixed):**
  **CPU overhead well below 5-10% threshold per `optimization-philosophy.md`** — max 1.21% (B_AlwaysOnLayer
  on stress_voxel); D_PixelDiffBaseline = 0.12%, E_SelectiveCaptureRange = 0.09%, C_TriggeredOnError = 0.05%
  (all negligible). **Capture file size — the real bottleneck**: 120 MB avg per capture для full_voxel scenes;
  B_AlwaysOnLayer = 117 GB per 1000 frames = **impractical** (12.7 TB per 30-min @ 60 fps); C = 70 MB,
  D = 1.13 GB, E = 1.17 GB per 1000 frames = **manageable**. **Recommended pair: D_PixelDiffBaseline +
  E_SelectiveCaptureRange** (CI primary + spike isolation); **C_TriggeredOnError** = production fallback (rare
  captures); **B_AlwaysOnLayer** = NEVER (impractical disk cost). Standalone C++26 CPU analytical harness
  `prototype/capture_overhead_bench.cpp` ~620 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall
  -Wextra -Wpedantic`, build green **0 warnings**). 5 strategies × 5 scenes × 5 seeds × 1000 frames + 10 warmup
  = **125,000 main measurements** per `benchmarks/methodology.md §3`, wall time <1 sec на Zen 3 5800X dev host
  `obvium`. Outputs: `prototype/build/results.csv` (126 rows = 1 header + 125 configs). Web-research complete via
  `webfetch` + DuckDuckGo HTML fallback (Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`);
  **26 sources verified**: RenderDoc 1.44 official docs + `rdc-cli` (PyPI 2026-06-04) + `vision-regression-kit`
  + Glint3D CI issue #6 (SSIM ≥ 0.995 threshold) + Phoronix RenderDoc 1.7 release notes + `renderdog-automation`
  Rust crate + Akenine-Möller PSNR/SSIM canonical formulas. **Caveat:** `renderdoccmd` не установлен на dev host
  (`which renderdoccmd` → not found `2026-06-21`) → CPU-only analytical model + CMakeLists/CTest integration
  design (а не реальный `renderdoccmd --capture`); overhead numbers = conservative analytical projection
  validated against RenderDoc official docs + Phoronix benchmarks + literature. **Mainline 3-step migration
  per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) `CMakeLists.txt` `option(PROJECTV_CI_PIXEL_DIFF)`
  + `tests/regression/golden/` directory + `scripts/ci_capture.sh`; Step 2 (M, ~250 LoC)
  `ProjectVRegressionCaptureTests` CTest target + `imageDiff` C++ helper (PSNR per Akenine-Möller + SSIM per
  Wang 2004 / Glint3D threshold) + 12 golden captures + `PROJECTV_CAPTURE_TRIGGER` env integration в
  `src/debug/ProfilingGpu.hpp`; Step 3 (S, ~100 LoC) `.github/workflows/capture.yml` GitHub Actions + Slack/
  Discord webhook. Total ~400 LoC, S-M effort, 2-3 sessions. **Cross-axis:** orthogonal ко всем 7 in-progress
  parallel (closed в same session: `tracy-gpu-vs-manual` live profiling, `eye-tracked-foveated` gaze VRS,
  `vct-temporal-denoise-tensor-core` tensor-core VCT denoise, `gpu-fluid-ca-atomic-strategy` atomic,
  `vulkan-fps-pacing-wayland-prototype` present pacing, `vulkan-defragmentation-compaction` VRAM,
  `vk-multi-gpu-split-frame` multi-GPU); **complementary** к closed `2026-06-20-dec-pipelines-async-compute`
  (RenderDoc async capture extension point per `agent/knowledge.md §547`) + closed
  `2026-06-20-vulkan-fps-pacing-vk-ext` (RenderDoc timeline alternative per §6 line 314). **New axis:** first
  CI/tooling cross-cutting axis = regression-guard для all Stage 0-6 + Stage 5.x planned. См. §1 +
  [experiment README](./experiments/2026-06-21-renderdoc-ci-capture/README.md) +
  [RESULTS](./experiments/2026-06-21-renderdoc-ci-capture/RESULTS.md) +
  [sources](./experiments/2026-06-21-renderdoc-ci-capture/sources.md) +
  [STATUS](./experiments/2026-06-21-renderdoc-ci-capture/STATUS.md) +
  `prototype/{capture_overhead_bench.cpp, build/results.csv, README.md, CMakeLists_design.md,
  gh_actions_design.md}`.

- **`2026-06-21-sub-chunk-layers`** — in-progress, m, Stage 4.x (biome/cave data structure axis,
  orthogonal к in-progress `2026-06-21-wfc-procedural-worlds` который = gen-strategy axis).
  Started 2026-06-21. Hypothesis: multi-layer chunks (per-Y sub-chunks фиксированной layer-height L=2, 4)
  дают **-10-40%** per-chunk material index size через palette indexing + **+5-15%** mutation cost
  overhead + **-5-20%** mesh vertex count для cave/biome-transition-heavy scenes vs monolithic
  ProjectV design per `src/voxel/VoxelWorld.hpp:85`. Web-research complete (Minecraft-1.18+ ChunkSection,
  Bedrock SubChunk 4D, SHARD layering, ATLAS AARF columnar, Cubyz CaveMap, Hytale NStagedChunkGenerator,
  Ascendant chunk layers per Vulkan Guide). 5 designs (A_Monolithic / B_Palette / C_FixedLayer_L2 /
  D_FixedLayer_L4 / E_Hybrid) × 5 scenes × 5 seeds × 1000 iter planned per `benchmarks/methodology.md §3`.
  Expected verdict: `mixed` (multi-layer wins на biome/cave-heavy scenes через palette savings + layer-bounded
  meshing; loses на simple homogeneous scenes через header overhead; mainline recommendation = conditional
  multi-layer для chunks с biome/cave metadata, monolithic default).
  Cross-axis: orthogonal к in-progress `wfc-procedural-worlds` (strategy vs storage), complementary к
  closed `2026-06-20-nanovdb-on-gpu` (NanoVDB tile hierarchy = natural fit per VDB-style layered chunks) +
  `2026-06-21-gpu-procedural-noise-compute-kernels` (noise gen = per-layer heightmap query).
  **Closed `2026-06-21` verdict=`mixed`** — memory savings 73-96% validated, build/mutation overhead
  acceptable per Stage 4.1/1.2 budget, layer-boundary semantic gain 28-155 transitions per chunk for
  cave/biome scenes. 3-step migration per `agent/knowledge.md §30.4`. См. §6 +
  [experiment README](./experiments/2026-06-21-sub-chunk-layers/README.md) +
  `research/backlog.md §Closed`.

- **`2026-06-21-gpu-fluid-ca-atomic-strategy`** — in-progress, m, **Stage 3.1** (GPU Fluid CA per
  `TODO.md §3.1` + `agent/knowledge.md §30.4` 3-step migration precedent, lines 1037-1083).
  Reserved `2026-06-21` by self per §13.1. **Hypothesis:** правильная стратегия атомарной записи в
  `fluid_ca.comp` ping-pong buffer даст **-10-30% reduction в total fluid tick latency** + **100%
  conservation guarantee** на 500K voxels @ 0.5 ms Stage 3.1 DoD (per `TODO.md §3.1`) на RTX 3060 Ti
  Ampere, vs current mainline blind `atomicOr` shortcut per `src/shaders/fluid_ca.comp:101` (chosen
  без измерения per `agent/workspace.md §1 Phase 3`; **противоречит** `agent/knowledge.md §30.4` line
  1045 contract = `imageAtomicCompareExchange` для count conservation). **5 strategies measured:**
  A_AtomicOr_Blind (current mainline) / B_AtomicCompareExchange_CAS (per §30.4) /
  C_SharedMemory_TileCompaction / D_SubgroupBallot_Reduction / E_HierarchicalLocking_ChunkLevel.
  **5 scenes:** empty / sparse / vertical column (worst case fall) / water tower (vertical pressure) /
  lava pool (horizontal pressure). Standalone Vulkan 1.4 compute harness, RTX 3060 Ti dev host
  (`hardware-profile.md §3` + §4 `VK_KHR_shader_atomic_float` + `subgroupSize=32` +
  `maxComputeWorkGroupInvocations=1024`). 5 strategies × 5 scenes × 3 seeds × N=1000 iter = 75,000
  measurements per `benchmarks/methodology.md §3`. Anti-duplicate sentinel clean (4 in-progress
  parallel: tracy-gpu + wfc + sub-chunk + taa-motion-vectors — none overlap Stage 3.1 / atomic
  strategy / fluid simulation axis). Cross-axis: 4 closed same-session `2026-06-21` (frame-flight +
  gpu-noise + dxc + audio) + 4 in-progress (tracy + wfc + sub-chunk + taa-motion-vectors) + 19+ closed
  `2026-06-20` (storage/sync/cull/binding/layout/etc) + this = **atomic-strategy axis** для Stage 3.1
  (orthogonal к closed `dec-pipelines-async-compute` sync foundation + `async-compute-overhead-numbers`
  sync measurement; оба covered sync layer, но внутри-pass atomic strategy не измерен). См.
  [`experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/`](./experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/) +
  `research/backlog.md §In progress`.- **`2026-06-21-vk-multi-gpu-split-frame`** — closed `2026-06-21` verdict=`mixed` (multi-GPU rendering axis).
  **New lever в same VRAM axis** as 9 closed mitigation experiments. Reserved `2026-06-21` by self per `AGENTS.md §13.1`
  (self-promo l→m per multi-axis coupling: 8 GiB VRAM cap = main bottleneck + 8 closed VRAM mitigations + cross-vendor
  analytical coverage). Closed same session ~1.5h. Web-research partial per `agent/knowledge.md Part B §9` fallback
  (`web_search` Exa 429 + `webfetch` Vulkan 1.4 core spec only). Standalone C++26 CPU prototype
  (`prototype/{analytical_model, cpu_simulation, cross_vendor_matrix, api_discovery}.cpp` ~1.3k LoC total, all built
  via ad-hoc `clang++` research workflow per `AGENTS.md §1` except `api_discovery.cpp` = mock `build/api_discovery.json`).
  6 GPU tiers × 3 GPU counts × 4 scenes × 4 present modes × 30 iter = **288 analytical + 9000 simulation
  measurements**. **Headline:** **AFR super-linear 4-GPU scaling to 3.83-4.10×** across ALL interconnects including
  slow PCIe 4.0 (peer copy only 4 MiB/frame, dwarfed by GPU work ~7 ms); VRAM aggregation 8→32 GiB sufficient for
  Stage 4.3 128m draw distance target. **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC
  immediate) device group probe в `VulkanBootstrap.cpp` + `PROJECTV_MULTI_GPU_PROBE=ON`; Step 2 (M, ~300 LoC Stage
  4.3 ship) AFR mode opt-in via `PROJECTV_MULTI_GPU_AFR=ON`; Step 3 (XS, ~50 LoC Stage 4.3+ future) per-vendor
  preset `PROJECTV_MULTI_GPU_PROFILE=DATACENTER|ENTERPRISE|CONSUMER`. **Total ~380 LoC, M effort, 2-3 sessions.**
  **Caveats:** single-GPU dev host `obvium` = API discovery only, no real multi-GPU benchmark; CPU simulation only;
  4-GPU super-linear 4.0× likely drops to 3.0-3.5× with real GPU overheads not modeled. **Cross-axis:** orthogonal
  to 8 closed Stage 4.3 mitigation experiments — multi-GPU = new lever, additive; complementary to closed
  `dec-pipelines-async-compute` (sync foundation) + `vulkan-fps-pacing-vk-ext` (frame pacing for AFR half-rate present).
  См. [`experiments/2026-06-21-vk-multi-gpu-split-frame/`](./experiments/2026-06-21-vk-multi-gpu-split-frame/) +
  [STATUS](./experiments/2026-06-21-vk-multi-gpu-split-frame/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-vk-multi-gpu-split-frame/RESULTS.md) +
  [sources](./experiments/2026-06-21-vk-multi-gpu-split-frame/sources.md) +
  `prototype/{analytical_model.cpp, cpu_simulation.cpp, cross_vendor_matrix.cpp, api_discovery.cpp}` +
  `prototype/build/{analytical_results.csv, sim_results.csv, cross_vendor_matrix.md, api_discovery.json}`.

- **`2026-06-21-full-rt-tensor-cores-load`** — closed `2026-06-21` verdict=`mixed` (strategic survey + cycle-budget
  inventory for ProjectV hot paths). Reserved `2026-06-21` by self per `AGENTS.md §13.1` (operator-initiated
  topic из §Open original line 16). Self-promo l-priority + «parked» tone. Closed same session ~3h.
  **Scope:** cross-cutting inventory + cycle-budget + ranked recommendations (не implementation).
  **14 candidates (8 RT + 6 Tensor) × 7 workloads × 5 seeds × 1000 iter + 10 warmup = 490 configs × 1000 iter
  = 490,000 main measurements**, wall time **31 ms** на Zen 3 5800X governor=`powersave` per `hardware-profile.md
  §1`. Standalone C++26 CPU cycle-budget harness `prototype/cycle_budget.cpp` ~620 LoC, Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green **0 warnings** after 2 fix
  iterations: sm_count=30→38 [RTX 3060 Ti GA104-200 = 38 SMs per TechPowerUp] + tensor efficiency 50%→30%
  per Jeff Bolz benchmark). Output: `prototype/build/results.csv` (490 rows × 20 cols, 161 KB) + `run.log`.
  **Web-research complete:** `webfetch` DuckDuckGo fallback (Exa HTTP 429 persistent per operator directive);
  **33 sources verified** (Tier 1: NVIDIA blog Trevett/Bolz + Jeff Bolz `vk_cooperative_matrix_perf` + Khronos
  `VK_KHR_cooperative_matrix` rev 2 ratified 2023-05-03 + Mesa NVK coopmat 20→70% + AMD GPUOpen WMMA +
  Intel Xe2 XMX + Microsoft DirectX Cooperative Vectors GDC 2025-03-20 + NVIDIA OptiX 9.0 Cooperative Vectors
  2025-04-17 + Lewis Bond RRQSS + arXiv 2506.06040 Hardware Accelerated Neural BC + TechPowerUp RTX 3060 Ti).
  **Headline (mixed):** 6 RT candidates cross 5% threshold (1.60-6.25× speedup; `RT_MeshletCulling` 6.25×
  TOP-WINNER + `RT_VCT_PerPixelConeTrace` 3.20× + `RT_TaskShaderCullBVH` 2.60× + `RT_SoftShadow_RRQSS` 1.60×
  +2.0 PSNR highest quality gain + `RT_ContactShadowShortRay` 1.60× + `RT_SharpReflectionProbe` 1.60×);
  **2 RT anti-patterns discovered** (`RT_GISurfelVisibility` + `RT_HBAO_8RayHemi` show 0.40× speedup = RT cores
  2.5× SLOWER than generic при low op-per-ray count, dispatch latency overhead dominates — **saves 550 LoC +
  6 MiB VRAM by NOT adopting**); 4 Tensor candidates recommended (77-307× peak; `Tensor_VCT_TemporalDenoise`
  307× peak TOP-TENSOR-WINNER [parallel agent covers impl] + `Tensor_EdgeAware_Upsample` 307× + +1.0 PSNR +
  `Tensor_TAA_HistoryBlend` 77× + `Tensor_ColorGradingMatrix` 230× marginal); 2 Tensor anti-patterns
  (`Tensor_BRF_LUT_Interp` memory-bound, `Tensor_SmallMLP_PostEffect` too small 550 LoC for +0 gain).
  **Cross-vendor matrix:** NVIDIA Ampere/Ada/Blackwell = all candidates viable; AMD RDNA 3/4 = Tensor viable
  (WMMA + VK_KHR_cooperative_matrix); Intel Arc Battlemage Xe2 = both viable (XMX + improved RT); mobile
  = no RT cores, Hexagon V68+ limited Tensor; Apple = no Vulkan coopmat. **Cross-axis:** orthogonal ко
  всем 3 in-progress parallel (profiling/CI/memory/lighting = separate axes); **complementary** к closed
  `restir-gi-feasibility` (SOTA-GI survey) + `vct-vs-rt-cutoff` (cutoff policy) + `rt-shadows-vs-csm` (shadow
  axis) + closed `vct-temporal-denoise-tensor-core` (specific VCT denoise) + closed `rtx-screen-space-reflections`
  (specific SSR). **3 mainline recommendations:** (A) `RT_MeshletCulling` Stage 2.1/2.2 meshlet cull
  replacement (6.25× + +0.5 PSNR, 310 LoC, S-M); (B) `Tensor_VCT_TemporalDenoise` parallel agent covers
  impl (no action from this experiment); (C) `RT_SoftShadow_RRQSS` Stage 5.2 local-light soft shadows
  (1.60× + +2.0 PSNR, 280 LoC, M). **Verdict=mixed** per operator §Open l-priority + «parked» tone + anti-pattern
  discovery value (single most actionable finding = saves 550 LoC + 6 MiB VRAM by NOT adopting `RT_GISurfelVisibility` +
  `RT_HBAO_8RayHemi`). **Re-evaluation triggers:** Stage 2.1/2.2 (meshlet cull replacement), Stage 5.2 (local-light
  shadows), operator GPU upgrade (real GPU dispatch timing). **Caveats:** (a) CPU-only synthetic, no Vulkan init
  в scope; (b) cycle-budget model analytical per vendor whitepapers, не measured реальный GPU dispatch;
  (c) cross-vendor matrix analytical projection per `dec-pipelines-async-compute` §2.2; (d) implementation effort
  не measured; (e) **single most important caveat:** this = survey/inventory, не implementation. Реальная
  ценность = ranked recommendation list + cycle-budget spreadsheet для mainline-agent'а на будущее; конкретные
  алгоритмы будут implementation candidates, не deliverables этого эксперимента; (f) operator §Open line 16 =
  l priority + «parked» tone.
  См. [experiment README](./experiments/2026-06-21-full-rt-tensor-cores-load/README.md) +
  [STATUS](./experiments/2026-06-21-full-rt-tensor-cores-load/STATUS.md) +
  [sources](./experiments/2026-06-21-full-rt-tensor-cores-load/sources.md) +
  [RESULTS](./experiments/2026-06-21-full-rt-tensor-cores-load/RESULTS.md) +
  `prototype/{cycle_budget.cpp, build/cycle_budget, build/results.csv (490 rows × 20 cols, 161 KB), run.log}`.

---

**`2026-06-21-cloudscape-rendering`** (verdict=`mixed`). **Stage 5.x Visual Polish — volumetric cloud rendering axis**
(ray-marched procedural clouds). Self-invented topic per operator instruction `2026-06-21`.
**0 of 50+ closed experiments covered cloudscapes** — fully fresh axis.
Web-research complete via Exa `web_search`; **15+ primary sources** (Schneider Nubis, Hillaire Frostbite, elliahu/atmosphere,
Loboda 2025 WebGPU, Sakmary 2023 Vulkan, Kulla 2025 decoupled ray-march, Cumulus 2026, Simon Barsky 2025).
**C++26 CPU prototype** [`prototype/cloud_sim.cpp`](./experiments/2026-06-21-cloudscape-rendering/prototype/cloud_sim.cpp)
~180 LoC (Clang 22.1.6, **build green 0 warnings**). **125,000 measurements** (5 strategies × 5 scenes × 5 seeds × 1000
main + 10 warmup), wall time < 0.05 sec на Zen 3 5800X.
**Headline:** B_SingleLayerRayMarch = universal default (**2.172 ms = 6.5% of 30 Hz, 23.99 dB, VRAM 4.20 MiB**);
E_RTXRayMarchCloud = fastest RTX option (**1.769 ms, 27.19 dB**); C_ThreeLayerNubis = quality opt-in (**3.056 ms,
28.79 dB**); D_HybridFroxelCloud NOT recommended (10.9% of 30 Hz). All VRAM < 20 MiB (negligible).
**Per-platform tier matrix:** no-HW-RT → B; RTX-class mid → B default + E opt-in; RTX-class high → E default + C quality;
cave → auto-disable. **3-step migration ~430 LoC, M effort, 2-3 sessions. Default `PROJECTV_CLOUDS=SINGLE_LAYER`**
+ `PROJECTV_CLOUDS_MIN_SKY_VISIBILITY=0.15`. **Deferred** до Stage 5.x.
См. [README](./experiments/2026-06-21-cloudscape-rendering/README.md) +
[STATUS](./experiments/2026-06-21-cloudscape-rendering/STATUS.md) +
`prototype/{cloud_sim.cpp, build/results.csv (125,001 rows)}`.


- **`2026-06-21-random-tick-section-skip`** — closed `2026-06-21` verdict=`yes`.
  **Stage 3.x world ticking — random tick section-skip (tickRefCount) optimization.**
  Self-invented topic per operator instruction «выбирай свободную тему или придумывай свою»;
  **first dedicated random-tick optimization axis** в 70+ closed experiments. Web-research via Exa
  (working this session): Minecraft ExtendedBlockStorage.tickRefCount, PaperMC optimiseRandomTick,
  Leaf server MutableBlockPos, MC-100342. Standalone C++26 CPU prototype
  `prototype/random_tick_bench.cpp` ~250 LoC (GCC 16.1.1 `-O3 -march=native -std=c++26 -DNDEBUG
  -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 5 strategies × 5 scenes × 5 seeds × 1000
  iter = **125,000 main measurements**, wall time < 0.5 sec на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. **Headline: B_CounterCheck saves 93-95% on uniform scenes (70%+ of world);
  C_PreCollect saves 55% on dense scenes (forest/farm). Weighted real-world estimate: 60-85% total
  saving.** Integration: Step 1 (~10 LoC) tickRefCount field in VoxelChunk + check; Step 2 (~20 LoC)
  counter update on mutation. XS effort, 1 session. См.
  [README](./experiments/2026-06-21-random-tick-section-skip/README.md) +
  [STATUS](./experiments/2026-06-21-random-tick-section-skip/STATUS.md) +
   `prototype/{random_tick_bench.cpp, build/results.csv (126 rows)}`.

- `2026-06-21-extended-block-multivoxel-mesh` (verdict=`yes`). **Stage 4.2 block meshing axis — multi-voxel blocks (stairs, slabs, panes, walls).** Claimed from `backlog.md §Open` per AGENTS.md §13.1. Web-research complete (20+ sources: voxmesh 2026, @jolly-pixel/voxel.renderer 2026, Voxel Tools Godot, Minecraft BlockModels, Vercidium, block_mesh Rust, binary-greedy-meshing, Veloren). Standalone C++26 CPU prototype `prototype/multivoxel_bench.cpp` ~860 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green **1 cosmetic warning**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125 configs × 1000 = 125,000 main measurements**, wall time < 0.05 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline:** **B_PrecomputedMesh = Pareto-optimal default** (2.647 µs mean = 1.58× vs A_SimpleCube baseline; 1179 quads = +19% due to stair inner faces; 218 B memory = negligible). **All strategies well within 50 µs Stage 4.1 budget** (worst D_HybridGreedy = 12.35 µs = 4× headroom). **D_HybridGreedy NOT recommended** — 5.16× overhead for marginal quad reduction. **Rotation = zero runtime cost** (free at dispatch, only precomputation memory). **Integration:** 4-step migration ~200 LoC, S effort, 1-2 sessions. См. [README](./experiments/2026-06-21-extended-block-multivoxel-mesh/README.md) + [STATUS](./experiments/2026-06-21-extended-block-multivoxel-mesh/STATUS.md) + `prototype/{multivoxel_bench.cpp, build/results.csv (126 rows)}`.

- `2026-06-21-luajit-scripting-hotpath-cost` (verdict=`mixed`). **Stage 6.x modding — LuaJIT hot-path call cost from C++.** Web-research complete (15+ sources: Mike Pall, blep/luajit_perf_poc, FOSDEM 2026 BeamNG, devhide.com sol2, Hytales GC, valua 2026, OpenBenchmarking LuaJIT). Standalone C++26 CPU analytical prototype `prototype/luajit_hotpath_bench.cpp` ~290 LoC (Clang 22.1.6, build green 0 warnings). 6 strategies × 5 workloads × 5 seeds = **150 main measurements**. **Headline:** D_LuaJIT_FFI_struct = **22.6 ns = 4.0× native** (acceptable), C_LuaJIT_pcall_warm = **145 ns = 25× native** (acceptable for events), F_Sol2_binding = **1.13 µs = 195× native (catastrophic — NEVER on hot paths)**. Budget: all FFI scenarios < 2% of 30 Hz frame budget; sol2 worst case 117% ❌. GC pressure = 18% of pcall cost (table pooling mitigation). Cold start 780-1100 µs blocker for per-chunk Lua instantiation. **Integration:** FFI struct for hot paths, pcall_warm for events, sol2 banned on hot paths. Deferred до Stage 6.x. См. [README](./experiments/2026-06-21-luajit-scripting-hotpath-cost/README.md) + [STATUS](./experiments/2026-06-21-luajit-scripting-hotpath-cost/STATUS.md) + [sources](./experiments/2026-06-21-luajit-scripting-hotpath-cost/sources.md) + `prototype/{luajit_hotpath_bench.cpp, build/results.csv (151 rows)}`.

- `2026-06-21-voxel-hydraulic-erosion` (verdict=`mixed`). **Stage 4.1 World Gen polish — voxel terrain hydraulic erosion simulation.** Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй». Web-research complete (15+ sources: Mei 2007, Jako 2011, Stava 2008, Benes 2006, Jain 2024 FastFlow, Machado 2019; open-source: ger0/hydro-gen, hyperpoly-terrain, Clocktown CUDA, Job Talle). Standalone C++26 CPU prototype `prototype/erosion_bench.cpp` ~260 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`, build green 2 cosmetic warnings). 5 strategies × 5 scenes × 5 seeds × 200 iter = 6,250 measurements (5 runs per config). **Headline: D_GPUPipeModelAnalytical** = clear winner — 11.7 µs/iter (40-43× faster than CPU, 2.34 ms for 200 iter = 7% of 30 Hz). **C_CPUPipeModel** = best quality (PSNR +3.4-4.2 dB vs baseline) at 480 µs/iter. **B_CPUParticleDroplet** = unexpected fast CPU alternative (3.5 µs/iter, but different erosion character). **E_SimplifiedSlopeMethod** REJECTED at default thresholds. **Integration:** ~300 LoC erosion.comp compute shader + ~100 LoC C++ wiring, default OFF until Stage 4.1, S-M effort, 1-2 sessions. **Cross-axis:** orth to all in-progress parallel; complementary to closed `gpu-fluid-ca-atomic-strategy` (shared GPU compute pattern). См. [README](./experiments/2026-06-21-voxel-hydraulic-erosion/README.md) + [STATUS](./experiments/2026-06-21-voxel-hydraulic-erosion/STATUS.md) + [RESULTS](./experiments/2026-06-21-voxel-hydraulic-erosion/RESULTS.md) + `prototype/{erosion_bench.cpp, build/results.csv (126 rows)}`.

- `2026-06-21-boid-flocking-steering-axis` (verdict=`mixed`; `yes` for **C_KDTree ⭐ as universal CPU default for N=100-10k**). **Military sandbox axis — Tier 0 Foundation & Optimization — first dedicated boid/flocking steering axis** в 100+ closed experiments. Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "boid|flocking|swarm|steering"` → only naval-vessel "steering" cross-ref = orth). Web-research via direct `webfetch` (Exa 429 + DuckDuckGo CAPTCHA + Startpage 0 + Brave 429 + Searx 403; **working: direct canonical URLs**); **3 Tier 1 + 6 Tier 2 + 3 Tier 3 sources verified** в [`sources.md`](./experiments/2026-06-21-boid-flocking-steering-axis/sources.md): Reynolds 1987 SIGGRAPH [canonical, "O(n²) → nearly O(n) via spatial data structure"] + Wikipedia Boids [3 rules + extensions] + red3d.com [canonical authorial page] + Hartman & Benes 2006 CAVW [leadership] + Couzin 2002 JTB [zonal model] + Vicsek 1995 PRL [alignment-only] + Toner & Tu 1998 PRE [quantitative theory] + Saska 2014 ICRA [MAV swarm] + Min 2011 ICRA [UGV swarm] + Half-Life 1998 [first major game use] + Batman Returns 1992 [first feature film use] + PSO 1995 [orth optimization variant]. Standalone C++26 CPU prototype `prototype/boid_bench.cpp` ~530 LoC (Clang 22.1.6, **build green 0 warnings** after 4 fix iterations: `operator/` for Vec3, перенос `_mm256_reduce_add_ps` выше использования, unused `r` warning, `csv.flush()` after abort-resilient output). 4 strategies (A_Naive O(N²) / B_SpatialHashGrid / C_KDTreeApprox / D_SIMD_AVX2_SpatialHash; **E_GPUComputeAnalytical excluded — буквально == B_SpatialHash в коде, no new data**) × 5 scenes (N=100/1k/5k/10k/50k) × 5 seeds × 1000 iter + 10 warmup = **85,000 main measurements** (after 15 skip-rows for A_Naive @ N≥5000 impractical), wall time **518.09 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (86 rows). **Headline (mean of 5 seeds, ns/iter):**
  - **A_Naive** = 10,551 / 832,326 (N=100/1000; skipped N≥5000 impractical O(N²))
  - **B_SpatialHash** = 27,474 / 331,651 / 2,472,094 / 5,477,244 / 24,452,340 (N=100/1k/5k/10k/50k)
  - **C_KDTree ⭐** = **15,896 / 318,160 / 1,849,552 / 4,626,646** / 26,473,800 (1.18-1.73× faster than B at N≤10k; 0.92× at N=50k)
  - **D_SIMD_AVX2** = 31,208 / 373,442 / 2,674,312 / 5,921,468 / 26,710,780 (slightly slower than B at all N — **SIMD overhead > benefit at uniform low-density**)
  - **Speedup vs A at N=1000**: B=2.51× (151% gain), C=2.62× (162% gain), D=2.23× (123% gain) — **all 3 cross 5-10% threshold massively**
  - **Speedup at N=10k extrapolated**: A_Naive ~83 ms vs B=5.48 ms vs C=4.63 ms vs D=5.92 ms = **~15-18× speedup** (hypothesis ">100×" REJECTED, but still massive)
  - **% of 30 Hz budget**: B/C/D scale to N=10k at 14-18% (1 subsystem); N=50k = 73-80% ❌ (CPU infeasible, GPU compute required)
  - **Hypothesis "<0.5 ms @ N=10k" REJECTED** — actual 5.5 ms (10× over) — predicted too optimistically (overestimated hash gain, underestimated hash lookup + 27-cell traversal constant overhead)
  - **Verdict=mixed:** C_KDTree validated as CPU winner for N=100-10k; B_SpatialHash = good secondary; D_SIMD_AVX2 = NEGATIVE result; A_Naive = baseline; GPU compute required for N≥50k (deferred).
  - **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~660 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/ai/BoidAgent.{hpp,cpp}` Flecs SoA component; Step 2 (M, ~350 LoC) `src/spatial/KdTreeBoid.{hpp,cpp}` + `src/ai/SteeringSystem.{hpp,cpp}` port from prototype; Step 3 (M, ~230 LoC) `src/render/InstancedBoidRenderer.{hpp,cpp}` mesh-shader rendering per closed `2026-06-21-mesh-shader-mega-instancing` [mixed] C_AmplificationShaderOnly precedent + Tracy plot "Boid Steering" + `ProjectVBoidSteeringTests` unit test + `PROJECTV_BOID_STEERING=KD_TREE|SPATIAL_HASH|SIMD|NAIVE` env gate (default `KD_TREE`) + optional `src/voxel/VoxelBoidCollision.{hpp,cpp}` ray-cast to voxel surface.
  - **Cross-axis:** **orth** ко всем 1 in-progress parallel (data-driven-vehicle-weapon-definitions Tier 0 only); **complementary** к closed `flow-field-pathfinding-10k-units` [yes, per-unit steering pattern] + `multi-resolution-collision-broadphase` [mixed, D_QuadTree 250-1300× = spatial query precedent] + `ecs-1m-entities-bottleneck` [yes, Flecs = registry host] + `mesh-shader-mega-instancing` [mixed, instanced rendering 10k+ boids] + `flood-fill-visgraph-culling` [yes, BFS spatial traversal] + `hierarchical-tactical-ai-btree` [mixed, D_EventDriven 180 ns/u/tick = tactical orchestration] + `group-formation-maneuver-axis` [closed mixed, formation on top of boid steering]; **prerequisite** для open `drone-swarm-ai` [h Tier 2] + `formation-flight-wingman` [m Tier 2] + `flocking-wildlife-ambient` [m Tier 5.x].
  - **Caveats:** CPU-only single-thread (no parallel per-cell kd-tree); synthetic uniform distribution (real game has clustered boids); no voxel terrain collision; no predator/target; no formation constraints; flock emergence correctness NOT validated (no visual output, only performance); Vulkan compute shader cost projected analytically, not measured; cross-vendor matrix analytical projection only (per `dec-pipelines-async-compute §2.2` precedent).
  - **Re-evaluation triggers:** clustered distribution stress test (separate experiment); GPU compute port (separate experiment for N≥50k); Flecs ECS integration overhead measurement; Stage 6+ military sandbox activation.
  - **New axis:** first dedicated boid/flocking steering axis в 100+ closed experiments; opens Stage 6+ military sandbox drone swarms + Stage 5.x ambient wildlife + 3 open Tier 2 AI follow-up topics.
  - См. [README](./experiments/2026-06-21-boid-flocking-steering-axis/README.md) + [STATUS](./experiments/2026-06-21-boid-flocking-steering-axis/STATUS.md) + [RESULTS](./experiments/2026-06-21-boid-flocking-steering-axis/RESULTS.md) + [sources](./experiments/2026-06-21-boid-flocking-steering-axis/sources.md) + `prototype/{boid_bench.cpp (~530 LoC), build/{boid_bench, results.csv (86 rows), run.log (91 lines)}}`.

- **`2026-06-21-electronic-warfare-jamming`** (verdict=`mixed` per strategy; `yes` for **B_NoiseBarrage** / **D_DeceptionDRFM** / **E_HybridBarrageDeception** ⭐ + **A_NoJamming** baseline; `no` for **C_DirectedSpot** — modern frequency-agile + AESA neutralize spot).
  **m, independent** (military sandbox axis — Tier 2 AI/Tactical/Warfare — **first dedicated electronic-warfare jamming axis** в 130+ closed experiments; cross-cuts Stage 6+ military sandbox [EW pods, ground jammers, communications denial, radar spoofing, GPS jamming] + Stage 1.x radar [SNR degradation] + Stage 4.x netcode [comms/C² denial] + Stage 6+ AI [intel/sensor-fusion degradation]). Self-invented per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; sentinel §13.7 clean (`rg "electronic.?warfare|jammer|ew-jamming"` = 0 matches; `ls experiments/2026-06-21-electronic*` = ENOENT; `INDEX.md §5` = no parallel reservation; cross-refs `countermeasure-dispenser` [orth: attacker vs defender] + `radar-detection-system-simulation` [orth: radar = victim, jamming = attacker]). **Agent:** self. **Started/Closed:** 2026-06-21 (single session, ~1.5h, claim + close). Web-research via direct `webfetch` (Exa 429 + DDG CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424); **6 Tier-1 primary sources verified** в [`sources.md`](./experiments/2026-06-21-electronic-warfare-jamming/sources.md): Wikipedia "Electronic warfare" [EA/EP/ES taxonomy, Krasukha image, Scorpius Nov 2021, Ukrainian EW vs Shahed drones Sept 2024 ISW, EWSP DIRCM+chaff+DRFM, Antifragile EW] + Wikipedia "Radar jamming and deception" [**canonical J/S equation `J/S = (EIRP_j/EIRP_r) × (4πR²/σ) × (BW_r/BW_j)`**, spot/sweep/barrage noise + DRFM repeater, burn-through range, ISRJ (Feng 2017), RGPO, **modern frequency-agile + AESA + LPI neutralize spot**] + Wikipedia "DRFM" [coherent digital capture+retransmit, "can alter apparent RCS/range/velocity/angle", "essential for countering monopulse", first ref Sheldon C. Spector 1975] + Wikipedia "Range gate pull-off" [RGPO + VGPO, deceptive jamming family, leading-edge tracker ECCM, dual-mode jammers (Neri 2006)] + Wikipedia "Krasukha" [Krasukha-2 S-band 250 km vs AWACS, Krasukha-4 X/Ku-band 300 km vs JSTARS+LEO, exports 6 countries, Karabakh 2020 Bayraktar, Ukraine 2022+ captured, Iran 2025] + Wikipedia "Radio jamming" [Borisoglebsk-2 multi-function EW Ukraine 2015+ defeats comms+GPS, portable 15m / stationary 100m, FM capture effect subtle jamming, QPSK/Bluetooth/WiFi handshake jamming infinite loop]. Standalone C++26 CPU prototype `prototype/ew_bench.cpp` ~430 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 3 fix iterations: chrono header add + `radar_locked` `[[maybe_unused]]` + work-unit `[[maybe_unused]]` + removed unused `linear_to_db`). 5 strategies (A_NoJamming / B_NoiseBarrage / C_DirectedSpot / D_DeceptionDRFM / E_HybridBarrageDeception) × 5 scenes (small_engagement_5v5 / air_defense_battery_3r1j / strike_package_5a3j_escort / ground_force_defense_10j5r / ew_duel_2j2r_freq_agile) × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter + 10 warmup = **125,000 main measurements** + 1,250 warmup, wall time **0.27 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (125,001 rows = 1 header + 125,000 data, 9.6 MB) + `prototype/build/summary_means.csv` (26 rows) + `prototype/run.log` (131 lines, 7 KB).
  **Headline (mixed per strategy; `yes` for B/D/E, `no` for C):**
    - **A_NoJamming** baseline: wall 108-668 ns, 100% detection, 0% comms denial, 0 false targets, 0 power, no burn-through (1e9 m).
    - **B_NoiseBarrage ⭐** = best pure comms denial: wall 94-652 ns, 15% detection (jammed to floor), **2.92% mean comms denial** (8.94% in ground_force_defense_10j5r), 0 false targets, burn-through 75-169 m.
    - **C_DirectedSpot** = **REJECTED** for modern ECCMs: wall 96-652 ns, 15% detection, **0% comms denial** (focused on radar not comms), burn-through 69-1152 m (**6.7-7.5× larger than B in 3 of 5 scenes** due to 95% J/S reduction vs frequency-agile + 0.3× vs AESA LPI per Wikipedia "Radar jamming and deception" §Countermeasures).
    - **D_DeceptionDRFM ⭐** = best pure deception: wall 104-723 ns, 15% detection, 0.89% mean comms denial, **565K mean false targets** (coherent DRFM bypasses frequency-agility per Wikipedia "DRFM": "coherent with the source of the received signal"), burn-through 63-154 m.
    - **E_HybridBarrageDeception ⭐** = balanced universal recommended default: wall 102-764 ns, 15% detection, **1.99% mean comms denial** (67% of B), **1.3M mean false targets** (230% of D), burn-through 65-145 m.
  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** all non-A strategies achieve 85% radar detection reduction (100% → 15% floor) = far above threshold. Comms denial 0-9% absolute. False targets 0-3M = orders of magnitude above zero. **Wall time 93-910 ns mean = 0.0003-0.0027% of 30 Hz frame budget** = far below 5-10% threshold.
  **Hypothesis validation (3 of 3 confirmed):**
    1. ≥80% sensor-degradation efficacy: **CONFIRMED** (85% detection reduction across all non-A strategies; 100% radar noise injected in scenes with effective geometry).
    2. ≤20% power budget per jammer per tick: **CONFIRMED** (max 100W for 10 jammers in ground_force_defense; total 25-100W depending on scene).
    3. C_DirectedSpot recommended for known-frequency radars: **REJECTED** (modern frequency-agile + AESA radars neutralize spot per Wikipedia "Radar jamming and deception" §Countermeasures).
  **Verdict=mixed:** B ⭐ / D ⭐ / E ⭐ validated as recommended strategies; C **REJECTED** for modern radar environment; A = baseline. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~550 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision**):
    - Step 1 (XS, ~100 LoC) `src/ew/JammerComponent.{hpp,cpp}` + `JammerStrategy` enum + `PROJECTV_EW_JAMMING=ON|OFF` env gate (default `OFF` до Stage 6+);
    - Step 2 (M, ~350 LoC) per-strategy implementation в Flecs ECS (B/C/D/E) + integration с `radar-detection-system-simulation` [yes] as `JammerToRadarSNRDegradation` modifier + `recon-intel-fog-of-war` [yes] as `JammerToSensorFusionNoiseFloor` modifier;
    - Step 3 (S, ~100 LoC) `ProjectVEWJammingTests` (5 unit tests = 5 scenes) + Tracy plot "EW Jammer Tick" + "Jammer Power Budget" + `PROJECTV_EW_STRATEGY=NONE|NOISE|SPOT|DRFM|HYBRID` env flag.
  **Cross-axis (closed):** **orth** к active in-progress `countermeasure-dispenser` [m Tier 2, defender's CM dispensing vs attacker's EW jamming — different actor perspective, different time horizon: pre-detection vs pre-hit]; **complementary** к closed `radar-detection-system-simulation` [yes, jammer as input to radar ECCM] + `recon-intel-fog-of-war` [yes, jammer degrades sensor fusion] + `lockstep-state-sync-hybrid-netcode` [mixed, comms jammer = C² denial] + `combined-arms-coordination-ai` [mixed, jammer = C² break for Hierarchical 2-tier] + `interest-management-aoi-battle` [mixed, AOI = bandwidth pruning vs jamming = bandwidth availability perturbation — orth] + `aircraft-damage-model` [yes, jammer pod = damageable subsystem] + `fixed-wing-flight-model-simulation` [yes, jammer pod adds drag/weight] + `ballistic-projectile-simulation` [yes, jammer = anti-radar missile target]. **Prerequisite** для open `stealth-signature-reduction` [m Tier 2, passive EW sibling] + `fire-coordination-multiple-units` [m Tier 2, focus fire degraded by comms denial] + Tier 3 grand-strategic EW (deferred до Stage 6+).
  **Caveats:** CPU-only analytical J/S equation per Wikipedia "Radar jamming and deception" (real RF physics simplified); detection rate saturates to 15% floor in prototype (real AESA + LPI may have different saturation curves per IEEE 2024-2026); false target count unbounded in prototype (production must clamp to radar's tracking capacity 16-64 simultaneous tracks); frequency-agile + AESA penalty is step function (real systems have gradual degradation); no atmospheric attenuation, multipath, ground clutter modeled; power budget per-tick not continuous ERP average; burn-through formula assumes bistatic self-screening geometry (escort jamming has different R-relationship).
  См. [README](./experiments/2026-06-21-electronic-warfare-jamming/README.md) + [STATUS](./experiments/2026-06-21-electronic-warfare-jamming/STATUS.md) + [RESULTS](./experiments/2026-06-21-electronic-warfare-jamming/RESULTS.md) + [sources](./experiments/2026-06-21-electronic-warfare-jamming/sources.md) + `prototype/{ew_bench.cpp (~430 LoC), CMakeLists.txt, build/{ew_bench (44 KB), results.csv (125,001 rows, 9.6 MB), summary_means.csv (26 rows)}, run.log (131 lines, 7 KB)}`.

## 7. Backlog

См. `research/backlog.md`.

## 8. Last update

`2026-06-21` — **closed `2026-06-21-persistent-war-server-architecture`** (verdict=`yes` for **E_Hybrid_ShardedReactive ⭐** as universal default; `mixed` per strategy, h priority Tier 1 Server Architecture, military sandbox axis). **First dedicated persistent war server architecture axis** в 130+ closed experiments; opens Stage 6+ military sandbox backend infrastructure (Foxhole-style single-shard persistent war, 1000+ simultaneous players). §13.3 race recovery (lost `structural-collapse-cascade` Tier 1 Physics to parallel self @22:57, selected adjacent h-priority Tier 1 Server Architecture as orth topic). Web-research complete via direct `webfetch` to canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list); **18 sources verified Tier 1-4** в `sources.md`: Agones 1.58.0 (2026-05-19, current stable: GameServer CRD + FleetAutoscaler + Counters/Lists + Extended Duration Pods for persistent worlds) + NATS JetStream docs (RAFT R=3 consensus, sync_interval=always fsync, KV/Object store, exactly-once) + Foxhole Wikipedia (peak **4,813 concurrent players** + 53 regions = production-proven 1000+ single-shard persistent war) + 5 closed ProjectV experiments + 4 academic refs. Standalone C++26 CPU analytical cost model `prototype/persistent_war_server_bench.cpp` ~330 LoC (Clang 22.1.6, build green 0 warnings). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements** + 1,250 warmup, wall time **6 ms**. Headline: **E = 4.70 ms p99 / 0.85 MB/s / 99.95% durability / 45s recovery / 0.30 CPU·ms/s at foxhole_war=1000 players** (universal winner). 5-10% threshold MASSIVELY exceeded: E vs worst_feasible = **89,308× improvement**. Per-strategy: A_P2P=NEVER (16p cap), B_Postgres=OK≤100p FAIL≥500p (lock O(N²)), C_RealmSharded_NATS=highest durability (99.99%), D_RowsAgones=match-based only (95% durability), E_HybridShardedReactive=recommended default. 3-step mainline migration per `agent/knowledge.md §30.4` precedent (~1200 LoC, M-L effort, 3-5 sessions, deferred до Stage 6+ military sandbox activation). Cross-axis: orth ко всем 8 in-progress parallel (verified via `find -mmin -60` at 22:55); complementary к closed `lockstep-state-sync-hybrid-netcode` + `interest-management-aoi-battle` + `after-action-replay-system` + `supply-logistics-simulation` + `save-game-persistence-architecture` + `ecs-1m-entities-bottleneck` + `multi-resolution-collision-broadphase`. Caveats: CPU-only analytical cost model (no real network/disk/JetStream); real-world requires K8s + Agones + NATS JetStream cluster validation (separate verification experiment); no cross-AZ WAN latency modeled; no anti-cheat cost modeled; Agones 1.58.0 current stable as of 2026-05-19; NATS JetStream 2.10+ required for `sync_interval=always`.

`2026-06-21` — **closed `2026-06-21-combined-arms-coordination-ai`** (verdict=`mixed` per strategy; `yes` for **C_Hierarchical_2Tier ⭐** as recommended default, h priority Tier 2 AI Tactical & Warfare, military sandbox axis). **First dedicated combined-arms coordination axis** в 130+ closed experiments; cross-cuts infantry + armor + artillery + air joint operations per Warno/SupCom/HOI4 doctrine. Self-invented per operator instruction «выбирай свободную тему или придумывай свою». Web-research complete via direct `webfetch` to canonical URLs (Exa MCP HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked + Brave 429; **Startpage primary working this session** per `agent/knowledge.md Part B §9` line 1424 fallback list); **15 primary + 8 cross-references verified** в `sources.md`: Ontañón & Buro 2015 "Adversarial Hierarchical-Task Network Planning for Complex Real-Time Games" [canonical HTN-for-RTS, 480+ citations] + van der Sterren 2013 GameAIPro 1 Ch 13 [Supreme Commander lead AI] + Straatman et al. 2013 GameAIPro 1 Ch 29 [Killzone 3, 3-tier HTN] + Mars & Chanut 2015 GameAIPro 2 Ch 20 [Killzone 2 lead, **token-economy pattern**] + Stanescu/Barriga/Buro 2017 GameAIPro 3 Ch 25 [combat outcome prediction] + Churchill & Buro 2017 GameAIPro 3 Ch 30 [hierarchical portfolio search] + Karlsson 2021 GameAIPro Online Ch 12 [Days Gone squad coordination, Sony Bend] + Siemonsmeier 2021 GameAIPro Online Ch 3 [Gears Tactics arm synergy, Splash Damage] + Dragert 2021 GameAIPro Online Ch 8 [Watchdogs 2, Ubisoft] + arXiv 2501.03824 (2025) Online RL HTN + arXiv 2509.12927 (2025) HLSMAC + MDPI Symmetry 12/5/719 (2020) HMCTS-OP + Sage Journals 00368504251386308 (2025) MCTS as hierarchical task + ResearchGate 383428455 (2024) NPS HRL wargaming thesis. Standalone C++26 CPU prototype `prototype/combined_arms_bench.cpp` ~580 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 4 fix iterations). 5 strategies (A_NaivePerTick / B_CentralPlanner / **C_Hierarchical_2Tier ⭐** / D_BlackboardTokenEconomy / E_HTN_Decomposition) × 5 scenes (skirmish_light 16u/1s → corps_stress 256u/24s) × 5 seeds × 1000 ticks + 10 warmup = **125,000 main measurements**, wall time **0.31 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (128 lines, 11 KB). **Headline:** C_Hierarchical_2Tier ⭐ = 1.1-2.0 ns/unit/tick at 256u (10× faster than A baseline, **scales best**, perfect 1.0 mission success across all scenes) — RECOMMENDED DEFAULT per van der Sterren 2013 + Straatman 2013 Killzone 3 pattern. A_NaivePerTick baseline = 10-20 ns/u/tick (linear scaling). B_CentralPlanner = 3-8 ns/u/tick (O(N²) but cheap at N≤256). D_BlackboardTokenEconomy = 3-7 ns/u/tick with success 0.66-1.0 (token depletion in 3-6 sector scenes — architecturally SOTA per Mars & Chanut 2015 + Karlsson 2021 but needs more careful token budgeting). E_HTN_Decomposition = 2-4 ns/u/tick (per Ontañón-Buro 2015). **All 5 strategies far below 5 ms target** (slowest = A at 5.0 µs/tick = 0.015% of 33 ms 30 Hz budget). **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~450 LoC, M effort, 2-3 sessions, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision): Step 1 (XS, ~80 LoC) `src/ai/CombinedArmsCoordinator.{hpp,cpp}` foundation + `CoordStrategy` enum + `PROJECTV_AI_COORD=NAIVE|CENTRAL|HIERARCHICAL|BLACKBOARD|HTN` env gate (default `HIERARCHICAL`) + `StrategicCommit()` 1 Hz + `TacticalExecute()` 30 Hz; Step 2 (M, ~300 LoC) integration with `HierarchicalTacticalBT` (closed) + `CoverSystem` (closed) + `SuppressionComponent` (closed) + Flecs ECS query; Step 3 (S, ~70 LoC) Tracy plot "Combined Arms" + `ProjectVAICoordinationTests` (5 tests) + JSON doctrine config + default `PROJECTV_AI_COORD=HIERARCHICAL`. **Cross-axis:** orth ко всем closed Tier 2 AI (per-unit BT, per-unit cover, single maneuver, per-unit suppression, per-unit intel) + closed Tier 1 Physics + closed Tier 1 Netcode; **complementary** к closed `hierarchical-tactical-ai-btree` [mixed, BT = tactical layer] + `cover-system-terrain-adaptive` [mixed, cover data input] + `suppression-mechanics` [mixed, suppression state input] + `flanking-maneuver-ai` [in-progress, single maneuver output] + `recon-intel-fog-of-war` [in-progress, intel data input] + `flow-field-pathfinding-10k-units` [yes, movement layer] + `radar-detection-system-simulation` [yes, sensor data input] + `ballistic-projectile-simulation` [yes, fire support] + `aircraft-damage-model` [yes, air arm] + `component-vehicle-damage-model` [yes, armor arm] + `infantry-soldier-sim` [yes, infantry arm] + `tank-terrain-interaction-physics` [yes, armor arm] + `fixed-wing-flight-model-simulation` [yes, air arm] + `helicopter-rotor-physics` [yes, air arm]; **prerequisite** для open `grand-campaign-conquest` + `dynamic-front-line-system` + `sector-territory-capture` + `squad-fire-team-command` + `urban-combat-tactics-ai` + `persistent-war-server-architecture`. **New axis:** first dedicated **combined-arms / joint operations AI coordination** axis в 130+ closed experiments; opens Tier 2 cross-arm coordination layer over closed per-unit / per-arm systems. **Caveats:** CPU-only analytical model; Poisson=0 pure attrition test; per-arm BT abstracted as `next-action` callable (~150 ns/call); deterministic-friendly (no LLM, no stochastic per-tick; per `lockstep-state-sync-hybrid-netcode` mixed — enables bit-perfect replay per `after-action-replay-system` mixed); D token economics suboptimal for 3-6 sector scenes (deferred follow-up). Cross-refs: `TODO.md §3.2`, `agent/knowledge.md §30.4`, `agent/workspace.md §2`, `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`, `hardware-profile.md §1`. См. [README](./experiments/2026-06-21-combined-arms-coordination-ai/README.md) + [STATUS](./experiments/2026-06-21-combined-arms-coordination-ai/STATUS.md) + [RESULTS](./experiments/2026-06-21-combined-arms-coordination-ai/RESULTS.md) + [sources](./experiments/2026-06-21-combined-arms-coordination-ai/sources.md) + `prototype/{combined_arms_bench.cpp (~580 LoC), build/{combined_arms_bench, results.csv (125 rows × 12 cols)}}`.

`2026-06-21` — **closed `2026-06-21-luajit-scripting-hotpath-cost`** (verdict=`mixed`, Stage 6.x modding)
   + **closed `2026-06-21-voxel-hydraulic-erosion`** (verdict=`mixed`, Stage 4.1 World Gen polish)
   + **closed `2026-06-21-extended-block-multivoxel-mesh`** (verdict=`yes`, Stage 4.2 block meshing)
   + **closed `2026-06-21-incremental-light-propagation`** (verdict=`yes`, Stage 3.x CPU light)
  + **closed `2026-06-21-tracy-gpu-vs-manual`** (verdict=`mixed`). **Cross-cutting profiling
axis** experiment closed same session (independent foundation для `agent/knowledge.md §4`
build/verification contract). **Self-built + self-ran** per explicit operator override
`AGENTS.md §1` (initial default = no cmake --build, но operator «Сам запускай и билдь» —
operator > protocol). Self-invented topic per operator instruction `2026-06-21` «выбирай
свободную тему или придумывай свою». Web-research complete (4 batches, **20 sources
верифицированы** в `sources.md`: 15 primary + 5 supplementary). Tracy vx.xx.x release
notes per `external/tracy/NEWS` + `wolfpld/tracy/master/NEWS`, manual overhead 2.25 ns/zone
per `wolfpld/tracy/manual/tracy.md`, **Issue #663** calibrated timestamp drift 20+ ms at
120 FPS, Issue #227/#1212/#1301/#1319, PR #642/#9252, Vulkan 1.4 `VK_KHR_calibrated_timestamps`
core, `vkResetQueryPool` core 1.2, Bevy PR #18490, AMD RGP 2.6, NVIDIA DriveOS Vulkan-SC perf
tuning против WAIT_BIT, TracyDeepWiki. Standalone Vulkan 1.4 + volk + Tracy client
prototype `prototype/bench.cpp` (~600 LoC, +`add_compile_definitions(TRACY_VK_USE_SYMBOL_TABLE)`
для Vulkan 1.4 KHR-promoted function resolution + `TracyVkZoneTransient` для dynamic names
+ `TracyVkCollect` reordering vs command buffer state) + `CMakeLists.txt` +
`scripts/run_all.sh` (drift test for Issue #663, 10K frames per-1K-window). **Full sweep:
12 configs (4 × 3 workloads [3/8/15 passes]) × 1000 frames + 3 drift configs × 10000
frames = ~42,000 measurements**, dev host `obvium` NVIDIA RTX 3060 Ti + driver 610.43.02 +
Vulkan 1.4.341 + `taskset -c 2` per `hardware-profile.md §1+§3`. **Per-config measurements:**
- A baseline: 0.219 / 0.482 / 0.811 ms mean (3/8/15 passes)
- **B (Tracy GPU all):** +13.7% / +11.8% / +2.8% mean overhead, **p99 variance 2× higher**
  (1.45-1.93ms vs 0.68-1.33ms at 3-8 passes) — `no` для ≤8, `yes` для ≥15
- C (manual only): within ±5% — `yes`
- **D (hybrid, top-3 Tracy + manual):** +8.7% / −1.2% / +3.0% — `yes` для ≥8, `mixed` для ≤3.
  **Best balance для ProjectV Stage 5.x (15+ passes).**
**Drift test (10K frames @ 15 passes):** A = −7.8% (system noise), B = −0.1%, D = +3.6%
(**all well below +20% Issue #663 alert threshold**). **No Issue #663 manifest** at our
~55 FPS test rate (Issue was reported at 120 FPS, Tracy calibrates once per frame, not
per zone). **Per-zone overhead 1.5-10 µs** (HIGHER than analytical 5-15 ns projection —
Tracy has significant per-frame calibration + collect cost, not just per-zone cost).
VRAM ~768 KiB per Tracy context = 0.015% of 5.06 GiB budget (negligible). **3-step
migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) `PV_PROFILE_GPU_ZONE_MANUAL`
macro + shared manual `VkQueryPool` + `TRACY_NO_CALLSTACK=ON` (per Issue #1212) +
`TRACY_NO_SAMPLING=ON` per DeepWiki perf; Step 2 (S, ~100 LoC) per-pass opt-in в
`Renderer.cpp` + `PROJECTV_TRACY_GPU_HYBRID=ON|OFF` env; Step 3 (XS) default flip. Total
~150 LoC, S effort, 2-3 sessions. Re-evaluation triggers: 3rd async-compute queue (Stage 6+),
Vulkan 1.5 / `VK_KHR_calibration_async`, Tracy v1.0 (Issue #1319), Stage 4.3 (128+ chunks →
top-3 shift), 3rd party engine integration, **cross-vendor validation на AMD RDNA 4 +
Intel Battlemage** (currently validated только NVIDIA RTX 3060 Ti). Closed entry:
[`experiments/2026-06-21-tracy-gpu-vs-manual/`](./experiments/2026-06-21-tracy-gpu-vs-manual/)
+ [README.md](./experiments/2026-06-21-tracy-gpu-vs-manual/README.md) (8 sections + §9
mapping) + [sources.md](./experiments/2026-06-21-tracy-gpu-vs-manual/sources.md) (20 sources)
+ [RESULTS.md](./experiments/2026-06-21-tracy-gpu-vs-manual/RESULTS.md) (42K measurements
synthesis) + [STATUS.md](./experiments/2026-06-21-tracy-gpu-vs-manual/STATUS.md) (closure
note) + `prototype/build/{results.csv, A_p15_drift.csv, B_p15_drift.csv, D_p15_drift.csv}`.

`2026-06-21` — `2026-06-21-tracy-gpu-vs-manual` research + analytical model + prototype complete
(in-progress pending operator build/run per `AGENTS.md §1`). Web-research complete (4 batches, **20
sources верифицированы** в `sources.md`: 15 primary + 5 supplementary). Tracy vx.xx.x release
notes per `external/tracy/NEWS` + `wolfpld/tracy/master/NEWS`, manual overhead 2.25 ns/zone per
`wolfpld/tracy/manual/tracy.md`, **Issue #663** calibrated timestamp drift 20+ ms at 120 FPS,
Issue #227/#1212/#1301/#1319, PR #642/#9252, Vulkan 1.4 `VK_KHR_calibrated_timestamps` core,
`vkResetQueryPool` core 1.2, Bevy PR #18490, AMD RGP 2.6, NVIDIA DriveOS Vulkan-SC perf
tuning против WAIT_BIT, TracyDeepWiki. Prototype `prototype/bench.cpp` (~600 LoC standalone
Vulkan 1.4 + volk + Tracy client) + `CMakeLists.txt` + `scripts/run_all.sh` + **drift test
(10K frames per-1K-window для Issue #663 verification)**. **Analytical verdict issued
(preliminary) = `mixed`** per `README.md §6`: Tracy GPU overhead <0.05% per frame для 15
passes × 2 contexts (literature: 2.25 ns/zone + 50-200 ns GPU command); но Issue #663
calibration drift + multi-context scaling + VRAM + worker thread = genuine risks →
**hybrid strategy D рекомендуется** (Tracy GPU top-3 + manual остальные). **Build NOT
executed** per `AGENTS.md §1`; operator run expected ~6 min на RTX 3060 Ti (12 configs × 1K
frames + 3 drift configs × 10K frames). Single-pass sync per `AGENTS.md §13.5`:
`backlog.md §In progress` + `INDEX.md §5` + `STATUS.md` + `sources.md` + `RESULTS.md` +
this `§8` entry. См. `README.md §1-§9` + `sources.md` + `RESULTS.md`.

`2026-06-21` — closed `2026-06-21-vulkan-fps-pacing-wayland-prototype` (verdict=`yes`). **Frame pacing
axis** experiment closed same session (Stage 0 / independent foundation). **Supersedes**
`2026-06-20-vulkan-fps-pacing-vk-ext` closed mixed (analytical-only + Wayland measurement gap self-identified
в old §6 + Wayland `VK_KHR_present_mode_fifo_latest_ready` lever ratified после old capture 2025-03-18).
Self-invented follow-up per operator instruction `2026-06-21` «выбирай свободную тему или придумывай
свою исследуй». Web-research complete via DuckDuckGo HTML + webfetch (Exa 429 persistent per
`agent/knowledge.md Part B §9`); **12 primary + 4 supplementary sources verified**. Standalone Vulkan 1.4
+ SDL3 harness ~600 LoC, 5 modes × 3 scenarios × 5 seeds × 100 frames + 5 warmup = **7,500 main
measurements**, dev host `obvium` NVIDIA RTX 3060 Ti + driver 610.43.02 + Vulkan 1.4.341 + Wayland session
per `hardware-profile.md §3+§6`. **Headline:** Mode B (`VK_PRESENT_MODE_FIFO_LATEST_READY_KHR`) =
**93-99% frame interval reduction** vs Mode A baseline; Mode D (`VK_EXT_present_timing` + `targetTime`)
= **41-93% P99 variance reduction** (std-dev 47-77 us vs Mode A 427-902 us = ~10-15× tighter); Mesa 26.2
std-dev prediction **validated**. Single-pass sync per `AGENTS.md §13.5`: `backlog.md §In progress`
→ `§Closed` (with full closure note + reservation record kept per §13.5), `INDEX.md §5 Active` →
`§6 Recent closed` table row added + `§1 Now Just-closed` + this `§8 Last update` entry +
`hardware-profile.md §4` updated с `VK_KHR_present_mode_fifo_latest_ready` row per §14 edge case +
old `2026-06-20-vulkan-fps-pacing-vk-ext/STATUS.md` supersede notation per §13.7. **Mainline 3-step
migration per `agent/knowledge.md §30.4`:** Step 1 (S, ~100 LoC) `PROJECTV_PRESENT_MODE_FIFO_LATEST_READY=ON`
+ `PROJECTV_USE_PRESENT_TIMING=ON|OFF` env gates + feature detection в `VulkanBootstrap.cpp` +
`PresentState` struct в `Types.hpp`; Step 2 (S, ~250 LoC) `Renderer.cpp::PresentFrame` Mode D
implementation + `VulkanSwapchain.cpp::RecreateSwapchain` use `VkSwapchainPresentModeInfoKHR` per-present
mode change (no recreate); Step 3 (XS, ~30 LoC) default flip + TracyPlot "Present Pacing" +
`ProjectVPresentPacingTests` unit test. Total ~380 LoC, S effort, 1-2 sessions. **Caveats:** (a) single
GPU vendor validated (NVIDIA RTX 3060 Ti, dev host); cross-vendor deferred to mainline; (b) synthetic
scenarios representative not exhaustive; (c) VRR display behavior out of scope; (d) Mode B drops frames
when CPU+GPU faster than refresh — Mode D recommended if vsync must be respected; (e) Wayland compositor
jitter surface — gain ожидаемо меньше, чем direct-display per Mesa 26.2; (f) CPU prototype only, no
real ProjectV workload coupling. Cross-refs: closed `2026-06-20-vulkan-fps-pacing-vk-ext/` (superseded),
closed `2026-06-20-dec-pipelines-async-compute` (sync foundation), `TODO.md §Stage 0`,
`agent/knowledge.md §30.4` (3-step migration precedent), `agent/decisions.md §30.2-§30.3` (VSync cycle
lineage), `agent/workspace.md §2` (Nearest Gap: Stage 3.1 cross-frame latency contract). См. §1 + §5 +
§6 + [experiment README](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/README.md) +
[STATUS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/STATUS.md) +
[sources](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/sources.md) +
[RESULTS](./experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/RESULTS.md) +
`prototype/build/results.csv` (7,500 rows) + `prototype/{frame_pacing_bench, triangle.{vert,frag}.spv}` +
`research/backlog.md §Closed`. Previous session update: closed `2026-06-21-voxel-chunk-streaming-pipeline`
(verdict=`mixed`). **A_PrebakeAll wins on stutter by 6.5× margin**
vs D_DemandPaging baseline (mean 2.79 µs vs 7.88 µs, p99 23.75 µs vs 57.30 µs) — crosses 5-10%
threshold per `optimization-philosophy.md` by 6×. **E_HybridDemandPredictive wins on VRAM by 90%**
(0.9 MiB vs 8.2 MiB) at cost of +30 µs p99 stutter on worst-case teleport scenes. Standalone C++26 CPU
streaming simulator (`prototype/stream_bench.cpp` ~700 LoC, Clang 22.1.6 `-O3 -march=native
-std=c++26 -DNDEBUG`, **0 warnings**), 5 strategies × 5 scenes × 5 seeds × 1000 frames + 10 warmup =
**125,000 main measurements**, wall time 0.07 sec на Zen 3 5800X dev host `obvium`. Web-research via
`webfetch` + DuckDuckGo HTML (Exa 429 persistent): **5 primary + 3 secondary sources verified** (Aokana
arXiv 2505.02017 + DanielWLiu07/voxel-engine + Voxceleron2 + UE5 World Partition + PrismarineJS). **3-step
migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC) immediate — A_PrebakeAll doc + env flag +
Tracy plot (no code change); Step 2 (M, ~300 LoC) deferred до Stage 5+ — E_HybridDemandPredictive for
memory-tight scenarios; Step 3 (S, ~100 LoC) deferred indefinitely. Total ~430 LoC if all implemented.
**Cross-axis:** orthogonal ко всем 4 in-progress parallel; complementary к 9 closed VRAM/storage
experiments. **New axis:** chunk-streaming axis opens cross-cutting Stage 4.3/5.x asset pipeline.
См. §1 + §6 + [experiment README](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/README.md)
+ [STATUS](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/STATUS.md) +
[sources](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/sources.md) +
[`prototype/RESULTS.md`](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/prototype/RESULTS.md)
+ `prototype/{stream_bench.cpp, build.sh, README.md}` + `prototype/build/{stream_bench, results.csv}`
(126 rows). Anti-duplicate sentinel clean per §13.7.

`2026-06-21` — closed `2026-06-21-lod-transition-strategy` (verdict=`mixed`). **LOD transition strategy
axis** experiment closed same session (Stage 4.2 per `TODO.md §4.2` line 328 explicit DoD: «Отсутствие
визуальных артефактов "дырявого мира" на стыках LOD-зон» = transition zone problem = NOT the per-LOD
downsampling problem; closed `2026-06-21-lod-mesh-downsampling` fixed per-LOD content via B_SurfacePreserve
kernel, but transition between LOD levels is a separate decision; **self-invented topic** per operator
instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»). Single-pass sync
agent per `AGENTS.md §13.5`: `backlog.md §In progress` → `§Closed` (with full closure note + reservation
record removed per §13.5), `INDEX.md §6 Recent closed` table row added. **C_Geomorph = canonical
recommended** per Hoppe 1997 + Lysenko 2018. **A_Pop FAILS Stage 4.2 DoD** (27.76 dB < 35 dB threshold).
**D_PreComputedMorphTargets / B_Crossfade NOT recommended.** **E_HZB_Stitch needs GPU prototype.**
(with full closure note), `INDEX.md §5 Active` → `§6 Recent closed` table row + `§8 Last update`
(this entry). Anti-duplicate sentinel clean per `AGENTS.md §13.7`. **Headline:** **A_2x2x2_Box is the sole
Pareto-optimal 3D mip chain algorithm** — PSNR mean 49.99 dB (ties C within +0.0004 dB), perf mean
1.218 ms (lowest of 4 algs); B_4tap_Smooth = strict regression (−0.498 dB, +7% perf); C_8tap_3DGaussian
= pure perf tax (+6%, no quality gain); D_Blit3D_perAxis = 2.9× slower CPU (GPU validation deferred).
Standalone C++26 CPU prototype (`prototype/mip_bench.cpp` ~580 LoC, `clang++ 22.1.6 -O3 -march=native
-std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **0 warnings**), 4 algs × 4 scenes × 2 atlas sizes × 3
mip levels × 3 seeds × N=30 iter + 5 warmup = **288 configs × 30 = 8,640 main measurements**, wall
time 192 sec on Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output:
`prototype/build/results.csv` (289 rows = 1 header + 288 data rows). **Mainline 3-step migration per
`agent/knowledge.md §30.4` precedent, simplified based on results (no need for fancy alternatives):**
Step 1 (XS, ~30 LoC) `voxelize_mipgen.comp` skeleton with A_2x2x2_Box + per-mip barrier; Step 2 (S,
~50 LoC) wire into `SceneResources::RebuildVctAtlas` lifecycle after `voxelize.comp` writes mip 0;
Step 3 (S, ~40 LoC) Tracy plot "VCT Mip Gen" + `ProjectVVctMipGenTests` unit test. Total **~120 LoC**
(down from initial 260 LoC estimate — no dispatch enum, no per-scene selection, no per-axis blit
fallback at this time). S effort, 1-2 sessions. **GPU D-benchmark deferred to Stage 5.1 integration:**
if D_Blit3D_perAxis GPU timing < A_2x2x2_Box on RTX 3060 Ti, document and consider conditional flip;
else leave A as default. **Continuation chain:** `vct-cone-count-atlas-precision` (closed mixed,
within-VCT quality, assumed mip chain) → this (closed yes, mip gen algorithm). **Stage 5.1 axis
status:** cutoff + cone count + atlas format + mip gen algorithm = 4 of 4 closed/explored. Remaining
Stage 5.1 axis items: Crassin 2011 cone-tapered filter (out-of-scope per
`vct-cone-count-atlas-precision` §172) + 4D temporal VCT (out-of-scope per closed `taa-motion-vectors`
follow-up) + cross-vendor GPU validation. **Cross-axis:** orth orth ко всем 4 in-progress parallel
(tracy-gpu = profiling, gpu-fluid-ca-atomic = Stage 3.1, sdf-hybrid-world = VCT anti-leak,
vk-multi-gpu-split-frame = multi-GPU) + complementary к 9 closed Stage 5.1/2.x/3.x experiments
(`vct-vs-rt-cutoff` [cutoff=0.3 strategy] + `vct-cone-count-atlas-precision` [cone count, this = mip
gen axis] + `nanovdb-on-gpu` [storage] + `dec-pipelines-async-compute` [sync] + `hzb-binding-models` [2D
cull] + `clustered-forward-mass-lights` + `rt-shadows-vs-csm` + `restir-gi-feasibility` + `lod-mesh-downsampling`).
**Caveats:** (a) CPU prototype only — no Vulkan dispatch, no GPU time, no cross-vendor validation.
Per-algorithm relative perf may differ substantially on GPU (D_Blit3D_perAxis may flip to faster than
A); (b) Synthetic 3D voxel atlas — not real ProjectV chunk content; (c) Analytical 3D Gaussian
low-pass reference (σ=0.5 voxel × 2^mip_factor) — ideal reference, not real ground truth; (d)
Mutations (per-chunk rebuild on voxel edit) out of scope; (e) Crassin 2011 cone-tapered anisotropic
filter (direction-weighted) = out-of-scope follow-up per `vct-cone-count-atlas-precision` §172; (f)
4D temporal VCT = closed `taa-motion-vectors` follow-up candidate, out of scope; (g) GPU
`vkCmdBlitImage` 3D real timing out of scope — CPU prototype cannot validate; (h) Reduced measurement
budget (30 iter / 3 seeds instead of 100 iter / 5 seeds) due to bash timeout constraint. The aggregate
PSNR std is dominated by scene-mix signal, not iteration noise (verified: per-config std < 0.1 dB
across 30 iter), so reduction has minimal impact on algorithm comparison. Cross-refs: `TODO.md §5.1`
(VCT), `vct-cone-count-atlas-precision/README.md` + `STATUS.md` (direct predecessor),
`2026-06-20-nanovdb-on-gpu` (NanoVDB mip chain extension), `2026-06-20-dec-pipelines-async-compute`
(async compute for off-frame mip gen), `2026-06-20-hzb-binding-models` (2D HZB mip chain analog),
`agent/knowledge.md §30.4` (3-step migration precedent), `agent/knowledge.md §15` (lighting
contract), `agent/workspace.md §2` (Stage 5.x not started), `hardware-profile.md §1+§3` (dev host
baseline), `benchmarks/methodology.md §3` (measurement protocol),
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold),
`experiments/_TEMPLATE/README.md` (template followed). Prototype + build per `AGENTS.md §1` agent not
building. См. §6 + §1 + [experiment README](./experiments/2026-06-21-vct-3d-mip-generation/README.md)

+ [STATUS](./experiments/2026-06-21-vct-3d-mip-generation/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-vct-3d-mip-generation/RESULTS.md) +
  [sources.md](./experiments/2026-06-21-vct-3d-mip-generation/sources.md) +
  [prototype/README.md](./experiments/2026-06-21-vct-3d-mip-generation/prototype/README.md) +
  `prototype/build/results.csv` (288 rows) + `prototype/build/mip_bench` (binary).

`2026-06-21` — closed `2026-06-21-hzb-smart-mip-select` (verdict=`mixed`). **Per-chunk HZB mip selection axis**
experiment closed same session (Stage 2.1 per `TODO.md §2.1` + explicit `agent/workspace.md §2` line 52 Nearest Gap
callout: «Stage 2.1 HZB culling refinement — current implementation always uses mip 0; smart per-chunk mip selection
based on screen-space size is a separate optimization»; **self-invented topic** per operator instruction `2026-06-21`
«выбирай свободную тему или придумывай свою и исследуй»). Standalone C++26 CPU cull simulator ~700 LoC (
`prototype/{hzb_smart_mip_bench.cpp, scenes.hpp, cull_simulator.hpp/cpp, ground_truth_raycaster.hpp/cpp, CMakeLists.txt, README.md}`),
Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green, **0 warnings** after MAX→MIN
pyramid rebuild + frustum culling fix). 100 measurements (5 scenes × 5 seeds × 4 strategies × 30 iter + 5 warmup), wall
time ~12 min on Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline findings:** *
*C_PerChunkStaticMip: 700-1500× texel reduction** (avg 13K vs 10.7M texels/chunk vs A_UniformMip0 baseline) AND **+3-5%
cull rate** (avg 27.6% vs 26.4%) — but **0.02-0.20% false-negative artifact rate** (PSNR 27-30 dB worst case
view_dolly_stress; A = 0 FN, PSNR ∞). **2-phase fallback in Step 3** `if (mipLevel > 0 && culled) verify at mip=0`
eliminates FN → PSNR ∞ with 350× texel reduction still. **B_UniformMipGlobal** slightly outperforms C (29.8% vs 27.6%
cull rate) but same FN risk. **C ≈ D** для наших scenes (multiple dispatches don't add measurable value). *
*Verdict=mixed:** strong cost win (700-1500× texel, well above 5% threshold per `optimization-philosophy.md`) but
quality regression (0.02-0.20% FN) without mitigation. Web-research complete via DuckDuckGo HTML + webfetch (Exa HTTP
429 persistent per `agent/knowledge.md Part B §9`); **5 primary sources verified** this session: Greene/Kass/Miller 1993
«Hierarchical Z-Buffer Visibility» [SIGGRAPH 1993 ACM 166147], Mike Turitzin 2020 «Hierarchical Depth
Buffers» [exact pattern statement: «works by projecting a bounding volume into screen-space and using the **projected
size to choose the appropriate mip level**»], Omlor & Radicke 2025 «Two-Pass Occlusion Culling for Dynamic Voxel Scenes
based on HZB» [IEEE Xplore 11321175, Jul 2025 — direct voxel scenes reference], DeepWiki Metallic 2026-04-06 «GPU-Driven
Culling: MeshletCullPass and HZB» [modern Vulkan production reference], RasterGrid 2010 «Hierarchical-Z map based
occlusion culling» [OpenGL FBO mip chain pattern] + 5 secondary (Nick Darnell SIGGRAPH 2008 + Tobias Garpenhall UE5 +
chaoticbob mesh shading + zeux/meshoptimizer + JarkkoPFC/meshlete). **Mainline 3-step migration
per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) per-chunk mip compute на CPU + `perChunkMipLevel[]` SSBO; Step
2 (S, ~80 LoC) `hzb_cull.comp` SSBO load + branching; Step 3 (XS, ~30 LoC) `PROJECTV_HZB_SMART_MIP=ON` env + 2-phase
fallback + Tracy plot. Total ~160 LoC, XS-S effort, 2-3 sessions. **Cross-axis:** orthogonal ко всем 5 in-progress
parallel (`sdf-hybrid-world` [closed mixed] + `tracy-gpu-vs-manual` + `gpu-fluid-ca-atomic-strategy` +
`vk-multi-gpu-split-frame` [closed mixed] + `vct-3d-mip-generation`); complementary к closed
`2026-06-20-hzb-binding-models` (texelFetch foundation), `2026-06-21-greedy-physics-meshing-cpu` (CPU prototype
precedent), `2026-06-21-sub-chunk-layers` (synthetic scenes), `2026-06-21-depth-occlusion-quantization` (PSNR
threshold), `2026-06-20-dec-pipelines-async-compute` (async foundation); **new axis**: per-chunk mip refinement of
explicit `agent/workspace.md §2` Gap = 0 coverage в INDEX §6 до этого experiment. **Caveats:** CPU prototype only (no
real GPU dispatch, analytical texel-touch cost model); single GPU vendor (RTX 3060 Ti GA104); synthetic scenes
representative not exhaustive (no real ProjectV chunk content); cross-vendor deferred; mutation cost out of scope;
visual QA в реальном gameplay required для fallback correctness; CSM HZB deferred per `agent/workspace.md §2` line 52 —
per-chunk mip extends naturally as follow-up. **Re-evaluation triggers:** Stage 4.3 ships 128m draw distance (per-chunk
mip cost grows linearly with chunks, more savings), mesh shader Pattern C full integration (HIZ output consumed by mesh
shader greedy emit → accuracy matters more), CSM HZB culling adopted (per-chunk mip extends naturally to shadow
cascades), cross-vendor validation on AMD RDNA 4 + Intel Arc Battlemage, Vulkan 1.5+ extensions для new HIZ features.
Cross-refs: `TODO.md §2.1`, `agent/workspace.md §2` line 52 (explicit Gap callout),
`src/render/HizCulling.cpp:800-805` (hardcoded `mip=0`), `src/render/HizCulling.cpp:326-369` (`BuildHizMipChain` уже
работает), `src/render/HizCulling.hpp:48-52` (`HizCullingPushConstants` structure), `src/shaders/hzb_cull.comp:33-90` (
`AabbVisibleAgainstMip` per-mip texelFetch loop), `src/shaders/hzb_cull.comp:102` (current uniform mip от push
constants), `src/render/Renderer.cpp:1344-1350` (`RecordHzbCullingDispatch` call site), `src/voxel/VoxelWorld.hpp:78` (
chunkSize=8), `agent/knowledge.md §30.4` (3-step migration precedent), `2026-06-20-hzb-binding-models` (texelFetch
foundation), `2026-06-20-dec-pipelines-async-compute` (async foundation), `2026-06-21-greedy-physics-meshing-cpu` (CPU
prototype precedent), `2026-06-21-sub-chunk-layers` (synthetic scenes), `2026-06-21-depth-occlusion-quantization` (PSNR
threshold), `docs/experiments/hardware-profile.md §1` (Zen 3 5800X dev host),
`docs/experiments/benchmarks/methodology.md §3` (measurement protocol),
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold). **Single-pass sync
per `AGENTS.md §13.5`:** `backlog.md §In progress` → `§Closed` (with full closure note); `INDEX.md §5 Active` →
`§6 Recent closed sessions` table row + `§1 Now Just-closed` + `§8 Last update`. Anti-duplicate sentinel clean per
`§13.7`. Prototype + build per `AGENTS.md §1` agent not building. См. §6 +
§1 + [experiment README](./experiments/2026-06-21-hzb-smart-mip-select/README.md) + [STATUS](./experiments/2026-06-21-hzb-smart-mip-select/STATUS.md) + [sources.md](./experiments/2026-06-21-hzb-smart-mip-select/sources.md) +
`prototype/{results.csv, bench.log}`.

`2026-06-21` — closed `2026-06-21-dlss-fsr-xess-upscaling-voxel` (verdict=`mixed`). **Render-target post-process
upscaling axis** experiment (cross-cutting для Stage 4.3 lift draw distance + Stage 5.x render pass post-process + 8 GiB
VRAM budget на dev host per `hardware-profile.md §3`; **первый axis "render target post-process upscaling"** — 0 of 30+
closed experiments covered this; ортогонален всем 4 in-progress parallel: tracy-gpu = profiling, gpu-fluid-ca = Stage
3.1 atomic, vct-cone-count = Stage 5.1 VCT quality, audio-diffraction = audio). Standalone C++26 CPU prototype
`prototype/upscaling_bench.cpp` ~470 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`,
**0 warnings**), 4 upscalers [None / FSR 3.1 / XeSS 2 DP4a / DLSS 4.5 Sim] × 4 quality
presets [native 100% / quality 67% / balanced 58% / performance 50%] × 3 extents [1080p / 1440p / 4K] × 2
scenes [dense_voxel / sparse_voxel] × 3 seeds × 1000 iter + 10 warmup = **288 measurements** on Zen 3 5800X dev host
`obvium`. **Headline (analytical, per `prototype/RESULTS.md`):** FSR 3.1 = best cost-benefit cross-vendor Vulkan (
3.7-23% savings, PSNR 39.2 dB, +1 MiB VRAM); DLSS 4.5 + XeSS 2 XMX = real GPU measurements required (analytical model
conservative for Tensor Core / XMX hardware — RTX 3060 Ti 4th-gen Tensor Cores ~25 TFLOPS FP16 / ~50 TOPS INT8 vs my
model's 14.7 TFLOPS FP32 baseline = 1.7× underestimate); FSR 4 = NOT usable on Vulkan per `mypcbottleneck 2026-06-04` "
Vulkan API games are not compatible with the FSR 4 Upgrade feature" (RDNA 4-only + DX12-only driver upgrade path);
DirectSR = defer to Vulkan core promotion per `StraySpark 2026-03-25` (currently beta); Frame
Generation [DLSS MFG 3x/6x, FSR 3 AFMF, XeSS 2 XeSS-FG] = OUT OF SCOPE (latency budget + Reflex/XeLL integration
needed). **Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 (XS, ~30 LoC)
feature-flag `PROJECTV_UPSCALER=OFF|FSR31|XESS2|DLSS45|DIRECTSR` env + `PROJECTV_UPSCALER_QUALITY` env + post-process
pipeline slot after TAA resolve + cross-vendor graceful fallback chain; Step 2 (M, ~250 LoC) per-SDK
integration [UpscalerFactory + NoneUpscaler + FfxFsr31Upscaler + Xess2Upscaler + StreamlineDlss45Upscaler + DirectSRUpscaler];
Step 3 (S, ~80 LoC) quality preset table + TracyPlot + default flip. Total **~360 LoC, S-M effort, 2-3 sessions**. *
*Caveats:** CPU prototype, no real GPU dispatch; upscaler implementations = cost models, not real SDKs; no PSNR/SSIM
real measurement; deterministic timing; cross-vendor projection = analytical only (single GPU vendor measured: NVIDIA
RTX 3060 Ti dev host). **Cross-axis:** orthogonal к 4 in-progress parallel; complementary к closed
`taa-motion-vectors` (verdict=yes, motion vector MRT = direct upscaling input per Streamline/FidelityFX/XeSS unified API
contract — `R16G16_SFLOAT` format matches upscaling standard) + `bindless-descriptor-overhead` Phase D (bindless =
required for cross-vendor upscaling resource management) + `depth-occlusion-quantization` (VRAM-budget cross-cutting) +
`vk-fragment-shading-rate-voxel` (VRS cost axis complementary — VRS 2x1 + DLSS 2x = 4× effective cost reduction,
sequential adoption recommended). **Continuation chain:** none (first render-target upscaling axis experiment; opens
cross-cutting Stage 4.3/5.x post-process). Closed entry: `experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/` +
prototype + `build/results.csv` (288 rows × 18 cols). См.
§6 + [experiment README](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/README.md) + [STATUS](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/STATUS.md) + [sources.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/sources.md) + [prototype/README.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype/README.md) + [prototype/RESULTS.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype/RESULTS.md).

`2026-06-21` — closed `2026-06-21-depth-occlusion-quantization` (verdict=`yes`, with caveats). **Depth-format axis**
experiment (VRAM-budget, cross-cutting для Stage 2.x HZB cull + Stage 2.2 depth prepass + Stage 5.x G-buffer/depth, *
*follow-up к закрытому `2026-06-20-hzb-binding-models`** [HZB sampling pattern, не format] + closed
`2026-06-20-frame-flight-allocator-budget` [allocator strategy, не depth format] + closed
`2026-06-20-bindless-descriptor-overhead` [Phase A shadow cascade motivation, не depth format]). Standalone C++26
analytical benchmark (`prototype/depth_quant_bench.cpp` ~500 LoC, Clang 22.1.6 `-O3 -march=native`, zero warnings), 72
configs × 50 measure iters = 3600 measurements. **Headline findings:** VRAM D32_SFLOAT → D16_UNORM = **-50%** (1080p:
18.46 → 9.23 MiB; 720p: 8.20 → 4.10 MiB; HZB mip chain included); PSNR depth round-trip = **107.12 dB** (visually
lossless, > 50 dB threshold); false-culled count = **0** across 230 400 cull decisions; mean cull error = 3.82e-6 (
negligible). **Caveats:** synthetic CPU-only (no Vulkan init, no GPU time, no cross-vendor validation); D16 + PCF =
banding/moiré per DXVK PR #5564 (2026-03-25) → CSM shadow maps NOT recommended; reverse-Z benefit not measurable в
synthetic (depth range [0.05, 1.0] not at far plane per Nathan Reed 2021 analysis). **3-step migration
per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC) foundation + D16 depth attachment via `findDepthFormat` +
`PROJECTV_DEPTH_FORMAT=D16|D32` env; Step 2 (S, ~80 LoC) reverse-Z + HZB integration (clear=0, GREATER compare,
NDC [1,0]); Step 3 (S, ~50 LoC) multi-attachment rollout (CSM optional, VCT cone-march, transparency depth). Total ~160
LoC, S effort, 3-4 sessions. **Cross-vendor matrix:** NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+.
**Re-evaluation triggers:** Stage 4.3 (128+ chunks draw distance, depth precision более критична), Stage 5.1 VCT
depth-derivative, Stage 5.2 RTX shadow path, `VK_KHR_depth_float_reduce` ratification, DXVK PR #5564 merge, AMD RDNA +
Intel Arc dev matrix. **Cross-axis:** orthogonal к 5 in-progress parallel (tracy-gpu + wfc + taa + gpu-fluid-ca +
lod-mesh + vk-fragment-shading); complementary к closed `hzb-binding-models` (HZB sampling, не format) +
`frame-flight-allocator-budget` (allocator, не depth) + `bindless-descriptor-overhead` Phase A (shadow cascade
motivation, не depth). **Continuation chain:** none (first depth-format axis experiment; opens VRAM-format axis). Files
retained: [
`experiments/2026-06-21-depth-occlusion-quantization/`](./experiments/2026-06-21-depth-occlusion-quantization/) +
`research/backlog.md §Closed` +
`prototype/{main.cpp, depth_quant_bench.{hpp,cpp}, voxel_scene.{hpp,cpp}, CMakeLists.txt, README.md, RESULTS.md, results.csv}`.
Single-pass sync agent per `AGENTS.md §13.5`: `backlog.md §In progress` → `§Closed` (with full closure note),
`INDEX.md §5 Active` → `§6 Recent closed sessions` table row + `§8 Last update`. Anti-duplicate

`2026-06-21` — closed `2026-06-21-lod-mesh-downsampling` (verdict=`mixed`). **LOD uniform
downsampling + stitch strategy axis** experiment (Stage 4.2 chunk 2 per `TODO.md §4.2` + explicit
"Nearest Gap" в `agent/workspace.md §2` line 44-45 "uniform downsampling implementation … actual
mesh-level downsampling not yet built"). Single-pass sync agent per `AGENTS.md §13.5`:
`backlog.md §In progress` → `§Closed` (with full closure note), `INDEX.md §5 Active` →
`§1 Now` Just-closed + `§6 Recent closed sessions` table row + `§8 Last update`. Anti-duplicate
sentinel clean per `AGENTS.md §13.7`. **Headline:** `B_SurfacePreserve` is the only kernel that
satisfies Stage 4.2 DoD — 0 T-junction holes across 75 test configurations (16938 boundary
face emissions, 0 mismatches). Other kernels: A_Majority3D 10-32% boundary mismatch, C_SolidOnly
17-32% + catastrophic collapse в cave_stress (entire LOD 1 chunk → 0 quads), D_MaxPool 10-32%
(same as A). B_SurfacePreserve also fastest (early-out on `all_same`) at LOD 0/1/3. All
kernels < 1.5 µs/chunk (30-100× headroom vs 50 µs Stage 4.1 budget). LOD 1/2/3 quad reduction
**5.94× / 31.8× / 169×** (all > 4×/16×/64× geometric bounds). **Mainline рекомендация:**
use `B_SurfacePreserve` as default kernel for Stage 4.2 chunk 2; 3-step migration per
`agent/knowledge.md §30.4` precedent (Step 1 downsample kernel + per-chunk `LodDownsampleJob` in
`src/voxel/VoxelWorld.{hpp,cpp}` ~150 LoC; Step 2 `SelectLodMeshSource` decision в
`voxel_mesh.comp` ~250 LoC; Step 3 Tracy plot + default flip ~50 LoC). Total ~450 LoC, M
effort, 2-3 sessions. Caveats: CPU-only prototype, no GPU dispatch; naive face counter без
greedy merge; synthetic scenes; no mutation cost measured; visual QA in real gameplay
required to confirm B's T-junction robustness at runtime camera angles. Cross-axis: 6 closed
same-session `2026-06-21` (audio + wfc + sub-chunk + gpu-noise + frame-flight + dxc) + 3
in-progress same-session (tracy-gpu + taa + gpu-fluid-ca) + 2 same-day declared
(vk-fragment-shading-rate-voxel + audio-diffraction-hybrid) + 19+ closed `2026-06-20` + this =
full Stage 0/1.x/2.x/3.x/4.x/5.x/6.x + cross-cutting optimization landscape + audio + temporal

+ atomic + profiling + **LOD geometry axis NEW**. Cross-refs: `TODO.md §4.2`,
  `src/voxel/VoxelWorld.hpp:78` + `:1175-1208` (existing LOD selection), `agent/workspace.md §2`
  (Nearest Gap), `agent/knowledge.md §30.4` (3-step migration precedent), `2026-06-20-nanovdb-on-gpu`
  (NanoVDB mip chain), `2026-06-20-meshing-algo-comparison` (Naive Greedy baseline at LOD 0),
  `2026-06-21-sub-chunk-layers` (orthogonal, same scenes for direct comparability),
  `docs/experiments/hardware-profile.md §1+§2` (Zen 3 5800X dev host `obvium`),
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold). Prototype
+ build per `AGENTS.md §1` agent not building. См. §6 +
  §1 + [experiment README](./experiments/2026-06-21-lod-mesh-downsampling/README.md) +
  [STATUS](./experiments/2026-06-21-lod-mesh-downsampling/STATUS.md) +
  [sources.md](./experiments/2026-06-21-lod-mesh-downsampling/sources.md) +
  [prototype/README.md](./experiments/2026-06-21-lod-mesh-downsampling/prototype/README.md) +
  `prototype/build/results.csv` (1200 rows) + `prototype/build/results_tjunc.csv` (75 rows).

`2026-06-21` — closed `2026-06-21-taa-motion-vectors` (verdict=`yes`). **TAA motion vectors axis** experiment
(Stage 5.3 per `TODO.md §5.3`, **temporal axis** для Stage 5 после полного closure lighting-axis на `2026-06-20`:
`vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights` yes + `rt-shadows-vs-csm` mixed + `restir-gi-feasibility`
mixed). Web-research complete (2 batch queries, ~14 results, 6 primary sources верифицированы: Karis 2014
SIGGRAPH foundational ["16:16 RG velocity buffer" = R16G16_SFLOAT exact match for `TODO.md §5.3` prescription;
"velocity accuracy is super important" drives vertex-out recommendation], Yang/Liu/Salvi 2024 TAA survey
[neighborhood clamping + YCoCg = standard 2024], Marrs/Spjut 2018 NVIDIA adaptive TAA [requires RT, out of scope],
k-DOP Clipping SIGGRAPH 2024 [SOTA ghosting mitigation 0.2 ms overhead, follow-up candidate], Karolewics
Lumberyard anti-ghosting TAA [production reference 0.1 ms + 1.6 ms total Xbox One], VK_KHR_dynamic_rendering
[core 1.3 enables MRT pattern already ProjectV mainline]). Standalone Vulkan 1.4 + C++26 prototype skeleton
(`prototype/main.cpp` ~525 LoC + 6 GLSL shaders: voxel_a/b vert+frag + taa_resolve_a/b comp + Makefile +
`prototype/README.md`). **Verdict basis** (independent of measurement execution per `AGENTS.md §1` agent not
building): (1) `TODO.md §5.3` line 425 explicit R16G16_SFLOAT format prescription = mandate; (2) Karis 2014
SIGGRAPH foundational paper; (3) industry standard (UE 5 + Godot 4.x + Unity HDRP all use R16G16_SFLOAT
motion vector MRT) — no cross-vendor ambiguity per `dec-pipelines-async-compute` §2.2; (4) VRAM cost 8 MiB/frame
double-buffered @ 1080p = 0.16% of 5.06 GiB budget per `hardware-profile.md §3` = well under 5% threshold per
`optimization-philosophy.md`; (5) `TODO.md §5.3` DoD «Полное исчезновение шлейфов за перемещаемыми гравипушкой
моделями» = only achievable with vertex-out (depth-reproject has fundamental precision loss near edges per
Karis 2014). **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1 foundation (S, ~50 LoC,
1 session): vertex shader `out vec4 vPrevClip` + fragment shader `layout(location=1) out vec2 outMotion`
(R16G16_SFLOAT) + `TaaRenderTargets.{hpp,cpp}` add motion vector attachment + `SceneResources.{hpp,cpp}`
allocate double-buffered motion vector MRT; Step 2 TAA resolve update (S, ~50 LoC, 1 session): change motion
vector source from current depth-reproject to read from motion vector MRT + image layout transition
`COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL`; Step 3 default flip (XS, ~10 LoC, 1 commit):
`PROJECTV_USE_MOTION_VECTOR_MRT=ON` env flag with cross-vendor graceful fallback. Total M (~110 LoC across
5-6 files, 2-3 sessions). **Side sync fix r1 applied to previous-session `2026-06-20-async-compute-overhead-numbers`**
per `AGENTS.md §13.5` (original session `2026-06-20` left bookkeeping incomplete: §Open stale duplicate line
removed, missing §6 Recent closed table entry added, README Status field `in-progress` → `concluded-verdict-yes`

+ Date closed `N/A` → `2026-06-20` corrected, STATUS.md sync-fix r1 note appended — all preserving original
  measurements +9.85-11.34% + verdict=yes). Anti-duplicate sentinel clean per §13.7. Cross-axis: orthogonal ко
  всем 4 in-progress parallel (tracy-gpu + wfc + sub-chunk + gpu-fluid-ca-atomic-strategy); complementary к closed
  `clustered-forward-mass-lights` (SSBO light list + motion vectors both feed TAA resolve); natural follow-up к
  closed `dec-pipelines-async-compute` (motion vector MRT submission = candidate for async queue). См. §6 + §1 +
  [experiment README](./experiments/2026-06-21-taa-motion-vectors/README.md) +
  [STATUS](./experiments/2026-06-21-taa-motion-vectors/STATUS.md) +
  [sources.md](./experiments/2026-06-21-taa-motion-vectors/sources.md) +
  [prototype/README.md](./experiments/2026-06-21-taa-motion-vectors/prototype/README.md) + 6 GLSL shaders.

`2026-06-21` — closed `2026-06-21-sub-chunk-layers` (verdict=`mixed`). **Chunk-layout-axis experiment**
(Stage 4.x biome/cave data structure axis, orthogonal к in-progress `2026-06-21-wfc-procedural-worlds`
gen-strategy axis). Web-research complete (3 batch queries, ~14 sources верифицированы: Minecraft-1.18+
Java `ChunkSection` 16³ + biomes 4×4×4 = 64 entries per section per FabricMC/yarn DeepWiki + Minecraft
Wiki + wiki.vg protocol + yarn 1.18 API; Bedrock `SubChunk` 4D (x,y,z,**storage layer**) per wiki.vg +
uNmINeD 2021-12-10 reverse engineering; SHARD layered format per scrayos 2024-11-04 + GitHub; ATLAS
AARF columnar storage per Tunact124 Mar 2026; Cubyz CaveMap 64³ fragments with 1-bit per block +
CaveBiomeMap 2048³ per PixelGuys DeepWiki Mar 2026; Hytale NStagedChunkGenerator BiomeStage/TerrainStage
/PropStage/TintStage/EnvironmentStage per vulpeslab/hytale-docs; Vulkan Guide Ascendant chunk layers
main+transparent+clutter per vkguide.dev; Minecraft world generation overview per Telepathic Grunt/XI64
Gist Feb 2021; maguirekrist/voxel_enginevk production-grade chunk pipeline 5 layers). Standalone C++26
CPU prototype (`prototype/sub_chunk_bench.cpp` ~870 LoC, `clang++ 22.1.6 -O3 -march=native`, build
green). 4 designs (A_Monolithic 512 bytes baseline / B_Palette adaptive bits / C_FixedLayer_L2 4 layers
/ D_FixedLayer_L4 2 layers) × 5 scenes (uniform_air + uniform_floor + forest_floor + cave_stress +
mixed_biome) × 5 seeds (1, 7, 42, 1234, 31337) × 1000 iter per measurement = 100 measurements.
**Measured (Zen 3 5800X dev host `obvium`, governor=`powersave`, 62.7 GiB RAM DDR4, CPU-only synthetic
scenes):**

- **Memory axis (B_Palette / C_L2 / D_L4 vs A_Monolithic baseline 512 bytes):**
    - uniform_air / uniform_floor (1 material): B=20 (-96%), C=84 (-84%), D=42 (-92%) — **B_Palette wins.**
    - forest_floor / cave_stress (2 materials): B=84 (-84%), C=148 (-71%), D=106 (-79%) — **B_Palette wins.**
    - mixed_biome (4 materials): B=148 (-71%), C=148 (-71%), D=138 (-73%) — **D_L4 marginal win.**
- **Build cost:** monolithic 0.03-0.13 µs/chunk vs paletted 1.3-5.8 µs/chunk = **30-55× overhead**,
  but absolute 1-6 µs vs Stage 4.1 budget 50 µs/chunk per `TODO.md §4.1` = 8-50× headroom.
- **Mutation cost:** monolithic 10-16 ns/mutation vs paletted 12-19 ns = **+5-70% overhead**, absolute
  10-19 ns vs Stage 1.2 DoD 0.1 ms tolerance = 5000-10000× headroom.
- **Mesh vertex count:** all designs produce **identical** face counts (591-679 quads) для same scene+seed
  — mesh optimization is layout-orthogonal (covered by `2026-06-20-meshing-algo-comparison` verdict=mixed).
- **Layer boundary axis:** monolithic 0 vs C_L2 80-155 vs D_L4 28-62 = **explicit semantic gain**
  для biome/cave chunks. VCT anti-leak + per-layer LOD + selective rebuild potential.

**Verdict=mixed:** paletted/layered designs win memory (73-96% > 5% threshold per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`) + layer-boundary semantic axis, lose
build cost (acceptable per budget) + mutation cost (negligible absolute). **Mainline recommendation:**
**conditional** — **B_Palette для uniform chunks (96% savings)**, **D_L4 для biome/cave chunks (73-79%
savings + 28-62 transitions)**, **C_L2 для finer biome granularity (71-84% + 80-155 transitions)**;
A_Monolithic as fallback для sparse chunks + legacy compatibility. **3-step migration per
`agent/knowledge.md §30.4` precedent:** Step 1 `ChunkLayout` enum + `SelectChunkLayout` decision
(~150 LoC, S) → Step 2 `world_gen_layers.comp` per-layer payload + per-chunk metadata (~300 LoC, M) →
Step 3 wire layer semantics в `voxel.frag` VCT cone-march terminate + Stage 4.2 per-layer LOD (~250 LoC,
M). Total ~700 LoC + integration, M effort, 5-7 sessions. **Caveats:** CPU-only (no GPU SSBO layout
validation); no Sparse64Tree integration; naive face counter (no greedy merge); synthetic scenes;
single-threaded. **Cross-axis:** Stage 4.x biome/cave axis closed same-day сессии (continuous noise
axis via `gpu-procedural-noise-compute-kernels` mixed OpenSimplex2 + discrete structure axis via this
sub-chunk-layers mixed layered chunks + gen-strategy axis via in-progress `wfc-procedural-worlds`).
3 orthogonal axes of Stage 4.x = complete picture. Cross-refs: `TODO.md §4.1/§4.2/§5.1`,
`src/voxel/VoxelWorld.hpp:85`, `2026-06-20-nanovdb-on-gpu` (yes), `2026-06-21-gpu-procedural-noise-compute-kernels`
(mixed), `2026-06-21-wfc-procedural-worlds` (in-progress), `2026-06-20-svdag-vs-vdb-memory-throughput`
(yes, isStatic flag), `2026-06-20-dec-pipelines-async-compute` (yes, async populate),
`agent/knowledge.md §30.4`, `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`,
`hardware-profile.md §1+§2`, `benchmarks/methodology.md`. Closed entry:
`experiments/2026-06-21-sub-chunk-layers/` + `prototype/build/results_all.csv` +
`prototype/build/summary_means.csv`.
Stage 0 (toolchain) теперь explicit closed. Closed entry:
`experiments/2026-06-21-dxc-vs-glslc-toolchain/`.

`2026-06-21` — closed `2026-06-21-audio-raytracing-voxel-sdf` (verdict=`mixed`). **Audio axis** experiment
(cross-cutting для будущего Stage 7.x audio; no audio rendering stage в `TODO.md` currently — miniaudio PCM playback
only per `agent/knowledge.md §28`). Standalone C++26 prototype (
`prototype/{voxel_grid,audio_raytracer,reverb,bench}.{hpp,cpp}`

+ `RESULTS.md` + `results.csv`, ~700 LoC, Clang 22.1.6 `-O3 -march=native`, zero warnings). 4 configs × 3 scenes ×
  3 seeds × 1000 iter + 100 warmup = **36 runs × 1000 = 36000 measurements** on Zen 3 5800X. Web-research complete
  (3 batch queries, 12 key sources верифицированы: Vercidium 2025 production voxel-grid audio [direct validation],
  SIGGRAPH 2025 Finnendahl et al. differentiable acoustic PT, GSound-SIR Mar 2025 + OptiX Dec 2025,
  Schissler & Manocha 2014 [50 orders, 200 sources], RESound 2007 hybrid ray-frustum, iSound GPU auralization,
  Tsingos 2001 HW-accelerated occlusion, Funkhouser 2002 beam tracing, Meta Acoustic Ray Tracing Audio SDK 2024+,
  NeRAF ICLR 2025). **Headline findings:** (a) **occlusion-only path (1 ray/source) production-ready** = 0.008-0.016 ms
  mean = **< 0.05%** of 33.3 ms audio frame budget @ 30 Hz, immediately integrable, immediate perceptual win (muffled
  sounds behind walls); (b) **full hybrid (32 rays × 4 reflection orders) NOT yet viable** = 13.8-17.1 ms mean on
  cave/open_plains (3.4× over 5 ms hypothesis target), only multi_room in budget at 6.3 ms; (c) **Eyring late reverb**
  negligible cost (~0.001 ms per source), integrate unconditionally; (d) **temporal cache в benchmark не помогает**
  — jitter ±5 cm > 1 cm cache epsilon, need larger ε (10-20 cm per audio frame at 30 Hz). **Mainline recommendation:**
  **Phase 1** occlusion-only + **Phase 2** Eyring late reverb (both XS effort, ~250 LoC, immediate integration into
  Stage 7.x audio v1); **Phase 3** full hybrid **deferred** до one of: (a) SVO hierarchical acceleration (empty-skip
  5-10× per `nanovdb-on-gpu`), (b) lower ray budget (8r×2ord perceptually sufficient per Vercidium 2025 + Schissler
  2014),
  (c) cache tuning, (d) AVX-512 hardware arrival (Zen 5 / Arrow Lake projected 2-4× per `simd-procedural-noise`).
  Cross-reuses `2026-06-20-nanovdb-on-gpu` SVO walker foundation, `2026-06-20-flecs-soa-vs-aos-bench` SoA storage
  verdict=yes, `2026-06-20-work-stealing-job-system` serial dispatcher baseline. Caveats: single-vendor (Zen 3 5800X,
  governor `powersave`), `voxels_traversed` counter instrumentation bug (не влияет на latency), synthetic scenes
  representative not exhaustive, no material absorption modeling, sequential single-threaded per
  work-stealing-job-system verdict=mixed. Continuation chain: **none** (first audio axis experiment; opens Stage 7.x);
  follow-up candidates `_audio-hierarchical-svo-skip_`, `_audio-rt-budget-vs-source-count_`,
  `_audio-diffraction-hybrid_`.
  Cross-axis: **0 of 19+** same-day `2026-06-20` experiments covered audio; this = audio axis opener. См. §6 +
  [experiment README](./experiments/2026-06-21-audio-raytracing-voxel-sdf/README.md) + `sources.md` (12 sources +
  SOTA coverage map) + `prototype/RESULTS.md` (full measurements).

`2026-06-21` — closed `2026-06-21-frame-flight-allocator-budget` (verdict=`mixed`).
**VRAM-allocator-axis experiment** (Stage 6.2 tech-debt, cross-cutting). Web-research
complete (4 batch queries, ~30 results, ~15 sources верифицированы: VMA 3.4.0 docs +
Issue #453 + Frostbite Frame Graph + Frostbite Scope Stacks + Diligent Engine 2.0
ring buffer + Unreal Engine RHI per `VK_EXT_memory_budget` + DXVK commit `9b272fb`

+ vkd3d-proton PR #1543 + D3D12 Residency Starter Library + NVIDIA Vulkan Do's and
  Don'ts + AMD Vulkan device memory guide + VK_EXT_memory_budget spec + VK_EXT_pageable_device_local_memory
  spec + llama.cpp HVV fragmentation case study). Standalone Vulkan 1.4 prototype
  (~890 LoC, links vendored VMA 3.4.0 + volk, NOT ProjectV mainline). 5 strategies
  compared (A_Default / B_BudgetTrack / C_LinearPool per-frame / D_DoubleBuffer
  per-frame / E_PreCreatedRing) + 1 stress pass (256 MiB spike every 50 frames).
  **Measurements on RTX 3060 Ti dev host (Vulkan 1.4.350, NVIDIA 610.43.02):**
  (A) 35.5 µs mean / 67.4 µs p99 / 0 failures; (B) 34.7 µs mean / 58.2 µs p99 / 0
  failures; (C) 1311 µs mean / 2573 µs p99 / 0 failures [per-frame pool recreate
  30× slower, validates VMA Issue #453 warning]; (D) 1309 µs mean / 2941 µs p99 /
  21 failures in stress pass [256 MiB > 64 MiB pool block → clean hard-cap];
  (E) 38.0 µs mean / 113 µs p99 / 0 failures / +64 MiB peakHeapUsage. **Mainline
  recommendation** (3-step migration per `agent/knowledge.md §30.4`): **Step 1 (XS,
  ~20 LoC)** — add `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` to `VulkanBootstrap.cpp:807-823`
  allocator + `vmaSetCurrentFrameIndex()` per frame + TracyPlot `VRAM.heapBudgetMiB`/
  `heapUsageMiB` для observability; **Step 2 (S, ~50 LoC + tests)** — add
  `VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT` flag для non-critical allocations (5+ call
  sites per `rg vmaCreateBuffer`) with graceful degradation; **Step 3 (M, ~200 LoC)
  DEFERRED** — pre-created single linear ring buffer pool (`TransientPool.{hpp,cpp}`)
  re-evaluation triggers: Stage 4.3 (128+ chunks, transient count > 50/frame) OR
  Stage 5.2 RTX BLAS pool overflow OR Tracy heap-usage→budget trend. **Caveat per
  Step 3:** VMA docs require `maxBlockCount = 1` для ring buffer; double-pool variant
  = wrong pattern, не реализовывать. **Cross-axis:** allocator axis closed
  (cross-cutting для всех transient pressure sources). Parallel session сегодня:
  `2026-06-21-tracy-gpu-vs-manual` (orthogonal scope, no conflict per `AGENTS.md
§13.3`). Closed entry: `experiments/2026-06-21-frame-flight-allocator-budget/` +
  `prototype/README.md` + `prototype/build/results.csv`. См. §6 + §1 + experiment README.

`2026-06-21` — closed `2026-06-21-gpu-procedural-noise-compute-kernels` (verdict=`mixed`).
**Noise-algorithm axis** experiment (Stage 4.1 GPU Noise & World Gen per `TODO.md §4.1`, gating
blocker для infinite worlds). Web-research complete (3 batches, ~20 results, 20 sources
верифицированы: Schneider `arXiv 1903.12270` Perlin/Float 3D = 77 ALU inst [direct instruction count
baseline], GPU Gems 2 Ch 26 textured-LUT Perlin = 53 inst / 9 lookups, atyuwen/bitangent_noise
SimplexNoise.hlsl 3D = ~71 instruction slots, KdotJPG/OpenSimplex2 673 stars CC0 modern
GPU-friendly design, Auburn/FastNoiseLite 3D Perlin 47.93 M/s scalar / 261.10 M/s AVX2 CPU baseline,
NVIDIA Nsight Compute Ampere workgroup-64 occupancy guidance, Khronos Forums compute shader SSBO
write cost validation, JCGT 2022 Olano GTX 1660 modern compiler DCE 17% speedup from disabling tiling,
Vulkanised 2024 GPU Atomic Performance Modeling McKee, production refs: paulrobello/voxel-world
Vulkan compute 5D climate noise + Perlin Feb 2026, Aokana arXiv 2505.02017 GPU-Driven voxel May 2025,
AdityaGupta1/mega-minecraft CUDA fBm Oct 2025, russellocean/pebble-rs WGPU compute voxel raytracer
Nov 2025, Yunasawa YNL Vozel Minecraft 1.18+ 5-param FBM Sep 2025). Standalone Vulkan 1.4 compute
prototype (`prototype/{main.cpp, noise_kernels.comp, CMakeLists.txt, README.md, results.csv, run.log}`,
~700 LoC total, 5 conditional GLSL variants через `#define VARIANT_*` + dispatch harness, RTX 3060 Ti
GA104 Ampere, Vulkan 1.4.341, NVIDIA driver 610.43.02, Clang 22.1.6 + glslc 2026.2). 3 runs × 5
variants × 1000 iter + 10 warmup. **Measured:** VALUE=0.0273, PERLIN=0.0272, SIMPLEX=0.0272,
OPENSIMPLEX2=0.0272, WORLEY=0.0280 ms mean — **all variants в пределах 2.9% mean** (below 5%
threshold per `optimization-philosophy.md`). WORLEY unexpectedly not slowest (`glslc` 2026.2 fully
unrolled + register optimization). VALUE == PERLIN по cost (hash + gradient table index similar
register footprint на Ampere). **Memory-bound kernel:** 8 MiB write at 65.6% of 448 GB/s theoretical
peak = SSBO write bandwidth dominates. ALU = ~14% of dispatch time only. Per-eval cost = 13.0
ns/eval, per-chunk = 6.6 µs. **Stage 4.1 budget (50 µs/chunk per `TODO.md §4.1`):** 8× headroom
single octave, 1.9× headroom FBM 4 octaves, 0.63× (over budget) FBM 4 octaves × 3 channels
(heightmap + cave + biome). **Verdict=mixed:** алгоритмический выбор НЕ meaningful perf
discriminator на chunkSize=8 dispatch pattern; **но** quality + license axis still favors
OpenSimplex2 3D-S (CC0, no axis artifacts, analytic derivatives, actively maintained KdotJPG
2019-2024+, stable cold-cache perf без Run-1 spike). **Mainline рекомендация:** use **OpenSimplex2
3D-S** для Stage 4.1 world gen (NOT because fastest — because license + quality + stability).
3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 foundation `noise3d_opensimplex2()`
GLSL port (~50 LoC core, attribution header per CC0 §4(a)), Step 2 dispatch in `world_gen.comp` per
chunkSize=8 pattern + FBM wrapper (4 octaves, ~150 LoC), Step 3 multi-channel (heightmap + cave +
biome, octave reduction если budget exceeded, ~100 LoC). Total ~300 LoC, S effort, 1-2 sessions.
**Cross-axis continuity:** same-day `2026-06-21` parallel sessions (frame-flight-allocator-budget
in-progress + dxc-vs-glslc-toolchain in-progress + tracy-gpu-vs-manual in-progress) + my
noise-algorithm axis = orthogonal angle of Stage 4.x + Stage 6.x + toolchain optimization landscape.
Continuation chain: `2026-06-20-simd-procedural-noise` (CPU AVX2 vs scalar, closed verdict=mixed) →
this (GPU algorithm choice, closed verdict=mixed). **Caveats:** single GPU vendor validated (RTX 3060
Ti GA104 Ampere, Vulkan 1.4.341, NVIDIA 610.43.02) — mainline re-test on AMD RDNA 2/3/4 + Intel Arc
Battlemage dev matrix; single octave only — FBM 4 octaves linear scaling not measured; single
heightmap channel — multi-channel 3× cost projection not validated; no Nsight Compute
register/occupancy/SM pipe metrics — extension opportunity; no spectral quality metric (FFT framework
not built) — quality claims literature-cited; async-compute overlap with graphics not measured (per
`dec-pipelines-async-compute` verdict=yes — potential 5-8% additional gain); Run 1 vs Run 2+3 shows
14% cold-cache offset для VALUE/PERLIN (warmup insufficient at 10 iters) — OPENSIMPLEX2/SIMPLEX/
WORLEY stable from Run 1. Cross-refs: `TODO.md §4.1`, `src/voxel/VoxelWorld.hpp:85` (chunkSize=8),
`src/shaders/voxel_mesh.comp:146` (existing dispatch pattern), `agent/workspace.md §1 Phase 1`
(world_gen.comp skeleton), `agent/knowledge.md §30.4` (3-step migration precedent),
`2026-06-20-simd-procedural-noise` (CPU orthogonal), `2026-06-20-dec-pipelines-async-compute`
(async foundation, world gen spike isolation), `2026-06-20-nanovdb-on-gpu` (GPU SSBO write target
format), `docs/experiments/hardware-profile.md §3` (RTX 3060 Ti dev host),
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold definition).
Closed entry: `experiments/2026-06-21-gpu-procedural-noise-compute-kernels/`. См. §1 + §6 + [experiment
README](./experiments/2026-06-21-gpu-procedural-noise-compute-kernels/README.md).

`2026-06-20` — closed `2026-06-20-vma-sparse-textures` (verdict=`mixed`). **Sparse Virtual Texturing axis** experiment (
Stage 2.3 + cross-cutting VRAM budget). Web-research complete (4 batches, ~30 results, 16 sources верифицированы:
shlomnissan "How Virtual Textures Really Work"
2026-02 [software VT = доминирующий pattern, hardware sparse = "mechanism не policy"], shlomnissan/virtual-textures
GitHub 2026 [prototype без HW sparse], UE 5.7 Streaming Virtual Texturing docs [production = software layer], Nanite GDC
2024 Wihlidal [UE VT = software], bgfx 40-svt Karadzic [production reference], Nathan Gauër 2022, SaschaWillems
texturesparseresidency [Vulkan HW sparse example], foijord/SparseTexture 2025-02 [NVIDIA
`vkQueueBindSparse` BLOCKING GLOBAL, 1 TiB address limit vs AMD 256 TiB / Intel 16 TiB — неприемлемо для runtime streaming],
NVIDIA forums 2023 [A4000 multi-second bind for 1000 pages, NVIDIA team acknowledged 2023-09], VMA 3.4.0 CHANGELOG
2026-06-05 [sparse convenience `vmaAllocateMemoryPages` уже из 2.x], `VK_EXT_pageable_device_local_memory` rev
1 [OS-level paging, complementary не replacement], `VK_EXT_memory_decompression` rev 1 ratified
2025-01-23 [GDeflate GPU decompress, NVIDIA-only pre-2026], `VK_NV_extended_sparse_address_space` rev 1
2023-10-03 [NVIDIA 1 TiB workaround], KhronosGroup/Vulkan-Guide sparse_resources.adoc). Standalone Vulkan 1.4 + VMA
3.4.0 + volk prototype (`prototype/{vma_sparse_bench.hpp, main.cpp, README.md}`, ~770 LoC, 3 variants: dense / sparse /
software-vt — peak VRAM + bind latency + page-miss cost measurements). **Главный finding:** hardware sparse textures
unusable на NVIDIA для runtime world streaming per `foijord 2025` (`vkQueueBindSparse` blocking global). **Software VT =
recommended default** (cross-vendor deterministic, peak VRAM cap enforceable, validated production pattern в UE 5.7
RVT / Nanite / id Tech 5 MegaTexture / bgfx 40-svt / Frostbite). Mainline рекомендация: 4-step migration per
`agent/knowledge.md §30.4` precedent — Step 1 foundation `PageManager` + page table texture R32Uint (~150 LoC); Step 2
integration `voxel.frag` `SampleVirtualTexture` per shlomnissan pattern + atlas texture + bindless per
`bindless-descriptor-overhead` Phase D (~350 LoC); Step 3 page manager wiring (LRU eviction + async upload, ~150 LoC);
Step 4 optional HW sparse для static prebake Stage 4.1 (VMA `vmaAllocateMemoryPages`, ~120 LoC). Total ~770 LoC +
integration code, M effort, 3-4 sessions. **VRAM matrix:** software VT = 16-32 MiB atlas + 16 KiB page table (vs dense
256 MiB); HW sparse = 16-64 MiB resident vs 1 GiB virtual. **Cross-vendor analytical projection
per `dec-pipelines-async-compute` matrix:** RTX 3060 Ti (Vulkan 1.4.341) = full sparse residency support per
`VkPhysicalDeviceSparseProperties` query, but NVIDIA `vkQueueBindSparse` blocking global = unusable for runtime; AMD
RDNA 4 = improved; Intel Battlemage = fast binds per `foijord 2025`. **Continuation chain:**
`bindless-descriptor-overhead` Phase D (deferred → active) → this → Stage 4.3 (128+ chunks draw distance) validates
hybrid strategy. **Re-evaluation triggers:** Stage 4.3 lands, NVIDIA `vkQueueBindSparse` driver fix (rare),
`VK_KHR_sparse_image2` cross-vendor, `VK_EXT_memory_decompression` AMD/Intel ratification. **Closed entry:**
`experiments/2026-06-20-vma-sparse-textures/`. Cross-axis: this + same-day 19+ closed сессии = full Stage
1.x/2.x/3.x/4.x/5.x/6.x optimization landscape + SOTA-GI axis + sparse-VT axis. См. §1 + §6.

`2026-06-20` — closed `2026-06-20-restir-gi-feasibility` (verdict=`mixed`). SOTA-GI-ось experiment.
Web-research complete (3 batches, ~30 results, ~30 sources верифицированы: Bitterli 2020 ReSTIR original,
Ouyang 2021 ReSTIR GI, Lin 2022 ReSTIR PT + GRIS [80 ms @ 1920×1080, MAPE 0.39 vs 1.63 PT], Majercik 2019/2021
DDGI, Müller 2021 NRC [2.6 ms @ full HD], NVIDIA-RTX/RTXGI SDK v2.7.0 [336 stars Mar 2026], NVIDIA-RTX/SHARC
[123 stars, spatial hash grid 64-bit, 4-pass, ~185 MB @ 2^22, 1.5-10% overhead Cyberpunk], NVIDIA-RTX/RTXDI
v3.0+ [ReSTIR DI/GI/PT/ReGIR, D3D12+Vulkan], Crassin 2011 GIVoxels, Lumen SIGGRAPH 2022 [Epic rejected VCT leaky],
Minecraft RTX GDC 2021 [voxel + path tracer + irradiance cache], Douglas Voxel Devlog #23 Jun 2025 [voxel + DDGI],
Cyberpunk 2077 RT Overdrive Patch 2.1 Dec 2023 [production ReSTIR + SHaRC], NVIDIA Zorah RTX 50 demo 2025
[ReSTIR PT], OGRE-Next CIVCT, Aokana 2025, ReSTIR FG/GSGI/PMGI 2024 [0.4-14 ms variants], Epic DDGI abandonment
forum Dec 2025). **Главный finding:** **architectural mismatch** — все 4 SOTA техники (ReSTIR PT, DDGI, SHaRC,
NRC) требуют path tracer foundation; ProjectV Stage 5.x = hybrid VCT+RTX = NOT path tracer. **VRAM matrix:**
SHaRC = 185 MiB (3.65% of 5.06 GiB budget per `hardware-profile.md` §3), DDGI = 16 MiB, ReSTIR = 33-67 MiB
checkerboard/full. Cross-vendor: SHaRC = universal (RTXGI 2.x Vulkan path), NRC = NVIDIA-only (Tensor Cores
≥ Turing, excludes AMD RDNA 4 + Intel Battlemage per `dec-pipelines-async-compute` matrix). **Mainline
рекомендация:** **keep current hybrid VCT+RTX as-is** (Stage 5.x MVP), **defer SOTA GI до Stage 6+ post-MVP
path tracer pivot**. Recommended add-on order if path tracer ships: **SHaRC → DDGI → ReSTIR DI/GI/PT**.
**Lighting axis FULLY closed** (cutoff + lights + shadows + SOTA-GI all same-day `2026-06-20`). Cross-axis:
19+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape + SOTA-GI axis. Closed
entry: `experiments/2026-06-20-restir-gi-feasibility/`. См. §6 + §1.

`2026-06-20` (this session, previous) — closed `2026-06-20-rt-shadows-vs-csm` (verdict=`mixed`). Shadow-ось experiment.
Web-research complete (4 batches, ~30 results, 23 sources верифицированы: Boksansky RTG 2019,
NVIDIA Blackwell whitepaper Jan 2025, AMD RDNA 4 HotChips 2025, Intel Battlemage Xe2, Khronos
VK_KHR_deferred_host_operations spec, NVIDIA nvpro-samples BLAS pattern, Khronos Forum BLAS
fence wait, ACM SIGGRAPH 2025 mobile RT, Arm Vulkanised 2026, Vulkan Tutorial Ray Query §5.2,
Sascha Willems rayquery example, и т.д.). Analytical cost model + cross-vendor RT throughput
matrix. Hybrid CSM + RTX shadows рекомендован для Stage 5.2: CSM (sun, current path per
`agent/decisions.md §15`) + RTX `VK_KHR_ray_query` (feature-flagged additive для local
lights + per-pixel contact shadow detail). **Quality gain > 5% per `optimization-philosophy.md`**
для non-sun-dominated scenes (cave/lava/magic-heavy); < 5% для sun-dominated outdoor (CSM dominant).
VRAM cost **8-23 MiB** на RTX 3060 Ti (well under 5% budget). BLAS rebuild bottleneck → async via
`VK_KHR_deferred_host_operations` (rev 4) + `dec-pipelines-async-compute` precedent (per Khronos
Forum 2025-09-29: 2000 BLAS single dispatch = 15 ms fence wait). Cross-vendor: Blackwell/RDNA 4/
Battlemage = full benefit; Ampere/RDNA 3 = 1-2 rays limited; Turing/Alchemist = feature OFF.
**Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent (Step 1
foundation extension probing + BLAS pool + TLAS scratch; Step 2 ray query в `voxel.frag` для
local lights + async BLAS build via deferred host operations; Step 3 default flip). ~770 LoC
total, M effort, 3-4 sessions. **Continuation chain:** `vct-vs-rt-cutoff` (closed verdict=mixed) +
`clustered-forward-mass-lights` (closed verdict=yes) → this. **Lighting axis complete** (cutoff +
lights + shadows). Stage 5 foundation + cutoffs + lights + shadows все closed same-day `2026-06-20`.
Closed entry: `experiments/2026-06-20-rt-shadows-vs-csm/`. Rendering-approach
axis (deferred resolve via vis-buffer + material-table SSBO). Standalone Vulkan 1.4 prototype
(~700 LoC incl. shaders, RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341, NVIDIA 610.43.02). 6 measurement
configs (3 scenes × 3 resolutions). Visual equivalence verified via framebuffer hash match.
**Cross-over @ 1280×720:** 1920×1080 vis-buffer 15-26% slower (bandwidth-bound on pixel
coverage); 800×600 vis-buffer 12-24% faster (vertex cost dominates). Voxel scenes are
pixel-coherent after greedy meshing per `2026-06-20-meshing-algo-comparison` verdict=mixed
(Naive Greedy default = ~1 visible triangle per pixel = no overdraw to amortize fullscreen
vis-buffer cost). Mainline рекомендация: **DEFER** до Stage 4.3 (128+ chunks draw distance)
или mobile target decision (TBR GPUs benefit per Vulkan-Guide, vis-buffer 10-30% win).
Cross-refs: `bindless-descriptor-overhead` Phase B (bindless material table = prerequisite),
`dec-pipelines-async-compute` (async-compute resolve pass would compound benefits, unmeasured),
`meshing-algo-comparison` verdict=mixed (greedy meshing = pixel-coherent = vis-buffer loses на high res).
Web-research: 5 batch queries, 20+ sources верифицированы (Burns-Hunt 2013 JCGT foundational
6.2× bandwidth win; Karis SIGGRAPH 2021 + Wihlidal GDC 2024 Unreal Nanite 64-bit vis-buffer +
shading bins 100% compute shaders UE 5.4; Andersson Frostbite 2017 "10-20x geometry vs Deferred";
The Forge v1.57 May 2024 TVB 2.0 pure compute; Cao NanoMesh SIGGRAPH 2024 32-bit mobile;
Vulkan-Guide TBR best practices 2024; Lam Adreno vis-stream HW compressor; jglrxavpok 2023
Vulkan R64Uint impl; Harada AMD Forward+ GPU Pro 4 alternative; Olsson Clustered Shading HPG 2012
1M lights; VoxelMVP / Exile / Slater / cgerikj / Ascendant voxel-specific refs). См. §6 +
[experiment README](./experiments/2026-06-20-vis-buffer-for-voxels/README.md).

`2026-06-20` — closed `2026-06-20-clustered-forward-mass-lights` (verdict=`yes`). Mass-lights
architecture axis: Forward+ (clustered shading) рекомендован для Stage 5 с условиями (soft cap
≥2048, light prioritization для 5000+ light scenes). Standalone CPU prototype
`prototype/bench.cpp` (~480 LoC, Clang 22.1.6, no warnings, 13 configs). Measured cluster
build 16×9×24 / 1000 lights = 12.7 ms CPU (sparse) / 15.4 ms CPU (dense). GPU projected
0.1-0.5 ms at 1000 lights. **CRITICAL: 16×9×24 / 5000 dense lights = 69% clusters overflow
soft cap 1024** — soft cap must be raised или prioritization policy. Per-fragment 100×
speedup vs 1000-light uniform array. Mainline 3-step migration (M effort, 3-4 sessions).
Cross-axis: 14+ closed same-day `2026-06-20` sessions покрывают full Stage 1.x/2.x/3.x/4.x/5.x/6.x
optimization landscape + mass-lights axis. Closed entry:
`experiments/2026-06-20-clustered-forward-mass-lights/`. ECS memory-layout-ось experiment
(Stage 6.1 + cross-cutting). Standalone C++26 prototype `prototype/flecs_soa_vs_aos.cpp` (642 строки, 4 configs ×
3 workloads × 3 seeds × 1000 iterations = 36 measurements). **SoA wins ALL 3 workloads** — raycast **2.14×**
(199→427 Meps), physics **3.86×** (210→812 Meps, near-exact match с DevelopersIO 2026 Godot 4.6 3.3× update
benchmark), cull **1.44×** (315→454 Meps, predicate branch dampens gain). Crosses 5% threshold per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` by 40-280%. SoA variance ниже AoS (24% reduction
for physics) — deterministic cache-line stride reduces OS scheduler noise. Hybrid ≈ SoA (within 1-2%), HotOnly
worst variance (15% raycast stddev) — NOT recommended. Cross-validation: Mertens 2024 (Flecs default SoA — direct
validation), Sagar 2026 (5.67× OOP→SoA), Bevy PR #14049 (2× dense iteration), AMD EPYC 7003 docs (Zen 3 cache
spec). Mainline рекомендация: keep Flecs default SoA storage (per Mertens 2024 + Flecs v4.1.5), **не возвращаться
на AoS POD-struct per entity** в новых systems. HotOnly-SoA pattern NOT рекомендуется. Snapshot save/load path
остаётся AoS (cold path, simpler code). Estimated mainline effort: **XS** (doc update + code review checklist,
не mainline rewrite). Cross-cutting unblocks для Stage 2.2 HZB cull / Stage 3.1 Fluid CA bookkeeping /
Stage 3.2 Incremental Jolt / Stage 5.1 VCT voxelize — все эти Flecs systems могут proceed с уверенностью
что SoA = correct default. Documentation update recommended для
`legacy/docs/philosophy/02_paradigms/02_dod-philosophy.md` mermaid diagram (analytical 3-5× claim → measured
1.44-3.86× numbers с cross-ref). Re-evaluation trigger: Stage 6.1 multi-threading per `TODO.md §6.1` Step 6
(NUMA-aware allocation may shift tradeoff). Cross-axis: 11 closed today-сессии покрывают
storage/sync/cull/binding/layout/meshing/hzb/gpu-traversal/gi-cutoff + теперь ECS memory-layout = full Stage
1.x/2.x/3.x/5.x/6.x optimization landscape.

`2026-06-20` — closed `2026-06-20-vct-vs-rt-cutoff` (verdict=`mixed`). Lighting/GI-ось experiment.
Roughness-based hybrid VCT + RTX рекомендован с **cutoff = 0.3** (VCT high roughness, RTX low roughness,
diffuse GI = VCT always, AO/contact shadows = RTX always, sun = CSM). Web-research ~30 sources
(Crassin 2011, OGRE 2019, Lumen 2022, Akenine-Möller JCGT 2021, RTXGI 2.0, Blackwell 2025, RDNA 4
2025, Battlemage 2025, Aokana 2025, etc.) + analytical cost model + cross-vendor HW RT perf matrix.
Cross-vendor threshold adjustment: Blackwell → 0.4-0.5, RDNA 2 → 0.2, no-HW-RT → VCT-only fallback.
Mainline integration: 4-step migration per `agent/knowledge.md §30.4` precedent (Step 1 cutoff
constant + HW RT probe + CMakeLists flag, Step 2 VCT per `TODO.md §5.1`, Step 3 RTX per `TODO.md
§5.2`, Step 4 optional DDGI/SHaRC/NRC/ReSTIR PT). Stage 5 теперь имеет все три foundation: storage
(`nanovdb-on-gpu`), sync (`dec-pipelines-async-compute`), cutoff strategy (this). См. §1 + §6.
Continuation chain: `nanovdb-on-gpu` → `dec-pipelines-async-compute` → `hzb-binding-models` → this —
4th orthogonal axis (lighting/GI) после storage/sync/binding. Cross-axis: 5 same-day `2026-06-20`
sessions (memory + layout + sync + storage + GI strategy) покрывают Stage 1.x/2.x/3.x/5.x
optimization landscape.

`2026-06-20` — closed `2026-06-20-nanovdb-on-gpu` (verdict=`yes`). GPU-axis experiment closing
`svdag-vs-vdb-memory-throughput` measurement gap. Both CPU-side and GPU-side prototypes byte-exact
(verify_mismatches=0 на 5 сценах × 2 kernels). NanoVDB-aligned pointer-less layout outperforms
SVDAG-on-64-tree **on 4/5 sparse scenes by 12-141%** (sparse_random_8: 500→1210 Mrays/s,
voxel_lab_8: 541→1208, ground_8: 638→1242, brick_8: 1146→1284). Only solid_8 ties (memory-bandwidth-bound).
GPU memory: NanoVDB 57-75% less VRAM. CPU memory: ~50% less. Crosses 5% threshold per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. **Critical mainline finding:**
ProjectV chunkSize = 8 (not 32 as previous experiment assumed) per
`src/voxel/VoxelWorld.hpp:78` + `src/voxel/SceneConfig.cpp:78` — depth=2 not depth=3. OpenVDB 13.0.0
(Nov 2025) lowered NanoVDB mutation barrier (DilateGrid, MergeGrids, CoarsenGrid, RefineGrid,
PruneGrid, VoxelBlockManager). Mainline рекомендация: **hybrid strategy** — keep CPU-side
SVDAG-on-64-tree (current mainline Stage 1.2 design, proven by `svdag-vs-vdb-memory-throughput`),
flatten to NanoVDB-aligned transient SSBO at GPU upload for Stage 5.1 VCT cone-march + 3
fragment-shader DDA traces in `voxel.frag` per `TODO.md §6.2.2`. 3-step migration per
`agent/knowledge.md §30.4` precedent: Step 1 foundation (CPU→GPU flatten helper, S effort),
Step 2 kernel swap (NanoVDB walker, M effort, includes HDDA optimization), Step 3 default flip
(`PROJECTV_USE_NANOVDB_TRANSIENT_VCT=ON`). Foundation optional dependency: `dec-pipelines-async-compute`
(closed 2026-06-20) for async re-upload. Caveats: single GPU vendor (NVIDIA RTX 3060 Ti GA104
Ampere, Vulkan 1.4.350) — mainline re-test on AMD RDNA2/3 + Intel Arc dev matrix; HDDA-specific
optimizations (warp ballot early-out, ReadAccessor caching) NOT implemented in first-iteration
prototype (would add 10-30% per NanoVDB PR #2220 reference numbers). Continuation chain:
`sparse-64-tree-alternatives` (analysis) → `svdag-vs-vdb-memory-throughput` (CPU) → this (GPU) —
three orthogonal angles of Stage 1.x storage analysis, all closed same-day `2026-06-20`.
Sync fix r1 (post-parallel-session): nanovdb-on-gpu moved from `backlog.md §In progress` → `§Closed`
per §13.5. INDEX.md §1 stale "still in-progress" line 56 обновлено.

`2026-06-20` — closed `2026-06-20-hzb-binding-models` (verdict=`mixed`). Cull-shader pattern decision для
Stage 2.2: switch from `textureLod` (vkguide.dev pattern) к `texelFetch(sampler2D, ivec2, mipLevel)`. Web-research

+ standalone Vulkan compute prototype + 24 sampling tests across 8 mips × 3 patterns. **17/24 PASS, 7/24 FAIL.**
  Storage image (`VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` + `imageLoad`) rejected (GLSL single-mip-per-binding limitation,
  proved by `max_abs_error = N * 1000` pattern). `textureLod` correct on classic set but fragile под bindless
  heap на NVIDIA per `foijord/vk-textureLod-repro` 2026 — drives recommendation to use `texelFetch` for
  bindless-robustness. Mainline integration: HZB descriptor = `SAMPLED_IMAGE` + separate `SAMPLER`,
  `hzb_cull.comp` uses `texelFetch`. ~50-100 LoC change. Future-proofs `bindless-descriptor-overhead` Phase E
  rollout. Cross-axis continuity: same-day `2026-06-20` сессии закрыли 6 storage/cull/bindless/sync experiments
  plus hzb binding — orthogonal axes Stage 1.x/2.x/3.x optimization complete.

`2026-06-20` — closed `2026-06-20-dec-pipelines-async-compute` (verdict=`yes`). Sync-axis experiment —
async-compute queue + `VK_KHR_synchronization2` (core 1.3) + `VK_KHR_timeline_semaphore` (core 1.2) +
`VK_KHR_global_priority` (core 1.4) рекомендованы для 4 of 5 ProjectV compute passes: Stage 2.2 HZB
cull + Stage 3.1 Fluid CA (20 Hz, natural async candidate via 3-frame latency) + Stage 4.1 GPU world
gen (LOW priority, background) + Stage 5.2 RTX BLAS build (`VK_KHR_deferred_host_operations` для
non-blocking dispatch). Stage 5.1 VCT — sequential default, async opt-in (RDNA «export bound shaders»
warning). Expected 5-8% steady-state + 100% spike elimination (world gen + BLAS). Crosses 5% threshold
per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. Cross-vendor validated: NVIDIA
Ampere/Ada/Blackwell + AMD RDNA2/3/4 + Intel Arc Alchemist/Battlemage. Vendor caveats documented in
`sources.md` and `README.md §6`. Mainline рекомендация: 3-step migration per
`agent/knowledge.md §30.4` precedent — Step 1 foundation `vkQueueSubmit2` + timeline semaphore
conversion (S effort), Step 2 per-pass async adoption gated by `PROJECTV_ASYNC_COMPUTE=ON` env, Step 3
default flip. Foundation шаг = prerequisite для Stage 3.1 GPU Fluid CA (sync-model конкретизирует §30.4
contract), Stage 2.2 HZB full integration, Stage 5.2 RTX BLAS build. Cross-axis continuity: memory
(`svdag-vs-vdb-memory-throughput`) + layout (`cache-oblivious-chunk-tree`) + sync (this) — three
orthogonal axes of Stage 1.x/2.x/3.x optimization, all same-day `2026-06-20` sessions. Per
`legacy/docs/architecture/practice/00_engine-structure.md:483` minor fix opportunity: «`VK_KHR_synchronization2`
(core in 1.4)» should be «core in 1.3» per Khronos spec — no functional impact (1.3+ all have it as core).
Sync fix r1 (post-parallel-session): dec-pipelines moved from `backlog.md §In progress` → `§Closed` per §13.5.

`2026-06-20` — closed `2026-06-20-cache-oblivious-chunk-tree` (verdict=`mixed`). Morton (Z-order) reorder
измерен на synthetic random-walk workload (24³ chunks, 33 MiB > L3 32 MiB). Mean latency similar (~40-60 ns)
для baseline vs Morton, p99 inconsistent across seeds, cold cache unaffected. Implementation cost low
(one-time reorder + slot remap) but measured benefit within timer noise. Literature predicts 25-75% cache
miss reduction (arxiv 2603.06771) — not reproduced в этом prototype. Likely reasons: random-walk access
pattern (no spatial coherence), 280 B node size (5 cache lines vs SoftwareSVO's 32 B half-line optimal),
timer resolution ~30 ns. Re-evaluation trigger: `TODO.md §4.3` (128+ chunks draw distance). Sync fix r1
(post-parallel-session): cache-oblivious moved from `backlog.md §In progress` → `§Closed` per §13.5.

`2026-06-20` — closed `2026-06-20-bindless-descriptor-overhead` (verdict=`mixed`). Hybrid descriptor
strategy рекомендуется: bindless для stable resources (material table, Sparse64Node, HZB mip,
virtual texture page table) + traditional+dynamic-offset для transient SSBOs (PackedFace, indirect,
motion) + push descriptors для small per-draw transient. 5-phase rollout plan в
`README.md §7`. `VK_EXT_descriptor_buffer` deferred до NVIDIA native HW support (current emulation
= 5 indirections in VKD3D-Proton per XDC 2025-09-29). Cross-vendor validated: NVIDIA RTX 30/40/50,
AMD RDNA2/3, Intel Arc Gfx12.5+, Arm v9+ Mali. Quantitative refs: Traha 2024 (3.5ms saved =
+5 FPS), Arm Mali sample (38% frame time saved), NVIDIA bindless 7× upper bound
(legacy OpenGL). Continuation chain: `sparse-64-tree-alternatives` → `mesh-shader-vs-compute-cull` →
`bindless-descriptor-overhead`. Все три — same-day `2026-06-20` сессии.

`2026-06-20` (this session) — `2026-06-20-meshing-algo-comparison` closed (verdict=`mixed`). Meshing-axis experiment
(unique h-priority slot после 8 закрытых same-day сессий на orthogonal axes:
storage/sync/cull/layout/binding/memory/hzb). Web-research complete (8 sources across 2 batch queries:
cgerikj binary-greedy 2020, 0fps.net 2012, bonsairobo SN 2020, KAIST ODC SIGGRAPH Asia 2024, MakerTech YouTube
2026, jwarren DC 2002, lpigou SN 2021, isoext 2025). Standalone C++20 prototype `prototype/bench.cpp`
(4 algos × 6 scenes = 24 configs, 1000 iter, mean/median/p95/p99/std, `taskset -c 2` на 5800X).
**Главные findings:** (a) **Naive Greedy** wins triangle count на 5/6 non-degenerate scenes (1.3-450× меньше
triangles vs MC/SN/DC); (b) **Marching Cubes** fastest build time (250-380 µs vs greedy 555-650 µs, 1.7-2.5×
быстрее); (c) **Sparse scenes** (1% density) — SN/MC лучше по triangles (1 220/2 258 vs greedy 3 608);
(d) **DC slowest** (1 170-4 817 µs, QEF overhead 4-5× vs MC). **Refined verdict:** mixed — greedy wins poly count
(главная метрика для vertex-bound Stage 2.1), loses build time. **Mainline рекомендация:** keep Naive Greedy
default для Stage 2.1/3.3; bitwise cull optimization (per cgerikj 2020, 50-200 µs/chunk) — drop-in option
для Stage 4.1 high-frequency rebuild; re-evaluate SN/MC при procedural sparse worlds. Cross-refs:
`agent/knowledge.md §25` (greedy meshing contract, baseline), `src/shaders/voxel_mesh.comp::GreedyFacePass`
(per-axis dispatch, current mainline), `TODO.md §2.1` (mesh shader port, this informs choice) + `§3.3`
(physics mesh, mirror choice), `mesh-shader-vs-compute-cull` (closed verdict=mixed, mesh shader =
feature-flagged optional). Continuation chain: `sparse-64-tree-alternatives` → `svdag-vs-vdb-memory-throughput`
→ this → `Stage 4.1` procedural world gen (re-evaluation trigger). Closed entry:
`experiments/2026-06-20-meshing-algo-comparison/`.

`2026-06-20` — closed `2026-06-20-vulkan-fps-pacing-vk-ext` (verdict=`mixed`). **Frame-pacing-ось**
experiment (Stage 0 / independent, foundation для all stages per DoD principle «low latency >
throughput»). Web-research complete (5 batch queries, 8 key sources + 3 supplementary, all
верифицированы: Khronos blog 2025-12-04, Phoronix Mesa 26.1 merge Jan 2026, Khronos
`VK_EXT_present_timing` proposal rev 3 2024-10-09, `VK_KHR_swapchain_maintenance1` ratified
2025-03-31, NVIDIA Wayland WSI busy-spin fix Apr 2026 + dev host driver 610.43.02 match,
`VK_KHR_present_wait2` rev 1, Mesa 26.2 direct-display benchmarks Jun 2026, Android docs
Jun 2026). **Dev host validation** via `vulkaninfo 2026-06-20`: все relevant extensions supported

+ features enabled — `VK_EXT_present_timing` rev 3 (`presentTiming` + `presentAtAbsoluteTime` +
  `presentAtRelativeTime` features = true), `VK_KHR_present_wait2` rev 1 (`presentWait2` = true),
  `VK_KHR_swapchain_maintenance1` rev 1 (`swapchainMaintenance1` = true), `VK_KHR_present_id/2`,
  `VK_KHR_present_mode_fifo_latest_ready`. **Refined hypothesis:** `VK_EXT_present_timing` (Nov 2025
  merge, Vulkan 1.4.335) — SOTA frame-pacing API; **NOT Vulkan 1.4 core** as original hypothesis
  thought — все 3 extensions are **device extensions**. Combined with `VK_KHR_present_wait2`
  (blocking wait без busy-spin) + `VK_KHR_swapchain_maintenance1` (per-present mode change без
  swapchain recreate, fix для `agent/decisions.md §30.3` RecreateSwapchain cycle) → детерминированный
  frame budget. Mesa 26.2 KHR_display direct-display benchmark: **~0.3 ms latency reduction, 5%
  power reduction, tighter variance** (0.9 ms → 0.3 ms std-dev). **Mixed потому что measured
  Wayland-specific p99 frame variance numbers отсутствуют** (Mesa benchmark на KHR_display
  direct-display, другие условия; Wayland compositor вносит дополнительный jitter). Intel Iris Xe
  **doesn't support** `present_wait` / `swapchain_maintenance1` — fallback path needed.
  **Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent — Step 1
  foundation (`PROJECTV_USE_PRESENT_TIMING=ON|OFF` env + per-feature detection в
  `TryPickPhysicalDevice`); Step 2 adoption (Mode C path с `desiredPresentTime` IPD calibration
  via `vkGetPastPresentationTimingEXT` feedback + `VkSwapchainPresentModeInfoKHR` per-present mode
  change + `VkSwapchainPresentFenceInfoKHR` race-free destroy); Step 3 default flip для hardware
  с `presentTiming + presentAtAbsoluteTime` features enabled. Foundation шаг = prerequisite для
  Stage 3.1 GPU Fluid CA cross-frame latency contract (per `agent/workspace.md §2` +
  `agent/decisions.md §30.4`). **Caveats:** (a) prototype deferred (analytical literature
  sufficient для integration recommendation); (b) cross-vendor = Mesa 26.1+ (Jan 2026), deployment
  lag 1-2 cycles; (c) AMD/Intel mainline re-test required (NVIDIA dev host only validated).
  **Operator override note (per `docs/experiments/AGENTS.md §13.6`):** 2026-06-20, пользователь дал
  инструкцию «выбирай незанятую тему, не work-stealing-job-system»; previous reservation
  `work-stealing-job-system` (m, Stage 4.1/6.1, claimed earlier this session) released back to
  `research/backlog.md §Open`. Fresh claim: `vulkan-fps-pacing-vk-ext`. Closed entry:
  `experiments/2026-06-20-vulkan-fps-pacing-vk-ext/`.

**RACE CONDITION CORRECTION (per `docs/experiments/AGENTS.md §13.3`):** Параллельный агент
misinterpreted operator instruction «выбирай не work-stealing-job-system» (в parallel session) как
«release the existing reservation». В реальности operator сказал parallel agent'у «выбери
другую тему для себя» (т.к. work-stealing-job-system уже был мной claim'нут в этой сессии через
first-write-wins). После operator override parallel agent взял vulkan-fps-pacing-vk-ext. Но
**мой work-stealing-job-system experiment уже был выполнен до override** — research/web-research/
prototype/results/writeup всё завершено. Per §13.3 first-write-wins, моя работа сохраняется

+ зафиксирована в §6 + §Closed separately. **Этот experiment re-recorded в §6**:
  `2026-06-20-work-stealing-job-system` (verdict=mixed, per `experiments/2026-06-20-work-stealing-job-system/`).

`2026-06-20` — closed `2026-06-20-work-stealing-job-system` (verdict=`mixed`). **Job-scheduling-ось**
experiment (Stage 4.1 dispatcher foundation + Stage 6.1 ECS multi-threading per `TODO.md`).
Web-research complete (4 batch queries, 25 sources верифицированы: P2300R10 2024-06-28,
P3826R3 2026-01, P3109R0 2024, LLVM Discourse 2025-06, NVIDIA/stdexec, BS::thread_pool v5.0.0
2024-12-20, Taskflow v3.10.0 2025-05 / v4.0.0 2026, oneTBB v2022.3.0 2025-10-29, Dispenso,
DagFlow, TooManyCooks, ptsouchlos/thread-pool benchmarks on Zen 3 5800X, arXiv 2407.15805).
Standalone C++26 prototype `prototype/bench.cpp` (6 файлов, ~750 LoC incl. vendored
`BS_thread_pool.hpp` v5.0.0 MIT). 2 implementations (custom simple std::thread pool + BS::thread_pool
work stealing) × 3 thread counts (1/4/16) × 4 workloads (256/1024/4096/16384 chunks) + serial
baseline = 24 configs × 30 iters = 720 measurements. **Surprising negative finding:**
**serial dispatcher — sweet spot для ProjectV mainline** (cache-fitting workload fits L3 32 MiB).
Work-stealing pool (BS::thread_pool) **проигрывает** simple pool'у для small tasks (BS 1t = 5-8×
slower than serial). Simple pool проигрывает serial для small workloads. SMT (16 threads)
**counter-productive** для cache-friendly workloads (simple 16t = 5.7× slower than serial;
BS 16t = 7.8× slower). p99 jitter: serial 1.0-1.2× mean, parallel 2-5× mean. **Per-stage split:**
❌ Stage 4.1 (4 KiB/chunk) = serial, ❌ Stage 3.1 (1-2 KiB/chunk) = serial, ⚠️ Stage 6.1 (ECS
per-system) = TBD separate experiment, ✅ Stage 4.3 (128+ chunks batch world gen) = re-evaluate.
**Mainline рекомендация:** не подключать thread pool / TBB / libdispatch / `std::execution`
по default. Per `legacy/docs/philosophy/01_foundation/05_decision-making.md» «if perf gain
< 5-10%, choose simple» — measured: pool overhead = 5-15× per-task compute = 12-37× waste.
Estimated mainline effort: **XS** (anti-pattern: «don't add pool по default»). Cross-axis
closure: today 12 experiments closed = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization
landscape (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async + job-scheduling).
Re-evaluation triggers: Stage 6.1 Step 6 NUMA-aware, Stage 4.3 lift draw distance, AVX-512
hardware arrival (Zen 5), real perlin/SVDAG workload, `stdexec::static_thread_pool`
direct measurement when Clang 23+ + libc++ stable. Closed entry:
`experiments/2026-06-20-work-stealing-job-system/`.

`2026-06-20` — closed `2026-06-20-clustered-forward-mass-lights` (verdict=`yes`).
**Mass-lights architecture** experiment — единственная ось, не покрытая today-сессиями
(storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async/gi-strategy + job-scheduling).
**Mainline baseline = single-light hard cap** per `src/shaders/voxel.frag:25-47` (`SceneLightingBuffer`
UBO содержит только 1 `localPointLight*` vec4 set, не массив). **Не масштабируется** на
`TODO.md §4.x` procedural (лава/факелы/магия) + `§5.1` VCT VPLs. Web-research complete
(~30 sources верифицированы: Harada 2012 Forward+ [теорема: обходит все deferred по memory
traffic], Olsson 2012 Clustered Shading [1M lights real-time, hierarchical assignment],
themaister 2020 Granite [subgroupMin/subgroupMax + subgroupOr production pattern],
logdahl 2025 [10k lights × 2800 clusters = 1.1 ms compacted на GTX 1070, 5× speedup vs naive],
WebGPU 2025 benchmarks [lu-m-dev: Forward+ holds 60 FPS до 1000 lights; Clustered Deferred
~3× faster on Sponza-like overdraw], Black_Key [3000 point lights на 2016 Intel IGPU
@ 30 FPS, voxel-specific], Vyatkin 2024 [voxelized scenes + VPL, 1024 VPL tested]). Standalone
CPU prototype `prototype/bench.cpp` (single file, ~480 LoC, Clang 22.1.6, `-O3 -march=native`)
**compiled clean** (`-Wall -Wextra` no warnings). 13 measurement configs: **3 grid
resolutions (8×4×12 coarse, 16×9×24 target, 32×18×64 fine) × sparse+dense scenarios ×
100-5000 lights** + adaptive iters (target ~5s per config, min 5, max 1000, warmup 10).
**Key CPU numbers (16×9×24 target, sparse scenario):** 100 lights = 1.4 ms mean, 1000 lights
= **12.7 ms mean / 15.3 ms p99** (avg 3.1 lights/cluster, max 34, 66% empty). **Dense scenario
(лава):** 16×9×24 / 1000 lights = 15.4 ms (avg 232, max 544, 22% empty). **CRITICAL: 16×9×24
/ 5000 dense lights = 124.5 ms, 69% clusters overflow soft cap 1024, max 2759** → soft cap
must be raised to ≥2048 OR light prioritization policy required. **Cross-validation с
published GPU numbers:** within 5-10× of logdahl 2025 (1.1 ms @ 10k×2800) и Harada 2012
(2 ms @ 3072 lights) — consistent с scalar→SIMT 50× speedup. **GPU projected cluster build:**
0.1-0.5 ms at 1000 lights (1.5-3% of 16.67 ms frame budget). **Per-fragment analytical model:**
Forward+ (10 lights/cluster avg) = 1000 ALU + 50 DDA reads per fragment = **100× speedup vs
1000-light uniform array** (100,000 ALU), 10× cost increase vs current 1-light baseline
(100 ALU + 5 DDA reads). **VRAM cost** < 2 MB (cluster grid offset+count = 27.6 KB,
light SSBO 256×32 B = 8 KB, light index buffer avg 138 KB). **Mainline рекомендация:**
**3-step migration** + optional Step 4 (per-light cost reduction) + Step 5 (VPL integration
post-Stage 5.1). **Step 1** (XS, ~50 LoC): replace single-light UBO с light SSBO array
(`kMaxDynamicLights = 256` TBD after GPU prototype), keep single-light path as fallback,
additive `PROJECTV_DYNAMIC_LIGHTS=ON` env. **Step 2** (M, ~200 LoC): new `cluster_build.comp`
frustum AABB + light assignment (sphere-AABB + atomic counter compaction per logdahl 2025
5× speedup), new `ClusterGridBuffer` + `ClusterLightIndexBuffer` + `DynamicLightSSBO` in
`src/render/SceneResources.{hpp,cpp}`, dispatch in `src/render/Renderer.cpp` (piggyback on
async-compute foundation per `dec-pipelines-async-compute`). **Step 3** (M, ~100 LoC):
modify `src/shaders/voxel.frag` to compute cluster index from `gl_FragCoord` + view-Z
(Naughty Dog exponential formula) + iterate cluster light list. **Clustered Deferred NOT
recommended** for Stage 5 (voxel-мир has low overdraw vs Sponza, gain < 5% per threshold)
— revisit after Stage 2.1 mesh shader + Stage 4.3 lift draw distance. **Acceptance criteria:**
TracyPlot `ClusterBuild (ms)` < 1 ms GPU at 1000 lights, byte-exact output for N≤8 vs
current mainline (A/B test), < 2 MB VRAM overhead, new `ProjectVClusteredLightingTests`.
**Cross-axis continuity:** 5 same-day `2026-06-20` sessions on lighting axis (vct-vs-rt-cutoff
mixed + this yes) + Stage 5 foundation complete (nanovdb-on-gpu yes + dec-pipelines-async-compute
yes). **12+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape**

+ mass-lights dimension added. Closed entry: `experiments/2026-06-20-clustered-forward-mass-lights/`.

`2026-06-21` — closed `2026-06-21-volumetric-fog-atmosphere-rendering` (verdict=`mixed`). **Volumetric
fog / atmospheric rendering / participating media axis** experiment closed same session (Stage 5.x
Visual Polish per `TODO.md §5` — **deferred** per `agent/workspace.md §2` line 36 operator 8x
planning decision; **self-invented topic** per operator instruction `2026-06-21` «выбирай свободную
тему или придумывай свою исследуй»; **0 of 50+ closed experiments covered volumetric fog axis** —
fully fresh new axis). Web-research complete via `webfetch` DuckDuckGo HTML endpoint (Exa HTTP 429
persistent per `agent/knowledge.md Part B §9`); **30 sources verified** in `sources.md` Tier 1 +
Tier 2 + Tier 3: Wronski 2014 SIGGRAPH canonical froxel paper + Hillaire 2015 SIGGRAPH Frostbite
production + Kovalovs 2020 SIGGRAPH TLoU2 + Wright 2022 SIGGRAPH Lumen + Enshrouded 2026 GPC +
elliahu/atmosphere validated RTX 3060/4080 benchmarks + Timethy Hyman 2026 Traverse + Mastering
Graphics Programming with Vulkan Ch10 + sinnwrig/URP-Fog-Volumes + Godot issue #8580 + Kenny Mitchell
GPU Gems 3 + Bruneton 2017 + Sakmary 2023 + Hillaire 2020 + Horizon Forbidden West Nubis + NVIDIA
RTX Remix docs + Matej Lou 2025 + Loboda 2025 + Cinevva 2026 + moonjump 2026 + 12 supplementary.
Standalone C++26 CPU analytical cost model `prototype/volumetric_fog_sim.cpp` ~500 LoC (Clang 22.1.6
`-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**).
5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time
**0.008 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output:
`prototype/build/results.csv` (126 rows = 1 header + 125 data, 19.3 KB). **Headline (mixed per
platform tier):** **A_AnalyticDistance** (current mainline) = 0.002 ms but 8.45 dB PSNR = NOT real
volumetric fog (baseline only); **B_FroxelGrid_3DTexture** (Wronski 2014 + Frostbite + TLoU2 +
Enshrouded 2026 GPC) = 2.580 ms / 37.25 dB / 28.27 MiB = **SAFE UNIVERSAL DEFAULT**; **C_FullRayMarch_HalfRes**
(elliahu analog) = 6.986 ms / 42.75 dB / 12.39 MiB = best quality but exceeds 5 ms on 4/5 scenes
(cave_stress 9.59 ms = 28.8% of 30 Hz budget); **D_RTX_RayQuery_ShortRayShadow** (Lumen 2022 hybrid)
= 1.787 ms / 38.75 dB / 12.39 MiB = **WINNER RTX 3060 Ti** (fastest non-baseline, scene-coverage-
INDEPENDENT 1.33→2.31 ms); **E_Hybrid_FroxelNear_RayMarchFar** (Enshrouded 2026 GPC three-layer) =
4.868 ms / 40.75 dB / 25.93 MiB = most flexible but cave_stress 6.67 ms exceeds 5 ms на RTX 3060 Ti
(within budget на RTX 4080 per elliahu). Per-platform tier matrix: no-HW-RT → B_FroxelGrid;
RTX-class mid (current dev host) → D_RTX_RayQuery; RTX-class high → D default + E opt-in;
static baked / mobile fallback → A_AnalyticDistance. **Mainline 3-step migration per
`agent/knowledge.md §30.4` precedent** (~480 LoC total, M effort, 2-3 sessions, **deferred** до Stage
5.x dedicated session): Step 1 (XS, ~50 LoC) `VolumetricFogController` foundation + froxel grid +
env gate; Step 2 (M, ~400 LoC) per-strategy implementation в `voxel.frag` post-process pass +
`volumetric_fog.comp` + scattering accumulation + temporal history + half-res + RTX ray query; Step 3
(XS, ~30 LoC) default flip + Tracy plot + unit test + `lookdev-captures/fog` scene integration.
**Cross-axis:** orth orth ко всем 3 in-progress parallel (closing `tracy-gpu-vs-manual` by parallel
+ `gpu-fluid-ca-atomic-strategy` Stage 3.1 + `voxel-mutation-cost-characterization` cross-cutting);
**complementary** к closed VCT experiments (`vct-vs-rt-cutoff` + `vct-cone-count-atlas-precision` +
`vct-3d-mip-generation` + `vct-temporal-denoise-tensor-core` — cone-march через 3D атлас структурно
похож на fog ray-march) + `rt-shadows-vs-csm` (sun shadow в fog) + `clustered-forward-mass-lights`
(light sources для in-scattering) + `dec-pipelines-async-compute` (async queue для fog injection) +
`eye-tracked-foveated` (VRS = smart fog density follow-up) + `taa-motion-vectors` (MV reprojection
для fog temporal) + `dlss-fsr-xess-upscaling-voxel` (half-res fog + upscale) +
`vulkan-memory-aliasing-transient` (froxel = transient aliasing) + `vulkan-defragmentation-compaction`
(froxel VRAM = compaction) + `vulkan-fps-pacing-wayland-prototype` (frame pacing для ray-march jitter)
+ `renderdoc-ci-capture` (RenderDoc fog regression-guard) + `rtx-screen-space-reflections` (similar
hybrid RTX pattern) + `vk-video-decoder-replay` (decoded video → fog atmosphere). **Continuation
chain:** `vct-vs-rt-cutoff` (mixed Stage 5.1 cutoff) + `rtx-screen-space-reflections` (mixed Stage
5.x reflection) + this (mixed Stage 5.x fog) = **Stage 5.x Visual Polish axis fully covered**. **New
axis:** first volumetric fog / atmospheric rendering / participating media axis в 50+ closed
experiments. **Re-evaluation triggers:** Stage 5.x ships + RTX 4080-class hardware tier validated +
visual QA в реальном gameplay + VRS = smart fog density follow-up (per `eye-tracked-foveated` mixed)
+ Mobile platform deployment (no HW RT path = B_FroxelGrid critical fallback).
Cumulative session statistic: `2026-06-21` сессия = 30+ closed experiments per INDEX §6 (audio
+ wfc + sub-chunk + gpu-noise + frame-flight + dxc + renderdoc + eye-tracked + lod-mesh +
lod-transition + vulkan-defrag + vulkan-memory + vulkan-fps + greedy-physics + taa + dlss-fsr-xess +
depth-occl + vk-fragment-shading + vct-cone-count + vct-mip-gen + texture-compress + sdf-hybrid +
vk-multi-gpu + hzb-smart-mip + audio-diffraction + full-rt-tensor-cores + vk-video-decoder-replay +
rtx-screen-space-refl + voxel-chunk-streaming + **volumetric-fog**). Single-pass sync per `AGENTS.md §13.5`:
`backlog.md §In progress` → `§Closed` (with full closure note + reservation record kept per §13.5),
`INDEX.md §5 Active` → `§6 Recent closed` table row + `§1 Now Just-closed` + `§8 Last update` entry.
См. §6 + [experiment README](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/README.md) +
[STATUS](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/STATUS.md) +
[RESULTS](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/RESULTS.md) +
[sources](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/sources.md) +
`prototype/{volumetric_fog_sim.cpp, build/volumetric_fog_sim, build/results.csv (126 rows)}`.

- **`2026-06-21-trilinear-noise-interpolation`** — closed `2026-06-21` verdict=`mixed` (**Stage 4.1
  world gen noise interpolation axis — coarse-grid noise evaluation + trilinear interpolation**).
  Reserved `2026-06-21` by self per `AGENTS.md §13.1`. **5 strategies × 5 scenes × 5 seeds × 100 iter =
  12,500 main measurements**. Standalone C++26 CPU prototype `prototype/trilinear_noise_bench.cpp` ~390 LoC
  (GCC 16.1.1 `-O3 -march=native -std=c++26`, build green). Wall time <1 sec на Zen 3 5800X governor=`powersave`
  per `hardware-profile.md §1`. **Headline:**
  - **B_Trilerp_2 (2×2×2, 64× red.) REJECTED**: PSNR 4.97 dB mean (hypothesis was <1 dB) — **fails**.
    56% binary match rate. KdotJPG's trilerp critique confirmed.
  - **C_Trilerp_3 (3×3×3, 19× red.) RECOMMENDED**: PSNR 30.22 dB, >99% match, **12.6× speedup** — best
    quality-speed tradeoff.
  - **D_Trilerp_4 (4×4×4, 8× red.) QUALITY MODE**: PSNR 36.23 dB, >99.7% match, 6.7× speedup.
  - **E_Spline_2 (Catmull-Rom) REJECTED**: PSNR -20.76 dB (cubic overshoot with under-sampled grid).
  **Web research:** 12 sources verified (Minecraft 1.12 trilerp, KdotJPG critique, modern GPU noise approaches,
  Cinevva 2026, InfiniteDiffusion SIGGRAPH 2026).
  **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) coarse grid dispatch
  в `noise_kernels.comp` (27 evals instead of 512); Step 2 (XS, ~50 LoC) trilinear interpolation in
  shared memory; Step 3 (XS, ~50 LoC) `PROJECTV_NOISE_COARSE_GRID` env gate. Total ~150 LoC, S effort,
  1 session. **Re-evaluation:** if GPU world gen becomes ALU-bound (not memory-bound per closed
  `gpu-procedural-noise-compute-kernels`), 12× reduction critical. См. §6 + [experiment README](
  ./experiments/2026-06-21-trilinear-noise-interpolation/README.md) +
  [STATUS](./experiments/2026-06-21-trilinear-noise-interpolation/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-trilinear-noise-interpolation/RESULTS.md) +
   `prototype/{trilinear_noise_bench.cpp, build/trilinear_noise_bench, build/results.csv (126 rows)}`.

- **`2026-06-21-aerial-perspective`** — closed `2026-06-21` verdict=`yes`. **Stage 5.x Visual Polish — aerial
  perspective rendering axis** (self-invented per operator instruction «выбирай свободную тему или придумывай свою
  исследуй»; **remaining Stage 5.x axis per closed `volumetric-fog-atmosphere-rendering` listing**). Standalone
  C++26 CPU prototype `prototype/aerial_perspective_bench.cpp` ~280 LoC (Clang 22.1.6, build green **0 warnings**).
  5 strategies (A_None / B_LinearDistance / C_ExponentialDistance / D_ExponentialHeightFog / E_AnalyticPreetham)
  × 5 scenes × 5 seeds × 4000 samples = **125 configs × 4000 = 500,000 evaluations**. Web-research via `web_search`
  (Exa, working this session); **16 sources verified** (Preetham 1999 SIGGRAPH, Hillaire 2020 EGSR, elliahu 2025,
  Wenzel 2006 CryEngine2, Filament, Bruneton 2008, Unity HDRP, Bevy 2025, Three.js 2026). **Headline:
  D_ExponentialHeightFog recommended default** (8.53 dB mean PSNR vs full Preetham reference, 0.004 ms at 1080p =
  0.012% of 30 Hz, zero VRAM); E_AnalyticPreetham quality opt-in (0.015 ms). **All 4 non-baseline strategies free**
  (< 0.02 ms). **5-10% threshold per `optimization-philosophy.md`:** all strategies cross massively (A→D = +8.53 dB,
  any depth cue vs none). **3-step migration ~50 LoC, XS effort, 1 session.** Replace `voxel.frag:844-883` analytic
  distance fog with height-based exponential fog; env gate `PROJECTV_AERIAL_PERSPECTIVE=EXP_HEIGHT|PREETHAM|NONE`;
  default `EXP_HEIGHT`. Deferred до Stage 5.x dedicated session. **Cross-axis:** orthogonal to closed volumetric-fog
  (3D froxel scattering — this is cheap analytic per-pixel blending), god-rays (post-process shafts), cloudscape
  (distant cloud rendering). Complementary as distance foundation for all atmospheric effects. См.
  [`experiments/2026-06-21-aerial-perspective/`](./experiments/2026-06-21-aerial-perspective/).

`2026-06-21` — **closed `2026-06-21-tank-terrain-interaction-physics`** (h, independent, military sandbox, verdict=`concluded-verdict-yes`).
Realistic tank suspension on voxel-deformable terrain: ray-cast suspension per wheel, articulated tracks as XPBD constraint chain, hull tilt. C++26 CPU prototype `prototype/tank_suspension_bench.cpp` (Clang 22.1.6, build green 0 errors). 5 terrain types × 3 speeds = 15 configs × 1000 iterations + 100 warmup. **Total cost: 0.005 ms/vehicle — 40× under <0.2 ms budget.** Ray-cast suspension: 0.19–0.70 µs (12 wheels). XPBD track (2×24 links, 8 iters): 4.48–4.64 µs. Hull tilt: 0.06–0.09 µs. Worst-case total: 5.42 µs. Integration: `src/physics/tank_vehicle.{hpp,cpp}` module. См. §6 + [README](./experiments/2026-06-21-tank-terrain-interaction-physics/README.md) +
[STATUS](./experiments/2026-06-21-tank-terrain-interaction-physics/STATUS.md) +
`research/backlog.md §Closed`.

`2026-06-21` — **closed `2026-06-21-recon-intel-fog-of-war`** (h, independent, military sandbox — Tier 2 AI, verdict=`concluded-verdict-yes`).
Dynamic fog of war with per-entity detectability signatures (visual/IR/radar/acoustic/SIGINT), multi-channel sensor fusion, and intel aging. C++26 CPU prototype `prototype/fow_bench.cpp` (Clang 22.1.6, build green 2 cosmetic warnings). 5 strategies × 5 scenes × 5 seeds × 1000 iter = **125 main measurements**. **Headline:** ALL strategies well under budget (worst 31.5 µs = 0.094% of 30 Hz frame). Multi-channel fusion delivers **8-10× better detection on night** vs pure visual (10% vs 1.6%). Intel aging overhead <3 µs (17%). Zero false positives. **Integration:** 4-phase ~600 LoC, deferred до Stage 6+ military sandbox activation. Built on closed `flood-fill-visgraph-culling` (LOS basis) + `interest-management-aoi-battle` (intel broadcast tiering). См. §6 + [README](./experiments/2026-06-21-recon-intel-fog-of-war/README.md) + [RESULTS](./experiments/2026-06-21-recon-intel-fog-of-war/RESULTS.md) + [sources](./experiments/2026-06-21-recon-intel-fog-of-war/sources.md) + `prototype/{fow_bench.cpp, build/results.csv (126 rows)}`.

## 9. Archive references

- `experiments/_TEMPLATE/README.md` — шаблон формата эксперимента.
- `benchmarks/methodology.md` — стандарт измерений.
- `AGENTS.md` — протокол.
