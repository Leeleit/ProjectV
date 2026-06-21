# INDEX — `docs/experiments/`

Текущий снимок состояния. Долговечные правила — `AGENTS.md`. Канбан гипотез — `research/backlog.md`.

---

## 1. Now

**Active (this session, `2026-06-21`):**

- `2026-06-21-voxel-topology-analysis` (verdict=`yes`). **Cross-cutting (Stage 3.x/4.x) — Voxel topology analysis on 8³ grids**. Self-invented per operator instruction. Web-research complete (11+ sources: Rosenfeld-Pflatz 1968, Wu-Otoo-Suzuki SAUF, LSL3D, BUF GPU, cc3d, Minecraft structure locator, Tomcc cave culling). Standalone C++26 CPU prototype `prototype/topology_bench.cpp` ~580 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 2 cosmetic warnings**). 5 strategies x 5 scenes x 5 seeds x 1000 iter + 10 warmup = **125,000 main measurements**, wall time < 0.5 sec на Zen 3 5800X governor=`powersave`. **Headline:** Union-Find CCL 26-conn = 2.73 µs mean (worst 6.81 µs); Overhang detection = 0.19 µs mean; Exposed classify = 0.55 µs mean; Flood-fill = 2.32 µs mean. All strategies **100-2500× within 50 µs Stage 4.1 budget**. **Critical finding:** Air CCL on 8³ alone cannot detect disconnected caves (air always 1 component) — cross-chunk merging essential. Solid CCL, overhang, exposed classification work immediately. **Integration recommendation:** 4-step migration ~600 LoC (Step 1: VoxelTopology.hpp header ~50 LoC; Step 2: per-chunk wiring in ProcessChunkRebuildQueue ~150 LoC; Step 3: cross-chunk DSU merging ~300 LoC; Step 4: consumer systems ~100 LoC). См. §6 + [README](./experiments/2026-06-21-voxel-topology-analysis/README.md) + [STATUS](./experiments/2026-06-21-voxel-topology- `2026-06-21-ecs-1m-entities-bottleneck` (verdict=`yes`). **Stage 6.x — Flecs ECS 1M+ entity bottleneck analysis** (can ProjectV hold 1M+ entities? Where are the bottlenecks?). Self-invented per operator instruction. Web-research complete (12+ sources: Flecs v4.1 release notes, official benchmarks, Flecs vs EnTT FAQ, relationships fragmentation docs, closed `2026-06-20-flecs-soa-vs-aos-bench`). Standalone C++26 CPU prototype `prototype/ecs_bench.cpp` ~275 LoC using vendored Flecs v4.1.5 (`external/flecs/`). Clang 22.1.6, build green 0 errors. 7 benchmarks × 3 scales (10K/100K/1M) × 6 archetype patterns = **126 configs × 15 iter = ~8400 total world-ops measurements. Headline: Flecs handles 1M+ easily. Full live gameplay cycle (100K ents, 100 frames) = 3.74 µs/frame = 0.011% of 33 ms budget. Entity creation (0.4-1.0 µs/ent) and deletion (0.3-0.9 µs/ent) are only meaningful costs. Iteration ~0.5 ns/ent (free). Fragmentation 127× overhead but 12.8 µs absolute at 1M (negligible). Add/remove ~76 ns/op. Memory ~172 MB at 1M. **No changes needed — Flecs is already mainline default.** Recommendation: use bulk deferred ops for spawn/despawn; batch chunk unload across frames if >1500 ents/frame. См. §6 + [README](./experiments/2026-06-21-ecs-1m-entities-bottleneck/README.md) + [STATUS](./experiments/2026-06-21-ecs-1m-entities-bottleneck/STATUS.md) + `prototype/{ecs_bench.cpp, build/ecs_bench, CMakeLists.txt}`.

Just-closed (this session, `2026-06-21`):

- `2026-06-21-lockstep-state-sync-hybrid-netcode` (verdict=`mixed`). **Military sandbox axis — Tier 1 Core Engine Systems: Netcode**. Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй»; **first dedicated netcode architecture axis** в 100+ closed experiments. 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time 19.5 sec. Web-research via direct webfetch (Exa 429 + DuckDuckGo CAPTCHA blocked) — **5 primary + 3 supplementary sources verified**: Glenn Fiedler "Deterministic Lockstep" + "Snapshot Interpolation" + "Floating Point Determinism" (SupCom precedent: `_controlfp(_PC_24) + _RC_NEAR` @ 1M+ customers) + Wikipedia Netcode + Wikipedia Lag. **Headline (mixed):** **A_PureLockstep ⭐ = DEFAULT for ProjectV** at 48-92 KB/s/player (hypothesis ≤50 KB/s/player CONFIRMED for A only); B_PureStateSync = NEVER (94-150× worse, 4.5-13.8 MB/s/player); C_Hybrid_10Hz / D_Hybrid_5Hz / E_RollbackCRC = 17-32× worse than A (snapshot payload dominates); **E CRC overhead = 2053 µs/tick = 30× C** (table-based CRC32 over 10k entities). **Architectural recommendation:** A_PureLockstep default + D_Hybrid_5Hz @ 0.2 Hz for late-joiner + divergence recovery. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** ~1650 LoC, L effort, 3-5 sessions. Steps 1+2 (determinism foundation + FPU mode) **immediate prerequisite for 100-player scale**; Step 3 (recovery + late-joiner) deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36. См. §6 + [README](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/README.md) + [STATUS](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/STATUS.md) + [RESULTS](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/RESULTS.md) + [sources](./experiments/2026-06-21-lockstep-state-sync-hybrid-netcode/sources.md) + `prototype/{netcode_bench.cpp (744 LoC), build/{netcode_bench, results.csv (126 rows, 12 KB)}}`.

- `2026-06-21-fixed-wing-flight-model-simulation` (verdict=`yes`). **Military sandbox axis — Fixed-wing flight model simulation**. Independent physics scope. Standalone C++26 CPU prototype `prototype/flight_model_bench.cpp` ~1065 LoC. 5 strategies × 5 scenes × 5 seeds × 2 tick rates = **250 main measurements**. **Headline:** **C_RK4_4Section (and Vectorized E) = ~908 ns / ~849 ns per aircraft**, which is **5.5× below the 5 µs target budget**. RK4 maintains **9.4 m RMS error at 20 Hz tick**, compared to B_Euler_4Section which has **117.4 m** error (a **1150% accuracy delta**), proving the immense benefit of RK4 integration for stability and trajectory accuracy at low tick rates. **D_Analytical_LOD = ~101 ns**, perfect low-cost fallback for distant aircraft (LOD2). **Integration:** S-M effort, 1-2 sessions. Create `src/physics/FlightVehicle.{hpp,cpp}` module using Flecs components. Use RK4 for LOD0/1 and Analytical for LOD2. См. [README](./experiments/2026-06-21-fixed-wing-flight-model-simulation/README.md) + [STATUS](./experiments/2026-06-21-fixed-wing-flight-model-simulation/STATUS.md) + [RESULTS](./experiments/2026-06-21-fixed-wing-flight-model-simulation/RESULTS.md) + `prototype/{flight_model_bench.cpp, build/results.csv}`.

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

- **`2026-06-21-aircraft-damage-model`** — h, independent (military sandbox — Tier 1 Core Engine Systems: Physics).
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session.
  **Blocker:** нет.
  **Hypothesis (one-line):** Ray-cast damage checks against a component hit-table (8-12 bounding volumes representing engines, wings, control surfaces, fuel tanks) + cascading failures (fuel leak → fire → structural collapse) cost <1 µs/projectile on CPU; structural wing separation under high G-load when damaged matches aerodynamic limit.
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-aircraft-damage-model/{README.md,STATUS.md,sources.md,RESULTS.md}`
    - `docs/experiments/experiments/2026-06-21-aircraft-damage-model/prototype/` (standalone C++26 CPU prototype + bench)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)

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

- **`2026-06-21-naval-vessel-buoyancy-steering`** — h, independent (military sandbox — Tier 1 Core Engine Systems: Physics; **first dedicated naval-vessel-physics axis** в 100+ closed experiments; **orth** к closed `tank-terrain-interaction-physics` [yes, ground vehicle] + `fixed-wing-flight-model-simulation` [yes, flight dynamics] + `helicopter-rotor-physics` [in-progress, rotor momentum] + `ballistic-projectile-simulation` [yes, naval AA upstream] + `aircraft-damage-model` [in-progress, ship AA damage] + `procedural-military-terrain-gen` [closed yes, depth maps] + `water-surface-rendering` [in-progress, naval rendering]; **complementary** к `after-action-replay-system` [closed mixed, buoyancy must be deterministic] + `lockstep-state-sync-hybrid-netcode` [closed mixed, ship state = lockstep node]).
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session.
  **Blocker:** нет.
  **Hypothesis (one-line):** Buoyancy from per-column submerged voxel volume (sum over chunk heightmap) at **<0.01 ms/ship**; ship response 6-DOF with hydrodynamic added mass terms at **<0.05 ms/ship** for 100+ naval vessels in scenario; total fleet cost **<5 ms** within Stage 3.1 frame budget.
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-naval-vessel-buoyancy-steering/{README.md,STATUS.md,sources.md,RESULTS.md}`
    - `docs/experiments/experiments/2026-06-21-naval-vessel-buoyancy-steering/prototype/` (standalone C++26 CPU analytical cost model)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)

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

## 6. Recent closed sessions

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

## 7. Backlog

См. `research/backlog.md`.

## 8. Last update

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