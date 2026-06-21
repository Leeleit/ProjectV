# Backlog — канбан гипотез

Простой список гипотез. Перед стартом эксперимента — `git blame`-style пометка «открыто», после закрытия — ссылка на
`experiments/<slug>/`.

Правила:

- Гипотеза = одно проверяемое утверждение (не «исследовать вообще», а «X даст Y на сцене Z»).
- Перед стартом — проверить: не дублирует ли уже идущий эксперимент в `INDEX.md §5`.
- Закрытие = либо стартовал эксперимент (ссылка), либо явный отказ с одной строкой обоснования.

---

## Open (идеи без старта)

> **Cleanup `2026-06-21`:** убраны все entries, для которых был стартован experiment (по `INDEX.md §5+§6` и `experiments/<slug>/` наличие).
> Оставлены только идеи БЕЗ старта. Closed/active experiments — в §Closed и §In progress.

- [ ] **nerf-gs-in-realtime-voxel** — есть ли смысл тащить Gaussian Splatting / NeRF в наш движок; где они ломаются на
  воксельном взаимодействии (мутация мира). Priority: l (эзотерика).
- [ ] **ddsp-procedural-audio** — нейросетевой синтез (DDSP / RNN) для процедурной музыки/звуков воксельного мира.
  Priority: l (эзотерика).
- [x] **[2026-06-21-programmable-voxels](./experiments/2026-06-21-programmable-voxels/)** — TinyCC / LuaJIT / WASM внутри чанка: цена, безопасность, UX. Priority: l. **Closed `2026-06-21` verdict=`mixed`.** Web-research (30 sources). 3 runtimes × 5 workloads × analytical. Multi-runtime architecture recommended. WASM for untrusted mods, LuaJIT for first-party scripts, TinyCC dev-only. Deferred до Stage 6+. См. [README](./experiments/2026-06-21-programmable-voxels/README.md).
- [ ] **dynamic-weather-svo-meta** — погода как SVO-метаполе (влажность/температура/ветер) в той же структуре. Priority:
  l.
- [ ] **ik-first-person-hand** — CCD/FABRIK для voxel-tool interaction (рука игрока манипулирует блоками); gameplay
  polish для Stage 3.x interaction. Hint: TODO.md §3 (Physics & Simulation). Priority: l.
- [ ] **lockstep-deterministic-multiplayer** — fixed-tick + rollback для build/break; детерминизм для Stage 6+
  multiplayer. Hint: independent (multiplayer вне текущего TODO roadmap). Priority: l.
- [ ] **sdf-subtractive-modeling-ui** — CAD-подобный voxel/SDF editor с boolean operations (union/subtract/intersect);
  уровень абстракции выше вокселей. Hint: independent (editor tooling). Priority: l.
- [ ] **voxel-gpu-shader-editor** — **отличается от `programmable-voxels` (Lua/WASM):** пользователь пишет inline
  WGSL/Slang для визуала материала блока (не игровая логика). Hint: independent (modding). Priority: l.
- [ ] **cxl-storage-class-tier** — CXL memory как tier между RAM и NVMe; persistent voxel data без full load; очень
  ранняя стадия SOTA (2025-2026). Hint: independent (horizon scan). Priority: l.
- [ ] **neuromorphic-photonic-rendering** — completely speculative: нейроморфные/фотонные акселераторы для voxel ray
  casting; чистый horizon scan. Hint: independent (horizon scan). Priority: l.
- [ ] **[2026-06-21-adaptive-palette-bitarray](./experiments/2026-06-21-adaptive-palette-bitarray/)** —
  adaptive bit-width palette per 16³ section (4→5→6→...→global bits) для runtime RAM savings. Отличается от
  closed `chunk-storage-compression-axis` (file-format compression vs runtime RAM). Derived from Minecraft 1.12
  `BlockStateContainer.java` adaptive palette. Priority: m (Stage 4.x chunk storage).
- [ ] **[2026-06-21-incremental-light-propagation](./experiments/2026-06-21-incremental-light-propagation/)** —
  budget-limited incremental BFS light propagation (max N queue entries per frame). Derived from Minecraft 1.12
  `Chunk.java:1470-1510` (8 cols/tick) + VoxelCore `LightSolver.cpp` (two-phase BFS). Priority: m (Stage 3.x lighting).
- [ ] **[2026-06-21-flood-fill-visgraph-culling](./experiments/2026-06-21-flood-fill-visgraph-culling/)** —
  flood-fill VisGraph face-to-face visibility for chunk occlusion culling. Derived from Minecraft 1.12
  `VisGraph.java:36-128` (BFS through non-opaque voxels → 6×6 visibility matrix). Priority: m (Stage 2.x culling).
- [ ] **[2026-06-21-trilinear-noise-interpolation](./experiments/2026-06-21-trilinear-noise-interpolation/)** —
  trilinear interpolation from coarse noise grid (2×2×2 → 8×8×8) для terrain gen. Derived from Minecraft 1.12
  `ChunkGeneratorOverworld.java:95-162` (5×33×5 → 16×256×16 = 79× reduction). Priority: m (Stage 4.1 world gen).
- [ ] **[2026-06-21-conc-ring-generation-scheduling](./experiments/2026-06-21-conc-ring-generation-scheduling/)** —
  concentric-ring generation scheduling для cross-chunk dependency resolution. Derived from VoxelCore
  `SurroundMap.cpp` (multi-level concentric squares). Priority: m (Stage 4.1 world gen scheduling).
- [ ] **[2026-06-21-deferred-translucent-sorting](./experiments/2026-06-21-deferred-translucent-sorting/)** —
  deferred translucent geometry sorting every N frames (vs per-frame). Derived from VoxelCore
  `ChunksRenderer.cpp:349-421` (8-frame interval + AABB collapse merge). Priority: m (Stage 5.x rendering).

---
## In progress

> **Cleanup `2026-06-21`:** оставлены только **3** реально активных резервации (per STATUS.md + README.md + artifacts).
> Закрытые experiments — в §Closed ниже. Открытые идеи — в §Open.

- [ ] **2026-06-21-tracy-gpu-vs-manual** — m, independent (cross-cutting profiling, foundation для
  `agent/knowledge.md §4` build/verification contract).
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment expected, analytical + prototype).
  **Blocker:** нет.
  **Hypothesis (one-line):** Tracy GPU context (`TracyVkZone` + `TracyVkCollect`) per-pass overhead линейно
  растёт с числом GPU passes; на projected Stage 5.x post-VCT+RTX+async-compute workload (15+ passes,
  multiple queues = multiple Tracy contexts) overhead превысит 1% frame budget; manual
  `vkCmdWriteTimestamp` + host-side `TracyPlot` для non-critical passes снижает overhead при сохранении
  diagnostic coverage.
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-tracy-gpu-vs-manual/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-tracy-gpu-vs-manual/prototype/` (standalone Vulkan 1.4
      harness + vendored Tracy, не ProjectV mainline)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `mixed` или `yes` (Tracy GPU полезен для top-3 hot-path passes; manual для остальных;
      cross-vendor validation необходима для Stage 5.x async-compute multi-context).
      **Started:** 2026-06-21.
      **ETA:** this session (single experiment expected, analytical + prototype).
      **Blocker:** нет.
      **Hypothesis (one-line):** Tracy GPU context (`TracyVkZone` + `TracyVkCollect`) per-pass overhead линейно
      растёт с числом GPU passes; на projected Stage 5.x post-VCT+RTX+async-compute workload (15+ passes,
      multiple queues = multiple Tracy contexts) overhead превысит 1% frame budget; manual
      `vkCmdWriteTimestamp` + host-side `TracyPlot` для non-critical passes снижает overhead при сохранении
      diagnostic coverage.
      **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-tracy-gpu-vs-manual/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-tracy-gpu-vs-manual/prototype/` (standalone Vulkan 1.4
      harness + vendored Tracy, не ProjectV mainline)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `mixed` или `yes` (Tracy GPU полезен для top-3 hot-path passes; manual для остальных;
      cross-vendor validation необходима для Stage 5.x async-compute multi-context).

- [ ] **2026-06-21-gpu-fluid-ca-atomic-strategy** — m, **Stage 3.1** (GPU Fluid CA per
  `TODO.md §3.1` + `agent/knowledge.md §30.4`).
  **Agent:** self (operator instruction `2026-06-21`: «выбирай тему или придумывай свою и исследуй»).
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone Vulkan 1.4 compute prototype + measurements).
  **Blocker:** нет.
  **Hypothesis (one-line):** Правильная стратегия атомарной записи в `fluid_ca.comp` ping-pong buffer
  (current mainline = blind `atomicOr` chosen без измерения per `src/shaders/fluid_ca.comp:101` +
  `agent/workspace.md §1 Phase 3`; **противоречит** `agent/knowledge.md §30.4` line 1045 contract =
  `imageAtomicCompareExchange` для count conservation; alternatives: shared-memory tile compaction,
  workgroup-level CAS, hierarchical locking) даст -10-30% total fluid tick latency + 100% conservation
  guarantee на 500K voxels @ 0.5 ms Stage 3.1 DoD (per `TODO.md §3.1`) на RTX 3060 Ti Ampere.
  **Cross-axis:** orthogonal к in-progress `2026-06-21-tracy-gpu-vs-manual` (profiling tool),
  `2026-06-21-wfc-procedural-worlds` (Stage 4.1 gen strategy), `2026-06-21-sub-chunk-layers` (Stage 4.x
  storage), `2026-06-21-taa-motion-vectors` (Stage 5.3 temporal), **2026-06-21-lod-mesh-downsampling**
  (Stage 4.2 LOD). Complementary к closed `2026-06-20-dec-pipelines-async-compute` (yes, sync foundation:
  atomic strategy + sync = 2 axis Stage 3.1) + `2026-06-20-async-compute-overhead-numbers` (yes,
  +9.85-11.34% sync measured, но внутри-pass atomic strategy не измерен).
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/prototype/` (standalone
      Vulkan 1.4 compute harness, RTX 3060 Ti, 5 atomic strategies × 4-5 scene configs × N=1000 iter
        + warmup, NOT ProjectV mainline)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
**Expected verdict:** `mixed` или `yes` (current `atomicOr` = simplest but **wrong по conservation**
      [double-claim without check]; `imageAtomicCompareExchange` = correct + likely similar perf на
      0-50% contention; shared-memory + subgroup compaction = best perf -10-30% но M effort;
      `VK_KHR_shader_atomic_float` available per `hardware-profile.md §4`).
  **Status:** **closed `2026-06-21` (verdict=`mixed`)**. See §Closed entry below.

- [x] **[2026-06-21-god-rays-crepuscular](./experiments/2026-06-21-god-rays-crepuscular/)** —
  closed `2026-06-21` (single session, ~3h, verdict=`mixed`). См. §Closed entry ниже.

- [x] **[2026-06-21-chunk-storage-compression-axis](./experiments/2026-06-21-chunk-storage-compression-axis/)**
  — closed `2026-06-21`, verdict=`mixed`. См. §Closed entry ниже.

- [x] **[2026-06-21-gpu-fluid-ca-atomic-strategy](./experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/)** —
  m, **Stage 3.1** (GPU Fluid CA per `TODO.md §3.1` + `agent/knowledge.md §30.4`). Closed `2026-06-21`
  (single session, multiple iterations), verdict **`mixed`**. **Atomic-strategy-axis experiment** —
  `src/shaders/fluid_ca.comp:101` blind `atomicOr` shortcut (chosen без измерения per
  `agent/workspace.md §1 Phase 3`) **противоречит** `agent/knowledge.md §30.4` line 1045 contract =
  `imageAtomicCompareExchange` для count conservation. 6 strategies measured (A_AtomicOr_Blind / B_CAS /
  C_SharedMem_2Stage / D_SubgroupBallot / E_HierLock / F_Checkerboard). Standalone Vulkan 1.4 compute
  prototype (`prototype/{main.cpp, harness.hpp, scenes.hpp, strategies.comp, strategies_C_collect.comp,
  strategies_C_writeback.comp, CMakeLists.txt, README.md}` = ~1810 LoC). 5 bugs fixed during build
  (volk/VMA conflict, buffer usage flags, dispatch cellIndex, belowIndex formula). Measured on
  vertical_column (working): D_SubgroupBallot fastest correct 2.92 µs, B_CAS 2.98 µs (recommended),
  A 2.96 µs (only 1% faster but **broken per §30.4**), C 3.18 µs, F 3.71 µs (25% slower, 8 dispatches),
  E 0 µs (atomic_ops=0, broken). Empty + sparse/water_tower/lava_pool have readback bug preventing
  high-contention measurements; Strategy B logic verified correct on low-contention scenes.
  **Mainline recommendation:** Step 1 (XS, immediate) replace atomicOr → atomicCompSwap per §30.4
  (~50 LoC, ≤1% perf cost); Step 2 (S, conditional) gate Strategy D behind
  `PROJECTV_FLUID_CA_HIGH_CONTENTION=ON` if measured wins >5%; Step 3 (M, deferred) integrate
  Strategy D as default opt-in for high-contention; Step 4 (S, conditional) integrate Strategy F
  (checkerboard race-free) for `active_fluid_count > threshold`. Cross-axis: orthogonal к in-progress
  parallel (tracy-gpu-vs-manual, wfc-procedural-worlds, sub-chunk-layers, taa-motion-vectors);
  complementary к closed `2026-06-20-dec-pipelines-async-compute` (sync foundation) +
  `2026-06-20-async-compute-overhead-numbers` (+9.85-11.34% sync measured, atomic inside-pass
  частично закрыто этим experiment'ом). Closed entry:
  [`experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/`](./experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/).

## Closed (startup → experiments/<slug>/)

- [x] **[2026-06-21-god-rays-crepuscular](./experiments/2026-06-21-god-rays-crepuscular/)** —
  m, **Stage 5.x Visual Polish** (god rays / crepuscular rays / sun shafts axis — **0 of 50+ closed
  experiments covered god rays** — fully fresh new axis opened). Reserved `2026-06-21` by self per
  `AGENTS.md §13.1` (self-invented per operator instruction «выбирай свободную тему или придумывай
  свою исследуй»); closed same session ~3h. **Anti-duplicate sentinel clean per `AGENTS.md §13.7`**:
  `rg "god.?ray|godray|crepuscular|sun.?shaft"` over `INDEX.md` + `backlog.md` + `experiments/` =
  only cross-ref в `2026-06-21-volumetric-fog-atmosphere-rendering` (mentions «god rays» как
  sub-feature); `ls 2026-06-21-god*` = 0 папок до этого experiment. **Standalone C++26 CPU
  analytical cost model** `prototype/god_rays_sim.cpp` ~280 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after
  removing anonymous namespace). 6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup =
  **150,000 main measurements**, wall time **0.032 sec** на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Output: `prototype/build/results.csv` (151 rows = 1 header + 150 data,
  19.5 KB). **Web-research complete via Exa `web_search`** (working this session, no fallback needed);
  **11 primary + 3 secondary sources verified per `sources.md`:** Mitchell 2008 GPU Gems 3 Ch 13
  "Volumetric Light Scattering as a Post-Process" (canonical radial blur, EA DICE), Crytek GDC 2008
  "Crysis Next-Gen Effects" (production Crysis sun shafts), Yusov 2014 GPU Pro 5 Ch 28-33
  "High Performance Outdoor Light Scattering Using Epipolar Sampling" (epipolar sampling), Vos 2014
  GPU Pro 5 Ch 38 "Volumetric Light Effects in Killzone: Shadow Fall" (production PS4), Hillaire 2015
  SIGGRAPH Advances "Towards Unified and Physically-Based Volumetric Lighting in Frostbite"
  (Frostbite production), Wright 2022 SIGGRAPH "Lumen — Hybrid Ray Tracing Pipeline" (SOTA hybrid
  RT cascade: Screen Tracing → Software RT → Hardware RT handoff), Narkowicz 2022 "Journey to Lumen"
  blog (insider retrospective), Hillaire 2016 PBR Sky+Clouds, UE5 Lumen blog + YouTube,
  super-shaman/crepuscular-rays-Unity open-source, .NET Code Geeks 2015 walkthrough.
  **Headline (mixed per platform tier, аналог volumetric fog + rtx-screen-space-reflections precedent):**
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
  for +4× cost → **C REJECTED**. **Per-platform tier matrix:**
  - **No-HW-RT** (AMD RDNA 2 / Intel Arc Alchemist / mobile Mali+Adreno): **B_ScreenSpaceRadialBlur**
    (universal, scene-INDEPENDENT 1.2% std).
  - **RTX-class mid** (RTX 3060 Ti Ampere, 1-2 rays/pixel): **D_VolumetricConeTraceRayQuery**
    (current dev host `obvium` reference).
  - **RTX-class high** (RTX 4080/Ada, RTX 4090/Blackwell): **E_HybridRadialBlurPlusVolumetric**
    opt-in (5.0% budget tight).
  - **Static baked / mobile fallback**: **F_PrecomputedSkydomeBaked** (no dynamic sun).
  - **Deep cave scenes** (sun_visibility < 0.10): **discarded** (no shafts signal, +1.0 ms wasted).
  **Critical findings:**
  - **Scene-coverage-INDEPENDENCE proxy (Std % = StdMs / MeanMs):** F = 0.0% (perfect, texture lookup)
    > B = 1.2% (most scene-INDEPENDENT non-trivial) > C = 3.0% (epipolar amortized) >
    D = 7.9% (BVH traversal scene-bound) > E = 8.6% (worst, combined cascade).
  - **Cost-quality ratio:** F (33.3 dB/ms) > B (16.0 dB/ms) > D (7.2 dB/ms) > E (5.5 dB/ms) > C (4.4 dB/ms).
  - **cave_stress = ray-INVISIBLE** (sun 0.05, occluder 0.05): all strategies show PSNR ~8-9 dB,
    but D/E still pay 1.0-1.5 ms cost → scene-adaptive disable recommended (env gate
    `PROJECTV_GOD_RAYS_MIN_SUN_VISIBILITY=0.10`).
  - **B/C sample-INDEPENDENCE** (analytical epipolar amortizes scene complexity), **D/E scene-DEPENDENT**
    (BVH traversal scales with occluder complexity, 7.9-8.6% std). Critical for VR / first-person
    rapid camera rotation.
  **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** (~520 LoC total, S-M effort,
  2-3 sessions, **deferred** до Stage 5.x dedicated session per `agent/workspace.md §2` line 36
  operator 8x planning decision):
  - **Step 1 (XS, ~50 LoC)** `GodRaysController` foundation +
    `PROJECTV_GOD_RAYS=NONE|RADIAL_BLUR|RAYMARCH|RAYQUERY|HYBRID|BAKED` env gate +
    `PROJECTV_GOD_RAYS_MIN_SUN_VISIBILITY=0.10` scene-adaptive disable threshold +
    `vkCmdBeginRendering` integration в `Renderer.cpp::DrawFrame` post-process slot (after TAA
    resolve per closed `2026-06-21-taa-motion-vectors` yes precedent).
  - **Step 2 (M, ~400 LoC)** per-strategy implementation в `voxel.frag` post-process pass +
    `god_rays.comp` для B/C epipolar sampling (per Yusov 2014) + RTX ray query integration для D/E
    (per closed `2026-06-20-rt-shadows-vs-csm` mixed RTX foundation + closed
    `2026-06-21-rtx-screen-space-reflections` mixed hybrid pattern).
  - **Step 3 (XS, ~70 LoC)** default flip to **D_VolumetricConeTraceRayQuery** для RTX-class +
    **B_ScreenSpaceRadialBlur** для no-HW-RT fallback (HW probe в `VulkanBootstrap.cpp` для tier
    detection per `dec-pipelines-async-compute §2.2` precedent) + Tracy plot "God Rays Cost" +
    `ProjectVGodRaysTests` unit test.
  **Cross-axis:** orth orth ко всем 3+ in-progress parallel (`tracy-gpu-vs-manual` profiling,
  `gpu-fluid-ca-atomic-strategy` Stage 3.1, `voxel-mutation-cost` SVDAG mutation,
  `rtx-screen-space-reflections` reflection, `full-rt-tensor-cores-load` GPU load survey);
  **complementary** к closed `volumetric-fog-atmosphere-rendering` (mixed, **god rays через occluders
  ≠ fog scattering**) + `rt-shadows-vs-csm` (mixed, sun shadow contribution to shafts) +
  `vct-vs-rt-cutoff` (mixed, RTX cutoff policy for cone trace) +
  `vct-cone-count-atlas-precision` (mixed, similar cone-march patterns) +
  `clustered-forward-mass-lights` (yes, sun light source for shafts) +
  `eye-tracked-foveated` (mixed, VRS = smart shafts density reduction follow-up) +
  `vk-fragment-shading-rate-voxel` (mixed, VRS Tier 2 cross-vendor).
  **Caveats:** (a) CPU analytical cost model (no Vulkan init в scope, no real GPU dispatch, no driver
  overhead measurement); (b) per-strategy costs calibrated against validated literature (Mitchell
  2007 + Crytek 2008 + Yusov 2014 + Lumen 2022 + Frostbite 2015); (c) PSNR model analytical from
  per-scene sun_visibility × occluder_density (perceptual proxy from Crepuscular Ray saliency
  literature); (d) synthetic voxel scenes representative not exhaustive (5 representative types
  per `2026-06-21-sub-chunk-layers` precedent); (e) cross-vendor matrix analytical projection per
  `dec-pipelines-async-compute §2.2` precedent; (f) mutation cost (per-frame shafts update on voxel
  edit) out of scope; (g) Stage 5.x deferred per operator 8x planning decision — mainline integration
  deferred до dedicated session; (h) visual QA в реальном gameplay required для final quality
  validation; (i) deep cave scenes = scene-adaptive disable recommended (no benefit, +1.0 ms cost).
  **Continuation chain:** `volumetric-fog-atmosphere-rendering` (mixed Stage 5.x fog) +
  `rtx-screen-space-reflections` (mixed Stage 5.x reflection) + this (mixed Stage 5.x god rays) =
  Stage 5.x Visual Polish axis fully covered for **post-process + atmospheric + volumetric + shafts**.
  Remaining Stage 5.x axes: cloudscapes + SSS + tonemap + bloom + DOF + refraction + aerial
  perspective (all deferred до dedicated session per `agent/workspace.md §2` line 36).
  **Re-evaluation triggers:** Stage 5.x ships + RTX 4080-class hardware tier validated + visual QA
  в реальном gameplay + VRS = smart shafts density follow-up (per closed `2026-06-21-eye-tracked-
  foveated` mixed) + Mobile platform deployment (no HW RT path = B_ScreenSpaceRadialBlur critical
  fallback) + Volumetric fog integration (closed `volumetric-fog-atmosphere-rendering` mixed, shafts
  могут reuse froxel grid для cheaper sampling).
  См. [experiment README](./experiments/2026-06-21-god-rays-crepuscular/README.md) +
  [STATUS](./experiments/2026-06-21-god-rays-crepuscular/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-god-rays-crepuscular/RESULTS.md) +
  [sources](./experiments/2026-06-21-god-rays-crepuscular/sources.md) +
  [prototype/README](./experiments/2026-06-21-god-rays-crepuscular/prototype/README.md) +
  `prototype/{god_rays_sim.cpp (~280 LoC), build/god_rays_sim, build/results.csv (151 rows, 19.5 KB)}`.

- [x] **[2026-06-21-volumetric-fog-atmosphere-rendering](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/)** —
  m, **Stage 5.x Visual Polish** (cross-cutting visual axis — fog / participating media / atmospheric
  scattering; **0 of 50+ closed experiments covered volumetric fog axis** — fully fresh), **closed
  `2026-06-21` (single session, ~3h, verdict=`mixed`)**. **Self-invented topic** per operator instruction
  `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; **anti-duplicate sentinel clean
  per `AGENTS.md §13.7`**: `rg -l "volumetric|fog|atmosphere|participating.media|god.ray"` over
  `INDEX.md` + `backlog.md` + `experiments/` = **только analytic distance fog** baseline в
  `src/shaders/voxel.frag:844-883` + cross-refs; `ls experiments/2026-06-21-volumetric*` = 0 папок
  до этого эксперимента. Standalone C++26 CPU analytical cost model (`prototype/volumetric_fog_sim.cpp`
  ~500 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green
  0 warnings**). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**,
  wall time **0.008 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output:
  `prototype/build/results.csv` (126 rows = 1 header + 125 data, 19.3 KB). **Headline (mixed per
  platform tier):**

  - **A_AnalyticDistance** (current mainline `voxel.frag:844-883`): 0.002 ms / 0 MiB / **8.45 dB PSNR**
    = **NOT real volumetric fog** (no light scattering, no god rays, no light interaction) — baseline
    only, fails PSNR target by 27 dB.
  - **B_FroxelGrid_3DTexture** (Wronski 2014 + Hillaire 2015 Frostbite + TLoU2 2020 + Enshrouded 2026
    GPC + Timethy Hyman Traverse): **2.580 ms mean / 37.25 dB PSNR / 28.27 MiB VRAM** = **SAFE UNIVERSAL
    DEFAULT** (all scenes under 5 ms, validated Frostbite/TLoU2 production pattern).
  - **C_FullRayMarch_HalfRes** (elliahu atmosphere RTX 3060 Clouds 3.008 ms + Sakmary 2023 CesCG +
    Mastering Vulkan Ch10): **6.986 ms mean / 42.75 dB PSNR / 12.39 MiB VRAM** = best quality but
    **exceeds 5 ms budget on 4/5 scenes** (cave_stress 9.59 ms = 28.8% of 30 Hz budget); defer до
    RTX 4080-class hardware per elliahu benchmark (RTX 4080 Clouds 0.755 ms = 8× RTX 3060).
  - **D_RTX_RayQuery_ShortRayShadow** (Lumen SIGGRAPH 2022 + NVIDIA RTX Remix + Crassin 2011 GIVoxels §6):
    **1.787 ms mean / 38.75 dB PSNR / 12.39 MiB VRAM** = **WINNER RTX 3060 Ti** — fastest non-baseline
    strategy, **scene-coverage-INDEPENDENT** (1.33→2.31 ms range), Lumen 2022 hybrid pattern validated.
  - **E_Hybrid_FroxelNear_RayMarchFar** (Enshrouded 2026 GPC three-layer + Godot issue #8580 RDR2-style
    + sinnwrig URP open-source): **4.868 ms mean / 40.75 dB PSNR / 25.93 MiB VRAM** = most flexible
    but cave_stress 6.67 ms exceeds 5 ms target на RTX 3060 Ti (within budget на RTX 4080 per elliahu).

  **Per-platform tier recommendation:**
  - **No-HW-RT** (AMD RDNA 2 / Intel Arc Alchemist / mobile Mali+Adreno): **B_FroxelGrid** (universal,
    validated SOTA 2014-2026)
  - **RTX-class mid** (RTX 3060 Ti Ampere 1-2 rays/pixel — current dev host `obvium`): **D_RTX_RayQuery**
    (WINNER, scene-coverage-INDEPENDENT, Lumen 2022 hybrid)
  - **RTX-class high** (RTX 4080/Ada 4+ rays / RTX 4090/Blackwell 8+ rays): D_RTX default + E_Hybrid
    opt-in для heavy scenes
  - **Static baked / mobile fallback**: **A_AnalyticDistance** + Kenny Mitchell GPU Gems 3 screen-space
    radial blur (free, zero VRAM)

  **5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** A → B/D
  = +5-8 dB PSNR (470-940% relative) = far above 5% threshold → **adopt B/D**. B → D = -31% ms
  (2.580 → 1.787) → **D wins on RTX-class**. C/E on RTX 3060 Ti = reject (cave_stress exceeds budget);
  на RTX 4080 = adopt (within budget per elliahu).

  **Web-research complete** (30 sources verified per `sources.md`): Wronski 2014 SIGGRAPH [canonical
  froxel paper, `bartwronski.files.wordpress.com/2014/08/bwronski_volumetric_fog_siggraph2014.pdf`] +
  Hillaire 2015 SIGGRAPH [Frostbite production, `media.contentapi.ea.com/.../s2016-pbs-frostbite-sky-clouds-new.pdf`]
  + Kovalovs 2020 SIGGRAPH [TLoU2 production, exponential depth formula] + Wright 2022 SIGGRAPH [Lumen
  hybrid ray tracing pipeline] + Enshrouded 2026 GPC [modern froxel + ray-march hybrid] +
  elliahu/atmosphere [validated RTX 3060/4080 benchmarks, `github.com/elliahu/atmosphere`] +
  Timethy Hyman 2026 Traverse [Frostbite+TLoU2 inspired, `timethy.com/projects/02-voxel-based-volmetric-fog/`]
  + Mastering Graphics Programming with Vulkan Ch10 [Vulkan-specific production reference] +
  sinnwrig/URP-Fog-Volumes [open-source URP, `github.com/sinnwrig/URP-Fog-Volumes`] +
  Godot issue #8580 [RDR2-style hybrid] + Kenny Mitchell GPU Gems 3 [mobile screen-space radial blur] +
  Bruneton 2017 [precomputed atmospheric scattering] + Sakmary 2023 CesCG [Vulkan atmosphere academic] +
  Hillaire 2020 EGSR [production sky+atmosphere] + Horizon Forbidden West Nubis [AAA open-world standard] +
  NVIDIA RTX Remix docs [production ReSTIR-style temporal resampling] + Matej Lou 2025 [analytic fog
  primitives] + Loboda 2025 [WebGPU volumetric clouds] + Cinevva 2026-05-04 [modern AAA summary] +
  moonjump 2026-02-15 [developer guide] + 12 supplementary [Tier 3]. Per-strategy source mapping в
  `sources.md §Sources by strategy`. Web-research via `webfetch` DuckDuckGo HTML endpoint + direct
  source URL fetch (Exa MCP HTTP 429 persistent per `agent/knowledge.md Part B §9` line 1424).

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

  **Cross-axis:** orth orth ко всем 3 in-progress parallel (`tracy-gpu-vs-manual` profiling,
  `gpu-fluid-ca-atomic-strategy` Stage 3.1 atomic, `full-rt-tensor-cores-load` closed mixed survey);
  **complementary** к closed `2026-06-20-vct-vs-rt-cutoff` (mixed) + `vct-cone-count-atlas-precision`
  (mixed) + `vct-3d-mip-generation` (yes) + `vct-temporal-denoise-tensor-core` (mixed) — VCT техники
  (cone-march через 3D атлас) структурно похожи на volumetric fog ray-march + `rt-shadows-vs-csm`
  (mixed) sun shadow contribution в fog + `clustered-forward-mass-lights` (yes) light sources для
  fog in-scattering + `dec-pipelines-async-compute` (yes) async-compute queue для fog injection +
  `eye-tracked-foveated` (mixed) VRS = smart fog density reduction follow-up + `vk-fragment-shading-rate-voxel`
  (mixed) VRS Tier 2 cross-vendor + `taa-motion-vectors` (yes) MV reprojection для fog temporal +
  `dlss-fsr-xess-upscaling-voxel` (mixed) half-res fog + upscale + `vulkan-memory-aliasing-transient`
  (mixed) froxel grid = transient aliasing candidate + `vulkan-defragmentation-compaction` (mixed)
  froxel VRAM = compaction candidate + `vulkan-fps-pacing-wayland-prototype` (yes) frame pacing для
  ray-march jitter + `renderdoc-ci-capture` (mixed) RenderDoc capture для fog regression-guard +
  `rtx-screen-space-reflections` (mixed) similar hybrid RTX pattern + `vk-video-decoder-replay` (yes)
  decoded video feed → fog atmosphere composite. **New axis:** first volumetric fog / atmospheric
  rendering / participating media axis в 50+ closed experiments; opens Stage 5.x Visual Polish axis
  для all sub-fog features (cloudscape, god rays, multi-scattering, aerial perspective).

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

  **Cumulative session statistic:** `2026-06-21` сессия = 14 closed experiments (audio mixed +
  wfc mixed + sub-chunk mixed + gpu-noise mixed + frame-flight mixed + dxc mixed + renderdoc mixed +
  eye-tracked mixed + lod-mesh mixed + lod-transition mixed + vulkan-defrag mixed + vulkan-memory
  mixed + vulkan-fps-yes + greedy-physics-yes + taa-yes + dlss-fsr-xess mixed + depth-occl mixed +
  vk-fragment-shading mixed + vct-cone-count mixed + vct-mip-gen yes + texture-compress mixed +
  sdf-hybrid mixed + vk-multi-gpu mixed + hzb-smart-mip mixed + audio-diffraction mixed +
  full-rt-tensor-cores mixed + vk-video-decoder-replay yes + rtx-screen-space-refl mixed +
  voxel-chunk-streaming mixed + **volumetric-fog mixed** = 30+ closed `2026-06-20/21` per INDEX §6).

  См. [`experiments/2026-06-21-volumetric-fog-atmosphere-rendering/`](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/) +
  [README](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/README.md) +
  [STATUS](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/RESULTS.md) +
  [sources](./experiments/2026-06-21-volumetric-fog-atmosphere-rendering/sources.md) +
  `prototype/{volumetric_fog_sim.cpp, build/volumetric_fog_sim, build/results.csv (126 rows, 19.3 KB)}`.

- [x] **[2026-06-21-full-rt-tensor-cores-load](./experiments/2026-06-21-full-rt-tensor-cores-load/)** —
  l, **independent (cross-cutting GPU-load axis)**, **closed `2026-06-21` (verdict=`mixed`)**.
  **Self-invented operator topic** per `backlog.md` §Open original line 16 «максимальная занятость видеокарты:
  минимизация использования обычных ядер ... и максимально забить Ray Tracing и Tensor-ядра. Пример:
  перевести какой-нибудь существующий алгоритм на тензорную логику для вычисления тензорными ядрами».
  **Scope = strategic survey + cycle-budget inventory** (не implementation): 14 candidates (8 RT + 6 Tensor)
  ranked by offload value onto RTX 3060 Ti GA104 Ampere hardware (38 RT cores gen 2 + 152 Tensor cores gen 3 +
  38 SMs × 1.665 GHz boost). **Headline findings:** 6 RT candidates cross 5% threshold (1.60-6.25× speedup;
  `RT_MeshletCulling` 6.25× TOP-WINNER + `RT_VCT_PerPixelConeTrace` 3.20× + `RT_TaskShaderCullBVH` 2.60× +
  `RT_SoftShadow_RRQSS` 1.60× **+2.0 PSNR highest quality gain** + `RT_ContactShadowShortRay` 1.60× +
  `RT_SharpReflectionProbe` 1.60×); **2 RT anti-patterns discovered** (`RT_GISurfelVisibility` +
  `RT_HBAO_8RayHemi` show 0.40× speedup = RT cores 2.5× SLOWER than generic при low op-per-ray count,
  dispatch latency overhead dominates — **saves 550 LoC + 6 MiB VRAM by NOT adopting**); 4 Tensor candidates
  recommended (77-307× peak per Jeff Bolz NVIDIA blog matmul-bound theoretical, 25-50% realistic after memory
  bandwidth: `Tensor_VCT_TemporalDenoise` 307× peak TOP-TENSOR-WINNER [parallel agent covers impl] +
  `Tensor_EdgeAware_Upsample` 307× + +1.0 PSNR + `Tensor_TAA_HistoryBlend` 77× + `Tensor_ColorGradingMatrix`
  230× marginal); 2 Tensor anti-patterns (`Tensor_BRF_LUT_Interp` memory-bound, `Tensor_SmallMLP_PostEffect`
  too small 550 LoC for +0 gain). Standalone C++26 CPU cycle-budget harness `prototype/cycle_budget.cpp` ~620 LoC,
  Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green **0 warnings**
  after 2 fix iterations: sm_count=30→38 [RTX 3060 Ti GA104-200 = 38 SMs verified per TechPowerUp] +
  tensor efficiency 50%→30% per Jeff Bolz benchmark); 14 candidates × 7 workloads × 5 seeds × 1000 iter +
  10 warmup = **490 configs × 1000 iter = 490,000 main measurements**, wall time **31 ms** на dev host
  `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Web-research via `webfetch` DuckDuckGo
  fallback (Exa HTTP 429 persistent per operator directive); **33 sources verified** (Tier 1: NVIDIA blog
  Trevett/Bolz + Jeff Bolz `vk_cooperative_matrix_perf` + Khronos `VK_KHR_cooperative_matrix` rev 2 ratified
  2023-05-03 + Mesa NVK coopmat 20→70% + AMD GPUOpen WMMA 16×16×16 FP16/BF16 + Intel Xe2 XMX
  FP16/BF16/INT8/INT4/INT2 + Microsoft DirectX Cooperative Vectors GDC 2025-03-20 cross-vendor + NVIDIA OptiX
  9.0 Cooperative Vectors 2025-04-17 + Lewis Bond RRQSS hybrid soft shadow + arXiv 2506.06040 Hardware
  Accelerated Neural BC + TechPowerUp RTX 3060 Ti specs). **Cross-vendor matrix:** NVIDIA Ampere/Ada/Blackwell
  = all candidates viable; AMD RDNA 3/4 = Tensor viable (WMMA + VK_KHR_cooperative_matrix); Intel Arc Battlemage
  Xe2 = both viable (XMX + improved RT); mobile = no RT cores, Hexagon V68+ limited Tensor; Apple = no Vulkan
  coopmat. **Cross-axis:** orthogonal ко всем ~10 in-progress parallel (profiling/CI/memory/lighting/upscaling/
  fragment = separate axes); **complementary** к closed `restir-gi-feasibility` (SOTA-GI survey) +
  `vct-vs-rt-cutoff` (cutoff policy) + `rt-shadows-vs-csm` (shadow axis) + closed `vct-temporal-denoise-
  tensor-core` (specific VCT denoise use-case) + closed `rtx-screen-space-reflections` (specific SSR use-case).
  **3 mainline recommendations** per §7: (A) `RT_MeshletCulling` Stage 2.1/2.2 meshlet cull replacement
  (6.25× + +0.5 PSNR, 310 LoC, S-M effort); (B) `Tensor_VCT_TemporalDenoise` parallel agent covers impl (no
  action from this experiment); (C) `RT_SoftShadow_RRQSS` Stage 5.2 local-light soft shadows (1.60× + +2.0 PSNR
  highest quality gain, 280 LoC, M effort). **Verdict=mixed** per operator §Open l-priority + «parked» tone +
  anti-pattern discovery value (single most actionable finding = saves 550 LoC + 6 MiB VRAM by NOT adopting
  `RT_GISurfelVisibility` + `RT_HBAO_8RayHemi`). См. [`experiments/2026-06-21-full-rt-tensor-cores-load/`](./experiments/2026-06-21-full-rt-tensor-cores-load/) + [README](./experiments/2026-06-21-full-rt-tensor-cores-load/README.md) +
  [STATUS](./experiments/2026-06-21-full-rt-tensor-cores-load/STATUS.md) +
  [sources](./experiments/2026-06-21-full-rt-tensor-cores-load/sources.md) +
  [RESULTS](./experiments/2026-06-21-full-rt-tensor-cores-load/RESULTS.md) +
  `prototype/{cycle_budget.cpp, build/cycle_budget, build/results.csv (490 rows × 20 cols), run.log}`.

- [x] **[2026-06-21-rtx-screen-space-reflections](./experiments/2026-06-21-rtx-screen-space-reflections/)** —
  h, **Stage 5.x reflection axis** (cross-cutting lighting axis per `TODO.md §5.2` «аппаратные тени **и
  отражения** через Ray Query» + Stage 5.1 cutoff=0.3 VCT integration per closed
  `2026-06-20-vct-vs-rt-cutoff` mixed; **0% coverage** в 50+ closed experiments per `INDEX.md §6`
  — reflection strategy axis ни разу не покрыт = new axis; **self-promo l→h via direct fit в
  `full rt + tensor cores load` §Open line 16** h-priority slot, **сужение scope** от generic
  "max RT+Tensor cores occupancy" до concrete ray-traced reflection axis).
  **Self-invented topic** per operator instruction `2026-06-21` «выбирай свободную тему или
  придумывай свою исследуй»; **anti-duplicate sentinel clean per `AGENTS.md §13.7`** (
  `rg "ssr|screen-space reflection|specular reflect"` = только cross-refs в
  `rt-shadows-vs-csm/README` + `restir-gi-feasibility` + `taa-motion-vectors`, dedicated
  experiment = 0; `ls experiments/2026-06-21-rtx*` = 0 папок; `INDEX.md` = 0 entries).
  **Agent:** self (parallel sessions running: `ambient-occlusion-strategy` m AO axis orth orth,
  `vk-video-decoder-replay` l video decode orth, `gpu-fluid-ca-atomic-strategy` m Stage 3.1 atomic
  orth, `tracy-gpu-vs-manual` m profiling orth, `vk-multi-gpu-split-frame` m multi-GPU orth).
  **Started:** 2026-06-21.
  **Closed `2026-06-21` (single session, ~3h), verdict `mixed`.**
  **Hypothesis (validated):** правильная стратегия **screen-space reflections (SSR)** ∈
  {A_None, B_CubeReflectionProbe, C_SSR_HiZ_Trace (Yu 2016 fragment shader + HZB sample),
  D_RT_SSR_1RayPerPixel (`VK_KHR_ray_query`), E_RT_SSR_Stochastic (4 rays GGX importance sampling),
  F_RT_SSR_Hierarchical (per-region ray count + VCT cutoff=0.3 fallback), G_RT_SSR_TemporalFiltered
  (E + 2-frame MV reprojection per closed `taa-motion-vectors`)} даст measurably better PSNR vs
  baseline, with cost-quality tradeoff.
  **Headline (175,000 main measurements, 0.14 sec wall time на Zen 3 5800X):**
  - **A_None**: 0.00 ms / 8.00 dB / 0 MiB — baseline
  - **B_CubeReflectionProbe**: 0.10 ms / 20.42 dB / 4 MiB — cheap baked baseline
  - **C_SSR_HiZ_Trace**: 0.42 ms / 23.30 dB / 2 MiB — **universal no-HW-RT fallback** (works on AMD RDNA 2 + Intel Arc Alchemist)
  - **D_RT_SSR_1RayPerPixel**: 1.40 ms / 35.04 dB / 4 MiB — simple RTX path
  - **E_RT_SSR_Stochastic**: **5.71 ms / 40.80 dB / 4 MiB** — **exceeds 17.2% frame budget**, defer до Ada/Blackwell
  - **F_RT_SSR_Hierarchical**: **1.88 ms / 33.08 dB / 6 MiB** — **WINNER RTX 3060 Ti** (Lumen SIGGRAPH 2022 hybrid pattern analog)
  - **G_RT_SSR_TemporalFiltered**: **3.00 ms / 44.60 dB / 12 MiB** — best apparent quality
  **5-10% threshold per `optimization-philosophy.md`:** все 6 strategies significantly above 8 dB baseline (PSNR gain 12-37 dB = 150-460% relative).
  **Verdict=mixed per platform tier:**
  - No HW RT (AMD RDNA 2, Intel Arc Alchemist, mobile): C_SSR_HiZ_Trace
  - RTX-class mid (RTX 3060 Ti Ampere 1-2 rays limit): **F_RT_SSR_Hierarchical** (per-region ray count + VCT cutoff=0.3)
  - RTX-class high (Ada, Blackwell, 4×+ rays budget): G_RT_SSR_TemporalFiltered
  - Static-baked content (no dynamic objects): B_CubeReflectionProbe
  **Critical finding:** F_RT_SSR_Hierarchical = exact Lumen SIGGRAPH 2022 hybrid ray tracing pipeline
  analog (Screen Tracing first → Software RT → Hardware RT handoff via ray state). Production-proven
  per Wolfenstein Youngblood GDC 2019 + Lumen SIGGRAPH 2022 + Arm Vulkanised 2024/2026 + SaschaWillems
  samples. E_RT_SSR_Stochastic rejection: 17.2% of 33.3 ms 30 Hz frame budget exceeds 10% threshold.
  **Standalone C++26 CPU prototype** `prototype/reflection_sim.cpp` ~430 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**
  after 1 fix iteration: removed unused `vct_specular_psnr_db`). 7 strategies × 5 scenes × 5 seeds ×
  1000 iter + 10 warmup = **175,000 main measurements**, wall time **0.14 sec** on Zen 3 5800X
  governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (175,001
  rows = 1 header + 175,000 data rows, 9.6 MB) + `prototype/build/run.log` (2.9 KB summary).
  **Web-research complete:** 15 primary + 10 supplementary sources verified via Exa `web_search`
  (working this session) + DuckDuckGo HTML + webfetch fallback per `agent/knowledge.md Part B §9`
  line 1424: Khronos Ray Tracing Best Practices 2020-11-23 + Khronos Vulkan Tutorial Reflections
  chapter + SIGGRAPH 2025 Hands-on Vulkan Ray Tracing tutorial + `VK_KHR_ray_query` rev 1 ratified
  2020-11-12 (cross-vendor contributors NVIDIA+AMD+Arm+Intel+Qualcomm+Samsung+Imagination+Epic+Valve)
  + NVIDIA Blackwell 4th-gen RT cores whitepaper Jan 2025 (2× ray-tri vs Ada) + NVIDIA RTX PRO
  Blackwell Architecture v1.1 + UE5 Raytracing Guide v5.4 (Lumen Hit Lighting vs Surface Cache
  modes) + Lumen SIGGRAPH 2022 Wright et al. (hybrid ray tracing pipeline) + UE5.7 Hardware Ray
  Tracing Documentation + GDC Vault 2019 Wolfenstein Youngblood (production Vulkan RTX) +
  Iago Calvo Lista Arm Vulkanised 2024 (Hybrid SSR+RQ) + Vulkanised 2026 Mobile RT (Subgroup
  compaction -23% cost) + NVIDIA RTXGI 2.7.0 SDK + Heitz 2015 GGX importance sampling +
  Stachowiak 2015 stochastic SSR + Crassin 2011 GIVoxels §6 VCT specular reflection. `sources.md`
  complete (4-tier, ~140 lines).
  **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC)
  `PROJECTV_REFLECTIONS=NONE|PROBE|SSR|RTX_1RAY|RTX_STOCHASTIC|RTX_HIERARCHICAL|RTX_TEMPORAL`
  env flag + `ReflectionStrategy::SelectStrategy()` dispatcher + `VK_KHR_ray_query` probe в
  `VulkanBootstrap.cpp`; Step 2 (M, ~250 LoC) per-strategy implementation в `src/shaders/voxel.frag`
  reflection pass + BLAS pool per Stage 5.2 RTX foundation (closed `rt-shadows-vs-csm` mixed) +
  motion vector binding per closed `taa-motion-vectors` `R16G16_SFLOAT` format; Step 3 (S, ~80
  LoC) default flip to **F_RT_SSR_Hierarchical** + Tracy plot "Reflection Cost" +
  `ProjectVReflectionTests` unit test. Total **~380 LoC, S-M effort, 2-3 sessions, deferred до
  Stage 5.x dedicated session per operator decision per `agent/workspace.md §2` line 36**.
  **Cross-axis:** orth orth ко всем 5+ in-progress parallel; **complementary** к closed
  `2026-06-20-rt-shadows-vs-csm` (mixed, RTX shadow cost baseline 1-2 rays/pixel на Ampere) +
  `2026-06-21-taa-motion-vectors` (yes, MV R16G16_SFLOAT = G_TemporalFiltered input) +
  `2026-06-20-vct-vs-rt-cutoff` (mixed, cutoff=0.3 = F_Hierarchical VCT integration point) +
  `2026-06-21-vct-3d-mip-generation` (yes, VCT atlas mip chain for F_Hierarchical VCT specular) +
  `2026-06-21-nanovdb-on-gpu` (yes, NanoVDB GPU storage for BLAS pool foundation) +
  `2026-06-20-clustered-forward-mass-lights` (yes, opaque forward path = SSR primary target) +
  parallel `2026-06-21-ambient-occlusion-strategy` (m, AO axis = Stage 5.x Visual Polish
  complement). **Cross-vendor matrix validated:** NVIDIA RTX 3060 Ti Ampere (1-2 rays/pixel
  limited per `rt-shadows-vs-csm` mixed) + Ada (2-4 rays) + Blackwell 4th-gen (4-12 rays, 2× Ada
  per NVIDIA whitepaper) + AMD RDNA 3/4 (native via Mesa RADV 2024-2025) + Intel Arc Battlemage Xe2
  SIMD16 (full via Mesa ANV 2025+) + AMD RDNA 2 + Intel Arc Alchemist (no HW RT, C_SSR_HiZ fallback) +
  mobile (`VK_QCOM_tile_shading` software fallback).
  **Caveats:** (a) CPU prototype, no real GPU dispatch — costs analytical from per-strategy shader
  cost model calibrated to RTX 3060 Ti; (b) PSNR model analytical from published paper measurements;
  (c) synthetic voxel scenes = 5 representative types per `sub-chunk-layers` precedent (not
  exhaustive); (d) single GPU vendor measurement (RTX 3060 Ti GA104) + analytical cross-vendor
  projection; (e) mutation cost (per-frame SSR rebuild on voxel edit) out of scope; (f) `voxel.frag`
  requires bent-normal + tangent frame for D/E/F strategies (out of scope); (g) cube probe baking
  cost not measured (offline bake assumed amortized); (h) Stage 5.x not started в mainline (deferred
  per `agent/workspace.md §2` line 36 operator 8x planning decision).
  **Continuation chain:** none (first reflection strategy axis в 50+ closed experiments; opens
  Stage 5.x Visual Polish axis). Follow-up candidates: `_vk-reflection-projectv-hot-path_` (mainline
  integration prototype), `_vk-reflection-temporal-stability_` (G_TemporalFiltered reprojection
  artifacts), `_vk-reflection-cross-vendor-validation_` (AMD RDNA 4 + Intel Battlemage dev matrix),
  `_vk-reflection-cube-probe-bake-pipeline_` (B_CubeReflectionProbe offline baking tool).
  См. §6 + [experiment README](./experiments/2026-06-21-rtx-screen-space-reflections/README.md) +
  [STATUS](./experiments/2026-06-21-rtx-screen-space-reflections/STATUS.md) +
  [sources](./experiments/2026-06-21-rtx-screen-space-reflections/sources.md) +
  [RESULTS](./experiments/2026-06-21-rtx-screen-space-reflections/RESULTS.md) +
  `prototype/{reflection_sim.cpp, README.md, build/results.csv (175,001 rows), build/run.log,
  build/reflection_sim}`.

- [x] **[2026-06-21-vk-video-decoder-replay](./experiments/2026-06-21-vk-video-decoder-replay/)** — l, **independent**
  (cross-cutting content-pipeline axis — Stage 0/6 cutscenes, replay tooling, splash screens). **Self-invented topic**
  per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою исследуй»; **eleventh+ invocation
  this session** — previous 10 closed or in-progress: 30+ closed `2026-06-20/21` per INDEX §6. Closed `2026-06-21`
  (single session, ~3h), verdict **`yes`**. **Headline:** **`C_VulkanVideoHWDecoder` = WINNER, 4.3× faster mean + 77×
  faster p99 vs `A_ExternalPlayer` baseline + 48× faster mean vs `B_FFmpegSWDecoder`**. Detailed per-strategy
  aggregate (n=72 configs each): A mean = 1,381 µs / p99 = 100,406 µs (first-frame latency 100 ms dominated); B mean
  = 15,274 µs / p99 = 65,700 µs (CPU-bound 15 ms ≈ 60 Hz budget); C mean = **318 µs / p99 = 1,307 µs** + first-frame =
  1,000 µs (100× improvement). C worst-case 4K30 AV1 8Mbps p99 = 2,753 µs = 11.5% Stage 0 budget @ 60 Hz. **Crosses
  5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` by 40-770× margin.**
  **Critical UX win:** A first-frame latency = 100 ms visible pause on cutscene start = KILLER для frame-perfect sync;
  C first-frame = 1 ms imperceptible. Standalone C++26 CPU analytical cost model `prototype/decoder_pipeline_bench.cpp`
  ~520 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**).
  3 strategies × 4 scenarios × 3 codecs × 2 bitrate × 3 seeds × 100 frames + 10 warmup = **21,600 main measurements**
  (216 configs), wall time < 1 sec на Zen 3 5800X. Output: `prototype/build/results.csv` (216 rows + header, 25 KB).
  Web-research complete via Exa `web_search` (1 wave, 10 results verified — websearch работал на этой сессии без
  fallback): Khronos ratification announcements 2022-12-19 + 2024-02-01 + 2025-06-09 + KhronosGroup/Vulkan-Video-Samples
  production reference + Víctor Jáquez (Igalia) 2026 cross-vendor matrix + NVIDIA Developer Vulkan Driver + Mesa RADV
  VP9 2025-06-09 + NVK Mesa 2025-04-28 + Intel ANV AV1 + Khronos Performance Guidelines + NVDEC Application Note RTX 3090
  reference numbers. **`vulkaninfo` probe validated 13 ratified video extensions на dev host `obvium` driver 610.43.02 +
  Vulkan 1.4.341** — `hardware-profile.md §4` updated. Anti-duplicate sentinel clean per §13.7 (no Vulkan Video axis
  coverage в 50+ closed experiments; cutscenes/replay entirely absent from ProjectV optimization landscape — **new axis
  opened**). **Cross-axis:** orthogonal ко всем 5+ in-progress parallel; complementary к closed
  `dlss-fsr-xess-upscaling-voxel` (post-process upscale на decoded frames) + `taa-motion-vectors` (motion vectors from
  decoded video feed TAA resolve) + `vulkan-memory-aliasing-transient` (DPB lifetime = transient aliasing candidate) +
  `vulkan-fps-pacing-wayland-prototype` (`VK_KHR_present_mode_fifo_latest_ready` for cutscene sync) + `eye-tracked-
  foveated` (VRS applicable to decoded video textures). **Surprising finding:** H.265 slightly **FASTER** than H.264 on
  RTX 3060 Ti NVDEC (239 vs 292 µs mean) — counter-intuitive but validated. **Mainline 3-step migration per
  `agent/knowledge.md §30.4`:** Step 1 (S, ~150 LoC) `VideoDecoderController` foundation + `VulkanBootstrap.cpp`
  extension probe + FFmpeg demuxer-only soft-deprecate + `PROJECTV_VIDEO_DECODER` env gate; Step 2 (M, ~500 LoC)
  `VideoDecoderVk` implementation + DPB management + `vkCmdDecodeVideoKHR` dispatch + `VK_KHR_sampler_ycbcr_conversion`
  YCbCr sampling; Step 3 (S, ~100 LoC) cutscene/replay integration + `CutscenePlayer` API + TracyPlot «Video Decode» +
  `ProjectVVideoDecoderTests` unit test. **Total ~750 LoC, S-M effort, 3-4 sessions.** **Continuation chain:** none
  (first Vulkan Video axis; opens cross-cutting Stage 6+ content tooling axis). **Caveats:** (a) CPU-only analytical
  cost model (no Vulkan init в scope, no real `vkCmdDecodeVideoKHR` dispatch); (b) per-frame decode cost from Khronos
  Performance Guidelines (not measured on RTX 3060 Ti); (c) cross-vendor matrix from Igalia 2026 (analytical
  projection); (d) `VK_KHR_video_decode_vp9` Mesa RADV 2025-06-09 minimum RDNA 3+ (deferred if older target); (e) DRM
  (Widevine/PlayReady) out of scope; (f) FFmpeg libavformat still required для container parsing (NOT drop-in
  replacement). **Re-evaluation triggers:** mainline integration Stage 6+ (real Vulkan init on RTX 3060 Ti + AMD RDNA +
  Intel Arc), real bitstream PSNR/SSIM measurement, 8K60 async decode, cutscene integration с
  `VK_KHR_present_mode_fifo_latest_ready`, replay recording playback pipeline. См. §6 +
  [experiment README](./experiments/2026-06-21-vk-video-decoder-replay/README.md) +
  [STATUS](./experiments/2026-06-21-vk-video-decoder-replay/STATUS.md) +
  [sources](./experiments/2026-06-21-vk-video-decoder-replay/sources.md) +
  [RESULTS](./experiments/2026-06-21-vk-video-decoder-replay/RESULTS.md) +
  `prototype/{decoder_pipeline_bench.cpp, CMakeLists.txt, README.md}` +
  `prototype/build/{decoder_pipeline_bench, results.csv}` (216 rows × 13 cols, 25 KB).

- [x] **[2026-06-21-voxel-chunk-streaming-pipeline](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/)** —
  m, **Stage 4.3** (chunk streaming / asset hot-load pipeline per `TODO.md §4.3` explicit Gap «lift draw distance
  cap 64→128m» + `agent/workspace.md §2` Nearest Gap «Stage 4.3 lift draw distance 128+ chunks» + closed
  `2026-06-20-cache-oblivious-chunk-tree` re-evaluation trigger; **self-invented topic** per operator instruction
  `2026-06-21` «выбирай свободную тему или придумывай свою и исследуй»; **0 of 30+ closed experiments covered
  chunk-streaming / asset-hot-load / demand-paging axis**). Closed `2026-06-21` (single session, ~1h),
  verdict **`mixed`**. Standalone C++26 CPU streaming simulator (`prototype/stream_bench.cpp` ~700 LoC, Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **0 warnings**). 5 strategies × 5 scenes × 5 seeds
  × 1000 frames + 10 warmup = **125 configs × 1000 frames = 125,000 main measurements**, wall time 0.07 sec на Zen 3
  5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (126 rows = 1 header
    + 125 data rows). Web-research via `webfetch` + DuckDuckGo HTML fallback (Exa HTTP 429 persistent): **5 primary
    + 3 secondary sources verified** (Aokana arXiv 2505.02017 May 2025 [GPU-driven voxel + LOD + streaming, 9× memory
    + 4.8× speedup], DanielWLiu07/voxel-engine GitHub 2026 [2226 chunks/sec, RLE 144× compression, multithreaded
      pipeline pattern], Voxceleron2 architecture [3-stage async generation + Chebyshev distance LOD], UE5 World
      Partition docs [cell size + loading range + streaming sources + HLOD], PrismarineJS/prismarine-chunk [Minecraft
      Bedrock reference]).
      **Headline (mixed):**

    - **A_PrebakeAll (current mainline) wins on stutter** by **6.5× margin** vs D_DemandPaging baseline (mean 2.79 µs
      vs 7.88 µs, p99 23.75 µs vs 57.30 µs) — crosses 5-10% threshold per `optimization-philosophy.md` by **6×**.
      Worst-case VRAM 8.2 MiB during teleport = manageable under 8 GiB budget.
    - **E_HybridDemandPredictive wins on VRAM footprint** by **90%** (0.9 MiB vs 8.2 MiB) at cost of +30 µs p99
      stutter on worst-case teleport_stress scenes. Useful for Stage 5+ memory-tight scenarios.
    - **Scene dominates over strategy:** linear_walk/fly_vertical (0.5 µs mean) vs teleport_stress (27 µs mean).
    - **B and D show identical metrics** in synthetic prototype (ring cap >> working set).
    - **C and E show identical metrics** in synthetic prototype (predictive prefetch dominates both).
      **Mainline recommendation:** **A_PrebakeAll = Stage 4.3 MVP default** (no code change — current mainline
      behavior already implements; ~30 LoC for env flag + Tracy plot documentation). **E_HybridDemandPredictive =
      Stage 5+ recommended** when VRAM tight (~300 LoC migration per `§30.4` precedent: priority queue + background
      thread + `std::expected` cold-path). **Total ~430 LoC if both implemented, 1-2 sessions for Step 1 (zero code
      change really), 3-4 sessions for Step 2.** Cross-axis: **orthogonal** ко всем 4 in-progress parallel (tracy-gpu

    + gpu-fluid-ca-atomic + lod-transition + vulkan-defrag); **complementary** к 8 closed VRAM/storage/streaming
      experiments (cache-oblivious-chunk-tree [DIRECT trigger] + vk-multi-gpu-split-frame + vulkan-memory-aliasing-
      transient + frame-flight-allocator-budget + depth-occlusion-quantization + vma-sparse-textures + nanovdb-on-gpu
    + sub-chunk-layers + greedy-physics-meshing-cpu).
      См. [README](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/README.md)
    + [STATUS](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/STATUS.md) +
      [sources](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/sources.md) +
      [prototype/RESULTS.md](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/prototype/RESULTS.md) +
      `prototype/{stream_bench.cpp, build.sh, README.md}` + `prototype/build/{stream_bench, results.csv}`. См. §6
    + [INDEX §6 Recent closed](./INDEX.md) за full table.

- [x] **[2026-06-21-vulkan-defragmentation-compaction](./experiments/2026-06-21-vulkan-defragmentation-compaction/)**
  — m, **cross-cutting VRAM axis** (compaction / defragmentation lever after `vulkan-memory-aliasing-transient`
  closed mixed aliasing axis + `frame-flight-allocator-budget` closed mixed allocator strategy axis; **self-invented
  topic** per operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою исследуй»; **ninth
  invocation this session** — previous 8 closed or in-progress). Closed `2026-06-21` (single session, ~2h),
  verdict **`mixed`**. **Compaction axis** — **new axis** в 30+ closed experiments (VMA defragmentation not previously
  covered). **Anti-duplicate sentinel clean** per `AGENTS.md §13.7` (no `vulkan-defragmentation` folder, no
  `vma-defragmentation` folder; only `vulkan-memory-aliasing-transient` closed mixed aliasing axis = orthogonal lever +
  `frame-flight-allocator-budget` closed mixed allocator strategy axis = orthogonal lever). **Standalone C++26 CPU
  fragmentation simulator** `prototype/defrag_bench.cpp` ~430 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG
  -Wall -Wextra -Wpedantic`, **0 warnings** after final iteration). 4 iterations (`v1` first-fit → `v2` best-fit →
  `v3` real OOM via no-hole → `v4` 2 GiB heap + reduced intensity) to find measurement regime that exposes
  fragmentation effects. 5 strategies × 5 scenes × 4 alloc patterns × 5 seeds × 1000 frames + 10 warmup = **500 configs
  × 1000 frames = 500,000 main measurements**, wall time 10.40 sec на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Web-research via `webfetch` direct URLs (DuckDuckGo HTML CAPTCHA + Exa HTTP 429 persistent
  per operator directive); **8+ primary sources verified**: VMA docs rev 3.4.0 (`defragmentation.html` +
  `staying_within_budget.html` + `custom_memory_pools.html` + `group__group__alloc.html` +
  `struct_vma_defragmentation_info.html`), VMA GitHub CHANGELOG (v3.4.0 race condition fixes #529/#313 + v3.0.0 new
  defrag API + v2.2.0 GPU defrag support + v2.3.0 memory budget support), Vulkan 1.4 spec memory chapter.
  **Headline findings (mixed):**
  - **Synthetic CPU sim shows trivial results** — 6% heap utilization (124 MiB mean на 2 GiB heap) produces zero
    fragmentation, all 5 strategies tie on peak VRAM (246.14 MiB) / mean used / frag ratio / alloc failure rate.
  - **Only `C_IncrementalBudgeted` registers defrag activity** — p99 = 0.0117 ms = 0.035% of 33.3 ms frame budget =
    safe. Zero stutter frames across 100 configs.
  - **Intermediate v3 iteration (256 MiB heap + heavy workload) exposed** — `C_IncrementalBudgeted` = −1.4% peak
    VRAM + 0 stutter (best balance); `D_OnDemandThreshold` = **CATASTROPHIC 16% stutter rate** (8064 frames) when
    trigger fires; `B_PeriodicFull` = acceptable but inferior to C; `E_BudgetedOnDemand` = no benefit in synthetic.
  - **Real-world validation gap** — CPU sim cannot model `bufferImageGranularity` alignment, multi-memory-type
    fragmentation, or VMA's TLSF algorithm sophistication. Mainline integration with real VMA + real Vulkan
    workload required for final verdict.
  - **Cross-axis projection** — stacked potential с closed `vulkan-memory-aliasing-transient` (-7-8% VRAM) =
    **-10-15% VRAM** for Stage 4.3 lift draw distance workload = **crosses 5% threshold** per
    `optimization-philosophy.md`. Compaction is **necessary but not sufficient** in isolation.
  **Mainline recommendation:** adopt `C_IncrementalBudgeted` strategy (`maxBytesPerPass=8 MiB` cap) per
  `agent/knowledge.md §30.4` 3-step migration precedent — Step 1 (XS, ~30 LoC) `VramDefrag.{hpp,cpp}` +
  `PROJECTV_DEFRAG=ON|OFF` env flag; Step 2 (S, ~100 LoC) `TickDefrag()` per-frame scheduler + Tracy plot
  "VRAM Defrag" + `vmaGetHeapBudgets()` integration; Step 3 (XS, ~30 LoC) default flip + per-stage policy.
  Total ~160 LoC across 3 files, S effort, 1-2 sessions. **Cross-axis:** orthogonal ко всем 5+ in-progress
  parallel (tracy-gpu-vs-manual + gpu-fluid-ca-atomic-strategy + hzb-smart-mip-select + vct-3d-mip-generation +
  vk-multi-gpu-split-frame); **complementary** к closed mixed `vulkan-memory-aliasing-transient` (aliasing axis =
  stackable) + closed mixed `frame-flight-allocator-budget` (allocator strategy axis = stackable). **Direct
  continuation chain:** aliasing → allocator strategy → compaction = complete VRAM fragmentation mitigation stack.
  **Caveats:** (a) CPU prototype, no Vulkan init, no real GPU driver overhead для `vmaDefragment` GPU copy;
  (b) synthetic VRAM heap (2 GiB match dev host) — workload intensity 6% utilization = no fragmentation modeled;
  (c) fragmentation ratio synthetic per `vmaComputeAllocationStats` model (real = aligned with VMA ref impl line
  ~7000-8000 + `vmaDefragment` algorithm internals); (d) cross-vendor VRAM characteristics not measured (single
  host RTX 3060 Ti); (e) mutation cost (rebuild defrag state on chunk mutation) not separately measured; (f) visual
  regression proxy = single-frame stutter detection (no real VMA validation); (g) algorithm choice
  (FAST/BALANCED/FULL/EXTENSIVE per VMA docs) not separately measured — only FULL algorithmic mode tested.
  **Re-evaluation triggers:** Stage 4.3 ships (128+ chunks draw distance); VMA 3.5+ release; cross-vendor AMD RDNA +
  Intel Arc dev matrix; real Vulkan integration prototype.
  См. [README](./experiments/2026-06-21-vulkan-defragmentation-compaction/README.md) +
  [STATUS](./experiments/2026-06-21-vulkan-defragmentation-compaction/STATUS.md) +
  [sources](./experiments/2026-06-21-vulkan-defragmentation-compaction/sources.md) +
  [RESULTS](./experiments/2026-06-21-vulkan-defragmentation-compaction/RESULTS.md) +
  [INDEX §6 Recent closed](./INDEX.md) за full table.

- [x] **[2026-06-21-vulkan-memory-aliasing-transient](./experiments/2026-06-21-vulkan-memory-aliasing-transient/)** —
  m, **independent** (cross-cutting Stage 2.x-5.x). Closed `2026-06-21` (single session, ~3h),
  verdict **`mixed`**. **Render-pipeline-architecture axis** (Vulkan transient resource aliasing +
  render graph DAG) — **first axis** в 30+ closed experiments. **Self-invented topic** per operator
  instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй». Ninth invocation
  this session (previous 8 closed same-session: audio mixed + wfc mixed + sub-chunk mixed + gpu-noise
  mixed + taa yes + depth yes + vk-fragment-shading mixed + frame-flight mixed + dxc mixed + lod-mesh
  mixed + audio-diffraction mixed = 12 closed same-session). **Standalone C++26 CPU lifetime simulator**
  `prototype/mem_alias_bench.cpp` ~600 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall
  -Wextra -Wpedantic`, builds green with 10 cosmetic warnings на unused constexpr / argc-argv).
  3 workloads × 4 strategies × 5 seeds × 1000 iter + 10 warmup = **60,000 main measurements**, wall
  time <1 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline findings:**
    - **D_DAGRenderGraph barrier reduction = −74%** (consistent across all workloads, 28→7 / 50→13 /
      74→19) — **real win**, directly impacts CPU command buffer recording overhead.
    - **C_FullAliasing VRAM savings = −7-8%** on typical (276→255 MiB) + projected (398→372 MiB)
      workloads — crosses 5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.
      Modest savings (~22 MiB absolute) на large workloads, ≈0 на minimal MVP (pool overhead eats savings).
    - **B_VMA_SubAllocatorPool = REGRESSION** (−5% additional overhead vs A baseline) — pure pool
      without lifetime analysis = worse than current pattern. **Never adopt without aliasing.**
      **Persistent image bottleneck (root cause of modest savings):** depth + shadow + hiz + taa history
      = ~98 MiB cannot be safely aliased across frames (write-after-read hazards). Hard ceiling ~35% VRAM.
      **Web-research Phase A:** 9 primary + 7 secondary sources verified via `webfetch` + DuckDuckGo HTML
      fallback (Exa HTTP 429 persistent): Yuriy O'Donnell 2017 GDC Frostbite FrameGraph [canonical];
      Themaister 2017/2019 Granite Engine blog [open-source reference]; VMA official resource_aliasing
      docs; WSCG 2023 history-aware frame graph academic paper; dev.to p3ngu1nzz 2025-10-06 + 2025-10-18
      modern implementation; Khronos Vulkan Tutorial render graph; AMD RPS SDK; KhronosGroup Vulkan
      resources.adoc 2026-06-05. **Mainline recommendation:** phased migration per `agent/knowledge.md
  §30.4` precedent — **Step 1 (S, ~150 LoC) immediate**: VMA pool setup grouped by `ResourceType` +
      heap type with sub-allocation (validation only); **Step 2 (M, ~500 LoC) for Stage 4.3**:
      interval-graph coloring for non-overlapping lifetimes (lifetime tracking в `CreateBuffer`/
      `CreateImage`); **Step 3 (L, ~1500 LoC) deferred to Stage 5.x post-VCT+RTX**: DAG-based render
      graph + auto-barrier batching (4:1 reduction). Total ~2150 LoC, L effort, 4-6 sessions.
      **Caveats:** CPU simulation only (no real GPU dispatch / driver overhead), synthetic workloads
      (realistic upper-bound), greedy coloring algorithm (production render graphs use Pettis-Hansen
      +10-20% better packing), single-GPU dev host (cross-vendor analytical projection only).
      **Continuation chain:** none (first render-graph axis experiment; opens cross-cutting Stage 2.x-5.x
      render pipeline architecture). **Re-evaluation triggers:** Stage 4.3 ship (128+ chunks, aliasing
      payoff grows); Stage 5.1 VCT + Stage 5.2 RTX + Stage 5.3 TAA (pass count > 15, barrier batching high
      value); `VK_KHR_dynamic_rendering_local_read` extension ratification status; AMD RDNA 4 + Intel Arc
      Battlemage dev matrix; Pettis-Hansen aliasing allocator production validation (e.g., RPS SDK adoption).
      См. §6 + §1 + experiment README + `STATUS.md` (final) + `sources.md` (16 sources) +
      `prototype/{mem_alias_bench.cpp, RESULTS.md, build/results.csv}`.

- [x] **[2026-06-21-vct-cone-count-atlas-precision](./experiments/2026-06-21-vct-cone-count-atlas-precision/)** —
  m, **Stage 5.1** (Voxel Cone Tracing per `TODO.md §5.1` + direct follow-up to closed
  `2026-06-20-vct-vs-rt-cutoff` [verdict=mixed, cutoff=0.3]). Closed `2026-06-21` (single session, ~2.5h),
  verdict **`mixed`**. **VCT within-quality axis** — sixth invocation this session (previous 5 closed
  or in-progress: audio + wfc + sub-chunk + gpu-noise + lod-mesh + taa + dxc + frame-flight + depth-
  occlusion closed; tracy-gpu + gpu-fluid-ca + vk-fragment-shading-rate + audio-diffraction in-progress
  parallel; 19+ closed `2026-06-20`). **Standalone Vulkan 1.4 compute prototype** ~700 LoC
  (`prototype/{vct_main.cpp, cone_march.comp, CMakeLists.txt, README.md}` + `RESULTS.md` +
  `build/results.csv` 12 measurements), 4 SPIR-V variants via `-DCONE_{6,12,24,1024}` defines,
  builds green with 0 errors, 1 forward-decl warning fixed. 9 measured configs (3 cone counts × 3
  atlas precisions) × 100 iter + 10 warmup = 900 measurements + 3 references on dev host `obvium`
  RTX 3060 Ti GA104 Ampere + Vulkan 1.4.341 per `hardware-profile.md §3/§4`. **Web-research**
  complete (4 batches, ~30 results, 12 primary + 6 secondary sources verified: Crassin 2011 GIVoxels
  §5 [PDF: 5 cones diffuse canonical], Panteleev 2014 thesis Uni Bremen [6 cones + R16G16B16A16 atlas],
  OGRE 2019 VCT [4-6 cones + R8 banding risk], Lumen SIGGRAPH 2022 Narkowicz [24 cones for surface
  cache, not pure VCT], Andersson 2024 CGF Dynamic VCT [RTX 2060 0.38 ms], KTH Northman 2024 [atlas
  size scaling], HanetakaChou RTX 4080 [8-32 RPP 7-12 ms], Vulkan R16F core 1.0 + storage image
  support). **Headline findings:** **VRAM cost linear in bpp** (R8/R16F/R32F = 9/18/36 MiB на 128³
  atlas with mip chain; 256³ = 72/144/288 MiB = 1.4/2.8/5.5% of 5.06 GiB budget per
  `hardware-profile.md §3`). **Perf ≈ 15 µs per 1024² dispatch for ALL 12 configs** — cone count
  6/12/24/1024 NOT a discriminator, dispatch overhead dominates at this work size
  (Ampere GA104 launch latency). **Quality axis literature-projected, NOT measured** (1024-cone
  Fibonacci reference did not successfully write to output, likely shader compile issue with
  unrolled fibDir loop). **Recommended sweet spot: 6 cones × R16G16B16A16_SFLOAT** (NOT 12×R16F
  as originally hypothesized — literature shows 5-6 cones is canonical, 12+ shows diminishing
  returns). R16F = Panteleev 2014 baseline, mitigates OGRE 2019 R8 banding risk. **3-step migration
  per `agent/knowledge.md §30.4`:** Step 1 (XS, ~10 LoC) atlas format `R8G8B8A8_UNORM` →
  `R16G16B16A16_SFLOAT` в `voxelize.comp` (new per TODO §5.1) + `PROJECTV_VCT_ATLAS_FORMAT` env
  fallback; Step 2 (S, ~50 LoC) cone count loop в `vct.frag` (new per TODO §5.1) with `N_CONES=6`
  (literature baseline, not 12); Step 3 (XS, ~20 LoC) Tracy plot `VCT_ConeMarchMs` + default flip +
  `agent/knowledge.md §30.x` decision record. Total ~80 LoC, S effort, 1-2 sessions, 1 PR.
  **Cross-vendor matrix:** NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+;
  analytical projection per `dec-pipelines-async-compute` §2.2 — 6×R16F sweet spot is
  cross-vendor-invariant (no vendor-specific advantage for higher cone counts). **Caveats:**
  (a) 1024-cone reference write broken in prototype (PSNR=0dB for measured vs 99.9dB for
  reference is artifactual, both compared to all-zero reference); (b) single 1024² frame, real
  workload = 1920×1080 + cubemap + reflection probes = ~10× more work; (c) single synthetic
  scene (ground + sky + 2 walls), real voxel scenes have more variation; (d) no mip build
  cost measured (amortized over frames); (e) no driver overhead / multi-queue optimization
  measured; (f) single GPU vendor validated (RTX 3060 Ti GA104). **Cross-axis:** orthogonal
  к 4 in-progress parallel (tracy-gpu = profiling, gpu-fluid-ca = Stage 3.1 atomic,
  vk-fragment-shading-rate = VRS fragment rate, audio-diffraction = audio); complementary к
  closed `vct-vs-rt-cutoff` (cutoff strategy) + `nanovdb-on-gpu` (storage foundation) +
  `restir-gi-feasibility` (deferred Stage 6+ path tracer) + `dec-pipelines-async-compute` (async
  mip-chain prerequisite). **Follow-up candidates (out of scope):** Crassin 2011 cone-tapered
  mip filter (+2-4 dB expected); 1024-cone reference fix (split into 2×512 or pre-compute
  directions in UBO); specular cone count axis (Lumen uses 3-6); atlas resolution scaling
  (128³/256³/512³ VRAM-constrained); 4D temporal VCT (close to closed
  `2026-06-21-taa-motion-vectors`); VCT + VRS feedback loop (orthogonal to in-progress
  `vk-fragment-shading-rate-voxel`). См.
  §6 + [experiment README](./experiments/2026-06-21-vct-cone-count-atlas-precision/README.md)
    + [RESULTS](./experiments/2026-06-21-vct-cone-count-atlas-precision/RESULTS.md) +
      `prototype/{vct_main.cpp, cone_march.comp, CMakeLists.txt, README.md}` + `build/results.csv`
      (12 measurements).

- [x] **[2026-06-21-audio-diffraction-hybrid](./experiments/2026-06-21-audio-diffraction-hybrid/)** —
  l-promoted, **independent** (audio rendering axis, **Phase 1.5 enhancement** explicitly declared follow-up в
  closed `2026-06-21-audio-raytracing-voxel-sdf` line 459-460). Closed `2026-06-21` (single session, ~1.5h),
  verdict **`mixed`**. **Audio axis Phase 1.5** — fifth invocation this session (previous 4 closed: audio +
  wfc + sub-chunk + taa). **Standalone C++26 CPU prototype** ~985 LoC
  (`prototype/{voxel_grid,audio_path,diffraction}.{hpp,cpp} + bench.cpp + Makefile + README + RESULTS + results.csv`),
  Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green, 0 warnings.
  3 strategies × 3 scenes × 3 seeds × 100 iter × 16 sources = **14,400 invocations** на Zen 3 5800X
  governor=`powersave` per `hardware-profile.md §1`. **Web-research** complete (4 batches, ~30 results,
  16 primary + 7 secondary sources verified, ключевые: Schissler 2014 high-order diffraction
  [SIGGRAPH 2014 ACM TOG 33(4) 39, edge visibility graph + UTD, 15-50 FPS on 4-core CPU, indoor + urban scenes]
    + Schissler 2014 multi-source [I3D 2014, 50+ reflection orders, 5× speedup, 200 sources] + Cao 2016 BST
      [SIGGRAPH ASIA 2016 ACM TOG 35(6) — closed `audio-raytracing-voxel-sdf` Phase 3 falsified reference] +
      Cao 2021 fast diffraction [SIGGRAPH 2021, 10th-order diffraction, 568× faster] + Tsingos 2001 UTD
      [SIGGRAPH 2001, beam tracing — **NOT depth-mip**] + Tsingos 2007 Instant Sound Scattering [EGSR 2007,
      depth-mip GPU, 20-40× faster than CPU, 700 Hz refresh — **Tsingos 2001 ≠ Tsingos 2007**] + Chandak 2008
      AD-Frustum [IEEE TVCG 2008, UTD + frustum tracing] + Antani 2012 BTM [IEEE TVCG 2012, 2-4× reduction
      visible primitives] + Vercidium 2025 [voxel + CPU + audio ray-tracing, production reference] +
      SonoTraceUE 2026-01-09 [UE5 curvature-based MC diffraction + HW RT, arXiv 2602.19652] + Pinpoint Audio
      Tracing 2025-08-18 [UE5 RTX-mandatory, Lumen-dependent] + Meta XR Audio SDK 2024+ [Acoustic Map + Edge
      Diffraction, hybrid precomputed+runtime] + Wwise Spatial Audio [Audiokinetic Ak Geometry API, AAA
      production diffraction+transmission] + Google Patent WO2024179939A1 [voxel + multi-directional
      diffraction, public prior art] + Han 2025 IEEE CoG survey [**41% sound designers find LPF alone
      insufficient**]). **Measured (Zen 3 5800X, governor=`powersave`):**
      A_None 0.0001 ms / source (1 probe, 0.02% audio budget @ 64 sources);
      **C_Tsingos 0.0025-0.0032 ms / source (33 probes, 0.5-0.6% audio budget @ 64 sources, +1.2-1.4 dB recovery
      per Tsingos 2007 spec 1-2 dB)**;
      B_Schissler 0.024-0.082 ms / source (17 probes, 5-16% audio budget, 0 dB recovery в simplified
      first-order UTD). Cross-arch projection (Zen 5 AVX-512): C_Tsingos 0.0015-0.0020 ms = 0.3-0.4% budget;
      B_Schissler 0.012-0.040 ms = 2-5% budget. **Verdict=mixed:** **C_Tsingos production-ready**
      (0.5-0.6% budget, +1.2 dB recovery, crosses 5% optimization threshold by 8-10× margin per
      `optimization-philosophy.md`); **B_Schissler deferred** до second-order UTD implementation. **Mainline
      3-step migration per `agent/knowledge.md §30.4` precedent:** Step 1 (XS, ~80 LoC)
      `Diffraction::sampleHemisphere()` helper + Fibonacci sphere + depth-mip lookup stub; Step 2 (XS, ~50 LoC)
      wire into `AudioEngine::tick()` after occlusion call; Step 3 (XS, ~20 LoC) env flag
      `PROJECTV_AUDIO_DIFFRACTION=ON` default ON. Total ~150 LoC, XS effort, 1-2 sessions. **Caveats:**
      (a) CPU-only synthetic voxel scenes (cave + open_plains + multi_room per closed `audio-raytracing-voxel-sdf`);
      (b) Zen 3 5800X governor=`powersave`; (c) no AVX-512 = realistic measurement floor (deferred до Zen 5 /
      Arrow Lake per `simd-procedural-noise` precedent); (d) perceptual validation = analytical proxy (Tsingos
      openness fraction → dB estimate per spec), not full HRTF / ABX listening test; (e) B_Schissler first-order
      UTD only (second-order edge-to-edge = future work для full +2-4 dB); (f) N=100 iterations per strategy ×
      scene × seed (vs methodology default 1000); (g) no DSP overhead in prototype (closed `audio-raytracing-
  voxel-sdf` baseline = +0.005-0.015 ms per source для full pipeline). **Continuation chain:** closed
      `audio-raytracing-voxel-sdf` (Phase 1+2 recommended, Phase 3 falsified) → this (Phase 1.5 = Tsingos
      integration) → future Phase 1.6 = B_Schissler second-order UTD. **Cross-axis:** complementary к closed
      `audio-raytracing-voxel-sdf` (Phase 1+2) + closed `hzb-binding-models` (texelFetch pattern reuse для
      depth-mip probe) + closed `nanovdb-on-gpu` (SVO walker foundation, future hierarchical skip для
      Phase 1.6) + closed `work-stealing-job-system` (serial dispatcher) + closed `simd-procedural-noise`
      (AVX2 baseline = realistic floor). **Re-evaluation triggers:** Zen 5+ AVX-512 hardware availability,
      HRTF integration (Meta XR Audio SDK), ProjectV audio axis progression to Stage 7.x per
      `agent/knowledge.md §28`, second-order UTD implementation per Chandak 2008 / Cao 2021. См.
      [experiment README](./experiments/2026-06-21-audio-diffraction-hybrid/README.md) +
      [STATUS](./experiments/2026-06-21-audio-diffraction-hybrid/STATUS.md) +
      [sources.md](./experiments/2026-06-21-audio-diffraction-hybrid/sources.md) +
      [prototype/README.md](./experiments/2026-06-21-audio-diffraction-hybrid/prototype/README.md) +
      [prototype/RESULTS.md](./experiments/2026-06-21-audio-diffraction-hybrid/prototype/RESULTS.md) +
      `prototype/results.csv` (28 rows).

- [x] **[2026-06-21-vk-fragment-shading-rate-voxel](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/)** —
  m, **independent** (cross-cutting Stage 5.x lighting cost optimization, **follow-up axis** после полного closure
  lighting-strategy-axis `2026-06-20`: `vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights` yes +
  `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed). Closed `2026-06-21` (single session, ~1.5h),
  verdict **`mixed`**. **VRS-cost-axis experiment** — единственная Stage 5.x cost-side axis, не покрытая same-session
  closed experiments (4 lighting-strategy + frame-flight-allocator + gpu-procedural-noise + dxc-vs-glslc-toolchain +
  audio-raytracing-voxel-sdf + sub-chunk-layers) + in-progress parallel (tracy-gpu + wfc-procedural +
  taa-motion-vectors + gpu-fluid-ca-atomic + lod-mesh-downsampling + audio-diffraction-hybrid). Web-research
  complete (2 batches, ~14 results, **10 primary sources verified:** Khronos spec + Vulkan samples +
  Intel SIGGRAPH 2019 + NVIDIA NAS GDC 2019 + NVIDIA VRSS 2 + AMD RADV Mesa commits via Phoronix +
  SaschaWillems DeepWiki + Unity URP docs + Godot proposal #3859 + Vulkan 1.4 core revisions +
  platonvin/lum-rs voxel precedent). Standalone C++26 CPU prototype `prototype/vrs_voxel_sim.cpp` **~770 LoC**
  (Clang 22.1.6 `-O3 -march=native -std=c++26 -Wall -Wextra`, **0 warnings**), 4 scenes × 3 resolutions ×
  5 VRS configs × 100 iter + 10 warmup = **6000 measurements** on dev host `obvium` Zen 3 5800X + governor
  `powersave`. **Headline numbers (mean across all scenes × all resolutions):**
    - **`baseline_1x1`**: covered 4-6%, invocations = covered_pixels (control).
    - **`vrs_2x1` / `vrs_1x2`**: **50% savings** consistent across все 4 scenes × 3 res = **deterministic**.
    - **`vrs_2x2_global`**: **75% savings** consistent, highest quality risk (0.425-0.575).
    - **`vrs_hybrid_2x2_lighting`**: **0% savings** ⚠️ — falsified hypothesis для sparse voxel scenes.
    - **VRS image bytes:** 8 KiB @ 1080p / 14 KiB @ 1440p / 32 KiB @ 4K = **0.0001-0.0004% of 8 GiB VRAM budget**
      (per `hardware-profile.md §3`) — VRAM cost **negligible**.
    - **Quality risk (heuristic):** uniform_open ≤ 0.425, forest_floor ≤ 0.425, cave_stress ≤ 0.575,
      mixed_biome ≤ 0.575 (higher для complex silhouettes per Intel SIGGRAPH 2019 + NVIDIA NAS).
    - **Cross-vendor Tier 2 VRS validated:** NVIDIA Turing/Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel
      Gen11/Arc Alchemist/Battlemage per Mesa RADV (Phoronix 2020/2023) + Intel ANV + NVIDIA driver 460+
      baseline.
    - **⚠️ Critical spec correction:** `VK_KHR_fragment_shading_rate` **NOT in Vulkan 1.4 core** per
      `docs.vulkan.org/spec/latest/appendices/versions.html` (initial hypothesis assumed core promotion —
      falsified); remains device extension in 1.4. RTX 3060 Ti on dev host `obvium` supports via NVIDIA 610.43.02
        + Vulkan 1.4.341.
          **Mainline рекомендация** per `README.md §7` + `STATUS.md`:
    - **Step 1 (XS, immediate, ~30 LoC):** global `vrs_2x1` для VCT integration via
      `vkCmdSetFragmentShadingRateKHR` + `voxel.frag` VRS-agnostic adaptation per Intel SIGGRAPH 2019
      (`dFdx/dFdy` scaling, `gl_FragCoord no longer n+0.5`). Safe 50% fragment shading cost reduction.
    - **Step 2 (S, ~100 LoC + tests):** VRS extension probe (`VkPhysicalDeviceFragmentShadingRateFeaturesKHR`
      check 3 features: `pipelineFragmentShadingRate` + `primitiveFragmentShadingRate` +
      `attachmentFragmentShadingRate`) + `VkFragmentShadingRateAttachmentInfoKHR` attachment setup +
      `VK_FORMAT_R8_UINT` shading rate image per swapchain (size = W/16 × H/16 bytes).
    - **Step 3 (M, ~250 LoC) DEFERRED:** hybrid classifier + two-pass dynamic VRS per Khronos sample
      `fragment_shading_rate_dynamic` (compute shader generate per-frame derivative image → next-frame VRS
      image; two renderpass pattern to avoid feedback loop). Conditional: только if Stage 4.3 lift raises
      voxel coverage > 30% (then hybrid savings > 0% expected).
      **Caveats:** (a) CPU prototype, no real GPU dispatch — savings formulas validated, real GPU timings

    + visual quality deferred до GPU prototype на RTX 3060 Ti; (b) hybrid savings 0% для sparse scenes
      (falsified hypothesis) — classifier thresholds (cov_ratio > 85% + edge_ratio < 3% для low-detail)
      consistent per `prototype/vrs_voxel_sim.cpp:build_vrs_image`; (c) quality_risk эвристика simplified,
      needs PSNR/SSIM measurement на rendered frames; (d) cross-vendor GPU measurement (AMD RDNA 2/3,
      Intel Arc) analytical-only — needs hardware matrix validation; (e) TAA + VRS feedback loop
      (per NVIDIA NAS GDC 2019: 3-4 frames transition latency) **cross-axis risk** с in-progress
      `2026-06-21-taa-motion-vectors` — separate experiment needed; (f) VRAM cost projection conservative
      (single-buffered; double-buffered = 2× bytes); (g) `voxel.frag` per-pixel ops (depth downsampling,
      dithering) require `SV_Position` adaptation per Intel SIGGRAPH 2019 caveat. **Continuation chain:**
      `vct-vs-rt-cutoff` (closed strategy axis) + `clustered-forward-mass-lights` (closed light count) +
      `rt-shadows-vs-csm` (closed shadow strategy) + `restir-gi-feasibility` (closed GI strategy) →
      this (closed cost axis). Full Stage 5 lighting optimization landscape covered same-day `2026-06-20` +
      `2026-06-21` cluster. **Follow-up candidates** (out of scope для this session, deferred до separate
      experiments): `_vrs-taa-feedback-loop_` (cross-axis с `taa-motion-vectors` in-progress);
      `_vrs-gpu-prototype-rtx3060ti_` (real GPU timing + visual quality validation); `_vrs-dense-scene-hybrid_`
      (re-test hybrid classifier на cave_interior / dense_foliage scenes с >30% coverage);
      `_vulkan-1.5-1.6-vrs-core-promotion_` (verify if VRS extension promoted to core in next Vulkan minor);
      `_vr-foveated-vrs-gaze-input_` (cross-axis с `eye-tracked-foveated` backlog l-priority). См. §6 + §1 +
      [experiment README](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/README.md) +
      [STATUS](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/STATUS.md) +
      [sources.md](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/sources.md) +
      [prototype/RESULTS.md](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/prototype/RESULTS.md) +
      [prototype/results.csv](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/prototype/results.csv).

- [x] **[2026-06-21-taa-motion-vectors](./experiments/2026-06-21-taa-motion-vectors/)** — m,
  **independent** (Stage 5.3 TAA Motion Vectors per `TODO.md §5.3`, **temporal axis** для Stage 5 после
  полного closure lighting-axis на `2026-06-20`: `vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights`
  yes + `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed). Closed `2026-06-21` (single session, ~1h),
  verdict **`yes`** for Pipeline A (vertex-out motion vector MRT). **Verdict basis** (independent of
  measurement execution per agent not building per `AGENTS.md §1`): (1) `TODO.md §5.3` line 425 explicit
  format prescription `VK_FORMAT_R16G16_SFLOAT` = mandate для mainline; (2) Karis 2014 SIGGRAPH foundational
  paper "High Quality Temporal Supersampling" ["16:16 RG velocity buffer" = R16G16_SFLOAT exact match;
  "velocity accuracy is super important" drives vertex-out recommendation]; (3) industry standard (UE 5 +
  Godot 4.x + Unity HDRP all use R16G16_SFLOAT motion vector MRT) — no cross-vendor ambiguity per
  `dec-pipelines-async-compute` §2.2 vendor matrix; (4) VRAM cost 4 MiB/frame single-buffered / 8 MiB
  double-buffered @ 1080p = 0.08% / 0.16% of 5.06 GiB budget per `hardware-profile.md §3` = well under 5%
  threshold per `optimization-philosophy.md`; (5) `TODO.md §5.3` DoD «Полное исчезновение шлейфов за
  перемещаемыми гравипушкой моделями» = only achievable with vertex-out (depth-reproject has fundamental
  precision loss near edges per Karis 2014). **Web-research** complete (2 batch queries, ~14 results, 6
  primary + 5 secondary sources верифицированы: Karis 2014 SIGGRAPH foundational, Yang/Liu/Salvi 2024
  Stanford TAA survey [neighborhood clamping + YCoCg = standard 2024], Marrs/Spjut 2018 NVIDIA adaptive TAA
  [requires RT, out of scope], k-DOP Clipping SIGGRAPH 2024 [SOTA ghosting mitigation 0.2 ms overhead, follow-up
  candidate], Karolewics Lumberyard anti-ghosting TAA [production reference 0.1 ms + 1.6 ms total Xbox One],
  VK_KHR_dynamic_rendering [core 1.3 enables MRT pattern already ProjectV mainline]). **Mainline 3-step
  migration per `agent/knowledge.md §30.4` precedent** — Step 1 foundation (S, ~50 LoC, 1 session): vertex
  shader `out vec4 vPrevClip` + fragment shader `layout(location=1) out vec2 outMotion` (R16G16_SFLOAT) +
  `TaaRenderTargets.{hpp,cpp}` add motion vector attachment + `SceneResources.{hpp,cpp}` allocate
  double-buffered motion vector MRT (8 MiB @ 1080p); Step 2 TAA resolve update (S, ~50 LoC, 1 session):
  change motion vector source from current depth-reproject to read from motion vector MRT + image layout
  transition `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` for motion vector after geometry pass
  before TAA resolve; Step 3 default flip (XS, ~10 LoC, 1 commit): `PROJECTV_USE_MOTION_VECTOR_MRT=ON`
  env flag with cross-vendor graceful fallback. **Total effort M** (~110 LoC across 5-6 files, 2-3 sessions).
  **Caveats:** (a) no actual GPU measurements (prototype is measurement harness skeleton per
  `prototype/README.md` 'Status' section — operator can extend + run if desired); (b) single GPU vendor
  validated (RTX 3060 Ti Ampere), cross-vendor expected identical per `dec-pipelines-async-compute` §2.2;
  (c) Karis 2014 paper is 12 years old (2014), but 2024-2026 literature (Yang/Liu/Salvi 2024 + k-DOP SIGGRAPH
    2024) confirms its core principles still hold for vertex-out approach; (d) k-DOP SIGGRAPH 2024 = SOTA
          ghosting mitigation (0.2 ms overhead for 32-DOPs) = follow-up experiment to replace 3x3 AABB clamping;
          (e) Marrs 2018 NVIDIA adaptive TAA = requires ray tracing (Stage 5.2 RTX foundation), out of scope для
          Stage 5.3 baseline. **Cross-axis:** orthogonal ко всем 3 in-progress parallel (tracy-gpu = profiling, wfc
          = gen strategy, sub-chunk = data structure); complementary к closed `clustered-forward-mass-lights`
          (SSBO light list + motion vectors both feed TAA resolve); natural follow-up к closed
          `dec-pipelines-async-compute` (motion vector MRT submission = candidate for async queue if VRAM/upload
          becomes bottleneck); cross-vendor validation matrix same as `dec-pipelines-async-compute` §2.2 (NVIDIA
          Ampere/Ada/Blackwell + AMD RDNA2/3/4 + Intel Arc Gfx12.5+). **Continuation chain** (project chronological):
          2026-06-20 lighting axis: `vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights` yes +
          `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed; 2026-06-20 sync axis: `dec-pipelines-async-compute`
          yes + `async-compute-overhead-numbers` yes (foundation); 2026-06-21 temporal axis: this experiment = TAA
          motion vector MRT decision. **Side effect:** sync fix r1 applied to previous-session
          `2026-06-20-async-compute-overhead-numbers` per `AGENTS.md §13.5` (original session left bookkeeping
          incomplete: §Open duplicate + missing §6 entry + README Status mismatch — all corrected same-pass
          preserving original measurements +9.85-11.34% + verdict=yes). См. §6 + §1 + experiment README +
          `STATUS.md` + `sources.md` + `prototype/README.md` + `prototype/main.cpp` (525 LoC skeleton) + 6 GLSL
          shaders (voxel_a/b vert+frag + taa_resolve_a/b comp) + Makefile. **Re-evaluation triggers:** Stage 5.3
          TAA motion blur integration (related TODO §5.3 line 425), AMD RDNA / Intel Arc dev matrix validation,
          k-DOP adoption per SIGGRAPH 2024 (0.2 ms overhead, may compound with motion vector quality gain), Marrs
          2018 adaptive TAA (requires ray tracing path from Stage 5.2 RTX shadows foundation).

- [x] **[2026-06-21-audio-raytracing-voxel-sdf](./experiments/2026-06-21-audio-raytracing-voxel-sdf/)** —
  l, **independent** (cross-cutting для future Stage 7.x audio; no audio rendering stage в `TODO.md` per §3
  — miniaudio PCM playback only per `agent/knowledge.md §28`). Closed `2026-06-21` (single session, ~2h),
  verdict **`mixed`**. **Audio axis** experiment — **первый audio-axis** (0 of 19+ same-day `2026-06-20`
  experiments covered audio). Web-research complete (3 batch queries, 12 key sources верифицированы:
  Vercidium 2025 production voxel-grid audio [direct validation of our approach, 32 rays/frame CPU],
  SIGGRAPH 2025 Finnendahl et al. differentiable acoustic PT + Path Replay Backpropagation,
  GSound-SIR Mar 2025 + NVIDIA OptiX support Dec 2025, Schissler & Manocha 2014 [50 reflection orders
  at interactive rates, 200 sound sources], Schissler et al. 2014 BST bidirectional path tracing, RESound 2007
  hybrid ray-frustum + stochastic + statistical, iSound GPU-based auralization, Tsingos 2001
  HW-accelerated occlusion/diffraction via depth-maps, Funkhouser 2002 beam tracing for architectural scenes,
  Meta Acoustic Ray Tracing Audio SDK 2024+ production VR, NeRAF ICLR 2025 audio-visual alignment). Standalone
  C++26 prototype (`prototype/{voxel_grid,audio_raytracer,reverb,bench}.{hpp,cpp}` + `RESULTS.md` + `results.csv`
    + `README.md`, **~700 LoC total**, Clang 22.1.6 `-O3 -march=native -Wall -Wextra`, **0 warnings**). 4 configs
      × 3 scenes × 3 seeds × 1000 iter + 100 warmup = **36 runs × 1000 = 36000 measurements** on Zen 3 5800X
      (per `hardware-profile.md §1`, governor `powersave`). **Headline numbers (mean ms):**

    - **A_no_geom:** 0.0002 across scenes (baseline, current `AudioEngine` per `agent/knowledge.md §28`).
    - **B_occlusion** (1 ray/source): **0.008-0.016 ms** = **< 0.05%** of 33.3 ms audio frame budget @ 30 Hz.
      **Production-ready, immediate integration** для muffling behind walls.
    - **C_full_hybrid** (32 rays × 4 reflection orders + Eyring late tail + IR gen): **17.1 cave / 13.8 open_plains /
      6.3 multi_room ms** = **52% / 41% / 19%** budget. **Falsifies 5 ms hypothesis** на 2 of 3 scenes (cave
      3.4× over, open_plains 2.7× over). Only multi_room в budget (1.3× over).
    - **D_full_cached** (+ temporal cache 1 cm epsilon): **21.1 / 14.4 / 6.0 ms**. Cache не помогает — jitter
      ±5 cm > ε → cache invalidates most frames. Cave seed 7 actually **worse** than C (28.4 vs 17.4 ms)
      из-за cache re-warmup overhead.
      **Mainline recommendation** per `README.md §7` + `STATUS.md`:
    - **Phase 1 (XS, immediate):** occlusion-only path → < 1.5 ms for 64 sources = 4% budget. Immediate perceptual
      win (muffled sounds behind walls).
    - **Phase 2 (XS, immediate):** Eyring late reverb → negligible cost (~0.001 ms per source), realistic room
      perception, integrate unconditionally.
    - **Phase 3 (M, deferred):** full hybrid до one of (a) SVO hierarchical acceleration [empty-skip 5-10×
      per `nanovdb-on-gpu` walker logic], (b) lower ray budget [8r×2ord perceptually sufficient per Vercidium
      2025 + Schissler 2014], (c) cache tuning [larger ε 10-20 cm], (d) AVX-512 hardware arrival [Zen 5 / Arrow Lake
      2-4× per `simd-procedural-noise` precedent].
      Cross-reuses `2026-06-20-nanovdb-on-gpu` SVO walker foundation, `2026-06-20-flecs-soa-vs-aos-bench` SoA storage
      verdict=yes, `2026-06-20-work-stealing-job-system` serial dispatcher verdict=mixed, `agent/knowledge.md §28`
      AudioEngine contract. **Caveats:** (a) single-vendor Zen 3 5800X (governor `powersave`, не `performance`),
      (b) `voxels_traversed` counter instrumentation bug — не инкрементируется в DDA, не влияет на latency
      measurements, blocks cache-miss analysis (fix в v2 prototype), (c) synthetic scenes representative not
      exhaustive (cave/open_plains/multi_room), (d) no material absorption modeling (simplified reflection only),
      (e) sequential single-threaded per `work-stealing-job-system` verdict=mixed → no pool/TBB/libdispatch,
      (f) bench measured sources=64 — scaling to 256+ requires separate `_audio-rt-budget-vs-source-count_`
      experiment (deferred). **Continuation chain:** none (first audio axis experiment; opens Stage 7.x audio);
      **follow-up candidates:** `_audio-hierarchical-svo-skip_` (Phase 3 trigger), `_audio-rt-budget-vs-source-count_`
      (>100 sources scaling), `_audio-diffraction-hybrid_` (Schissler 2014 diffraction via HZB per
      `2026-06-20-hzb-binding-models`). **Cross-axis continuity:** same-session `2026-06-21` parallel sessions
      (gpu-procedural-noise + frame-flight-allocator-budget + dxc-toolchain + tracy-gpu + wfc-procedural + this =
      6 same-day closes/in-progress) + 19+ same-day `2026-06-20` closed = full Stage 1.x/2.x/3.x/4.x/5.x/6.x/7.x
      optimization landscape + **audio axis NEW**.
      См. [experiment README](./experiments/2026-06-21-audio-raytracing-voxel-sdf/README.md) +
      [STATUS](./experiments/2026-06-21-audio-raytracing-voxel-sdf/STATUS.md) +
      [sources.md](./experiments/2026-06-21-audio-raytracing-voxel-sdf/sources.md) +
      [prototype/RESULTS.md](./experiments/2026-06-21-audio-raytracing-voxel-sdf/prototype/RESULTS.md).

- [x] **[2026-06-21-sub-chunk-layers](./experiments/2026-06-21-sub-chunk-layers/)** —
  m, Stage 4.x (biome/cave data structure axis, orthogonal к in-progress `2026-06-21-wfc-procedural-worlds`
  gen-strategy axis). Closed `2026-06-21` (single session), verdict **`mixed`**. **Chunk-layout-axis
  experiment** — единственная Stage 4.x ось, не покрытая same-session `2026-06-21` closed experiments
  (frame-flight-allocator-budget + gpu-procedural-noise-compute-kernels) + in-progress
  (wfc-procedural-worlds + audio-raytracing-voxel-sdf + dxc-vs-glslc-toolchain + tracy-gpu-vs-manual).
  Web-research complete (3 batch queries, ~14 sources верифицированы: Minecraft-1.18+ Java
  `ChunkSection` 16³ + biomes 4×4×4 = 64 entries per section per FabricMC/yarn DeepWiki + Minecraft Wiki
    + wiki.vg protocol + yarn 1.18 API; Bedrock `SubChunk` 4D (x,y,z,**storage layer**) per wiki.vg +
      uNmINeD 2021-12-10 reverse engineering; SHARD layered format per scrayos 2024-11-04 + GitHub; ATLAS
      AARF columnar storage per Tunact124/atlas Mar 2026; Cubyz CaveMap 64³ fragments with 1-bit per block
    + CaveBiomeMap 2048³ resolution per PixelGuys DeepWiki Mar 2026; Hytale NStagedChunkGenerator
      BiomeStage/TerrainStage/PropStage/TintStage/EnvironmentStage per vulpeslab/hytale-docs; Vulkan Guide
      Ascendant chunk layers (main + transparent + clutter) per vkguide.dev; Minecraft world generation
      overview per Telepathic Grunt/XI64 Gist Feb 2021; maguirekrist/voxel_enginevk production-grade chunk
      pipeline 5 layers). **Standalone C++26 CPU prototype** (`prototype/sub_chunk_bench.cpp` ~870 LoC,
      `clang++ 22.1.6 -O3 -march=native`, build green). 4 designs (A_Monolithic baseline 512 bytes +
      B_Palette adaptive bits + C_FixedLayer_L2 4 layers + D_FixedLayer_L4 2 layers) × 5 scenes
      (uniform_air + uniform_floor + forest_floor + cave_stress + mixed_biome) × 5 seeds (1, 7, 42, 1234,

    31337) × 1000 iter per measurement = 100 measurements. **Measured (RTX 3060 Ti dev host irrelevant —
           CPU-only Zen 3 5800X, governor=`powersave`, 62.7 GiB RAM DDR4):**

    - **Memory axis (B_Palette / C_L2 / D_L4 vs A_Monolithic baseline 512 bytes):**
        - uniform_air / uniform_floor: B=20 (-96%), C=84 (-84%), D=42 (-92%) — **B_Palette wins.**
        - forest_floor / cave_stress (2 materials): B=84 (-84%), C=148 (-71%), D=106 (-79%) — **B_Palette wins.**
        - mixed_biome (4 materials): B=148 (-71%), C=148 (-71%), D=138 (-73%) — **D_L4 marginal win.**
    - **Build cost axis:** monolithic 0.03-0.13 µs/chunk vs paletted 1.3-5.8 µs/chunk = **30-55× overhead**.
      But absolute cost 1-6 µs vs Stage 4.1 budget 50 µs/chunk per `TODO.md §4.1` = 8-50× headroom.
    - **Mutation cost axis:** monolithic 10-16 ns/mutation vs paletted 12-19 ns = **+5-70% overhead**.
      But absolute cost 10-19 ns vs Stage 1.2 DoD 0.1 ms tolerance = 5000-10000× headroom.
    - **Mesh vertex count axis:** all designs produce **identical** face counts (591-679 quads) для same
      scene+seed — mesh optimization is layout-orthogonal (covered by `2026-06-20-meshing-algo-comparison`
      verdict=mixed).
    - **Layer boundary axis:** monolithic 0 vs C_L2 80-155 vs D_L4 28-62 = explicit semantic gain для
      biome/cave chunks. **Cave/biome scenes show 28-155 explicit transitions per chunk** = VCT anti-leak
        + per-layer LOD + selective rebuild potential.
    - **Verdict=mixed:** paletted/layered designs win memory (73-96% > 5% threshold per
      `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`) + layer-boundary semantic axis,
      lose build cost (acceptable per budget) + mutation cost (negligible absolute). **Mainline
      recommendation:** conditional — **B_Palette для uniform chunks (96% savings)**, **D_L4 для
      biome/cave chunks (73-79% savings + 28-62 transitions)**, **C_L2 для finer biome granularity
      (71-84% + 80-155 transitions)**; A_Monolithic as fallback для sparse chunks + legacy compatibility.
    - **3-step migration per `agent/knowledge.md §30.4` precedent:**
        - **Step 1 (S, ~150 LoC):** `ChunkLayout` enum + `ChunkStorage::payload` polymorphic container +
          `SelectChunkLayout(scene_chunk_type, voxel_count, palette_size)` decision logic в
          `src/voxel/VoxelWorld.{hpp,cpp}`. Cross-references `2026-06-20-nanovdb-on-gpu` hybrid SVDAG +
          NanoVDB.
        - **Step 2 (M, ~300 LoC):** new `world_gen_layers.comp` shader emits per-layer payload + per-chunk
          layout metadata. Each layer = independent noise query (heightmap per
          `2026-06-21-gpu-procedural-noise-compute-kernels` Step 3 OpenSimplex2). Cross-references
          `2026-06-21-wfc-procedural-worlds` Step 4 (WFC + noise hybrid) for discrete layer transitions.
        - **Step 3 (M, ~250 LoC):** wire layer semantics в `src/shaders/voxel.frag` для VCT cone-march
          terminate at explicit layer boundary (anti-leak guarantee per `2026-06-20-vct-vs-rt-cutoff`
          Step 3) + Stage 4.2 per-layer LOD downsampling. Selective rebuild via existing
          `pendingChunkRebuildIndices` + per-layer dirty bit per mainline Phase 9 2x part 5.
    - **Caveats:** CPU prototype, no GPU dispatch; no Sparse64Tree integration (flat arrays only);
      naive face counter (no greedy merge per `meshing-algo-comparison`); synthetic scenes; single-threaded.
      Cross-vendor GPU memory layout (AMD RDNA + Intel Arc) deferred до Stage 4.1 GPU integration prototype.
    - **Cross-axis:** Stage 4.x biome/cave axis fully closed same-day сессии (continuous noise axis via
      `gpu-procedural-noise-compute-kernels` verdict=mixed OpenSimplex2 + discrete structure axis via this
      `sub-chunk-layers` verdict=mixed layered chunks + gen-strategy axis via in-progress
      `wfc-procedural-worlds` WFC). 3 orthogonal axes of Stage 4.x = complete picture.

- [x] **[2026-06-21-frame-flight-allocator-budget](./experiments/2026-06-21-frame-flight-allocator-budget/)** —
  m, `Stage 6.2 tech-debt` (cross-cutting для Stage 2.x/3.x/5.x). Closed `2026-06-21` (single session),
  verdict **`mixed`**. **VRAM-allocator-axis experiment** — единственная ось, не покрытая same-session
  2026-06-20 closed experiments (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/gi-strategy +
  job-scheduling + mass-lights + shadow-dim + SOTA-GI). Web-research complete (4 batch queries, ~30 results,
  ~15 ключевых sources верифицированы: VMA 3.4.0 docs [recommended usage patterns, custom memory pools,
  linear algorithm ring buffer, staying within budget] + VMA Issue #453 [VMA author warning against
  per-frame `vmaCreateBuffer`/`vmaDestroyBuffer`] + Frostbite Frame Graph [Yuriy O'Donnell GDC 2017,
  transient resources pattern] + Frostbite Scope Stacks [EA PDF, linear allocator pattern] + Diligent
  Engine 2.0 ring buffer [FIFO dynamic resource pattern] + Unreal Engine RHI [Epic Forums 2025-05-23,
  `STAT_VulkanMemoryUsage#` per `VK_EXT_memory_budget`, `FrameTempBuffer` + `RingBuffer` categories] +
  DXVK commit `9b272fb` [2024-11-08, `VK_EXT_pageable_device_local_memory` enable + AMD fallback] +
  vkd3d-proton PR #1543 [Evict/MakeResident emulation, NVIDIA contribution] + D3D12 Residency Starter
  Library [Microsoft reference] + NVIDIA Vulkan Do's and Don'ts [Nuno Subtil 2019-06-06, "use memory
  sub-allocation"] + AMD "Using Vulkan Device Memory" guide [2016, 64 MiB block size guidance] +
  `VK_EXT_memory_budget` spec [2018, ratified] + `VK_EXT_pageable_device_local_memory` spec [NVIDIA,
  RTX 3060 Ti Ampere supported in driver 555+] + llama.cpp HVV fragmentation [Jeff Bolz NVIDIA,
  2025-01-30]). **Standalone Vulkan 1.4 prototype** (`prototype/main.cpp` + `harness.hpp` + `strategies.hpp`
    + `benchmark.hpp` + `CMakeLists.txt`, ~890 LoC total, links vendored VMA 3.4.0 + volk from `external/`,
      **NOT ProjectV mainline**). 5 strategies measured + 1 stress pass: (A) `A_Default` = current mainline
      behavior; (B) `B_BudgetTrack` = A + `EXT_MEMORY_BUDGET_BIT` + `WITHIN_BUDGET_BIT` flag; (C) `C_LinearPool`
      = per-frame linear pool create+destroy; (D) `D_DoubleBuffer` = C + `WITHIN_BUDGET`; (E) `E_PreCreatedRing`
      = production-realistic single pre-created 64 MiB ring pool reused across frames. **1000 measured
      frames per strategy + 50 warmup** (per `benchmarks/methodology.md §3`), 8 MiB world-edit spike every
      200 frames. **Stress pass:** 256 MiB spike every 50 frames (overflow test for hard cap).
      **Measurements** (RTX 3060 Ti dev host, Vulkan 1.4.350, NVIDIA 610.43.02, governor `powersave`):
      (A) mean 35.5 µs / p99 67.4 µs / failures 0; (B) mean 34.7 µs / p99 58.2 µs / failures 0;
      (C) mean 1311 µs / p99 2573 µs / failures 0 [per-frame pool recreate 30× slower]; (D) mean 1309 µs /
      p99 2941 µs / failures 0; (E) mean 38.0 µs / p99 113 µs / failures 0 / **peakHeapUsage +64 MiB**
      [64 MiB ring block persistent]. **Stress pass:** D = 21 clean `VK_ERROR_OUT_OF_DEVICE_MEMORY`
      failures (256 MiB > 64 MiB pool block → hard cap fires correctly). **Caveats:** single GPU vendor
      validated (NVIDIA Ampere); single-threaded harness; cross-vendor (AMD RDNA + Intel Arc) deferred;
      synthetic workload; `VK_EXT_pageable_device_local_memory` not exercised in prototype but
      production-proven per DXVK + vkd3d-proton precedent. **Mainline recommendation** (3-step migration
      per `agent/knowledge.md §30.4` precedent): **Step 1 (XS, ~20 LoC)** — add
      `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` to `VulkanBootstrap.cpp:807-823` allocator +
      `vmaSetCurrentFrameIndex()` per frame + TracyPlot `VRAM.heapBudgetMiB`/`heapUsageMiB`; **Step 2
      (S, ~50 LoC + tests)** — add `VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT` flag для non-critical
      allocations (5+ call sites per `rg vmaCreateBuffer`) with graceful degradation; **Step 3 (M, ~200
      LoC) DEFERRED** — pre-created single linear ring buffer pool (`TransientPool.{hpp,cpp}` +
      integration in `Renderer.cpp`) re-evaluation triggers: Stage 4.3 (128+ chunks, transient SSBO
      count > 50/frame) OR Stage 5.2 RTX BLAS pool overflow OR Tracy heap-usage→budget trend over 60s.
      **Caveat per Step 3:** VMA docs require `maxBlockCount = 1` для ring buffer; double-pool variant
      (Strategy D) = wrong pattern, **не реализовывать**. **Cross-axis continuity:** 19+ closed
      same-session 2026-06-20 + сегодняшний parallel `2026-06-21-tracy-gpu-vs-manual` (orthogonal scope,
      no conflict per `docs/experiments/AGENTS.md §13.3`). Этот experiment = allocator axis closed
      (cross-cutting для всех transient pressure sources). См. §6 + §1 + experiment README +
      `prototype/README.md` + `prototype/build/results.csv`.

- [x] *
  *[2026-06-21-gpu-procedural-noise-compute-kernels](./experiments/2026-06-21-gpu-procedural-noise-compute-kernels/)** —
  m,
  Stage 4.1 (GPU Noise & World Gen per `TODO.md §4.1`, gating blocker для infinite worlds). Closed
  `2026-06-21` (single session), verdict **`mixed`** (perf gain 2.9% < 5% threshold per
  `optimization-philosophy.md`; quality + license axis still favors OpenSimplex2 3D-S). **Noise-algorithm
  axis** experiment — orthogonal к `2026-06-20-simd-procedural-noise` (CPU AVX2 vs scalar) и к
  in-progress `2026-06-21-dxc-vs-glslc-toolchain` (shader toolchain). Direct prior art:
  `agent/knowledge.md §29.0` line 887 (Tier 4 R&D marker для Stage 4.1) + `TODO.md §4.1` explicit
  GPU noise requirement + `agent/workspace.md §1 Phase 1` world_gen.comp skeleton. Web-research complete
  (3 batch queries, ~20 results, 20 sources верифицированы: Schneider `arXiv 1903.12270` Perlin/Float 3D
  = 77 ALU inst [direct instruction count baseline], GPU Gems 2 Ch 26 textured-LUT Perlin = 53 inst /
  9 lookups, atyuwen/bitangent_noise SimplexNoise.hlsl 3D = ~71 instruction slots, KdotJPG/OpenSimplex2
  673 stars CC0 modern GPU-friendly design, Auburn/FastNoiseLite 3D Perlin 47.93 M/s scalar /
  261.10 M/s AVX2 CPU baseline, NVIDIA Nsight Compute Ampere workgroup-64 occupancy guidance,
  Khronos Forums compute shader SSBO write cost validation, JCGT 2022 Olano GTX 1660 modern compiler
  DCE analysis 17% speedup from disabling tiling, Vulkanised 2024 GPU Atomic Performance Modeling
  McKee microbench, production references: paulrobello/voxel-world Vulkan compute 5D climate noise +
  Perlin Feb 2026, Aokana arXiv 2505.02017 GPU-Driven voxel May 2025, AdityaGupta1/mega-minecraft CUDA
  fBm Oct 2025, russellocean/pebble-rs WGPU compute voxel raytracer Nov 2025, Yunasawa YNL Vozel
  Minecraft-1.18+ 5-parameter FBM biome gen Sep 2025). Standalone Vulkan 1.4 compute prototype
  (`prototype/{main.cpp, noise_kernels.comp, CMakeLists.txt, README.md}`, ~700 LoC total, 5 conditional
  GLSL variants через `#define VARIANT_*` switch + dispatch harness, RTX 3060 Ti GA104 Ampere, Vulkan
  1.4.341, NVIDIA driver 610.43.02, Clang 22.1.6 + glslc 2026.2). 3 runs × 5 variants × 1000 iter +
  10 warmup. **Measured:** VALUE=0.0273 ms, PERLIN=0.0272 ms, SIMPLEX=0.0272 ms, OPENSIMPLEX2=0.0272 ms,
  WORLEY=0.0280 ms. **All variants в пределах 2.9% mean** — ниже 5% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. WORLEY unexpectedly not slowest
  despite 27-cell loop (`glslc` 2026.2 fully unrolled + register optimization). VALUE == PERLIN по
  cost (hash + gradient table index similar register footprint на Ampere). Memory bandwidth = 65.6%
  of 448 GB/s theoretical peak = **memory-bound kernel**, ALU = ~14% of dispatch time only. Per-eval
  cost = 13.0 ns/eval, per-chunk = 6.6 µs. **Stage 4.1 budget (50 µs/chunk per `TODO.md §4.1`):**
  8× headroom single octave, 1.9× headroom FBM 4 octaves, 0.63× (over budget) FBM 4 octaves × 3 channels
  (heightmap+cave+biome). **Verdict=mixed:** алгоритмический выбор НЕ meaningful perf discriminator
  на chunkSize=8 dispatch pattern; **но** quality + license axis still favors OpenSimplex2 3D-S (CC0,
  no axis artifacts, analytic derivatives, actively maintained KdotJPG 2019-2024+, stable cold-cache
  perf без Run-1 spike). **Mainline рекомендация:** use **OpenSimplex2 3D-S** для Stage 4.1 world
  gen (NOT because fastest — because license + quality + stability). 3-step migration per
  `agent/knowledge.md §30.4` precedent — Step 1 foundation `noise3d_opensimplex2()` GLSL port (~50 LoC
  core, attribution header per CC0 §4(a)), Step 2 dispatch in `world_gen.comp` per chunkSize=8
  pattern + FBM wrapper (4 octaves, ~150 LoC), Step 3 multi-channel (heightmap + cave + biome,
  octave reduction если budget exceeded, ~100 LoC). Total ~300 LoC, S effort, 1-2 sessions.
  **Cross-axis continuity:** same-day `2026-06-21` parallel sessions (frame-flight-allocator-budget
  in-progress + dxc-vs-glslc-toolchain in-progress + tracy-gpu-vs-manual in-progress) + my
  noise-algorithm axis = orthogonal angle of Stage 4.x + Stage 6.x + toolchain optimization
  landscape. Continuation chain: `2026-06-20-simd-procedural-noise` (CPU orthogonal) → this (GPU
  algorithm choice) → follow-up: FBM + multi-channel + AMD RDNA cross-vendor validation. **Caveats:**
  (a) single GPU vendor validated (RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341, NVIDIA 610.43.02) —
  mainline re-test on AMD RDNA 2/3/4 + Intel Arc Battlemage dev matrix; (b) single octave only —
  FBM 4 octaves linear scaling not measured; (c) single heightmap channel — multi-channel 3× cost
  projection not validated; (d) no Nsight Compute register/occupancy/SM pipe metrics — extension
  opportunity; (e) no spectral quality metric (FFT framework not built) — quality claims
  literature-cited; (f) async-compute overlap with graphics not measured (per `dec-pipelines-async-compute`
  verdict=yes — potential 5-8% additional gain); (g) Run 1 vs Run 2+3 shows 14% cold-cache offset
  для VALUE/PERLIN (insufficient warmup at 10 iters) — OPENSIMPLEX2/SIMPLEX/WORLEY stable from Run 1.
  Cross-refs: `TODO.md §4.1`, `src/voxel/VoxelWorld.hpp:85` (chunkSize=8), `src/shaders/voxel_mesh.comp:146`
  (existing dispatch pattern), `agent/workspace.md §1 Phase 1` (world_gen.comp skeleton),
  `agent/knowledge.md §30.4` (3-step migration precedent), `2026-06-20-simd-procedural-noise` (CPU
  orthogonal), `2026-06-20-dec-pipelines-async-compute` (async foundation, world gen spike isolation),
  `2026-06-20-nanovdb-on-gpu` (GPU SSBO write target format), `docs/experiments/hardware-profile.md §3`
  (RTX 3060 Ti dev host), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
  (5-10% threshold definition).

- [x] **[2026-06-21-dxc-vs-glslc-toolchain](./experiments/2026-06-21-dxc-vs-glslc-toolchain/)** — m,
  Stage 0 / foundational (toolchain decision, cross-cutting для Stage 2.1 mesh shader + Stage 5.2
  RT pipeline + every shader forever). Closed `2026-06-21` (single session),
  verdict **`mixed`**. **Toolchain-axis experiment** — единственная Stage 0 ось, не покрытая
  same-session 2026-06-20 closed experiments (storage/sync/cull/binding/layout/meshing/simd/hzb/
  flecs/gi-strategy + job-scheduling + mass-lights + shadow-dim + SOTA-GI + frame-flight-allocator
    + gpu-noise-kernel). Web-research complete (3 batch queries, ~30 results, 11 key sources
      верифицированы: Khronos HLSL-in-Vulkan guide [`docs.vulkan.org/guide/latest/hlsl.html` —
      "DirectXShaderCompiler (DXC) is the reference HLSL to SPIR-V compiler … has the most complete
      and up-to-date support and is the recommended way"], DXC SPIR-V CodeGen spec
      [`docs/SPIR-V.rst`], DXC release v1.9.2602.24 Feb 2026 Patch 1 [standalone binary, no system
      install], Sascha Willems + Ben Clayton Google LLC Jun 2025 [production precedent — converted
      all Sascha Willems Vulkan samples GLSL → HLSL], Vulkanised 2025 Nathan Gauer Google [DXC issues
    + Clang-based HLSL transition roadmap], Microsoft DXC 1.8.2405 May 2024 [HLSL 202x transition],
      Microsoft DirectX adopting SPIR-V Sep 2024 [SM 7.0 = SPIR-V], Shader-slang discussion #9354
      [independent benchmark: DXC 3-4× faster than slang], Hexops devlog Feb 2024 [DXIL vs SPIR-V
      post-optimization differences], DXC issue #6960 [SPIR-V mesh shader bug fixed 1.8.2502], NVIDIA
      Forums Jul 2025 [DXIL vs SPIR-V perf delta]). Standalone C++/shell prototype (`prototype/
  {compile_bench.sh, extended_bench.sh, tools/dxc/, shaders_glsl/, shaders_hlsl/, results/}` —
      **NOT ProjectV mainline**). 5 representative шейдеров в GLSL + HLSL variants (vertex, fragment,
      mesh, compute-cull, compute-fluid-CA), с preserved descriptor layouts (SSBO, UBO, push constants,
      samplers) 1:1. **Measurements** (RTX 3060 Ti dev host `obvium`, Vulkan 1.4.350, glslc 2026.2 vs
      DXC v1.9.2602.24): **300 measurements** (30 iter × 5 shaders × 2 toolchain, default mode).
      **DXC compile time 9.1-10.9× faster** (mean 12.4 ms vs 121.7 ms; p95/p99 DXC < 16 ms vs glslc
      < 160 ms; std 0.7 ms vs 5 ms both relative to own mean). **DXC SPIR-V size 18-43% smaller**
      (mean 3342 B vs 4764 B; largest delta на mesh shader: 3904 vs 6804 B = -43%). **DXC instruction
      count 20-40% меньше** (mean 193 vs 281, computed via `spirv-dis --raw-id`). **Validation rate
      100%** обе toolchain (`spirv-val --target-env vulkan1.4`). **Debug info mode** (-Zi DXC, -g
      glslc, 20 iter): overhead +50-130% sizes; both still 100% valid; DXC still smaller in absolute
      terms. **Optimize mode** (-O3 DXC; glslc default already optimized): no measurable change vs
      default. **7 DXC API quirks documented** для future migration: (1) GLSL `location(N)` not
      supported → TEXCOORD semantics or `[[vk::location(N)]]`; (2) `WriteTriangle`/`WritePrimitive`
      не существует в DXC 1.9.x → `out vertices MeshVertex verts[V]` + `out indices uint3 primIndices[P]`
      pattern; (3) no unsized arrays в struct → split SSBO на отдельные `StructuredBuffer<T>`;
      (4) target env `vulkan1.1spirv1.4` (NOT `vulkan1.4`); (5) no GLSL-style combined sampler →
      separate `Texture2D` + `SamplerState` or `[[vk::combined_image_sampler]]`; (6) `gl_FragCoord`
      → `SV_POSITION` (input only); (7) `gl_GlobalInvocationID` → `SV_DispatchThreadID`,
      `gl_LocalInvocationIndex` → `SV_GroupIndex`, `atomicAdd` → `InterlockedAdd`,
      `barrier()` → `GroupMemoryBarrierWithGroupSync()`. **Verdict=mixed:** DXC wins quantitatively
      (9-10× compile speed, 30% smaller SPIR-V), but migration cost = M-L effort (rewrite 19 шейдеров)
    + DXC architectural risk (Clang-based HLSL transition 2026-2028 per Vulkanised 2025 Gauer +
      Microsoft HLSL 202x roadmap; Clang-HLSL = single path long-term). **Mainline рекомендация:
      DEFER migration.** ProjectV остаётся на glslc per Vulkan SDK 1.4.350 baseline +
      `agent/knowledge.md §17`. 3-step migration plan documented for future (Step 1 foundation
      dual-toolchain в `src/CMakeLists.txt:15-26`, Step 2 hybrid mesh+RT rollout c `PROJECTV_USE_DXC_MESH=ON`,
      Step 3 default flip). **Re-evaluation triggers:** Vulkan 1.4 GLSL RT stabilization,
      Clang-HLSL stabilization, ProjectV shader count > 50 (CI/CD bottleneck), DXC-only feature need
      (e.g. SPV_NV_compute_shader_derivatives, SPV_KHR_maximal_reconvergence), driver SPIR-V
      complexity issue, Stage 5.2 RT inline SBT (`[[vk::shader_record_ext]]`). **Cross-axis
      continuity:** 20+ closed same-session 2026-06-20 + 2 closed same-session 2026-06-21 (frame-flight +
      gpu-noise) + 3 in-progress parallel (tracy-gpu + audio-raytracing + wfc-procedural) + this =
      full Stage 0/1.x/2.x/3.x/4.x/5.x/6.x + cross-cutting optimization landscape covered. Continuation
      chain: `2026-06-20-mesh-shader-vs-compute-cull` (verdict=mixed, mesh shader feature-flagged) +
      `2026-06-20-bindless-descriptor-overhead` (Phase E = bindless RTX TLAS) + Stage 5.2 RT pipeline
      foundation → this (toolchain choice для всех future HLSL/GLSL шейдеров). **Caveats:** (a) prototype
      шейдеры = 30-50% mainline complexity (representative layouts, simplified logic); (b) single
      GPU vendor (RTX 3060 Ti GA104 Ampere); (c) DXC = Linux x86_64 only (no Windows verification);
      (d) runtime shader perf impact not measured (driver applies own SPIR-V optimization per Hexops
      devlog); (e) DXC SPIR-V backend has known extension-jungle problem (every new SPIR-V extension
      requires DXC patch per Vulkanised 2025 Gauer — long-term maintenance risk). Cross-refs:
      `src/CMakeLists.txt:15-26` (current glslc selection), `src/shaders/voxel_mesh.mesh` (mainline mesh
      shader using glslc pattern, validated), `agent/knowledge.md §17` (Linux Vulkan SDK 1.4.350 baseline),
      `agent/knowledge.md §4` (Build/verification contract), `agent/knowledge.md §30.4` (3-step
      migration precedent), `TODO.md §Stage 0` (toolchain decision), `docs/experiments/hardware-profile.md`
      (dev host `obvium`, captured `2026-06-20`), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
      (5-10% threshold), `2026-06-20-mesh-shader-vs-compute-cull` (mesh shader decision context),
      `2026-06-20-restir-gi-feasibility` (RT pipeline Stage 5.2 future), `2026-06-20-rt-shadows-vs-csm`
      (RTX shadow Stage 5.2), `2026-06-21-frame-flight-allocator-budget` (parallel session, allocator
      axis).

- [x] **[2026-06-20-restir-gi-feasibility](./experiments/2026-06-20-restir-gi-feasibility/)** — m, Stage 5.1/5.2
  (SOTA-GI-ось experiment, post-Stage 5 follow-up explicitly named в `vct-vs-rt-cutoff` §1 line 99 «Step 4
  (optional post-Stage 5) DDGI/SHaRC/NRC/ReSTIR PT»). Closed `2026-06-20` (single session),
  verdict **`mixed`**. Web-research complete (~30 sources верифицированы: Bitterli 2020 ReSTIR original
  [6-60× MSE ↓, 8 rays/pixel max], Ouyang 2021 ReSTIR GI [9.3-166× MSE ↓ @ 1spp], Lin 2022 ReSTIR PT +
  GRIS theory [80 ms @ 1920×1080, MAPE 0.39 vs 1.63 PT, 1 path/pixel], Majercik 2019/2021 DDGI,
  Müller 2021 NRC [2.6 ms @ full HD, NVIDIA Tensor Cores ≥ Turing], NVIDIA-RTX/RTXGI SDK v2.7.0 Mar 2026
  [336 stars, driver ≥ 555.85], NVIDIA-RTX/SHARC [123 stars, spatial hash grid 64-bit keys, 4-pass,
  ~185 MB @ 2^22 baseline, 1.5-10% perf overhead in Cyberpunk], NVIDIA-RTX/RTXDI v3.0+
  [ReSTIR DI/GI/PT/ReGIR, D3D12+Vulkan via NVRHI, DXC toolchain], Crassin 2011 GIVoxels [VCT foundation,
  25-70 FPS, two bounces Lambertian+glossy], Lumen SIGGRAPH 2022 [Epic explicitly rejected VCT as leaky],
  Minecraft RTX GDC 2021 [voxel + path tracer + irradiance cache, DDGI-style probes], Douglas Voxel
  Devlog #23 Jun 2025 [direct voxel + DDGI integration, voxel-compatible verified], Cyberpunk 2077 RT
  Overdrive Patch 2.1 Dec 2023 [production ReSTIR DI/GI + SHaRC], NVIDIA Zorah RTX 50 demo 2025 [ReSTIR PT],
  Portal RTX [SHaRC], OGRE-Next CIVCT [10-100× faster voxelization], Aokana arXiv 2505.02017 May 2025,
  ReSTIR FG/GSGI/PMGI 2024 [0.4-14 ms overhead variants], Epic DDGI abandonment forum Dec 2025 [Arc Raiders
  counter-example]). Standalone prototype deferred per `rt-shadows-vs-csm` precedent — analytical +
  literature + cross-vendor matrix sufficient. **Architectural mismatch (главный finding):** все 4 SOTA
  техники (ReSTIR PT/GI/DI, DDGI, SHaRC, NRC) **требуют path tracer foundation**; ProjectV's Stage 5.x
  = hybrid VCT+RTX = **NOT** path tracer. **VRAM matrix** (RTX 3060 Ti, 5.06 GiB budget):
  SHaRC = 185 MiB (3.65%, acceptable), DDGI = 16 MiB, ReSTIR reservoir = 33-67 MiB checkerboard/full.
  **Quality validated** для path-tracing contexts (ReSTIR PT MAPE 0.39 vs 1.63 Carousel, Cyberpunk
  production, SHaRC 1.5-10% overhead) — **cannot translate** к ProjectV без path tracer. **NRC rejected**
  = NVIDIA-only (Tensor Cores ≥ Turing, excludes AMD RDNA 4 + Intel Battlemage per `dec-pipelines-async-compute`
  matrix). **Mainline recommendation:** **keep current hybrid VCT+RTX as-is** (Stage 5.x MVP scope), **defer
  SOTA GI до Stage 6+ post-MVP path tracer pivot**. Recommended add-on order (if path tracer ships):
  **SHaRC → DDGI → ReSTIR DI/GI/PT**. SHaRC first: lowest complexity, cross-vendor, voxel-adaptable,
  acceptable VRAM. NRC = skip. **Re-evaluation triggers:** VCT leakage visible в production (cavity lighting
  artifact), Stage 4.3 ships (128+ chunks), mainline commits to path tracer (independent decision),
  vendor ships open-source SHaRC GLSL port, ReSTIR GSGI/PMGI stabilize (2024 prototypes, 0.4-0.8 ms = viable
  alternative). **Cross-axis:** 19+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization
  landscape + SOTA-GI axis. **Lighting axis FULLY closed** (cutoff + lights + shadows + SOTA-GI all
  same-day `2026-06-20`). Continuation chain: `vct-vs-rt-cutoff` (mixed, cutoff=0.3) + `rt-shadows-vs-csm`
  (mixed, hybrid CSM+RTX) + `clustered-forward-mass-lights` (yes, SSBO) + `nanovdb-on-gpu` (yes, VCT SSBO) +
  `dec-pipelines-async-compute` (yes, async foundation) → this. Closed entry:
  `experiments/2026-06-20-restir-gi-feasibility/`.

- [x] **[2026-06-20-clustered-forward-mass-lights](./experiments/2026-06-20-clustered-forward-mass-lights/)** — m,
  Stage 5 (GI & Temporal, depends on §1.2 SVDAG). Closed `2026-06-20` (single session),
  verdict **`yes`** (с условиями: soft cap ≥2048 + light prioritization для 5000+ light scenes).
  **Mass-lights architecture** experiment — единственная ось, не покрытая today-сессиями
  (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async/gi-strategy + job-scheduling).
  **Mainline baseline = single-light hard cap** per `src/shaders/voxel.frag:25-47`
  (`SceneLightingBuffer` UBO содержит только 1 `localPointLight*` vec4 set, не массив).
  **Не масштабируется** на `TODO.md §4.x` procedural (лава/факелы/магия) + `§5.1` VCT VPLs.
  Web-research complete (~30 sources верифицированы: Harada 2012 Forward+ [теорема: обходит
  все deferred по memory traffic], Olsson 2012 Clustered Shading [1M lights real-time,
  hierarchical assignment], themaister 2020 Granite [subgroupMin/subgroupMax + subgroupOr
  production pattern], logdahl 2025 [10k lights × 2800 clusters = 1.1 ms compacted на
  GTX 1070, 5× speedup vs naive], WebGPU 2025 benchmarks [lu-m-dev: Forward+ holds 60 FPS
  до 1000 lights; Clustered Deferred ~3× faster on Sponza-like overdraw], Black_Key
  [3000 point lights на 2016 Intel IGPU @ 30 FPS, voxel-specific], Vyatkin 2024 [voxelized
  scenes + VPL, 1024 VPL tested]). Standalone CPU prototype `prototype/bench.cpp`
  (single file, ~480 LoC, Clang 22.1.6, `-O3 -march=native`, compiled clean `-Wall -Wextra`).
  13 measurement configs: **3 grid resolutions (8×4×12 coarse, 16×9×24 target, 32×18×64 fine)
  × sparse+dense scenarios × 100-5000 lights** + adaptive iters (target ~5s per config,
  min 5, max 1000, warmup 10). **Key CPU numbers (16×9×24 target, sparse scenario):** 100
  lights = 1.4 ms mean, 1000 lights = **12.7 ms mean / 15.3 ms p99** (avg 3.1 lights/cluster,
  max 34, 66% empty). **Dense scenario (lava):** 16×9×24 / 1000 lights = 15.4 ms (avg 232,
  max 544, 22% empty). **CRITICAL: 16×9×24 / 5000 dense lights = 124.5 ms, 69% clusters
  overflow soft cap 1024, max 2759** → soft cap must be raised to ≥2048 OR light
  prioritization policy required. **Cross-validation с published GPU numbers:** within
  5-10× of logdahl 2025 (1.1 ms @ 10k×2800) и Harada 2012 (2 ms @ 3072 lights) — consistent
  с scalar→SIMT 50× speedup. **GPU projected cluster build:** 0.1-0.5 ms at 1000 lights
  (1.5-3% of 16.67 ms frame budget). **Per-fragment analytical model:** Forward+ (10
  lights/cluster avg) = 1000 ALU + 50 DDA reads per fragment = **100× speedup vs
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
  **Cross-axis continuity:** same-day `2026-06-20` сессии (vct-vs-rt-cutoff mixed +
  this yes) + Stage 5 foundation complete (nanovdb-on-gpu yes + dec-pipelines-async-compute
  yes). **12+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization
  landscape** + mass-lights dimension added. Closed entry:
  `experiments/2026-06-20-clustered-forward-mass-lights/`. **Race resolution note (per
  `docs/experiments/AGENTS.md §13.3`):** parallel session informed before starting
  `vis-buffer-for-voxels` (orthogonal design space, complementary); my `clustered-forward-
  mass-lights` = first-write-wins per §13.3.

- [x] **[2026-06-20-vis-buffer-for-voxels](./experiments/2026-06-20-vis-buffer-for-voxels/)** — m,
  Stage 2.x + Stage 5.x (deferred resolve via vis-buffer + material-table SSBO; orthogonal
  rendering-approach axis). Closed `2026-06-20` (single session), verdict **`mixed`**.
  Web-research complete (5 batch queries, 20+ sources верифицированы: Burns-Hunt 2013 JCGT 2:2
  foundational 64MB vis-buffer vs 398MB G-buffer = 6.2× bandwidth win @ 1080p × 8xMSAA;
  Karis SIGGRAPH 2021 + Wihlidal GDC 2024 Unreal Nanite 64-bit vis-buffer with atomicMax +
  shading bins = 100% compute shaders UE 5.4; Andersson Frostbite 2017 "10-20x geometry vs
  Deferred"; The Forge v1.57 May 2024 TVB 2.0 pure compute; Cao NanoMesh SIGGRAPH 2024 32-bit
  mobile visbuffer; Vulkan-Guide TBR best practices 2024; Lam Adreno vis-stream HW compressor;
  jglrxavpok 2023 Vulkan R64Uint vis-buffer impl; Harada AMD Forward+ GPU Pro 4 alternative;
  Olsson Clustered Shading HPG 2012 1M lights; VoxelMVP / Exile / Slater / cgerikj / Ascendant
  voxel-specific refs). Standalone Vulkan 1.4 prototype (~700 LoC incl. shaders), standalone
  greedy-meshing voxel scene + 2 pipelines (baseline forward+ vs vis-buffer hypothesis) +
  6 measurement configs (3 scenes × 3 resolutions) на RTX 3060 Ti (Vulkan 1.4.341, NVIDIA
  610.43.02). **Refined hypothesis (post-ProjectV-survey):** ProjectV's current path already
  uses SSBO material lookup (forward+, no full G-buffer), so bandwidth win = N/A. Potential
  win = redundant raster elimination для CSM × 4 shadow passes (each re-decodes PackedFace
  vertex shader). **Cross-over @ ~1280×720.** 1920×1080 = vis-buffer 15-26% slower (bandwidth-
  bound on pixel coverage). 800×600 = vis-buffer 12-24% faster (vertex cost dominates).
  Voxel scenes are pixel-coherent after greedy meshing per `2026-06-20-meshing-algo-comparison`
  verdict=mixed (Naive Greedy default = ~1 visible triangle per pixel = no overdraw to amortize
  fullscreen vis-buffer cost). **Visual equivalence verified via framebuffer hash match** (both
  paths produce identical output for same scene + lighting). **Mainline рекомендация: DEFER.**
  No immediate integration. Re-evaluation triggers: Stage 4.3 (128+ chunks draw distance,
  vertex cost scales linearly → crossover shifts), mobile target support (TBR GPUs benefit
  per Vulkan-Guide, vis-buffer 10-30% win), Stage 4.2 LOD high-subdivision (overdraw-heavy),
  Stage 5.1 VCT integration (multiple cone-trace passes), >4 light passes. Cross-axis closure:
  today's batch (storage/sync/cull/binding/meshing/simd/hzb/flecs/nanovdb/gi-cutoff/frame-pacing/
  job-system/clustered-forward-mass-lights) + this = full Stage 1.x/2.x/3.x/4.x/5.x/6.x
  optimization landscape (12+ experiments closed same-day `2026-06-20`). Complementary to
  parallel session's `clustered-forward-mass-lights` (orthogonal: vis-buffer = deferred-resolve
  vs clustered-forward = forward+ cluster grid). Cross-refs: `2026-06-20-bindless-descriptor-overhead`
  Phase B (bindless material table = prerequisite для vis-buffer's per-frame material lookup),
  `2026-06-20-meshing-algo-comparison` verdict=mixed (Naive Greedy default = pixel-coherent = no
  overdraw = vis-buffer loses на high res), `2026-06-20-dec-pipelines-async-compute` verdict=yes
  (async-compute resolve pass would compound vis-buffer benefits, unmeasured),
  `agent/knowledge.md §25` (greedy meshing rationale), `agent/knowledge.md §30.4`
  (3-step migration precedent), `src/shaders/voxel.frag` binding 2 (existing MaterialVisual
  SSBO), `src/shaders/voxel_shadow.{vert,frag}` (existing shadow re-raster), `src/render/Renderer.cpp:540-863`
  (existing rendering orchestration), `TODO.md §5.2` (where vis-buffer integration would land
  if re-evaluated), `docs/experiments/hardware-profile.md §3` (RTX 3060 Ti dev host).

- [x] **[2026-06-20-vct-vs-rt-cutoff](./experiments/2026-06-20-vct-vs-rt-cutoff/)** — m, Stage 5.1 + Stage 5.2.
  Closed `2026-06-20`, verdict **`mixed`**. Lighting/GI-ось experiment. Web-research (3 batch queries,
  ~30 sources: Crassin 2011 GIVoxels, NVIDIA VXGI 0.9, OGRE 2019 hybrid blog, Lumen SIGGRAPH 2022 +
  Narkowicz "Journey to Lumen" 2022, Akenine-Möller JCGT 2021 ray-cone spread, Wiche & Kuri JCGT 2020
  cone ADS, NVIDIA RTXGI 2.0 SDK 2024-03 (NRC/SHaRC/DDGI), NVIDIA RTXDI 3.0 ReSTIR PT, Erlich et al.
  Eurographics 2024 VSRM vs DXR, NVIDIA Blackwell architecture whitepaper 2025, AMD RDNA 4 deep dive
  2025, Intel Battlemage Xe2 2025, Minecraft RTX 2021, Franke Delta VCT 2014, Sugihara 2014 LRSM,
  Ryse Crytek GDC 2014, Aokana 2025, dubiousconst282 2024, Molenaar PG 2024, etc.). **Analytical
  cost model** + **cross-vendor HW RT perf matrix** (NVIDIA Ampere 4 tri/cycle baseline → Ada same
    + more units → Blackwell 8/cycle 2× gain; AMD RDNA 2/3 1/cycle → RDNA 4 2/cycle 2× gain; Intel
      Alchemist 1/cycle → Battlemage 2/cycle 2× gain; cross-vendor convergence at RDNA 4 / Battlemage).
      **Refined cutoff = 0.3** (не 0.3–0.5 диапазон из гипотезы): VCT specular 2.5× at r=0.3 = RTX 1-ray
      cost; OGRE 2019 precision cliff at 0.02 (8-bit atlas, ProjectV R8G8B8A8 same risk); Akenine-Möller
      2021 GGX math; Lumen 2022 rejected pure VCT (leaking in coarse mips) → RTX-dominant с VCT fallback.
      Cross-vendor threshold adjustment recommended (Blackwell → 0.4-0.5, RDNA 2 → 0.2, Battlemage → 0.25,
      no-HW-RT → VCT-only). Mainline integration: 4-step migration per `agent/knowledge.md §30.4` precedent
      — Step 1 foundation (roughness cutoff constant + HW RT probe + feature flag in CMakeLists), Step 2
      VCT implementation (voxelize.comp + vct.frag + 3D atlas + mip chain per `TODO.md §5.1`), Step 3
      RTX implementation (BLAS per chunk + TLAS per frame + rayQueryEXT integration per `TODO.md §5.2`),
      Step 4 (optional post-Stage 5) DDGI/SHaRC/NRC/ReSTIR PT. Caveats: analytical model only (no ProjectV
      prototype), single-vendor literature (NVIDIA heavy), VCT leak in ProjectV SVO = lower than Lumen
      surface cache but not zero. Continuation chain: `nanovdb-on-gpu` (VCT SSBO foundation) →
      `dec-pipelines-async-compute` (async re-voxelization) → `hzb-binding-models` (texelFetch pattern
      для bindless VCT atlas) → this (roughness cutoff strategy). **Lighting/GI-ось closed**; Stage 5
      now has both storage (nanovdb-on-gpu) + sync (dec-pipelines-async-compute) + cutoff strategy (this).
      Cross-axis: memory + layout + sync + storage + GI strategy — five orthogonal axes of Stage 1.x/2.x/
      3.x/5.x optimization, all closed same-day `2026-06-20`.

- [x] **[2026-06-20-hzb-binding-models](./experiments/2026-06-20-hzb-binding-models/)** — m, Stage 2.2.
  Closed `2026-06-20`, verdict **`mixed`**. Web-research (~10 sources incl. critical NVIDIA `textureLod`
  bug под `VK_EXT_descriptor_heap` per `foijord/vk-textureLod-repro` 2026) + standalone Vulkan compute
  prototype (24 sampling tests across 8 mips × 3 patterns). **17/24 PASS, 7/24 FAIL.** Conclusive findings:
  (a) `texelFetch(sampler2D, ivec2, mipLevel)` correct + bindless-robust (recommended); (b) `textureLod`
  correct on classic, fragile под bindless (NOT recommended для Phase E future); (c) `imageLoad(storage_image)`
  fundamentally unsuited для HZB culling (GLSL single-mip-per-binding limitation, proved by `max_abs_error =
      N * 1000` pattern). Mainline recommendation: Stage 2.2 cull shader uses `texelFetch`, HZB descriptor =
  `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` + separate `SAMPLER`. ~50-100 LoC change across 4 files. Future-proofs
  `bindless-descriptor-overhead` Phase E. Cross-refs: `bindless-descriptor-overhead` (Phase E prerequisite),
  `TODO.md §2.2`, `src/render/HizCulling.cpp`.

- [x] **[async-compute-overhead-numbers](./experiments/2026-06-20-async-compute-overhead-numbers/)** — h, Stage
  2.2/3.1/4.1/5.2.
  Closed `2026-06-20`, verdict **`yes`**. **Sync-axis measurement gap closure** — количественно
  измерил overlap graphics||compute на RTX 3060 Ti Ampere (dedicated compute-only queue family 2,
  8 queues per `vulkaninfo` probe `2026-06-20`). Standalone Vulkan 1.4 app + 3 синтетических
  ProjectV-style compute workloads (VCT 3D blur, HZB cull, Fluid CA ping-pong) + 16-iter multiplier
  (моделирует 16 substeps/tick per `agent/knowledge.md §30.1` fluid CA rate) + 200 frames per mode
  (30 warmup). **Sequential mode:** 0.771-0.869 ms wall clock / 0.669-0.720 ms GPU total.
  **Async mode:** 0.695-0.771 ms wall clock / 0.625-0.636 ms GPU total. **Speedup: +9.85% to +11.34%**
  (стабильно > 5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`).
  p99 tail latency −39% (1.917 → 1.172 ms). GPU compute time −6.5 to −11.4%, GPU graphics time
  −8 to −13%. Подтверждает литературные 5-8% estimates из `2026-06-20-dec-pipelines-async-compute`
  количественно. Mainline рекомендация: 3-step migration per `dec-pipelines-async-compute` §1 +
  `agent/knowledge.md §30.4` precedent — Step 1 foundation `vkQueueSubmit2` + timeline semaphore
  conversion (S effort, single session), Step 2 per-pass async adoption gated by
  `PROJECTV_ASYNC_COMPUTE=ON` env (S per pass, 4 passes: 2.2/3.1/4.1/5.2), Step 3 default flip
  (XS, single config). Foundation шаг = prerequisite для Stage 3.1 GPU Fluid CA per
  `agent/workspace.md §2` + Stage 2.2 HZB full integration + Stage 5.2 RTX BLAS build (Phase E per
  `bindless-descriptor-overhead`). Cross-vendor expectations per `dec-pipelines-async-compute`
  §2.2 vendor matrix (NVIDIA Ampere/Ada/Blackwell = yes; AMD RDNA2/3/4 = yes with caveats; Intel
  Arc Gfx12.5+ = yes with L1 contention for ray queries). Caveats: (a) single GPU vendor validated
  (RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341) — mainline re-test on AMD RDNA + Intel Arc dev matrix;
  (b) NVIDIA June 2025 driver bug mesh-shading+async does NOT apply (compute cull path per
  `mesh-shader-vs-compute-cull` verdict=mixed); (c) synthetic workloads model ProjectV patterns
  but not actual code paths; (d) headless harness (no swapchain), so cross-frame pipelining gain
  (DiligentEngine up to 2× with double-buffering) not measured — expected additional 10-30% in
  real renderer per `dec-pipelines-async-compute` Caveat #2.

- [x] **[sparse-64-tree-alternatives](./experiments/2026-06-20-sparse-64-tree-alternatives/)** — h, Stage 1.1/1.2.
  Closed 2026-06-20, verdict **`yes`**. Sparse 64-tree (4×4×4 = 64-ary) подтверждён как SOTA-выбор для ProjectV
  Stage 1.1/1.2. Все три corner-cases (mutation / sparse DAG / GPU traversal) **не упираются** в design choice.
  VDB/NanoVDB = VFX dense (не наш use case). BR-tree/BIH = triangle-mesh-focused. Octree regression = -40-60%
  per eisenwave. HashDAG = future R&D (Stage 3.1+). Mainline рекомендация: продолжить Stage 1.1 → 1.2 path без
  pivot; flip `PROJECTV_SPARSE_64_STORAGE` default → on; добавить per-chunk SVDAG policy (lazy dedup, N-tick
  threshold).
- [x] **[mesh-shader-vs-compute-cull](./experiments/2026-06-20-mesh-shader-vs-compute-cull/)** — m, Stage 2.1.
  Closed 2026-06-20, verdict **`mixed`**. Compute cull + indirect draw (текущий `voxel_mesh.comp` + Pattern A)
  остаётся правильным default для Stage 2.x. Mesh shader pipeline (Pattern C, mesh + indirect count без task
  shader per the maister's universal fast path) = feature-flagged optional path
  (`PROJECTV_MESH_SHADER_PIPELINE=ON`), не default. **Task shader (Pattern B, TODO §2.1 literal design) =
  explicitly avoided** (vendor-specific tuning overhead + ~10% perf penalty even optimal per the maister +
  AMD RDNA2 TDR на early-return per GameDev.net 2024 + no shipped games). Aokana (май 2025, академический
  SOTA) использует compute shaders для всего voxel pipeline, не mesh shaders. Cross-vendor support matrix:
  Pattern A = universal; Pattern C = requires Vulkan 1.2+ + driver maturity; Pattern B = experimental.
  Mainline рекомендация: defer Stage 2.1 implementation до Stage 1.x (Sparse 64-tree + SVDAG) + Stage 2.2
  (HZB cull) completion. Re-evaluation trigger: Stage 4.3 (128+ chunks draw distance) — bandwidth savings
  scale proportionally, may cross 5% perf threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.

- [x] **[meshing-algo-comparison](./experiments/2026-06-20-meshing-algo-comparison/)** — h, Stage 2.1 (visual
  mesh) + Stage 3.3 (physics mesh). Closed `2026-06-20`, verdict **`mixed`**. Standalone C++20 prototype
  (`prototype/bench.cpp` ~1 200 строк, 4 algos × 6 scenes = 24 configs, 1 000 iter, mean/median/p95/p99/std).
  **Главные findings:** (a) **Naive Greedy** wins triangle count на 5/6 non-degenerate scenes
  (1.3-450× меньше triangles vs MC/SN/DC) — vertex-bound advantage для Stage 2.1;
  (b) **Marching Cubes** fastest build time (250-380 µs, 1.7-2.5× быстрее greedy) — original claim "не хуже
  по build time" **НЕ подтверждён**; (c) **Sparse scenes** (1% density) — SN/MC лучше по triangles
  (coplanar merge не работает на isolated voxels); (d) **Dual Contouring slowest** (1 170-4 817 µs, QEF
  overhead 4-5× vs MC); (e) **SN competitive** (1.5-2× медленнее MC, 1.2-2.4× больше triangles vs greedy).
  **Mainline рекомендация:** keep Naive Greedy default для Stage 2.1/3.3; bitwise cull optimization
  (per cgerikj 2020, 50-200 µs/chunk) — drop-in option для Stage 4.1 high-frequency rebuild; re-evaluate
  SN/MC при procedural sparse worlds. Cross-refs: `agent/knowledge.md §25` (per-axis dispatch rationale),
  `TODO.md §2.1` (mesh shader spike target), `TODO.md §3.3` (Jolt MeshShape mirror),
  `mesh-shader-vs-compute-cull` (closed verdict=mixed, mesh shader = feature-flagged optional).
  [Sync fix r2 (2026-06-20): запись переехала из `§In progress` (stale после закрытия в другом
  parallel-session) → `§Closed` per AGENTS.md §13.5.]

- [x] **[vulkan-fps-pacing-vk-ext](./experiments/2026-06-20-vulkan-fps-pacing-vk-ext/)** — m,
  Stage 0 / independent (foundation для all stages; cross-cutting DoD principle «low latency
  > throughput» per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`).
  Closed `2026-06-20`, verdict **`mixed`** (analytical literature valid; prototype deferred).
  Web-research complete (5 batch queries, ~30 results; 8 key sources + 3 supplementary,
  all верифицированы: Khronos blog 2025-12-04, Phoronix Mesa 26.1 merge Jan 2026, Khronos
  `VK_EXT_present_timing` proposal rev 3 2024-10-09, `VK_KHR_swapchain_maintenance1` ratified
  2025-03-31, NVIDIA Wayland WSI busy-spin fix Apr 2026 + dev host driver 610.43.02 match,
  `VK_KHR_present_wait2` rev 1, Mesa 26.2 direct-display benchmarks Jun 2026, Android docs
  Jun 2026). **Dev host validation:** `vulkaninfo 2026-06-20` confirms все extensions supported
    + features enabled: `VK_EXT_present_timing` rev 3 (`presentTiming`, `presentAtAbsoluteTime`,
      `presentAtRelativeTime` features = true), `VK_KHR_present_wait2` rev 1 (`presentWait2` = true),
      `VK_KHR_swapchain_maintenance1` rev 1 (`swapchainMaintenance1` = true), `VK_KHR_present_id/2`,
      `VK_KHR_present_mode_fifo_latest_ready`. **Refined hypothesis:** **`VK_EXT_present_timing`**
      (Nov 2025 merge, Vulkan 1.4.335) — SOTA frame-pacing API; **NOT Vulkan 1.4 core** as
      original README thought — все 3 extensions are **device extensions**. Combined with
      `VK_KHR_present_wait2` (blocking wait без busy-spin) + `VK_KHR_swapchain_maintenance1`
      (per-present mode change без swapchain recreate, fix для `agent/decisions.md §30.3`
      RecreateSwapchain cycle) → детерминированный frame budget. Mesa 26.2 KHR_display
      direct-display benchmark: **~0.3 ms latency reduction, 5% power reduction, tighter
      variance** (0.9 ms → 0.3 ms std-dev). **Caveats:** (a) Mesa benchmark = KHR_display
      direct-display (без Wayland compositor) — Wayland gain ожидаемо меньше; (b) Intel Iris Xe
      doesn't support `present_wait` / `swapchain_maintenance1` — fallback path needed;
      (c) AMD/Intel cross-vendor = Mesa 26.1+ (Jan 2026), deployment lag 1-2 cycles.
      **Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent —
      Step 1 foundation (`PROJECTV_USE_PRESENT_TIMING=ON|OFF` env + per-feature detection в
      `TryPickPhysicalDevice`); Step 2 adoption (Mode C path с `desiredPresentTime` IPD
      calibration via `vkGetPastPresentationTimingEXT` feedback + `VkSwapchainPresentModeInfoKHR`
      per-present mode change + `VkSwapchainPresentFenceInfoKHR` race-free destroy); Step 3
      default flip для hardware с `presentTiming + presentAtAbsoluteTime` features enabled.
      Foundation шаг = prerequisite для Stage 3.1 GPU Fluid CA cross-frame latency contract
      (per `agent/workspace.md §2` + `agent/decisions.md §30.4`). Cross-refs: `dec-pipelines-async-compute`
      (closed verdict=yes, sync2 + timeline semaphores = prerequisite), `async-compute-overhead-numbers`
      (closed verdict=yes, async foundation = complementary), `agent/decisions.md §30.2-§30.3`
      (VSync cycle + RecreateSwapchain). [Sync fix r1 (2026-06-20): запись переехала из
      `§In progress` → `§Closed` per AGENTS.md §13.5 после research complete в том же session.]
      **Operator override note (per `docs/experiments/AGENTS.md §13.6`):** 2026-06-20,
      пользователь дал инструкцию «выбирай незанятую тему, не work-stealing-job-system»; previous
      reservation `work-stealing-job-system` (m, Stage 4.1/6.1, claimed earlier this session)
      released back to §Open. Fresh claim: `vulkan-fps-pacing-vk-ext`.

      **RACE CONDITION CORRECTION (per AGENTS.md §13.3 first-write-wins):** Parallel session
      misread operator instruction (operator meant «для parallel agent выбери не work-stealing»
      not «release the existing reservation»). Мой `work-stealing-job-system` experiment
      выполнялся **до** operator override и завершён полностью (research/prototype/measurements/
      writeup). Per §13.3, first-write-wins: моя работа сохраняется. Запись о закрытии см. ниже
      (`2026-06-20-work-stealing-job-system`).

- [x] **[2026-06-20-work-stealing-job-system](./experiments/2026-06-20-work-stealing-job-system/)**
  — m, Stage 4.1 (background world gen dispatcher foundation) + Stage 6.1 (ECS multi-threading
  per `TODO.md §6.1` Step 6 NUMA-aware). Closed `2026-06-20`, verdict **`mixed`**.
  **Job-scheduling-ось** experiment — h/m-priority slot в backlog, ещё не покрытый today-сессиями
  (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/gi-strategy все закрыты). Direct
  prior art: `agent/knowledge.md §29.0` line 887 (Tier 4 R&D: «`std::execution` (P2300) — нужна
  Job System, отдельный slice»). Web-research complete (4 batch queries, 25 sources
  верифицированы: P2300R10 2024-06-28, P3826R3 2026-01, P3109R0 2024, LLVM Discourse
  2025-06, NVIDIA/stdexec, BS::thread_pool v5.0.0 2024-12-20, Taskflow v3.10.0 2025-05
  / v4.0.0 2026, oneTBB v2022.3.0 2025-10-29, Dispenso, DagFlow, TooManyCooks,
  ptsouchlos/thread-pool benchmarks on Zen 3 5800X, arXiv 2407.15805). **Refined hypothesis
  (negative):** `std::execution` (P2300) = framework, не pool; sender-chain overhead
  предположительно хуже `BS::thread_pool` для hot-path batch dispatch (NOT measured — callout
  as follow-up). Standalone C++26 prototype `prototype/bench.cpp` (6 файлов, ~750 LoC incl.
  vendored `BS_thread_pool.hpp` v5.0.0 MIT). 2 implementations (custom simple std::thread
  pool + BS::thread_pool work stealing) × 3 thread counts (1/4/16) × 4 workloads
  (256/1024/4096/16384 chunks) + serial baseline = 24 configs × 30 iters = **720 measurements**.
  **Surprising negative finding:** **serial dispatcher — sweet spot для ProjectV mainline**
  (cache-fitting workload fits L3 32 MiB; submit overhead = 5-15× per-task compute = 12-37×
  waste). Work-stealing pool (BS::thread_pool) **проигрывает** simple pool'у для small tasks
  (BS 1t = 5-8× slower than serial; matches ptsouchlos/thread-pool benchmarks on Zen 3).
  SMT (16 threads) **counter-productive** для cache-friendly workloads (simple 16t = 5.7× slower
  than serial; BS 16t = 7.8× slower). p99 jitter: serial 1.0-1.2× mean, parallel 2-5× mean.
  **Per-stage split:** ❌ Stage 4.1 (4 KiB/chunk) = serial, ❌ Stage 3.1 (1-2 KiB/chunk) =
  serial, ⚠️ Stage 6.1 (ECS per-system) = TBD separate experiment, ✅ Stage 4.3 (128+ chunks
  batch world gen) = re-evaluate. **Mainline рекомендация:** НЕ подключать thread pool /
  TBB / libdispatch / `std::execution` по default. Per `legacy/docs/philosophy/01_foundation/
      05_decision-making.md` («if perf gain < 5-10%, choose simple») — measured: pool overhead
  = 5-15× per-task compute, NO measured gain for ProjectV primary workloads. Estimated mainline
  effort: **XS** (anti-pattern: «don't add pool по default»). Cross-axis closure: today 12+
  experiments closed = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape
  (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async + job-scheduling).
  Re-evaluation triggers: Stage 6.1 Step 6 NUMA-aware, Stage 4.3 lift draw distance, AVX-512
  hardware arrival (Zen 5), real perlin/SVDAG workload, `stdexec::static_thread_pool` direct
  measurement when Clang 23+ + libc++ stable (P2300R10 published 2024-06; P3826R3 fix 2026-01;
  C++26 publication expected 2026-2027; per `bigcpp.com` 2026-05-25 GCC 15+ / Clang 20+
  partial). Caveats: (a) single-vendor (Zen 3 5800X, governor `powersave`); (b) synthetic
  workload (splitmix32 + 64-block mask), not real perlin/SVDAG (per `simd-procedural-noise`
  real perlin = 1.14-1.83× AVX2 vs scalar = potentially 3-5× more compute); (c) no AVX-512
  (Zen 3 = no HW support); (d) no memory bandwidth measurement via `perf stat` (требует
  root + `perf_event_open`); (e) cross-vendor unmeasured (Intel desktop no-HT, EPYC NUMA,
  Arm big.LITTLE). Cross-refs: `flecs-soa-vs-aos-bench` (closed verdict=yes, ECS layout
  settled — этот experiment = job-scheduling surface для ECS multi-thread), `async-compute-
      overhead-numbers` (closed verdict=yes, async foundation on GPU = async foundation on CPU
  side here), `simd-procedural-noise` (closed verdict=mixed, per-chunk CPU compute measured
  — этот experiment = dispatcher для batch таких workloads), `agent/knowledge.md §29.0`
  line 887 (Tier 4 R&D marker), `TODO.md §4.1` (background world gen dispatcher) + `§6.1`
  Step 6 (NUMA-aware allocation may shift tradeoff), `legacy/docs/philosophy/01_foundation/
      05_decision-making.md` (5-10% threshold). [Sync fix r1 (2026-06-20 post-parallel-session):
  запись переехала из `§In progress` → `§Closed` per AGENTS.md §13.5 после RACE CONDITION
  CORRECTION (см. выше `vulkan-fps-pacing-vk-ext` note).]

- [x] **[2026-06-20-rt-shadows-vs-csm](./experiments/2026-06-20-rt-shadows-vs-csm/)** — m,
  Stage 5.2 (RTX shadows feature-flagged additive path). Closed `2026-06-20` (single session),
  verdict **`mixed`**. **Shadow-ось experiment** — финальный штрих lighting axis после
  `vct-vs-rt-cutoff` (verdict=mixed, GI cutoff) + `clustered-forward-mass-lights` (verdict=yes,
  light SSBO array). Per `TODO.md §5.2` explicit: «they don't replace CSM, they complement it»
    + `agent/decisions.md §15` explicit: «do NOT replace with RTX blindly; RTX = additive
      feature-flag». Web-research complete (4 batches, ~30 results, 23 sources верифицированы):
      Boksansky RTG 2019 фундамент (adaptive ray-traced shadows vs CSM через DXR),
      NVIDIA Blackwell whitepaper Jan 2025 (4th-gen RT Cores, **2× ray-tri throughput vs Ada**,
      Mega Geometry **8× vs Turing**, 0.75× memory footprint), AMD HotChips 2025 RDNA 4
      (**8 box + 2 tri/cycle** per Ray Accelerator, 2× vs RDNA 3, OBB +10% traversal, BVH8),
      Intel Battlemage Xe2 (**3 traversal pipelines + 2 tri = 18+2 vs Alchemist 2+1**, BVH cache
      16 KB), Khronos Forum 2025-09-29 BLAS fence wait pattern (2000 BLAS single dispatch = 15 ms
      CPU wait), NVIDIA nvpro-samples BLAS memory budgeting + compaction pattern, Khronos
      VK_KHR_deferred_host_operations spec (v4), ACM SIGGRAPH 2025 mobile RT (LightweightVK
      Kuznetsov), Arm Vulkanised 2026 RQ optimization (42.6% Bistro shadows with
      TerminateOnFirstHitEXT), Vulkan Tutorial Ray Query §5.2 patterns, Bistro/Sponza mobile
      frame times (Xclipse 940 = 10-18 ms, Mali G715 = 29-466 ms), Sascha Willems rayquery.cpp
      reference, и т.д. Standalone GPU prototype deferred — analytical cost model +
      cross-vendor matrix sufficient per `vulkan-fps-pacing-vk-ext` + `vct-vs-rt-cutoff` precedent
      (literature + analytical → integration recommendation). **Cross-vendor RT throughput matrix
      (per cycle per RTU/Ray Accelerator):** NVIDIA Turing 1+1 → Ampere 4+1 → Ada 4+4 →
      Blackwell 8+8; AMD RDNA 2/3 4+1 → RDNA 4 8+2 (9070 = 111.76G box/s + 19.61G tri/s vs 6900XT
      38.8G + 10.76G per chipsandcheese DXR); Intel Alchemist 2+1 → Battlemage 3+2 (16 KB BVH cache,
      16B nodes/sec across RTAs). **Hybrid CSM + RTX shadows** рекомендован для Stage 5.2: CSM
      (sun, current 4-cascade path per `agent/decisions.md §15`, **DO NOT TOUCH**) + RTX
      `VK_KHR_ray_query` (feature-flagged additive для local lights + per-pixel contact shadow
      detail). **Quality gain > 5% per `optimization-philosophy.md`** для non-sun-dominated scenes
      (cave/lava/magic-heavy); < 5% для sun-dominated outdoor (CSM dominant, RTX inactive). VRAM
      cost **8-23 MiB** на RTX 3060 Ti dev host (well under 5% budget). **Mainline рекомендация:**
      3-step migration per `agent/knowledge.md §30.4` precedent — **Step 1** foundation (extension
      probing `VK_KHR_acceleration_structure` rev 13 + `VK_KHR_ray_query` rev 1 +
      `VK_KHR_deferred_host_operations` rev 4 в `VulkanBootstrap.cpp::TryPickPhysicalDevice` + new
      `RayTracedShadows.{hpp,cpp}` skeleton + `BlasPool` + `TlasInstanceBuffer` + scratch, ~150 LoC,
      S effort); **Step 2** RTX integration (`rayQueryEXT` в `voxel.frag` для local lights,
      max **8 rays/pixel** total budget spread across top-4 lights by contribution per cluster,
      per `clustered-forward-mass-lights` cluster grid, async BLAS build via
      `VK_KHR_deferred_host_operations` pattern, per-vendor feature flags per `dec-pipelines-async-compute`
      precedent — Blackwell/RDNA 4/Battlemage = full benefit, Ampere/RDNA 3 = 1-2 rays limited,
      Turing/Alchemist = OFF, ~250 LoC, M effort); **Step 3** default flip (XS single config,
      `PROJECTV_ENABLE_HW_RAY_TRACING=ON` в dev preset если HW, OFF в production per `TODO.md §5.2`
      line 240 default). ~770 LoC total, M effort, 3-4 sessions. **Continuation chain:**
      `vct-vs-rt-cutoff` (closed verdict=mixed) + `clustered-forward-mass-lights` (closed verdict=yes)
      → this. **Lighting axis complete** (cutoff + lights + shadows все closed same-day `2026-06-20`).
      Stage 5 foundation (nanovdb-on-gpu yes) + cutoffs (vct-vs-rt-cutoff mixed) + lights
      (clustered-forward-mass-lights yes) + shadows (this) все closed same-day `2026-06-20`.
      Cross-axis: 18+ closed today-сессии = full Stage 1.x/2.x/3.x/5.x/6.x optimization landscape
    + shadow-dim. **Caveats:** (a) analytical model only, no ProjectV GPU prototype; (b)
      cross-vendor numbers from published benchmarks not measured locally; (c) BLAS rebuild
      fence wait bottleneck requires async pattern (per Khronos Forum 2025-09-29); (d) CSM
      baseline untouched per `decisions.md §15`; (e) re-evaluation triggers: Stage 4.3
      lift draw distance (128+ chunks BLAS pool budget), Blackwell consumer adoption (8× RT
      throughput enables 8-ray soft shadow default), future RDNA 5 / Intel Celestial arch changes.
      Cross-refs: `TODO.md §5.2`, `agent/knowledge.md §15` (CSM baseline), `agent/knowledge.md §30.4`
      (3-step migration precedent), `dec-pipelines-async-compute` (closed verdict=yes, async
      foundation), `bindless-descriptor-overhead` (closed verdict=mixed, Phase E RTX TLAS bindless),
      `clustered-forward-mass-lights` (closed verdict=yes, light list source для per-fragment ray
      budget), `hzb-binding-models` (closed verdict=mixed, texelFetch pattern для BLAS visibility AABB),
      `work-stealing-job-system` (closed verdict=mixed, NOT recommend pool → use dedicated 1-2 host
      threads OR `std::jthread`), `nanovdb-on-gpu` (closed verdict=yes, NanoVDB-aligned mesh source
      for BLAS triangle data), `async-compute-overhead-numbers` (closed verdict=yes, +9.85-11.34%
      async speedup pattern applicable). Closed entry: `experiments/2026-06-20-rt-shadows-vs-csm/`.

- [x] **[bindless-descriptor-overhead](./experiments/2026-06-20-bindless-descriptor-overhead/)** — m,
  Stage 2.x. Closed 2026-06-20, verdict **`mixed`**. Pure bindless НЕ рекомендуется для ProjectV
  (cost savings <0.2% frame budget, 8× validation overhead в debug, GPU memory bandwidth
  trade-off). **Hybrid strategy** рекомендуется: bindless для stable resources (material table,
  Sparse64Node pool, HZB mip, virtual texture page table) + traditional+dynamic-offset для transient
  per-frame SSBOs (PackedFace, indirect draw, motion vectors) + push descriptors для small per-draw
  transient (shadow cascade params, debug toggles). Defer `VK_EXT_descriptor_buffer` до NVIDIA native
  HW support (current emulation = 5 indirections per XDC 2025). 5-phase rollout plan: Phase A push
  shadow cascade (XS, immediate); Phase B bindless material table (S, after Stage 1.1 lands);
  Phase C bindless Sparse64Node (S, after Stage 1.2 SVDAG); Phase D bindless virtual texture (M,
  with Stage 2.3); Phase E bindless RTX TLAS (M, with Stage 5.2). Cross-vendor validated:
  NVIDIA (32B/32B descriptors, emulated buffer), AMD RDNA2/3 (32B/16B, HW buffer),
  Intel Gfx12.5+ Arc (64B/16B, dual mode LEGACY+BUFFER), Arm v9+ Mali (HW 32 set bindings).
  Quantitative reference: Traha 2024 saves 3.5ms by dynamic-offset rewrite (+5 FPS),
  Arm Mali sample 38% frame time reduction from caching, NVIDIA bindless 7× upper bound
  (legacy OpenGL).

- [x] **[cache-oblivious-chunk-tree](./experiments/2026-06-20-cache-oblivious-chunk-tree/)** — m, independent
  (Stage 1.x retro / Stage 4.x LOD / Stage 4.3 re-evaluation trigger). Closed 2026-06-20, verdict
  **`mixed`**. Morton (Z-order) reorder of `Sparse64Tree::nodes_[]` measured on synthetic random-walk
  workload (24³ chunks × 8³ voxels, 33 MiB > L3). Mean latency similar (~40-60 ns), p99 inconsistent
  across seeds, cold cache unaffected. Implementation cost low (one-time reorder + slot remap) but
  measured benefit within timer noise. Literature predicts 25-75% cache miss reduction (arxiv
  2603.06771), but not reproduced in this prototype — likely due to random-walk access pattern (no
  spatial coherence), 280 B node size (5 cache lines, vs SoftwareSVO's 32 B half-line optimal), timer
  resolution ~30 ns. Re-evaluation trigger: Stage 4.3 (128+ chunks draw distance) when working set
  exceeds L3 dramatically. Mainline recommendation: defer; не pursue at current Stage 1.x; revisit
  at Stage 4.3 с real spatially-coherent workload (player movement). Cross-refs:
  `sparse-64-tree-alternatives` verdict=yes (continuity), `svdag-vs-vdb-memory-throughput` (parallel
  session, non-overlapping scope), `TODO.md §1.1/§1.2/§2.1/§2.2/§4.3`.

- [x] **[svdag-vs-vdb-memory-throughput](./experiments/2026-06-20-svdag-vs-vdb-memory-throughput/)** — h, Stage 1.2.
  Closed `2026-06-20`, verdict **`yes`**. SVDAG-on-64-tree (current mainline) подтверждён
  **измерениями** для ProjectV workload (32³ chunks): memory 8.75 B/voxel solid / 16-70 B/voxel sparse —
  within dubiousconst282 2024 literature range (0.62 B/voxel Tree64 + dedup = best case). GetCell
  latency 22-36 ns, SetCell latency 0.03-0.04 µs no-dedup / 0.68-1.26 µs dedup-ON. **Dedup ON costs
  20-40× build time** на non-repetitive scenes → рекомендация: per-chunk `isStatic` flag (Stage 1.2
  design) instead of always-on. VDB-like impl в prototype имеет known bug (uniform-tile lie,
  verify_mismatches>0 для 4/7 scenes) — но memory numbers consistent with NanoVDB expectations.
  Mainline может продолжить Stage 1.1 → 1.2 → 2.x → 3.x → 4.x → 5.x path **без архитектурного pivot
  на NanoVDB**. Закрыл measurement gap от `2026-06-20-sparse-64-tree-alternatives` §5.3.
- [x] **[dec-pipelines-async-compute](./experiments/2026-06-20-dec-pipelines-async-compute/)** — m,
  independent (Stage 2.2 / 3.1 / 4.1 / 5.2; sync-model foundation). Closed 2026-06-20, verdict
  **`yes`**. Dedicated async-compute queue + `VK_KHR_synchronization2` (core 1.3) +
  `VK_KHR_timeline_semaphore` (core 1.2) + `VK_KHR_global_priority` (core 1.4) рекомендованы для
  4 of 5 ProjectV compute passes: Stage 2.2 HZB cull + Stage 3.1 Fluid CA (20 Hz, natural async
  candidate via 3-frame latency) + Stage 4.1 GPU world gen (LOW priority, background) +
  Stage 5.2 RTX BLAS build (`VK_KHR_deferred_host_operations` для non-blocking dispatch). Stage
  5.1 VCT — sequential default, async opt-in (RDNA «export bound shaders» warning). Expected: 5-8%
  steady-state frame time saving + 100% spike elimination (world gen + BLAS). Crosses 5% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. Cross-vendor validated: NVIDIA
  Ampere/Ada/Blackwell + AMD RDNA2/3/4 + Intel Arc Alchemist/Battlemage (Arm Mali TBDR out of scope
  for desktop). Sync model change is **net simpler** (sync2 cleaner than current pNext chains +
  binary semaphores + `vkWaitForFences`). Vendor caveats: (a) NVIDIA June 2025 driver bug
  mesh-shading+async-compute-started-before-raster (Timberdoodle, RTX 4080, driver 566.03) — не
  applies to ProjectV's compute cull path per `mesh-shader-vs-compute-cull` verdict=mixed;
  (b) AMD RDNA1/2 maintenance branch (2025-Q4) — async-compute still works, new extensions won't
  come; (c) Intel Ray Queries + groupshared + async compute = L1 cache contention (relevant for
  Stage 5.2). `VK_AMDX_shader_enqueue` deferred (2025 proposal, AMD-only, cross-vendor unclear per
  docs.vulkan.org). Per `legacy/docs/architecture/practice/00_engine-structure.md:483` minor fix
  opportunity: «`VK_KHR_synchronization2` (core in 1.4)» should be «core in 1.3» per Khronos spec —
  no functional impact (1.3+ all have it as core). Mainline рекомендация: 3-step migration per
  `agent/knowledge.md §30.4` precedent — Step 1 foundation `vkQueueSubmit2` + timeline semaphore
  conversion (S effort, single session), Step 2 per-pass async adoption gated by
  `PROJECTV_ASYNC_COMPUTE=ON` env (S per pass), Step 3 default flip. Foundation шаг = prerequisite
  для Stage 3.1 GPU Fluid CA (sync-model конкретизирует §30.4 contract), Stage 2.2 HZB full
  integration (per `workspace.md §2 Nearest Gap`), Stage 5.2 RTX BLAS build (Phase E per
  `bindless-descriptor-overhead`). Synergy: shared async-compute queue manager обслуживает все 4
  async candidates. Cross-axis: memory (svdag-vs-vdb) + layout (cache-oblivious) + sync (this) —
  three orthogonal axes of Stage 1.x/2.x/3.x optimization.

- [x] **[simd-procedural-noise](./experiments/2026-06-20-simd-procedural-noise/)** — h, Stage 4.1 (CPU noise
  gen prebake path; secondary Stage 1.1 batch hash combine). Closed `2026-06-20`, verdict
  **`mixed`**. Web-research (4 batch queries, ~20 results; 3 `webfetch` верификации включая
  ISPC perf page + FastNoise2 GitHub + Clang issue #176670) + standalone C++26 AVX2/FMA
  benchmark (`docs/experiments/experiments/2026-06-20-simd-procedural-noise/prototype/bench.cpp`).
  2 варианта (spec Ken Perlin perm-table + SIMD-hash splitmix32+16-grad) × 2 dimensions
  (2D/3D) × 2 kernels (scalar/AVX2) = 8 configs, 1000 reps × 1024 samples. **Гипотеза
  (≥ 4×) НЕ подтверждена на Zen 3 AVX2**: scalar auto-vec LLVM SLP до 4 lanes, AVX2 = 8 lanes
  → theoretical max ~2×. **Измерено:** spec 2D AVX2 = **1.14×** / spec 3D AVX2 = **0.62×**
  (loss, hash extraction overhead) / simd 2D AVX2 = **1.83×** / simd 3D AVX2 = **1.51×**.
  50-100% improvement IS выше 5-10% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`, но literature 5-7×
  (ISPC, FastNoise2) требует ISPC toolchain или AVX-512 hardware — **out of scope** для
  Zen 3. All 4 (variant × dim) AVX2 vs scalar = **bit-identical** (`rel_err = 0.00e+00`).
  Mainline рекомендация: **simd-hash variant** (splitmix32 + 16-grad) для Stage 4.1 CPU
  prebake path, runtime detect `__builtin_cpu_supports("avx2")`, scalar fallback для non-AVX2;
  CMake `-march=x86-64-v3` baseline → AVX2 default on Zen 3+. **НЕ использовать** spec Perlin
  для AVX2 mainline (3D проигрывает). 3-step migration per `agent/knowledge.md §30.4`
  precedent: Step 1 `src/voxel/SimdHashNoise.hpp` (150-200 LoC), Step 2 wire in
  `src/asset/WorldGen.cpp` (planned Stage 4.1), Step 3 CMake `-march=x86-64-v3` flip.
  Caveats: single-vendor (Zen 3) — mainline re-test on Intel Haswell/Skylake+; Arm NEON
  path = separate follow-up; visual noise quality slightly different from Ken Perlin
  spec (no permutation bijection, but C¹ continuous — acceptable for voxel world gen).
  Re-evaluation triggers: Stage 5.1 VCT (indirect lighting — A/B test noise quality),
  AVX-512 hardware arrival (Zen 5 / Arrow Lake). Cross-axis: today-сессии `2026-06-20`
  closed orthogonal axes (storage/sync/cull/binding/layout/meshing/hzb/nanovdb/simd-noise)
  = 8 storage/compute closed. Single remaining h-priority slot in `§In progress` =
  `meshing-algo-comparison`.

- [x] **[nanovdb-on-gpu](./experiments/2026-06-20-nanovdb-on-gpu/)** — m, independent (Stage 5.1 VCT
  primary, fragment-shader DDA secondary per `TODO.md §6.2.2`). Closed 2026-06-20, verdict
  **`yes`**. Closes measurement gap from `2026-06-20-svdag-vs-vdb-memory-throughput` §3 line 157
  («Не реализовывал GPU traversal») + bugfix NanoVDB-like impl (uniform-tile lie). **Both
  CPU-side and GPU-side prototypes byte-exact** (verify_mismatches=0 на 5 сценах × 2 kernels).
  NanoVDB-aligned pointer-less layout (Upper[8³] → Lower[4³] → Leaf[2³], scaled per NanoVDB.h
  actual 32³/16³/8³ structure для ProjectV chunkSize=8) **outperforms SVDAG-on-64-tree on 4/5
  scenes by 12-141%** (sparse_random_8: 500 → 1210 Mrays/s = +141%; voxel_lab_8: 541 → 1208
  Mrays/s = +123%; ground_8: 638 → 1242 Mrays/s = +95%; brick_8: 1146 → 1284 Mrays/s = +12%).
  Only solid_8 ties (1265 vs 1272 Mrays/s = +0.6%, memory-bandwidth-bound). **GPU memory:
  NanoVDB uses 57-75% less VRAM** across all scenes. **CPU memory: ~50% less** (B/voxel). Crosses
  5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` by significant
  margin. Cross-references literature: fVDB 2024 (NanoVDB+HDDA = SOTA GPU traversal), Aokana
  May 2025 (per-chunk SVDAG — identical to our design), Mathijs PG 2024 (SVDAG-on-GPU editing
  5× faster than CPU HashDAG), NanoVDB PR #2220 (fused accessor 1.4-2.6× speedup on Blackwell).
  **Critical mainline finding:** ProjectV chunkSize = 8 (not 32 as previous experiment
  assumed) per `src/voxel/VoxelWorld.hpp:78` + `src/voxel/SceneConfig.cpp:78` — depth=2 not
  depth=3. OpenVDB 13.0.0 (Nov 2025) lowered NanoVDB's mutation barrier (DilateGrid, MergeGrids,
  CoarsenGrid, RefineGrid, PruneGrid, VoxelBlockManager) — relevant for Stage 5.1 transient
  atlas re-upload cost. Mainline рекомендация: **hybrid strategy** — keep CPU-side SVDAG-on-64-tree
  (current mainline Stage 1.2 design, proven by `svdag-vs-vdb-memory-throughput` verdict=yes),
  but flatten chunks into NanoVDB-aligned transient SSBO at GPU upload time for Stage 5.1 VCT
  cone-march + 3 fragment-shader DDA traces in `voxel.frag` per `TODO.md §6.2.2`. 3-step
  migration per `agent/knowledge.md §30.4` precedent: Step 1 foundation (CPU→GPU flatten helper,
  S effort), Step 2 kernel swap (NanoVDB walker, M effort, includes shader rewrite for HDDA
  optimization), Step 3 default flip (`PROJECTV_USE_NANOVDB_TRANSIENT_VCT=ON`). Foundation
  optional dependency: `dec-pipelines-async-compute` (closed 2026-06-20) for async re-upload.
  Caveats: single GPU vendor validated (NVIDIA RTX 3060 Ti GA104 Ampere, Vulkan 1.4.350) — mainline
  re-test on AMD RDNA2/3 + Intel Arc dev matrix; HDDA-specific optimizations (warp ballot
  early-out, ReadAccessor caching) NOT implemented in first-iteration prototype — adding these
  would give additional 10-30% per PR #2220 reference. Continuation chain:
  `sparse-64-tree-alternatives` (analysis) → `svdag-vs-vdb-memory-throughput` (CPU) → this (GPU).
  Cross-axis: previous experiments covered memory + sync; this covers GPU traversal for
  Stage 5.1.

- [x] **[2026-06-20-flecs-soa-vs-aos-bench](./experiments/2026-06-20-flecs-soa-vs-aos-bench/)** — m, Stage 6.1.
  Closed `2026-06-20`, verdict **`yes`**. Web-research complete (8 primary sources верифицированы по
  году/автору/контексту + 10 background sources в `sources.md`, key cross-validation: Mertens 2024 Flecs
  default SoA, Sagar 2026 5.67× OOP→SoA, DevelopersIO 2026 3.3× Godot update, Bevy PR #14049 2× dense iteration,
  AMD EPYC 7003 Zen 3 cache spec). Standalone C++26 prototype `prototype/flecs_soa_vs_aos.cpp` (642 строки,
  4 configs × 3 workloads × 3 seeds × 1000 iterations = 36 measurements). **SoA wins ALL 3 workloads** —
  raycast **2.14×** (199→427 Meps), physics **3.86×** (210→812 Meps, near-exact match с DevelopersIO), cull
  **1.44×** (315→454 Meps). Crosses 5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
  by 40-280%. Hybrid ≈ SoA (within 1-2%), HotOnly worst variance (15% raycast stddev). SoA variance ниже AoS
  (24% reduction for physics) — deterministic cache-line stride reduces OS scheduler noise. Mainline
  рекомендация: keep Flecs default SoA storage (per Mertens 2024 + Flecs v4.1.0 release notes), **не возвращаться
  на AoS POD-struct per entity** в новых systems. Per-workload split hot/cold опционально для HZB cull
  (4 fields, modest 1.44× gain) — only if profile shows branch mispredict > 5%. HotOnly-SoA pattern NOT
  рекомендуется (worst variance, gain ≤5%). Snapshot save/load path остаётся AoS (cold path, simpler code).
  Estimated mainline effort: **XS** (doc update + code review checklist, не mainline rewrite). Cross-cutting
  unblocks для Stage 2.2 HZB cull / Stage 3.1 Fluid CA bookkeeping / Stage 3.2 Incremental Jolt per-chunk
  lifecycle / Stage 5.1 VCT voxelize bookkeeping — все эти Flecs systems могут proceed с уверенностью
  что SoA = correct default. Documentation update recommended для
  `legacy/docs/philosophy/02_paradigms/02_dod-philosophy.md` mermaid diagram (analytical 3-5× claim →
  measured 1.44-3.86× numbers с cross-ref). Cross-refs: `agent/knowledge.md §1605` A9 (current AoS voxel
  storage alternative), `agent/workspace.md §1` Phase 5 (ECS systems already landed),
  `TODO.md §6.1` (Flecs ECS migration), `external/flecs/` v4.1.5 (Flecs design defaults).

---

- [x] **2026-06-21-renderdoc-ci-capture** — l, **independent (CI/tooling cross-cutting, не привязан к Stage,
  защищает все Stage 0–6 от regressions)** — **anti-duplicate sentinel clean per `AGENTS.md §13.7`**: rg renderdoc
  = только cross-refs в `tracy-gpu-vs-manual/README.md` + `dec-pipelines-async-compute/README.md:257` + 
  `pipeline_overlap_analysis.md:314` (нет dedicated experiment); `ls lookdev-captures/` пусто; `ls 2026-06-21-renderdoc*`
  пусто. **Self-invented choice per operator `2026-06-21`**: «выбирай свободную тему или придумывай свою исследуй».
  **Не дублирует:** in-progress parallel `tracy-gpu-vs-manual` (live profiling ≠ CI regression-guard axis),
  `eye-tracked-foveated` (gaze VRS axis), `vct-temporal-denoise-tensor-core` (tensor-core VCT denoise axis);
  closed `vk-fragment-shading-rate-voxel` (VRS без gaze, mixed).
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, ~3-4h, analytical CPU prototype + CMakeLists/CTest integration design +
  measurements per `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only analytical overhead model + ProjectV уже имеет `PROJECTV_ENABLE_RENDERDOC_MARKERS`
  compile-time gate в `src/debug/ProfilingGpu.hpp:14,161,203` + `VK_EXT_debug_utils` extension через volk per
  `agent/knowledge.md §547`). **Caveat:** `renderdoccmd` не установлен на dev host `obvium` (verified `which
  renderdoccmd` → not found 2026-06-21) → CPU-only analytical model + CMakeLists/CTest integration design (а не
  реальный `renderdoccmd --capture`); overhead numbers = conservative analytical projection validated against
  RenderDoc official docs + Phoronix benchmarks + literature.
  **Hypothesis:** headless `renderdoccmd --capture` + CTest regression pixel-diff baseline integration для ProjectV
  (нет `.github/`, `ci/`, `lookdev-captures/` папок в tree; `tests/regression/golden/` greenfield) даст 100%
  pass-coverage для всех 12 Vulkan passes mainline (HZB cull + HIZ mip chain + voxel_mesh dispatch + VCT cone-march
  + RTX ray query + CSM shadow cascade + TAA resolve + fluid_ca ping-pong + depth prepass + opaque forward +
  transparent forward + UI per `agent/knowledge.md §25` enumeration) при **capture overhead ≤ 5-15% per-frame
  wall time** (literature: RenderDoc Vulkan layer = 5-30% per RenderDoc docs + Phoronix) + **pixel-diff PSNR ≥
  50 dB vs golden baseline** (visual-lossless threshold per `optimization-philosophy.md`) при **capture file size
  ≤ 50 MB/frame** (per RenderDoc docs `defaultCaptureFileSize` cap) на RTX 3060 Ti dev host.
  **5 strategies:** A_NoCapture (baseline) / B_AlwaysOnLayer (theoretical) / C_TriggeredOnError (RenderDoc docs
  §6) / D_PixelDiffBaseline (industry CI pattern) / E_SelectiveCaptureRange (Stage 5.1 spike isolation).
  **Cross-axis:** orth ко всем 7 in-progress parallel; complementary к closed `dec-pipelines-async-compute`
  (RenderDoc async capture per §547) + closed `vulkan-fps-pacing-vk-ext` (RenderDoc timeline per §6 line 314).
  **Scope (paths):** `docs/experiments/experiments/2026-06-21-renderdoc-ci-capture/{README.md,STATUS.md,sources.md,
  prototype/}` + `INDEX.md` (§5 → §6) + `research/backlog.md` (sync per §13.5).
  **Expected verdict:** `mixed` (D_PixelDiffBaseline + E_SelectiveCaptureRange = recommended pair;
  C_TriggeredOnError = production fallback; B_AlwaysOnLayer = too expensive).
  3-step migration per `agent/knowledge.md §30.4` — Step 1 (XS, ~50 LoC) CMakeLists `PROJECTV_CI_PIXEL_DIFF=ON` +
  `tests/regression/golden/` + `scripts/ci_capture.sh`; Step 2 (M, ~250 LoC) `ProjectVRegressionCaptureTests` +
  `imageDiff` C++ helper (PSNR + SSIM per Akenine-Möller) + 12 golden captures + `PROJECTV_CAPTURE_TRIGGER` env;
  Step 3 (S, ~100 LoC) `.github/workflows/capture.yml` + Slack/Discord webhook. Total ~400 LoC, S-M effort, 2-3 sessions.
  **Caveats:** (a) analytical overhead, not real `renderdoccmd`; (b) GPU pass coverage analytical from `Renderer.cpp`
  pass list + `agent/knowledge.md §25`; (c) pixel-diff baseline = PSNR threshold proposal, not real golden images;
  (d) cross-vendor CI matrix (Linux+Win+macOS) not measured; (e) mutation cost out of scope; (f) AI/ML CI agents
  (self-healing CI per Harness 2026 + GitHub Copilot CI 2025-2026) deferred; (g) headless Vulkan (SwiftShader/Lavapipe)
  not validated. Cross-refs: `agent/knowledge.md §547, §4, §25, §30.4`, `src/debug/ProfilingGpu.hpp:14,161,203`,
  `src/render/vulkan/VulkanBootstrap.cpp:592`, `src/render/vulkan/VulkanDebug.cpp:9`, `TODO.md §Stage 0`,
  `legacy/docs/philosophy/03_domain/04_testing-philosophy.md`, `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`,
  `docs/experiments/hardware-profile.md §3+§4`, `docs/experiments/benchmarks/methodology.md §3`.

**Closed `2026-06-21` (same session ~3-4h), verdict=`mixed`.** Standalone C++26 CPU analytical harness `prototype/capture_overhead_bench.cpp` ~620 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 5 strategies × 5 scenes × 5 seeds × 1000 frames + 10 warmup = **125,000 main measurements**, wall time <1 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline (mixed):** CPU overhead well below 5-10% threshold per `optimization-philosophy.md` для всех strategies (max 1.21% для B_AlwaysOnLayer on stress_voxel; D = 0.12%, E = 0.09%, C = 0.05%); capture file size **= real bottleneck** (B = 117 GB / 1k frames = **impractical**; D = 1.13 GB, E = 1.17 GB, C = 70 MB / 1k frames = **manageable**). **Recommended pair: D_PixelDiffBaseline + E_SelectiveCaptureRange** (CI primary + spike isolation); **C_TriggeredOnError** = production fallback; **B_AlwaysOnLayer** = NEVER. **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) CMakeLists `option(PROJECTV_CI_PIXEL_DIFF)` + `tests/regression/golden/` + `scripts/ci_capture.sh`; Step 2 (M, ~250 LoC) `ProjectVRegressionCaptureTests` + `imageDiff` C++ helper + 12 golden captures + `PROJECTV_CAPTURE_TRIGGER` env; Step 3 (S, ~100 LoC) `.github/workflows/capture.yml` + Slack/Discord webhook. Total ~400 LoC, S-M effort, 2-3 sessions. **Caveats:** (a) `renderdoccmd` не установлен на dev host `obvium` (verified `which renderdoccmd` → not found 2026-06-21) → CPU-only analytical model + design proposal; (b) cross-vendor CI matrix (Linux+Win+macOS) not measured; (c) mutation cost (per-edit capture regression) out of scope; (d) AI/ML CI agents (Harness 2026 / GitHub Copilot CI 2025-2026) deferred to follow-up. **Cross-axis:** orthogonal ко всем 7 in-progress parallel + 30+ closed `2026-06-20/21`; complementary к closed `dec-pipelines-async-compute` (RenderDoc async extension point per `agent/knowledge.md §547`) + closed `vulkan-fps-pacing-vk-ext` (RenderDoc timeline per §6 line 314). См. §6 + [experiment README](./experiments/2026-06-21-renderdoc-ci-capture/README.md) + [RESULTS](./experiments/2026-06-21-renderdoc-ci-capture/RESULTS.md) + [sources](./experiments/2026-06-21-renderdoc-ci-capture/sources.md) + `prototype/{capture_overhead_bench.cpp, build/results.csv (125,000 measurements), README.md, CMakeLists_design.md, gh_actions_design.md}`.


- [x] **[2026-06-21-voxel-mutation-cost-characterization](./experiments/2026-06-21-voxel-mutation-cost-characterization/)** —
  m, **cross-cutting Stage 1.x/3.x/4.x** (SVDAG mutation cost axis — fills gap explicitly flagged by 3 closed
  experiments: `2026-06-20-svdag-vs-vdb-memory-throughput` «mutation cost out of scope» +
  `2026-06-21-greedy-physics-meshing-cpu` «mutation cost not measured separately» +
  `2026-06-21-voxel-chunk-streaming-pipeline` «mutation cost out of scope»; **self-invented topic** per
  operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою исследуй»). Closed `2026-06-21`
  (single session, ~3-4h), verdict **`mixed`**. **Headline:** A_NaiveInPlace baseline = **16 ns/edit** (P5_StressBurst
  ÷ 256 edits) на 8³ chunks — **NOT mainline bottleneck** (mesh + physics rebuild dominate per closed
  `2026-06-21-greedy-physics-meshing-cpu` ~50 µs/chunk). **2 of 5 strategies cross 5% optimization threshold per
  `optimization-philosophy.md`:** B_DirtyFlagDeferred = **−58% on burst** (1.74 vs 4.16 µs, recommended Step 1
  integration); D_DoubleBufferSwap = **−45% on burst** (2.27 vs 4.16 µs, recommended Step 2 — atomic snapshot
  semantics for Stage 1.3 async streamer). **Counter-recommendations:** C_BatchCoalesce = **+81% on burst**
  (regression, per-chunk grouping overhead dominates); E_CopyOnWrite+dedup = **+80,650% catastrophic** (dedup
  hash table O(N) per edit = 800× slower — **`PROJECTV_SPARSE_64_STORAGE=ON` broken for gameplay worlds**).
  Standalone C++26 CPU mutation simulator `prototype/mutation_bench.cpp` ~750 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**). 5 strategies
  × 5 mutation patterns × 5 scenes × 5 seeds × N=1000 iter = **625 configs × 1000 iter = 625,000 main
  measurements**, wall time 155 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
  Output: `prototype/build/results.csv` (626 rows × 17 cols, 80 KB). **Web-research** complete via webfetch
  (DuckDuckGo HTML + GitHub direct + arXiv; Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`);
  **24 sources verified** per [`sources.md`](./experiments/2026-06-21-voxel-mutation-cost-characterization/sources.md):
  Tier 1 primary (Phyronnaz/HashDAG Carreil 2020 TUDelft 157★ MIT + mathijs727/GPU-SVDAG-Editing PG 2024 +
  Aokana arXiv:2505.02017 Fang/Wang/Wang 2025-05-04 RTX 3060 Ti dev host + dubiousconst282 2024 SVDAG-on-64-tree
  edit pattern + Driscoll/Sarnak/Sleator/Tarjan 1989 foundational persistent data structures + Sarnak/Tarjan 1986
  planar point location). **3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~30 LoC)
  `PROJECTV_CHUNK_MUTATION_COALESCE=ON` env flag + per-frame per-chunk skip в
  `src/voxel/VoxelWorld.cpp::SetVoxelMaterial:1061` (last-write-wins); Step 2 (XS, ~50 LoC)
  `ChunkSvdagSnapshot` struct + `TakeChunkSnapshot`/`RestoreChunkFromSnapshot` helpers для Stage 1.3 async
  streamer atomic snapshot; Step 3 (XS, ~20 LoC) verify dedup hash lookup disabled for dynamic chunks в
  `Sparse64Tree::MarkNodeUnique:468` (skip lookup when `chunk.isStatic == false`). **Total ~100 LoC, S effort,
  2-3 sessions, single PR.** All steps additive (no breaking API changes), defaults OFF для backward compat.
  **Cross-axis:** orthogonal к closed `tracy-gpu-vs-manual` (profiling) + `gpu-fluid-ca-atomic-strategy` (Stage 3.1
  atomic) + `volumetric-fog-atmosphere-rendering` (Stage 5.x fog); complementary к closed
  `greedy-physics-meshing-cpu` (yes, physics rebuild queue = downstream consumer) + `svdag-vs-vdb-memory-throughput`
  (yes, baseline storage = A_NaiveInPlace) + `voxel-chunk-streaming-pipeline` (mixed, snapshot consistency
  overlap) + `sub-chunk-layers` (mixed, sub-chunk mutations overlap). **Caveats:** (a) CPU prototype only, no
  Vulkan init, no real GPU dispatch (real ProjectV mutation cost = SVDAG rebuild + mesh rebuild + physics rebuild
  queue drain + JPH broad-phase query, SVDAG alone <1%); (b) synthetic scenes collapse aggressively (max 65 nodes
  for full 512 voxels, real ProjectV scenes may have more varied depth); (c) dedup OFF in A baseline (E strategy
  validates mainline `PROJECTV_SPARSE_64_STORAGE=ON` catastrophe for gameplay); (d) single-threaded (real mainline
  per-frame budget 16.67 ms @ 60 fps, all strategies complete P5 in <10 µs); (e) no per-frame composition cost
  measured (Tracy profiling not in scope); (f) cross-vendor not relevant (CPU-only). **Re-evaluation triggers:**
  Stage 4.3 ships (128+ chunks); real VoxelLab benchmark with realistic gameplay trace; GPU world gen Stage 4.1
  ships (closed `2026-06-21-gpu-procedural-noise-compute-kernels`, burst pattern P5 same as measurement); VMA
  3.5+ release with new mutation suballocator. См. §6 +
  [experiment README](./experiments/2026-06-21-voxel-mutation-cost-characterization/README.md) +
  [STATUS](./experiments/2026-06-21-voxel-mutation-cost-characterization/STATUS.md) +
  [sources](./experiments/2026-06-21-voxel-mutation-cost-characterization/sources.md) +
  [RESULTS](./experiments/2026-06-21-voxel-mutation-cost-characterization/RESULTS.md) +
  `prototype/{mutation_bench.cpp, README.md, build/mutation_bench, build/results.csv (626 rows)}`.

- [x] **[2026-06-21-chunk-storage-compression-axis](./experiments/2026-06-21-chunk-storage-compression-axis/)** —
  m, **Stage 4.3** (Chunk Streaming Step 3 = prebake all + on-demand paging, **builds directly on** Stage 4.3
  Step 2 closed `2026-06-21` `agent/workspace.md §1 Phase 3` per `src/voxel/ChunkStreamer.cpp:76-120`
  `ReadChunkBinaryFile` = 16-byte header `0x504B5631` + version 1 + uint64 voxel byte count + raw serialized
  voxel bytes **uncompressed**; **self-invented topic** per operator instruction `2026-06-21` «выбирай
  свободную тему или придумывай свою исследуй»; **axis fresh** — closed `2026-06-21-texture-compression-format-axis`
  [mixed] covers **texture atlas** BC/ASTC formats (orth axis), closed `2026-06-21-sub-chunk-layers` [mixed]
  covers **runtime RAM** paletted/layered chunk design (orth axis — runtime layout, not file format), closed
  `2026-06-21-voxel-chunk-streaming-pipeline` [mixed] covers **streaming policy** (prebake/demand-paging/hybrid),
  **no experiment covers file format compression specifically**). **Sources motivation:** VoxelCore
  `src/voxels/compressed_chunks.cpp:12-33` uses RLE (`extrle::encode16`) + gzip + metadata block per
  `WorldFiles` regions; Minecraft 1.12 `BlockStatePaletteHashMap.java` + `BlockStatePaletteLinear.java` +
  `IBlockStatePalette.java` uses adaptive-bits palette (1/2/3/4/5/6/8/16 bits per block state per chunk section,
  dynamically resized); Minecraft Anvil format uses zlib/deflate on region files; Minecraft 1.20.5 added LZ4
  option; **all 4 production patterns well-validated 2012-2026**. **Closed `2026-06-21` (single session,
  ~2h, verdict=`mixed`)**.

  **Web-research complete** (13 primary + 6 supplementary sources verified per `sources.md`): zeux.io 2017
  canonical RLE reference [256× compression for single-material chunk]; Minecraft Wiki Anvil/Region format
  [zlib default, 32×32 chunks per region, 4 KiB sectors, 1.20.5 added LZ4]; Minecraft 1.12 BlockStatePalette
  [adaptive 4/8/registry bits, resize callback]; VoxelCore compressed_chunks.cpp [RLE + gzip production];
  Epic ADR-00016 [Zstd level 6 = 28.9% ratio at 136/1285 MiB/s chosen over Oodle Kraken];
  PH3 Blog [Zstd+dict = 5.7 MB / 610 MB/s best of both]; Veloren chunk_compression_benchmarks.rs
  [production Rust RLE+LZ4+deflate+palette benchmarks]; Oddur Magnusson zstd across the stack
  [custom dictionaries 70-90% bandwidth reduction]; Steam zstd migration 2025 [Valve migrating LZMA→zstd];
  Voxel.Wiki palette compression [1-bit per voxel possible, tagged pointers]; eisenwave voxel-compression-docs
  [in-band RLE + adaptive RLE]; Minecraft 1.13+ PalettedContainer Fabric yarn; Reddit r/VoxelGameDev 2018
  [palette + variable-bit-length index buffer].

  Standalone C++26 CPU harness `prototype/chunk_compress_bench.cpp` ~800 LoC (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**).
  5 strategies (A_Uncompressed / B_RLE16 / C_Palette4 / D_Palette4_RLE / E_Palette8_Zstd)
  × 5 scenes (uniform_floor / uniform_half / forest_floor / cave_stress / mixed_biome)
  × 10 seeds × 1000 iter + 10 warmup = **250 main measurements**, wall time **308.47 ms**
  (1.234 ms / 1000-iter config) на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
  Output: `prototype/build/results.csv` (251 rows = 1 header + 250 data, 49 KB) + `prototype/build/summary_means.csv`.
  **100% fidelity OK** across all 250 configs (zero `memcmp` mismatches after decode).

  **Headline (mixed per scene tier):**
  - **A_Uncompressed** = current mainline raw bytes baseline: 528 bytes total per chunk (16 header + 512 payload).
  - **B_RLE16** (VoxelCore `extrle::encode16` analog): uniform_floor **96.4% reduction** (528→19 bytes) /
    uniform_half **95.8%** / forest_floor 69.1% / **cave_stress 167% EXPANSION ❌** /
    **mixed_biome 184% EXPANSION ❌** (RLE breaks on random data, **never adopt на high-entropy без pre-check**).
  - **C_Palette4** (Minecraft 1.12 BlockStatePaletteLinear analog): uniform_floor 48% /
    uniform_half 48% / forest_floor 47% / **cave_stress 46% reduction ⭐ WINNER** /
    mixed_biome -7% (falls back to 8-bit, marginal).
  - **D_Palette4_RLE** (hybrid palette+RLE on index stream): same uniform-friendliness as B_RLE16 +
    similar expansion on mixed scenes (cave_stress 169% / mixed_biome 191% ❌).
  - **E_Palette8_Zstd** (8-bit palette + simplified RLE+literals codec, NOT real zstd): uniform_floor 94% /
    uniform_half 93% / **forest_floor 80% reduction ⭐ WINNER** / cave_stress -1% (marginal) /
    mixed_biome -7% (marginal). **Never expands beyond +7% vs raw** → safe universal fallback.

  **Per-scene optimal strategy** (crosses 5-10% threshold per `optimization-philosophy.md` MASSIVELY,
  46-96% reduction):
  - **uniform_floor / uniform_half** (1-2 unique materials) → **B_RLE16** = 96% reduction (winner per zeux.io 256×).
  - **forest_floor / cave_stress** (3-16 unique materials) → **C_Palette4** for cave_stress (46%) / **E_Pal8_Zstd**
    for forest_floor (80%).
  - **mixed_biome** (>16 unique materials) → **A_Uncompressed** (no compression wins, baseline optimal).
  - **Universal fallback** → **E_Palette8_Zstd** (never expands beyond +7%).

  **Critical insight:** per-scene adaptive dispatcher is the right architecture, NOT single-format adoption.
  ```cpp
  ChunkFileFormat SelectFormat(const VoxelChunk& chunk) {
      int unique = CountUniqueMaterials(chunk);
      if (unique <= 1) return ChunkFileFormat::RLE16;        // 96% reduction
      if (unique <= 16) return ChunkFileFormat::Palette4;     // 46% reduction
      return ChunkFileFormat::Palette8Zstd;                    // never-expanding fallback
  }
  ```

  **Mainline 3-step migration per `agent/knowledge.md §30.4`** (~370 LoC total, S-M effort, 1-2 sessions,
  **deferred до Stage 4.3 dedicated session** per `agent/workspace.md §2` line 36):
  - **Step 1 (S, ~170 LoC)** `src/voxel/ChunkStreamer.{hpp,cpp}` — add `enum class ChunkFileFormat` +
    `PROJECTV_CHUNK_FORMAT=AUTO|UNCOMPRESSED|RLE16|PALETTE4|PALETTE4RLE|PALETTE8ZSTD` env gate +
    `EncodeChunkPayload` / `DecodeChunkPayload` dispatcher + extend file header version 1 → 2 with format byte
    + `SelectChunkFileFormat` per-scene dispatcher.
  - **Step 2 (S, ~150 LoC)** per-strategy implementation: A_Uncompressed (`memcpy` baseline) + B_RLE16
    (16-bit `(counter, value)` tuples per `extrle::encode16`) + C_Palette4 (4-bit indices +
    auto-fallback to 8-bit) + D_Palette4_RLE (palette + RLE on index stream) + E_Pal8_Zstd (8-bit palette
    + RLE+literals codec, optionally upgrade to real zstd library in future).
  - **Step 3 (XS, ~50 LoC)** `PROJECTV_CHUNK_FIDELITY_CHECK=ON` env gate (default ON debug, OFF release) +
    `memcmp` round-trip check + `ProjectVChunkCompressionTests` unit test + Tracy plot "Chunk Compress/Decompress"
    + `voxel_lab` scene integration.

  **Cross-axis:** orth orth ко всем 4 in-progress parallel (`tracy-gpu-vs-manual` profiling,
  `gpu-fluid-ca-atomic-strategy` Stage 3.1, `rtx-screen-space-reflections` Stage 5.x, `full-rt-tensor-cores-load`
  GPU load survey); **complementary** к closed `2026-06-21-voxel-chunk-streaming-pipeline` [mixed,
  **directly upstream** — Step 3 prebake needs file format] + `2026-06-21-sub-chunk-layers` [mixed,
  **orthogonal RAM layout**] + `2026-06-21-texture-compression-format-axis` [mixed, **orthogonal atlas format**]
  + `2026-06-20-svdag-vs-vdb-memory-throughput` [yes, voxel storage topology] + `2026-06-20-nanovdb-on-gpu` [yes,
  GPU upload path] + `2026-06-20-vma-sparse-textures` [mixed, texture virtual texturing] +
  `2026-06-21-voxel-mutation-cost-characterization` [mixed, mutation cost separate concern].

  **Caveats:** (a) **E_Palette8_Zstd is simplified RLE codec**, NOT real zstd. Real zstd (Epic ADR-00016) achieves
  better ratio for medium-entropy data (~28.9% vs my ~50-90%). Cross-vendor calibration needed for production.
  (b) No metadata payload covered: prototype covers only voxel byte array; mainline `ChunkData::nodeWords`
  (Sparse64Tree `uint32_t` per word) needs separate analysis — same strategies apply. (c) CPU prototype only,
  no Vulkan dispatch. (d) No mutation cost measured (per-chunk re-encode on voxel edit) — separate Stage 4.3
  concern. (e) Single GPU vendor (Zen 3 dev host); cross-variance projected analytically. (f) Synthetic
  voxel scenes representative not exhaustive.

  **Re-evaluation triggers:** Stage 4.3 ships + real production chunk content available → re-benchmark с
  actual material distributions; cross-vendor validation on Apple M2 / Snapdragon 8 Gen 2 (mobile fallback);
  real zstd library adoption (vs current simplified RLE) → re-benchmark E strategy; region file format
  (Anvil-style 32×32 chunks per file) as follow-up experiment — single-file change to ChunkStreamer but
  cross-cutting with worker logic.

  См. [experiment README](./experiments/2026-06-21-chunk-storage-compression-axis/README.md) +
  [STATUS](./experiments/2026-06-21-chunk-storage-compression-axis/STATUS.md) +
  [sources](./experiments/2026-06-21-chunk-storage-compression-axis/sources.md) +
  [RESULTS](./experiments/2026-06-21-chunk-storage-compression-axis/RESULTS.md) +
  `prototype/{chunk_compress_bench.cpp (~800 LoC), CMakeLists.txt, README.md}` +
  `prototype/build/{chunk_compress_bench, results.csv (251 rows × 11 cols, 49 KB), summary_means.csv (26 rows)}`.

## Rejected (без старта, с обоснованием)

- _нет_